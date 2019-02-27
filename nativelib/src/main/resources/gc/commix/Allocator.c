#include <stdlib.h>
#include "Allocator.h"
#include "State.h"
#include "Sweeper.h"
#include <stdio.h>
#include <memory.h>

bool Allocator_getNextLine(Allocator *allocator);
bool Allocator_newBlock(Allocator *allocator);
bool Allocator_newOverflowBlock(Allocator *allocator);

void Allocator_Init(Allocator *allocator, BlockAllocator *blockAllocator,
                    Bytemap *bytemap, word_t *blockMetaStart,
                    word_t *heapStart) {
    allocator->blockMetaStart = blockMetaStart;
    allocator->blockAllocator = blockAllocator;
    allocator->bytemap = bytemap;
    allocator->heapStart = heapStart;

    BlockList_Init(&allocator->recycledBlocks);

    allocator->recycledBlockCount = 0;

    // Init cursor
    bool didInit = Allocator_newBlock(allocator);
    assert(didInit);

    // Init large cursor
    bool didLargeInit = Allocator_newOverflowBlock(allocator);
    assert(didLargeInit);
}

/**
 * The Allocator needs one free block for overflow allocation and a free or
 * recyclable block for normal allocation.
 *
 * @param allocator
 * @return `true` if there are enough block to initialise the cursors, `false`
 * otherwise.
 */
bool Allocator_CanInitCursors(Allocator *allocator) {
    uint32_t freeBlockCount =
        (uint32_t)allocator->blockAllocator->freeBlockCount;
    return freeBlockCount >= 2 ||
           (freeBlockCount == 1 && allocator->recycledBlockCount > 0);
}

void Allocator_Clear(Allocator *allocator) {
    BlockList_Clear(&allocator->recycledBlocks);
    allocator->recycledBlockCount = 0;
    allocator->limit = NULL;
    allocator->block = NULL;
    allocator->largeLimit = NULL;
    allocator->largeBlock = NULL;
}

bool Allocator_newOverflowBlock(Allocator *allocator) {
    BlockMeta *largeBlock =
        BlockAllocator_GetFreeBlock(allocator->blockAllocator);
    if (largeBlock == NULL) {
        return false;
    }
    allocator->largeBlock = largeBlock;
    word_t *largeBlockStart = BlockMeta_GetBlockStart(
        allocator->blockMetaStart, allocator->heapStart, largeBlock);
    allocator->largeBlockStart = largeBlockStart;
    allocator->largeCursor = largeBlockStart;
    allocator->largeLimit = Block_GetBlockEnd(largeBlockStart);
    return true;
}

/**
 * Overflow allocation uses only free blocks, it is used when the bump limit of
 * the fast allocator is too small to fit
 * the block to alloc.
 */
word_t *Allocator_overflowAllocation(Allocator *allocator, size_t size) {
    word_t *start = allocator->largeCursor;
    word_t *end = (word_t *)((uint8_t *)start + size);

    // allocator->largeLimit == NULL implies end > allocator->largeLimit
    assert(allocator->largeLimit != NULL || end > allocator->largeLimit);
    if (end > allocator->largeLimit) {
        if (!Allocator_newOverflowBlock(allocator)) {
            return NULL;
        }
        return Allocator_overflowAllocation(allocator, size);
    }

    allocator->largeCursor = end;

    return start;
}

/**
 * Allocation fast path, uses the cursor and limit.
 */
INLINE word_t *Allocator_tryAlloc(Allocator *allocator, size_t size) {
    word_t *start = allocator->cursor;
    word_t *end = (word_t *)((uint8_t *)start + size);

    // allocator->limit == NULL implies end > allocator->limit
    assert(allocator->limit != NULL || end > allocator->limit);
    if (end > allocator->limit) {
        // If it overlaps but the block to allocate is a `medium` sized block,
        // use overflow allocation
        if (size > LINE_SIZE) {
            return Allocator_overflowAllocation(allocator, size);
        } else {
            // Otherwise try to get a new line.
            if (Allocator_getNextLine(allocator)) {
                return Allocator_tryAlloc(allocator, size);
            }

            return NULL;
        }
    }

    allocator->cursor = end;

    return start;
}

/**
 * Updates the cursor and the limit of the Allocator to point the next line.
 */
bool Allocator_getNextLine(Allocator *allocator) {
    BlockMeta *block = allocator->block;
    if (block == NULL) {
        return Allocator_newBlock(allocator);
    }
    word_t *blockStart = allocator->blockStart;

    int lineIndex = BlockMeta_FirstFreeLine(block);
    if (lineIndex == LAST_HOLE) {
        return Allocator_newBlock(allocator);
    }

    word_t *line = Block_GetLineAddress(blockStart, lineIndex);

    allocator->cursor = line;
    FreeLineMeta *lineMeta = (FreeLineMeta *)line;
    BlockMeta_SetFirstFreeLine(block, lineMeta->next);
    uint16_t size = lineMeta->size;
    allocator->limit = line + (size * WORDS_IN_LINE);
    assert(allocator->limit <= Block_GetBlockEnd(blockStart));

    return true;
}

