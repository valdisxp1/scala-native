package java.lang

import java.util
import java.lang.Thread.UncaughtExceptionHandler

import scala.collection.JavaConversions._
import scala.scalanative.runtime.ShadowLock

// Ported from Harmony

class ThreadGroup private[lang] (
                                 // Parent thread group of this thread group
                                 private val parent: ThreadGroup,
                                 // This group's name
                                 private[lang] val name: String,
                                 mainGroup: scala.Boolean)
    extends UncaughtExceptionHandler {

  import ThreadGroup._

  // This group's max priority
  var maxPriority: Int = Thread.MAX_PRIORITY

  // Indicated if this thread group was marked as daemon
  private var daemon: scala.Boolean = false

  // Indicates if this thread group was already destroyed
  private var destroyed: scala.Boolean = false

  // List of subgroups of this thread group
  private val groups: util.LinkedList[ThreadGroup] =
    new util.LinkedList[ThreadGroup]()

  // All threads in the group
  private val threads: util.LinkedList[Thread] = new util.LinkedList[Thread]()

  def this(parent: ThreadGroup, name: String) = {
    this(parent, name, mainGroup = false)
    if (parent == null) {
      throw new NullPointerException(
        "The parent thread group specified is null!")
    }

    parent.checkAccess()
    this.daemon = parent.daemon
    this.maxPriority = parent.maxPriority
    parent.add(this)
  }

  def this(name: String) = this(Thread.currentThread.group, name)

  def activeCount(): Int = {
    val (groupsCopy: util.List[ThreadGroup], threadsCopy: util.List[Thread]) =
      lock.safeSynchronized {
        if (destroyed) {
          new util.ArrayList[ThreadGroup](0) -> new util.ArrayList[Thread](0)
        } else {
          val copyOfGroups  = groups.clone().asInstanceOf[util.List[ThreadGroup]]
          val copyOfThreads = threads.clone().asInstanceOf[util.List[Thread]]
          copyOfGroups -> copyOfThreads
        }
      }

    threadsCopy.toList.count(_.isAlive) + groupsCopy.map(_.activeCount()).sum
  }

  def activeGroupCount(): Int = {
    val groupsCopy: util.List[ThreadGroup] =
      lock.safeSynchronized {
        if (destroyed) {
          new util.ArrayList[ThreadGroup](0)
        } else {
          groups.clone().asInstanceOf[util.List[ThreadGroup]]
        }
      }

    groupsCopy.size + groupsCopy.toList.map(_.activeGroupCount()).sum
  }

  @deprecated
  def allowThreadSuspension(b: scala.Boolean): scala.Boolean = false

  def checkAccess(): Unit = ()

  def destroy(): Unit = {
    checkAccess()
    lock.safeSynchronized {
      if (destroyed)
        throw new IllegalThreadStateException(
          "The thread group " + name + " is already destroyed!")
      nonsecureDestroy()
    }
  }

  def enumerate(list: Array[Thread]): Int = {
    checkAccess()
    enumerateThreads(list, 0, true)
  }

  def enumerate(list: Array[Thread], recurse: scala.Boolean): Int = {
    checkAccess()
    enumerateThreads(list, 0, recurse)
  }

  def enumerate(list: Array[ThreadGroup]): Int = {
    checkAccess()
    enumerateGroups(list, 0, true)
  }

  def enumerate(list: Array[ThreadGroup], recurse: scala.Boolean): Int = {
    checkAccess()
    enumerateGroups(list, 0, recurse)
  }

  def getMaxPriority: Int = maxPriority

  def getName: String = name

  def getParent: ThreadGroup = {
    if (parent != null) parent.checkAccess()
    parent
  }

  def interrupt(): Unit = {
    checkAccess()
    nonsecureInterrupt()
  }

  def isDaemon: scala.Boolean = daemon

  def isDestroyed: scala.Boolean = destroyed

  def list(): Unit = list("")

  def parentOf(group: ThreadGroup): scala.Boolean = {
    var parent: ThreadGroup = group
    while (parent != null) {
      if (this == parent) return true
      parent = parent.getParent
    }
    false
  }

  @deprecated
  def resume(): Unit = {
    checkAccess()
    nonsecureResume()
  }

  def setDaemon(daemon: scala.Boolean): Unit = {
    checkAccess()
    this.daemon = daemon
  }

  def setMaxPriority(priority: Int): Unit = {
    checkAccess()

    /*
     * GMJ : note that this is to match a known bug in the RI
     * http://bugs.sun.com/bugdatabase/view_bug.do?bug_id=4708197
     * We agreed to follow bug for now to prevent breaking apps
     */
    if (priority > Thread.MAX_PRIORITY) return
    if (priority < Thread.MIN_PRIORITY) {
      this.maxPriority = Thread.MIN_PRIORITY
      return
    }
    val new_priority: Int = {
      if (parent != null && parent.maxPriority < priority)
        parent.maxPriority
      else
        priority
    }

    nonsecureSetMaxPriority(new_priority)
  }

  @deprecated
  def stop(): Unit = {
    checkAccess()
    nonsecureStop()
  }

  @deprecated
  def suspend(): Unit = {
    checkAccess()
    nonsecureSuspend()
  }

  override def toString: String = {
    getClass.getName + "[name=" + name + ",maxpri=" + maxPriority + "]"
  }

  def uncaughtException(thread: Thread, throwable: Throwable): Unit = {
    if (parent != null) {
      parent.uncaughtException(thread, throwable)
      return
    }
    val defaultHandler: Thread.UncaughtExceptionHandler =
      Thread.getDefaultUncaughtExceptionHandler
    if (defaultHandler != null) {
      defaultHandler.uncaughtException(thread, throwable)
      return
    }
    if (throwable.isInstanceOf[ThreadDeath]) return
    System.err.println("Uncaught exception in " + thread.getName + ":")
    throwable.printStackTrace()
  }

  def add(thread: Thread): Unit = {
    lock.safeSynchronized {
      if (destroyed)
        throw new IllegalThreadStateException(
          "The thread group is already destroyed!")
      threads.add(thread)
    }
  }

  def checkGroup(): Unit = {
    lock.safeSynchronized {
      if (destroyed)
        throw new IllegalThreadStateException(
          "The thread group is already destroyed!")
    }
  }

  def remove(thread: Thread): Unit = {
    lock.safeSynchronized {
      if (destroyed) return
      threads.remove(thread)
      thread.group = null
      if (daemon && threads.isEmpty && groups.isEmpty) {
        // destroy this group
        if (parent != null) {
          parent.remove(this)
          destroyed = true
        }
      }
    }
  }

  def add(group: ThreadGroup): Unit = {
    lock.safeSynchronized {
      if (destroyed)
        throw new IllegalThreadStateException(
          "The thread group is already destroyed!")
      groups.add(group)
    }
  }

  private[lang] def enumerateThreads(list: Array[Thread],
                                     of: Int,
                                     recurse: scala.Boolean): Int = {
    var offset: Int = of
    if (list.isEmpty) return 0
    var groupsCopy: util.List[ThreadGroup] = null // a copy of subgroups list
    var threadsCopy: util.List[Thread]     = null // a copy of threads list
    lock.safeSynchronized {
      if (destroyed)
        return offset
      threadsCopy = threads.clone().asInstanceOf[util.List[Thread]]
      if (recurse)
        groupsCopy = groups.clone().asInstanceOf[util.List[ThreadGroup]]
    }
    for (thread: Thread <- threadsCopy.toList) {
      if (thread.isAlive) {
        list(offset) = thread
        offset += 1
        if (offset == list.length) return offset
      }
    }
    if (recurse) {
      val it: util.Iterator[ThreadGroup] = groupsCopy.iterator()
      while (offset < list.length && it.hasNext) {
        offset = it
          .next()
          .enumerateThreads(list, offset, true)
      }
    }
    offset
  }

  private[lang] def enumerateGroups(list: Array[ThreadGroup],
                                    of: Int,
                                    recurse: scala.Boolean): Int = {
    var offset: Int = of
    if (destroyed)
      return offset
    val firstGroupIdx: Int = offset
    lock.safeSynchronized {
      for (group: ThreadGroup <- groups.toList) {
        list(offset) = group
        offset += 1
        if (offset == list.length)
          return offset
      }
    }
    if (recurse) {
      val lastGroupIdx: Int = offset
      var i: Int            = firstGroupIdx
      while (offset < list.length && i < lastGroupIdx) {
        offset = list(i).enumerateGroups(list, offset, true)
        i += 1
      }
    }
    offset
  }

  private def list(pr: String): Unit = {
    var prefix: String = pr
    println(prefix + toString)
    prefix += LISTING_INDENT
    var groupsCopy: util.List[ThreadGroup] = null // a copy of subgroups list
    var threadsCopy: util.List[Thread]     = null // a copy of threads list
    lock.safeSynchronized {
      threadsCopy = threads.clone().asInstanceOf[util.List[Thread]]
      groupsCopy = groups.clone().asInstanceOf[util.List[ThreadGroup]]
    }
    for (thread: Thread <- threadsCopy.toList)
      println(prefix + thread)
    for (group: ThreadGroup <- groupsCopy.toList)
      group.list(prefix)
  }

  def nonsecureDestroy(): Unit = {
    var groupsCopy: util.List[ThreadGroup] = null

    lock.safeSynchronized {
      if (threads.size > 0)
        throw new IllegalThreadStateException(
          "The thread group " + name + "is not empty")
      destroyed = true
      groupsCopy = groups.clone().asInstanceOf[util.List[ThreadGroup]]
    }

    if (parent != null)
      parent.remove(this)

    for (group: ThreadGroup <- groupsCopy.toList)
      group.nonsecureDestroy()
  }

  private def nonsecureInterrupt(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads.toList)
        thread.interrupt()
      for (group: ThreadGroup <- groups.toList)
        group.nonsecureInterrupt
    }
  }

  private def nonsecureResume(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads.toList)
        thread.resume()
      for (group: ThreadGroup <- groups.toList)
        group.nonsecureResume
    }
  }

  private def nonsecureSetMaxPriority(priority: Int): Unit = {
    lock.safeSynchronized {
      this.maxPriority = priority

      for (group: ThreadGroup <- groups.toList)
        group.nonsecureSetMaxPriority(priority)
    }
  }

  private def nonsecureStop(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads.toList) thread.stop()
      for (group: ThreadGroup  <- groups.toList)
        group.nonsecureStop
    }
  }

  private def nonsecureSuspend(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads.toList) thread.suspend()
      for (group: ThreadGroup  <- groups.toList)
        group.nonsecureSuspend
    }
  }

  private def remove(group: ThreadGroup): Unit = {
    lock.safeSynchronized {
      groups.remove(group)
      if (daemon && threads.isEmpty && groups.isEmpty) {
        // destroy this group
        if (parent != null) {
          parent.remove(this)
          destroyed = true
        }
      }
    }
  }

  def nonDaemonThreadExists: scala.Boolean = {
    lock.safeSynchronized {
      val threadIt = threads.iterator()
      var exists   = false
      while (!exists && threadIt.hasNext) {
        exists |= !threadIt.next().isDaemon
      }
      val groupIt = groups.iterator()
      while (!exists && groupIt.hasNext) {
        exists |= groupIt.next().nonDaemonThreadExists
      }
      exists
    }
  }

}

object ThreadGroup {

  // Indent used to print information about thread group
  private final val LISTING_INDENT = "    "

  // ThreadGroup lock object
  private final class ThreadGroupLock extends ShadowLock
  private final val lock: ThreadGroupLock = new ThreadGroupLock
}
