#include "State.h"

Heap heap;
GCThread *gcThreads = NULL;
Stack stack;
Allocator allocator;
LargeAllocator largeAllocator;
BlockAllocator blockAllocator;

// For stackoverflow handling
bool overflow = false;
