/*
 * This file is 0x24BBE8..0x250B28: bobj_a.c + bobj_b.c, merged at integration (the second part starts at the
 * separator comment further down; btl_obj.c is in front, bobj_b_b.c follows).
 *
 * Battle object, second part (0x24BBE8..0x24F1F0): the animation player of a battle object, the events and attack
 * volumes it produces, the body volumes, the object transform and node matrices, the flash / fade timers and the
 * face animation. Layouts and the animation file format are in include/battle/bobj_a.h.
 *
 * One frame of an object that BtlObj_UpdateAll steps itself (anim.manual == 0; effect models):
 *   BtlObj_SaveNodePositions, BtlObjAnim_Step, BtlObjAnim_SamplePose, BtlObjAnim_UpdateEvents,
 *   BtlObjPose_CalcMatrices, BtlObj_UpdateChains, BtlObjPose_CalcMatrices again.
 * A fighter's object has anim.manual == 1 (set by BtlObjAnim_Play); the fighter code moves anim.frame itself and
 * calls the same passes.
 */
#include "common.h"
#include "battle/bobj_a.h"

extern void *memset(void *dst, s32 c, u32 n);
extern s32 memcmp(const void *a, const void *b, u32 n);
extern s32 rand(void);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern void Bpe_Decode(void *src, void *dst, s32 *size);
extern f32 BtlObj_GetUnk44FC4(void);
#ifdef PORT /* PC build: the full parameter list (see BtlObjMdl_CopyTexAlpha) */
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
#else
extern void Res_RelocateOffsets(void *out, void *base);
#endif

/* The battle work block: only the 64-bit flag word is read here (0x100 = pause). */
typedef struct BobjBattleWork {
    /* 0x0000 */ u8 unk00[0x19F0];
    /* 0x19F0 */ u64 flags;
} BobjBattleWork;
extern BobjBattleWork *Battle_GetWork(void);

/* VU0 helpers of src/sys: matrix product, matrix copy, inverse of a rigid matrix, xyz copy (w kept),
   dst = a * t + b * (1 - t). */
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
extern void Vec3_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);
/* The VU0 matrix stack: the current matrix lives in vf16..vf19, the stack in VU0 data memory at vi15.
   0x120A98 current = identity, 0x120AB0 push, 0x120AE0 pop n, 0x120B18 empty the stack, 0x120B80 load,
   0x120B98 store, 0x120C18 translate, 0x120F00 rotate by Euler angles (z, x, y), 0x120F88 scale. */
extern void Vu0Cur_LoadIdentity(void);
extern void Vu0Cur_Push(void);
extern void Vu0Cur_PopN(s32 count);
extern void Vu0Cur_ResetStack(void);
extern void Vu0Cur_LoadMtx(Mtx44 *m);
extern void Vu0Cur_StoreMtx(Mtx44 *m);
extern void Vu0Cur_Translate(Vec4 *v);
extern void Vu0Cur_RotateZXY(Vec4 *angles);
extern void Vu0Cur_ScaleDiagUniform(f32 scale);
/* Collision shapes: box grow by a 16-byte point / a 12-byte point, box of a sphere / of an oriented box, box grow
   by a sphere centre, sphere and oriented box constructors. */
extern void StgAabb_SetEmpty(BobjAabb *box);
extern void ColBox_AddPoint(BobjAabb *box, Vec4 *point);
extern void ColBox_AddPointKeepW(BobjAabb *box, f32 *point);
extern void ColObb_Init(BobjObb *box, Vec4 *center, f32 x, f32 y, f32 z);
extern void ColObb_Update(BobjObb *box, Mtx44 *m);
extern void ColBounds_OfSphere(BobjAabb *out, BobjSphere *sphere);
extern void ColBounds_AddSphere(BobjAabb *box, Vec4 *point);
extern void ColBounds_OfObb(BobjAabb *out, BobjObb *box);
extern void ColSphere_Set(BobjSphere *out, Vec4 *center, f32 radius);
/* The second part of this file (formerly bobj_b.c): aliased declarations with this part's types, because the
   second part defines these functions with its own (a cast macro would rewrite the definitions). */
extern BobjBone *BtlObj_FindBound_a(BobjObj *obj, s32 node) __asm__("BtlObj_FindBound");
#define BtlObj_FindBound BtlObj_FindBound_a
extern BobjPart *BtlObj_GetNode_a(BobjObj *obj, s32 node) __asm__("BtlObj_GetNode");
#define BtlObj_GetNode BtlObj_GetNode_a
extern BobjPart *BtlObj_GetNodeByCode_a(BobjObj *obj, s32 code) __asm__("BtlObj_GetNodeByCode");
#define BtlObj_GetNodeByCode BtlObj_GetNodeByCode_a
extern s32 BtlObj_IsJawActive_a(BobjObj *obj) __asm__("BtlObj_IsJawActive");
#define BtlObj_IsJawActive BtlObj_IsJawActive_a
extern void BtlObj_GetJawRot_a(BobjObj *obj, Quat *out) __asm__("BtlObj_GetJawRot");
#define BtlObj_GetJawRot BtlObj_GetJawRot_a

/* 0, 0, 0, 1: the zero translation and the identity rotation. */
extern Vec4 D_002EC2C0;
/* Reference translation of each node (71 x {x, y, z}) that motions are authored against: the first 0x360 bytes of
   this object's .rodata (0x2F2420..0x2F2780), emitted by the INCLUDE_RODATA below the declarations. */
extern f32 D_002F2420[BOBJ_NODE_MAX * 3];
/* Talk patterns {frame, mouth shape} (7 and 5 steps) and jaw rotation tracks {x, y, z, frame} (4 keys each). */
extern BobjTalkStep D_002C6C20[];
extern BobjTalkStep D_002C6C40[];
extern BobjJawKey D_002C6C58[];
extern BobjJawKey D_002C6C98[];
extern BobjJawKey D_002C6CD8[];
extern BobjJawKey D_002C6D18[];
extern BobjJawKey D_002C6D58[];
extern BobjJawKey D_002C6D98[];
extern BobjJawKey D_002C6DD8[];
extern BobjJawKey D_002C6E18[];

INCLUDE_RODATA("asm/nonmatchings/battle/bobj_a", D_002F2420);

#define TRACK_AT(anim, off) ((BobjTrack *)((u16 *)(anim) + (off) * 2))
#define QUAT64(q) ((u64)(q).lo + ((u64)(q).hi << 32))

static f32 retarget_height(BobjObj *obj) {
#ifdef PORT
    /* B14 carries the skeleton scale in part 3 of each costume's model,
       rather than the shared character parameter block. */
    if (PortGame_IsBT4()) {
        const u8 *rig = *(const u8 **)&obj->mdl;
        if (rig != NULL) return *(const f32 *)(rig + 0x74);
    }
#endif
    return obj->mdl.param->height;
}

/* Moves the translation keys of a motion from the skeleton it was made for to this object's skeleton: subtracts
   the source rest translation, scales y by the height ratio and adds this object's rest translation. The source
   is the built-in reference table (useTable), the character's own reference pose (useOwn) or another object. */
void BtlObjAnim_Retarget(BobjAnim *anim, BobjObj *obj, BobjObj *other, s32 useTable, s32 useOwn) {
    BobjBone *bone;
    f32 scale;
    u32 id;
    BobjTrack *trk;
    BobjKey *keys;
    s32 n;
    s32 i;
    BobjBone *src;

    bone = obj->mdl.nodes;
    if (useOwn != 0 && obj->mdl.refPose == NULL) {
        useOwn = 0;
    }
    if (useTable != 0) {
        scale = retarget_height(obj);
    } else if (useOwn != 0) {
        scale = retarget_height(obj) / 1.1f;
    } else {
        scale = retarget_height(obj) / retarget_height(other);
    }
    while (1) {
        id = bone->id;
        if (id < BOBJ_NODE_MAX && anim->track[id] != 0) {
            if (useTable != 0) {
                trk = TRACK_AT(anim, anim->track[id]);
                if (!BOBJ_TRACK_ROT_ONLY(trk)) {
                    n = trk->count;
                    keys = trk->u.key;
                    for (i = 0; i < n; i++) {
                        keys[i].x -= D_002F2420[id * 3];
                        keys[i].y -= D_002F2420[id * 3 + 1];
                        keys[i].z -= D_002F2420[id * 3 + 2];
                        keys[i].y *= scale;
                        keys[i].x += bone->rest.x;
                        keys[i].y += bone->rest.y;
                        keys[i].z += bone->rest.z;
                    }
                }
            } else if (useOwn != 0) {
                trk = TRACK_AT(anim, anim->track[id]);
                if (!BOBJ_TRACK_ROT_ONLY(trk)) {
                    n = trk->count;
                    keys = trk->u.key;
                    for (i = 0; i < n; i++) {
                        keys[i].x -= obj->mdl.refPose[id].x;
                        keys[i].y -= obj->mdl.refPose[id].y;
                        keys[i].z -= obj->mdl.refPose[id].z;
                        keys[i].y *= scale;
                        keys[i].x += bone->rest.x;
                        keys[i].y += bone->rest.y;
                        keys[i].z += bone->rest.z;
                    }
                }
            } else {
                src = BtlObj_FindBound(other, id);
                if (src != NULL) {
                    trk = TRACK_AT(anim, anim->track[id]);
                    if (!BOBJ_TRACK_ROT_ONLY(trk)) {
                        n = trk->count;
                        keys = trk->u.key;
                        for (i = 0; i < n; i++) {
                            keys[i].x -= src->rest.x;
                            keys[i].y -= src->rest.y;
                            keys[i].z -= src->rest.z;
                            keys[i].y *= scale;
                            keys[i].x += bone->rest.x;
                            keys[i].y += bone->rest.y;
                            keys[i].z += bone->rest.z;
                        }
                    }
                }
            }
        }
        if (bone->last != 0) {
            break;
        } else {
            bone = (BobjBone *)((u8 *)bone + bone->next);
        }
    }
}

/* Samples a rotation-only track at a frame. The search starts at the key found last time. */
void BtlObjAnim_SampleRot(BobjTrack *trk, Quat *out, u16 *cursor, f32 frame) {
    Quat next;
    Quat prev;
    s32 n;
    u16 *times;
    BobjPackedQuat *keys;
    BobjPackedQuat *key;
    s32 i;
    s32 j;
    s32 found;
    f32 t;
    u64 qa;
    u64 qb;

    n = trk->count;
    if (BOBJ_TRACK_ROT_ONLY(trk)) {
        times = (u16 *)((u8 *)trk + n * 8 + 4);
    } else {
        times = (u16 *)((u8 *)trk + n * 0x18 + 4);
    }
    if (n == 1) {
        key = trk->u.rot;
        Quat_Unpack(out, QUAT64(key[0]));
        return;
    }
    i = *cursor;
    keys = trk->u.rot;
    if (i >= n - 1) {
        *cursor = 0;
        i = 0;
    }
    found = 0;
    while (1) {
        key = &keys[i];
        for (j = i + 1; j < n; j++) {
            key++;
            if ((f32)times[j - 1] == frame) {
                Quat_Unpack(out, QUAT64(key[-1]));
                *cursor = j - 1;
                found = 1;
                break;
            }
            if ((f32)times[j - 1] < frame && frame < (f32)times[j]) {
                t = (frame - (f32)times[j - 1]) / (f32)(times[j] - times[j - 1]);
                qa = QUAT64(key[0]);
                qb = QUAT64(key[-1]);
                Quat_Unpack(&next, qa);
                Quat_Unpack(&prev, qb);
                Quat_Slerp(out, &prev, &next, t);
                *cursor = j - 1;
                found = 1;
                break;
            }
            if ((f32)times[j] == frame) {
                Quat_Unpack(out, QUAT64(key[0]));
                *cursor = j - 1;
                found = 1;
                break;
            }
        }
        if (found) {
            return;
        }
        if (*cursor == 0) {
            Vec4_Copy((Vec4 *)out, &D_002EC2C0);
            return;
        } else {
            *cursor = 0;
            i = 0;
        }
    }
}

/* Samples a full (translation + rotation) track at a frame. */
/* A key's frame as a float. The original had a helper like this one (inlined): each use of the previous key's
   frame computes `key - 1` and loads the frame again, which only an inlined argument reproduces. */
static inline f32 BobjKey_GetFrame(BobjKey *key) {
    return (f32)key->frame;
}

