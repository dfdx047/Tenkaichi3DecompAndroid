/*
 * Battle stage, first part, continued: 0x240DB8..0x242D28 (split from stg_a.c at integration, around the three
 * VU0 macro-code rotations at 0x240C68..0x240DB8, which stay an assembly chunk; see the note at the end of stg_a.c).
 * Debris pieces, destructible objects, stage change, paths, file binding, placements, getters.
 */
#include "common.h"
#include "sys/math3d.h"
#include "sys/common.h"
#include "sys/heap.h"
#include "battle/battle.h"
#include "battle/battle_setup.h"
#include "battle/battle_work.h"
#include "battle/stg_a.h"

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);
extern f32 sqrtf(f32 x);
extern f32 atan2f(f32 y, f32 x);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *v, f32 s);
extern void Vec3_Scale(Vec4 *out, Vec4 *v, f32 s);
extern void Vec3_Normalize(Vec4 *out, Vec4 *v);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Mtx_MulVec4(Vec4 *out, Mtx44 *m, Vec4 *v);
extern void Vec4_Div(Vec4 *out, Vec4 *v, f32 d);      /* out = v / d */
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);         /* matrix copy */
extern void Vu0Cur_LoadIdentity(void);                           /* VU0: current matrix = identity */
extern void Vu0Cur_Translate(Vec4 *pos);                      /* VU0: translate the current matrix */
extern void Vu0Cur_StoreMtx(void *m);                        /* VU0: store the current matrix */

extern void *File_Request(s32 id, void *buf, s32 arg);
extern void StreamSe_PlayDefault(s32 se, s32 id);
extern void BtlCharApi_PlaySoundAt(Vec4 *pos, s32 kind, s32 id, f32 near, f32 far);
extern void BtlCharApi_RumbleNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
extern void BtlCharApi_ShakeCamsNear(Vec4 *pos, f32 near, f32 far, f32 power, f32 time);
extern s32 BtlCharApi_IsInputInjected(s32 objId);

/* Stage model code (0x114C60..0x115DE0) and stage code after this file. */
extern void BtlStage_Relocate(void *arg);
extern void StgModel_InitStage(void);
extern void StgModel_ResetAnims(void);
extern void StgModel_BindAnims(void);
extern void StgModel_ClearAnims(void);
extern void ColMesh_SetBase(void *arg);
extern void StgRigid_Reset(void);                           /* stage rigid bodies: reset */
extern void StgRigid_Init(void);                           /* stage rigid bodies: init */
extern void StgRigid_Term(void);                           /* stage rigid bodies: term */
extern s32 StgRigid_Create(Vec4 *pos, f32 radius, s32 arg);  /* new rigid body, handle or < 0 */
extern void StgRigid_Launch(s32 body, Vec4 *hitPos, s32 material);
extern void StgRigid_Release(s32 body);                       /* release a rigid body */
extern void StgRigid_GetMatrix(StgNode *node, s32 body);        /* node matrix = body transform */
extern void ColBox_SetCenterHalf(StgBox *out, Vec4 *center, StgVec16 *extent); /* box = center +- extent */
extern void StgAmb_Start(void);
extern void StgAmb_Stop(void);
extern s32 BtlStage_GetChangeTarget(void);
extern void StgFx_Reset(void);
extern void StgGround_Probe(s32 zone, StgBox *probe, StgGroundHit *out, f32 y);
extern void StgNav_UnblockObj(s32 idx);
extern void HudTimer_ShowMark(void);
extern void EftGndDust_SpawnDebris(Vec4 *pos, f32 a, f32 b, f32 scale); /* dust effect */
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftWater_AddSplashAt(Vec4 pos, f32 size);
#define EftWater_AddSplashAt(pos, size) EftWater_AddSplashAt(*(Vec4 *)(pos), size)
#else
extern void EftWater_AddSplashAt(Vec4 *pos, f32 size);                /* splash effect */
#endif

extern s32 gStgDbgState;       /* gStgDbgState */
extern s32 gStgDbgUnk;         /* gStgDbgUnk */
extern s32 gStgDbgExternFile;  /* gStgDbgExternFile */
extern StgPathTable *gStgPaths; /* gStgPaths */
extern StgMef *gBtlStageFxRes;        /* gBtlStageFxRes */
extern s32 gStgHasWater;       /* gStgHasWater */
extern f32 gStgWaterY;         /* gStgWaterY */
extern f32 gStgLastRadius;     /* gStgLastRadius */
extern f32 gStgLastTop;        /* gStgLastTop */
extern f32 gStgLastBottom;     /* gStgLastBottom */
extern StgPlace gStgDbgPlaces[6]; /* gStgDbgPlaces */
extern Vec4 gStgLastLightVecA; /* gStgLastLightVecA */
extern Vec4 gStgLastLightVecB;  /* gStgLastLightVecB */
extern Vec4 D_002EC2C0;        /* zero vector */
extern StgView *gBtlCamView;

