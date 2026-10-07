#include "common.h"
#include "sys/heap.h"

extern void *malloc(u32 size);
s32 Mem_Align4Pad(u32 value);
s32 Mem_Align4Rem(u32 value);

extern HeapBlock *gHeapStart[HEAP_COUNT];
extern HeapBlock *gHeapEnd[HEAP_COUNT];
extern s32 gHeapSize[HEAP_COUNT];
/* Written by the setters below and cleared by Heap_Init, but never read: leftovers of a debug feature. */
extern s32 gHeapDebugValue;
extern u8 gHeapDebugFlag;
extern s32 gHeapDebugUnused;
extern u8 D_3BE71C[];
extern s32 gHeapUnk70[HEAP_COUNT];
extern s32 gHeapUnk78[HEAP_COUNT];

void Heap_ReportBadFree(void);

u32 Heap_BlockData(HeapBlock *block) {
    return (u32)block + sizeof(HeapBlock);
}

void Heap_InitFreeBlock(HeapBlock *block, s32 size) {
    block->magic = HEAP_MAGIC;
    block->align = 0x20;
    block->dataSize = size - 0x20;
    block->used = 0;
    block->fromTail = 0;
    block->size = size;
    block->data = 0;
    *(s32 *)((u8 *)block + size - 4) = size;
}

void Heap_InitUsedBlock(HeapBlock *block, s32 size, s32 dataSize, u32 align, s32 fromTail) {
    u32 data = Heap_BlockData(block);

    block->magic = HEAP_MAGIC;
    block->used = 1;
    block->fromTail = fromTail;
    block->align = align;
    block->size = size;
    block->data = data + (align - data % align) % align;
    block->dataSize = dataSize;
    *(s32 *)((u8 *)block + size - 4) = size;
}

HeapBlock *Heap_PrevBlock(HeapBlock *block) {
    HeapBlock *prev = NULL;

    if (block != NULL) {
        if (block != gHeapStart[0] && block != gHeapStart[1]) {
            prev = (HeapBlock *)((u8 *)block - *(s32 *)((u8 *)block - 4));
        }
    }
    return prev;
}

HeapBlock *Heap_NextBlock(HeapBlock *block) {
    HeapBlock *next = NULL;

    if (block != NULL) {
        block = (HeapBlock *)((u8 *)block + block->size);
        if (block != gHeapEnd[0] && block != gHeapEnd[1]) {
            next = block;
        }
    }
    return next;
}

void Heap_Create(void) {
    s32 size = 0x1EFB000 - (u32)D_3BE71C;
    HeapBlock *start;

    size += Mem_Align4Pad(size);
    start = malloc(size);
    gHeapSize[0] = size;
    gHeapEnd[0] = (HeapBlock *)((u8 *)start + size);
    gHeapStart[0] = start;
    Heap_InitFreeBlock(start, size);
}

HeapBlock *Heap_FindBlock(u32 data) {
    HeapBlock *found = NULL;
    HeapBlock *block;
    s32 i;

    for (i = 0; i < HEAP_COUNT; i++) {
        for (block = gHeapStart[i]; block != NULL; block = Heap_NextBlock(block)) {
            if (block->used == 1 && block->data == data) {
                found = block;
                break;
            }
        }
    }
    return found;
}

HeapBlock *Heap_FindFreeFromHead(s32 heap, s32 size);

HeapBlock *Heap_SearchFromHead(s32 size, s32 heap) {
    HeapBlock *block = NULL;
    s32 i;

    if (heap != HEAP_ANY) {
        block = Heap_FindFreeFromHead(heap, size);
    } else {
        for (i = 0; i < HEAP_COUNT; i++) {
            block = Heap_FindFreeFromHead(i, size);
            if (block != NULL) {
                break;
            }
        }
    }
    return block;
}

HeapBlock *Heap_FindFreeFromHead(s32 heap, s32 size) {
    HeapBlock *found = NULL;
    HeapBlock *block;

    for (block = gHeapStart[heap]; block != NULL; block = Heap_NextBlock(block)) {
        if (block->used == 0 && block->size >= size) {
            found = block;
            break;
        }
    }
    return found;
}

HeapBlock *Heap_FindFreeFromTail(s32 heap, s32 size);

HeapBlock *Heap_SearchFromTail(s32 size, s32 heap) {
    HeapBlock *block = NULL;
    s32 i;

    if (heap != HEAP_ANY) {
        block = Heap_FindFreeFromTail(heap, size);
    } else {
        for (i = HEAP_COUNT - 1; i >= 0; i--) {
            block = Heap_FindFreeFromTail(i, size);
            if (block != NULL) {
                break;
            }
        }
    }
    return block;
}

HeapBlock *Heap_FindFreeFromTail(s32 heap, s32 size) {
    HeapBlock *found = NULL;
    HeapBlock *block;

    for (block = Heap_PrevBlock(gHeapEnd[heap]); block != NULL; block = Heap_PrevBlock(block)) {
        if (block->used == 0 && block->size >= size) {
            found = block;
            break;
        }
    }
    return found;
}

