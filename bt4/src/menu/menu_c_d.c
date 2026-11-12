#include "common.h"
#include "menu/menu_c.h"

HistSave *gHistSave = NULL; /* 0x3B1304 */

/*
 * HistSave, 0x341E08..0x342190: the save screen of the story mode (progress mode 10). A black screen that runs
 * the memory-card save flow McFlow_Start(0) after the fade in and leaves 15 frames after the flow reports back.
 * It has no strings or tables, so its object boundaries show only in the code: the three empty functions
 * (the screen pattern's Draw / Update / Input) and its own work pointer.
 */

/* Card flow callback (done and not done alike): leave the screen after 15 frames. */
void HistSave_OnCardDone(void) {
    gHistSave->flags |= 1;
    gHistSave->flags |= 2;
    gHistSave->timer = 15;
}

/* Loads the screen (section `section` of archive 2: only the dialog resources) and the card module. */
void HistSave_Init(s32 section) {
    gHistSave = Heap_Alloc(0x14, 0x20, 0, 2);
    memset(gHistSave, 0, 0x14);
    gHistSave->pack = (u32 *)MPACK_AT(gMenuArc2, section);
    gHistSave->res = Sprite_Unpack(gHistSave->pack, NULL, NULL);
    Dialog_Init(MPACK_AT(gHistSave->res, 1), NULL, 0);
    McFlow_Init(1);
    McFlow_SetDoneCb(0, HistSave_OnCardDone, NULL);
    McFlow_SetDoneCb(1, HistSave_OnCardDone, NULL);
}

/* Frees the screen. */
void HistSave_Term(void) {
    Dialog_Term();
    McFlow_Term();
    if (gHistSave->res != NULL) {
        Heap_Free(gHistSave->res);
        gHistSave->res = NULL;
    }
    if (gHistSave != NULL) {
        Heap_Free(gHistSave);
        gHistSave = NULL;
    }
}

/* Empty: the screen draws nothing of its own. */
void HistSave_Draw(void) {
}

/* Empty. */
void HistSave_Update(void) {
}

/* Empty: the card flow reads the pad itself. */
void HistSave_Input(s32 *result) {
}

/* The save screen's frame loop. The result depends on where the player came from: 1 after the result screen
   (mode 8: back to the episode list), 0 after the ending movie or the outro (modes 3 and 9: back to the saga select). */
s32 HistSave_Run(s32 section) {
    s32 result = 1;

    HistSave_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        HistSave_Update();
        HistSave_Draw();
        gHistSave->mcState = McFlow_Update();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (gProgress->flags & MPROG_FREEZE) {
            continue;
        }
        if (ColorFade_IsInDone()) {
            if (!(gHistSave->flags & 8)) {
                gHistSave->flags |= 8;
                McFlow_Start(0);
            }
        }
        if (ColorFade_IsFadingOut()) {
            continue;
        }
        if (ColorFade_IsOutDone()) {
            break;
        }
        if (gHistSave->flags & 2) {
            if (--gHistSave->timer == -1) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gHistSave->mcState == 0) {
            HistSave_Input(&result);
        }
    }
    switch (gProgress->prevMode) {
    case 3:
        result = 0;
        break;
    case 8:
        result = 1;
        break;
    case 9:
        result = 0;
        break;
    }
    HistSave_Term();
    Dma_ResetBuffers();
    return result;
}
