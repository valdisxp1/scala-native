#include "GCThread.h"
#include "Constants.h"

void *GCThread_loop(void *arg) {
    GCThread *thread = (GCThread *) arg;
    Heap *heap = thread->heap;
    pthread_mutex_t *startMutex = &heap->gcThreads.startMutex;
    pthread_cond_t *start = &heap->gcThreads.start;
    while (true) {
        thread->active = false;
        pthread_mutex_lock(startMutex);
        pthread_cond_wait(start, startMutex);
        pthread_mutex_unlock(startMutex);
        thread->active = true;

        thread->sweep.cursorDone = 0;

        while (!Heap_IsSweepDone(heap)) {
            Heap_Sweep(heap, &thread->sweep.cursorDone, SWEEP_BATCH_SIZE);
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