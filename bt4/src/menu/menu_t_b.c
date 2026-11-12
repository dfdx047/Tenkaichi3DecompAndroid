#include "common.h"
#include "menu/menu_t.h"
#include "sys/pad.h"

/*
 * SimEvent, 0x3900D0..0x3911A8: the card game (gSimEvent[28]) with its four helpers, then gSimEvent[29..30].
 * A new object starts at 0x3900D0: its read-only data begins at 0x3BACF0, 12 bytes behind the jump table of
 * event 27 (strings are only 8-byte aligned, so the gap is the linker's 16-byte object alignment). Whether
 * events 29 and 30 are in this file or in a third one cannot be told from the layout (their read-only data is
 * jump tables only); they are kept here.
 *
 * The scripts follow the pattern described in menu_t.c.
 */

/* Plays "fl_select_in" on the card under the cursor. */
void SimEv28_SelectCard(TSimDay *day) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = &day->flash[SIMEV_FLASH_CARD];

    sprintf(name, "mc_card_anime%02d", day->cur[3]);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_select_in");
}

/* Plays "fl_off" on the card under the cursor. */
void SimEv28_UnselectCard(TSimDay *day) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = &day->flash[SIMEV_FLASH_CARD];

    sprintf(name, "mc_card_anime%02d", day->cur[3]);
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_off");
}

/* Next place left (dir 0) or right of the cursor whose card was not picked yet, wrapping; -1 = none. */
s32 SimEv28_MoveCursor(TSimDay *day, s32 dir) {
    s32 n = gSimTrain0[gSimCardRank].param - 1;
    s32 cur = day->cur[day->state];
    s32 i;

    for (i = 0; i < n; i++) {
        if (dir != 0) {
            cur++;
            if (cur > n) {
                cur = 0;
            }
        } else {
            cur--;
            if (cur < 0) {
                cur = n;
            }
        }
        if (!((gSimCardPicked >> cur) & 1)) {
            return cur;
        }
    }
    return -1;
}

/* Deals n cards: shuffles 0..9 with libc rand(), keeps the first n as the places and sorts them for the order to pick in. */
void SimEv28_Deal(s32 n) {
    s32 i, j, r, t;

    for (i = 9; i >= 0; i--) {
        gSimCardOrder[i] = i;
    }
    for (i = 0; i < 10; i++) {
        r = rand() % 10;
        t = gSimCardOrder[i];
        gSimCardOrder[i] = gSimCardOrder[r];
        gSimCardOrder[r] = t;
    }
    memcpy(gSimCardShown, gSimCardOrder, n * 4);
    for (i = 0; i < n; i++) {
        for (j = n - 1; j > i; j--) {
            if (gSimCardOrder[j] < gSimCardOrder[j - 1]) {
                t = gSimCardOrder[j];
                gSimCardOrder[j] = gSimCardOrder[j - 1];
                gSimCardOrder[j - 1] = t;
            }
        }
    }
    for (i = 0; i < n; i++) {
    }
    gSimCardNext = 0;
    gSimCardPicked = 0;
    gSimCardWrong = 0;
}

/*
 * Random event 23: picture 1 asks; yes = the card game at the rank of training 0. The cards (param of them, out of
 * 0..9) lie face up for `seconds`, then are picked one by one; each pick must be the lowest card left. All right =
 * points and attack (TSimTrain.points / gain); one wrong pick ends the game with TSimTrain.loss on attack. After
 * the switch every frame sets the picture of each card and the cross of a wrong pick. Pad 0: left / right
 * (gameRepeat 1 / 2), confirm (gamePressed 0x200).
 *
 * Toolchain note: in case 8 the original has `lw $a0,%lo(gSimTrain0)($a0) / jal SimEv28_Deal / nop`. Sony's
 * assembler did not give the last instruction of a `symbol(reg)` load to the delay slot when the symbol was not
 * defined yet (it deferred the small-data decision even under -G0); the modern gas under -G0 does. This is the
 * only such site in the overlay, so this file alone is ASSEMBLED with -G8 (the compiler still gets -G0): see
 * AS_G_FLAGS in configure.py and scripts/fdiff.py. That is only safe because the file has no float constants.
 */
