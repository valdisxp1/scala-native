#include "../Object.h"
#include "GreyPacket.h"
#include "../Log.h"

void GreyPacket_Init(GreyPacket *packet) {
    packet->size = 0;
    packet->next = GREYLIST_LAST;
}

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
    list->head = (word_t) NULL;
}

void GreyList_Push(GreyList *list, GreyPacket *packet) {
    assert(packet->next == GREYLIST_LAST);
    GreyPacket *head = (GreyPacket *) list->head;
    do {
        // head will be replaced with actual value if
        // atomic_compare_exchange_strong fails
        if (head == NULL) {
            packet->next = GREYLIST_LAST;
        } else {
            packet->next = head;
        }
    } while (!atomic_compare_exchange_strong(&list->head, (word_t *)&head, (word_t)packet));
    list->size += 1;
}

void GreyList_PushAll(GreyList *list, GreyPacket *first, uint_fast32_t size) {
    GreyPacket *last = first + (size - 1);
    GreyPacket *head = (GreyPacket *) list->head;
    do {
        // head will be replaced with actual value if
        // atomic_compare_exchange_strong fails
        if (head == NULL) {
            last->next = GREYLIST_LAST;
        } else {
            last->next = head;
        }
    } while (!atomic_compare_exchange_strong(&list->head, (word_t *)&head, (word_t)first));
    list->size += size;
}

GreyPacket *GreyList_next(GreyPacket *packet) {
    void *next = packet->next;
    if (next == GREYLIST_LAST) {
        return NULL;
    } else if (next == NULL) {
        return packet + 1;
    } else {
        return (GreyPacket*) next;
    }
}

GreyPacket *GreyList_Pop(GreyList *list) {
    GreyPacket *head = (GreyPacket*) list->head;
    // not allowing clang to pull this before the NULL check
    // avoiding a SEGFAULT
    volatile word_t newValue;
    do {
        // head will be replaced with actual value if
        // atomic_compare_exchange_strong fails
        if (head == NULL) {
            return NULL;
        }
        newValue = (word_t) GreyList_next(head);
    } while(!atomic_compare_exchange_strong(&list->head, (word_t *)&head, newValue));
    list->size -= 1;
#ifdef DEBUG_ASSERT
    head->next = GREYLIST_LAST;
#endif
    return head;
}