#ifndef SYS_QUEUE_H
#define SYS_QUEUE_H

#include "types.h"

/* One queued value. Free nodes are chained through next as well. */
typedef struct QueueNode {
    /* 0x00 */ struct QueueNode *next;
    /* 0x04 */ void *value;
} QueueNode;

/* Fixed-capacity FIFO of pointers, backed by a node pool from the heap. */
typedef struct Queue {
    /* 0x00 */ s32 capacity;
    /* 0x04 */ QueueNode *nodes;    /* pool of capacity nodes */
    /* 0x08 */ QueueNode *freeList; /* nodes returned by Queue_Pop */
    /* 0x0C */ QueueNode *head;     /* oldest queued node */
    /* 0x10 */ s32 used;            /* pool nodes handed out so far */
} Queue;

QueueNode *Queue_AllocNode(Queue *queue);
QueueNode *Queue_GetTail(Queue *queue);
void Queue_Init(Queue *queue, s32 capacity);
void Queue_Destroy(Queue *queue);
void Queue_Clear(Queue *queue);
void *Queue_Push(Queue *queue, void *value);
void *Queue_Peek(Queue *queue);
void *Queue_Pop(Queue *queue);
s32 Queue_Count(Queue *queue);

#endif
