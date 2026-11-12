#include "common.h"
#include "sys/heap.h"
#include "sys/heap_info.h"

extern HeapBlock *gHeapStart[HEAP_COUNT];

HeapBlock *Heap_NextBlock(HeapBlock *block);

/* Number of blocks (free and used) in one heap. */
s32 Heap_CountBlocks(s32 heap) {
    return Heap_CountFreeBlocks(heap) + Heap_CountUsedBlocks(heap);
}

/* Number of free blocks in one heap. */
s32 Heap_CountFreeBlocks(s32 heap) {
    s32 count = 0;
    HeapBlock *block;

    for (block = gHeapStart[heap]; block != NULL; block = Heap_NextBlock(block)) {
        if (block->used == 0) {
            count++;
        }
    }
    return count;
}

/* Number of used blocks in one heap. */
s32 Heap_CountUsedBlocks(s32 heap) {
    s32 count = 0;
    HeapBlock *block;

    for (block = gHeapStart[heap]; block != NULL; block = Heap_NextBlock(block)) {
        if (block->used == 1) {
            count++;
        }
    }
    return count;
}
