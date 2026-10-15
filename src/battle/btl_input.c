#include "common.h"
#include "battle/btl_input.h"
#include "battle/btl_char_ctl.h"
#include "battle/battle.h"
#include "sys/pad.h"
#include "sys/save.h"

/*
 * Per-fighter battle input: 0x1D3B40..0x1D60A0.
 *
 * Once per unpaused frame, for every fighter whose chr+0x1320 is not positive (BtlChars_SampleInput -> BtlChar_SampleInput):
 *   BtlInput_Sample      copies gPad[chr->pad] (raw button word, left stick as two bytes, pad status) into
 *                        chr->input and calls
 *   BtlInput_BuildRecord which builds the frame's BtlInputRecord (chr->input.rec):
 *       - chr->injectOn == 0: RemapButtons (key config) -> MergeStick -> AddDoubleTaps -> BuildCommands;
 *       - chr->injectOn != 0 (CPU side): the buttons and stick the AI wrote -> MergeStick -> BuildCommands;
 *       - the fighter takes no input (flag 2 or 3 clear, flag 0x136 set, or BtlChars_IsTimeStopped() != 0): all neutral;
 *       - otherwise the replay hook: when a replay is playing, buttons and stick are REPLACED by the recorded
 *         frame and the commands rebuilt; when not, {buttons, stick} are appended to the replay buffer.
 * Later in the same frame, in the fighter update (BtlChars_UpdateInput -> BtlChar_BeginFrame):
 *   BtlInput_Update      BtlInput_Fetch pushes the record into the 8-entry ring and pops the oldest entry (the
 *                        ring is empty before the push, so this is the same frame: no delay), blanks or filters
 *                        it according to battle flags, then held / prev / pressed / released, the smoothed
 *                        stick and the per-bit frame counters are derived. The readers at the end of the file
 *                        are what the fighter logic uses.
 * Two sets of counters exist: recCount / recCmdCount follow the record (ticked in BuildRecord, used by the
 * double-tap and command logic), count / cmdCount follow the fetched input (ticked in Update, used by the readers).
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
extern f32 atan2f(f32 y, f32 x);
extern f32 fabsf(f32 x);

/* Declared here and not taken from other modules' headers. */
extern s32 BtlChar_TestFlag(BtlInputChr *chr, s32 bit);
extern void BtlChar_ClearFlag(BtlInputChr *chr, s32 bit);
extern f32 BtlAct_GetFacingRelCam(BtlInputChr *chr); /* wrap(pose heading - fighter camera yaw) */
extern s32 BattleReplay_IsActive(void); /* 0x12A9E8: a replay is being played back */
extern BtlInputSwitchEntry *BtlSuper_GetPromptRow(BtlInputChr *chr, s32 arg);
extern s32 BtlChars_IsTimeStopped(void);          /* gBtlChars->unk274 */
extern void BtlReplay_Play(BtlInputChr *chr, u32 *buttons, u8 *stick);              /* replay: read this frame */
extern void BtlReplay_Record(BtlInputChr *chr, u32 buttons, u32 commands, u8 *stick); /* replay: record this frame */
extern s32 BtlChar_IsStage4Or27(void);          /* Battle_GetStage() is 4 or 27 */
extern Pad *BtlChar_GetPad(BtlInputChr *chr); /* &gPad[chr->pad] */
extern f32 BtlUtil_WrapAngle(f32 angle);     /* wraps an angle into -pi..pi */

/* Turns a stick value (-1..1) into a byte (0..0xFE, 0x7F = centre). */
u8 BtlInput_StickToByte(f32 v) {
    s32 n = (v * 0.5f + 0.5f) * 254.0f;

    if (n < 0) {
        n = 0;
    }
    if (n > 254) {
        n = 254;
    }
    return n;
}

/* Turns a stick byte back into -1..1. */
f32 BtlInput_ByteToStick(u8 b) {
    return b * (1.0f / 254.0f) * 2.0f - 1.0f;
}

/* Returns the fighter's input block. */
BtlCharInput *BtlInput_Get(BtlInputChr *chr) {
    return &chr->input;
}

/* Builds the battle button word from the raw pad word: bit i is set when any bit of mask[i] is down (all of them for bit 15). */
u32 BtlInput_RemapButtons(u32 rawHeld, u32 *mask) {
    u32 out = 0;
    s32 i;

    for (i = 0; i < BTL_BUTTON_BITS; i++) {
        u32 bit = 1 << i;

        if (bit & BTLB_L3R3) {
            if ((rawHeld & *mask) == *mask) {
                out |= bit;
            }
        } else {
            if (rawHeld & *mask) {
                out |= bit;
            }
        }
        mask++;
    }
    return out;
}

/* Resets a frame counter when cond is 0, otherwise counts it up to BTL_COUNT_MAX. */
void BtlInput_TickCounter(s32 cond, s8 *count) {
    if (cond) {
        if (*count < BTL_COUNT_MAX) {
            (*count)++;
        }
    } else {
        *count = 0;
    }
}

/* Advances the five counter rows of a 32-bit word by one frame. */
void BtlInput_TickCounters(BtlInputCounters *c, u32 held, u32 pressed, u32 released) {
    s32 i;

    c->prevSincePress = c->sincePress;
    for (i = 0; i < 32; i++) {
        u32 bit = 1 << i;

        BtlInput_TickCounter(held & bit, &c->held.n[i]);
        BtlInput_TickCounter((held & bit) == 0, &c->notHeld.n[i]);
        BtlInput_TickCounter((pressed & bit) == 0, &c->sincePress.n[i]);
        BtlInput_TickCounter((released & bit) == 0, &c->sinceRelease.n[i]);
    }
}

/* Appends one frame of input to the ring; dropped when the ring is full. */
void BtlInput_RingPush(BtlInputChr *chr, u32 buttons, u32 commands, u8 *stick) {
    BtlCharInput *in = &chr->input;

    if ((in->ringWrite + 1) % BTL_INPUT_RING != in->ringRead) {
        in->ringButtons[in->ringWrite] = buttons;
        in->ringCommands[in->ringWrite] = commands;
        in->ringStick[in->ringWrite][0] = stick[0];
        in->ringStick[in->ringWrite][1] = stick[1];
        in->ringWrite = (in->ringWrite + 1) % BTL_INPUT_RING;
    }
}

/* Takes the oldest frame of input out of the ring; neutral input when it is empty. */
void BtlInput_RingPop(BtlInputChr *chr, u32 *buttons, u32 *commands, u8 *stick) {
    BtlCharInput *in = &chr->input;
    u32 b;
    u32 c;

    if (in->ringWrite == in->ringRead) {
        if (buttons != NULL) {
            *buttons = 0;
        }
        if (commands != NULL) {
            *commands = 0;
        }
        stick[1] = stick[0] = BTL_STICK_CENTER;
        return;
    }
    b = in->ringButtons[in->ringRead];
    c = in->ringCommands[in->ringRead];
    stick[0] = in->ringStick[in->ringRead][0];
    stick[1] = in->ringStick[in->ringRead][1];
    if (buttons != NULL) {
        *buttons = b;
    }
    if (commands != NULL) {
        *commands = c;
    }
    in->ringRead = (in->ringRead + 1) % BTL_INPUT_RING;
}

