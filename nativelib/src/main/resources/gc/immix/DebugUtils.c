#ifdef DEBUG_ASSERT
#include "DebugUtils.h"

BlockMeta* Debug_BlockMetas() {
    return (BlockMeta *) heap.blockMetaStart;
}

uint32_t Debug_BlockIdForWord(word_t *p) {
    return Block_GetBlockIndexForWord(heap.heapStart, p);
}
uint32_t Debug_BlockRangeFirst(BlockRangeVal blockRange){
    BlockRange_First(blockRange);
}
uint32_t Debug_BlockRangeLimit(BlockRangeVal blockRange){
    BlockRange_Limit(blockRange);
}
#endif