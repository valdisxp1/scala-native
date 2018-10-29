#ifndef IMMIX_BLOCKALLOCATOR_H
#define IMMIX_BLOCKALLOCATOR_H

#include "datastructures/BlockList.h"
#include "datastructures/BlockRange.h"
#include "Constants.h"
#include <stddef.h>

#define SUPERBLOCK_LIST_SIZE (BLOCK_COUNT_BITS + 1)

typedef struct {
    struct {
        BlockMeta *cursor;
        BlockMeta *limit;
    } smallestSuperblock;
    BlockRange smallestSuperblock0;
    int minNonEmptyIndex;
    int maxNonEmptyIndex;
    uint32_t freeBlockCount;
    struct {
        BlockMeta *first;
        BlockMeta *limit;
    } coalescingSuperblock;
    BlockRange coalescingSuperblock0;
    BlockList freeSuperblocks[SUPERBLOCK_LIST_SIZE];
} BlockAllocator;

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart,
                         uint32_t blockCount);
BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator);
BlockMeta *BlockAllocator_GetFreeSuperblock(BlockAllocator *blockAllocator,
                                            uint32_t size);
void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator,
                                  BlockMeta *block, uint32_t count);
void BlockAllocator_Clear(BlockAllocator *blockAllocator);

#endif // IMMIX_BLOCKALLOCATOR_H