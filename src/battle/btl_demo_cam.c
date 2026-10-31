#include "common.h"
#include "battle/battle.h"
#include "battle/btl_demo_cam.h"
#include "sys/heap.h"
#ifdef PORT
#include "battle/tag_team.h"
#include "battle/btl_seq.h"
static DemoCam fighterDemo[PORT_FIGHTER_CAPACITY];
static s32 fighterDemoTick[PORT_FIGHTER_CAPACITY];
static u32 fighterDemoFrame;
static CamShake fighterDemoShake[PORT_FIGHTER_CAPACITY];
#endif

/* Scripted (demo) camera, 0x23D1E8-0x23E040. See battle/btl_demo_cam.h. */

extern void *memset(void *dst, s32 c, u32 n);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_SetZeroW1(Vec4 *dst);                /* dst = 0 */
extern void Mtx_StoreIdentity(Mtx44 *dst);
extern void Mtx_Mul(Mtx44 *a, Mtx44 *b, Mtx44 *dst); /* matrix product */
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);       /* copy */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);       /* inverse of a rotation + translation matrix */
extern void Mtx_RotateZ(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Z */
extern void Mtx_RotateX(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about X */
extern void Mtx_RotateY(Mtx44 *dst, Mtx44 *src, f32 angle); /* rotate about Y */
extern void Vu0View_LoadMtx(Mtx44 *m);

extern void *BtlObj_Get(s32 id);
extern void *BtlCharApi_GetPlayer(void *arg);
extern s32 BtlCharApi_HasMemberUnk70(void *arg);
extern void BtlCharApi_GetCamUnk460(void *arg, Vec4 *out);
extern s32 BtlStage_IsReady(void);
extern DemoCamAnim *BtlStage_GetFileMember(s32 idx);          /* the stage's camera animation idx (0..2) */
extern u8 *BtlObj_GetNode(void *obj, s32 arg);

/* Turns the file offsets of a camera animation into pointers (once). */
void DemoCam_FixupAnim(DemoCamAnim *anim) {
    s32 i;
    DemoCamChannel *ch;

    if (anim->needsFixup != 0) {
        anim->channels = (DemoCamChannel *)((u8 *)anim->channels + (s32)anim);
        anim->defaults = (DemoCamPose *)((u8 *)anim->defaults + (s32)anim);
        if (anim->unk1C != NULL) {
            anim->unk1C = anim->unk1C + (s32)anim;
        }
        if (anim->version < 2) {
            anim->unk1C = NULL;
        }
        ch = anim->channels;
        for (i = 0; i < anim->channelCount; i++) {
            ch->keys = (DemoCamKey *)((u8 *)ch->keys + (s32)anim);
            ch++;
        }
        anim->needsFixup = 0;
    }
}

/* Evaluates the current animation at `time`: defaults, then each channel's keys (linear or step). */
void DemoCam_EvalAnim(DemoCamPose *out, f32 time) {
    DemoCamAnim *anim = gDemoCam->anim;
    DemoCamChannel *ch = anim->channels;
    s32 i;

    *out = *anim->defaults;
    for (i = 0; i < anim->channelCount; i++) {
        DemoCamKey *key = ch->keys;
        DemoCamKey *prev = NULL;
        DemoCamKey *next = NULL;
        s32 j;

        for (j = 0; j < ch->keyCount; j++) {
            if (key->time == time) {
                out->f.frame = (s32)key->time;
                prev = key;
                break;
            }
            if (key->time < time) {
                prev = key;
            }
            if (time < key->time) {
                next = key;
                break;
            }
            key++;
        }
        if (next != NULL && prev != NULL) {
            if (prev->flags & 0x10) {
                out->v[ch->index] = prev->value;
            } else {
                out->v[ch->index] =
                    (next->value - prev->value) * ((time - prev->time) / (next->time - prev->time)) + prev->value;
            }
        } else if (prev != NULL) {
            out->v[ch->index] = prev->value;
        } else if (next != NULL) {
            out->v[ch->index] = next->value;
        }
        ch++;
    }
}

/* Returns the demo camera block. */
DemoCam *DemoCam_Get(void) {
    return gDemoCam;
}

/* Forgets the animation and state (everything after the view), back to a full-screen idle camera. */
void DemoCam_Reset(void) {
    memset(&gDemoCam->base, 0, 0xE0);
    View_InitLayout(&gDemoCam->view, VIEW_LAYOUT_FULL);
    Mtx_StoreIdentity(&gDemoCam->base);
    Vec4_SetZeroW1(&gDemoCam->fixedPos);
    Vec4_SetZeroW1(&gDemoCam->fixedRot);
}

/* Allocates the demo camera. */
void DemoCam_Init(void) {
#ifdef PORT
    memset(fighterDemo,0,sizeof(fighterDemo));
    memset(fighterDemoTick,-1,sizeof(fighterDemoTick));
#endif
    gDemoCam = Heap_Alloc(sizeof(DemoCam), 0x20, 0, HEAP_ANY);
    memset(gDemoCam, 0, sizeof(DemoCam));
    View_InitLayout(&gDemoCam->view, VIEW_LAYOUT_FULL);
    DemoCam_Reset();
}

/* Frees the demo camera. */
void DemoCam_Term(void) {
    Heap_Free(gDemoCam);
    gDemoCam = NULL;
}

/* Selects the animation to play (fixing up its pointers) and detaches the camera from any object. */
void DemoCam_SetAnim(DemoCamAnim *anim) {
    gDemoCam->anim = anim;
    DemoCam_FixupAnim(gDemoCam->anim);
    gDemoCam->unk2F8 = 0;
    gDemoCam->unk2FC = 0;
    gDemoCam->obj = NULL;
    gDemoCam->chr = NULL;
    Mtx_StoreIdentity(&gDemoCam->base);
}

/* Per frame: advances and evaluates the animation (or the fixed pose), adds shake, builds and applies the view. */
#if 0
/* 11 of 384 instructions differ, all in the fixed-pose branch: the registers holding &pos and &rot are swapped
 * ($s0 / $s1). The animation branch, the frame layout and every call match. (Routing the tail through a shared
 * `Vec4 *eye` fixes that swap but swaps the registers of from/to/hit in the collision part instead.) */
/* Second cleanup pass (allocator dump): both addresses live in one basic block and tie exactly in the local
 * allocator's priority (&pos 5 references over 120 instructions, &rot 4 over 96: 2 * 5 / 120 = 2 * 4 / 96); the
 * original has &pos first, so one instruction more or less at that stage decides it. Tried without effect: the
 * offset additions of rot in front of those of pos (29 differences), `rot.w` / `pos.w` set behind the next
 * call (15, 16). */
s32 DemoCam_Update(void) {
    if (gDemoCam->fixed != 0) {
        Vec4 posOfs;
        Vec4 rotOfs;
        Vec4 pos;
        Vec4 rot;

        memset(&posOfs, 0, sizeof(Vec4));
        posOfs.w = 1.0f;
        memset(&rotOfs, 0, sizeof(Vec4));
        rotOfs.w = 1.0f;
        memset(&pos, 0, sizeof(Vec4));
        pos.w = 1.0f;
        memset(&rot, 0, sizeof(Vec4));
        rot.w = 1.0f;
        Vec4_Copy(&pos, &gDemoCam->fixedPos);
        Vec4_Copy(&rot, &gDemoCam->fixedRot);
        CamShake_Calc(&gDemoCam->shake, &posOfs, &rotOfs);
        pos.x += posOfs.x;
        pos.y += posOfs.y;
        pos.z += posOfs.z;
        rot.x += rotOfs.x;
        rot.y += rotOfs.y;
        rot.z += rotOfs.z;
        CamShake_Tick(&gDemoCam->shake);
        View_SetTransform(&gDemoCam->view.world2view, &gDemoCam->view.world2view2, &pos, &rot);
        View_UpdateMatrices(&gDemoCam->view, &pos);
        View_Apply(&gDemoCam->view, 1);
        return 0;
    } else {
        Mtx44 inv;
        Mtx44 m;
        Vec4 rot;
        Vec4 shakePos;
        Vec4 shakeRot;
        Vec4 from;
        Vec4 to;
        Vec4 hit;
        f32 frac;
        DemoCamPose *pose;
        void *owner;
        f32 t;
        s32 i;

        if (gDemoCam->anim == NULL) {
            return 1;
        }
        if (gDemoCam->flags & DEMO_CAM_AUTO) {
            if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
                t = DemoCam_GetTime() + 2.0f;
                if (DemoCam_GetLength() < t) {
                    if (gDemoCam->flags & DEMO_CAM_STOP_AT_END) {
                        DemoCam_Stop();
                    }
                    DemoCam_SetTime(DemoCam_GetLength());
                } else {
                    DemoCam_SetTime(t);
                }
            }
        }
        if (gDemoCam->obj != NULL) {
            DemoCam_SetBase((Mtx44 *)(BtlObj_GetNode(gDemoCam->obj, 0) + 0x10));
        } else if (gDemoCam->chr != NULL) {
            DemoCam_SetBase((Mtx44 *)((u8 *)gDemoCam->chr + 0x9A0));
        }
        pose = &gDemoCam->pose;
        DemoCam_EvalAnim(pose, gDemoCam->time);
        memset(&shakePos, 0, sizeof(Vec4));
        shakePos.w = 1.0f;
        Vec4_Set(&rot, pose->f.rot[0], pose->f.rot[1], pose->f.rot[2], 0.0f);
        CamShake_Calc(&gDemoCam->shake, &shakePos, &shakeRot);
        rot.x += shakeRot.x;
        rot.y += shakeRot.y;
        rot.z += shakeRot.z;
        CamShake_Tick(&gDemoCam->shake);
        for (i = 0; i < 3; i++) {
            f32 *a = &((f32 *)&rot)[i];

            if (*a > 3.14159265f) {
                *a -= 6.2831853f;
            }
            if (*a < -3.14159265f) {
                *a += 6.2831853f;
            }
            if (*a > 3.14159265f) {
                *a -= 6.2831853f;
            }
            if (*a < -3.14159265f) {
                *a += 6.2831853f;
            }
        }
        Mtx_StoreIdentity(&gDemoCam->view.world2view2);
        Mtx_StoreIdentity(&m);
        Mtx_RotateX(&m, &m, rot.x);
        Mtx_Mul(&gDemoCam->view.world2view2, &m, &gDemoCam->view.world2view2);
        Mtx_StoreIdentity(&m);
        Mtx_RotateY(&m, &m, rot.y);
        Mtx_Mul(&gDemoCam->view.world2view2, &m, &gDemoCam->view.world2view2);
        Mtx_StoreIdentity(&m);
        Mtx_RotateZ(&m, &m, rot.z);
        Mtx_Mul(&gDemoCam->view.world2view2, &m, &gDemoCam->view.world2view2);
        if (gDemoCam->obj != NULL) {
            owner = gDemoCam->obj;
        } else {
            owner = gDemoCam->chr;
        }
        if (owner != NULL && BtlCharApi_HasMemberUnk70(BtlCharApi_GetPlayer(*(void **)((u8 *)owner + 0x10))) != 0 &&
            gDemoCam->scaleHeight != 0) {
            f32 scale = (*(f32 **)((u8 *)owner + 0x91C))[1] / 1.1f;

            gDemoCam->view.world2view2.m[3][0] = pose->f.pos[0] + shakePos.x;
            gDemoCam->view.world2view2.m[3][1] = pose->f.pos[1] * scale + shakePos.y;
            gDemoCam->view.world2view2.m[3][2] = pose->f.pos[2] + shakePos.z;
        } else {
            gDemoCam->view.world2view2.m[3][0] = pose->f.pos[0] + shakePos.x;
            gDemoCam->view.world2view2.m[3][1] = pose->f.pos[1] + shakePos.y;
            gDemoCam->view.world2view2.m[3][2] = pose->f.pos[2] + shakePos.z;
        }
        Mtx_Mul(&gDemoCam->view.world2view2, &gDemoCam->base, &gDemoCam->view.world2view2);
        Mtx_InverseRT(&gDemoCam->view.world2view2, &gDemoCam->view.world2view2);
        Vu0View_LoadMtx(&gDemoCam->view.world2view2);
        Mtx_InverseRT(&inv, &gDemoCam->view.world2view2);
        if (gDemoCam->obj != NULL || gDemoCam->chr != NULL) {
            if (BtlStage_IsReady() != 0) {
                Vec4_Copy(&from, (Vec4 *)inv.m[3]);
                if (gDemoCam->obj != NULL) {
                    BtlCharApi_GetCamUnk460(*(void **)((u8 *)gDemoCam->obj + 0x10), &to);
                } else {
                    BtlCharApi_GetCamUnk460(*(void **)((u8 *)gDemoCam->chr + 0x10), &to);
                }
                if (BtlCam_TraceStage(&hit, &from, &to, &frac, NULL) != 0) {
                    Vec4_Copy((Vec4 *)inv.m[3], &hit);
                    Mtx_InverseRT(&gDemoCam->view.world2view2, &inv);
                    Vu0View_LoadMtx(&gDemoCam->view.world2view2);
                }
            }
        }
        View_UpdateMatrices(&gDemoCam->view, (Vec4 *)inv.m[3]);
        View_Apply(&gDemoCam->view, 1);
        return 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/btl_demo_cam", DemoCam_Update);
#endif

/* Sets the matrix the animation is relative to (ignored while no animation is selected). */
void DemoCam_SetBase(Mtx44 *base) {
    if (gDemoCam->anim != NULL) {
        Mtx_Copy(&gDemoCam->base, base);
    }
}

/* Length of the selected animation (0 without one). */
f32 DemoCam_GetLength(void) {
    DemoCamAnim *anim = gDemoCam->anim;
    f32 ret = 0.0f;

    if (anim != NULL) {
        ret = anim->length;
    }
    return ret;
}

/* Current animation time (0 without an animation). */
f32 DemoCam_GetTime(void) {
    f32 ret = 0.0f;

    if (gDemoCam->anim != NULL) {
        ret = gDemoCam->time;
    }
    return ret;
}

/* Sets the animation time, remembering the smaller of the old and new one. */
void DemoCam_SetTime(f32 time) {
    if (gDemoCam->anim != NULL) {
        if (!(gDemoCam->time <= time)) {
            gDemoCam->prevTime = time;
        } else {
            gDemoCam->prevTime = gDemoCam->time;
        }
        gDemoCam->time = time;
    }
}

/* 1 while the demo camera owns the screen; a pending stop request resets it and gives 0. */
s32 DemoCam_IsActive(void) {
    s32 playing;
    s32 flags;

    if (gDemoCam->fixed != 0) {
        return 1;
    }
    if (gDemoCam->anim != NULL) {
        flags = gDemoCam->flags;
        playing = flags & DEMO_CAM_PLAYING;
        flags &= DEMO_CAM_STOP;
        if (flags) {
            DemoCam_Reset();
        } else if (playing) {
            return 1;
        }
    }
    return 0;
}

/* 1 when an animation is selected, else whether the fixed pose is on. */
s32 DemoCam_IsInUse(void) {
    if (gDemoCam->anim != NULL) {
        return 1;
    }
    return gDemoCam->fixed != 0;
}

/* Starts the selected animation; the caller steps the time. */
void DemoCam_Start(void) {
    if (gDemoCam->anim != NULL) {
        gDemoCam->fixed = 0;
        gDemoCam->flags = DEMO_CAM_PLAYING;
    }
}

/* Starts the selected animation, advancing by itself and holding its last frame. */
void DemoCam_StartAuto(void) {
    if (gDemoCam->anim != NULL) {
        gDemoCam->fixed = 0;
        gDemoCam->flags = DEMO_CAM_PLAYING | DEMO_CAM_AUTO;
    }
}

/* Starts the selected animation, advancing by itself and ending the demo camera at its end. */
void DemoCam_StartAutoOnce(void) {
    if (gDemoCam->anim != NULL) {
        gDemoCam->fixed = 0;
        gDemoCam->flags = DEMO_CAM_PLAYING | DEMO_CAM_AUTO | DEMO_CAM_STOP_AT_END;
    }
}

/* Asks for the animation to end at the next DemoCam_IsActive(). */
void DemoCam_Stop(void) {
    if (gDemoCam->anim != NULL) {
        gDemoCam->flags |= DEMO_CAM_STOP;
    }
}

/* Attaches the animation to an object. */
void DemoCam_SetObj(void *obj) {
    if (gDemoCam->anim != NULL) {
        gDemoCam->obj = obj;
        gDemoCam->chr = NULL;
    }
}

/* Attaches the animation to a fighter. */
void DemoCam_SetChr(void *chr) {
    if (gDemoCam->anim != NULL) {
        gDemoCam->chr = chr;
        gDemoCam->obj = NULL;
    }
}

/* Adds a shake of strength 1 lasting `time` seconds. */
void DemoCam_AddShake(f32 time) {
    CamShake_Add(&gDemoCam->shake, 1.0f, time);
}

/* Time left of the demo camera's shake. */
f32 DemoCam_GetShakeTime(void) {
    return CamShake_GetTime(&gDemoCam->shake);
}

/* Switches to a fixed pose (ending any animation). */
void DemoCam_SetFixedPose(Vec4 *pos, Vec4 *rot) {
    if (gDemoCam->anim != NULL) {
        gDemoCam->flags |= DEMO_CAM_STOP;
    }
    gDemoCam->fixed = 1;
    Vec4_Copy(&gDemoCam->fixedPos, pos);
    Vec4_Copy(&gDemoCam->fixedRot, rot);
    gDemoCam->fixedPos.w = 1.0f;
    gDemoCam->fixedRot.w = 1.0f;
}

/* Copies the fixed pose out. */
void DemoCam_GetFixedPose(Vec4 *pos, Vec4 *rot) {
    Vec4_Copy(pos, &gDemoCam->fixedPos);
    Vec4_Copy(rot, &gDemoCam->fixedRot);
}

/* Enables scaling the animated camera height by the attached fighter's size. */
void DemoCam_SetScaleHeight(s32 on) {
    gDemoCam->scaleHeight = on;
}

/* Leaves the fixed pose. */
void DemoCam_ClearFixed(void) {
    gDemoCam->fixed = 0;
}

/* Whether the fixed pose is on. */
s32 DemoCam_IsFixed(void) {
    return gDemoCam->fixed;
}

/* Plays camera animation `idx` of a battle object (pointer table at obj + 0xB0), attached to it. */
void DemoCam_PlayObjAnim(s32 objId, s32 idx) {
    u8 *obj = BtlObj_Get(objId);
    DemoCamAnim *anim = ((DemoCamAnim **)(obj + 0xB0))[idx];

    if (anim != NULL) {
        DemoCam_SetAnim(anim);
        DemoCam_SetObj(obj);
        DemoCam_SetTime(0.0f);
        DemoCam_StartAuto();
    }
}

/* Plays the fighter's camera animation at obj + 0x908, attached to the fighter. */
void DemoCam_PlayCharAnim0(s32 objId) {
    u8 *obj = BtlObj_Get(objId);
    DemoCamAnim *anim = *(DemoCamAnim **)(obj + 0x908);

    if (anim != NULL) {
        DemoCam_SetAnim(anim);
        DemoCam_SetChr(obj);
        DemoCam_SetTime(0.0f);
        DemoCam_StartAuto();
    }
}

/* Plays the fighter's camera animation at obj + 0x90C, attached to the fighter. */
void DemoCam_PlayCharAnim1(s32 objId) {
    u8 *obj = BtlObj_Get(objId);
    DemoCamAnim *anim = *(DemoCamAnim **)(obj + 0x90C);

    if (anim != NULL) {
        DemoCam_SetAnim(anim);
        DemoCam_SetChr(obj);
        DemoCam_SetTime(0.0f);
        DemoCam_StartAuto();
    }
}

/* Plays the fighter's camera animation at obj + 0x910, attached as an object. */
void DemoCam_PlayCharAnim2(s32 objId) {
    u8 *obj = BtlObj_Get(objId);
    DemoCamAnim *anim = *(DemoCamAnim **)(obj + 0x910);

    if (anim != NULL) {
        DemoCam_SetAnim(anim);
        DemoCam_SetObj(obj);
        DemoCam_SetTime(0.0f);
        DemoCam_StartAuto();
    }
}

/* Plays the stage's camera animation idx (the intro cuts) in world space; 0 if the stage has none. */
s32 DemoCam_PlayStageAnim(s32 idx) {
    DemoCamAnim *anim = BtlStage_GetFileMember(idx);

    if (anim != NULL) {
        DemoCam_SetAnim(anim);
        DemoCam_SetTime(0.0f);
        DemoCam_StartAutoOnce();
        return 1;
    }
    return 0;
}

#ifdef PORT
/* Character camera files used to share the single stage/intro camera. Keep
   their native animation, but give each fighter its own playback context. */
void *DemoCam_EnterFighter(s32 actor) {
    DemoCam *saved=gDemoCam;gDemoCam=&fighterDemo[actor];return saved;
}
void DemoCam_LeaveFighter(void *saved) {gDemoCam=saved;}
void DemoCam_FighterRequest(s32 actor,s32 objId,s32 flags) {
    DemoCam *saved=gDemoCam;
    if((u32)actor>=PORT_FIGHTER_CAPACITY)return;
    gDemoCam=&fighterDemo[actor];
    if(flags&8)DemoCam_Stop();
    if(flags&1)DemoCam_PlayCharAnim0(objId);
    if(flags&2)DemoCam_PlayCharAnim1(objId);
    if(flags&4)DemoCam_PlayCharAnim2(objId);
    if(flags)fighterDemoTick[actor]=-1;
    gDemoCam=saved;
}
void DemoCam_ResetFighters(void) {
    memset(fighterDemo,0,sizeof(fighterDemo));
    memset(fighterDemoTick,-1,sizeof(fighterDemoTick));
}
/* The duel clock stops during transformations and other cinematics. A camera
   needs one step per simulation update, not one step per duel-timer tick. */
void DemoCam_BeginFighterFrame(void) {fighterDemoFrame++;}
s32 DemoCam_FighterActive(s32 actor) {
    DemoCam *saved=gDemoCam;s32 active;
    if(!TagTeam_Active() || (u32)actor>=(u32)TagTeam_Count())return 0;
    gDemoCam=&fighterDemo[actor];active=DemoCam_IsActive();gDemoCam=saved;
    return active;
}
s32 DemoCam_ApplyFighter(s32 actor,View *view,Vec4 *eye) {
    DemoCam *saved=gDemoCam;Mtx44 inv;s32 flags,frame,repeat;
    if(!DemoCam_FighterActive(actor))return 0;
    gDemoCam=&fighterDemo[actor];gDemoCam->view=*view;
    frame=fighterDemoFrame;
    flags=gDemoCam->flags;
    /* Fused partners and the target may all view the same animation. Advance
       it once per simulation update, independent of viewport count. */
    repeat=fighterDemoTick[actor]==frame;
    if(repeat)gDemoCam->shake=fighterDemoShake[actor];
    else fighterDemoShake[actor]=gDemoCam->shake;
    if(repeat || TagTeam_CinematicPaused(actor))gDemoCam->flags&=~DEMO_CAM_AUTO;
    DemoCam_Update();
    gDemoCam->flags|=flags&DEMO_CAM_AUTO;
    fighterDemoTick[actor]=frame;
    *view=gDemoCam->view;
    Mtx_InverseRT(&inv,&view->world2view2);
    Vec4_Copy(eye,(Vec4 *)inv.m[3]);
    gDemoCam=saved;
    return 1;
}
#endif
