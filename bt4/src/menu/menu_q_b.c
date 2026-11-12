#include "common.h"
#include "menu/menu_q.h"
#include "sys/pad.h"
#ifdef PORT
extern void Port_MenuPage(int screen);
extern int Port_MenuPart(int part); /* port/src/gs_marker.c: widescreen menus */
extern void Port_MenuFill(int on);
#endif

/* The screen's work area (.data, 0x3B7384). */
SimDay *gSimDay = NULL;

/*
 * SimDay (0x37F850..0x3851B0): the day screen of mode 22, the board of the "sim" ladder (clip names
 * "mc_sim_botan", "mc_sim_monita", "fl_syugyo_*"). A run is a number of rounds of ten turns; on each of the
 * first nine the player picks a button of the board (a training, a rest, a search, ...) and an event script
 * (the 37 handlers behind gSimEvent, 0x38C408..) plays out on the movies through SimDay_Cmd; the tenth turn is
 * the round's fight, set up by SimDay_SetupBattle. The object was written as two halves, merged
 * here: this file to 0x3840E0, and src/menu/menu_r.c (0x3840E0..0x3851B0: pad handler, clip helper, portrait
 * loader, frame loop), which named it. Its read-only data starts with the
 * tables below (0x3B9010, behind the padding at 0x3B9008) and runs past this chunk's last string (0x3B9870).
 */

/* The five ranks of each of the three trainings; a rank starts at `turn`. */
const SimTrain gSimTrain0[SIMDAY_TRAIN_RANKS] = {
    { 5, 3, 1000, 5, -5, 0 }, { 6, 3, 3000, 5, -5, 11 }, { 7, 3, 5000, 5, -5, 21 }, { 7, 2, 8000, 5, -5, 31 },
    { 7, 2, 10000, 5, -5, 41 },
};
const SimTrain gSimTrain1[SIMDAY_TRAIN_RANKS] = {
    { 1, 0, 1000, 5, -5, 0 }, { 2, 0, 3000, 5, -5, 7 }, { 3, 1, 5000, 5, -5, 14 }, { 4, 1, 8000, 5, -5, 21 },
    { 4, 2, 10000, 5, -5, 28 },
};
const SimTrain gSimTrain2[SIMDAY_TRAIN_RANKS] = {
    { 5, 0, 1000, 5, -5, 0 }, { 5, 1, 3000, 5, -5, 7 }, { 8, 1, 5000, 5, -5, 14 }, { 8, 2, 8000, 5, -5, 21 },
    { 10, 2, 10000, 5, -5, 28 },
};

/* Music of each round (+ SIMDAY_BGM_FIRST). */
const s32 gSimBgm[8] = { 4, 1, 2, 0, 6, 4, 8, 0 };

/* The items (0-based ids) an event may hand out, if the save owns them. */
const s32 gSimEventItems[42] = {
    0x22, 0x23, 0x26, 0x28, 0x2A, 0x2D, 0x2E, 0x30, 0x32, 0x33, 0x35, 0x37, 0x3A, 0x3E,
    0x3F, 0x44, 0x45, 0x49, 0x4B, 0x4C, 0x4D, 0x4E, 0x50, 0x53, 0x54, 0x57, 0x58, 0x5B,
    0x5D, 0x5E, 0x60, 0x64, 0x65, 0x66, 0x68, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
};

/* Rank (0..4) of training `kind` at the current turn. */
s32 SimDay_GetTrainRank(s32 kind) {
    s32 turn = QPROG->sim.turn;
    s32 i;
    s32 last;

    for (i = 0; i < 4; i++) {
        switch (kind) {
        case 0:
            last = gSimTrain0[i + 1].turn - 1;
            if (turn < last) {
                return i;
            }
            break;
        case 1:
            last = gSimTrain1[i + 1].turn - 1;
            if (turn < last) {
                return i;
            }
            break;
        case 2:
            last = gSimTrain2[i + 1].turn - 1;
            if (turn < last) {
                return i;
            }
            break;
        }
    }
    return 4;
}

/* Puts picture `n` of the first face set on the monitor (12 = the default picture). */
void SimDay_SetFaceA(s32 n) {
    if (n == 12) {
        gSimDay->tex0[32] = gSimDay->faceDefault;
    } else {
        gSimDay->tex0[32] = MTEX(gSimDay->faceResA, n);
    }
}

/* Puts picture `n` of the second face set on the monitor. */
void SimDay_SetFaceB(s32 n) {
    gSimDay->tex0[33] = MTEX(gSimDay->faceResB, n);
}

/*
 * The battle hand-off of the round's fight: the rule from the round table, the player's fighter with the
 * board's attack and defence as two items and the carried items, the opponent from the enemy table.
 */
void SimDay_SetupBattle(void) {
    u16 items[8];
    u16 enemyItems[8];
    s32 round = QPROG->sim.turn / SIM_TURNS;
    SimRound *rd = &gSimDay->round[round];
    s32 announcer = rd->announcer;
    s32 unk4 = rd->unk4 != 0;
    s32 timeLimit = rd->timeLimit;
    s32 stage = rd->stage;
    s32 bgm = rd->bgm;
    s32 unk14 = rd->unk14 != 0;
    SimEnemy *en = &gSimDay->enemy[rd->enemy];
    s32 i;
    s32 n;
    s32 color;

    if (announcer == SIMDAY_RANDOM) {
        announcer = Rand_Range(8);
    }
    if (stage == SIMDAY_RANDOM) {
        stage = Rand_Range(0x23);
    }
    if (bgm == SIMDAY_RANDOM) {
        bgm = 0x18;
    }
    Battle_ClearWork();
    BattleSetup_SetRule(0, 2, bgm, timeLimit, announcer, stage, unk4);
    if (en->chara == SIMDAY_RANDOM) {
        s32 pool;

        n = 0;
        pool = 0;
        switch (round) {
        case 0:
            break;
        case 3:
            pool = 1;
            break;
        case 5:
            pool = 2;
            break;
        }
        for (i = 0; i < 16; i++) {
            if (gSimDay->pool[pool].chara[i] == SIMDAY_NONE) {
                break;
            }
            n++;
        }
        gSimDay->enemyChara = gSimDay->pool[pool].chara[Rand_Range(n)];
    } else {
        gSimDay->enemyChara = en->chara;
    }
    memset(items, 0, sizeof(items));
    items[0] = QPROG->sim.stat[SIM_STAT_ATK] + 0xC4;
    items[1] = QPROG->sim.stat[SIM_STAT_DEF] + 0x110;
    for (i = 0; i < SIM_ITEM_NUM; i++) {
        s32 slot = (QPROG->sim.itemHead + i) % SIM_ITEM_NUM;

        if (QPROG->sim.item[slot] != -1) {
            items[2 + i] = QPROG->sim.item[slot] + 1;
        }
    }
    memset(enemyItems, 0, sizeof(enemyItems));
    n = 0;
    for (i = 0; i < 7; i++) {
        s32 id = en->item[i];

        if (id != SIMDAY_NONE) {
            enemyItems[n++] = id + 1;
        }
    }
    if (en->lastItem == SIMDAY_NONE) {
        enemyItems[7] = 0;
    } else {
        enemyItems[7] = en->lastItem + 1;
    }
    BattleSetup_SetSide(0, 0, 0, 1, 1, 1, 0, 0);
    BattleSetup_SetSide(1, 2, 1, 1, unk14, 1, 0, 0);
    BattleSetup_SetMember(0, 0, QPROG->member.chara, QPROG->member.color, 0, 0, QPROG->sim.stat[SIM_STAT_HP], items);
    color = en->color;
    if (gSimDay->enemyChara == QPROG->member.chara) {
        if (QPROG->member.color == color) {
            color = color == 0;
        }
    }
    BattleSetup_SetMember(1, 0, gSimDay->enemyChara, color, 0, en->cpuLevel, 100.0f, enemyItems);
    BattleSetup_FinishEx(1);
}

/* Whether item `item` (0-based) is one an event may hand out. */
s32 SimDay_IsEventItem(s32 item) {
    u32 i;

    for (i = 0; i < 42; i++) {
        if (gSimEventItems[i] == item) {
            return 1;
        }
    }
    return 0;
}

/* Lists the owned items that events may hand out and picks the column of the event table. */
void SimDay_ListItems(void) {
    s32 any = 0;
    s32 i;
    s32 all;

    gSimDay->ownCount = 0;
    for (i = 0; i < Q_ITEM_NUM; i++) {
        if ((u8)(QSAVE->item[i] & 1) && SimDay_IsEventItem(i)) {
            any = 1;
            gSimDay->own[gSimDay->ownCount] = i;
            gSimDay->ownCount++;
        }
    }
    i = QSAVE->unk20C;
    all = i == 20;
    if (!all) {
        if (!any) {
            gSimDay->eventSet = 0;
        } else {
            gSimDay->eventSet = 1;
        }
    } else {
        if (!any) {
            gSimDay->eventSet = 2;
        } else {
            gSimDay->eventSet = 3;
        }
    }
}

/* Queues a change of stat `stat` (points are given in units and kept in hundreds). */
void SimDay_AddChange(s32 stat, s32 amount) {
    gSimDay->chg.mask |= 1 << stat;
    if (stat == SIM_STAT_POINT) {
        gSimDay->chg.delta[SIM_STAT_POINT] = amount / 100;
    } else {
        gSimDay->chg.delta[stat] = amount;
    }
}

/* A stat as the scripts see it: the preview value, or the run's (points in units). */
s32 SimDay_GetStat(s32 stat, s32 preview) {
    if (preview) {
        return gSimDay->preview[stat];
    }
    if (stat == SIM_STAT_POINT) {
        return QPROG->sim.stat[SIM_STAT_POINT] * 100;
    }
    return QPROG->sim.stat[stat];
}

/* Hands out one of the listed items at random: it replaces the oldest of the three carried. */
void SimDay_GiveItem(void) {
    s32 n = Rand_Range(gSimDay->ownCount);

    QPROG->sim.item[QPROG->sim.itemHead] = gSimDay->own[n];
    QPROG->sim.itemHead = (QPROG->sim.itemHead + 1) % SIM_ITEM_NUM;
    SimDay_Cmd(2);
}

/* Takes the oldest carried item away. */
void SimDay_TakeItem(void) {
    s32 i;

    for (i = 0; i < SIM_ITEM_NUM; i++) {
        s32 slot = (QPROG->sim.itemHead + i) % SIM_ITEM_NUM;

        if (QPROG->sim.item[slot] != -1) {
            QPROG->sim.item[slot] = -1;
            SimDay_Cmd(3);
            return;
        }
    }
}

