#ifndef IMMIX_BLOCKALLOCATOR_H
#define IMMIX_BLOCKALLOCATOR_H

#include "datastructures/BlockList.h"
#include "datastructures/SuperblockList.h"
#include <stddef.h>

typedef struct {
    BlockList freeBlocks;
    struct {
        BlockMeta *cursor;
        BlockMeta *limit;
    } currentSuperblock;
    SuperblockList freeSuperblocks;
    uint64_t freeBlockCount;
} BlockAllocator;

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount);
BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator);
void BlockAllocator_AddFreeBlock(BlockAllocator *blockAllocator, BlockMeta *block);
void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count);
void BlockAllocator_Clear(BlockAllocator *blockAllocator);

#endif // IMMIX_BLOCKALLOCATOR_H