#include "common.h"

/*
 * Memory-card dialog flows: see include/sys/mcflow_a.h for the overview, docs for each state machine.
 * All 22 functions match. The object starts here: its .sdata begins with gMcFlow (0x2FE940), its .rodata
 * with the two card titles (0x2EBD30).
 */
#include "sys/mcflow_a.h"
#include "sys/heap.h"
#include "sys/pad.h"

/* One entry of the replay slot list in the progress block (what the replay menu shows). */
typedef struct McFlowSlotInfo {
    /* 0x00 */ s32 flags;        /* bit 0: the entry was filled in */
    /* 0x04 */ s32 chara[2][5];
} McFlowSlotInfo; /* size 0x2C */

/* The part of the progress block this module touches (local view). */
typedef struct McFlowProgress {
    /* 0x000 */ s32 unk0;
    /* 0x004 */ s32 fileBase;    /* first id of the common files */
    /* 0x008 */ s32 unk8[3];
    /* 0x014 */ s32 flags;       /* bit 2: "continue without saving" was chosen at boot; bit 3: a save was loaded at boot */
    /* 0x018 */ u8 unk18[0x674];
    /* 0x68C */ s32 unk68C;      /* bit 0 picks the retry question of the scan flow */
    /* 0x690 */ s32 unk690[3];
    /* 0x69C */ McFlowSlotInfo slot[8];
} McFlowProgress;

/* Card state per port, kept by the card layer (local view; the names are readings of how it is used). */
typedef struct McCardInfo {
    /* 0x00 */ s32 unk0;         /* bit 0 after a read: the file was not usable */
    /* 0x04 */ s32 callResult;
    /* 0x08 */ s32 result;       /* result of the last operation (sceMcSync) */
    /* 0x0C */ s32 type;         /* 2 = PS2 memory card */
    /* 0x10 */ s32 free;         /* free clusters */
    /* 0x14 */ s32 formatted;
    /* 0x18 */ s32 unk18;
    /* 0x1C */ s32 need;         /* clusters the save still needs (McCard_CalcNeed) */
    /* 0x20 */ s32 unk20;
} McCardInfo; /* size 0x24 */

extern McFlowProgress *gProgress;
extern McFlowSave *gSaveData;
extern McCardInfo gMcCardPort[2];
#define gMcCard gMcCardPort

/* The card layer below (0x115478..0x1198D8). Names from config/symbols/stgm_a.txt. */
extern void McCard_ResetStep(void);
extern s32 McCard_GetInfo(s32 port);
extern void McCard_Probe(s32 port);
extern s32 McCard_Format(s32 port);
extern s32 McCard_CreateSave(s32 port, McFlowReq *req);
extern s32 McCard_WriteSave(s32 port, void *buf, s32 size, McFlowReq *req);
extern s32 McCard_ReadFile(s32 port, void *buf, s32 size, McFlowReq *req);
extern s32 McCard_CheckFiles(s32 port, McFlowReq *req);
extern void McCardFile_SetNames(McFlowReq *req, char *dir, s32 slot, s32 a3, s32 kind);
extern s32 McCard_IsFatalError(s32 code);
extern s32 McCard_FindSystemSave(s32 port);
extern s32 McCard_FindSave(s32 port, McFlowReq *req);
extern s32 McCard_Checksum(void *buf, s32 size, s32 write);
extern s32 McCard_CalcNeed(s32 port, McFlowReq *req, s32 kind);

extern void *Sprite_Unpack(void *src, void *dst, s32 *rawSize);
extern void *File_LoadSync(s32 id, void *buf, s32 unused);
extern McFlowReplay *BattleReplay_GetBuffer(s32 *size);
extern s32 BattleReplay_Load(McFlowReplay *buf, s32 size);
extern void Battle_ClearWork(void);
extern void *memset(void *dst, s32 c, u32 n);
extern void SndOpt_Apply(void);   /* applies the loaded sound options (Snd_SetStereo ...) */
extern void Progress_ClearTeams(void);
extern void Progress_ClearSession(void);
extern void Save_SetDefaults(McFlowSave *save);
extern s32 Snd_PlaySe(u32 mask, s32 id);
/* sys/dialog.h, declared here so that this file does not depend on a header that is being renamed */
extern void Dialog_Draw(s32 visible);
extern void Dialog_Start(s32 cmd);
extern s32 Dialog_Input(s32 allowCancel);
extern void Dialog_SetChoices(s32 on);
extern void Dialog_SetLayout(s32 layout);
extern void Dialog_SetMsg(s32 idx);
extern void Dialog_SetTitle(s32 idx);
extern void Dialog_SetCursor(s32 choice);
extern s32 Dialog_IsClosed(void);
extern s32 Dialog_IsOpen(void);

/* Card titles, full-width Shift-JIS: "DBZ-BUDOKAI-TENKAICHI3" and "DBZ-TENKAICHI3-REPLAY-0". */
#define MCFLOW_TITLE_SAVE "\x82\x63\x82\x61\x82\x79\x81\x7C\x82\x61\x82\x74\x82\x63\x82\x6E\x82\x6A\x82\x60\x82\x68\x81\x7C\x82\x73\x82\x64\x82\x6D\x82\x6A\x82\x60\x82\x68\x82\x62\x82\x67\x82\x68\x82\x52"
#define MCFLOW_TITLE_REPLAY "\x82\x63\x82\x61\x82\x79\x81\x7C\x82\x73\x82\x64\x82\x6D\x82\x6A\x82\x60\x82\x68\x82\x62\x82\x67\x82\x68\x82\x52\x81\x7C\x82\x71\x82\x64\x82\x6F\x82\x6B\x82\x60\x82\x78\x81\x7C\x82\x4F"

/* Fragments every flow repeats. They are plain statements: use them inside braces only. */
#define MCF_CARD gMcCard[0]
/* go to a state and give it the 120-frame (4 s) time-out / minimum display time */
#define MCF_GO(st) \
    gMcFlow->state = (st); \
    gMcFlow->timer = 0x78
/* read the dialog; an answer is acted on 12 frames later */
#define MCF_INPUT() \
    gMcFlow->choice = Dialog_Input(0); \
    gMcFlow->timer = 0xC
/* while the timer runs: wait for the asynchronous card check, and for the timer */
#define MCF_POLL() \
    if (McCard_GetInfo(0) == 0) { \
        break; \
    } \
    if (gMcFlow->timer > 0) { \
        break; \
    }
#define MCF_OK_PRESSED() (gPad[0].gamePressed & 0x200)

/* 0x2FE940: the first word of this object's .sdata. */
McFlow *gMcFlow = NULL;


