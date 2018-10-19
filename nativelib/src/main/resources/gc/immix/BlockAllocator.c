#include "BlockAllocator.h"
#include "Log.h"
#include <stdio.h>

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount) {
    SuperblockList_Init(&blockAllocator->freeSuperblocks, blockMetaStart);
    BlockAllocator_Clear(blockAllocator);
    BlockAllocator_AddFreeBlocks(blockAllocator, (BlockMeta *) blockMetaStart, blockCount);
}

BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator) {
    if (blockAllocator->smallestSuperblock.cursor >= blockAllocator->smallestSuperblock.limit) {
        BlockMeta *superblock = SuperblockList_Poll(&blockAllocator->freeSuperblocks);
        if (superblock != NULL) {
            blockAllocator->smallestSuperblock.cursor = superblock;
            blockAllocator->smallestSuperblock.limit = superblock + superblock->superblockSize;
            // it might be safe to remove this
            superblock->superblockSize = 0;
        } else {
            assert(block0 == NULL);
            return NULL;
        }
    }
    BlockMeta *block = blockAllocator->smallestSuperblock.cursor;
    blockAllocator->smallestSuperblock.cursor++;

    // not decrementing freeBlockCount, because it is only used after sweep
    return block;
}

void BlockAllocator_AddFreeBlock(BlockAllocator *blockAllocator, BlockMeta *block) {
    block->superblockSize = 1;
    SuperblockList_AddLast(&blockAllocator->freeSuperblocks, block);
    blockAllocator->freeBlockCount++;
}

void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count) {
    block->superblockSize = count;
    SuperblockList_AddLast(&blockAllocator->freeSuperblocks, block);
    blockAllocator->freeBlockCount += count;
}

void BlockAllocator_Clear(BlockAllocator *blockAllocator) {
    SuperblockList_Clear(&blockAllocator->freeSuperblocks);
    SuperblockList_Clear(&blockAllocator->freeSuperblocks);
    blockAllocator->freeBlockCount = 0;
    blockAllocator->smallestSuperblock.cursor = NULL;
    blockAllocator->smallestSuperblock.limit = NULL;
}