#include "common.h"
#include "sys/iop_heap.h"

/*
 * I/O-processor heap wrappers, 0x11EC10..0x11ECB8. Each one only forwards to Sony's libkernl iopheap call
 * (SIF RPC server 0x80000003). Used by the sound module (bank header / sequence memory) and by boot.
 */

/* Binds the IOP heap RPC service. */
void Sys_InitIopHeap(void) {
    sceSifInitIopHeap();
}

/* Allocates `size` bytes of IOP memory; returns the IOP address, or NULL. */
void *IopHeap_Alloc(s32 size) {
    return sceSifAllocIopHeap(size);
}

/* Frees IOP memory returned by IopHeap_Alloc. */
s32 IopHeap_Free(void *addr) {
    return sceSifFreeIopHeap(addr);
}

/* Total free IOP memory in bytes, -1 when the RPC fails. */
s32 IopHeap_GetTotalFree(void) {
    return sceSifQueryTotalFreeMemSize();
}

/* Largest free IOP block in bytes, -1 when the RPC fails. */
s32 IopHeap_GetMaxFree(void) {
    return sceSifQueryMaxFreeMemSize();
}

/*
 * Queries both free sizes and drops the results (what used them, presumably a debug print of the sizes in
 * kilobytes, is compiled out). The conditional with two dead assignments is what makes the compiler save
 * $s0 without using it and keeps the second call from being a tail call: its branch survives until after
 * register allocation, so `total` is given a saved register, and only then is everything deleted.
 */
void IopHeap_PrintFree(void) {
    s32 total = IopHeap_GetTotalFree();
    s32 max = IopHeap_GetMaxFree();

    if (total != 0) {
        total /= 1024;
        max /= 1024;
    }
}
