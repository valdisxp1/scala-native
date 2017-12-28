// Boehm GC cannot handle this
// rename to PiFuturesBenchmark to re-enable
package montecarlo

import benchmarks.{BenchmarkRunningTime, LongRunningTime}

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent.Future
import scala.concurrent.forkjoin.ThreadLocalRandom

class PiFuturesBenchmarkDisabled extends benchmarks.Benchmark[Double] {

  override val runningTime: BenchmarkRunningTime = LongRunningTime

  override def run(): Double = {
    val points = 200000
    val futures: Seq[Future[Boolean]] = (1 to points).map { _ =>
      Future {
        val random = ThreadLocalRandom.current()
        val x      = random.nextDouble()
        val y      = random.nextDouble()
        x * x + y * y < 1
      }
    }

    val countFuture: Future[Int] = Future.fold(futures)(0) { (a, b) =>
      a + (if (b) 1 else 0)
    }

    // workaround for not having await
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