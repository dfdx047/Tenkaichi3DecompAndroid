#include "common.h"
#include "sys/heap.h"
#include "sys/queue.h"

extern void *memset(void *dst, s32 value, u32 size);

/* Takes a node from the free list, else the next unused one of the pool; NULL when full. */
QueueNode *Queue_AllocNode(Queue *queue) {
    QueueNode *node;

    if (queue->freeList != NULL) {
        node = queue->freeList;
        queue->freeList = node->next;
    } else if (queue->used < queue->capacity - 1) {
        node = &queue->nodes[queue->used++];
    } else {
        return NULL;
    }
    return node;
}

/* Last queued node, or NULL when empty. */
QueueNode *Queue_GetTail(Queue *queue) {
    QueueNode *tail = NULL;
    QueueNode *node = queue->head;

    while (node != NULL) {
        tail = node;
        node = node->next;
    }
    return tail;
}

/* Clears the queue and allocates its node pool. */
void Queue_Init(Queue *queue, s32 capacity) {
    memset(queue, 0, sizeof(Queue));
    queue->capacity = capacity;
    queue->nodes = Heap_Alloc(capacity * sizeof(QueueNode), 0x20, 0, HEAP_ANY);
    memset(queue->nodes, 0, queue->capacity * sizeof(QueueNode));
}

/* Frees the node pool. */
void Queue_Destroy(Queue *queue) {
    if (queue->nodes != NULL) {
        Heap_Free(queue->nodes);
        queue->nodes = NULL;
    }
}

/* Drops every queued value. */
void Queue_Clear(Queue *queue) {
    queue->freeList = NULL;
    queue->head = NULL;
    queue->used = 0;
}

/* Appends a value; returns it, or NULL when the queue is full. */
void *Queue_Push(Queue *queue, void *value) {
    QueueNode *node = Queue_AllocNode(queue);
    QueueNode *tail;

    if (node == NULL) {
        return NULL;
    }
    if (value != NULL) {
        node->value = value;
    }
    tail = Queue_GetTail(queue);
    if (tail == NULL) {
        queue->head = node;
    } else {
        tail->next = node;
    }
    node->next = NULL;
    return node->value;
}

/* Oldest value without removing it, or NULL when empty. */
void *Queue_Peek(Queue *queue) {
    QueueNode *node = queue->head;
    void *value = NULL;

    if (node != NULL) {
        value = node->value;
    }
    return value;
}

/* Removes and returns the oldest value, or NULL when empty. The node goes back on the free list. */
void *Queue_Pop(Queue *queue) {
    QueueNode *node = queue->head;
    void *value = NULL;

    if (node != NULL) {
        if (node->next != NULL) {
            queue->head = node->next;
            node->next = queue->freeList;
            queue->freeList = node;
        } else {
            queue->head = NULL;
            node->next = queue->freeList;
            queue->freeList = node;
        }
        value = node->value;
    }
    return value;
}

/* Number of queued values. */
s32 Queue_Count(Queue *queue) {
    s32 count = 0;
    QueueNode *node = queue->head;
    QueueNode *next;

    if (node != NULL) {
        do {
            next = node->next;
            count++;
            node = next;
        } while (next != NULL);
    }
    return count;
}
