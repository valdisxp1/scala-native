#ifndef IMMIX_SWEEPER_H
#define IMMIX_SWEEPER_H

#include "Heap.h"
#include "datastructures/BlockRange.h"
#include "SweepResult.h"

void Sweeper_Sweep(Heap *heap, BlockRange *cursorDone,
                   uint32_t maxCount);
void Sweeper_LazyCoalesce(Heap *heap);
Object *Sweeper_LazySweep(Heap *heap, uint32_t size);
Object *Sweeper_LazySweepLarge(Heap *heap, uint32_t size);

static inline bool Sweeper_IsSweepDone(Heap *heap) {
    return heap->sweep.coalesceDone >= heap->sweep.limit;
}

#endif // IMMIX_SWEEPER_H
