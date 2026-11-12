#include "common.h"
#include "menu/menu_t.h"

/*
 * SimEvent handlers, 0x38C360..0x38CB38: gSimEvent[0..2] (table 0x3B7388, run by SimEvent_Run of
 * src/menu/menu_r_b.c), the three trainings of the sim day screen, each behind the roll of its outcome. The
 * object goes on in the next chunk (menu_t: SimEv03 at 0x38CB38 and the rest of the 37 handlers). These six
 * functions emit no read-only data.
 *
 * A handler is called once a frame with the day screen's work area until it returns 1; day->seq is its
 * position. The three are one piece of source instantiated per training: step 0 rolls the outcome and puts
 * picture N on the monitor, step 1 shows line 0x59 with one of two sounds, step 2 shows the outcome's line
 * (0x5A + outcome) and queues the changes of attack, defence and health, step 3 ends the event.
 */

/* Rolls the outcome (0..5) of training 0: one draw of 0..100 against the weights of the repeat count. */
s32 SimEv00_Roll(TSimDay *day) {
    s32 r = Rand_Range(101);
    s32 sum;
    s32 i;

    if (day->flags & SIMEV_FLAG1000) {
        return 5;
    }
    if (day->flags & SIMEV_FLAG800) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < SIMTRAIN_OUTCOMES; i++) {
        sum += day->train[0].weight[day->repeat][i];
        if (r < sum) {
            return i;
        }
    }
    return 5;
}

