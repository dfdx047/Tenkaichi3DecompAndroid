#include "menu/menu_u.h"

/* SimEvent, 0x392D20..0x392F10: gSimEvent[36], the last one. Read-only data: its jump table, 0x3BB110..0x3BB140. */

/* Event 36 (pictures 7 and 3): a gift of 5000 points, then the usual random loss of attack and defence. */
s32 SimEv36(USimDay *day) {
    switch (day->seq) {
    case 0:
        day->faceA = 11;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x62;
        Snd_PlaySe(2, 0x17);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 2:
        day->msgLine = -1;
        SimDay_Cmd(0x19);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        day->faceB = 3;
        day->faceA = 7;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(6);
        day->msgLine = 0xAF;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0xD3;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0xD4;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = 0xD5;
        SimDay_AddChange(3, 5000);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 8:
        day->msgLine = 0xD6;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0x63;
        SimDay_AddChange(0, -Rand_Range(3));
        SimDay_AddChange(1, -Rand_Range(3));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 11:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}
