package scala.scalanative
package runtime

import java.util.concurrent.atomic.AtomicReference

import scala.concurrent.ExecutionContextExecutor

object ExecutionContext {
  def global: ExecutionContextExecutor = QueueExecutionContext

  private class Queue {
    private val ref: AtomicReference[List[Runnable]] = new AtomicReference(Nil)
    def enqueue(runnable: Runnable): Unit = {
      var oldValue: List[Runnable] = Nil
      var newValue: List[Runnable] = Nil
      do {
        oldValue = ref.get()
        newValue = oldValue :+ runnable
      } while (!ref.compareAndSet(oldValue, newValue))
    }

    /**
     * @return null if empty
     */
    def dequeue(): Runnable = {
      var item: Runnable           = null
      var oldValue: List[Runnable] = Nil
      var newValue: List[Runnable] = Nil
      do {
        oldValue = ref.get()
        if (!oldValue.isEmpty) {
          newValue = oldValue.tail
          item = oldValue.head
        } else {
          item = null
        }
      } while (!oldValue.isEmpty && !ref.compareAndSet(oldValue, newValue))
      item
    }

    def isEmpty: scala.Boolean = ref.get.isEmpty
  }

  private object QueueExecutionContext extends ExecutionContextExecutor {
    def execute(runnable: Runnable): Unit = {
      queue enqueue runnable
      threads
      parking.synchronized {
        parking.notify()
      }
    }
    def reportFailure(t: Throwable): Unit = t.printStackTrace()
  }

  private val queue: Queue      = new Queue
  private val parking: Object   = new Object
  private val numExecutors: Int = Runtime.getRuntime.availableProcessors()
  private val threadGroup       = new ThreadGroup("Executor Thread Group")
  threadGroup.setDaemon(true)
  private lazy val threads =
    scala.collection.immutable.Vector.tabulate(numExecutors)(i =>
      new ExecutorThread(i))

  private class ExecutorThread(id: scala.Int)
      extends Thread(threadGroup, "Executor-" + id) {
    override def run(): Unit = {
      while (true) {
        val runnable = queue.dequeue()
        if (runnable != null) {
          try {
            runnable.run()
          } catch {
            case t: Throwable =>
              QueueExecutionContext.reportFailure(t)
          }
        } else {
          parking.synchronized {
            if (queue.isEmpty) {
              parking.wait()
            }
          }
        }

      }
    }
  }
}