#define STG_RES ((BattleRes *)gCommonRes->unk20)

void StgDbg_ClearState(void);
void StgDbg_BindFile(void);
void StgVu_RotateZ(f32 angle);
void StgVu_RotateX(f32 angle);
void StgVu_RotateY(f32 angle);

/* Both are defined in stg_a.c, the first half of the same original source file, so the original compiler had seen
 * their bodies before it reached the callers here and knew what they touch. The attributes stand in for that:
 * Stg_WrapRange is pure arithmetic on its arguments (StgPart_Animate only matches when its calls are scheduled as
 * calls that read and write no memory); StgNode_IsRigid only reads memory (BtlStage_UpdateObjs). */
extern f32 Stg_WrapRange(f32 lo, f32 hi, f32 v) __attribute__((const));
extern s32 StgNode_IsRigid(StgNode *node) __attribute__((pure));


/* Node matrix = rotation X, Y, Z then translation, through the VU0 matrix registers; returns the position. */
#define STG_NODE_BUILD()                          \
    Vu0Cur_LoadIdentity();                              \
    StgVu_RotateX(rot.x);                         \
    StgVu_RotateY(rot.y);                         \
    StgVu_RotateZ(rot.z);                         \
    Vu0Cur_Translate(&pos);                          \
    Vu0Cur_StoreMtx(node);                          \
    Vec4_Copy(out, (Vec4 *)node->mtx.m[3])

#define STG_ANGLE(deg) Stg_WrapRange(-3.14159265f, 3.14159265f, (deg) * 3.14159265f / 180.0f)

/* Poses a debris piece at `frame` of its key animation (position linear, angles through +3600 so that they
 * interpolate the short way), stores the matrix in its node and returns the position. Does nothing when the
 * frame falls between no pair of keys. */
void StgPart_Animate(StgPart *part, f32 frame, Vec4 *out) {
    Vec4 pos;
    Vec4 rot;
    StgNode *node = part->node;
    StgKey *keys = node->keys;
    s32 i;
    f32 t;

    rot.w = 1.0f;
    pos.w = 1.0f;
    if ((f32)node->endFrame <= frame) {
        pos.x = keys[node->keyCount - 1].x;
        pos.y = keys[node->keyCount - 1].y;
        pos.z = keys[node->keyCount - 1].z;
        pos.w = 1.0f;
        rot.x = STG_ANGLE(keys[node->keyCount - 1].rx);
        rot.y = STG_ANGLE(keys[node->keyCount - 1].ry);
        rot.z = STG_ANGLE(keys[node->keyCount - 1].rz);
        rot.w = 1.0f;
        STG_NODE_BUILD();
        return;
    }
    for (i = 1; i < node->keyCount; i++) {
        if ((f32)keys[i - 1].frame == frame) {
            pos.x = keys[i - 1].x;
            pos.y = keys[i - 1].y;
            pos.z = keys[i - 1].z;
            pos.w = 1.0f;
            rot.x = STG_ANGLE(keys[i - 1].rx);
            rot.y = STG_ANGLE(keys[i - 1].ry);
            rot.z = STG_ANGLE(keys[i - 1].rz);
            rot.w = 1.0f;
            STG_NODE_BUILD();
            return;
        }
        if ((f32)keys[i - 1].frame < frame && frame < (f32)keys[i].frame) {
            t = (frame - (f32)keys[i - 1].frame) / ((f32)keys[i].frame - (f32)keys[i - 1].frame);
            rot.x = STG_ANGLE(((keys[i].rx + 3600.0f) - (keys[i - 1].rx + 3600.0f)) * t + keys[i - 1].rx);
            rot.y = STG_ANGLE(((keys[i].ry + 3600.0f) - (keys[i - 1].ry + 3600.0f)) * t + keys[i - 1].ry);
            rot.z = STG_ANGLE(((keys[i].rz + 3600.0f) - (keys[i - 1].rz + 3600.0f)) * t + keys[i - 1].rz);
            rot.w = 1.0f;
            pos.x = (keys[i].x - keys[i - 1].x) * t + keys[i - 1].x;
            pos.y = (keys[i].y - keys[i - 1].y) * t + keys[i - 1].y;
            pos.z = (keys[i].z - keys[i - 1].z) * t + keys[i - 1].z;
            pos.w = 1.0f;
            STG_NODE_BUILD();
            return;
        }
        if (frame == (f32)keys[i].frame) {
            pos.x = keys[i].x;
            pos.y = keys[i].y;
            pos.z = keys[i].z;
            pos.w = 1.0f;
            rot.x = STG_ANGLE(keys[i].rx);
            rot.y = STG_ANGLE(keys[i].ry);
            rot.z = STG_ANGLE(keys[i].rz);
            rot.w = 1.0f;
            STG_NODE_BUILD();
            return;
        }
    }
}

