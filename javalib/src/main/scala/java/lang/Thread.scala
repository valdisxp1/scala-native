package java.lang

import java.util
import java.lang.Thread._
import java.lang.Thread.State

import scala.scalanative.runtime.{CAtomicInt, NativeThread, ThreadBase}
import scala.scalanative.native.{
  CFunctionPtr,
  CInt,
  Ptr,
  ULong,
  sizeof,
  stackalloc
}
import scala.scalanative.native.stdlib.{free, malloc}
import scala.scalanative.posix.sys.types.{
  pthread_attr_t,
  pthread_key_t,
  pthread_t
}
import scala.scalanative.posix.pthread._
import scala.scalanative.posix.sched._

// Ported from Harmony

class Thread private (
    parentThread: Thread, // only the main thread does not have a parent (= null)
    rawGroup: ThreadGroup,
    // Thread's target - a Runnable object whose run method should be invoked
    private val target: Runnable,
    rawName: String,
    // Stack size to be passes to VM for thread execution
    val stackSize: scala.Long,
    mainThread: scala.Boolean)
    extends ThreadBase
    with Runnable {

  private var livenessState = CAtomicInt(internalNew)

  // Thread's ID
  private val threadId: scala.Long = getNextThreadId

  // Thread's name
  // throws NullPointerException if the given name is null
  private[this] var name: String =
    if (rawName != THREAD) rawName.toString else THREAD + threadId

  // This thread's thread group
  private[lang] var group: ThreadGroup =
    if (rawGroup != null) rawGroup else parentThread.group
  group.checkGroup()

  // This thread's context class loader
  private var contextClassLoader: ClassLoader =
    if (!mainThread) parentThread.contextClassLoader else null

  // Indicates whether this thread was marked as daemon
  private var daemon: scala.Boolean =
    if (!mainThread) parentThread.daemon else false

  // Thread's priority
  private var priority: Int = if (!mainThread) parentThread.priority else 5

  // Indicates if the thread was already started
  def started: scala.Boolean = {
    val value = livenessState.load()
    value > internalNew
  }

  // Uncaught exception handler for this thread
  private var exceptionHandler: Thread.UncaughtExceptionHandler = _

  // The underlying pthread ID
  /*
   * NOTE: This is used to keep track of the pthread linked to this Thread,
   * it might be easier/better to handle this at lower level
   */
  private[this] var underlying: pthread_t = 0.asInstanceOf[ULong]

  private val sleepMutex = new Object

  // Synchronization is done using internal lock
  val lock: Object = new Object
  // ThreadLocal values : local and inheritable
  var localValues: ThreadLocal.Values = _

  var inheritableValues: ThreadLocal.Values =
    if (parentThread != null && parentThread.inheritableValues != null) {
      new ThreadLocal.Values(parentThread.inheritableValues)
    } else null

  checkGCWatermark()
  checkAccess()

  def this(group: ThreadGroup,
           target: Runnable,
           name: String,
           stacksize: scala.Long) = {
    this(Thread.currentThread(),
         group,
         target,
         name,
         stacksize,
         mainThread = false)
  }

  def this() = this(null, null, THREAD, 0)

  def this(target: Runnable) = this(null, target, THREAD, 0)

  def this(target: Runnable, name: String) = this(null, target, name, 0)

  def this(name: String) = this(null, null, name, 0)

  def this(group: ThreadGroup, target: Runnable) =
    this(group, target, THREAD, 0)

  def this(group: ThreadGroup, target: Runnable, name: String) =
    this(group, target, name, 0)

  def this(group: ThreadGroup, name: String) = this(group, null, name, 0)

  final def checkAccess(): Unit = ()

  override final def clone(): AnyRef =
    throw new CloneNotSupportedException("Thread.clone() is not meaningful")

  @deprecated
  def countStackFrames: Int = 0 //deprecated

  @deprecated
  def destroy(): Unit =
    // this method is not implemented
    throw new NoSuchMethodError()

  def getContextClassLoader: ClassLoader =
    lock.synchronized(contextClassLoader)

  final def getName: String = name

  final def getPriority: Int = priority

  final def getThreadGroup: ThreadGroup = group

  def getId: scala.Long = threadId

  def interrupt(): Unit = {
    checkAccess()
    livenessState.compareAndSwapStrong(internalStarting, internalInterrupted)
    livenessState.compareAndSwapStrong(internalRunnable, internalInterrupted)
    sleepMutex.synchronized {
      sleepMutex.notify()
    }
  }

  var oldValue = -1

  final def isAlive: scala.Boolean = {
    val value = livenessState.load()
    value == internalRunnable || value == internalStarting
  }

  final def isDaemon: scala.Boolean = daemon

  def isInterrupted: scala.Boolean = {
    val value = livenessState.load()
    value == internalInterrupted || value == internalInterruptedTerminated
  }

  final def join(): Unit = while (isAlive) synchronized { wait() }

  final def join(ml: scala.Long): Unit = {
    var millis: scala.Long = ml
    if (millis == 0)
      join()
    else {
      val end: scala.Long         = System.currentTimeMillis() + millis
      var continue: scala.Boolean = true
      while (isAlive && continue) {
        synchronized {
          wait(millis)
        }
        millis = end - System.currentTimeMillis()
        if (millis <= 0)
          continue = false
      }
    }
  }

  final def join(ml: scala.Long, n: Int): Unit = {
    var nanos: Int         = n
    var millis: scala.Long = ml
    if (millis < 0 || nanos < 0 || nanos > 999999)
      throw new IllegalArgumentException()
    else if (millis == 0 && nanos == 0)
      join()
    else {
      val end: scala.Long         = System.nanoTime() + 1000000 * millis + nanos.toLong
      var rest: scala.Long        = 0L
      var continue: scala.Boolean = true
      while (isAlive && continue) {
        synchronized {
          wait(millis, nanos)
        }
        rest = end - System.nanoTime()
        if (rest <= 0)
          continue = false
        if (continue) {
          nanos = (rest % 1000000).toInt
          millis = rest / 1000000
        }
      }
    }
  }

  @deprecated
  final def resume(): Unit = {
    checkAccess()
    if (started && NativeThread.resume(underlying) != 0)
      throw new RuntimeException(
        "Error while trying to unpark thread " + toString)
  }

  def run(): Unit = {
    if (target != null) {
      target.run()
    }
  }

  def getStackTrace: Array[StackTraceElement] = new Array[StackTraceElement](0)

  def setContextClassLoader(classLoader: ClassLoader): Unit =
    lock.synchronized(contextClassLoader = classLoader)

  final def setDaemon(daemon: scala.Boolean): Unit = {
    lock.synchronized {
      checkAccess()
      if (isAlive)
        throw new IllegalThreadStateException()
      this.daemon = daemon
    }
  }

  final def setName(name: String): Unit = {
    checkAccess()
    // throws NullPointerException if the given name is null
    this.name = name.toString
  }

  final def setPriority(priority: Int): Unit = {
    checkAccess()
    if (priority > 10 || priority < 1)
      throw new IllegalArgumentException("Wrong Thread priority value")
    val threadGroup: ThreadGroup = group
    this.priority = priority
    if (started)
      NativeThread.setPriority(underlying, priority)
  }

  //synchronized
  def start(): Unit = {
    if (!livenessState.compareAndSwapStrong(internalNew, internalStarting)._1) {
      //this thread was started
      throw new IllegalThreadStateException("This thread was already started!")
    }
    // adding the thread to the thread group
    group.add(this)

    val threadPtr = malloc(sizeof[Thread]).asInstanceOf[Ptr[Thread]]
    !threadPtr = this

    val id = stackalloc[pthread_t]
    val status =
      pthread_create(id,
                     null.asInstanceOf[Ptr[pthread_attr_t]],
                     callRunRoutine,
                     threadPtr.asInstanceOf[Ptr[scala.Byte]])
    if (status != 0)
      throw new Exception(
        "Failed to create new thread, pthread error " + status)

    underlying = !id
  }

  def getState: State = {
    val value = livenessState.load()
    if (value == internalNew) {
      State.NEW
    } else if (value == internalStarting) {
      State.RUNNABLE
    } else if (value == internalRunnable) {
      val lockState = getLockState
      if (lockState == ThreadBase.Blocked) {
        State.BLOCKED
      } else if (lockState == ThreadBase.Waiting) {
        State.WAITING
      } else if (lockState == ThreadBase.TimedWaiting) {
        State.TIMED_WAITING
      } else {
        State.RUNNABLE
      }
    } else {
      State.TERMINATED
    }
  }

  @deprecated
  final def stop(): Unit = {
    lock.synchronized {
      if (isAlive)
        stop(new ThreadDeath())
    }
  }

  @deprecated
  final def stop(throwable: Throwable): Unit = {
    if (throwable == null)
      throw new NullPointerException("The argument is null!")
    lock.synchronized {
      if (isAlive) {
        val status: Int = pthread_cancel(underlying)
        if (status != 0)
          throw new InternalError("Pthread error " + status)
      }
    }
  }

  @deprecated
  final def suspend(): Unit = {
    checkAccess()
    if (started && NativeThread.suspend(underlying) != 0)
      throw new RuntimeException(
        "Error while trying to park thread " + toString)
  }

  override def toString: String = {
    val threadGroup: ThreadGroup = group
    val s: String                = if (threadGroup == null) "" else threadGroup.name
    "Thread[" + name + "," + priority + "," + s + "]"
  }

  private def checkGCWatermark(): Unit = {
    currentGCWatermarkCount += 1
    if (currentGCWatermarkCount % GC_WATERMARK_MAX_COUNT == 0)
      System.gc()
  }

  def getUncaughtExceptionHandler: Thread.UncaughtExceptionHandler = {
    if (exceptionHandler != null)
      return exceptionHandler
    getThreadGroup
  }

  def setUncaughtExceptionHandler(eh: Thread.UncaughtExceptionHandler): Unit =
    exceptionHandler = eh
}

