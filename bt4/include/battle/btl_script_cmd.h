#ifndef BATTLE_BTL_SCRIPT_CMD_H
#define BATTLE_BTL_SCRIPT_CMD_H

#include "types.h"

/*
 * Battle script command handlers: src/battle/btl_script_cmd.c = 0x259EC8..0x25C2A8 (46 functions).
 * They are the 46 entries of gBtlScriptCommands (0x2C6EC0), called by the GSC engine (sys/gsc.h) as
 * handler(phase, task work): phase 0 begin (4 when the task is aborting), 2 every frame until non-zero, 1 end.
 * A handler that returns 1 from the begin phase does not wait.
 *
 * Operand conventions (all verified by the matching code):
 *   who / side   an integer; bit 15 is the side (0x8000 = side 1). The low 12 bits are a team member index,
 *                read only by commands 12 and 1603; the fighter commands ignore them.
 *   angles       degrees in the script, converted to radians (x * pi / 180).
 *   seconds      script time: 1.0 = 30 frames.
 *   -x           a lettered option; its operands follow it.
 *
 * Command reference. "waits" says what ends the update phase; no entry = done in the begin phase.
 *
 *   id    handler        operands and options                      what it does
 *   1     Wait           seconds                                   waits until the task time reaches seconds * 300
 *   2     SetStory       n                                         BtlScript_SetStory(n)
 *   3     Nop3           -                                         nothing (first command of every event action)
 *   4     EndEvent       -                                         StopCharMove(0/1), ClearFixedCamera; skipped for an
 *                                                                  event of group != 0 that has a command 1201
 *   5     BeginScene     -                                         no running event (main action): fighters to their
 *                                                                  start places (0x2427A0), move type 0 looping. Event
 *                                                                  with a command 1201: the same after BeginInterrupt
 *                                                                  and BtlScene_Reset(2), (3). Other events: nothing
 *   6     EndScene       -                                         no running event: SetCtrlFF(0/1), ClearCtrl101(0/1),
 *                                                                  ShakeCamera(0). Event with a command 1201: the same
 *                                                                  plus EndInterrupt. Other events: nothing
 *   7     SetTriggers    -a trigger action ... -v trigger line ... BtlScript_ResetView, then AddEvent / AddLine(.., 1)
 *   8     Battle         what arg                                  BeginWait, sub-command (table below), ResetSubClock
 *   9     SetResult      n                                         n < 0: nothing. Else BtlFacade_SetResult(n); waits
 *                                                                  until BtlFacade_IsNotFinished() has held for 2 frames
 *   10    SetRule        stage bgm timeLimit announcer unk10       Battle_ClearWork, BattleSetup_SetRule(0, 1, ...)
 *   11    SetSide        side members                              BattleSetup_SetSide(side, side ? 2 : 0, 0, members..)
 *   12    SetMember      who chara costume variant cpuLevel item7  BattleSetup_SetMemberByItemIds; items kept in the
 *                        health item0..item6                       work. cpuLevel + 6 / + 9 by gProgress->0x3C
 *   13    SetStatus0     -C member -h n -H n -f n -F n -b n -B n   side 0: see "status options" below
 *                        -0..-6 item
 *   14    SetStatus1     the same, plus -l cpuLevel, -a item7      side 1
 *   15    SetSpeakers    -v line who ...                           BtlScript_SetSpeaker(line, who)
 *   16    SetRewards     15 integers                               stored at gProgress + 0x40
 *   701   LoadText       -                                         BtlScript_RequestTextFile(story + 0x231 / 0x263)
 *   702   LoadFile8      -                                         BtlScript_RequestFile8(story + 0x295 / 0x2C7)
 *   801   PlaceChar      who x y z rx ry rz                        SetCharPos, SetCharDir
 *   802   SetCharPos     who x y z                                 SetCharPos
 *   803   SetCharDir     who rx ry rz                              SetCharDir
 *   804   ClearCtrl100   who                                       BtlFacade_ClearCtrl100
 *   805   SetCtrl100     who                                       BtlFacade_SetCtrl100
 *   806   SetCtrlFE      who                                       BtlFacade_SetCtrlFE
 *   807   SetCtrlFF      who                                       BtlFacade_SetCtrlFF
 *   808   SetCtrl101     who                                       BtlFacade_SetCtrl101 (begin only, not on abort)
 *   809   ClearCtrl101   who                                       BtlFacade_ClearCtrl101
 *   810   SetCtrl102     who                                       BtlFacade_SetCtrl102
 *   901   MoveChar       who type [-t value] [-l] [-w]             ClearCtrl100, StartCharMove(side, type, value, -l).
 *                                                                  -w without -l waits until the move has played
 *                                                                  (IsCharMoveDone() == 0; the name is inverted)
 *   902   StopCharMoves  -                                         StopCharMove(0/1)
 *   1001  FadeIn         seconds [-W] [-w]                         Fade_Start(slot 0, or 1 with -W, FADE_IN); -w waits
 *                                                                  for Fade_IsDone + 1 frame, then Fade_Reset(slot)
 *   1002  FadeOut        seconds [-W] [-w]                         the same with FADE_OUT
 *   1003  ResetFades     -                                         Fade_Reset(0), Fade_Reset(1)
 *   1201  SetCamera      x y z rx ry rz                            stops the camera move, SetFixedCamera
 *   1202  MoveCamera     -l seconds x y z rx ry rz (1..8) [-w]     camera move through the keys, from the current
 *                                                                  pose; -w waits until the move is over. Abort: the
 *                                                                  move is stopped and its last key takes the pose
 *   1203  ClearCamera    -                                         ClearFixedCamera
 *   1204  ShakeCamera    seconds [-l]                              ShakeCamera; -l: kept up by BtlScript_UpdateView
 *   1205  StopShake      -                                         shake time 0, ShakeCamera(0)
 *   1301  Nop1301        -                                         nothing
 *   1302  SetWindow      -l lang -n window -d x y -a sx sy -s v    fills a text window; ignored unless lang == 1
 *                        -c r g b a -p a b -w v -T v -O a b
 *                        -C r g b a
 *   1501  PlayBgm        n                                         Bgm_Play(0x10B16 + n)
 *   1502  StopBgm        [-w]                                      Bgm_FadeOutStep; -w waits for Bgm_IsStopped
 *   1601  PlaySe         n                                         Snd_PlaySe(4, n) (begin only, not on abort)
 *   1602  PlayVoice      line [-h channel] [-w]                    Voice_PlayDefault(0, voice file of the line); -w
 *                                                                  waits for Voice_IsStopped(0). Abort: Voice_Stop
 *   1603  Talk           who line [-h channel] [-w]                text + voice + mouth movement; -w waits for
 *                                                                  Voice_IsStopped and 60 more frames. End: text off
 *   1701  WaitButton     bit                                       waits for gPad[0].gamePressed & (1 << bit), then
 *                                                                  one more frame
 *
 * Sub-commands of command 8 (`what & 0xFFF`; side = bit 15 of `what`):
 *   0, 13, 14, 15  nothing            1  CallSeqExtra            2  CallSeqExtra2        3  RequestStageChange
 *   4  ForceBoth110                   5, 6, 7  ForceBoth111      8  ForceActions(2, 2)   9  ForceActions(2, 3)
 *   10 ForceActions(3, 2)             11 ForceActions(3, 3)      12 ForceActions(4, 4)
 *   16..20  ChangeMember(side, 0..4)  21, 22  UseSkillA(side, arg)                       23 UseSkillB(side, arg)
 *   24 SetPowerUp(side, 1)            25 ForceAction01(side, 0)  26 ForceAction01(side, 1)
 *   27 ForceAction23(side, 0)         28 ForceAction23(side, 1)  29 ForceAction4(side)
 * BtlScript_ScanEvents (btl_script.c) reads the same operand: kind 1 = 16..20, kind 2 = 21..23.
 *
 * Status options of commands 13 / 14 (member = the fighting one, or -C):
 *   -h n   n > 0: RaiseHp(n), else LowerHp(-n)        -H n   AddHp(n)
 *   -f n   n > 0: RaiseGaugeC(n), else LowerGaugeC(-n) -F n   AddGaugeC(n)
 *   -b n   n > 0: RaiseGauge14(n), else LowerGauge14(-n)
 *   -B n   AddGaugeC(n): the code calls the +0xC gauge here, not BtlFacade_AddGauge14 (which has no caller)
 *   -0..-6 (and -a = slot 7, command 14 only) replace one item id of the member; BtlFacade_SetMemberItems is
 *          always called at the end, options or not
 *   -l n   command 14 only: BtlFacade_SetCpuParam8(n + 0 / 6 / 9 by gProgress->0x3C)
 */

