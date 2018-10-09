#ifndef IMMIX_BYTEMAP_H
#define IMMIX_BYTEMAP_H

#include <stddef.h>
#include "../GCTypes.h"
#include "../Constants.h"

typedef struct {
    word_t *firstAddress;
    size_t size;
    ubyte_t *end;
    ubyte_t data[0];
} Bytemap;

typedef enum {
    bm_free = 0x0,
    bm_placeholder = 0x1,
    bm_allocated = 0x2,
    bm_marked = 0x3,
} Flag;

void Bytemap_Init(Bytemap *bytemap, word_t *firstAddress, size_t size);
int Bytemap_IsFree(Bytemap *bytemap, word_t* address);
int Bytemap_IsAllocated(Bytemap *bytemap, word_t* address);
int Bytemap_IsPlaceholder(Bytemap *bytemap, word_t* address);
int Bytemap_IsMarked(Bytemap *bytemap, word_t* address);

void Bytemap_SetFree(Bytemap *bytemap, word_t* address);
void Bytemap_SetPlaceholder(Bytemap *bytemap, word_t* address);
void Bytemap_SetAllocated(Bytemap *bytemap, word_t* address);
void Bytemap_SetMarked(Bytemap *bytemap, word_t* address);

void Bytemap_SetAreaFree(Bytemap *bytemap, word_t* start, size_t words);

#endif // IMMIX_BYTEMAP_H