s32 SimEv28(TSimDay *day) {
    char name[0x40];
    MFlashRef ref;
    MFlashUv uv;
    MFlash *flash;
    s32 i;
    s32 cur;
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 left = gPad[0].gameRepeat & 1;
    s32 right = gPad[0].gameRepeat & 2;

    switch (day->seq) {
    case 0:
        day->faceA = 11;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x62;
        Snd_PlaySe(2, 0x17);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 2:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_SURPRISE);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        day->faceA = 1;
        day->faceB = 1;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x92;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x9E;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = -1;
        if (day->answer == 0) {
            day->seq = 23;
            day->seqTimer = 0;
        } else {
            gSimCardRank = day->rank[0];
            day->seqTimer = 0;
            day->seq++;
        }
        break;
    case 7:
        day->msgLine = 0xA0;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        StreamSe_PlayDefault(0, 0x15E);
        Bgm_Stop();
        SimEv28_Deal(gSimTrain0[gSimCardRank].param);
        day->msgLine = 0xA1;
        SimDay_Cmd(0x24); /* the card movie starts */
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        if (day->flash[SIMEV_FLASH_CARD].trig & 1) {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 10:
        SimDay_Cmd(0x25); /* the cards turn face up */
        Snd_PlaySe(2, 4);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        if (day->flash[SIMEV_FLASH_CARD].trig & 8) {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 12:
        if (++day->seqTimer > gSimTrain0[gSimCardRank].seconds * 60) {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 13:
        SimDay_Cmd(0x26); /* and face down again */
        Snd_PlaySe(2, 4);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        if (day->flash[SIMEV_FLASH_CARD].trig & 0x10) {
            SimEv28_SelectCard(day);
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 15:
        if (gSimCardNext == gSimTrain0[gSimCardRank].param || gSimCardNext == -1) {
            if (++day->seqTimer > 60) {
                day->seq = 17;
                day->seqTimer = 0;
            }
        } else if (ok) {
            gSimCardPicked |= 1 << day->cur[day->state];
            SimDay_Cmd(0x27); /* the picked card turns over */
            Snd_PlaySe(2, 4);
            if (gSimCardShown[day->cur[day->state]] == gSimCardOrder[gSimCardNext]) {
                gSimCardNext++;
            } else {
                gSimCardNext = -1;
                gSimCardWrong |= 1 << day->cur[day->state];
            }
            day->seq++;
            day->seqTimer = 0;
        } else if (left) {
            cur = SimEv28_MoveCursor(day, 0);
            if (cur >= 0) {
                SimEv28_UnselectCard(day);
                day->cur[day->state] = cur;
                SimEv28_SelectCard(day);
            }
        } else if (right) {
            cur = SimEv28_MoveCursor(day, 1);
            if (cur >= 0) {
                SimEv28_UnselectCard(day);
                day->cur[day->state] = cur;
                SimEv28_SelectCard(day);
            }
        }
        break;
    case 16:
        if (day->flash[SIMEV_FLASH_CARD].trig & 4) {
            if (gSimCardNext != -1) {
                s32 place = SimEv28_MoveCursor(day, 1);

                if (place >= 0) {
                    day->cur[day->state] = place;
                    SimEv28_SelectCard(day);
                }
            }
            day->seq = 15;
            day->seqTimer = 0;
        }
        break;
    case 17:
        if (gSimCardWrong != 0) {
            SimDay_Cmd(0x29); /* "mc_lose_text" */
            Snd_PlaySe(2, 0x20);
        } else {
            SimDay_Cmd(0x28); /* "mc_win_text" */
            Snd_PlaySe(2, 0x1F);
        }
        day->seq++;
        day->seqTimer = 0;
        break;
    case 18:
        if (day->flash[SIMEV_FLASH_CARD].trig & 2) {
            StreamSe_Stop(0);
            SimDay_PlayBgm();
            SimDay_Cmd(0x2A); /* the cards leave */
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 19:
        if (day->flash[SIMEV_FLASH_CARD].trig & 0x20) {
            SimDay_Cmd(0x2B); /* the card movie stops */
            if (gSimCardWrong != 0) {
                day->seq = 21;
                day->seqTimer = 0;
            } else {
                day->seq++;
                day->seqTimer = 0;
            }
        }
        break;
    case 20:
        day->msgLine = 0xA2;
        SimDay_AddChange(SIMEV_STAT_POINT, gSimTrain0[gSimCardRank].points);
        SimDay_AddChange(SIMEV_STAT_ATK, gSimTrain0[gSimCardRank].gain);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq = 22;
        break;
    case 21:
        day->msgLine = 0xA3;
        SimDay_AddChange(SIMEV_STAT_ATK, gSimTrain0[gSimCardRank].loss);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 22:
        day->msgLine = 0x9D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 26;
        day->seqTimer = 0;
        break;
    case 23:
        day->msgLine = 0x9F;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 24:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 25:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 26:
        day->msgLine = -1;
        return 1;
    }
    flash = &day->flash[SIMEV_FLASH_CARD];
    for (i = 0; i < gSimTrain0[gSimCardRank].param; i++) {
        s32 x;
        s32 y;

        sprintf(name, "mc_card_anime%02d", i);
        x = gSimCardShown[i] % 4;
        y = gSimCardShown[i] / 4;
        x *= 64;
        y *= 64;
        uv.x0 = x;
        uv.y0 = y;
        uv.x1 = x + 64;
        uv.y1 = y + 64;
        Flash_FindLabel(flash, name, "mc_card_num", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if ((gSimCardWrong >> i) & 1) {
            uv.x0 = 64;
            uv.y0 = 0;
            uv.x1 = 128;
            uv.y1 = 64;
            Flash_FindLabel(flash, name, "mc_maru_batsu", &ref);
            Flash_ClipSetUv(flash, &ref, &uv);
        }
    }
    uv.x0 = 0;
    uv.y0 = 0x80;
    uv.x1 = 0x200;
    uv.y1 = 0x100;
    Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (flash->se & 1) {
        Snd_PlaySe(2, 0x3C);
    }
    return 0;
}

/* Random event 24: pictures 8 / 9 ask; yes = attack + 15, defence + 15 and a level-up (command 33). */
s32 SimEv29(TSimDay *day) {
    switch (day->seq) {
    case 0:
        day->faceA = 11;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x62;
        Snd_PlaySe(2, 0x17);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 2:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_SURPRISE);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        day->faceA = 8;
        day->faceB = 9;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0xA4;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0xA5;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0xA6;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = 0xA7;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 18;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0xA9;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0xAA;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0xAB;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = 0x98;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0x98;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0xAC;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0xAD;
        day->flags |= SIMEV_FLAG200 | SIMEV_LEVEL_UP;
        SimDay_AddChange(SIMEV_STAT_ATK, 15);
        SimDay_AddChange(SIMEV_STAT_DEF, 15);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 16:
        SimDay_Cmd(SIMEV_CMD_LEVEL_UP);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 17:
        day->msgLine = 0xAE;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 21;
        day->seqTimer = 0;
        break;
    case 18:
        day->msgLine = 0xA8;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 19:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 20:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 21:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 25: pictures 6 / 3 ask; yes = picture 2 comes and defence - 5; no = 1000 points. */
s32 SimEv30(TSimDay *day) {
    switch (day->seq) {
    case 0:
        day->faceA = 11;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x62;
        Snd_PlaySe(2, 0x17);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 2:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_SURPRISE);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        day->faceB = 3;
        day->faceA = 6;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0xAF;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0xB0;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = -1;
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 13;
            day->seqTimer = 0;
        }
        break;
    case 7:
        day->msgLine = 0xB1;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->faceB = 2;
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0xB2;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0xB3;
        SimDay_AddChange(SIMEV_STAT_DEF, -5);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 12:
        day->msgLine = 0xB4;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 17;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0xB5;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0xB6;
        SimDay_AddChange(SIMEV_STAT_POINT, 1000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 15:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 16:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 17:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}
