#include <stdio.h>
#include <setjmp.h>
#include "Marker.h"
#include "Object.h"
#include "Log.h"
#include "State.h"
#include "datastructures/Stack.h"
#include "headers/ObjectHeader.h"
#include "Block.h"
#include "StackoverflowHandler.h"

extern word_t *__modules;
extern int __modules_size;
extern word_t **__stack_bottom;

#define LAST_FIELD_OFFSET -1

void Marker_markObject(Heap *heap, Stack *stack, Bytemap *bytemap,
                       Object *object, ObjectMeta *objectMeta, bool collectingOld) {
    assert(Object_Size(object) != 0);
    Object_Mark(heap, object, objectMeta, collectingOld);
    if (!overflow) {
        overflow = Stack_Push(stack, object);
    }
}

bool Marker_mark(Heap *heap, Stack *stack, Bytemap *bytemap, Object *object, ObjectMeta *objectMeta, bool collectingOld) {
    if (!collectingOld && ObjectMeta_IsAllocated(objectMeta)) {
        Marker_markObject(heap, stack, bytemap, object, objectMeta, collectingOld);
        return true;
    } else if (collectingOld && ObjectMeta_IsMarked(objectMeta)) {
        Marker_markObject(heap, stack, bytemap, object, objectMeta, collectingOld);
        return true;
    }
    return false;
}

void Marker_markConservative(Heap *heap, Stack *stack, word_t *address, bool collectingOld) {
    assert(Heap_IsWordInHeap(heap, address));
    Object *object = NULL;
    bool largeObject = false;
    Bytemap *bytemap;
    if (Heap_IsWordInSmallHeap(heap, address)) {
        if (!collectingOld) {
            object = Object_GetYoungObject(heap, address);
        } else {
            object = Object_GetOldObject(heap, address);
        }
        bytemap = heap->smallBytemap;
#ifdef DEBUG_PRINT
        if (object == NULL) {
            printf("Not found: %p\n", address);
        }
#endif
    } else {
        bytemap = heap->largeBytemap;
        if (!collectingOld) {
            object = Object_GetLargeYoungObject(bytemap, address);
        } else {
            object = Object_GetLargeOldObject(bytemap, address);
        }
    }
    if (object != NULL) {
        ObjectMeta *objectMeta = Bytemap_Get(bytemap, (word_t *)object);
        Marker_mark(heap, stack, bytemap, object, objectMeta, collectingOld);
    }
}

