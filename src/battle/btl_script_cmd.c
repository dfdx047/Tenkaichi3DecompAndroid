#include "common.h"
#include "battle/btl_script_cmd.h"
#include "battle/btl_script.h"
#include "sys/gsc.h"

#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "battle/btl_facade.h"
#include "battle/btl_scene.h"
#include "sys/adx.h"
#include "sys/fade.h"
#include "sys/pad.h"
#include "sys/save.h"
#include "sys/snd.h"

/*
 * Battle script command handlers: 0x259EC8..0x25C2A8, the 46 handlers of gBtlScriptCommands.
 * The command reference (ids, operands, options, waits) is in btl_script_cmd.h.
 *
 * The object starts here: its jump tables are the first data of the rodata file at 0x2F2F20. The functions
 * are in the order of the original source, which is not the order of the command ids: camera (1201, 1203,
 * 1204, 1205, 1202), fighter (801..810, 901, 902), fade (1001..1003), 1301, files (701, 702), 1302, sound
 * (1501, 1502, 1601..1603), then the core commands 1, 1701, 2..6, 8, 9, 7, 10..16.
 *
 * The phase parameter is unsigned in the original (an enum, most likely): the handlers that switch over it
 * compile to an unsigned compare tree, which `s32 phase` does not give. sys/gsc.h declares GscHandler with
 * `s32 phase`; the call is the same either way.
 */

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);

/* btl_facade.h declares (side, type, mode, value); the call here sets up the float before the mode, which is
   the order (side, type, value, mode). Same registers either way. */
#ifdef PORT /* PC build: the definition's parameter order */
extern void BtlFacade_PlayCharMotion(s32 side, s32 type, s32 mode, f32 value);
#define BtlFacade_StartCharMoveF(side, type, value, mode) BtlFacade_PlayCharMotion(side, type, mode, value)
#else
extern void BtlFacade_StartCharMoveF(s32 side, s32 type, f32 value, s32 mode) __asm__("BtlFacade_PlayCharMotion");
#endif

/* the text module's default window (0x23AC50, not decompiled) */
extern BtlScriptCmdWindow *Font_GetStyle(void);
/* start position and direction of a side (0x2427A0, stage code, not decompiled) */
extern void BtlStage_GetStartPlace(s32 side, Vec4 *pos, Vec4 *rot, s32 arg);
/* called with a second argument (0) that BtlFacade_SetCpuParam8 does not take */
extern void BtlFacade_SetCpuParam8Ex(s32 value, s32 unused) __asm__("BtlFacade_SetCpuParam8");

extern BtlScriptCmdProgress *gProgress;
extern BtlScriptWork gBtlScript;

#define SCRIPT_SIDE(v) (((v) >> 15) & 1)
/* Steps a camera pointer by one key, so that p->key[1] is the next key. */
#define CAM_NEXT(p) ((BtlScriptCam *)((u8 *)(p) + sizeof(BtlScriptCamKey)))
#define SCRIPT_RGBA(r, g, b, a) (((r) & 0xFF) | (((g) & 0xFF) << 8) | (((b) & 0xFF) << 16) | ((a) << 24))
#define DEG2RAD(d) ((d) * 3.14159265f / 180.0f)