void BtlObjAnim_SamplePosRot(BobjTrack *trk, Vec4 *pos, Quat *rot, u16 *cursor, f32 frame) {
    Quat next;
    Quat prev;
    s32 n;
    BobjKey *keys;
    BobjKey *a;
    BobjKey *b;
    s32 i;
    s32 j;
    s32 found;
    f32 t;
    u64 qa;
    u64 qb;

    n = trk->count;
    if (n == 1) {
        b = trk->u.key;
        pos->x = b->x;
        pos->y = b->y;
        pos->z = b->z;
        pos->w = 1.0f;
        Quat_Unpack(rot, QUAT64(b->rot));
        return;
    }
    i = *cursor;
    if (i >= n - 1) {
        *cursor = 0;
        i = 0;
    }
    keys = trk->u.key;
    found = 0;
    while (1) {
        b = &keys[i];
        for (j = i + 1; j < n; j++) {
            b++;
            a = b - 1;
            if (BobjKey_GetFrame(b - 1) == frame) {
                pos->x = a->x;
                pos->y = a->y;
                pos->z = a->z;
                pos->w = 1.0f;
                Quat_Unpack(rot, QUAT64(b[-1].rot));
                *cursor = j - 1;
                found = 1;
                break;
            }
            if (BobjKey_GetFrame(b - 1) < frame && frame < BobjKey_GetFrame(b)) {
                t = (frame - BobjKey_GetFrame(b - 1)) / (f32)(b->frame - (b - 1)->frame);
                pos->x = (b->x - a->x) * t + a->x;
                pos->y = (b->y - a->y) * t + a->y;
                pos->z = (b->z - a->z) * t + a->z;
                pos->w = 1.0f;
                qa = QUAT64(b->rot);
                qb = QUAT64(b[-1].rot);
                Quat_Unpack(&next, qa);
                Quat_Unpack(&prev, qb);
                Quat_Slerp(rot, &prev, &next, t);
                *cursor = j - 1;
                found = 1;
                break;
            }
            if (BobjKey_GetFrame(b) == frame) {
                pos->x = b->x;
                pos->y = b->y;
                pos->z = b->z;
                pos->w = 1.0f;
                Quat_Unpack(rot, QUAT64(b->rot));
                *cursor = j - 1;
                found = 1;
                break;
            }
        }
        if (found) {
            return;
        }
        if (*cursor == 0) {
            Vec4_Copy((Vec4 *)rot, &D_002EC2C0);
            Vec4_Copy(pos, &D_002EC2C0);
            return;
        } else {
            *cursor = 0;
            i = 0;
        }
    }
}

/* Returns motion `anim` of an object: ids below 0x19E are decompressed into buf, the others (the model's own
   motions, which follow in the same table) are returned in place. */
BobjAnim *BtlObjAnim_Load(BobjObj *obj, void *buf, s32 anim, s32 *size) {
    void **tbl = (void **)&obj->mdl;
    void *ret;

    if (anim >= BOBJ_ANIM_MAX) {
        ret = (tbl + anim)[0x2A]; /* mdl->anims[anim]: runs on into modelAnims[] and the tables behind it */
        *size = 1;
    } else if (obj->mdl.anims[anim] != NULL) {
#ifdef PORT
        if(PortGame_IsBT4()) {
            s32 bytes=*(s32 *)obj->mdl.anims[anim];
            if(bytes<148 || bytes>PortGame_AnimationCapacity()) {*size=0;return NULL;}
            buf=PortGame_AnimationBuffer(buf);
        }
#endif
        Bpe_Decode(obj->mdl.anims[anim], buf, size);
#ifdef PORT
        if(PortGame_IsBT4() && PortGame_CheckAnimation(buf,*size)<0) return NULL;
#endif
        ret = buf;
    } else {
        ret = NULL;
    }
    return ret;
}

/* Initial player state: default step, identity extra rotations, volume scale 1. */
void BtlObjAnim_Init(BobjObj *obj) {
    BobjHit *hit = &obj->hit;
    BobjAnimPlayer *ap = &obj->anim;

    ap->step = BtlObj_GetUnk44FC4();
    Quat_SetIdentity(&obj->look.node2);
    Quat_SetIdentity(&obj->look.node2E);
    Quat_SetIdentity(&obj->look.node2F);
    hit->scale = 1.0f;
}

/* Starts a blend of `seconds` from the current pose: remembers every node's translation and rotation. */
void BtlObjAnim_StartBlend(BobjObj *obj, f32 seconds) {
    BobjAnimPlayer *ap = &obj->anim;
    BobjBone *bone;
    BobjPart *part;

    if (!(1.0f / 30.0f < seconds)) {
        ap->blend = 0.0f;
        ap->blendStep = 1.0f;
        return;
    }
    ap->blend = 1.0f;
    ap->blendStep = 1.0f / (seconds * 30.0f);
    bone = obj->mdl.nodes;
    for (;;) {
        part = BtlObj_GetNode(obj, bone->id);
        Vec4_Copy(&part->fromPos, &part->pos);
        Vec4_Copy((Vec4 *)&part->fromRot, (Vec4 *)&part->rot);
        if (bone->last != 0) {
            break;
        }
        bone = (BobjBone *)((u8 *)bone + bone->next);
    }
}

/* Zeroes the chosen axes of every translation key of the root node's track. */
void BtlObjAnim_ZeroRootAxes(BobjAnim *anim, s32 x, s32 y, s32 z) {
    BobjTrack *trk;
    s32 n;
    BobjKey *keys;
    s32 i;

    if (anim->track[0] == 0) {
        return;
    }
    trk = TRACK_AT(anim, anim->track[0]);
    if (BOBJ_TRACK_ROT_ONLY(trk)) {
        return;
    }
    n = trk->count;
    keys = trk->u.key;
    if (x != 0) {
        for (i = 0; i < n; i++) {
            keys[i].x = 0.0f;
        }
    }
    if (y != 0) {
        for (i = 0; i < n; i++) {
            keys[i].y = 0.0f;
        }
    }
    if (z != 0) {
        for (i = 0; i < n; i++) {
            keys[i].z = 0.0f;
        }
    }
}

/* Makes the root node's track start at the origin: subtracts the first key's translation from every key. */
void BtlObjAnim_RebaseRoot(BobjAnim *anim) {
    BobjVec3 first;
    BobjTrack *trk;
    BobjKey *keys;
    s32 n;
    s32 i;

    if (anim->track[0] == 0) {
        return;
    }
    trk = TRACK_AT(anim, anim->track[0]);
    if (BOBJ_TRACK_ROT_ONLY(trk)) {
        return;
    }
    keys = trk->u.key;
    n = trk->count;
    first.x = keys[0].x;
    first.y = keys[0].y;
    first.z = keys[0].z;
    for (i = 0; i < n; i++) {
        keys[i].x -= first.x;
        keys[i].y -= first.y;
        keys[i].z -= first.z;
    }
}

/* Replaces every rotation key of a node's track by the packed identity. */
void BtlObjAnim_ClearNodeRot(BobjAnim *anim, s32 node) {
    BobjTrack *trk;
    s32 n;
    s32 i;

    if (anim->track[node] == 0) {
        return;
    }
    trk = TRACK_AT(anim, anim->track[node]);
    if (BOBJ_TRACK_ROT_ONLY(trk)) {
        BobjPackedQuat *q;

        n = trk->count;
        q = trk->u.rot;
        for (i = 0; i < n; i++) {
            q[i].lo = 0xFFF7FFFF;
            q[i].hi = 0x37FFFF7F;
        }
    } else {
        BobjKey *keys;

        n = trk->count;
        keys = trk->u.key;
        for (i = 0; i < n; i++) {
            keys[i].rot.lo = 0xFFF7FFFF;
            keys[i].rot.hi = 0x37FFFF7F;
        }
    }
}

/* Fills every node's local translation and rotation for the current frame: layer 0, then layer 1 mixed in by
   anim.mix, then the blend from the pose stored by BtlObjAnim_StartBlend (not for node 0). */
void BtlObjAnim_SamplePose(BobjObj *obj) {
    BobjAnimLayer *layer[2];
    Vec4 pos;
    Quat rot;
    BobjAnimPlayer *ap;
    BobjBone *bone;
    BobjPart *part;
    BobjTrack *trk;
    u32 id;
    f32 w;
    f32 frame;
    Vec4 *dst;

    ap = &obj->anim;
    layer[1] = &obj->anim.layer[1];
    layer[0] = &ap->layer[0];
    if (ap->layer[0].data == NULL) {
        return;
    }
    w = 0.0f;
    bone = obj->mdl.nodes;
    if (0.0f < ap->blend) {
        w = ap->blend * ap->blend * ap->blend * -2.0f + ap->blend * ap->blend * 3.0f;
    }
    while (1) {
        id = bone->id;
        part = BtlObj_GetNode(obj, id);
        part->flags &= ~1;
        if (id < BOBJ_NODE_MAX) {
            if (layer[0]->data->track[id] != 0) {
                trk = TRACK_AT(layer[0]->data, layer[0]->data->track[id]);
                if (BOBJ_TRACK_ROT_ONLY(trk)) {
                    BtlObjAnim_SampleRot(trk, &part->rot, &layer[0]->cursor[id], ap->frame);
                    dst = &part->pos;
                    Vec4_Copy(dst, &bone->rest);
                } else {
                    dst = &part->pos;
                    BtlObjAnim_SamplePosRot(trk, dst, &part->rot, &layer[0]->cursor[id], ap->frame);
                }
            } else {
                dst = &part->pos;
                Vec4_Copy(dst, &bone->rest);
                part->rot.x = 0.0f;
                part->rot.y = 0.0f;
                part->rot.z = 0.0f;
                part->rot.w = 1.0f;
            }
            if (layer[1]->data != NULL && 0.0f < ap->mix) {
                frame = ap->frame;
                if (layer[0]->length != layer[1]->length) {
                    frame = frame * (layer[1]->length / layer[0]->length);
                    if (layer[1]->length < frame) {
                        frame = layer[1]->length;
                    }
                }
                if (layer[1]->data->track[id] != 0) {
                    trk = TRACK_AT(layer[1]->data, layer[1]->data->track[id]);
                    if (BOBJ_TRACK_ROT_ONLY(trk)) {
                        BtlObjAnim_SampleRot(trk, &rot, &layer[1]->cursor[id], frame);
                        Vec4_Copy(&pos, &bone->rest);
                    } else {
                        BtlObjAnim_SamplePosRot(trk, &pos, &rot, &layer[1]->cursor[id], frame);
                    }
                    Vec4_Lerp(dst, &pos, dst, ap->mix);
                    part->pos.w = 1.0f;
                    Quat_Slerp(&part->rot, &part->rot, &rot, ap->mix);
                }
            }
            if (id != 0 && 0.0f < w) {
                Vec4_Lerp(dst, &part->fromPos, dst, w);
                part->pos.w = 1.0f;
                Quat_Slerp(&part->rot, &part->rot, &part->fromRot, w);
            }
        } else {
            Vec4_Copy(&part->pos, &bone->rest);
            part->rot.x = 0.0f;
            part->rot.y = 0.0f;
            part->rot.z = 0.0f;
            part->rot.w = 1.0f;
        }
        if (bone->last != 0) {
            break;
        } else {
            bone = (BobjBone *)((u8 *)bone + bone->next);
        }
    }
}

/* Reads the events of layer 0 for this frame: the hit window that is open (its volume and nodes), the number of
   hit windows, and up to eight distinct events whose frame lies in (previous frame, frame]. */
void BtlObjAnim_UpdateEvents(BobjObj *obj) {
    BobjHit *hit;
    s32 begin;
    BobjAnimPlayer *ap;
    u32 arg;
    s32 shape;
    s32 begins;
    s32 ends;
    BobjAnim *data;
    s32 sphere;
    BobjAnimEvent *events;
    s32 i;
    s32 prev;
    s32 cur;
    s64 attr;
    s32 end;
    s32 n;
    s32 k;

    hit = &obj->hit;
    begin = 0;
    ap = &obj->anim;
    shape = 0;
    arg = 0;
    hit->hitIndex = -1;
    begins = 0;
    hit->nodeMask = 0;
    ends = 0;
    hit->hitCount = 0;
    data = ap->layer[0].data;
    sphere = 0;
    if (data == NULL) {
        return;
    }
    events = data->event;
    cur = ap->frame + 1.0f / 60.0f;
    prev = ap->prevFrame + 1.0f / 60.0f;
    if (cur == prev) {
        prev = prev - 1;
    }
    for (i = 0; i < data->eventCount; i++) {
        attr = (u64)events[i].attrLo + ((u64)events[i].attrHi << 32);
        if ((s32)(attr & BOBJ_EV_HIT_BEGIN)) {
            begin = events[i].frame;
            arg = events[i].arg;
            sphere = (s32)(attr >> 32) & 1;
            shape = events[i].shape;
            begins++;
        }
        if (attr & BOBJ_EV_HIT_END) {
            ends++;
            end = events[i].frame;
            if (begins == ends) {
                hit->hitCount++;
                if (hit->hitIndex < 0) {
                    if (!(cur < begin) && prev < end) {
                        BobjAnimShape *s = (BobjAnimShape *)((u32 *)data + shape);

                        hit->x = s->x;
                        hit->y = s->y;
                        hit->z = s->z;
                        hit->r0 = s->r0 * hit->scale;
                        hit->r1 = s->r1 * hit->scale;
                        hit->r2 = s->r2 * hit->scale;
                        hit->hitIndex = ends - 1;
                        hit->nodeMask = arg;
                        hit->sphere = sphere;
                    }
                }
            }
        }
    }
    if (hit->hitCount > 0 && hit->hitIndex < 0) {
        hit->hitIndex = -1;
        for (i = 0; i < data->eventCount; i++) {
            attr = (u64)events[i].attrLo + ((u64)events[i].attrHi << 32);
            if ((s32)(attr & BOBJ_EV_HIT_BEGIN)) {
                if (cur < events[i].frame) {
                    break;
                }
                hit->hitIndex--;
            }
        }
    }
    n = 0;
    for (i = 0; i < data->eventCount; i++) {
        if (prev > 0) {
            if (cur < events[i].frame) {
                continue;
            }
            if (!(prev < events[i].frame)) {
                continue;
            }
        } else {
            if (cur < events[i].frame) {
                continue;
            }
            if (events[i].frame < prev) {
                continue;
            }
        }
        if (n >= BOBJ_EVENT_MAX) {
            continue;
        }
        attr = (u64)events[i].attrLo + ((u64)events[i].attrHi << 32);
        for (k = 0; k < n; k++) {
            if (hit->event[k] == attr) {
                hit->eventArg[k] |= events[i].arg;
                break;
            }
        }
        if (k == n) {
            hit->event[n] = attr;
            hit->eventArg[n] = events[i].arg;
            n++;
        }
    }
    hit->eventCount = n;
    if (BtlObjAnim_TestEvent(obj, 0x400000000)) {
        hit->node = BtlObjAnim_MaskToNode(BtlObjAnim_GetEventArg(obj, 0x400000000));
    }
}

