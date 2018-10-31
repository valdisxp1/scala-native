#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>
#include "Heap.h"
#include "Log.h"
#include "Allocator.h"
#include "Marker.h"
#include "State.h"
#include "utils/MathUtils.h"
#include "StackTrace.h"
#include "Settings.h"
#include "Memory.h"
#include <memory.h>
#include <time.h>

// Allow read and write
#define HEAP_MEM_PROT (PROT_READ | PROT_WRITE)
// Map private anonymous memory, and prevent from reserving swap
#define HEAP_MEM_FLAGS (MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS)
// Map anonymous memory (not a file)
#define HEAP_MEM_FD -1
#define HEAP_MEM_FD_OFFSET 0

void Heap_sweep(Heap *heap, uint32_t maxCount);
void Heap_sweepDone(Heap *heap);
Object *Heap_lazySweep(Heap *heap, uint32_t size);
Object *Heap_lazySweepLarge(Heap *heap, uint32_t size);

void Heap_exitWithOutOfMemory() {
    printf("Out of heap space\n");
    StackTrace_PrintStackTrace();
    exit(1);
}

bool Heap_isGrowingPossible(Heap *heap, uint32_t incrementInBlocks) {
    return heap->blockCount + incrementInBlocks <= heap->maxBlockCount;
}

size_t Heap_getMemoryLimit() {
    size_t memorySize = getMemorySize();
    if ((uint64_t)memorySize > MAX_HEAP_SIZE) {
        return (size_t)MAX_HEAP_SIZE;
    } else {
        return memorySize;
    }
}

/**
 * Maps `MAX_SIZE` of memory and returns the first address aligned on
 * `alignement` mask
 */
word_t *Heap_mapAndAlign(size_t memoryLimit, size_t alignmentSize) {
    assert(alignmentSize % WORD_SIZE == 0);
    word_t *heapStart = mmap(NULL, memoryLimit, HEAP_MEM_PROT, HEAP_MEM_FLAGS,
                             HEAP_MEM_FD, HEAP_MEM_FD_OFFSET);

    size_t alignmentMask = ~(alignmentSize - 1);
    // Heap start not aligned on
    if (((word_t)heapStart & alignmentMask) != (word_t)heapStart) {
        word_t *previousBlock = (word_t *)((word_t)heapStart & alignmentMask);
        heapStart = previousBlock + alignmentSize / WORD_SIZE;
    }
    return heapStart;
}


/**
 * Allocates the heap struct and initializes it
 */
