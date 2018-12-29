#ifndef IMMIX_GREYPACKET_H
#define IMMIX_GREYPACKET_H

#include <stdatomic.h>
#include <stdbool.h>
#include "../Constants.h"
#include "../GCTypes.h"

typedef Object *Stack_Type;

typedef struct {
    void *next;
    word_t size;
    Stack_Type items[GREY_PACKET_ITEMS];
} GreyPacket;

#define GREYLIST_LAST ((void *)1)

typedef struct {
    atomic_uint_fast32_t size;
    atomic_uintptr_t head;
} GreyList;

void GreyPacket_Init(GreyPacket *packet);
bool GreyPacket_Push(GreyPacket *packet, Stack_Type value);
Stack_Type GreyPacket_Pop(GreyPacket *packet);
bool GreyPacket_IsEmpty(GreyPacket *packet);
bool GreyPacket_IsFull(GreyPacket *packet);

void GreyList_Init(GreyList *list);
void GreyList_Push(GreyList *list, GreyPacket *packet);
void GreyList_PushAll(GreyList *list, GreyPacket *first, uint_fast32_t size);
GreyPacket *GreyList_Pop(GreyList *list);


#endif // IMMIX_GREYPACKET_H