HeapBlock *Heap_AllocBlock(s32 size, u32 align, s32 fromTail, s32 heap) {
    HeapBlock *block;
    HeapBlock *free;
    s32 need;

    if (size <= 0) {
        return NULL;
    }
    need = size + align + 0x24;
    if (fromTail == 0) {
        block = Heap_SearchFromHead(need, heap);
    } else {
        block = Heap_SearchFromTail(need, heap);
    }
    if (block == NULL) {
        return NULL;
    }
    if (block->dataSize - need > 0x80) {
        if (fromTail == 0) {
            need += Mem_Align4Pad((u32)block + need);
            Heap_InitFreeBlock((HeapBlock *)((u8 *)block + need), block->size - need);
        } else {
            free = block;
            need += Mem_Align4Rem((u32)block + (block->size - need));
            block = (HeapBlock *)((u8 *)block + (free->size - need));
            free->dataSize -= need;
            free->size -= need;
            *(s32 *)((u8 *)free + free->size - 4) = free->size;
        }
    } else {
        need = block->size;
    }
    Heap_InitUsedBlock(block, need, size, align, fromTail);
    return block;
}

void Heap_FreeBlock(HeapBlock *block) {
    HeapBlock *prev;
    HeapBlock *next;
    s32 merge = 0;

    Heap_InitFreeBlock(block, block->size);
    prev = Heap_PrevBlock(block);
    next = Heap_NextBlock(block);
    if (prev != NULL) {
        merge = prev->used == 0;
    }
    if (next != NULL && next->used == 0) {
        merge |= 2;
    }
    switch (merge) {
    case 0:
        break;
    case 1:
        prev->size += block->size;
        prev->dataSize += block->size;
        *(s32 *)((u8 *)prev + prev->size - 4) = prev->size;
        break;
    case 2:
        block->size += next->size;
        block->dataSize += next->size;
        *(s32 *)((u8 *)block + block->size - 4) = block->size;
        break;
    case 3:
        prev->size += block->size + next->size;
        prev->dataSize += block->size + next->size;
        *(s32 *)((u8 *)prev + prev->size - 4) = prev->size;
        break;
    }
}

void Heap_Init(void) {
    gHeapDebugValue = 0;
    gHeapDebugFlag = 0;
    gHeapDebugUnused = 0;
    Heap_Create();
}

void *Heap_Alloc(s32 size, u32 align, s32 fromTail, s32 heap) {
    HeapBlock *block = Heap_AllocBlock(size, align, fromTail, heap);
    void *data = NULL;

    if (block != NULL) {
        data = (void *)block->data;
    }
    return data;
}

void Heap_Free(void *ptr) {
    HeapBlock *block;

#ifdef PORT
    {
        /* PC build: a block that is not the heap's (a large stage buffer, battle_load.c) goes back to the port */
        extern int Port_GameBigFree(unsigned addr);
        if (Port_GameBigFree((u32)ptr)) {
            return;
        }
    }
#endif
    block = Heap_FindBlock((u32)ptr);

    if (block == NULL) {
        Heap_ReportBadFree();
    }
    Heap_FreeBlock(block);
}

void Heap_SetDebugValue(s32 value);
void Heap_SetDebugFlag(u8 flag);

void Heap_SetDebug(s32 value, u8 flag) {
    Heap_SetDebugValue(value);
    Heap_SetDebugFlag(flag);
}

void Heap_SetDebugValue(s32 value) {
    gHeapDebugValue = value;
}

void Heap_SetDebugFlag(u8 flag) {
    gHeapDebugFlag = flag;
}

/* Total bytes available in free blocks, across every heap. */
s32 Heap_GetFreeSize(void) {
    s32 total = 0;
    HeapBlock *block;
    s32 i;

    for (i = 0; i < HEAP_COUNT; i++) {
        for (block = gHeapStart[i]; block != NULL; block = Heap_NextBlock(block)) {
            if (block->used == 0) {
                total += block->dataSize;
            }
        }
    }
    return total;
}

/* Total bytes requested by live allocations, across every heap. */
s32 Heap_GetUsedSize(void) {
    s32 total = 0;
    HeapBlock *block;
    s32 i;

    for (i = 0; i < HEAP_COUNT; i++) {
        for (block = gHeapStart[i]; block != NULL; block = Heap_NextBlock(block)) {
            if (block->used == 1) {
                total += block->dataSize;
            }
        }
    }
    return total;
}

/* Size of the largest single free block. */
u32 Heap_GetLargestFree(void) {
    u32 largest = 0;
    HeapBlock *block;
    s32 i;

    for (i = 0; i < HEAP_COUNT; i++) {
        for (block = gHeapStart[i]; block != NULL; block = Heap_NextBlock(block)) {
            if (block->used == 0) {
                if (largest < (u32)block->dataSize) {
                    largest = block->dataSize;
                }
            }
        }
    }
    return largest;
}

s32 Heap_GetTotalSize(void) {
    return gHeapSize[0] + gHeapSize[1];
}

/* Called by Heap_Free for a pointer that is not a live block. The report itself was compiled out. */
void Heap_ReportBadFree(void) {
}

HeapBlock *Heap_GetStart(s32 heap) {
    return gHeapStart[heap];
}

HeapBlock *Heap_GetEnd(s32 heap) {
    return gHeapEnd[heap];
}

s32 Heap_GetSize(s32 heap) {
    return gHeapSize[heap];
}

s32 Heap_GetUnk70(s32 heap) {
    return gHeapUnk70[heap];
}

s32 Heap_GetUnk78(s32 heap) {
    return gHeapUnk78[heap];
}