/* Command 1201 "camera": x y z rx ry rz (degrees). Stops the camera move and fixes the camera at that pose. */
s32 BtlScriptCmd_SetCamera(u32 phase, void *taskWork) {
    Vec4 pos;
    Vec4 rot;
    BtlScriptCam *cam = &gBtlScript.cam;
    s32 ret = 0;

    switch (phase) {
    case GSC_PHASE_BEGIN:
    case GSC_PHASE_BEGIN_ABORT:
        cam->active = 0;
        cam->time = 0;
        cam->index = 0;
        pos.x = Gsc_GetFloat();
        pos.y = Gsc_GetFloat();
        pos.z = Gsc_GetFloat();
        rot.x = DEG2RAD(Gsc_GetFloat());
        rot.y = DEG2RAD(Gsc_GetFloat());
        rot.z = DEG2RAD(Gsc_GetFloat());
        BtlFacade_SetFixedCamera(&pos, &rot);
        ret = 1;
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_UPDATE:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1203 "camera off": gives the camera back to the battle. */
s32 BtlScriptCmd_ClearCamera(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_ClearFixedCamera();
    }
    return 1;
}

/* Command 1204 "shake": seconds [-l]. Shakes the camera; with -l the shake is kept up until command 1205. */
s32 BtlScriptCmd_ShakeCamera(u32 phase, void *taskWork) {
    s32 ret = 0;
    f32 time;

    switch (phase) {
    case GSC_PHASE_BEGIN:
        time = Gsc_GetFloat();
        BtlFacade_ShakeCamera(time);
        if (Gsc_FindOption('l')) {
            gBtlScript.shakeTime = time;
        }
        ret = 1;
        break;
    case GSC_PHASE_BEGIN_ABORT:
        ret = 1;
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_UPDATE:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1205 "shake off": ends the kept-up shake. */
s32 BtlScriptCmd_StopShake(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlScriptWork *work = &gBtlScript;

        work->shakeTime = 0.0f;
        BtlFacade_ShakeCamera(work->shakeTime);
    }
    return 1;
}

/* Command 1202 "camera move": -l seconds x y z rx ry rz (up to 8 times) [-w]. Starts a move through the keys
   from the current pose; with -w the command waits until the move is over. */
s32 BtlScriptCmd_MoveCamera(u32 phase, void *taskWork) {
    Vec4 pos;
    Vec4 rot;
    Vec4 curPos;
    Vec4 curRot;
    BtlScriptCam *cam = &gBtlScript.cam;
    s32 ret = 0;
    s32 i;
    BtlScriptCam *p;
    BtlScriptCam *last;
    f32 time;

    switch (phase) {
    case GSC_PHASE_BEGIN:
        cam->active = 0;
        cam->time = 0;
        cam->index = 0;
        for (i = 8; i >= 0; i--) {
            cam->key[i].flags = 0;
        }
        if (Gsc_FindOption('l')) {
            BtlFacade_GetFixedCamera(&curPos, &curRot);
            Vec4_Copy(&cam->key[0].pos, &curPos);
            Vec4_Copy(&cam->key[0].rot, &curRot);
            cam->key[0].time = 0.0f;
            cam->key[0].flags = 1;
            p = cam;
            for (i = 0; Gsc_FindOptionN('l', i) && i < 8;) {
                i++;
                time = Gsc_GetFloat();
                pos.x = Gsc_GetFloat();
                pos.y = Gsc_GetFloat();
                pos.z = Gsc_GetFloat();
                rot.x = DEG2RAD(Gsc_GetFloat());
                rot.y = DEG2RAD(Gsc_GetFloat());
                rot.z = DEG2RAD(Gsc_GetFloat());
                Vec4_Copy(&p->key[1].pos, &pos);
                Vec4_Copy(&p->key[1].rot, &rot);
                p->key[1].flags = 1;
                p->key[1].time = time;
                p = CAM_NEXT(p);
            }
            cam->active = 1;
        }
        if (Gsc_FindOption('w') == 0) {
            ret = 1;
        }
        break;
    case GSC_PHASE_UPDATE:
        if (cam->active == 0) {
            ret = 1;
        }
        break;
    case GSC_PHASE_BEGIN_ABORT:
        cam->active = 0;
        for (last = cam;; last = CAM_NEXT(last)) {
            if (last->key[1].flags == 0) {
                BtlFacade_GetFixedCamera(&last->key[0].pos, &last->key[0].rot);
                break;
            }
        }
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 801 "place": side x y z rx ry rz (degrees). Puts a fighter at a position, facing a direction. */
s32 BtlScriptCmd_PlaceChar(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        Vec4 pos;
        Vec4 rot;
        s32 side = SCRIPT_SIDE(Gsc_GetInt());

        pos.x = Gsc_GetFloat();
        pos.y = Gsc_GetFloat();
        pos.z = Gsc_GetFloat();
        pos.w = 1.0f;
        rot.x = DEG2RAD(Gsc_GetFloat());
        rot.y = DEG2RAD(Gsc_GetFloat());
        rot.z = DEG2RAD(Gsc_GetFloat());
        rot.w = 1.0f;
        BtlFacade_SetCharPos(side, &pos);
        BtlFacade_SetCharRot(side, &rot);
    }
    return 1;
}

/* Command 802: side x y z. Position only. */
s32 BtlScriptCmd_SetCharPos(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        Vec4 pos;
        s32 side = SCRIPT_SIDE(Gsc_GetInt());

        pos.x = Gsc_GetFloat();
        pos.y = Gsc_GetFloat();
        pos.z = Gsc_GetFloat();
        pos.w = 1.0f;
        BtlFacade_SetCharPos(side, &pos);
    }
    return 1;
}

/* Command 803: side rx ry rz (degrees). Direction only. */
s32 BtlScriptCmd_SetCharDir(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        Vec4 rot;
        s32 side = SCRIPT_SIDE(Gsc_GetInt());

        rot.x = DEG2RAD(Gsc_GetFloat());
        rot.y = DEG2RAD(Gsc_GetFloat());
        rot.z = DEG2RAD(Gsc_GetFloat());
        rot.w = 1.0f;
        BtlFacade_SetCharRot(side, &rot);
    }
    return 1;
}

/* Command 804: side. Clears control flag 0x100. */
s32 BtlScriptCmd_ClearCtrl100(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_ClearCtrl100(SCRIPT_SIDE(Gsc_GetInt()));
    }
    return 1;
}

/* Command 805: side. Sets control flag 0x100. */
s32 BtlScriptCmd_SetCtrl100(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_SetCtrl100(SCRIPT_SIDE(Gsc_GetInt()));
    }
    return 1;
}

/* Command 806: side. Sets control flag 0xFE. */
s32 BtlScriptCmd_SetCtrlFE(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_SetCtrlFE(SCRIPT_SIDE(Gsc_GetInt()));
    }
    return 1;
}

/* Command 807: side. Sets control flag 0xFF. */
s32 BtlScriptCmd_SetCtrlFF(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_SetCtrlFF(SCRIPT_SIDE(Gsc_GetInt()));
    }
    return 1;
}