/* gSimEvent[0]: training 0. */
s32 SimEv00(TSimDay *day) {
    s32 atk[2];
    s32 def[2];
    s32 hp;

    switch (day->seq) {
    case 0:
        gSimTrainOutcome0 = SimEv00_Roll(day);
        day->faceA = 1;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x59;
        if (Rand_Range(2)) {
            Snd_PlaySe(2, 0x15);
        } else {
            Snd_PlaySe(2, 0x16);
        }
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 2:
        day->msgLine = gSimTrainOutcome0 + 0x5A;
        atk[0] = day->train[0].atkMin[gSimTrainOutcome0];
        atk[1] = day->train[0].atkMax[gSimTrainOutcome0];
        def[0] = day->train[0].defMin[gSimTrainOutcome0];
        def[1] = day->train[0].defMax[gSimTrainOutcome0];
        hp = day->train[0].hp[gSimTrainOutcome0];
        if (atk[0] == atk[1]) {
            SimDay_AddChange(0, atk[0]);
        } else {
            SimDay_AddChange(0, Rand_Range(atk[1] - atk[0] + 1) + atk[0]);
        }
        if (def[0] == def[1]) {
            SimDay_AddChange(1, def[0]);
        } else {
            SimDay_AddChange(1, Rand_Range(def[1] - def[0] + 1) + def[0]);
        }
        SimDay_AddChange(2, hp);
        SimDay_Cmd(0x1A);
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Rolls the outcome (0..5) of training 1: one draw of 0..100 against the weights of the repeat count. */
s32 SimEv01_Roll(TSimDay *day) {
    s32 r = Rand_Range(101);
    s32 sum;
    s32 i;

    if (day->flags & SIMEV_FLAG1000) {
        return 5;
    }
    if (day->flags & SIMEV_FLAG800) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < SIMTRAIN_OUTCOMES; i++) {
        sum += day->train[1].weight[day->repeat][i];
        if (r < sum) {
            return i;
        }
    }
    return 5;
}

/* gSimEvent[1]: training 1. */
s32 SimEv01(TSimDay *day) {
    s32 atk[2];
    s32 def[2];
    s32 hp;

    switch (day->seq) {
    case 0:
        gSimTrainOutcome1 = SimEv01_Roll(day);
        day->faceA = 2;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x59;
        if (Rand_Range(2)) {
            Snd_PlaySe(2, 0x15);
        } else {
            Snd_PlaySe(2, 0x16);
        }
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 2:
        day->msgLine = gSimTrainOutcome1 + 0x5A;
        atk[0] = day->train[1].atkMin[gSimTrainOutcome1];
        atk[1] = day->train[1].atkMax[gSimTrainOutcome1];
        def[0] = day->train[1].defMin[gSimTrainOutcome1];
        def[1] = day->train[1].defMax[gSimTrainOutcome1];
        hp = day->train[1].hp[gSimTrainOutcome1];
        if (atk[0] == atk[1]) {
            SimDay_AddChange(0, atk[0]);
        } else {
            SimDay_AddChange(0, Rand_Range(atk[1] - atk[0] + 1) + atk[0]);
        }
        if (def[0] == def[1]) {
            SimDay_AddChange(1, def[0]);
        } else {
            SimDay_AddChange(1, Rand_Range(def[1] - def[0] + 1) + def[0]);
        }
        SimDay_AddChange(2, hp);
        SimDay_Cmd(0x1A);
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Rolls the outcome (0..5) of training 2: one draw of 0..100 against the weights of the repeat count. */
s32 SimEv02_Roll(TSimDay *day) {
    s32 r = Rand_Range(101);
    s32 sum;
    s32 i;

    if (day->flags & SIMEV_FLAG1000) {
        return 5;
    }
    if (day->flags & SIMEV_FLAG800) {
        return 0;
    }
    sum = 0;
    for (i = 0; i < SIMTRAIN_OUTCOMES; i++) {
        sum += day->train[2].weight[day->repeat][i];
        if (r < sum) {
            return i;
        }
    }
    return 5;
}

/* gSimEvent[2]: training 2. */
s32 SimEv02(TSimDay *day) {
    s32 atk[2];
    s32 def[2];
    s32 hp;

    switch (day->seq) {
    case 0:
        gSimTrainOutcome2 = SimEv02_Roll(day);
        day->faceA = 3;
        SimDay_Cmd(5);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x59;
        if (Rand_Range(2)) {
            Snd_PlaySe(2, 0x15);
        } else {
            Snd_PlaySe(2, 0x16);
        }
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 2:
        day->msgLine = gSimTrainOutcome2 + 0x5A;
        atk[0] = day->train[2].atkMin[gSimTrainOutcome2];
        atk[1] = day->train[2].atkMax[gSimTrainOutcome2];
        def[0] = day->train[2].defMin[gSimTrainOutcome2];
        def[1] = day->train[2].defMax[gSimTrainOutcome2];
        hp = day->train[2].hp[gSimTrainOutcome2];
        if (atk[0] == atk[1]) {
            SimDay_AddChange(0, atk[0]);
        } else {
            SimDay_AddChange(0, Rand_Range(atk[1] - atk[0] + 1) + atk[0]);
        }
        if (def[0] == def[1]) {
            SimDay_AddChange(1, def[0]);
        } else {
            SimDay_AddChange(1, Rand_Range(def[1] - def[0] + 1) + def[0]);
        }
        SimDay_AddChange(2, hp);
        SimDay_Cmd(0x1A);
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/*
 * Merged (integration step 11): this file is menu_s_d.c (gSimEvent[0..2], 0x38C360..0x38CB38) followed by the
 * former menu_t.c (gSimEvent[3..27], 0x38CB38..0x3900D0): one object, 0x38C360..0x3900D0. Both halves now use
 * the TSimDay view of include/menu/menu_t.h (the head's SimDayS of menu_s.h is gone: step -> seq, wait ->
 * seqTimer, SIMDAY_* -> SIMEV_*). The two header comments are kept as written.
 */

/*
 * SimEvent, 0x38CB38..0x3900D0: gSimEvent[3..27], day events of the "sim" day screen (mode 22). One source file
 * with menu_s_d.c (gSimEvent[0..2], from 0x38C360) in front; the object ends at 0x3900D0, where the card game's
 * read-only data starts on a new 16-byte boundary (menu_t_b.c).
 *
 * A handler is a script: `day->seq` is the step, run once a frame; a step that sets SIMEV_WAIT_KEY holds the
 * script until the confirm button. Common steps: 0 monitor off, 1 "!?" (line 0x62, sound 0x17), 2 the surprise
 * icon, 3 the two pictures, 4 the character comes in; near the end the character leaves (command 7) and the
 * turn's price is paid (line 0x63: attack and defence - Rand_Range(3) each); the last step returns 1.
 * Every random amount is Rand_Range (the shared Mersenne Twister); only the card shuffle uses libc rand().
 */

/* Board row 2: after 61 frames gives back half the health + 5 (line 0x60), then takes 1 attack and 1 defence (line 0x63). */
s32 SimEv03(TSimDay *day) {
    switch (day->seq) {
    case 0:
        day->faceA = 10;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        Snd_PlaySe(2, 0x18);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 2:
        if (++day->seqTimer > 60) {
            day->msgLine = 0x60;
            SimDay_AddChange(SIMEV_STAT_HP, SimDay_GetStat(SIMEV_STAT_HP, 0) / 2 + 5);
            SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
            day->flags |= SIMEV_WAIT_KEY;
            day->seqTimer = 0;
            day->seq++;
        }
        break;
    case 3:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, ~Rand_Range(1));
        SimDay_AddChange(SIMEV_STAT_DEF, ~Rand_Range(1));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 4:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Board row 3: health back to 100 (line 0x61). */
s32 SimEv04(TSimDay *day) {
    switch (day->seq) {
    case 0:
        day->faceA = 10;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 1:
        day->msgLine = 0x61;
        SimDay_AddChange(SIMEV_STAT_HP, 100 - SimDay_GetStat(SIMEV_STAT_HP, 0));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 2:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 0: nothing is found (lines 0x62, 0x64); attack and defence drop by 0..2 each. */
s32 SimEv05(TSimDay *day) {
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
        day->msgLine = 0x64;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 4:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 1: a character (picture Rand_Range(12)) gives Rand_Range(turn) * 100 + 100 points. */
s32 SimEv06(TSimDay *day) {
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
        day->faceA = Rand_Range(12);
        day->faceB = 11;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x65;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x66;
        SimDay_AddChange(SIMEV_STAT_POINT, Rand_Range(TPROG->turn) * 100 + 100);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->seq++;
        day->seqTimer = 0;
        day->flags |= SIMEV_WAIT_KEY;
        break;
    case 7:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 9:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 2: a character offers an item (yes / no window); yes = SimDay_GiveItem. */
s32 SimEv07(TSimDay *day) {
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
        day->faceA = Rand_Range(12);
        day->faceB = 10;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x67;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->msgLine = 0x68;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
            day->seq = 8;
            day->seqTimer = 0;
        }
        break;
    case 7:
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        SimDay_GiveItem();
        day->msgLine = 0x69;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 9:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 3: pictures 0 / 7; a fifth of the health comes back. */
s32 SimEv08(TSimDay *day) {
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
        day->faceB = 7;
        day->faceA = 0;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x6A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        day->msgLine = 0x6B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        SimDay_AddChange(SIMEV_STAT_HP, SimDay_GetStat(SIMEV_STAT_HP, 0) / 5);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x6C;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x6E;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 9:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 4: as event 3, and attack + 1..5; no closing penalty. */
s32 SimEv09(TSimDay *day) {
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
        day->faceB = 7;
        day->faceA = 0;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x6A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        day->msgLine = 0x6B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        SimDay_AddChange(SIMEV_STAT_HP, SimDay_GetStat(SIMEV_STAT_HP, 0) / 5);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x6C;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_AddChange(SIMEV_STAT_ATK, Rand_Range(5) + 1);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x6D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = 0x6E;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 5: as event 3, and defence + 1..5; no closing penalty. */
s32 SimEv10(TSimDay *day) {
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
        day->faceB = 7;
        day->faceA = 0;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x6A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        day->msgLine = 0x6B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        SimDay_AddChange(SIMEV_STAT_HP, SimDay_GetStat(SIMEV_STAT_HP, 0) / 5);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x6C;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_AddChange(SIMEV_STAT_DEF, Rand_Range(5) + 1);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x6D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = 0x6E;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 6: as event 3, and attack and defence + 1..5 each; no closing penalty. */
s32 SimEv11(TSimDay *day) {
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
        day->faceB = 7;
        day->faceA = 0;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 3:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x6A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        day->msgLine = 0x6B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        SimDay_AddChange(SIMEV_STAT_HP, SimDay_GetStat(SIMEV_STAT_HP, 0) / 5);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x6C;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_AddChange(SIMEV_STAT_ATK, Rand_Range(5) + 1);
        SimDay_AddChange(SIMEV_STAT_DEF, Rand_Range(5) + 1);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x6D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = 0x6E;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 7: pictures 0 / 7; costs 5..20 % of the health, pays 1000..5000 points. */
s32 SimEv12(TSimDay *day) {
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
        day->faceA = 0;
        day->faceB = 7;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x6A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x6F;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x70;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = 0x71;
        SimDay_AddChange(SIMEV_STAT_HP, -(SimDay_GetStat(SIMEV_STAT_HP, 0) * (s32)(Rand_Range(16) + 5) / 100));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 8:
        day->msgLine = 0x66;
        SimDay_AddChange(SIMEV_STAT_POINT, Rand_Range(4001) + 1000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 9:
        day->msgLine = 0x72;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 12:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 8: pictures 4 / 6 offer a deal for 10000 points (line 0xD7 when short of points); this one gives nothing. */
s32 SimEv13(TSimDay *day) {
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
        day->faceA = 4;
        day->faceB = 6;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x73;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x74;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x75;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = -1;
        if (day->answer == 0) {
            day->seq = 14;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 8:
        if (SimDay_GetStat(SIMEV_STAT_POINT, 0) < 10000) {
            day->seq = 13;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0x76;
        SimDay_AddChange(SIMEV_STAT_POINT, -10000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        Snd_PlaySe(2, 0x1D);
        day->msgLine = 0x78;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        Snd_PlaySe(2, 0x22);
        day->msgLine = 0x79;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = 0x7A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 15;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0xD7;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0x77;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
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

/* Random event 9: the 10000-point deal that pays 30000 points. */
s32 SimEv14(TSimDay *day) {
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
        day->faceA = 4;
        day->faceB = 6;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x73;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x74;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x75;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = -1;
        if (day->answer == 0) {
            day->seq = 15;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 8:
        if (SimDay_GetStat(SIMEV_STAT_POINT, 0) < 10000) {
            day->seq = 14;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0x76;
        SimDay_AddChange(SIMEV_STAT_POINT, -10000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        Snd_PlaySe(2, 0x1D);
        day->msgLine = 0x78;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        Snd_PlaySe(2, 0x1F);
        day->msgLine = 0x81;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = 0x66;
        SimDay_AddChange(SIMEV_STAT_POINT, 30000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 13:
        day->msgLine = 0x7D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 16;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0xD7;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x77;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 16:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 17:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 18:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 10: the 10000-point deal that gives attack + 5 and defence + 5 (it ends without the closing penalty). */
s32 SimEv15(TSimDay *day) {
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
        day->faceA = 4;
        day->faceB = 6;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x73;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x74;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x75;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = -1;
        if (day->answer == 0) {
            day->seq = 15;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 8:
        if (SimDay_GetStat(SIMEV_STAT_POINT, 0) < 10000) {
            day->seq = 14;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0x76;
        SimDay_AddChange(SIMEV_STAT_POINT, -10000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        Snd_PlaySe(2, 0x1D);
        day->msgLine = 0x78;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        Snd_PlaySe(2, 0x1F);
        day->msgLine = 0x7E;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = 0x7F;
        SimDay_AddChange(SIMEV_STAT_ATK, 5);
        SimDay_AddChange(SIMEV_STAT_DEF, 5);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 13:
        day->msgLine = 0x7D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 18;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0xD7;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x77;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 16:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 17:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 18:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 11: the 10000-point deal that offers an item (a second yes / no window). */
s32 SimEv16(TSimDay *day) {
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
        day->faceA = 4;
        day->faceB = 6;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x73;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x74;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x75;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = -1;
        if (day->answer == 0) {
            day->seq = 18;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 8:
        if (SimDay_GetStat(SIMEV_STAT_POINT, 0) < 10000) {
            day->seq = 17;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0x76;
        SimDay_AddChange(SIMEV_STAT_POINT, -10000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        Snd_PlaySe(2, 0x1D);
        day->msgLine = 0x78;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        Snd_PlaySe(2, 0x1F);
        day->msgLine = 0x80;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->msgLine = 0x68;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 13:
        if (day->answer == 0) {
            day->seq = 16;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 14:
        SimDay_GiveItem();
        day->msgLine = 0x69;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
    case 16:
        day->msgLine = 0x7D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 19;
        day->seqTimer = 0;
        break;
    case 17:
        day->msgLine = 0xD7;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 18:
        day->msgLine = 0x77;
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

/* Random event 12: the 10000-point deal that adds one to gSaveData + 0x20C. */
s32 SimEv17(TSimDay *day) {
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
        day->faceA = 4;
        day->faceB = 6;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x73;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x74;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x75;
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = -1;
        if (day->answer == 0) {
            day->seq = 15;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 8:
        if (SimDay_GetStat(SIMEV_STAT_POINT, 0) < 10000) {
            day->seq = 14;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0x76;
        SimDay_AddChange(SIMEV_STAT_POINT, -10000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        Snd_PlaySe(2, 0x1D);
        day->msgLine = 0x78;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        Snd_PlaySe(2, 0x1F);
        day->msgLine = 0x7B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        gSaveData->unk20C++;
        day->msgLine = 0x7C;
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 13:
        day->msgLine = 0x7D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 16;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0xD7;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x77;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 16:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 17:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 18:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 13: picture 5; health back to 100. */
s32 SimEv18(TSimDay *day) {
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
        day->faceA = 5;
        day->faceB = 5;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x82;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x83;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_AddChange(SIMEV_STAT_HP, 100 - SimDay_GetStat(SIMEV_STAT_HP, 0));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->msgLine = 0x84;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        day->msgLine = 0x85;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 14: picture 5 pays 300 points (line 0x86). */
s32 SimEv19(TSimDay *day) {
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
        day->faceA = 5;
        day->faceB = 5;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x82;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x86;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x66;
        SimDay_AddChange(SIMEV_STAT_POINT, 300);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 7:
        day->msgLine = 0x85;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 15: as event 14 with line 0x87. */
s32 SimEv20(TSimDay *day) {
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
        day->faceA = 5;
        day->faceB = 5;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x82;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x87;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x66;
        SimDay_AddChange(SIMEV_STAT_POINT, 300);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 7:
        day->msgLine = 0x85;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 16: as event 14 with line 0x88. */
s32 SimEv21(TSimDay *day) {
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
        day->faceA = 5;
        day->faceB = 5;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x82;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x88;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x66;
        SimDay_AddChange(SIMEV_STAT_POINT, 300);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 7:
        day->msgLine = 0x85;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 17: as event 14 with line 0x89. */
s32 SimEv22(TSimDay *day) {
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
        day->faceA = 5;
        day->faceB = 5;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x82;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x89;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        day->msgLine = 0x66;
        SimDay_AddChange(SIMEV_STAT_POINT, 300);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 7:
        day->msgLine = 0x85;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 8:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 18: pictures 6 / 8 sell, for 20000 points, flag 0x1000: the next training comes out as outcome 5. */
s32 SimEv23(TSimDay *day) {
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
        day->faceA = 6;
        day->faceB = 8;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x8A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x8B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->msgLine = 0x8C;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 13;
            day->seqTimer = 0;
        }
        break;
    case 8:
        if (SimDay_GetStat(SIMEV_STAT_POINT, 0) < 20000) {
            day->seq = 12;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0x8E;
        SimDay_AddChange(SIMEV_STAT_POINT, -20000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        day->flags = (day->flags & ~SIMEV_FLAG800) | SIMEV_FLAG1000;
        Snd_PlaySe(2, 0x1F);
        day->msgLine = 0x8F;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0x90;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 14;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = 0xD7;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0x8D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 16:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 19: the same sale, flag 0x800: the next training comes out as outcome 0. */
s32 SimEv24(TSimDay *day) {
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
        day->faceA = 6;
        day->faceB = 8;
        SimDay_Cmd(SIMEV_CMD_MONITOR_OFF);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 4:
        SimDay_Cmd(SIMEV_CMD_CHARA_IN);
        day->msgLine = 0x8A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 5:
        day->msgLine = 0x8B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->msgLine = 0x8C;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 13;
            day->seqTimer = 0;
        }
        break;
    case 8:
        if (SimDay_GetStat(SIMEV_STAT_POINT, 0) < 20000) {
            day->seq = 12;
            day->seqTimer = 0;
        } else {
            day->seq++;
            day->seqTimer = 0;
        }
        break;
    case 9:
        day->msgLine = 0x8E;
        SimDay_AddChange(SIMEV_STAT_POINT, -20000);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 10:
        day->flags = (day->flags & ~SIMEV_FLAG1000) | SIMEV_FLAG800;
        Snd_PlaySe(2, 0x1F);
        day->msgLine = 0x91;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0x90;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 14;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = 0xD7;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0x8D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 16:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 20: picture 1 asks; yes = 500 points. */
s32 SimEv25(TSimDay *day) {
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
        day->msgLine = 0x93;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->msgLine = 0x94;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 13;
            day->seqTimer = 0;
        }
        break;
    case 8:
        day->msgLine = 0x96;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0x97;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0x98;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0x99;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 12:
        day->msgLine = 0x9D;
        SimDay_AddChange(SIMEV_STAT_POINT, 500);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq = 14;
        break;
    case 13:
        day->msgLine = 0x95;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 16:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 21: picture 1 asks; yes = half the health lost, 500 points. */
s32 SimEv26(TSimDay *day) {
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
        day->msgLine = 0x93;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->msgLine = 0x94;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 12;
            day->seqTimer = 0;
        }
        break;
    case 8:
        day->msgLine = 0x96;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0x97;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0x9A;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0x9D;
        SimDay_AddChange(SIMEV_STAT_HP, -(SimDay_GetStat(SIMEV_STAT_HP, 0) / 2));
        SimDay_AddChange(SIMEV_STAT_POINT, 500);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq = 13;
        break;
    case 12:
        day->msgLine = 0x95;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 15:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}

/* Random event 22: picture 1 asks; yes = attack + 15, defence + 15, half the health lost, 700 points. */
s32 SimEv27(TSimDay *day) {
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
        day->msgLine = 0x93;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 6:
        SimDay_Cmd(SIMEV_CMD_SELECT);
        day->msgLine = 0x94;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 7:
        if (day->answer != 0) {
            day->seq++;
            day->seqTimer = 0;
        } else {
            day->seq = 13;
            day->seqTimer = 0;
        }
        break;
    case 8:
        day->msgLine = 0x96;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 9:
        day->msgLine = 0x97;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 10:
        day->msgLine = 0x9B;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 11:
        day->msgLine = 0x9C;
        SimDay_AddChange(SIMEV_STAT_ATK, 15);
        SimDay_AddChange(SIMEV_STAT_DEF, 15);
        SimDay_AddChange(SIMEV_STAT_HP, -(SimDay_GetStat(SIMEV_STAT_HP, 0) / 2));
        SimDay_AddChange(SIMEV_STAT_POINT, 700);
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 12:
        day->msgLine = 0x9D;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq = 16;
        day->seqTimer = 0;
        break;
    case 13:
        day->msgLine = 0x95;
        day->flags |= SIMEV_WAIT_KEY;
        day->seq++;
        day->seqTimer = 0;
        break;
    case 14:
        day->msgLine = -1;
        SimDay_Cmd(SIMEV_CMD_CHARA_OUT);
        day->seq++;
        day->seqTimer = 0;
        break;
    case 15:
        day->msgLine = 0x63;
        SimDay_AddChange(SIMEV_STAT_ATK, -Rand_Range(3));
        SimDay_AddChange(SIMEV_STAT_DEF, -Rand_Range(3));
        SimDay_Cmd(SIMEV_CMD_SHOW_CHANGE);
        day->flags |= SIMEV_WAIT_KEY;
        day->seqTimer = 0;
        day->seq++;
        break;
    case 16:
        day->msgLine = -1;
        return 1;
    }
    return 0;
}