void Heap_Init(Heap *heap, size_t minHeapSize, size_t maxHeapSize) {
    size_t memoryLimit = Heap_getMemoryLimit();

    if (maxHeapSize < MIN_HEAP_SIZE) {
        fprintf(stderr,
                "SCALANATIVE_MAX_HEAP_SIZE too small to initialize heap.\n");
        fprintf(stderr, "Minimum required: %zum \n",
                MIN_HEAP_SIZE / 1024 / 1024);
        fflush(stderr);
        exit(1);
    }

    if (minHeapSize > memoryLimit) {
        fprintf(stderr, "SCALANATIVE_MIN_HEAP_SIZE is too large.\n");
        fprintf(stderr, "Maximum possible: %zug \n",
                memoryLimit / 1024 / 1024 / 1024);
        fflush(stderr);
        exit(1);
    }

    if (maxHeapSize < minHeapSize) {
        fprintf(stderr, "SCALANATIVE_MAX_HEAP_SIZE should be at least "
                        "SCALANATIVE_MIN_HEAP_SIZE\n");
        fflush(stderr);
        exit(1);
    }

    if (minHeapSize < MIN_HEAP_SIZE) {
        minHeapSize = MIN_HEAP_SIZE;
    }

    if (maxHeapSize == UNLIMITED_HEAP_SIZE) {
        maxHeapSize = memoryLimit;
    }

    uint32_t maxNumberOfBlocks = maxHeapSize / SPACE_USED_PER_BLOCK;
    uint32_t initialBlockCount = minHeapSize / SPACE_USED_PER_BLOCK;
    heap->maxHeapSize = maxHeapSize;
    heap->blockCount = initialBlockCount;
    heap->maxBlockCount = maxNumberOfBlocks;

    // reserve space for block headers
    size_t blockMetaSpaceSize = maxNumberOfBlocks * sizeof(BlockMeta);
    word_t *blockMetaStart = Heap_mapAndAlign(blockMetaSpaceSize, WORD_SIZE);
    heap->blockMetaStart = blockMetaStart;
    heap->blockMetaEnd =
        blockMetaStart + initialBlockCount * sizeof(BlockMeta) / WORD_SIZE;

    // reserve space for line headers
    size_t lineMetaSpaceSize =
        (size_t)maxNumberOfBlocks * LINE_COUNT * LINE_METADATA_SIZE;
    word_t *lineMetaStart = Heap_mapAndAlign(lineMetaSpaceSize, WORD_SIZE);
    heap->lineMetaStart = lineMetaStart;
    assert(LINE_COUNT * LINE_SIZE == BLOCK_TOTAL_SIZE);
    assert(LINE_COUNT * LINE_METADATA_SIZE % WORD_SIZE == 0);
    heap->lineMetaEnd = lineMetaStart + initialBlockCount * LINE_COUNT *
                                            LINE_METADATA_SIZE / WORD_SIZE;

    word_t *heapStart = Heap_mapAndAlign(maxHeapSize, BLOCK_TOTAL_SIZE);

    BlockAllocator_Init(&blockAllocator, blockMetaStart, initialBlockCount);

    // reserve space for bytemap
    Bytemap *bytemap = (Bytemap *)Heap_mapAndAlign(
        maxHeapSize / ALLOCATION_ALIGNMENT + sizeof(Bytemap),
        ALLOCATION_ALIGNMENT);
    heap->bytemap = bytemap;

    // Init heap for small objects
    heap->heapSize = minHeapSize;
    heap->heapStart = heapStart;
    heap->heapEnd = heapStart + minHeapSize / WORD_SIZE;
    heap->sweep.cursor = SWEEP_DONE;
    heap->sweep.cursorDone = SWEEP_DONE;
    Bytemap_Init(bytemap, heapStart, maxHeapSize);
    Allocator_Init(&allocator, &blockAllocator, bytemap, blockMetaStart,
                   heapStart);

    LargeAllocator_Init(&largeAllocator, &blockAllocator, bytemap,
                        blockMetaStart, heapStart);
    char *statsFile = Settings_StatsFileName();
    if (statsFile != NULL) {
        heap->stats = malloc(sizeof(Stats));
        Stats_Init(heap->stats, statsFile);
    }
}

Object *Heap_lazySweepLarge(Heap *heap, uint32_t size) {
    Object *object = LargeAllocator_GetBlock(&largeAllocator, size);
    size_t increment = MathUtils_DivAndRoundUp(size, BLOCK_TOTAL_SIZE);
    while (object == NULL && !Heap_IsSweepDone(heap)) {
        Heap_sweep(heap, increment);
        object = LargeAllocator_GetBlock(&largeAllocator, size);
    }
    return object;
}

/**
 * Allocates large objects using the `LargeAllocator`.
 * If allocation fails, because there is not enough memory available, it will
 * trigger a collection of both the small and the large heap.
 */
word_t *Heap_AllocLarge(Heap *heap, uint32_t size) {

    assert(size % ALLOCATION_ALIGNMENT == 0);
    assert(size >= MIN_BLOCK_SIZE);

    // Request an object from the `LargeAllocator`
    Object *object = Heap_lazySweepLarge(heap, size);
    // If the object is not NULL, update it's metadata and return it
    if (object != NULL) {
        return (word_t *)object;
    } else {
        // Otherwise collect
        Heap_Collect(heap, &stack);

        // After collection, try to alloc again, if it fails, grow the heap by
        // at least the size of the object we want to alloc
        object = LargeAllocator_GetBlock(&largeAllocator, size);
        if (object != NULL) {
            assert(Heap_IsWordInHeap(heap, (word_t *)object));
            return (word_t *)object;
        } else {
            size_t increment = MathUtils_DivAndRoundUp(size, BLOCK_TOTAL_SIZE);
            uint32_t pow2increment = 1U << MathUtils_Log2Ceil(increment);
            Heap_Grow(heap, pow2increment);

            object = LargeAllocator_GetBlock(&largeAllocator, size);
            assert(object != NULL);
            assert(Heap_IsWordInHeap(heap, (word_t *)object));
            return (word_t *)object;
        }
    }
}

Object *Heap_lazySweep(Heap *heap, uint32_t size) {
    Object *object = (Object *)Allocator_Alloc(&allocator, size);
    while (object == NULL && !Heap_IsSweepDone(heap)) {
        Heap_sweep(heap, 1);
        Object *object = (Object *)Allocator_Alloc(&allocator, size);
    }
    return object;
}

