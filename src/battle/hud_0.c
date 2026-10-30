#include "common.h"
#include "battle/hud_0.h"
#include "battle/battle.h"
#include "sys/pad.h"

/*
 * Battle pause menu, part 1. Source range 0x2129C8-0x213238.
 *
 * The first three functions are the end of the battle-level wrappers of btl_tech_b.c (BtlGame_*). The rest is
 * the pause menu proper: it owns no state of its own except gPauseMenuWhich; the menu trees are data
 * (0x2C4E70-0x2C6070) and the engine that runs them is hud_0_b.c (BtlMenu_*).
 *
 * PauseMenu_Update(pad, which) is called by the battle sequence: with which = 0 from the fight state while
 * BATTLE_FLAG_PAUSE_MENU is set (the pause menu), with which = 1 from the end state in battle mode 0 (the
 * result menu). It returns 0 once the menu has closed itself (only the pause menu can: triangle / start on its
 * top level); it clears BATTLE_FLAG_PAUSE itself in that case. A picked item of type "result" ends the battle
 * through BattleResult_Set(BTL_RESULT_ABORT, reason); the reason bit is chosen by the item id:
 *
 *   id 0x0C, 0x13  0x8000      id 0x0D  0x10       id 0x0E  0x80      id 0x0F  0x08      id 0x10  0x100
 *   id 0x11        0x400       id 0x12  0x200      id 0x14  0x40 (pad 0: 0x20)            id 0x15  0x20
 *   id 0x16        nothing     id 0x17  0x1000     id 0x18, 0x19  0x10000                 id 0x1A  0x20
 *
 * (0x8000 / 0x10000 are the two restart bits, BTL_REASON_RESTART.)
 */

extern s32 gBtlGameReplayActive;
extern s32 gPauseMenuWhich;

extern BtlMenu gPauseMenuVersus;
extern BtlMenu gPauseMenuResultVersus;
extern BtlMenu gPauseMenuReplayRec;
extern BtlMenu gPauseMenuResultReplayRec;
extern BtlMenu gPauseMenuReplayPlay;
extern BtlMenu gPauseMenuResultReplayPlay;
extern BtlMenu gPauseMenuMode1;
extern BtlMenu gPauseMenuResultMode1;
extern BtlMenu gPauseMenuMode2;
extern BtlMenu gPauseMenuMode3;
extern BtlMenu gPauseMenuMode4;
extern BtlMenu gPauseMenuMode5;
extern BtlMenu gPauseMenuMode6;
extern BtlMenu gPauseMenuMode7;
/* The cursor words (BtlMenu + 0x18) of the yes / no box at 0x2C4F00 and of the CPU level menu at 0x2C5158. */
extern s32 gPauseMenuConfirmCursor[8];
extern s32 gPauseMenuCpuLevelCursor[8];

/* Local view of the battle object: only the skill-list script pointer is used. */
typedef struct PauseMenuObj {
    /* 0x00 */ u8 unk0[0xBC];
    /* 0xBC */ u16 *skillText;
} PauseMenuObj;

extern void Hud_Draw(void); /* Hud_Draw (config/symbols/hud_a.txt, not linked yet) */
extern void BtlScript_UpdateView(void);
extern s32 BtlSeq_IsFighting(void);
extern s32 Battle_GetMode(void);
extern s32 BattleReplay_IsActive(void);
extern s32 BattleReplay_IsLoaded(void);
extern s32 BattleSide_GetObjId(s32 side);
extern PauseMenuObj *BtlObj_Get(s32 id);
extern void BattleResult_Set(s32 flags, s32 reason);
extern s32 Snd_PlaySe(u32 mask, s32 id);
extern void Adx_ResumeSeVoice(void);
extern void FontIcon_ResetAnim(void);
extern s32 BtlAiMgr_GetLevel(s32 side);
extern void BtlAiMgr_SetLevel(s32 side, s32 level);
extern s32 CpuLevel_ToSetting(s32 level);   /* internal CPU level -> menu difficulty 0..4, -1 when off */
extern s32 CpuLevel_FromSetting(s32 setting); /* menu difficulty 0..4 -> internal CPU level 0, 6, 13, 21, 29 */

/* Draws the battle's 2D layers: the HUD, the script's view update, then the pause menu. */
void BtlGame_Draw(void) {
    Hud_Draw();
#ifdef PORT
    {
        extern void Port_GsMarker(s32 effect);
        Port_GsMarker(0x13); /* PORT_2D_END: what follows is not part of the HUD */
    }
#endif
    BtlScript_UpdateView();
    PauseMenu_Draw();
}

