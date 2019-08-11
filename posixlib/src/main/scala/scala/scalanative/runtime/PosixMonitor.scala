package scala.scalanative.runtime

import scala.scalanative.posix.errno.{EBUSY, EPERM}
import scala.scalanative.posix.pthread._
import scala.scalanative.posix.sys.time.{gettimeofday, timeval}
import scala.scalanative.posix.sys.types.{pthread_cond_t, pthread_condattr_t, pthread_mutex_t, pthread_mutexattr_t}
import scala.scalanative.posix.time.timespec
import scala.scalanative.runtime.ThreadBase._
import scala.scalanative.runtime.libc.malloc
import scala.scalanative.unsafe.{Ptr, stackalloc}

final class PosixMonitor private[runtime] (shadow: Boolean) extends Monitor {
  // memory leak
  // TODO destroy the mutex and release the memory
  private val mutexPtr = fromRawPtr[pthread_mutex_t](malloc(pthread_mutex_t_size))
  pthread_mutex_init(mutexPtr, PosixMonitor.mutexAttrPtr)
  // memory leak
  // TODO destroy the condition and release the memory
  private val condPtr = fromRawPtr[pthread_cond_t](malloc(pthread_cond_t_size))
  pthread_cond_init(condPtr, PosixMonitor.condAttrPtr)

  def _notify(): Unit    = pthread_cond_signal(condPtr)
  def _notifyAll(): Unit = pthread_cond_broadcast(condPtr)
  def _wait(): Unit = {
    val thread = ThreadBase.currentThreadInternal()
    if (thread != null) {
      thread.setLockState(Waiting)
    }
    val returnVal = pthread_cond_wait(condPtr, mutexPtr)
    if (thread != null) {
      thread.setLockState(Normal)
    }
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
  }
  def _wait(millis: scala.Long, nanos: Int): Unit = {
    val thread = ThreadBase.currentThreadInternal()
    if (thread != null) {
      thread.setLockState(TimedWaiting)
    }
    val ts = stackalloc[timespec]
    val tv = stackalloc[timeval]
    gettimeofday(tv, null)
    val curSeconds     = tv._1
    val curNanos       = tv._2
    val overflownNanos = curNanos + nanos + (millis % 1000) * 1000000

    val deadlineNanos   = overflownNanos % 1000000000
    val deadLineSeconds = curSeconds + millis / 1000 + overflownNanos / 1000000000

    ts._1 = deadLineSeconds
    ts._2 = deadlineNanos

    val returnVal = pthread_cond_timedwait(condPtr, mutexPtr, ts)
    if (thread != null) {
      thread.setLockState(Normal)
    }
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
  }
  def enter(): Unit = {
    if (pthread_mutex_trylock(mutexPtr) == EBUSY) {
      val thread = ThreadBase.currentThreadInternal()
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
    val thread = ThreadBase.currentThreadInternal()
    if (thread != null) {
      thread.locks(thread.size) = this
      thread.size += 1
      if (thread.size >= thread.locks.length) {
        val oldArray = thread.locks
        val newArray = new scala.Array[Monitor](oldArray.length * 2)
        System.arraycopy(oldArray, 0, newArray, 0, oldArray.length)
        thread.locks = newArray
      }
    }
  }

  @inline
  private def popLock(): Unit = {
    val thread = ThreadBase.currentThreadInternal()
    if (thread != null) {
      if (thread.locks(thread.size - 1) == this) {
        thread.size -= 1
      }
    }
  }
}

object PosixMonitor {
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
  pthread_mutex_init(monitorCreationMutexPtr, PosixMonitor.mutexAttrPtr)

  private def unsafeCreate(x: Object): Monitor = new PosixMonitor(x.isInstanceOf[ShadowLock])
}