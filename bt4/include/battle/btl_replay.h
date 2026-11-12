#ifndef BATTLE_BTL_REPLAY_H
#define BATTLE_BTL_REPLAY_H

#include "types.h"

/*
 * Replay input tracks: what lives in gBattleReplay.data (battle/battle_setup.h declares that part as an
 * opaque BtlReplayData of 0x1A5F0 bytes; this is its real layout).
 *
 *   gBattleReplay (0x301268, 0x1ABA8 bytes)
 *     +0x00000  BattleSetup setup            0x5A8   "btls" / 7, the whole battle setup
 *     +0x005A8  BtlReplayTrack track[0]      0xD2F8  fighter with chr->player == 0
 *     +0x0D8A0  BtlReplayTrack track[1]      0xD2F8  fighter with chr->player == 1
 *     +0x1AB98  s32 flags                            bit 0 = BTL_REPLAY_END
 *     +0x1AB9C  s32 active                           playing back (not part of the saved meaning: Load sets 1)
 *     +0x1ABA0  s32 loaded
 *     +0x1ABA4  s32 unk
 *
 * Nothing else is stored: no random seed, no frame counter other than each track's count, no result.
 */

#define BTL_REPLAY_FRAMES 9000 /* 5 minutes at 30 frames per second */
#define BTL_REPLAY_END 1       /* BtlReplayTracks.flags: recording hit the limit / playback ran out of frames */

/* One fighter's recorded input. */
typedef struct BtlReplayTrack {
    /* 0x0000 */ u8 stick[BTL_REPLAY_FRAMES][2]; /* BtlInputRecord stickX, stickY (0..0xFE, 0x7F neutral) */
    /* 0x4650 */ u32 buttons[BTL_REPLAY_FRAMES]; /* BtlInputRecord buttons (BTLB_*) */
    /* 0xD2F0 */ s32 count;                      /* frames recorded */
    /* 0xD2F4 */ s32 pos;                        /* next frame to play */
} BtlReplayTrack; /* size 0xD2F8 */

/* What BattleReplay_GetData() really points at (same memory as BtlReplayRec). */
typedef struct BtlReplayTracks {
    /* 0x00000 */ BtlReplayTrack track[2];
    /* 0x1A5F0 */ s32 flags; /* BTL_REPLAY_END */
} BtlReplayTracks; /* size 0x1A5F4 */

/* Replay viewer state inside the fighter roster (gBtlChars + 0x130). */
typedef struct BtlReplayViewer {
    /* 0x0 */ s32 row;  /* up/down cursor; always forced back to 0 by BtlReplay_UpdateViewer */
    /* 0x4 */ s32 side; /* side whose camera is shown full screen, 0 or 1 (left/right) */
} BtlReplayViewer;

/* Partial view of the roster (gBtlChars, 0x280 bytes). */
typedef struct BtlReplayRoster {
    /* 0x000 */ u8 unk0[0x130];
    /* 0x130 */ BtlReplayViewer viewer;
} BtlReplayRoster;

/* Partial view of a fighter. */
typedef struct BtlReplayChr {
    /* 0x0000 */ s32 player; /* track index */
} BtlReplayChr;

void BtlReplay_Rewind(BtlReplayChr *chr);
void BtlReplay_Record(BtlReplayChr *chr, u32 buttons, u32 commands, u8 *stick);
void BtlReplay_Play(BtlReplayChr *chr, u32 *buttons, u8 *stick);
void BtlReplay_ResetViewer(void);
void BtlReplay_UpdateViewer(void);
s32 BtlReplay_GetViewSide(void);

#endif
