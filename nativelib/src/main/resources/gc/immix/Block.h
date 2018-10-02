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

static inline uint32_t Block_GetBlockIndexForWord(word_t *word) {
    word_t *firstWord = (word_t *)((word_t)word & BLOCK_SIZE_IN_BYTES_INVERSE_MASK);
    return (uint32_t) ((firstWord - heap.heapStart) / WORDS_IN_BLOCK);
}

static inline BlockHeader *Block_GetBlockHeader(word_t *word) {
    uint32_t index = Block_GetBlockIndexForWord(word);
    return (BlockHeader *)(heap.blockHeaderStart + (index * WORDS_IN_BLOCK_METADATA));
}

static inline word_t *BlockHeader_GetLineAddress(BlockHeader *blockHeader,
                                           int lineIndex) {
    assert(lineIndex < LINE_COUNT);
    return BlockHeader_GetBlockStart(blockHeader) + (WORDS_IN_LINE * lineIndex);
}

static inline word_t *BlockHeader_GetBlockEnd(BlockHeader *blockHeader) {
    return BlockHeader_GetBlockStart(blockHeader) + (WORDS_IN_LINE * LINE_COUNT);
}

static inline uint32_t Block_GetLineIndexFromWord(BlockHeader *blockHeader,
                                                  word_t *word) {
    word_t *blockStart = BlockHeader_GetBlockStart(blockHeader);
    return (uint32_t)((word_t)word - (word_t)blockStart) >> LINE_SIZE_BITS;
}

static inline word_t *BlockHeader_GetLineWord(BlockHeader *blockHeader, int lineIndex,
                                        int wordIndex) {
    assert(wordIndex < WORDS_IN_LINE);
    return &BlockHeader_GetLineAddress(blockHeader, lineIndex)[wordIndex];
}

static inline FreeLineHeader *BlockHeader_GetFreeLineHeader(BlockHeader *blockHeader,
                                                      int lineIndex) {
    return (FreeLineHeader *)BlockHeader_GetLineAddress(blockHeader, lineIndex);
}

void Block_Recycle(Allocator *, BlockHeader *);
void Block_Print(BlockHeader *block);
#endif // IMMIX_BLOCK_H