NOINLINE word_t *Heap_allocSmallSlow(Heap *heap, uint32_t size) {
    Object *object = Heap_lazySweep(heap, size);

    if (object != NULL)
        goto done;

    Heap_Collect(heap, &stack);
    object = Heap_lazySweep(heap, size);

    if (object != NULL)
        goto done;

    // A small object can always fit in a single free block
    // because it is no larger than 8K while the block is 32K.
    Heap_Grow(heap, 1);
    object = (Object *)Allocator_Alloc(&allocator, size);

done:
    assert(Heap_IsWordInHeap(heap, (word_t *)object));
    assert(object != NULL);
    ObjectMeta *objectMeta = Bytemap_Get(allocator.bytemap, (word_t *)object);
    ObjectMeta_SetAllocated(objectMeta);
    return (word_t *)object;
}

INLINE word_t *Heap_AllocSmall(Heap *heap, uint32_t size) {
    assert(size % ALLOCATION_ALIGNMENT == 0);
    assert(size < MIN_BLOCK_SIZE);

    word_t *start = allocator.cursor;
    word_t *end = (word_t *)((uint8_t *)start + size);

    // Checks if the end of the block overlaps with the limit
    if (end >= allocator.limit) {
        return Heap_allocSmallSlow(heap, size);
    }

    allocator.cursor = end;

    memset(start, 0, size);

    Object *object = (Object *)start;
    ObjectMeta *objectMeta = Bytemap_Get(allocator.bytemap, (word_t *)object);
    ObjectMeta_SetAllocated(objectMeta);

    __builtin_prefetch(object + 36, 0, 3);

    assert(Heap_IsWordInHeap(heap, (word_t *)object));
    return (word_t *)object;
}

word_t *Heap_Alloc(Heap *heap, uint32_t objectSize) {
    assert(objectSize % ALLOCATION_ALIGNMENT == 0);

    if (objectSize >= LARGE_BLOCK_SIZE) {
        return Heap_AllocLarge(heap, objectSize);
    } else {
        return Heap_AllocSmall(heap, objectSize);
    }
}

void Heap_Collect(Heap *heap, Stack *stack) {
    assert(Heap_IsSweepDone(heap));
    uint64_t start_ns, end_ns;
    Stats *stats = heap->stats;
#ifdef DEBUG_PRINT
    printf("\nCollect\n");
    fflush(stdout);
#endif
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
    }
    Marker_MarkRoots(heap, stack);
    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordMark(stats, start_ns, end_ns);
    }
    Heap_Recycle(heap);
#ifdef DEBUG_PRINT
    printf("End collect\n");
    fflush(stdout);
#endif
}

bool Heap_shouldGrow(Heap *heap) {
    uint32_t freeBlockCount = blockAllocator.freeBlockCount;
    uint32_t blockCount = heap->blockCount;
    uint32_t recycledBlockCount = allocator.recycledBlockCount;
    uint32_t unavailableBlockCount =
        blockCount - (freeBlockCount + recycledBlockCount);

#ifdef DEBUG_PRINT
    printf("\n\nBlock count: %llu\n", blockCount);
    printf("Unavailable: %llu\n", unavailableBlockCount);
    printf("Free: %llu\n", freeBlockCount);
    printf("Recycled: %llu\n", recycledBlockCount);
    fflush(stdout);
#endif

    return freeBlockCount * 2 < blockCount ||
           4 * unavailableBlockCount > blockCount;
}

