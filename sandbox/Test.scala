object Test {
  def main(args: Array[String]): Unit = {
    println("Hello, World!")
    Console.err.println("1a")
    System.err.println("1b")
    Console.withErr(System.out) {
      Console.err.println("2a")
      System.err.println("2b")
    }
    Console.err.println("2a")
    System.err.println("2b")

    // does not compile: cannot link: @java.lang.System$::setErr_java.io.PrintStream_unit
    System.setErr(System.out)
    Console.err.println("3a")
    System.err.println("3b")

    // does not compile: reassignment to val
//    System.err = System.out
    Console.err.println("4a")
    System.err.println("4b")
  }
}