/* 1 while the battle sequence is in the fight state. */
s32 BtlGame_IsFighting(void) {
    return BtlSeq_IsFighting();
}

/* 1 when the battle is a replay being played (latched by BtlGame_Reset). */
s32 BtlGame_IsReplay(void) {
    return gBtlGameReplayActive;
}

/* Returns the menu tree for the battle: which = 0 the pause menu, 1 the result menu (modes 0 and 1 only). */
BtlMenu *PauseMenu_GetMenu(s32 which) {
    BtlMenu *menu = NULL;

    switch (which) {
    case 0:
        switch (Battle_GetMode()) {
        case 0:
            if (!BattleReplay_IsActive()) {
                menu = &gPauseMenuVersus;
            } else if (!BattleReplay_IsLoaded()) {
                menu = &gPauseMenuReplayRec;
            } else {
                menu = &gPauseMenuReplayPlay;
            }
            break;
        case 1:
            menu = &gPauseMenuMode1;
            break;
        case 2:
            menu = &gPauseMenuMode2;
            break;
        case 3:
            menu = &gPauseMenuMode3;
            break;
        case 4:
            menu = &gPauseMenuMode4;
            break;
        case 5:
            menu = &gPauseMenuMode5;
            break;
        case 6:
            menu = &gPauseMenuMode6;
            break;
        case 7:
            menu = &gPauseMenuMode7;
            break;
        }
        break;
    case 1:
        switch (Battle_GetMode()) {
        case 0:
            if (!BattleReplay_IsActive()) {
                menu = &gPauseMenuResultVersus;
            } else if (!BattleReplay_IsLoaded()) {
                menu = &gPauseMenuResultReplayRec;
            } else {
                menu = &gPauseMenuResultReplayPlay;
            }
            break;
        case 1:
            menu = &gPauseMenuResultMode1;
            break;
        }
        break;
    }
    return menu;
}

/* Item callback of the yes / no box: when the item is confirmed, clears the values of items 1 and 0 and puts
   the box's cursor on its third line. */
s32 PauseMenu_ConfirmFunc(BtlMenuEvent *ev) {
    if (ev->selected) {
        BtlMenu *menu = PauseMenu_GetMenu(gPauseMenuWhich);

        BtlMenu_SetValue(menu, 1, 0);
        BtlMenu_SetValue(menu, 0, 0);
        gPauseMenuConfirmCursor[0] = 2;
    }
    return 0;
}

/* Item callback of the skill list: left / right change the page (wrapping), up / down move the entry cursor
   (clamped) and scroll the 7-line window. Reads the owner pad's repeat word. */
s32 PauseMenu_SkillListFunc(BtlMenuEvent *ev) {
    BtlMenuWork *work = BtlMenu_GetWork();

    if (ev->menu->state == BTL_MENU_STATE_ACTIVE) {
        BtlMenuList *list = &work->list[work->pad];
        u32 pad = gPad[work->pad].gameRepeat;
        s32 last = list->pages - 1;
        s32 page = list->page;
        s32 move;
        s32 max;
        s32 scroll;
        s32 cursor;
        s32 old;
        s32 lim;

        if (pad & PADG_RIGHT) {
            page++;
            Snd_PlaySe(1, 0);
        }
        if (pad & PADG_LEFT) {
            page--;
            Snd_PlaySe(1, 0);
        }
        if (page > last) {
            page = 0;
        }
        if (page < 0) {
            page = last;
        }
        move = 0;
        if (pad & PADG_UP) {
            move = -1;
        }
        if (pad & PADG_DOWN) {
            move++;
        }
        scroll = list->scroll[page];
        cursor = list->cursor[page];
        max = list->count[page] - 1;
        lim = scroll + 6;
        old = cursor;
        if (move != 0) {
            if (move < 0) {
                if (cursor != 0) {
                    Snd_PlaySe(1, 0);
                }
            } else {
                if (cursor != max) {
                    Snd_PlaySe(1, 0);
                }
            }
            cursor += move;
        }
        if (cursor > max) {
            cursor = max;
        }
        if (cursor < 0) {
            cursor = 0;
        }
        if (cursor < scroll) {
            scroll = cursor;
        }
        if (cursor > lim) {
            scroll = cursor - 6;
        }
        list->scroll[page] = scroll;
        list->cursor[page] = cursor;
        list->page = page;
        if (cursor != old) {
            FontIcon_ResetAnim();
        }
    }
    return 0;
}

