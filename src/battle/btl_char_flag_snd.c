#include "common.h"
#include "battle/btl_char_flag.h"
#include "battle/btl_cam.h"
#include "sys/snd.h"
#include "sys/adx.h"

/*
 * Fighter sound: 0x1D9A20..0x1DA8B8.
 *
 * Fighter code never plays a sound directly. It queues a request (position, near / far distance, id,
 * owner, kind) in the roster's list of four; BtlCharSnd_PlayRequests, run once per frame after the
 * fighters, turns each request into a volume and a pan relative to the battle camera and plays it.
 * Sample-bank sounds return a handle that is remembered per side: one-shot sounds in one set of four
 * slots (so that they can be cut short), looping sounds in another. A looping sound has to be requested
 * again every frame: one that was not requested this frame is stopped.
 */

extern f32 atan2f(f32 y, f32 x);
extern f32 Mathf_Sin(f32 x);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src); /* inverse of a rotation + translation matrix */
extern f32 BtlUtil_WrapAngle(f32 a);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);
extern s32 BtlUtil_Max(s32 a, s32 b);
extern s32 BtlChar_GetCount(void);
extern BtlFlagChr *BtlChar_Get(s32 i);
extern void *BtlChar_GetObj(BtlFlagChr *chr);
extern s32 BtlChar_TestMemberUnk70(BtlFlagChr *chr);
extern void BtlObj_SetSubState(void *obj, s32 state, s32 arg);
extern s32 Battle_IsSplitScreen(void);
extern s32 Battle_GetMode(void);
extern s32 BtlScript_IsTextShown(void);
extern s32 BtlAnim_TestAttr(BtlFlagChr *chr, u64 mask);       /* event bits of the object's animation */
extern s32 BtlAct_GetCurrentClass(BtlFlagChr *chr);                 /* technique slot of the current action, or -1 */
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out); /* world position of a model node */
extern s32 BtlSuper_GetFlags(BtlFlagChr *chr, s32 slot);       /* attribute word of technique `slot` */

extern BtlFlagRoster *gBtlChars;
/* Sample bank of request kinds 0..3; none for the rest. Defined here: this object's first rodata. */
const s32 gBtlSndBankMask[16] = { 4, 8, 0x10, 0x20, -1, -1, -1, -1, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000 };

static s32 own_kind(s32 player) { return player < 2 ? player + 2 : player + 6; }

/* A four-fighter frame can request voices, strikes and loops together. Keep
 * the original roster layout intact and give the native fight its own queue. */
#ifdef PORT
static BtlFlagSndReq nativeRequests[8*PORT_FIGHTER_CAPACITY];
static s32 nativeRequestCount;
#endif
static BtlFlagSndReq *sound_requests(void) {
#ifdef PORT
    if(TagTeam_Active()) return nativeRequests;
#endif
    return gBtlChars->snd.req;
}
static s32 *sound_count(void) {
#ifdef PORT
    if(TagTeam_Active()) return &nativeRequestCount;
#endif
    return &gBtlChars->snd.count;
}
static s32 sound_capacity(void) {
#ifdef PORT
    if(TagTeam_Active()) return 8*PORT_FIGHTER_CAPACITY;
#endif
    return 4;
}


/* The fighter's set of one-shot sound slots. */
BtlFlagSoundSet *BtlCharSnd_GetSet(BtlFlagChr *chr) {
    if (gBtlChars != NULL) {
        return &gBtlChars->sounds[chr->player];
    }
    return NULL;
}

/* The fighter's set of looping sound slots. */
BtlFlagSoundSet *BtlCharSnd_GetLoopSet(BtlFlagChr *chr) {
    if (gBtlChars != NULL) {
        return &gBtlChars->loopSounds[chr->player];
    }
    return NULL;
}

/* 1 when the fighter already has this looping sound playing. */
s32 BtlCharSnd_IsLoopPlaying(s32 player, s32 kind, s32 id) {
    BtlFlagChr *chr = BtlChar_Get(player);
    BtlFlagSound *slot;
    s32 i;

    if (chr == NULL) {
        return 0;
    }
    slot = BtlCharSnd_GetLoopSet(chr)->slot;
    for (i = 0; i < 4; i++, slot++) {
        if (slot->handle >= 0 && id == slot->id && kind == slot->kind) {
            return 1;
        }
    }
    return 0;
}

