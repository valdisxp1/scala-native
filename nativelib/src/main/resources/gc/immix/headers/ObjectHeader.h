#ifndef IMMIX_OBJECTHEADER_H
#define IMMIX_OBJECTHEADER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "../GCTypes.h"
#include "../Constants.h"
#include "../Log.h"
#include "../utils/MathUtils.h"

extern int __object_array_id;
extern int __array_ids_min;
extern int __array_ids_max;

typedef struct {
    struct {
        int32_t id;
        int32_t tid;
        word_t *name;
        int8_t kind;
    } rt;
    int64_t size;
    struct {
        int32_t from;
        int32_t to;
    } range;
    struct {
        int32_t dyn_method_count;
        word_t *dyn_method_salt;
        word_t *dyn_method_keys;
        word_t *dyn_methods;
    } dynDispatchTable;
    int64_t *refMapStruct;
} Rtti;

typedef word_t *Field_t;

typedef struct {
    Rtti *rtti;
    Field_t fields[0];
} Object;

<<<<<<< HEAD
typedef struct {
    Rtti *rtti;
    int32_t length;
    int32_t stride;
} ArrayHeader;
=======
static inline bool Object_IsMarked(ObjectHeader *objectHeader) {
    return objectHeader->flag == object_marked;
}

static inline void Object_MarkObjectHeader(ObjectHeader *objectHeader) {
    objectHeader->flag = object_marked;
}

static inline void Object_SetAllocated(ObjectHeader *objectHeader) {
    objectHeader->flag = object_allocated;
}

static inline void Object_SetFree(ObjectHeader *objectHeader) {
    objectHeader->flag = object_free;
}

static inline bool Object_IsFree(ObjectHeader *objectHeader) {
    return objectHeader->flag == object_free;
}

static inline bool Object_IsAllocated(ObjectHeader *objectHeader) {
    return objectHeader->flag == object_allocated;
}

static inline void Object_SetRemembered(ObjectHeader *objectHeader) {
    objectHeader->remembered = 1;
}

static inline void Object_SetUnremembered(ObjectHeader *objectHeader) {
    objectHeader->remembered = 0;
}

static inline bool Object_IsRemembered(ObjectHeader *objectHeader) {
    return objectHeader->remembered == 1;
}

static inline bool Object_IsStandardObject(ObjectHeader *objectHeader) {
    return objectHeader->type == object_standard;
}
static inline bool Object_IsLargeObject(ObjectHeader *objectHeader) {
    return objectHeader->type == object_large;
}

static inline void Object_SetObjectType(ObjectHeader *objectHeader,
                                        ObjectType objectType) {
    objectHeader->type = objectType;
}

static inline size_t Object_Size(ObjectHeader *objectHeader) {
    uint32_t size = objectHeader->size;
    assert((Object_IsStandardObject(objectHeader) && size < LARGE_BLOCK_SIZE) ||
           !Object_IsStandardObject(objectHeader));
>>>>>>> generational gc: Aging-semispace

typedef struct Chunk Chunk;

struct Chunk {
    void *nothing;
    size_t size;
    Chunk *next;
};

static inline bool Object_IsArray(Object *object) {
    int32_t id = object->rtti->rt.id;
    return __array_ids_min <= id && id <= __array_ids_max;
}

static inline size_t Object_Size(Object *object) {
    if (Object_IsArray(object)) {
        ArrayHeader *arrayHeader = (ArrayHeader *)object;
        return MathUtils_RoundToNextMultiple(
            sizeof(ArrayHeader) +
                (size_t)arrayHeader->length * (size_t)arrayHeader->stride,
            ALLOCATION_ALIGNMENT);
    } else {
        return MathUtils_RoundToNextMultiple((size_t)object->rtti->size,
                                             ALLOCATION_ALIGNMENT);
    }
}

#endif // IMMIX_OBJECTHEADER_H
