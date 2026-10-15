#include "common.h"
#include "battle/btl_char_get.h"

/*
 * Battle helpers and fighter accessors: 0x1DBF20..0x1DC9A0.
 * Scalar helpers first, then the roster lookups, the fighters' frame counter and random generator,
 * pad vibration and voice lines. The object file this belongs to is larger (its float constants run on
 * from 0x1DBAD0's and into 0x1DCB88's), so the range is a slice of it, not a whole source file.
 */

extern f32 sqrtf(f32 x);
extern f32 sinf(f32 x);
extern void *BtlObj_Get(s32 id);
extern void BtlObj_SetSubState(void *obj, s32 state, s32 arg);
extern s32 BtlChar_TestFlag(BtlCharGetChr *chr, s32 bit);
extern s32 BattleReplay_IsActive(void);
#ifdef PORT
extern s32 Port_RumbleTest(void); /* port/src/gs/gs_input.c */
#endif
extern s32 Battle_GetStage(void);
extern s32 Battle_GetMode(void);
extern s32 Voice_IsStopped(s32 voice);
extern void Voice_Stop(s32 voice);
extern BtlCharGetVitals *BtlMember_GetActiveGauge(BtlCharGetChr *chr); /* active member's block + 0x40 */
extern s32 BtlChars_IsTimeStopped(void);                             /* gBtlChars + 0x274 */
extern void BtlCharSnd_PlayOwn(BtlCharGetChr *chr, s32 line);    /* sound request, channel chr->player + 2 */
extern void BtlCharSnd_PlayVoice(BtlCharGetChr *chr, s32 line);    /* sound request, channel chr->player + 5 */
extern void BtlCharSnd_StopOwn(BtlCharGetChr *chr, s32 line);
extern s32 BtlScript_IsTextShown(void);

extern BtlCharGetRoster *gBtlChars;

/* Brings an angle back into -pi..pi (one turn at most). */
f32 BtlUtil_WrapAngle(f32 a) {
    if (a < -3.14159265f) {
        a += 6.2831853f;
    }
    if (a > 3.14159265f) {
        a -= 6.2831853f;
    }
    return a;
}

/* Wraps the three angles of src into dst; w is copied. */
void BtlUtil_WrapAngles(Vec4 *dst, Vec4 *src) {
    dst->x = BtlUtil_WrapAngle(src->x);
    dst->y = BtlUtil_WrapAngle(src->y);
    dst->z = BtlUtil_WrapAngle(src->z);
    dst->w = src->w;
}

/* Horizontal length of a vector. */
f32 BtlUtil_LengthXZ(Vec4 *v) {
    return sqrtf(v->x * v->x + v->z * v->z);
}

/* Moves cur toward target by |step| without passing it. */
f32 BtlUtil_ApproachF(f32 cur, f32 target, f32 step) {
    step = __builtin_fabsf(step);
    if (cur + step < target) {
        cur += step;
    } else {
        cur -= step;
        if (!(target < cur)) {
            cur = target;
        }
    }
    return cur;
}

/* Integer form of BtlUtil_ApproachF. */
s32 BtlUtil_Approach(s32 cur, s32 target, s32 step) {
    step = __builtin_abs(step);
    if (cur + step < target) {
        cur += step;
    } else if (target < cur - step) {
        cur -= step;
    } else {
        cur = target;
    }
    return cur;
}

/* Clamps v to lo..hi. */
f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi) {
    if (v < lo) {
        v = lo;
    }
    if (hi < v) {
        v = hi;
    }
    return v;
}

/* Smaller of two floats. */
f32 BtlUtil_MinF(f32 a, f32 b) {
    if (b < a) {
        a = b;
    }
    return a;
}

/* Larger of two floats. */
f32 BtlUtil_MaxF(f32 a, f32 b) {
    if (a < b) {
        a = b;
    }
    return a;
}

/* Clamps v to lo..hi. */
s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi) {
    if (v < lo) {
        v = lo;
    }
    if (hi < v) {
        v = hi;
    }
    return v;
}

/* Smaller of two integers. */
s32 BtlUtil_Min(s32 a, s32 b) {
    return b < a ? b : a;
}

/* Larger of two integers. */
s32 BtlUtil_Max(s32 a, s32 b) {
    return a < b ? b : a;
}

/* Rounds n up to a multiple of unit. */
s32 BtlUtil_RoundUp(s32 n, s32 unit) {
    s32 r = n % unit;

    if (r != 0) {
        n += unit - r;
    }
    return n;
}

