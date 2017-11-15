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

  test("GC should not crash with multiple threads") {
    val muncher1 = new MemoryMuncher(10000)
    val muncher2 = new MemoryMuncher(10000)
    muncher1.start()
    muncher2.start()
    muncher1.join()
    muncher2.join()
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
      label: String = "Equal values")(left: => T, right: => T) =
    eventually(maxDelay, recheckEvery, label)(left == right)

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

  test("wait suspends execution by at least the requested amount") {
    val mutex            = new Object()
    val millisecondTests = Seq(0, 1, 5, 100, 1000)
    millisecondTests.foreach { ms =>
      mutex.synchronized {
        takesAtLeast(ms) {
          mutex.wait(ms)
        }
      }
    }
    millisecondTests.foreach { ms =>
      mutex.synchronized {
        takesAtLeast(ms) {
          mutex.wait(ms, 0)
        }
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
        mutex.synchronized {
          takesAtLeast(ms, nanos) {
            mutex.wait(ms, nanos)
          }
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
    eventuallyEquals()(shared, 1)
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
    eventuallyEquals()(t.internal, 1)
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
    eventuallyEquals()(runnable.internal, 1)
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
    val t = new Thread(new Runnable {
      def run(): Unit = {
        hasTwoArgMethod.call("abc", 123)
        hasTwoArgMethod.call("abc", 123)
      }
    })
    t.start()
    t.join(eternity)
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
    eventually()(detector.wasException)
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
    thread.join(eternity)
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
    assert(thread.isAlive)
    assertEquals(Thread.State.RUNNABLE, thread.getState)
    thread.join()
    assertEquals(Thread.State.TERMINATED, thread.getState)
    assertNot(thread.isAlive)
  }

  test("Thread.clone should fail") {
    val thread = new Thread("abc")
    expectThrows(classOf[CloneNotSupportedException], thread.clone())
  }

  test("Synchronized block should be executed by at most 1 thread") {
    val mutex = new Object
    var tmp   = 0
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

  test("Thread.currentThread") {
    new Thread {
      override def run(): Unit = {
        assertEquals(this, Thread.currentThread())
      }
    }.start()
    assert(Thread.currentThread() != null)
  }

  class WaitingThread(mutex: AnyRef) extends Thread {
    private var notified = false

    def timesNotified = if (notified) 1 else 0

    override def run(): Unit = {
      mutex.synchronized {
        mutex.wait()
      }
      notified = true
    }
  }
  test("wait-notify") {
    val mutex = new Object
    new Thread {
      override def run() = {
        Thread.sleep(100)
        mutex.synchronized {
          mutex.notify()
        }
      }
    }.start()
    mutex.synchronized {
      mutex.wait(1000)
    }
  }
  test("wait-notify 2") {
    val mutex         = new Object
    val waiter1       = new WaitingThread(mutex)
    val waiter2       = new WaitingThread(mutex)
    def timesNotified = waiter1.timesNotified + waiter2.timesNotified
    waiter1.start()
    waiter2.start()
    assertEquals(timesNotified, 0)
    eventuallyEquals(label = "waiter1.getState == Thread.State.WAITING")(
      waiter1.getState,
      Thread.State.WAITING)
    eventuallyEquals(label = "waiter2.getState == Thread.State.WAITING")(
      waiter2.getState,
      Thread.State.WAITING)
    mutex.synchronized {
      mutex.notify()
    }
    eventuallyEquals(label = "timesNotified == 1")(timesNotified, 1)
    mutex.synchronized {
      mutex.notify()
    }
    eventuallyEquals(label = "timesNotified == 2")(timesNotified, 2)
  }
  test("wait-notifyAll") {
    val mutex         = new Object
    val waiter1       = new WaitingThread(mutex)
    val waiter2       = new WaitingThread(mutex)
    def timesNotified = waiter1.timesNotified + waiter2.timesNotified
    waiter1.start()
    waiter2.start()
    assertEquals(timesNotified, 0)
    eventuallyEquals(label = "waiter1.getState == Thread.State.WAITING")(
      waiter1.getState,
      Thread.State.WAITING)
    eventuallyEquals(label = "waiter2.getState == Thread.State.WAITING")(
      waiter2.getState,
      Thread.State.WAITING)
    mutex.synchronized {
      mutex.notifyAll()
    }
    eventuallyEquals(label = "timesNotified == 2")(timesNotified, 2)
  }
  test("Object.wait puts the Thread into TIMED_WAITING state") {
    val mutex = new Object
    val thread = new Thread {
      override def run() = {
        mutex.synchronized {
          takesAtLeast(1000) {
            mutex.wait(1000)
          }
        }
        Thread.sleep(2000)
      }
    }
    thread.start()
    eventuallyEquals(label = "thread.getState == Thread.State.TIMED_WAITING")(
      thread.getState,
      Thread.State.TIMED_WAITING)
    thread.synchronized {
      thread.notify()
    }
    eventuallyEquals(label = "thread.getState == Thread.State.RUNNABLE")(
      thread.getState,
      Thread.State.RUNNABLE)
  }
  test("Multiple locks should not conflict") {
    val mutex1 = new Object
    val mutex2 = new Object
    var goOn   = true
    var doingStuff = false
    val waiter = new Thread {
      override def run() =
        mutex1.synchronized {
          while (goOn) {
            doingStuff = true
            Thread.sleep(10)
          }
        }
    }
    waiter.start()
    eventually()(doingStuff)

    val stopper = new Thread {
      override def run() = {
        Thread.sleep(100)
        mutex2.synchronized {
          goOn = false
        }
      }
    }
    stopper.start()
    stopper.join(eternity)
    assertNot(stopper.isAlive)
  }

  test("Thread.interrupt should interrupt sleep") {
    val thread = new Thread {
      override def run() = {
        expectThrows(classOf[InterruptedException], Thread.sleep(10 * eternity))
      }
    }
    thread.start()
    eventuallyEquals()(Thread.State.TIMED_WAITING, thread.getState)
    thread.interrupt()
    eventuallyEquals()(Thread.State.TERMINATED, thread.getState)
  }
  test("Thread.interrupt should interrupt between calculations") {
    var doingStuff = false
    val thread = new Thread {
      override def run() = {
        while (!Thread.interrupted()) {
          doingStuff = true
          //some intense calculation
          scala.collection.immutable.Range(1, 10000, 1).reduce(_ + _)
        }
      }
    }
    thread.start()
    eventually()(doingStuff)
    thread.interrupt()
    eventuallyEquals()(Thread.State.TERMINATED, thread.getState)
  }
}