/* Command 808: side. Sets control flag 0x101 (not when the task is aborting). */
s32 BtlScriptCmd_SetCtrl101(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN) {
        BtlFacade_SetCtrl101(SCRIPT_SIDE(Gsc_GetInt()));
    }
    return 1;
}

/* Command 809: side. Clears control flag 0x101. */
s32 BtlScriptCmd_ClearCtrl101(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_ClearCtrl101(SCRIPT_SIDE(Gsc_GetInt()));
    }
    return 1;
}

/* Command 810: side. Sets control flag 0x102. */
s32 BtlScriptCmd_SetCtrl102(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_SetCtrl102(SCRIPT_SIDE(Gsc_GetInt()));
    }
    return 1;
}

/* Command 901 "move": who type [-t value] [-l] [-w]. Starts scripted move `type` (-l: looping). It waits only
   when -w is given without -l, and then until BtlFacade_IsCharMotionPlaying() returns 0: despite its name that
   function returns 1 while a one-shot move is still playing (fighter flag 0x31 clear) and always 1 for a looping
   move, so the wait ends when the move has played to its end. */
s32 BtlScriptCmd_MoveChar(u32 phase, void *taskWork) {
    s32 *work = taskWork;
    s32 ret = 0;
    s32 side;
    s32 type;
    s32 loop;
    s32 done;
    f32 value;

    switch (phase) {
    case GSC_PHASE_BEGIN:
    case GSC_PHASE_BEGIN_ABORT:
        side = SCRIPT_SIDE(Gsc_GetInt());
        value = 0.0f;
        type = Gsc_GetInt();
        if (Gsc_FindOption('t')) {
            value = Gsc_GetFloat();
        }
        done = 1;
        loop = Gsc_FindOption('l') != 0;
        if (Gsc_FindOption('w')) {
            ret = loop;
        } else {
            ret = done;
        }
        BtlFacade_ClearCtrl100(side);
        work[0] = side;
        BtlFacade_StartCharMoveF(side, type, value, loop);
        if (phase == GSC_PHASE_BEGIN_ABORT) {
            ret = 1;
        }
        break;
    case GSC_PHASE_UPDATE:
        ret = BtlFacade_IsCharMotionPlaying(work[0]) == 0;
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 902 "stop moves": ends the scripted move of both fighters. */
s32 BtlScriptCmd_StopCharMoves(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlFacade_StopCharMotion(0);
        BtlFacade_StopCharMotion(1);
    }
    return 1;
}

/* Command 1001 "fade in": seconds [-W] [-w]. Fades in slot 0 (slot 1 with -W); -w waits for the fade, and the
   slot is then released when the command ends. */
s32 BtlScriptCmd_FadeIn(u32 phase, void *taskWork) {
    BtlScriptCmdFade *work = taskWork;
    s32 ret = 0;
    f32 time;

    switch (phase) {
    case GSC_PHASE_BEGIN:
    case GSC_PHASE_BEGIN_ABORT:
        time = Gsc_GetFloat();
        if (Gsc_FindOption('W') == 0) {
            work->slot = 0;
        } else {
            work->slot = 1;
        }
        Fade_Start(work->slot, FADE_IN, time);
        work->frames = 0;
        work->wait = Gsc_FindOption('w');
        if (work->wait == 0) {
            ret = 1;
        }
        break;
    case GSC_PHASE_UPDATE:
        if (Fade_IsDone(work->slot)) {
            ret = work->frames > 0;
            work->frames++;
        }
        break;
    case GSC_PHASE_END:
        if (work->wait != 0) {
            Fade_Reset(work->slot);
        }
        break;
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1002 "fade out": seconds [-W] [-w]. As 1001, the other way. */
s32 BtlScriptCmd_FadeOut(u32 phase, void *taskWork) {
    BtlScriptCmdFade *work = taskWork;
    s32 ret = 0;
    f32 time;

    switch (phase) {
    case GSC_PHASE_BEGIN:
    case GSC_PHASE_BEGIN_ABORT:
        time = Gsc_GetFloat();
        if (Gsc_FindOption('W') == 0) {
            work->slot = 0;
        } else {
            work->slot = 1;
        }
        Fade_Start(work->slot, FADE_OUT, time);
        work->frames = 0;
        work->wait = Gsc_FindOption('w');
        if (work->wait == 0) {
            ret = 1;
        }
        break;
    case GSC_PHASE_UPDATE:
        if (Fade_IsDone(work->slot)) {
            ret = work->frames > 0;
            work->frames++;
        }
        break;
    case GSC_PHASE_END:
        if (work->wait != 0) {
            Fade_Reset(work->slot);
        }
        break;
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1003 "fade off": releases fade slots 0 and 1. */
s32 BtlScriptCmd_ResetFades(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        Fade_Reset(0);
        Fade_Reset(1);
    }
    return 1;
}

/* Command 1301: does nothing. */
s32 BtlScriptCmd_Nop1301(u32 phase, void *taskWork) {
    return 1;
}

/* Command 701 "load text": requests the story's text file (by the language in gProgress). */
s32 BtlScriptCmd_LoadText(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 base;

        base = -1;
        switch (gProgress->unk0) {
        case 0:
            base = DISC_RESOURCE(0x231, 0x26F);
            break;
        case 1:
            base = DISC_RESOURCE(0x263, 0x26F);
            break;
        }
        BtlScript_RequestTextFile(base + BtlScript_GetStory());
    }
    return 1;
}

/* Command 702 "load lip data": requests the story's second file (by the save's voice set). */
s32 BtlScriptCmd_LoadFile8(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 base;

        if (gSaveData->flags & SAVE_FLAG_VOICE) {
            base = DISC_RESOURCE(0x2C7, 0x34E);
        } else {
            base = DISC_RESOURCE(0x295, 0x2DE);
        }
        BtlScript_RequestFile8(base + BtlScript_GetStory());
    }
    return 1;
}

/* Command 1302 "window": -l language -n window -d x y -a sx sy -s v -c r g b a -p a b -w v -T v -O a b
   -C r g b a. Sets up a text window from the default one; only the entry for language 1 is used. */
s32 BtlScriptCmd_SetWindow(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlScriptCmdWindow *win;
        u8 r;
        u8 g;
        u8 b;
        s32 a;

        Gsc_FindOption('l');
        if (Gsc_GetInt() == 1) {
            Gsc_FindOption('n');
            win = (BtlScriptCmdWindow *)BtlScript_GetWindow(Gsc_GetIntOr(0));
            *win = *Font_GetStyle();
            Gsc_FindOption('d');
            win->x = Gsc_GetIntOr(0);
            win->y = Gsc_GetIntOr(0);
            Gsc_FindOption('a');
            win->scaleX = Gsc_GetFloatOr(1.0f);
            win->scaleY = Gsc_GetFloatOr(1.0f);
            Gsc_FindOption('s');
            win->unk8 = Gsc_GetFloatOr(1.0f);
            Gsc_FindOption('c');
            r = Gsc_GetIntOr(0xFF);
            g = Gsc_GetIntOr(0xFF);
            b = Gsc_GetIntOr(0xFF);
            a = Gsc_GetIntOr(0x80);
            win->color = r | (a << 24) | ((b << 16) | (g << 8));
            Gsc_FindOption('p');
            win->unk14 = Gsc_GetIntOr(2);
            win->unk18 = Gsc_GetIntOr(5);
            Gsc_FindOption('w');
            win->unk1C = Gsc_GetIntOr(0);
            Gsc_FindOption('T');
            win->unk24 = Gsc_GetIntOr(2);
            Gsc_FindOption('O');
            win->unk2C = Gsc_GetIntOr(1);
            win->unk2E = Gsc_GetIntOr(2);
            Gsc_FindOption('C');
            r = Gsc_GetIntOr(0xFF);
            g = Gsc_GetIntOr(0xFF);
            b = Gsc_GetIntOr(0xFF);
            a = Gsc_GetIntOr(0x80);
            win->color28 = r | (a << 24) | ((b << 16) | (g << 8));
        }
    }
    return 1;
}

/* Command 1501 "music": n. Plays stream 0x10B16 + n as the battle music. */
s32 BtlScriptCmd_PlayBgm(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        Bgm_Play(Gsc_GetInt() + 0x10B16);
    }
    return 1;
}

