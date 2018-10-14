#ifndef IMMIX_HEAP_H
#define IMMIX_HEAP_H

#include "GCTypes.h"
#include "Allocator.h"
#include "LargeAllocator.h"
#include "datastructures/Stack.h"
#include "datastructures/Bytemap.h"
#include "headers/LineHeader.h"
#include <stdio.h>

typedef struct {
    FILE *outFile;
    uint64_t collections;
    uint64_t timestamp_us[GC_STATS_MEASUREMENTS];
    uint64_t mark_time_us[GC_STATS_MEASUREMENTS];
    uint64_t sweep_time_us[GC_STATS_MEASUREMENTS];
} HeapStats;

typedef struct {
    size_t memoryLimit;
    word_t *blockHeaderStart;
    word_t *blockHeaderEnd;
    word_t *lineHeaderStart;
    word_t *lineHeaderEnd;
    word_t *heapStart;
    word_t *heapEnd;
    size_t smallHeapSize;
    word_t *largeHeapStart;
    word_t *largeHeapEnd;
    size_t largeHeapSize;
    Bytemap *smallBytemap;
    Bytemap *largeBytemap;
    HeapStats *stats;
} Heap;

extern Heap heap;

static inline bool Heap_IsWordInLargeHeap(word_t *word) {
    return word >= heap.largeHeapStart && word < heap.largeHeapEnd;
}

static inline bool Heap_IsWordInSmallHeap(word_t *word) {
    return word >= heap.heapStart && word < heap.heapEnd;
}

static inline bool Heap_IsWordInHeap(word_t *word) {
    return Heap_IsWordInSmallHeap(word) ||
           Heap_IsWordInLargeHeap(word);
}

static inline Bytemap *Heap_BytemapForWord(word_t *word) {
    if (Heap_IsWordInSmallHeap(word)) {
        return heap.smallBytemap;
    } else if(Heap_IsWordInLargeHeap(word)) {
        return heap.largeBytemap;
    } else {
        return NULL;
    }
}

static inline LineHeader *Heap_LineHeaderForWord(word_t *word) {
    // assumes there are no gaps between lines
    assert(LINE_COUNT * LINE_SIZE == BLOCK_TOTAL_SIZE);
    assert(Heap_IsWordInSmallHeap(word));
    word_t lineGlobalIndex = ((word_t)word - (word_t)heap.heapStart) >> LINE_SIZE_BITS;
    assert(lineGlobalIndex >= 0);
    LineHeader *lineHeader = (LineHeader *) heap.lineHeaderStart + lineGlobalIndex;
    assert(lineHeader < (LineHeader *) heap.lineHeaderEnd);
    return lineHeader;
}


void Heap_Init(size_t initialSmallHeapSize,
               size_t initialLargeHeapSize);
void Heap_AfterExit();
word_t *Heap_Alloc(uint32_t objectSize);
word_t *Heap_AllocSmall(uint32_t objectSize);
word_t *Heap_AllocLarge(uint32_t objectSize);

void Heap_Collect(Stack *stack);

void Heap_Recycle();
void Heap_Grow(size_t increment);
void Heap_GrowLarge(size_t increment);

#endif // IMMIX_HEAP_H
