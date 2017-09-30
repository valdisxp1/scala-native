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

  test("Thread should be able to change a shared var"){
    var shared: Int = 0
    new Thread(new Runnable {
      def run(): Unit = {
        shared = 1
      }
    }).start()
    Thread.sleep(100)
    assertEquals(shared,1)
  }

  def withExceptionHandler[U](handler: Thread.UncaughtExceptionHandler)(f: => U): U = {
    val oldHandler = Thread.getDefaultUncaughtExceptionHandler
    try {
      f
    } finally {
      Thread.setDefaultUncaughtExceptionHandler(oldHandler)
    }
  }

  test("Exceptions in Threads should be handled"){
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
  }
}
