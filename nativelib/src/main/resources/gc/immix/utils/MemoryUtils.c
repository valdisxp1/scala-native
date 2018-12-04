#include "MemoryUtils.h"
#include <sys/mman.h>
#include "../Log.h"
#include "../Constants.h"

// Allow read and write
#define HEAP_MEM_PROT (PROT_READ | PROT_WRITE)
// Map private anonymous memory, and prevent from reserving swap
#define HEAP_MEM_FLAGS (MAP_NORESERVE | MAP_PRIVATE | MAP_ANONYMOUS)
// Map anonymous memory (not a file)
#define HEAP_MEM_FD -1
#define HEAP_MEM_FD_OFFSET 0

/**
 * Maps `MAX_SIZE` of memory and returns the first address aligned on
 * `alignment` mask
 */
word_t *Memory_MapAndAlign(size_t memoryLimit, size_t alignmentSize) {
    assert(alignmentSize % WORD_SIZE == 0);
    word_t *start = mmap(NULL, memoryLimit, HEAP_MEM_PROT, HEAP_MEM_FLAGS,
                             HEAP_MEM_FD, HEAP_MEM_FD_OFFSET);

    size_t alignmentMask = ~(alignmentSize - 1);
    // Area start not aligned on
    if (((word_t)start & alignmentMask) != (word_t)start) {
        word_t *previousBlock = (word_t *)((word_t)start & alignmentMask);
        start = previousBlock + alignmentSize / WORD_SIZE;
    }
    return start;
}