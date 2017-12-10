package java.lang

object ThreadGroupSuite extends tests.MultiThreadSuite {
  test("Constructors") {
    val groupName = "groupNameGoesHere"
    val threadGroup = new ThreadGroup(groupName)
    assertEquals(threadGroup.getName, groupName)

    val subgroupName = "this is a subgroup"
    val subgroup = new ThreadGroup(threadGroup, subgroupName)
    assertEquals(subgroup.getName, subgroupName)
    assertEquals(subgroup.getParent, threadGroup)
  }

  test("ThreadGroup.checkAccess does not crash") {
    val mainThreadGroup = Thread.currentThread().getThreadGroup
    val threadGroup     = new ThreadGroup("abc")

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

  test("ThreadGroup.interrupt should interrupt sleep for all threads") {
    var fail = false
    class SleepyThread(group: ThreadGroup, name: String)
        extends Thread(group, name) {
      override def run() = {
        expectThrows(classOf[InterruptedException], {
          Thread.sleep(10 * eternity)
          fail = true
        })
      }
    }
    val group    = new ThreadGroup("group")
    val subgroup = new ThreadGroup(group, "subgroup")
    val threads = scala.Seq(
      new SleepyThread(group, "G-1"),
      new SleepyThread(group, "G-2"),
      new SleepyThread(group, "G-3"),
      new SleepyThread(subgroup, "SG-1"),
      new SleepyThread(subgroup, "SG-2")
    )
    threads.foreach(_.start())
    threads.foreach { thread: Thread =>
      eventuallyEquals(
        label = thread.getName + "is in Thread.State.TIMED_WAITING")(
        Thread.State.TIMED_WAITING,
        thread.getState)
    }
    group.interrupt()
    threads.foreach { thread: Thread =>
      eventuallyEquals(
        label = thread.getName + "is in Thread.State.TERMINATED")(
        Thread.State.TERMINATED,
        thread.getState)
    }
    assertNot(fail)
  }

  test("Thread.setPriority respects threadGroups maxPriority") {
    val fastGroup = new ThreadGroup("fastGroup")
    assertEquals(fastGroup.getMaxPriority, Thread.MAX_PRIORITY)

    val fastThread = new Thread(fastGroup, "fastThread")
    fastThread.setPriority(Thread.MAX_PRIORITY)
    assertEquals(fastThread.getPriority, Thread.MAX_PRIORITY)

    val slowGroup = new ThreadGroup("slowGroup")
    slowGroup.setMaxPriority(Thread.MIN_PRIORITY)
    assertEquals(slowGroup.getMaxPriority, Thread.MIN_PRIORITY)

    val slowThread = new Thread(slowGroup, "slowThread")
    slowThread.setPriority(Thread.MAX_PRIORITY)
    assertEquals(slowThread.getPriority, Thread.MIN_PRIORITY)
  }

  test("*DEPRECATED*  ThreadGroup.suspend and resume should affect all threads") {
    val group    = new ThreadGroup("group")
    val subgroup = new ThreadGroup(group, "subgroup")
    val threads = scala.Seq(
      new Counter(group, "G-1"),
      new Counter(group, "G-2"),
      new Counter(group, "G-3"),
      new Counter(subgroup, "SG-1"),
      new Counter(subgroup, "SG-2")
    )
    threads.foreach(_.start())
    threads.foreach { thread: Counter =>
      eventually()(thread.count > 1)
    }
    threads.foreach(_.suspend())
    threads.foreach { thread: Counter =>
      eventuallyConstant()(thread.count)
    }
    threads.foreach(_.resume())
  }
}
