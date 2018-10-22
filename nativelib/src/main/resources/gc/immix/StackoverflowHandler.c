#include <stdio.h>
#include "StackoverflowHandler.h"
#include "State.h"
#include "Block.h"
#include "Object.h"
#include "Marker.h"

extern int __object_array_id;

#define LAST_FIELD_OFFSET -1

void StackOverflowHandler_smallHeapOverflowHeapScan(Heap *heap, Stack *stack);
bool StackOverflowHandler_largeHeapOverflowHeapScan(Heap *heap, Stack *stack);
bool StackOverflowHandler_overflowBlockScan(Heap *heap, Stack *stack);

void StackOverflowHandler_CheckForOverflow() {
    if (overflow) {
        // Set overflow address to the first word of the heap
        currentOverflowBlock = (BlockMeta *) heap.blockMetaStart;
        currentOverflowAddress = heap.heapStart;
        overflow = false;
        Stack_DoubleSize(&stack);

#ifdef PRINT_STACK_OVERFLOW
        printf("Stack grew to %zu bytes\n",
               stack.nb_words * sizeof(Stack_Type));
        fflush(stdout);
#endif

        word_t *heapEnd = heap.heapEnd;
        // Continue while we don't hit the end of the heap.
        while (currentOverflowAddress < heapEnd) {
            StackOverflowHandler_smallHeapOverflowHeapScan(&heap,&stack);

            // At every iteration when a object is found, trace it
            Marker_Mark(&heap, &stack);
        }
    }
}

void StackOverflowHandler_smallHeapOverflowHeapScan(Heap *heap, Stack *stack) {
    word_t *blockMetaEnd = heap->blockMetaEnd;

    while ((word_t *)currentOverflowBlock < blockMetaEnd) {
        assert(!BlockMeta_IsSuperblockMiddle(currentOverflowBlock));
        int size;
        if (BlockMeta_IsSuperblockStart(currentOverflowBlock)) {
            size = BlockMeta_SuperblockSize(currentOverflowBlock);
            assert(size > 0);
            if (StackOverflowHandler_largeHeapOverflowHeapScan(heap, stack)) {
                return;
            }
        } else {
            size = 1;
            if (StackOverflowHandler_overflowBlockScan(heap, stack)) {
                return;
            }
        }
        currentOverflowBlock += size;
        currentOverflowAddress = BlockMeta_GetBlockStart(heap->blockMetaStart, heap->heapStart, currentOverflowBlock);
    }
    return;
}

bool StackOverflowHandler_overflowMark(Heap *heap, Stack *stack, Object *object,
                                       ObjectMeta *objectMeta) {

    Bytemap *bytemap = heap->bytemap;
    if (ObjectMeta_IsMarked(objectMeta)) {
        if (Object_IsArray(object)) {
            if (object->rtti->rt.id == __object_array_id) {
                ArrayHeader *arrayHeader = (ArrayHeader *)object;
                size_t length = arrayHeader->length;
                word_t **fields = (word_t **)(arrayHeader + 1);
                for (int i = 0; i < length; i++) {
                    word_t *field = fields[i];
                    Object *fieldObject = (Object *)field;
                    if (Heap_IsWordInHeap(heap, field)) {
                        ObjectMeta *metaF = Bytemap_Get(bytemap, field);
                        if (ObjectMeta_IsAllocated(metaF)) {
                            Stack_Push(stack, object);
                            return true;
                        }
                    }
                }
            }
            // non-object arrays do not contain pointers
        } else {
            int64_t *ptr_map = object->rtti->refMapStruct;
            int i = 0;
            while (ptr_map[i] != LAST_FIELD_OFFSET) {
                word_t *field = object->fields[ptr_map[i]];
                Object *fieldObject = (Object *)field;
                if (Heap_IsWordInHeap(heap, field)) {
                    ObjectMeta *metaF = Bytemap_Get(bytemap, field);
                    if (ObjectMeta_IsAllocated(metaF)) {
                        Stack_Push(stack, object);
                        return true;
                    }
                }
                ++i;
            }
        }
    }
    return false;
}

/**
 * Scans through the large heap to find marked blocks with unmarked children.
 * Updates `currentOverflowAddress` while doing so.
 */
bool StackOverflowHandler_largeHeapOverflowHeapScan(Heap *heap, Stack *stack) {
    word_t *blockStart = BlockMeta_GetBlockStart(heap->blockMetaStart, heap->heapStart, currentOverflowBlock);
    word_t *end = blockStart + WORDS_IN_BLOCK * BlockMeta_SuperblockSize(currentOverflowBlock);
    while (currentOverflowAddress < end) {
        Object *object = (Object *)currentOverflowAddress;
        ObjectMeta *cursorMeta =
            Bytemap_Get(heap->bytemap, currentOverflowAddress);
        assert(!ObjectMeta_IsFree(cursorMeta));
        if (StackOverflowHandler_overflowMark(heap, stack, object, cursorMeta)) {
            return true;
        }
        currentOverflowAddress = (word_t *)Object_NextLargeObject(object);
    }
    return false;
}

bool overflowScanLine(Heap *heap, Stack *stack, BlockMeta *block,
                      word_t *blockStart, int lineIndex) {
    Bytemap *bytemap = heap->bytemap;

    word_t *lineStart = Block_GetLineAddress(blockStart, lineIndex);
    if (Line_IsMarked(Heap_LineMetaForWord(heap, lineStart))) {
        word_t *lineEnd = lineStart + WORDS_IN_LINE;
        word_t *cursor = lineStart;
        ObjectMeta *cursorMeta = Bytemap_Get(bytemap, cursor);
        while (cursor < lineEnd) {
            Object *object = (Object *)cursor;
            if (ObjectMeta_IsMarked(cursorMeta) &&
                StackOverflowHandler_overflowMark(heap, stack, object,
                                                  cursorMeta)) {
                return true;
            }

            cursor += ALLOCATION_ALIGNMENT_WORDS;
            cursorMeta += 1;
        }
    }
    return false;
}

/**
 *
 * This method is used in case of overflow during the marking phase.
 * It sweeps through the block starting at `currentOverflowAddress` until it
 * finds a marked block with unmarked children.
 * It updates the value of `currentOverflowAddress` while sweeping through the
 * block
 * Once an object is found it adds it to the stack and returns `true`. If no
 * object is found it returns `false`.
 *
 */
bool StackOverflowHandler_overflowBlockScan(Heap *heap, Stack *stack) {
    if (!BlockMeta_IsMarked(currentOverflowBlock)) {
        return false;
    }

    word_t *blockStart =
        BlockMeta_GetBlockStart(heap->blockMetaStart, heap->heapStart, currentOverflowBlock);
    int lineIndex =
        Block_GetLineIndexFromWord(blockStart, currentOverflowAddress);
    while (lineIndex < LINE_COUNT) {
        if (overflowScanLine(heap, stack, currentOverflowBlock, blockStart, lineIndex)) {
            return true;
        }

        lineIndex++;
    }
    return false;
}