/* 1 when the request list holds this sound for this owner. */
s32 BtlCharSnd_IsRequested(s32 owner, s32 kind, s32 id) {
    BtlFlagSndReq *requests = sound_requests();
    s32 *count = sound_count();
    s32 i;

    for (i = 0; i < (*count); i++) {
        if (owner == requests[i].p.owner && id == requests[i].p.id && kind == requests[i].p.kind) {
            return 1;
        }
    }
    return 0;
}

/* Queues a positional sound up to this battle's request capacity. */
void BtlCharSnd_Request(Vec4 *pos, s32 owner, s32 kind, f32 near, f32 far, s32 id) {
    BtlFlagSndReq *requests;
    s32 *count;
    BtlFlagSndReq *req;
    s32 ok;

    if (gBtlChars != NULL) {
        requests = sound_requests();
        count = sound_count();
        ok = (*count) < sound_capacity();
        if (ok) {
            req = &requests[(*count)];
            Vec4_Copy(&req->pos, pos);
            req->p.owner = owner;
            req->p.kind = kind;
            req->p.id = id;
            req->p.near = near;
            req->p.far = far;
            (*count)++;
        }
    }
}

/* Remembers a playing sound in the first free slot of a set (any slot when force is set). */
void BtlCharSnd_StoreHandle(BtlFlagSoundSet *set, s32 handle, s32 player, s32 kind, s32 id, s32 force) {
    s32 i;

    if (BtlChar_Get(player) != NULL) {
        for (i = 0; i < 4; i++) {
            s32 n = (set->next + i) % 4;

            if (set->slot[n].handle < 0 || force) {
                set->slot[n].handle = handle;
                set->slot[n].kind = kind;
                set->slot[n].id = (u8)id;
                set->next = (set->next + 1) % 4;
                break;
            }
        }
    }
}

/* Stops the fighter's looping sounds that were not requested this frame. */
void BtlCharSnd_StopUnrequestedLoops(BtlFlagChr *chr) {
    BtlFlagSound *slot = BtlCharSnd_GetLoopSet(chr)->slot;
    s32 i;

    for (i = 0; i < 4; i++, slot++) {
        if (slot->handle >= 0 && !BtlCharSnd_IsRequested(chr->player, slot->kind, slot->id)) {
            Snd_StopHandle(slot->handle);
            slot->handle = -1;
        }
    }
}

/* Volume (0..0x80) and pan (-0x40..0x40) of a sound at pos for the battle camera; centred at 0x40 in split screen. */
void BtlCharSnd_CalcVolPan(Vec4 *pos, s32 *vol, s32 *pan, f32 near, f32 far) {
    Vec4 d;
    Vec4 fwd;
    Mtx44 cam;
    f32 dist;
    f32 ang;
    s32 v;
    s32 p;

    if (Battle_IsSplitScreen()) {
        *vol = 0x40;
        *pan = 0;
        return;
    }
    Mtx_InverseRT(&cam, &gBtlCamView->world2view2);
    Vec4_Sub(&d, pos, (Vec4 *)cam.m[3]);
    dist = Vec3_Length(&d);
    Vec4_Copy(&fwd, (Vec4 *)cam.m[2]);
    if (__builtin_fabsf(d.x) < 0.001f && __builtin_fabsf(d.z) < 0.001f) {
        ang = 0.0f;
    } else if (__builtin_fabsf(fwd.x) < 0.001f && __builtin_fabsf(fwd.z) < 0.001f) {
        ang = 0.0f;
    } else {
        ang = atan2f(d.x, d.z);
        ang = BtlUtil_WrapAngle(ang - atan2f(fwd.x, fwd.z));
    }
    v = BtlUtil_Clamp((1.0f - (dist - near) / (far - near)) * 128.0f, 0, 0x80);
    p = BtlUtil_Clamp(Mathf_Sin(ang) * 64.0f, -0x40, 0x40);
    if (Battle_GetMode() == 1) {
        if (BtlScript_IsTextShown()) {
            v = BtlUtil_Max(v - 0x20, 0);
        }
    }
    *vol = v;
    *pan = p;
}

/* Empties the request slots and every fighter's sound slots (the request count is left alone). */
void BtlCharSnd_Init(void) {
    BtlFlagSndReq *requests = sound_requests();
    s32 *count = sound_count();
    s32 i;
    s32 j;

    *count = 0;
    for (i = 0; i < sound_capacity(); i++) {
        BtlFlagSndParam *p = &requests[i].p;

        p->kind = -1;
        p->id = 0xFF;
        p->owner = -1;
    }
    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlFlagChr *chr = BtlChar_Get(i);
        BtlFlagSoundSet *set = BtlCharSnd_GetSet(chr);
        BtlFlagSoundSet *loop = BtlCharSnd_GetLoopSet(chr);

        for (j = 0; j < 4; j++) {
            set->slot[j].handle = -1;
            loop->slot[j].handle = -1;
        }
    }
}

