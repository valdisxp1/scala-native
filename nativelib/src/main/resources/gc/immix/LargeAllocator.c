#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "LargeAllocator.h"
#include "utils/MathUtils.h"
#include "Object.h"
#include "Log.h"
#include "headers/ObjectHeader.h"

inline static int LargeAllocator_sizeToLinkedListIndex(size_t size) {
    assert(size >= MIN_BLOCK_SIZE);
    return log2_floor(size) - LARGE_OBJECT_MIN_SIZE_BITS;
}

Chunk *LargeAllocator_chunkAddOffset(Chunk *chunk, size_t words) {
    return (Chunk *)((ubyte_t *)chunk + words);
}

void LargeAllocator_printFreeList(FreeList *list, int i) {
    Chunk *current = list->first;
    printf("list %d: ", i);
    while (current != NULL) {
        assert((1 << (i + LARGE_OBJECT_MIN_SIZE_BITS)) == current->size);
        printf("[%p %zu] -> ", current, current->size);
        current = current->next;
    }
    printf("\n");
}

void LargeAllocator_freeListAddBlockLast(FreeList *freeList, Chunk *chunk) {
    if (freeList->first == NULL) {
        freeList->first = chunk;
    }
    freeList->last = chunk;
    chunk->next = NULL;
}

Chunk *LargeAllocator_freeListRemoveFirstBlock(FreeList *freeList) {
    if (freeList->first == NULL) {
        return NULL;
    }
    Chunk *chunk = freeList->first;
    if (freeList->first == freeList->last) {
        freeList->last = NULL;
    }

    freeList->first = chunk->next;
    return chunk;
}

void LargeAllocator_freeListInit(FreeList *freeList) {
    freeList->first = NULL;
    freeList->last = NULL;
}

void LargeAllocator_Init(word_t *offset, size_t size, Bytemap *bytemap) {
    largeAllocator.offset = offset;
    largeAllocator.size = size;
    largeAllocator.bytemap = bytemap;

    for (int i = 0; i < FREE_LIST_COUNT; i++) {
        LargeAllocator_freeListInit(&largeAllocator.freeLists[i]);
    }

    LargeAllocator_AddChunk((Chunk *)offset, size);
}

void LargeAllocator_AddChunk(Chunk *chunk, size_t total_block_size) {
    assert(total_block_size >= MIN_BLOCK_SIZE);
    assert(total_block_size % MIN_BLOCK_SIZE == 0);

    size_t remaining_size = total_block_size;
    ubyte_t *current = (ubyte_t *)chunk;
    while (remaining_size > 0) {
        int log2_f = log2_floor(remaining_size);
        size_t chunkSize = 1UL << log2_f;
        chunkSize = chunkSize > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : chunkSize;
        assert(chunkSize >= MIN_BLOCK_SIZE && chunkSize <= MAX_BLOCK_SIZE);
        int listIndex = LargeAllocator_sizeToLinkedListIndex(chunkSize);

        Chunk *currentChunk = (Chunk *)current;
        LargeAllocator_freeListAddBlockLast(&largeAllocator.freeLists[listIndex],
                                            (Chunk *)current);

        currentChunk->nothing = NULL;
        currentChunk->size = chunkSize;
        Bytemap_SetPlaceholder(largeAllocator.bytemap, (word_t*) current);

        current += chunkSize;
        remaining_size -= chunkSize;
    }
}

Object *LargeAllocator_GetBlock(size_t requestedBlockSize) {
    size_t actualBlockSize =
        MathUtils_RoundToNextMultiple(requestedBlockSize, MIN_BLOCK_SIZE);
    size_t requiredChunkSize = 1UL << MathUtils_Log2Ceil(actualBlockSize);

    int listIndex = LargeAllocator_sizeToLinkedListIndex(requiredChunkSize);
    Chunk *chunk = NULL;
    while (listIndex <= FREE_LIST_COUNT - 1 &&
           (chunk = largeAllocator.freeLists[listIndex].first) == NULL) {
        ++listIndex;
    }

    if (chunk == NULL) {
        return NULL;
    }

    size_t chunkSize = chunk->size;
    assert(chunkSize >= MIN_BLOCK_SIZE);

    if (chunkSize - MIN_BLOCK_SIZE >= actualBlockSize) {
        Chunk *remainingChunk =
            LargeAllocator_chunkAddOffset(chunk, actualBlockSize);
        LargeAllocator_freeListRemoveFirstBlock(
            &largeAllocator.freeLists[listIndex]);
        size_t remainingChunkSize = chunkSize - actualBlockSize;
        LargeAllocator_AddChunk(remainingChunk, remainingChunkSize);
    } else {
        LargeAllocator_freeListRemoveFirstBlock(
            &largeAllocator.freeLists[listIndex]);
    }

    Bytemap_SetAllocated(largeAllocator.bytemap, (word_t*) chunk);
    Object *object = (Object *)chunk;
    memset(object, 0, actualBlockSize);
    return object;
}

void LargeAllocator_Print() {
    for (int i = 0; i < FREE_LIST_COUNT; i++) {
        if (largeAllocator.freeLists[i].first != NULL) {
            LargeAllocator_printFreeList(&largeAllocator.freeLists[i], i);
        }
    }
}

void LargeAllocator_clearFreeLists() {
    for (int i = 0; i < FREE_LIST_COUNT; i++) {
       largeAllocator.freeLists[i].first = NULL;
       largeAllocator.freeLists[i].last = NULL;
    }
}

void LargeAllocator_Sweep() {
    LargeAllocator_clearFreeLists();

    Object *current = (Object *)largeAllocator.offset;
    void *heapEnd = (ubyte_t *)largeAllocator.offset +largeAllocator.size;

    while (current != heapEnd) {
        assert(!Bytemap_IsFree(largeAllocator.bytemap, (word_t *)current));
        if (Bytemap_IsMarked(largeAllocator.bytemap, (word_t *)current)) {
            Bytemap_SetAllocated(largeAllocator.bytemap, (word_t *)current);

            current = Object_NextLargeObject(current);
        } else {
            size_t currentSize = Object_ChunkSize(current);
            Object *next = Object_NextLargeObject(current);
            while (next != heapEnd && !Bytemap_IsMarked(largeAllocator.bytemap, (word_t *)next)) {
                currentSize += Object_ChunkSize(next);
                Bytemap_SetFree(largeAllocator.bytemap, (word_t *)next);
                next = Object_NextLargeObject(next);
            }
            LargeAllocator_AddChunk((Chunk *)current, currentSize);
            current = next;
        }
    }
}
