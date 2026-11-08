#include "common.h"
#include "menu/menu_h.h"

/*
 * 0x3590A8..0x359358: two of the handlers Progress_Main dispatches to. They emit no data. They are not part of
 * the CharRef object: appended to menu_h_b.c, CharRefMode_Main no longer matches (CharRef_Run is then defined
 * in the same file). Put in front of menu_h_d.c (the Train object) both still match, so they are either the
 * head of that object or a file of their own.
 */

extern s32 Train_Run(s32 section); /* the training menu's frame loop (next chunk, menu_i) */
extern s32 CharSel_Run(s32 kind);  /* the character select (chunk 5, menu_e) */

#define MODE_MAIN_MENU 4
#define MODE_TRAIN_MENU 44
#define MODE_TRAIN_SELECT 45
#define MODE_CHAR_REF 60

#define BGM_CHAR_REF 0x10B18
#define BGM_TRAIN 0x10B1A

/* Mode 60: loads archive 9 (file baseFile + 15) and runs the character reference; back to the main menu. */
s32 CharRefMode_Main(void) {
    s32 bgm = 0;
    s32 done = 0;
    s32 result = 1;

    if (gMenuArc9 == NULL) {
        gMenuArc9 = File_LoadSync(gProgress->baseFile + 15, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case MODE_CHAR_REF:
            if (!bgm) {
                bgm = 1;
                Bgm_Play(DISC_RESOURCE(BGM_CHAR_REF, 0x83C));
            }
            if (CharRef_Run(1) == 0) {
                Adx_StopAll();
                bgm = 0;
                result = 0;
                gProgress->mode = MODE_MAIN_MENU;
                done = 1;
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc9 != NULL) {
        Heap_Free(gMenuArc9);
        gMenuArc9 = NULL;
    }
    return result;
}

/*
 * Modes 44..45: loads archive 6 (file baseFile + 13). Mode 44 is the training menu, mode 45 the character
 * select it leads to. Returns 1 when a battle was set up (Progress_Main then leaves the overlay) and 0 when the
 * player went back to the main menu.
 */
s32 TrainMode_Main(void) {
    s32 done = 0;
    s32 result = 1;
    s32 r;

    if (gMenuArc6 == NULL) {
        gMenuArc6 = File_LoadSync(gProgress->baseFile + 13, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case MODE_TRAIN_MENU:
            Bgm_Play(DISC_RESOURCE(BGM_TRAIN, 0x855));
            r = Train_Run(1);
            if (r != 0) {
                if (r != 1) {
                    if (r == MODE_TRAIN_SELECT) {
                        gProgress->mode = r;
                    }
                } else {
                    done = 1;
                }
                Adx_StopAll();
            } else {
                Adx_StopAll();
                result = 0;
                gProgress->mode = MODE_MAIN_MENU;
                done = 1;
            }
            break;
        case MODE_TRAIN_SELECT:
            if (CharSel_Run(2)) {
                Adx_StopAll();
                done = 1;
            } else {
                Adx_StopAll();
                gProgress->mode = MODE_TRAIN_MENU;
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc6 != NULL) {
        Heap_Free(gMenuArc6);
        gMenuArc6 = NULL;
    }
    return result;
}
