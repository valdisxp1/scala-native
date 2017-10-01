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

/*  test("Thread should be able to change its internal state") {
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
  }*/

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
        hasTwoArgMethod.call("abc",123)
      }
    }).start()
    Thread.sleep(100)
    assertEquals(hasTwoArgMethod.timesCalled, 1)
  }

  def withExceptionHandler[U](handler: Thread.UncaughtExceptionHandler)(f: => U): U = {
    val oldHandler = Thread.getDefaultUncaughtExceptionHandler
    try {
      f
    } finally {
      Thread.setDefaultUncaughtExceptionHandler(oldHandler)
    }
  }

/*  test("Exceptions in Threads should be handled"){
    val exception = new NullPointerException("There must be a null somewhere")
    val thread = new Thread(new Runnable {
      def run(): Unit = {
        throw exception
      }
    })
    var wasException = false
    withExceptionHandler(new Thread.UncaughtExceptionHandler{
      def uncaughtException(t: Thread, e: Throwable) = {
        assertEquals(t, thread)
        assertEquals(e, exception)
        wasException = true
      }
    }){
      thread.start()
      Thread.sleep(100)
    }
    assert(wasException)
  }*/
}
