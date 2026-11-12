#include "common.h"
#include "sys/debug.h"

/* Empty hook called once at boot, between Dma_InitBuffers and Pad_Init. */
void Dbg_Init(void) {
}

/* Empty hook called by Gfx_EndFrame. */
void Dbg_EndFrame(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub2630A8(void) {
}

/* Empty hook called by Gfx_BeginFrame. */
void Dbg_BeginFrame(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub2630B8(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub2630C0(void) {
}

/* Unused stub that returns 2. */
s32 Dbg_Stub2630C8(void) {
    return 2;
}

/* Unused stub that returns 0. */
s32 Dbg_Stub2630D0(void) {
    return 0;
}

/* Empty hook called after the frame buffer read-back (sceGsExecStoreImage) at 0x244314. */
void Dbg_AfterStoreImage(void) {
}

/* Empty stub, no callers. */
void Dbg_Stub2630E0(void) {
}
