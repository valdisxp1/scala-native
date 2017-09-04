package java.lang

object ThreadSuite extends tests.Suite {

  test("sleep 100ms") {
    val expectedDelay = 100
    val maxAdditionalDelay = 1

    val start = System.currentTimeMillis()
    Thread.sleep(expectedDelay)
    val end = System.currentTimeMillis()

    val measuredDelay = end - start
    assert(measuredDelay >= expectedDelay)
    assert(measuredDelay <= expectedDelay + maxAdditionalDelay)
  }
}
