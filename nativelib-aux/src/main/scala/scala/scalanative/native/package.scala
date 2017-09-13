package scala.scalanative

import scala.language.experimental.macros
import java.nio.charset.Charset
import scalanative.runtime.undefined

package object native {

  /** Int on 32-bit architectures and Long on 64-bit ones. */
  type Word = Long

  /** The C/C++ 'ptrdiff_t' type. */
  type CPtrDiff = Long

  /** The C 'sizeof' operator. */
  def sizeof[T](implicit tag: Tag[T]): CSize = undefined

  /** The C/C++ 'size_t' type. */
  type CSize = Word
}