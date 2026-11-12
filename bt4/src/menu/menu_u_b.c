#include "menu/menu_u.h"

/* SimEvent, 0x391430..0x391B70: the rock game (gSimEvent[32]). Read-only data 0x3BAEC0..0x3BAF5C. */

/* (an int copy of the flag byte: the bit 0 test is done on a word, as the original does) */
static inline s32 SimRock_Test(s32 flags, s32 bit) {
    return flags & bit;
}

/*
 * Event 32 (pictures 2 and 0): the rock game, at the rank of training 1. USimTrain.param rocks cross the screen
 * one after another, speed * 3 + 3 units a frame; confirm (pad 0, gamePressed 0x200) swings, and a swing whose
 * hit frame (movie trigger 0x10) finds the rock at x -412..-342 breaks it. All rocks broken = points and attack
 * (USimTrain.points / gain); otherwise USimTrain.loss on attack. Declining costs the usual random loss. After
 * the switch every frame places the rock and sets three texture windows; the movie's sound bits play SE 0x3B /
 * 0x3C. The music stops for the game (stream SE 0x10BE1) and resumes after it.
 */
s32 SimEv32(USimDay *day) {
    MFlashRef ref;
    MFlashUv uv;
    s32 done = 0;
    s32 ok = gPad[0].gamePressed & 0x200;
    MFlash *flash;

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
        day->faceA = 2;
        day->faceB = 0;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(6);
        day->msgLine = 0xB8;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0xB9;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0xBA;
        SimDay_Cmd(0x11);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = -1;
        if (day->unkB8C == 0) {
            day->seq = 19;
            day->seqTimer = 0;
        } else {
            gSimRockRank = day->rank[1];
            gSimRockSpeed = gSimTrain1[gSimRockRank].speed * 3 + 3;
            gSimRockLeft = gSimTrain1[gSimRockRank].param;
            gSimRockHits = 0;
            gSimRockX = 0;
            gSimRockFlags = 0;
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 8:
        day->msgLine = 0xBC;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        StreamSe_PlayDefault(0, 0x15E);
        Bgm_Stop();
        day->msgLine = 0xBD;
        SimDay_Cmd(0x37);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        if (day->flash[4].trig & 1) {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 11:
        if (!SimRock_Test(gSimRockFlags, 1) && ok) {
            SimDay_Cmd(0x38);
            gSimRockFlags |= 1;
            Snd_PlaySe(2, 0x21);
        }
        if (day->flash[4].trig & 0x20) {
            gSimRockFlags &= ~1;
        }
        if (day->flash[4].trig & 0x10) {
            if (gSimRockX >= -412 && gSimRockX <= -342) {
                SimDay_Cmd(0x39);
                day->seq++;
                day->seqTimer = 0;
                gSimRockFlags &= ~1;
                Snd_PlaySe(2, 0x22);
                gSimRockHits++;
            }
        }
        gSimRockX -= gSimRockSpeed;
        if (gSimRockX < -700) {
            if (gSimRockLeft == 0 || (gSimRockX = 0, --gSimRockLeft == 0)) {
                day->seq = 13;
                day->seqTimer = 0;
            }
        }
        break;
    case 12:
        if (day->flash[4].trig & 4) {
            gSimRockX = 0;
            gSimRockLeft--;
            if (gSimRockLeft == 0) {
                if (gSimRockHits == gSimTrain1[gSimRockRank].param) {
                    gSimRockFlags |= 2;
                    day->seq++;
                    day->seqTimer = 0;
                }
                day->seq = 13;
                day->seqTimer = 0;
            } else {
                SimDay_Cmd(0x3A);
                day->seq = 11;
                day->seqTimer = 0;
            }
        }
        break;
    case 13:
        if (SimRock_Test(gSimRockFlags, 2)) {
            SimDay_Cmd(0x3B);
            Snd_PlaySe(2, 0x1F);
        } else {
            SimDay_Cmd(0x3C);
            Snd_PlaySe(2, 0x20);
        }
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        if (day->flash[4].trig & 2) {
            SimDay_Cmd(0x3D);
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 15:
        if (day->flash[4].trig & 8) {
            StreamSe_Stop(0);
            SimDay_PlayBgm();
            SimDay_Cmd(0x3E);
            if (SimRock_Test(gSimRockFlags, 2)) {
                day->seq++;
                day->seqTimer = 0;
            } else {
                day->seq = 17;
                day->seqTimer = 0;
            }
        }
        break;
    case 16:
        day->msgLine = 0xBE;
        SimDay_AddChange(0, gSimTrain1[gSimRockRank].gain);
        SimDay_AddChange(3, gSimTrain1[gSimRockRank].points);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq = 18;
        break;
    case 17:
        day->msgLine = 0xC0;
        SimDay_AddChange(0, gSimTrain1[gSimRockRank].loss);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 18:
        day->msgLine = 0xBF;
        day->flags |= 0x80;
        day->seq = 22;
        day->seqTimer = 0;
        break;
    case 19:
        day->msgLine = 0xBB;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 20:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 21:
        day->msgLine = 0x63;
        SimDay_AddChange(0, -Rand_Range(3));
        SimDay_AddChange(1, -Rand_Range(3));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 22:
        day->msgLine = -1;
        done = 1;
        break;
    }

    flash = &day->flash[4];
    Flash_FindLabel(flash, 0, "mc_rock00", &ref);
    Flash_ClipSetOffset(flash, &ref, gSimRockX, 0);
    uv.x1 = 0x200;
    uv.x0 = 0;
    uv.y0 = 0x80;
    uv.y1 = 0x100;
    Flash_FindLabel(flash, 0, "mc_lose_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x1 = 0x180;
    uv.x0 = 0x100;
    uv.y0 = 0;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, 0, "mc_count_num00", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    uv.x1 = 0x100;
    uv.x0 = 0x80;
    uv.y0 = 0;
    uv.y1 = 0x80;
    Flash_FindLabel(flash, 0, "mc_count_num01", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    if (flash->se & 1) {
        Snd_PlaySe(2, 0x3B);
    }
    if (flash->se & 2) {
        Snd_PlaySe(2, 0x3C);
    }
    return done;
}
