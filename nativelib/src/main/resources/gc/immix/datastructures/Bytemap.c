#include "Bytemap.h"
#include "../Constants.h"
#include "../Log.h"
#include "../utils/MathUtils.h"
#include <stdio.h>

void Bytemap_Init(Bytemap *bytemap, word_t *firstAddress, size_t size) {
    bytemap->firstAddress = firstAddress;
    bytemap->size = size / WORD_SIZE;
    bytemap->end = &bytemap->data[bytemap->size];
    assert(Bytemap_index(bytemap, (word_t *)((ubyte_t *)(firstAddress) + size) - 1) < bytemap->size);
}