/* Material of an object for its break effects: 0, 1 or 2 from type bits 0, 1, 2. */
#define STG_MATERIAL_OF(obj) \
    if ((obj)->type & STG_OBJ_MAT0) { \
        return 0; \
    } \
    if ((obj)->type & STG_OBJ_MAT1) { \
        return 1; \
    } \
    if ((obj)->type & STG_OBJ_MAT2) { \
        return 2; \
    } \
    return 0

/* Dust of a breaking object: -1 for none, else its material; *scale = 0.7 for the small kind, else 1. */
s32 StgObj_GetDust(StgObj *obj, f32 *scale) {
    s32 type = obj->type;
    s32 small = type & STG_OBJ_SMALL_DUST;

    type &= STG_OBJ_MAT0 | STG_OBJ_MAT1 | STG_OBJ_MAT2;
    if (type == 0) {
        return -1;
    }
    if (small) {
        *scale = 0.7f;
    } else {
        *scale = 1.0f;
    }
    STG_MATERIAL_OF(obj);
}

/* Material of an object: 0, 1 or 2. */
s32 StgObj_GetMaterial(StgObj *obj) {
    STG_MATERIAL_OF(obj);
}

/* For an object that may hold the hidden item: copies the position of object definition 0, returns 1. */
s32 StgObj_GetItemPos(Vec4 *out, StgObj *obj, s32 idx) {
    if (!(obj->type & STG_OBJ_ITEM)) {
        return 0;
    }
    Vec4_Copy(out, &gBtlStage->data->objDefs->pos);
    return 1;
}

/* Bit 0 of a part's flags, for a part that may be missing. The two variables (the pointer that is tested and
 * the one that is used) are what the original code generation shows: p is loaded into v0, then copied. */
#define STG_PART_SET(part)        \
    p = (part);                   \
    if (p != NULL) {              \
        q = p;                    \
        q->flags |= 1;            \
    }
#define STG_PART_CLEAR(part)      \
    p = (part);                   \
    if (p != NULL) {              \
        q = p;                    \
        q->flags &= ~1;           \
    }

/* Breaks object idx and, recursively, every object that names it as parent: swaps the whole model for the
 * broken one, starts the debris animation, shakes cameras and pads near it. Returns 0 if it was already broken. */
s32 BtlStage_BreakObj(s32 idx, Vec4 *hitPos, StgBreakInfo *info) {
    Vec4 pos;
    StgObj *objs = gBtlStage->objs;
    StgObj *obj = (StgObj *)(idx * (s32)sizeof(StgObj) + (s32)objs);
    StgPart *part = NULL;
    StgPart *p;
    StgPart *q;
    u32 i;

    if (obj->state & STG_OBJ_BROKEN) {
        return 0;
    }
    p = obj->parts[0];
    if (p != NULL) {
        q = p;
        q->flags &= ~1;
        part = q;
    }
    STG_PART_CLEAR(obj->parts[2]);
    STG_PART_CLEAR(obj->parts[4]);
    STG_PART_SET(obj->parts[1]);
    STG_PART_SET(obj->parts[3]);
    STG_PART_SET(obj->parts[5]);
    obj->hp = 0;
    obj->state = STG_OBJ_BROKEN | STG_OBJ_FALLING;
    obj->frame = 0;
    Vec4_Copy(&obj->hitPos, hitPos);
    if (part != NULL) {
        /* Declaration order and the repeated `far` / `rumbleFar` assignments in every arm below are what the
         * original code generation shows (which register holds which value, and 1500 / 1000 being reloaded in
         * the small-shake arm only): do not simplify them. */
        s32 shake = 0;
        f32 time = 0.0f;
        f32 power = 0.0f;
        f32 rumbleTime = 0.0f;
        f32 rumblePower = 0.0f;
        f32 near = 100.0f;
        f32 far = 1500.0f;
        f32 rumbleFar = 1000.0f;
        f32 r;

        info->types |= obj->type;
        info->radius = part->radius;
        Vec4_Set(&info->pos, part->x, part->y, part->z, 1.0f);
        info->count++;
        if (obj->type & (STG_OBJ_SHAKE_S | STG_OBJ_SHAKE_M | STG_OBJ_SHAKE_L)) {
            Vec4_Set(&pos, part->x, part->y, part->z, 1.0f);
            r = part->radius;
            if (obj->type & STG_OBJ_SHAKE_L) {
                power = 3.0f;
                rumblePower = 1.0f;
                time = 0.7f;
                shake = 1;
                rumbleTime = 0.3f;
                near = 300.0f;
                far = 1500.0f;
                rumbleFar = 1000.0f;
            }
            if (obj->type & STG_OBJ_SHAKE_M) {
                power = 2.0f;
                shake = 1;
                time = 0.5f;
                rumblePower = 0.8f;
                rumbleTime = 0.2f;
                near = 100.0f;
                far = 1500.0f;
                rumbleFar = 1000.0f;
            }
            if (obj->type & STG_OBJ_SHAKE_S) {
                time = 0.3f;
                power = 1.0f;
                rumblePower = 0.5f;
                shake = 1;
                rumbleTime = 0.1f;
                near = 100.0f;
                far = 1500.0f;
                rumbleFar = 1000.0f;
            }
            if (shake) {
                near += r;
                far += r;
                rumbleFar += r;
                BtlCharApi_ShakeCamsNear(&pos, near, far, power, time);
                BtlCharApi_RumbleNear(&pos, near, rumbleFar, rumblePower, rumbleTime);
            }
        }
    }
    for (i = 0; i < gBtlStage->objCount; i++) {
        if (objs[i].parent == idx) {
            BtlStage_BreakObj(i, hitPos, info);
        }
    }
    return 1;
}

