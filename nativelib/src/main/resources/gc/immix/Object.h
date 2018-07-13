#ifndef IMMIX_OBJECT_H
#define IMMIX_OBJECT_H

#include "headers/ObjectHeader.h"
#include "datastructures/Bytemap.h"
#include "LargeAllocator.h"
#include "Heap.h"

Object *Object_NextLargeObject(Object *object);
word_t *Object_LastWord(Object *object);
Object *Object_GetUnmarkedObject(Heap *heap, word_t *address);
Object *Object_GetLargeUnmarkedObject(Bytemap *bytemap, word_t *address);
void Object_Mark(Heap *heap, Object *object, ObjectMeta *objectMeta, bool collectingOld);
size_t Object_ChunkSize(Object *object);
Object *Object_GetObject(word_t *address);
Object *Object_GetYoungObject(word_t *address);
Object *Object_GetOldObject(word_t *address);
Object *Object_GetLargeObject(LargeAllocator *largeAllocator, word_t *address);
Object *Object_GetLargeYoungObject(LargeAllocator *largeAllocator, word_t *address);
Object *Object_GetLargeOldObject(LargeAllocator *largeAllocator, word_t *address);
void Object_Unmark(Object *objectHeader);

#endif // IMMIX_OBJECT_H
