#include "GCThread.h"
#include "Constants.h"
#include "Sweeper.h"
#include "Marker.h"
#include <semaphore.h>

static inline void GCThread_mark(GCThread *thread, Heap *heap, Stats *stats) {
    uint64_t start_ns, end_ns;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
    }
    while (!Marker_IsMarkDone(heap)) {
        Marker_Mark(heap);
    }
    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordEvent(stats, event_concurrent_mark, thread->id,
                          start_ns, end_ns);
    }
}

static inline void GCThread_sweep(GCThread *thread, Heap *heap, Stats *stats) {
    thread->sweep.cursorDone = 0;
    uint64_t start_ns, end_ns;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
    }
    while (!Sweeper_IsSweepDone(heap)) {
        Sweeper_Sweep(heap, &thread->sweep.cursorDone, SWEEP_BATCH_SIZE);
        Sweeper_LazyCoalesce(heap);
    }
    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordEvent(stats, event_concurrent_sweep, thread->id,
                          start_ns, end_ns);
    }
}

void *GCThread_loop(void *arg) {
    GCThread *thread = (GCThread *)arg;
    Heap *heap = thread->heap;
    sem_t *start = &heap->gcThreads.start;
    Stats *stats = heap->stats;
    while (true) {
        thread->active = false;
        sem_wait(start);
        thread->active = true;

        uint8_t phase = heap->gcThreads.phase;
        switch (phase) {
            case gc_idle:
                break;
            case gc_mark:
                GCThread_mark(thread, heap, stats);
                break;
            case gc_sweep:
                GCThread_sweep(thread, heap, stats);
                break;
        }
    }
    return NULL;
}

void GCThread_Init(GCThread *thread, int id, Heap *heap) {
    thread->id = id;
    thread->heap = heap;
    thread->active = false;
    // we do not use the pthread value
    pthread_t self;

    pthread_create(&self, NULL, GCThread_loop, (void *)thread);
}

bool GCThread_AnyActive(Heap *heap) {
    int gcThreadCount = heap->gcThreads.count;
    GCThread *gcThreads = (GCThread *) heap->gcThreads.all;
    bool anyActive = false;
    for (int i = 0; i < gcThreadCount; i++) {
        if (gcThreads[i].active) {
            return true;
        }
    }
    return false;
}