void Marker_Mark(Heap *heap, Stack *stack, bool collectingOld) {
    while (!Stack_IsEmpty(stack)) {
        Object *object = Stack_Pop(stack);
        ObjectMeta *objectMeta = Bytemap_Get(Heap_BytemapForWord(heap, (word_t *)object), (word_t *)object);
        bool willBeOld = true;

        if (Heap_IsWordInSmallHeap(heap, (word_t *)object)) {
            BlockMeta *bMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, (word_t *)object);
            willBeOld = BlockMeta_IsOld(bMeta) || (BlockMeta_GetAge(bMeta) == MAX_AGE_YOUNG_BLOCK - 1);
        }

        bool hasPointerToYoung = false;
        bool hasPointerToOld = false;

        if (Object_IsArray(object)) {
            if (object->rtti->rt.id == __object_array_id) {

                ArrayHeader *arrayHeader = (ArrayHeader *)object;
                size_t length = arrayHeader->length;
                word_t **fields = (word_t **)(arrayHeader + 1);
                for (int i = 0; i < length; i++) {
                    word_t *field = fields[i];
                    Bytemap *bytemap = Heap_BytemapForWord(heap, field);
                    if (bytemap != NULL) {
                        // is within heap
                        ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                        Marker_mark(heap, stack, bytemap, (Object *)field, fieldMeta, collectingOld);

                        if (Heap_IsWordInLargeHeap(heap, field)) {
                            hasPointerToOld = true;
                        } else {
                            BlockMeta *blockMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, field);
                            if (BlockMeta_IsOld(blockMeta) || BlockMeta_GetAge(blockMeta) == (MAX_AGE_YOUNG_BLOCK - 1)) {
                                hasPointerToOld = true;
                            } else {
                                hasPointerToYoung = true;
                            }
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
                Bytemap *bytemap = Heap_BytemapForWord(heap, field);
                if (bytemap != NULL) {
                    // is within heap
                    ObjectMeta *fieldMeta = Bytemap_Get(bytemap, field);
                    Marker_mark(heap, stack, bytemap, (Object *)field, fieldMeta, collectingOld);

                    if (Heap_IsWordInLargeHeap(heap, field)) {
                        hasPointerToOld = true;
                    } else {
                        BlockMeta *blockMeta = Block_GetBlockMeta(heap->blockMetaStart, heap->heapStart, field);
                        if (BlockMeta_IsOld(blockMeta) || BlockMeta_GetAge(blockMeta) == (MAX_AGE_YOUNG_BLOCK - 1)) {
                            hasPointerToOld = true;
                        } else {
                            hasPointerToYoung = true;
                        }
                    }
                }
                ++i;
            }
        }

        if (willBeOld && hasPointerToYoung && !ObjectMeta_IsRemembered(objectMeta)) {
            ObjectMeta_SetRemembered(objectMeta);
            Stack_Push(allocator.rememberedObjects, object);
        } else if (!willBeOld && hasPointerToOld) {
            Stack_Push(allocator.rememberedYoungObjects, object);
        }
    }
}

void Marker_markProgramStack(Heap *heap, Stack *stack, bool collectingOld) {
    // Dumps registers into 'regs' which is on stack
    jmp_buf regs;
    setjmp(regs);
    word_t *dummy;

    word_t **current = &dummy;
    word_t **stackBottom = __stack_bottom;

    while (current <= stackBottom) {

        word_t *stackObject = *current;
        if (Heap_IsWordInHeap(heap, stackObject)) {
            Marker_markConservative(heap, stack, stackObject, collectingOld);
        }
        current += 1;
    }
}

void Marker_markModules(Heap *heap, Stack *stack, bool collectingOld) {
    word_t **modules = &__modules;
    int nb_modules = __modules_size;

    for (int i = 0; i < nb_modules; i++) {
        Object *object = (Object *)modules[i];
        Bytemap *bytemap = Heap_BytemapForWord(heap, (word_t *)object);
        if (bytemap != NULL) {
            // is within heap
            ObjectMeta *objectMeta = Bytemap_Get(bytemap, (word_t *)object);
            Marker_mark(heap, stack, bytemap, object, objectMeta, collectingOld);
        }
    }
}

void Marker_markRemembered(Heap *heap, Stack *stack) {
    Stack *roots = allocator.rememberedObjects;
    while (!Stack_IsEmpty(roots)) {
        Object *object = (Object *)Stack_Pop(roots);
        Bytemap *bytemap = Heap_BytemapForWord(heap, (word_t *)object);
        if (bytemap != NULL) {
            ObjectMeta *objectMeta = Bytemap_Get(bytemap, (word_t *)object);
            if (ObjectMeta_IsMarked(objectMeta)) {
                ObjectMeta_SetUnremembered(objectMeta);
                Stack_Push(stack, object);
            }
        }
    }
}

void Marker_markYoungRemembered(Heap *heap,Stack *stack) {
    Stack *roots = allocator.rememberedYoungObjects;
    while(!Stack_IsEmpty(roots)) {
        Object *object = (Object *)Stack_Pop(roots);
        Bytemap *bytemap = Heap_BytemapForWord(heap, (word_t *)object);
        if (bytemap != NULL) {
            ObjectMeta *objectMeta = Bytemap_Get(bytemap, (word_t *)object);
            if (ObjectMeta_IsAllocated(objectMeta)) {
                Stack_Push(stack, object);
            }
        }
    }
}

void Marker_MarkRoots(Heap *heap, Stack *stack, bool collectingOld) {

    if (!collectingOld) {
        Marker_markRemembered(heap, stack);
    } else {
        Marker_markYoungRemembered(heap, stack);
    }

    Marker_markProgramStack(heap, stack, collectingOld);

    Marker_markModules(heap, stack, collectingOld);

    Marker_Mark(heap, stack, collectingOld);
}