/* Sets the frames advanced per update. */
void BtlObjAnim_SetStep(BobjObj *obj, f32 step) {
    obj->anim.step = step;
}

/* Starts motion `anim` on a layer. Layer 0 with reset restarts at frame 0; without it the frame is kept
   (clamped to the new length). The owner steps the object from now on (anim.manual). */
void BtlObjAnim_Play(BobjObj *obj, s32 layer, s32 anim, s32 reset) {
    s32 size;
    BobjAnimPlayer *ap = &obj->anim;
    BobjAnimLayer *l;
    BobjAnim *data;
    BobjWork *work;

    work = obj->work;
    if (layer == 0) {
        l = &ap->layer[0];
        l->data = BtlObjAnim_Load(obj, work->layer[0], anim, &size);
    } else {
        data = BtlObjAnim_Load(obj, work->layer[1], anim, &size);
        l = &obj->anim.layer[1];
        l->data = data;
    }
    if (l->data == NULL) {
        return;
    }
    ap->manual = 1;
    l->id = anim;
    l->length = l->data->length;
    if (layer == 0) {
        obj->hit.node = 3;
        if (reset != 0) {
            ap->frame = 0.0f;
            ap->prevFrame = 0.0f;
        } else {
            if (ap->frame > l->length) {
                ap->frame = l->length;
            }
            if (ap->prevFrame > l->length) {
                ap->prevFrame = l->length;
            }
        }
    }
    memset(l->cursor, 0, sizeof(l->cursor));
    if (obj->work->retarget != 0) {
        BtlObjAnim_Retarget(l->data, obj, obj, 0, 1);
    }
}

/* Starts motion `anim` of ANOTHER object's table on layer 0 (frame 0), retargeted to this object. */
void BtlObjAnim_PlayFrom(BobjObj *obj, BobjObj *other, s32 anim, s32 useTable) {
    s32 size;
    BobjAnimPlayer *ap;
    BobjAnim *data;

    ap = &obj->anim;
    ap->layer[0].data = BtlObjAnim_Load(other, obj->work->layer[0], anim, &size);
    obj->hit.node = 3;
    data = ap->layer[0].data;
    if (data != NULL) {
        ap->manual = 1;
        ap->layer[0].id = anim;
        ap->layer[0].length = data->length;
        ap->frame = 0.0f;
        ap->prevFrame = 0.0f;
        memset(ap->layer[0].cursor, 0, sizeof(ap->layer[0].cursor));
        BtlObjAnim_Retarget(ap->layer[0].data, obj, other, useTable, 0);
    }
}

/* One decoded motion buffer. */
typedef struct BobjLayerBuf {
    u8 b[BOBJ_WORK_LAYER_SIZE];
} BobjLayerBuf;

/* Layer 1 becomes layer 0: copies its decoded motion, id and length, and restarts at frame 0. */
void BtlObjAnim_PromoteLayer(BobjObj *obj) {
    BobjAnimPlayer *ap = &obj->anim;

#ifdef PORT
    { extern void *memcpy(void *,const void *,u32);
      memcpy(ap->layer[0].data,ap->layer[1].data,PortGame_AnimationCapacity()); }
#else
    *(BobjLayerBuf *)ap->layer[0].data = *(BobjLayerBuf *)ap->layer[1].data;
#endif
    ap->layer[0].id = ap->layer[1].id;
    ap->layer[0].length = ap->layer[1].length;
    ap->frame = 0.0f;
    ap->prevFrame = 0.0f;
    memset(ap->layer[0].cursor, 0, sizeof(ap->layer[0].cursor));
}

/* Starts motion `anim` (0..0x19D) on layer 0 and lets BtlObj_UpdateAll step it in the given mode. */
void BtlObjAnim_PlayAuto(BobjObj *obj, s32 anim, s32 mode) {
    BobjAnimPlayer *ap = &obj->anim;
    s32 m;

    if (anim >= BOBJ_ANIM_MAX) {
        return;
    }
    if (anim < 0) {
        return;
    }
    BtlObjAnim_Play(obj, 0, anim, 1);
    m = ap->layer[0].data != NULL ? mode : 0;
    ap->manual = 0;
    ap->mode = m;
}

/* Starts one of the eight motions of the model file itself, in the given mode. */
void BtlObjAnim_PlayModel(BobjObj *obj, s32 anim, s32 mode) {
    BobjMdl *mdl = &obj->mdl;
    BobjAnimPlayer *ap = &obj->anim;

    if (anim >= 8) {
        return;
    }
    if (anim >= 0) {
        ap->layer[0].data = mdl->modelAnims[anim];
        if (ap->layer[0].data != NULL) {
            ap->layer[0].id = anim;
            ap->frame = 0.0f;
            ap->prevFrame = 0.0f;
            ap->layer[0].length = ap->layer[0].data->length;
            ap->mode = mode;
            memset(ap->layer[0].cursor, 0, sizeof(ap->layer[0].cursor));
        } else {
            ap->mode = 0;
        }
    }
}

/* Advances the frame of a self-stepped object by anim.step: once (stops on the last frame) or looping. */
void BtlObjAnim_Step(BobjObj *obj) {
    BobjAnimPlayer *ap = &obj->anim;
    extern f32 TagGiant_AnimationStep(void *);
    f32 step=ap->step*TagGiant_AnimationStep(obj);

    switch (ap->mode) {
    case BOBJ_PLAY_ONCE:
        ap->prevFrame = ap->frame;
        ap->frame = ap->frame + step;
        if (ap->layer[0].length <= ap->frame) {
            ap->mode = BOBJ_PLAY_NONE;
            ap->frame = ap->layer[0].length;
            ap->prevFrame = ap->layer[0].length;
        }
        break;
    case BOBJ_PLAY_NONE:
        break;
    case BOBJ_PLAY_LOOP:
        ap->prevFrame = ap->frame;
        ap->frame = ap->frame + step;
        if (ap->layer[0].length <= ap->frame) {
            ap->frame = 0.0f;
            ap->prevFrame = 0.0f;
        }
        break;
    }
}

/* Whether an event with any of the mask's attribute bits fell in this frame's interval. */
s32 BtlObjAnim_TestEvent(BobjObj *obj, u64 mask) {
    u64 bits = 0;
    s32 i;

    for (i = 0; i < obj->hit.eventCount; i++) {
        bits |= obj->hit.event[i] & mask;
    }
    return bits != 0;
}

/* OR of the arguments of this frame's events that have any of the mask's attribute bits. */
u32 BtlObjAnim_GetEventArg(BobjObj *obj, u64 mask) {
    u32 arg = 0;
    s32 i;

    for (i = 0; i < obj->hit.eventCount; i++) {
        if (obj->hit.event[i] & mask) {
            arg |= obj->hit.eventArg[i];
        }
    }
    return arg;
}

/* Lowest set bit of an event argument -> model node id (the order of the tests is the priority). */
s32 BtlObjAnim_MaskToNode(u32 mask) {
    if (mask & 1) {
        return 3;
    }
    if (mask & 2) {
        return 4;
    }
    if (mask & 4) {
        return 10;
    }
    if (mask & 8) {
        return 11;
    }
    if (mask & 0x40) {
        return 0x13;
    }
    if (mask & 0x80) {
        return 0x14;
    }
    if (mask & 0x100) {
        return 0x15;
    }
    if (mask & 0x10) {
        return 0xE;
    }
    if (mask & 0x20) {
        return 0xF;
    }
    if (mask & 0x200) {
        return 0x21;
    }
    if (mask & 0x400) {
        return 0x22;
    }
    if (mask & 0x800) {
        return 0x23;
    }
    if (mask & 0x1000) {
        return 0x2E;
    }
    if (mask & 0x2000) {
        return 0x40;
    }
    if (mask & 0x4000) {
        return 0x41;
    }
    if (mask & 0x8000) {
        return 0;
    }
    if (mask & 0x10000) {
        return 0x1F;
    }
    if (mask & 0x20000) {
        return 0x2D;
    }
    if (mask & 0x40000) {
        return 0x36;
    }
    return 0;
}

/* Scans every event of a layer's motion that has any of the mask's attribute bits, relative to the current frame:
   what 0 = frame of the first, 1 = frame of the first one still ahead, 2 = frame of the last, 3 = how many,
   4 = how many still ahead, 5 = how many reached, 6 / 7 / 8 = the argument of 0 / 1 / 2. Frames are -1 and
   arguments 0 when there is none. */
s32 BtlObjAnim_QueryEvent(BobjObj *obj, u64 mask, s32 layer, u32 what) {
    BobjAnimPlayer *ap = &obj->anim;
    s32 first = -1;
    s32 next = -1;
    s32 last = -1;
    u32 firstArg = 0;
    u32 nextArg = 0;
    BobjAnim *data = ap->layer[layer].data;
    u32 lastArg = 0;
    s32 count = 0;
    s32 ahead = 0;
    s32 reached = 0;
    BobjAnimEvent *ev;
    s32 i;

    if (data == NULL) {
        return 0;
    }
    {
    BobjAnimEvent *events = data->event;
    for (i = 0; i < data->eventCount; i++) {
        if (((u64)events[i].attrLo + ((u64)events[i].attrHi << 32)) & mask) {
            if (first < 0) {
                first = events[i].frame;
                firstArg = events[i].arg;
            }
            if (next < 0) {
                if (ap->frame < (f32)events[i].frame) {
                    next = events[i].frame;
                    nextArg = events[i].arg;
                }
            }
            last = events[i].frame;
            count++;
            lastArg = events[i].arg;
            if (ap->frame < (f32)last) {
                ahead++;
            }
            if ((f32)last <= ap->frame) {
                reached++;
            }
        }
    }
    }
    switch (what) {
    case 0:
        return first;
    case 1:
        return next;
    case 2:
        return last;
    case 3:
        return count;
    case 4:
        return ahead;
    case 5:
        return reached;
    case 6:
        return firstArg;
    case 7:
        return nextArg;
    case 8:
        return lastArg;
    }
    return 0;
}

/* Returns the play mode of a self-stepped object (0 = finished / none). */
s32 BtlObjAnim_GetMode(BobjObj *obj) {
    return obj->anim.mode;
}

/* Whether the object has body volumes: a fighter (type 0) whose model carries body data. */
s32 BtlObjBody_Exists(BobjObj *obj) {
    if (obj->mdl.body == NULL) {
        return 0;
    }
    return obj->type == 0;
}

/* Swaps the two body sphere records (not while the battle is paused). */
void BtlObjBody_Swap(BobjBody *body) {
    if (!(Battle_GetWork()->flags & 0x100)) {
        s32 old = body->index;

        body->cur = &body->sphere[old];
        body->prev = &body->sphere[old ^ 1];
        body->index = old ^ 1;
    }
}

/* Places an oriented box: stores the matrix, its rows as axes and centre, and the eight world corners. */
void BtlObjObb_SetMtx(BobjObb *box, Mtx44 *m) {
    Mtx_Copy(&box->mtx, m);
    box->axis[0].x = box->mtx.m[0][0];
    box->axis[0].y = box->mtx.m[0][1];
    box->axis[0].z = box->mtx.m[0][2];
    box->axis[1].x = box->mtx.m[1][0];
    box->axis[1].y = box->mtx.m[1][1];
    box->axis[1].z = box->mtx.m[1][2];
    box->axis[2].x = box->mtx.m[2][0];
    box->axis[2].y = box->mtx.m[2][1];
    box->axis[2].z = box->mtx.m[2][2];
    box->center.x = box->mtx.m[3][0];
    box->center.y = box->mtx.m[3][1];
    box->center.z = box->mtx.m[3][2];
    Mtx_MulVec4(&box->world[0], &box->mtx, &box->local[0]);
    Mtx_MulVec4(&box->world[1], &box->mtx, &box->local[1]);
    Mtx_MulVec4(&box->world[2], &box->mtx, &box->local[2]);
    Mtx_MulVec4(&box->world[3], &box->mtx, &box->local[3]);
    Mtx_MulVec4(&box->world[4], &box->mtx, &box->local[4]);
    Mtx_MulVec4(&box->world[5], &box->mtx, &box->local[5]);
    Mtx_MulVec4(&box->world[6], &box->mtx, &box->local[6]);
    Mtx_MulVec4(&box->world[7], &box->mtx, &box->local[7]);
}

/* Stores a body part's world centre. */
void BtlObjBody_SetCenter(Vec4 *dst, Vec4 *src) {
    Vec4_Copy(dst, src);
}

/* Rebuilds the body sphere: centred on node 0 in x / z, at node 0x11's height minus 1.2, never lower than
   radius + 1.2 above node 0 (+Y is down). */
void BtlObjBody_UpdateSphere(BobjObj *obj, BobjBody *body) {
    BobjSphere *s = body->cur;
    BobjPart *part;
    f32 y;

    part = BtlObj_GetNode(obj, 0);
    if (part != NULL) {
        y = part->mtx.m[3][1];
        Vec4_Copy(&s->center, (Vec4 *)part->mtx.m[3]);
        part = BtlObj_GetNode(obj, 0x11);
        if (part != NULL) {
            s->radius = body->radius;
            s->radiusSq = body->radius * body->radius;
            s->center.y = part->mtx.m[3][1] - 1.2f;
            if (y - 1.2f < s->center.y + s->radius) {
                s->center.y = y - s->radius - 1.2f;
            }
        }
    }
}

