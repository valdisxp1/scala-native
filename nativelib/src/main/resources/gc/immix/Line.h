#ifndef IMMIX_LINE_H
#define IMMIX_LINE_H

#include "headers/ObjectHeader.h"
#include "headers/LineHeader.h"
#include "Block.h"

static INLINE Object *Line_GetFirstObject(BlockHeader *blockHeader, LineHeader *lineHeader, word_t* blockStart) {
    assert(Line_ContainsObject(lineHeader));
    uint8_t offset = Line_GetFirstObjectOffset(lineHeader);

    uint32_t lineIndex =
        BlockHeader_GetLineIndexFromLineHeader(blockHeader, lineHeader);

    return (Object *)Block_GetLineWord(blockStart, lineIndex,
                                             offset / WORD_SIZE);
}

static INLINE void Line_Update(BlockHeader *blockHeader, word_t* blockStart, word_t *objectStart) {

    int lineIndex = Block_GetLineIndexFromWord(blockStart, objectStart);
    LineHeader *lineHeader = BlockHeader_GetLineHeader(blockHeader, lineIndex);

    if (!Line_ContainsObject(lineHeader)) {
        uint8_t offset = (uint8_t)((word_t)objectStart & LINE_SIZE_MASK);

        Line_SetOffset(lineHeader, offset);
    }
}

#endif // IMMIX_LINE_H
