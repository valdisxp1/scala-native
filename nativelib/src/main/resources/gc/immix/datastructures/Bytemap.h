#ifndef IMMIX_BYTEMAP_H
#define IMMIX_BYTEMAP_H

#include <stddef.h>
#include "../GCTypes.h"
#include "../Constants.h"

typedef struct {
    word_t *firstAddress;
    uint_32_t size;
    uint_32_t *end;
    ubyte_t data[0];
} ByteMap;

typedef enum {
    bm_free = 0x0,
    bm_placeholder = 0x1,
    bm_allocated = 0x2,
} Flag;

void Bytemap_Init(Bytemap *bytemap, word_t *firstAddress, word_t *lastAddress);
ubyte_t Bytemap_Get(Bytemap *bytemap, word_t* address);
bool Bytemap_IsObject(Bytemap *bytemap, word_t* address);
void Bytemap_SetFree(Bytemap *bytemap, word_t* address);
void Bytemap_SetPlaceholder(Bytemap *bytemap, word_t* address);
void Bytemap_SetAllocated(Bytemap *bytemap, word_t* address);

#endif // IMMIX_BYTEMAP_H