/* Rebuilds every body part's box from its node matrix, and the two bounding boxes of the body. */
void BtlObjBody_UpdateParts(BobjBody *body) {
    Vec4 local;
    Vec4 center;
    Mtx44 m;
    BobjBodyPart *part;

    part = body->parts;
    StgAabb_SetEmpty(&body->box);
    StgAabb_SetEmpty(&body->centerBox);
    for (;;) {
        Vec3_Sub(&local, &part->offset, &part->bone->origin);
        Mtx_MulVec4(&center, &part->part->mtx, &local);
        Mtx_Copy(&m, &part->part->mtx);
        Vec4_Copy((Vec4 *)m.m[3], &center);
        BtlObjObb_SetMtx(&part->box, &m);
        BtlObjBody_SetCenter(&part->center, (Vec4 *)m.m[3]);
        ColBox_AddPoint(&body->box, &part->box.world[0]);
        ColBox_AddPoint(&body->box, &part->box.world[1]);
        ColBox_AddPoint(&body->box, &part->box.world[2]);
        ColBox_AddPoint(&body->box, &part->box.world[3]);
        ColBox_AddPoint(&body->box, &part->box.world[4]);
        ColBox_AddPoint(&body->box, &part->box.world[5]);
        ColBox_AddPoint(&body->box, &part->box.world[6]);
        ColBox_AddPoint(&body->box, &part->box.world[7]);
        ColBounds_AddSphere(&body->centerBox, &part->center);
        if (part->flags & 1) {
            break;
        }
        part++;
    }
}

/* Clears the body block and fills it from the model: sizes, sphere radius, and each body part's node. */
void BtlObjBody_Init(BobjObj *obj) {
    BobjBody *body = &obj->body;
    BobjMdl *mdl = &obj->mdl;
    BobjParam *param = mdl->param;
/* The original walks the part list in the register that held `body`: the match needs one variable for both. */
#define bp ((BobjBodyPart *)body)

    memset(body, 0, sizeof(BobjBody));
    body->size[0] = mdl->model->size[0];
    body->size[1] = mdl->model->size[1];
    body->size[2] = mdl->model->size[2];
    body->size[3] = mdl->model->size[3];
    if (param != NULL) {
        if (param->radius <= 0.0f) {
            body->radius = body->size[3] + body->size[3];
        } else {
            body->radius = param->radius;
        }
    } else {
        body->radius = 4.1f;
    }
    if (BtlObjBody_Exists(obj)) {
        body->unkA0 = mdl->body->unk04;
        body->unkB8 = 4.1f;
        body->parts = mdl->body->part;
        body = (BobjBody *)body->parts;
        for (;;) {
            bp->part = BtlObj_GetNode(obj, bp->node);
            bp->bone = BtlObj_FindBound(obj, bp->node);
            if (bp->flags & 1) {
                break;
            }
            body = (BobjBody *)(bp + 1);
        }
        BtlObjBody_Reset(obj);
    }
#undef bp
    /* Spawn spacing asks for dimensions before the first pose update. */
    { extern void TagGiant_UpdateBody(void *); TagGiant_UpdateBody(obj); }
}

/* Rebuilds the body volumes from the node matrices: the sphere (parts == 0) or the part boxes. Declared with a
   result it never sets: the calls are not tail calls in the original. */
s32 BtlObjBody_Update(BobjObj *obj, s32 parts) {
    extern void TagGiant_UpdateBody(void *);
    extern f32 TagGiant_ObjectScale(void *);
    f32 scale=TagGiant_ObjectScale(obj);
    TagGiant_UpdateBody(obj);
    if (BtlObjBody_Exists(obj)) {
        if (parts == 0) {
            BtlObjBody_UpdateSphere(obj, &obj->body);
        } else {
            BtlObjBody_UpdateParts(&obj->body);
            if(obj->mdl.param && ((u8 *)obj->mdl.param)[2]==4) {
                BobjBodyPart *p=obj->body.parts;
                for(;;p++) {
                    s32 j;
                    /* Broad bounds already contain scaled world corners. The
                       narrow OBB solver needs unit axes and scaled extents. */
                    for(j=0;j<3;j++) {
                        p->box.axis[j].x/=scale;
                        p->box.axis[j].y/=scale;
                        p->box.axis[j].z/=scale;
                    }
                    p->box.unk10.x=p->box.local[0].x*scale;
                    p->box.unk10.y=p->box.local[0].y*scale;
                    p->box.unk10.z=p->box.local[0].z*scale;
                    if(p->flags&1) break;
                }
            }
        }
    }
}

/* Start of a fighter's collision frame: the stage contact word moves to "previous" and is cleared, and the body
   sphere records are swapped. */
void BtlObjBody_BeginFrame(BobjObj *obj) {
    if (BtlObjBody_Exists(obj)) {
        BobjWork *work = obj->work;

        if (work != NULL) {
            u32 contact = work->contact;

            work->contact = 0;
            work->prevContact = contact;
        }
        BtlObjBody_Swap(&obj->body);
    }
}

/* Marks the body as teleported (body.reset) and swaps the sphere records. */
void BtlObjBody_Reset(BobjObj *obj) {
    if (BtlObjBody_Exists(obj)) {
        BobjBody *body = &obj->body;
        s32 old = body->index;

        body->reset = 1;
        body->index = old ^ 1;
        body->prev = &body->sphere[old];
        body->cur = &body->sphere[body->index];
    }
}

/* Puts both body sphere records at a position (no sweep from the old one). */
void BtlObjBody_Warp(BobjObj *obj, Vec4 *pos) {
    if (BtlObjBody_Exists(obj)) {
        Vec4_Copy(&obj->body.sphere[0].center, pos);
        Vec4_Copy(&obj->body.sphere[1].center, pos);
        BtlObjBody_Reset(obj);
    }
}

/* Rebuilds the body part boxes and this frame's attack volumes: one sphere or oriented box per attack node of
   the open hit window, plus their bounding box. Clears the top byte of the stage contact word. */
void BtlObjHit_BuildVolumes(BobjObj *obj) {
    BobjAabb box;
    Vec4 offset;
    Vec4 center;
    Mtx44 m;
    BobjHit *hit;
    BobjWork *work;
    s32 i;
    BobjPart *part;
    extern f32 TagGiant_ObjectScale(void *);
    f32 scale=TagGiant_ObjectScale(obj);

    hit = &obj->hit;
    work = obj->work;
    BtlObjBody_Update(obj, 1);
    if (work == NULL) {
        return;
    }
    work->atkSphereCount = 0;
    work->atkObbCount = 0;
    work->contact &= 0xFFFFFF;
#ifdef PORT
    TagContact_Clear(obj->id);
#endif
    StgAabb_SetEmpty(&work->atkBox);
    if (obj->hit.hitIndex < 0) {
        return;
    }
    for (i = 0; i < BOBJ_ATK_NODE_MAX; i++) {
        if ((1 << i) & hit->nodeMask) {
            part = BtlObj_GetNodeByCode(obj, i);
            if (hit->sphere == 1) {
                offset.x = hit->x;
                offset.y = hit->y;
                offset.z = hit->z;
                offset.w = 1.0f;
                Mtx_MulVec4(&center, &part->mtx, &offset);
                ColSphere_Set(&work->atkSphere[work->atkSphereCount], &center, hit->r0*scale);
                ColBounds_OfSphere(&box, &work->atkSphere[work->atkSphereCount]);
                work->atkSphereCount++;
                ColBox_AddPointKeepW(&work->atkBox, box.min);
                ColBox_AddPointKeepW(&work->atkBox, box.max);
            } else {
                offset.x = hit->x;
                offset.y = hit->y;
                offset.z = hit->z;
                offset.w = 1.0f;
                Mtx_MulVec4(&center, &part->mtx, &offset);
                Mtx_Copy(&m, &part->mtx);
                Vec4_Copy((Vec4 *)m.m[3], &center);
                if(scale!=1) {
                    s32 row,col;
                    for(row=0;row<3;row++) for(col=0;col<3;col++) m.m[row][col]/=scale;
                }
                ColObb_Init(&work->atkObb[work->atkObbCount], &center, hit->r0*scale, hit->r1*scale, hit->r2*scale);
                ColObb_Update(&work->atkObb[work->atkObbCount], &m);
                ColBounds_OfObb(&box, &work->atkObb[work->atkObbCount]);
                work->atkObbCount++;
                ColBox_AddPointKeepW(&work->atkBox, box.min);
                ColBox_AddPointKeepW(&work->atkBox, box.max);
            }
        }
    }
}

/* Local rotation matrix of a node. On a fighter, node 2 gets the extra rotation look.node2, nodes 0x2E / 0x2F
   are slerped towards look.node2E / node2F, and node 0x31 (the jaw) takes the face's rotation while it talks. */
void BtlObjPose_GetNodeMtx(BobjObj *obj, Mtx44 *out, BobjPart *part) {
    Quat q;
    BobjLook *look = &obj->look;

    if (obj->type == 0) {
        switch (part->id) {
        case 2:
            Quat_Mul(&q, &look->node2, &part->rot);
            break;
        case 0x2E:
            Quat_Slerp(&q, &part->rot, &obj->look.node2E, look->weight2E);
            break;
        case 0x2F:
            Quat_Slerp(&q, &part->rot, &obj->look.node2F, look->weight2F);
            break;
        case 0x31:
            if (BtlObj_IsJawActive(obj)) {
                BtlObj_GetJawRot(obj, &q);
            } else {
                Vec4_Copy((Vec4 *)&q, (Vec4 *)&part->rot);
            }
            break;
        default:
            Vec4_Copy((Vec4 *)&q, (Vec4 *)&part->rot);
            break;
        }
        Quat_ToMtx(out, &q);
    } else {
        Vec4_Copy((Vec4 *)&q, (Vec4 *)&part->rot);
        Quat_ToMtx(out, &q);
    }
}

/* Resets the transform: position and rotation 0, scale 1, matrix rebuilt. */
void BtlObjXf_Reset(BobjObj *obj) {
    BobjXf *xf = &obj->xf;

    obj->xf.same = 0;
    Vec4_Copy(&xf->pos, &D_002EC2C0);
    Vec4_Copy(&obj->xf.rot, &D_002EC2C0);
    xf->scale = 1.0f;
    BtlObjXf_Update(obj);
    Vec4_Copy(&obj->xf.rootPos, &xf->pos);
}

/* Rebuilds the object's matrix and its inverse from pos / rot / scale. The model faces -Z, so the yaw is turned
   by pi. */
void BtlObjXf_Update(BobjObj *obj) {
    Vec4 *rot = &obj->xf.rot;
    BobjXf *xf = &obj->xf;
    Vec4 *mtxRot = &obj->xf.mtxRot;
    Vec4 *mtxPos = &obj->xf.mtxPos;
    f32 scale = xf->scale;
    Mtx44 *mtx;
    s32 changed;

    changed = memcmp(rot, mtxRot, sizeof(Vec4)) != 0;
    obj->xf.pos.w = 1.0f;
    obj->xf.rot.w = 0.0f;
    Vec4_Copy(mtxPos, &xf->pos);
    Vec4_Copy(mtxRot, rot);
    mtx = &obj->xf.mtx;
    xf->mtxRot.y += 3.14159265f;
    if (3.14159265f < xf->mtxRot.y) {
        xf->mtxRot.y -= 6.2831853f;
    }
    Vu0Cur_LoadIdentity();
    Vu0Cur_ScaleDiagUniform(scale);
    Vu0Cur_RotateZXY(mtxRot);
    Vu0Cur_Translate(mtxPos);
    Vu0Cur_StoreMtx(mtx);
    Mtx_InverseRT(&obj->xf.inv, mtx);
    obj->xf.same = ~changed;
}

/* Sets the object's matrix directly, and its inverse. */
void BtlObjXf_SetMtx(BobjObj *obj, Mtx44 *m) {
    Mtx44 *dst = &obj->xf.mtx;

    Mtx_Copy(dst, m);
    Mtx_InverseRT(&obj->xf.inv, dst);
}

/* World matrix of every node, parents first, on the VU0 matrix stack: node = local * parent. Stores the root
   node's world position in xf.rootPos. */
void BtlObjPose_CalcMatrices(BobjObj *obj) {
    Mtx44 local;
    BobjXf *xf = &obj->xf;
    BobjBone *bone = obj->mdl.nodes;
    BobjPart *part;
    s32 done;

    Vu0Cur_ResetStack();
    Vu0Cur_LoadMtx(&obj->xf.mtx);
    while (1) {
        part = BtlObj_GetNode(obj, bone->id);
        Vu0Cur_Push();
        Vu0Cur_StoreMtx(&part->parent);
        BtlObjPose_GetNodeMtx(obj, &local, part);
        Vec3_Copy((Vec4 *)local.m[3], &part->pos);
        Mtx_Mul(&part->mtx, &part->parent, &local);
        Vu0Cur_LoadMtx(&part->mtx);
        if (bone->pop != 0) {
            Vu0Cur_PopN(bone->pop);
        }
        if (bone->last != 0) {
            break;
        } else {
        bone = (BobjBone *)((u8 *)bone + bone->next);
        }
    }
end:
    Vec4_Copy(&xf->rootPos, (Vec4 *)BtlObj_GetNode(obj, 0)->mtx.m[3]);
}

/* Whether the object has the fighter buffer. */
s32 BtlObj_HasWork(BobjObj *obj) {
    return obj->work != NULL;
}

/* Returns the flash / fade block of a fighter object, NULL for other objects. */
BobjFlash *BtlObjFlash_Get(BobjObj *obj) {
    if (BtlObj_HasWork(obj)) {
        return &obj->work->flash;
    }
    return NULL;
}

