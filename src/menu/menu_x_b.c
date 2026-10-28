#include "common.h"
#include "menu/menu_x.h"

/*
 * 0x39FAA8..0x39FBB8: the handler Progress_Main dispatches to for mode 62. It emits no data, so nothing says
 * whether it is the last function of the EvoTop object, the first of the Option object or a file of its own.
 */

#define MODE_MAIN_MENU 4
#define MODE_OPTION 62

#define BGM_OPTION 0x10B18

/* Mode 62: loads archive 10 (file baseFile + 5) and runs the options screen; back to the main menu. */
s32 OptMode_Main(void) {
    s32 bgm = 0;
    s32 result = 1;

    if (gMenuArc10 == NULL) {
        gMenuArc10 = File_LoadSync(gProgress->baseFile + 5, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case MODE_OPTION:
            if (!bgm) {
                bgm = 1;
                Bgm_Play(DISC_RESOURCE(BGM_OPTION, 0x83E));
            }
            if (Option_Run(1) == 0) {
                Adx_StopAll();
                bgm = 0;
                result = 0;
                gProgress->mode = MODE_MAIN_MENU;
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (result > 0);
    if (gMenuArc10 != NULL) {
        Heap_Free(gMenuArc10);
        gMenuArc10 = NULL;
    }
    return result;
}
