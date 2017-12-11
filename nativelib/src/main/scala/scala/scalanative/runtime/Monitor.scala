package scala.scalanative.runtime

import java.util

import scala.scalanative.native._
import scala.scalanative.native.stdlib.malloc
import scala.scalanative.posix.errno.{EBUSY, EPERM}
import scala.scalanative.posix.pthread._
import scala.scalanative.posix.sys.time.{
  CLOCK_REALTIME,
  clock_gettime,
  timespec
}
import scala.scalanative.posix.sys.types.{
  pthread_cond_t,
  pthread_condattr_t,
  pthread_mutex_t,
  pthread_mutexattr_t
}
import scala.scalanative.runtime.ThreadBase._

final class Monitor private[runtime] (shadow: Boolean) {
  // memory leak
  // TODO destroy the mutex and release the memory
  private val mutexPtr: Ptr[pthread_mutex_t] = {
    val result = malloc(pthread_mutex_t_size)
      .asInstanceOf[Ptr[pthread_mutex_t]]

    {
      import scala.scalanative.native.stdio._
      import scala.scalanative.runtime._
      fprintf(stderr,c"WHAT? %ld -> %ld\n", pthread_self() , result)
    }
    result
  }
  pthread_mutex_init(mutexPtr, Monitor.mutexAttrPtr)
  // memory leak
  // TODO destroy the condition and release the memory
  private val condPtr: Ptr[pthread_cond_t] = malloc(pthread_cond_t_size)
    .asInstanceOf[Ptr[pthread_cond_t]]
  pthread_cond_init(condPtr, Monitor.condAttrPtr)

  def _notify(): Unit    = pthread_cond_signal(condPtr)
  def _notifyAll(): Unit = pthread_cond_broadcast(condPtr)
  def _wait(): Unit = {
    val thread = Thread.currentThread().asInstanceOf[ThreadBase]
    thread.setLockState(Waiting)
    val returnVal = pthread_cond_wait(condPtr, mutexPtr)
    thread.setLockState(Normal)
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
  }
  def _wait(millis: scala.Long): Unit = _wait(millis, 0)
  def _wait(millis: scala.Long, nanos: Int): Unit = {
    val thread = Thread.currentThread().asInstanceOf[ThreadBase]
    thread.setLockState(TimedWaiting)
    val tsPtr = stackalloc[timespec]
    clock_gettime(CLOCK_REALTIME, tsPtr)
    val curSeconds     = !tsPtr._1
    val curNanos       = !tsPtr._2
    val overflownNanos = curNanos + nanos + (millis % 1000) * 1000000

    val deadlineNanos   = overflownNanos % 1000000000
    val deadLineSeconds = curSeconds + millis / 1000 + overflownNanos / 1000000000

    !tsPtr._1 = deadLineSeconds
    !tsPtr._2 = deadlineNanos

    val returnVal = pthread_cond_timedwait(condPtr, mutexPtr, tsPtr)
    thread.setLockState(Normal)
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
  }
  def enter(): Unit = {
    if (this == null) {
      import scala.scalanative.native.stdio._
      import scala.scalanative.runtime._
      fprintf(stderr,c"NPE?@ %ld ", pthread_self())
      printf(c"NPE?@ %ld ", pthread_self())
    }
    if (mutexPtr == null) {
      import scala.scalanative.native.stdio._
      import scala.scalanative.runtime._
      fprintf(stderr,c"NPE@ %ld ", pthread_self())
      printf(c"NPE@ %ld ", pthread_self())
    }

    {
      import scala.scalanative.native.stdio._
      import scala.scalanative.runtime._
      fprintf(stderr,c"before trylock@ %ld ", pthread_self())
      printf(c"before trylock@ %ld ", pthread_self())
    }

    if (pthread_mutex_trylock(mutexPtr) == EBUSY) {
      {
        import scala.scalanative.native.stdio._
        import scala.scalanative.runtime._
        fprintf(stderr,c"trylock@ %ld ", pthread_self())
        printf(c"trylock@ %ld ", pthread_self())
      }
      val thread = Thread.currentThread().asInstanceOf[ThreadBase]
      if (thread != null) {
        thread.setLockState(Blocked)
        // try again and block until you get one
        pthread_mutex_lock(mutexPtr)
        // finally got the lock
        thread.setLockState(Normal)
      } else {
        // Thread class in not initialized yet, just try again
        pthread_mutex_lock(mutexPtr)
      }
    }
    if (!shadow) {
      pushLock()
    }
  }

