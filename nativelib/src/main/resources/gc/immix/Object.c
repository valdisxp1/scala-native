#include <stddef.h>
#include <stdio.h>
#include "Object.h"
#include "Block.h"
#include "Log.h"
#include "utils/MathUtils.h"
#include "metadata/BlockMeta.h"

#define LAST_FIELD_OFFSET -1

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

Object *Object_getInnerPointer(word_t *blockStart, word_t *word,
                               ObjectMeta *wordMeta, bool youngObject) {
    word_t *current = word;
    ObjectMeta *currentMeta = wordMeta;
    while (current >= blockStart && ObjectMeta_IsFree(currentMeta)) {
        current -= ALLOCATION_ALIGNMENT_WORDS;
        currentMeta = Bytemap_PreviousWord(currentMeta);
    }
    Object *object = (Object *)current;
    if (((youngObject && ObjectMeta_IsAllocated(currentMeta)) || (!youngObject && ObjectMeta_IsMarked(currentMeta))) &&
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

Object *Object_findObject(Heap *heap, word_t *word, bool youngObject) {
    BlockMeta *blockMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, word);

    if (!Object_isAligned(word)) {
#ifdef DEBUG_PRINT
        printf("Word not aligned: %p aligning to %p\n", word,
               (word_t *)((word_t)word & ALLOCATION_ALIGNMENT_INVERSE_MASK));
        fflush(stdout);
#endif
        word = (word_t *)((word_t)word & ALLOCATION_ALIGNMENT_INVERSE_MASK);
    }

    ObjectMeta *wordMeta = Bytemap_Get(heap->smallBytemap, word);
    if (ObjectMeta_IsPlaceholder(wordMeta)) {
        return NULL;
    }
    else if (youngObject && ObjectMeta_IsAllocated(wordMeta)) {
        return (Object *)word;
    } else if (youngObject && ObjectMeta_IsMarked(wordMeta)) {
        return NULL;
    } else if (!youngObject && ObjectMeta_IsMarked(wordMeta)) {
        return (Object *)word;
    } else if (!youngObject && ObjectMeta_IsAllocated(wordMeta)) {
        return NULL;
    } else {
        word_t *blockStart = Block_GetBlockStartForWord(word);
        return Object_getInnerPointer(blockStart, word, wordMeta, youngObject);
    }
}

Object *Object_GetYoungObject(Heap *heap, word_t *word) {
    return Object_findObject(heap, word, true);
}

Object *Object_GetOldObject(Heap *heap, word_t *word) {
    return Object_findObject(heap, word, false);
}

Object *Object_getLargeInnerPointer(word_t *word, ObjectMeta *wordMeta, bool youngObject) {
    word_t *current = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    ObjectMeta *currentMeta = wordMeta;

    while (ObjectMeta_IsFree(currentMeta)) {
        current -= ALLOCATION_ALIGNMENT_WORDS;
        currentMeta = Bytemap_PreviousWord(currentMeta);
    }

    Object *object = (Object *)current;
    if (((youngObject && ObjectMeta_IsAllocated(currentMeta))||(!youngObject && ObjectMeta_IsMarked(currentMeta))) &&
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

Object *Object_findLargeObject(Bytemap *bytemap, word_t *word, bool youngObject) {
    if (((word_t)word & LARGE_BLOCK_MASK) != (word_t)word) {
        word = (word_t *)((word_t)word & LARGE_BLOCK_MASK);
    }
    ObjectMeta *wordMeta = Bytemap_Get(bytemap, word);
    if (ObjectMeta_IsPlaceholder(wordMeta)) {
        printf("Placeholder object\n");fflush(stdout);
        return NULL;
    } else if (youngObject && ObjectMeta_IsAllocated(wordMeta)) {
        return (Object *)word;
    } else if (youngObject && ObjectMeta_IsMarked(wordMeta)) {
        return NULL;
    } else if (!youngObject && ObjectMeta_IsMarked(wordMeta)) {
        return (Object *)word;
    } else if (!youngObject && ObjectMeta_IsAllocated(wordMeta)) {
        return NULL;
    } else {
        Object *object = Object_getLargeInnerPointer(word, wordMeta, youngObject);
        assert(object == NULL ||
               (word >= (word_t *)object &&
                word < (word_t *)Object_NextLargeObject(object)));
        return object;
    }
}

Object *Object_GetLargeYoungObject(Bytemap *bytemap, word_t *word) {
    return Object_findLargeObject(bytemap, word, true);
}

Object *Object_GetLargeOldObject(Bytemap *bytemap, word_t *word) {
    return Object_findLargeObject(bytemap, word, false);
}

void Object_Mark(Heap *heap, Object *object, ObjectMeta *objectMeta, bool collectingOld) {
    // Mark the object itself
    if(!collectingOld) {
        assert(ObjectMeta_IsAllocated(objectMeta));
        ObjectMeta_SetMarked(objectMeta);
    } else {
        assert(ObjectMeta_IsMarked(objectMeta));
        ObjectMeta_SetAllocated(objectMeta);
    }

    if (Heap_IsWordInSmallHeap(heap, (word_t *)object)) {
        // Mark the block
        BlockMeta *blockMeta = Block_GetBlockMeta(
            heap->blockMetaStart, heap->heapStart, (word_t *)object);
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

bool Object_HasPointerToYoungObject(Heap *heap, Object *object, bool largeObject) {
    word_t *blockStart = Block_GetBlockStartForWord((word_t *)object);
    if (Object_IsArray(object)) {
        if (object->rtti->rt.id == __object_array_id) {
            ArrayHeader *arrayHeader = (ArrayHeader *)object;
            size_t length = arrayHeader->length;
            word_t **fields = (word_t **)(arrayHeader + 1);
            for (int i  = 0; i < length; i++) {
                word_t *field = fields[i];
                Bytemap *bytemap = Heap_BytemapForWord(heap, field);
                if (bytemap != NULL) {
                    ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                    word_t *currentBlockStart = Block_GetBlockStartForWord(field);
                    BlockMeta *currentBlockMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, field);
                    if (largeObject) {
                        if (!BlockMeta_IsOld(currentBlockMeta) && ObjectMeta_IsAllocated(fieldMeta)) {
                            return true;
                        }
                    } else if (!ObjectMeta_IsFree(fieldMeta)){
                        if (blockStart > currentBlockStart && !BlockMeta_IsOld(currentBlockMeta)) {
                            return true;
                        }
                        if (blockStart < currentBlockStart && (BlockMeta_GetAge(currentBlockMeta) < MAX_AGE_YOUNG_BLOCK - 1)) {
                            return true;
                        }
                    }
                }
            }
        }
    } else {
        int64_t *ptr_map = object->rtti->refMapStruct;
        int i = 0;
        while (ptr_map[i] != LAST_FIELD_OFFSET) {
            word_t *field = object->fields[ptr_map[i]];
            Bytemap *bytemap = Heap_BytemapForWord(heap, field);
            if (bytemap != NULL) {
                ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                word_t *currentBlockStart = Block_GetBlockStartForWord(field);
                BlockMeta *currentBlockMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, field);
                if (largeObject) {
                    if (!BlockMeta_IsOld(currentBlockMeta) && ObjectMeta_IsAllocated(fieldMeta)) {
                        return true;
                    }
                } else if (!ObjectMeta_IsFree(fieldMeta)){
                    if (blockStart > currentBlockStart && !BlockMeta_IsOld(currentBlockMeta)) {
                        return true;
                    }
                    if (blockStart < currentBlockStart && (BlockMeta_GetAge(currentBlockMeta) < MAX_AGE_YOUNG_BLOCK - 1)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool Object_HasPointerToOldObject(Heap *heap, Object *object, bool largeObject) {
    word_t *blockStart = Block_GetBlockStartForWord((word_t *)object);
    if (Object_IsArray(object)) {
        if (object->rtti->rt.id == __object_array_id) {
            ArrayHeader *arrayHeader = (ArrayHeader *)object;
            size_t length = arrayHeader->length;
            word_t **fields = (word_t **)(arrayHeader + 1);
            for (int i  = 0; i < length; i++) {
                word_t *field = fields[i];
                Bytemap *bytemap = Heap_BytemapForWord(heap, field);
                if (bytemap != NULL) {
                    ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                    word_t *currentBlockStart = Block_GetBlockStartForWord(field);
                    BlockMeta *currentBlockMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, field);
                    if (largeObject) {
                        if (BlockMeta_IsOld(currentBlockMeta) && ObjectMeta_IsMarked(fieldMeta)) {
                            return true;
                        }
                    } else if (!ObjectMeta_IsFree(fieldMeta)){
                        if (blockStart > currentBlockStart && BlockMeta_IsOld(currentBlockMeta)) {
                            return true;
                        }
                        if (blockStart < currentBlockStart && (BlockMeta_GetAge(currentBlockMeta) == MAX_AGE_YOUNG_BLOCK - 1)) {
                            return true;
                        }
                    }
                }
            }
        }
    } else {
        int64_t *ptr_map = object->rtti->refMapStruct;
        int i = 0;
        while (ptr_map[i] != LAST_FIELD_OFFSET) {
            word_t *field = object->fields[ptr_map[i]];
            Bytemap *bytemap = Heap_BytemapForWord(heap, field);
            if (bytemap != NULL) {
                ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                word_t *currentBlockStart = Block_GetBlockStartForWord(field);
                BlockMeta *currentBlockMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, field);
                if (largeObject) {
                    if (BlockMeta_IsOld(currentBlockMeta) && ObjectMeta_IsMarked(fieldMeta)) {
                        return true;
                    }
                } else if (!ObjectMeta_IsFree(fieldMeta)){
                    if (blockStart > currentBlockStart && BlockMeta_IsOld(currentBlockMeta)) {
                        return true;
                    }
                    if (blockStart < currentBlockStart && (BlockMeta_GetAge(currentBlockMeta) == MAX_AGE_YOUNG_BLOCK - 1)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}