/* Adds the double-tap bits of ASCEND and DESCEND (second press within BTL_TAP_FRAMES of the first). */
u32 BtlInput_AddDoubleTaps(BtlInputChr *chr, u32 buttons, u32 prev, s32 padStatus) {
    BtlCharInput *in = &chr->input;
    u32 pressed = buttons & ~prev;

    switch (padStatus) {
        case 0:
        case 1:
            if ((pressed & BTLB_ASCEND) && in->recCount.prevSincePress.n[11] < BTL_TAP_FRAMES) {
                buttons |= BTLB_ASCEND_TAP2;
            }
            if ((pressed & BTLB_DESCEND) && in->recCount.prevSincePress.n[12] < BTL_TAP_FRAMES) {
                buttons |= BTLB_DESCEND_TAP2;
            }
            break;
    }
    return buttons;
}

/* Tells whether the buttons ask for BTLC_SWITCH: RUSH pressed while holding the direction of the entry BtlSuper_GetPromptRow returns. */
s32 BtlInput_TestSwitch(BtlInputChr *chr, s32 arg, u32 held, u32 pressed) {
    if (BtlChar_TestFlag(chr, 0xA2)) {
        if (!(held & BTLB_BLAST)) {
            return 1;
        }
    } else if (BtlChar_TestFlag(chr, 0xA3)) {
        if (pressed & BTLB_BLAST) {
            return 1;
        }
    } else {
        switch (BtlSuper_GetPromptRow(chr, arg)->dir) {
            case 1:
                if ((held & BTLB_DOWN) && (pressed & BTLB_RUSH)) {
                    return 1;
                }
                break;
            case 0:
                if ((held & BTLB_UP) && (pressed & BTLB_RUSH)) {
                    return 1;
                }
                break;
            case 2:
                if ((held & BTLB_LEFT) && (pressed & BTLB_RUSH)) {
                    return 1;
                }
                break;
            case 3:
                if ((held & BTLB_RIGHT) && (pressed & BTLB_RUSH)) {
                    return 1;
                }
                break;
        }
    }
    return 0;
}

/* Makes d-pad and stick agree: a d-pad bit forces the stick byte to its end, otherwise the stick's dominant axis sets a direction bit. */
u32 BtlInput_MergeStick(BtlInputChr *chr, u32 buttons, u8 *stick) {
    if (buttons & BTLB_DIR_MASK) {
        if (buttons & BTLB_LEFT) {
            stick[0] = 0;
        }
        if (buttons & BTLB_RIGHT) {
            stick[0] = 0xFF;
        }
        if (buttons & BTLB_UP) {
            stick[1] = 0;
        }
        if (buttons & BTLB_DOWN) {
            stick[1] = 0xFF;
        }
    } else {
        u8 x = stick[0];
        u8 y = stick[1];
        s32 dx = __builtin_abs(x - BTL_STICK_CENTER);
        s32 dy = __builtin_abs(y - BTL_STICK_CENTER);

        if (dx >= 2 || dy >= 2) {
            if (dy < dx) {
                buttons = x > BTL_STICK_CENTER ? buttons | BTLB_RIGHT : buttons | BTLB_LEFT;
            } else {
                buttons = y > BTL_STICK_CENTER ? buttons | BTLB_DOWN : buttons | BTLB_UP;
            }
        }
    }
    return buttons;
}

/* Builds the command word from this frame's and the previous frame's button words. */
u32 BtlInput_BuildCommands(BtlInputChr *chr, u32 held, u32 prev) {
    BtlCharInput *in = &chr->input;
    u32 cmd = 0;
    u32 released = ~held & prev;
    u32 pressed = held & ~prev;

    if (held & BTLB_DASH) {
        u32 charge = held & BTLB_CHARGE;

        cmd = BTLC_DASH_CHARGE_H;
        if (!charge) {
            cmd = BTLC_DASH_HELD;
        }
    }
    if (pressed & BTLB_DASH) {
        cmd |= BTLC_DASH_P3 | BTLC_DASH_P;
        cmd |= BTLC_DASH_P4;
        if (held & BTLB_CHARGE) {
            cmd |= BTLC_DASH_CHARGE_P;
        }
        if (held & BTLB_LEFT) {
            cmd |= BTLC_DIR_LEFT | BTLC_DASH_P2;
        }
        if (held & BTLB_RIGHT) {
            cmd |= BTLC_DIR_RIGHT | BTLC_DASH_P2;
        }
        if (held & BTLB_DOWN) {
            cmd |= BTLC_DIR_DOWN | BTLC_DASH_P2;
        }
        if (held & BTLB_UP) {
            cmd |= BTLC_DIR_UP | BTLC_DASH_P2;
        }
        if (!(held & BTLB_DIR_MASK)) {
            cmd |= BTLC_DIR_UP | BTLC_DASH_P2;
            if (in->recCount.prevSincePress.n[1] < BTL_TAP_FRAMES) {
                cmd |= BTLC_DASH_TAP2;
            }
        }
    }
    if (held & BTLB_GUARD) {
        cmd |= BTLC_GUARD_H2 | BTLC_GUARD_H;
        if (pressed & BTLB_LEFT) {
            cmd |= BTLC_GUARD_SIDE;
        }
        if (pressed & BTLB_RIGHT) {
            cmd |= BTLC_GUARD_SIDE;
        }
    }
    if (held & BTLB_CHARGE) {
        if (pressed & BTLB_BLAST) {
            if (held & BTLB_DOWN) {
                cmd |= BTLC_CHARGE_BLAST_D;
            } else if (held & BTLB_UP) {
                cmd |= BTLC_CHARGE_BLAST_U;
            } else {
                cmd |= BTLC_CHARGE_BLAST;
            }
        }
        if (pressed & BTLB_GUARD) {
            if (held & BTLB_UP) {
                cmd |= BTLC_CHARGE_GUARD_U;
            } else {
                cmd |= BTLC_CHARGE_GUARD;
            }
        }
    }
    if (released & BTLB_RUSH) {
        if (in->recCount.sincePress.n[3] < BTL_TAP_FRAMES) {
            cmd |= BTLC_RUSH_TAP;
        }
    }
    if (held & BTLB_RUSH) {
        if (pressed & BTLB_RUSH) {
            cmd |= BTLC_RUSH_P;
        }
        if (in->recCount.sincePress.n[3] == 6) {
            cmd |= BTLC_RUSH_HOLD6;
        }
        cmd |= BTLC_RUSH_H;
        if (held & BTLB_UP) {
            cmd |= BTLC_DIR_UP;
        } else if (held & BTLB_DOWN) {
            cmd |= BTLC_DIR_DOWN;
        } else if (held & BTLB_LEFT) {
            cmd |= BTLC_DIR_LEFT;
        } else if (held & BTLB_RIGHT) {
            cmd |= BTLC_DIR_RIGHT;
        }
    } else {
        cmd |= BTLC_RUSH_NOT_H;
    }
    if ((pressed & BTLB_BLAST) && !(held & BTLB_CHARGE)) {
        if (held & BTLB_UP) {
            cmd |= BTLC_DIR_UP | BTLC_BLAST_P;
        } else if (held & BTLB_LEFT) {
            cmd |= BTLC_DIR_LEFT | BTLC_BLAST_P;
        } else if (held & BTLB_RIGHT) {
            cmd |= BTLC_DIR_RIGHT | BTLC_BLAST_P;
        } else {
            cmd |= BTLC_DIR_DOWN | BTLC_BLAST_P;
        }
    }
    if (pressed & BTLB_LOCKON) {
        cmd |= BTLC_LOCKON_P;
    }
    if (pressed & BTLB_DIR_MASK) {
        cmd |= BTLC_DIR_P;
    }
    if (chr->unk1594 >= 2) {
        if (BtlInput_TestSwitch(chr, chr->unk1594, held, pressed)) {
            cmd |= BTLC_SWITCH;
        }
    }
    return cmd;
}

