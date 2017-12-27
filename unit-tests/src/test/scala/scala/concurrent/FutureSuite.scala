package scala.concurrent

import scala.concurrent.ExecutionContext.Implicits.global

object FutureSuite extends tests.MultiThreadSuite {
  def getResult[T](delay: Long = eternity)(future: Future[T]): Option[T] = {
    var value: Option[T] = None
    val mutex = new Object
    future.foreach {
      v: T =>
        mutex.synchronized {
          value = Some(v)
          mutex.notifyAll()
        }
    }
    if (value.isEmpty) {
      mutex.synchronized {
        if (value.isEmpty) {
          mutex.wait(delay)
        }
      }
    }
    value
  }

  test("Future.successful") {
    val future = Future.successful(3)
    assertEquals(getResult()(future), Some(3))
  }

  test("Future.failed") {
    val future = Future.failed(new NullPointerException("Nothing here"))
    assertEquals(getResult(200)(future), None)
  }

  test("Future.apply") {
    val future = Future(3)
    assertEquals(getResult()(future), Some(3))
  }

  test("Future.apply delayed") {
    val future = Future {
      Thread.sleep(1000)
      3
    }
    assertEquals(getResult()(future), Some(3))
  }

  test("Future.map") {
    val future = Future(7).map(_ * 191)
    assertEquals(getResult()(future), Some(1337))
  }

  test("Future.map delayed") {
    val future = Future {
      Thread.sleep(1000)
      7
    }.map { x =>
      Thread.sleep(1000)
      x * 191
    }
    assertEquals(getResult()(future), Some(1337))
  }

  test("Future.flatMap instant") {
    val future1 = Future.successful(7)
    val future = Future.successful(6).flatMap {
      b =>
        future1.map {
          a =>
            a * b
        }
    }
    assertEquals(getResult()(future), Some(42))
  }

  test("Future.flatMap") {
    val future1 = Future(7)
    val future = Future(6).flatMap {
      b =>
        future1.map {
          a =>
            a * b
        }
    }
    assertEquals(getResult()(future), Some(42))
  }

  test("Future.flatMap delayed") {
    val future1 = Future {
      Thread.sleep(1000)
      7
    }
    val future = Future {
      Thread.sleep(1000)
      6
    }.flatMap {
      b =>
        future1.map {
          a =>
            a * b
        }
    }
    assertEquals(getResult()(future), Some(42))
  }

  test("Future.flatMap delayed2") {
    val future1 = Future {
      Thread.sleep(600)
      7
    }
    val future = Future {
      Thread.sleep(1000)
      6
    }.flatMap {
      b =>
        future1.map {
          a =>
            a * b
        }
    }
    assertEquals(getResult()(future), Some(42))
  }
}
