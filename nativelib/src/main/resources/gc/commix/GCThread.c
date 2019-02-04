#include "GCThread.h"
#include "Constants.h"
#include "Sweeper.h"
#include "Marker.h"
#include "Phase.h"
#include <semaphore.h>

static inline void GCThread_markMaster(Heap *heap, Stats *stats) {
#ifdef ENABLE_GC_STATS
    uint64_t start_ns, end_ns;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
        stats->mark_waiting_start_ns = 0;
        stats->mark_waiting_end_ns = 0;
    }
#endif

    while (!Marker_IsMarkDone(heap)) {
        Marker_MarkAndScale(heap, stats);
        if (!Marker_IsMarkDone(heap)) {
            sched_yield();
        }
    }

#ifdef ENABLE_GC_STATS
    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordEvent(stats, event_concurrent_mark,
                          start_ns, end_ns);
#ifdef ENABLE_GC_STATS_SYNC
        if (stats->mark_waiting_start_ns != 0) {
            Stats_RecordEvent(stats, mark_waiting, stats->mark_waiting_start_ns, stats->mark_waiting_end_ns);
        }
#endif // ENABLE_GC_STATS_SYNC
    }
#endif // ENABLE_GC_STATS_BATCHES
}

static inline void GCThread_mark(Heap *heap, Stats *stats) {
#ifdef ENABLE_GC_STATS
    uint64_t start_ns, end_ns;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
        stats->mark_waiting_start_ns = 0;
        stats->mark_waiting_end_ns = 0;
    }
#endif

    Marker_Mark(heap, stats);

#ifdef ENABLE_GC_STATS
    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordEvent(stats, event_concurrent_mark,
                          start_ns, end_ns);
#ifdef ENABLE_GC_STATS_SYNC
        if (stats->mark_waiting_start_ns != 0) {
            Stats_RecordEvent(stats, mark_waiting, stats->mark_waiting_start_ns, stats->mark_waiting_end_ns);
        }
#endif // ENABLE_GC_STATS_SYNC
    }
#endif // ENABLE_GC_STATS_BATCHES
}

static inline void GCThread_sweep(GCThread *thread, Heap *heap, Stats *stats) {
    thread->sweep.cursorDone = 0;
#ifdef ENABLE_GC_STATS
    uint64_t start_ns, end_ns;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
    }
#endif

    while (heap->sweep.cursor < heap->sweep.limit) {
        Sweeper_Sweep(heap, stats, &thread->sweep.cursorDone, SWEEP_BATCH_SIZE);
    }
    thread->sweep.cursorDone = heap->sweep.limit;

#ifdef ENABLE_GC_STATS
    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordEvent(stats, event_concurrent_sweep, start_ns, end_ns);
    }
#endif
}

static inline void GCThread_sweepMaster(GCThread *thread, Heap *heap, Stats *stats) {
    thread->sweep.cursorDone = 0;
#ifdef ENABLE_GC_STATS
    uint64_t start_ns, end_ns;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
    }
#endif

    while (heap->sweep.cursor < heap->sweep.limit) {
        Sweeper_Sweep(heap, stats, &thread->sweep.cursorDone, SWEEP_BATCH_SIZE);
        Sweeper_LazyCoalesce(heap, stats);
    }
    thread->sweep.cursorDone = heap->sweep.limit;
    while (!Sweeper_IsCoalescingDone(heap)) {
        Sweeper_LazyCoalesce(heap, stats);
    }
    if (!heap->sweep.postSweepDone) {
        Phase_SweepDone(heap, stats);
    }
#ifdef ENABLE_GC_STATS
    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordEvent(stats, event_concurrent_sweep, start_ns, end_ns);
    }
#endif
}

