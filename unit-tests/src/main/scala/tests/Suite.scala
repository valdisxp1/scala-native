package tests

import sbt.testing.{EventHandler, Logger, Status}

import scala.collection.mutable
import scala.reflect.ClassTag

final case object AssertionFailed extends Exception

final case class Test(name: String, run: () => Boolean)

trait Suite {
  private val tests = new mutable.UnrolledBuffer[Test]

  def assert(cond: Boolean): Unit =
    if (!cond) throw AssertionFailed else ()

  def assertNot(cond: Boolean): Unit =
    if (cond) throw AssertionFailed else ()

  def assertThrowsAnd[T: ClassTag](f: => Unit)(fe: T => Boolean): Unit = {
    try {
      f
    } catch {
      case exc: Throwable =>
        if (exc.getClass.equals(implicitly[ClassTag[T]].runtimeClass) &&
            fe(exc.asInstanceOf[T]))
          return
        else
          throw AssertionFailed
    }
    throw AssertionFailed
  }

  def assertThrows[T: ClassTag](f: => Unit): Unit =
    assertThrowsAnd[T](f)(_ => true)

  def assertEquals[T](left: T, right: T): Unit =
    assert(left == right)

  private def assertThrowsImpl(cls: Class[_], f: => Unit): Unit = {
    try {
      f
    } catch {
      case exc: Throwable =>
        if (exc.getClass.equals(cls))
          return
        else
          throw AssertionFailed
    }
    throw AssertionFailed
  }

  def expectThrows[T <: Throwable, U](expectedThrowable: Class[T],
                                      code: => U): Unit =
    assertThrowsImpl(expectedThrowable, code)

  def test(name: String)(body: => Unit): Unit =
    tests += Test(name, { () =>
      try {
        body
        true
      } catch {
        case _: Throwable => false
      }
    })

  def testFails(name: String, issue: Int)(body: => Unit): Unit =
    tests += Test(name, { () =>
      try {
        body
        false
      } catch {
        case _: Throwable => true
      }
    })

  def run(eventHandler: EventHandler, loggers: Array[Logger]): Boolean = {
    val className = this.getClass.getName
    loggers.foreach(_.info("* " + className))
    var success = true

    tests.foreach { test =>
      val testSuccess = test.run()
      val (status, statusStr, color) =
        if (testSuccess) (Status.Success, "  [ok] ", Console.GREEN)
        else (Status.Failure, "  [fail] ", Console.RED)
      val event = NativeEvent(className, test.name, NativeFingerprint, status)
      loggers.foreach(_.info(color + statusStr + test.name + Console.RESET))
      eventHandler.handle(event)
      success = success && testSuccess

    }

    success
  }
}

trait MultiThreadSuite extends Suite {
  class FatObject(val id: Int = 0) {
    var x1, x2, x3, x4, x5, x6, x7, x8 = 0L

    def nextOne = new FatObject(id + 1)
  }

  class MemoryMuncher(times: Int) extends Thread {
    var visibleState = new FatObject()

    override def run(): Unit = {
      var remainingCount = times
      while (remainingCount > 0) {
        visibleState = visibleState.nextOne
        remainingCount -= 1
      }
    }
  }

  def takesAtLeast[R](expectedDelayMs: scala.Long)(f: => R): R = {
    val start  = System.currentTimeMillis()
    val result = f
    val end    = System.currentTimeMillis()
    val actual = end - start
    Console.out.println(
      "It took " + actual + " ms, expected at least " + expectedDelayMs + " ms")
    assert(actual >= expectedDelayMs)

    result
  }

  def takesAtLeast[R](expectedDelayMs: scala.Long,
                      expectedDelayNanos: scala.Int)(f: => R): R = {
    val expectedDelay = expectedDelayMs * 1000000 + expectedDelayMs
    val start         = System.nanoTime()
    val result        = f
    val end           = System.nanoTime()

    val actual = end - start
    Console.out.println(
      "It took " + actual + " ns, expected at least " + expectedDelay + " ns")
    assert(actual >= expectedDelay)

    result
  }

  val eternity = 300000 //ms
  def eventually(maxDelay: scala.Long = eternity,
                 recheckEvery: scala.Long = 200,
                 label: String = "Condition")(p: => scala.Boolean): Unit = {
    val start    = System.currentTimeMillis()
    val deadline = start + maxDelay
    var current  = 0L
    var continue = true
    while (continue && current <= deadline) {
      current = System.currentTimeMillis()
      if (p) {
        continue = false
      }
      Thread.sleep(recheckEvery)
    }
    if (current <= deadline) {
      // all is good
      Console.out.println(
        label + " reached after " + (current - start) + " ms; max delay: " + maxDelay + " ms")
      assert(true)
    } else {
      Console.out.println(
        "Timeout: " + label + " not reached after " + maxDelay + " ms")
      assert(false)
    }
  }

  def eventuallyEquals[T](
      maxDelay: scala.Long = eternity,
      recheckEvery: scala.Long = 200,
      label: String = "Equal values")(left: => T, right: => T): Unit =
    eventually(maxDelay, recheckEvery, label)(left == right)

  def eventuallyConstant[T](maxDelay: scala.Long = eternity,
                            recheckEvery: scala.Long = 200,
                            minDuration: scala.Long = 1000,
                            label: String = "Value")(value: => T): Option[T] = {
    val start        = System.currentTimeMillis()
    val deadline     = start + maxDelay + minDuration
    var current      = 0L
    var continue     = true
    var reached      = false
    var lastValue: T = value
    var lastValueTs  = start
    while (continue && current <= deadline) {
      current = System.currentTimeMillis()
      val currentValue = value
      if (currentValue == value) {
        if (current >= lastValueTs + minDuration) {
          continue = false
          reached = true
        }
      } else {
        lastValueTs = current
        lastValue = currentValue
      }
      Thread.sleep(recheckEvery)
    }
    if (reached) {
      // all is good
      Console.out.println(
        label + " remained constant after " + (lastValueTs - start) + " ms for at least " + minDuration + "ms ; max delay: " + maxDelay + " ms")
      assert(true)
      Some(lastValue)
    } else {
      Console.out.println(
        "Timeout: " + label + " not remained constant after " + maxDelay + " ms")
      assert(false)
      None
    }
  }

  def withExceptionHandler[U](handler: Thread.UncaughtExceptionHandler)(
      f: => U): U = {
    val oldHandler = Thread.getDefaultUncaughtExceptionHandler
    Thread.setDefaultUncaughtExceptionHandler(handler)
    try {
      f
    } finally {
      Thread.setDefaultUncaughtExceptionHandler(oldHandler)
    }
  }

  class ExceptionDetector(thread: Thread, exception: Throwable)
      extends Thread.UncaughtExceptionHandler {
    private var _wasException       = false
    def wasException: scala.Boolean = _wasException
    def uncaughtException(t: Thread, e: Throwable): Unit = {
      assertEquals(t, thread)
      assertEquals(e, exception)
      _wasException = true
    }
  }

  class Counter(threadGroup: ThreadGroup, name: String) extends Thread {
    def this() = this(Thread.currentThread().getThreadGroup, "Counter")
    var count = 0L
    var goOn  = true
    override def run() = {
      while (goOn) {
        count += 1
        Thread.sleep(10)
      }
    }
  }
}
