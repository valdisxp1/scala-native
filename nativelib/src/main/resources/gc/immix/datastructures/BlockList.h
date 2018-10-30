#ifndef IMMIX_BLOCLIST_H
#define IMMIX_BLOCLIST_H

#include "../metadata/BlockMeta.h"

typedef struct {
    word_t *blockMetaStart;
    BlockMeta *head;
} BlockList;

void BlockList_Init(BlockList *blockList, word_t *blockMetaStart);
void BlockList_Clear(BlockList *blockList);
BlockMeta *BlockList_Pop(BlockList *blockList);
void BlockList_Push(BlockList *blockList, BlockMeta *block);

#endif // IMMIX_BLOCLIST_H
