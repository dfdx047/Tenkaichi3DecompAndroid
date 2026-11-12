#ifndef SYS_IOP_HEAP_H
#define SYS_IOP_HEAP_H

#include "types.h"

/* I/O-processor heap wrappers, src/sys/iop_heap.c = 0x11EC10..0x11ECB8. */

/* Sony libkernl (iopheap.c), RPC server 0x80000003. */
s32 sceSifInitIopHeap(void);
void *sceSifAllocIopHeap(s32 size);          /* fno 1 */
s32 sceSifFreeIopHeap(void *addr);           /* fno 2, through sceSifFreeSysMemory */
s32 sceSifQueryMaxFreeMemSize(void);         /* fno 6 */
s32 sceSifQueryTotalFreeMemSize(void);       /* fno 7 */

void Sys_InitIopHeap(void);
void *IopHeap_Alloc(s32 size);
s32 IopHeap_Free(void *addr);
s32 IopHeap_GetTotalFree(void);
s32 IopHeap_GetMaxFree(void);
void IopHeap_PrintFree(void);

#endif
