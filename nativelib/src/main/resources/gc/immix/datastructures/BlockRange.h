#ifndef IMMIX_BLOCKRANGE_H
#define IMMIX_BLOCKRANGE_H

#include "../GCTypes.h"
#include "../Constants.h"
#include <stdbool.h>
#include <stdatomic.h>

typedef atomic_uint_fast64_t BlockRange;
typedef uint_fast64_t BlockRangeVal;

#define NO_BLOCK_INDEX (~((uint32_t) 0))

static inline BlockRangeVal BlockRange_Pack(uint32_t first, uint32_t limit) {
    return ((uint64_t) limit  << 32) | (uint64_t) first;
}

static inline uint32_t BlockRange_First(BlockRangeVal blockRange) {
    return (uint32_t) (blockRange);
}

static inline uint32_t BlockRange_Limit(BlockRangeVal blockRange) {
    return (uint32_t) (blockRange >> 32);
}

static inline void BlockRange_Clear(BlockRange *blockRange) {
    *blockRange = 0;
}

static inline bool BlockRange_IsEmpty(BlockRangeVal blockRange) {
    return BlockRange_First(blockRange) >= BlockRange_Limit(blockRange);
}

static inline bool BlockRange_AppendLast(BlockRange *blockRange, uint32_t first, uint32_t count) {
    BlockRangeVal old = *blockRange;
    do {
        // old will be replaced with actual value if atomic_compare_exchange_strong fails
        BlockRangeVal newValue;
        if (BlockRange_IsEmpty(old)) {
            newValue = BlockRange_Pack(first, first + count);
        } else if (BlockRange_Limit(old) == first) {
            newValue = BlockRange_Pack(BlockRange_First(old), first + count);
        } else {
            return false;
        }
    } while(!atomic_compare_exchange_strong(blockRange, (BlockRangeVal *)&old, newValue));

    return true;
}

static inline uint32_t BlockRange_Size(BlockRange blockRange) {
    assert(!BlockRange_IsEmpty(blockRange));
    return BlockRange_Limit(blockRange) - BlockRange_First(blockRange);
}

static inline BlockRangeVal BlockRange_Replace(BlockRange *blockRange, uint32_t first, uint32_t count) {
    BlockRangeVal newRange = BlockRange_Pack(first, first + count);
    return atomic_exchange(blockRange, newRange);
}

static inline uint32_t BlockRange_PollFirst(BlockRange *blockRange, uint32_t count) {
    BlockRangeVal old = *blockRange;
    uint32_t first;
    do {
        // old will be replaced with actual value if atomic_compare_exchange_strong fails
        first = BlockRange_First(old);
        uint32_t limit = BlockRange_Limit(old);
        if (BlockRange_IsEmpty(old) || BlockRange_Size(old) < count){
            return NO_BLOCK_INDEX;
        }
        BlockRangeVal newValue = BlockRange_Pack(first + count, limit);
    } while(!atomic_compare_exchange_strong(blockRange, (BlockRangeVal *)&old, newValue));
    return first;
}

#endif // IMMIX_BLOCKRANGE_H