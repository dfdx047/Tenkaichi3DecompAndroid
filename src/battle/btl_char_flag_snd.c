#include "common.h"
#include "battle/btl_char_flag.h"
#include "battle/btl_cam.h"
#include "sys/snd.h"
#include "sys/adx.h"
#include <stdio.h>

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
const s32 gBtlSndBankMask[8] = { 4, 8, 0x10, 0x20, -1, -1, -1, -1 };
#ifdef PORT
extern s32 TagBattle_Active(void),BtlChar_IsDead(BtlFlagChr *);
extern s32 BattleSide_GetStartChara(s32),TagBattle_VoiceBank(s32),Voice_CharaFile(s32,s32);
extern void Port_SeBind(s32,s32),Port_SeActorFile(s32,s32),Port_ActorAdxStopAll(void);
extern s32 Port_SeActorLoop(s32,s32);
extern void *Port_ActorAdx(s32,s32);
extern void ADXT_Stop(void *),ADXT_SetOutVol(void *,s32),ADXT_SetOutPan(void *,s32,s32),ADXT_Pause(void *,s32);
extern void File_StartAdxById(void *,s32);
extern s32 Adx_CalcOutVol(s32,s32),Adx_ConvPan(s32);
extern s32 Snd_QueuePlay(s32,u32,u32,u32,s32,s32),Snd_ScaleVolume(s32,s32),Snd_ScalePan(s32);
static BtlFlagSndReq sTagSndReq[64];
static s32 sTagSndCount;
static s32 sAudioAuditChecks,sAudioAuditFailures;
static void AudioAudit_Check(const char *name,s32 ok){++sAudioAuditChecks;if(!ok)++sAudioAuditFailures;printf("tag audio audit: %s %s\n",name,ok?"PASS":"FAIL");}
static s32 TagSnd_Count(void){return TagBattle_Active()?sTagSndCount:gBtlChars->snd.count;}
static BtlFlagSndReq *TagSnd_At(s32 n){return TagBattle_Active()?&sTagSndReq[n]:&gBtlChars->snd.req[n];}
static void TagSnd_Stream(s32 actor,s32 voice,s32 id,s32 vol,s32 pan){
    void *handle=Port_ActorAdx(actor,voice);if(!handle)return;
    ADXT_SetOutPan(handle,0,Adx_ConvPan(pan));ADXT_SetOutPan(handle,1,Adx_ConvPan(pan));
    ADXT_SetOutVol(handle,Adx_CalcOutVol(voice?4:2,vol));ADXT_Pause(handle,0);File_StartAdxById(handle,id);
}
static s32 TagSnd_Loop(s32 actor,s32 kind,s32 id){
    return kind==2||kind==3?Port_SeActorLoop(actor,id):Snd_IsLoopSe(gBtlSndBankMask[kind],id);
}
static s32 TagSnd_Sample(s32 actor,s32 kind,s32 id,s32 vol,s32 pan){
    s32 handle;
    if(kind==2||kind==3)handle=Snd_QueuePlay(gBtlSndBankMask[kind],id,Snd_ScaleVolume(kind+2,vol),Snd_ScalePan(pan),0,TagSnd_Loop(actor,kind,id));
    else handle=Snd_PlaySeEx(gBtlSndBankMask[kind],id,vol,pan,0);
    Port_SeBind(handle,actor);return handle;
}
#endif


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
    BtlFlagSound *slot = BtlCharSnd_GetLoopSet(chr)->slot;
    s32 i;

    if (chr == NULL) {
        return 0;
    }
    for (i = 0; i < 4; i++, slot++) {
        if (slot->handle >= 0 && id == slot->id && kind == slot->kind) {
            return 1;
        }
    }
    return 0;
}

/* 1 when the request list holds this sound for this owner. */
s32 BtlCharSnd_IsRequested(s32 owner, s32 kind, s32 id) {
    BtlFlagSndReqList *list = &gBtlChars->snd;
    s32 i;

    for (i = 0; i <
#ifdef PORT
         TagSnd_Count()
#else
         list->count
#endif
         ; i++) {
#ifdef PORT
        BtlFlagSndReq *req=TagSnd_At(i);
#else
        BtlFlagSndReq *req=&list->req[i];
#endif
        if (owner == req->p.owner && id == req->p.id && kind == req->p.kind) {
            return 1;
        }
    }
    return 0;
}

