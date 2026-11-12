#ifndef SYS_SPU_HEAP_H
#define SYS_SPU_HEAP_H

#include "types.h"

/*
 * Sound-RAM (SPU2) address allocator, src/sys/spu_heap.c = 0x11ECB8..0x11F190.
 * It only hands out address ranges; nothing here touches SPU memory itself.
 */

#define SPU_HEAP_BASE 0x5010     /* first SPU address managed */
#define SPU_HEAP_SIZE 0x1F0FD0   /* bytes managed: 0x5010..0x1F5FE0 */
#define SPU_HEAP_BLOCKS 17       /* descriptors: free and used ranges together */

/* SpuBlock.state */
enum {
    SPU_BLOCK_NONE = 0, /* descriptor not in use */
    SPU_BLOCK_FREE = 1, /* free address range */
    SPU_BLOCK_USED = 2  /* allocated address range */
};

/* One address range, 0xC bytes. The table is unordered; neighbours are found by address. */
typedef struct SpuBlock {
    /* 0x00 */ s32 state; /* SPU_BLOCK_* */
    /* 0x04 */ s32 addr;  /* SPU address of the first byte */
    /* 0x08 */ s32 size;  /* bytes, a multiple of 16 for used blocks */
} SpuBlock;

void SpuBlock_Clear(SpuBlock *block);
void SpuBlock_SetFree(SpuBlock *block, s32 addr, s32 size);
void SpuBlock_SetUsed(SpuBlock *block, s32 addr, s32 size);
SpuBlock *SpuHeap_FindBlock(s32 addr, s32 byEnd);
void SpuHeap_Init(void);
s32 SpuHeap_Alloc(s32 size);
void SpuHeap_Free(s32 addr);
s32 SpuHeap_GetUsedSize(void);
s32 SpuHeap_GetFreeSize(void);
s32 SpuHeap_GetMaxFree(void);
void SpuHeap_Walk(void);

#endif