/**
 * Updates the the cursor and the limit of the Allocator to point to the first
 * free line of the new block.
 */
bool Allocator_newBlock(Allocator *allocator) {
    bool concurrent = allocator->blockAllocator->concurrent;
    word_t *blockMetaStart = allocator->blockMetaStart;
    BlockMeta *block;
    if (concurrent) {
        block = BlockList_Pop(&allocator->recycledBlocks, blockMetaStart);
    } else {
        block = BlockList_PopOnlyThread(&allocator->recycledBlocks,
                                        blockMetaStart);
    }
    word_t *blockStart;

    if (block != NULL) {
        // get all the changes done by sweeping
        atomic_thread_fence(memory_order_acquire);
#ifdef DEBUG_PRINT
        printf("Allocator_newBlock RECYCLED %p %" PRIu32 "\n", block,
               BlockMeta_GetBlockIndex(blockMetaStart, block));
        fflush(stdout);
#endif
        assert(block->debugFlag == dbg_partial_free);
#ifdef DEBUG_ASSERT
        block->debugFlag = dbg_in_use;
#endif
        blockStart = BlockMeta_GetBlockStart(blockMetaStart,
                                             allocator->heapStart, block);

        int lineIndex = BlockMeta_FirstFreeLine(block);
        assert(lineIndex < LINE_COUNT);
        word_t *line = Block_GetLineAddress(blockStart, lineIndex);

        allocator->cursor = line;
        FreeLineMeta *lineMeta = (FreeLineMeta *)line;
        BlockMeta_SetFirstFreeLine(block, lineMeta->next);
        uint16_t size = lineMeta->size;
        assert(size > 0);
        allocator->limit = line + (size * WORDS_IN_LINE);
        assert(allocator->limit <= Block_GetBlockEnd(blockStart));
    } else {
        block = BlockAllocator_GetFreeBlock(allocator->blockAllocator);
#ifdef DEBUG_PRINT
        printf("Allocator_newBlock %p %" PRIu32 "\n", block,
               BlockMeta_GetBlockIndex(blockMetaStart, block));
        fflush(stdout);
#endif
        if (block == NULL) {
            return false;
        }
        blockStart = BlockMeta_GetBlockStart(blockMetaStart,
                                             allocator->heapStart, block);

        allocator->cursor = blockStart;
        allocator->limit = Block_GetBlockEnd(blockStart);
        BlockMeta_SetFirstFreeLine(block, LAST_HOLE);
    }

    allocator->block = block;
    allocator->blockStart = blockStart;

    return true;
}

INLINE
word_t *Allocator_lazySweep(Heap *heap, uint32_t size) {
    word_t *object = NULL;
    Stats_DefineOrNothing(stats, heap->stats);
    Stats_RecordTime(stats, start_ns);
    // mark as active
    heap->lazySweep.lastActivity = BlockRange_Pack(1, heap->sweep.cursor);
    while (object == NULL && heap->sweep.cursor < heap->sweep.limit) {
        Sweeper_Sweep(heap, heap->stats, &heap->lazySweep.cursorDone,
                      LAZY_SWEEP_MIN_BATCH);
        object = Allocator_tryAlloc(&allocator, size);
    }
    // mark as inactive
    heap->lazySweep.lastActivity = BlockRange_Pack(0, heap->sweep.cursor);
    while (object == NULL && !Sweeper_IsSweepDone(heap)) {
        object = Allocator_tryAlloc(&allocator, size);
        if (object == NULL) {
            sched_yield();
        }
    }
    Stats_RecordTime(stats, end_ns);
    Stats_RecordEvent(stats, event_sweep, start_ns, end_ns);
    return object;
}

NOINLINE word_t *Allocator_allocSlow(Heap *heap, uint32_t size) {
    word_t *object = Allocator_tryAlloc(&allocator, size);

    if (object != NULL) {
    done:
        assert(Heap_IsWordInHeap(heap, object));
        assert(object != NULL);
        memset(object, 0, size);
        ObjectMeta *objectMeta = Bytemap_Get(allocator.bytemap, object);
#ifdef DEBUG_ASSERT
        ObjectMeta_AssertIsValidAllocation(objectMeta, size);
#endif
        ObjectMeta_SetAllocated(objectMeta);
        return object;
    }

    if (!Sweeper_IsSweepDone(heap)) {
        object = Allocator_lazySweep(heap, size);

        if (object != NULL)
            goto done;
    }

    Heap_Collect(heap);
    object = Allocator_tryAlloc(&allocator, size);

    if (object != NULL)
        goto done;

    if (!Sweeper_IsSweepDone(heap)) {
        object = Allocator_lazySweep(heap, size);

        if (object != NULL)
            goto done;
    }

    // A small object can always fit in a single free block
    // because it is no larger than 8K while the block is 32K.
    Heap_Grow(heap, 1);
    object = Allocator_tryAlloc(&allocator, size);

    goto done;
}