  def exit(): Unit = {
    if (!shadow) {
      popLock()
    }
    pthread_mutex_unlock(mutexPtr)
  }

  @inline
  private def pushLock(): Unit = {
    val thread = Thread.currentThread().asInstanceOf[ThreadBase]
    thread.locks(thread.size) = this
    thread.size += 1
    if (thread.size >= thread.locks.length) {
      val oldArray = thread.locks
      val newArray = new scala.Array[Monitor](oldArray.length * 2)
      System.arraycopy(oldArray, 0, newArray, 0, oldArray.length)
      thread.locks = newArray
    }
  }

  @inline
  private def popLock(): Unit = {
    Thread.currentThread().asInstanceOf[ThreadBase].size -= 1
  }
}

abstract class ThreadBase {
  def threadModuleBase: ThreadModuleBase
  def initMainThread(): Unit
  private var state                               = Normal
  final def getLockState: Int                     = state
  private[runtime] def setLockState(s: Int): Unit = state = s
  // only here to implement holdsLock
  private[runtime] var locks = new scala.Array[Monitor](8)
  private[runtime] var size  = 0
  final def holdsLock(obj: Object): scala.Boolean = {
    if (size == 0) {
      false
    } else {
      val target = Monitor(obj)
      var i: Int = 0
      while (i < size && locks(i) != target) {
        i += 1
      }
      i < size
    }
  }
  final def freeAllLocks(): Unit = {
    var i: Int = 0
    while (i < size) {
      locks(i).exit()
      i += 1
    }
  }
}

object ThreadBase {
  final val Normal       = 0
  final val Blocked      = 1
  final val Waiting      = 2
  final val TimedWaiting = 3
}

abstract class ThreadModuleBase {
  def shutdownCheckLoop(): Unit
  protected[runtime] def mainThreadEnds(): Unit
}

object Monitor {
  private val mutexAttrPtr: Ptr[pthread_mutexattr_t] = malloc(
    pthread_mutexattr_t_size).asInstanceOf[Ptr[pthread_mutexattr_t]]
  pthread_mutexattr_init(mutexAttrPtr)
  pthread_mutexattr_settype(mutexAttrPtr, PTHREAD_MUTEX_RECURSIVE)

  private val condAttrPtr: Ptr[pthread_condattr_t] = malloc(
    pthread_condattr_t_size).asInstanceOf[Ptr[pthread_cond_t]]
  pthread_condattr_init(condAttrPtr)
  pthread_condattr_setpshared(condAttrPtr, PTHREAD_PROCESS_SHARED)

  private[runtime] val monitorCreationMutexPtr: Ptr[pthread_mutex_t] = malloc(
    pthread_mutex_t_size)
    .asInstanceOf[Ptr[pthread_mutex_t]]
  pthread_mutex_init(monitorCreationMutexPtr, Monitor.mutexAttrPtr)

  def apply(x: java.lang.Object): Monitor = {
    val o = x.asInstanceOf[_Object]
    if (o.__monitor != null) {
      o.__monitor
    } else {
      try {
        pthread_mutex_lock(monitorCreationMutexPtr)
        if (o.__monitor == null) {
          o.__monitor = new Monitor(x.isInstanceOf[ShadowLock])
        }
        o.__monitor
      } finally {
        pthread_mutex_unlock(monitorCreationMutexPtr)
      }
    }
  }
}

/**
 * Cannot be checked with Thread.holdsLock
 */
class ShadowLock {
  // workaround so lock is freed when exception is thrown
  def safeSynchronized[T](f: => T): T = {
    var throwable: Throwable = null

    if (Monitor(this) == null) {
      import scala.scalanative.native.stdio._
      import scala.scalanative.runtime._
      fprintf(stderr,c"safeSynchronized %ld ", pthread_self())
      printf(c"safeSynchronized %ld ", pthread_self())
    }
    val result = synchronized {
      try {
        f
      } catch {
        case t: Throwable =>
          throwable = t
      }
    }
    if (throwable != null) {
      throw throwable
    } else {
      result.asInstanceOf[T]
    }

  }
}