/* Marks a slot as present with empty teams. */
void McFlow_ClearSlotInfo(s32 kind, s32 slot) {
    s32 i, j;

    if (kind == 0) {
        gProgress->slot[slot].flags |= 1;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 5; j++) {
                gProgress->slot[slot].chara[i][j] = MCFLOW_NO_CHARA;
            }
        }
    } else if (kind == 1) {
        gProgress->slot[slot].flags |= 1;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 5; j++) {
                gProgress->slot[slot].chara[i][j] = MCFLOW_NO_CHARA;
            }
        }
    }
}

/* Copies the teams of a replay header into the slot list. */
void McFlow_CopySlotInfo(s32 kind, s32 slot) {
    s32 i, j;

    if (kind == 0) {
        gProgress->slot[slot].flags |= 1;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 5; j++) {
                gProgress->slot[slot].chara[i][j] = gMcFlow->repSave.hdr.chara[i][j];
            }
        }
    } else if (kind == 1) {
        gProgress->slot[slot].flags |= 1;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 5; j++) {
                gProgress->slot[slot].chara[i][j] = gMcFlow->slotHdr[slot].chara[i][j];
            }
        }
    }
}

/* Fills the card request for the system save (kind 0) or replay slot `slot` (kind 1). */
void McFlow_SetupRequest(s32 kind, s32 slot) {
    if (kind == 0) {
        gMcFlow->req.icon = (u8 *)gMcFlow->res + (gMcFlow->res[1] / 4 * 4);
        gMcFlow->req.iconSize = 0x9C9A;
        strcpy((char *)&gMcFlow->req.title, MCFLOW_TITLE_SAVE);
        gMcFlow->req.titleBreak = 0x18;
        McCardFile_SetNames(&gMcFlow->req, "DBZT3", -1, -1, 0);
    } else {
        gMcFlow->req.icon = (u8 *)gMcFlow->res + (gMcFlow->res[2] / 4 * 4);
        gMcFlow->req.iconSize = 0xBE8A;
        strcpy((char *)&gMcFlow->req.title, MCFLOW_TITLE_REPLAY);
        gMcFlow->req.titleBreak = 0x1E;
        gMcFlow->req.title.text[0x2D] += slot + 1;
        McCardFile_SetNames(&gMcFlow->req, "DBZT3R", slot, -1, 1);
    }
}

/* Runs the "finished" callback that fits the flags, then clears the result flags. */
void McFlow_CallDone(void) {
    s32 arg;
    McFlowCb cb;

    if (gMcFlow->flags & 8) {
        cb = gMcFlow->doneCb[0];
        arg = gMcFlow->doneArg[0];
    } else if (gMcFlow->flags & 0x100) {
        cb = gMcFlow->doneCb[2];
        arg = gMcFlow->doneArg[2];
        gMcFlow->flags ^= 0x100;
    } else {
        cb = gMcFlow->doneCb[1];
        arg = gMcFlow->doneArg[1];
    }
    if (cb != NULL) {
        cb(arg);
    }
    gMcFlow->flags &= ~8;
    gMcFlow->flags &= ~0x100;
}

/* The same for the callbacks registered per mode. */
void McFlow_CallModeDone(void) {
    s32 arg;
    McFlowCb cb;

    if (gMcFlow->flags & 8) {
        cb = gMcFlow->modeCb[gMcFlow->mode][0];
        arg = gMcFlow->modeArg[gMcFlow->mode][0];
    } else if (gMcFlow->flags & 0x100) {
        cb = gMcFlow->modeCb[gMcFlow->mode][2];
        arg = gMcFlow->modeArg[gMcFlow->mode][2];
    } else {
        cb = gMcFlow->modeCb[gMcFlow->mode][1];
        arg = gMcFlow->modeArg[gMcFlow->mode][1];
    }
    if (cb != NULL) {
        cb(arg);
    }
    gMcFlow->flags &= ~8;
    gMcFlow->flags &= ~0x100;
}

/* Runs the per-mode callback 3 (the card was removed while the window was up). */
void McFlow_CallModeRemoved(void) {
    McFlowCb cb = gMcFlow->modeCb[gMcFlow->mode][3];
    s32 arg = gMcFlow->modeArg[gMcFlow->mode][3];

    if (cb != NULL) {
        cb(arg);
    }
}


#ifdef PORT
/*
 * Quick saving (setting save_quick, on unless 0; never in online play, where both games have to run the same
 * frames): the saves (flows 0 and 4), the load at start (flow 2) and a new game's first save (flow 3) run without their
 * window; the load from the options (flow 1), and a new game after a save was loaded, ask their questions and then
 * run without the waits. The save's question
 * is answered yes, no step waits its minimum display time (the card answers at once on PC), and the window's
 * opening and closing are skipped; a save shows a small icon instead (gs/ui.cpp). A question or an error the player
 * has to see (no card, a damaged or missing save at start, a full or unformatted card, a failed write, "continue
 * without saving?") opens the window as the game does and the flow goes on from there as on the PS2.
 */
extern int Port_Setting(const char *name, int def); /* plat_settings.c */
extern int Port_NetActive(void);                    /* gs/net.c: online play */
extern s32 gMcCardCmd, gMcCardStep;                 /* src/battle/stgm_a_b.c: the card operation running, its step */
#define MCCARD_CMD_INFO 10                          /* (include/battle/stgm_a_b.h) */
extern void Port_UiSaving(int state);               /* gs/ui.cpp: the save icon (1 saving, 0 saved, -1 the window took over) */
static s32 sMcQuick;
static s32 sMcQuickState, sMcQuickPolled; /* the state of the frame before; its card check has answered */

/* The step's minimum time: a step that asks the card for its state first (MCF_POLL) is over once the card has
   answered (McCard_GetInfo at its last step), and goes on with its own card operation; another is cut to `keep`. */
static void McFlow_QuickTimer(s32 polls, s32 keep) {
    if (gMcFlow->state != sMcQuickState) {
        sMcQuickState = gMcFlow->state;
        sMcQuickPolled = 0;
    }
    if (polls) {
        if (gMcCardCmd == MCCARD_CMD_INFO && gMcCardStep >= 2) {
            sMcQuickPolled = 1;
        }
        gMcFlow->timer = sMcQuickPolled ? 0 : 1;
    } else if (gMcFlow->timer > keep) {
        gMcFlow->timer = keep;
    }
}

/* Flow 1 (Options > Load), and flow 3 (New Game) after a save was loaded: the window and its questions as the game
   has them; after yes, the checking, loading and saving pages go as soon as the card answers and "loaded" or
   "saved" stays half a second. An error page is the game's own. */
