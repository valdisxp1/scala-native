#ifndef IMMIX_MEMUTILS_H
#define IMMIX_MEMUTILS_H

#include <memory.h>
#include "../GCTypes.h"

word_t *Memory_MapAndAlign(size_t memoryLimit, size_t alignmentSize);

#endif //IMMIX_MEMUTILS_H