#ifndef IMMIX_BLOCKHEADER_H
#define IMMIX_BLOCKHEADER_H

#include <stdint.h>
#include "LineHeader.h"
#include "../GCTypes.h"
#include "../Constants.h"
#include "../Log.h"

typedef enum {
    block_free = 0x0,
    block_recyclable = 0x1,
    block_unavailable = 0x2
} BlockFlag;

typedef struct {
    struct {
        uint8_t mark;
        uint8_t flags;
        int16_t first;
        int32_t nextBlock;
    } header;
    LineHeader lineHeaders[LINE_COUNT];
} BlockHeader;

static inline bool BlockHeader_IsRecyclable(BlockHeader *blockHeader) {
    return blockHeader->header.flags == block_recyclable;
}
static inline bool BlockHeader_IsUnavailable(BlockHeader *blockHeader) {
    return blockHeader->header.flags == block_unavailable;
}
static inline bool BlockHeader_IsFree(BlockHeader *blockHeader) {
    return blockHeader->header.flags == block_free;
}

static inline void BlockHeader_SetFlag(BlockHeader *blockHeader,
                                 BlockFlag blockFlag) {
    blockHeader->header.flags = blockFlag;
}

static inline bool BlockHeader_IsMarked(BlockHeader *blockHeader) {
    return blockHeader->header.mark == 1;
}

static inline void BlockHeader_Unmark(BlockHeader *blockHeader) {
    blockHeader->header.mark = 0;
}

static inline void BlockHeader_Mark(BlockHeader *blockHeader) {
    blockHeader->header.mark = 1;
}

static inline uint32_t
BlockHeader_GetLineIndexFromLineHeader(BlockHeader *blockHeader,
                                       LineHeader *lineHeader) {
    return (uint32_t)(lineHeader - blockHeader->lineHeaders);
}

static inline LineHeader *BlockHeader_GetLineHeader(BlockHeader *blockHeader,
                                              int lineIndex) {
    return &blockHeader->lineHeaders[lineIndex];
}

static inline uint32_t BlockHeader_GetBlockIndex(BlockHeader *blockHeader);
static inline word_t *BlockHeader_GetBlockStart(BlockHeader *blockHeader);
static inline uint32_t Block_GetBlockIndexForWord(word_t *word);
static inline BlockHeader *Block_GetBlockHeader(word_t *word);
static inline uint32_t Block_GetLineIndexFromWord(word_t *blockStart,
                                                  word_t *word);


static inline word_t * Block_GetBlockStartForWord(word_t *word);
static inline word_t *Block_GetBlockEnd(word_t *blockStart);
static inline word_t *Block_GetLineAddress(word_t *blockStart, int lineIndex);
static inline FreeLineHeader *Block_GetFreeLineHeader(word_t *blockStart,
                                                      int lineIndex);
static inline word_t *Block_GetLineWord(word_t *blockStart, int lineIndex,
                                        int wordIndex);

#endif // IMMIX_BLOCKHEADER_H