object Thread {

  import scala.collection.mutable

  val myThreadKey: pthread_key_t = {
    val ptr = stackalloc[pthread_key_t]
    pthread_key_create(ptr, null)
    !ptr
  }

  // defined as Ptr[Void] => Ptr[Void]
  // called as Ptr[Thread] => Ptr[Void]
  private def callRun(p: Ptr[scala.Byte]): Ptr[scala.Byte] = {
    val thread = !p.asInstanceOf[Ptr[Thread]]
    pthread_setspecific(myThreadKey, p)
    free(p)
    thread.livenessState
      .compareAndSwapStrong(internalStarting, internalRunnable)
    try {
      thread.run()
    } catch {
      case e: Throwable =>
        thread.getUncaughtExceptionHandler.uncaughtException(thread, e)
    } finally {
      post(thread)
    }

    null.asInstanceOf[Ptr[scala.Byte]]
  }

  private def post(thread: Thread) = {
    thread.group.remove(thread)
    thread.livenessState
      .compareAndSwapStrong(internalRunnable, internalTerminated)
    thread.livenessState
      .compareAndSwapStrong(internalInterrupted, internalInterruptedTerminated)
    thread synchronized {
      thread.notifyAll()
    }
  }

  // internal liveness state values
  // waiting and blocked handled separately
  private final val internalNew                   = 0
  private final val internalStarting              = 1
  private final val internalRunnable              = 2
  private final val internalInterrupted           = 3
  private final val internalTerminated            = 4
  private final val internalInterruptedTerminated = 5