static void McFlow_QuickWindow(void) {
    switch (gMcFlow->state) {
    case 0x23:
    case 0x25:
    case 0x26:
        break;
    case 2: /* "load?" */
    case 1:
    case 0xA:
    case 0xD:
    case 0x21: /* (flow 3) "new game?", "create a save file?", "overwrite?" */
        if (gMcFlow->choice > 0) {
            McFlow_QuickTimer(0, 0);
        }
        break;
    case 4:
    case 0x13:
    case 3:
    case 0xB:
    case 0xF:
    case 0x22: /* (flow 3) */
        McFlow_QuickTimer(1, 0);
        break;
    case 6:
    case 0x1F:
        McFlow_QuickTimer(0, 1);
        break;
    case 0x14: /* (loaded) */
    case 0x10: /* (flow 3: saved) */
        McFlow_QuickTimer(0, 30);
        break;
    default:
        sMcQuick = 0;
        break;
    }
}

/* Flow 7 (the Data Center's replays: the headers of the seven files read into the list): without the window and its
   waits; a card error, or a damaged file's question, opens the window as the game has it. */
static s32 McFlow_QuickReplayScan(void) {
    switch (gMcFlow->state) {
    case 0x23: /* (no window) */
        gMcFlow->choice = 0;
        gMcFlow->index = 0;
        gMcFlow->state = gMcFlow->next;
        return 1;
    case 4:
    case 0x13:
        McFlow_QuickTimer(1, 0);
        return 0;
    case 6:
    case 0x27:
    case 0x28:
        return 0;
    case 0x25:
        if (gMcFlow->flags & 8) {
            McCard_Probe(0);
            if (McCard_IsFatalError(MCF_CARD.result)) {
                break; /* (card removed: the game's own way, below) */
            }
        }
        gMcFlow->state = 0;
        sMcQuick = 0;
        McCard_ResetStep();
        McFlow_CallModeDone();
        return 1;
    default:
        break;
    }
    sMcQuick = 0; /* the player has to see this one */
    Dialog_SetLayout(1);
    Dialog_SetCursor(1);
    Dialog_SetTitle(0);
    Dialog_Start(0);
    return 0;
}

/* Before the flow's own frame. Returns 1 when that frame is done here. */
static s32 McFlow_QuickStep(void) {
    s32 boot = gMcFlow->mode == MCFLOW_MODE_BOOT_LOAD;
    s32 fresh = gMcFlow->mode == MCFLOW_MODE_NEW_SAVE; /* (no save was loaded) */
    s32 polls = 0; /* the step asks the card for its state first (MCF_POLL) */

    if (sMcQuick == 2) {
        McFlow_QuickWindow();
        return 0;
    }
    if (sMcQuick == 3) {
        return McFlow_QuickReplayScan();
    }
    switch (gMcFlow->state) {
    case 0x23: /* (no window) */
        gMcFlow->state = boot ? 5 : fresh ? 0xA : gMcFlow->next;
        gMcFlow->choice = 0;
        return 1;
    case 0xA:
    case 0xC:
    case 0x20:
    case 0xD:
    case 0x21: /* "save?", "create a save file?", "overwrite?": yes; a new game's "overwrite?" is shown (the save on
                  the card would be lost) */
        if (boot || (fresh && gMcFlow->state == 0xD) || (!fresh && gMcFlow->state == 0xA)) {
            goto shown;
        }
        gMcFlow->choice = 1;
        break;
    case 3:
    case 0xB:
    case 0xF:
    case 0x22:
        polls = 1;
        /* fallthrough */
    case 0x1F:
    case 0x10: /* (saved) */
        if (boot) {
            goto shown;
        }
        break;
    case 4:
    case 5:
    case 0x13:
        polls = 1;
        /* fallthrough */
    case 0x14: /* (loaded) */
        if (!boot) {
            goto shown;
        }
        break;
    case 6: /* (both) */
        break;
    case 0x25:
    case 0x26:
        gMcFlow->state = 0;
        sMcQuick = 0;
        if (boot) {
            gMcFlow->flags |= 8;
        } else {
            Port_UiSaving(0);
        }
        McFlow_CallDone();
        return 1;
    default:
    shown: /* the player has to see this one */
        sMcQuick = 0;
        if (!boot) {
            Port_UiSaving(-1);
        }
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        return 0;
    }
    McFlow_QuickTimer(polls, 1);
    return 0;
}
#endif