/* Item callback of the CPU level menu: on confirm, clears its six lines and puts the cursor on the current
   level of side 1 (line 1 = off); while the menu is active, writes the cursor's level back every frame. */
s32 PauseMenu_CpuLevelFunc(BtlMenuEvent *ev) {
    s32 level;

    if (ev->selected) {
        BtlMenu *menu = PauseMenu_GetMenu(gPauseMenuWhich);

        BtlMenu_SetValue(menu, 2, 0);
        BtlMenu_SetValue(menu, 3, 0);
        BtlMenu_SetValue(menu, 4, 0);
        BtlMenu_SetValue(menu, 5, 0);
        BtlMenu_SetValue(menu, 6, 0);
        BtlMenu_SetValue(menu, 7, 0);
        BtlMenu_SetPicked(menu, 2, 0);
        BtlMenu_SetPicked(menu, 3, 0);
        BtlMenu_SetPicked(menu, 4, 0);
        BtlMenu_SetPicked(menu, 5, 0);
        BtlMenu_SetPicked(menu, 6, 0);
        BtlMenu_SetPicked(menu, 7, 0);
        level = CpuLevel_ToSetting(BtlAiMgr_GetLevel(1));
        if (level == -1) {
            gPauseMenuCpuLevelCursor[0] = 1;
        } else {
            gPauseMenuCpuLevelCursor[0] = level + 2;
        }
    }
    if (ev->menu->state == BTL_MENU_STATE_ACTIVE) {
        BtlMenu *menu = PauseMenu_GetMenu(gPauseMenuWhich);
        s32 cursor;

        BtlMenu_GetPicked(menu, 2);
        BtlMenu_GetPicked(menu, 3);
        BtlMenu_GetPicked(menu, 4);
        BtlMenu_GetPicked(menu, 5);
        BtlMenu_GetPicked(menu, 6);
        BtlMenu_GetPicked(menu, 7);
        cursor = gPauseMenuCpuLevelCursor[0];
        if (cursor == 1) {
            level = -1;
        } else {
            level = CpuLevel_FromSetting(cursor - 2);
        }
        BtlAiMgr_SetLevel(1, level);
    }
    return 0;
}

/* Round reset: closes both trees. */
void PauseMenu_Reset(void) {
    BtlMenu_Reset(PauseMenu_GetMenu(0));
    BtlMenu_Reset(PauseMenu_GetMenu(1));
    gPauseMenuWhich = 0;
}

#ifdef PORT
/* Takes the entry with an id out of a menu (the entries behind it move up). The engine finds entries by id and
   sizes the box by the count, so nothing else has to know. */
static void PauseMenu_PortRemove(BtlMenu *menu, s32 id) {
    s32 i;
    s32 k;

    for (i = 0; i < menu->count; i++) {
        if (menu->items[i].id == id) {
            for (k = i; k + 1 < menu->count; k++) {
                menu->items[k] = menu->items[k + 1];
            }
            menu->count--;
            return;
        }
    }
}

/* A lobby's match (port/src/gs/net_lobby.c). Only the lobby's owner goes back to the lobby window, and the game's
   own way out does it: "Return to Vs Menu" (id 0xE: the versus menu is where the rules are chosen, the lobby
   window in a lobby) ends the battle for everyone, and the session ends there (menu_a_b.c). The pause menu and the
   result menu get two forms each: the owner's, with that entry ("Return to Main Menu" of the pause menu becomes
   it, with its yes / no box; the result menu keeps the one it has), and the other player's, without it and
   without "Return to Main Menu". Which form a menu has is chosen each time it runs, by the pad it runs for, the
   same on every copy (the owner's side travels with the rules). */
static BtlMenuItem sLobbyPause[2][8], sLobbyResult[2][8];
static s32 sLobbyPauseCount[2], sLobbyResultCount[2], sLobbyBuilt;

