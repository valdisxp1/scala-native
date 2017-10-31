package scala.scalanative.runtime

import scala.scalanative.posix.pthread._
import scala.scalanative.posix.sys.types.{pthread_mutex_t, pthread_mutexattr_t}
import scala.scalanative.native._
import scala.scalanative.native.stdlib.malloc
final class Monitor private () {

  private val mutexPtr: Ptr[pthread_mutex_t] = malloc(pthread_mutex_t_size).asInstanceOf[Ptr[pthread_mutex_t]]
  pthread_mutex_init(mutexPtr, Monitor.mutexAttrPtr)

  def _notify(): Unit                              = ()
  def _notifyAll(): Unit                           = ()
  def _wait(): Unit                                = ()
  def _wait(timeout: scala.Long): Unit             = ()
  def _wait(timeout: scala.Long, nanos: Int): Unit = ()
  def enter(): Unit                                = pthread_mutex_lock(mutexPtr)
  def exit(): Unit                                 = pthread_mutex_unlock(mutexPtr)
}

object Monitor {
  private val mutexAttrPtr: Ptr[pthread_mutexattr_t] = malloc(pthread_mutexattr_t_size).asInstanceOf[Ptr[pthread_mutexattr_t]]
  pthread_mutexattr_init(mutexAttrPtr)
  pthread_mutexattr_settype(mutexAttrPtr, PTHREAD_MUTEX_RECURSIVE)

  val global: Monitor = new Monitor()
}
