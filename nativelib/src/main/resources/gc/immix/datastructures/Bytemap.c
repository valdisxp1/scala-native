#include "Bytemap.h"
#include "../Constants.h"
#include "../Log.h"
#include "../utils/MathUtils.h"
#include <stdio.h>

void Bytemap_Init(Bytemap *bytemap, word_t *firstAddress, word_t *lastAddress) {
    bytemap->firstAddress = firstAddress;
    bytemap->size = (uint32_t)(lastAddress - firstAddress);
    bytemap->end = &bytemap->data[bytemap->size];
}

inline uint32_t Bytemap_index(Bytemap *bytemap, word_t* address){
    uint32_t index = (uint32_t)((address - bytemap->firstAddress) >> WORD_SIZE_BITS);
    assert(index >= 0);
    assert(index < bytemap -> size);
    return index;
}

ubyte_t Bytemap_Get(Bytemap *bytemap, word_t* address) {
    return bytemap->data[Bytemap_index(bytemap, address)];
}

int Bytemap_IsAllocated(Bytemap *bytemap, word_t* address) {
    return bytemap->data[Bytemap_index(bytemap, address)] == bm_allocated;
}

int Bytemap_IsFree(Bytemap *bytemap, word_t* address) {
    return bytemap->data[Bytemap_index(bytemap, address)] == bm_allocated;
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