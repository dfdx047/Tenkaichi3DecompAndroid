#include "common.h"
#include "menu/menu_u.h"

/*
 * SimEvent, 0x391E28..0x392A30: the shell game (gSimEvent[34]) with its three helpers. One source file: its
 * read-only data (0x3BAFB0..0x3BB0C0) starts on the linker's 16-byte object boundary behind the jump table of
 * event 33 and repeats a string of event 32's file ("mc_lose_text").
 */

/* While SimEv34 is INCLUDE_ASM its jump table comes from the assembly file, which only aligns it to 8: the
   original object's .rodata was 16-aligned (it starts at 0x3BAFB0, 8 bytes behind the previous object). */
RODATA_ALIGN16();

static const s32 sSimPopoX[5] = { 76, 150, 224, 298, 372 };
static const s32 sSimPopoSpeed[3] = { 6, 10, 14 };
/* A named object because SimEv34 (INCLUDE_ASM) uses it too; it sits where the first literal of the pool was. */
static const char sSimPopoClip[] __attribute__((aligned(8))) = "mc_popo%02d";

/* Swaps two of the five figures: down, across, up. Returns 1 when the swap is over. */
s32 SimPopo_Swap(USimDay *day, s32 a, s32 b) {
    MFlashRef ref;
    char name[64];
    s32 done = 0;
    s32 d;
    MFlash *flash;

    switch (gSimPopoState) {
    case 0:
        gSimPopoDy -= sSimPopoSpeed[gSimPopoSpeed];
        if (gSimPopoDy <= -110) {
            gSimPopoState = 1;
            gSimPopoDy = -110;
        }
        break;
    case 1:
        d = sSimPopoX[b] - sSimPopoX[a];
        gSimPopoDx += sSimPopoSpeed[gSimPopoSpeed];
        if (gSimPopoDx >= d) {
            gSimPopoDx = d;
            gSimPopoState = 2;
        }
        break;
    case 2:
        gSimPopoDy += sSimPopoSpeed[gSimPopoSpeed];
        if (gSimPopoDy >= 0) {
            gSimPopoDy = 0;
            gSimPopoState = 3;
        }
        break;
    case 3:
        done = 1;
        gSimPopoState = 0;
        gSimPopoDy = 0;
        gSimPopoDx = 0;
        if (a == gSimPopoTarget) {
            gSimPopoTarget = b;
        } else if (b == gSimPopoTarget) {
            gSimPopoTarget = a;
        }
        break;
    }
    flash = &day->flash[3];
    sprintf(name, sSimPopoClip, a);
    Flash_FindLabel(flash, 0, name, &ref);
    Flash_ClipSetOffset(flash, &ref, gSimPopoDx, gSimPopoDy);
    sprintf(name, sSimPopoClip, b);
    Flash_FindLabel(flash, 0, name, &ref);
    Flash_ClipSetOffset(flash, &ref, -gSimPopoDx, gSimPopoDy);
    return done;
}

/* Lights the figure under the cursor. */
void SimPopo_CursorOn(USimDay *day) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &day->flash[3];

    sprintf(name, sSimPopoClip, day->cur[3]);
    Flash_FindLabel(flash, 0, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_select_loop");
}

/* Puts out the figure under the cursor. */
void SimPopo_CursorOff(USimDay *day) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &day->flash[3];

    sprintf(name, sSimPopoClip, day->cur[3]);
    Flash_FindLabel(flash, 0, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, "fl_select_off");
}

