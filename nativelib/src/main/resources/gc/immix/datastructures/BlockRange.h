#ifndef IMMIX_BLOCKRANGE_H
#define IMMIX_BLOCKRANGE_H

#include "../GCTypes.h"
#include "../Constants.h"
#include <stdbool.h>

typedef uint64_t BlockRange;

#define NO_BLOCK_INDEX (~((uint32_t) 0))

static inline BlockRange BlockRange_Pack(uint32_t first, uint32_t limit) {
    return ((uint64_t) limit  << 32) | (uint64_t) first;
}

static inline uint32_t BlockRange_First(BlockRange blockRange) {
    return (uint32_t) (blockRange);
}

static inline uint32_t BlockRange_Limit(BlockRange blockRange) {
    return (uint32_t) (blockRange >> 32);
}

static inline void BlockRange_Clear(BlockRange *blockRange) {
    *blockRange = 0;
}

static inline bool BlockRange_IsEmpty(BlockRange blockRange) {
    return BlockRange_First(blockRange) >= BlockRange_Limit(blockRange);
}

static inline bool BlockRange_AppendLast(BlockRange *blockRange, uint32_t first, uint32_t count) {
    BlockRange old = *blockRange;
    if (BlockRange_IsEmpty(old)) {
        *blockRange = BlockRange_Pack(first, first + count);
        return true;
    } else if (BlockRange_Limit(old) == first) {
        *blockRange = BlockRange_Pack(BlockRange_First(old), first + count);
        return true;
    } else {
        return false;
    }
}

static inline uint32_t BlockRange_Size(BlockRange blockRange) {
    assert(!BlockRange_IsEmpty(blockRange));
    return BlockRange_Limit(blockRange) - BlockRange_First(blockRange);
}

static inline BlockRange BlockRange_Replace(BlockRange *blockRange, uint32_t first, uint32_t count) {
    BlockRange old = *blockRange;
    BlockRange newRange = BlockRange_Pack(first, first + count);
    *blockRange = newRange;
    return old;
}

static inline uint32_t BlockRange_PollFirst(BlockRange *blockRange, uint32_t count) {
    BlockRange old = *blockRange;
    uint32_t first = BlockRange_First(old);
    uint32_t limit = BlockRange_Limit(old);
    if (!BlockRange_IsEmpty(old) && BlockRange_Size(old) >= count) {
        *blockRange = BlockRange_Pack(first + count, limit);
        return first;
    } else {
        return NO_BLOCK_INDEX;
    }
}

#endif // IMMIX_BLOCKRANGE_H