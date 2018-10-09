#ifndef IMMIX_LINE_H
#define IMMIX_LINE_H

#include "headers/ObjectHeader.h"
#include "headers/LineHeader.h"
#include "Block.h"

static INLINE Object *Line_GetFirstObject(uint32_t lineIndex, LineHeader *lineHeader, word_t *blockStart) {
    assert(Line_ContainsObject(lineHeader));
    uint8_t offset = Line_GetFirstObjectOffset(lineHeader);

    return (Object *)Block_GetLineWord(blockStart, lineIndex,
                                             offset / WORD_SIZE);
}

static INLINE Object *Line_GetFirstObject2(Bytemap *bytemap, word_t *blockStart, uint32_t lineIndex) {
    word_t *lineStart = Block_GetLineAddress(blockStart, lineIndex);
    word_t *lineEnd = lineStart + WORDS_IN_LINE;
    word_t *current = lineStart;
    while(current < lineEnd) {
        if (!Bytemap_IsFree(bytemap, current)) {
            return (Object *) current;
        }
        current += 1; /* 1 WORD*/
    }

    return NULL;
}

static INLINE int Line_ContainsObject2(Bytemap *bytemap, word_t *blockStart, uint32_t lineIndex) {
    return Line_GetFirstObject2(bytemap, blockStart, lineIndex) != NULL;
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
