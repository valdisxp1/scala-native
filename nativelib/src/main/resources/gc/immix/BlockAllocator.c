#include "BlockAllocator.h"
#include "Log.h"
#include "utils/MathUtils.h"
#include <stdio.h>

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
BlockAllocator_pollSuperblock(BlockAllocator *blockAllocator, int* index) {
    for (int i = *index; i < SUPERBLOCK_LIST_SIZE; i++) {
        BlockMeta *superblock =
            BlockList_Pop(&blockAllocator->freeSuperblocks[i]);
        if (superblock != NULL) {
            *index = i;
            return superblock;
        }
    }
    return NULL;
}

NOINLINE BlockMeta *
BlockAllocator_getFreeBlockSlow(BlockAllocator *blockAllocator) {
    int index = 0;
    BlockMeta *superblock = BlockAllocator_pollSuperblock(blockAllocator, &index);
    if (superblock != NULL) {
        blockAllocator->smallestSuperblock.cursor = superblock + 1;
        uint32_t size = 1 << index;
        blockAllocator->smallestSuperblock.limit = superblock + size;
        assert(BlockMeta_IsFree(superblock));
        BlockMeta_SetFlag(superblock, block_simple);
        return superblock;
    } else {
        // as the last resort look in the superblock being coalesced
        uint32_t blockIdx = BlockRange_PollFirst(&blockAllocator->coalescingSuperblock, 1);
        BlockMeta *block = NULL;
        if (blockIdx != NO_BLOCK_INDEX) {
            block = BlockMeta_GetFromIndex(blockAllocator->blockMetaStart, blockIdx);
            assert(BlockMeta_IsFree(block));
            BlockMeta_SetFlag(block, block_simple);
        }
        return block;
    }
}

INLINE BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator) {
    if (blockAllocator->smallestSuperblock.cursor >=
        blockAllocator->smallestSuperblock.limit) {
        return BlockAllocator_getFreeBlockSlow(blockAllocator);
    }
    BlockMeta *block = blockAllocator->smallestSuperblock.cursor;
    assert(BlockMeta_IsFree(block));
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
        int index = MathUtils_Log2Ceil((size_t)size);
        superblock = BlockAllocator_pollSuperblock(blockAllocator, &index);
        uint32_t receivedSize = 1 << index;

        if (superblock != NULL) {
            if (receivedSize > size) {
                BlockMeta *leftover = superblock + size;
                BlockAllocator_AddFreeSuperblock(
                    blockAllocator, leftover,
                    receivedSize - size);
            }
        } else {
            // as the last resort look in the superblock being coalesced
            uint32_t superblockIdx = BlockRange_PollFirst(&blockAllocator->coalescingSuperblock, size);
            if (superblockIdx != NO_BLOCK_INDEX) {
                superblock = BlockMeta_GetFromIndex(blockAllocator->blockMetaStart, superblockIdx);
            }
        }

        if (superblock == NULL) {
            return NULL;
        }
    }

    assert(superblock != NULL);

    assert(BlockMeta_IsFree(superblock));
    BlockMeta_SetFlag(superblock, block_superblock_start);
    BlockMeta *limit = superblock + size;
    for (BlockMeta *current = superblock + 1; current < limit; current++) {
        assert(BlockMeta_IsFree(current));
        BlockMeta_SetFlag(current, block_superblock_middle);
    }
    // not decrementing freeBlockCount, because it is only used after sweep
    return superblock;
}

static inline void
BlockAllocator_addSuperblockToBlockLists(BlockAllocator *blockAllocator,
                                         BlockMeta *superblock, uint32_t count) {
    int i = BlockAllocator_sizeToLinkedListIndex(count);
    BlockList_Push(&blockAllocator->freeSuperblocks[i], superblock);
}

void BlockAllocator_splitAndAdd(BlockAllocator *blockAllocator,
                                BlockMeta *superblock,
                                uint32_t count) {
    uint32_t remaining_count = count;
    uint32_t powerOf2 = 1;
    BlockMeta *current = superblock;
    // splits the superblock into smaller superblocks that are a powers of 2
    while (remaining_count > 0) {
        if ((powerOf2 & remaining_count) > 0) {
            BlockAllocator_addSuperblockToBlockLists(blockAllocator, current,
                                                     powerOf2);
            remaining_count -= powerOf2;
            current += powerOf2;
        }
        powerOf2 <<= 1;
    }
}

void BlockAllocator_AddFreeSuperblock(BlockAllocator *blockAllocator,
                                      BlockMeta *superblock,
                                      uint32_t count) {

    BlockMeta *limit = superblock + count;
    for (BlockMeta *current = superblock; current < limit; current++) {
        BlockMeta_Clear(current);
    }
    // all the sweeping changes should be visible to all threads by now
    atomic_thread_fence(memory_order_seq_cst);
    BlockAllocator_splitAndAdd(blockAllocator, superblock, count);
    blockAllocator->freeBlockCount += count;
}

void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator,
                                  BlockMeta *superblock, uint32_t count) {
    assert(count > 0);
    BlockMeta *limit = superblock + count;
    for (BlockMeta *current = superblock; current < limit; current++) {
        BlockMeta_Clear(current);
    }
    // all the sweeping changes should be visible to all threads by now
    atomic_thread_fence(memory_order_seq_cst);
    uint32_t superblockIdx = BlockMeta_GetBlockIndex(blockAllocator->blockMetaStart, superblock);
    bool didAppend = BlockRange_AppendLast(&blockAllocator->coalescingSuperblock, superblockIdx, count);
    if (!didAppend) {
        uint32_t superblockIdx = BlockMeta_GetBlockIndex(blockAllocator->blockMetaStart, superblock);
        BlockRangeVal oldRange = BlockRange_Replace(&blockAllocator->coalescingSuperblock, superblockIdx, count);
        uint32_t size = BlockRange_Size(oldRange);
        BlockMeta *replaced = BlockMeta_GetFromIndex(blockAllocator->blockMetaStart, BlockRange_First(oldRange));

        BlockAllocator_splitAndAdd(blockAllocator, replaced, size);
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
    BlockRange_Clear(&blockAllocator->coalescingSuperblock);
}