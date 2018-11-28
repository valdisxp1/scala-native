#ifndef IMMIX_STATE_H
#define IMMIX_STATE_H

#include "Heap.h"

extern Heap heap;
#include "GCThread.h"
extern GCThread *gcThreads;
extern Stack stack;
extern Allocator allocator;
extern LargeAllocator largeAllocator;
extern BlockAllocator blockAllocator;

#endif // IMMIX_STATE_H