/* Flows 0 and 4: save the system data. `next` is the question state (0xC: message 0xD, 0x20: message 0x20). */
void McFlow_UpdateSave(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        gMcFlow->state = gMcFlow->next;
        break;
    case 0x20:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x20;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 3;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_ResetStep();
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x12;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 0xC:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0xD;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 3;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_ResetStep();
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x12;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 3:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.result != 0) {
                if (gProgress->flags & 4) {
                    gProgress->flags ^= 4;
                    break;
                }
                McCard_Probe(0);
                if (McCard_IsFatalError(MCF_CARD.result)) {
                    MCF_GO(0x1B);
                    break;
                }
                gMcFlow->state = 0xE;
                Dialog_SetCursor(1);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
            }
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            if (MCF_CARD.free < 0x3D) {
                MCF_GO(0x1E);
                break;
            }
            gMcFlow->state = 6;
        } else {
            McCard_ResetStep();
            gMcFlow->state = 0x1F;
        }
        break;
    case 0xE:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0xF;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 3;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_Probe(0);
        } else if (gMcFlow->choice < -1) {
            gMcFlow->state = 0x12;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 0x17:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x18;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x18;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x12;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 0x18:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x19;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1A);
            }
            break;
        }
        if (McCard_Format(0) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x1A);
            }
        } else {
            MCF_GO(0x19);
        }
        break;
    case 0xD:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0xE;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0xF;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x12;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 0xB:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0xA;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_CreateSave(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            gMcFlow->state = 0xF;
        }
        break;
    case 0xF:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x10;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_WriteSave(0, gSaveData, 0x4000, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            MCF_GO(0x10);
        }
        break;
    case 6:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (McCard_FindSystemSave(0) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.result == 1) {
            MCF_GO(0xB);
        } else {
            MCF_GO(0xB);
        }
        break;
    case 0x1E:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1F;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x10:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x11;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->flags |= 8;
                gMcFlow->state = 0x25;
                break;
            }
        }
        if (--gMcFlow->timer >= 0) {
            break;
        }
        gMcFlow->flags |= 8;
        gMcFlow->state = 0x25;
        break;
    case 0x11:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x12;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x19:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1A;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (--gMcFlow->timer >= 0) {
            break;
        }
        MCF_GO(0xB);
        break;
    case 0x1A:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1B;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x1B:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1C;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x12:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x13;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->state = gMcFlow->next;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
        } else {
            MCF_INPUT();
        }
        break;
    case 0x1F:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (McCard_CalcNeed(0, &gMcFlow->req, 0) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.need > 0) {
            if (MCF_CARD.free < MCF_CARD.need) {
                MCF_GO(0x1E);
                break;
            }
            MCF_GO(0x21);
            Dialog_SetCursor(1);
        } else {
            MCF_GO(0xD);
            Dialog_SetCursor(1);
        }
        break;
    case 0x21:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0xE;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x22;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x12;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 0x22:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x10;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_CreateSave(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            gMcFlow->state = 0xF;
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (Dialog_IsClosed()) {
            gMcFlow->state = 0;
            McFlow_CallDone();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
#ifdef PORT
    if (sMcQuick) {
        return;
    }
#endif
    Dialog_Draw(1);
}

/* Flow 1: load the system data after asking. */
void McFlow_UpdateLoad(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        gMcFlow->state = 2;
        break;
    case 2:
        Dialog_SetChoices(1);
        gMcFlow->msg = 4;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 4;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_ResetStep();
        } else if (gMcFlow->choice < 0) {
            Dialog_SetCursor(1);
            gMcFlow->state = 0x16;
            gMcFlow->choice = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 4:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            gMcFlow->state = 6;
        } else {
            MCF_GO(0x13);
        }
        break;
    case 6:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (McCard_FindSystemSave(0) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.result == 1) {
            MCF_GO(0x1D);
        } else {
            MCF_GO(0x1C);
        }
        break;
    case 0x13:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x14;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x15);
            }
            break;
        }
        if (McCard_ReadFile(0, &gMcFlow->saveBuf, 0x4000, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x15);
            }
        } else {
            if (MCF_CARD.unk0 & 1) {
                MCF_GO(0x1D);
                break;
            }
            if (McCard_Checksum(&gMcFlow->saveBuf, 0x4000, 0) != 0) {
                MCF_GO(0x1D);
                break;
            }
            *gSaveData = gMcFlow->saveBuf;
            SndOpt_Apply();
            Progress_ClearSession();
            MCF_GO(0x14);
        }
        break;
    case 0x14:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x15;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->flags |= 8;
                gMcFlow->state = 0x25;
                break;
            }
        }
        if (--gMcFlow->timer >= 0) {
            break;
        }
        gMcFlow->flags |= 8;
        gMcFlow->state = 0x25;
        break;
    case 0x15:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x16;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x1D:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1E;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x1B:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1C;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x1C:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1D;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x16:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x17;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->state = 2;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
        } else {
            MCF_INPUT();
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (Dialog_IsClosed()) {
            gMcFlow->state = 0;
            McFlow_CallDone();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
    Dialog_Draw(1);
}

/* Flow 2: load the system data without asking (boot). */
void McFlow_UpdateBootLoad(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->timer = 0x78;
        gMcFlow->choice = 0;
        gMcFlow->state = 5;
        break;
    case 5:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (MCF_CARD.type != 2) {
                gMcFlow->state = 7;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->state = 7;
                Dialog_SetCursor(1);
            }
            break;
        }
        if (MCF_CARD.formatted == 0) {
            gMcFlow->state = 0x25;
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.type != 2) {
                gMcFlow->state = 7;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->state = 7;
                Dialog_SetCursor(1);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            gMcFlow->state = 6;
        } else {
            MCF_GO(0x13);
        }
        break;
    case 4:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (--gMcFlow->timer >= 0) {
            if (McCard_GetInfo(0) == 0) {
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->state = 7;
                Dialog_SetCursor(1);
            }
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->state = 7;
                Dialog_SetCursor(1);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            gMcFlow->state = 6;
        } else {
            MCF_GO(0x13);
        }
        break;
    case 7:
        Dialog_SetChoices(1);
        gMcFlow->msg = 6;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gProgress->flags |= 4;
            break;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        if (McCard_GetInfo(0) == 0) {
            break;
        }
        McCard_ResetStep();
        if (MCF_CARD.type == 2) {
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x25;
                break;
            }
            if (MCF_CARD.result != -1) {
                break;
            }
            MCF_GO(4);
        } else {
            gMcFlow->cardGone = 1;
            MCF_CARD.type = 0;
            MCF_CARD.free = 0;
        }
        break;
    case 8:
        Dialog_SetChoices(1);
        gMcFlow->msg = 7;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            break;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        if (McCard_GetInfo(0) == 0) {
            break;
        }
        McCard_ResetStep();
        if (gMcFlow->cardGone == 1) {
            gMcFlow->state = 7;
            Dialog_SetCursor(1);
            break;
        }
        if (MCF_CARD.type == 2) {
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x25;
                break;
            }
            if (MCF_CARD.result != -1) {
                break;
            }
            MCF_GO(4);
        } else {
            gMcFlow->cardGone = 1;
            MCF_CARD.type = 0;
            MCF_CARD.free = 0;
        }
        break;
    case 6:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (McCard_FindSystemSave(0) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->state = 7;
                Dialog_SetCursor(1);
            }
            break;
        }
        if (MCF_CARD.result == 1) {
            MCF_GO(0x1D);
            if (MCF_CARD.free < 0x3D) {
                gMcFlow->cardGone = 4;
                gMcFlow->state = 8;
                Dialog_SetCursor(1);
            }
        } else {
            gMcFlow->state = 7;
            if (MCF_CARD.free >= 0x3D) {
                gMcFlow->state = 0x25;
            } else {
                gMcFlow->cardGone = 4;
                gMcFlow->state = 8;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x13:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x14;
        if (--gMcFlow->timer >= 0) {
            if (McCard_GetInfo(0) == 0) {
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x15);
            }
            break;
        }
        if (McCard_ReadFile(0, &gMcFlow->saveBuf, 0x4000, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x15);
            }
        } else {
            if (MCF_CARD.unk0 & 1) {
                MCF_GO(0x1D);
                break;
            }
            if (McCard_Checksum(&gMcFlow->saveBuf, 0x4000, 0) != 0) {
                MCF_GO(0x1D);
                break;
            }
            *gSaveData = gMcFlow->saveBuf;
            SndOpt_Apply();
            MCF_GO(0x14);
        }
        break;
    case 0x14:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x15;
        gProgress->flags |= 8;
        if (--gMcFlow->timer >= 0) {
            break;
        }
        gMcFlow->state = 0x25;
        break;
    case 0x15:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x16;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                MCF_GO(5);
            }
        }
        break;
    case 0x1D:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1E;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x16:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x17;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->state = 5;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
        } else {
            MCF_INPUT();
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (Dialog_IsClosed()) {
            gMcFlow->state = 0;
            gMcFlow->flags |= 8;
            McFlow_CallDone();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
#ifdef PORT
    if (sMcQuick) {
        return;
    }
#endif
    Dialog_Draw(1);
}

/* Flow 3: create the system data (new game). */
void McFlow_UpdateNewSave(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        if (gProgress->flags & 8) {
            gMcFlow->state = 1;
        } else {
            gMcFlow->state = 0xA;
        }
        break;
    case 1:
        Dialog_SetChoices(1);
        gMcFlow->msg = 3;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0xA;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 9:
        Dialog_SetChoices(1);
        gMcFlow->msg = 8;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            gMcFlow->flags |= 8;
            Save_SetDefaults(gSaveData);
            McCard_Checksum(gSaveData, 0x4000, 1);
            SndOpt_Apply();
            Progress_ClearSession();
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0xA:
        Dialog_SetChoices(1);
        gMcFlow->msg = 9;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 3;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_ResetStep();
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 9;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 3:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
            }
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            if (MCF_CARD.free < 0x3D) {
                MCF_GO(0x1E);
                break;
            }
            gMcFlow->state = 6;
        } else {
            McCard_ResetStep();
            gMcFlow->state = 0x1F;
        }
        break;
    case 0x17:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x18;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x18;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            Dialog_SetCursor(1);
            gMcFlow->state = 9;
            gMcFlow->choice = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x18:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x19;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1A);
            }
            break;
        }
        if (McCard_Format(0) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x1A);
            }
        } else {
            MCF_GO(0x19);
        }
        break;
    case 0x19:
        Dialog_SetChoices(0);
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            MCF_GO(0x1B);
            break;
        }
        if (--gMcFlow->timer >= 0) {
            gMcFlow->msg = 0x1A;
            break;
        }
        MCF_GO(0xB);
        break;
    case 0x1A:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1B;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 9;
                gMcFlow->choice = 0;
            }
        }
        break;
    case 0xD:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0xE;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            MCF_GO(0x1B);
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0xF;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Save_SetDefaults(gSaveData);
            McCard_Checksum(gSaveData, 0x4000, 1);
            SndOpt_Apply();
            Progress_ClearSession();
        } else if (gMcFlow->choice < 0) {
            Dialog_SetCursor(1);
            gMcFlow->state = 9;
            gMcFlow->choice = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0xB:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0xA;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_CreateSave(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            Save_SetDefaults(gSaveData);
            McCard_Checksum(gSaveData, 0x4000, 1);
            SndOpt_Apply();
            Progress_ClearSession();
            gMcFlow->state = 0xF;
        }
        break;
    case 0xF:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x10;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_WriteSave(0, gSaveData, 0x4000, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            MCF_GO(0x10);
        }
        break;
    case 6:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (McCard_FindSystemSave(0) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.result == 1) {
            MCF_GO(0xB);
        } else {
            MCF_GO(0xB);
        }
        break;
    case 0x10:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x11;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
            }
        }
        if (--gMcFlow->timer <= 0 || (MCF_OK_PRESSED() && Dialog_IsOpen())) {
            gMcFlow->state = 0x25;
            gMcFlow->flags |= 8;
        }
        break;
    case 0x11:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x12;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 9;
                gMcFlow->choice = 0;
            }
        }
        break;
    case 0x1E:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1F;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 9;
                gMcFlow->choice = 0;
            }
        }
        break;
    case 0x1B:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1C;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 9;
                gMcFlow->choice = 0;
            }
        }
        break;
    case 0x1F:
        Dialog_SetChoices(0);
        gMcFlow->msg = 5;
        if (McCard_CalcNeed(0, &gMcFlow->req, 0) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.need > 0) {
            if (MCF_CARD.free < MCF_CARD.need) {
                MCF_GO(0x1E);
                break;
            }
            gMcFlow->state = 0x21;
            gMcFlow->timer = 0x78;
            Dialog_SetCursor(1);
        } else {
            gMcFlow->state = 0xD;
            gMcFlow->timer = 0x78;
            Dialog_SetCursor(1);
        }
        break;
    case 0x21:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0xE;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x22;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 9;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
        } else {
            MCF_INPUT();
        }
        break;
    case 0x22:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x10;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_CreateSave(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            Save_SetDefaults(gSaveData);
            McCard_Checksum(gSaveData, 0x4000, 1);
            SndOpt_Apply();
            Progress_ClearSession();
            gMcFlow->state = 0xF;
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (Dialog_IsClosed()) {
            gMcFlow->state = 0;
            McFlow_CallDone();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
#ifdef PORT
    if (sMcQuick == 1) {
        return;
    }
#endif
    Dialog_Draw(1);
}

/* Flows 7 and 9: read the header of each of the seven replay files into the slot list. */
void McFlow_UpdateReplayScan(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        gMcFlow->index = 0;
        gMcFlow->state = gMcFlow->next;
        break;
    case 0x24:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(2);
        gMcFlow->choice = 0;
        gMcFlow->index = 0;
        gMcFlow->state = gMcFlow->next;
        break;
    case 0x28:
        gMcFlow->index++;
        if (gMcFlow->index >= 7) {
            gMcFlow->flags |= 8;
            gMcFlow->state = 0x25;
            break;
        }
        gMcFlow->state = 0x27;
        /* fallthrough */
    case 0x27:
        if (gMcFlow->index == 0) {
            gMcFlow->flags &= ~0x70;
        }
        McFlow_SetupRequest(1, gMcFlow->index);
        gMcFlow->state = 4;
        gMcFlow->timer = gMcFlow->index != 0 ? 0xC : 0x78;
        gMcFlow->choice = 0;
        McCard_ResetStep();
        /* fallthrough */
    case 4:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            gMcFlow->state = 6;
        } else {
            gMcFlow->state = 0x13;
            gMcFlow->timer = 0xC;
        }
        break;
    case 6:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (McCard_FindSave(0, &gMcFlow->req) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.result == 1) {
            McFlow_ClearSlotInfo(1, gMcFlow->index);
            gMcFlow->flags |= 0x20;
            gMcFlow->state = 0x28;
        } else {
            gMcFlow->flags |= 0x40;
            gMcFlow->state = 0x28;
        }
        break;
    case 0x13:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->state = 0x28;
            }
            break;
        }
        if (McCard_ReadFile(0, &gMcFlow->slotHdr[gMcFlow->index], 0x38, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                gMcFlow->state = 0x28;
            }
        } else {
            if (MCF_CARD.unk0 & 1) {
                McFlow_ClearSlotInfo(1, gMcFlow->index);
                gMcFlow->flags |= 0x20;
                gMcFlow->state = 0x28;
                break;
            }
            if (McCard_Checksum(&gMcFlow->slotHdr[gMcFlow->index].hdrSum, 0x30, 0) != 0) {
                McFlow_ClearSlotInfo(1, gMcFlow->index);
                gMcFlow->flags |= 0x20;
                gMcFlow->state = 0x28;
                break;
            }
            gMcFlow->flags |= 0x10;
            McFlow_CopySlotInfo(1, gMcFlow->index);
            gMcFlow->state = 0x28;
        }
        break;
    case 0x1D:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x3B;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = (gProgress->unk68C & 1) ? 0x12 : 0x16;
            }
        }
        break;
    case 0x1B:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1C;
        if (gMcFlow->timer > 0) {
            gMcFlow->timer--;
            break;
        }
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = (gProgress->unk68C & 1) ? 0x12 : 0x16;
            }
        }
        break;
    case 0x1C:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x3A;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = (gProgress->unk68C & 1) ? 0x12 : 0x16;
            }
        }
        break;
    case 0x12:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x13;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->state = 0x27;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
            gMcFlow->index = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x16:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x17;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->state = 0x27;
            gMcFlow->timer = 0xC;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
            gMcFlow->index = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (gMcFlow->flags & 8) {
            McCard_Probe(0);
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->flags |= 0x80;
                gMcFlow->flags ^= 8;
                McFlow_CallModeRemoved();
                break;
            }
        }
        if (Dialog_IsClosed()) {
            if (gMcFlow->flags & 0x80) {
                gMcFlow->state = 0x23;
                gMcFlow->next = 0x1B;
                gMcFlow->flags ^= 0x80;
            } else {
                gMcFlow->state = 0;
                McFlow_CallModeDone();
            }
            McCard_ResetStep();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
#ifdef PORT
    if (sMcQuick == 3) {
        return;
    }
#endif
    Dialog_Draw(1);
}

