#ifndef IMMIX_HEAP_H
#define IMMIX_HEAP_H

#include "GCTypes.h"
#include "Allocator.h"
#include "LargeAllocator.h"
#include "datastructures/Stack.h"
#include "datastructures/Bytemap.h"
#include "metadata/LineMeta.h"
#include "Stats.h"
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>

#define SWEEP_DONE ~((uint32_t)0)

typedef struct {
    word_t *blockMetaStart;
    word_t *blockMetaEnd;
    word_t *lineMetaStart;
    word_t *lineMetaEnd;
    word_t *heapStart;
    word_t *heapEnd;
    size_t heapSize;
    size_t maxHeapSize;
    uint32_t blockCount;
    uint32_t maxBlockCount;
    struct {
        pthread_mutex_t startMutex;
        pthread_cond_t start;
    } gcThreads;
    int gcThreadCount;
    struct {
        atomic_uint_fast32_t cursor;
        atomic_uint_fast32_t limit;
        // making cursorDone atomic so it keeps sequential consistency with the other atomics
        atomic_uint_fast32_t cursorDone;
    } sweep;
    struct {
        atomic_uint_fast32_t cursor;
        // making cursorDone atomic so it keeps sequential consistency with the other atomics
        atomic_uint_fast32_t cursorDone;
    } coalesce;
    bool postSweepDone;
    Bytemap *bytemap;
    Stats *stats;
} Heap;

static inline bool Heap_IsWordInHeap(Heap *heap, word_t *word) {
    return word >= heap->heapStart && word < heap->heapEnd;
}

static inline LineMeta *Heap_LineMetaForWord(Heap *heap, word_t *word) {
    // assumes there are no gaps between lines
    assert(LINE_COUNT * LINE_SIZE == BLOCK_TOTAL_SIZE);
    assert(Heap_IsWordInHeap(heap, word));
    word_t lineGlobalIndex =
        ((word_t)word - (word_t)heap->heapStart) >> LINE_SIZE_BITS;
    assert(lineGlobalIndex >= 0);
    LineMeta *lineMeta = (LineMeta *)heap->lineMetaStart + lineGlobalIndex;
    assert(lineMeta < (LineMeta *)heap->lineMetaEnd);
    return lineMeta;
}

static inline bool Heap_IsSweepDone(Heap *heap) {
    return heap->coalesce.cursorDone >= heap->blockCount;
}

void Heap_Init(Heap *heap, size_t minHeapSize, size_t maxHeapSize);
word_t *Heap_Alloc(Heap *heap, uint32_t objectSize);
word_t *Heap_AllocSmall(Heap *heap, uint32_t objectSize);
word_t *Heap_AllocLarge(Heap *heap, uint32_t objectSize);

void Heap_Collect(Heap *heap, Stack *stack);

void Heap_Recycle(Heap *heap);
void Heap_Sweep(Heap *heap, atomic_uint_fast32_t *cursorDone, uint32_t maxCount);
void Heap_Grow(Heap *heap, uint32_t increment);

#endif // IMMIX_HEAP_H
