package scala.scalanative

import scalanative.annotation.alwaysinline
import scalanative.unsafe._
import scalanative.runtime.Intrinsics._

package object runtime {

  /** Runtime Type Information. */
  type Type = CStruct2[Int, String]

  implicit class TypeOps(val self: Ptr[Type]) extends AnyVal {
    @alwaysinline def id: Int          = self._1
    @alwaysinline def name: String     = self._2
    @alwaysinline def isClass: Boolean = id >= 0
  }

  /** Class runtime type information. */
  type ClassType = CStruct3[Type, Int, Int]

  implicit class ClassTypeOps(val self: Ptr[ClassType]) extends AnyVal {
    @alwaysinline def id: Int            = self._1._1
    @alwaysinline def name: String       = self._1._2
    @alwaysinline def size: Int          = self._2
    @alwaysinline def idRangeUntil: Long = self._3
  }

  /** Used as a stub right hand of intrinsified methods. */
  def intrinsic: Nothing = throwUndefined()

  @alwaysinline def toRawType(cls: Class[_]): RawPtr =
    cls.asInstanceOf[java.lang._Class[_]].rawty

  /** Read type information of given object. */
  @alwaysinline def getRawType(obj: Object): RawPtr = {
    val rawptr = Intrinsics.castObjectToRawPtr(obj)
    Intrinsics.loadRawPtr(rawptr)
  }

  @alwaysinline def fromRawPtr[T](rawptr: RawPtr): Ptr[T] =
    Boxes.boxToPtr(rawptr)

  @alwaysinline def toRawPtr[T](ptr: Ptr[T]): RawPtr =
    Boxes.unboxToPtr(ptr)

  /** Called by the generated code in case of division by zero. */
  @noinline def throwDivisionByZero(): Nothing =
    throw new java.lang.ArithmeticException("/ by zero")

  /** Called by the generated code in case of incorrect class cast. */
  @noinline def throwClassCast(from: RawPtr, to: RawPtr): Nothing = {
    val fromName = loadObject(elemRawPtr(from, 8))
    val toName   = loadObject(elemRawPtr(to, 8))
    throw new java.lang.ClassCastException(
      s"$fromName cannot be cast to $toName")
  }

  /** Called by the generated code in case of operations on null. */
  @noinline def throwNullPointer(): Nothing =
    throw new NullPointerException()

  /** Called by the generated code in case of unexpected condition. */
  @noinline def throwUndefined(): Nothing =
    throw new UndefinedBehaviorError

  /** Called by the generated code in case of out of bounds on array access. */
  @noinline def throwOutOfBounds(i: Int): Nothing =
    throw new IndexOutOfBoundsException(i.toString)

  /** Called by the generated code in case of missing method on reflective call. */
  @noinline def throwNoSuchMethod(sig: String): Nothing =
    throw new NoSuchMethodException(sig)
}
