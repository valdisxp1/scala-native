package java.lang

object ThreadGroupSuite extends tests.MultiThreadSuite {
  test("ThreadGroup.checkAccess does not crash") {
    val mainThreadGroup = Thread.currentThread().getThreadGroup
    val threadGroup = new ThreadGroup("abc")

    mainThreadGroup.checkAccess()
    threadGroup.checkAccess()

    val thread = new Thread {
      override def run() = {
        mainThreadGroup.checkAccess()
        threadGroup.checkAccess()
      }
    }
    thread.start()
    thread.join()
  }
}
