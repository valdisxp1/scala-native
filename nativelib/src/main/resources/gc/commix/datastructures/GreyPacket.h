#ifndef IMMIX_GREYPACKET_H
#define IMMIX_GREYPACKET_H

#include <stdatomic.h>
#include <inttypes.h>
#include <stdbool.h>
#include "../Constants.h"
#include "../GCTypes.h"
#include "BlockRange.h"
#include "../Log.h"

typedef Object *Stack_Type;

typedef struct {
    BlockRange next;// _First index _Limit timesPoped
    atomic_uint_least32_t timesPoped; // used to avoid ABA problems when popping
    uint32_t size;
    Stack_Type items[GREY_PACKET_ITEMS];
} GreyPacket;

#define GREYLIST_NEXT ((uint32_t)0)
#define GREYLIST_LAST ((uint32_t)1)

typedef struct {
    atomic_uint_fast32_t size;
    BlockRange head; // _First index _Limit timesPoped
} GreyList;

bool GreyPacket_Push(GreyPacket *packet, Stack_Type value);
Stack_Type GreyPacket_Pop(GreyPacket *packet);
bool GreyPacket_IsEmpty(GreyPacket *packet);
bool GreyPacket_IsFull(GreyPacket *packet);

void GreyList_Init(GreyList *list);
void GreyList_Push(GreyList *list, word_t *greyPacketsStart, GreyPacket *packet);
void GreyList_PushAll(GreyList *list, word_t *greyPacketsStart, GreyPacket *first, uint_fast32_t size);
GreyPacket *GreyList_Pop(GreyList *list, word_t *greyPacketsStart);

static inline uint32_t GreyPacket_IndexOf(word_t *greyPacketsStart, GreyPacket *packet) {
    assert(packet != NULL);
    assert((void*)packet >= (void*)greyPacketsStart);
    return (uint32_t)(packet - (GreyPacket *) greyPacketsStart) + 2;
}

static inline GreyPacket *GreyPacket_FromIndex(word_t *greyPacketsStart, uint32_t idx) {
    assert(idx >= 2);
    return (GreyPacket *) greyPacketsStart + (idx - 2);
}

#endif // IMMIX_GREYPACKET_H