package benchmarks

import java.lang.System.exit

import montecarlo.{PiFuturesBenchmark, PiMultiThreadBenchmark}

object Main {
  def main(args: Array[String]): Unit = {
    val threads = System.getProperty("THREADS","4").toInt
    val benchmarks = Seq(new PiMultiThreadBenchmark, new PiFuturesBenchmark)

    val opts = Opts(args)
    val iterations = System.getProperty("ITERATIONS","5").toInt
    val results = benchmarks.map { bench =>
      bench.loop(iterations)
      bench.loop(iterations)
    }
    val success = results.forall(_.success)

    println(opts.format.show(results))

    if (success) exit(0) else exit(1)
  }
}
