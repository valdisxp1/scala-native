package scala.scalanative

package object runtime {
  /** Used as a stub right hand of intrinsified methods. */
  def undefined: Nothing = throw new UndefinedBehaviorError
}