/* Zeroes the flash / fade block. */
void BtlObjFlash_Clear(BobjObj *obj) {
    if (BtlObj_HasWork(obj)) {
        memset(BtlObjFlash_Get(obj), 0, sizeof(BobjFlash));
    }
}

/* Same (the entry btl_obj.c calls). */
void BtlObjFlash_Reset(BobjObj *obj) {
    BtlObjFlash_Clear(obj);
}

/* Starts a 0.2 s colour flash: kind 0 yellow, 1 white, 2 strong white. */
s32 BtlObjFlash_Start(BobjObj *obj, s32 kind) {
    BobjFlash *flash = BtlObjFlash_Get(obj);

    if (flash == NULL) {
        return 0;
    }
    flash->time = 0.2f;
    switch (kind) {
    case 0:
        Vec4_Set(&flash->color, 255.0f, 255.0f, 0.0f, 32.0f);
        break;
    case 1:
        Vec4_Set(&flash->color, 255.0f, 255.0f, 255.0f, 32.0f);
        break;
    case 2:
        Vec4_Set(&flash->color, 255.0f, 255.0f, 255.0f, 96.0f);
        break;
    default:
        Vec4_Set(&flash->color, 255.0f, 255.0f, 0.0f, 32.0f);
        break;
    }
    return 1;
}

/* Counts the flash down by one frame (1/30 s). */
s32 BtlObjFlash_Step(BobjObj *obj) {
    BobjFlash *flash = BtlObjFlash_Get(obj);

    if (flash == NULL) {
        return 0;
    }
    flash->time -= 1.0f / 30.0f;
    if (flash->time < 0.0f) {
        flash->time = 0.0f;
    }
    return 1;
}

/* Returns the flash colour and its current strength (0..1); result 0 when no flash is running. */
s32 BtlObjFlash_GetColor(BobjObj *obj, Vec4 *color, f32 *alpha) {
    BobjFlash *flash = BtlObjFlash_Get(obj);
    s32 result;

    if (flash == NULL) {
        return 0;
    }
    *alpha = flash->time / 0.2f * flash->color.w * (1.0f / 128.0f);
    Vec4_Copy(color, &flash->color);
    result = 1;
    if (flash->time <= 0.0f) {
        result = 0;
    }
    return result;
}

/* Moves the fade value by 0.1 per frame: up to 1 while object flag 0x40000 is set, else down to 0. */
s32 BtlObjFade_Step(BobjObj *obj) {
    BobjFlash *flash = BtlObjFlash_Get(obj);

    if (flash == NULL) {
        return 0;
    }
    if (obj->flags & 0x40000) {
        flash->fade += 0.1f;
        if (1.0f < flash->fade) {
            flash->fade = 1.0f;
        }
    } else {
        flash->fade -= 0.1f;
        if (flash->fade < 0.0f) {
            flash->fade = 0.0f;
        }
    }
    return 1;
}

/* Returns the fade value; result 0 when it is zero. */
s32 BtlObjFade_Get(BobjObj *obj, f32 *out) {
    BobjFlash *flash = BtlObjFlash_Get(obj);

    if (flash == NULL) {
        return 0;
    }
    *out = flash->fade;
    if (flash->fade != 0.0f) {
        return 1;
    }
    return 0;
}

/* Stores the byte at object + 0xB30. */
void BtlObj_SetUnkB30(BobjObj *obj, s32 value) {
    obj->unkB30 = value;
}

/* Stops the mouth animation; keep == 0 also resets the mouth state and the eye direction. */
void BtlObjFace_Reset(BobjFace *face, s32 keep) {
    face->talkEnd = 0.0f;
    face->shape = 0;
    face->talkTime = 0.0f;
    face->talkCount = 0;
    face->talkKeys = NULL;
    face->talkLoops = 0;
    if (keep == 0) {
        face->mode = 0;
        face->unk28 = 0;
        face->unk44 = 0.0f;
        face->unk48 = 0.0f;
        face->unk42 = 0;
        face->unk40 = 0;
        face->unk3C = 0;
        Vec4_Set(&face->jaw, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    face->step = 2.0f;
}

/* Whether entry `index` (0..99) of the table at model block + 0x740 exists. */
s32 BtlObjMdl_HasLipTrack(BobjMdl *mdl, u32 index) {
    if (index < 100 && mdl->lip[index] != NULL) {
        return 1;
    }
    return 0;
}

/* Copies the 16 alpha bytes of the model's shared material record into every record of a material table. */
void BtlObjMdl_CopyTexAlpha(BobjMdl *mdl, BobjTexTable *src, BobjTexTable *dst) {
    BobjTexTable *hdr = dst;
    u8 *from;
    u8 *to;
    u32 i;
    s32 j;

#ifdef PORT /* the original passes no third argument: a2 still holds this function's own `dst` at the call.
               Written out, the compiler copies it through v0 (no explicit form reproduces the code). */
    Res_RelocateOffsets(&hdr, dst, dst);
#else
    Res_RelocateOffsets(&hdr, dst);
#endif
    from = *(u8 **)(src->rec + mdl->model->faceTex * 0x40 + 0x3C) + 0x60;
    for (i = 0; i < hdr->count; i++) {
        to = *(u8 **)(hdr->rec + i * 0x40 + 0x3C) + 0x60;
        for (j = 0; j < 16; j++) {
            to[j * 4 + 3] = from[j * 4 + 3];
        }
    }
}

/* Whether the model has an animated face. */
s32 BtlObjMdl_HasJaw(BobjObj *obj) {
    return (obj->mdl.model->flags & 0xC00000) != 0;
}

/* Picks one of four eye direction tracks at random (libc rand); model flag 0x400000 selects the second set. */
void BtlObjFace_PickJawKeys(BobjObj *obj, BobjFace *face) {
    if (!BtlObjMdl_HasJaw(obj)) {
        return;
    }
    if (obj->mdl.model->flags & 0x400000) {
        switch (rand() % 4) {
        case 0:
            face->jawKeys = D_002C6D58;
            break;
        case 1:
            face->jawKeys = D_002C6D98;
            break;
        case 2:
            face->jawKeys = D_002C6DD8;
            break;
        default:
            face->jawKeys = D_002C6E18;
            break;
        }
    } else {
        switch (rand() % 4) {
        case 0:
            face->jawKeys = D_002C6C58;
            break;
        case 1:
            face->jawKeys = D_002C6C98;
            break;
        case 2:
            face->jawKeys = D_002C6CD8;
            break;
        default:
            face->jawKeys = D_002C6D18;
            break;
        }
    }
}

/* Empty. */
void BtlObjFace_Nop(BobjFace *face) {
}

/* Face setup of a new object: shares the material alpha bytes, then the (empty) face hook. */
void BtlObjFace_Init(BobjObj *obj, BobjFace *face) {
    BobjMdl *mdl = &obj->mdl;

    BtlObjMdl_CopyTexAlpha(mdl, mdl->faceTex, mdl->eyeTex);
    BtlObjFace_Nop(face);
}

/* Eye frame of this update: the forced frame (object flag 0x4000000), shut (flag 0x8000000), or the random
   blink: every 90..179 updates the eyes close for 3..5 updates (two libc rand() draws per blink). */
void BtlObjFace_StepBlink(BobjObj *obj, BobjFace *face) {
    BobjMdl *mdl = &obj->mdl;

    if (obj->flags & 0x4000000) {
        u32 n = face->eyeForced - 1;

        if (n < mdl->eyeTex->count && (s32)n >= 0) {
            face->eye = face->eyeForced;
        } else {
            face->eye = 0;
        }
    } else if (obj->flags & 0x8000000) {
        face->eye = 1;
    } else {
        if (face->blinkWait-- < 0) {
            face->blinkTime = rand() % 3 + 3;
            face->blinkWait = rand() % 90 + 90;
        }
        if (face->blinkTime != 0) {
            if (face->blinkTime-- < 0) {
                face->blinkTime = 0;
                face->eye = 0;
            } else {
                face->eye = 1;
            }
        } else {
            face->eye = 0;
        }
    }
}

/* Whether mouth state `state` may replace the current one: states 9..12 only yield to 0 or 9..12. */
s32 BtlObjFace_CanStart(BobjFace *face, s32 state) {
    if ((u32)(face->mode - 9) < 4) {
        if ((u32)(state - 9) < 4 || state == 0) {
            return 1;
        }
        return 0;
    }
    return 1;
}

/* One update of the mouth: advances the talk pattern of the mode, then sets the jaw rotation (models with a jaw
   bone) or the mouth shape (models with mouth textures) from the pattern at the current time. */
void BtlObjFace_Step(BobjObj *obj, BobjFace *face) {
    s32 i;

    switch (face->mode) {
    case 0:
        BtlObjFace_Reset(face, 0);
        return;
    case 3:
        face->talkTime += face->step;
        if (face->talkEnd <= face->talkTime) {
            if (rand() & 1) {
                face->talkCount = 7;
                face->talkKeys = D_002C6C20;
            } else {
                face->talkCount = 5;
                face->talkKeys = D_002C6C40;
            }
            face->talkTime = 0.0f;
            face->shape = face->talkKeys->shape;
        }
        break;
    case 4:
        face->talkTime += face->step;
        if (face->talkEnd <= face->talkTime) {
            BtlObjFace_Reset(face, 0);
        }
        break;
    case 13:
        face->talkTime += face->step;
        if (face->talkEnd <= face->talkTime) {
            if (face->talkLoops++ < 3) {
                if (rand() & 1) {
                    face->talkCount = 7;
                    face->talkKeys = D_002C6C20;
                } else {
                    face->talkCount = 5;
                    face->talkKeys = D_002C6C40;
                }
                face->talkTime = 0.0f;
                face->shape = face->talkKeys->shape;
            } else if (face->talkTime >= face->talkEnd) {
                BtlObjFace_Reset(face, 0);
            }
        }
        break;
    case 10:
        face->jaw.x = 0.3665191f;
        face->jaw.w = 1.0f;
        face->jaw.z = 0.0f;
        face->jaw.y = 0.0f;
        return;
    case 1:
    case 2:
    case 14:
        break;
    default:
        return;
    }
    if (BtlObjMdl_HasJaw(obj)) {
        for (i = 1; i < face->talkCount; i++) {
            if ((f32)face->jawKeys[i - 1].frame == face->talkTime) {
                Vec4_Set(&face->jaw, face->jawKeys[i - 1].x, face->jawKeys[i - 1].y, face->jawKeys[i - 1].z, 1.0f);
                break;
            }
            if ((f32)face->jawKeys[i - 1].frame < face->talkTime && face->talkTime < (f32)face->jawKeys[i].frame) {
                f32 t = (face->talkTime - (f32)face->jawKeys[i - 1].frame) /
                        ((f32)face->jawKeys[i].frame - (f32)face->jawKeys[i - 1].frame);

                face->jaw.x = (face->jawKeys[i].x - face->jawKeys[i - 1].x) * t + face->jawKeys[i - 1].x;
                face->jaw.y = (face->jawKeys[i].y - face->jawKeys[i - 1].y) * t + face->jawKeys[i - 1].y;
                face->jaw.z = (face->jawKeys[i].z - face->jawKeys[i - 1].z) * t + face->jawKeys[i - 1].z;
                face->jaw.w = 1.0f;
                break;
            }
            if ((f32)face->jawKeys[i].frame == face->talkTime) {
                Vec4_Set(&face->jaw, face->jawKeys[i].x, face->jawKeys[i].y, face->jawKeys[i].z, 1.0f);
                break;
            }
        }
        face->shape = 0;
    } else {
        for (i = 1; i < face->talkCount; i++) {
            if ((f32)face->talkKeys[i - 1].frame == face->talkTime) {
                face->shape = face->talkKeys[i - 1].shape;
                return;
            }
            if ((f32)face->talkKeys[i - 1].frame <= face->talkTime && face->talkTime <= (f32)face->talkKeys[i].frame) {
                face->shape = face->talkKeys[i - 1].shape;
                return;
            }
            if ((f32)face->talkKeys[i - 1].frame == face->talkTime) {
                face->shape = face->talkKeys[i].shape;
                return;
            }
        }
    }
}

/* Whether face texture `n` (1-based) of the model's first table exists. */
s32 BtlObjMdl_HasMouthSet(BobjMdl *mdl, s32 n) {
    s32 i = n - 1;

    if (i < 0) {
        return 0;
    }
    return mdl->mouthTexA[i] != NULL;
}

/* Whether the model has anything a face animates: an entry in either texture table, or a jaw bone. */
s32 BtlObjMdl_HasMouth(BobjMdl *mdl) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (mdl->mouthTexA[i] != NULL) {
            return 1;
        }
        if (mdl->mouthTexB[i] != NULL) {
            return 1;
        }
    }
    return (mdl->model->flags & 0xC00000) != 0;
}

/* ------------------------------------------------------------------------------------------------------------
 * Second part (formerly bobj_b.c, 0x24F1F0..0x250B28), merged at integration because BtlObj_UpdateFace and
 * BtlObj_IsJawActive only match with the face helpers of the first part defined above them. It keeps its own
 * header and view types. Names the first part already declared with
 * other types are reached through cast macros (the generated code is the same).
 * ------------------------------------------------------------------------------------------------------------ */
