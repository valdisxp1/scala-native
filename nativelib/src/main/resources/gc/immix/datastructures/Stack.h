#ifndef IMMIX_STACK_H
#define IMMIX_STACK_H

#include "../GCTypes.h"
#include "../headers/ObjectHeader.h"

#define INITIAL_STACK_SIZE (256 * 1024)

typedef Object *Stack_Type;

typedef struct {
    Stack_Type *bottom;
    size_t nb_words;
    int current;
} Stack;

extern Stack stack;

void Stack_Init(size_t size);

bool Stack_Push(Stack_Type word);

Stack_Type Stack_Pop();

bool Stack_IsEmpty();

void Stack_DoubleSize();

#endif // IMMIX_STACK_H
