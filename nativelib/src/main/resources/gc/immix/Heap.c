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
#include "GCThread.h"
#include <memory.h>
#include <time.h>
#include <inttypes.h>
#include <sched.h>

// Allow read and write
#define HEAP_MEM_PROT (PROT_READ | PROT_WRITE)
// Map private anonymous memory, and prevent from reserving swap
#define HEAP_MEM_FLAGS (MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS)
// Map anonymous memory (not a file)
#define HEAP_MEM_FD -1
#define HEAP_MEM_FD_OFFSET 0

void Heap_sweepDone(Heap *heap);
Object *Heap_lazySweep(Heap *heap, uint32_t size);
Object *Heap_lazySweepLarge(Heap *heap, uint32_t size);
void Heap_lazyCoalesce(Heap *heap);

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
    heap->sweep.cursor = initialBlockCount;
    heap->sweep.cursorDone = initialBlockCount;
    heap->sweep.limit = initialBlockCount;
    heap->coalesce = BlockRange_Pack(initialBlockCount, initialBlockCount);
    heap->postSweepDone = true;
    Bytemap_Init(bytemap, heapStart, maxHeapSize);
    Allocator_Init(&allocator, &blockAllocator, bytemap, blockMetaStart,
                   heapStart);

    LargeAllocator_Init(&largeAllocator, &blockAllocator, bytemap,
                        blockMetaStart, heapStart);

    // Init all GCThreads
    pthread_mutex_init(&heap->gcThreads.startMutex, NULL);
    pthread_cond_init(&heap->gcThreads.start, NULL);

    int gcThreadCount = Settings_GCThreadCount();
    heap->gcThreadCount = gcThreadCount;
    gcThreads = (GCThread *) malloc(sizeof(GCThread) * gcThreadCount);
    for (int i = 0; i < gcThreadCount; i++) {
        GCThread_Init(&gcThreads[i], i, heap);
    }

    char *statsFile = Settings_StatsFileName();
    if (statsFile != NULL) {
        heap->stats = malloc(sizeof(Stats));
        Stats_Init(heap->stats, statsFile);
    }
}