/* Puts every object in one state: broken != 1 restores them whole (hit points, type and parent reloaded from the
 * stage data); broken == 1 marks them all broken without any animation. */
void BtlStage_ResetObjs(s32 broken) {
    StgObj *objs = gBtlStage->objs;
    StgObj *obj;
    StgPart *p;
    StgPart *q;
    u32 i;

    if (broken == 1) {
        obj = objs;
        for (i = 0; i < gBtlStage->objCount; i++, obj++) {
            STG_PART_CLEAR(obj->parts[0]);
            STG_PART_CLEAR(obj->parts[2]);
            STG_PART_CLEAR(obj->parts[4]);
            STG_PART_SET(obj->parts[1]);
            STG_PART_SET(obj->parts[3]);
            STG_PART_SET(obj->parts[5]);
            obj->state = broken;
            obj->frame = 0;
            obj->hp = 0;
        }
    } else {
        for (i = 0; i < gBtlStage->objCount; i++) {
            obj = &objs[i];
            STG_PART_SET(obj->parts[0]);
            STG_PART_SET(obj->parts[2]);
            STG_PART_SET(obj->parts[4]);
            STG_PART_CLEAR(obj->parts[1]);
            STG_PART_CLEAR(obj->parts[3]);
            STG_PART_CLEAR(obj->parts[5]);
            obj->state = broken;
            obj->frame = 0;
            StgObj_LoadDef(obj, i);
        }
    }
}

/* Breaks object idx for the fighter / effect objId (< 0: nobody), with the break sound. In mode 1, when the
 * rule holds a hidden item and the object is an item holder, one rand() draw in 8 awards it. Always returns 1. */
s32 BtlStage_DestroyObj(s32 objId, s32 idx, Vec4 *hitPos) {
    StgBreakInfo info;
    Vec4 itemPos;
    s32 broke;
    StgObj *objs;
    BattleRule *rule;
    BattleResult *result;

    memset(&info, 0, sizeof(info));
    StgNav_UnblockObj(idx);
    broke = BtlStage_BreakObj(idx, hitPos, &info);
    if (info.count != 0) {
        BtlCharApi_PlaySoundAt(&info.pos, 1, 8, info.radius + 200.0f, info.radius + 1500.0f);
    }
    if (broke != 0) {
        if (objId >= 0) {
            objs = gBtlStage->objs;
            rule = &Battle_GetSetup()->rule;
            result = BattleResult_GetPtr();
            if (!BtlCharApi_IsInputInjected(objId)) {
                if (rule->unk18 != 0) {
                    if (StgObj_GetItemPos(&itemPos, &objs[idx], idx)) {
                        if (((rand() >> 8) & 7) == 0) {
                            result->unk44 = rule->unk18;
                            rule->unk18 = 0;
                            StreamSe_PlayDefault(0, 0x8D3A);
                            HudTimer_ShowMark();
                        }
                    }
                }
            }
        }
    }
    return 1;
}

/* 1 when object idx is broken. */
s32 BtlStage_IsObjBroken(s32 idx) {
    if (gBtlStage->objs[idx].state & STG_OBJ_BROKEN) {
        return 1;
    }
    return 0;
}

/* Takes `damage` hit points from object idx; 1 when it has none left (the caller then breaks it). 0 if broken. */
s32 BtlStage_DamageObj(s32 idx, s32 damage) {
    StgObj *obj = (StgObj *)(idx * (s32)sizeof(StgObj) + (s32)gBtlStage->objs);

    if (obj->state & STG_OBJ_BROKEN) {
        return 0;
    }
    obj->hp -= damage;
    return obj->hp <= 0;
}

