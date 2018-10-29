#include "BlockAllocator.h"
#include "Log.h"
#include "utils/MathUtils.h"
#include <stdio.h>

void BlockAllocator_addFreeBlocksInternal(BlockAllocator *blockAllocator,
                                          BlockMeta *block, uint32_t count);

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart,
                         uint32_t blockCount) {
    for (int i = 0; i < SUPERBLOCK_LIST_SIZE; i++) {
        BlockList_Init(&blockAllocator->freeSuperblocks[i], blockMetaStart);
    }
    BlockAllocator_Clear(blockAllocator);

    blockAllocator->blockMetaStart = blockMetaStart;
    blockAllocator->smallestSuperblock.cursor = (BlockMeta *)blockMetaStart;
    blockAllocator->smallestSuperblock.limit =
        (BlockMeta *)blockMetaStart + blockCount;
}

inline static int BlockAllocator_sizeToLinkedListIndex(uint32_t size) {
    int result = MathUtils_Log2Floor((size_t)size);
    assert(result >= 0);
    assert(result < SUPERBLOCK_LIST_SIZE);
    return result;
}

inline static BlockMeta *
BlockAllocator_pollSuperblock(BlockAllocator *blockAllocator, int first) {
    int maxNonEmptyIndex = blockAllocator->maxNonEmptyIndex;
    for (int i = first; i <= maxNonEmptyIndex; i++) {
        BlockMeta *superblock =
            BlockList_Poll(&blockAllocator->freeSuperblocks[i]);
        if (superblock != NULL) {
            assert(BlockMeta_SuperblockSize(superblock) > 0);
            return superblock;
        } else {
            blockAllocator->minNonEmptyIndex = i + 1;
        }
    }
    return NULL;
}

NOINLINE BlockMeta *
BlockAllocator_getFreeBlockSlow(BlockAllocator *blockAllocator) {
    BlockMeta *superblock = BlockAllocator_pollSuperblock(
        blockAllocator, blockAllocator->minNonEmptyIndex);
    if (superblock != NULL) {
        blockAllocator->smallestSuperblock.cursor = superblock + 1;
        blockAllocator->smallestSuperblock.limit =
            superblock + BlockMeta_SuperblockSize(superblock);
        BlockMeta_SetFlag(superblock, block_simple);
        return superblock;
    } else {
        // as the last resort look in the superblock being coalesced
        BlockMeta *first = blockAllocator->coalescingSuperblock.first;
        BlockMeta *limit = blockAllocator->coalescingSuperblock.limit;
        uint32_t block0Idx = BlockRange_PollFirst(&blockAllocator->coalescingSuperblock0, 1);
        BlockMeta *block0 = NULL;
        if (block0Idx != NO_BLOCK_INDEX) {
            block0 = BlockMeta_GetFromIndex(blockAllocator->blockMetaStart, block0Idx);
        }
        if (limit - first >= 1) {
            blockAllocator->coalescingSuperblock.first += 1;
            BlockMeta_SetFlag(first, block_simple);
            assert(block0 == first);
            return first;
        } else {
            assert(block0 == NULL);
            return NULL;
        }
    }
}

INLINE BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator) {
    if (blockAllocator->smallestSuperblock.cursor >=
        blockAllocator->smallestSuperblock.limit) {
        return BlockAllocator_getFreeBlockSlow(blockAllocator);
    }
    BlockMeta *block = blockAllocator->smallestSuperblock.cursor;
    BlockMeta_SetFlag(block, block_simple);
    blockAllocator->smallestSuperblock.cursor++;

    // not decrementing freeBlockCount, because it is only used after sweep
    return block;
}