/* Flow 5: save the replay into slot `slot`. */
void McFlow_UpdateReplaySave(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        McCard_ResetStep();
        gMcFlow->state = 0xC;
        break;
    case 0xC:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x2F;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x3;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_ResetStep();
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            gMcFlow->flags |= 0x100;
        } else {
            MCF_INPUT();
        }
        break;
    case 3:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
            }
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            if (MCF_CARD.free < 0xA0) {
                MCF_GO(0x1E);
                break;
            }
            gMcFlow->state = 6;
        } else {
            McCard_ResetStep();
            gMcFlow->state = 0x1F;
        }
        break;
    case 0x17:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x18;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x18;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            gMcFlow->flags |= 0x100;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x18:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x19;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1A);
            }
            break;
        }
        if (McCard_Format(0) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x1A);
            }
        } else {
            MCF_GO(0x19);
        }
        break;
    case 0xD:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x30;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0xF;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            gMcFlow->flags |= 0x100;
        } else {
            MCF_INPUT();
        }
        break;
    case 0xB:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x2C;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_CreateSave(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            gMcFlow->state = 0xF;
        }
        break;
    case 0xF:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x32;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_WriteSave(0, &gMcFlow->repSave.hdr, 0x1AC00, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            McFlow_CopySlotInfo(0, gMcFlow->slot);
            MCF_GO(0x10);
        }
        break;
    case 6:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (McCard_FindSave(0, &gMcFlow->req) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.result == 1) {
            MCF_GO(0xB);
        } else {
            MCF_GO(0xB);
        }
        break;
    case 0x1E:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x3C;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x10:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x33;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->flags |= 8;
                gMcFlow->state = 0x25;
                break;
            }
        }
        if (--gMcFlow->timer >= 0) {
            break;
        }
        gMcFlow->flags |= 8;
        gMcFlow->state = 0x25;
        break;
    case 0x11:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x34;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x19:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1A;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (--gMcFlow->timer >= 0) {
            break;
        }
        MCF_GO(0xB);
        break;
    case 0x1A:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1B;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
            }
        }
        break;
    case 0x1B:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1C;
        if (!(gMcFlow->flags & 0x80)) {
            McFlow_CallModeRemoved();
            gMcFlow->flags |= 0x80;
        }
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
                gMcFlow->flags &= ~0x80;
            }
        }
        break;
    case 0x12:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x13;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->mode = MCFLOW_MODE_REPLAY_SCAN;
            gMcFlow->flow = McFlow_UpdateReplayScan;
            gMcFlow->state = 0x27;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
            gMcFlow->index = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x1F:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (McCard_CalcNeed(0, &gMcFlow->req, 1) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.formatted == 0) {
                gMcFlow->state = 0x17;
                Dialog_SetCursor(1);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.need > 0) {
            if (MCF_CARD.free < MCF_CARD.need) {
                MCF_GO(0x1E);
                break;
            }
            gMcFlow->state = 0x21;
            gMcFlow->timer = 0x78;
            Dialog_SetCursor(1);
        } else {
            gMcFlow->state = 0xD;
            gMcFlow->timer = 0x78;
            Dialog_SetCursor(1);
        }
        break;
    case 0x21:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x30;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x22;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            gMcFlow->flags |= 0x100;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x22:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x32;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x11);
            }
            break;
        }
        if (McCard_CreateSave(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.result < 0) {
                MCF_GO(0x11);
            }
        } else {
            gMcFlow->state = 0xF;
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (gMcFlow->flags & 0x100) {
            McCard_Probe(0);
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->flags |= 0x80;
                gMcFlow->flags ^= 0x100;
                McFlow_CallModeRemoved();
                break;
            }
        }
        if (Dialog_IsClosed()) {
            if (gMcFlow->flags & 0x80) {
                gMcFlow->mode = MCFLOW_MODE_REPLAY_SCAN;
                gMcFlow->flow = McFlow_UpdateReplayScan;
                gMcFlow->state = 0x23;
                gMcFlow->next = 0x1B;
                gMcFlow->flags ^= 0x80;
            } else {
                gMcFlow->state = 0;
                McFlow_CallModeDone();
            }
            McCard_ResetStep();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
    Dialog_Draw(1);
}

