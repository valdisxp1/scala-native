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
//    printf("ADD chunk (%p %p) size: %zu\n", chunk, (ubyte_t *) chunk + total_block_size, total_block_size);
//    fflush(stdout);
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

static inline Chunk* LargeAllocator_getChunkForSize(LargeAllocator *allocator, size_t requiredChunkSize) {
    for (int listIndex = LargeAllocator_sizeToLinkedListIndex(requiredChunkSize); listIndex < FREE_LIST_COUNT; listIndex++) {
        Chunk *chunk = allocator->freeLists[listIndex].first;
        if (chunk != NULL) {
            return chunk;
        }
    }
    return NULL;
}

Object *LargeAllocator_GetBlock(LargeAllocator *allocator,
                                size_t requestedBlockSize) {
    size_t actualBlockSize =
        MathUtils_RoundToNextMultiple(requestedBlockSize, MIN_BLOCK_SIZE);

    Chunk *chunk = NULL;
    size_t requiredChunkSize = 1UL << MathUtils_Log2Ceil(actualBlockSize);
    if (requiredChunkSize < BLOCK_TOTAL_SIZE) {
        // only need to look in free lists for chunks smaller than a block
        chunk = LargeAllocator_getChunkForSize(allocator, requiredChunkSize);
    }

    if (chunk == NULL) {
        uint32_t superblockSize = (uint32_t) MathUtils_DivAndRoundUp(requiredChunkSize, BLOCK_TOTAL_SIZE);
        BlockMeta *superblock = BlockAllocator_GetFreeSuperblock(allocator->blockAllocator, superblockSize);
        if (superblock != NULL) {
            chunk = (Chunk *) BlockMeta_GetBlockStart(allocator->blockMetaStart, allocator->heapStart, superblock);
            chunk->nothing = NULL;
            chunk->size = requiredChunkSize;
        }
    }

    if (chunk == NULL) {
        return NULL;
    }

    size_t chunkSize = chunk->size;
    assert(chunkSize >= MIN_BLOCK_SIZE);

    if (chunkSize - MIN_BLOCK_SIZE >= actualBlockSize) {
        Chunk *remainingChunk =
            LargeAllocator_chunkAddOffset(chunk, actualBlockSize);

        size_t remainingChunkSize = chunkSize - actualBlockSize;
        LargeAllocator_AddChunk(allocator, remainingChunk, remainingChunkSize);
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

void LargeAllocator_Sweep(LargeAllocator *allocator, BlockMeta *blockMeta, word_t *blockStart) {
    uint32_t superblockSize = BlockMeta_SuperblockSize(blockMeta);
    word_t *blockEnd = blockStart + WORDS_IN_BLOCK * superblockSize;
//    printf("block (%p %p) size:%u\n", blockStart, blockEnd, superblockSize);

    ObjectMeta *firstObject = Bytemap_Get(allocator->bytemap, blockStart);
//    printf("firstObject %p=%d\n", blockStart, *firstObject);
//    fflush(stdout);
    Object *current = (Object *)blockStart;
    if (!ObjectMeta_IsMarked(firstObject)) {
        // release free superblocks starting from the first object
        ObjectMeta_SetFree(firstObject);
        Object *next = Object_NextLargeObject(current);
        ObjectMeta *nextMeta = Bytemap_Get(allocator->bytemap, (word_t *)next);
        while ((word_t *) next < blockEnd && !ObjectMeta_IsMarked(nextMeta)) {
            assert(!ObjectMeta_IsFree(nextMeta));
            ObjectMeta_SetFree(nextMeta);
            next = Object_NextLargeObject(next);
            nextMeta = Bytemap_Get(allocator->bytemap, (word_t *)next);
        }
        word_t *freeBlockEnd = Block_GetBlockStartForWord((word_t *) next);
        uint32_t size = (uint32_t) (((word_t *) freeBlockEnd - blockStart) / WORDS_IN_BLOCK);
        if (size > 0) {
//            printf("SUPERBLOCK (%p, %p) size:%u\n", blockMeta, freeBlockEnd, size);
//            fflush(stdout);
            BlockAllocator_AddFreeBlocks(allocator->blockAllocator, blockMeta, size);
        }

        assert(size <= superblockSize);
        // mark the remaining superblock
        uint32_t remainingSuperblockSize = superblockSize - size;
        if (size > 0 && remainingSuperblockSize > 0) {
            BlockMeta *remainingSuperblock = blockMeta + size;
            BlockMeta_SetFlag(remainingSuperblock, block_superblock_start);
            BlockMeta_SetSuperblockSize(remainingSuperblock, remainingSuperblockSize);
        }

        assert(freeBlockEnd <= (word_t *) next);
        size_t remainingSizeInWords = (word_t *) next - freeBlockEnd;
        if (remainingSizeInWords > 0) {
            LargeAllocator_AddChunk(allocator, (Chunk *) current, remainingSizeInWords * WORD_SIZE);
        }

        current = next;
    }

    while ((word_t *) current < blockEnd) {
        ObjectMeta *currentMeta =
            Bytemap_Get(allocator->bytemap, (word_t *)current);
//        printf("object %p=%d\n", current, *currentMeta);
//        fflush(stdout);
        assert(!ObjectMeta_IsFree(currentMeta));
        if (ObjectMeta_IsMarked(currentMeta)) {
            ObjectMeta_SetAllocated(currentMeta);

            current = Object_NextLargeObject(current);
        } else {
            size_t currentSize = Object_ChunkSize(current);
            Object *next = Object_NextLargeObject(current);
            ObjectMeta *nextMeta =
                Bytemap_Get(allocator->bytemap, (word_t *)next);
            while ((word_t *) next < blockEnd && !ObjectMeta_IsMarked(nextMeta)) {
                assert(!ObjectMeta_IsFree(nextMeta));
                currentSize += Object_ChunkSize(next);
                ObjectMeta_SetFree(nextMeta);
                next = Object_NextLargeObject(next);
                nextMeta = Bytemap_Get(allocator->bytemap, (word_t *)next);
//                printf("object %p=%d NEXT\n", next, *nextMeta);
//                fflush(stdout);
            }
            LargeAllocator_AddChunk(allocator, (Chunk *)current, currentSize);
            current = next;
        }
    }
}