/* Type bits of object idx, 0 once it is broken. */
s32 BtlStage_GetObjType(s32 idx) {
    StgObj *obj = (StgObj *)(idx * (s32)sizeof(StgObj) + (s32)gBtlStage->objs);

    if (obj->state & STG_OBJ_BROKEN) {
        return 0;
    }
    return obj->type;
}

/* Per frame (not while paused): advances the debris of every object that is falling. */
/* Written as the original was, which the code generation shows: the object is indexed (`objs[i]`) at every use
 * instead of through an element pointer (the compiler then keeps several copies of that pointer and runs out of
 * saved registers: the index and its byte offset live on the stack), `first` is set in both arms of an if / else,
 * and the dust cases 0 and 1 are separate although identical. */
void BtlStage_UpdateObjs(void) {
    Vec4 pos;
    Vec4 prev;
    Vec4 center;
    f32 scale;
    StgObj *objs = gBtlStage->objs;
    StgPart *part;
    StgNode *node;
    u32 i;
    u32 j;
    s32 first;

    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    for (i = 0; i < gBtlStage->objCount; i++) {
        if (!(objs[i].state & STG_OBJ_FALLING)) {
            continue;
        }
        if ((f32)objs[i].frame == 0.0f) {
            first = 1;
        } else {
            first = 0;
        }
        objs[i].frame = (f32)objs[i].frame + 2.0f;
        if (objs[i].animEnd == 0) {
            objs[i].animEnd = 0x94;
        }
        if ((f32)objs[i].animEnd <= (f32)objs[i].frame) {
            objs[i].frame = 0;
            objs[i].state ^= STG_OBJ_FALLING;
            for (j = 0; j < objs[i].pieceCount; j++) {
                part = objs[i].pieces[j];
                node = part->node;
                if (StgNode_IsRigid(node)) {
                    if (node->body >= 0) {
                        StgRigid_Release(node->body);
                    }
                }
            }
            continue;
        }
        if (first) {
            Vec4_Copy(&center, &D_002EC2C0);
            for (j = 0; j < objs[i].pieceCount; j++) {
                part = objs[i].pieces[j];
                if (part->node != NULL) {
                    StgPart_Animate(part, 0.0f, &pos);
                    Vec4_Add(&center, &center, &pos);
                }
            }
            Vec3_Scale(&center, &center, 1.0f / (f32)objs[i].pieceCount);
        }
        for (j = 0; j < objs[i].pieceCount; j++) {
            part = objs[i].pieces[j];
            node = part->node;
            if (node == NULL) {
                continue;
            }
            if (!first) {
                StgPart_GetPos(part, &prev);
            }
            if (StgNode_IsRigid(node)) {
                if (first) {
                    StgPart_Animate(part, 0.0f, &pos);
                    node->body = StgRigid_Create(&pos, part->radius, 0);
                    if (node->body >= 0) {
                        StgRigid_Launch(node->body, &objs[i].hitPos, StgObj_GetMaterial(&objs[i]));
                    }
                }
                StgRigid_GetMatrix(node, node->body);
            } else {
                node->body = 0;
                StgPart_Animate(part, (f32)objs[i].frame, &pos);
            }
            if (first) {
                switch (StgObj_GetDust(&objs[i], &scale)) {
                case 0:
                    if ((j & 3) == 0) {
                        EftGndDust_SpawnDebris(&pos, 1.0f, 1.0f, scale);
                    }
                    break;
                case 1:
                    if ((j & 3) == 0) {
                        EftGndDust_SpawnDebris(&pos, 1.0f, 1.0f, scale);
                    }
                    break;
                case 2:
                    if ((j & 3) == 0) {
                        EftGndDust_SpawnDebris(&pos, 1.0f, 1.0f, scale);
                    }
                    break;
                }
                if (first) {
                    continue;
                }
            }
            StgPart_GetPos(part, &pos);
            if (objs[i].type & STG_OBJ_SPLASH) {
                if (prev.y < 0.0f && 0.0f < pos.y) {
                    EftWater_AddSplashAt(&prev, 3.0f);
                }
            }
        }
    }
}

/* Asks the loader for the stage this one changes into (BtlStage_GetChangeTarget). */
void BtlStage_RequestChange(void) {
    BtlLoad_RequestStageChange(BtlStage_GetChangeTarget());
}

/* Finds the path table in the stage file (header entry 24) and relocates it. */
void BtlStage_BindPaths(void) {
    u8 *file = STG_RES->stage;

    gStgPaths = (StgPathTable *)(file + ((u32 *)file)[0x18] / 4 * 4);
    StgPathTable_Relocate(gStgPaths);
}

/* Empty. */
void BtlStage_PathStub(void) {
}

/* Forgets the path table. */
void BtlStage_ClearPaths(void) {
    gStgPaths = NULL;
}

/* Number of stage paths, 0 before the table is bound. */
s32 BtlStage_GetPathCount(void) {
    if (gStgPaths != NULL && gStgPaths->relocated != 0) {
        return gStgPaths->count;
    }
    return 0;
}

