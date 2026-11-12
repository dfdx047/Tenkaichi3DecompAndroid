#include "common.h"
#include "sys/late_a.h"
#include "sys/heap.h"
#include "sys/snd.h"

/*
 * ShenSave, 0x2BF370..0x2BF6B0: the save screen that follows the wish screen (gProgress->mode 71). Nothing of
 * its own is drawn: the screen is the memory-card flow's dialog over a black fade. Built with -G0. See
 * sys/late_a.h.
 */

extern void *memset(void *dst, s32 c, u32 n);

extern void *Sprite_Unpack(void *src, void *dst, s32 *rawSize);
extern void File_Stub264D90(void);
extern void Pad_Update(void);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 vsyncs);
extern void Dma_Flush(void);
extern void Dma_ResetBuffers(void);
extern void ColorFade_StartIn(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_StartOut(s32 r, s32 g, s32 b, s32 frames);
extern void ColorFade_Update(void);
extern void ColorFade_Draw(void);
extern s32 ColorFade_IsInDone(void);
extern void Dialog_Init(u32 *file, u32 *msgTbl, s32 size);
extern void Dialog_Term(void);
/* sys/mcflow_a.c. McFlow_Init takes no argument; this caller passes 1. */
extern void McFlow_Init(s32 unused);
extern void McFlow_Term(void);
extern void McFlow_Start(s32 mode);
extern void McFlow_SetDoneCb(s32 idx, void (*cb)(s32 arg), s32 arg);
extern s32 McFlow_Update(void);

/* The menu overlay's archive pointer (DBZP.BIN data). */
extern u32 *gMenuArc12;

/* Defined here: this object's .data (0x2EB350). */
ShenSave *gShenSave = NULL;

/* "Done" callback of the memory-card flow (both slots): the screen leaves 15 frames later. */
void ShenSave_OnFlowDone(s32 arg) {
    gShenSave->flags |= 1;
    gShenSave->timer = 15;
}

/* Unpacks the screen's pack file, builds the dialog and the memory-card flow. */
void ShenSave_Init(s32 section) {
    gShenSave = Heap_Alloc(sizeof(ShenSave), 0x20, 0, HEAP_ANY);
    memset(gShenSave, 0, sizeof(ShenSave));
    gShenSave->pack = PACK_AT(gMenuArc12, section);
    gShenSave->res = Sprite_Unpack(gShenSave->pack, NULL, NULL);
    Dialog_Init(PACK_AT(gShenSave->res, 10), NULL, 0);
    McFlow_Init(1);
    McFlow_SetDoneCb(0, ShenSave_OnFlowDone, 0);
    McFlow_SetDoneCb(1, ShenSave_OnFlowDone, 0);
}

/* Frees everything ShenSave_Init made. */
void ShenSave_Term(void) {
    Dialog_Term();
    McFlow_Term();
    if (gShenSave->res != NULL) {
        Heap_Free(gShenSave->res);
        gShenSave->res = NULL;
    }
    if (gShenSave != NULL) {
        Heap_Free(gShenSave);
        gShenSave = NULL;
    }
}

/* Empty: the screen draws nothing of its own. */
void ShenSave_Draw(void) {
}

/* Empty. */
void ShenSave_Advance(void) {
}

/* Empty: called on the frames on which the memory-card flow reports state 0. */
void ShenSave_Update(void) {
}

/* After the flow has ended: starts the fade-out once and counts the timer down. Returns 1 when it reaches 0. */
s32 ShenSave_UpdateExit(void) {
    if (gShenSave->flags & 1) {
        if (!(gShenSave->flags & 2)) {
            gShenSave->flags |= 2;
            ColorFade_StartOut(0, 0, 0, 20);
        }
        if (--gShenSave->timer == 0) {
            return 1;
        }
    }
    return 0;
}

/*
 * The save screen's frame loop (Gfx_EndFrame(1): one vertical blank per frame). Per frame: Gfx_BeginFrame,
 * Pad_Update, ColorFade_Update, Snd_Update, the two empty hooks, ColorFade_Draw, McFlow_Update (its state is
 * kept), Gfx_EndFrame, Dma_Flush. Memory-card flow 0 (save) is started once the fade-in is complete. Leaves 15
 * frames after the flow's "done" callback, the first of them starting a 20-frame fade-out.
 */
s32 ShenSave_Run(s32 section) {
    s32 ret;

    ShenSave_Init(section);
    ColorFade_StartIn(0, 0, 0, 20);
    for (;;) {
        Gfx_BeginFrame();
        Pad_Update();
        ColorFade_Update();
        Snd_Update();
        ShenSave_Advance();
        ShenSave_Draw();
        ColorFade_Draw();
        gShenSave->flowState = McFlow_Update();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ShenSave_UpdateExit()) {
            break;
        }
        if (ColorFade_IsInDone() && !(gShenSave->flags & 4)) {
            McFlow_Start(0);
            gShenSave->flags |= 4;
        }
        if (gShenSave->flowState == 0) {
            ShenSave_Update();
        }
    }
    ret = gShenSave->result;
    ShenSave_Term();
    Dma_ResetBuffers();
    return ret;
}
