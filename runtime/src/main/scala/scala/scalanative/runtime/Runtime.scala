package scala.scalanative
package runtime

import scala.scalanative.runtime.ExecutionContext.QueueExecutionContext
import scalanative.annotation.alwaysinline
import scalanative.unsafe._

object RuntimeEnviromenment {
  /** Get monitor for given object. */
  @alwaysinline def getMonitor(obj: Object): PosixMonitor = PosixMonitor(obj)

  /** Initialize runtime with given arguments and return the
   *  rest as Java-style array.
   */
  def init(argc: Int, rawargv: RawPtr): scala.Array[String] = {
    val argv = fromRawPtr[CString](rawargv)
    val args = new scala.Array[String](argc - 1)

    // force Thread class initialization
    // to make sure it is initialized from the main thread
    ThreadBase.initMainThread()
    // make sure Cached stackTrace is initialized
    new Throwable().getStackTrace

    // skip the executable name in argv(0)
    var c = 0
    while (c < argc - 1) {
      // use the default Charset (UTF_8 atm)
      args(c) = fromCString(argv(c + 1))
      c += 1
    }

    args
  }

  /** Run the runtime's event loop. The method is called from the
   *  generated C-style after the application's main method terminates.
   */
  def loop(): Unit = {
    new Thread("EventLoop") {
      override def run() =
        ExecutionContext.global
          .asInstanceOf[QueueExecutionContext]
          .waitUntilDone()
    }.start()
    ThreadBase.mainThreadEnds()
    ThreadBase.shutdownCheckLoop()
  }
}