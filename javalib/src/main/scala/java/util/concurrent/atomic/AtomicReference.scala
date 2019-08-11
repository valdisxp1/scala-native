package java.util.concurrent.atomic

import scala.scalanative.runtime.CAtomicLong
import scala.scalanative.runtime.Intrinsics.{castObjectToRawPtr, castRawPtrToLong, castLongToRawPtr, castRawPtrToObject}

class AtomicReference[T <: AnyRef](private[this] var value: T)
    extends Serializable {

  def this() = this(null.asInstanceOf[T])

  // XXX Immix and Commix will not mark this reference
  private[this] val inner = CAtomicLong(value)

  final def get(): T = inner.load()

  final def set(newValue: T): Unit = inner.store(newValue)

  final def lazySet(newValue: T): Unit = inner.store(newValue)

  final def compareAndSet(expect: T, update: T): Boolean =
    inner.compareAndSwapStrong(expect, update)._1

  final def weakCompareAndSet(expect: T, update: T): Boolean =
    inner.compareAndSwapWeak(expect, update)._1

  final def getAndSet(newValue: T): T = {
    val old = inner.load()
    inner.store(newValue)
    old
  }

  override def toString(): String =
    String.valueOf(value)

  private implicit def toLong(e: T): Long = castRawPtrToLong(castObjectToRawPtr(e))
  private implicit def toRef(l: Long): T  = castRawPtrToObject(castLongToRawPtr(l)).asInstanceOf
}

object AtomicReference {

  private final val serialVersionUID: Long = -1848883965231344442L

}
