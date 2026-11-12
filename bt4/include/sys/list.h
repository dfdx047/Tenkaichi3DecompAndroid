#ifndef SYS_LIST_H
#define SYS_LIST_H

#include "types.h"

/* Link embedded at the start of every doubly linked list element. */
typedef struct ListNode {
    /* 0x00 */ struct ListNode *prev;
    /* 0x04 */ struct ListNode *next;
} ListNode;

/* Intrusive doubly linked list.
 * count sits in an anonymous union with a pointer: the original code never reorders count
 * accesses across head/tail/node stores, which ee-gcc only reproduces when count shares the
 * pointers' alias set. Use list->count as a plain s32. */
typedef struct List {
    /* 0x00 */ ListNode *head;
    /* 0x04 */ ListNode *tail;
    /* 0x08 */ union { s32 count; ListNode *countAlias; };
} List;

/* Link embedded at the start of every singly linked list element. */
typedef struct SListNode {
    /* 0x00 */ struct SListNode *next;
} SListNode;

/* Intrusive singly linked list. Same union as List, for the same reason. */
typedef struct SList {
    /* 0x00 */ SListNode *head;
    /* 0x04 */ SListNode *tail;
    /* 0x08 */ union { s32 count; SListNode *countAlias; };
} SList;

void List_Copy(List *dst, List *src);
void SList_Copy(SList *dst, SList *src);

void List_Init(List *list);
ListNode *List_GetHead(List *list);
ListNode *List_GetTail(List *list);
s32 List_GetCount(List *list);
void List_PushFront(List *list, ListNode *node);
void List_PushBack(List *list, ListNode *node);
void List_PrependList(List *list, List *other);
void List_AppendList(List *list, List *other);
s32 List_InsertBefore(List *list, ListNode *pos, ListNode *node);
s32 List_InsertAfter(List *list, ListNode *pos, ListNode *node);
ListNode *List_Remove(List *list, ListNode *node);
ListNode *List_PopFront(List *list);
ListNode *List_PopBack(List *list);
ListNode *List_GetPrev(ListNode *node);
ListNode *List_GetNext(ListNode *node);
s32 List_Contains(List *list, ListNode *node);

void SList_Init(SList *list);
SListNode *SList_GetHead(SList *list);
SListNode *SList_GetTail(SList *list);
s32 SList_GetCount(SList *list);
void SList_PushFront(SList *list, SListNode *node);
void SList_PushBack(SList *list, SListNode *node);
void SList_PrependList(SList *list, SList *other);
void SList_AppendList(SList *list, SList *other);
SListNode *SList_PopFront(SList *list);
SListNode *SList_GetNext(SListNode *node);
s32 SList_Contains(SList *list, SListNode *node);

#endif
