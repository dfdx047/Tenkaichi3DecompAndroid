#include "common.h"
#include "battle/btl_pool.h"
#include "sys/heap.h"

extern void *memset(void *dst, s32 c, u32 n);

extern BtlPool *gBtlPool;
/* Written by BtlPool_Init and BtlPool_SetInitArg, never read. */
extern s32 gBtlPoolInitArg;

/* The arena block is reached through a pointer to the field, not as a plain member: after a store to it
 * the compiler reloads gBtlPool, which a direct `gBtlPool->arenaMem = x` does not reproduce. */
#define ARENA_MEM (*&gBtlPool->arenaMem)

#ifdef PORT
/* Host allocations keep effect pools independent. The original bump arenas
 * silently overwrite the next pool when more than two fighters create tasks. */
typedef struct NativePoolBlock { struct NativePoolBlock *next; } NativePoolBlock;
static NativePoolBlock *nativeBlocks[BTL_POOL_SLOT_COUNT];
static s32 nativePools;
extern unsigned Port_GameBigAlloc(s32 size);
extern s32 Port_GameBigFree(unsigned ptr);
extern void Port_TagTeamFatal(const char *message);
#endif

s32 BtlPool_CalcArenaSize(void);
void BtlPool_CarveArenas(void);
u8 *BtlPool_InitSlot(BtlPoolSlot *slot, s32 size, u8 *mem);

/* Allocates the pool manager and the memory behind every arena slot. */
void BtlPool_Init(s32 arg) {
    s32 size;

    gBtlPoolInitArg = arg;
    gBtlPool = Heap_Alloc(sizeof(BtlPool), 0x20, 0, HEAP_ANY);
    memset(gBtlPool, 0, sizeof(BtlPool));
    gBtlPool->arenaMask = 0x3FE;
#ifdef PORT
    /* Native effect structures have capacity for ten actors even in a normal
       duel. BT4 also adds costume emitters that exceed BT3's fixed arenas.
       Use independent allocations in every native battle, not just tag mode. */
    nativePools = 1;
    memset(nativeBlocks, 0, sizeof(nativeBlocks));
    if (nativePools) gBtlPool->arenaMask = 0;
#endif
    size = BtlPool_CalcArenaSize();
    if (size > 0) {
        ARENA_MEM = Heap_Alloc(size, 0x20, 0, HEAP_ANY);
        memset(ARENA_MEM, 0, size);
        BtlPool_CarveArenas();
    }
}

/* Frees the arena memory and the pool manager. */
void BtlPool_Term(void) {
#ifdef PORT
    s32 i;
    if (nativePools) for (i = 0; i < BTL_POOL_SLOT_COUNT; i++) BtlPool_Reset(i);
#endif
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
#ifdef PORT
    if (nativePools) {
        NativePoolBlock *block;
        if ((u32)slot >= BTL_POOL_SLOT_COUNT || size < 0) Port_TagTeamFatal("invalid effect pool allocation");
        block = (NativePoolBlock *)Port_GameBigAlloc(size + 64);
        if (!block) Port_TagTeamFatal("native effect memory exhausted");
        block->next = nativeBlocks[slot];
        nativeBlocks[slot] = block;
        return (u8 *)block + 64;
    }
#endif
    s32 aligned = BtlPool_AlignUp32(size);
    BtlPoolSlot *s = &gBtlPool->slots[slot];
    void *ptr;

    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(slot)) {
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
#ifdef PORT
    if (nativePools) {
        NativePoolBlock **at;
        if ((u32)slot >= BTL_POOL_SLOT_COUNT) Port_TagTeamFatal("invalid effect pool release");
        at = &nativeBlocks[slot];
        while (*at) {
            NativePoolBlock *block = *at;
            if ((u8 *)block + 64 == ptr) { *at = block->next; Port_GameBigFree((u32)block); return; }
            at = &block->next;
        }
        return;
    }
#endif
    if (!(gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(slot))) {
        if (ptr != NULL) {
            Heap_Free(ptr);
        }
    }
}

/* Rewinds an arena slot to empty. */
void BtlPool_Reset(s32 slot) {
#ifdef PORT
    if (nativePools) {
        if ((u32)slot >= BTL_POOL_SLOT_COUNT) Port_TagTeamFatal("invalid effect pool reset");
        while (nativeBlocks[slot]) {
            NativePoolBlock *block = nativeBlocks[slot];
            nativeBlocks[slot] = block->next;
            Port_GameBigFree((u32)block);
        }
        return;
    }
#endif
    BtlPoolSlot *s = &gBtlPool->slots[slot];

    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(slot)) {
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
        total = BtlPool_AlignUp32(0xA000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(1)) {
        total += BtlPool_AlignUp32(0x113000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(2)) {
        total += BtlPool_AlignUp32(0x1E000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(3)) {
        total += BtlPool_AlignUp32(0x1400);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(4)) {
        total += BtlPool_AlignUp32(0x19000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(5)) {
        total += BtlPool_AlignUp32(0x19000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(6)) {
        total += BtlPool_AlignUp32(0x400);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(7)) {
        total += BtlPool_AlignUp32(0x19000);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(8)) {
        total += BtlPool_AlignUp32(0x19000);
    }
    return total;
}

/* Hands each enabled arena slot its piece of arenaMem. Slots 5 and 8 test the bit of slots 4 and 7 (original bug). */
void BtlPool_CarveArenas(void) {
    u8 *mem = gBtlPool->arenaMem;

    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(0)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[0], BtlPool_AlignUp32(0xA000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(1)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[1], BtlPool_AlignUp32(0x113000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(2)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[2], BtlPool_AlignUp32(0x1E000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(3)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[3], BtlPool_AlignUp32(0x1400), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(4)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[4], BtlPool_AlignUp32(0x19000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(4)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[5], BtlPool_AlignUp32(0x19000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(6)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[6], BtlPool_AlignUp32(0x400), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(7)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[7], BtlPool_AlignUp32(0x19000), mem);
    }
    if (gBtlPool->arenaMask & BTL_POOL_ARENA_BIT(7)) {
        mem = BtlPool_InitSlot(&gBtlPool->slots[8], BtlPool_AlignUp32(0x19000), mem);
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