static void PauseMenu_PortLobby(void) {
    BtlMenuItem *pause = gPauseMenuVersus.items, *result = gPauseMenuResultVersus.items, back;
    s32 i, k, haveBack = 0;

    if (sLobbyBuilt || gPauseMenuVersus.count > 8 || gPauseMenuResultVersus.count > 8) {
        return; /* (built at the session's first battle, from the game's own menus: a session starts from the game's
                   first moment, where this is 0 again) */
    }
    for (i = 0; i < gPauseMenuResultVersus.count; i++) {
        if (result[i].id == 0xE) {
            back = result[i];
            haveBack = 1;
        }
    }
    for (k = 0; k < 2; k++) {
        sLobbyPauseCount[k] = sLobbyResultCount[k] = 0;
        for (i = 0; i < gPauseMenuVersus.count; i++) {
            if (pause[i].id != 0xF) {
                sLobbyPause[k][sLobbyPauseCount[k]++] = pause[i];
            } else if (k == 1 && haveBack) {
                sLobbyPause[k][sLobbyPauseCount[k]] = pause[i]; /* (its yes / no box stays) */
                sLobbyPause[k][sLobbyPauseCount[k]].id = 0xE;
                sLobbyPause[k][sLobbyPauseCount[k]].text[0] = 0x6FFE; /* "Back to the Lobby" (hud_0_b.c) */
                sLobbyPause[k][sLobbyPauseCount[k]].text[1] = 0x6FFE;
                sLobbyPauseCount[k]++;
            }
        }
        for (i = 0; i < gPauseMenuResultVersus.count; i++) {
            /* (and its title picture, a row with a negative text number: BtlMenu_Draw puts the first line of words
               one row down for it, so without it the words sat a row low and the last one below the box) */
            if (result[i].text[0] < 0 || result[i].id == 0x13 || result[i].id == 0xD || (k == 1 && result[i].id == 0xE)) {
                sLobbyResult[k][sLobbyResultCount[k]] = result[i];
                if (result[i].id == 0xE) { /* "Return to Vs Menu" reads "Back to the Lobby" (hud_0_b.c) */
                    sLobbyResult[k][sLobbyResultCount[k]].text[0] = 0x6FFE;
                    sLobbyResult[k][sLobbyResultCount[k]].text[1] = 0x6FFE;
                }
                sLobbyResultCount[k]++;
            }
        }
    }
    sLobbyBuilt = 1;
}

/* The form of the menu for the pad it runs for (a lobby's match only). */
static void PauseMenu_PortLobbyPick(BtlMenu *menu, s32 pad) {
    extern int Port_NetLobbyOwnerSide(void);
    s32 owner = Port_NetLobbyOwnerSide(), mine;

    if (owner < 0 || !sLobbyBuilt) {
        return;
    }
    mine = pad == owner;
    if (menu == &gPauseMenuVersus) {
        menu->items = sLobbyPause[mine];
        menu->count = sLobbyPauseCount[mine];
    } else if (menu == &gPauseMenuResultVersus) {
        menu->items = sLobbyResult[mine];
        menu->count = sLobbyResultCount[mine];
    }
}
#endif

#ifdef PORT
/* The battle's menu work exists (between PauseMenu_Init and PauseMenu_Term): a spectator's own pause menu
   (port/src/spec_menu.c) is drawn with it. Set the same way on every copy. */
s32 gPortBtlMenuLive;
#endif

#ifdef PORT
/* The versus pause menu's own yes / no box ("EXIT ?", Yes, No: a sub menu of three, its title a picture), for a
   spectator's own pause menu to ask the same way (port/src/spec_menu.c); NULL when there is none. */
BtlMenu *PauseMenu_PortConfirmBox(void) {
    s32 i;

    for (i = 0; i < gPauseMenuVersus.count; i++) {
        BtlMenu *sub = gPauseMenuVersus.items[i].sub;

        if (sub != NULL && sub->count == 3 && sub->items[0].text[0] < 0) {
            return sub;
        }
    }
    return NULL;
}
#endif

/* Battle start: allocates the menu work. */
void PauseMenu_Init(void) {
#ifdef PORT
    {
        /* Online the menu at the end of a battle does without three of its six entries (asked for by the user):
           "Return to Vs Menu" (id 0xE; a session has no versus menu, the host sets the rules in the lobby window),
           "View Replay Data" (id 0x18) and "Save Replay Data" (id 0x17). Done on both copies at every battle start (the first time takes them
           out, later ones find nothing); the program is started afresh after a session, so offline has them. */
        extern int Port_NetActive(void);
        extern int Port_NetLobbyOwnerSide(void);
        if (Port_NetActive() && Port_NetLobbyOwnerSide() >= 0) {
            PauseMenu_PortLobby(); /* a lobby's match: the menus' two forms (above) */
        } else if (Port_NetActive()) {
            PauseMenu_PortRemove(&gPauseMenuResultVersus, 0xE);
            PauseMenu_PortRemove(&gPauseMenuResultVersus, 0x18);
            PauseMenu_PortRemove(&gPauseMenuResultVersus, 0x17);
        }
    }
    {
        /* The pause menu of the story mode (battle mode 1: Continue, Skill List, quit) gets the restart entry that
           the pause menu of mode 6 has (id 0xC, with its yes / no box: PauseMenu_Update ends the battle with the
           restart reason, and Battle_Loop starts it again), in front of the quit entry. */
        static BtlMenuItem sItems[5];

        if (gPauseMenuMode1.count == 4 && gPauseMenuMode6.count > 4 && gPauseMenuMode6.items[4].id == 0xC) {
            sItems[0] = gPauseMenuMode1.items[0];
            sItems[1] = gPauseMenuMode1.items[1];
            sItems[2] = gPauseMenuMode1.items[2];
            sItems[3] = gPauseMenuMode6.items[4];
            sItems[3].text[0] = sItems[3].text[1] = 3; /* "Fight Again", the versus result menu (in mode 6 the entry reads "Reset standing position") */
            sItems[4] = gPauseMenuMode1.items[3];
            gPauseMenuMode1.items = sItems;
            gPauseMenuMode1.count = 5;
        }
    }
#endif
    BtlMenu_Init();
#ifdef PORT
    gPortBtlMenuLive = 1;
#endif
}

