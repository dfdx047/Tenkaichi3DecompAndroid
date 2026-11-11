extern int TagBattle_Active(void);
#include "common.h"
#include "battle/btl_pool.h"
#include "sys/heap.h"
#include "battle/tag_team_native.h"
#include <stdio.h>
#include <stdlib.h>
extern BtlPool *gBtlPool;
static BtlPoolSlot sTagPools[16];
static s32 TagPool_Size(s32 size) { return BtlPool_AlignUp32(size * ((TagBattle_Count()+1)/2)); }
static BtlPoolSlot *TagPool_Slot(s32 slot) { if(slot<0||slot>=25){fprintf(stderr,"tag pool: invalid slot %d\n",slot);exit(3);}return slot<9 ? &gBtlPool->slots[slot] : &sTagPools[slot-9]; }

extern void *memset(void *dst, s32 c, u32 n);

extern BtlPool *gBtlPool;
/* Written by BtlPool_Init and BtlPool_SetInitArg, never read. */
extern s32 gBtlPoolInitArg;

/* The arena block is reached through a pointer to the field, not as a plain member: after a store to it
 * the compiler reloads gBtlPool, which a direct `gBtlPool->arenaMem = x` does not reproduce. */
#define ARENA_MEM (*&gBtlPool->arenaMem)

s32 BtlPool_CalcArenaSize(void);
void BtlPool_CarveArenas(void);
u8 *BtlPool_InitSlot(BtlPoolSlot *slot, s32 size, u8 *mem);

/* Allocates the pool manager and the memory behind every arena slot. */
void BtlPool_Init(s32 arg) {
    s32 size,i;

    gBtlPoolInitArg = arg;
    gBtlPool = Heap_Alloc(sizeof(BtlPool), 0x20, 0, HEAP_ANY);
    memset(gBtlPool, 0, sizeof(BtlPool));
    gBtlPool->arenaMask = 0x3FE;
    size = BtlPool_CalcArenaSize();
    if (size > 0) {
        if(TagBattle_Active()){extern unsigned Port_GameBigAlloc(int size);ARENA_MEM=(u8 *)Port_GameBigAlloc(size);}
        else ARENA_MEM = Heap_Alloc(size, 0x20, 0, HEAP_ANY);
        memset(ARENA_MEM, 0, size);
        BtlPool_CarveArenas();
    }
    memset(sTagPools,0,sizeof(sTagPools));
    for(i=0;i<(TagBattle_Count()-2)*2;++i){
        extern unsigned Port_GameBigAlloc(int size);
        u8 *mem=(u8 *)Port_GameBigAlloc(0x80000);
        memset(mem,0,0x80000);BtlPool_InitSlot(&sTagPools[i],0x80000,mem);
    }
}

/* Frees the arena memory and the pool manager. */
void BtlPool_Term(void) {
    s32 i;
    for(i=0;i<16;++i)if(sTagPools[i].base!=NULL){Heap_Free(sTagPools[i].base);memset(&sTagPools[i],0,sizeof(BtlPoolSlot));}
    if (ARENA_MEM != NULL) {
        Heap_Free(ARENA_MEM);
        ARENA_MEM = NULL;
    }
    if (gBtlPool != NULL) {
        Heap_Free(gBtlPool);
        gBtlPool = NULL;
    }
}

/* Allocates size bytes (rounded up to 32) from a slot: bump allocation for an arena, the heap otherwise. */
void *BtlPool_Alloc(s32 slot, s32 size) {
    s32 aligned = BtlPool_AlignUp32(size);
    BtlPoolSlot *s = TagPool_Slot(slot);
    void *ptr;

    if (slot >= 9 || (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(slot))) {
        if(TagBattle_Active() && s->used+aligned>s->size){fprintf(stderr,"tag pool overflow: slot %d used %d request %d capacity %d\n",slot,s->used,aligned,s->size);exit(3);}
        ptr = s->cur;
        s->cur += aligned;
        s->used += aligned;
    } else {
        s->used += aligned;
        ptr = Heap_Alloc(size, 0x20, 0, HEAP_ANY);
    }
    return ptr;
}

/* Frees a pointer from a slot; a no-op for arena slots, which are only released by BtlPool_Reset. */
void BtlPool_Free(s32 slot, void *ptr) {
    if (slot < 9 && !(gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(slot))) {
        if (ptr != NULL) {
            Heap_Free(ptr);
        }
    }
}

/* Rewinds an arena slot to empty. */
void BtlPool_Reset(s32 slot) {
    BtlPoolSlot *s = TagPool_Slot(slot);

    if (slot >= 9 || (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(slot))) {
        s->cur = s->base;
        s->used = 0;
    }
}

/* Selects the slot that BtlPool_GetCurrent reports. */
void BtlPool_SetCurrent(s32 slot) {
    gBtlPool->current = slot;
}

/* Returns the slot selected by BtlPool_SetCurrent. */
s32 BtlPool_GetCurrent(void) {
    return gBtlPool->current;
}

/* Stores the value BtlPool_Init also records (never read back). */
void BtlPool_SetInitArg(s32 arg) {
    gBtlPoolInitArg = arg;
}

/* Returns the total size of all enabled arena slots. */
s32 BtlPool_CalcArenaSize(void) {
    s32 total = 0;

    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(0)) {
        total = TagPool_Size(0xA000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(1)) {
        total += TagPool_Size(0x113000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(2)) {
        total += TagPool_Size(0x1E000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(3)) {
        total += TagPool_Size(0x1400);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(4)) {
        total += TagPool_Size(0x19000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(5)) {
        total += TagPool_Size(0x19000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(6)) {
        total += TagPool_Size(0x400);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(7)) {
        total += TagPool_Size(0x19000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(8)) {
        total += TagPool_Size(0x19000);
    }
    return total;
}

/* Hands each enabled arena slot its piece of arenaMem. Slots 5 and 8 test the bit of slots 4 and 7 (original bug). */
void BtlPool_CarveArenas(void) {
    u8 *mem = gBtlPool->arenaMem;

    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(0)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[0], TagPool_Size(0xA000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(1)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[1], TagPool_Size(0x113000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(2)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[2], TagPool_Size(0x1E000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(3)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[3], TagPool_Size(0x1400), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(4)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[4], TagPool_Size(0x19000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(4)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[5], TagPool_Size(0x19000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(6)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[6], TagPool_Size(0x400), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(7)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[7], TagPool_Size(0x19000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(7)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[8], TagPool_Size(0x19000), mem);
    }
}

/* Points a slot at its arena memory and returns the address just past it. */
u8 *BtlPool_InitSlot(BtlPoolSlot *slot, s32 size, u8 *mem) {
    slot->size = size;
    slot->base = mem;
    slot->cur = mem;
    return mem + size;
}

/* Rounds a size up to a multiple of 32. */
s32 BtlPool_AlignUp32(s32 size) {
    if (size % 32) {
        size = size / 32 * 32 + 32;
    }
    return size;
}
