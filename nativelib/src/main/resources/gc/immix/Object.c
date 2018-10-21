#include <stddef.h>
#include <stdio.h>
#include "Object.h"
#include "Block.h"
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

Object *Object_getInnerPointer(word_t *blockStart, word_t *word,
                               ObjectMeta *wordMeta) {
    word_t *current = word;
    ObjectMeta *currentMeta = wordMeta;
    while (current >= blockStart && ObjectMeta_IsFree(currentMeta)) {
        // maybe use MIN_LARGE_BLOCK/WORDS when traversing large objects
        current -= ALLOCATION_ALIGNMENT_WORDS;
        currentMeta -= 1;
    }
    Object *object = (Object *)current;
    if (ObjectMeta_IsAllocated(currentMeta) &&
        word < current + Object_Size(object) / WORD_SIZE) {
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
    BlockMeta *blockMeta =
        Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, word);
    word_t *blockStart = Block_GetBlockStartForWord(word);

    if (BlockMeta_ContainsLargeObjects(blockMeta)) {
        word = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    } else {
        word = (word_t *)((word_t)word & ALLOCATION_ALIGNMENT_INVERSE_MASK);
    }

    ObjectMeta *wordMeta = Bytemap_Get(heap->bytemap, word);
    if (ObjectMeta_IsPlaceholder(wordMeta) || ObjectMeta_IsMarked(wordMeta)) {
        return NULL;
    } else if (ObjectMeta_IsAllocated(wordMeta)) {
        return (Object *)word;
    } else {
        return Object_getInnerPointer(blockStart, word, wordMeta);
    }
}

void Object_Mark(Heap *heap, Object *object, ObjectMeta *objectMeta) {
    // Mark the object itself
    ObjectMeta_SetMarked(objectMeta);

    BlockMeta *blockMeta = Block_GetBlockMeta(
        heap->blockMetaStart, heap->heapStart, (word_t *)object);
    if (!BlockMeta_ContainsLargeObjects(blockMeta)) {
        // Mark the block
        word_t *blockStart = Block_GetBlockStartForWord((word_t *)object);
        BlockMeta_Mark(blockMeta);

        // Mark all Lines
        word_t *lastWord = Object_LastWord(object);

        assert(blockMeta == Block_GetBlockMeta(heap->blockMetaStart,
                                               heap->heapStart, lastWord));
        LineMeta *firstLineMeta = Heap_LineMetaForWord(heap, (word_t *)object);
        LineMeta *lastLineMeta = Heap_LineMetaForWord(heap, lastWord);
        assert(firstLineMeta <= lastLineMeta);
        for (LineMeta *lineMeta = firstLineMeta; lineMeta <= lastLineMeta;
             lineMeta++) {
            Line_Mark(lineMeta);
        }
    }
}

size_t Object_ChunkSize(Object *object) {
    if (object->rtti == NULL) {
        Chunk *chunk = (Chunk *)object;
        return chunk->size;
    } else {
        return MathUtils_RoundToNextMultiple(Object_Size(object),
                                             MIN_BLOCK_SIZE);
    }
}