/* Builds this frame's input record from the injected input or the pad sample, then lets the replay code overwrite or record it. */
void BtlInput_BuildRecord(BtlInputChr *chr) {
    u8 stick[2];
    f32 fstick[2];
    u32 buttons;
    BtlCharInput *in = &chr->input;
    u32 commands;
    u32 prevButtons;
    u32 prevCommands;
    s32 active;

    buttons = 0;
    prevButtons = chr->input.rec.buttons;
    prevCommands = chr->input.rec.commands;
    stick[1] = stick[0] = BTL_STICK_CENTER;
    if (chr->injectOn) {
        f32 x = chr->injectStickX;
        f32 y = chr->injectStickY;

        buttons = chr->injectButtons;
        fstick[0] = x;
        fstick[1] = y;
        if (buttons & BTLB_GUARD) {
            buttons |= BTLB_ANY_FACE;
        }
        if (buttons & BTLB_BLAST) {
            buttons |= BTLB_BLAST2;
        }
        if (buttons & BTLB_RS_UP) {
            buttons |= BTLB_RS_UP2;
        }
        if (buttons & BTLB_RS_DOWN) {
            buttons |= BTLB_RS_DOWN2;
        }
        stick[0] = BtlInput_StickToByte(x);
        stick[1] = BtlInput_StickToByte(y);
        buttons = BtlInput_MergeStick(chr, buttons, stick);
        commands = BtlInput_BuildCommands(chr, buttons, prevButtons);
    } else {
        BtlCharInput *raw = BtlInput_Get(chr);

        if (raw != NULL) {
            buttons = BtlInput_RemapButtons(raw->rawHeld, raw->mask);
            stick[0] = raw->rawStickX;
            stick[1] = raw->rawStickY;
        }
        buttons = BtlInput_MergeStick(chr, buttons, stick);
        buttons = BtlInput_AddDoubleTaps(chr, buttons, prevButtons, in->padStatus);
        commands = BtlInput_BuildCommands(chr, buttons, prevButtons);
    }
    active = 0;
    if (BtlChar_TestFlag(chr, 2) || BtlChar_TestFlag(chr, 3)) {
        if (!BtlChar_TestFlag(chr, 0x136)) {
            active = BtlChars_IsTimeStopped() == 0;
        }
    }
    if (active) {
        if (BattleReplay_IsActive()) {
            BtlReplay_Play(chr, &buttons, stick);
            commands = BtlInput_BuildCommands(chr, buttons, prevButtons);
        } else {
            BtlReplay_Record(chr, buttons, commands, stick);
        }
    } else {
        buttons = 0;
        stick[1] = stick[0] = BTL_STICK_CENTER;
        commands = 0;
    }
    chr->input.rec.tag[0] = 'O';
    chr->input.rec.tag[1] = 'P';
    chr->input.rec.tag[2] = 'R';
    chr->input.rec.tag[3] = 'T';
    chr->input.rec.player = chr->player;
    chr->input.rec.buttons = buttons;
    chr->input.rec.stickX = stick[0];
    chr->input.rec.stickY = stick[1];
    chr->input.rec.commands = commands;
    chr->input.rec.unk5 = 0;
    if (!BtlChars_IsTimeStopped()) {
        BtlInput_TickCounters(&chr->input.recCount, buttons, buttons & ~prevButtons, prevButtons & ~buttons);
        BtlInput_TickCounters(&chr->input.recCmdCount, commands, commands & ~prevCommands, prevCommands & ~commands);
    }
}

/* Queues this frame's record, takes the oldest queued frame and blanks or filters it according to the battle state. */
void BtlInput_Fetch(BtlInputChr *chr, u32 *outButtons, u32 *outCommands, u8 *stick) {
    u32 buttons;
    u32 commands;
    s32 block = 0;
    s32 filter = 0;

    stick[1] = stick[0] = BTL_STICK_CENTER;
    buttons = 0;
    commands = 0;
    BtlInput_RingPush(chr, chr->input.rec.buttons, chr->input.rec.commands, &chr->input.rec.stickX);
    BtlInput_RingPop(chr, &buttons, &commands, stick);
    if (Battle_GetWork()->flags & BTL_INPUT_FLAG_FILTER) {
        if (BtlChar_IsStage4Or27()) {
            block = 1;
        } else {
            filter = 1;
        }
    }
    if ((Battle_GetWork()->flags & BTL_INPUT_FLAG_BLOCK) || BtlChar_TestFlag(chr, 0x11E) || BtlChar_TestFlag(chr, 0x11F) ||
        BtlChar_TestFlag(chr, 0xB1)) {
        block = 1;
    }
    if (filter) {
        buttons &= BTLB_FILTER_MASK;
        commands &= BTLC_FILTER_MASK;
    }
    if (block) {
        buttons = 0;
        commands = 0;
        stick[1] = stick[0] = BTL_STICK_CENTER;
    }
    if (outButtons != NULL) {
        *outButtons = buttons;
    }
    if (outCommands != NULL) {
        *outCommands = commands;
    }
}

/* Returns the raw pad bit of the physical button the player's key config assigns to an action (0 when none). */
u32 BtlInput_GetKeyMask(BtlInputChr *chr, s32 action) {
    s32 padBit[SAVE_KEY_COUNT] = { PAD_CIRCLE, PAD_CROSS, PAD_SQUARE, PAD_TRIANGLE, PAD_L1, PAD_L2, PAD_R1, PAD_R2 };
    s32 i;

    for (i = 0; i < SAVE_KEY_COUNT; i++) {
        if (gSaveData->key[chr->player][i] == action) {
            return padBit[i];
        }
    }
    return 0;
}

