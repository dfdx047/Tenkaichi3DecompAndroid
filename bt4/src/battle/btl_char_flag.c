#include "common.h"
#include "battle/btl_char_flag.h"
#include "battle/battle_setup.h"

/*
 * Fighter flags: 0x1DA8B8..0x1DB048. The layout and the stage mechanism are described in the header.
 */

extern BtlFlagChr *BtlChar_Get(s32 i);
extern f32 BtlChar_GetSpacing(BtlFlagChr *chr, s32 arg);          /* lock-on range */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);   /* world position of a model node */
extern void ColSeg_Set(void *seg, Vec4 *a, Vec4 *b);      /* builds a segment from two points */
extern s32 StgCol_TraceSegment(void *seg);                         /* segment against the stage */

/* Promotes every flag last written in the given stage: previous := current, plain flag cleared. */
void BtlChar_PromoteFlags(BtlFlagChr *chr, u8 stage) {
    s32 n;

    for (n = 0; n < BTL_FLAG_COUNT; n++) {
        if (chr->stamp[n] == stage) {
            s32 i = n >> 3;
            u8 m = 1 << (n & 7);

            chr->prevHeld[i] &= ~m;
            chr->prevHeld[i] |= chr->held[i] & m;
            chr->prevPulse[i] &= ~m;
            chr->prevPulse[i] |= chr->pulse[i] & m;
            chr->pulse[i] &= ~m;
        }
    }
}

/* Enters a stage of the frame: promotes the flags written in that stage a frame ago (and stage 1's when the frame wraps). */
void BtlChar_SetStage(BtlFlagChr *chr, u8 stage) {
    if (stage < chr->stage) {
        BtlChar_PromoteFlags(chr, 1);
    }
    BtlChar_PromoteFlags(chr, stage);
    chr->stage = stage;
}

/* Current stage of the frame. */
u32 BtlChar_GetStage(BtlFlagChr *chr) {
    return chr->stage;
}

/* Raises a held flag: it stays up until BtlChar_ClearFlag. */
void BtlChar_SetHeldFlag(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);

    if (chr->stage < chr->stamp[n]) {
        chr->prevHeld[i] &= ~m;
        chr->prevHeld[i] |= chr->held[i] & m;
    }
    chr->held[i] |= m;
    chr->stamp[n] = chr->stage;
}

/* Drops a flag of either kind. */
void BtlChar_ClearFlag(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);

    if (chr->stage < chr->stamp[n]) {
        chr->prevHeld[i] &= ~m;
        chr->prevHeld[i] |= chr->held[i] & m;
    }
    chr->held[i] &= ~m;
    chr->pulse[i] &= ~m;
    chr->stamp[n] = chr->stage;
}

/* Drops flags a..b inclusive (in either order). */
void BtlChar_ClearFlagRange(BtlFlagChr *chr, u32 a, u32 b) {
    u32 n;

    if (b < a) {
        u32 t = a;
        a = b;
        b = t;
    }
    for (n = a; n <= b; n++) {
        BtlChar_ClearFlag(chr, n);
    }
}

/* Inverts a held flag. */
void BtlChar_ToggleHeldFlag(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);

    if (chr->stage < chr->stamp[n]) {
        chr->prevHeld[i] &= ~m;
        chr->prevHeld[i] |= chr->held[i] & m;
    }
    chr->held[i] ^= m;
    chr->stamp[n] = chr->stage;
}

/* Raises a plain flag: it drops by itself when the stage it was raised in comes round again. */
void BtlChar_SetFlag(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);

    if (chr->stage < chr->stamp[n]) {
        chr->prevPulse[i] &= ~m;
        chr->prevPulse[i] |= chr->pulse[i] & m;
        chr->pulse[i] &= ~m;
    }
    chr->pulse[i] |= m;
    chr->stamp[n] = chr->stage;
}

/* 1 when the flag is up (held or plain). */
s32 BtlChar_TestFlag(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);
    u8 cur = chr->held[i] | chr->pulse[i];

    return (cur & m) != 0;
}

