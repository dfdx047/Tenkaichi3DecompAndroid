#include "common.h"
#include "sys/dma.h"
#include "sys/heap.h"
#include "sys/pad.h"
#include "sys/pad_watch.h"

extern void *memset(void *dst, s32 value, u32 size);

/* Local views (see the report): only the fields this file reads. */
typedef struct PadWatchBattleWork {
    /* 0x0000 */ u8 unk0[0x19F8];
    /* 0x19F8 */ s32 running; /* BattleWork.running: 1 while a battle is running */
} PadWatchBattleWork;

typedef struct PadWatchProgress {
    /* 0x000 */ u8 unk0[0x18];
    /* 0x018 */ s32 mode;
    /* 0x01C */ u8 unk1C[0x604];
    /* 0x620 */ s32 unk620; /* 1: the screen of mode 0x27 / 0x28 needs both controllers */
} PadWatchProgress;

typedef struct PadWatchCommonRes {
    /* 0x00 */ u32 *boot; /* CommonRes.boot (file 5); words at +0x24: offsets of the three messages */
} PadWatchCommonRes;

extern PadWatchProgress *gProgress;
extern PadWatchCommonRes *gCommonRes;

extern PadWatchBattleWork *Battle_GetWork(void);
extern s32 Battle_IsSplitScreen(void);
extern s32 Battle_GetMode(void);
extern s32 BtlGame_IsReplay(void);  /* returns the word at gBtlGameReplayActive */
extern void Gfx_AddDefaultEnv(void);
extern s32 Font_GetCmdCount(void);          /* current font */
extern void Font_Flush(s32 font);     /* select a font */
extern s32 Font_GetHeight(u16 *str);      /* height of a text in pixels */
extern void Font_PrintAt(s32 x, s32 y, u16 *str);
extern void Font_PushStyle(void);
extern void Font_PopStyle(void);
extern void Font_SetClip(s32 x0, s32 y0, s32 x1, s32 y1);
extern void Font_SetAlign(s32 align);
extern void Font_SetShadowMode(s32 a);
extern void Font_SetColorRGBA(s32 r, s32 g, s32 b, s32 a);
extern void Font_SetShadowColorRGBA(s32 r, s32 g, s32 b, s32 a);

/* 1 when a usable controller is plugged into the port. */
s32 PadWatch_IsConnected(s32 port) {
    return gPad[port].status != PAD_STATUS_NONE;
}

/* Turns the warning on (enable != 0) or off. */
void PadWatch_SetEnabled(s32 enable) {
    gPadWatch->disabled = enable == 0;
}

/*
 * 1 when a needed controller is missing; *port (if not NULL) receives which one: 0 when controller 1
 * is missing (also when both are), 1 when only controller 2 is. Returns 0 while no port has a settled
 * reading or there is no warning. Polled by the battle's pause check (BtlPause_CheckOpen).
 *
 * NON-MATCHING: 2 of 69 instructions. In the "controller 2 missing" arm the original has
 * `beqz s0,<exit>; nop` and this compiles to `beqzl s0,<exit + 4>; ld s0,0(sp)`: the delay-slot pass
 * here pulls the first instruction of the epilogue into the slot and the original left it empty.
 * Same instructions, registers and branch targets otherwise. Tried without effect: early returns in
 * every combination of the four arms, goto forms, `port == NULL` tests, a variable for the 1.
 */
/* Cleanup pass 2: read from the delay-slot pass (reorg.c). The `li v0,1` in the slot of the preceding
   `bnez v0,<exit>` makes the copy of `li v0,1` at the target redundant for `beqz s0`, so this compiler skips it
   and takes the next instruction of the target (`ld s0`, annulled: beqzl). The original has `beqz` + nop, which
   is what is left when `beqz s0` took `li v0,1` first and lost it again as redundant in the relax step: the
   preceding branch got its `li v0,1` only later (second round), i.e. it did not point at the `return 1` block
   directly when the slots were filled first. Eight more arrangements of the `*port = 1` arm (returns, inverted
   tests, a local for the 1) give the same two instructions or more differences. */
#if 0
s32 PadWatch_GetMissing(s32 *port) {
    if ((gPadWatch->valid[0] == 0 && gPadWatch->valid[1] == 0) || gPadWatch->message < 0) {
        return 0;
    }
    if (gPadWatch->valid[0] != 0 || gPadWatch->valid[1] != 0) {
        if (Battle_GetWork()->running != 0
                ? (BtlGame_IsReplay() == 0 && Battle_IsSplitScreen() != 0)
                : ((u32)(gProgress->mode - 0x27) < 2 && gProgress->unk620 == 1)) {
            if (!(gPadWatch->state[0] & 1)) {
                if (!(gPadWatch->state[1] & 1)) {
                    if (port != NULL) {
                        *port = 0;
                    }
                } else {
                    if (port != NULL) {
                        *port = 0;
                    }
                }
            } else if (!(gPadWatch->state[1] & 1)) {
                if (port != NULL) {
                    *port = 1;
                }
            }
        } else {
            if (!(gPadWatch->state[0] & 1)) {
                if (port != NULL) {
                    *port = 0;
                }
            }
        }
    }
    return 1;
}
#endif
INCLUDE_ASM("asm/nonmatchings/sys/pad_watch", PadWatch_GetMissing);