/* Stage path n, NULL when out of range. */
StgPath *BtlStage_GetPath(s32 n) {
    if (gStgPaths != NULL) {
        s32 count = gStgPaths->count;
        StgPath *paths = gStgPaths->paths;

        if (n < count) {
            return &paths[n];
        }
    }
    return NULL;
}

/* Stage path 0 (no caller). */
StgPath *BtlStage_GetPath0(void) {
    return BtlStage_GetPath(0);
}

static inline void Stg_Normalize(Vec4 *v) {
    f32 len = sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);

    if (len > 0.0f) {
        v->x /= len;
        v->y /= len;
        v->z /= len;
    } else {
        v->z = 0.0f;
        v->x = 0.0f;
        v->y = 0.0f;
    }
}

/* Yaw that faces from (x0, z0) to (x1, z1), in -pi..pi. */
f32 Stg_CalcYaw(f32 x0, f32 z0, f32 x1, f32 z1) {
    Vec4 d;
    f32 a;

    memset(&d, 0, sizeof(d));
    d.x = x0 - x1;
    d.y = 1.0f;
    d.w = 1.0f;
    d.z = z0 - z1;
    Stg_Normalize(&d);
    a = atan2f(d.x, d.z) - 3.14159265f;
    if (a > 3.14159265f) {
        a -= 6.2831853f;
    }
    if (a < -3.14159265f) {
        a += 6.2831853f;
    }
    return a;
}

/* Relocates the three point lists of the "MEF0" block and negates y and z of every point. */
void BtlStage_BindMef(void) {
    StgMef *m = gBtlStageFxRes;
    StgMefPoint *p;
    s32 i;

    if (m != NULL && m->magic == 0x3046454D) {
        m->list0 = (StgMefPoint *)((u8 *)m + (s32)m->list0);
        gBtlStageFxRes->list1 = (StgMefPoint *)((u8 *)m + (s32)gBtlStageFxRes->list1);
        gBtlStageFxRes->list2 = (StgMefPoint *)((u8 *)m + (s32)gBtlStageFxRes->list2);
        for (i = 0, p = gBtlStageFxRes->list0; i < gBtlStageFxRes->count0; i++, p++) {
            p->y = -p->y;
            p->z = -p->z;
        }
        for (i = 0, p = gBtlStageFxRes->list1; i < gBtlStageFxRes->count1; i++, p++) {
            p->y = -p->y;
            p->z = -p->z;
        }
        p = gBtlStageFxRes->list2;
        for (i = 0; i < gBtlStageFxRes->count2; i++) {
            p[i].y = -p[i].y;
            p[i].z = -p[i].z;
        }
    }
}

#define STG_RELOC(field) d->field = (void *)((u8 *)base + (s32)d->field)

/* Binds the stage parameter block: turns its offsets into pointers and negates y and z of every position. */
void BtlStage_BindData(BtlStage *stage, StgDataA *base) {
    StgDataA *d;
    s32 i;

    if (base != NULL) {
        stage->data = base;
        d = gBtlStage->data;
        STG_RELOC(objDefs);
        STG_RELOC(env);
        STG_RELOC(list88);
        STG_RELOC(light);
        STG_RELOC(points);
        STG_RELOC(starts);
        STG_RELOC(altStarts);
        STG_RELOC(places);
        STG_RELOC(list40);
        STG_RELOC(list48);
        STG_RELOC(list50);
        STG_RELOC(list90);
        STG_RELOC(listA0);
        STG_RELOC(list98);
        STG_RELOC(list60);
        STG_RELOC(list68);
        STG_RELOC(list70);
        STG_RELOC(list58);
        STG_RELOC(list78);
        STG_RELOC(list80);
        for (i = 0; i < d->objCount; i++) {
            d->objDefs[i].pos.y = -d->objDefs[i].pos.y;
            d->objDefs[i].pos.z = -d->objDefs[i].pos.z;
        }
        for (i = 0; i < d->envCount; i++) {
            d->env[i].waterY = -d->env[i].waterY;
        }
        for (i = 0; i < d->lightCount; i++) {
            d->light[i].dirA.y = -d->light[i].dirA.y;
            d->light[i].dirA.z = -d->light[i].dirA.z;
            d->light[i].dir.y = -d->light[i].dir.y;
            d->light[i].dir.z = -d->light[i].dir.z;
        }
        for (i = 0; i < d->pointCount; i++) {
            d->points[i].y = -d->points[i].y;
            d->points[i].z = -d->points[i].z;
        }
        for (i = 0; i < d->startCount; i++) {
            d->starts[i].pos.y = -d->starts[i].pos.y;
            d->starts[i].pos.z = -d->starts[i].pos.z;
            d->starts[i].target.y = -d->starts[i].target.y;
            d->starts[i].target.z = -d->starts[i].target.z;
        }
        for (i = 0; i < d->altCount; i++) {
            d->altStarts[i].pos.y = -d->altStarts[i].pos.y;
            d->altStarts[i].pos.z = -d->altStarts[i].pos.z;
            d->altStarts[i].target.y = -d->altStarts[i].target.y;
            d->altStarts[i].target.z = -d->altStarts[i].target.z;
        }
        for (i = 0; i < d->placeCount; i++) {
            d->places[i].pos.y = -d->places[i].pos.y;
            d->places[i].pos.z = -d->places[i].pos.z;
            d->places[i].target.y = -d->places[i].target.y;
            d->places[i].target.z = -d->places[i].target.z;
        }
        for (i = 0; i < d->count60; i++) {
            d->list60[i].y = -d->list60[i].y;
            d->list60[i].z = -d->list60[i].z;
        }
        for (i = 0; i < d->count58; i++) {
            d->list58[i].pos.y = -d->list58[i].pos.y;
            d->list58[i].pos.z = -d->list58[i].pos.z;
        }
    }
}

