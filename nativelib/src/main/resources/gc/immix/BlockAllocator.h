#ifndef IMMIX_BLOCKALLOCATOR_H
#define IMMIX_BLOCKALLOCATOR_H

#include "datastructures/SuperblockList.h"
#include "Constants.h"
#include <stddef.h>

#define SUPERBLOCK_LIST_SIZE (BLOCK_COUNT_BITS + 1)

typedef struct {
    struct {
        BlockMeta *cursor;
        BlockMeta *limit;
    } smallestSuperblock;
    SuperblockList freeSuperblocks[SUPERBLOCK_LIST_SIZE];
    int minNonEmptyIndex;
    int maxNonEmptyIndex;
    uint64_t freeBlockCount;
} BlockAllocator;

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount);
BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator);
void BlockAllocator_AddFreeBlock(BlockAllocator *blockAllocator, BlockMeta *block);
void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count);
void BlockAllocator_Clear(BlockAllocator *blockAllocator);

#endif // IMMIX_BLOCKALLOCATOR_H