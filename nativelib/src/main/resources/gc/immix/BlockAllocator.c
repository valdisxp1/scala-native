#include "BlockAllocator.h"

void BlockAllocator_Init(BlockAllocator *blockAllocator, word_t *blockMetaStart, uint32_t blockCount) {
    BlockList_Init(&blockAllocator->freeBlocks, blockMetaStart);

   // Init the free block list
   blockAllocator->freeBlocks.first = (BlockMeta *)blockMetaStart;
   BlockMeta *lastBlockMeta =
       (BlockMeta *)(blockMetaStart +
                     ((blockCount - 1) * WORDS_IN_BLOCK_METADATA));
   blockAllocator->freeBlocks.last = lastBlockMeta;
   lastBlockMeta->nextBlock = LAST_BLOCK;
}

BlockMeta *BlockAllocator_GetFreeBlock(BlockAllocator *blockAllocator) {
    BlockMeta *block = NULL;
    if (!BlockList_IsEmpty(&blockAllocator->freeBlocks)) {
        block = BlockList_RemoveFirstBlock(&blockAllocator->freeBlocks);
    }
    return block;
}