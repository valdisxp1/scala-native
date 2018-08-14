#ifndef IMMIX_OBJECT_H
#define IMMIX_OBJECT_H

#include "headers/ObjectHeader.h"
#include "datastructures/Bytemap.h"
#include "LargeAllocator.h"
#include "Heap.h"

Object *Object_NextLargeObject(Object *object);
word_t *Object_LastWord(Object *object);
Object *Object_GetYoungObject(Heap *heap, word_t *address);
Object *Object_GetOldObject(Heap *heap, word_t *address);
Object *Object_GetLargeYoungObject(Bytemap *bytemap, word_t *address);
Object *Object_GetLargeOldObject(Bytemap *bytemap, word_t *address);
void Object_Mark(Heap *heap, Object *object, ObjectMeta *objectMeta, bool collectingOld);
size_t Object_ChunkSize(Object *object);
bool Object_HasPointerToYoungObject(Heap *heap, Object *object, bool largeObject);
bool Object_HasPointerToOldObject(Heap *heap, Object *object, bool largeObject);

#endif // IMMIX_OBJECT_H