/* Fills the fighter's table of raw pad masks, one per battle button bit, from the key config. */
void BtlInput_BuildMaskTable(BtlInputChr *chr) {
    BtlCharInput *in = &chr->input;

    in->mask[3] = BtlInput_GetKeyMask(chr, 0);
    in->mask[2] = BtlInput_GetKeyMask(chr, 3);
    in->mask[0] = BtlInput_GetKeyMask(chr, 2);
    in->mask[1] = BtlInput_GetKeyMask(chr, 1);
    in->mask[11] = BtlInput_GetKeyMask(chr, 6);
    in->mask[12] = BtlInput_GetKeyMask(chr, 7);
    in->mask[9] = BtlInput_GetKeyMask(chr, 5);
    in->mask[14] = BtlInput_GetKeyMask(chr, 4);
    in->mask[13] = PAD_R3;
    in->mask[15] = PAD_L3 | PAD_R3;
    in->mask[4] = PAD_UP;
    in->mask[5] = PAD_DOWN;
    in->mask[6] = PAD_LEFT;
    in->mask[7] = PAD_RIGHT;
    in->mask[20] = in->mask[3] | in->mask[2] | in->mask[0] | in->mask[1];
    in->mask[23] = PAD_SELECT;
    in->mask[16] = PAD_RSTICK_UP;
    in->mask[17] = PAD_RSTICK_DOWN;
    in->mask[18] = PAD_RSTICK_LEFT;
    in->mask[19] = PAD_RSTICK_RIGHT;
    in->mask[25] = PAD_RSTICK_UP;
    in->mask[26] = PAD_RSTICK_DOWN;
    in->mask[8] = PAD_RSTICK_LEFT;
    in->mask[21] = 0;
    in->mask[22] = 0;
    in->mask[24] = in->mask[2];
    in->mask[10] = PAD_RSTICK_RIGHT;
    in->mask[27] = PAD_CIRCLE;
    in->mask[28] = PAD_CROSS;
    in->mask[29] = PAD_TRIANGLE;
    in->mask[30] = PAD_SQUARE;
}

/* Sets up the fighter's input: builds the mask table. */
void BtlInput_Init(BtlInputChr *chr) {
    BtlInput_BuildMaskTable(chr);
}

/* Samples the fighter's controller and builds this frame's input record. */
void BtlInput_Sample(BtlInputChr *chr) {
    Pad *pad = BtlChar_GetPad(chr);
    BtlCharInput *in = &chr->input;

    in->padStatus = pad->status;
    in->padLastStatus = pad->lastStatus;
    in->rawHeld = pad->held;
    in->rawStickX = BtlInput_StickToByte(pad->left[0]);
    in->rawStickY = BtlInput_StickToByte(pad->left[1]);
#ifdef PORT
    {
        /* Online the fight goes on under the pause menu (btl_seq.c BtlSeqFight_Update): the buttons of the player
           whose menu it is go to the menu and not to the fighter, who stands as with a pad left alone. */
        extern int Port_NetActive(void);
        extern s32 BtlPause_GetMenuPad(void);
        if (Port_NetActive() && (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE_MENU) && pad == &gPad[BtlPause_GetMenuPad()]) {
            in->rawHeld = 0;
            in->rawStickX = 0x7F;
            in->rawStickY = 0x7F;
        }
    }
#endif
    BtlInput_BuildRecord(chr);
}

/* Per-frame update: fetches the input and derives pressed/released words, the smoothed stick and the frame counters. */
/* FAKE MATCH: the empty `__asm__("" : "=r"(pressed) : "0"(pressed));` behind the computation of `pressed`. It
 * emits no instruction; it is one more register copy tied to `pressed` (two more references), and stands for
 * whatever gave the original's `pressed` six references instead of four: no natural source form was found.
 * Without it the code is the same with `sw v0` (the `pressed` store) in front of the three float stores instead of
 * behind them, or, with the store written where it is here, with v0 / v1 / a0 / a1 permuted among the four
 * results. Behaviour is identical either way (differential test of the original bytes against this C:
 * build/scratch_cleanup3_S/t_input.py, all branches).
 * What decides the first block (checked against the -dS / -dR / -dl dumps and gcc's local-alloc.c):
 * - first scheduling pass: one memory instruction and one other per cycle; the load of `held` goes first (the
 *   two stack loads wait one cycle behind the call), so its `nor` is the third instruction whatever the source
 *   order; float loads, `and`s and stores come out in source order;
 * - local register allocation: a `nor` temporary and its `and` result are one quantity (4 references); priority
 *   is floor_log2(refs) * refs / (position of the store - position of the `nor`), ties to the one born first;
 *   the four results overlap, so they take v0, v1, a0, a1 in priority order. The original's order is pressed,
 *   cmdPressed, released, cmdReleased;
 * - second scheduling pass: stores with equal dependents keep the order of the first pass; a store whose source
 *   register is set again before the next call (a0: the next argument, f0: the call's result) goes last.
 * So the `pressed` store stands behind the float stores in the source, which gives `pressed` 8 / 17 against
 * cmdPressed's 8 / 13; with four references no statement order can make it win (the `nor` is pinned at the
 * third instruction and the store cannot come earlier), with six it is 12 / 18. The float values and the four
 * results go through locals because the loads and the stores are in different orders. */
void BtlInput_Update(BtlInputChr *chr) {
    u8 stick[2];
    u32 buttons;
    u32 commands;
    BtlCharInput *in = &chr->input;
    f32 len;
    f32 rawX;
    f32 rawY;
    f32 stickX;
    f32 stickY;
    u32 pressed;
    u32 released;
    u32 cmdPressed;
    u32 cmdReleased;

    BtlChar_GetPad(chr);
    BtlInput_Fetch(chr, &buttons, &commands, stick);
    released = in->held & ~buttons;
    rawX = in->stickRaw[0];
    rawY = in->stickRaw[1];
    stickX = in->stick[0];
    stickY = in->stick[1];
    cmdReleased = in->cmdHeld & ~commands;
    pressed = buttons & ~in->held;
    __asm__("" : "=r"(pressed) : "0"(pressed));
    cmdPressed = commands & ~in->cmdHeld;
    in->stickPrev[0] = stickX;
    in->stickPrev[1] = stickY;
    in->stickRawPrev[0] = rawX;
    in->stickRawPrev[1] = rawY;
    in->pressed = pressed;
    in->released = released;
    in->cmdPressed = cmdPressed;
    in->cmdReleased = cmdReleased;
    in->prev = in->held;
    in->cmdPrev = in->cmdHeld;
    in->held = buttons;
    in->cmdHeld = commands;
    in->stickRaw[0] = BtlInput_ByteToStick(stick[0]);
    in->stickRaw[1] = BtlInput_ByteToStick(stick[1]);
    if (BtlChars_IsTimeStopped()) {
        in->stick[0] = in->stickRaw[0];
        in->stick[1] = in->stickRaw[1];
    } else {
        in->stick[0] += (in->stickRaw[0] - in->stick[0]) * 0.5f;
        in->stick[1] += (in->stickRaw[1] - in->stick[1]) * 0.5f;
    }
    len = sqrtf(in->stick[0] * in->stick[0] + in->stick[1] * in->stick[1]);
    if (len > 1.0f) {
        f32 inv = 1.0f / len;

        in->stick[0] *= inv;
        in->stick[1] *= inv;
    }
    if ((__builtin_fabsf(in->stickRaw[0]) > 0.01f || __builtin_fabsf(in->stickRaw[1]) > 0.01f) &&
        (__builtin_fabsf(in->stickRawPrev[0]) > 0.01f || __builtin_fabsf(in->stickRawPrev[1]) > 0.01f)) {
        in->stickTurn = BtlUtil_WrapAngle(atan2f(in->stickRaw[0], in->stickRaw[1]) - atan2f(in->stickRawPrev[0], in->stickRawPrev[1]));
    } else {
        in->stickTurn = 0.0f;
    }
    if (!BtlChars_IsTimeStopped()) {
        BtlInput_TickCounters(&in->count, in->held, in->pressed, in->released);
        BtlInput_TickCounters(&in->cmdCount, in->cmdHeld, in->cmdPressed, in->cmdReleased);
    }
    BtlInput_ClearFrameBits(chr);
}