/* Command 1502 "music off": [-w]. Starts the music fade-out; -w waits until the stream has stopped. */
s32 BtlScriptCmd_StopBgm(u32 phase, void *taskWork) {
    s32 ret = 0;

    switch (phase) {
    case GSC_PHASE_BEGIN:
    case GSC_PHASE_BEGIN_ABORT:
        Bgm_FadeOutStep();
        if (Gsc_FindOption('w') == 0) {
            ret = 1;
        }
        break;
    case GSC_PHASE_UPDATE:
        ret = Bgm_IsStopped() != 0;
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1601 "sound": n. Plays sound effect n of bank mask 4 (not when the task is aborting). */
s32 BtlScriptCmd_PlaySe(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN) {
        Snd_PlaySe(4, Gsc_GetInt());
    }
    return 1;
}

/* Command 1602 "voice": line [-h channel] [-w]. Plays a line's voice file on channel 0 without text; -w
   waits until the voice has stopped. The -h value is read and then overwritten with 0. */
s32 BtlScriptCmd_PlayVoice(u32 phase, void *taskWork) {
    s32 *work = taskWork;
    s32 ret = 0;
    s32 line;

    switch (phase) {
    case GSC_PHASE_BEGIN:
        line = Gsc_GetInt();
        if (Gsc_FindOption('h')) {
            work[0] = Gsc_GetInt();
        }
        ret = Gsc_FindOption('w') == 0;
        work[0] = 0;
        Voice_PlayDefault(0, BtlScript_GetVoiceFile(line));
        break;
    case GSC_PHASE_UPDATE:
        if (Voice_IsStopped(work[0])) {
            ret = 1;
        }
        break;
    case GSC_PHASE_BEGIN_ABORT:
        if (Gsc_IsCurrentBegun()) {
            Voice_Stop(work[0]);
        }
        ret = 1;
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1603 "talk": who line [-h channel] [-w]. A fighter says a line: voice, text (window 0 when the
   speaker is the fighting member, else window 1) and, for the fighting member, mouth movement. -w waits until
   the voice has stopped and 60 more frames. */
s32 BtlScriptCmd_Talk(u32 phase, void *taskWork) {
    BtlScriptCmdTalk *work = taskWork;
    s32 who;
    s32 side;
    s32 member;
    s32 line;
    s32 channel;
    s32 ret = 0;

    switch (phase) {
    case GSC_PHASE_BEGIN:
        channel = 0;
        who = Gsc_GetInt();
        member = who & 0xFFF;
        side = SCRIPT_SIDE(who);
        line = Gsc_GetInt();
        if (Gsc_FindOption('h')) {
            channel = Gsc_GetInt();
        }
        ret = Gsc_FindOption('w') == 0;
        if (BtlFacade_GetActiveMember(side) == member) {
            BtlScript_SetText(0, line);
        } else {
            BtlScript_SetText(1, line);
        }
        work->channel = channel;
        work->timer = 600;
        work->line = line;
        work->side = side;
        Voice_PlayDefault(channel, BtlScript_GetVoiceFile(line));
        work->lip = NULL;
        work->lipStarted = 0;
        if (BtlFacade_GetActiveMember(side) == member) {
            work->lip = BtlScript_GetFile8Entry(line);
        }
        break;
    case GSC_PHASE_UPDATE:
        if (work->lip != NULL && work->lipStarted == 0 && Adx_GetStat(work->channel + 4) == 3) {
            BtlFacade_SetObjSubState14(work->side, (s32)work->lip);
            work->lipStarted = 1;
        }
        if (Voice_IsStopped(work->channel)) {
            BtlFacade_EndObjSubState3(work->side);
            work->timer -= 10;
            if (work->timer <= 0) {
                ret = 1;
            }
        }
        break;
    case GSC_PHASE_END:
        BtlScript_ClearText(work->line);
        break;
    case GSC_PHASE_BEGIN_ABORT:
        if (Gsc_IsCurrentBegun()) {
            Voice_Stop(work->channel);
            BtlFacade_EndObjSubState3(work->side);
        }
        ret = 1;
        break;
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1 "wait": seconds. Done when the task has waited that long (script time 300 per second). */
s32 BtlScriptCmd_Wait(u32 phase, void *taskWork) {
    s32 ret = 0;

    switch (phase) {
    case GSC_PHASE_UPDATE:
        if (Gsc_GetCurrentTime() < (s32)(Gsc_GetFloat() * 300.0f)) {
            break;
        }
    case GSC_PHASE_BEGIN_ABORT:
        ret = 1;
        break;
    case GSC_PHASE_BEGIN:
    case GSC_PHASE_END:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 1701 "wait button": bit. Waits until pad 0 reports that button as pressed, plus one frame. */
s32 BtlScriptCmd_WaitButton(u32 phase, void *taskWork) {
    BtlScriptCmdButton *work = taskWork;
    s32 ret = 0;

    switch (phase) {
    case GSC_PHASE_BEGIN:
        work->button = Gsc_GetInt();
        work->pressed = 0;
        break;
    case GSC_PHASE_UPDATE:
        if (work->pressed != 0) {
            ret = 1;
        } else if (gPad[0].gamePressed & (1 << work->button)) {
            work->pressed = 1;
        }
        break;
    case GSC_PHASE_BEGIN_ABORT:
        ret = 1;
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 2 "story": n. Sets the story number that picks the text, lip and voice files. */
s32 BtlScriptCmd_SetStory(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        BtlScript_SetStory(Gsc_GetInt());
    }
    return 1;
}

/* Command 3: does nothing (first command of every event action). */
s32 BtlScriptCmd_Nop3(u32 phase, void *taskWork) {
    return 1;
}

/* Command 4 "event end": ends the scripted moves and the fixed camera, unless the event is a mid-fight one
   (group != 0) that has a camera command. */
s32 BtlScriptCmd_EndEvent(u32 phase, void *taskWork) {
    BtlScriptEvent *event = gBtlScript.cur.event;

    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        if (event == NULL || event->group == 0 || !event->cmd4B1) {
            BtlFacade_StopCharMotion(0);
            BtlFacade_StopCharMotion(1);
            BtlFacade_ClearFixedCamera();
        }
    }
    return 1;
}

/* Command 5 "scene begin": puts both fighters at their start places, standing (move type 0, looping), when no
   event is running (the main action) or when the running event has a camera command; in the second case the
   fight is interrupted and both fighters' effects are reset first. Any other event: nothing. */
s32 BtlScriptCmd_BeginScene(u32 phase, void *taskWork) {
    Vec4 pos0;
    Vec4 pos1;
    Vec4 rot0;
    Vec4 rot1;
    BtlScriptWork *work = &gBtlScript;

    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        if (work->cur.event != NULL && work->cur.event->cmd4B1) {
            BtlFacade_BeginInterrupt();
            BtlScene_Reset(BTL_SCENE_RESET_CHAR0);
            BtlScene_Reset(BTL_SCENE_RESET_CHAR1);
            BtlStage_GetStartPlace(0, &pos0, &rot0, 0);
            BtlStage_GetStartPlace(1, &pos1, &rot1, 0);
            BtlFacade_SetCharPos(0, &pos0);
            BtlFacade_SetCharRot(0, &rot0);
            BtlFacade_SetCharPos(1, &pos1);
            BtlFacade_SetCharRot(1, &rot1);
            BtlFacade_StartCharMoveF(0, 0, 0.0f, 1);
            BtlFacade_StartCharMoveF(1, 0, 0.0f, 1);
        }
        if (work->cur.event == NULL) {
            BtlStage_GetStartPlace(0, &pos0, &rot0, 0);
            BtlStage_GetStartPlace(1, &pos1, &rot1, 0);
            BtlFacade_SetCharPos(0, &pos0);
            BtlFacade_SetCharRot(0, &rot0);
            BtlFacade_SetCharPos(1, &pos1);
            BtlFacade_SetCharRot(1, &rot1);
            BtlFacade_StartCharMoveF(0, 0, 0.0f, 1);
            BtlFacade_StartCharMoveF(1, 0, 0.0f, 1);
        }
    }
    return 1;
}

/* Command 6 "scene end": gives both fighters back to the fight (control flags, shake off) under the same
   condition as command 5; an event with a camera command also ends the interrupt. */
s32 BtlScriptCmd_EndScene(u32 phase, void *taskWork) {
    BtlScriptWork *work = &gBtlScript;

    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        if (work->cur.event != NULL && work->cur.event->cmd4B1) {
            BtlFacade_SetCtrlFF(0);
            BtlFacade_SetCtrlFF(1);
            BtlFacade_ClearCtrl101(0);
            BtlFacade_ClearCtrl101(1);
            BtlFacade_ShakeCamera(0.0f);
            BtlFacade_EndInterrupt();
        }
        if (work->cur.event == NULL) {
            BtlFacade_SetCtrlFF(0);
            BtlFacade_SetCtrlFF(1);
            BtlFacade_ClearCtrl101(0);
            BtlFacade_ClearCtrl101(1);
            BtlFacade_ShakeCamera(0.0f);
        }
    }
    return 1;
}

/* Command 8 "battle": what arg. `what` is side << 15 | sub-command; see the table in the header. */
s32 BtlScriptCmd_Battle(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 what;
        s32 arg;
        s32 side;

        BtlFacade_BeginWait();
        what = Gsc_GetInt();
        arg = Gsc_GetInt();
        side = SCRIPT_SIDE(what);
        switch (what & 0xFFF) {
        case 0:
            break;
        case 1:
            BtlFacade_CallSeqExtra();
            break;
        case 2:
            BtlFacade_CallSeqExtra2();
            break;
        case 3:
            BtlFacade_RequestStageChange();
            break;
        case 4:
            BtlFacade_ForceBoth110();
            break;
        case 5:
        case 6:
        case 7:
            BtlFacade_ForceBoth111();
            break;
        case 8:
            BtlFacade_ForceActions(2, 2);
            break;
        case 9:
            BtlFacade_ForceActions(2, 3);
            break;
        case 10:
            BtlFacade_ForceActions(3, 2);
            break;
        case 11:
            BtlFacade_ForceActions(3, 3);
            break;
        case 12:
            BtlFacade_ForceActions(4, 4);
            break;
        case 13:
        case 14:
        case 15:
            break;
        case 16:
            BtlFacade_ChangeMember(side, 0);
            break;
        case 17:
            BtlFacade_ChangeMember(side, 1);
            break;
        case 18:
            BtlFacade_ChangeMember(side, 2);
            break;
        case 19:
            BtlFacade_ChangeMember(side, 3);
            break;
        case 20:
            BtlFacade_ChangeMember(side, 4);
            break;
        case 21:
        case 22:
            BtlFacade_Transform(side, arg);
            break;
        case 23:
            BtlFacade_Fuse(side, arg);
            break;
        case 24:
            BtlFacade_SetMaxPower(side, 1);
            break;
        case 25:
            BtlFacade_ForceAction01(side, 0);
            break;
        case 26:
            BtlFacade_ForceAction01(side, 1);
            break;
        case 27:
            BtlFacade_ForceAction23(side, 0);
            break;
        case 28:
            BtlFacade_ForceAction23(side, 1);
            break;
        case 29:
            BtlFacade_ForceAction4(side);
            break;
        }
        BtlFacade_ResetSubClock();
    }
    return 1;
}

/* Command 9 "result": n. n < 0 does nothing; else sets the battle result and waits until the battle reports
   it is no longer finished, plus one frame. */
s32 BtlScriptCmd_SetResult(u32 phase, void *taskWork) {
    s32 *work = taskWork;
    s32 ret = 0;
    s32 result;

    switch (phase) {
    case GSC_PHASE_BEGIN:
    case GSC_PHASE_BEGIN_ABORT:
        work[0] = 0;
        result = Gsc_GetInt();
        if (result >= 0) {
            BtlFacade_SetResult(result);
        } else {
            ret = 1;
        }
        break;
    case GSC_PHASE_UPDATE:
        if (BtlFacade_IsNotFinished()) {
            ret = work[0] > 0;
            work[0]++;
        }
        break;
    case GSC_PHASE_END:
    case GSC_PHASE_NOTIFY:
        break;
    }
    return ret;
}

/* Command 7 "triggers": -a trigger action (any number) -v trigger line (any number). Replaces the trigger
   list: start an event action / say a line when a battle event fires. Also clears text, camera move, voices. */
s32 BtlScriptCmd_SetTriggers(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 i;
        s32 trigger;

        BtlScript_ResetView();
        i = 0;
        while (Gsc_FindOptionN('a', i++)) {
            trigger = Gsc_GetInt();
            BtlScript_AddEvent(trigger, Gsc_GetInt());
        }
        i = 0;
        while (Gsc_FindOptionN('v', i++)) {
            trigger = Gsc_GetInt();
            BtlScript_AddLine(trigger, Gsc_GetInt(), 1);
        }
    }
    return 1;
}

/* Command 10 "rule": stage bgm timeLimit announcer unk. Clears the battle work and sets the rule of a story
   battle (mode 1). Setup action only. */
s32 BtlScriptCmd_SetRule(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 stage = Gsc_GetInt();
        s32 bgm = Gsc_GetInt();
        s32 timeLimit = Gsc_GetInt();
        s32 announcer = Gsc_GetInt();
        s32 unk10 = Gsc_GetInt();

        Battle_ClearWork();
        BattleSetup_SetRule(0, 1, bgm, timeLimit, announcer, stage, unk10);
    }
    return 1;
}

/* Command 11 "side": side members. Side 0 is the pad, any other side the CPU. Setup action only. */
s32 BtlScriptCmd_SetSide(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 side = Gsc_GetInt();
        s32 count = Gsc_GetInt();

        BattleSetup_SetSide(side, side != 0 ? 2 : 0, 0, count, 0, 0, 0, NULL);
    }
    return 1;
}

/* Command 12 "member": who chara costume variant cpuLevel item7 health item0..item6. Adds a team member
   (who = side << 15 | member index) and remembers its eight item ids. The CPU level goes up by 6 / 9 with the
   difficulty. A negative chara does nothing. Setup action only. */
s32 BtlScriptCmd_SetMember(u32 phase, void *taskWork) {
    BtlScriptCmdItems items;
    BtlScriptCmdWork *work = (BtlScriptCmdWork *)&gBtlScript;

    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 who = Gsc_GetInt();
        s32 member = who & 0xFFF;
        s32 side = SCRIPT_SIDE(who);
        s32 chara = Gsc_GetInt();
        s32 costume = Gsc_GetInt();
        s32 variant = Gsc_GetInt();
        s32 cpuLevel = Gsc_GetInt();
        s32 item7 = Gsc_GetInt();
        s32 health = Gsc_GetInt();

        items.id[0] = Gsc_GetInt();
        items.id[1] = Gsc_GetInt();
        items.id[2] = Gsc_GetInt();
        items.id[3] = Gsc_GetInt();
        items.id[4] = Gsc_GetInt();
        items.id[5] = Gsc_GetInt();
        items.id[6] = Gsc_GetInt();
        items.id[7] = item7;
        if (chara >= 0) {
            switch (gProgress->difficulty) {
            case 1:
                cpuLevel += 6;
                break;
            case 2:
                cpuLevel += 9;
                break;
            }
            BattleSetup_SetMemberByItemIds(side, member, chara, costume, variant, cpuLevel, health, (u32 *)&items);
            work->items[side][member] = items;
        }
    }
    return 1;
}

