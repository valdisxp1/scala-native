#include "BlockAllocator.h"
#include "Log.h"
#include "utils/MathUtils.h"
#include <stdio.h>

void BlockAllocator_addFreeBlocksInternal(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count);

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount) {
    for (int i = 0; i < SUPERBLOCK_LIST_SIZE; i++) {
        BlockList_Init(&blockAllocator->freeSuperblocks[i], blockMetaStart);
    }
    blockAllocator->blockCount = blockCount;
    BlockAllocator_Clear(blockAllocator);
    BlockAllocator_addFreeBlocksInternal(blockAllocator, (BlockMeta *) blockMetaStart, blockCount);
}

inline static int BlockAllocator_sizeToLinkedListIndex(uint32_t size) {
    int result = MathUtils_Log2Floor((size_t) size);
    assert(result >= 0);
    assert(result < SUPERBLOCK_LIST_SIZE);
    return result;
}

inline static BlockMeta *BlockAllocator_pollSuperblock(BlockAllocator *blockAllocator, int first) {
    int maxNonEmptyIndex = blockAllocator->maxNonEmptyIndex;
    for (int i = first; i <= maxNonEmptyIndex; i++) {
        BlockMeta *superblock = BlockList_Poll(&blockAllocator->freeSuperblocks[i]);
        if (superblock != NULL) {
            assert(BlockMeta_SuperblockSize(superblock) > 0);
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
            blockAllocator->smallestSuperblock.limit = superblock + BlockMeta_SuperblockSize(superblock);
            // it might be safe to remove this
            // not using BlockMeta_SetSuperblockSize, because it is just empty space
            superblock->superblockSize = 0;
        } else {
            return NULL;
        }
    }
    BlockMeta *block = blockAllocator->smallestSuperblock.cursor;
    blockAllocator->smallestSuperblock.cursor++;

    // not decrementing freeBlockCount, because it is only used after sweep
//    printf("GOT block %p\n", block);
//    fflush(stdout);
    return block;
}

BlockMeta *BlockAllocator_GetFreeSuperblock(BlockAllocator *blockAllocator, uint32_t size) {
    BlockMeta *superblock;
    if (blockAllocator->smallestSuperblock.limit - blockAllocator->smallestSuperblock.cursor >= size) {
        // first check the smallestSuperblock
        superblock = blockAllocator->smallestSuperblock.cursor;
        blockAllocator->smallestSuperblock.cursor += size;
    } else {
        // look in the freelists
        int target = MathUtils_Log2Ceil((size_t) size);
        int minNonEmptyIndex = blockAllocator->minNonEmptyIndex;
        int first = (minNonEmptyIndex > target) ? minNonEmptyIndex : target;
        superblock = BlockAllocator_pollSuperblock(blockAllocator, first);
        if (superblock == NULL) {
            return NULL;
        }
        if (BlockMeta_SuperblockSize(superblock) > size) {
            BlockMeta *leftover = superblock + size;
            BlockAllocator_addFreeBlocksInternal(blockAllocator, leftover, BlockMeta_SuperblockSize(superblock) - size);
        }
    }

    BlockMeta_SetFlag(superblock, block_superblock_start);
    BlockMeta_SetSuperblockSize(superblock, size);
    BlockMeta *limit = superblock + size;
    for (BlockMeta *current = superblock + 1; current < limit; current++) {
        BlockMeta_SetFlag(current, block_superblock_middle);
    }
    // not decrementing freeBlockCount, because it is only used after sweep
//    printf("GOT superblock %p , size=%u\n", superblock, size);
//    fflush(stdout);
    return superblock;
}

static inline void BlockAllocator_addFreeBlocksInternal0(BlockAllocator *blockAllocator, BlockMeta *superblock, uint32_t count) {
    // not using BlockMeta_SetSuperblockSize, because it is just empty space
    superblock->superblockSize = count;
    int i = BlockAllocator_sizeToLinkedListIndex(count);
    if (i < blockAllocator->minNonEmptyIndex) {
        blockAllocator->minNonEmptyIndex = i;
    }
    if (i > blockAllocator->maxNonEmptyIndex) {
        blockAllocator->maxNonEmptyIndex = i;
    }
    BlockMeta *limit = superblock + count;
    for (BlockMeta *current = superblock; current < limit; current++) {
        BlockMeta_SetFlag(current, block_free);
    }
    BlockList_AddLast(&blockAllocator->freeSuperblocks[i], superblock);
}

void BlockAllocator_addFreeBlocksInternal(BlockAllocator *blockAllocator, BlockMeta *superblock, uint32_t count) {
    uint32_t remaining_count = count;
    uint32_t powerOf2 = 1;
    BlockMeta *current = superblock;
    // splits the superblock into smaller superblocks that are a powers of 2
    while (remaining_count > 0) {
        if ((powerOf2 & remaining_count) > 0) {
            BlockAllocator_addFreeBlocksInternal0(blockAllocator, current, powerOf2);
            remaining_count -= powerOf2;
            current += powerOf2;
        }
        powerOf2 <<= 1;
    }
}

void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count) {
    assert(count > 0);
    if (blockAllocator->coalescingSuperblock.first == NULL) {
        blockAllocator->coalescingSuperblock.first = block;
        blockAllocator->coalescingSuperblock.limit = block + count;
    } else if(blockAllocator->coalescingSuperblock.limit == block) {
        blockAllocator->coalescingSuperblock.limit = block + count;
    } else {
        uint32_t size = (uint32_t) (blockAllocator->coalescingSuperblock.limit - blockAllocator->coalescingSuperblock.first);
        BlockAllocator_addFreeBlocksInternal(blockAllocator, blockAllocator->coalescingSuperblock.first, size);
        blockAllocator->coalescingSuperblock.first = block;
        blockAllocator->coalescingSuperblock.limit = block + count;
    }
    blockAllocator->freeBlockCount += count;
}

void BlockAllocator_SweepDone(BlockAllocator *blockAllocator) {
    if (blockAllocator->coalescingSuperblock.first != NULL) {
        uint32_t size = (uint32_t) (blockAllocator->coalescingSuperblock.limit - blockAllocator->coalescingSuperblock.first);
        BlockAllocator_addFreeBlocksInternal(blockAllocator, blockAllocator->coalescingSuperblock.first, size);
        blockAllocator->coalescingSuperblock.first = NULL;
        blockAllocator->coalescingSuperblock.limit = NULL;
    }
}

void BlockAllocator_Clear(BlockAllocator *blockAllocator) {
    for (int i = 0; i < SUPERBLOCK_LIST_SIZE; i++) {
        BlockList_Clear(&blockAllocator->freeSuperblocks[i]);
    }
    blockAllocator->freeBlockCount = 0;
    blockAllocator->smallestSuperblock.cursor = NULL;
    blockAllocator->smallestSuperblock.limit = NULL;
    blockAllocator->coalescingSuperblock.first = NULL;
    blockAllocator->coalescingSuperblock.limit = NULL;
    blockAllocator->minNonEmptyIndex = SUPERBLOCK_LIST_SIZE;
    blockAllocator->maxNonEmptyIndex = -1;
}