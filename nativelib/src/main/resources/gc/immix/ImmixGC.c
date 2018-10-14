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

void scalanative_afterexit() {
    Heap_AfterExit();
}

NOINLINE void scalanative_init() {
    Heap_Init(INITIAL_SMALL_HEAP_SIZE, INITIAL_LARGE_HEAP_SIZE);
    Stack_Init(INITIAL_STACK_SIZE);
    atexit(scalanative_afterexit);
}

INLINE void *scalanative_alloc(void *info, size_t size) {
    size = MathUtils_RoundToNextMultiple(size, WORD_SIZE);

    void **alloc = (void **)Heap_Alloc(size);
    *alloc = info;
    return (void *)alloc;
}

INLINE void *scalanative_alloc_small(void *info, size_t size) {
    size = MathUtils_RoundToNextMultiple(size, WORD_SIZE);

    void **alloc = (void **)Heap_AllocSmall(size);
    *alloc = info;
    return (void *)alloc;
}

INLINE void *scalanative_alloc_large(void *info, size_t size) {
    size = MathUtils_RoundToNextMultiple(size, WORD_SIZE);

    void **alloc = (void **)Heap_AllocLarge(size);
    *alloc = info;
    return (void *)alloc;
}

INLINE void *scalanative_alloc_atomic(void *info, size_t size) {
    return scalanative_alloc(info, size);
}

INLINE void scalanative_collect() { Heap_Collect(); }
