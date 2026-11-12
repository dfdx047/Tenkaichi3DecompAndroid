#include "common.h"
#include "menu/menu_i.h"

BootCard *gBootCard = NULL; /* 0x3B5908 */

/*
 * BootCard, 0x35D660..0x35D948: the memory card check shown once at boot (called by FirstRun_Main). It runs the
 * main executable's card flow 2 (boot load: look for a system save, load it or offer to go on without one)
 * over a black screen; the flow draws its own dialogs.
 */

/* Card-flow callback (both outcomes): start the 15-frame countdown to the fade out. */
void BootCard_OnFlowDone(void) {
    gBootCard->flags |= BOOTCARD_DONE;
    gBootCard->flags |= BOOTCARD_LEAVING;
    gBootCard->timer = 0xF;
}

/* Unpacks the screen (section `section` of archive 0) and starts the dialog and card modules. */
void BootCard_Init(s32 section) {
    gBootCard = Heap_Alloc(0x14, 0x20, 0, 2);
    memset(gBootCard, 0, 0x14);
    gBootCard->pack = (u32 *)MPACK_AT(gMenuArc0, section);
    gBootCard->res = Sprite_Unpack(gBootCard->pack, NULL, NULL);
    Dialog_Init(MPACK_AT(gBootCard->res, 1), NULL, 0);
    McFlow_Init(1);
    McFlow_SetDoneCb(0, BootCard_OnFlowDone, NULL);
    McFlow_SetDoneCb(1, BootCard_OnFlowDone, NULL);
}

/* Stops the dialog and card modules and frees the screen. */
void BootCard_Term(void) {
    Dialog_Term();
    McFlow_Term();
    if (gBootCard->res != NULL) {
        Heap_Free(gBootCard->res);
        gBootCard->res = NULL;
    }
    if (gBootCard != NULL) {
        Heap_Free(gBootCard);
        gBootCard = NULL;
    }
}

/* The screen's frame loop: fade in, run the boot-load card flow, wait 15 frames, fade out. Returns 1. */
s32 BootCard_Run(s32 section) {
    if(gBootCard==NULL)BootCard_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        gBootCard->mcState = McFlow_Update();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gBootCard->flags & BOOTCARD_STARTED)) {
                gBootCard->flags |= BOOTCARD_STARTED;
                McFlow_Start(2);
            }
        }
        if (ColorFade_IsFadingOut()) {
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gBootCard->flags & BOOTCARD_LEAVING) {
            if (--gBootCard->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        }
    }
    sceGsSyncPath(0, 0);
    BootCard_Term();
    Dma_ResetBuffers();
    return 1;
}
