#ifndef SYS_HEAP_H
#define SYS_HEAP_H

#include "types.h"

#define HEAP_MAGIC 0x53484254 /* 'TBHS' */
#define HEAP_COUNT 2
#define HEAP_ANY 2 /* heap index meaning "search every heap" */

/* Header in front of every block. The last word of a block repeats its total size. */
typedef struct HeapBlock {
    /* 0x00 */ u32 magic;
    /* 0x04 */ s32 used;
    /* 0x08 */ s32 fromTail; /* allocated from the tail end of the heap */
    /* 0x0C */ u32 align;
    /* 0x10 */ s32 size;     /* whole block: header, data and trailer */
    /* 0x14 */ u32 data;     /* address handed to the caller; 0 while free */
    /* 0x18 */ s32 dataSize; /* bytes requested; for a free block, size - 0x20 */
    /* 0x1C */ s32 unk1C;
} HeapBlock;

void Heap_Init(void);
void *Heap_Alloc(s32 size, u32 align, s32 fromTail, s32 heap);
void Heap_Free(void *ptr);

#endif