void Heap_sweep(Heap *heap, uint32_t maxCount) {
    uint64_t start_ns, end_ns;
    Stats *stats = heap->stats;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
    }

    uint32_t startIdx = heap->sweep.cursor;
    uint32_t limitIdx = startIdx + maxCount;
    heap->sweep.cursor = limitIdx;
    uint32_t blockCount = heap->blockCount;
    if (limitIdx > blockCount) {
        limitIdx = blockCount;
    }

    BlockMeta *lastFreeBlockStart = NULL;

    BlockMeta *current = BlockMeta_GetFromIndex(heap->blockMetaStart, startIdx);
    BlockMeta *limit = BlockMeta_GetFromIndex(heap->blockMetaStart, limitIdx);
    word_t *currentBlockStart = Block_GetStartFromIndex(heap->heapStart, startIdx);
    LineMeta *lineMetas = Line_getFromBlockIndex(heap->lineMetaStart, startIdx);
    while (current < limit) {
        int size = 1;
        uint32_t freeCount = 0;
        if (BlockMeta_IsSimpleBlock(current)) {
            freeCount = Allocator_Sweep(&allocator, current, currentBlockStart, lineMetas);
        } else if (BlockMeta_IsSuperblockStart(current)) {
            size = BlockMeta_SuperblockSize(current);
            freeCount = LargeAllocator_Sweep(&largeAllocator, current, currentBlockStart);
        } else if (BlockMeta_IsFree(current)) {
            freeCount = 1;
        }
        // ignore superblock middle blocks, that superblock will be swept by someone else
        assert(freeCount <= size);
        if (lastFreeBlockStart == NULL) {
            if (freeCount > 0) {
                lastFreeBlockStart = current;
            }
        } else {
            if (freeCount < size) {
                BlockMeta *freeLimit = current + freeCount;
                uint32_t totalSize = (uint32_t) (freeLimit - lastFreeBlockStart);
                assert(totalSize > 0);
                BlockAllocator_AddFreeSuperblock(&blockAllocator, lastFreeBlockStart, totalSize);
                lastFreeBlockStart = NULL;
            }
        }

        assert(size > 0);
        current += size;
        currentBlockStart += WORDS_IN_BLOCK * size;
        lineMetas += LINE_COUNT * size;
    }
    if (lastFreeBlockStart != NULL) {
        uint32_t totalSize = (uint32_t) (limit - lastFreeBlockStart);
        assert(totalSize > 0);
        BlockAllocator_AddFreeSuperblock(&blockAllocator, lastFreeBlockStart, totalSize);
    }

    heap->sweep.cursorDone = limitIdx;

    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordLazySweep(stats, start_ns, end_ns);
    }

    if (Heap_IsSweepDone(heap)) {
        Heap_sweepDone(heap);
    }
}

void Heap_Recycle(Heap *heap) {
    Allocator_Clear(&allocator);
    LargeAllocator_Clear(&largeAllocator);
    BlockAllocator_Clear(&blockAllocator);

    heap->sweep.cursor = 0;
    Heap_sweep(heap, heap->blockCount);
}

void Heap_sweepDone(Heap *heap) {
    if (Heap_shouldGrow(heap)) {
        double growth;
        if (heap->heapSize < EARLY_GROWTH_THRESHOLD) {
            growth = EARLY_GROWTH_RATE;
        } else {
            growth = GROWTH_RATE;
        }
        uint32_t blocks = heap->blockCount * (growth - 1);
        if (Heap_isGrowingPossible(heap, blocks)) {
            Heap_Grow(heap, blocks);
        } else {
            uint32_t remainingGrowth = heap->maxBlockCount - heap->blockCount;
            if (remainingGrowth > 0) {
                Heap_Grow(heap, remainingGrowth);
            }
        }
    }
    if (!Allocator_CanInitCursors(&allocator)) {
        Heap_exitWithOutOfMemory();
    }
    heap->sweep.cursorDone = SWEEP_DONE;
    Stats *stats = heap->stats;
    if (stats != NULL) {
        Stats_RecordCollectionDone(stats);
    }
}

void Heap_Grow(Heap *heap, uint32_t incrementInBlocks) {
    if (!Heap_isGrowingPossible(heap, incrementInBlocks)) {
        Heap_exitWithOutOfMemory();
    }

#ifdef DEBUG_PRINT
    printf("Growing small heap by %zu bytes, to %zu bytes\n",
           increment * WORD_SIZE,
           heap->heapSize + incrementInBlocks * SPACE_USED_PER_BLOCK);
    fflush(stdout);
#endif

    word_t *heapEnd = heap->heapEnd;
    heap->heapEnd = heapEnd + incrementInBlocks * BLOCK_TOTAL_SIZE;
    heap->heapSize += incrementInBlocks * SPACE_USED_PER_BLOCK;
    word_t *blockMetaEnd = heap->blockMetaEnd;
    heap->blockMetaEnd =
        (word_t *)(((BlockMeta *)heap->blockMetaEnd) + incrementInBlocks);
    heap->lineMetaEnd +=
        incrementInBlocks * LINE_COUNT * LINE_METADATA_SIZE / WORD_SIZE;

    BlockAllocator_AddFreeBlocks(&blockAllocator, (BlockMeta *)blockMetaEnd,
                                 incrementInBlocks);

    heap->blockCount += incrementInBlocks;
}