/* The first part reaches these through aliased declarations with its own types; the definitions follow. */
#undef BtlObj_FindBound
#undef BtlObj_GetNode
#undef BtlObj_GetNodeByCode
#undef BtlObj_IsJawActive
#undef BtlObj_GetJawRot
#undef BOBJ_NODE_MAX
/*
 * Battle object, third part (0x24F1F0..0x2527B0): what sits between the animation event queries (bobj_a) and
 * src/sys/fade.c. Layouts are in include/battle/bobj_b.h.
 *
 *   0x24F1F0..0x24FA20  the face: eye frame, mouth modes ("sub-states"), lip-sync tracks
 *   0x24FA20..0x2500E8  the tables an object reads out of its files, initial flags, colour preset
 *   0x2500E8..0x250570  colour mode requests and part visibility bits
 *   0x250570..0x250B28  model nodes: lookup, "is shown", position snapshots, node velocity
 *   0x250B28..0x2527B0  secondary motion: push / sway inputs and the two kinds of chains
 *
 * Per frame (BtlObj_UpdateAll in btl_obj.c, for every object in use, skipped while the battle is paused):
 *   BtlObj_SaveNodePositions(obj, 0)      positions of last frame's pose, for node velocities
 *   animation and pose passes (bobj_a)
 *   BtlObj_UpdateChains(obj)              chains rewrite node rotations; the pose pass then runs again
 *   BtlObj_UpdateFace(obj)                blink and mouth
 * None of it looks at the camera, a view or a draw list: the same calls run whether or not the object is drawn.
 */
#include "battle/bobj_b.h"

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 atan2f(f32 y, f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
extern f32 Mathf_Sin(f32 a);
extern f32 Mathf_Cos(f32 a);
#define Res_RelocateOffsets ((void (*)(void *out, void *base, void *hdr))Res_RelocateOffsets)

/* dst = 0. */
extern void Vec4_SetZero(Vec4 *dst);
/* Inverse of a rotation + translation matrix. */
extern void Mtx_InverseRT(Mtx44 *dst, Mtx44 *src);
/* Matrix product into the first argument. */
extern void Mtx_Mul(Mtx44 *dst, Mtx44 *a, Mtx44 *b);
/* dst = lerp(a, b, t). */
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t);
/* Rotations of a vector: about an axis vector, about x, about y. */
extern void Vec3_RotateAxis(Vec4 *dst, Vec4 *src, Vec4 *axis, f32 angle);
extern void Vec3_RotateX(Vec4 *dst, Vec4 *src, f32 angle);
extern void Vec3_RotateY(Vec4 *dst, Vec4 *src, f32 angle);
/* Named "light direction" in stg_b.c; the chains use it as a wind vector whose w is the strength. */
extern s32 BtlStage_GetLightDir(Vec4 *out);

/* The 4-entry pool of btl_obj.c. */
extern BObjFace *BtlObjPool70_Alloc(void);
extern s32 BtlObjPool70_Free(BObjFace *node);

/* Face helpers of the previous file (bobj_a's range): mouth reset, "lip track exists", alpha patch of a mouth
   texture set, "model has a jaw bone", jaw table pick (rand() % 4), blink update (rand() % 3, rand() % 90),
   "mode change allowed", mouth animation, "mouth set exists", "model has a mouth". */
#define BtlObjFace_Reset ((void (*)(BObjFace *face, s32 keep))BtlObjFace_Reset)
#define BtlObjMdl_HasLipTrack ((s32 (*)(BObjMdl *mdl, s32 line))BtlObjMdl_HasLipTrack)
#define BtlObjMdl_CopyTexAlpha ((void (*)(BObjMdl *mdl, BObjTexTable *tex, void *set))BtlObjMdl_CopyTexAlpha)
#define BtlObjMdl_HasJaw ((s32 (*)(BObj *obj))BtlObjMdl_HasJaw)
#define BtlObjFace_PickJawKeys ((void (*)(BObj *obj, BObjFace *face))BtlObjFace_PickJawKeys)
#define BtlObjFace_Init ((void (*)(BObj *obj, BObjFace *face))BtlObjFace_Init)
#define BtlObjFace_StepBlink ((void (*)(BObj *obj, BObjFace *face))BtlObjFace_StepBlink)
#define BtlObjFace_CanStart ((s32 (*)(BObjFace *face, s32 mode))BtlObjFace_CanStart)
#define BtlObjFace_Step ((void (*)(BObj *obj, BObjFace *face))BtlObjFace_Step)
#define BtlObjMdl_HasMouthSet ((s32 (*)(BObjMdl *mdl, s32 shape))BtlObjMdl_HasMouthSet)
#define BtlObjMdl_HasMouth ((s32 (*)(BObjMdl *mdl))BtlObjMdl_HasMouth)

/* Talk patterns {frame, shape}: 7 and 5 records. */
#define D_002C6C20 (*(s16(*)[])&D_002C6C20)
#define D_002C6C40 (*(s16(*)[])&D_002C6C40)
/* (0, 0, 0, 1): the identity rotation. */
extern Vec4 D_002EC2C0;
/* Written by BtlObj_InitChains (0.1, 0, 0, 0); no reader found. */
extern Vec4 D_003337D0;

typedef struct BObjProgress {
    /* 0x00 */ s32 unk00;
} BObjProgress;

typedef struct BObjSave {
    /* 0x0000 */ u8 unk0000[0x1608];
    /* 0x1608 */ u32 flags; /* bit 0: second voice language */
} BObjSave;

typedef struct BObjCommon {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ BObjFile file3; /* only buf is real: the pointers of the three common files follow each other */
} BObjCommon;

extern BObjProgress *gProgress;
extern BObjSave *gSaveData;
extern BObjCommon *gCommonRes;

/* Offset of BObj.nodes: the two accessors only match with the index added to the object first. */
#define BOBJ_NODE_TABLE 0xD6C

#define SET_COLOR(obj, v) ((obj)->state.flags = ((obj)->state.flags & ~BOBJ_FLAG_COLOR_MASK) | (v))
#define Q_IDENT(q) ((q)->x = 0.0f, (q)->y = 0.0f, (q)->z = 0.0f, (q)->w = 1.0f)
#define FLAG_BITS(obj) ((BObjFlagBits *)&(obj)->state.flags)

/* Resets the mouth and patches the alpha bytes of every mouth texture set the model has. */
void BObjFace_InitMouth(BObj *obj, BObjFace *face) {
    BObjMdl *mdl;
    s32 i;

    BtlObjFace_Reset(face, 0);
    mdl = &obj->mdl;
    for (i = 0; i < 8; i++) {
        if (obj->mdl.mouthB[i] != NULL) {
            BtlObjMdl_CopyTexAlpha(mdl, mdl->tex, obj->mdl.mouthB[i]);
        }
    }
}

/* Advances the lip track: finds the record the time falls in and opens or closes the mouth by its code. */
void BObjFace_StepLipData(BObj *obj, BObjFace *face) {
    BObjLipKey *keys;
    f32 time;
    s32 found;
    u16 i;

    keys = face->lip;
    if (keys == NULL) {
        return;
    }
    time = face->lipTime;
    if (0.0f <= time) {
        found = -1;
        for (i = face->lipIndex; i < face->lipCount - 1; i++) {
            if (keys[i].frame <= time && time < keys[i + 1].frame) {
                found = i;
                face->lipIndex = i;
                break;
            }
        }
        if (face->lipEnd < time || found == -1) {
            BtlObj_SetSubState(obj, BOBJ_MOUTH_REST, 0);
            return;
        }
        switch (keys[found].code) {
        case 1:
            if (face->mode != BOBJ_MOUTH_TALK) {
                BtlObj_SetSubState(obj, BOBJ_MOUTH_TALK, 0);
            }
            break;
        default:
            if (face->mode != BOBJ_MOUTH_TALK) {
                BtlObj_SetSubState(obj, BOBJ_MOUTH_TALK, 0);
            }
            break;
        case 0:
            BtlObj_SetSubState(obj, BOBJ_MOUTH_CLOSED, 0);
            break;
        }
    }
    face->lipTime = face->lipTime + face->step;
}

/* Mouth update: lip track, then the mouth animation of the mode. */
void BObjFace_UpdateMouth(BObj *obj, BObjFace *face) {
    BObjFace_StepLipData(obj, face);
    BtlObjFace_Step(obj, face);
}

/* Matches only with the body of BtlObjFace_StepBlink (0x24EB70, first part) visible to the compiler (otherwise a
   beqz comes out as beqzl): the reason the two parts are one file. */
/* Per-frame face pass: blink, mouth. */
s32 BtlObj_UpdateFace(BObj *obj) {
    BObjFace *face;

    face = obj->face;
    if (face != NULL) {
        if (face->flags & BOBJ_FACE_EYES) {
            BtlObjFace_StepBlink(obj, face);
        }
        if (face->flags & BOBJ_FACE_MOUTH) {
            BObjFace_UpdateMouth(obj, face);
        }
#ifdef PORT
        {
            extern void Port_TagFaceTrace(u32,s32,s32,s32,s32,s32,s32,f32);
            Port_TagFaceTrace((u32)obj,face->mode,face->lipArg,(s32)face->lipTime,
                face->shape,face->lip ? face->lip[0].frame : -1,face->lipCount,face->jaw.x);
        }
#endif
    }
}

/* Gives the object a face state when its model has eyes or a mouth. */
void BtlObj_CreateFace(BObj *obj) {
    s32 flags;
    BObjFace *face;
    BObjMdl *mdl;

    mdl = &obj->mdl;
    flags = 0;
    if (mdl->eyes != NULL) {
        flags |= BOBJ_FACE_EYES;
    }
    if (BtlObjMdl_HasMouth(mdl)) {
        flags |= BOBJ_FACE_MOUTH;
    }
    if (flags != 0) {
        face = BtlObjPool70_Alloc();
        memset(face, 0, sizeof(BObjFace));
        face->flags = flags;
        if (flags & BOBJ_FACE_EYES) {
            BtlObjFace_Init(obj, face);
        }
        if (face->flags & BOBJ_FACE_MOUTH) {
            BObjFace_InitMouth(obj, face);
        }
        obj->face = face;
    }
}

/* Returns the face state to its pool. */
void BtlObj_FreeFace(BObj *obj) {
    if (obj->face != NULL) {
        BtlObjPool70_Free(obj->face);
    }
}

/* Forces eye frame `frame`; 9 hands the eyes back to the blink. */
void BtlObj_SetEyeFrame(BObj *obj, u32 frame) {
    BObjFace *face;
    BObjMdl *mdl;

    face = obj->face;
    mdl = &obj->mdl;
    if (face != NULL && (face->flags & BOBJ_FACE_EYES)) {
        if (frame != 9) {
            obj->state.flags |= BOBJ_FLAG_EYE_FORCED;
            if (!(mdl->eyes->count < frame)) {
                face->eyeForced = frame;
            }
        } else {
            obj->state.flags &= ~BOBJ_FLAG_EYE_FORCED;
        }
    }
}

/* Sets the mouth mode (BOBJ_MOUTH_*); arg is the lip track number or pointer for modes 2 and 14. */
void BtlObj_SetSubState(BObj *obj, s32 mode, s32 arg) {
    BObjFace *face;
    BObjMdl *mdl;
    BObjLipData *data;
    s16 end;

    face = obj->face;
    mdl = &obj->mdl;
    if (face == NULL) {
        return;
    }
    if (!(face->flags & BOBJ_FACE_MOUTH)) {
        return;
    }
    if (arg >= 0 && mode == BOBJ_MOUTH_LIP) {
        if (!BtlObjMdl_HasLipTrack(mdl, arg)) {
            return;
        }
    }
    if (!BtlObjFace_CanStart(face, mode)) {
        return;
    }
    face->mode = mode;
    switch (mode) {
    case BOBJ_MOUTH_REST:
        BtlObjFace_Reset(face, 0);
        return;
    case BOBJ_MOUTH_CLOSED:
        BtlObjFace_Reset(face, 1);
        break;
    case BOBJ_MOUTH_TALK:
    case BOBJ_MOUTH_TALK_ONCE:
    case BOBJ_MOUTH_TALK_3:
        if (rand() & 1) {
            face->talkCount = 7;
            face->talkTable = D_002C6C20;
        } else {
            face->talkCount = 5;
            face->talkTable = D_002C6C40;
        }
        end = face->talkTable[face->talkCount * 2 - 2];
        face->talkTime = 0.0f;
        face->talkEnd = end;
        face->talkLoops = 0;
        BtlObjFace_PickJawKeys(obj, face);
        break;
    case BOBJ_MOUTH_LIP:
        data = mdl->lip[arg];
        if (data == NULL) {
            break;
        }
        face->lipIndex = 0;
        face->lipCount = data->count;
        face->lip = data->keys;
        face->lipArg = arg;
        face->lipTime = 0.0f;
        face->lipEnd = face->lip[face->lipCount - 1].frame;
        BtlObjFace_PickJawKeys(obj, face);
        break;
    case BOBJ_MOUTH_LIP_PTR:
        face->lipIndex = 0;
        face->lipCount = ((BObjLipData *)arg)->count;
        face->lip = ((BObjLipData *)arg)->keys;
        face->lipArg = arg;
        face->lipTime = 0.0f;
        face->lipEnd = face->lip[face->lipCount - 1].frame;
        BtlObjFace_PickJawKeys(obj, face);
        break;
    case BOBJ_MOUTH_SHAPE_1:
        face->shape = 1;
        break;
    case BOBJ_MOUTH_SHAPE_2:
        face->shape = 2;
        break;
    case BOBJ_MOUTH_SHAPE_3:
        face->shape = 3;
        break;
    case BOBJ_MOUTH_SHAPE_4:
        face->shape = 4;
        break;
    case BOBJ_MOUTH_HOLD_5:
        BtlObjFace_Reset(face, 0);
        face->mode = mode;
        face->shape = 5;
        break;
    case BOBJ_MOUTH_HOLD_OPEN:
        BtlObjFace_Reset(face, 0);
        face->mode = mode;
        face->shape = 6;
        face->jaw.x = 0.3665191f;
        face->jaw.y = 0.0f;
        face->jaw.z = 0.0f;
        face->jaw.w = 1.0f;
        break;
    case BOBJ_MOUTH_HOLD_7:
        BtlObjFace_Reset(face, 0);
        face->mode = mode;
        face->shape = 7;
        break;
    case BOBJ_MOUTH_HOLD_8:
        BtlObjFace_Reset(face, 0);
        face->mode = mode;
        face->shape = 8;
        break;
    }
}

