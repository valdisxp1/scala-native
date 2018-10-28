#ifndef IMMIX_BLOCKRANGE_H
#define IMMIX_BLOCKRANGE_H

#include "../metadata/BlockMeta.h"
#include "../GCTypes.h"
#include "../Constants.h"
#include <stdbool.h>

typedef struct {
    BlockMeta *first;
    BlockMeta *limit;
} BlockRange;

static inline void BlockRange_Clear(BlockRange *blockRange) {
    blockRange->first = NULL;
    blockRange->limit = NULL;
}

static inline bool BlockRange_IsEmpty(BlockRange *blockRange) {
    return blockRange->first == NULL || blockRange->first >= blockRange->limit;
}

static inline bool BlockRange_AppendLast(BlockRange *blockRange, BlockMeta *first, uint32_t count) {
    if (BlockRange_IsEmpty(blockRange)) {
        blockRange->first = first;
        blockRange->limit = first + count;
        return true;
    } else if (blockRange->limit == first) {
        blockRange->limit = first + count;
        return true;
    } else {
        return false;
    }
}

static inline uint32_t BlockRange_Size(BlockRange *blockRange) {
    return (uint32_t) (blockRange->limit - blockRange->first);
}

static inline BlockMeta *BlockRange_Replace(BlockRange *blockRange, BlockMeta *first, uint32_t count) {
    BlockMeta *old = blockRange->first;
    blockRange->first = first;
    blockRange->limit = first + count;
    return old;
}

static inline BlockMeta *BlockRange_PollFirst(BlockRange *blockRange, uint32_t count) {
    if (BlockRange_Size(blockRange) >= count) {
        BlockMeta *old = blockRange->first;
        blockRange->first = old + count;
        return old;
    } else {
        return NULL;
    }
}

#endif // IMMIX_BLOCKRANGE_H