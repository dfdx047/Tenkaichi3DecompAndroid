#include "menu/menu_u.h"

/* SimEvent, 0x391B70..0x391E28: gSimEvent[33]. Read-only data: its jump table, 0x3BAF60..0x3BAFA8. */

/*
 * Event 33 (pictures 5 and 4): a yes / no offer. Yes: picture 9, attack +10 and defence +10 for 30 % of the
 * current health (flag 0x100 is set with the message), no random loss. No: the usual random loss.
 */
s32 SimEv33(USimDay *day) {
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
        day->faceA = 5;
        day->faceB = 4;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(6);
        day->msgLine = 0xC1;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0xC2;
        SimDay_Cmd(0x11);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = -1;
        if (day->unkB8C == 0) {
            day->seq = 14;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 7:
        day->msgLine = 0xC4;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->faceA = 9;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0xC5;
        day->flags |= 0x100;
        SimDay_AddChange(0, 10);
        SimDay_AddChange(1, 10);
        SimDay_AddChange(2, -(SimDay_GetStat(2, 0) * 3 / 10));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 11:
        day->msgLine = -1;
        day->faceA = 5;
        day->faceB = 4;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        SimDay_Cmd(6);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0xC6;
        day->flags |= 0x80;
        day->seq = 17;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0xC3;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 16:
        day->msgLine = 0x63;
        SimDay_AddChange(0, -Rand_Range(3));
        SimDay_AddChange(1, -Rand_Range(3));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 17:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}