/* Empties the request list (start of the frame). */
void BtlCharSnd_ClearRequests(void) {
    BtlFlagSndReq *requests = sound_requests();
    s32 *count = sound_count();
    s32 i;

    for (i = 0; i < sound_capacity(); i++) {
        BtlFlagSndParam *p = &requests[i].p;

        p->kind = -1;
        p->id = 0xFF;
        p->owner = -1;
    }
    (*count) = 0;
}

/* Plays the queued requests, then stops the looping sounds nobody asked for this frame. */
void BtlCharSnd_PlayRequests(void) {
    BtlFlagSndReq *requests = sound_requests();
    s32 *count = sound_count();
    s32 i;

    for (i = 0; i < (*count); i++) {
        BtlFlagSndReq *req = &requests[i];
        s32 vol;
        s32 pan;
        s32 owner;
        BtlFlagChr *chr;
        s32 chara;
        s32 handle;

        owner = 0;
        if (req->p.owner >= 0) {
            owner = req->p.owner;
        }
        chr = BtlChar_Get(owner);
        chara = *(s32 *)((u8 *)BtlChar_GetObj(chr) + 0xC);
        if (BtlChar_TestMemberUnk70(chr)) {
            chara = 0x56;
        }
        switch (req->p.kind) {
        case 4:
            BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
            StreamSe_Play(owner, req->p.id, vol, pan);
            break;
        case 5:
        case 6:
            BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
            Voice_PlayCharaEx(owner, chara, req->p.id, vol, pan);
            break;
        case 7:
            BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
            Voice_Play(owner, req->p.id, vol, pan);
            break;
        default:
            if (Snd_IsLoopSe(gBtlSndBankMask[req->p.kind], req->p.id)) {
                if (!BtlCharSnd_IsLoopPlaying(owner, req->p.kind, req->p.id)) {
                    BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
                    handle = Snd_PlaySeEx(gBtlSndBankMask[req->p.kind], req->p.id, vol, pan, 0);
                    BtlCharSnd_StoreHandle(BtlCharSnd_GetLoopSet(chr), handle, owner, req->p.kind, req->p.id, 0);
                }
            } else {
                BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
                handle = Snd_PlaySeEx(gBtlSndBankMask[req->p.kind], req->p.id, vol, pan, 0);
                BtlCharSnd_StoreHandle(BtlCharSnd_GetSet(chr), handle, owner, req->p.kind, req->p.id, 1);
            }
            break;
        }
    }
    for (i = 0; i < BtlChar_GetCount(); i++) BtlCharSnd_StopUnrequestedLoops(BtlChar_Get(i));
}

/* Queues a sound that belongs to no fighter. */
void BtlCharSnd_RequestAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far) {
    BtlCharSnd_Request(pos, -1, kind, near, far, id);
}

/* Queues a sound of bank 4 (kind 0) at the fighter's node 3. */
void BtlCharSnd_PlayCommon(BtlFlagChr *chr, s32 id) {
    Vec4 pos;

    BtlCharApi_GetNodePos(chr->objId, 3, &pos);
    BtlCharSnd_Request(&pos, chr->player, 0, 200.0f, 1500.0f, id);
}

/* The same, heard at full volume anywhere (near = far = 100000). */
void BtlCharSnd_PlayCommonFar(BtlFlagChr *chr, s32 id) {
    Vec4 pos;

    BtlCharApi_GetNodePos(chr->objId, 3, &pos);
    BtlCharSnd_Request(&pos, chr->player, 0, 100000.0f, 100000.0f, id);
}

/* Queues a sound of bank 8 (kind 1) at the fighter's node 3. */
void BtlCharSnd_PlayBank8(BtlFlagChr *chr, s32 id) {
    Vec4 pos;

    BtlCharApi_GetNodePos(chr->objId, 3, &pos);
    BtlCharSnd_Request(&pos, chr->player, 1, 200.0f, 1500.0f, id);
}