/* A text window as command 1302 fills it (the same 0x50 bytes btl_script.h calls BtlScriptWindow). */
typedef struct BtlScriptCmdWindow {
    /* 0x00 */ f32 scaleX;  /* -a */
    /* 0x04 */ f32 scaleY;
    /* 0x08 */ f32 unk8;    /* -s */
    /* 0x0C */ u32 color;   /* -c: r | g << 8 | b << 16 | a << 24 */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ s32 unk14;   /* -p, first */
    /* 0x18 */ s32 unk18;   /* -p, second */
    /* 0x1C */ s32 unk1C;   /* -w */
    /* 0x20 */ s32 unk20;
    /* 0x24 */ s32 unk24;   /* -T */
    /* 0x28 */ u32 color28; /* -C */
    /* 0x2C */ s16 unk2C;   /* -O, first */
    /* 0x2E */ s16 unk2E;   /* -O, second */
    /* 0x30 */ u8 unk30[0x18];
    /* 0x48 */ s32 x;       /* -d */
    /* 0x4C */ s32 y;
} BtlScriptCmdWindow; /* size 0x50 */

/* Item ids of one team member: gBtlScript + 0xC + (side * 5 + member) * 0x20. */
typedef struct BtlScriptCmdItems {
    /* 0x00 */ s32 id[8];
} BtlScriptCmdItems; /* size 0x20 */