/* Number of items carried. */
s32 SimDay_CountItems(void) {
    s32 n = 0;
    s32 i;

    for (i = 0; i < SIM_ITEM_NUM; i++) {
        if (QPROG->sim.item[i] != -1) {
            n++;
        }
    }
    return n;
}

/* The same again (a second copy in the original). */
s32 SimDay_CountItems2(void) {
    s32 n = 0;
    s32 i;

    for (i = 0; i < SIM_ITEM_NUM; i++) {
        if (QPROG->sim.item[i] != -1) {
            n++;
        }
    }
    return n;
}

/* Opens (on) or greys out button `button` of the board, if it is not in that state already. */
void SimDay_SetButton(s32 on, s32 button) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gSimDay->flash[SIMDAY_FL_BOARD];

    sprintf(name, "mc_sim_botan%02d", button);
    Flash_FindLabel(flash, NULL, name, &ref);
    if (on) {
        if ((QPROG->sim.off >> button) & 1) {
            Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
            QPROG->sim.off &= ~(1 << button);
        }
    } else {
        if (!((QPROG->sim.off >> button) & 1)) {
            Flash_ClipGotoLabel(flash, &ref, "fl_no_select");
            QPROG->sim.off |= 1 << button;
        }
    }
}

/* Counts `turns` off the wait of button 3. */
void SimDay_PassWait(s32 turns) {
    if (QPROG->sim.wait != 0) {
        QPROG->sim.wait -= turns;
        if (QPROG->sim.wait < 0) {
            QPROG->sim.wait = 0;
        }
    }
}

/* The first stat with a change pending, -1 if none. */
s32 SimDay_NextChange(void) {
    s32 i;

    for (i = 0; i < SIM_STAT_NUM; i++) {
        if ((gSimDay->chg.mask >> i) & 1) {
            return i;
        }
    }
    return -1;
}

/*
 * Starts the animation of the next pending change. Attack and defence go in steps of 1 or 2 drawn at random
 * (the change stays pending until used up); health and points are shown in one go.
 */
void SimDay_ShowChange(void) {
    s32 stat = SimDay_NextChange();

    if (stat == -1) {
        return;
    }
    gSimDay->chg.stat = stat;
    switch (stat) {
    case SIM_STAT_ATK:
    case SIM_STAT_DEF: {
        s32 step = Rand_Range(2) + 1;

        if (gSimDay->chg.delta[stat] != 0) {
            if (gSimDay->chg.delta[stat] > 0) {
                if (gSimDay->chg.delta[stat] < step) {
                    step = gSimDay->chg.delta[stat];
                }
                gSimDay->chg.delta[stat] -= step;
            } else {
                s32 neg = -step;

                if (neg < gSimDay->chg.delta[stat]) {
                    step = -gSimDay->chg.delta[stat];
                    neg = gSimDay->chg.delta[stat];
                }
                gSimDay->chg.delta[stat] += step;
                step = neg;
            }
            gSimDay->chg.shown = step + 2;
        } else {
            gSimDay->chg.shown = 2;
            gSimDay->chg.delta[stat] = 0;
        }
        gSimDay->chg.mark = Rand_Range(3);
        if (gSimDay->chg.delta[stat] == 0) {
            gSimDay->chg.mask &= ~(1 << stat);
        }
        SimDay_Cmd(0x1B);
        break;
    }
    case SIM_STAT_HP:
        SimDay_Cmd(0x1D);
        gSimDay->chg.shown = gSimDay->chg.delta[SIM_STAT_HP];
        gSimDay->chg.mask &= ~4;
        break;
    case SIM_STAT_POINT:
        SimDay_Cmd(0x1F);
        gSimDay->chg.shown = gSimDay->chg.delta[SIM_STAT_POINT];
        gSimDay->chg.mask &= ~8;
        break;
    }
}

/* The window shows either its three item names (text) or its three plates. */
void SimDay_SetMenuAlpha(s32 text) {
    MFlashRef ref;
    char name[64];
    MFlash *flash = &gSimDay->flash[SIMDAY_FL_MENU];
    f32 nameAlpha;
    f32 plateAlpha;
    s32 i;

    if (text) {
        nameAlpha = 1.0f;
        plateAlpha = 0.0f;
    } else {
        nameAlpha = 0.0f;
        plateAlpha = 1.0f;
    }
    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_item_name_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetAlpha(flash, &ref, nameAlpha);
    }
    for (i = 0; i < 3; i++) {
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipSetAlpha(flash, &ref, plateAlpha);
    }
}

/* Starts the round's music. */
void SimDay_PlayBgm(void) {
    Bgm_Play(gSimDay->bgm + SIMDAY_BGM_FIRST);
}

/*
 * Runs animation command `cmd` of the event scripts. The command number picks the movie: 0..18 the board,
 * 19..23 the window, 24..34 the event movie, 35..43 / 44..53 / 54..62 the three trainings.
 */
