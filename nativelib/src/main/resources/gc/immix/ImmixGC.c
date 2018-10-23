#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "GCTypes.h"
#include "Heap.h"
#include "datastructures/Stack.h"
#include "Marker.h"
#include "Log.h"
#include "Object.h"
#include "State.h"
#include "utils/MathUtils.h"
#include "Constants.h"

void scalanative_collect();

NOINLINE void scalanative_init() {
    Heap_Init(&heap, INITIAL_SMALL_HEAP_SIZE, INITIAL_LARGE_HEAP_SIZE);
    Stack_Init(&stack, INITIAL_STACK_SIZE);
}

INLINE void *scalanative_alloc(void *info, size_t size) {
    size = MathUtils_RoundToNextMultiple(size, ALLOCATION_ALIGNMENT);

    void **alloc = (void **)Heap_Alloc(&heap, size);
    *alloc = info;
    return (void *)alloc;
}

INLINE void *scalanative_alloc_small(void *info, size_t size) {
    size = MathUtils_RoundToNextMultiple(size, ALLOCATION_ALIGNMENT);

    void **alloc = (void **)Heap_AllocSmall(&heap, size);
    *alloc = info;
    return (void *)alloc;
}

INLINE void *scalanative_alloc_large(void *info, size_t size) {
    size = MathUtils_RoundToNextMultiple(size, ALLOCATION_ALIGNMENT);

    void **alloc = (void **)Heap_AllocLarge(&heap, size);
    *alloc = info;
    return (void *)alloc;
}

INLINE void *scalanative_alloc_atomic(void *info, size_t size) {
    return scalanative_alloc(info, size);
}

INLINE void scalanative_collect() {
    Heap_Collect(&heap, &stack);
    Heap_CollectOld(&heap, &stack);
}

void scalanative_write_barrier(void *object) {
    Bytemap *bytemap = Heap_BytemapForWord(&heap, (word_t *)object);
    ObjectMeta *objectMeta = Bytemap_Get(bytemap, (word_t *)object);
    if (ObjectMeta_IsMarked(objectMeta) && !ObjectMeta_IsRemembered(objectMeta)) {
        ObjectMeta_SetRemembered(objectMeta);
        Stack_Push(allocator.rememberedObjects, object);
    }
}
