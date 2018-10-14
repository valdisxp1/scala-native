#include <stdio.h>
#include "StackoverflowHandler.h"
#include "State.h"
#include "Block.h"
#include "Object.h"
#include "Marker.h"

extern int __object_array_id;

#define LAST_FIELD_OFFSET -1

bool StackOverflowHandler_smallHeapOverflowHeapScan();
void StackOverflowHandler_largeHeapOverflowHeapScan();
bool StackOverflowHandler_overflowBlockScan(BlockHeader *block);

void StackOverflowHandler_CheckForOverflow() {
    if (overflow) {
        // Set overflow address to the first word of the heap
        currentOverflowAddress = heap.heapStart;
        overflow = false;
        Stack_DoubleSize();

#ifdef PRINT_STACK_OVERFLOW
        printf("Stack grew to %zu bytes\n",
               stack.nb_words * sizeof(Stack_Type));
        fflush(stdout);
#endif

        word_t *largeHeapEnd = heap.largeHeapEnd;
        // Continue while we don' hit the end of the large heap.
        while (currentOverflowAddress != largeHeapEnd) {

            // If the current overflow address is in the small heap, scan the
            // small heap.
            if (Heap_IsWordInSmallHeap(currentOverflowAddress)) {
                // If no object was found in the small heap, move on to large
                // heap
                if (!StackOverflowHandler_smallHeapOverflowHeapScan()) {
                    currentOverflowAddress = heap.largeHeapStart;
                }
            } else {
                StackOverflowHandler_largeHeapOverflowHeapScan();
            }

            // At every iteration when a object is found, trace it
            Marker_Mark();
        }
    }
}

bool StackOverflowHandler_smallHeapOverflowHeapScan() {
    assert(Heap_IsWordInSmallHeap(currentOverflowAddress));
    BlockHeader *currentBlock = Block_GetBlockHeader(heap.blockHeaderStart, heap.heapStart, currentOverflowAddress);
    word_t *blockHeaderEnd = heap.blockHeaderEnd;

    while ((word_t *)currentBlock < blockHeaderEnd) {
        if (StackOverflowHandler_overflowBlockScan(currentBlock)) {
            return true;
        }
        currentBlock = (BlockHeader *)((word_t *)currentBlock + WORDS_IN_BLOCK_METADATA);
        currentOverflowAddress = BlockHeader_GetBlockStart(heap.blockHeaderStart, heap.heapStart, currentBlock);
    }
    return false;
}

bool StackOverflowHandler_overflowMark(Object *object) {
    Bytemap *bytemap = Heap_BytemapForWord((word_t*) object);

    if (Bytemap_IsMarked(bytemap, (word_t*) object)) {
        if (Object_IsArray(object)) {
            if (object->rtti->rt.id == __object_array_id) {
                ArrayHeader *arrayHeader = (ArrayHeader *) object;
                size_t length = arrayHeader -> length;
                word_t **fields = (word_t **) (arrayHeader + 1);
                for (int i = 0; i < length; i++) {
                    word_t *field = fields[i];
                    Object *fieldObject = (Object *) field;
                    Bytemap *bytemapF = Heap_BytemapForWord((word_t*) fieldObject);
                    if (bytemapF != NULL &&
                        Bytemap_IsAllocated(bytemapF, (word_t*) fieldObject)) {
                        Stack_Push(object);
                        return true;
                    }
                }
            }
            // non-object arrays do not contain pointers
        } else {
            int64_t *ptr_map = object->rtti->refMapStruct;
            int i = 0;
            while (ptr_map[i] != LAST_FIELD_OFFSET) {
                word_t *field = object->fields[ptr_map[i]];
                Object *fieldObject = (Object *) field;
                Bytemap *bytemapF = Heap_BytemapForWord((word_t*) fieldObject);
                if (bytemapF != NULL &&
                    Bytemap_IsAllocated(bytemapF, (word_t*) fieldObject)) {
                    Stack_Push(object);
                    return true;
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
void StackOverflowHandler_largeHeapOverflowHeapScan() {
    assert(Heap_IsWordInLargeHeap(currentOverflowAddress));
    void *heapEnd = heap.largeHeapEnd;

    while (currentOverflowAddress != heapEnd) {
        Object *object = (Object *)currentOverflowAddress;
        if (StackOverflowHandler_overflowMark(object)) {
            return;
        }
        currentOverflowAddress = (word_t *)Object_NextLargeObject(object);
    }
}

bool StackOverflowHandler_overflowScanLine(BlockHeader *block, word_t* blockStart,
                      int lineIndex) {
    Bytemap *bytemap = heap.smallBytemap;

    word_t *lineStart = Block_GetLineAddress(blockStart, lineIndex);
    if (Line_IsMarked(Heap_LineHeaderForWord(lineStart))) {
        word_t *lineEnd = lineStart + WORDS_IN_LINE;
        for (word_t *cursor = lineStart; cursor < lineEnd; cursor++) {
            Object *object = (Object *) cursor;
            if (Bytemap_IsMarked(bytemap, cursor) && StackOverflowHandler_overflowMark(object)) {
                return true;
            }
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
bool StackOverflowHandler_overflowBlockScan(BlockHeader *block) {
    if (!BlockHeader_IsMarked(block)) {
        return false;
    }

    word_t *blockStart = BlockHeader_GetBlockStart(heap.blockHeaderStart, heap.heapStart, block);
    int lineIndex = Block_GetLineIndexFromWord(blockStart, currentOverflowAddress);
    while (lineIndex < LINE_COUNT) {
        if (StackOverflowHandler_overflowScanLine(block, blockStart, lineIndex)) {
            return true;
        }

        lineIndex++;
    }
    return false;
}
