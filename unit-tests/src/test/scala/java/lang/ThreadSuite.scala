package java.lang

import java.io.{OutputStream, PrintStream}

import scala.collection.mutable

object ThreadSuite extends tests.MultiThreadSuite {

  test("Constructors") {
    {
      val thread = new Thread
      //just do not crash
      thread.start()
      thread.join()
    }

    Console.out.println("1")

    {
      val name        = "Dave"
      val threadGroup = new ThreadGroup("MyGroup")
      val thread      = new Thread(threadGroup, name)
      assertEquals(thread.getName, name)
      assertEquals(thread.getThreadGroup, threadGroup)
      thread.start()
      assertEquals(thread.getName, name)
      // threadGroup may be null or the value, because it may have already ended
      thread.join()
      assertEquals(thread.getName, name)
      assertEquals(thread.getThreadGroup, null)
    }

    Console.out.println("2")

    {
      val name   = "Dave"
      val thread = new Thread(name)
      assertEquals(thread.getName, name)
      thread.start()
      assertEquals(thread.getName, name)
      thread.join()
      assertEquals(thread.getName, name)
    }

    Console.out.println("3")

    {
      val name         = "Dave"
      var didSomething = false
      val runnable = new Runnable {
        def run(): Unit = {
          didSomething = true
        }
      }
      val thread = new Thread(runnable, name)
      assertEquals(thread.getName, name)
      thread.start()
      assertEquals(thread.getName, name)
      thread.join()
      assert(didSomething)
      assertEquals(thread.getName, name)
    }

    Console.out.println("4")

    {
      val name         = "Dave"
      var didSomething = false
      val threadGroup  = new ThreadGroup("MyGroup")
      val runnable = new Runnable {
        def run(): Unit = {
          didSomething = true
        }
      }
      val thread = new Thread(threadGroup, runnable, name)
      assertEquals(thread.getName, name)
      assertEquals(thread.getThreadGroup, threadGroup)
      thread.start()
      assertEquals(thread.getName, name)
      // threadGroup may be null or the value, because it may have already ended
      thread.join()
      assert(didSomething)
      assertEquals(thread.getName, name)
      assertEquals(thread.getThreadGroup, null)
    }

    Console.out.println("5")

    {
      val name         = "Dave"
      var didSomething = false
      val threadGroup  = new ThreadGroup("MyGroup")
      val runnable = new Runnable {
        def run(): Unit = {
          didSomething = true
        }
      }
      val stackSize = 6L * 1024L * 1024L
      val thread    = new Thread(threadGroup, runnable, name, stackSize)
      // no way to check stack size yet, just do not crash
      assertEquals(thread.getName, name)
      assertEquals(thread.getThreadGroup, threadGroup)
      thread.start()
      assertEquals(thread.getName, name)
      // threadGroup may be null or the value, because it may have already ended
      thread.join()
      assert(didSomething)
      assertEquals(thread.getName, name)
      assertEquals(thread.getThreadGroup, null)
    }
  }

  test("Priority constants are valid") {

    val max  = Thread.MAX_PRIORITY
    val min  = Thread.MIN_PRIORITY
    val norm = Thread.NORM_PRIORITY

    assert(min <= max)
    assert(min <= norm)
    assert(norm <= max)
  }

  test("Get/Set Priority work as it should with currentThread") {
    val currentThread = Thread.currentThread()
    val old           = currentThread.getPriority
    try {
      currentThread.setPriority(3)
      assert(currentThread.getPriority == 3)
    } finally {
      currentThread.setPriority(old)
    }
  }

  test("Get/Set Priority work as it should with other thread") {
    val thread = new Thread()

    thread.setPriority(2)
    assert(thread.getPriority == 2)
  }

  test("Thread.yield does not crash") {
    Thread.`yield`()
    val thread = new Thread {
      override def run() = Thread.`yield`()
    }
    thread.start()
    thread.join()
  }

  test("Thread.checkAccess does not crash") {
    Thread.currentThread().checkAccess()
    val thread = new Thread {
      override def run() = checkAccess()
    }
    thread.start()
    thread.join()
  }

