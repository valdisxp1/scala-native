package montecarlo

import java.util.Random

import benchmarks.{BenchmarkRunningTime, ShortRunningTime}

class PiMultiThreadBenchmark(val threadCount: Int)
    extends benchmarks.Benchmark[Double] {

  override val runningTime: BenchmarkRunningTime = ShortRunningTime

  class MonteCarloThread(id: Int, iterations: Int)
      extends Thread(s"Monte Carlo Pi-$id") {
    var count = 0
    override def run() = {
      val random = new Random(System.currentTimeMillis())
      count = (1 to iterations).count { _ =>
        val x = random.nextDouble()
        val y = random.nextDouble()
        x * x + y * y < 1
      }
    }
  }

  override def run(): Double = {
    val points = 3000000
    val spare  = points % threadCount
    val threads = (1 to threadCount).map { id =>
      val toRun = points / threadCount + {
        // divide spare points to the first threads
        if (id < spare) 1 else 0
      }
      new MonteCarloThread(id, toRun)
    }
    threads.foreach(_.start())
    threads.foreach(_.join())
    val count = threads.map(_.count).sum
    4.0 * count / points
  }

  override def check(result: Double): Boolean =
    (Math.PI - result) * (Math.PI - result) < 0.01
}
