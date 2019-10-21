object Test {
  def main(args: Array[String]): Unit = {
    val mutex = new Object
    mutex.synchronized {
      mutex.wait(10)
    }
    println("Hello, World!")
  }
}
