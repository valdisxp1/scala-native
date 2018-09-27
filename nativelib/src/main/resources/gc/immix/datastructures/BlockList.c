#include <stddef.h>
#include <stdio.h>
#include "BlockList.h"
#include "../Log.h"
#include "../headers/BlockHeader.h"

int32_t BlockList_getBlockIndex(word_t *blockHeaderStart, BlockHeader *blockHeader) {
    return (uint32_t)((word_t *)blockHeader - blockHeaderStart) / WORDS_IN_BLOCK_METADATA;
}

BlockHeader *BlockList_getBlockFromIndex(word_t *blockHeaderStart, int32_t index) {
    return (BlockHeader *)(blockHeaderStart + (index * WORDS_IN_BLOCK_METADATA));
}

BlockHeader *BlockList_getNextBlock(word_t *blockHeaderStart, BlockHeader *header) {
    int32_t nextBlockId = header->header.nextBlock;
    if (nextBlockId == LAST_BLOCK) {
        return NULL;
    } else if (nextBlockId == 0) {
        nextBlockId = BlockList_getBlockIndex(blockHeaderStart, header) + 1;
    }
    return BlockList_getBlockFromIndex(blockHeaderStart, nextBlockId);
}

void BlockList_Init(BlockList *blockList, word_t *blockHeaderStart) {
    blockList->blockHeaderStart = blockHeaderStart;
    blockList->first = NULL;
    blockList->last = NULL;
}

inline bool BlockList_IsEmpty(BlockList *blockList) {
    return blockList->first == NULL;
}

BlockHeader *BlockList_RemoveFirstBlock(BlockList *blockList) {
    assert(blockList->first != NULL);
    BlockHeader *block = blockList->first;
    if (block == blockList->last) {
        blockList->first = NULL;
    }
    blockList->first = BlockList_getNextBlock(blockList->blockHeaderStart, block);
    return block;
}

void BlockList_AddLast(BlockList *blockList, BlockHeader *blockHeader) {
    if (blockList->first == NULL) {
        blockList->first = blockHeader;
    } else {
        blockList->last->header.nextBlock =
            BlockList_getBlockIndex(blockList->blockHeaderStart, blockHeader);
    }
    blockList->last = blockHeader;
    blockHeader->header.nextBlock = LAST_BLOCK;
}

void BlockList_AddBlocksLast(BlockList *blockList, BlockHeader *first,
                             BlockHeader *last) {
    if (blockList->first == NULL) {
        blockList->first = first;
    } else {
        blockList->last->header.nextBlock =
           BlockList_getBlockIndex(blockList->blockHeaderStart, first);
    }
    blockList->last = last;
    last->header.nextBlock = LAST_BLOCK;
}

void BlockList_Clear(BlockList *blockList) {
    blockList->first = NULL;
    blockList->last = NULL;
}

void BlockList_Print(BlockList *blockList) {
    printf("BlockList: ");
    BlockHeader *current = blockList->first;
    while (current != NULL) {
        printf("[%p %d] -> ", current, current->header.first);
        current = BlockList_getNextBlock(blockList->blockHeaderStart, current);
    }
    printf("\n");
}
