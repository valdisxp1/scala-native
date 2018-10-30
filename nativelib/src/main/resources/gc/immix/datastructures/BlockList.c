#include <stddef.h>
#include <stdio.h>
#include "BlockList.h"
#include "../Log.h"
#include "../metadata/BlockMeta.h"

#define LAST_BLOCK -1

BlockMeta *BlockList_getNextBlock(word_t *blockMetaStart,
                                  BlockMeta *blockMeta) {
    int32_t nextBlockId = blockMeta->nextBlock;
    if (nextBlockId == LAST_BLOCK) {
        return NULL;
    } else {
        return BlockMeta_GetFromIndex(blockMetaStart, nextBlockId);
    }
}

void BlockList_Init(BlockList *blockList, word_t *blockMetaStart) {
    blockList->blockMetaStart = blockMetaStart;
    blockList->head = NULL;
}

BlockMeta *BlockList_Pop(BlockList *blockList) {
    BlockMeta *block = blockList->head;
    if (block != NULL) {
        blockList->head =
            BlockList_getNextBlock(blockList->blockMetaStart, block);
    }
    return block;
}

void BlockList_Push(BlockList *blockList, BlockMeta *blockMeta) {
    if (blockList->head == NULL) {
        blockMeta->nextBlock = LAST_BLOCK;
    } else {
        blockMeta->nextBlock =
            BlockMeta_GetBlockIndex(blockList->blockMetaStart, blockMeta);
    }
    blockList->head = blockMeta;
}

void BlockList_Clear(BlockList *blockList) {
    blockList->head = NULL;
}