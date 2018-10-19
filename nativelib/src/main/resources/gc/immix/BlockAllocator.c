#include "BlockAllocator.h"
#include "Log.h"
#include "utils/MathUtils.h"
#include <stdio.h>

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount) {
    for (int i = 0; i < SUPERBLOCK_LIST_SIZE; i++) {
        SuperblockList_Init(&blockAllocator->freeSuperblocks[i], blockMetaStart);
    }
    BlockAllocator_Clear(blockAllocator);
    BlockAllocator_AddFreeBlocks(blockAllocator, (BlockMeta *) blockMetaStart, blockCount);
}

inline static int BlockAllocator_sizeToLinkedListIndex(uint32_t size) {
    int result = MathUtils_log2_floor((size_t) size);
    assert(result >= 0);
    assert(result < SUPERBLOCK_LIST_SIZE);
    return result;
}

inline static BlockMeta *BlockAllocator_pollSuperblock(BlockAllocator *blockAllocator, int first) {
//    int target = BlockAllocator_sizeToLinkedListIndex(count);
//    int minNonEmptyIndex = blockAllocator->minNonEmptyIndex;
//    int first = (minNonEmptyIndex > target) ? minNonEmptyIndex : target;
    int maxNonEmptyIndex = blockAllocator->maxNonEmptyIndex;
    for (int i = first; i <= maxNonEmptyIndex; i++) {
        BlockMeta *superblock = SuperblockList_Poll(&blockAllocator->freeSuperblocks[i]);
        if (superblock != NULL) {
            return superblock;
        } else {
            blockAllocator->minNonEmptyIndex = i + 1;
        }
    }
    return NULL;
}

BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator) {
    if (blockAllocator->smallestSuperblock.cursor >= blockAllocator->smallestSuperblock.limit) {
        BlockMeta *superblock = BlockAllocator_pollSuperblock(blockAllocator, blockAllocator->minNonEmptyIndex);
        if (superblock != NULL) {
            blockAllocator->smallestSuperblock.cursor = superblock;
            blockAllocator->smallestSuperblock.limit = superblock + superblock->superblockSize;
            // it might be safe to remove this
            superblock->superblockSize = 0;
        } else {
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
    int i = 0;
    blockAllocator->minNonEmptyIndex = i;
    if (i > blockAllocator->maxNonEmptyIndex) {
        blockAllocator->maxNonEmptyIndex = i;
    }
    SuperblockList_AddLast(&blockAllocator->freeSuperblocks[i], block);
    blockAllocator->freeBlockCount++;
}

void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count) {
    block->superblockSize = count;
    int i = BlockAllocator_sizeToLinkedListIndex(count);
    if (i < blockAllocator->minNonEmptyIndex) {
        blockAllocator->minNonEmptyIndex = i;
    }
    if (i > blockAllocator->maxNonEmptyIndex) {
            blockAllocator->maxNonEmptyIndex = i;
    }
    SuperblockList_AddLast(&blockAllocator->freeSuperblocks[i], block);
    blockAllocator->freeBlockCount += count;
}

void BlockAllocator_Clear(BlockAllocator *blockAllocator) {
    for (int i = 0; i < SUPERBLOCK_LIST_SIZE; i++) {
        SuperblockList_Clear(&blockAllocator->freeSuperblocks[i]);
    }
    blockAllocator->freeBlockCount = 0;
    blockAllocator->smallestSuperblock.cursor = NULL;
    blockAllocator->smallestSuperblock.limit = NULL;
    blockAllocator->minNonEmptyIndex = SUPERBLOCK_LIST_SIZE;
    blockAllocator->maxNonEmptyIndex = -1;
}