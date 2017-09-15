package scala.concurrent.forkjoin

private[forkjoin] object Workaround {

  /**
   * Avoid Unit type by having an Int constant as the last item in the block.
   * Otherwise the compiler crashes with a NullPointerException.
   */
  final val avoidUnitType = 0;
}
