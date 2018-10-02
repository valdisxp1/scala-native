#ifndef IMMIX_BLOCK_H
#define IMMIX_BLOCK_H

#include "headers/BlockHeader.h"
#include "Heap.h"
#include "Line.h"

#define LAST_HOLE -1

extern Heap heap;

static inline uint32_t BlockHeader_GetBlockIndex(BlockHeader *blockHeader) {
    return (uint32_t)((word_t *)blockHeader - heap.blockHeaderStart) / WORDS_IN_BLOCK_METADATA;
}

static inline word_t *BlockHeader_GetBlockStart(BlockHeader *blockHeader) {
    uint32_t index = BlockHeader_GetBlockIndex(blockHeader);
    return heap.heapStart + (WORDS_IN_BLOCK * index);
}

static inline word_t * Block_GetBlockStartForWord(word_t *word) {
    return (word_t *)((word_t)word & BLOCK_SIZE_IN_BYTES_INVERSE_MASK);
}

static inline uint32_t Block_GetBlockIndexForWord(word_t *word) {
    word_t *blockStart = Block_GetBlockStartForWord(word);
    return (uint32_t) ((blockStart - heap.heapStart) / WORDS_IN_BLOCK);
}

static inline BlockHeader *Block_GetBlockHeader(word_t *word) {
    uint32_t index = Block_GetBlockIndexForWord(word);
    return (BlockHeader *)(heap.blockHeaderStart + (index * WORDS_IN_BLOCK_METADATA));
}

static inline word_t *Block_GetLineAddress(word_t *blockStart, int lineIndex) {
    assert(lineIndex < LINE_COUNT);
    return blockStart + (WORDS_IN_LINE * lineIndex);
}

static inline word_t *Block_GetBlockEnd(word_t *blockStart) {
    return blockStart + (WORDS_IN_LINE * LINE_COUNT);
}

static inline uint32_t Block_GetLineIndexFromWord(word_t *blockStart,
                                                  word_t *word) {
    return (uint32_t)((word_t)word - (word_t)blockStart) >> LINE_SIZE_BITS;
}

static inline word_t *Block_GetLineWord(word_t *blockStart, int lineIndex,
                                        int wordIndex) {
    assert(wordIndex < WORDS_IN_LINE);
    return &Block_GetLineAddress(blockStart, lineIndex)[wordIndex];
}

static inline FreeLineHeader *Block_GetFreeLineHeader(word_t *blockStart,
                                                      int lineIndex) {
    return (FreeLineHeader *)Block_GetLineAddress(blockStart, lineIndex);
}

void Block_Recycle(Allocator *, BlockHeader *);
void Block_Print(BlockHeader *block);
#endif // IMMIX_BLOCK_H
