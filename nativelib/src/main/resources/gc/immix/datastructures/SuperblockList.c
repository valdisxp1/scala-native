#include <stddef.h>
#include <stdio.h>
#include "SuperblockList.h"
#include "../Log.h"
#include "../metadata/BlockMeta.h"

BlockMeta *SuperblockList_getNextBlock(word_t *blockMetaStart,
                                       BlockMeta *blockMeta) {
    int32_t nextBlockId = blockMeta->nextSuperblock;
    if (nextBlockId == LAST_BLOCK) {
        return NULL;
    }
    return BlockMeta_GetFromIndex(blockMetaStart, nextBlockId);
}

void SuperblockList_Init(SuperblockList *blockList, word_t *blockMetaStart) {
    blockList->blockMetaStart = blockMetaStart;
    blockList->first = NULL;
    blockList->last = NULL;
}

BlockMeta *SuperblockList_Poll(SuperblockList *blockList) {
    BlockMeta *block = blockList->first;
    if (block != NULL) {
        if (block == blockList->last) {
            blockList->first = NULL;
        }
        blockList->first = SuperblockList_getNextBlock(blockList->blockMetaStart, block);
        assert(block->superblockSize > 0);
    }
    return block;
}

void SuperblockList_AddLast(SuperblockList *blockList, BlockMeta *blockMeta) {
    assert(blockMeta->superblockSize > 0);
    if (blockList->first == NULL) {
        blockList->first = blockMeta;
    } else {
        blockList->last->nextSuperblock =
            BlockMeta_GetBlockIndex(blockList->blockMetaStart, blockMeta);
    }
    blockList->last = blockMeta;
    blockMeta->nextSuperblock = LAST_BLOCK;
}

void SuperblockList_Clear(SuperblockList *blockList) {
    blockList->first = NULL;
    blockList->last = NULL;
}