/* Does nothing. */
void BtlInput_Stub(void) {
}

/* Queues a frame of input for the fighter (public form of BtlInput_RingPush). */
void BtlInput_Push(BtlInputChr *chr, u32 buttons, u32 commands, u8 *stick) {
    BtlInput_RingPush(chr, buttons, commands, stick);
}

/* Takes the oldest queued frame of input; every output pointer may be NULL. */
void BtlInput_Pop(BtlInputChr *chr, u32 *buttons, u32 *commands, u8 *stick) {
    u8 s[2];
    u32 b;
    u32 c;

    BtlInput_RingPop(chr, &b, &c, s);
    if (buttons != NULL) {
        *buttons = b;
    }
    if (commands != NULL) {
        *commands = c;
    }
    if (stick != NULL) {
        stick[0] = s[0];
        stick[1] = s[1];
    }
}

/* Returns the number of frames waiting in the ring. */
s32 BtlInput_GetQueued(BtlInputChr *chr) {
    BtlCharInput *in = &chr->input;

    return (in->ringWrite - in->ringRead + BTL_INPUT_RING) % BTL_INPUT_RING;
}

/* True when any button of mask went down this frame. */
s32 BtlInput_IsPressed(BtlInputChr *chr, u32 mask) {
    return (chr->input.pressed & mask) != 0;
}

/* True when a button of mask is down and none of mask was down last frame. */
s32 BtlInput_IsHeldNotPrev(BtlInputChr *chr, u32 mask) {
    if (chr->input.prev & mask) {
        return 0;
    }
    return (chr->input.held & mask) != 0;
}

/* True when any button of mask is down. */
s32 BtlInput_IsHeld(BtlInputChr *chr, u32 mask) {
    return (chr->input.held & mask) != 0;
}

/* True when any button of mask went up this frame. */
s32 BtlInput_IsReleased(BtlInputChr *chr, u32 mask) {
    return (chr->input.released & mask) != 0;
}

/* Frames button bit `bit` has been down. */
s32 BtlInput_GetHeldFrames(BtlInputChr *chr, s32 bit) {
    return chr->input.count.held.n[bit];
}

/* Frames button bit `bit` has been up. */
s32 BtlInput_GetNotHeldFrames(BtlInputChr *chr, s32 bit) {
    return chr->input.count.notHeld.n[bit];
}

/* Frames since button bit `bit` was last pressed. */
s32 BtlInput_GetFramesSincePress(BtlInputChr *chr, s32 bit) {
    return chr->input.count.sincePress.n[bit];
}

/* Frames since button bit `bit` was last released. */
s32 BtlInput_GetFramesSinceRelease(BtlInputChr *chr, s32 bit) {
    return chr->input.count.sinceRelease.n[bit];
}

/* Last frame's "frames since press" of button bit `bit` (on a press frame: the gap to the previous press). */
s32 BtlInput_GetPrevFramesSincePress(BtlInputChr *chr, s32 bit) {
    return chr->input.count.prevSincePress.n[bit];
}

/* True when any command of mask became set this frame. */
s32 BtlInput_IsCmdPressed(BtlInputChr *chr, u32 mask) {
    return (chr->input.cmdPressed & mask) != 0;
}

/* True when any command of mask is set. */
s32 BtlInput_IsCmdHeld(BtlInputChr *chr, u32 mask) {
    return (chr->input.cmdHeld & mask) != 0;
}

/* True when any command of mask became clear this frame. */
s32 BtlInput_IsCmdReleased(BtlInputChr *chr, u32 mask) {
    return (chr->input.cmdReleased & mask) != 0;
}

/* Frames command bit `bit` has been set. */
s32 BtlInput_GetCmdHeldFrames(BtlInputChr *chr, s32 bit) {
    return chr->input.cmdCount.held.n[bit];
}

/* Frames command bit `bit` has been clear. */
s32 BtlInput_GetCmdNotHeldFrames(BtlInputChr *chr, s32 bit) {
    return chr->input.cmdCount.notHeld.n[bit];
}

/* Last frame's "frames since set" of command bit `bit`. */
s32 BtlInput_GetCmdPrevFramesSincePress(BtlInputChr *chr, s32 bit) {
    return chr->input.cmdCount.prevSincePress.n[bit];
}

/* Sets a bit in one of the fighter's two per-frame bit sets. */
void BtlInput_SetFrameBit(BtlInputChr *chr, s32 bit, s32 set) {
    s32 byte = bit >> 3;
    s32 n = bit & 7;

    set = set != 0;
    chr->input.frameBits[set + byte * 2] |= 1 << n;
}

/* Tests a bit in one of the fighter's two per-frame bit sets. */
s32 BtlInput_TestFrameBit(BtlInputChr *chr, s32 bit, s32 set) {
    s32 byte = bit >> 3;
    s32 n = bit & 7;
    u8 mask;

    set = set != 0;
    mask = 1 << n;
    return (chr->input.frameBits[set + byte * 2] & mask) != 0;
}

/* Clears both per-frame bit sets. */
void BtlInput_ClearFrameBits(BtlInputChr *chr) {
    memset(chr->input.frameBits, 0, sizeof(chr->input.frameBits));
}

/*
 * Action input queries and stick readers: 0x1D4F30..0x1D60A0. Part of this translation unit in the original:
 * BtlInput_TestAction only compiles to the original bytes with BtlInput_IsHeld, BtlInput_IsReleased and
 * BtlInput_GetNotHeldFrames defined above it (8 branches are plain `bnez` instead of `bnezl`).
 *
 * BtlInput_TestAction(chr, id, want) is what the fighter state machine calls (166 direct call sites) to ask
 * "is action input `id` happening this frame". It first records the query in the per-frame bit sets
 * (BtlInput_SetFrameBit(chr, id, want): set 1 when want != 0, set 0 otherwise), then evaluates the condition and
 * returns it when want != 0 and its negation when want == 0. Ids outside 1..114 evaluate to false. The table
 * of ids is in include/battle/btl_char_ctl.h.
 * Side effects: ids 108..110, 113 and 114 consume a "forced command" fighter flag (0x122, 0x123, 0x124, 0x120,
 * 0x121): when the flag is set it is cleared together with flag 0x11E and the query is true whatever the pad says.
 * Ids 111 and 112 are true while flag 0x11F is set (not cleared here).
 */

