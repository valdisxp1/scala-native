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
    return MathUtils_Log2Floor(size) - LARGE_OBJECT_MIN_SIZE_BITS;
}

Chunk *LargeAllocator_chunkAddOffset(Chunk *chunk, size_t words) {
    return (Chunk *)((ubyte_t *)chunk + words);
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

void LargeAllocator_Init(LargeAllocator *allocator, BlockAllocator *blockAllocator, Bytemap *bytemap,
                         word_t *blockMetaStart, word_t *heapStart) {
    allocator->heapStart = heapStart;
    allocator->blockMetaStart = blockMetaStart;
    allocator->bytemap = bytemap;
    allocator->blockAllocator = blockAllocator;

    for (int i = 0; i < FREE_LIST_COUNT; i++) {
        LargeAllocator_freeListInit(&allocator->freeLists[i]);
    }
}

void LargeAllocator_AddChunk(LargeAllocator *allocator, Chunk *chunk,
                             size_t total_block_size) {
    assert(total_block_size >= MIN_BLOCK_SIZE);
    assert(total_block_size % MIN_BLOCK_SIZE == 0);

    size_t remaining_size = total_block_size;
    ubyte_t *current = (ubyte_t *)chunk;
    while (remaining_size > 0) {
        int log2_f = MathUtils_Log2Floor(remaining_size);
        size_t chunkSize = 1UL << log2_f;
        chunkSize = chunkSize > MAX_BLOCK_SIZE ? MAX_BLOCK_SIZE : chunkSize;
        assert(chunkSize >= MIN_BLOCK_SIZE && chunkSize <= MAX_BLOCK_SIZE);
        int listIndex = LargeAllocator_sizeToLinkedListIndex(chunkSize);

        Chunk *currentChunk = (Chunk *)current;
        LargeAllocator_freeListAddBlockLast(&allocator->freeLists[listIndex],
                                            (Chunk *)current);

        currentChunk->nothing = NULL;
        currentChunk->size = chunkSize;
        ObjectMeta *currentMeta =
            Bytemap_Get(allocator->bytemap, (word_t *)current);
        ObjectMeta_SetPlaceholder(currentMeta);

        current += chunkSize;
        remaining_size -= chunkSize;
    }
}

Object *LargeAllocator_GetBlock(LargeAllocator *allocator,
                                size_t requestedBlockSize) {
    size_t actualBlockSize =
        MathUtils_RoundToNextMultiple(requestedBlockSize, MIN_BLOCK_SIZE);
    size_t requiredChunkSize = 1UL << MathUtils_Log2Ceil(actualBlockSize);

    int listIndex = LargeAllocator_sizeToLinkedListIndex(requiredChunkSize);
    Chunk *chunk = NULL;
    while (listIndex <= FREE_LIST_COUNT - 1 &&
           (chunk = allocator->freeLists[listIndex].first) == NULL) {
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
            &allocator->freeLists[listIndex]);
        size_t remainingChunkSize = chunkSize - actualBlockSize;
        LargeAllocator_AddChunk(allocator, remainingChunk, remainingChunkSize);
    } else {
        LargeAllocator_freeListRemoveFirstBlock(
            &allocator->freeLists[listIndex]);
    }

    ObjectMeta *objectMeta = Bytemap_Get(allocator->bytemap, (word_t *)chunk);
    ObjectMeta_SetAllocated(objectMeta);
    Object *object = (Object *)chunk;
    memset(object, 0, actualBlockSize);
    return object;
}

void LargeAllocator_Clear(LargeAllocator *allocator) {
    for (int i = 0; i < FREE_LIST_COUNT; i++) {
        allocator->freeLists[i].first = NULL;
        allocator->freeLists[i].last = NULL;
    }
}

static inline void LargeAllocator_FreeSpace(LargeAllocator *allocator, Chunk *chunk, size_t total_block_size) {
    // round the start up
    word_t *superblockStart = (word_t *) MathUtils_RoundToNextMultiple((word_t) chunk, BLOCK_TOTAL_SIZE);
    word_t *chunkEnd = (word_t *) ((ubyte_t *) chunk + total_block_size);
    // round the end down
    word_t *superblockEnd = Block_GetBlockStartForWord(chunkEnd);

    if (superblockEnd > superblockStart) {
        // before
        size_t beforeSize = (ubyte_t *) superblockStart - (ubyte_t *) chunk;
        if (beforeSize > 0) {
            LargeAllocator_AddChunk(allocator, chunk, beforeSize);
        }
        // superblock
        BlockMeta *superblock = Block_GetBlockMeta(allocator->blockMetaStart, allocator->heapStart, superblockStart);
        uint32_t size = (uint32_t) ((superblockEnd - superblockStart) / WORDS_IN_BLOCK);
        BlockAllocator_AddFreeBlocks(allocator->blockAllocator, superblock, size);
        // after
        size_t afterSize = (ubyte_t *) chunkEnd - (ubyte_t *) superblockEnd;
        if (afterSize > 0) {
            LargeAllocator_AddChunk(allocator, (Chunk *) superblockEnd, total_block_size);
        }
    } else {
        LargeAllocator_AddChunk(allocator, chunk, total_block_size);
    }
}

void LargeAllocator_Sweep(LargeAllocator *allocator, BlockMeta *blockMeta, word_t *blockStart) {
    Object *current = (Object *)blockStart;
    void *blockEnd = blockStart + WORDS_IN_BLOCK * blockMeta->superblockSize;

    while (current != blockEnd) {
        ObjectMeta *currentMeta =
            Bytemap_Get(allocator->bytemap, (word_t *)current);
        assert(!ObjectMeta_IsFree(currentMeta));
        if (ObjectMeta_IsMarked(currentMeta)) {
            ObjectMeta_SetAllocated(currentMeta);

            current = Object_NextLargeObject(current);
        } else {
            size_t currentSize = Object_ChunkSize(current);
            Object *next = Object_NextLargeObject(current);
            ObjectMeta *nextMeta =
                Bytemap_Get(allocator->bytemap, (word_t *)next);
            while (next != blockEnd && !ObjectMeta_IsMarked(nextMeta)) {
                assert(!ObjectMeta_IsFree(nextMeta));
                currentSize += Object_ChunkSize(next);
                ObjectMeta_SetFree(nextMeta);
                next = Object_NextLargeObject(next);
                nextMeta = Bytemap_Get(allocator->bytemap, (word_t *)next);
            }
            LargeAllocator_FreeSpace(allocator, (Chunk *)current, currentSize);
            current = next;
        }
    }
}
