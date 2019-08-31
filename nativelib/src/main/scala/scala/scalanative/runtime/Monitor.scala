package scala.scalanative.runtime

abstract class Monitor {
  def _notify(): Unit = ???
  def _notifyAll(): Unit = ???
  def _wait(): Unit = ???
  def _wait(millis: scala.Long): Unit = _wait(millis, 0)
  def _wait(millis: scala.Long, nanos: Int): Unit = ???
  def enter(): Unit = ???
  def exit(): Unit = ???
}