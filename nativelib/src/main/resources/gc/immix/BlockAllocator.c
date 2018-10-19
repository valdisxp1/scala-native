#include "BlockAllocator.h"

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount) {
    BlockList_Init(&blockAllocator->freeBlocks, blockMetaStart);
    SuperblockList_Init(&blockAllocator->freeSuperblocks, blockMetaStart);
    BlockAllocator_Clear(blockAllocator);
    BlockAllocator_AddFreeBlocks(blockAllocator, (BlockMeta *) blockMetaStart, blockCount);
}

BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator) {
    BlockMeta *block = NULL;
    block = BlockList_Poll(&blockAllocator->freeBlocks);
    BlockMeta *superblock = SuperblockList_Poll(&blockAllocator->freeSuperblocks);
    assert(block == superblock);
    // not decrementing freeBlockCount, because it is only used after sweep
    return block;
}

void BlockAllocator_AddFreeBlock(BlockAllocator *blockAllocator, BlockMeta *block) {
    BlockList_AddLast(&blockAllocator->freeBlocks, block);
    SuperblockList_AddLast(&blockAllocator->freeSuperblocks, block);
    blockAllocator->freeBlockCount++;
}

void BlockAllocator_AddFreeBlocks(BlockAllocator *blockAllocator, BlockMeta *block, uint32_t count) {
    BlockList_AddBlocksLast(&blockAllocator->freeBlocks, block, block + (count - 1));
    SuperblockList_AddBlocksLast(&blockAllocator->freeSuperblocks, block, block + (count - 1));
    blockAllocator->freeBlockCount += count;
}

void BlockAllocator_Clear(BlockAllocator *blockAllocator) {
    BlockList_Clear(&blockAllocator->freeBlocks);
    SuperblockList_Clear(&blockAllocator->freeSuperblocks);
    blockAllocator->freeBlockCount = 0;
    blockAllocator->currentSuperblock.cursor = NULL;
    blockAllocator->currentSuperblock.limit = NULL;
}