/* Queues a positional sound; dropped when four are already queued. */
void BtlCharSnd_Request(Vec4 *pos, s32 owner, s32 kind, f32 near, f32 far, s32 id) {
    BtlFlagSndReqList *list = &gBtlChars->snd;
    BtlFlagSndReq *req;
    s32 ok;

#ifdef PORT
    if(gBtlChars&&TagBattle_Active()){
        if(sTagSndCount<64){req=&sTagSndReq[sTagSndCount++];Vec4_Copy(&req->pos,pos);
            req->p.owner=owner;req->p.kind=kind;req->p.id=id;req->p.near=near;req->p.far=far;}
        return;
    }
#endif
    if (gBtlChars != NULL) {
        ok = list->count < 4;
        req = &list->req[list->count];
        if (ok) {
            Vec4_Copy(&req->pos, pos);
            req->p.owner = owner;
            req->p.kind = kind;
            req->p.id = id;
            req->p.near = near;
            req->p.far = far;
            list->count++;
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
        if (slot->handle >= 0 && (BtlChar_IsDead(chr) || !BtlCharSnd_IsRequested(chr->player, slot->kind, slot->id))) {
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

#ifdef PORT
    /* Actor ownership supplies smooth native distance gain. Camera cuts must
     * not move the listener or bake a permanent volume into a looping sound. */
    if(TagBattle_Active()){*vol=0x80;*pan=0;return;}
#endif
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
#ifdef PORT
    if(TagBattle_Active()){
        sTagSndCount=0;memset(sTagSndReq,0,sizeof(sTagSndReq));Port_ActorAdxStopAll();
        for(s32 a=0;a<BtlChar_GetCount();++a)Port_SeActorFile(a,TagBattle_VoiceBank(BattleSide_GetStartChara(a)));
    }
#endif
    BtlFlagSndReqList *list = &gBtlChars->snd;
    s32 i;
    s32 j;

    for (i = 0; i < 4; i++) {
        BtlFlagSndParam *p = &list->req[i].p;

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
#ifdef PORT
    sTagSndCount=0;memset(sTagSndReq,0,sizeof(sTagSndReq));
#endif
    BtlFlagSndReqList *list = &gBtlChars->snd;
    s32 i;

    for (i = 0; i < 4; i++) {
        BtlFlagSndParam *p = &list->req[i].p;

        p->kind = -1;
        p->id = 0xFF;
        p->owner = -1;
    }
    list->count = 0;
}

/* Plays the queued requests, then stops the looping sounds nobody asked for this frame. */
void BtlCharSnd_PlayRequests(void) {
    BtlFlagSndReqList *list = &gBtlChars->snd;
    s32 i;

    for (i = 0; i <
#ifdef PORT
         TagSnd_Count()
#else
         list->count
#endif
         ; i++) {
#ifdef PORT
        BtlFlagSndReq *req=TagSnd_At(i);
#else
        BtlFlagSndReq *req = &list->req[i];
#endif
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
#ifdef PORT
            if(TagBattle_Active())TagSnd_Stream(owner,0,req->p.id,vol,pan);else
#endif
            StreamSe_Play(owner & 1, req->p.id, vol, pan);
            break;
        case 5:
        case 6:
            BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
#ifdef PORT
            if(TagBattle_Active())TagSnd_Stream(owner,1,Voice_CharaFile(chara,req->p.id),vol,pan);else
#endif
            Voice_PlayCharaEx(owner & 1, chara, req->p.id, vol, pan);
            break;
        case 7:
            BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
#ifdef PORT
            if(TagBattle_Active())TagSnd_Stream(owner,1,req->p.id,vol,pan);else
#endif
            Voice_Play(owner & 1, req->p.id, vol, pan);
            break;
        default:
            if (
#ifdef PORT
                TagBattle_Active()?TagSnd_Loop(owner,req->p.kind,req->p.id):
#endif
                Snd_IsLoopSe(gBtlSndBankMask[req->p.kind], req->p.id)) {
                if (!BtlChar_IsDead(chr) && !BtlCharSnd_IsLoopPlaying(owner, req->p.kind, req->p.id)) {
                    BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
                    handle =
#ifdef PORT
                    TagBattle_Active()?TagSnd_Sample(owner,req->p.kind,req->p.id,vol,pan):
#endif
                    Snd_PlaySeEx(gBtlSndBankMask[req->p.kind], req->p.id, vol, pan, 0);
                    BtlCharSnd_StoreHandle(BtlCharSnd_GetLoopSet(chr), handle, owner, req->p.kind, req->p.id, 0);
                }
            } else {
                BtlCharSnd_CalcVolPan(&req->pos, &vol, &pan, req->p.near, req->p.far);
                handle =
#ifdef PORT
                    TagBattle_Active()?TagSnd_Sample(owner,req->p.kind,req->p.id,vol,pan):
#endif
                    Snd_PlaySeEx(gBtlSndBankMask[req->p.kind], req->p.id, vol, pan, 0);
                BtlCharSnd_StoreHandle(BtlCharSnd_GetSet(chr), handle, owner, req->p.kind, req->p.id, 1);
            }
            break;
        }
    }
    for(i=0;i<BtlChar_GetCount();++i)BtlCharSnd_StopUnrequestedLoops(BtlChar_Get(i));
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
    BtlCharSnd_Request(&pos, chr->player, (chr->player & 1) + 2, 200.0f, 1500.0f, id);
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
    BtlCharSnd_Request(&pos, chr->player, (chr->player & 1) + 5, 200.0f, 1500.0f, line);
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
    BtlCharSnd_Stop(chr, (chr->player & 1) + 2, id);
}

/* Sample bank mask of a request kind. (The load only assembles as in the original, with its last instruction in
   the delay slot of the return, because gBtlSndBankMask is defined in this file, above.) */
s32 BtlCharSnd_GetBankMask(s32 kind) {
    return gBtlSndBankMask[kind];
}

#ifdef PORT
/* Runs only from the explicit hidden combat regression hook. */
s32 BtlCharSnd_RunAudit(void){
    extern void *BtlMember_GetActiveGauge(BtlFlagChr *);
    s32 a,i,loop=-1,n=BtlChar_GetCount(),savedHealth;
    Vec4 pos={0};sAudioAuditChecks=sAudioAuditFailures=0;
    BtlCharSnd_ClearRequests();
    for(a=0;a<n;a++)BtlCharSnd_Request(&pos,a,0,10,160,0);
    AudioAudit_Check("roster_requests_not_limited_to_four",TagSnd_Count()==n&&n>4);
    for(a=0;a<n;a++)AudioAudit_Check("each_actor_request_keeps_its_owner",BtlCharSnd_IsRequested(a,0,0));
    for(i=0;i<100;i++)if(Snd_IsLoopSe(4,i)){loop=i;break;}
    AudioAudit_Check("charge_loop_fixture_available",loop>=0);
    if(loop>=0){
        BtlCharSnd_ClearRequests();
        for(a=0;a<n;a++)BtlCharSnd_Request(&pos,a,0,10,160,loop);
        BtlCharSnd_PlayRequests();Snd_Update();
        for(a=0;a<n;a++)AudioAudit_Check("each_actor_starts_own_loop",BtlCharSnd_IsLoopPlaying(a,0,loop));
        savedHealth=*(s32 *)BtlMember_GetActiveGauge(BtlChar_Get(2));
        *(s32 *)BtlMember_GetActiveGauge(BtlChar_Get(2))=0;
        BtlCharSnd_PlayRequests();Snd_Update();
        AudioAudit_Check("KO_stops_actor_two_despite_stale_request",!BtlCharSnd_IsLoopPlaying(2,0,loop));
        AudioAudit_Check("KO_does_not_stop_other_actor",BtlCharSnd_IsLoopPlaying(0,0,loop));
        *(s32 *)BtlMember_GetActiveGauge(BtlChar_Get(2))=savedHealth;
        BtlCharSnd_ClearRequests();BtlCharSnd_PlayRequests();Snd_Update();
        for(a=0;a<n;a++)AudioAudit_Check("unrequested_loop_stops_for_entire_roster",!BtlCharSnd_IsLoopPlaying(a,0,loop));
    }
    AudioAudit_Check("independent_streams_for_actor_zero_and_two",Port_ActorAdx(0,1)!=Port_ActorAdx(2,1));
    AudioAudit_Check("voice_and_effect_streams_are_distinct",Port_ActorAdx(2,1)!=Port_ActorAdx(2,0));
    printf("tag audio audit: checks=%d failures=%d\n",sAudioAuditChecks,sAudioAuditFailures);return sAudioAuditFailures;
}
#endif
