package scala.scalanative.runtime

abstract class Monitor {
  def _notify(): Unit
  def _notifyAll(): Unit
  def _wait(): Unit
  def _wait(millis: scala.Long): Unit = _wait(millis, 0)
  def _wait(millis: scala.Long, nanos: Int): Unit
  def enter(): Unit
  def exit(): Unit
}

object Monitor {
  private val globalMonitor: Monitor = unsafeCreate(new ShadowLock())

  def apply(x: java.lang.Object): Monitor = {
    val o = x.asInstanceOf[_Object]
    if (o.__monitor != null) {
      o.__monitor
    } else {
      try {
        globalMonitor.enter()
        if (o.__monitor == null) {
          o.__monitor = unsafeCreate(x)
        }
        o.__monitor
      } finally {
        globalMonitor.exit()
      }
    }
  }

  //TODO magically insert the code from `PosixMonitor.unsafeCreate`
  private def unsafeCreate(x: Object): Monitor = ???
}
