#ifndef IMMIX_GCTHREAD_H
#define IMMIX_GCTHREAD_H

#include "Heap.h"
#include <stdatomic.h>
#include <pthread.h>

typedef struct {
    int id;
    pthread_t self;
    Heap *heap;
    pthread_mutex_t *startMutex;
    pthread_cond_t *start;
    struct {
        // making cursorDone atomic so it keeps sequential consistency with the other atomics
        atomic_uint_fast32_t cursorDone;
    } sweep;
} GCThread;

void GCThread_Init(GCThread *thread, int id, Heap *heap);

#endif // IMMIX_GCTHREAD_H