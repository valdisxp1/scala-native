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

static inline bool Object_isAligned(word_t *word) {
    return ((word_t)word & ALLOCATION_ALIGNMENT_INVERSE_MASK) == (word_t)word;
}

Object *Object_getInnerPointer(Bytemap *bytemap, word_t *blockStart, word_t *word) {
    word_t *current = word;
    while (current >= blockStart && Bytemap_IsFree(bytemap, current)) {
        current -= ALLOCATION_ALIGNMENT_WORDS;
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

Object *Object_GetUnmarkedObject(Heap *heap, word_t *word) {
    BlockHeader *blockHeader = Block_GetBlockHeader(heap->blockHeaderStart, heap->heapStart, word);
    word_t *blockStart = Block_GetBlockStartForWord(word);

    if (!Object_isAligned(word)) {
#ifdef DEBUG_PRINT
        printf("Word not aligned: %p aligning to %p\n", word,
               (word_t *)((word_t)word & ALLOCATION_ALIGNMENT_INVERSE_MASK));
        fflush(stdout);
#endif
        word = (word_t *)((word_t)word & ALLOCATION_ALIGNMENT_INVERSE_MASK);
    }

    if (Bytemap_IsPlaceholder(heap->smallBytemap, word) || Bytemap_IsMarked(heap->smallBytemap, word)) {
        return NULL;
    } else if (Bytemap_IsAllocated(heap->smallBytemap, word)) {
        return (Object *) word;
    } else {
       return Object_getInnerPointer(heap->smallBytemap, blockStart, word);
    }
}

Object *Object_getLargeInnerPointer(LargeAllocator *allocator, word_t *word) {
    word_t *current = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    Bytemap *bytemap = allocator->bytemap;

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

Object *Object_GetLargeUnmarkedObject(LargeAllocator *allocator, word_t *word) {
    if (((word_t)word & LARGE_BLOCK_MASK) != (word_t)word) {
        word = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    }
    if (Bytemap_IsPlaceholder(allocator->bytemap, word) || Bytemap_IsMarked(allocator->bytemap, word)) {
        return NULL;
    } else if (Bytemap_IsAllocated(allocator->bytemap, word)) {
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
    Bytemap *bytemap = Heap_BytemapForWord(heap, (word_t*) object);
    Bytemap_SetMarked(bytemap, (word_t*) object);

    if (Heap_IsWordInSmallHeap(heap, (word_t*) object)) {
        // Mark the block
        BlockHeader *blockHeader = Block_GetBlockHeader(heap->blockHeaderStart, heap->heapStart, (word_t *)object);
        word_t *blockStart = Block_GetBlockStartForWord((word_t *)object);
        BlockHeader_Mark(blockHeader);

        // Mark all Lines
        word_t *lastWord = Object_LastWord(object);

        assert(blockHeader == Block_GetBlockHeader(heap->blockHeaderStart, heap->heapStart, lastWord));
        LineHeader *firstHeader = Heap_LineHeaderForWord(heap, (word_t *)object);
        LineHeader *lastHeader = Heap_LineHeaderForWord(heap, lastWord);
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
