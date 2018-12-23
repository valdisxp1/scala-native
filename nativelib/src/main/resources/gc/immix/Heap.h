#ifndef IMMIX_HEAP_H
#define IMMIX_HEAP_H

#include "GCTypes.h"
#include "Allocator.h"
#include "LargeAllocator.h"
#include "datastructures/Stack.h"
#include "datastructures/Bytemap.h"
#include "datastructures/BlockRange.h"
#include "metadata/LineMeta.h"
#include "Stats.h"
#include <stdio.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

typedef enum {
    gc_idle = 0x0,
    gc_sweep = 0x2
} GCThreadPhase;

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
        sem_t start;
        atomic_uint_fast8_t phase;
        int count;
        void *all;
    } gcThreads;
    struct {
        atomic_uint_fast32_t cursor;
        atomic_uint_fast32_t limit;
        BlockRange coalesce; // _First = cursorDone, _Limit = cursor
        atomic_uint_fast32_t coalesceDone;
        atomic_bool postSweepDone;
    } sweep;
    struct {
        // making cursorDone atomic so it keeps sequential consistency with the
        // other atomics
        atomic_uint_fast32_t cursorDone;
    } lazySweep;
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

void Heap_Init(Heap *heap, size_t minHeapSize, size_t maxHeapSize);
word_t *Heap_Alloc(Heap *heap, uint32_t objectSize);
word_t *Heap_AllocSmall(Heap *heap, uint32_t objectSize);
word_t *Heap_AllocLarge(Heap *heap, uint32_t objectSize);

void Heap_Collect(Heap *heap, Stack *stack);
void Heap_Recycle(Heap *heap);
void Heap_GrowIfNeeded(Heap *heap);
void Heap_Grow(Heap *heap, uint32_t increment);

#endif // IMMIX_HEAP_H
