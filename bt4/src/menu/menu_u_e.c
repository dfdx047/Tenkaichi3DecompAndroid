#include "menu/menu_u.h"

/* SimEvent, 0x392A30..0x392D20: gSimEvent[35]. Read-only data: its jump table, 0x3BB0C0..0x3BB104. */

/*
 * Event 35 (picture 2 twice, SE 0x23): a robbery. With items carried, one is taken (SimDay_TakeItem) and the
 * usual random loss follows; without items but with 100 points or more, a tenth of the points goes, then the
 * random loss; with neither, defence drops by 5 and the script ends without the random loss.
 */
s32 SimEv35(USimDay *day) {
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
        day->faceB = 2;
        day->faceA = 2;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(6);
        Snd_PlaySe(2, 0x23);
        day->msgLine = 0xAF;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0xCE;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0xCF;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        if (SimDay_CountItems2() == 0) {
            if (SimDay_GetStat(3, 0) >= 100) {
                day->seq = 9;
                day->seqTimer = 0;
            } else {
                day->seq = 10;
                day->seqTimer = 0;
            }
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 8:
        SimDay_TakeItem();
        day->msgLine = 0xD0;
        day->flags |= 0x80;
        day->seq = 13;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0xD1;
        SimDay_AddChange(3, -(SimDay_GetStat(3, 0) / 10));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq = 13;
        break;
    case 10:
        day->msgLine = 0xB3;
        SimDay_AddChange(1, -5);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 11:
        day->msgLine = 0xB4;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq = 16;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0xD2;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x63;
        SimDay_AddChange(0, -Rand_Range(3));
        SimDay_AddChange(1, -Rand_Range(3));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 16:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}
