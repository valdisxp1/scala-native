#include "../Object.h"
#include "GreyPacket.h"
#include "../Log.h"

bool GreyPacket_Push(GreyPacket *packet, Stack_Type value) {
    assert(value != NULL);
    if (packet->size >= GREY_PACKET_ITEMS) {
        return false;
    } else {
        packet->items[packet->size++] = value;
        return true;
    }
}
Stack_Type GreyPacket_Pop(GreyPacket *packet) {
    assert(packet->size > 0);
    return packet->items[--packet->size];
}

bool GreyPacket_IsEmpty(GreyPacket *packet) {
   return packet->size == 0;
}

bool GreyPacket_IsFull(GreyPacket *packet) {
   return packet->size == GREY_PACKET_ITEMS;
}

void GreyList_Init(GreyList *list) {
    list->head = BlockRange_Pack(GREYLIST_LAST, 0);
}

void GreyList_Push(GreyList *list, word_t *greyPacketsStart, GreyPacket *packet) {
    uint32_t packetIdx = GreyPacket_IndexOf(greyPacketsStart, packet);
    BlockRangeVal newHead = BlockRange_Pack(packetIdx, packet->timesPoped);
    BlockRangeVal head = list->head;
    do {
        // head will be replaced with actual value if
        // atomic_compare_exchange_strong fails
        uint32_t nextIdx = BlockRange_First(head);
        if (nextIdx == GREYLIST_LAST) {
            packet->next = BlockRange_Pack(GREYLIST_LAST, 0);
        } else {
            packet->next = head;
        }
    } while (!atomic_compare_exchange_strong(&list->head, &head, newHead));
    list->size += 1;
}

void GreyList_PushAll(GreyList *list, word_t *greyPacketsStart, GreyPacket *first, uint_fast32_t size) {
    uint32_t packetIdx = GreyPacket_IndexOf(greyPacketsStart, first);
    BlockRangeVal newHead = BlockRange_Pack(packetIdx, first->timesPoped);
    GreyPacket *last = first + (size - 1);
    BlockRangeVal head = list->head;
    do {
        // head will be replaced with actual value if
        // atomic_compare_exchange_strong fails
        uint32_t nextIdx = BlockRange_First(head);
        if (nextIdx == GREYLIST_LAST) {
            last->next = BlockRange_Pack(GREYLIST_LAST, 0);
        } else {
            last->next = head;
        }
    } while (!atomic_compare_exchange_strong(&list->head, &head, newHead));
    list->size += size;
}

GreyPacket *GreyList_Pop(GreyList *list, word_t *greyPacketsStart) {
    BlockRangeVal head = list->head;
    BlockRangeVal nextValue;
    uint32_t headIdx;
    GreyPacket *res;
    do {
        // head will be replaced with actual value if
        // atomic_compare_exchange_strong fails
        headIdx = BlockRange_First(head);
        assert(headIdx != GREYLIST_NEXT);
        if (headIdx == GREYLIST_LAST) {
            return NULL;
        }
        res = GreyPacket_FromIndex(greyPacketsStart, headIdx);
        BlockRangeVal next = res->next;
        nextValue = (next != 0L)? next : BlockRange_Pack(headIdx + 1, 0);
    } while(!atomic_compare_exchange_strong(&list->head, &head, nextValue));
    list->size -= 1;
    res->timesPoped += 1;
    return res;
}