/* Returns the mouth shape in use minus one, or -1 (no face, mouth closed, or the model lacks that set). */
s32 BtlObj_GetSubState(BObj *obj) {
    BObjFace *face;

    face = obj->face;
    if (face == NULL) {
        return -1;
    }
    if (BtlObjMdl_HasMouthSet(&obj->mdl, face->shape)) {
        return face->shape - 1;
    }
    return -1;
}

/* Returns the texture entry of the face part being shown: mouth shape 5..8, else the eye frame. */
BObjTexEntry *BtlObj_GetFaceTexture(BObj *obj) {
    BObjFace *face;
    BObjTexTable *set;
    s32 shape;

    face = obj->face;
    if (face == NULL) {
        return NULL;
    }
    shape = BtlObj_GetSubState(obj);
    if (shape >= 4) {
        set = obj->mdl.mouthB[shape];
        if (set != NULL) {
            return set->entries;
        }
    } else if (face->eye != 0) {
        return &obj->mdl.eyes->entries[face->eye] - 1;
    }
    return NULL;
}

/* 1 when the object's face has eyes. */
s32 BtlObj_HasEyes(BObj *obj) {
    BObjFace *face = obj->face;

    if (face == NULL) {
        return 0;
    }
    if (face->flags & BOBJ_FACE_EYES) {
        return 1;
    }
    return 0;
}

/* 1 when the object's face has a mouth. */
s32 BtlObj_HasMouth(BObj *obj) {
    BObjFace *face = obj->face;

    if (face == NULL) {
        return 0;
    }
    if (face->flags & BOBJ_FACE_MOUTH) {
        return 1;
    }
    return 0;
}

/* Matches only with the body of BtlObjMdl_HasJaw (0x24E9C8, first part) visible; same cause as BtlObj_UpdateFace. */
/* 1 when the mouth is in a mode other than rest and the model moves a jaw bone for it. */
s32 BtlObj_IsJawActive(BObj *obj) {
    BObjFace *face = obj->face;

    if (face == NULL) {
        return 0;
    }
    if (face->mode != BOBJ_MOUTH_REST) {
        if (BtlObjMdl_HasJaw(obj)) {
            return 1;
        }
    }
    return 0;
}

/* Jaw rotation as a quaternion; identity (and 0) when the object has no face. */
s32 BtlObj_GetJawRot(BObj *obj, Quat *out) {
    BObjFace *face = obj->face;

    if (face == NULL) {
        Vec4_Copy((Vec4 *)out, &D_002EC2C0);
        return 0;
    }
    Quat_FromEuler(out, &face->jaw);
    return 1;
}

/* Returns the mouth mode, -1 without a face. */
s32 BtlObj_GetMouthMode(BObj *obj) {
    BObjFace *face = obj->face;

    if (face == NULL) {
        return -1;
    }
    return face->mode;
}

/* Returns the forced eye frame, -1 without a face. */
s32 BtlObj_GetEyeFrame(BObj *obj) {
    BObjFace *face = obj->face;

    if (face == NULL) {
        return -1;
    }
    return face->eyeForced;
}

/* Returns entry n of a file's offset table, NULL when the entry is empty. */
void *BObjFile_GetEntry(BObjFile *file, s32 n) {
    u32 *buf = file->buf;
#ifdef PORT
    if (buf == NULL || n < 1 || (PortGame_IsBT4() && (u32)n > buf[0])) return NULL;
#endif
    if (buf[n + 1] != buf[n]) {
        return (u8 *)buf + (buf[n] >> 2 << 2);
    }
    return NULL;
}

/* Fills the object's table pointers from the files of its resource slot. */
/* The original's object type nests the model block: the two arrays written through `obj` are addressed as
   (obj + 0x20) + 0xA0 / + 0x90, and this compiler splits every member offset into a multiple of 16 (added to the
   index) and a remainder (added to the base), so the remainders along the member chain sum to 0x20. One level
   (mdl at 0x18, arrays at 0xA8 / 0x98) gives 0x10; a block at 0xC holding the model block at 0xC gives 0x20.
   Only the nesting is verified, not where the outer block really starts or ends. */
typedef struct BObjBindView {
    /* 0x00 */ u8 unk00[0xC];
    /* 0x0C */ struct {
        /* 0x00 */ u8 unk0C[0xC]; /* the slot pointer is the last word (object + 0x14) */
        /* 0x0C */ BObjMdl mdl;   /* object + 0x18 */
    } res;
} BObjBindView;

void BtlObj_BindTables(BObj *obj) {
    BObjMdl *mdl;
    BObjSlot *slot;
    s32 i;
    s32 j;
    s32 base;

    mdl = &obj->mdl;
    slot = obj->slot;
    mdl->unk00 = BObjFile_GetEntry(&slot->file[0], 3);
    mdl->unk04 = BObjFile_GetEntry(&slot->file[0], 0xC);
    mdl->unk3C = BObjFile_GetEntry(&slot->file[0], 2);
#ifdef PORT
    if (PortGame_IsBT4() && mdl->unk3C) {
        u32 *parts=slot->file[0].buf;
        PortGame_RepairModelBody(mdl->unk3C,(parts[3]&~3u)-(parts[2]&~3u),slot->file[0].id);
    }
#endif
    if (slot->file[1].buf != NULL) {
        for (i = 0; i < 0x19E; i++) {
            ((BObjBindView *)obj)->res.mdl.anims[i] = BObjFile_GetEntry(&slot->file[1], i + 1);
        }
    }
    for (i = 0; i < 8; i++) {
        mdl->unk720[i] = BObjFile_GetEntry(&slot->file[0], i + 0x1D);
    }
    for (i = 0; i < 8; i++) {
        mdl->mouthA[i] = BObjFile_GetEntry(&slot->file[0], i + 4);
    }
    for (i = 0, j = 0; i < 8; i++) {
        if (i >= 4) {
            mdl->mouthB[i] = BObjFile_GetEntry(&slot->file[0], j + 0xE);
            j++;
        } else {
            mdl->mouthB[i] = NULL;
        }
    }
    mdl->eyes = BObjFile_GetEntry(&slot->file[0], 0xD);
    for (i = 0; i < 3; i++) {
        ((BObjBindView *)obj)->res.mdl.unk98[i] = BObjFile_GetEntry(&slot->file[0], i + 0x29);
    }
    for (i = 0; i < 8; i++) {
        mdl->unk8D0[i] = BObjFile_GetEntry(&slot->file[0], i + 0x1D);
    }
    for (i = 0; i < 4; i++) {
        mdl->unk8F0[i] = BObjFile_GetEntry(&slot->file[0], i + 0x25);
    }
    mdl->unkA4 = BObjFile_GetEntry(&slot->file[0], gProgress->unk00 + 0x2D);
    base = 0x64;
    if (!(gSaveData->flags & 1)) {
        base = 0;
    }
    for (i = 0; i < 100; i++) {
        mdl->lip[i] = BObjFile_GetEntry(&slot->file[0], base + 0x35 + i);
    }
    if (slot->file[2].buf != NULL) {
        for (i = 0; i < 5; i++) {
            mdl->unk84[i] = BObjFile_GetEntry(&slot->file[2], i + 1);
        }
        mdl->unk40 = BObjFile_GetEntry(&slot->file[2], 6);
    }
    mdl->unk38 = BObjFile_GetEntry(&slot->file[0], 1);
#ifdef PORT
    if (PortGame_IsBT4()) {
        s32 chara = PortGame_ModelCharacter(slot->file[0].id);
        u32 *meta = PortGame_Metadata(chara, 0);
        u32 *lips = PortGame_Metadata(chara, (gSaveData->flags & 1) ? 3 : 2);
        if (meta != NULL) {
            u32 *ai = (u32 *)((u8 *)meta + (meta[3] & ~3u));
            mdl->unk38 = (u8 *)meta + (meta[1] & ~3u);
            for (i=0; i<3; ++i)
                ((BObjBindView *)obj)->res.mdl.unk98[i] = ai[i+1] != ai[i+2] ? (u8 *)ai+(ai[i+1]&~3u) : NULL;
            mdl->unkA4 = PortGame_Metadata(chara, 1);
            for (i=0; i<100; ++i)
                mdl->lip[i] = lips && lips[i+1]!=lips[i+2] ? (void *)((u8 *)lips+(lips[i+1]&~3u)) : NULL;
        }
    }
#endif
    if (mdl->unk38 != NULL) {
        Res_RelocateOffsets(&mdl->unk38, mdl->unk38, mdl->unk38);
    }
    mdl->unk924 = BObjFile_GetEntry(&slot->file[0], 0x16);
    mdl->unk904 = BObjFile_GetEntry(&slot->file[0], 0x12);
    mdl->unk908 = BObjFile_GetEntry(&slot->file[0], 0x13);
    mdl->unk90C = BObjFile_GetEntry(&slot->file[0], 0x14);
    mdl->unk910 = BObjFile_GetEntry(&slot->file[0], 0x15);
    mdl->unk914 = BObjFile_GetEntry(&slot->file[0], 0x18);
    mdl->unk918 = BObjFile_GetEntry(&slot->file[0], 0x19);
    mdl->unk91C = BObjFile_GetEntry(&slot->file[0], 0x1B);
    mdl->unk920 = BObjFile_GetEntry(&slot->file[0], 0x1C);
#ifdef PORT
    if (PortGame_IsBT4()) {
        u32 *meta=PortGame_Metadata(PortGame_ModelCharacter(slot->file[0].id),0);
        if (meta) {
            u8 *params=(u8 *)meta+(meta[2]&~3u);
            mdl->unk904=(s8 *)params;
            mdl->unk908=params+0x100;
            mdl->unk90C=params+0x2240;
            mdl->unk910=params+0x2500;
            mdl->unk924=params+0x25C0;
            mdl->unk914=params+0x29D0;
            mdl->unk918=params+0x2C50;
            mdl->unk91C=params+0x2D50;
            mdl->unk920=params+0x2F50;
        }
    }
#endif
    mdl->chain = BObjFile_GetEntry(&slot->file[0], 0x17);
}

/* Sets the object's initial flags from its type and model header, then the colour preset. */
void BtlObj_InitFlags(BObj *obj) {
    BObjState *state = &obj->state;
    BObjMdl *mdl = &obj->mdl;
    s32 top;

    state->flags = 0x2000006;
    if (obj->type == 0 || obj->type == 2) {
        state->flags = 0x12000016;
    } else if (obj->type != 1) {
        state->flags = 0x22000016;
    }
    if (mdl->model->flags & BOBJ_MODEL_FADE) {
        state->flags |= 8;
    }
    if (mdl->model->flags & BOBJ_MODEL_COLOR_A) {
        state->flags |= 0x20000;
    } else {
        state->flags |= 0x10000;
    }
    if (mdl->unk904 == NULL) {
        BtlObj_SetColorPreset(obj, 0, 0);
    } else {
        top = ((BObjModelBytes *)mdl->model)->flagsTop;
        BtlObj_SetColorPreset(obj, mdl->unk904[3], top & 1);
    }
}

/* Copies row `row` of preset table `set` (negative = the model's own, bit 24 of its flags) to the draw state. */
void BtlObj_SetColorPreset(BObj *obj, s32 row, s32 set) {
    BObjState *state = &obj->state;
    BObjMdl *mdl = &obj->mdl;
    s32 top;

    if (set < 0) {
        top = ((BObjModelBytes *)mdl->model)->flagsTop;
        set = top & 1;
    } else if (set >= 2) {
        set = 0;
    }
    {
        f32 table[2][11][5] = {
            {
                { 63.0f, 63.0f, 255.0f, 40.0f, 65.0f },
                { 255.0f, 63.0f, 255.0f, 40.0f, 65.0f },
                { 142.0f, 142.0f, 25.0f, 40.0f, 65.0f },
                { 255.0f, 63.0f, 63.0f, 40.0f, 65.0f },
                { 63.0f, 255.0f, 63.0f, 40.0f, 65.0f },
                { 63.0f, 100.0f, 255.0f, 40.0f, 65.0f },
                { 255.0f, 63.0f, 63.0f, 40.0f, 65.0f },
                { 128.0f, 63.0f, 255.0f, 40.0f, 65.0f },
                { 142.0f, 255.0f, 25.0f, 40.0f, 65.0f },
                { 142.0f, 255.0f, 25.0f, 40.0f, 65.0f },
                { 142.0f, 255.0f, 25.0f, 40.0f, 65.0f },
            },
            {
                { 63.0f, 63.0f, 255.0f, 40.0f, 65.0f },
                { 255.0f, 63.0f, 255.0f, 40.0f, 65.0f },
                { 142.0f, 142.0f, 25.0f, 40.0f, 65.0f },
                { 255.0f, 63.0f, 63.0f, 40.0f, 65.0f },
                { 0.0f, 171.0f, 0.0f, 40.0f, 65.0f },
                { 63.0f, 100.0f, 255.0f, 40.0f, 65.0f },
                { 255.0f, 63.0f, 63.0f, 40.0f, 65.0f },
                { 128.0f, 63.0f, 255.0f, 40.0f, 65.0f },
                { 142.0f, 142.0f, 32.0f, 40.0f, 65.0f },
                { 142.0f, 142.0f, 32.0f, 40.0f, 65.0f },
                { 142.0f, 142.0f, 32.0f, 40.0f, 65.0f },
            },
        };

        state->preset[0] = table[set][row][0];
        state->preset[1] = table[set][row][1];
        state->preset[2] = table[set][row][2];
        state->preset[3] = table[set][row][3];
        state->preset[4] = table[set][row][4];
    }
}

