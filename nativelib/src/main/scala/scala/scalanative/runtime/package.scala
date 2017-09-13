package scala.scalanative

import native._
import runtime.Intrinsics._

package object runtime {
  implicit class TypeOps(val self: Ptr[Type]) extends AnyVal {
    def id: Int      = !(self._1)
    def name: String = !(self._2)
    def kind: Long   = !(self._3)
  }

  /** Class runtime type information. */
  type ClassType = CStruct3[Type, Long, CStruct2[Int, Int]]

  implicit class ClassTypeOps(val self: Ptr[ClassType]) extends AnyVal {
    def id: Int           = self._1.id
    def name: String      = self._1.name
    def kind: Long        = self._1.kind
    def size: Long        = !(self._2)
    def idRangeFrom: Long = !(self._3._1)
    def idRangeTo: Long   = !(self._3._2)
  }

  final val CLASS_KIND  = 0
  final val TRAIT_KIND  = 1
  final val STRUCT_KIND = 2

  /** Intrinsified byte to unsigned int converstion. */
  def byteToUInt(b: Byte): Int = undefined

  /** Intrinsified byte to unsigned long conversion. */
  def byteToULong(b: Byte): Long = undefined

  /** Intrinsified short to unsigned int conversion. */
  def shortToUInt(v: Short): Int = undefined

  /** Intrinsified short to unsigned long conversion. */
  def shortToULong(v: Short): Long = undefined

  /** Intrinsified int to unsigned long conversion. */
  def intToULong(v: Int): Long = undefined

  /** Select value without branching. */
  @deprecated("Use if-then-else instead.", "0.3.3")
  def select[T](cond: Boolean, thenp: T, elsep: T)(implicit tag: Tag[T]): T =
    undefined

  /** Read type information of given object. */
  def getType(obj: Object): Ptr[ClassType] = !obj.cast[Ptr[Ptr[ClassType]]]

  /** Get monitor for given object. */
  def getMonitor(obj: Object): Monitor = Monitor.dummy

  /** Initialize runtime with given arguments and return the
   *  rest as Java-style array.
   */
  def init(argc: Int, argv: Ptr[Ptr[Byte]]): ObjectArray = {
    val args = new scala.Array[String](argc - 1)

    // skip the executable name in argv(0)
    var c = 0
    while (c < argc - 1) {
      // use the default Charset (UTF_8 atm)
      args(c) = fromCString(argv(c + 1))
      c += 1
    }

    args.asInstanceOf[ObjectArray]
  }

  /** Run the runtime's event loop. The method is called from the
   *  generated C-style after the application's main method terminates.
   */
  def loop(): Unit = ExecutionContext.loop()
}