/* Battle end: frees it. */
void PauseMenu_Term(void) {
#ifdef PORT
    gPortBtlMenuLive = 0;
#endif
    BtlMenu_Term();
}

/* One frame of the pause menu (which = 0) or the result menu (which = 1) for controller port `pad`.
   Returns 0 when the menu closed (pause flag cleared, voices resumed), else 1. */
s32 PauseMenu_Update(s32 pad, s32 which) {
    void (*set)(s32, s32);
    BtlMenu *menu;
    BtlMenuItem *item;

    gPauseMenuWhich = which;
    menu = PauseMenu_GetMenu(which);
#ifdef PORT
    PauseMenu_PortLobbyPick(menu, pad);
#endif
    BtlMenu_SetScript(BtlObj_Get(BattleSide_GetObjId(pad))->skillText);
    set = BattleResult_Set;
    if (BtlMenu_Update(which, NULL, menu, pad)) {
        Battle_GetWork()->flags &= ~BATTLE_FLAG_PAUSE;
        BtlMenu_ClearActive();
        Adx_ResumeSeVoice();
        return 0;
    }
    BtlMenu_SetTop(menu);
    item = BtlMenu_FindResult(NULL, menu);
    if (item != NULL) {
        Snd_PlaySe(1, 1);
        switch (item->id) {
        case 0xC:
            set(BTL_RESULT_ABORT, 0x8000);
            break;
        case 0x13:
            set(BTL_RESULT_ABORT, 0x8000);
            break;
        case 0xD:
            set(BTL_RESULT_ABORT, 0x10);
            break;
        case 0xE:
            set(BTL_RESULT_ABORT, 0x80);
            break;
        case 0xF:
            set(BTL_RESULT_ABORT, 8);
            break;
        case 0x10:
            set(BTL_RESULT_ABORT, 0x100);
            break;
        case 0x12:
            set(BTL_RESULT_ABORT, 0x200);
            break;
        case 0x11:
            set(BTL_RESULT_ABORT, 0x400);
            break;
        case 0x14:
            if (pad == 0) {
                set(BTL_RESULT_ABORT, 0x20);
            } else {
                set(BTL_RESULT_ABORT, 0x40);
            }
            break;
        case 0x17:
            set(BTL_RESULT_ABORT, 0x1000);
            break;
        case 0x18:
            set(BTL_RESULT_ABORT, 0x10000);
            break;
        case 0x19:
            set(BTL_RESULT_ABORT, 0x10000);
            break;
        case 0x1A:
            set(BTL_RESULT_ABORT, 0x20);
            break;
        case 0x15:
            set(BTL_RESULT_ABORT, 0x20);
            break;
        case 0x16:
            break;
        }
    }
    return 1;
}

/* Draws the menu when it is shown. */
void PauseMenu_Draw(void) {
#ifdef PORT
    {
        /* Online the pause menu belongs to the one who opened it (the fight goes on under it, btl_seq.c): it is
           not drawn on the other screen. The result menu (which = 1) is drawn for both as before. */
        extern int Port_NetActive(void);
        extern int Port_NetMe(void);
        extern s32 BtlPause_GetMenuPad(void);
        if (Port_NetActive() && gPauseMenuWhich == 0 && (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) &&
            BtlPause_GetMenuPad() != Port_NetMe()) {
            return;
        }
    }
#endif
    BtlMenu_DrawAll();
}
