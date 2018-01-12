package benchmarks

import java.lang.System.exit

import montecarlo.{PiFuturesBenchmark, PiMultiThreadBenchmark}

import scala.scalanative.native.{Zone, stdlib}
import scala.scalanative.posix.unistd

object Main {
  def dumpMemInfo(): Unit = Zone {
    implicit zone =>
      import scala.scalanative.native.toCString
      val pid = unistd.getpid()
      val command = toCString("ps u " + pid)
      stdlib.system(command)
  }
  def main(args: Array[String]): Unit = {
    val opts = Opts(args)
    val benchmarks = Seq(new PiMultiThreadBenchmark(opts.threadCount))

    val results = benchmarks.map { bench =>
      bench.loop(opts.iterations)
      bench.loop(opts.iterations)
    }
    val success = results.forall(_.success)

    println(opts.format.show(results))

    if (opts.meminfo) {
      dumpMemInfo()
    }

    if (success) exit(0) else exit(1)
  }
}
