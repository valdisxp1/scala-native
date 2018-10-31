#ifndef IMMIX_BLOCK_H
#define IMMIX_BLOCK_H

#include "metadata/BlockMeta.h"
#include "Heap.h"

int Block_Recycle(Allocator *allocator, BlockMeta *block, word_t *blockStart,
                   LineMeta *lineMetas);
#endif // IMMIX_BLOCK_H
