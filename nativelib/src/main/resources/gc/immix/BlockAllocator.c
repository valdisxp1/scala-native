#include "BlockAllocator.h"

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount) {
    BlockList_Init(&blockAllocator->freeBlocks, blockMetaStart);
    BlockMeta *lastBlock = BlockMeta_GetFromIndex(blockMetaStart, blockCount - 1);
    BlockList_AddBlocksLast(&blockAllocator->freeBlocks, (BlockMeta *)blockMetaStart, lastBlock);
    blockAllocator->freeBlockCount = (uint64_t)blockCount;
}

BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator) {
    BlockMeta *block = NULL;
    if (!BlockList_IsEmpty(&blockAllocator->freeBlocks)) {
        block = BlockList_RemoveFirstBlock(&blockAllocator->freeBlocks);
    }
    // not decrementing freeBlockCount, because it is only used after sweep
    return block;
}

void BlockAllocator_AddFreeBlock(BlockAllocator *blockAllocator, BlockMeta *block) {
    BlockList_AddLast(&blockAllocator->freeBlocks, block);
    blockAllocator->freeBlockCount++;
}

void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count) {
    BlockList_AddBlocksLast(&blockAllocator->freeBlocks, block, block + (count - 1));
    blockAllocator->freeBlockCount += count;
}

void BlockAllocator_Clear(BlockAllocator *blockAllocator) {
    BlockList_Clear(&blockAllocator->freeBlocks);
    blockAllocator->freeBlockCount = 0;
}