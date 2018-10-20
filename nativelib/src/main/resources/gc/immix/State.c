#include "State.h"

Heap heap;
Stack stack;
Allocator allocator;
LargeAllocator largeAllocator;
BlockAllocator blockAllocator;

// For stackoverflow handling
bool overflow = false;
BlockMeta *currentOverflowBlock = NULL;
word_t *currentOverflowAddress = NULL;
