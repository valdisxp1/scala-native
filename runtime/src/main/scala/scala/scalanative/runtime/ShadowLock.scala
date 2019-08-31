package scala.scalanative
package runtime

/**
  * Cannot be checked with Thread.holdsLock
  */
class ShadowLock {
  // workaround so lock is freed when exception is thrown
  def safeSynchronized[T](f: => T): T = {
    var throwable: Throwable = null
    val result = synchronized {
      try {
        f
      } catch {
        case t: Throwable =>
          throwable = t
      }
    }
    if (throwable != null) {
      throw throwable
    } else {
      result.asInstanceOf[T]
    }

  }
}