Object *Heap_lazySweepLarge(Heap *heap, uint32_t size) {
    Object *object = LargeAllocator_GetBlock(&largeAllocator, size);
    uint32_t increment = (uint32_t) MathUtils_DivAndRoundUp(size, BLOCK_TOTAL_SIZE);
    #ifdef DEBUG_PRINT
        printf("Heap_lazySweepLarge (%" PRIu32 ") => %" PRIu32 "\n", size, increment);
        fflush(stdout);
    #endif
    while (object == NULL && !Heap_IsSweepDone(heap)) {
        Heap_Sweep(heap, &heap->sweep.cursorDone, LAZY_SWEEP_MIN_BATCH);
        object = LargeAllocator_GetBlock(&largeAllocator, size);
    }
    if (Heap_IsSweepDone(heap) && !heap->postSweepDone) {
        Heap_sweepDone(heap);
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
        object = Heap_lazySweepLarge(heap, size);
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
        Heap_Sweep(heap, &heap->sweep.cursorDone, LAZY_SWEEP_MIN_BATCH);
        object = (Object *)Allocator_Alloc(&allocator, size);
    }
    if (Heap_IsSweepDone(heap) && !heap->postSweepDone) {
        Heap_sweepDone(heap);
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
    #ifdef DEBUG_ASSERT
        ObjectMeta_AssertIsValidAllocation(objectMeta, size);
    #endif
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
    #ifdef DEBUG_ASSERT
        ObjectMeta_AssertIsValidAllocation(objectMeta, size);
    #endif
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

#ifdef DEBUG_ASSERT
void Heap_clearIsSwept(Heap *heap) {
    BlockMeta *current = (BlockMeta *) heap->blockMetaStart;
    BlockMeta *limit = (BlockMeta *) heap->blockMetaEnd;
    while (current < limit) {
        current->debugFlag = dbg_must_sweep;
        current++;
    }
}

void Heap_assertIsConsistent(Heap *heap) {
    BlockMeta *current = (BlockMeta *) heap->blockMetaStart;
    LineMeta *lineMetas = (LineMeta *) heap->lineMetaStart;
    BlockMeta *limit = (BlockMeta *) heap->blockMetaEnd;
    ObjectMeta *currentBlockStart = Bytemap_Get(heap->bytemap, heap->heapStart);
    while (current < limit) {
        assert(!BlockMeta_IsCoalesceMe(current));
        assert(!BlockMeta_IsSuperblockStartMe(current));
        assert(!BlockMeta_IsSuperblockMiddle(current));
        assert(!BlockMeta_IsMarked(current));

        int size = 1;
        if (BlockMeta_IsSuperblockStart(current)) {
            size = BlockMeta_SuperblockSize(current);
        }
        BlockMeta *next = current + size;
        LineMeta *nextLineMetas = lineMetas + LINE_COUNT * size;
        ObjectMeta *nextBlockStart = currentBlockStart + (WORDS_IN_BLOCK / ALLOCATION_ALIGNMENT_WORDS) * size;

        for (LineMeta *line = lineMetas; line < nextLineMetas; line++) {
            assert(!Line_IsMarked(line));
        }
        for (ObjectMeta *object = currentBlockStart; object < nextBlockStart; object++) {
            assert(!ObjectMeta_IsMarked(object));
        }

        current = next;
        lineMetas = nextLineMetas;
        currentBlockStart = nextBlockStart;
    }
    assert(current == limit);
}
#endif

void Heap_Collect(Heap *heap, Stack *stack) {
    assert(Heap_IsSweepDone(heap));
#ifdef DEBUG_ASSERT
    Heap_clearIsSwept(heap);
    Heap_assertIsConsistent(heap);
#endif
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
        Stats_RecordEvent(stats, event_mark, start_ns, end_ns);
    }
    Heap_Recycle(heap);
#ifdef DEBUG_PRINT
    printf("End collect\n");
    fflush(stdout);
#endif
}

bool Heap_shouldGrow(Heap *heap) {
    uint32_t freeBlockCount = (uint32_t) blockAllocator.freeBlockCount;
    uint32_t blockCount = heap->blockCount;
    uint32_t recycledBlockCount = (uint32_t) allocator.recycledBlockCount;
    uint32_t unavailableBlockCount =
        blockCount - (freeBlockCount + recycledBlockCount);

#ifdef DEBUG_PRINT
    printf("\n\nBlock count: %" PRIu32 "\n", blockCount);
    printf("Unavailable: %" PRIu32 "\n", unavailableBlockCount);
    printf("Free: %" PRIu32 "\n", freeBlockCount);
    printf("Recycled: %" PRIu32 "\n", recycledBlockCount);
    fflush(stdout);
#endif

    return freeBlockCount * 2 < blockCount ||
           4 * unavailableBlockCount > blockCount;
}

void Heap_Sweep(Heap *heap, atomic_uint_fast32_t *cursorDone, uint32_t maxCount) {
    uint64_t start_ns, end_ns;
    Stats *stats = heap->stats;
    if (stats != NULL) {
        start_ns = scalanative_nano_time();
    }
    uint32_t cursor = heap->sweep.cursor;
    uint32_t sweepLimit = heap->sweep.limit;
    // protect against sweep.cursor overflow
    uint32_t startIdx = sweepLimit;
    if (cursor < sweepLimit) {
        startIdx = (uint32_t) atomic_fetch_add(&heap->sweep.cursor, maxCount);
    }
    uint32_t limitIdx = startIdx + maxCount;
    assert(*cursorDone <= startIdx);
    if (limitIdx > sweepLimit) {
        limitIdx = sweepLimit;
    }

    BlockMeta *lastFreeBlockStart = NULL;

    BlockMeta *first = BlockMeta_GetFromIndex(heap->blockMetaStart, startIdx);
    BlockMeta *limit = BlockMeta_GetFromIndex(heap->blockMetaStart, limitIdx);

    #ifdef DEBUG_PRINT
        printf("Heap_sweep(%p %" PRIu32 ",%p %" PRIu32 "\n",
               first, startIdx, limit, limitIdx);
        fflush(stdout);
    #endif

    // skip superblock_middle these are handled by the previous batch
    // (BlockMeta_IsSuperblockStartMe(first) || BlockMeta_IsSuperblockMiddle(first)) && first < limit
    while (((first->block.simple.flags & 0x3) == 0x3) && first < limit) {
        #ifdef DEBUG_PRINT
            printf("Heap_sweep SuperblockMiddle %p %" PRIu32 "\n",
                   first, BlockMeta_GetBlockIndex(heap->blockMetaStart, first));
            fflush(stdout);
        #endif
        startIdx += 1;
        first += 1;
    }

    BlockMeta *current = first;
    word_t *currentBlockStart = Block_GetStartFromIndex(heap->heapStart, startIdx);
    LineMeta *lineMetas = Line_getFromBlockIndex(heap->lineMetaStart, startIdx);
    while (current < limit) {
        int size = 1;
        uint32_t freeCount = 0;
        assert(!BlockMeta_IsCoalesceMe(current));
        assert(!BlockMeta_IsSuperblockMiddle(current));
        assert(!BlockMeta_IsSuperblockStartMe(current));
        if (BlockMeta_IsSimpleBlock(current)) {
            freeCount = Allocator_Sweep(&allocator, current, currentBlockStart, lineMetas);
            #ifdef DEBUG_PRINT
                printf("Heap_sweep SimpleBlock %p %" PRIu32 "\n",
                       current, BlockMeta_GetBlockIndex(heap->blockMetaStart, current));
                fflush(stdout);
            #endif
        } else if (BlockMeta_IsSuperblockStart(current)) {
            size = BlockMeta_SuperblockSize(current);
            assert(size > 0);
            freeCount = LargeAllocator_Sweep(&largeAllocator, current, currentBlockStart, limit);
            #ifdef DEBUG_PRINT
                printf("Heap_sweep Superblock(%" PRIu32 ") %p %" PRIu32 "\n",
                       size, current, BlockMeta_GetBlockIndex(heap->blockMetaStart, current));
                fflush(stdout);
            #endif
        } else {
            assert(BlockMeta_IsFree(current));
            freeCount = 1;
            assert(current->debugFlag == dbg_must_sweep);
            #ifdef DEBUG_ASSERT
                current->debugFlag = dbg_free;
            #endif
            #ifdef DEBUG_PRINT
                printf("Heap_sweep FreeBlock %p %" PRIu32 "\n",
                       current, BlockMeta_GetBlockIndex(heap->blockMetaStart, current));
                fflush(stdout);
            #endif
        }
        // ignore superblock middle blocks, that superblock will be swept by someone else
        assert(size > 0);
        assert(freeCount <= size);
        if (lastFreeBlockStart == NULL) {
            if (freeCount > 0) {
                lastFreeBlockStart = current;
            }
        }
        if (lastFreeBlockStart != NULL && freeCount < size) {
            BlockMeta *freeLimit = current + freeCount;
            uint32_t totalSize = (uint32_t) (freeLimit - lastFreeBlockStart);
            if (lastFreeBlockStart == first || freeLimit >= limit) {
                // Free blocks in the start or the end
                // There may be some free blocks before this batch that needs to be coalesced with this block.
                BlockMeta_SetFlag(lastFreeBlockStart, block_coalesce_me);
                BlockMeta_SetSuperblockSize(lastFreeBlockStart, totalSize);
            } else {
                // Free blocks in the middle
                assert(totalSize > 0);
                BlockAllocator_AddFreeSuperblock(&blockAllocator, lastFreeBlockStart, totalSize);
            }
            lastFreeBlockStart = NULL;
        }

        current += size;
        currentBlockStart += WORDS_IN_BLOCK * size;
        lineMetas += LINE_COUNT * size;
    }
    BlockMeta *doneUntil = current;
    if (lastFreeBlockStart != NULL) {
        // Free blocks in the end or the entire batch is free
        uint32_t totalSize = (uint32_t) (doneUntil - lastFreeBlockStart);
        assert(totalSize > 0);
        // There may be some free blocks after this batch that needs to be coalesced with this block.
        BlockMeta_SetFlag(lastFreeBlockStart, block_coalesce_me);
        BlockMeta_SetSuperblockSize(lastFreeBlockStart, totalSize);
    }

    // coalescing might be done by another thread
    // block_coalesce_me marks should be visible
    atomic_thread_fence(memory_order_seq_cst);

    *cursorDone = limitIdx;

    Heap_lazyCoalesce(heap);

    if (stats != NULL) {
        end_ns = scalanative_nano_time();
        Stats_RecordEvent(stats, event_sweep, start_ns, end_ns);
    }

}

uint_fast32_t Heap_minSweepCursor(Heap *heap) {
    uint_fast32_t min = heap->sweep.cursorDone;
    int gcThreadCount = heap->gcThreadCount;
    for (int i = 0; i < gcThreadCount; i++) {
        uint_fast32_t cursorDone = gcThreads[i].sweep.cursorDone;
        if (gcThreads[i].active && cursorDone < min) {
            min = cursorDone;
        }
    }
    return min;
}

void Heap_lazyCoalesce(Heap *heap) {
    // the previous coalesce is done and there is work
    BlockRangeVal coalesce = heap->coalesce;
    uint_fast32_t startIdx = BlockRange_Limit(coalesce);
    uint_fast32_t coalesceDoneIdx = BlockRange_First(coalesce);
    uint_fast32_t limitIdx = Heap_minSweepCursor(heap);
    assert(coalesceDoneIdx <= startIdx);
    BlockRangeVal newValue = BlockRange_Pack(coalesceDoneIdx, limitIdx);
    while (startIdx == coalesceDoneIdx && startIdx < limitIdx) {
        if (!atomic_compare_exchange_strong(&heap->coalesce, &coalesce, newValue)) {
            // coalesce is updated by atomic_compare_exchange_strong
            startIdx = BlockRange_Limit(coalesce);
            coalesceDoneIdx = BlockRange_First(coalesce);
            limitIdx = Heap_minSweepCursor(heap);
            newValue = BlockRange_Pack(coalesceDoneIdx, limitIdx);
            assert(coalesceDoneIdx <= startIdx);
            continue;
        }

        BlockMeta *lastFreeBlockStart = NULL;
        BlockMeta *lastCoalesceMe = NULL;
        BlockMeta *first = BlockMeta_GetFromIndex(heap->blockMetaStart, (uint32_t) startIdx);
        BlockMeta *current = first;
        BlockMeta *limit = BlockMeta_GetFromIndex(heap->blockMetaStart, (uint32_t) limitIdx);

        while (current < limit) {
            // updates lastFreeBlockStart and adds blocks
            if (lastFreeBlockStart == NULL) {
                if (BlockMeta_IsCoalesceMe(current)) {
                    lastFreeBlockStart = current;
                }
            } else {
                if (!BlockMeta_IsCoalesceMe(current)) {
                    BlockMeta *freeLimit = current;
                    uint32_t totalSize = (uint32_t) (freeLimit - lastFreeBlockStart);
                    BlockAllocator_AddFreeBlocks(&blockAllocator, lastFreeBlockStart, totalSize);
                    lastFreeBlockStart = NULL;
                }
            }

            // controls movement forward
            int size = 1;
            if (BlockMeta_IsCoalesceMe(current)) {
                lastCoalesceMe = current;
                size = BlockMeta_SuperblockSize(current);
            } else if (BlockMeta_IsSuperblockStart(current)) {
                size = BlockMeta_SuperblockSize(current);
            } else if (BlockMeta_IsSuperblockStartMe(current)) {
                // finish the LargeAllocator_Sweep in the case when the last block is not free
                BlockMeta_SetFlag(current, block_superblock_start);
                BlockMeta_SetSuperblockSize(current, 1);
            }

            current += size;
        }
        if (lastFreeBlockStart != NULL) {
            assert(current <= (BlockMeta *) heap->blockMetaEnd);
            assert(current >= limit);
            if (current == limit) {
                uint32_t totalSize = (uint32_t) (limit - lastFreeBlockStart);
                BlockAllocator_AddFreeBlocks(&blockAllocator, lastFreeBlockStart, totalSize);
            } else {
                // the last superblock crossed the limit
                // other sweepers still need to sweep it
                // add the part that is fully swept
                uint32_t totalSize = (uint32_t) (lastCoalesceMe - lastFreeBlockStart);
                assert(lastFreeBlockStart + totalSize <= limit);
                if (totalSize > 0) {
                    BlockAllocator_AddFreeBlocks(&blockAllocator, lastFreeBlockStart, totalSize);
                }
                // try to advance the sweep cursor past the superblock
                uint_fast32_t advanceTo = BlockMeta_GetBlockIndex(heap->blockMetaStart, current);
                uint_fast32_t sweepCursor = heap->sweep.cursor;
                while (sweepCursor < advanceTo) {
                    atomic_compare_exchange_strong(&heap->sweep.cursor, &sweepCursor, advanceTo);
                    // sweepCursor is updated by atomic_compare_exchange_strong
                }
                // retreat the coalesce cursor
                uint_fast32_t retreatTo = BlockMeta_GetBlockIndex(heap->blockMetaStart, lastCoalesceMe);
                heap->coalesce = BlockRange_Pack(retreatTo, retreatTo);
                // do no more to avoid infinite loops
                return;
            }
        }

        heap->coalesce = BlockRange_Pack(limitIdx, limitIdx);
    }
}

NOINLINE void Heap_waitForGCThreadsSlow(int gcThreadCount) {
    // extremely unlikely to enter here
    // unless very many threads running
    bool anyActive = true;
    while (anyActive) {
        sched_yield();
        anyActive = false;
        for (int i = 0; i < gcThreadCount; i++) {
            anyActive |= gcThreads[i].active;
       }
    }
}

INLINE void Heap_waitForGCThreads(Heap *heap) {
    int gcThreadCount = heap->gcThreadCount;
    bool anyActive = false;
    for (int i = 0; i < gcThreadCount; i++) {
        anyActive |= gcThreads[i].active;
    }
    if (anyActive) {
        Heap_waitForGCThreadsSlow(gcThreadCount);
    }
}

void Heap_Recycle(Heap *heap) {
    Allocator_Clear(&allocator);
    LargeAllocator_Clear(&largeAllocator);
    BlockAllocator_Clear(&blockAllocator);

    // all the marking changes should be visible to all threads by now
    atomic_thread_fence(memory_order_seq_cst);

    // before changing the cursor and limit values, makes sure no gc threads are running
    Heap_waitForGCThreads(heap);

    heap->sweep.cursor = 0;
    heap->sweep.limit = heap->blockCount;
    heap->sweep.cursorDone = 0;
    heap->coalesce = BlockRange_Pack(0, 0);
    heap->postSweepDone = false;

    pthread_mutex_t *startMutex = &heap->gcThreads.startMutex;
    pthread_cond_t *start = &heap->gcThreads.start;

    // wake all the GC threads
    pthread_mutex_lock(startMutex);
    pthread_cond_broadcast(start);
    pthread_mutex_unlock(startMutex);
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
    heap->postSweepDone = true;
}

void Heap_Grow(Heap *heap, uint32_t incrementInBlocks) {
    if (!Heap_isGrowingPossible(heap, incrementInBlocks)) {
        Heap_exitWithOutOfMemory();
    }

#ifdef DEBUG_PRINT
    printf("Growing small heap by %zu bytes, to %zu bytes\n",
           incrementInBlocks * SPACE_USED_PER_BLOCK,
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

#ifdef DEBUG_ASSERT
    BlockMeta *end = (BlockMeta *)blockMetaEnd;
    for (BlockMeta *block = end; block < end + incrementInBlocks; block++) {
        block->debugFlag = dbg_free;
    }
#endif

    BlockAllocator_AddFreeBlocks(&blockAllocator, (BlockMeta *)blockMetaEnd,
                                 incrementInBlocks);

    heap->blockCount += incrementInBlocks;
}