/* Command 13 "player status": [-C member] [-h n] [-H n] [-f n] [-F n] [-b n] [-B n] [-0..-6 item]. Changes
   health and gauges of a member of side 0 (default: the fighting one) and its items. */
s32 BtlScriptCmd_SetStatus0(u32 phase, void *taskWork) {
    BtlScriptCmdWork *work = (BtlScriptCmdWork *)&gBtlScript;

    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 member = BtlFacade_GetActiveMember(0);
        s32 value;
        BtlScriptCmdItems *items;

        if (Gsc_FindOption('C')) {
            member = Gsc_GetInt();
        }
        if (Gsc_FindOption('h')) {
            value = Gsc_GetInt();
            if (value > 0) {
                BtlFacade_RaiseHp(0, member, value);
            } else {
                BtlFacade_LowerHp(0, member, -value);
            }
        }
        if (Gsc_FindOption('H')) {
            BtlFacade_AddHp(0, member, Gsc_GetInt());
        }
        if (Gsc_FindOption('f')) {
            value = Gsc_GetInt();
            if (value > 0) {
                BtlFacade_RaiseGaugeC(0, member, value);
            } else {
                BtlFacade_LowerGaugeC(0, member, -value);
            }
        }
        if (Gsc_FindOption('F')) {
            BtlFacade_AddGaugeC(0, member, Gsc_GetInt());
        }
        if (Gsc_FindOption('b')) {
            value = Gsc_GetInt();
            if (value > 0) {
                BtlFacade_RaiseGauge14(0, member, value);
            } else {
                BtlFacade_LowerGauge14(0, member, -value);
            }
        }
        if (Gsc_FindOption('B')) {
            BtlFacade_AddGaugeC(0, member, Gsc_GetInt());
        }
        items = &work->items[0][member];
        if (Gsc_FindOption('0')) {
            items->id[0] = Gsc_GetInt();
        }
        if (Gsc_FindOption('1')) {
            items->id[1] = Gsc_GetInt();
        }
        if (Gsc_FindOption('2')) {
            items->id[2] = Gsc_GetInt();
        }
        if (Gsc_FindOption('3')) {
            items->id[3] = Gsc_GetInt();
        }
        if (Gsc_FindOption('4')) {
            items->id[4] = Gsc_GetInt();
        }
        if (Gsc_FindOption('5')) {
            items->id[5] = Gsc_GetInt();
        }
        if (Gsc_FindOption('6')) {
            items->id[6] = Gsc_GetInt();
        }
        BtlFacade_SetMemberItems(0, member, (BtlFacadeItem *)items);
    }
    return 1;
}

