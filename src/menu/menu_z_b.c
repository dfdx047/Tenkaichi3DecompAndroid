#include "common.h"
#include "menu/menu_z.h"

/*
 * Menu overlay DBZP.BIN, 0x3A9850..0x3A9A70: Dc_Main, the handler of progress modes 53..56 (the Data Center,
 * main-menu item 7). No data of its own, so whether it is a file of its own or the last function of the DcList
 * file cannot be told from the layout.
 */

#define DC_BGM 0x10B18

/*
 * Runs the Data Center until it is left: mode 53 the top menu, 54 the password screen, 55 the list of saved
 * custom characters, 56 the replay menu. Returns 1 to leave the overlay (a replay was chosen: the battle starts),
 * 0 to go on with the mode it has set.
 */
s32 Dc_Main(void) {
    s32 ret = 1;
    s32 done;
    s32 next;

    ZPROG->dcCursor = -1;
    ZPROG->unk698 = 0;
    done = 0;
    if (gMenuArc8 == NULL) {
        gMenuArc8 = File_LoadSync(gProgress->baseFile + 0xB, NULL, 0);
    }
    do {
        switch (gProgress->mode) {
        case 53:
            Bgm_Play(DISC_RESOURCE(DC_BGM, 0x856));
            next = DcMenu_Run(1);
            if (next != 0) {
                Adx_StopAll();
                gProgress->mode = next;
            } else {
                ret = 0;
                Adx_StopAll();
                gProgress->mode = 4;
                done = 1;
            }
            break;
        case 54:
            Bgm_Play(DISC_RESOURCE(DC_BGM, 0x856));
            DcPass_Run(3);
            Adx_StopAll();
            gProgress->mode = 53;
            break;
        case 55:
            Bgm_Play(DISC_RESOURCE(DC_BGM, 0x892));
            DcList_Run(2);
            Adx_StopAll();
            gProgress->mode = 53;
            break;
        case 56:
            Bgm_Play(DISC_RESOURCE(DC_BGM, 0x87E));
            next = ReplayMenu_Run(4);
            if (next != 0) {
                /* The constant lives in a variable set BEFORE the call: only then is its lifetime long enough
                   for the loop pass to move the load out of the loop (li s5,1 in front of the loop). */
                s32 start = 1;

                Adx_StopAll();
                if (next == start) {
                    ret = 1;
                } else {
                    ret = 0;
                    gProgress->mode = next;
                }
                done = 1;
            } else {
                Adx_StopAll();
                gProgress->mode = 53;
            }
            break;
        }
        sceGsSyncPath(0, 0);
    } while (!done);
    if (gMenuArc8 != NULL) {
        Heap_Free(gMenuArc8);
        gMenuArc8 = NULL;
    }
    ZPROG->dcVisits++;
    return ret;
}
