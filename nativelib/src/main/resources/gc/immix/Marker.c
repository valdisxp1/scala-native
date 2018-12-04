#include <stdio.h>
#include <setjmp.h>
#include "Marker.h"
#include "Object.h"
#include "Log.h"
#include "State.h"
#include "headers/ObjectHeader.h"
#include "datastructures/GreyPacket.h"

extern word_t *__modules;
extern int __modules_size;
extern word_t **__stack_bottom;

#define LAST_FIELD_OFFSET -1

static inline GreyPacket *Marker_takeEmptyPacket(Heap *heap) {
    GreyPacket *packet = GreyList_Pop(&heap->mark.empty);
    if (packet != NULL) {
        // Another thread setting size = 0 might not arrived, just write it now.
        // Avoiding a memfence.
        packet->size = 0;
    }
    // TODO handle out of empty packets
    assert(packet != NULL);
    return packet;
}

static inline GreyPacket *Marker_takeFullPacket(Heap *heap) {
    GreyPacket *packet = GreyList_Pop(&heap->mark.full);
    assert(packet == NULL || packet->size > 0);
    return packet;
}

static inline void Marker_giveEmptyPacket(Heap *heap, GreyPacket *packet) {
    assert(packet->size == 0);
    // no memfence needed see Marker_takeEmptyPacket
    GreyList_Push(&heap->mark.empty, packet);
}

static inline void Marker_giveFullPacket(Heap *heap, GreyPacket *packet) {
    assert(packet->size > 0);
    // make all the contents visible to other threads
    atomic_thread_fence(memory_order_seq_cst);
    GreyList_Push(&heap->mark.full, packet);
}

void Marker_markObject(Heap *heap, GreyPacket **outHolder, Bytemap *bytemap,
                       Object *object, ObjectMeta *objectMeta) {
    assert(ObjectMeta_IsAllocated(objectMeta) || ObjectMeta_IsMarked(objectMeta));

    assert(Object_Size(object) != 0);
    Object_Mark(heap, object, objectMeta);

    GreyPacket *out = *outHolder;
    if (!GreyPacket_Push(out, object)) {
        Marker_giveFullPacket(heap, out);
        *outHolder = out = Marker_takeEmptyPacket(heap);
        GreyPacket_Push(out, object);
    }
}

void Marker_markConservative(Heap *heap, GreyPacket **outHolder, word_t *address) {
    assert(Heap_IsWordInHeap(heap, address));
    Object *object = Object_GetUnmarkedObject(heap, address);
    Bytemap *bytemap = heap->bytemap;
    if (object != NULL) {
        ObjectMeta *objectMeta = Bytemap_Get(bytemap, (word_t *)object);
        assert(ObjectMeta_IsAllocated(objectMeta));
        if (ObjectMeta_IsAllocated(objectMeta)) {
            Marker_markObject(heap, outHolder, bytemap, object, objectMeta);
        }
    }
}

void Marker_markPacket(Heap *heap, GreyPacket* in, GreyPacket **outHolder) {
    Bytemap *bytemap = heap->bytemap;
    if (*outHolder == NULL) {
        GreyPacket *fresh = Marker_takeEmptyPacket(heap);
        assert(fresh != NULL);
        *outHolder = fresh;
    }
    while (!GreyPacket_IsEmpty(in)) {
        Object *object = GreyPacket_Pop(in);

        if (Object_IsArray(object)) {
            if (object->rtti->rt.id == __object_array_id) {
                ArrayHeader *arrayHeader = (ArrayHeader *)object;
                size_t length = arrayHeader->length;
                word_t **fields = (word_t **)(arrayHeader + 1);
                for (int i = 0; i < length; i++) {
                    word_t *field = fields[i];
                    if (Heap_IsWordInHeap(heap, field)) {
                        ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                        if (ObjectMeta_IsAllocated(fieldMeta)) {
                            Marker_markObject(heap, outHolder, bytemap,
                                              (Object *)field, fieldMeta);
                        }
                    }
                }
            }
            // non-object arrays do not contain pointers
        } else {
            int64_t *ptr_map = object->rtti->refMapStruct;
            int i = 0;
            while (ptr_map[i] != LAST_FIELD_OFFSET) {
                word_t *field = object->fields[ptr_map[i]];
                if (Heap_IsWordInHeap(heap, field)) {
                    ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                    if (ObjectMeta_IsAllocated(fieldMeta)) {
                        Marker_markObject(heap, outHolder, bytemap, (Object *)field,
                                          fieldMeta);
                    }
                }
                ++i;
            }
        }
    }
}

void Marker_Mark(Heap *heap) {
    GreyPacket* in = Marker_takeFullPacket(heap);
    GreyPacket *out = NULL;
    while (in != NULL) {
        Marker_markPacket(heap, in, &out);
        GreyPacket *next = Marker_takeFullPacket(heap);
        if (next == NULL && !GreyPacket_IsEmpty(out)) {
            GreyPacket *tmp = out;
            out = in;
            in = tmp;
        } else {
            Marker_giveEmptyPacket(heap, in);
            in = next;
        }
    }
    assert(in == NULL);
    if (out != NULL) {
        if (out->size > 0) {
            Marker_giveFullPacket(heap, out);
        } else {
            Marker_giveEmptyPacket(heap, out);
        }
    }
}

void Marker_markProgramStack(Heap *heap, GreyPacket **outHolder) {
    // Dumps registers into 'regs' which is on stack
    jmp_buf regs;
    setjmp(regs);
    word_t *dummy;

    word_t **current = &dummy;
    word_t **stackBottom = __stack_bottom;

    while (current <= stackBottom) {

        word_t *stackObject = *current;
        if (Heap_IsWordInHeap(heap, stackObject)) {
            Marker_markConservative(heap, outHolder, stackObject);
        }
        current += 1;
    }
}

void Marker_markModules(Heap *heap, GreyPacket **outHolder) {
    word_t **modules = &__modules;
    int nb_modules = __modules_size;
    Bytemap *bytemap = heap->bytemap;
    for (int i = 0; i < nb_modules; i++) {
        Object *object = (Object *)modules[i];
        if (Heap_IsWordInHeap(heap, (word_t *)object)) {
            // is within heap
            ObjectMeta *objectMeta = Bytemap_Get(bytemap, (word_t *)object);
            if (ObjectMeta_IsAllocated(objectMeta)) {
                Marker_markObject(heap, outHolder, bytemap, object, objectMeta);
            }
        }
    }
}

void Marker_MarkRoots(Heap *heap) {
    GreyPacket *out = Marker_takeEmptyPacket(heap);
    Marker_markProgramStack(heap, &out);
    Marker_markModules(heap, &out);
    Marker_giveFullPacket(heap, out);
}

bool Marker_IsMarkDone(Heap *heap) {
    return heap->mark.empty.size == heap->mark.total;
}
