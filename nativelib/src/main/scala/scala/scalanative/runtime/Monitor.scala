package scala.scalanative.runtime

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

final class Monitor private[runtime] (debug: Boolean, owner: Object) {

  private val mutexPtr: Ptr[pthread_mutex_t] = malloc(pthread_mutex_t_size)
    .asInstanceOf[Ptr[pthread_mutex_t]]
  pthread_mutex_init(mutexPtr, Monitor.mutexAttrPtr)
  private val condPtr: Ptr[pthread_cond_t] = malloc(pthread_cond_t_size)
    .asInstanceOf[Ptr[pthread_cond_t]]
  pthread_cond_init(condPtr, Monitor.condAttrPtr)

  def _notify(): Unit    = {
    if(debug) {
      Console.out.println("NOTIFY!!!   " + System.identityHashCode(owner) + ":" + owner)
    }
    pthread_cond_signal(condPtr)
  }
  def _notifyAll(): Unit = {
    if(debug) {
      Console.out.println("NOTIFYALL!!!" + System.identityHashCode(owner) + ":" + owner)
    }
    pthread_cond_broadcast(condPtr)
  }
  def _wait(): Unit = {
    val thread = Thread.currentThread().asInstanceOf[ThreadBase]
    thread.setState(Waiting)
    val returnVal = pthread_cond_wait(condPtr, mutexPtr)
    thread.setState(Normal)
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
  }
  def _wait(millis: scala.Long): Unit = _wait(millis, 0)
  def _wait(millis: scala.Long, nanos: Int): Unit = {
    val thread = Thread.currentThread().asInstanceOf[ThreadBase]
    val tsPtr  = stackalloc[timespec]
    clock_gettime(CLOCK_REALTIME, tsPtr)
    val curSeconds     = !tsPtr._1
    val curNanos       = !tsPtr._2
    val overflownNanos = curNanos + nanos + (millis % 1000) * 1000000

    val deadlineNanos   = overflownNanos % 1000000000
    val deadLineSeconds = curSeconds + overflownNanos / 1000000000

    !tsPtr._1 = deadLineSeconds
    !tsPtr._2 = deadlineNanos

    thread.setState(TimedWaiting)
    val returnVal = pthread_cond_timedwait(condPtr, mutexPtr, tsPtr)
    thread.setState(Normal)
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
  }
  def enter(): Unit = {
    if (pthread_mutex_trylock(mutexPtr) == EBUSY) {
      val thread = Thread.currentThread().asInstanceOf[ThreadBase]
      if (thread != null) {
        thread.setState(Blocked)
        // try again and block until you get one
        pthread_mutex_lock(mutexPtr)
        // finally got the lock
        thread.setState(Normal)
      } else {
        // Thread class in not initialized yet, just try again
        pthread_mutex_lock(mutexPtr)
      }
    }
  }
  def exit(): Unit = pthread_mutex_unlock(mutexPtr)
}

abstract class ThreadBase {
  private var state                           = Normal
  final protected def getLockState: Int       = state
  private[runtime] def setState(s: Int): Unit = state = s
}

object ThreadBase {
  final val Normal       = 0
  final val Blocked      = 1
  final val Waiting      = 2
  final val TimedWaiting = 3
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
        o.__monitor = new Monitor(!x.isInstanceOf[Thread] && !x.isInstanceOf[BoringLock],owner = x)
        o.__monitor
      } finally {
        pthread_mutex_unlock(monitorCreationMutexPtr)
      }
    }
  }
}

class BoringLock{}