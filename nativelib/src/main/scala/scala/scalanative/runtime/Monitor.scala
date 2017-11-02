package scala.scalanative.runtime

import scala.scalanative.posix.pthread._
import scala.scalanative.posix.errno.{EBUSY, EPERM}
import scala.scalanative.posix.sys.types.{
  pthread_cond_t,
  pthread_condattr_t,
  pthread_mutex_t,
  pthread_mutexattr_t
}
import scala.scalanative.native._
import scala.scalanative.native.stdlib.malloc
import scala.scalanative.posix.sys.time.{
  timespec,
  CLOCK_REALTIME,
  clock_gettime
}
final class Monitor private () {

  private val mutexPtr: Ptr[pthread_mutex_t] = malloc(pthread_mutex_t_size)
    .asInstanceOf[Ptr[pthread_mutex_t]]
  pthread_mutex_init(mutexPtr, Monitor.mutexAttrPtr)
  private val condPtr: Ptr[pthread_cond_t] = malloc(pthread_cond_t_size)
    .asInstanceOf[Ptr[pthread_cond_t]]
  pthread_cond_init(condPtr, Monitor.condAttrPtr)

  def _notify(): Unit    = pthread_cond_signal(condPtr)
  def _notifyAll(): Unit = pthread_cond_broadcast(condPtr)
  def _wait(): Unit = {
    val returnVal = pthread_cond_wait(condPtr, mutexPtr)
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
  }
  def _wait(millis: scala.Long): Unit = _wait(millis, 0)
  def _wait(millis: scala.Long, nanos: Int): Unit = {
    val tsPtr = stackalloc[timespec]
    clock_gettime(CLOCK_REALTIME, tsPtr)
    val curSeconds = !tsPtr._1
    val curNanos   = !tsPtr._2
    println("start:    " + curSeconds + "." + curNanos)
    val overflownNanos = curNanos + nanos + (millis % 1000) * 1000000

    val deadlineNanos   = overflownNanos % 1000000000
    val deadLineSeconds = curSeconds + overflownNanos / 1000000000

    !tsPtr._1 = deadLineSeconds
    !tsPtr._2 = deadlineNanos

    println("deadline: " + !tsPtr._1 + "." + !tsPtr._2)

    val returnVal = pthread_cond_timedwait(condPtr, mutexPtr, tsPtr)
    if (returnVal == EPERM) {
      throw new IllegalMonitorStateException()
    }
    clock_gettime(CLOCK_REALTIME, tsPtr)
    println("time:    " + !tsPtr._1 + "." + !tsPtr._2)
    println("!!!" + returnVal)
  }
  def enter(): Unit = {
    if (pthread_mutex_trylock(mutexPtr) == EBUSY) {
      val thread = Thread.currentThread().asInstanceOf[ThreadBase]
      if (thread != null) {
        thread.setBlocked(true)
        // try again and block until you get one
        pthread_mutex_lock(mutexPtr)
        // finally got the lock
        thread.setBlocked(false)
      } else {
        // Thread class in not initialized yet, just try again
        pthread_mutex_lock(mutexPtr)
      }
    }
  }
  def exit(): Unit = pthread_mutex_unlock(mutexPtr)
}

abstract class ThreadBase {
  private var blocked                               = false
  final protected def isBlocked                     = blocked
  private[runtime] def setBlocked(b: Boolean): Unit = blocked = b
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

  val global: Monitor = new Monitor()
}