INLINE word_t *Allocator_Alloc(Heap *heap, uint32_t size) {
    assert(size % ALLOCATION_ALIGNMENT == 0);
    assert(size < MIN_BLOCK_SIZE);

    word_t *start = allocator.cursor;
    word_t *end = (word_t *)((uint8_t *)start + size);

    // Checks if the end of the block overlaps with the limit
    if (end > allocator.limit) {
        return Allocator_allocSlow(heap, size);
    }

    allocator.cursor = end;

    memset(start, 0, size);

    word_t *object = start;
    ObjectMeta *objectMeta = Bytemap_Get(allocator.bytemap, object);
#ifdef DEBUG_ASSERT
    ObjectMeta_AssertIsValidAllocation(objectMeta, size);
#endif
    ObjectMeta_SetAllocated(objectMeta);

    __builtin_prefetch(object + 36, 0, 3);

    assert(Heap_IsWordInHeap(heap, object));
    return object;
}

uint32_t Allocator_Sweep(Allocator *allocator, BlockMeta *blockMeta,
                         word_t *blockStart, LineMeta *lineMetas,
                         SweepResult *result) {

    // If the block is not marked, it means that it's completely free
    assert(blockMeta->debugFlag == dbg_must_sweep);
    if (!BlockMeta_IsMarked(blockMeta)) {
        memset(blockMeta, 0, sizeof(BlockMeta));
        // does not unmark in LineMetas because those are ignored by the
        // allocator
        ObjectMeta_ClearBlockAt(Bytemap_Get(allocator->bytemap, blockStart));
#ifdef DEBUG_ASSERT
        blockMeta->debugFlag = dbg_free;
#endif
        return 1;
    } else {
        // If the block is marked, we need to recycle line by line
        assert(BlockMeta_IsMarked(blockMeta));
        BlockMeta_Unmark(blockMeta);
        Bytemap *bytemap = allocator->bytemap;

        // start at line zero, keep separate pointers into all affected data
        // structures
        int lineIndex = 0;
        LineMeta *lineMeta = lineMetas;
        word_t *lineStart = blockStart;
        ObjectMeta *bytemapCursor = Bytemap_Get(bytemap, lineStart);

        FreeLineMeta *lastRecyclable = NULL;
        while (lineIndex < LINE_COUNT) {
            // If the line is marked, we need to unmark all objects in the line
            if (Line_IsMarked(lineMeta)) {
                // Unmark line
                Line_Unmark(lineMeta);
                ObjectMeta_SweepLineAt(bytemapCursor);

                // next line
                lineIndex++;
                lineMeta++;
                lineStart += WORDS_IN_LINE;
                bytemapCursor = Bytemap_NextLine(bytemapCursor);
            } else {
                // If the line is not marked, we need to merge all continuous
                // unmarked lines.

                // If it's the first free line, update the block header to point
                // to it.
                if (lastRecyclable == NULL) {
                    BlockMeta_SetFirstFreeLine(blockMeta, lineIndex);
                } else {
                    // Update the last recyclable line to point to the current
                    // one
                    lastRecyclable->next = lineIndex;
                }
                ObjectMeta_ClearLineAt(bytemapCursor);
                lastRecyclable = (FreeLineMeta *)lineStart;

                // next line
                lineIndex++;
                lineMeta++;
                lineStart += WORDS_IN_LINE;
                bytemapCursor = Bytemap_NextLine(bytemapCursor);

                uint8_t size = 1;
                while (lineIndex < LINE_COUNT && !Line_IsMarked(lineMeta)) {
                    ObjectMeta_ClearLineAt(bytemapCursor);
                    size++;

                    // next line
                    lineIndex++;
                    lineMeta++;
                    lineStart += WORDS_IN_LINE;
                    bytemapCursor = Bytemap_NextLine(bytemapCursor);
                }
                lastRecyclable->size = size;
            }
        }
        // If there is no recyclable line, the block is unavailable
        if (lastRecyclable != NULL) {
            lastRecyclable->next = LAST_HOLE;

            assert(BlockMeta_FirstFreeLine(blockMeta) >= 0);
            assert(BlockMeta_FirstFreeLine(blockMeta) < LINE_COUNT);
            // allocator->recycledBlockCount++;
            atomic_fetch_add_explicit(&allocator->recycledBlockCount, 1,
                                      memory_order_relaxed);

#ifdef DEBUG_ASSERT
            blockMeta->debugFlag = dbg_partial_free;
#endif
            // the allocator thread must see the sweeping changes in recycled
            // blocks
            atomic_thread_fence(memory_order_release);
            LocalBlockList_Push(&result->recycledBlocks,
                                allocator->blockMetaStart, blockMeta);
#ifdef DEBUG_PRINT
            printf(
                "Allocator_Sweep %p %" PRIu32 " => RECYCLED\n", blockMeta,
                BlockMeta_GetBlockIndex(allocator->blockMetaStart, blockMeta));
            fflush(stdout);
#endif
        } else {
#ifdef DEBUG_ASSERT
            atomic_thread_fence(memory_order_release);
            blockMeta->debugFlag = dbg_not_free;
#endif
        }
        return 0;
    }
}