/* Flow 6: load the replay of slot `slot`. */
void McFlow_UpdateReplayLoad(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        McCard_ResetStep();
        gMcFlow->state = 2;
        /* fallthrough */
    case 2:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x36;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 4;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_ResetStep();
        } else if (gMcFlow->choice < 0) {
            Dialog_SetCursor(1);
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
            gMcFlow->flags |= 0x100;
        } else {
            MCF_INPUT();
        }
        break;
    case 4:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (McCard_CheckFiles(0, &gMcFlow->req) == 0) {
            if (MCF_CARD.type != 2) {
                MCF_GO(0x1B);
                break;
            }
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
                break;
            }
            if (MCF_CARD.result >= 0) {
                break;
            }
            gMcFlow->state = 6;
        } else {
            MCF_GO(0x13);
        }
        break;
    case 6:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x28;
        if (McCard_FindSave(0, &gMcFlow->req) == 0) {
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x1B);
            }
            break;
        }
        if (MCF_CARD.result == 1) {
            MCF_GO(0x1D);
        } else {
            MCF_GO(0x1C);
        }
        break;
    case 0x13:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x37;
        if (--gMcFlow->timer >= 0) {
            MCF_POLL();
            if (McCard_IsFatalError(MCF_CARD.result)) {
                MCF_GO(0x15);
            }
            break;
        }
        if (McCard_ReadFile(0, &gMcFlow->repLoad, 0x1AC00, &gMcFlow->req) == 0) {
            if (MCF_CARD.result == -3) {
                MCF_GO(0x1D);
                break;
            }
            if (MCF_CARD.result < 0) {
                MCF_GO(0x15);
            }
        } else {
            if (MCF_CARD.unk0 & 1) {
                MCF_GO(0x1D);
                break;
            }
            if (McCard_Checksum(&gMcFlow->repLoad, 0x1AC00, 0) != 0) {
                MCF_GO(0x1D);
                break;
            }
            Battle_ClearWork();
            BattleReplay_Load(&gMcFlow->repLoad.data, 0x1ABA8);
            MCF_GO(0x14);
        }
        break;
    case 0x14:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x38;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->flags |= 8;
                gMcFlow->state = 0x25;
                break;
            }
        }
        if (--gMcFlow->timer >= 0) {
            break;
        }
        gMcFlow->flags |= 8;
        gMcFlow->state = 0x25;
        break;
    case 0x15:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x39;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x1D:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x3B;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x1B:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1C;
        if (!(gMcFlow->flags & 0x80)) {
            McFlow_CallModeRemoved();
            gMcFlow->flags |= 0x80;
        }
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
                gMcFlow->flags &= ~0x80;
            }
        }
        break;
    case 0x1C:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x3A;
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                Dialog_SetCursor(1);
                gMcFlow->state = 0x16;
            }
        }
        break;
    case 0x16:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x17;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->mode = MCFLOW_MODE_REPLAY_SCAN;
            gMcFlow->flow = McFlow_UpdateReplayScan;
            gMcFlow->state = 0x27;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
            gMcFlow->index = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (gMcFlow->flags & 0x100) {
            McCard_Probe(0);
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->flags |= 0x80;
                gMcFlow->flags ^= 0x100;
                McFlow_CallModeRemoved();
                break;
            }
        }
        if (Dialog_IsClosed()) {
            if (gMcFlow->flags & 0x80) {
                gMcFlow->mode = MCFLOW_MODE_REPLAY_SCAN;
                gMcFlow->flow = McFlow_UpdateReplayScan;
                gMcFlow->state = 0x23;
                gMcFlow->next = 0x1B;
                gMcFlow->flags ^= 0x80;
            } else {
                gMcFlow->state = 0;
                McFlow_CallModeDone();
            }
            McCard_ResetStep();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
    Dialog_Draw(1);
}

