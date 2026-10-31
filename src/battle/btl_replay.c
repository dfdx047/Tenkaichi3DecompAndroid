#include "common.h"
#include "battle/btl_replay.h"
#include "battle/battle.h"
#include "sys/pad.h"

/*
 * Replay input tracks and the replay viewer: 0x1D8330..0x1D8750.
 *
 * BtlReplay_Rewind       per fighter, from the fighter reset (0x1C02C8, run by BtlChar_ResetAll): Battle_Restart
 *                        and the first frame of the Ready / Fight sequence state.
 * BtlReplay_Record/Play  the two hooks of BtlInput_BuildRecord, once per fighter per unpaused frame on which the
 *                        fighter takes input. A frame on which it does not is neither recorded nor consumed, so
 *                        a track index is not a frame number.
 * BtlReplay_*Viewer      pad 0 while a replay plays: left / right choose the side to watch (read by the camera
 *                        through 0x2080C8), up / down cycle a HUD mode.
 * Layout of the data: battle/btl_replay.h.
 */

extern BtlReplayTracks *BattleReplay_GetData(void);
extern s32 BattleReplay_IsActive(void);
extern s32 BtlMenu_IsActive(void); /* word 8 of the pause / text work (gBtlMenu) */
extern void Hud_SetReplayMode(s32 mode); /* HUD work (gHud) + 0x34 = mode */

extern BtlReplayRoster *gBtlChars;
extern s32 gBtlReplayHudMode;

/* Rewinds the fighter's track; when no replay is being played it is also emptied, ready to record. */
void BtlReplay_Rewind(BtlReplayChr *chr) {
#ifdef PORT
    /* The stock replay file contains exactly two input tracks. Native team
     * matches must not read or write beyond that serialized format. */
    if (TagTeam_Active()) return;
#endif
    BtlReplayTrack *trk = &BattleReplay_GetData()->track[chr->player];

    trk->pos = 0;
    if (!BattleReplay_IsActive()) {
        trk->count = 0;
    }
}

/* Appends this frame's stick bytes and button word to the fighter's track; a full track raises the end flag. */
void BtlReplay_Record(BtlReplayChr *chr, u32 buttons, u32 commands, u8 *stick) {
#ifdef PORT
    if (TagTeam_Active()) return;
#endif
    BtlReplayTracks *data = BattleReplay_GetData();
    BtlReplayTrack *trk = &data->track[chr->player];

    if (trk->count >= BTL_REPLAY_FRAMES) {
        data->flags |= BTL_REPLAY_END;
        return;
    }
    trk->stick[trk->count][0] = stick[0];
    trk->stick[trk->count][1] = stick[1];
    trk->buttons[trk->count] = buttons;
    trk->count++;
}

/* Reads the fighter's next recorded frame; past the last one raises the end flag and gives neutral input. */
void BtlReplay_Play(BtlReplayChr *chr, u32 *buttons, u8 *stick) {
    BtlReplayTracks *data = BattleReplay_GetData();
    BtlReplayTrack *trk = &data->track[chr->player];

    if (trk->pos >= trk->count) {
        data->flags |= BTL_REPLAY_END;
        *buttons = 0;
        stick[1] = stick[0] = 0x7F;
        return;
    }
    stick[0] = trk->stick[trk->pos][0];
    stick[1] = trk->stick[trk->pos][1];
    *buttons = trk->buttons[trk->pos];
    trk->pos++;
}

/* Clears the replay viewer state. */
void BtlReplay_ResetViewer(void) {
    if (gBtlChars != NULL) {
        *(u64 *)&gBtlChars->viewer = 0;
    }
}

/* While a replay plays: pad 0 up/down cycles the HUD mode (0..2), left/right picks the side being watched. */
void BtlReplay_UpdateViewer(void) {
    Pad *pad = &gPad[0];
    BtlReplayViewer *v;

    if (gBtlChars == NULL) {
        return;
    }
    v = &gBtlChars->viewer;
    if (!BattleReplay_IsActive()) {
        return;
    }
    if (BattleReplay_GetData()->flags & BTL_REPLAY_END) {
        return;
    }
    if (pad->gamePressed & 8) {
        v->row--;
    }
    if (pad->gamePressed & 4) {
        v->row++;
    }
    if (v->row < 0) {
        v->row = 1;
    }
    if (v->row >= 2) {
        v->row = 0;
    }
    if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) && !BtlMenu_IsActive()) {
        if (pad->gamePressed & 8) {
            gBtlReplayHudMode--;
        }
        if (pad->gamePressed & 4) {
            gBtlReplayHudMode++;
        }
        if (gBtlReplayHudMode >= 3) {
            gBtlReplayHudMode = 0;
        }
        if (gBtlReplayHudMode < 0) {
            gBtlReplayHudMode = 2;
        }
        Hud_SetReplayMode(gBtlReplayHudMode);
        v->row = 0;
    } else {
        v->row = 0;
    }
    if (pad->gamePressed & 1) {
        v->side--;
    }
    if (pad->gamePressed & 2) {
        v->side++;
    }
    if (v->row == 0) {
        if (v->side < 0) {
            v->side = 1;
        }
        if (v->side >= 2) {
            v->side = 0;
        }
    }
}

/* Side whose view the replay shows (0 while the viewer cursor is off its first row). */
s32 BtlReplay_GetViewSide(void) {
    BtlReplayViewer *v;

    if (gBtlChars == NULL) {
        return 0;
    }
    v = &gBtlChars->viewer;
    if (v->row == 0) {
        return v->side;
    }
    return 0;
}