/* Command 14 "enemy status": as 13 for side 1, plus [-l cpuLevel] (raised by 6 / 9 with the difficulty) and
   [-a item7]. */
s32 BtlScriptCmd_SetStatus1(u32 phase, void *taskWork) {
    s32 side = 1;
    BtlScriptCmdWork *work = (BtlScriptCmdWork *)&gBtlScript;

    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 member = BtlFacade_GetActiveMember(side);
        s32 value;
        BtlScriptCmdItems *items;

        if (Gsc_FindOption('C')) {
            member = Gsc_GetInt();
        }
        if (Gsc_FindOption('l')) {
            value = Gsc_GetInt();
            switch (gProgress->difficulty) {
            case 1:
                value += 6;
                break;
            case 2:
                value += 9;
                break;
            }
            BtlFacade_SetCpuParam8Ex(value, 0);
        }
        if (Gsc_FindOption('h')) {
            value = Gsc_GetInt();
            if (value > 0) {
                BtlFacade_RaiseHp(side, member, value);
            } else {
                BtlFacade_LowerHp(side, member, -value);
            }
        }
        if (Gsc_FindOption('H')) {
            BtlFacade_AddHp(side, member, Gsc_GetInt());
        }
        if (Gsc_FindOption('f')) {
            value = Gsc_GetInt();
            if (value > 0) {
                BtlFacade_RaiseGaugeC(side, member, value);
            } else {
                BtlFacade_LowerGaugeC(side, member, -value);
            }
        }
        if (Gsc_FindOption('F')) {
            BtlFacade_AddGaugeC(side, member, Gsc_GetInt());
        }
        if (Gsc_FindOption('b')) {
            value = Gsc_GetInt();
            if (value > 0) {
                BtlFacade_RaiseGauge14(side, member, value);
            } else {
                BtlFacade_LowerGauge14(side, member, -value);
            }
        }
        if (Gsc_FindOption('B')) {
            BtlFacade_AddGaugeC(side, member, Gsc_GetInt());
        }
        items = &work->items[side][member];
        if (Gsc_FindOption('0')) {
            items->id[0] = Gsc_GetInt();
        }
        if (Gsc_FindOption('1')) {
            items->id[1] = Gsc_GetInt();
        }
        if (Gsc_FindOption('2')) {
            items->id[2] = Gsc_GetInt();
        }
        if (Gsc_FindOption('3')) {
            items->id[3] = Gsc_GetInt();
        }
        if (Gsc_FindOption('4')) {
            items->id[4] = Gsc_GetInt();
        }
        if (Gsc_FindOption('5')) {
            items->id[5] = Gsc_GetInt();
        }
        if (Gsc_FindOption('6')) {
            items->id[6] = Gsc_GetInt();
        }
        if (Gsc_FindOption('a')) {
            items->id[7] = Gsc_GetInt();
        }
        BtlFacade_SetMemberItems(side, member, (BtlFacadeItem *)items);
    }
    return 1;
}