/* 1 when the given port's controller is missing and a warning is up. */
s32 PadWatch_IsPortMissing(s32 port) {
    if (gPadWatch->valid[port] == 0 || gPadWatch->message < 0) {
        return 0;
    }
    if (Battle_GetWork()->running != 0
            ? (BtlGame_IsReplay() == 0 && Battle_IsSplitScreen() != 0)
            : ((u32)(gProgress->mode - 0x27) < 2 && gProgress->unk620 == 1)) {
        if (!(gPadWatch->state[port] & 1)) {
            return 1;
        }
    } else {
        if (!(gPadWatch->state[port] & 1)) {
            return 1;
        }
    }
    return 0;
}

/* Per frame: samples and debounces both ports, then picks the warning to show. */
void PadWatch_Update(void) {
    s32 now[PAD_WATCH_PORTS];
    s32 i;

    gPadWatch->message = PAD_WATCH_MSG_NONE;
    for (i = 0; i < PAD_WATCH_PORTS; i++) {
        now[i] = PadWatch_IsConnected(i);
        if (gPadWatch->raw[i] == now[i]) {
            if (gPadWatch->count[i] >= PAD_WATCH_STABLE_FRAMES) {
                gPadWatch->valid[i] = 1;
                gPadWatch->state[i] = gPadWatch->raw[i];
            } else {
                gPadWatch->count[i]++;
            }
        } else {
            gPadWatch->valid[i] = 1;
            gPadWatch->count[i] = 0;
            gPadWatch->raw[i] = now[i];
        }
        now[i] = gPadWatch->state[i];
    }
    if (gPadWatch->disabled == 0 && (gPadWatch->valid[0] != 0 || gPadWatch->valid[1] != 0)) {
        if (Battle_GetWork()->running != 0
                ? (BtlGame_IsReplay() == 0 && Battle_IsSplitScreen() != 0)
                : ((u32)(gProgress->mode - 0x27) < 2 && gProgress->unk620 == 1)) {
            if (!(now[0] & 1)) {
                if (!(now[1] & 1)) {
                    gPadWatch->message = PAD_WATCH_MSG_BOTH;
                } else {
                    gPadWatch->message = PAD_WATCH_MSG_PAD1;
                }
            } else if (!(now[1] & 1)) {
                gPadWatch->message = PAD_WATCH_MSG_PAD2;
            }
        } else {
            if (!(now[0] & 1)) {
                gPadWatch->message = PAD_WATCH_MSG_PAD1;
            }
        }
    }
}

/* Darkens the screen and prints the current warning in its centre. */
void PadWatch_DrawMessage(void) {
    s32 message = gPadWatch->message;

    if (gPadWatch->disabled == 0 && (Battle_GetWork()->running == 0 || Battle_GetMode() != 7) &&
        (gPadWatch->valid[0] != 0 || gPadWatch->valid[1] != 0) && message >= 0) {
        u32 *boot = gCommonRes->boot;
        s32 font = Font_GetCmdCount();
        u16 *text = (u16 *)((u8 *)boot + ((boot[9 + message] >> 2) << 2));
        s32 height = Font_GetHeight(text);
        s32 *fontPtr = &font; /* taking a local's address is what stops the last call from becoming a tail call (needed to match) */
        u64 *p;

        Gfx_AddDefaultEnv();
        p = Dma_BeginDirect();
        p[0] = GIF_TAG(1, 1, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = 0x44; /* (Cs - Cd) * As + Cd */
        p[1] = GS_ALPHA_1;
        p += 2;
        p[0] = GIF_TAG_EX(1, 1, GIF_FLG_REGLIST, 4);
        p[1] = 0x5510; /* PRIM, RGBAQ, XYZ2, XYZ2 */
        p += 2;
        p[0] = 0x46; /* sprite, alpha blended */
        p[1] = 0x3F80000060000000ULL; /* black, alpha 0x60 */
        p += 2;
        p[0] = GS_SET_XYZ(0x6F00, 0x7100, 0xFFFFFF);
        p[1] = GS_SET_XYZ(0x9100, 0x8F00, 0xFFFFFF);
        Dma_EndDirect(p + 2);
        Font_PushStyle();
        Font_SetAlign(1);
        Font_SetShadowMode(1);
        Font_SetClip(0, 0, 0x200, 0x1C0);
        Font_SetColorRGBA(0xFF, 0xFF, 0xFF, 0x80);
        Font_SetShadowColorRGBA(0, 0, 0, 0x80);
        Font_PrintAt(0x100, 0xE0 - height / 2, text);
        Font_PopStyle();
        Font_Flush(*fontPtr);
    }
}

/* Allocates the watcher; both ports start as connected. */
void PadWatch_Init(void) {
    s32 i;

    gPadWatch = Heap_Alloc(sizeof(PadWatch), 0x20, 0, 2);
    memset(gPadWatch, 0, sizeof(PadWatch));
    for (i = 0; i < PAD_WATCH_PORTS; i++) {
        gPadWatch->raw[i] |= 1;
        gPadWatch->state[i] |= 1;
    }
}

/* Per frame, from Gfx_EndFrame: draws the warning if there is one. */
void PadWatch_Draw(void) {
    PadWatch_DrawMessage();
}

/* Frees the watcher. No caller. */
void PadWatch_Term(void) {
    Heap_Free(gPadWatch);
    gPadWatch = NULL;
}
