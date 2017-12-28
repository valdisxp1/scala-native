package montecarlo

import benchmarks.{BenchmarkRunningTime, ShortRunningTime}

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.duration._
import scala.concurrent.{Await, Future}
import scala.concurrent.forkjoin.ThreadLocalRandom

class PiFuturesBenchmark extends benchmarks.Benchmark[Double] {

  override val runningTime: BenchmarkRunningTime = ShortRunningTime

  override def run(): Double = {
    System.gc()
    val points = 200000
    val futures: Seq[Future[Boolean]] = (1 to points).map { _ =>
      Future {
        val random = ThreadLocalRandom.current()
        val x      = random.nextDouble()
        val y      = random.nextDouble()
        x * x + y * y < 1
      }
    }

    var n = 0
    val countFuture: Future[Int] = futures.foldLeft(Future.successful(0)) {
      (sumFuture: Future[Int], booleanFuture: Future[Boolean]) =>
        n += 1
        sumFuture.flatMap { a =>
          booleanFuture.map { b =>
            a + (if (b) 1 else 0)
          }
        }
    }

    var count = -1
    val mutex = new Object

    countFuture.foreach { number: Int =>
      mutex.synchronized {
        count = number
        mutex.notifyAll()
      }
    }

    if (count == -1) {
      mutex.synchronized {
        while (count == -1) {
          mutex.wait()
        }
      }
    }

    4.0 * count / points
  }

  override def check(result: Double): Boolean =
    (Math.PI - result) * (Math.PI - result) < 0.01
}
