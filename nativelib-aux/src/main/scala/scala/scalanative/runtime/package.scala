package scala.scalanative

import native._

package object runtime {
  /** Used as a stub right hand of intrinsified methods. */
  def undefined: Nothing = throw new UndefinedBehaviorError

  /** Runtime Type Information. */
  type Type = CStruct3[Int, String, Byte]

  /** Returns info pointer for given type. */
  def typeof[T](implicit tag: Tag[T]): Ptr[Type] = undefined

  /** Intrinsified unsigned devision on ints. */
  def divUInt(l: Int, r: Int): Int = undefined

  /** Intrinsified unsigned devision on longs. */
  def divULong(l: Long, r: Long): Long = undefined

  /** Intrinsified unsigned remainder on ints. */
  def remUInt(l: Int, r: Int): Int = undefined

  /** Intrinsified unsigned remainder on longs. */
  def remULong(l: Long, r: Long): Long = undefined
}