/* Command 15 "speakers": -v line who (any number). Says which fighter (side << 15 | member) speaks a line. */
s32 BtlScriptCmd_SetSpeakers(u32 phase, void *taskWork) {
    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        s32 i;
        s32 line;

        i = 0;
        while (Gsc_FindOptionN('v', i++)) {
            line = Gsc_GetInt();
            BtlScript_SetSpeaker(line, Gsc_GetInt());
        }
    }
    return 1;
}

/* Command 16 "rewards": 15 numbers stored in the progress block at +0x40. */
s32 BtlScriptCmd_SetRewards(u32 phase, void *taskWork) {
    BtlScriptCmdReward *reward = &gProgress->reward;

    if (phase == GSC_PHASE_BEGIN || phase == GSC_PHASE_BEGIN_ABORT) {
        reward->unk0[0] = Gsc_GetInt();
        reward->unk0[1] = Gsc_GetInt();
        reward->unk0[2] = Gsc_GetInt();
        reward->unkC[0] = Gsc_GetInt();
        reward->unkC[1] = Gsc_GetInt();
        reward->unkC[2] = Gsc_GetInt();
        reward->unk24[0] = Gsc_GetInt();
        reward->unk24[1] = Gsc_GetInt();
        reward->unk24[2] = Gsc_GetInt();
        reward->unk18[0] = Gsc_GetInt();
        reward->unk18[1] = Gsc_GetInt();
        reward->unk18[2] = Gsc_GetInt();
        reward->unk30[0] = Gsc_GetInt();
        reward->unk30[1] = Gsc_GetInt();
        reward->unk30[2] = Gsc_GetInt();
    }
    return 1;
}
