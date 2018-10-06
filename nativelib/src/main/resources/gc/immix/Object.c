#include <stddef.h>
#include <stdio.h>
#include "Object.h"
#include "Block.h"
#include "Line.h"
#include "Log.h"
#include "utils/MathUtils.h"

Object *Object_NextLargeObject(Object *object) {
    size_t size = Object_ChunkSize(object);
    assert(size != 0);
    return (Object *)((ubyte_t *)object + size);
}

Object *Object_NextObject(Object *object) {
    size_t size = Object_Size(&object->header);
    assert(size < LARGE_BLOCK_SIZE);
    if (size == 0) {
        return NULL;
    }
    Object *next = (Object *)((ubyte_t *)object + size);
    assert(Block_GetBlockStartForWord((word_t *)next) ==
               Block_GetBlockStartForWord((word_t *)object) ||
           Block_GetBlockStartForWord((word_t *)next) ==
               Block_GetBlockStartForWord((word_t *)object) +
                   WORDS_IN_BLOCK);
    return next;
}

static inline bool isWordAligned(word_t *word) {
    return ((word_t)word & WORD_INVERSE_MASK) == (word_t)word;
}

Object *Object_getInLine(BlockHeader *blockHeader, word_t *blockStart, int lineIndex,
                         word_t *word) {
    assert(Line_ContainsObject(BlockHeader_GetLineHeader(blockHeader, lineIndex)));

    Object *current =
        Line_GetFirstObject(blockHeader, BlockHeader_GetLineHeader(blockHeader, lineIndex), blockStart);
    Object *next = Object_NextObject(current);

    word_t *lineEnd = Block_GetLineAddress(blockStart, lineIndex) + WORDS_IN_LINE;

    while (next != NULL && (word_t *)next < lineEnd && (word_t *)next <= word) {
        current = next;
        next = Object_NextObject(next);
    }

    if (Object_IsAllocated(&current->header) && word >= (word_t *)current &&
        word < (word_t *)next) {
#ifdef DEBUG_PRINT
        if ((word_t *)current != word) {
            printf("inner pointer: %p object: %p\n", word, current);
            fflush(stdout);
        }
#endif
        return current;
    } else {
#ifdef DEBUG_PRINT
        printf("ignoring %p\n", word);
        fflush(stdout);
#endif
        return NULL;
    }
}

Object *Object_GetObject(Heap *heap, word_t *word) {
    BlockHeader *blockHeader = Block_GetBlockHeader(heap->blockHeaderStart, heap->heapStart, word);
    word_t *blockStart = Block_GetBlockStartForWord(word);

    if (!isWordAligned(word)) {
#ifdef DEBUG_PRINT
        printf("Word not aligned: %p aligning to %p\n", word,
               (word_t *)((word_t)word & WORD_INVERSE_MASK));
        fflush(stdout);
#endif
        word = (word_t *)((word_t)word & WORD_INVERSE_MASK);
    }

    int lineIndex = Block_GetLineIndexFromWord(blockStart, word);
    while (lineIndex > 0 &&
           !Line_ContainsObject(BlockHeader_GetLineHeader(blockHeader, lineIndex))) {
        lineIndex--;
    }

    if (Line_ContainsObject(BlockHeader_GetLineHeader(blockHeader, lineIndex))) {
        return Object_getInLine(blockHeader, blockStart, lineIndex, word);
    } else {
#ifdef DEBUG_PRINT
        printf("Word points to empty line %p\n", word);
        fflush(stdout);
#endif
        return NULL;
    }
}

Object *Object_getLargeInnerPointer(LargeAllocator *allocator, word_t *word) {
    word_t *current = (word_t *)((word_t)word & LARGE_BLOCK_MASK);

    assert(Bytemap_IsFree(allocator->bytemap, current) == !Bitmap_GetBit(allocator->bitmap, (ubyte_t *)current));
    while (Bytemap_IsFree(allocator->bytemap, current)) {
        current -= LARGE_BLOCK_SIZE / WORD_SIZE;
    }

    Object *object = (Object *)current;
    if (word < (word_t *)object + Object_ChunkSize(object) / WORD_SIZE &&
        object->rtti != NULL) {
#ifdef DEBUG_PRINT
        printf("large inner pointer: %p, object: %p\n", word, object);
        fflush(stdout);
#endif
        return object;
    } else {

        return NULL;
    }
}

Object *Object_GetLargeObject(LargeAllocator *allocator, word_t *word) {
    if (((word_t)word & LARGE_BLOCK_MASK) != (word_t)word) {
        word = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    }
    assert(Bytemap_IsAllocated(allocator->bytemap, word) == Bitmap_GetBit(allocator->bitmap, (ubyte_t *)word));
    if (Bytemap_IsAllocated(allocator->bytemap, word) && Object_IsAllocated(&((Object *)word)->header)) {
        return (Object *)word;
    } else {
        Object *object = Object_getLargeInnerPointer(allocator, word);
        assert(object == NULL ||
               (word >= (word_t *)object &&
                word < (word_t *)Object_NextLargeObject(object)));
        return object;
    }
}

void Object_Mark(Heap *heap, Object *object) {
    // Mark the object itself
    Object_MarkObjectHeader(&object->header);

    if (!Object_IsLargeObject(&object->header)) {
        // Mark the block
        BlockHeader *blockHeader = Block_GetBlockHeader(heap->blockHeaderStart, heap->heapStart, (word_t *)object);
        word_t *blockStart = Block_GetBlockStartForWord((word_t *)object);
        BlockHeader_Mark(blockHeader);

        // Mark all Lines
        int startIndex =
            Block_GetLineIndexFromWord(blockStart, (word_t *)object);
        word_t *lastWord = (word_t *)Object_NextObject(object) - 1;
        int endIndex = Block_GetLineIndexFromWord(blockStart, lastWord);
        assert(startIndex >= 0 && startIndex < LINE_COUNT);
        assert(endIndex >= 0 && endIndex < LINE_COUNT);
        assert(startIndex <= endIndex);
        for (int i = startIndex; i <= endIndex; i++) {
            LineHeader *lineHeader = BlockHeader_GetLineHeader(blockHeader, i);
            Line_Mark(lineHeader);
        }
    }
}

size_t Object_ChunkSize(Object *object) {
    return MathUtils_RoundToNextMultiple(Object_Size(&object->header),
                                         MIN_BLOCK_SIZE);
}
