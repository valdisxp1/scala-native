import java.util.concurrent.atomic.AtomicInteger
import scala.scalanative.native.{CCast, Tag, Word}

class FinalizingQueue {
  def isEmpty: Boolean       = ???
  def peak: FinalizingQueue  = ???
  def pop(): FinalizingQueue = ???
}

/*
 Finalizable objects are to be kept in separate, region(s), or their existance kept track.
 The collector after marking everything else will look at the finalizables.
 If a finalizable is unmarked and finalized, then it can be deleted.
 If a finalizable is unmarked and not finalized then it will be moved to a finalizing queue, marked and traced (gray).
 If it is marked then do nothing.
 It takes 2 gc cycles to destroy a finalizable.
 */
abstract class Finalizable {
  var finalized                 = false
  private var next: Finalizable = null
  def queue: FinalizingQueue
  def _finalize(): Unit
}

/*
 It takes 2-3 gc cycles to destroy a weaklyReferencable
 */
abstract class WeaklyReferencable[T] extends Finalizable {
  lazy val weakReference = new WeakReference(this)
  def _finalize(): Unit = {
    if (weakReference.state.compareAndSet(0, 2)) {
      finalized = true
    } else {
      weakReference.state.compareAndSet(1, 0)
    }
  }
}

class WeakReference[T <: AnyRef](strong: T) {
  /*
 0 = untouched
 1 = touched
 2 = destroyed
   */
  val state               = new AtomicInteger(0)
  var hiddenPointer: Word = strong.cast[Word]

  def get: T = {
    while (state.get() == 0) {
      state.compareAndSet(0, 1)
    }
    if (state.get() == 1) {
      hiddenPointer.cast[AnyRef].asIntanceOf[T]
    } else {
      null.asInstanceOf[T]
    }
  }
}