/* Flow 8: one question about the card (message 0x3D); the answer goes to the mode callbacks. */
void McFlow_UpdateReplayAsk(void) {
    if (gMcFlow->state == 0) {
        return;
    }
    switch (gMcFlow->state) {
    case 0x23:
        Dialog_SetLayout(1);
        Dialog_SetCursor(1);
        Dialog_SetTitle(0);
        Dialog_Start(0);
        gMcFlow->choice = 0;
        McCard_ResetStep();
        gMcFlow->state = 0x29;
        /* fallthrough */
    case 0x29:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x3D;
        McCard_Probe(0);
        if (McCard_IsFatalError(MCF_CARD.result)) {
            Dialog_SetChoices(0);
            gMcFlow->msg = 0x1C;
            gMcFlow->state = 0x1B;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            break;
        }
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            McCard_ResetStep();
            gMcFlow->flags |= 8;
        } else if (gMcFlow->choice < 0) {
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            gMcFlow->flags |= 0x100;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x1B:
        Dialog_SetChoices(0);
        gMcFlow->msg = 0x1C;
        if (!(gMcFlow->flags & 0x80)) {
            McFlow_CallModeRemoved();
            gMcFlow->flags |= 0x80;
        }
        if (MCF_OK_PRESSED()) {
            if (Dialog_IsOpen()) {
                Snd_PlaySe(1, 1);
                gMcFlow->state = 0x12;
                gMcFlow->choice = 0;
                Dialog_SetCursor(1);
                gMcFlow->flags &= ~0x80;
            }
        }
        break;
    case 0x12:
        Dialog_SetChoices(1);
        gMcFlow->msg = 0x13;
        if (gMcFlow->choice > 0) {
            if (--gMcFlow->timer >= 0) {
                break;
            }
            gMcFlow->state = 0x25;
            gMcFlow->choice = 0;
            gMcFlow->flags |= 8;
        } else if (gMcFlow->choice < -1) {
            gMcFlow->mode = MCFLOW_MODE_REPLAY_SCAN;
            gMcFlow->flow = McFlow_UpdateReplayScan;
            gMcFlow->state = 0x27;
            gMcFlow->timer = 0x78;
            gMcFlow->choice = 0;
            Dialog_SetCursor(1);
            McCard_ResetStep();
            gMcFlow->index = 0;
        } else {
            MCF_INPUT();
        }
        break;
    case 0x25:
        Dialog_SetChoices(0);
        Dialog_Start(1);
        gMcFlow->state = 0x26;
        /* fallthrough */
    case 0x26:
        Dialog_Input(0);
        if (gMcFlow->flags & 0x100) {
            McCard_Probe(0);
            if (McCard_IsFatalError(MCF_CARD.result)) {
                gMcFlow->flags |= 0x80;
                gMcFlow->flags ^= 0x100;
                McFlow_CallModeRemoved();
                break;
            }
        }
        if (Dialog_IsClosed()) {
            if (gMcFlow->flags & 0x80) {
                gMcFlow->mode = MCFLOW_MODE_REPLAY_SCAN;
                gMcFlow->flow = McFlow_UpdateReplayScan;
                gMcFlow->state = 0x23;
                gMcFlow->next = 0x1B;
                gMcFlow->flags ^= 0x80;
            } else {
                gMcFlow->state = 0;
                McFlow_CallModeDone();
            }
            McCard_ResetStep();
        }
        break;
    }
    Dialog_SetMsg(gMcFlow->msg);
    Dialog_Draw(1);
}

