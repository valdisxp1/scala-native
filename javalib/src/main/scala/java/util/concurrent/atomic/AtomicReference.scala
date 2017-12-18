package java.util.concurrent.atomic

import scala.scalanative.native.{CCast, CLong}
import scala.scalanative.runtime.CAtomicLong

class AtomicReference[T <: AnyRef](private[this] var value: T)
    extends Serializable {

  def this() = this(null.asInstanceOf[T])

  private[this] val inner = CAtomicLong(value.asInstanceOf[AnyRef].cast[CLong])

  final def get(): T = inner.load().cast[AnyRef].asInstanceOf[T]

  final def set(newValue: T): Unit =
    inner.store(newValue.asInstanceOf[AnyRef].cast[CLong])

  final def lazySet(newValue: T): Unit =
    inner.store(newValue.asInstanceOf[AnyRef].cast[CLong])

  final def compareAndSet(expect: T, update: T): Boolean = {
    inner
      .compareAndSwapStrong(expect.asInstanceOf[AnyRef].cast[CLong],
                            update.asInstanceOf[AnyRef].cast[CLong])
      ._1
  }

  final def weakCompareAndSet(expect: T, update: T): Boolean =
    inner
      .compareAndSwapWeak(expect.asInstanceOf[AnyRef].cast[CLong],
                          update.asInstanceOf[AnyRef].cast[CLong])
      ._1

  final def getAndSet(newValue: T): T = {
    val old = inner.load()
    inner.store(newValue.asInstanceOf[AnyRef].cast[CLong])
    old.cast[AnyRef].asInstanceOf[T]
  }

  override def toString(): String =
    String.valueOf(value)

}

object AtomicReference {

  private final val serialVersionUID: Long = -1848883965231344442L

}