/* Binds the three blocks of the stage file to the stage, once per file. */
void BtlStage_BindFile(BtlStage *stage) {
    BattleRes *res = STG_RES;
    u32 *file;
    u8 *data;
    u32 ofs;

    if (stage->data == NULL) {
        file = res->stage;
        gBtlStageFxRes = NULL;
        if (file == NULL) {
            return;
        }
        data = (u8 *)file + file[2] / 4 * 4;
        ofs = file[0x13] / 4 * 4;
        if (file[0x14] != file[0x13]) {
            gBtlStageFxRes = (StgMef *)((u8 *)file + ofs);
        }
        BtlStage_BindData(stage, (StgDataA *)data);
        BtlStage_BindMef();
        BtlStage_BindPaths();
    }
}

/* Water: returns 1 and the level when the stage has water, else 0 and 0. Keeps the last answer while the stage
 * is not ready. */
s32 BtlStage_GetWaterLevel(f32 *level) {
    if (BtlStage_IsReady()) {
        StgDataA *d = gBtlStage->data;
        StgEnv *env;

        if (d == NULL) {
            *level = gStgWaterY;
            return gStgHasWater;
        }
        env = d->env;
        if (env->flags & 2) {
            gStgHasWater = 1;
            gStgWaterY = env->waterY;
        } else {
            gStgHasWater = 0;
            gStgWaterY = 0.0f;
        }
    }
    *level = gStgWaterY;
    return gStgHasWater;
}

/* Ground under pos: finds its zone (starting from `zone`) and asks the collision code (StgGround_Probe) for the
 * ground point of a unit box around pos. Returns the zone; out is untouched while the stage is not ready. */
s32 BtlStage_ProbeGround(s32 zone, Vec4 *pos, StgGroundHit *out) {
    StgBox box;
    StgVec16 ext = { 1.0f, 1.0f, 1.0f, 1.0f };
    StgVec16 *e = &ext;
    s32 r;

    if (!BtlStage_IsReady()) {
        return zone;
    }
    memset(out, 0, sizeof(StgGroundHit));
    r = BtlStage_FindZoneNear(zone, pos);
    out->x = pos->x;
    out->y = pos->y;
    out->z = pos->z;
    ColBox_SetCenterHalf(&box, pos, e);
    StgGround_Probe(r, &box, out, box.min[1]);
    return r;
}

/* Start placement of a player: the stage's start point for `side` (the single alternative point when alt == 1),
 * dropped onto the ground from y = -700, with the yaw that faces the point's target. Returns the zone of the
 * point (the fighter keeps it as its "area"). Reads gBtlStage->data even when the stage is not ready. */
s32 BtlStage_GetStartPlace(s32 side, Vec4 *pos, Vec4 *rot, s32 alt) {
    StgGroundHit hit;
    s32 zone;
    StgDataA *d;
    StgPlace *starts;
    StgPlace *p;
    f32 yaw;

    BtlStage_IsReady();
    d = gBtlStage->data;
    p = d->altStarts;
    starts = d->starts;
    if (alt == 1) {
        yaw = Stg_CalcYaw(p->pos.x, p->pos.z, p->target.x, p->target.z);
        pos->x = p->pos.x;
        pos->y = -700.0f;
        pos->z = p->pos.z;
        pos->w = 1.0f;
    } else {
        p = (StgPlace *)(side * (s32)sizeof(StgPlace) + (s32)starts);
        yaw = Stg_CalcYaw(p->pos.x, p->pos.z, p->target.x, p->target.z);
        pos->x = p->pos.x;
        pos->y = -700.0f;
        pos->z = p->pos.z;
        pos->w = 1.0f;
    }
    rot->y = yaw;
    rot->w = 1.0f;
    rot->z = 0.0f;
    rot->x = 0.0f;
    zone = BtlStage_ProbeGround(-1, pos, &hit);
    pos->y = hit.y;
    return zone;
}