/* Allocates the work area, loads the card resources and builds the replay block to save. */
void McFlow_Init(void) {
    McFlow *work;
    McFlowHeader *hdr;
    McFlowReplay *rep;
    s32 i, j;

    gMcFlow = Heap_Alloc(0x39F90, 0x20, 0, 2);
    memset(gMcFlow, 0, 0x39F90);
    gMcFlow->pack = File_LoadSync(gProgress->fileBase + 0x1D, NULL, 0);
    gMcFlow->res = Sprite_Unpack(gMcFlow->pack, NULL, NULL);
    work = gMcFlow;
    rep = BattleReplay_GetBuffer(NULL);
    hdr = &work->repSave.hdr;
    work->repSave.data = *rep;
    for (i = 0; i < 2; i++) {
        for (j = 0; j < 5; j++) {
            if (j < rep->team[i].count) {
                hdr->chara[i][j] = rep->team[i].member[j].chara;
            } else {
                hdr->chara[i][j] = MCFLOW_NO_CHARA;
            }
        }
    }
    McCard_Checksum(&hdr->hdrSum, 0x30, 1);
    McCard_Checksum(hdr, 0x1AC00, 1);
}

/* Frees the resources and the work area. */
void McFlow_Term(void) {
    if (gMcFlow->res != NULL) {
        Heap_Free(gMcFlow->res);
        gMcFlow->res = NULL;
    }
    if (gMcFlow->pack != NULL) {
        Heap_Free(gMcFlow->pack);
        gMcFlow->pack = NULL;
    }
    if (gMcFlow != NULL) {
        Heap_Free(gMcFlow);
        gMcFlow = NULL;
    }
}

/* Starts flow `mode`. */
void McFlow_Start(s32 mode) {
    gMcFlow->mode = mode;
#ifdef PORT
    /* 1: without the window, 2: the window with its questions, without the waits */
    sMcQuick = (mode == MCFLOW_MODE_SAVE || mode == MCFLOW_MODE_SAVE_ALT || mode == MCFLOW_MODE_LOAD ||
                mode == MCFLOW_MODE_BOOT_LOAD || mode == MCFLOW_MODE_NEW_SAVE) && !Port_NetActive() &&
               Port_Setting("save_quick", 1) != 0;
    if (sMcQuick && (mode == MCFLOW_MODE_LOAD || (mode == MCFLOW_MODE_NEW_SAVE && (gProgress->flags & 8)))) {
        sMcQuick = 2;
    }
    if (mode == MCFLOW_MODE_REPLAY_SCAN && !Port_NetActive() && Port_Setting("save_quick", 1) != 0) {
        sMcQuick = 3; /* the Data Center's replays: their list read without the window */
    }
    sMcQuickState = -1;
    if (sMcQuick == 1 && mode != MCFLOW_MODE_BOOT_LOAD) {
        Port_UiSaving(1);
    }
#endif
    switch (mode) {
    case MCFLOW_MODE_SAVE:
        gMcFlow->flow = McFlow_UpdateSave;
        gMcFlow->state = 0x23;
        gMcFlow->next = 0xC;
        McFlow_SetupRequest(0, 0);
        McCard_Checksum(gSaveData, 0x4000, 1);
        break;
    case MCFLOW_MODE_LOAD:
        gMcFlow->flow = McFlow_UpdateLoad;
        gMcFlow->state = 0x23;
        gMcFlow->next = 2;
        McFlow_SetupRequest(0, 0);
        break;
    case MCFLOW_MODE_BOOT_LOAD:
        gMcFlow->flow = McFlow_UpdateBootLoad;
        gMcFlow->state = 0x23;
        McFlow_SetupRequest(0, 0);
        break;
    case MCFLOW_MODE_NEW_SAVE:
        gMcFlow->flow = McFlow_UpdateNewSave;
        gMcFlow->state = 0x23;
        McFlow_SetupRequest(0, 0);
        break;
    case MCFLOW_MODE_SAVE_ALT:
        gMcFlow->flow = McFlow_UpdateSave;
        gMcFlow->state = 0x23;
        gMcFlow->next = 0x20;
        McFlow_SetupRequest(0, 0);
        McCard_Checksum(gSaveData, 0x4000, 1);
        break;
    case MCFLOW_MODE_REPLAY_SAVE:
        gMcFlow->flow = McFlow_UpdateReplaySave;
        gMcFlow->state = 0x23;
        McFlow_SetupRequest(1, gMcFlow->slot);
        break;
    case MCFLOW_MODE_REPLAY_LOAD:
        gMcFlow->flow = McFlow_UpdateReplayLoad;
        gMcFlow->state = 0x23;
        McFlow_SetupRequest(1, gMcFlow->slot);
        break;
    case MCFLOW_MODE_REPLAY_SCAN:
        gMcFlow->flow = McFlow_UpdateReplayScan;
        gMcFlow->state = 0x23;
        gMcFlow->next = 0x27;
        gMcFlow->flags &= ~0x70;
        Progress_ClearTeams();
        break;
    case MCFLOW_MODE_REPLAY_ASK:
        gMcFlow->flow = McFlow_UpdateReplayAsk;
        gMcFlow->state = 0x23;
        break;
    case MCFLOW_MODE_REPLAY_ERR:
        gMcFlow->flow = McFlow_UpdateReplayScan;
        gMcFlow->state = 0x23;
        gMcFlow->next = 0x1B;
        break;
    }
}

/* Registers the "finished" callback and argument of result `idx`. */
void McFlow_SetDoneCb(s32 idx, McFlowCb cb, s32 arg) {
    gMcFlow->doneCb[idx] = cb;
    gMcFlow->doneArg[idx] = arg;
}

/* Registers a callback and argument per mode and result. */
void McFlow_SetModeCb(s32 mode, s32 idx, McFlowCb cb, s32 arg) {
    gMcFlow->modeCb[mode][idx] = cb;
    gMcFlow->modeArg[mode][idx] = arg;
}

/* Selects the replay slot the next replay flow works on. */
void McFlow_SetSlot(s32 slot) {
    gMcFlow->slot = slot;
}

/* Polls the card in port 0: 0 busy, 1 card ready, 2 no card or error. */
s32 McFlow_PollCard(void) {
    if (McCard_GetInfo(0) == 0) {
        return 0;
    }
    if (MCF_CARD.type != 2 || McCard_IsFatalError(MCF_CARD.result) != 0) {
        McCard_ResetStep();
        return 2;
    }
    McCard_ResetStep();
    return 1;
}

/* Runs the current flow; returns its state (0 = idle / finished). */
s32 McFlow_Update(void) {
    if (gMcFlow == NULL) {
        return 0;
    }
    if (gMcFlow->state == 0) {
        return 0;
    }
#ifdef PORT
    if (sMcQuick && McFlow_QuickStep()) {
        return gMcFlow->state;
    }
#endif
    if (gMcFlow->flow != NULL) {
        gMcFlow->flow();
    }
    return gMcFlow->state;
}