  final class State private (override val toString: String)

  object State {
    final val NEW           = new State("NEW")
    final val RUNNABLE      = new State("RUNNABLE")
    final val BLOCKED       = new State("BLOCKED")
    final val WAITING       = new State("WAITING")
    final val TIMED_WAITING = new State("TIMED_WAITING")
    final val TERMINATED    = new State("TERMINATED")
  }

  private val callRunRoutine = CFunctionPtr.fromFunction1(callRun)

  private val lock: Object = new Object

  final val MAX_PRIORITY: Int = NativeThread.THREAD_MAX_PRIORITY

  final val MIN_PRIORITY: Int = NativeThread.THREAD_MIN_PRIORITY

  final val NORM_PRIORITY: Int = NativeThread.THREAD_NORM_PRIORITY

  final val STACK_TRACE_INDENT: String = "    "

  // Default uncaught exception handler
  private var defaultExceptionHandler: UncaughtExceptionHandler = _

  // Counter used to generate thread's ID
  private var threadOrdinalNum: scala.Long = 0L

  // used to generate a default thread name
  private final val THREAD: String = "Thread-"

  // System thread group for keeping helper threads
  var systemThreadGroup: ThreadGroup = _

  // Number of threads that was created w/o garbage collection //TODO
  private var currentGCWatermarkCount: Int = 0