/* 1 when the flag was up a frame ago. */
s32 BtlChar_TestPrevFlag(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);
    u8 prev = chr->prevHeld[i] | chr->prevPulse[i];

    return (prev & m) != 0;
}

/* 1 when the flag is up and was not a frame ago. */
s32 BtlChar_IsFlagRaised(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);

    return ((chr->held[i] | chr->pulse[i]) & ~(chr->prevHeld[i] | chr->prevPulse[i]) & m) != 0;
}

/* 1 when the flag was up a frame ago and is not now. */
s32 BtlChar_IsFlagDropped(BtlFlagChr *chr, u32 n) {
    u32 i = n >> 3;
    u8 m = 1 << (n & 7);

    u8 cur = chr->held[i] | chr->pulse[i];
    u8 prev = chr->prevHeld[i] | chr->prevPulse[i];

    return (prev & ~cur & m) != 0;
}

/* Lock-on: drops flag 5 on flags 0x93 / 0xB2, and raises 0x13 and 5 when the opponent is within range. */
void BtlChar_UpdateLockOn(BtlFlagChr *chr) {
    f32 range;

    if (BtlChar_TestFlag(chr, 0x93)) {
        BtlChar_ClearFlag(chr, 5);
    }
    if (BtlChar_TestFlag(chr, 0xB2)) {
        BtlChar_ClearFlag(chr, 5);
    }
    BtlChar_ClearFlag(chr, 0x13);
    if (!BtlChar_TestFlag(chr, 0x1B) && !BtlChar_TestFlag(chr, 0x93) && chr->unkFB0 < 3 &&
        !BtlChar_TestFlag(chr, 0xB2)) {
        range = BtlChar_GetSpacing(chr, 0);
        if (BtlOpp_GetDistance(chr) < range) {
            BtlChar_SetHeldFlag(chr, 0x13);
            BtlChar_SetHeldFlag(chr, 5);
        }
    }
}

/* Raises held flag 0xBA on both fighters when the segment between their nodes 0x2F hits the stage. */
void BtlChars_UpdateSightFlag(void) {
    u8 seg[0x20];
    Vec4 a;
    Vec4 b;
    BtlFlagChr *chr0 = BtlChar_Get(0);
    BtlFlagChr *chr1 = BtlChar_Get(1);

    BtlChar_ClearFlag(chr0, 0xBA);
    BtlChar_ClearFlag(chr1, 0xBA);
    BtlCharApi_GetNodePos(chr0->objId, 0x2F, &a);
    BtlCharApi_GetNodePos(chr1->objId, 0x2F, &b);
    ColSeg_Set(seg, &a, &b);
    if (StgCol_TraceSegment(seg)) {
        BtlChar_SetHeldFlag(chr0, 0xBA);
        BtlChar_SetHeldFlag(chr1, 0xBA);
    }
}

/* Sets bit n of the fighter's set-once bits. */
void BtlChar_SetOnceBit(BtlFlagChr *chr, s32 n) {
    chr->bits.once[n >> 3] |= 1 << (n & 7);
}

/* 1 when bit n of the set-once bits is set. */
s32 BtlChar_TestOnceBit(BtlFlagChr *chr, s32 n) {
    u8 m = 1 << (n & 7);

    return (chr->bits.once[n >> 3] & m) != 0;
}

/* Adds bits to the per-frame bit set. */
void BtlChar_SetFrameBits(BtlFlagChr *chr, u64 bits) {
    chr->bits.frame |= bits;
}

/* The first clash won in a battle (by either side): set-once bit 0, flag 0xE3 and battle event 0x3C for this fighter. */
void BtlChar_RaiseFirstClash(BtlFlagChr *chr) {
    if (!BtlChar_TestOnceBit(BtlChar_Get(0), 0) && !BtlChar_TestOnceBit(BtlChar_Get(1), 0)) {
        BtlChar_SetOnceBit(chr, 0);
        BtlChar_SetFlag(chr, 0xE3);
        BtlEvent_Raise(chr->player, 0x3C);
    }
}