/* Points the object's lip tracks and two more tables at common file 3. */
void BtlObj_BindCommonTables(BObj *obj) {
    BObjCommon *res;
    BObjMdl *mdl;
    s32 first;
    s32 ofs;
    s32 i;
    u32 *p;

    first = 0x64;
    if (!(gSaveData->flags & 1)) {
        first = 0;
    }
    res = gCommonRes;
    mdl = &obj->mdl;
    /* Entry 0xE + first + i, written the way the original's address arithmetic comes out. */
    ofs = first * 4 + 0x38;
    for (i = 0; i < 100; i++) {
        p = (u32 *)(ofs + (u32)res->file3.buf);
        ofs += 4;
        if (p[1] != p[0]) {
            obj->mdl.lip[i] = (BObjLipData *)((u8 *)res->file3.buf + (p[0] >> 2 << 2));
        } else {
            obj->mdl.lip[i] = NULL;
        }
    }
    if (res->file3.buf[0xD] != res->file3.buf[0xC]) {
        mdl->common9C = (u8 *)res->file3.buf + (res->file3.buf[0xC] >> 2 << 2);
    } else {
        mdl->common9C = NULL;
    }
    if (res->file3.buf[0xE] != res->file3.buf[0xD]) {
        mdl->common900 = (u8 *)res->file3.buf + (res->file3.buf[0xD] >> 2 << 2);
    } else {
        mdl->common900 = NULL;
    }
}

/* Copies the 100 lip track pointers of another object. */
void BtlObj_CopyLipTables(BObj *obj, BObj *src) {
    BObjMdl *dst;
    BObjMdl *from;
    s32 i;

    dst = &obj->mdl;
    from = &src->mdl;
    for (i = 0; i < 100; i++) {
        dst->lip[i] = from->lip[i];
    }
}

/* Sets or clears one colour mode request and rewrites the colour flag (bits 16..23) by priority. */
void BtlObj_SetColorMode(BObj *obj, s32 bit, s32 on) {
    BObjState *state = &obj->state;
    u32 mask = 1 << bit;

    if (on) {
        state->modeBits |= mask;
    } else {
        state->modeBits &= ~mask;
    }
    if (bit == 7 || state->modeBits == 0) {
        if (obj->mdl.model->flags & BOBJ_MODEL_COLOR_A) {
            SET_COLOR(obj, 0x20000);
        } else {
            SET_COLOR(obj, 0x10000);
        }
        state->modeBits = 0;
    } else if (state->modeBits & 8) {
        SET_COLOR(obj, 0x80000);
    } else if (state->modeBits & 4) {
        SET_COLOR(obj, 0x40000);
    } else if (state->modeBits & 0x40) {
        SET_COLOR(obj, 0x400000);
    } else if (state->modeBits & 1) {
        SET_COLOR(obj, 0x10000);
    } else if (state->modeBits & 2) {
        SET_COLOR(obj, 0x20000);
    } else if (state->modeBits & 0x10) {
        SET_COLOR(obj, 0x100000);
    } else if (state->modeBits & 0x20) {
        SET_COLOR(obj, 0x200000);
    } else {
        if (obj->mdl.model->flags & BOBJ_MODEL_COLOR_A) {
            SET_COLOR(obj, 0x20000);
        } else {
            SET_COLOR(obj, 0x10000);
        }
        state->modeBits = 0;
    }
}

/* Returns the colour mode in force as a number 0..6. */
s32 BtlObj_GetColorMode(BObj *obj) {
    u32 flags = obj->state.flags;

    if (flags & 0x10000) {
        return 0;
    }
    if (flags & 0x20000) {
        return 1;
    }
    if (flags & 0x40000) {
        return 2;
    }
    if (flags & 0x80000) {
        return 3;
    }
    if (flags & 0x100000) {
        return 4;
    }
    if (flags & 0x400000) {
        return 6;
    }
    if (flags & 0x200000) {
        return 5;
    }
    return 0;
}

/* Shows or hides part 0..4 of the model; part 5 resets the low flags to "everything shown". */
void BtlObj_SetPartVisible(BObj *obj, u32 part, s32 on) {
    BObjMdl *mdl = &obj->mdl;
    u32 high;

    if (on) {
        switch (part) {
        case 0:
            if (!(obj->state.flags & BOBJ_FLAG_VISIBLE)) {
                obj->state.flags |= BOBJ_FLAG_VISIBLE;
            }
            break;
        case 1:
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part1)) {
                obj->state.flags |= BOBJ_FLAG_PART1;
            }
            break;
        case 4:
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part4)) {
                obj->state.flags |= BOBJ_FLAG_PART4;
            }
            break;
        case 2:
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2)) {
                obj->state.flags |= BOBJ_FLAG_PART2;
            }
            break;
        case 3:
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3)) {
                obj->state.flags |= BOBJ_FLAG_PART3;
            }
            break;
        }
    } else {
        switch (part) {
        case 0:
            if (obj->state.flags & BOBJ_FLAG_VISIBLE) {
                obj->state.flags &= ~BOBJ_FLAG_VISIBLE;
            }
            break;
        case 1:
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part1) {
                obj->state.flags &= ~BOBJ_FLAG_PART1;
            }
            break;
        case 4:
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part4) {
                obj->state.flags &= ~BOBJ_FLAG_PART4;
            }
            break;
        case 2:
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2) {
                obj->state.flags &= ~BOBJ_FLAG_PART2;
            }
            break;
        case 3:
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3) {
                obj->state.flags &= ~BOBJ_FLAG_PART3;
            }
            break;
        }
    }
    if (part == 5) {
        high = obj->state.flags & 0xFFFF0000;
        obj->state.flags = high | 0x16;
        if (mdl->model->flags & BOBJ_MODEL_FADE) {
            obj->state.flags = high | 0x1E;
        }
    }
}

/* 1 when part 0..4 of the model is shown. */
s32 BtlObj_IsPartVisible(BObj *obj, u32 part) {
    switch (part) {
    case 0:
        return ((s32)obj->state.flags >> 1) & 1;
    case 1:
        return FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part1;
    case 4:
        return FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part4;
    case 2:
        return FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2;
    case 3:
        return FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3;
    }
    return 0;
}

/* Returns the bounds record of node `node`, NULL when the model has none. */
BObjBound *BtlObj_FindBound(BObj *obj, s32 node) {
    BObjBound *bound = obj->mdl.bounds;

    while (1) {
        if (bound->node == node) {
            return bound;
        }
        if (bound->last != 0) {
            break;
        }
        bound = (BObjBound *)((u8 *)bound + bound->next);
    }
    return NULL;
}

/* Returns model node `node`, NULL when the model does not have it. */
BObjNode *BtlObj_GetNode(BObj *obj, s32 node) {
    BObjNode **p = (BObjNode **)((u8 *)obj + node * 4);

    return p[BOBJ_NODE_TABLE / 4];
}

/* 1 when the model has node `node`. */
s32 BtlObj_HasNode(BObj *obj, s32 node) {
    BObjNode **p = (BObjNode **)((u8 *)obj + node * 4);

    return p[BOBJ_NODE_TABLE / 4] != NULL;
}

/* 1 when the geometry hanging on node `node` is drawn, given the part bits and the model's layout class. */
s32 BtlObj_IsNodeShown(BObj *obj, s32 node) {
    s32 shown;

    shown = 1;
    if (obj->mdl.model->flags & BOBJ_MODEL_PARTS_A) {
        if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->hideA) {
            if (node >= 0x23 && node <= 0x2C) {
                shown = 0;
            } else if (node == 0x40) {
                shown = 0;
            } else if (node == 0x41) {
                shown = 0;
            } else if (node == 0x42) {
                shown = 0;
            } else if (node == 0x43) {
                shown = 0;
            }
        } else if (node >= 0x15 && node <= 0x1E) {
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3) {
                shown = 0;
            }
        } else if (node >= 0x23 && node <= 0x2C) {
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2) {
                shown = 0;
            }
        } else if (node == 0x42) {
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2)) {
                shown = 0;
            }
        } else if (node == 0x43) {
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3)) {
                shown = 0;
            }
        }
    } else if (obj->mdl.model->flags & BOBJ_MODEL_PARTS_B) {
        if (node >= 0x23 && node <= 0x2C) {
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2) {
                shown = 0;
            }
        } else if (node == 0x40) {
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2)) {
                shown = 0;
            }
        }
    } else if (obj->mdl.model->flags & BOBJ_MODEL_PARTS_C) {
        if (node >= 0x23 && node <= 0x2C) {
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3) {
                shown = 0;
            }
        } else if (node >= 0x15 && node <= 0x1E) {
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2) {
                shown = 0;
            }
        } else if (node == 0x40) {
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2)) {
                shown = 0;
            }
        } else if (node == 0x41) {
            if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3)) {
                shown = 0;
            }
        } else if (node == 0x42) {
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2) {
                shown = 0;
            }
        } else if (node == 0x43) {
            if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3) {
                shown = 0;
            }
        }
    } else if (node == 0x40) {
        if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2)) {
            shown = 0;
        }
    } else if (node == 0x41) {
        if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3)) {
            shown = 0;
        }
    } else if (node == 0x42) {
        if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part2) {
            shown = 0;
        }
    } else if (node == 0x43) {
        if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part3) {
            shown = 0;
        }
    } else if (node == 0x6F) {
        if (!(FLAG_BITS(obj)->visible && FLAG_BITS(obj)->part4)) {
            shown = 0;
        }
    } else if (node == 0x76) {
        if (FLAG_BITS(obj)->visible && FLAG_BITS(obj)->node76) {
            shown = 0;
        }
    }
    return shown;
}

/* Which side of the body a node id belongs to: 0 and 1 are the two sides, 2 the centre, 3 ids from 0x35 up. */
s32 BtlObj_GetNodeSide(s32 node) {
    if (node < 8) {
        return 2;
    }
    if (node < 0xC) {
        return 1;
    }
    if (node < 0x10) {
        return 0;
    }
    if (node < 0x12) {
        return 2;
    }
    if (node < 0x20) {
        return 1;
    }
    if (node < 0x2E) {
        return 0;
    }
    if (node < 0x35) {
        return 2;
    }
    return 3;
}

/* Stores every node's position in the object's frame (relative to node 0's vector when `relative` is set). */
void BtlObj_SaveNodePositions(BObj *obj, s32 relative) {
    Vec4 ref;
    BObjBound *bound;
    BObjPose *pose;
    BObjNode *node;

    bound = obj->mdl.bounds;
    pose = &obj->pose;
    if (relative) {
        Vec4_Copy(&ref, &BtlObj_GetNode(obj, 0)->unk90);
    } else {
        Vec4_SetZero(&ref);
    }
    while (1) {
        node = BtlObj_GetNode(obj, bound->node);
        Mtx_MulVec4(&node->prevPos, &pose->worldInv, (Vec4 *)node->world.m[3]);
        Vec4_Sub(&node->prevPos, &node->prevPos, &ref);
        if (bound->last != 0) {
            break;
        }
        bound = (BObjBound *)((u8 *)bound + bound->next);
    }
}

/* Movement of node `node` in the object's frame since the last BtlObj_SaveNodePositions; zero without the node. */
void BtlObj_GetNodeVelocity(BObj *obj, s32 node, Vec4 *out) {
    Vec4 pos;
    BObjNode *n;

    n = BtlObj_GetNode(obj, node);
    if (n == NULL) {
        Vec4_SetZero(out);
        return;
    }
    Mtx_MulVec4(&pos, &obj->pose.worldInv, (Vec4 *)n->world.m[3]);
    Vec4_Sub(out, &pos, &n->prevPos);
    out->w = 0.0f;
}

/* Returns the node for attachment code 0..18, NULL for other codes. */
BObjNode *BtlObj_GetNodeByCode(BObj *obj, u32 code) {
    switch (code) {
    case 0:
        return BtlObj_GetNode(obj, 3);
    case 1:
        return BtlObj_GetNode(obj, 7);
    case 2:
        return BtlObj_GetNode(obj, 0xA);
    case 3:
        return BtlObj_GetNode(obj, 0xB);
    case 4:
        return BtlObj_GetNode(obj, 0xE);
    case 5:
        return BtlObj_GetNode(obj, 0xF);
    case 6:
        return BtlObj_GetNode(obj, 0x13);
    case 7:
        return BtlObj_GetNode(obj, 0x14);
    case 8:
        return BtlObj_GetNode(obj, 0x15);
    case 9:
        return BtlObj_GetNode(obj, 0x21);
    case 10:
        return BtlObj_GetNode(obj, 0x22);
    case 11:
        return BtlObj_GetNode(obj, 0x23);
    case 12:
        return BtlObj_GetNode(obj, 0x2E);
    case 13:
        return BtlObj_GetNode(obj, 0x40);
    case 14:
        return BtlObj_GetNode(obj, 0x41);
    case 15:
        return BtlObj_GetNode(obj, 0);
    case 16:
        return BtlObj_GetNode(obj, 0x1F);
    case 17:
        return BtlObj_GetNode(obj, 0x2D);
    case 18:
        return BtlObj_GetNode(obj, 0x36);
    }
    return NULL;
}