/* Queues a sound of the fighter's own bank (kind 2 / 3: banks 0x10 / 0x20) at its node 0x30. */
void BtlCharSnd_PlayOwn(BtlFlagChr *chr, s32 id) {
    Vec4 pos;

    BtlCharApi_GetNodePos(chr->objId, 0x30, &pos);
    BtlCharSnd_Request(&pos, chr->player, own_kind(chr->player), 200.0f, 1500.0f, id);
}

/* Queues a streamed sound (kind 4) at the fighter's node 3. */
void BtlCharSnd_PlayStream(BtlFlagChr *chr, s32 id) {
    Vec4 pos;

    BtlCharApi_GetNodePos(chr->objId, 3, &pos);
    BtlCharSnd_Request(&pos, chr->player, 4, 200.0f, 1500.0f, id);
}

/* Queues a streamed voice line of the fighter's character (kind 5 / 6) at its node 0x30. */
void BtlCharSnd_PlayVoice(BtlFlagChr *chr, s32 line) {
    Vec4 pos;

    BtlCharApi_GetNodePos(chr->objId, 0x30, &pos);
    BtlCharSnd_Request(&pos, chr->player, 5, 200.0f, 1500.0f, line);
}

/* Plays the sounds and voice lines of the technique in use when its animation raises the matching event bits. */
void BtlCharSnd_PlayTechSounds(BtlFlagChr *chr) {
    s32 line = -1;
    s32 se = -1;
    void *obj = BtlChar_GetObj(chr);
    s32 slot = BtlAct_GetCurrentClass(chr);

    if (slot < 0) {
        return;
    }
    switch (slot) {
    case 0:
        se = -1;
        line = 0x53;
        break;
    case 1:
        se = -1;
        line = 0x55;
        break;
    case 2:
        se = 0x2D;
        line = 0x57;
        break;
    case 3:
        se = 0x31;
        line = 0x59;
        break;
    case 4:
        se = 0x35;
        line = 0x5B;
        break;
    }
    if (se >= 0) {
        if (BtlAnim_TestAttr(chr, 0x10000)) {
            BtlCharSnd_PlayOwn(chr, se);
        }
        if (BtlAnim_TestAttr(chr, 0x20000)) {
            if (!(BtlOpp_GetParamWord0(chr) & 0x80) || !(BtlSuper_GetFlags(chr, slot) & 0x20)) {
                BtlCharSnd_PlayOwn(chr, se + 1);
            }
        }
        if (BtlAnim_TestAttr(chr, 0x40000)) {
            if (!(BtlOpp_GetParamWord0(chr) & 0x80) || !(BtlSuper_GetFlags(chr, slot) & 0x40)) {
                BtlCharSnd_PlayOwn(chr, se + 2);
            }
        }
        if (BtlAnim_TestAttr(chr, 0x80000)) {
            BtlCharSnd_PlayOwn(chr, se + 3);
        }
    }
    if (line >= 0) {
        if (BtlAnim_TestAttr(chr, 0x8000)) {
            BtlCharSnd_PlayVoice(chr, line);
            BtlObj_SetSubState(obj, 2, line);
        }
        if (BtlAnim_TestAttr(chr, 0x8000000000)) {
            BtlCharSnd_PlayVoice(chr, line + 1);
            BtlObj_SetSubState(obj, 2, line + 1);
        }
    }
}

/* Cuts short the fighter's one-shot sound (kind, id) unless it was requested again this frame. */
void BtlCharSnd_Stop(BtlFlagChr *chr, s32 kind, s32 id) {
    BtlFlagSound *slot = BtlCharSnd_GetSet(chr)->slot;
    s32 i;

    for (i = 0; i < 4; i++, slot++) {
        if (slot->handle >= 0 && !BtlCharSnd_IsRequested(chr->player, slot->kind, slot->id) && slot->kind == kind &&
            slot->id == id) {
            Snd_StopHandle(slot->handle);
            slot->handle = -1;
        }
    }
}

/* Cuts short a bank 4 sound of the fighter. */
void BtlCharSnd_StopCommon(BtlFlagChr *chr, s32 id) {
    BtlCharSnd_Stop(chr, 0, id);
}

/* Cuts short a sound of the fighter's own bank. */
void BtlCharSnd_StopOwn(BtlFlagChr *chr, s32 id) {
    BtlCharSnd_Stop(chr, own_kind(chr->player), id);
}

/* Sample bank mask of a request kind. (The load only assembles as in the original, with its last instruction in
   the delay slot of the return, because gBtlSndBankMask is defined in this file, above.) */
s32 BtlCharSnd_GetBankMask(s32 kind) {
    return gBtlSndBankMask[kind];
}
