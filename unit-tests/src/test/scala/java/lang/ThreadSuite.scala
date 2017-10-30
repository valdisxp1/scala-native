package java.lang

object ThreadSuite extends tests.Suite {

  test("Runtime static variables access and currentThread do not crash") {

    val max  = Thread.MAX_PRIORITY
    val min  = Thread.MIN_PRIORITY
    val norm = Thread.NORM_PRIORITY

    val current = Thread.currentThread()

  }

  test("Get/Set Priority work as it should with currentThread") {

    val current = Thread.currentThread()

    current.setPriority(3)
    assert(current.getPriority == 3)

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

  test("sleep suspends execution by at least the requested amount") {
    val millisecondTests = Seq(0, 1, 5, 100)
    millisecondTests.foreach { ms =>
      takesAtLeast(ms) {
        Thread.sleep(ms)
      }
    }
    millisecondTests.foreach { ms =>
      takesAtLeast(ms) {
        Thread.sleep(ms, 0)
      }
    }

    val tests = Seq(0 -> 0,
                    0   -> 1,
                    0   -> 999999,
                    1   -> 0,
                    1   -> 1,
                    5   -> 0,
                    100 -> 0,
                    100 -> 50)

    tests.foreach {
      case (ms, nanos) =>
        takesAtLeast(ms, nanos) {
          Thread.sleep(ms, nanos)
        }
    }
  }

  test("Thread should be able to change a shared var") {
    var shared: Int = 0
    new Thread(new Runnable {
      def run(): Unit = {
        shared = 1
      }
    }).start()
    Thread.sleep(100)
    assertEquals(shared, 1)
  }

  test("Thread should be able to change its internal state") {
    class StatefulThread extends Thread {
      var internal = 0
      override def run() = {
        internal = 1
      }
    }
    val t = new StatefulThread
    t.start()
    Thread.sleep(100)
    assertEquals(t.internal, 1)
  }

  test("Thread should be able to change runnable's internal state") {
    class StatefulRunnable extends Runnable {
      var internal = 0
      def run(): Unit = {
        internal = 1
      }
    }
    val runnable = new StatefulRunnable
    new Thread(runnable).start()
    Thread.sleep(100)
    assertEquals(runnable.internal, 1)
  }

  test("Thread should be able to call a method") {
    object hasTwoArgMethod {
      var timesCalled = 0
      def call(arg: String, arg2: Int): Unit = {
        assertEquals("abc", arg)
        assertEquals(123, arg2)
        synchronized {
          timesCalled += 1
        }
      }
    }
    new Thread(new Runnable {
      def run(): Unit = {
        hasTwoArgMethod.call("abc", 123)
        hasTwoArgMethod.call("abc", 123)
      }
    }).start()
    Thread.sleep(100)
    assertEquals(hasTwoArgMethod.timesCalled, 2)
  }

  test("Exceptions in Threads should be handled") {
    val exception = new NullPointerException("There must be a null somewhere")
    val thread = new Thread(new Runnable {
      def run(): Unit = {
        throw exception
      }
    })
    val detector = new ExceptionDetector(thread, exception)
    thread.setUncaughtExceptionHandler(detector)

    thread.start()
    Thread.sleep(100)
    assert(detector.wasException)
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

  test("Exceptions in Threads should be handled") {
    val exception = new NullPointerException("There must be a null somewhere")
    val thread = new Thread(new Runnable {
      def run(): Unit = {
        throw exception
      }
    })
    val detector = new ExceptionDetector(thread, exception)
    withExceptionHandler(detector) {
      thread.start()
      Thread.sleep(100)
    }
    assert(detector.wasException)
  }

  test("Thread.join(ms) should wait until timeout") {
    val thread = new Thread {
      override def run(): Unit = {
        Thread.sleep(2000)
      }
    }
    thread.start()
    takesAtLeast(100) {
      thread.join(100)
    }
    assert(thread.isAlive)
  }

  test("Thread.join(ms,ns) should wait until timeout") {
    val thread = new Thread {
      override def run(): Unit = {
        Thread.sleep(2000)
      }
    }
    thread.start()
    takesAtLeast(100, 50) {
      thread.join(100, 50)
    }
    assert(thread.isAlive)
  }

  test("Thread.join should wait for thread finishing") {
    val thread = new Thread {
      override def run(): Unit = {
        Thread.sleep(100)
      }
    }
    thread.start()
    takesAtLeast(100) {
      thread.join(1000)
    }
    assertNot(thread.isAlive)
  }
  test("Thread.join should wait for thread finishing (no timeout)") {

    val thread = new Thread {
      override def run(): Unit = {
        Thread.sleep(100)
      }
    }
    thread.start()
    takesAtLeast(100) {
      thread.join()
    }
    assertNot(thread.isAlive)
  }

  test("Thread.getState and Thread.isAlive") {
    val thread = new Thread {
      override def run(): Unit = {
        Thread.sleep(100)
      }
    }
    assertEquals(Thread.State.NEW, thread.getState)
    thread.start()
    assertEquals(Thread.State.RUNNABLE, thread.getState)
    thread.join()
    assertEquals(Thread.State.TERMINATED, thread.getState)
    assertNot(thread.isAlive)
  }

  test("Thread.clone should fail") {
    val thread = new Thread("abc")
    expectThrows(classOf[CloneNotSupportedException], thread.clone())
  }

  test("Synchronized block should be executed by at most 1 thread"){
    val mutex = new Object
    var tmp = 0
    val runnable = new Runnable {
      def run(): Unit = mutex.synchronized {
        tmp *= 2
        tmp += 1
        Thread.sleep(100)
        tmp -= 1
      }
    }
    val t1 = new Thread(runnable)
    t1.start()
    val t2 = new Thread(runnable)
    t2.start()
    t1.join()
    t2.join()
    assertEquals(0, tmp)
  }
}