BlockMeta *BlockAllocator_GetFreeSuperblock(BlockAllocator *blockAllocator,
                                            uint32_t size) {
    BlockMeta *superblock;
    BlockMeta *sCursor = blockAllocator->smallestSuperblock.cursor;
    BlockMeta *sLimit = blockAllocator->smallestSuperblock.limit;

    if (sLimit - sCursor >= size) {
        // first check the smallestSuperblock
        blockAllocator->smallestSuperblock.cursor += size;
        superblock = sCursor;
    } else {
        // look in the freelists
        int target = MathUtils_Log2Ceil((size_t)size);
        int minNonEmptyIndex = blockAllocator->minNonEmptyIndex;
        int first = (minNonEmptyIndex > target) ? minNonEmptyIndex : target;
        superblock = BlockAllocator_pollSuperblock(blockAllocator, first);

        if (superblock != NULL) {
            if (BlockMeta_SuperblockSize(superblock) > size) {
                BlockMeta *leftover = superblock + size;
                BlockAllocator_addFreeBlocksInternal(
                    blockAllocator, leftover,
                    BlockMeta_SuperblockSize(superblock) - size);
            }
        } else {
            // as the last resort look in the superblock being coalesced
            BlockMeta *cFirst = blockAllocator->coalescingSuperblock.first;
            BlockMeta *cLimit = blockAllocator->coalescingSuperblock.limit;
            if (cLimit - cFirst >= size) {
                blockAllocator->coalescingSuperblock.first += size;
                superblock = cFirst;
            }
            uint32_t superblock0Idx = BlockRange_PollFirst(&blockAllocator->coalescingSuperblock0, size);
            BlockMeta *superblock0 = NULL;
            if (superblock0Idx != NO_BLOCK_INDEX) {
                BlockMeta_GetFromIndex(blockAllocator->blockMetaStart, superblock0Idx);
            }
            assert(superblock == superblock0);
        }

        if (superblock == NULL) {
            return NULL;
        }
    }

    assert(superblock != NULL);

    BlockMeta_SetFlag(superblock, block_superblock_start);
    BlockMeta_SetSuperblockSize(superblock, size);
    BlockMeta *limit = superblock + size;
    for (BlockMeta *current = superblock + 1; current < limit; current++) {
        BlockMeta_SetFlag(current, block_superblock_middle);
    }
    // not decrementing freeBlockCount, because it is only used after sweep
    return superblock;
}

static inline void
BlockAllocator_addFreeBlocksInternal0(BlockAllocator *blockAllocator,
                                      BlockMeta *superblock, uint32_t count) {
    int i = BlockAllocator_sizeToLinkedListIndex(count);
    if (i < blockAllocator->minNonEmptyIndex) {
        blockAllocator->minNonEmptyIndex = i;
    }
    if (i > blockAllocator->maxNonEmptyIndex) {
        blockAllocator->maxNonEmptyIndex = i;
    }
    BlockMeta_SetSuperblockSize(superblock, count);
    BlockList_AddLast(&blockAllocator->freeSuperblocks[i], superblock);
}

void BlockAllocator_addFreeBlocksInternal(BlockAllocator *blockAllocator,
                                          BlockMeta *superblock,
                                          uint32_t count) {
    uint32_t remaining_count = count;
    uint32_t powerOf2 = 1;
    BlockMeta *current = superblock;
    // splits the superblock into smaller superblocks that are a powers of 2
    while (remaining_count > 0) {
        if ((powerOf2 & remaining_count) > 0) {
            BlockAllocator_addFreeBlocksInternal0(blockAllocator, current,
                                                  powerOf2);
            remaining_count -= powerOf2;
            current += powerOf2;
        }
        powerOf2 <<= 1;
    }
}

void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator,
                                  BlockMeta *superblock, uint32_t count) {
    assert(count > 0);
    BlockMeta *limit = superblock + count;
    for (BlockMeta *current = superblock; current < limit; current++) {
        BlockMeta_Clear(current);
    }
    uint32_t superblockIdx = BlockMeta_GetBlockIndex(blockAllocator->blockMetaStart, superblock);
    bool didAppend = BlockRange_AppendLast(&blockAllocator->coalescingSuperblock0, superblockIdx, count);
    if (blockAllocator->coalescingSuperblock.first == NULL) {
        blockAllocator->coalescingSuperblock.first = superblock;
        blockAllocator->coalescingSuperblock.limit = superblock + count;
        assert(didAppend);
    } else if (blockAllocator->coalescingSuperblock.limit == superblock) {
        blockAllocator->coalescingSuperblock.limit = superblock + count;
        assert(didAppend);
    } else {
        assert(!didAppend);
        uint32_t size = (uint32_t)(blockAllocator->coalescingSuperblock.limit -
                                   blockAllocator->coalescingSuperblock.first);
        uint32_t superblockIdx = BlockMeta_GetBlockIndex(blockAllocator->blockMetaStart, superblock);
        BlockRange oldRange = BlockRange_Replace(&blockAllocator->coalescingSuperblock0, superblockIdx, count);
        uint32_t size0 = BlockRange_Size(oldRange);
        assert(size0 == size);
        BlockMeta *superblock0 = BlockMeta_GetFromIndex(blockAllocator->blockMetaStart, BlockRange_First(oldRange));

        assert(superblock0 == blockAllocator->coalescingSuperblock.first);
        BlockAllocator_addFreeBlocksInternal(
            blockAllocator, blockAllocator->coalescingSuperblock.first, size);
        blockAllocator->coalescingSuperblock.first = superblock;
        blockAllocator->coalescingSuperblock.limit = superblock + count;
    }
    blockAllocator->freeBlockCount += count;
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
    BlockRange_Clear(&blockAllocator->coalescingSuperblock0);
    blockAllocator->minNonEmptyIndex = SUPERBLOCK_LIST_SIZE;
    blockAllocator->maxNonEmptyIndex = -1;
}