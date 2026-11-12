#include "common.h"
#include "sys/align.h"
#include "sys/spu_heap.h"

/*
 * Sound-RAM (SPU2) address allocator, 0x11ECB8..0x11F190.
 *
 * A table of 17 range descriptors {state, addr, size} covers SPU addresses 0x5010..0x1F5FE0. Each descriptor is
 * unused, a free range or a used range; the ranges always tile the whole region. The table is not sorted: the
 * neighbour of a range is found by searching for the descriptor that starts (or ends) at a given address.
 *
 * Alloc: the size is rounded up to 16, then the ranges are walked in ADDRESS order from the base and the first free
 * one that is large enough is taken (first fit, lowest address). The used range is carved from its start; the
 * remainder stays free in the same descriptor (which is released when the fit is exact).
 * Free: the range becomes free and is merged with a free neighbour on either side.
 *
 * There is no failure path: Alloc with no fitting range, Alloc with no unused descriptor (17 in total) and Free of
 * an address that is not the start of a range all dereference NULL.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern SpuBlock gSpuHeapBlocks[SPU_HEAP_BLOCKS];

/* Marks a descriptor unused. */
void SpuBlock_Clear(SpuBlock *block) {
    block->state = SPU_BLOCK_NONE;
    block->addr = 0;
    block->size = 0;
}

/* Makes a descriptor a free range, or unused when nothing is left. */
void SpuBlock_SetFree(SpuBlock *block, s32 addr, s32 size) {
    if (size <= 0) {
        SpuBlock_Clear(block);
        return;
    }
    block->size = size;
    block->addr = addr;
    block->state = SPU_BLOCK_FREE;
}

/* Makes a descriptor a used range. */
void SpuBlock_SetUsed(SpuBlock *block, s32 addr, s32 size) {
    block->state = SPU_BLOCK_USED;
    block->addr = addr;
    block->size = size;
}

/* Finds the descriptor that starts at `addr` (byEnd 0) or ends at it (byEnd 1); NULL when there is none. */
SpuBlock *SpuHeap_FindBlock(s32 addr, s32 byEnd) {
    SpuBlock *found = NULL;
    SpuBlock *block = gSpuHeapBlocks;
    s32 key = 0;
    s32 i;

    for (i = 0; i < SPU_HEAP_BLOCKS; i++, block++) {
        switch (byEnd) {
        case 0:
            key = block->addr;
            break;
        case 1:
            key = block->addr + block->size;
            break;
        }
        if (key == addr) {
            found = block;
            break;
        }
    }
    return found;
}

/* Empties the table and makes the whole region one free range. */
void SpuHeap_Init(void) {
    memset(gSpuHeapBlocks, 0, sizeof(gSpuHeapBlocks));
    SpuBlock_SetFree(&gSpuHeapBlocks[0], SPU_HEAP_BASE, SPU_HEAP_SIZE);
}

/* Reserves `size` bytes (rounded up to 16) in the lowest free range that fits; returns the SPU address. */
s32 SpuHeap_Alloc(s32 size) {
    s32 addr = SPU_HEAP_BASE;
    SpuBlock *fit = NULL;
    SpuBlock *used = NULL;
    SpuBlock *block;
    s32 i;

    size += Mem_Align16Pad(size);
    while ((block = SpuHeap_FindBlock(addr, 0)) != NULL) {
        if (block->state == SPU_BLOCK_FREE && block->size >= size) {
            fit = block;
            break;
        }
        addr += block->size;
    }
    for (i = 0, block = gSpuHeapBlocks; i < SPU_HEAP_BLOCKS; i++, block++) {
        if (block->state == SPU_BLOCK_NONE) {
            used = block;
            break;
        }
    }
    SpuBlock_SetUsed(used, fit->addr, size);
    SpuBlock_SetFree(fit, fit->addr + used->size, fit->size - used->size);
    return used->addr;
}

/* Releases the range that starts at `addr`, merging it with free neighbours. */
void SpuHeap_Free(s32 addr) {
    SpuBlock *block;
    SpuBlock *prev;
    SpuBlock *next;
    s32 end;
    s32 merge;

    block = SpuHeap_FindBlock(addr, 0);
    end = block->addr + block->size;
    merge = 0;
    prev = SpuHeap_FindBlock(addr, 1);
    next = SpuHeap_FindBlock(end, 0);

    if (prev != NULL) {
        merge = prev->state == SPU_BLOCK_FREE;
    }
    if (next != NULL && next->state == SPU_BLOCK_FREE) {
        merge |= 2;
    }
    switch (merge) {
    case 0:
        SpuBlock_SetFree(block, block->addr, block->size);
        break;
    case 1:
        SpuBlock_SetFree(prev, prev->addr, prev->size + block->size);
        SpuBlock_Clear(block);
        break;
    case 2:
        SpuBlock_SetFree(block, block->addr, block->size + next->size);
        SpuBlock_Clear(next);
        break;
    case 3:
        SpuBlock_SetFree(prev, prev->addr, prev->size + block->size + next->size);
        SpuBlock_Clear(block);
        SpuBlock_Clear(next);
        break;
    }
}

/* Total bytes in used ranges. */
s32 SpuHeap_GetUsedSize(void) {
    s32 total = 0;
    s32 i;

    for (i = 0; i < SPU_HEAP_BLOCKS; i++) {
        if (gSpuHeapBlocks[i].state == SPU_BLOCK_USED) {
            total += gSpuHeapBlocks[i].size;
        }
    }
    return total;
}

/* Total bytes in free ranges. */
s32 SpuHeap_GetFreeSize(void) {
    s32 total = 0;
    s32 i;

    for (i = 0; i < SPU_HEAP_BLOCKS; i++) {
        if (gSpuHeapBlocks[i].state == SPU_BLOCK_FREE) {
            total += gSpuHeapBlocks[i].size;
        }
    }
    return total;
}

/* Size of the largest free range. */
s32 SpuHeap_GetMaxFree(void) {
    s32 max = 0;
    s32 i;

    for (i = 0; i < SPU_HEAP_BLOCKS; i++) {
        if (gSpuHeapBlocks[i].state == SPU_BLOCK_FREE) {
            if (max < gSpuHeapBlocks[i].size) {
                max = gSpuHeapBlocks[i].size;
            }
        }
    }
    return max;
}

/* Walks the ranges in address order from the base and does nothing with them (a debug dump compiled out). */
void SpuHeap_Walk(void) {
    s32 addr = SPU_HEAP_BASE;
    SpuBlock *block;

    while ((block = SpuHeap_FindBlock(addr, 0)) != NULL) {
        addr += block->size;
    }
}
