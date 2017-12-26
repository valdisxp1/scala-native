package scala.scalanative.runtime

/**
 * Makes sure atomic operations are atomic.
 */
object AtomicHammerSuite extends tests.MultiThreadSuite {
  test("fetch_add is atomic for byte") {

    val numThreads = 2
    testWithMinRepetitions() { n: Int =>
      var number = 0.asInstanceOf[Byte]
      withThreads(numThreads, label = "CounterExample") { _ =>
        var i = n
        val b = 1.asInstanceOf[Byte]
        // making this as fast as possible
        while (i > 0) {
          number = (number + b).asInstanceOf[Byte]
          i -= 1
        }
      }
      number != (n * numThreads).asInstanceOf[Byte]
    } { n: Int =>
      val number = CAtomicByte()
      withThreads(numThreads, label = "Test") { _ =>
        var i = n
        val b = 1.asInstanceOf[Byte]
        // making this as fast as possible
        while (i > 0) {
          number.fetchAdd(b)
          i -= 1
        }
      }

      val value    = number.load()
      val expected = (n * numThreads).asInstanceOf[Byte]
      Console.out.println(s"value: $value, expected: $expected")
      number.free()
      value == expected
    }
  }
}
