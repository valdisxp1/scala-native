#ifdef DEBUG_ASSERT

#include "State.h"
#include "Heap.h"
#include "metadata/BlockMeta.h"
#include "datastructures/BlockRange.h"
#include "Log.h"

__attribute__((used))
BlockMeta* Debug_BlockMetas();

__attribute__((used))
uint32_t Debug_BlockIdForWord(word_t *p);

__attribute__((used))
uint32_t Debug_BlockRangeFirst(BlockRangeVal blockRange);

__attribute__((used))
uint32_t Debug_BlockRangeLimit(BlockRangeVal blockRange);

#endif