/* Quarter-turn sector of an angle: 0 around 0, 2 around -pi/2, 3 around +pi/2, 1 around +-pi. */
s32 BtlUtil_AngleToSector(f32 a) {
    if (a < -2.35619449f) {
        return 1;
    }
    if (a < -0.78539816f) {
        return 2;
    }
    if (a < 0.78539816f) {
        return 0;
    }
    if (a < 2.35619449f) {
        return 3;
    }
    return 1;
}

/* Number of fighters. */
s32 BtlChar_GetCount(void) {
    return gBtlChars->count;
}

/* Fighter i. */
BtlCharGetChr *BtlChar_Get(s32 i) {
    return &gBtlChars->chars[i];
}

/* Fighter that owns battle object objId, or NULL. */
BtlCharGetChr *BtlChar_FindByObjId(s32 objId) {
    s32 i;

    if (gBtlChars == NULL) {
        return NULL;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlCharGetChr *chr = BtlChar_Get(i);

        if (chr->objId == objId) {
            return chr;
        }
    }
    return NULL;
}

/* Fighter whose side word is `side`, or NULL. */
BtlCharGetChr *BtlChar_FindBySide(s32 side) {
    s32 i;

    if (gBtlChars == NULL) {
        return NULL;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlCharGetChr *chr = BtlChar_Get(i);

        if (chr->side == side) {
            return chr;
        }
    }
    return NULL;
}

/* The fighter's battle object. */
void *BtlChar_GetObj(BtlCharGetChr *chr) {
    return BtlObj_Get(chr->objId);
}

/* Address of the fighter's position. */
Vec4 *BtlChar_GetPos(BtlCharGetChr *chr) {
    return &chr->pos;
}

/* The fighter's controller. */
Pad *BtlChar_GetPad(BtlCharGetChr *chr) {
    return &gPad[chr->pad];
}

/* 1 while the fighter's freeze counter is running. */
s32 BtlChar_IsFrozen(BtlCharGetChr *chr) {
    return chr->freeze > 0;
}

/* 1 when the fighter is alive, flag 0xBE is clear and counter 0xFE0 has run out. */
s32 BtlChar_IsFree(BtlCharGetChr *chr) {
    if (BtlChar_IsDead(chr) || BtlChar_TestFlag(chr, 0xBE)) {
        return 0;
    }
    return chr->unkFE0 <= 0;
}

/* 1 when the active member has no health left. */
s32 BtlChar_IsDead(BtlCharGetChr *chr) {
    return BtlMember_GetActiveGauge(chr)->hp <= 0;
}

/* 1 when the active member's word 0x70 is set. */
s32 BtlChar_TestMemberUnk70(BtlCharGetChr *chr) {
    return BtlMember_GetActiveGauge(chr)->unk30 != 0;
}

/* Frames simulated since the fighters were reset. No caller. */
s32 BtlChar_GetFrame(void) {
    return gBtlChars->frame;
}

/* Frame counter modulo n (n < 1 counts as 1): the cheap "random" pick used for voice lines and the like. */
s32 BtlChar_FrameMod(s32 n) {
    if (n <= 0) {
        n = 1;
    }
    return gBtlChars->frame % n;
}

/* The fighters' random generator: 0..150888. The state is kept unchanged while roster+0x274 is set. */
s32 BtlChar_Rand(void) {
    u64 state = gBtlChars->randState;
    s32 next = (state * 714025 + 4096) % 150889;

    if (!BtlChars_IsTimeStopped()) {
        gBtlChars->randState = next;
    }
    return next;
}

/* BtlChar_Rand as a float in 0..1. */
f32 BtlChar_RandF(void) {
    return (f32)(u32)BtlChar_Rand() / 150889.0f;
}

/* 1 on stages 4 and 27. */
s32 BtlChar_IsStage4Or27(void) {
    s32 stage = Battle_GetStage();

    if (stage == 4 || stage == 27) {
        return 1;
    }
    return 0;
}

/* On stages 4 and 27, adds time to the roster's stage timer (at most 150 frames). */
void BtlChar_AddStageTimer(f32 seconds) {
    s32 frames = seconds * 30.0f;

    if (BtlChar_IsStage4Or27() && gBtlChars != NULL) {
        gBtlChars->stageTimer = BtlUtil_Min(gBtlChars->stageTimer + frames, 150);
    }
}

/* Asks for big-motor vibration; a weaker request does not replace a running stronger one. */
void BtlChar_SetVibration(BtlCharGetChr *chr, f32 power, f32 seconds) {
    BtlCharVib *vib = &chr->vib;

    if (power > 1.0f) {
        power = 1.0f;
    }
    if (vib->time <= 0 || vib->power < power) {
        vib->power = power;
        vib->time = seconds * 30.0f;
    }
}