/* The start of the module work (gBtlScript, BtlScriptWork in btl_script.h) as the handlers use it: what
   btl_script.h calls unkC[0x140] is the item table. Local view. */
typedef struct BtlScriptCmdWork {
    /* 0x00 */ s32 story;
    /* 0x04 */ void *textFile;
    /* 0x08 */ void *file8;
    /* 0x0C */ BtlScriptCmdItems items[2][5]; /* [side][member] */
} BtlScriptCmdWork;

/* Task work of commands 1001 / 1002. */
typedef struct BtlScriptCmdFade {
    /* 0x00 */ s32 slot;
    /* 0x04 */ s32 frames; /* frames seen with the fade done */
    /* 0x08 */ s32 wait;   /* option -w */
} BtlScriptCmdFade;

/* Task work of command 1603. */
typedef struct BtlScriptCmdTalk {
    /* 0x00 */ s32 channel;    /* voice channel, option -h */
    /* 0x04 */ s32 line;
    /* 0x08 */ s32 timer;      /* 600, -10 per frame once the voice has stopped */
    /* 0x0C */ s32 side;
    /* 0x10 */ void *lip;      /* the line's entry of the second story file, NULL when the speaker is not fighting */
    /* 0x14 */ s32 lipStarted;
} BtlScriptCmdTalk;