/*
 * Event 34: the shell game. One of five figures holds the mark; it is shown, the figures are swapped in pairs
 * (param swaps at one of three speeds), then the player picks one: left / right (gameRepeat 1 / 2), confirm
 * (gamePressed 0x200). Right = points and defence (USimTrain.points / gain), wrong = USimTrain.loss on defence.
 * After the switch every frame places the mark and sets each figure's pictures.
 *
 * NOT MATCHING: 15 of 551 instructions, all in case 6 (0x3922EC..0x392328): the same instructions with other
 * registers and another order of the five stores (the original keeps the Rand_Range result in $v0 until the index
 * arithmetic is done and gives the three other address registers $a2 / $a3 / $a1; this attempt stores the result
 * at once). Every statement order, a temporary for the result, a split load of the speed and chained zero stores
 * were tried (build/scratch_menu_u/perm3.py .. perm6.py). Everything else, including the jump table and the
 * strings, is identical with every relocation applied (build/scratch_menu_u/linkcheck.py). The attempt is
 * behaviourally exact. Verified by enabling it; fdiff then reports the 15 instructions.
 *
 * Cleanup notes (build/scratch_cleanup2_I/ev*.py, rtl/ud.*): the high halves of the five addresses are not
 * local to case 6. The global CSE pass makes `%hi(gSimPopoTarget)` one register for the whole function and
 * copies it into every case that reaches the code behind the switch, so the place of that `lui` in case 6
 * follows from the whole function, and a change in case 6 moves registers in case 16 (0x392450). With the
 * result in a temporary and the order Count, Speed, Won, Target the three other `lui` come out as in the
 * original (a2 / a3 / a1, in that order) but the store of the target then follows the two zero stores (10
 * instructions, two of them in case 16); chained zero stores and a pointer to the table row change nothing.
 * The original has the target's `lui` last and its store first.
 */
