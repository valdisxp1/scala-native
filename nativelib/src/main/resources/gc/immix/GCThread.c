#include "GCThread.h"
#include "Constants.h"

void GCThread_loop(void *arg) {
    GCThread *thread = (GCThread *) arg;
    Heap *heap = thread->heap;
    pthread_mutex_t *startMutex = &heap->gcThreads.startMutex;
    pthread_cond_t *start = &heap->gcThreads.start;
    while (true) {
        pthead_mutex_lock(startMutex);
        pthread_cond_wait(start, startMutex);
        pthead_mutex_unlock(startMutex);

        while (!Heap_IsSweepDone(heap)) {
            Heap_Sweep(heap, &thread->sweep.cursorDone, SWEEP_BATCH_SIZE);
        }
    }
}

void GCThread_Init(GCThread *thread, int id, Heap *heap) {
   thread->id = id;
   thread->heap = heap;

   pthread_create(thread->self, NULL, GCThread_loop, (void *) thread);
}