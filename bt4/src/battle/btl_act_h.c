#include "common.h"
#include "battle/btl_act_h.h"

/*
 * Fighter action handlers: 0x1FC2B0..0x1FFAC0, in two C files because the one unmatched handler (BtlAct_GrabDash)
 * owns two constants in the middle of the float pool:
 *   src/battle/btl_act_h.c    0x1FC2B0..0x1FC598  release heading, grab dash
 *   src/battle/btl_act_h_b.c  0x1FC598..0x1FFAC0  everything else
 *
 * Throws (0xB4..0xBC), the carry pair (0xBE / 0xBF) and the character-change actions: transformation (0xEC..0xF0),
 * fusion (0xF1 / 0xF2) and the first action of a member switch (0xF3; 0xF4..0xF8 are in btl_act_i.c).
 *
 * A handler is `s32 handler(chr, phase)`: 0 enter, 1 run, 2 decide, 3 leave (btl_char_action.h).
 *
 * How a character change runs (all of 0xEC..0xF3 follow it):
 *   - enter: animation, camera cut, BtlChar_SavePlacement; every run frame raises the flags of
 *     BtlActChange_SetFlags (0x125 = hit-stop level 2: the opponent is frozen).
 *   - the run phase with actionFrame == 1 (the second frame of the action) pushes the request
 *     (BtlChange_RequestChara, for 0xEF / 0xF1 / 0xF2 a BtlChange_RequestObject in front of it). BtlChange_Update at
 *     the end of that frame makes it active and sets the time-stop word; actionFrame stops at 2 from then on.
 *   - the fighter plays its fixed lead-in and then loops, testing BtlChange_IsLoadedFor(player) once per frame.
 *   - on the first frame it is true (and the lead-in is over): BtlAnim_Request(next animation),
 *     BtlChange_SetReady(player) and, for the character request, BtlMember_SubBlast(cost).
 *   - the loader (Job_Run, start of the next frame) swaps the model and calls BtlChars_OnModelLoaded, which applies
 *     the requested animation; the handler sees the new animation (BtlAnim_IsNew) in that frame's run phase and
 *     calls BtlChange_SetDone(player). BtlChange_Update retires the request at the end of that frame: the time-stop
 *     word is clear again from the frame after, unless another request is queued.
 *   - the closing animation plays to its end, work[1] = 1, and the decide phase leaves through BtlActChange_Finish.
 */

/* Thrown fighter: sets the heading of the release from the hit reaction (mirrored when grabbed from behind). */
void BtlActThrow_SetReleaseHeading(BtlActHChr *chr) {
    BtlActHPose *pose = BtlChar_GetPos(chr);
    f32 yaw;
    f32 pitch;

    if (chr->react.back != 0) {
        yaw = BtlUtil_WrapAngle(BtlUtil_WrapAngle(pose->facing + chr->react.launchA) + BTL_DEG(180.0f));
        pitch = chr->react.launchB;
    } else {
        yaw = BtlUtil_WrapAngle(pose->facing + chr->react.launchA);
        pitch = -chr->react.launchB;
    }
    BtlMove_SetHeading(chr, yaw, pitch * Mathf_Cos(pose->rootRot.y));
}

/*
 * 0xB4, 0xB5, 0xB6: dash at the opponent to grab (animation 0x94 -> 0x95; 0xB6: 0x186 -> 0x187 at double rate).
 * Flag 0x7A (missed) plays 0x9D and ends in action 0xB; flag 0x5B (caught) starts the throw: 0xB4 / 0xB5 -> 0xB7,
 * or 0xBB when the character parameter flag 0x200 is set; 0xB6 -> 0xB9.
 * The yaw mode switch needs its redundant cases (0xB4 and 0xB6 assign the 6 the variable already has, behind an
 * empty default): with them the compiler keeps the branch; reduced to `case 0xB5` alone it makes a conditional move.
 */
s32 BtlAct_GrabDash(BtlActHChr *chr, s32 phase) {
    s32 yawMode;

    if (phase == 0) {
        BtlAnim_Play(chr, BtlAct_GetCurrent(chr) == 0xB6 ? 0x186 : 0x94, 0.15f);
        BtlMove_BeginRiseToOpponent(chr);
    }
    if (phase == 1) {
        yawMode = 6;
        switch (BtlAct_GetCurrent(chr)) {
        case 0xB5:
            yawMode = 2;
            break;
        default:
            break;
        case 0xB4:
            yawMode = 6;
            break;
        case 0xB6:
            yawMode = 6;
            break;
        }
        switch (BtlAnim_GetId(chr)) {
        case 0x94:
            BtlAnim_AdvanceThen(chr, 0x95, 0.0f, 0);
            break;
        case 0x186:
            BtlAnim_AdvanceThen(chr, 0x187, 0.0f, 0);
            break;
        case 0x95:
        case 0x9D:
        case 0x187:
            if (BtlAnim_Advance(chr, 0)) {
                BtlAct_Request(chr, 0xB);
            }
            break;
        }
        BtlMove_Step(chr, yawMode, 5, 2, 0.0f, BTL_KMH(100.0f));
        BtlMove_ApplyGravity(chr);
        BtlChar_SetFlag(chr, 0xD5);
        BtlChar_SetFlag(chr, 0x92);
        if (BtlAct_GetCurrent(chr) == 0xB6) {
            BtlAnim_SetObjRate(chr, 2.0f);
        }
    }
    if (phase == 2) {
        if (BtlChar_TestFlag(chr, 0x7A)) {
            BtlAnim_Request(chr, 0x9D, 0.0f);
        }
        if (BtlChar_TestFlag(chr, 0x5B)) {
            switch (BtlAct_GetCurrent(chr)) {
            case 0xB4:
            case 0xB5:
                if (BtlParam_GetFlags(chr) & 0x200) {
                    BtlAct_Request(chr, 0xBB);
                } else {
                    BtlAct_Request(chr, 0xB7);
                }
                break;
            case 0xB6:
                BtlAct_Request(chr, 0xB9);
                break;
            }
        }
    }
}
