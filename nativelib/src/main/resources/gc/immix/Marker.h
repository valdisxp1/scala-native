#ifndef IMMIX_MARKER_H
#define IMMIX_MARKER_H

#include "Heap.h"

void Marker_Init(Heap *heap);
void Marker_MarkRoots(Heap *heap);
void Marker_Mark(Heap *heap);
bool Marker_IsMarkDone(Heap *heap);
void Marker_GrowGreyArea(Heap *heap, int packetCount);

#endif // IMMIX_MARKER_H
