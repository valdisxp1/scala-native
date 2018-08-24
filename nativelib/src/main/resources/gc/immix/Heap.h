#ifndef IMMIX_HEAP_H
#define IMMIX_HEAP_H

#include "GCTypes.h"
#include "Allocator.h"
#include "LargeAllocator.h"
#include "datastructures/Stack.h"
#include <stdatomic.h>
#include <pthread.h>

#define SWEEP_PROCESSES_POST_ACTIONS_STARTED -100
#define SWEEP_PROCESSES_POST_ACTIONS_DONE -200

typedef struct {
    size_t memoryLimit;
    word_t *heapStart;
    word_t *heapEnd;
    struct {
        atomic_long cursor;
        atomic_int processCount;
        pthread_mutex_t startMutex;
        pthread_mutex_t postMutex;
        pthread_cond_t start;
        pthread_cond_t processStopped; // uses postMutex
        pthread_t thread;
    } sweep;
    size_t smallHeapSize;
    word_t *largeHeapStart;
    word_t *largeHeapEnd;
    size_t largeHeapSize;
} Heap;

static inline bool Heap_IsWordInLargeHeap(Heap *heap, word_t *word) {
    return word != NULL && word >= heap->largeHeapStart &&
           word < heap->largeHeapEnd;
}

static inline bool Heap_IsWordInSmallHeap(Heap *heap, word_t *word) {
    return word != NULL && word >= heap->heapStart && word < heap->heapEnd;
}

static inline bool Heap_IsWordInHeap(Heap *heap, word_t *word) {
    return Heap_IsWordInSmallHeap(heap, word) ||
           Heap_IsWordInLargeHeap(heap, word);
}

static inline bool Heap_IsSweepDone(Heap *heap) {
    return ((word_t *)heap->sweep.cursor) >= heap->heapEnd;
}

static inline bool heap_isObjectInHeap(Heap *heap, Object *object) {
    return Heap_IsWordInHeap(heap, (word_t *)object);
}

void Heap_Init(Heap *heap, size_t initialSmallHeapSize,
               size_t initialLargeHeapSize);
word_t *Heap_Alloc(Heap *heap, uint32_t objectSize);
word_t *Heap_AllocSmall(Heap *heap, uint32_t objectSize);
word_t *Heap_AllocLarge(Heap *heap, uint32_t objectSize);

void Heap_Collect(Heap *heap, Stack *stack);

void Heap_Recycle(Heap *heap);
word_t *Heap_LazySweep(Heap *heap, uint32_t size);
void Heap_SweepDone(Heap *heap);
void Heap_Grow(Heap *heap, size_t increment);
void Heap_GrowLarge(Heap *heap, size_t increment);

#endif // IMMIX_HEAP_H