void SimDay_Cmd(s32 cmd) {
    MFlashRef ref;
    char name[64];
    MFlash *flash;
    s32 day;
    s32 i;

    flash = &gSimDay->flash[SIMDAY_FL_SHOT];
    day = gSimDay->day;
    if (cmd < 0x36) {
        flash = &gSimDay->flash[SIMDAY_FL_POPO];
        if (cmd < 0x2C) {
            flash = &gSimDay->flash[SIMDAY_FL_CARD];
            if (cmd < 0x23) {
                flash = &gSimDay->flash[SIMDAY_FL_EVENT];
                if (cmd < 0x18) {
                    if (cmd >= 0x13) {
                        flash = &gSimDay->flash[SIMDAY_FL_MENU];
                    } else {
                        flash = &gSimDay->flash[SIMDAY_FL_BOARD];
                    }
                }
            }
        }
    }
    switch (cmd) {
    case 0:
        /* the stars of the level reached */
        for (i = 0; i <= QPROG->sim.level; i++) {
            sprintf(name, "mc_star_anime%02d", 6 - i);
            Flash_FindLabel(flash, "mc_chara_pt_plate", name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
        }
        break;
    case 1:
        for (i = 0; i <= QPROG->sim.level; i++) {
            sprintf(name, "mc_star_anime%02d", 6 - i);
            Flash_FindLabel(flash, "mc_chara_pt_plate", name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_on_loop");
        }
        break;
    case 2:
        /* lights the first free item plate */
        for (i = 0; i < SIM_ITEM_NUM; i++) {
            if (!((gSimDay->potara >> i) & 1)) {
                sprintf(name, "mc_plate_potara%d", i);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
                gSimDay->potara |= 1 << i;
                break;
            }
        }
        break;
    case 3:
        /* puts out the last lit item plate */
        for (i = SIM_ITEM_NUM - 1; i >= 0; i--) {
            if ((gSimDay->potara >> i) & 1) {
                sprintf(name, "mc_plate_potara%d", i);
                Flash_FindLabel(flash, NULL, name, &ref);
                Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
                gSimDay->potara &= ~(1 << i);
                break;
            }
        }
        break;
    case 4:
        Flash_FindLabel(flash, NULL, "mc_sim_monita", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 5:
        Flash_FindLabel(flash, NULL, "mc_sim_monita", &ref);
        if (gSimDay->flags & SIMDAY_FACE_ON) {
            gSimDay->flags &= ~SIMDAY_FACE_ON;
            Flash_ClipGotoLabel(flash, &ref, "fl_chara_out");
        } else {
            Flash_ClipGotoLabel(flash, &ref, "fl_monita_off");
        }
        gSimDay->flags |= SIMDAY_BUSY;
        break;
    case 6:
        SimDay_SetFaceB(gSimDay->faceB);
        Flash_FindLabel(flash, NULL, "mc_sim_monita", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_chara_in");
        gSimDay->flags |= SIMDAY_FACE_ON;
        break;
    case 7:
        Flash_FindLabel(flash, NULL, "mc_sim_monita", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_chara_only_out");
        gSimDay->flags &= ~SIMDAY_FACE_ON;
        break;
    case 8:
        Flash_FindLabel(flash, NULL, "mc_icon_timer", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        Flash_FindLabel(flash, "mc_icon_timer", "mc_icon_dragon_00", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_turn_on");
        break;
    case 9:
        /* the lamps of the turns played */
        for (i = 0; i <= day; i++) {
            sprintf(name, "mc_turn_lamp%02d", i);
            Flash_FindLabel(flash, "mc_icon_timer", name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
        }
        break;
    case 10:
        if (day == SIM_TURNS - 1) {
            Flash_FindLabel(flash, NULL, "mc_icon_timer", &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_enemy_on");
        }
        break;
    case 11:
        for (i = 0; i <= day; i++) {
            sprintf(name, "mc_turn_lamp%02d", i);
            Flash_FindLabel(flash, "mc_icon_timer", name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_red_on");
        }
        Flash_FindLabel(flash, "mc_icon_timer", "mc_icon_dragon_00", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_enemy_on");
        break;
    case 12:
        Flash_GotoLabel(flash, "fl_botan_in", 1);
        break;
    case 13:
        Flash_GotoLabel(flash, "fl_botan_out", 1);
        break;
    case 14:
        Flash_GotoLabel(flash, "fl_syugyo_botan_in", 1);
        SimDay_ClipGoto(0, SIMDAY_ST_TRAIN, "fl_off_start");
        gSimDay->cur[gSimDay->state] = 0;
        SimDay_ClipGoto(0, SIMDAY_ST_TRAIN, "fl_on_start");
        break;
    case 15:
        Flash_GotoLabel(flash, "fl_syugyo_botan_out", 1);
        break;
    case 16:
        Flash_GotoLabel(flash, "fl_syugyo_cancel", 1);
        gSimDay->state = SIMDAY_ST_BOARD;
        break;
    case 17:
        gSimDay->state = SIMDAY_ST_SELECT;
        Flash_GotoLabel(flash, "fl_select_in", 1);
        SimDay_ClipGoto(0, SIMDAY_ST_SELECT, "fl_off_start");
        gSimDay->cur[gSimDay->state] = 0;
        SimDay_ClipGoto(0, SIMDAY_ST_SELECT, "fl_on_start");
        break;
    case 18:
        Flash_GotoLabel(flash, "fl_select_out", 1);
        gSimDay->state = SIMDAY_ST_SCRIPT;
        break;
    case 19:
        break;
    case 20: {
        s32 cur;

        /* opens the window on the row it was left on */
        if (gSimDay->state != SIMDAY_ST_BACK) {
            gSimDay->prevState = gSimDay->state;
        }
        gSimDay->state = SIMDAY_ST_WINDOW;
        Flash_GotoLabel(flash, "fl_window_in", 1);
        cur = gSimDay->cur[gSimDay->state];
        for (i = 0; i < 3; i++) {
            gSimDay->cur[gSimDay->state] = i;
            if (i == cur) {
                SimDay_ClipGoto(5, SIMDAY_ST_WINDOW, "fl_on_start");
            } else {
                SimDay_ClipGoto(5, SIMDAY_ST_WINDOW, "fl_off_start");
            }
        }
        gSimDay->cur[gSimDay->state] = cur;
        gSimDay->flags |= SIMDAY_WINDOW;
        Snd_PlaySe(1, 4);
        gSimDay->menuText = 0;
        break;
    }
    case 21:
        Flash_GotoLabel(flash, "fl_window_out", 1);
        Snd_PlaySe(1, 5);
        break;
    case 22:
        Flash_GotoLabel(flash, "fl_wide", 1);
        Snd_PlaySe(1, 4);
        break;
    case 23:
        Flash_GotoLabel(flash, "fl_normal", 1);
        Snd_PlaySe(1, 5);
        break;
    case 24:
        break;
    case 25:
        Flash_FindLabel(flash, NULL, "mc_surprise_icon", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        gSimDay->state = SIMDAY_ST_WAIT;
        Snd_PlaySe(2, 0x19);
        break;
    case 26:
        SimDay_ShowChange();
        gSimDay->state = SIMDAY_ST_WAIT;
        break;
    case 27:
        Flash_GotoLabel(flash, "fl_at_df_in", 1);
        break;
    case 28:
        for (i = 0; i < 3; i++) {
            sprintf(name, "mc_mark%02d", i);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_start");
        }
        break;
    case 29:
        Flash_FindLabel(flash, NULL, "mc_pt_up_down", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 30:
        Flash_FindLabel(flash, NULL, "mc_pt_up_down", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_out");
        break;
    case 31:
        Flash_FindLabel(flash, NULL, "mc_score_up_down", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 32:
        Flash_FindLabel(flash, NULL, "mc_score_up_down", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_out");
        break;
    case 33:
        /* level up: as many levels as the points allow */
        if (QPROG->sim.level == SIM_LEVEL_MAX) {
            gSimDay->flags &= ~SIMDAY_LEVEL_UP;
        } else {
            i = QPROG->sim.level + 1;
            do {
                QPROG->sim.level++;
                i++;
            } while (gSimDay->level[i].need < QPROG->sim.stat[SIM_STAT_POINT] && QPROG->sim.level < SIM_LEVEL_MAX);
            gSimDay->chg.mask |= 0x10;
            Flash_FindLabel(flash, NULL, "mc_level_up", &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_start");
            gSimDay->msgLine = 0x57;
            gSimDay->state = SIMDAY_ST_WAIT;
            gSimDay->flags |= SIMDAY_LEVEL_UP;
        }
        break;
    case 34:
        break;
    case 35:
        break;
    case 36:
        Flash_Play(flash, 1);
        break;
    case 37:
        for (i = 0; i < gSimTrain0[gSimDay->rank[0]].param; i++) {
            sprintf(name, "mc_card_anime%02d", i);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_on_start");
        }
        break;
    case 38:
        for (i = 0; i < gSimTrain0[gSimDay->rank[0]].param; i++) {
            sprintf(name, "mc_card_anime%02d", i);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
        }
        break;
    case 39:
        sprintf(name, "mc_card_anime%02d", gSimDay->cur[SIMDAY_ST_SCRIPT]);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_on_maru_batsu");
        break;
    case 40:
        Flash_FindLabel(flash, NULL, "mc_win_text", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 41:
        Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 42:
        Flash_GotoLabel(flash, "fl_card_out", 1);
        break;
    case 43:
        Flash_Reset(flash, 0);
        Flash_Stop(flash);
        break;
    case 44:
        break;
    case 45:
        Flash_Play(flash, 1);
        break;
    case 46:
        Flash_FindLabel(flash, NULL, "mc_fukidasi_popo", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_on");
        break;
    case 47:
        Flash_FindLabel(flash, NULL, "mc_fukidasi_popo", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_end");
        break;
    case 48:
        for (i = 0; i < 5; i++) {
            sprintf(name, "mc_popo%02d", i);
            Flash_FindLabel(flash, NULL, name, &ref);
            Flash_ClipGotoLabel(flash, &ref, "fl_off_start");
        }
        break;
    case 49:
        sprintf(name, "mc_popo%02d", gSimDay->cur[gSimDay->state]);
        Flash_FindLabel(flash, NULL, name, &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_on_stop");
        break;
    case 50:
        Flash_FindLabel(flash, NULL, "mc_win_text", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 51:
        Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 52:
        Flash_GotoLabel(flash, "fl_game_out", 1);
        break;
    case 53:
        Flash_Reset(flash, 0);
        Flash_Stop(flash);
        break;
    case 54:
        break;
    case 55:
        Flash_Play(flash, 1);
        break;
    case 56:
        Flash_FindLabel(flash, NULL, "mc_shot", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_shot_fait");
        break;
    case 57:
        Flash_FindLabel(flash, NULL, "mc_shot", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_shot_hit");
        Flash_FindLabel(flash, NULL, "mc_rock00", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_bom_start");
        break;
    case 58:
        Flash_FindLabel(flash, NULL, "mc_shot", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_off");
        Flash_FindLabel(flash, NULL, "mc_rock00", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_off");
        break;
    case 59:
        Flash_FindLabel(flash, NULL, "mc_win_text", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 60:
        Flash_FindLabel(flash, NULL, "mc_lose_text", &ref);
        Flash_ClipGotoLabel(flash, &ref, "fl_start");
        break;
    case 61:
        Flash_GotoLabel(flash, "fl_game_out", 1);
        break;
    case 62:
        Flash_Reset(flash, 0);
        Flash_Stop(flash);
        break;
    }
}

/*
 * Decides what the turn turns into (the script of gSimEvent to run) from the button chosen: button 0 a
 * training (scripts 0..2, by the row of its sub menu), button 1 a random event drawn by weight from the event
 * table (scripts 5..36; 37 if the weights of the column do not add up to more than the draw), buttons 2 and 3
 * the fixed scripts 3 and 4. Also counts how often the same script came up in a row.
 */
void SimDay_PickEvent(s32 unused) {
    s32 r = Rand_Range(256);
    s32 sum;
    s32 i;
    s32 w;

    gSimDay->unkB68[0] = 0;
    gSimDay->unkB68[1] = 0;
    switch (gSimDay->cur[SIMDAY_ST_BOARD]) {
    case 0:
        gSimDay->event = gSimDay->cur[SIMDAY_ST_TRAIN];
        break;
    case 1:
        sum = 0;
        for (i = 0; i < SIMDAY_EVENT_ROWS; i++) {
            w = gSimDay->eventTbl[i].weight[gSimDay->eventSet];
            if (w == 0) {
                continue;
            }
            sum += w;
            if (r < sum) {
                break;
            }
        }
        gSimDay->event = i + 5;
        break;
    case 2:
        gSimDay->event = 3;
        break;
    case 3:
        gSimDay->event = 4;
        break;
    }
    gSimDay->unkB8C = 0;
    if (gSimDay->event == gSimDay->lastEvent) {
        gSimDay->repeat++;
        if (gSimDay->repeat >= 3) {
            gSimDay->repeat = 2;
        }
    } else {
        gSimDay->repeat = 0;
    }
    gSimDay->lastEvent = gSimDay->event;
    gSimDay->cur[SIMDAY_ST_SCRIPT] = 0;
    gSimDay->msgLine = -1;
}

/* Redraws the board's stars, turn icon and item plates, and starts a level-up if the points allow one. */
void SimDay_RefreshBoard(void) {
    s32 i;

    SimDay_Cmd(0);
    SimDay_Cmd(8);
    for (i = 0; i < SIM_ITEM_NUM; i++) {
        if (QPROG->sim.item[i] != -1) {
            SimDay_Cmd(2);
        }
    }
    gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
    if (QPROG->sim.level < SIM_LEVEL_MAX + 1) {
        if (gSimDay->level[QPROG->sim.level + 1].need < QPROG->sim.stat[SIM_STAT_POINT]) {
            gSimDay->levelUp = 1;
            SimDay_Cmd(0x21);
        }
    }
}

#define SD_RES(n) \
    res = (MTexRes *)MPACK_AT(gSimDay->res, n); \
    Res_RelocateOffsets(&res, res, res)

/*
 * Loads the screen (file baseFile + 0x1A, not a section of archive 3, which is freed to make room), builds its
 * seven movies and windows, and takes over the state of the run: coming back from a fight, the player's health
 * becomes what the fight left (at least 1), and the buttons are greyed out as the state demands.
 */
void SimDay_Init(s32 section) {
    MTexRes *res = NULL;
    s32 i;
    void *check;

    if (gMenuArc3 != NULL) {
        Heap_Free(gMenuArc3);
        gMenuArc3 = NULL;
    }
    gSimDay = Heap_Alloc(sizeof(SimDay), 0x20, 0, 2);
    memset(gSimDay, 0, sizeof(SimDay));
    for (i = 0; i < SIMDAY_TRAIN_KINDS; i++) {
        gSimDay->rank[i] = SimDay_GetTrainRank(i);
    }
    /*
     * The two dead reads stand for something the original compiled away here (stripped checks of the two
     * pointers, guess): without a few more instructions in front of it, the place where the compiler's
     * common-subexpression pass forgets the address of gSimDay after 1000 instructions (and from where the
     * code uses a second register for it, at 0x3821A8) comes one statement late.
     */
    gSimDay->pack = File_LoadSync(QPROG->baseFile + 0x1A, NULL, 0);
    check = gSimDay->pack;
    gSimDay->res = Sprite_Unpack(gSimDay->pack, NULL, NULL);
    check = gSimDay->res;
    gSimDay->faceFile[0] = Heap_Alloc(SIMDAY_FACE_SIZE, 0x40, 0, 2);
    gSimDay->faceFile[1] = Heap_Alloc(SIMDAY_FACE_SIZE, 0x40, 0, 2);
    gSimDay->faceRes[0] = Heap_Alloc(SIMDAY_FACE_RES_SIZE, 0x20, 0, 2);
    gSimDay->faceRes[1] = Heap_Alloc(SIMDAY_FACE_RES_SIZE, 0x20, 0, 2);
    SD_RES(4);
    gSimDay->bg = res;
    SD_RES(3);
    gSimDay->tex0[55] = MTEX(res, 0);
    gSimDay->tex0[0] = MTEX(res, 1);
    gSimDay->tex0[3] = MTEX(res, 2);
    gSimDay->tex0[2] = MTEX(res, 3);
    gSimDay->tex0[1] = MTEX(res, 4);
    gSimDay->tex0[6] = MTEX(res, 5);
    gSimDay->tex0[49] = MTEX(res, 6);
    gSimDay->tex0[51] = MTEX(res, 7);
    gSimDay->tex0[50] = MTEX(res, 8);
    gSimDay->tex0[57] = MTEX(res, 9);
    gSimDay->tex0[45] = MTEX(res, 10);
    gSimDay->tex0[44] = MTEX(res, 11);
    gSimDay->tex0[46] = MTEX(res, 12);
    gSimDay->tex0[56] = MTEX(res, 13);
    gSimDay->tex0[58] = MTEX(res, 14);
    gSimDay->tex0[59] = MTEX(res, 15);
    gSimDay->tex0[60] = MTEX(res, 16);
    gSimDay->tex0[62] = MTEX(res, 17);
    gSimDay->tex0[61] = MTEX(res, 18);
    gSimDay->tex0[54] = MTEX(res, 19);
    gSimDay->tex0[19] = MTEX(res, 20);
    gSimDay->tex0[20] = MTEX(res, 21);
    gSimDay->tex0[8] = MTEX(res, 22);
    gSimDay->tex0[9] = MTEX(res, 23);
    gSimDay->tex0[18] = MTEX(res, 24);
    gSimDay->tex0[11] = MTEX(res, 25);
    gSimDay->tex0[21] = MTEX(res, 26);
    gSimDay->tex0[22] = MTEX(res, 27);
    gSimDay->tex0[10] = MTEX(res, 28);
    gSimDay->tex0[27] = MTEX(res, 29);
    gSimDay->tex0[28] = MTEX(res, 30);
    gSimDay->tex0[16] = MTEX(res, 31);
    gSimDay->tex0[17] = MTEX(res, 32);
    gSimDay->tex0[14] = MTEX(res, 33);
    gSimDay->tex0[15] = MTEX(res, 34);
    gSimDay->tex0[12] = MTEX(res, 35);
    gSimDay->tex0[13] = MTEX(res, 36);
    gSimDay->tex0[7] = MTEX(res, 37);
    gSimDay->tex0[23] = MTEX(res, 38);
    gSimDay->tex0[24] = MTEX(res, 39);
    gSimDay->tex0[25] = MTEX(res, 40);
    gSimDay->tex0[26] = MTEX(res, 41);
    gSimDay->tex0[29] = MTEX(res, 42);
    gSimDay->tex0[30] = MTEX(res, 43);
    gSimDay->tex0[35] = MTEX(res, 44);
    gSimDay->tex0[34] = MTEX(res, 45);
    gSimDay->tex0[38] = MTEX(res, 46);
    gSimDay->tex0[31] = MTEX(res, 47);
    gSimDay->tex0[37] = MTEX(res, 48);
    gSimDay->tex0[36] = MTEX(res, 49);
    gSimDay->tex0[47] = MTEX(res, 50);
    gSimDay->tex0[48] = MTEX(res, 51);
    gSimDay->tex0[52] = MTEX(res, 52);
    gSimDay->tex0[4] = MTEX(res, 53);
    gSimDay->tex0[40] = MTEX(res, 54);
    gSimDay->tex0[41] = MTEX(res, 55);
    gSimDay->tex0[43] = MTEX(res, 56);
    gSimDay->tex0[42] = MTEX(res, 57);
    gSimDay->tex0[39] = MTEX(res, 58);
    gSimDay->tex0[5] = NULL;
    gSimDay->tex0[32] = NULL;
    gSimDay->faceDefault = MTEX(res, 59);
    SD_RES(31);
    gSimDay->tex0[5] = MTEX(res, 0);
    SD_RES(8);
    gSimDay->tex0[53] = MTEX(res, QPROG->member.chara);
    gSimDay->faceResA = (MTexRes *)MPACK_AT(gSimDay->res, 6);
    Res_RelocateOffsets(&gSimDay->faceResA, gSimDay->faceResA, gSimDay->faceResA);
    SimDay_SetFaceA(0);
    gSimDay->faceResB = (MTexRes *)MPACK_AT(gSimDay->res, 7);
    Res_RelocateOffsets(&gSimDay->faceResB, gSimDay->faceResB, gSimDay->faceResB);
    SimDay_SetFaceB(0);
    SD_RES(23);
    gSimDay->tex5[3] = MTEX(res, 0);
    gSimDay->tex5[0] = MTEX(res, 1);
    gSimDay->tex5[1] = MTEX(res, 2);
    gSimDay->tex5[4] = MTEX(res, 3);
    gSimDay->tex5[5] = MTEX(res, 4);
    gSimDay->tex5[2] = NULL;
    SD_RES(19);
    gSimDay->tex1[9] = MTEX(res, 0);
    gSimDay->tex1[10] = MTEX(res, 1);
    gSimDay->tex1[4] = MTEX(res, 2);
    gSimDay->tex1[6] = MTEX(res, 3);
    gSimDay->tex1[5] = MTEX(res, 4);
    gSimDay->tex1[8] = MTEX(res, 5);
    gSimDay->tex1[1] = MTEX(res, 6);
    gSimDay->tex1[0] = MTEX(res, 7);
    SD_RES(20);
    gSimDay->tex1[2] = MTEX(res, 0);
    SD_RES(21);
    gSimDay->tex1[3] = MTEX(res, 0);
    gSimDay->numRes = (MTexRes *)MPACK_AT(gSimDay->res, 22);
    Res_RelocateOffsets(&gSimDay->numRes, gSimDay->numRes, gSimDay->numRes);
    gSimDay->tex1[7] = MTEX(gSimDay->numRes, 0);
    SD_RES(16);
    gSimDay->tex2[0] = MTEX(res, 2);
    gSimDay->tex2[7] = MTEX(res, 5);
    gSimDay->tex2[8] = MTEX(res, 6);
    gSimDay->tex2[10] = MTEX(res, 7);
    gSimDay->tex2[9] = MTEX(res, 8);
    gSimDay->tex2[11] = MTEX(res, 9);
    gSimDay->tex2[6] = MTEX(res, 10);
    gSimDay->tex2[3] = MTEX(res, 12);
    gSimDay->tex2[1] = MTEX(res, 13);
    gSimDay->tex2[12] = MTEX(res, 14);
    gSimDay->tex2[13] = MTEX(res, 19);
    gSimDay->tex2[15] = MTEX(res, 27);
    gSimDay->tex3[6] = MTEX(res, 1);
    gSimDay->tex3[0] = MTEX(res, 2);
    gSimDay->tex3[3] = MTEX(res, 12);
    gSimDay->tex3[1] = MTEX(res, 13);
    gSimDay->tex3[11] = MTEX(res, 14);
    gSimDay->tex3[13] = MTEX(res, 19);
    gSimDay->tex3[12] = MTEX(res, 22);
    gSimDay->tex3[7] = MTEX(res, 23);
    gSimDay->tex3[8] = MTEX(res, 24);
    gSimDay->tex3[9] = MTEX(res, 25);
    gSimDay->tex3[10] = MTEX(res, 26);
    gSimDay->tex4[6] = MTEX(res, 0);
    gSimDay->tex4[0] = MTEX(res, 2);
    gSimDay->tex4[9] = MTEX(res, 4);
    gSimDay->tex4[12] = MTEX(res, 11);
    gSimDay->tex4[3] = MTEX(res, 12);
    gSimDay->tex4[1] = MTEX(res, 13);
    gSimDay->tex4[8] = MTEX(res, 15);
    gSimDay->tex4[7] = MTEX(res, 16);
    gSimDay->tex4[13] = MTEX(res, 17);
    gSimDay->tex4[11] = MTEX(res, 18);
    gSimDay->tex4[14] = MTEX(res, 19);
    gSimDay->tex4[16] = MTEX(res, 20);
    SD_RES(17);
    gSimDay->tex2[2] = MTEX(res, 0);
    gSimDay->tex2[5] = MTEX(res, 1);
    gSimDay->tex2[4] = MTEX(res, 2);
    gSimDay->tex2[14] = MTEX(res, 3);
    gSimDay->tex3[2] = MTEX(res, 0);
    gSimDay->tex3[5] = MTEX(res, 1);
    gSimDay->tex3[4] = MTEX(res, 2);
    gSimDay->tex3[14] = MTEX(res, 3);
    gSimDay->tex4[2] = MTEX(res, 0);
    gSimDay->tex4[5] = MTEX(res, 1);
    gSimDay->tex4[4] = MTEX(res, 2);
    gSimDay->tex4[15] = MTEX(res, 3);
    gSimDay->tex4[10] = MTEX(res, 4);
    gSimDay->tex4[17] = MTEX(res, 5);
    SD_RES(5);
    gSimDay->tex6[9] = MTEX(res, 0);
    gSimDay->tex6[10] = MTEX(res, 1);
    gSimDay->tex6[0] = MTEX(res, 2);
    gSimDay->tex6[1] = MTEX(res, 3);
    gSimDay->tex6[2] = MTEX(res, 4);
    gSimDay->tex6[3] = MTEX(res, 5);
    gSimDay->tex6[4] = MTEX(res, 6);
    gSimDay->tex6[5] = MTEX(res, 7);
    gSimDay->tex6[6] = MTEX(res, 8);
    gSimDay->tex6[7] = MTEX(res, 9);
    gSimDay->tex6[11] = MTEX(res, 10);
    gSimDay->tex6[12] = MTEX(res, 11);
    gSimDay->tex6[8] = MTEX(res, 12);
    gSimDay->tex6[15] = MTEX(res, 13);
    Flash_Create(&gSimDay->flash[SIMDAY_FL_BOARD], MPACK_AT(gSimDay->res, 1), gSimDay->tex0);
    Flash_Play(&gSimDay->flash[SIMDAY_FL_BOARD], 1);
    Flash_Advance(&gSimDay->flash[SIMDAY_FL_BOARD]);
    Flash_Create(&gSimDay->flash[SIMDAY_FL_MENU], MPACK_AT(gSimDay->res, 15), gSimDay->tex5);
    Flash_Play(&gSimDay->flash[SIMDAY_FL_MENU], 1);
    Flash_Create(&gSimDay->flash[SIMDAY_FL_ROUND], MPACK_AT(gSimDay->res, 2), gSimDay->tex6);
    Flash_Create(&gSimDay->flash[SIMDAY_FL_EVENT], MPACK_AT(gSimDay->res, 18), gSimDay->tex1);
    Flash_Play(&gSimDay->flash[SIMDAY_FL_EVENT], 1);
    /* the card movie depends on how many cards the rank puts on the table (5, 6 or 7: sections 10..12) */
    Flash_Create(&gSimDay->flash[SIMDAY_FL_CARD], MPACK_AT(gSimDay->res, gSimTrain0[gSimDay->rank[0]].param + 5),
                 gSimDay->tex2);
    Flash_Create(&gSimDay->flash[SIMDAY_FL_POPO], MPACK_AT(gSimDay->res, 13), gSimDay->tex3);
    Flash_Create(&gSimDay->flash[SIMDAY_FL_SHOT], MPACK_AT(gSimDay->res, 14), gSimDay->tex4);
    gSimDay->msgText = MPACK_AT(gSimDay->res, 24);
    MsgWin_Init(MPACK_AT(gSimDay->res, 9), gSimDay->msgText, 1, 0);
    MsgWin_Open();
    gSimDay->text = MPACK_AT(gSimDay->res, 32);
    for (i = 0; i < 3; i++) {
        TextBox_Init(&gSimDay->box[i], gSimDay->text, 0);
        TextBox_SetUnk50(&gSimDay->box[i], 1);
    }
    Dialog_Init(MPACK_AT(gSimDay->res, 33), gSimDay->msgText, 0);
    gSimDay->eventTbl = (void *)MPACK_AT(gSimDay->res, 28);
    gSimDay->round = (SimRound *)MPACK_AT(gSimDay->res, 25);
    gSimDay->pool = (SimPool *)MPACK_AT(gSimDay->res, 27);
    gSimDay->enemy = (SimEnemy *)MPACK_AT(gSimDay->res, 26);
    gSimDay->unkB5C = MPACK_AT(gSimDay->res, 29);
    gSimDay->level = (SimLevel *)MPACK_AT(gSimDay->res, 30);
    gSimDay->msgLine = -1;
    gSimDay->lastEvent = -1;
    gSimDay->menuText = 0;
    if (QPROG->sim.turn != 0) {
        s32 hp = BattleResult_GetPtr()->health[0];

        if (hp == 0) {
            hp = 1;
        }
        QPROG->sim.stat[SIM_STAT_HP] = hp;
        gSimDay->day = QPROG->sim.turn % SIM_TURNS;
        SimDay_PassWait(1);
        if (QPROG->sim.wait != 0) {
            SimDay_SetButton(0, 3);
        } else {
            SimDay_SetButton(1, 3);
        }
        if (QPROG->sim.stat[SIM_STAT_HP] < 21) {
            SimDay_SetButton(0, 0);
            SimDay_SetButton(0, 1);
            gSimDay->cur[gSimDay->state] = 2;
        }
    }
    SimDay_ClipGoto(0, 0, "fl_on_start");
    SimDay_ListItems();
    gSimDay->bgm = gSimBgm[QPROG->sim.turn / SIM_TURNS];
    SimDay_PlayBgm();
}

/* Frees the screen. */
void SimDay_Term(void) {
    s32 i;

    MsgWin_Term();
    Dialog_Term();
    for (i = 0; i < SIMDAY_FLASH_NUM; i++) {
        Flash_Destroy(&gSimDay->flash[i]);
    }
    if (gSimDay->faceFile[0] != NULL) {
        Heap_Free(gSimDay->faceFile[0]);
        gSimDay->faceFile[0] = NULL;
    }
    if (gSimDay->faceFile[1] != NULL) {
        Heap_Free(gSimDay->faceFile[1]);
        gSimDay->faceFile[1] = NULL;
    }
    if (gSimDay->faceRes[0] != NULL) {
        Heap_Free(gSimDay->faceRes[0]);
        gSimDay->faceRes[0] = NULL;
    }
    if (gSimDay->faceRes[1] != NULL) {
        Heap_Free(gSimDay->faceRes[1]);
        gSimDay->faceRes[1] = NULL;
    }
    if (gSimDay->res != NULL) {
        Heap_Free(gSimDay->res);
        gSimDay->res = NULL;
    }
    if (gSimDay->pack != NULL) {
        Heap_Free(gSimDay->pack);
        gSimDay->pack = NULL;
    }
    if (gSimDay != NULL) {
        Heap_Free(gSimDay);
        gSimDay = NULL;
    }
}

/* Draws the screen: status plates, the window's rows and item names, the stat change on show, the movies. */
void SimDay_Draw(void) {
    MFlashRef ref;
    MFlashUv uv;
    char name[64];
    MFlashUv uvB;
    MFlashUv uvC;
    MFlash *flash;
    s32 i = 0;
    s32 n;
    s32 done;

#ifdef PORT
    Port_MenuPage(32); /* widescreen menus: this screen's page, its backdrop filling the width */
    Port_MenuFill(1);
    Sprite_DrawPicture(gSimDay->bg, 0, 0, 0x80);
    Port_MenuFill(0);
#else
    Sprite_DrawPicture(gSimDay->bg, 0, 0, 0x80);
#endif
    flash = &gSimDay->flash[SIMDAY_FL_BOARD];
    Flash_FindLabel(flash, NULL, "mc_icon_timer", &ref);
    Flash_ClipSetFlags(flash, &ref, 0x400, 1);
    sprintf(name, "mc_turn_lamp%02d", 0);
    Flash_FindLabel(flash, "mc_icon_timer", name, &ref);
    Flash_FindLabel(flash, NULL, "mc_chara_pt_plate", &ref);
    Flash_ClipSetFlags(flash, &ref, 0x400, 1);
    Num_DrawChild(flash, "mc_icon_timer", "mc_sim_time_num%02d", 0, 3, QPROG->sim.turn + 1, 0x20, 0x20, 1, 0);
    Flash_FindLabel(flash, "mc_chara_pt_plate", "mc_gauge_green", &ref);
    Flash_ClipSetFlags(flash, &ref, 0x80, 1);
    Flash_FindLabel(flash, "mc_chara_pt_plate", "mc_gauge_red", &ref);
    Flash_ClipSetFlags(flash, &ref, 0x100, 1);
    Flash_ClipSetOffset(flash, &ref, (QPROG->sim.stat[SIM_STAT_HP] - 100) * 1.55f, 0);
    Num_DrawChild(flash, "mc_chara_pt_plate", "mc_status_num%02d", 0, 3, QPROG->sim.stat[SIM_STAT_HP], 0x20, 0x20, 1,
                  0);
    /* attack: sign picture, value, and the level's maximum */
    if (QPROG->sim.stat[SIM_STAT_ATK] >= 0) {
        for (i = 0; i < 2; i++) {
            sprintf(name, "mc_at_num%02d", i + 2);
            Flash_FindLabel(flash, "mc_at_plate", name, &ref);
            Flash_ClipSetTex(flash, &ref, 0);
        }
        Num_DrawChild(flash, "mc_at_plate", "mc_at_num%02d", 2, 2, QPROG->sim.stat[SIM_STAT_ATK], 0x20, 0x20, 1, 0);
    } else {
        for (i = 0; i < 2; i++) {
            sprintf(name, "mc_at_num%02d", i + 2);
            Flash_FindLabel(flash, "mc_at_plate", name, &ref);
            Flash_ClipSetTex(flash, &ref, 1);
        }
        Num_DrawChild(flash, "mc_at_plate", "mc_at_num%02d", 2, 2, -QPROG->sim.stat[SIM_STAT_ATK], 0x20, 0x20, 1, 0);
    }
    Num_DrawChild(flash, "mc_at_plate", "mc_at_num%02d", 0, 2, gSimDay->level[QPROG->sim.level].max, 0x20, 0x20, 1,
                  0);
    /* defence */
    if (QPROG->sim.stat[SIM_STAT_DEF] >= 0) {
        for (i = 0; i < 2; i++) {
            sprintf(name, "mc_df_num%02d", i + 2);
            Flash_FindLabel(flash, "mc_df_plate", name, &ref);
            Flash_ClipSetTex(flash, &ref, 0);
        }
        Num_DrawChild(flash, "mc_df_plate", "mc_df_num%02d", 2, 2, QPROG->sim.stat[SIM_STAT_DEF], 0x20, 0x20, 1, 0);
    } else {
        for (i = 0; i < 2; i++) {
            sprintf(name, "mc_df_num%02d", i + 2);
            Flash_FindLabel(flash, "mc_df_plate", name, &ref);
            Flash_ClipSetTex(flash, &ref, 1);
        }
        Num_DrawChild(flash, "mc_df_plate", "mc_df_num%02d", 2, 2, QPROG->sim.stat[SIM_STAT_DEF], 0x20, 0x20, 1, 0);
    }
    Num_DrawChild(flash, "mc_df_plate", "mc_df_num%02d", 0, 2, gSimDay->level[QPROG->sim.level].max, 0x20, 0x20, 1,
                  0);
    Num_DrawChild(flash, "mc_point_plate", "mc_score_num%02d", 2, 7, QPROG->sim.stat[SIM_STAT_POINT], 0x20, 0x20, 1,
                  0);
    /* the window: three rows cut from one texture; row 1 is dimmed when no item is carried */
    flash = &gSimDay->flash[SIMDAY_FL_MENU];
    SimDay_SetMenuAlpha(gSimDay->menuText);
    for (i = 0; i < 3; i++) {
        uv.x0 = 0;
        uv.y0 = i << 5;
        uv.x1 = 0x200;
        uv.y1 = (i << 5) + 0x20;
        sprintf(name, "mc_menu_plate_%d", i + 1);
        Flash_FindLabel(flash, name, "mc_menu_text_on", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (i == 1 && SimDay_CountItems() == 0) {
            Flash_ClipSetColor(flash, &ref, 0.4f);
        }
        Flash_FindLabel(flash, name, "mc_menu_text_off", &ref);
        Flash_ClipSetUv(flash, &ref, &uv);
        if (i == 1 && SimDay_CountItems() == 0) {
            Flash_ClipSetColor(flash, &ref, 0.4f);
        }
    }
    /* the names of the items carried, oldest first */
    n = 0;
    for (i = 0; i < SIM_ITEM_NUM; i++) {
        s32 slot = (QPROG->sim.itemHead + i) % SIM_ITEM_NUM;

        if (QPROG->sim.item[slot] != -1) {
            sprintf(name, "mc_item_name_%d", n + 1);
            Flash_FindLabel(flash, NULL, name, &ref);
            TextBox_AttachLine(flash, &ref, 0, 0, QPROG->sim.item[slot], &gSimDay->box[n]);
            n++;
        }
    }
    /* the stat change on show */
    flash = &gSimDay->flash[SIMDAY_FL_EVENT];
    switch (gSimDay->chg.stat) {
    case SIM_STAT_ATK:
    case SIM_STAT_DEF:
        for (i = 0; i < 3; i++) {
            sprintf(name, "mc_mark%02d", i);
            Flash_FindLabel(flash, NULL, name, &ref);
            if (gSimDay->chg.mark == i) {
                Flash_ClipSetFlags(flash, &ref, 2, 1);
                Flash_FindLabel(&gSimDay->flash[SIMDAY_FL_EVENT], NULL, name, &ref);
                Flash_ClipSetTex(&gSimDay->flash[SIMDAY_FL_EVENT], &ref, gSimDay->chg.stat);
                Flash_FindLabel(&gSimDay->flash[SIMDAY_FL_EVENT], name, "mc_pt_obj", &ref);
                Flash_ClipSetTex(&gSimDay->flash[SIMDAY_FL_EVENT], &ref, gSimDay->chg.shown);
            } else {
                Flash_ClipSetFlags(flash, &ref, 2, 0);
            }
        }
        break;
    case SIM_STAT_HP:
        Flash_FindLabel(flash, NULL, "mc_pt_up_down", &ref);
        Flash_ClipSetFlags(flash, &ref, 0x400, 1);
        Num_DrawChild(flash, "mc_pt_up_down", "mc_pt_num%02d", 0, 5, gSimDay->chg.shown, 0x20, 0x20, 0, 0);
        if (gSimDay->chg.shown < 0) {
            uvB.x0 = 0x20;
            uvB.y0 = 0;
            uvB.x1 = 0x40;
            uvB.y1 = 0x20;
            Flash_FindLabel(flash, "mc_pt_up_down", "mc_pt_yajirusi", &ref);
            Flash_ClipSetUv(flash, &ref, &uvB);
            uvB.x0 = 0x20;
            uvB.y0 = 0;
            uvB.x1 = 0x40;
            uvB.y1 = 0x20;
            Flash_FindLabel(flash, "mc_pt_up_down", "mc_plus_minus", &ref);
            Flash_ClipSetUv(flash, &ref, &uvB);
            gSimDay->tex1[7] = MTEX(gSimDay->numRes, 0);
        } else {
            gSimDay->tex1[7] = MTEX(gSimDay->numRes, 1);
        }
        break;
    case SIM_STAT_POINT:
        Flash_FindLabel(flash, NULL, "mc_score_up_down", &ref);
        Flash_ClipSetFlags(flash, &ref, 0x400, 1);
        Num_DrawChild(flash, "mc_score_up_down", "mc_score_num%02d", 0, 9, gSimDay->chg.shown * 100, 0x20, 0x20, 0, 0);
        if (gSimDay->chg.shown < 0) {
            uvC.x0 = 0x20;
            uvC.y0 = 0;
            uvC.x1 = 0x40;
            uvC.y1 = 0x20;
            Flash_FindLabel(flash, "mc_score_up_down", "mc_score_yajirusi", &ref);
            Flash_ClipSetUv(flash, &ref, &uvC);
            uvC.x0 = 0x20;
            uvC.y0 = 0;
            uvC.x1 = 0x40;
            uvC.y1 = 0x20;
            Flash_FindLabel(flash, "mc_score_up_down", "mc_score_plus_minus", &ref);
            Flash_ClipSetUv(flash, &ref, &uvC);
            gSimDay->tex1[7] = MTEX(gSimDay->numRes, 0);
        } else {
            gSimDay->tex1[7] = MTEX(gSimDay->numRes, 1);
        }
        break;
    }
    if (gSimDay->state == SIMDAY_ST_ROUND) {
        Num_Draw(&gSimDay->flash[SIMDAY_FL_ROUND], "mc_round_num%02d", 0, 2, QPROG->sim.turn / SIM_TURNS + 1, 0x40,
                 0x40, 1);
    }
    /* the message window goes between the fifth movie and the window's */
    done = 0;
    for (i = 0; i < SIMDAY_FLASH_NUM; i++) {
#ifdef PORT
        /* widescreen menus: the three-row window (the pause) at its true shape, its rows' labels with it: they are
           pieces as wide as the 4:3 screen, which the page fills with (port/src/gs_marker.c) */
        int prev = i == SIMDAY_FL_MENU ? Port_MenuPart(4 /* PORT_MENU_CENTER_ALL */) : 0;
        Flash_Draw(&gSimDay->flash[i]);
        if (i == SIMDAY_FL_MENU) {
            Port_MenuPart(prev);
        }
#else
        Flash_Draw(&gSimDay->flash[i]);
#endif
        if (i == 4 && !done) {
            done = 1;
            MsgWin_Draw(0, 0, gSimDay->msgLine);
        }
    }
    Dialog_Draw(1);
#ifdef PORT
    Port_MenuPage(0);
#endif
}

/*
 * Advances the screen: reacts to the movies' triggers (turn change, monitor, stat change steps, the window)
 * and counts the change on show into the stat.
 */
void SimDay_Update(void) {
    s32 i;
    s32 stat;

    if (QPROG->flags & MPROG_FREEZE) {
        return;
    }
    if (gSimDay->timer > 0) {
        gSimDay->timer--;
    }
    if (gSimDay->flash[SIMDAY_FL_BOARD].trig & 0x20) {
        SimDay_Cmd(4);
    }
    if (gSimDay->flash[SIMDAY_FL_BOARD].trig & 1) {
        switch (gSimDay->state) {
        case SIMDAY_ST_BOARD:
            /* the board is back: the turn is over */
            if (QPROG->sim.wait == 30) {
                SimDay_SetButton(0, 3);
            }
            if (gSimDay->flags & SIMDAY_SAME_TURN) {
                gSimDay->flags &= ~SIMDAY_SAME_TURN;
            } else if (gSimDay->flags & SIMDAY_SKIP_TURNS) {
                /* five turns pass, but not beyond the round's fight */
                gSimDay->flags &= ~SIMDAY_SKIP_TURNS;
                if (gSimDay->day >= 5) {
                    QPROG->sim.turn = QPROG->sim.turn - gSimDay->day + 9;
                } else {
                    QPROG->sim.turn += 5;
                }
                SimDay_PassWait(5);
            } else {
                QPROG->sim.turn++;
                SimDay_PassWait(1);
            }
            gSimDay->day = QPROG->sim.turn % SIM_TURNS;
            SimDay_Cmd(8);
            SimDay_Cmd(12);
            SimDay_SetFaceA(0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
            for (i = 0; i < SIMDAY_TRAIN_KINDS; i++) {
                gSimDay->rank[i] = SimDay_GetTrainRank(i);
            }
            break;
        case SIMDAY_ST_SCRIPT:
            SimDay_SetFaceA(gSimDay->faceA);
            SimDay_SetFaceB(gSimDay->faceB);
            break;
        case SIMDAY_ST_VERSUS:
            SimDay_SetFaceA(12);
            SimDay_Cmd(10);
            gSimDay->msgLine = 0x58;
            break;
        }
        Snd_PlaySe(2, 0x12);
    }
    if (gSimDay->flash[SIMDAY_FL_BOARD].trig & 4) {
        gSimDay->flags &= ~SIMDAY_BUSY;
    }
    if (gSimDay->flash[SIMDAY_FL_BOARD].trig & 8) {
        SimDay_Cmd(9);
    }
    if (gSimDay->flash[SIMDAY_FL_BOARD].trig & 2) {
        SimDay_Cmd(11);
    }
    if (gSimDay->flash[SIMDAY_FL_EVENT].trig & 2) {
        switch (gSimDay->chg.stat) {
        case SIM_STAT_ATK:
        case SIM_STAT_DEF:
            /* one step of attack or defence, kept within 0 and the level's maximum */
            SimDay_Cmd(0x1C);
            QPROG->sim.stat[gSimDay->chg.stat] += gSimDay->chg.shown - 2;
            if (QPROG->sim.stat[gSimDay->chg.stat] < 0) {
                QPROG->sim.stat[gSimDay->chg.stat] = 0;
            }
            if (gSimDay->level[QPROG->sim.level].max < QPROG->sim.stat[gSimDay->chg.stat]) {
                QPROG->sim.stat[gSimDay->chg.stat] = gSimDay->level[QPROG->sim.level].max;
            }
            if (gSimDay->chg.shown - 2 < 0) {
                Snd_PlaySe(2, 0x25);
            } else {
                Snd_PlaySe(2, 0x24);
            }
            break;
        case SIM_STAT_HP:
            if (gSimDay->chg.delta[SIM_STAT_HP] < 0) {
                Snd_PlaySe(2, 0x1B);
            } else {
                Snd_PlaySe(2, 0x1A);
            }
            gSimDay->chg.applying = 1;
            break;
        case SIM_STAT_POINT:
            Snd_PlaySe(2, 0x1C);
            gSimDay->chg.applying = 1;
            break;
        }
    }
    if (gSimDay->flash[SIMDAY_FL_EVENT].trig & 1) {
        if (SimDay_NextChange() == -1) {
            gSimDay->state = SIMDAY_ST_SCRIPT;
        } else {
            SimDay_Cmd(0x1A);
        }
    }
    if (gSimDay->flash[SIMDAY_FL_EVENT].trig & 4) {
        gSimDay->state = SIMDAY_ST_SCRIPT;
    }
    if (gSimDay->flash[SIMDAY_FL_EVENT].trig & 8) {
        /* the level-up animation ended */
        gSimDay->chg.mask &= ~0x10;
        SimDay_Cmd(0);
        if (gSimDay->flags & SIMDAY_LEVEL_UP) {
            if (gSimDay->levelUp) {
                gSimDay->state = SIMDAY_ST_BOARD;
                gSimDay->levelUp = 0;
                gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
            } else {
                gSimDay->state = SIMDAY_ST_SCRIPT;
            }
            gSimDay->flags &= ~SIMDAY_LEVEL_UP;
        }
    }
    if (gSimDay->flash[SIMDAY_FL_MENU].trig & 1) {
        if (gSimDay->state == SIMDAY_ST_WINDOW) {
            gSimDay->menuText = 0;
        } else {
            gSimDay->menuText = 1;
        }
    }
    if (gSimDay->flash[SIMDAY_FL_MENU].flags & 8) {
        /* the window has closed: row 2 asks whether to quit */
        if (gSimDay->cur[gSimDay->state] == 2) {
            gSimDay->state = SIMDAY_ST_CONFIRM;
            Dialog_SetChoices(1);
            Dialog_SetCursor(1);
            Dialog_Start(0);
        } else {
            gSimDay->state = gSimDay->prevState;
            gSimDay->flags &= ~SIMDAY_WINDOW;
        }
    }
    if (gSimDay->chg.applying) {
        /* health moves by 1 a frame within 20..100 (1..100 going up), points by 7 within 0..65535 */
        stat = gSimDay->chg.stat;
        if (gSimDay->chg.delta[stat] > 0) {
            switch (stat) {
            case SIM_STAT_HP:
                if (gSimDay->chg.delta[stat] >= 2) {
                    QPROG->sim.stat[stat]++;
                    gSimDay->chg.delta[stat]--;
                } else {
                    QPROG->sim.stat[stat] += gSimDay->chg.delta[stat];
                    gSimDay->chg.delta[stat] = 0;
                }
                if (QPROG->sim.stat[stat] > 100) {
                    QPROG->sim.stat[stat] = 100;
                }
                if (QPROG->sim.stat[stat] > 20) {
                    /* above 20 the two buttons that need health open again */
                    if (QPROG->sim.off & 1) {
                        SimDay_SetButton(1, 0);
                    }
                    if (QPROG->sim.off & 2) {
                        SimDay_SetButton(1, 1);
                    }
                    if ((u32)gSimDay->cur[SIMDAY_ST_BOARD] < 2) {
                        SimDay_ClipGoto(0, 0, "fl_on_start");
                    }
                }
                break;
            case SIM_STAT_POINT:
                if (gSimDay->chg.delta[stat] >= 8) {
                    QPROG->sim.stat[stat] += 7;
                    gSimDay->chg.delta[stat] -= 7;
                } else {
                    QPROG->sim.stat[stat] += gSimDay->chg.delta[stat];
                    gSimDay->chg.delta[stat] = 0;
                }
                if (QPROG->sim.stat[stat] > 0xFFFF) {
                    QPROG->sim.stat[stat] = 0xFFFF;
                }
                break;
            }
            if (stat == SIM_STAT_POINT) {
                if (QPROG->sim.level < SIM_LEVEL_MAX + 1) {
                    if (gSimDay->level[QPROG->sim.level + 1].need < QPROG->sim.stat[SIM_STAT_POINT]) {
                        SimDay_Cmd(0x21);
                    }
                }
            }
        } else if (gSimDay->chg.delta[stat] < 0) {
            switch (stat) {
            case SIM_STAT_HP:
                if (gSimDay->chg.delta[stat] < -1) {
                    QPROG->sim.stat[stat]--;
                    gSimDay->chg.delta[stat]++;
                } else {
                    QPROG->sim.stat[stat] += gSimDay->chg.delta[stat];
                    gSimDay->chg.delta[stat] = 0;
                }
                if (QPROG->sim.stat[stat] <= 0) {
                    QPROG->sim.stat[stat] = 1;
                }
                if (QPROG->sim.stat[stat] < 20) {
                    QPROG->sim.stat[stat] = 20;
                }
                if (QPROG->sim.stat[stat] < 21) {
                    /* at 20 the two buttons that need health are greyed out and the cursor moves off them */
                    s32 moved = 0;

                    if (!(QPROG->sim.off & 1)) {
                        SimDay_SetButton(0, 0);
                        moved = 1;
                    }
                    if (!(QPROG->sim.off & 2)) {
                        SimDay_SetButton(0, 1);
                        moved = 1;
                    }
                    if (moved) {
                        gSimDay->cur[SIMDAY_ST_BOARD] = 2;
                        SimDay_ClipGoto(0, 0, "fl_on_start");
                    }
                }
                break;
            case SIM_STAT_POINT:
                if (gSimDay->chg.delta[stat] < -7) {
                    QPROG->sim.stat[stat] -= 7;
                    gSimDay->chg.delta[stat] += 7;
                } else {
                    QPROG->sim.stat[stat] += gSimDay->chg.delta[stat];
                    gSimDay->chg.delta[stat] = 0;
                }
                if (QPROG->sim.stat[stat] < 0) {
                    QPROG->sim.stat[stat] = 0;
                }
                break;
            }
        }
        if (gSimDay->chg.delta[stat] == 0 && !(gSimDay->chg.mask & 0x10)) {
            switch (stat) {
            case SIM_STAT_HP:
                SimDay_Cmd(0x1E);
                break;
            case SIM_STAT_POINT:
                SimDay_Cmd(0x20);
                break;
            }
            gSimDay->chg.applying = 0;
        }
    }
    /* while the window is open only its movie runs */
    for (i = 0; i < SIMDAY_FLASH_NUM; i++) {
        if (!(gSimDay->flags & SIMDAY_WINDOW) || i == SIMDAY_FL_MENU) {
            Flash_Advance(&gSimDay->flash[i]);
        }
    }
}

/*
 * ---- The second half of the object (formerly src/menu/menu_r.c, 0x3840E0..0x3851B0) ----
 * Its .rodata ends at 0x3B98E4 with the two jump tables of this part (0x3B9890 SimDay_UpdateTalk, 0x3B98B0
 * SimDay_Input). The strings used here are shared with the first half (0x3B9240 "mc_sim_botan%02d", 0x3B9258
 * "fl_off_start", 0x3B9288 "mc_menu_plate_%d", 0x3B92D0 "fl_on_start"). SimDay_Input only matches when the compiler
 * has seen the definitions of SimDay_CountItems, SimDay_Cmd, SimDay_PickEvent, SimDay_RefreshBoard,
 * SimDay_SetupBattle and SimDay_ListItems above it (three of its branches come out as the other kind of branch
 * otherwise), which was the evidence that the two halves are one source file. SimDay_PickEvent is called with an
 * argument (0 from the board, 1 from the training menu) that the function does not use.
 */

/* The guide's closing line: starts line `talk` (or the next one when a button cuts the current one short). */
void SimDay_UpdateTalk(void) {
    if (gSimDay->talk[0] == 0) {
        return;
    }
    if (gProgress->flags & MPROG_FREEZE) {
        return;
    }
    if (gPad[0].gamePressed & 0x200) {
        gSimDay->talk[0]++;
        Snd_PlaySe(1, 1);
    } else if (gPad[0].gamePressed & 0x400) {
        gSimDay->talk[0]++;
        Snd_PlaySe(1, 2);
    } else if (gSimDay->msgLine != -1 && Voice_GetStat(0) != SIMDAY_VOICE_IDLE && gSimDay->talk[0] == gSimDay->talk[1]) {
        return;
    }
    switch (gSimDay->talk[0]) {
    case 1:
        gSimDay->msgLine = 0x19;
        break;
    case 2:
        gSimDay->msgLine = 0x1A;
        break;
    case 3:
        gSimDay->msgLine = 0x1B;
        break;
    case 4:
        gSimDay->msgLine = 0x1C;
        break;
    case 5:
        gSimDay->msgLine = 0x1D;
        break;
    }
    Voice_PlayWithSubtitle(NULL, SIMDAY_VOICE_BASE, gSimDay->msgLine);
    gSimDay->talk[0] = 0;
    gSimDay->msgLine = -1;
    gSimDay->talk[1] = gSimDay->talk[0];
}

/*
 * Pad 0: the board's menus, the turn's event script, the window, the announcement of the fight. When the script of
 * the tenth turn of a round is over, SimDay_SetupBattle writes the battle setup and the two portraits
 * are loaded; confirm then shows the round picture and the screen leaves. Clears *result when the ladder is quit
 * from the window.
 */
void SimDay_Input(s32 *result) {
    s32 ok = gPad[0].gamePressed & 0x200;
    s32 cancel = gPad[0].gamePressed & 0x400;
    s32 up = gPad[0].gameRepeat & 8;
    s32 down = gPad[0].gameRepeat & 4;
    s32 start = gPad[0].gamePressed & 0x1000;

    if (!(gSimDay->flash[SIMDAY_FL_BOARD].flags & MFLASH_PAD)) {
        return;
    }
    if (gSimDay->state == SIMDAY_ST_WINDOW || gSimDay->state == SIMDAY_ST_WINDOW_ITEMS) {
        if (!(gSimDay->flash[SIMDAY_FL_MENU].flags & MFLASH_PAD)) {
            return;
        }
    }
    if (gSimDay->flags & SIMDAY_BUSY) {
        return;
    }
    if (!(gSimDay->flags & SIMDAY_STARTED)) {
        SimDay_RefreshBoard();
        gSimDay->flags |= SIMDAY_STARTED;
    }
    if (gSimDay->state == SIMDAY_ST_BOARD) {
        if (start) {
            SimDay_Cmd(0x14);
            return;
        }
    }
    switch (gSimDay->state) {
    case SIMDAY_ST_BOARD:
        if (up) {
            SimDay_ClipGoto(0, 0, "fl_off_start");
            do {
                gSimDay->cur[gSimDay->state]--;
                if (gSimDay->cur[gSimDay->state] < 0) {
                    gSimDay->cur[gSimDay->state] = 3;
                }
            } while ((QPROG->sim.off >> gSimDay->cur[gSimDay->state]) & 1);
            SimDay_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
        } else if (down) {
            SimDay_ClipGoto(0, 0, "fl_off_start");
            do {
                gSimDay->cur[gSimDay->state]++;
                if (gSimDay->cur[gSimDay->state] >= 4) {
                    gSimDay->cur[gSimDay->state] = 0;
                }
            } while ((QPROG->sim.off >> gSimDay->cur[gSimDay->state]) & 1);
            SimDay_ClipGoto(0, 0, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
        } else if (ok) {
            if (gSimDay->cur[gSimDay->state] != 0) {
                switch (gSimDay->cur[gSimDay->state]) {
                case 1:
                    SimDay_PickEvent(0);
                    break;
                case 2:
                    SimDay_PickEvent(0);
                    break;
                case 3:
                    QPROG->sim.wait = 0x1E;
                    SimDay_PickEvent(0);
                    gSimDay->cur[gSimDay->state] = 0;
                    SimDay_ClipGoto(0, 0, "fl_on_start");
                    break;
                }
                SimDay_Cmd(0xD);
                gSimDay->state = SIMDAY_ST_SCRIPT;
            } else {
                gSimDay->state = SIMDAY_ST_TRAIN;
                SimDay_Cmd(0xE);
                gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x54;
            }
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMDAY_ST_TRAIN:
        if (up) {
            SimDay_ClipGoto(0, 1, "fl_off_start");
            if (--gSimDay->cur[gSimDay->state] < 0) {
                gSimDay->cur[gSimDay->state] = 2;
            }
            SimDay_ClipGoto(0, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x54;
        } else if (down) {
            SimDay_ClipGoto(0, 1, "fl_off_start");
            if (++gSimDay->cur[gSimDay->state] >= 3) {
                gSimDay->cur[gSimDay->state] = 0;
            }
            SimDay_ClipGoto(0, 1, "fl_on_start");
            Snd_PlaySe(1, 0);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x54;
        } else if (ok) {
            gSimDay->state = SIMDAY_ST_SCRIPT;
            SimDay_Cmd(0xF);
            SimDay_PickEvent(1);
            Snd_PlaySe(1, 1);
        } else if (cancel) {
            SimDay_Cmd(0x10);
            Snd_PlaySe(1, 2);
            gSimDay->msgLine = gSimDay->cur[gSimDay->state] + 0x50;
        }
        break;
    case SIMDAY_ST_WINDOW:
        if (up) {
            SimDay_ClipGoto(5, 6, "fl_off_start");
            if (--gSimDay->cur[gSimDay->state] < 0) {
                gSimDay->cur[gSimDay->state] = 2;
            }
            if (gSimDay->cur[gSimDay->state] == 1 && !SimDay_CountItems()) {
                gSimDay->cur[gSimDay->state]--;
            }
            SimDay_ClipGoto(5, 6, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (down) {
            SimDay_ClipGoto(5, 6, "fl_off_start");
            if (++gSimDay->cur[gSimDay->state] >= 3) {
                gSimDay->cur[gSimDay->state] = 0;
            }
            if (gSimDay->cur[gSimDay->state] == 1 && !SimDay_CountItems()) {
                gSimDay->cur[gSimDay->state]++;
            }
            SimDay_ClipGoto(5, 6, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (ok) {
            s32 row = gSimDay->cur[gSimDay->state];

            if (row == 1) {
                if (SimDay_CountItems()) {
                    SimDay_Cmd(0x16);
                    gSimDay->state = SIMDAY_ST_WINDOW_ITEMS;
                    gSimDay->menuText = row;
                    Snd_PlaySe(1, 1);
                } else {
                    Snd_PlaySe(1, 7);
                }
            } else {
                SimDay_Cmd(0x15);
                Snd_PlaySe(1, 1);
            }
        } else if (start) {
            gSimDay->cur[gSimDay->state] = 0;
            SimDay_Cmd(0x15);
        }
        break;
    case SIMDAY_ST_WINDOW_ITEMS:
        if (ok) {
            gSimDay->menuText = 0;
            SimDay_Cmd(0x17);
            gSimDay->state = SIMDAY_ST_WINDOW;
        }
        break;
    case SIMDAY_ST_CONFIRM: {
        s32 answer;

        Dialog_SetMsg(0xE);
        answer = Dialog_Input(0);
        if (answer > 0) {
            gSimDay->state = SIMDAY_ST_QUIT;
            Dialog_SetChoices(0);
            Dialog_Start(1);
        } else if (answer < 0) {
            gSimDay->state = SIMDAY_ST_BACK;
            Dialog_SetChoices(0);
            Dialog_Start(1);
        }
        break;
    }
    case SIMDAY_ST_BACK:
        if (Dialog_IsClosed()) {
            SimDay_Cmd(0x14);
        }
        break;
    case SIMDAY_ST_SELECT:
        if (up) {
            SimDay_ClipGoto(0, 2, "fl_off_start");
            if (--gSimDay->cur[gSimDay->state] < 0) {
                gSimDay->cur[gSimDay->state] = 1;
            }
            SimDay_ClipGoto(0, 2, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (down) {
            SimDay_ClipGoto(0, 2, "fl_off_start");
            if (++gSimDay->cur[gSimDay->state] >= 2) {
                gSimDay->cur[gSimDay->state] = 0;
            }
            SimDay_ClipGoto(0, 2, "fl_on_start");
            Snd_PlaySe(1, 0);
        } else if (ok) {
            if (gSimDay->cur[gSimDay->state] == 0) {
                gSimDay->unkB8C = 1;
            } else {
                gSimDay->unkB8C = 0;
            }
            SimDay_Cmd(0x12);
            Snd_PlaySe(1, 1);
        }
        break;
    case SIMDAY_ST_SCRIPT:
        if (gSimDay->flags & SIMDAY_WAIT_KEY) {
            if (ok) {
                gSimDay->flags &= ~SIMDAY_WAIT_KEY;
                Snd_PlaySe(1, 1);
            }
        } else if (SimEvent_Run(gSimDay, gSimDay->event)) {
            if (gSimDay->day == SIM_TURNS - 1) {
                gSimDay->state = SIMDAY_ST_VERSUS;
                SimDay_SetupBattle();
                gSimDay->loadState = SIMDAY_LOAD_REQUEST;
            } else {
                gSimDay->state = SIMDAY_ST_BOARD;
            }
            SimDay_ListItems();
            SimDay_Cmd(5);
        }
        break;
    case SIMDAY_ST_VERSUS:
        if (!(gSimDay->flags & SIMDAY_FACES_READY)) {
            return;
        }
        if (ok) {
            Flash_Play(&gSimDay->flash[SIMDAY_FL_ROUND], 1);
            Snd_PlaySe(2, 0x14);
            gSimDay->state = SIMDAY_ST_ROUND;
            Snd_PlaySe(1, 1);
            if (QPROG->sim.turn == 0x45) {
                gSimDay->talk[0] = 5;
            } else if (QPROG->member.chara == 0x66) {
                gSimDay->talk[0] = 4;
            } else {
                gSimDay->talk[0] = Rand_Range(3) + 1;
            }
        }
        break;
    case SIMDAY_ST_ROUND:
        gSimDay->state = SIMDAY_ST_ROUND_WAIT;
        gSimDay->wait = 300;
        break;
    case SIMDAY_ST_ROUND_WAIT:
        gSimDay->wait--;
        if (ok || gSimDay->wait == 0) {
            Bgm_FadeOutStep();
            Voice_StopWithLip();
            gSimDay->flags |= SIMDAY_DONE;
            gSimDay->flags |= SIMDAY_LEAVING;
            gSimDay->timer = 15;
            if (ok) {
                Snd_PlaySe(1, 1);
            }
        }
        break;
    case SIMDAY_ST_QUIT:
        ColorFade_StartOut(0, 0, 0, 0x14);
        *result = 0;
        break;
    }
}

/* Sends the plate under the cursor of state `kind` (0 board, 1 trainings, 2 two-row choice, 6 window) to `label`. */
void SimDay_ClipGoto(s32 movie, s32 kind, char *label) {
    MFlashRef ref;
    char name[0x40];
    MFlash *flash = &gSimDay->flash[movie];

    switch (kind) {
    case 0:
        sprintf(name, "mc_sim_botan%02d", gSimDay->cur[SIMDAY_ST_BOARD]);
        break;
    case 6:
        sprintf(name, "mc_menu_plate_%d", gSimDay->cur[SIMDAY_ST_WINDOW] + 1);
        break;
    case 1:
        sprintf(name, "mc_sim_botan%02d", gSimDay->cur[SIMDAY_ST_TRAIN] + 6);
        break;
    case 2:
        sprintf(name, "mc_sim_botan%02d", gSimDay->cur[SIMDAY_ST_SELECT] + 4);
        break;
    }
    Flash_FindLabel(flash, NULL, name, &ref);
    Flash_ClipGotoLabel(flash, &ref, label);
}

/* Loads the two portraits of the round picture in the background, one step per frame. */
void SimDay_UpdateFaceLoad(void) {
    s32 chara[2] = { 0 };
    MTexRes *res;

    switch (gSimDay->loadState) {
    case SIMDAY_LOAD_REQUEST:
        gSimDay->tex6[14] = NULL;
        gSimDay->tex6[13] = NULL;
        chara[0] = QPROG->member.chara;
        chara[1] = gSimDay->enemyChara;
        File_CancelRequests();
        File_Request(chara[0] + SIMDAY_FACE_FILE, gSimDay->faceFile[0], SIMDAY_FACE_SIZE);
        File_Request(chara[1] + SIMDAY_FACE_FILE, gSimDay->faceFile[1], SIMDAY_FACE_SIZE);
        gSimDay->loadState = SIMDAY_LOAD_READ;
        break;
    case SIMDAY_LOAD_READ:
        if (File_UpdateRequests()) {
            gSimDay->loadState = SIMDAY_LOAD_UNPACK;
        }
        break;
    case SIMDAY_LOAD_UNPACK:
        Sprite_Unpack(gSimDay->faceFile[0], gSimDay->faceRes[0], NULL);
        Sprite_Unpack(gSimDay->faceFile[1], gSimDay->faceRes[1], NULL);
        res = gSimDay->faceRes[0];
        Res_RelocateOffsets(&res, res, res);
        gSimDay->tex6[14] = res->tex;
        res = gSimDay->faceRes[1];
        Res_RelocateOffsets(&res, res, res);
        gSimDay->tex6[13] = res->tex;
        gSimDay->flags |= SIMDAY_FACES_READY;
        gSimDay->loadState = SIMDAY_LOAD_IDLE;
        break;
    }
}

/*
 * The board screen (mode 22). Returns 1 when the round's fight was set up (the handler then sets mode 23 and
 * leaves the overlay for the battle), 0 when the ladder was quit from the window (mode 13).
 */
s32 SimDay_Run(s32 section) {
    s32 result = 1;

    SimDay_Init(section);
    ColorFade_StartIn(0, 0, 0, 0x14);
    while (1) {
        Gfx_BeginFrame();
        Pad_Update();
        Snd_Update();
        ColorFade_Update();
        SimDay_UpdateFaceLoad();
        SimDay_Update();
        SimDay_UpdateTalk();
        SimDay_Draw();
        ColorFade_Draw();
        Gfx_EndFrame(1);
        Dma_Flush();
        File_Stub264D90();
        if (ColorFade_IsInDone()) {
            if (!(gSimDay->flags & SIMDAY_GREETED) && (gSimDay->flash[SIMDAY_FL_BOARD].flags & MFLASH_PAD)) {
                gSimDay->flags |= SIMDAY_GREETED;
            }
        }
        if (ColorFade_IsFadingOut()) {
            Bgm_FadeOutStep();
            Voice_FadeOutStep(0);
            continue;
        }
        if (ColorFade_IsOutDone()) {
            if (gSimDay->loadState != SIMDAY_LOAD_IDLE) {
                continue;
            }
            break;
        }
        if (gSimDay->flags & SIMDAY_LEAVING) {
            if (gSimDay->timer == 0) {
                ColorFade_StartOut(0, 0, 0, 0x14);
            }
        } else if (gSimDay->talk[0] == 0) {
            SimDay_Input(&result);
        }
    }
    QPROG->sim.off = 0;
    SimDay_Term();
    Dma_ResetBuffers();
    return result;
}