  // Max number of threads to be created w/o GC, required collect dead Thread references
  private final val GC_WATERMARK_MAX_COUNT: Int = 700

  def activeCount: Int = currentThread().group.activeCount()

  def currentThread(): Thread = {
    val ptr = pthread_getspecific(myThreadKey).asInstanceOf[Ptr[Thread]]
    if (ptr != null) !ptr else MainThread
  }

  def dumpStack(): Unit = {
    val stack: Array[StackTraceElement] = new Throwable().getStackTrace
    System.err.println("Stack trace")
    var i: Int = 0
    while (i < stack.length) {
      System.err.println(STACK_TRACE_INDENT + stack(i))
      i += 1
    }
  }

  def enumerate(list: Array[Thread]): Int =
    currentThread().group.enumerate(list)

  def holdsLock(obj: Object): scala.Boolean = ???

  def `yield`(): Unit = {
    sched_yield()
  }

  def getAllStackTraces: java.util.Map[Thread, Array[StackTraceElement]] = {
    var parent: ThreadGroup =
      new ThreadGroup(currentThread().getThreadGroup, "Temporary")
    var newParent: ThreadGroup = parent.getParent
    parent.destroy()
    while (newParent != null) {
      parent = newParent
      newParent = parent.getParent
    }
    var threadsCount: Int          = parent.activeCount() + 1
    var count: Int                 = 0
    var liveThreads: Array[Thread] = Array.empty
    var break: scala.Boolean       = false
    while (!break) {
      liveThreads = new Array[Thread](threadsCount)
      count = parent.enumerate(liveThreads)
      if (count == threadsCount) {
        threadsCount *= 2
      } else
        break = true
    }

    val map: java.util.Map[Thread, Array[StackTraceElement]] =
      new util.HashMap[Thread, Array[StackTraceElement]](count + 1)
    var i: Int = 0
    while (i < count) {
      val ste: Array[StackTraceElement] = liveThreads(i).getStackTrace
      if (ste.length != 0)
        map.put(liveThreads(i), ste)
      i += 1
    }

    map
  }

  def getDefaultUncaughtExceptionHandler: UncaughtExceptionHandler =
    defaultExceptionHandler

  def setDefaultUncaughtExceptionHandler(eh: UncaughtExceptionHandler): Unit =
    defaultExceptionHandler = eh

  //synchronized
  private def getNextThreadId: scala.Long = {
    threadOrdinalNum += 1
    threadOrdinalNum
  }

  def interrupted(): scala.Boolean = {
    currentThread().livenessState
      .compareAndSwapStrong(internalInterrupted, internalRunnable)
      ._1
  }

  def sleep(millis: scala.Long, nanos: scala.Int): Unit = {
    val sleepMutex: Object = currentThread().sleepMutex
    sleepMutex.synchronized {
      sleepMutex.wait(millis, nanos)
    }
    if (interrupted()) {
      throw new InterruptedException("Interrupted during sleep")
    }
  }

  def sleep(millis: scala.Long): Unit = sleep(millis, 0)

  trait UncaughtExceptionHandler {
    def uncaughtException(t: Thread, e: Throwable)
  }

  private val mainThreadGroup: ThreadGroup =
    new ThreadGroup(parent = null, name = "system", mainGroup = true)

  private val MainThread = new Thread(parentThread = null,
                                      mainThreadGroup,
                                      target = null,
                                      "main",
                                      0,
                                      mainThread = true)
}
