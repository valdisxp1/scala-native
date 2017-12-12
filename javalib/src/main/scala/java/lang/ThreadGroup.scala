package java.lang

import java.util
import java.lang.Thread.UncaughtExceptionHandler

import scala.collection.JavaConversions._
import scala.scalanative.runtime.ShadowLock
import scala.collection.immutable._

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
  private var groups: List[ThreadGroup] = Nil

  // All threads in the group
  private var threads: List[Thread] = Nil

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
    val (groupsCopy: List[ThreadGroup], threadsCopy: List[Thread]) =
      lock.safeSynchronized {
        if (destroyed) {
          Nil -> Nil
        } else {
          groups -> threads
        }
      }

    threadsCopy.count(_.isAlive) + groupsCopy.map(_.activeCount()).sum
  }

  def activeGroupCount(): Int = {
    val groupsCopy = groups
    if (destroyed) {
      0
    } else {
      groupsCopy.size + groupsCopy.map(_.activeGroupCount()).sum
    }
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
      threads ::= thread
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
      threads = threads.filter(_ != thread)
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
      groups ::= group
    }
  }

  private[lang] def enumerateThreads(list: Array[Thread],
                                     of: Int,
                                     recurse: scala.Boolean): Int = {
    var offset: Int = of
    if (list.isEmpty) return 0
    var groupsCopy: List[ThreadGroup] = null // a copy of subgroups list
    var threadsCopy: List[Thread]     = null // a copy of threads list
    lock.safeSynchronized {
      if (destroyed)
        return offset
      threadsCopy = threads
      if (recurse)
        groupsCopy = groups
    }
    for (thread: Thread <- threadsCopy) {
      if (thread.isAlive) {
        list(offset) = thread.asInstanceOf[Thread]
        offset += 1
        if (offset == list.length) return offset
      }
    }
    if (recurse) {
      val it = groupsCopy.iterator()
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
      for (group: ThreadGroup <- groups) {
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
    var prefix1: String = pr
    println(prefix1 + toString)
    val prefix2 = pr + LISTING_INDENT
    val (groupsCopy: List[ThreadGroup], threadsCopy: List[Thread]) =
      lock.safeSynchronized {
          groups -> threads
      }
    for (thread: Thread <- threadsCopy)
      println(prefix2 + thread)
    for (group: ThreadGroup <- groupsCopy)
      group.list(prefix2)
  }

  def nonsecureDestroy(): Unit = {
    val groupsCopy: List[ThreadGroup] = lock.safeSynchronized {
      if (threads.size > 0)
        throw new IllegalThreadStateException(
          "The thread group " + name + "is not empty")
      destroyed = true
      groups
    }

    if (parent != null)
      parent.remove(this)

    for (group: ThreadGroup <- groupsCopy)
      group.nonsecureDestroy()
  }

  private def nonsecureInterrupt(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads)
        thread.interrupt()
      for (group: ThreadGroup <- groups)
        group.nonsecureInterrupt
    }
  }

  private def nonsecureResume(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads)
        thread.resume()
      for (group: ThreadGroup <- groups)
        group.nonsecureResume
    }
  }

  private def nonsecureSetMaxPriority(priority: Int): Unit = {
    lock.safeSynchronized {
      this.maxPriority = priority

      for (group: ThreadGroup <- groups)
        group.nonsecureSetMaxPriority(priority)
    }
  }

  private def nonsecureStop(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads) thread.stop()
      for (group: ThreadGroup  <- groups)
        group.nonsecureStop
    }
  }

  private def nonsecureSuspend(): Unit = {
    lock.safeSynchronized {
      for (thread: Thread <- threads) thread.suspend()
      for (group: ThreadGroup  <- groups)
        group.nonsecureSuspend
    }
  }

  private def remove(group: ThreadGroup): Unit = {
    lock.safeSynchronized {
      groups = groups.filter(_ != group)
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
      threads.exists(t => !t.isDaemon) || groups.exists(_.nonDaemonThreadExists)
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
