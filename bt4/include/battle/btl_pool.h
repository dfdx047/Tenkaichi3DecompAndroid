#ifndef BATTLE_BTL_POOL_H
#define BATTLE_BTL_POOL_H

#include "types.h"

#define BTL_POOL_SLOT_COUNT 9
/* Bit in BtlPool.arenaMask saying that a slot is a bump arena. */
#define BTL_POOL_ARENA_BIT(slot) (1 << ((slot) + 1)) /* arenaMask is unsigned: a signed mask compiles to srav/andi */

/* One pool slot. base/cur/size are only set for slots that are bump arenas. */
typedef struct BtlPoolSlot {
    /* 0x00 */ u8 *base;  /* start of the arena */
    /* 0x04 */ u8 *cur;   /* next free byte of the arena */
    /* 0x08 */ s32 size;  /* arena capacity (never checked) */
    /* 0x0C */ s32 used;  /* bytes handed out, rounded up to 32 (also counted for heap slots) */
} BtlPoolSlot;

/* Pool manager state, Heap_Alloc'ed by BtlPool_Init. */
typedef struct BtlPool {
    /* 0x00 */ BtlPoolSlot slots[BTL_POOL_SLOT_COUNT];
    /* 0x90 */ u8 *arenaMem;  /* one heap block holding every arena back to back */
    /* 0x94 */ s32 current;   /* slot id used by callers: BtlPool_Alloc(BtlPool_GetCurrent(), n) */
    /* 0x98 */ u32 arenaMask; /* BTL_POOL_ARENA_BIT of every slot that is an arena */
} BtlPool; /* size 0x9C */

void BtlPool_Init(s32 arg);
void BtlPool_Term(void);
void *BtlPool_Alloc(s32 slot, s32 size);
void BtlPool_Free(s32 slot, void *ptr);
void BtlPool_Reset(s32 slot);
void BtlPool_SetCurrent(s32 slot);
s32 BtlPool_GetCurrent(void);
void BtlPool_SetInitArg(s32 arg);
s32 BtlPool_AlignUp32(s32 size);

#endif
