#ifndef IMMIX_SUPERBLOCKLIST_H
#define IMMIX_SUPERBLOCKLIST_H

#include "../metadata/BlockMeta.h"

#define LAST_BLOCK -1

typedef struct {
    word_t *blockMetaStart;
    BlockMeta *first;
    BlockMeta *last;
} SuperblockList;

void SuperblockList_Init(SuperblockList *blockList, word_t *blockMetaStart);
void SuperblockList_Clear(SuperblockList *blockList);
BlockMeta *SuperblockList_Poll(SuperblockList *blockList);
void SuperblockList_AddLast(SuperblockList *blockList, BlockMeta *block);
void SuperblockList_AddBlocksLast(SuperblockList *blockList, BlockMeta *first,
                             BlockMeta *last);

#endif // IMMIX_SUPERBLOCKLIST_H