#if 0
s32 SimEv34(USimDay *day) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    s32 done = 0;
    s32 left = gPad[0].gameRepeat & 1;
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 right = gPad[0].gameRepeat & 2;
    MFlash *flash;
    s32 i;

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
        day->msgLine = 0xC7;
        SimDay_Cmd(0x11);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        if (day->unkB8C == 0) {
            day->seq = 24;
            day->seqTimer = 0;
        } else {
            gSimPopoRank = day->rank[2];
            gSimPopoTarget = Rand_Range(5);
            gSimPopoCount = 0;
            gSimPopoWon = 0;
            gSimPopoSpeed = gSimTrain2[gSimPopoRank].speed;
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 7:
        day->msgLine = 0xC9;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        StreamSe_PlayDefault(0, 0x15E);
        Bgm_Stop();
        day->msgLine = 0xCA;
        SimDay_Cmd(0x2D);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        if (day->flash[3].trig & 1) {
            SimDay_Cmd(0x2E);
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 10:
        if (++day->seqTimer > 120) {
            SimDay_Cmd(0x2F);
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 11:
        if (++day->seqTimer > 10) {
            SimDay_Cmd(0x30);
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 12:
        if (day->flash[3].trig & 0x10) {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 13:
        if (++day->seqTimer > 60) {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 14:
        if (gSimPopoCount >= gSimTrain2[gSimPopoRank].param) {
            day->seq = 16;
            day->seqTimer = 0;
            SimPopo_CursorOn(day);
        } else {
            if (Rand_Range(0x100) < 0xAA) {
                gSimPopoA = Rand_Range(5);
            } else {
                gSimPopoA = gSimPopoTarget;
            }
            do {
                gSimPopoB = Rand_Range(5);
            } while (gSimPopoA == gSimPopoB || gSimPopoB == gSimPopoTarget);
            if (gSimPopoB < gSimPopoA) {
                s32 tmp = gSimPopoA;

                gSimPopoA = gSimPopoB;
                gSimPopoB = tmp;
            }
            day->seq++;
            day->seqTimer = 0;
            Snd_PlaySe(2, 0x1E);
            gSimPopoCount++;
        }
        break;
    case 15:
        if (SimPopo_Swap(day, gSimPopoA, gSimPopoB)) {
            day->seq = 14;
            day->seqTimer = 0;
        }
        break;
    case 16:
        if (ok) {
            if (day->cur[day->state] == gSimPopoTarget) {
                gSimPopoWon = 1;
            }
            SimDay_Cmd(0x31);
            day->seq++;
            day->seqTimer = 0;
        } else if (left) {
            SimPopo_CursorOff(day);
            day->cur[day->state]--;
            if (day->cur[day->state] < 0) {
                day->cur[day->state] = 4;
            }
            SimPopo_CursorOn(day);
        } else if (right) {
            SimPopo_CursorOff(day);
            day->cur[day->state]++;
            if (day->cur[day->state] >= 5) {
                day->cur[day->state] = 0;
            }
            SimPopo_CursorOn(day);
        }
        break;
    case 17:
        if (day->flash[3].trig & 4) {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 18:
        if (++day->seqTimer > 30) {
            if (gSimPopoWon) {
                SimDay_Cmd(0x32);
                Snd_PlaySe(2, 0x1F);
            } else {
                SimDay_Cmd(0x33);
                Snd_PlaySe(2, 0x20);
            }
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 19:
        if (day->flash[3].trig & 2) {
            StreamSe_Stop(0);
            SimDay_PlayBgm();
            SimDay_Cmd(0x34);
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 20:
        if (day->flash[3].trig & 0x20) {
            SimDay_Cmd(0x35);
            if (gSimPopoWon) {
                day->seq++;
                day->seqTimer = 0;
            } else {
                day->seq = 23;
                day->seqTimer = 0;
            }
        }
        break;
    case 21:
        day->msgLine = 0xCB;
        SimDay_AddChange(1, gSimTrain2[gSimPopoRank].gain);
        SimDay_AddChange(3, gSimTrain2[gSimPopoRank].points);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 22:
        day->msgLine = 0xCC;
        day->flags |= 0x80;
        day->seq = 27;
        day->seqTimer = 0;
        break;
    case 23:
        day->msgLine = 0xCD;
        SimDay_AddChange(1, gSimTrain2[gSimPopoRank].loss);
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq = 27;
        break;
    case 24:
        day->msgLine = 0xC8;
        day->flags |= 0x80;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 25:
        day->msgLine = -1;
        SimDay_Cmd(7);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 26:
        day->msgLine = 0x63;
        SimDay_AddChange(0, -Rand_Range(3));
        SimDay_AddChange(1, -Rand_Range(3));
        SimDay_Cmd(0x1A);
        day->flags |= 0x80;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 27:
        done = 1;
        day->msgLine = -1;
        break;
    }

    flash = &day->flash[3];
    Flash_FindLabel(flash, 0, "mc_fukidasi_popo", &ref);
    Flash_ClipSetOffset(flash, &ref, sSimPopoX[gSimPopoTarget], 0);
    uv.y0 = 0x80;
    uv.x1 = 0x200;
    uv.y1 = 0x100;
    uv.x0 = 0;
    Flash_FindLabel(flash, 0, "mc_lose_text", &ref);
    Flash_ClipSetUv(flash, &ref, &uv);
    for (i = 0; i < 5; i++) {
        if (i != gSimPopoTarget) {
            uv.x0 = 0x40;
            uv.y0 = 0;
            uv.x1 = 0x80;
            uv.y1 = 0x40;
        } else {
            uv.x0 = 0;
            uv.y0 = 0;
            uv.x1 = 0x40;
            uv.y1 = 0x40;
        }
        sprintf(name, sSimPopoClip, i);
        Flash_FindLabel(flash, name, "mc_maru_batsu", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (i != gSimPopoTarget) {
            uv.x0 = 0;
            uv.y0 = 0;
            uv.x1 = 0x40;
            uv.y1 = 0x80;
        } else {
            uv.x0 = 0x40;
            uv.y0 = 0;
            uv.x1 = 0x80;
            uv.y1 = 0x80;
        }
        Flash_FindLabel(flash, name, "mc_popo_anime00", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
    }
    if (flash->se & 1) {
        Snd_PlaySe(2, 0x3C);
    }
    return done;
}
#else
INCLUDE_ASM("asm/nonmatchings/menu/menu_u_d", SimEv34);
#endif
