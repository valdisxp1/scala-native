package tests

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

  def hammer(numThreads: Int = 2, label: String = "")(f: => Unit) = {
    val runnable = new Runnable {
      def run() = f
    }
    val threads = Seq.tabulate(numThreads) { id =>
      new Thread(runnable, "Hammer " + label + "-" + id)
    }
    threads.foreach(_.start())
    threads.foreach(_.join())
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

  def testWithMinDelay(delays: Seq[scala.Long] =
                         Seq(50, 100, 200, 500, 1000, 2000, 5000))(
      counterexample: scala.Long => scala.Boolean)(
      test: scala.Long => scala.Boolean) = {
    delays.find(counterexample) match {
      case Some(minDelay) =>
        Console.out.println(s"Found min delay: $minDelay")
        val delay = minDelay * 2
        Console.out.println(s"Using min delay*2 = $delay for the test")
        assert(test(delay))
      case None =>
        Console.out.println(
          s"Could not find delay, it may be larger than: ${delays.max}")
        assert(false)
    }
  }

  def testWithMinRepetitions(
      repetitions: Seq[scala.Int] =
        Seq(1000, 2000, 5000, 10000, 100000, 1000000, 10000000))(
      counterexample: scala.Int => scala.Boolean)(
      test: scala.Int => scala.Boolean) = {
    repetitions.find(counterexample) match {
      case Some(minDelay) =>
        Console.out.println(s"Found min repetitions: $minDelay")
        val repetitions = minDelay * 2
        Console.out.println(
          s"Using min repetitions*2 = $repetitions for the test")
        assert(test(repetitions))
      case None =>
        Console.out.println(
          s"Could not find necessary repetitions, it may be larger than: ${repetitions.max}")
        assert(false)
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

  class WaitingThread(mutex: AnyRef,
                      threadGroup: ThreadGroup =
                        Thread.currentThread().getThreadGroup,
                      name: String = "WaitingThread")
      extends Thread(threadGroup, name) {
    private var notified = false

    def timesNotified = if (notified) 1 else 0

    override def run(): Unit = {
      mutex.synchronized {
        mutex.wait()
      }
      notified = true
    }
  }

  class Counter(threadGroup: ThreadGroup, name: String)
      extends Thread(threadGroup, name) {
    def this() = this(Thread.currentThread().getThreadGroup, "Counter")
    var count = 0L
    var goOn  = true
    override def run() = {
      while (goOn) {
        count += 1
        Thread.`yield`()
        Thread.sleep(100)
      }
    }
  }
}
