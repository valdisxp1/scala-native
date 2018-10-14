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

word_t *Object_LastWord(Object *object) {
    size_t size = Object_Size(object);
    assert(size < LARGE_BLOCK_SIZE);
    word_t *last = (word_t *)((ubyte_t *)object + size) - 1;
    return last;
}

static inline bool Object_isWordAligned(word_t *word) {
    return ((word_t)word & WORD_INVERSE_MASK) == (word_t)word;
}

Object *Object_getInnerPointer(Bytemap *bytemap, word_t *blockStart, word_t *word) {
    word_t *current = word;
    while (current >= blockStart && Bytemap_IsFree(bytemap, current)) {
        current -= 1;// 1 WORD
    }
    Object *object = (Object *)current;
    if (Bytemap_IsAllocated(bytemap, current) && word <  current + Object_Size(object) / WORD_SIZE) {
#ifdef DEBUG_PRINT
        if ((word_t *)current != word) {
            printf("inner pointer: %p object: %p\n", word, current);
            fflush(stdout);
        }
#endif
        return object;
    } else {
        return NULL;
    }
}

Object *Object_GetUnmarkedObject(word_t *word) {
    BlockHeader *blockHeader = Block_GetBlockHeader(heap.blockHeaderStart, heap.heapStart, word);
    word_t *blockStart = Block_GetBlockStartForWord(word);

    if (!Object_isWordAligned(word)) {
#ifdef DEBUG_PRINT
        printf("Word not aligned: %p aligning to %p\n", word,
               (word_t *)((word_t)word & WORD_INVERSE_MASK));
        fflush(stdout);
#endif
        word = (word_t *)((word_t)word & WORD_INVERSE_MASK);
    }

    if (Bytemap_IsPlaceholder(heap.smallBytemap, word) || Bytemap_IsMarked(heap.smallBytemap, word)) {
        return NULL;
    } else if (Bytemap_IsAllocated(heap.smallBytemap, word)) {
        return (Object *) word;
    } else {
       return Object_getInnerPointer(heap.smallBytemap, blockStart, word);
    }
}

Object *Object_getLargeInnerPointer( word_t *word) {
    word_t *current = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    Bytemap *bytemap = allocator.bytemap;

    while (Bytemap_IsFree(bytemap, current)) {
        current -= LARGE_BLOCK_SIZE / WORD_SIZE;
    }

    Object *object = (Object *)current;
    if (Bytemap_IsAllocated(bytemap, current) &&
        word < (word_t *)object + Object_ChunkSize(object) / WORD_SIZE) {
#ifdef DEBUG_PRINT
        printf("large inner pointer: %p, object: %p\n", word, object);
        fflush(stdout);
#endif
        return object;
    } else {
        return NULL;
    }
}

Object *Object_GetLargeUnmarkedObject(word_t *word) {
    if (((word_t)word & LARGE_BLOCK_MASK) != (word_t)word) {
        word = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    }
    if (Bytemap_IsPlaceholder(largeAllocator.bytemap, word) || Bytemap_IsMarked(largeAllocator.bytemap, word)) {
        return NULL;
    } else if (Bytemap_IsAllocated(largeAllocator.bytemap, word)) {
        return (Object *)word;
    } else {
        Object *object = Object_getLargeInnerPointer(word);
        assert(object == NULL ||
               (word >= (word_t *)object &&
                word < (word_t *)Object_NextLargeObject(object)));
        return object;
    }
}

void Object_Mark(Object *object) {
    // Mark the object itself
    Bytemap *bytemap = Heap_BytemapForWord((word_t*) object);
    Bytemap_SetMarked(bytemap, (word_t*) object);

    if (Heap_IsWordInSmallHeap((word_t*) object)) {
        // Mark the block
        BlockHeader *blockHeader = Block_GetBlockHeader(heap.blockHeaderStart, heap.heapStart, (word_t *)object);
        word_t *blockStart = Block_GetBlockStartForWord((word_t *)object);
        BlockHeader_Mark(blockHeader);

        // Mark all Lines
        word_t *lastWord = Object_LastWord(object);

        assert(blockHeader == Block_GetBlockHeader(heap.blockHeaderStart, heap.heapStart, lastWord));
        LineHeader *firstHeader = Heap_LineHeaderForWord((word_t *)object);
        LineHeader *lastHeader = Heap_LineHeaderForWord(lastWord);
        assert(firstHeader <= lastHeader);
        for (LineHeader *lineHeader = firstHeader; lineHeader <= lastHeader; lineHeader++) {
            Line_Mark(lineHeader);
        }
    }
}

size_t Object_ChunkSize(Object *object) {
    if (object->rtti == NULL) {
        Chunk *chunk = (Chunk *) object;
        return chunk->size;
    } else {
        return MathUtils_RoundToNextMultiple(Object_Size(object), MIN_BLOCK_SIZE);
    }
}
