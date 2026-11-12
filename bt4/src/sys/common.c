#include "common.h"
#include "sys/common.h"
#include "sys/file.h"
#include "sys/heap.h"

/* libc */
extern void *memset(void *dst, s32 value, u32 size);

extern void Snd_LoadBank(s32 mask, void *data, s32 unk);

/* File_Request is called here with a third argument (the buffer size) that it ignores. */
#define File_RequestSized ((void *(*)(s32, void *, s32))File_Request)

/* Reads file 5 and allocates the buffers of files 2, 3 and 4 from their sizes. */
void Common_LoadBoot(void) {
    CommonRes *res = gCommonRes;

    memset(res, 0, 0x20);
    res->boot = File_LoadSync(COMMON_FILE_BOOT, NULL, 0);
    res->size[0] = File_GetSize(2);
    res->data[0] = Heap_Alloc(res->size[0], 0x40, 0, HEAP_ANY);
    res->size[1] = File_GetSize(3);
    res->data[1] = Heap_Alloc(res->size[1], 0x40, 0, HEAP_ANY);
    res->size[2] = File_GetSize(4);
    res->data[2] = Heap_Alloc(res->size[2], 0x40, 0, HEAP_ANY);
}

/* Reads files 2, 3 and 4 into their buffers and loads sound bank 0x14B, blocking until all are in. */
void Common_Reload(void) {
    CommonRes *res = gCommonRes;
    void *bank;

    File_CancelRequests();
    res->data[0] = File_RequestSized(2, res->data[0], res->size[0]);
    res->data[1] = File_RequestSized(3, res->data[1], res->size[1]);
    res->data[2] = File_RequestSized(4, res->data[2], res->size[2]);
    bank = File_RequestSized(COMMON_FILE_SND_BANK, NULL, 0);
    while (!File_UpdateRequests()) {
    }
    File_CancelRequests();
    Snd_LoadBank(1, bank, 1);
    Heap_Free(bank);
}

/* Allocates and clears gCommonRes. */
void Common_Init(void) {
    gCommonRes = Heap_Alloc(sizeof(CommonRes), 0x20, 0, HEAP_ANY);
    memset(gCommonRes, 0, sizeof(CommonRes));
}
