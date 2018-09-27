#ifndef IMMIX_ALLOCATOR_H
#define IMMIX_ALLOCATOR_H

#include "GCTypes.h"
#include <stddef.h>
#include "datastructures/BlockList.h"
#include "datastructures/Bytemap.h"
#include "datastructures/Stack.h"

typedef struct {
    word_t *blockMetaStart;
    Bytemap *bytemap;
    word_t *heapStart;
    uint64_t blockCount;
    uint64_t recycledBlockCount;
    BlockList freeBlocks;
    uint64_t freeBlockCount;
    uint64_t youngBlockCount;
    uint64_t oldBlockCount;
    BlockMeta *block;
    word_t *blockStart;
    word_t *cursor;
    word_t *limit;
    BlockMeta *pretenuredBlock;
    word_t *pretenuredBlockStart;
    word_t *pretenuredCursor;
    word_t *pretenuredLimit;
    BlockMeta *largeBlock;
    word_t *largeBlockStart;
    word_t *largeCursor;
    word_t *largeLimit;
    size_t freeMemoryAfterCollection;
    Stack *rememberedObjects;
    Stack *rememberedYoungObjects;
} Allocator;

void Allocator_Init(Allocator *allocator, Bytemap *bytemap,
                    word_t *blockMetaStart, word_t *heapStart,
                    uint32_t blockCount);
bool Allocator_CanInitCursors(Allocator *allocator);
void Allocator_InitCursors(Allocator *allocator);
word_t *Allocator_Alloc(Allocator *allocator, size_t size);
word_t *Allocator_AllocPretenured(Allocator *allocator, size_t size);

bool Allocator_ShouldGrow(Allocator *allocator);

#endif // IMMIX_ALLOCATOR_H
