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
    class StatefulThread extends Thread{
      var internal = 0
      override def run() = {
        internal = 1
      }
    }
    val t  = new StatefulThread
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

  def withExceptionHandler[U](handler: Thread.UncaughtExceptionHandler)(f: => U): U = {
    val oldHandler = Thread.getDefaultUncaughtExceptionHandler
    Thread.setDefaultUncaughtExceptionHandler(handler)
    try {
      f
    } finally {
      Thread.setDefaultUncaughtExceptionHandler(oldHandler)
    }
  }

  case class ExceptionDetector(thread: Thread, exception: Throwable) extends Thread.UncaughtExceptionHandler {
    private var _wasException = false
    def wasException: scala.Boolean = _wasException
    def uncaughtException(t: Thread, e: Throwable): Unit = {
      assertEquals(t, thread)
      assertEquals(e, exception)
      _wasException = true
    }
  }

  test("Exceptions in Threads should be handled"){
    val exception = new NullPointerException("There must be a null somewhere")
    val thread = new Thread(new Runnable {
      def run(): Unit = {
        throw exception
      }
    })
    val detector = new ExceptionDetector(thread, exception)
    withExceptionHandler(detector){
      thread.start()
      Thread.sleep(100)
    }
    assert(detector.wasException)
  }
}
