#include "Bytemap.h"
#include "../Constants.h"
#include "../Log.h"
#include "../utils/MathUtils.h"
#include <stdio.h>

inline uint32_t Bytemap_index(Bytemap *bytemap, word_t* address) {
    uint32_t index = (uint32_t)(address - bytemap->firstAddress);
    assert(address >= bytemap->firstAddress);
    assert(index < bytemap -> size);
    return index;
}

void Bytemap_Init(Bytemap *bytemap, word_t *firstAddress, size_t size) {
    bytemap->firstAddress = firstAddress;
    bytemap->size = size / WORD_SIZE;
    bytemap->end = &bytemap->data[bytemap->size];
    assert(Bytemap_index(bytemap, (word_t *)((ubyte_t *)(firstAddress) + size) - 1) < bytemap->size);
}

ubyte_t Bytemap_Get(Bytemap *bytemap, word_t* address) {
    return bytemap->data[Bytemap_index(bytemap, address)];
}

int Bytemap_IsAllocated(Bytemap *bytemap, word_t* address) {
    return bytemap->data[Bytemap_index(bytemap, address)] == bm_allocated;
}

int Bytemap_IsFree(Bytemap *bytemap, word_t* address) {
    return bytemap->data[Bytemap_index(bytemap, address)] == bm_free;
}

void Bytemap_SetFree(Bytemap *bytemap, word_t* address) {
    bytemap->data[Bytemap_index(bytemap, address)] = bm_free;
}

void Bytemap_SetPlaceholder(Bytemap *bytemap, word_t* address) {
    bytemap->data[Bytemap_index(bytemap, address)] = bm_placeholder;
}

void Bytemap_SetAllocated(Bytemap *bytemap, word_t* address) {
    bytemap->data[Bytemap_index(bytemap, address)] = bm_allocated;
}