/* Marks action input `id` as queried this frame, then tests it; returns the test when want != 0, its negation otherwise. */
s32 BtlInput_TestAction(BtlInputChr *chr, s32 id, s32 want) {
    s32 invert = want == 0;
    s32 result;

    BtlInput_SetFrameBit(chr, id, want);
    result = 0;
    switch (id) {
    case 1:
        if (!BtlInput_IsHeld(chr, BTLB_R3)) {
            result = BtlInput_IsHeld(chr, BTLB_DIR_MASK);
        }
        break;
    case 2:
        if (BtlInput_GetNotHeldFrames(chr, 4) < 3) {
            break;
        }
        if (BtlInput_GetNotHeldFrames(chr, 5) < 3) {
            break;
        }
        if (BtlInput_GetNotHeldFrames(chr, 6) >= 3 && BtlInput_GetNotHeldFrames(chr, 7) >= 3) {
            result = 1;
        }
        break;
    case 3:
        if (!BtlInput_IsHeld(chr, BTLB_R3)) {
            result = BtlInput_IsCmdHeld(chr, BTLC_DASH_HELD);
        }
        break;
    case 4:
        if (BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            if (fabsf(BtlInput_GetStickTurnFromFacing(chr)) > 0.78539816f) {
                result = BtlInput_IsHeldNotPrev(chr, BTLB_DIR_MASK);
            }
            if (fabsf(chr->input.stickTurn) > 1.4707963f) {
                result = 1;
            }
        }
        break;
    case 5:
        if (BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            if (fabsf(BtlInput_GetStickTurnFromFacing(chr)) > 0.78539816f) {
                result = BtlInput_IsCmdHeld(chr, BTLC_DASH_HELD);
            }
        }
        break;
    case 6:
        if (BtlInput_GetCmdNotHeldFrames(chr, 0) >= 6) {
            result = !BtlInput_IsHeld(chr, BTLB_DIR_MASK);
        }
        if (BtlInput_IsHeld(chr, BTLB_R3)) {
            result = 1;
        }
        break;
    case 7:
        if (!BtlInput_IsHeld(chr, BTLB_CHARGE)) {
            result = BtlInput_IsPressed(chr, BTLB_DASH);
        }
        break;
    case 8:
        result = BtlInput_IsPressed(chr, BTLB_ASCEND);
        break;
    case 9:
        result = BtlInput_IsPressed(chr, BTLB_ASCEND);
        break;
    case 10:
        if (!BtlInput_IsHeld(chr, BTLB_DESCEND)) {
            result = BtlInput_IsHeld(chr, BTLB_ASCEND);
        }
        break;
    case 12:
        if (!BtlInput_IsHeld(chr, BTLB_ASCEND)) {
            result = BtlInput_IsHeld(chr, BTLB_DESCEND);
        }
        break;
    case 14:
        if (!BtlInput_IsHeld(chr, BTLB_ASCEND)) {
            result = BtlInput_IsPressed(chr, BTLB_DESCEND);
        }
        break;
    case 11:
        result = !BtlInput_IsHeld(chr, BTLB_ASCEND) || BtlInput_IsHeld(chr, BTLB_DESCEND);
        break;
    case 13:
        result = !BtlInput_IsHeld(chr, BTLB_DESCEND) || BtlInput_IsHeld(chr, BTLB_ASCEND);
        break;
    case 15:
        result = BtlInput_IsPressed(chr, BTLB_ASCEND_TAP2);
        break;
    case 16:
        result = !BtlInput_IsHeld(chr, BTLB_ASCEND);
        break;
    case 17:
        result = BtlInput_IsPressed(chr, BTLB_DESCEND_TAP2);
        break;
    case 18:
        result = !BtlInput_IsHeld(chr, BTLB_DESCEND);
        break;
    case 20:
        if (!BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            result = BtlInput_IsCmdPressed(chr, BTLC_DASH_CHARGE_P);
        }
        break;
    case 21:
        if (BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            result = BtlInput_IsCmdPressed(chr, BTLC_DASH_CHARGE_P);
        }
        break;
    case 22:
        result = BtlInput_IsCmdPressed(chr, BTLC_DASH_P);
        break;
    case 23:
        if (!BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            result = BtlInput_IsCmdHeld(chr, BTLC_DASH_CHARGE_H);
        }
        break;
    case 24:
        if (BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            result = BtlInput_IsCmdHeld(chr, BTLC_DASH_CHARGE_H);
        }
        break;
    case 25:
        result = BtlInput_IsCmdPressed(chr, BTLC_DASH_P2) && BtlInput_IsCmdPressed(chr, BTLC_DIR_DOWN);
        break;
    case 26:
        result = BtlInput_IsCmdPressed(chr, BTLC_DASH_P2) && BtlInput_IsCmdPressed(chr, BTLC_DIR_LEFT);
        break;
    case 27:
        result = BtlInput_IsCmdPressed(chr, BTLC_DASH_P2) && BtlInput_IsCmdPressed(chr, BTLC_DIR_RIGHT);
        break;
    case 28:
        result = BtlInput_IsCmdPressed(chr, BTLC_DASH_P2) && BtlInput_IsCmdPressed(chr, BTLC_DIR_UP);
        break;
    case 30:
        result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H) && BtlInput_IsHeld(chr, BTLB_LEFT);
        break;
    case 31:
        result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H) && BtlInput_IsHeld(chr, BTLB_RIGHT);
        break;
    case 32:
        result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H) && BtlInput_IsHeld(chr, BTLB_UP);
        break;
    case 29:
        result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H) && BtlInput_IsHeld(chr, BTLB_DOWN);
        break;
    case 49:
        if (BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            if (!BtlInput_IsHeld(chr, BTLB_CHARGE)) {
                result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H);
            }
        }
        break;
    case 50:
        result = !BtlInput_IsCmdHeld(chr, BTLC_GUARD_H);
        break;
    case 51:
        result = BtlInput_IsCmdHeld(chr, BTLC_DIR_P);
        break;
    case 97:
        result = BtlInput_IsHeld(chr, BTLB_RS_LEFT);
        break;
    case 98:
        result = BtlInput_IsHeld(chr, BTLB_RS_RIGHT);
        break;
    case 19:
        result = BtlInput_IsHeld(chr, BTLB_CHARGE);
        break;
    case 34:
        result = BtlInput_IsCmdHeld(chr, BTLC_GUARD_H);
        break;
    case 35:
        result = BtlInput_IsCmdHeld(chr, BTLC_GUARD_H) && !BtlInput_IsHeld(chr, BTLB_UP) && !BtlInput_IsHeld(chr, BTLB_DOWN);
        break;
    case 36:
        result = BtlInput_IsCmdHeld(chr, BTLC_GUARD_H) && BtlInput_IsHeld(chr, BTLB_UP);
        break;
    case 37:
        result = BtlInput_IsCmdHeld(chr, BTLC_GUARD_H) && BtlInput_IsHeld(chr, BTLB_DOWN);
        break;
    case 38:
        result = BtlInput_IsPressed(chr, BTLB_UP) && BtlInput_IsPressed(chr, BTLB_RUSH);
        break;
    case 40:
        result = BtlInput_IsHeld(chr, BTLB_DOWN);
        break;
    case 41:
        result = BtlInput_IsHeld(chr, BTLB_UP);
        break;
    case 42:
        result = BtlInput_IsCmdPressed(chr, BTLC_DASH_P3);
        break;
    case 43:
        result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_SIDE);
        break;
    case 44:
        result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H2);
        break;
    case 45:
        if (BtlInput_IsHeld(chr, BTLB_DOWN)) {
            if (BtlInput_GetHeldFrames(chr, 11) >= 6 && BtlInput_GetHeldFrames(chr, 12) >= 6) {
                result = 1;
            }
        }
        break;
    case 46:
        if (BtlInput_IsCmdPressed(chr, BTLC_LOCKON_P)) {
            if (BtlInput_GetCmdPrevFramesSincePress(chr, 22) < BTL_TAP_FRAMES) {
                result = 1;
            }
        }
        break;
    case 47:
        if (BtlInput_IsHeld(chr, BTLB_DOWN)) {
            result = BtlInput_IsPressed(chr, BTLB_ANY_FACE);
        }
        break;
    case 48:
        if (BtlInput_IsHeld(chr, BTLB_UP)) {
            result = BtlInput_IsPressed(chr, BTLB_ANY_FACE);
        }
        break;
    case 52:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_TAP);
        break;
    case 53:
        result = BtlInput_IsCmdPressed(chr, BTLC_RUSH_HOLD6) && BtlInput_IsCmdHeld(chr, BTLC_DIR_UP);
        break;
    case 54:
        result = BtlInput_IsCmdPressed(chr, BTLC_RUSH_HOLD6) && BtlInput_IsCmdHeld(chr, BTLC_DIR_DOWN);
        break;
    case 55:
        result = BtlInput_IsCmdPressed(chr, BTLC_RUSH_HOLD6) && BtlInput_IsCmdHeld(chr, BTLC_DIR_LEFT);
        break;
    case 56:
        result = BtlInput_IsCmdPressed(chr, BTLC_RUSH_HOLD6) && BtlInput_IsCmdHeld(chr, BTLC_DIR_RIGHT);
        break;
    case 57:
        result = BtlInput_IsCmdPressed(chr, BTLC_RUSH_HOLD6) && !BtlInput_IsCmdHeld(chr, BTLC_DIR_ANY);
        break;
    case 58:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_H) && BtlInput_IsCmdHeld(chr, BTLC_DIR_UP);
        break;
    case 59:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_H) && BtlInput_IsCmdHeld(chr, BTLC_DIR_DOWN);
        break;
    case 60:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_H) && BtlInput_IsCmdHeld(chr, BTLC_DIR_LEFT);
        break;
    case 61:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_H) && BtlInput_IsCmdHeld(chr, BTLC_DIR_RIGHT);
        break;
    case 62:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_H) && !BtlInput_IsCmdHeld(chr, BTLC_DIR_ANY);
        break;
    case 63:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_NOT_H);
        break;
    case 64:
        if (!BtlInput_IsHeld(chr, BTLB_BLAST)) {
            result = BtlInput_IsPressed(chr, BTLB_RUSH);
        }
        break;
    case 65:
        result = !BtlInput_IsHeld(chr, BTLB_RUSH);
        break;
    case 66:
        if (!BtlInput_IsHeld(chr, BTLB_DIR_MASK | BTLB_RUSH)) {
            result = BtlInput_IsPressed(chr, BTLB_BLAST);
        }
        break;
    case 77:
        if (BtlInput_IsHeld(chr, BTLB_RUSH)) {
            break;
        }
        result = BtlInput_IsHeld(chr, BTLB_UP) && BtlInput_IsPressed(chr, BTLB_BLAST);
        break;
    case 78:
        if (BtlInput_IsHeld(chr, BTLB_RUSH)) {
            break;
        }
        result = BtlInput_IsHeld(chr, BTLB_DOWN) && BtlInput_IsPressed(chr, BTLB_BLAST);
        break;
    case 79:
        result = BtlInput_IsPressed(chr, BTLB_RUSH);
        break;
    case 80:
        result = BtlInput_IsPressed(chr, BTLB_BLAST);
        break;
    case 82:
        result = BtlInput_IsHeld(chr, BTLB_UP) && BtlInput_IsPressed(chr, BTLB_RUSH);
        break;
    case 83:
        result = BtlInput_IsHeld(chr, BTLB_UP) && BtlInput_IsPressed(chr, BTLB_BLAST);
        break;
    case 84:
        result = BtlInput_IsHeld(chr, BTLB_UP) && BtlInput_IsCmdPressed(chr, BTLC_GUARD_H);
        break;
    case 85:
        result = BtlInput_IsHeld(chr, BTLB_DOWN) && BtlInput_IsPressed(chr, BTLB_RUSH);
        break;
    case 86:
        result = BtlInput_IsHeld(chr, BTLB_DOWN) && BtlInput_IsPressed(chr, BTLB_BLAST);
        break;
    case 87:
        result = BtlInput_IsHeld(chr, BTLB_DOWN) && BtlInput_IsCmdPressed(chr, BTLC_GUARD_H);
        break;
    case 68:
        result = BtlInput_IsCmdHeld(chr, BTLC_BLAST_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_UP);
        break;
    case 69:
        result = BtlInput_IsCmdHeld(chr, BTLC_BLAST_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_DOWN);
        break;
    case 70:
        result = BtlInput_IsCmdHeld(chr, BTLC_BLAST_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_LEFT);
        break;
    case 71:
        result = BtlInput_IsCmdHeld(chr, BTLC_BLAST_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_RIGHT);
        break;
    case 74:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_UP);
        break;
    case 75:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_DOWN);
        break;
    case 72:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_LEFT);
        break;
    case 73:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_P) && BtlInput_IsCmdHeld(chr, BTLC_DIR_RIGHT);
        break;
    case 76:
        result = BtlInput_IsCmdHeld(chr, BTLC_RUSH_P) && !BtlInput_IsCmdHeld(chr, BTLC_DIR_ANY);
        break;
    case 92:
        if (!BtlInput_IsHeld(chr, BTLB_CHARGE)) {
            result = BtlInput_IsCmdPressed(chr, BTLC_DASH_TAP2);
        }
        break;
    case 93:
        result = BtlInput_IsCmdPressed(chr, BTLC_DASH_P4);
        break;
    case 94:
        if (BtlInput_IsHeld(chr, BTLB_CHARGE)) {
            break;
        }
        if (BtlInput_IsHeld(chr, BTLB_UP)) {
            result = BtlInput_IsPressed(chr, BTLB_BLAST);
        }
        break;
    case 95:
        if (BtlInput_IsCmdHeld(chr, BTLC_GUARD_H)) {
            result = BtlInput_IsPressed(chr, BTLB_BLAST2);
        }
        break;
    case 96:
        if (!BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H);
        }
        break;
    case 33:
    case 39:
    case 81:
        result = BtlInput_IsCmdPressed(chr, BTLC_GUARD_H);
        break;
    case 88:
        if (BtlInput_IsHeld(chr, BTLB_CHARGE | BTLB_RUSH)) {
            break;
        }
        if (BtlInput_IsReleased(chr, BTLB_BLAST)) {
            result = BtlInput_GetFramesSincePress(chr, 2) < 7;
        }
        if (BtlInput_IsHeld(chr, BTLB_BLAST)) {
            result = BtlInput_GetFramesSincePress(chr, 2) == 6;
        }
        break;
    case 89:
        if (BtlInput_IsHeld(chr, BTLB_CHARGE | BTLB_RUSH)) {
            break;
        }
        if (BtlInput_IsReleased(chr, BTLB_BLAST)) {
            result = BtlInput_GetFramesSincePress(chr, 2) < 7;
        }
        break;
    case 90:
        if (!BtlInput_IsHeld(chr, BTLB_CHARGE | BTLB_RUSH)) {
            if (BtlInput_IsHeld(chr, BTLB_BLAST)) {
                result = BtlInput_GetFramesSincePress(chr, 2) == 6;
            }
        }
        break;
    case 67:
    case 91:
        result = !BtlInput_IsHeld(chr, BTLB_BLAST);
        break;
    case 99:
        if (BtlInput_GetNotHeldFrames(chr, 15) < 30) {
            break;
        }
        if (!BtlInput_IsHeld(chr, BTLB_DIR_MASK)) {
            if (BtlInput_IsReleased(chr, BTLB_R3)) {
                result = BtlInput_GetFramesSincePress(chr, 13) < 13;
            }
        }
        break;
    case 100:
        if (BtlInput_GetNotHeldFrames(chr, 15) >= 30) {
            if (BtlInput_IsHeld(chr, BTLB_LEFT)) {
                if (BtlInput_IsReleased(chr, BTLB_R3)) {
                    result = BtlInput_GetFramesSincePress(chr, 13) < 13;
                }
            }
        }
        break;
    case 101:
        if (BtlInput_GetNotHeldFrames(chr, 15) >= 30) {
            if (BtlInput_IsHeld(chr, BTLB_UP)) {
                if (BtlInput_IsReleased(chr, BTLB_R3)) {
                    result = BtlInput_GetFramesSincePress(chr, 13) < 13;
                }
            }
        }
        break;
    case 102:
        if (BtlInput_GetNotHeldFrames(chr, 15) >= 30) {
            if (BtlInput_IsHeld(chr, BTLB_RIGHT)) {
                if (BtlInput_IsReleased(chr, BTLB_R3)) {
                    result = BtlInput_GetFramesSincePress(chr, 13) < 13;
                }
            }
        }
        break;
    case 103:
        if (BtlInput_GetNotHeldFrames(chr, 15) >= 30) {
            if (BtlInput_IsHeld(chr, BTLB_DOWN)) {
                if (BtlInput_IsReleased(chr, BTLB_R3)) {
                    result = BtlInput_GetFramesSincePress(chr, 13) < 13;
                }
            }
        }
        break;
    case 104:
        if (BtlInput_GetNotHeldFrames(chr, 15) >= 30) {
            if (BtlInput_IsHeld(chr, BTLB_LEFT)) {
                if (BtlInput_IsHeld(chr, BTLB_R3)) {
                    result = BtlInput_GetFramesSincePress(chr, 13) == 12;
                }
            }
        }
        break;
    case 105:
        if (BtlInput_GetNotHeldFrames(chr, 15) >= 30) {
            if (BtlInput_IsHeld(chr, BTLB_UP)) {
                if (BtlInput_IsHeld(chr, BTLB_R3)) {
                    result = BtlInput_GetFramesSincePress(chr, 13) == 12;
                }
            }
        }
        break;
    case 106:
        if (BtlInput_GetNotHeldFrames(chr, 15) >= 30) {
            if (BtlInput_IsHeld(chr, BTLB_RIGHT)) {
                if (BtlInput_IsHeld(chr, BTLB_R3)) {
                    result = BtlInput_GetFramesSincePress(chr, 13) == 12;
                }
            }
        }
        break;
    case 107:
        result = BtlInput_IsPressed(chr, BTLB_L3R3);
        break;
    case 108:
        result = BtlInput_IsCmdHeld(chr, BTLC_CHARGE_BLAST);
        if (BtlChar_TestFlag(chr, 0x122)) {
            BtlChar_ClearFlag(chr, 0x122);
            BtlChar_ClearFlag(chr, 0x11E);
            result = 1;
        }
        break;
    case 109:
        result = BtlInput_IsCmdHeld(chr, BTLC_CHARGE_BLAST_U);
        if (BtlChar_TestFlag(chr, 0x123)) {
            BtlChar_ClearFlag(chr, 0x123);
            BtlChar_ClearFlag(chr, 0x11E);
            result = 1;
        }
        break;
    case 110:
        result = BtlInput_IsCmdHeld(chr, BTLC_CHARGE_BLAST_D);
        if (BtlChar_TestFlag(chr, 0x124)) {
            BtlChar_ClearFlag(chr, 0x124);
            BtlChar_ClearFlag(chr, 0x11E);
            result = 1;
        }
        break;
    case 111:
        result = BtlInput_IsCmdHeld(chr, BTLC_SWITCH);
        if (BtlChar_TestFlag(chr, 0x11F)) {
            result = 1;
        }
        break;
    case 112:
        result = BtlInput_IsCmdHeld(chr, BTLC_UNUSED2000);
        if (BtlChar_TestFlag(chr, 0x11F)) {
            result = 1;
        }
        break;
    case 113:
        result = BtlInput_IsCmdHeld(chr, BTLC_CHARGE_GUARD);
        if (BtlChar_TestFlag(chr, 0x120)) {
            BtlChar_ClearFlag(chr, 0x120);
            BtlChar_ClearFlag(chr, 0x11E);
            result = 1;
        }
        break;
    case 114:
        result = BtlInput_IsCmdHeld(chr, BTLC_CHARGE_GUARD_U);
        if (BtlChar_TestFlag(chr, 0x121)) {
            BtlChar_ClearFlag(chr, 0x121);
            BtlChar_ClearFlag(chr, 0x11E);
            result = 1;
        }
        break;
    }
    return result ^ invert;
}

