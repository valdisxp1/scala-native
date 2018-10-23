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
    assert(size % MIN_BLOCK_SIZE == 0);
    int index = size / MIN_BLOCK_SIZE;
    assert(index < FREE_LIST_COUNT);
    return size / MIN_BLOCK_SIZE;
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

    int listIndex = LargeAllocator_sizeToLinkedListIndex(total_block_size);
    chunk->nothing = NULL;
    chunk->size = total_block_size;
    ObjectMeta *chunkMeta = Bytemap_Get(allocator->bytemap, (word_t *)chunk);
    ObjectMeta_SetPlaceholder(chunkMeta);

    LargeAllocator_freeListAddBlockLast(&allocator->freeLists[listIndex], chunk);
}

static inline Chunk* LargeAllocator_getChunkForSize(LargeAllocator *allocator, size_t requiredChunkSize) {
    for (int listIndex = LargeAllocator_sizeToLinkedListIndex(requiredChunkSize); listIndex < FREE_LIST_COUNT; listIndex++) {
        Chunk *chunk = allocator->freeLists[listIndex].first;
        if (chunk != NULL) {
            LargeAllocator_freeListRemoveFirstBlock(&allocator->freeLists[listIndex]);
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
    if (actualBlockSize < BLOCK_TOTAL_SIZE) {
        // only need to look in free lists for chunks smaller than a block
        chunk = LargeAllocator_getChunkForSize(allocator, actualBlockSize);
    }

    if (chunk == NULL) {
        uint32_t superblockSize = (uint32_t) MathUtils_DivAndRoundUp(actualBlockSize, BLOCK_TOTAL_SIZE);
        BlockMeta *superblock = BlockAllocator_GetFreeSuperblock(allocator->blockAllocator, superblockSize);
        if (superblock != NULL) {
            chunk = (Chunk *) BlockMeta_GetBlockStart(allocator->blockMetaStart, allocator->heapStart, superblock);
            chunk->nothing = NULL;
            chunk->size = superblockSize * BLOCK_TOTAL_SIZE;
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
    assert(MIN_BLOCK_SIZE * 4 == BLOCK_TOTAL_SIZE);
    uint32_t superblockSize = BlockMeta_SuperblockSize(blockMeta);
    word_t *blockEnd = blockStart + WORDS_IN_BLOCK * superblockSize;
//    printf("block [%p] (%p %p) size:%u\n", blockMeta, blockStart, blockEnd, superblockSize);

    ObjectMeta *firstObject = Bytemap_Get(allocator->bytemap, blockStart);

//    printf("firstObject [%p] %p=%d\n", firstObject , blockStart, *firstObject);
//    fflush(stdout);
    assert(!ObjectMeta_IsFree(firstObject));
    if (superblockSize > 1 && !ObjectMeta_IsMarked(firstObject)) {
        // release free superblocks starting from the first object
        if (size > 0) {
            printf("SUPERBLOCK [%p] (%p %p) size:%u\n", blockMeta, blockStart, blockEnd - WORDS_IN_BLOCK, superblockSize - 1);
            fflush(stdout);
            BlockAllocator_AddFreeBlocks(allocator->blockAllocator, blockMeta, superblockSize - 1);
        }

    }
    ObjectMeta beforeSweep[4];

    beforeSweep[0] = *firstObject;
    ObjectMeta_Sweep(firstObject);

    word_t *startOfLastBlock = blockEnd - WORDS_IN_BLOCK;
    ObjectMeta *cutpoint = Bytemap_Get(allocator->bytemap, startOfLastBlock);

    {
    int i = 1;
    ObjectMeta *cutpoint0 = cutpoint + i * MIN_BLOCK_SIZE / ALLOCATION_ALIGNMENT;
    ObjectMeta before = *cutpoint0;
    beforeSweep[i] = *before;
    *cutpoint0 = (before & 0x04) >> 1;
    }

    {
    int i = 2;
    ObjectMeta *cutpoint0 = cutpoint + i * MIN_BLOCK_SIZE / ALLOCATION_ALIGNMENT;
    ObjectMeta before = *cutpoint0;
    beforeSweep[i] = *before;
    *cutpoint0 = (before & 0x04) >> 1;
    }

    {
    int i = 3;
    ObjectMeta *cutpoint0 = cutpoint + i * MIN_BLOCK_SIZE / ALLOCATION_ALIGNMENT;
    ObjectMeta before = *cutpoint0;
    beforeSweep[i] = *before;
    *cutpoint0 = (before & 0x04) >> 1;
    }


    uint64_t = beforeSweep_singleNum = ((uint32_t *)beforeSweep)[0]
    if ((beforeSweep_singleNum & 0x04040404) == 0) {
        // all is free, therefore the block is free
        BlockAllocator_AddFreeBlocks(allocator->blockAllocator, blockMeta + superblockSize - 1, 1);
        return;
    }


    int startIndex;

    {
    int i = 0;
        if (beforeSweep[i] & 0x3) {
            startIndex = i;
        } else {
            startIndex = -1
        }
    }

    {
    int i = 1;
        if (startIndex == -1) {
            if (beforeSweep[i] & 0x3) {
                startIndex = i;
            }
        } else {
            if (ObjectMeta_IsMarked(beforeSweep[i])) {
                // send [startIndex, i - 1]
                word_t *current = startOfLastBlock + startIndex * MIN_BLOCK_SIZE;
                size_t currentSize = (size_t) (i - startIndex) * MIN_BLOCK_SIZE;
                LargeAllocator_AddChunk(allocator, (Chunk *)current, currentSize);
                startIndex = -1;
            }
        }
    }

    {
    int i = 2;
        if (startIndex == -1) {
            if (beforeSweep[i] & 0x3) {
                startIndex = i;
            }
        } else {
            if (ObjectMeta_IsMarked(beforeSweep[i])) {
                // send [startIndex, i - 1]
                word_t *current = startOfLastBlock + startIndex * MIN_BLOCK_SIZE;
                size_t currentSize = (size_t) (i - startIndex) * MIN_BLOCK_SIZE;
                LargeAllocator_AddChunk(allocator, (Chunk *)current, currentSize);
                startIndex = -1;
            }
        }
    }

    {
    int i = 3;
        if (startIndex == -1) {
            if (beforeSweep[i] & 0x3) {
                startIndex = i;
                // send [startIndex, 3]
                word_t *current = startOfLastBlock + startIndex * MIN_BLOCK_SIZE;
                size_t currentSize = (size_t) (3 - startIndex) * MIN_BLOCK_SIZE;
                LargeAllocator_AddChunk(allocator, (Chunk *)current, currentSize);
            }
        } else {
            if (ObjectMeta_IsMarked(beforeSweep[i])) {
                // send [startIndex, i - 1]
                word_t *current = startOfLastBlock + startIndex * MIN_BLOCK_SIZE;
                size_t currentSize = (size_t) (i - startIndex) * MIN_BLOCK_SIZE;
                LargeAllocator_AddChunk(allocator, (Chunk *)current, currentSize);
                startIndex = -1;
            } else {
                // send [startIndex, 3]
                word_t *current = startOfLastBlock + startIndex * MIN_BLOCK_SIZE;
                size_t currentSize = (size_t) (3 - startIndex) * MIN_BLOCK_SIZE;
                LargeAllocator_AddChunk(allocator, (Chunk *)current, currentSize);
            }
        }
    }
}
