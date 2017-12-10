package java.lang

object ThreadGroupSuite extends tests.MultiThreadSuite {
  test("Constructors") {
    val groupName   = "groupNameGoesHere"
    val threadGroup = new ThreadGroup(groupName)
    assertEquals(threadGroup.getName, groupName)

    val subgroupName = "this is a subgroup"
    val subgroup     = new ThreadGroup(threadGroup, subgroupName)
    assertEquals(subgroup.getName, subgroupName)
    assertEquals(subgroup.getParent, threadGroup)
    assert(threadGroup.parentOf(subgroup))
    assertNot(subgroup.parentOf(threadGroup))
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

  abstract class Structure[T <: Thread] {
    def makeTread(group: ThreadGroup, name: String): T

    val group = new ThreadGroup("group")
    val groupThreads: Seq[T] = scala.Seq(makeTread(group, "G-1"),
                                         makeTread(group, "G-2"),
                                         makeTread(group, "G-3"))

    val subgroup1 = new ThreadGroup(group, "subgroup")
    val subgroup1Threads: Seq[T] =
      scala.Seq(makeTread(subgroup1, "SG-1"), makeTread(subgroup1, "SG-2"))

    val subgroup2 = new ThreadGroup(group, "subgroup")
    val subgroup2Threads: Seq[T] =
      scala.Seq(makeTread(subgroup2, "SG-A"), makeTread(subgroup2, "SG-B"))

    val threads: Seq[T] = groupThreads ++ subgroup1Threads ++ subgroup2Threads
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
    val structure = new Structure[SleepyThread] {
      def makeTread(group: ThreadGroup, name: String) =
        new SleepyThread(group, name)
    }
    import structure._
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

  test("ThreadGroup.destroy") {
    val threadGroup = new ThreadGroup("abc")
    val thread      = new Counter(threadGroup, "active")
    assertNot(threadGroup.isDestroyed)

    thread.start()
    eventually()(thread.count > 1)
    //cannot destroy group with running threads
    assertThrows[IllegalThreadStateException](threadGroup.destroy())
    assertNot(threadGroup.isDestroyed)
    thread.goOn = false
    thread.join()

    threadGroup.destroy()
    assert(threadGroup.isDestroyed)

    // cannot destroy it twice
    assertThrows[IllegalThreadStateException](threadGroup.destroy())
    assert(threadGroup.isDestroyed)

    // cannot add new threads or threadGroups
    assertThrows[IllegalThreadStateException](
      new Thread(threadGroup, "Sad Thread"))
    assertThrows[IllegalThreadStateException](
      new ThreadGroup(threadGroup, "Sad ThreadGroup"))
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
    val structure = new Structure[Counter] {
      def makeTread(group: ThreadGroup, name: String) = new Counter(group, name)
    }
    import structure._
    threads.foreach(_.start())
    threads.foreach { thread: Counter =>
      eventually()(thread.count > 1)
    }
    group.suspend()
    val countsMap = threads.map { thread: Counter =>
      thread -> eventuallyConstant()(thread.count).get
    }.toMap
    group.resume()
    threads.foreach { thread: Counter =>
      eventually()(thread.count > countsMap(thread))
    }

    threads.foreach { thread: Counter =>
      thread.goOn = false
      thread.join()
    }
  }
}