void *GCThread_loop(void *arg) {
    GCThread *thread = (GCThread *)arg;
    Heap *heap = thread->heap;
    sem_t *start = &heap->gcThreads.startWorkers;
#ifdef ENABLE_GC_STATS
    Stats *stats = thread->stats;
#else
    Stats *stats = NULL;
#endif
    while (true) {
        thread->active = false;
        sem_wait(start);
        // hard fence before proceeding with the next phase
        atomic_thread_fence(memory_order_seq_cst);
        thread->active = true;

        uint8_t phase = heap->gcThreads.phase;
        switch (phase) {
            case gc_idle:
                break;
            case gc_mark:
                GCThread_mark(heap, stats);
                break;
            case gc_sweep:
                GCThread_sweep(thread, heap, stats);
#ifdef ENABLE_GC_STATS
                if (stats != NULL) {
                    Stats_WriteToFile(stats);
                }
#endif
                break;
        }
        // hard fence before proceeding with the next phase
        atomic_thread_fence(memory_order_seq_cst);
    }
    return NULL;
}

void *GCThread_loopMaster(void *arg) {
    GCThread *thread = (GCThread *)arg;
    Heap *heap = thread->heap;
    sem_t *start = &heap->gcThreads.startMaster;
#ifdef ENABLE_GC_STATS
    Stats *stats = thread->stats;
#else
    Stats *stats = NULL;
#endif
    while (true) {
        thread->active = false;
        sem_wait(start);
        // hard fence before proceeding with the next phase
        atomic_thread_fence(memory_order_seq_cst);
        thread->active = true;

        uint8_t phase = heap->gcThreads.phase;
        switch (phase) {
            case gc_idle:
                break;
            case gc_mark:
                GCThread_markMaster(heap, stats);
                break;
            case gc_sweep:
                GCThread_sweepMaster(thread, heap, stats);
#ifdef ENABLE_GC_STATS
                if (stats != NULL) {
                    Stats_WriteToFile(stats);
                }
#endif
                break;
        }
        // hard fence before proceeding with the next phase
        atomic_thread_fence(memory_order_seq_cst);
    }
    return NULL;
}

void GCThread_Init(GCThread *thread, int id, Heap *heap, Stats *stats) {
    thread->id = id;
    thread->heap = heap;
    thread->stats = stats;
    thread->active = false;
    // we do not use the pthread value
    pthread_t self;

    if (id == 0) {
        pthread_create(&self, NULL, GCThread_loopMaster, (void *)thread);
    } else {
        pthread_create(&self, NULL, GCThread_loop, (void *)thread);
    }
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

int GCThread_ActiveCount(Heap *heap) {
    int gcThreadCount = heap->gcThreads.count;
    GCThread *gcThreads = (GCThread *) heap->gcThreads.all;
    int count = 0;
    for (int i = 0; i < gcThreadCount; i++) {
        if (gcThreads[i].active) {
            count += 1;
        }
    }
    return count;
}

NOINLINE void GCThread_joinAllSlow(GCThread *gcThreads, int gcThreadCount) {
    // extremely unlikely to enter here
    // unless very many threads running
    bool anyActive = true;
    while (anyActive) {
        sched_yield();
        anyActive = false;
        for (int i = 0; i < gcThreadCount; i++) {
            anyActive |= gcThreads[i].active;
        }
    }
}

INLINE void GCThread_JoinAll(Heap *heap) {
    // semaphore drain - make sure no new threads are started
    Phase_Set(heap, gc_idle);
    sem_t *startMaster = &heap->gcThreads.startMaster;
    sem_t *startWorkers = &heap->gcThreads.startWorkers;
    while (!sem_trywait(startMaster)){}
    while (!sem_trywait(startWorkers)){}

    int gcThreadCount = heap->gcThreads.count;
    GCThread *gcThreads = (GCThread *) heap->gcThreads.all;
    bool anyActive = false;
    for (int i = 0; i < gcThreadCount; i++) {
        anyActive |= gcThreads[i].active;
    }
    if (anyActive) {
        GCThread_joinAllSlow(gcThreads, gcThreadCount);
    }
}


INLINE void GCThread_WakeMaster(Heap *heap) {
    sem_post(&heap->gcThreads.startMaster);
}

INLINE void GCThread_WakeWorkers(Heap *heap, int toWake) {
    sem_t *startWorkers = &heap->gcThreads.startWorkers;
    for (int i = 0; i < toWake; i++) {
        sem_post(startWorkers);
    }
}

INLINE void GCThread_Wake(Heap *heap, int toWake) {
    if (toWake > 0) {
        GCThread_WakeMaster(heap);
    }
    GCThread_WakeWorkers(heap, toWake - 1);
}