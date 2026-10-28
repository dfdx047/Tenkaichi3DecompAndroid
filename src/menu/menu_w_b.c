#include "common.h"
#include "menu/menu_w.h"

/*
 * Menu overlay DBZP.BIN, 0x39E940..0x39EB08: the handler of progress modes 48..50, "Evolution Z" (main-menu
 * item 5). It has no data of its own, so whether it is the last function of the Shop file, the first of the
 * EvoTop file or a file of its own cannot be told from the layout (the other handlers are files of their own).
 */

#define MODE_MAIN_MENU 4
#define MODE_EVOZ_MENU 48    /* the mode's top menu (EvoTop) */
#define MODE_EVOZ_CUSTOM 49  /* the character customising screen (EvoZ, chunk menu_u) */
#define MODE_EVOZ_SHOP 50    /* the item shop */

#define BGM_EVOZ 0x10B18

extern s32 EvoTop_Run(s32 section); /* 0x39F9D0, next chunk: 0 back, 1 customise, 2 shop */
extern s32 EvoZ_Run(s32 section);   /* 0x393AB0, chunk menu_u: the character customising screen */

/*
 * Modes 48..50: loads archive 7 (file baseFile + 4) and runs the top menu, which leads to the customising
 * screen (49) or the shop (50); both return to the top menu. Returns 0 (back to the main menu): this mode never
 * starts a battle, and Progress_Main ignores the result.
 */
s32 EvoMode_Main(void) {
    s32 bgm = 0;
    s32 done = 0;
    s32 result = 1;
    s32 r;

    EVO_PROGRESS_CURSOR = -1;
    if (gMenuArc7 == NULL) {
        gMenuArc7 = File_LoadSync(gProgress->baseFile + 4, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case MODE_EVOZ_MENU:
            if (!bgm) {
                bgm = 1;
                Bgm_Play(DISC_RESOURCE(BGM_EVOZ, 0x839));
            }
            r = EvoTop_Run(1);
            if (r != 0) {
                switch (r) {
                case 1:
                    gProgress->mode = MODE_EVOZ_CUSTOM;
                    break;
                case 2:
                    gProgress->mode = MODE_EVOZ_SHOP;
                    break;
                }
            } else {
                Adx_StopAll();
                bgm = 0;
                result = 0;
                gProgress->mode = MODE_MAIN_MENU;
                done = 1;
            }
            break;
        case MODE_EVOZ_CUSTOM:
            EvoZ_Run(3);
            gProgress->mode = MODE_EVOZ_MENU;
            break;
        case MODE_EVOZ_SHOP:
            Shop_Run(2);
            gProgress->mode = MODE_EVOZ_MENU;
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc7 != NULL) {
        Heap_Free(gMenuArc7);
        gMenuArc7 = NULL;
    }
    return result;
}
