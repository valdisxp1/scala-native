#ifndef IMMIX_MARKER_H
#define IMMIX_MARKER_H

#include "Heap.h"
#include "datastructures/Stack.h"

void Marker_MarkRoots(Stack *stack);
void Marker_Mark(Stack *stack);

#endif // IMMIX_MARKER_H