/* Returns the smoothed stick x. */
f32 BtlInput_GetStickX(BtlInputChr *chr) {
    return chr->input.stick[0];
}

/* Returns the smoothed stick y. */
f32 BtlInput_GetStickY(BtlInputChr *chr) {
    return chr->input.stick[1];
}

/* Returns the length of the smoothed stick vector. */
f32 BtlInput_GetStickLength(BtlInputChr *chr) {
    return sqrtf(chr->input.stick[0] * chr->input.stick[0] + chr->input.stick[1] * chr->input.stick[1]);
}

/* Returns the direction of the raw stick as an angle, atan2(x, -y): 0 = up, positive = right; 0 when both axes are under 0.01. */
f32 BtlInput_GetStickAngle(BtlInputChr *chr) {
    f32 x = chr->input.stickRaw[0];
    f32 y = chr->input.stickRaw[1];

    if (fabsf(x) < 0.01f && fabsf(y) < 0.01f) {
        return 0.0f;
    }
    return atan2f(x, -y);
}

/* Returns how far the stick direction is from the fighter's facing: stick angle - (heading - camera yaw), wrapped. */
f32 BtlInput_GetStickTurnFromFacing(BtlInputChr *chr) {
    return BtlUtil_WrapAngle(BtlInput_GetStickAngle(chr) - BtlAct_GetFacingRelCam(chr));
}
