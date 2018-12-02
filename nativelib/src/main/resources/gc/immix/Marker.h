#ifndef IMMIX_MARKER_H
#define IMMIX_MARKER_H

#include "Heap.h"
#include "datastructures/GreyPacket.h"

void Marker_MarkRoots(Heap *heap);
void Marker_Mark(Heap *heap);
void Marker_MarkPacket(Heap *heap, GreyPacket* in, GreyPacket **outHolder);

#endif // IMMIX_MARKER_H
