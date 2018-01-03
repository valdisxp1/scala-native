package benchmarks

final class Opts(val format: Format = TextFormat,
                 val test: Boolean = false,
                 val iterations: Int = 5,
                 val threadCount: Int = Runtime.getRuntime.availableProcessors()) {
  def copy(format: Format = format,
           test: Boolean = test,
           iterations: Int = iterations,
           threadCount: Int = threadCount): Opts =
    new Opts(format, test, iterations, threadCount)
}

object Opts {
  def apply(args: Array[String]): Opts = {
    def loop(opts: Opts, args: List[String]): Opts = args match {
      case "--format" :: format :: rest =>
        loop(opts.copy(format = Format(format)), rest)
      case "--threads" :: threads :: rest =>
        loop(opts.copy(threadCount = threads.toInt), rest)
      case "--iterations" :: iterations :: rest =>
        loop(opts.copy(iterations = iterations.toInt), rest)
      case "--test" :: rest =>
        loop(opts.copy(test = true), rest)
      case other :: _ =>
        throw new Exception("unrecognized option: " + other)
      case Nil =>
        opts
    }
    loop(new Opts(), args.toList)
  }
}