/* Asks for small-motor vibration; only ever lengthens it. */
void BtlChar_SetSmallVibration(BtlCharGetChr *chr, f32 seconds) {
    BtlCharVib *vib = &chr->vib;
    s32 frames = seconds * 30.0f;

    if (vib->smallTime < frames) {
        vib->smallTime = frames;
    }
}

/* Both motors. */
void BtlChar_Vibrate(BtlCharGetChr *chr, f32 power, f32 seconds) {
    BtlChar_SetVibration(chr, power, seconds);
    BtlChar_SetSmallVibration(chr, seconds);
}

/* Per frame: sends the fighter's vibration to its pad. Nothing when disabled, in a replay or for a CPU. */
void BtlChar_UpdateVibration(BtlCharGetChr *chr) {
    BtlCharVib *vib = &chr->vib;

#ifdef PORT /* PC build: BT3_RUMBLE_TEST=1 lets a replay vibrate as the fight it shows did (testing without a player) */
    if (!vib->enabled || (BattleReplay_IsActive() && !Port_RumbleTest()) || chr->injectOn) {
#else
    if (!vib->enabled || BattleReplay_IsActive() || chr->injectOn) {
#endif
        vib->time = 0;
        vib->smallTime = 0;
    } else {
        s32 pad = chr->pad;
        s32 small = 0;
        f32 power = 0.0f;
        s32 t;
        s32 st;

        if (vib->time > 0) {
            f32 half = 0.5f;

            power = vib->power * ((sinf((f32)vib->phase * 37.6991118f / 30.0f) * half + half) * half + half);
            vib->phase++;
        } else {
            vib->phase = 0;
        }
        if (vib->smallTime > 0) {
            small = (u32)vib->toggle % 2 == 0;
            vib->toggle++;
        } else {
            vib->toggle = 0;
        }
        Pad_AddVibration(pad, small, power);
        t = vib->time - 1;
        st = vib->smallTime - 1;
        if (t < 0) {
            t = 0;
        }
        if (st < 0) {
            st = 0;
        }
        vib->time = t;
        vib->smallTime = st;
    }
}

/* Entry of the voice table for a kind. */
BtlVoiceEntry *BtlChar_GetVoiceEntry(s32 kind) {
    return &gBtlChars->voiceTbl[kind];
}

/* Plays one of the lines of a voice kind unless the fighter is dead or the kind is cooling down. */
void BtlChar_PlayVoice(BtlCharGetChr *chr, s32 kind) {
    void *obj = BtlChar_GetObj(chr);
    BtlVoiceEntry *ent;
    s32 line;

    if (BtlChar_IsDead(chr)) {
        return;
    }
    if (chr->voice.timer[kind] > 0) {
        return;
    }
    if (Battle_GetMode() == 1 && kind >= 14) {
        if (kind < 17) {
            return;
        }
        if (kind == 34) {
            return;
        }
    }
    ent = BtlChar_GetVoiceEntry(kind);
    if (ent->count >= 2) {
        line = ent->first + BtlChar_FrameMod(ent->count - 1);
        if (line >= chr->voice.last[kind]) {
            line++;
        }
    } else {
        line = ent->first;
    }
    if (kind < 15) {
        BtlCharSnd_PlayOwn(chr, line);
        BtlObj_SetSubState(obj, 4, 0);
    } else {
        BtlCharSnd_PlayVoice(chr, line);
        BtlObj_SetSubState(obj, 2, line);
    }
    chr->voice.last[kind] = line;
    chr->voice.timer[kind] = ent->interval * 30.0f;
}

/* Stops the fighter's voice while flag 0x129 is set (in mode 1 only when BtlScript_IsTextShown returns 0). */
void BtlChar_StopVoiceOnFlag(BtlCharGetChr *chr) {
    if (Battle_GetMode() == 1) {
        if (BtlScript_IsTextShown()) {
            return;
        }
    }
    if (BtlChar_TestFlag(chr, 0x129) && !Voice_IsStopped(chr->player)) {
        Voice_Stop(chr->player);
    }
}

/* Per frame: counts the voice cooldowns down. */
void BtlChar_TickVoiceTimers(BtlCharGetChr *chr) {
    s32 i;

    for (i = 0; i < BTL_VOICE_KINDS; i++) {
        if (chr->voice.timer[i] > 0) {
            chr->voice.timer[i]--;
        }
    }
}

/* Plays the line of a voice kind that has only one, through BtlCharSnd_StopOwn. */
void BtlChar_PlayVoiceSingle(BtlCharGetChr *chr, s32 kind) {
    BtlVoiceEntry *ent = BtlChar_GetVoiceEntry(kind);

    if (ent->count < 2) {
        BtlCharSnd_StopOwn(chr, ent->first);
    }
}