  test("GC should not crash with multiple threads") {
    val muncher1 = new MemoryMuncher(10000)
    val muncher2 = new MemoryMuncher(10000)
    muncher1.start()
    muncher2.start()
    muncher1.join()
    muncher2.join()
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
    var goOn = true
    val thread = new Thread {
      override def run(): Unit = {
        while (goOn) {}
      }
    }
    assertEquals(Thread.State.NEW, thread.getState)
    thread.start()
    assert(thread.isAlive)
    assertEquals(Thread.State.RUNNABLE, thread.getState)
    goOn = false
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
    var goOn  = true
    val thread = new Thread {
      override def run() = {
        mutex.synchronized {
          takesAtLeast(1000) {
            mutex.wait(1000)
          }
        }
        while (goOn) {}
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
    goOn = false
    eventuallyEquals(label = "thread.getState == Thread.State.TERMINATED")(
      thread.getState,
      Thread.State.TERMINATED)
  }
  test("Multiple locks should not conflict") {
    val mutex1     = new Object
    val mutex2     = new Object
    var goOn       = true
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

  def withErr[T](stream: OutputStream)(f: => T): T = {
    val old = System.err
    try {
      System.setErr(new PrintStream(stream))
      f
    } finally {
      System.setErr(old)
    }
  }

  test("Thread.dumpStack should contain the method name") {
    object Something {
      def aMethodWithoutAnInterestingName = {
        Thread.dumpStack()
      }
    }
    val outputStream = new java.io.ByteArrayOutputStream()
    withErr(outputStream) {
      Something.aMethodWithoutAnInterestingName
    }
    assert(outputStream.toString.contains("aMethodWithoutAnInterestingName"))
  }
  test("currentThread().getStackTrace should contain the running method name") {
    object Something {
      def aMethodWithoutAnInterestingName = {
        Thread.currentThread().getStackTrace
      }
    }
    val rawStackTrace = Something.aMethodWithoutAnInterestingName
    assert(
      mutable.WrappedArray
        .make[StackTraceElement](rawStackTrace)
        .exists(_.getMethodName == "aMethodWithoutAnInterestingName"))
  }
  test("Thread.getAllStackTraces should contain our own thread") {
    object Something {
      def aMethodWithoutAnInterestingName = {
        Thread.getAllStackTraces
      }
    }
    val rawStackTraces = Something.aMethodWithoutAnInterestingName
    val currentThread  = Thread.currentThread()
    Console.out.println(
      "rawStackTraces.containsKey(currentThread): " + rawStackTraces
        .containsKey(currentThread))
    Console.out.println("rawStackTraces.size: " + rawStackTraces.size())
    assert(rawStackTraces.containsKey(currentThread))
    val currentThreadStackTrace = rawStackTraces.get(currentThread)
    assert(
      mutable.WrappedArray
        .make[StackTraceElement](currentThreadStackTrace)
        .exists(_.getMethodName == "aMethodWithoutAnInterestingName"))
  }
  test("thread.getStackTrace should be a stacktrace for that thread") {
    object Something {
      def aMethodWithoutAnInterestingName = {
        Thread.sleep(2000)
      }
    }
    val thread = new Thread {
      override def run() = {
        Something.aMethodWithoutAnInterestingName
      }
    }
    thread.start()
    eventuallyEquals(label = "thread.getState == Thread.State.TIMED_WAITING")(
      thread.getState,
      Thread.State.TIMED_WAITING)
    val rawStackTrace = thread.getStackTrace
    assert(
      mutable.WrappedArray
        .make[StackTraceElement](rawStackTrace)
        .exists(_.getMethodName == "aMethodWithoutAnInterestingName"))
  }
  test("newly created threads should show up in Thread.getAllStackTraces") {
    val mutex   = new Object
    val thread1 = new WaitingThread(mutex)
    val thread2 = new WaitingThread(mutex)

    {
      val stackTraces = Thread.getAllStackTraces
      assertEquals(stackTraces.containsKey(thread1), false)
      assertEquals(stackTraces.containsKey(thread2), false)
    }

    thread1.start()

    {
      val stackTraces = Thread.getAllStackTraces
      assertEquals(stackTraces.containsKey(thread1), true)
      assertEquals(stackTraces.containsKey(thread2), false)
    }

    thread2.start()

    {
      val stackTraces = Thread.getAllStackTraces
      assertEquals(stackTraces.containsKey(thread1), true)
      assertEquals(stackTraces.containsKey(thread2), true)
    }

    mutex.notifyAll()

    eventually() {
      val stackTraces = Thread.getAllStackTraces
      !stackTraces.containsKey(thread1) && !stackTraces.containsKey(thread2)
    }
  }

  test("holdsLock") {
    def twoMutexTest = {
      val mutex1 = new Object
      val mutex2 = new Object

      assertEquals(Thread.holdsLock(mutex1), false)
      assertEquals(Thread.holdsLock(mutex2), false)
      mutex1.synchronized {
        assertEquals(Thread.holdsLock(mutex1), true)
        assertEquals(Thread.holdsLock(mutex2), false)
        mutex2.synchronized {
          assertEquals(Thread.holdsLock(mutex1), true)
          assertEquals(Thread.holdsLock(mutex2), true)
        }
        assertEquals(Thread.holdsLock(mutex1), true)
        assertEquals(Thread.holdsLock(mutex2), false)
      }
      assertEquals(Thread.holdsLock(mutex1), false)
      assertEquals(Thread.holdsLock(mutex2), false)
    }

    twoMutexTest

    val thread = new Thread {
      override def run() = twoMutexTest
    }
    thread.start()
    thread.join()
  }
  test("contextClassLoaders not supported") {
    assertThrows[NotImplementedError](
      Thread.currentThread().setContextClassLoader(new ClassLoader() {}))
    assertThrows[NotImplementedError](
      Thread.currentThread().getContextClassLoader)
  }
}
