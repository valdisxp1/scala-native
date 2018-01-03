package montecarlo

import java.util.Random

import benchmarks.{BenchmarkRunningTime, ShortRunningTime}

class PiSingleThreadBenchmark extends benchmarks.Benchmark[Double] {

  override val runningTime: BenchmarkRunningTime = ShortRunningTime

  override def run(): Double = {
    val random = new Random(System.currentTimeMillis())
    val points = 200000
    val count: Int = (1 to points).count { _ =>
      val x = random.nextDouble()
      val y = random.nextDouble()
      x * x + y * y < 1
    }
    4.0 * count / points
  }

  override def check(result: Double): Boolean =
    (Math.PI - result) * (Math.PI - result) < 0.01
}