/* The stage's third placement (StgDataA +0x3C, first entry), dropped onto the ground like a start placement;
 * returns its zone. The fighter uses it for the placement requests 0xF4 / 0xF6 / 0xF7. */
s32 BtlStage_GetPlace(Vec4 *pos, Vec4 *rot) {
    StgGroundHit hit;
    s32 zone;
    StgPlace *p;
    f32 yaw;

    BtlStage_IsReady();
    p = gBtlStage->data->places;
    yaw = Stg_CalcYaw(p->pos.x, p->pos.z, p->target.x, p->target.z);
    pos->x = p->pos.x;
    pos->y = -700.0f;
    pos->z = p->pos.z;
    pos->w = 1.0f;
    rot->w = 1.0f;
    rot->y = yaw;
    rot->z = 0.0f;
    rot->x = 0.0f;
    zone = BtlStage_ProbeGround(-1, pos, &hit);
    pos->y = hit.y;
    return zone;
}

/* First vector of the stage light; the last one read while the stage is not ready. */
void BtlStage_GetLightVecA(Vec4 *out) {
    StgLight *l;

    if (!BtlStage_IsReady()) {
        Vec4_Copy(out, &gStgLastLightVecA);
        return;
    }
    l = gBtlStage->data->light;
    out->x = l->dirA.x;
    out->y = l->dirA.y;
    out->z = l->dirA.z;
    out->w = 1.0f;
    Vec4_Copy(&gStgLastLightVecA, out);
}

/* Second vector of the stage light (the battle object light direction); the last one read while not ready. */
void BtlStage_GetLightVecB(Vec4 *out) {
    StgLight *l;

    if (!BtlStage_IsReady()) {
        Vec4_Copy(out, &gStgLastLightVecB);
        return;
    }
    l = gBtlStage->data->light;
    out->x = l->dir.x;
    out->y = l->dir.y;
    out->z = l->dir.z;
    out->w = 1.0f;
    Vec4_Copy(&gStgLastLightVecB, out);
}

/* Reloads an object's hit points, type and parent from its definition. */
void StgObj_LoadDef(StgObj *obj, s32 idx) {
    StgObjDef *def;

    BtlStage *stg = gBtlStage;

    if (stg == NULL || stg->data != NULL) {
        def = (StgObjDef *)(idx * (s32)sizeof(StgObjDef) + (s32)stg->data->objDefs);
        obj->hp = def->hp;
        obj->type = def->type;
        obj->parent = def->parent;
    }
}

/* Object definition idx, NULL when out of range. */
StgObjDef *BtlStage_GetObjDef(s32 idx) {
    StgDataA *d;

    if (gBtlStage == NULL) {
        return NULL;
    }
    d = gBtlStage->data;
    if (d == NULL) {
        return NULL;
    }
    if (!(idx < d->objCount)) {
        return NULL;
    }
    return &d->objDefs[idx];
}

/* Entry idx of the list at StgDataA +0x44, NULL when out of range. */
StgVec *BtlStage_GetList40(s32 idx) {
    StgDataA *d;

    if (gBtlStage == NULL) {
        return NULL;
    }
    d = gBtlStage->data;
    if (d == NULL) {
        return NULL;
    }
    if (!(idx < d->count40)) {
        return NULL;
    }
    return &d->list40[idx];
}

#define STG_LIST_GETTER(name, count, list)   \
    void *name(void) {                       \
        StgDataA *d;                          \
                                             \
        if (gBtlStage == NULL) {             \
            return NULL;                     \
        }                                    \
        d = gBtlStage->data;                 \
        if (d == NULL) {                     \
            return NULL;                     \
        }                                    \
        if (d->count > 0) {                  \
            return d->list;                  \
        }                                    \
        return NULL;                         \
    }

/* The lists of the stage data that have no index: the pointer when the count is positive, else NULL. */
STG_LIST_GETTER(BtlStage_GetList48, count48, list48)
STG_LIST_GETTER(BtlStage_GetList50, count50, list50)
STG_LIST_GETTER(BtlStage_GetList90, count90, list90)
STG_LIST_GETTER(BtlStage_GetListA0, countA0, listA0)
STG_LIST_GETTER(BtlStage_GetList98, count98, list98)
STG_LIST_GETTER(BtlStage_GetList60, count60, list60)
STG_LIST_GETTER(BtlStage_GetList68, count68, list68)
STG_LIST_GETTER(BtlStage_GetList58, count58, list58)
STG_LIST_GETTER(BtlStage_GetLightColors, count78, list78)
STG_LIST_GETTER(BtlStage_GetList80, count80, list80)
