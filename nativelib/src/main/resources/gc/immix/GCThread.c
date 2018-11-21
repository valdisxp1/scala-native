#include "GCThread.h"
#include "Constants.h"
#include <semaphore.h>

void *GCThread_loop(void *arg) {
    GCThread *thread = (GCThread *) arg;
    Heap *heap = thread->heap;
    sem_t *start = &heap->gcThreads.start;
    while (true) {
        thread->active = false;
        sem_wait(start);
        thread->active = true;

        thread->sweep.cursorDone = 0;

        while (!Heap_IsSweepDone(heap)) {
            Heap_Sweep(heap, &thread->sweep.cursorDone, SWEEP_BATCH_SIZE);
            Heap_LazyCoalesce(heap);
        }
    }
    return NULL;
}

void GCThread_Init(GCThread *thread, int id, Heap *heap) {
   thread->id = id;
   thread->heap = heap;
   thread->active = false;

   pthread_create(&thread->self, NULL, GCThread_loop, (void *) thread);
}