/* Task work of command 1701. */
typedef struct BtlScriptCmdButton {
    /* 0x00 */ s32 pressed;
    /* 0x04 */ s32 button; /* bit number in Pad.gamePressed */
} BtlScriptCmdButton;

/* What command 16 fills: gProgress + 0x40. */
typedef struct BtlScriptCmdReward {
    /* 0x00 */ s32 unk0[3];  /* operands 0..2 */
    /* 0x0C */ s32 unkC[3];  /* operands 3..5 */
    /* 0x18 */ s32 unk18[3]; /* operands 9..11 */
    /* 0x24 */ s32 unk24[3]; /* operands 6..8 */
    /* 0x30 */ s32 unk30[3]; /* operands 12..14 */
} BtlScriptCmdReward; /* size 0x3C */

/* The part of the progress struct (gProgress) the handlers touch; local view. */
typedef struct BtlScriptCmdProgress {
    /* 0x00 */ s32 unk0;       /* 0 / 1: picks the text file set (language) */
    /* 0x04 */ u8 unk4[0x38];
    /* 0x3C */ s32 difficulty; /* 1: CPU level + 6, 2: + 9 */
    /* 0x40 */ BtlScriptCmdReward reward;
} BtlScriptCmdProgress;

s32 BtlScriptCmd_SetCamera(u32 phase, void *taskWork);
s32 BtlScriptCmd_ClearCamera(u32 phase, void *taskWork);
s32 BtlScriptCmd_ShakeCamera(u32 phase, void *taskWork);
s32 BtlScriptCmd_StopShake(u32 phase, void *taskWork);
s32 BtlScriptCmd_MoveCamera(u32 phase, void *taskWork);
s32 BtlScriptCmd_PlaceChar(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetCharPos(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetCharDir(u32 phase, void *taskWork);
s32 BtlScriptCmd_ClearCtrl100(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetCtrl100(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetCtrlFE(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetCtrlFF(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetCtrl101(u32 phase, void *taskWork);
s32 BtlScriptCmd_ClearCtrl101(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetCtrl102(u32 phase, void *taskWork);
s32 BtlScriptCmd_MoveChar(u32 phase, void *taskWork);
s32 BtlScriptCmd_StopCharMoves(u32 phase, void *taskWork);
s32 BtlScriptCmd_FadeIn(u32 phase, void *taskWork);
s32 BtlScriptCmd_FadeOut(u32 phase, void *taskWork);
s32 BtlScriptCmd_ResetFades(u32 phase, void *taskWork);
s32 BtlScriptCmd_Nop1301(u32 phase, void *taskWork);
s32 BtlScriptCmd_LoadText(u32 phase, void *taskWork);
s32 BtlScriptCmd_LoadFile8(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetWindow(u32 phase, void *taskWork);
s32 BtlScriptCmd_PlayBgm(u32 phase, void *taskWork);
s32 BtlScriptCmd_StopBgm(u32 phase, void *taskWork);
s32 BtlScriptCmd_PlaySe(u32 phase, void *taskWork);
s32 BtlScriptCmd_PlayVoice(u32 phase, void *taskWork);
s32 BtlScriptCmd_Talk(u32 phase, void *taskWork);
s32 BtlScriptCmd_Wait(u32 phase, void *taskWork);
s32 BtlScriptCmd_WaitButton(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetStory(u32 phase, void *taskWork);
s32 BtlScriptCmd_Nop3(u32 phase, void *taskWork);
s32 BtlScriptCmd_EndEvent(u32 phase, void *taskWork);
s32 BtlScriptCmd_BeginScene(u32 phase, void *taskWork);
s32 BtlScriptCmd_EndScene(u32 phase, void *taskWork);
s32 BtlScriptCmd_Battle(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetResult(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetTriggers(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetRule(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetSide(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetMember(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetStatus0(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetStatus1(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetSpeakers(u32 phase, void *taskWork);
s32 BtlScriptCmd_SetRewards(u32 phase, void *taskWork);

#endif
