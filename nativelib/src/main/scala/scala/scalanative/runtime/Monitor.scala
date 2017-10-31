package scala.scalanative.runtime

final class Monitor private[runtime] () {
  import scala.scalanative.posix.pthread._
  import scala.scalanative.posix.sys.types.pthread_mutex_t
  import scala.scalanative.native._
  import scala.scalanative.native.stdlib.malloc

  private val mutexPtr: Ptr[pthread_mutex_t] = malloc(pthread_mutex_t_size).asInstanceOf[Ptr[pthread_mutex_t]]
  PTHREAD_MUTEX_INITIALIZER(mutexPtr)

  def _notify(): Unit                              = ()
  def _notifyAll(): Unit                           = ()
  def _wait(): Unit                                = ()
  def _wait(timeout: scala.Long): Unit             = ()
  def _wait(timeout: scala.Long, nanos: Int): Unit = ()
  def enter(): Unit                                = pthread_mutex_lock(mutexPtr)
  def exit(): Unit                                 = pthread_mutex_unlock(mutexPtr)
}

object Monitor {
  val global: Monitor = new Monitor()
}
