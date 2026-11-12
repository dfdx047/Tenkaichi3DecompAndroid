#include "common.h"
#include "battle/btl_capi_a.h"

/*
 * Fighter interface, first part: 0x204E78..0x207020. 81 accessors keyed by battle object id (a few by roster
 * index), used by the AI, the effect scene and its modules, the camera, the HUD and the fighter code itself. See
 * include/battle/btl_capi_a.h; the same original file continues in btl_char_api.c at 0x207020.
 *
 * Float pool: this file owns .lit4 0x2FE07C..0x2FE0C8 (20 constants, the first of the original object's pool).
 *
 * Shapes that matter for matching here:
 *   - a getter is `if (x != NULL) { return ...; } return 0;` at every nesting level (the original returns the null
 *     pointer register itself);
 *   - a call followed by `return;` is a tail call, a call at the end of an if / else arm is not: the functions
 *     below that end with `if (obj == NULL) { ...; return; }` are written that way for the one call the original
 *     makes with jal;
 *   - the action-id tests are switches (slti ladders), not range compares.
 *
 * Callees still named by address: BtlObj_GetNode(obj, node) model node or NULL, BtlObj_FindBound(obj, n),
 * BtlObj_GetNodeVelocity(obj, node, out), BtlObj_AddPush(obj, vec, arg), Vec4_SetZeroW1 / Vec4_SetZero zero a vector (the
 * first is used for positions, the second for directions and rotations), Mtx_Copy copies a matrix,
 * Vec3_RotateAxis(out, v, axis, angle) turns v about axis.
 */

extern BtlCapiMgr *gBtlChars;
extern f32 sqrtf(f32 x);

extern s32 BtlChar_GetCount(void);
extern BtlCapiChr *BtlChar_Get(s32 idx);
extern BtlCapiChr *BtlChar_FindByObjId(s32 objId);
extern BtlCapiObj *BtlObj_Get(s32 objId);
extern BtlCapiObj *BtlChar_GetObj(BtlCapiChr *chr);
extern BtlCapiPose *BtlChar_GetPos(BtlCapiChr *chr);
extern s32 BtlChar_TestFlag(BtlCapiChr *chr, s32 flag);
extern s32 BtlChar_IsFrozen(BtlCapiChr *chr);
extern f32 BtlChar_RandF(void);
extern f32 BtlChar_GetSpacing(BtlCapiChr *chr, s32 mode); /* own size + the other fighter's + 30 (40 with flag 0x13) */
extern void *BtlMember_GetActive(BtlCapiChr *chr);
extern s32 BtlOpp_GetObjId(BtlCapiChr *chr);
extern void BtlOpp_GetNodePos(BtlCapiChr *chr, s32 node, Vec4 *out);
extern void BtlOpp_GetDelta(BtlCapiChr *chr, Vec4 *out);
extern s32 BtlAct_GetCurrent(BtlCapiChr *chr);
extern s32 BtlAct_GetPrev(BtlCapiChr *chr);
extern s32 BtlAct_IsTechniqueId(s32 action);          /* action is 0x105..0x132: a technique action */
extern s32 BtlAct_GetCurrentClass(BtlCapiChr *chr);
extern s32 BtlAct_GetMotionLevel(BtlCapiChr *chr, s32 anim);
extern s32 BtlAnim_GetId(BtlCapiChr *chr);
extern s32 BtlAnim_GetFlags(s32 anim);
extern s32 BtlParam_GetFlags(BtlCapiChr *chr);    /* parameter word +0x10 */
extern s32 BtlParam_GetAuraKind(BtlCapiChr *chr); /* parameter byte +3, replaced by abilities 0x4D.. */
extern s32 BtlSuper_GetFlags(BtlCapiChr *chr, s32 slot); /* attribute word of technique `slot` */
extern BtlCapiNode *BtlObj_GetNode(BtlCapiObj *obj, s32 node);
extern BtlCapiPart *BtlObj_FindBound(BtlCapiObj *obj, s32 n);
extern void BtlObj_GetNodeVelocity(BtlCapiObj *obj, s32 node, Vec4 *out);
extern void BtlObj_AddPush(BtlCapiObj *obj, Vec4 *v, f32 arg);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec4_SetZeroW1(Vec4 *dst);
extern void Vec4_SetZero(Vec4 *dst);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Vec3_RotateAxis(Vec4 *out, Vec4 *v, Vec4 *axis, f32 angle);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern f32 Mathf_Sin(f32 angle);
extern f32 Mathf_Cos(f32 angle);
extern f32 BtlUtil_ClampF(f32 v, f32 lo, f32 hi);
extern f32 BtlUtil_LengthXZ(Vec4 *v);
extern void BtlUtil_WrapAngles(Vec4 *dst, Vec4 *src);
extern void Quat_ToEuler(Vec4 *out, Quat *q);
extern void Quat_SetIdentity(Quat *q);

/* Character id of the object (object +0xC), 0 for no object. */
s32 BtlCharApi_GetChara(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->chara;
    }
    return 0;
}

/* Height of the character model in world units (object +0xFF4, from the model header); 10 for no object. */
f32 BtlCharApi_GetHeight(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->height;
    }
    return 10.0f;
}

/* Object float +0xFF8 (model header +0x1C); 10 for no object. */
f32 BtlCharApi_GetBodyUnkFF8(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->unkFF8;
    }
    return 10.0f;
}

/* Object float +0xFFC (model header +0x20): what a placement adds to a path point's y, so the distance from the
   placed point down to the origin; 5 for no object. */
f32 BtlCharApi_GetCenterHeight(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->centerHeight;
    }
    return 5.0f;
}

/* Object float +0x1000 (model header +0x24); 5 for no object. No caller. */
f32 BtlCharApi_GetBodyUnk1000(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->unk1000;
    }
    return 5.0f;
}

/* The 16-bit flag word at +0 of the character's parameter block. */
s32 BtlCharApi_ObjGetParamFlags0(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->param->flags0;
    }
    return 0;
}

/* The flag word at +0x10 of the character's parameter block. No caller. */
s32 BtlCharApi_ObjGetParamFlags(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return obj->param->flags;
    }
    return 0;
}

/* Aura type of the character: 3 while object flag 0x40000 is set, else the parameter byte +3 (for a fighter: as
   replaced by its abilities, BtlParam_GetAuraKind). */
s32 BtlCharApi_GetAuraType(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiObj *obj;

    if (chr != NULL) {
        if (BtlChar_GetObj(chr)->u.flags & 0x40000) {
            return 3;
        }
        return BtlParam_GetAuraKind(chr);
    }
    obj = BtlObj_Get(objId);
    if (obj != NULL) {
        if (obj->param == NULL) {
            return 0;
        }
        if (obj->u.flags & 0x40000) {
            return 3;
        }
        return obj->param->auraType;
    }
    return 0;
}

/* Float +0xC8 of the character's parameter block. No caller. */
f32 BtlCharApi_GetParamUnkC8(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL && obj->param != NULL) {
        return obj->param->unkC8;
    }
    return 0.0f;
}

/* Costume of the fighter's active member. */
s32 BtlCharApi_GetCostume(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return ((s32 *)BtlMember_GetActive(chr))[1];
    }
    return 0;
}

/* Both ki blast functions need three plain early returns (object NULL, table NULL, found): the `return 0` of the
   NULL table then keeps a jump with a barrier in front of the loop, the `return 1` block is moved behind it, and
   cross-jumping turns the entry into a jump to the test at the bottom of the loop. A nested `if (e != NULL)` gives
   the same instructions in another block layout. */
/* Whether one of the character's 13 ki blast records (object +0x924) has type 2. */
s32 BtlCharApi_HasKiBlastType2(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiKiBlast *e;
    s32 i;

    if (obj == NULL) {
        return 0;
    }
    e = obj->kiBlasts;
    if (e == NULL) {
        return 0;
    }
    for (i = 0; i < 13; i++) {
        if (e[i].type == 2) {
            return 1;
        }
    }
    return 0;
}

/* Whether one of the character's 13 ki blast records has type 3. */
s32 BtlCharApi_HasKiBlastType3(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiKiBlast *e;
    s32 i;

    if (obj == NULL) {
        return 0;
    }
    e = obj->kiBlasts;
    if (e == NULL) {
        return 0;
    }
    for (i = 0; i < 13; i++) {
        if (e[i].type == 3) {
            return 1;
        }
    }
    return 0;
}

/* Whether the object id is a fighter's. No caller. */
s32 BtlCharApi_IsFighter(s32 objId) {
    return BtlChar_FindByObjId(objId) != NULL;
}

/* Object id of fighter `player`, or `player` itself when there is none. No caller. */
s32 BtlCharApi_GetObjIdOfPlayer(s32 player) {
    BtlCapiChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        return chr->objId;
    }
    return player;
}

/* Player index of the fighter (fighter +0), -1 for a non-fighter. No caller. */
s32 BtlCharApi_GetPlayer(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->player;
    }
    return -1;
}

/* Object id of the fighter's opponent. */
s32 BtlCharApi_GetOpponentObjId(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlOpp_GetObjId(chr);
    }
    return 0;
}

/* Object id of the fighter's partner object (fighter +0x1338) while it has one (+0x1330), else objId itself. */
s32 BtlCharApi_GetPartnerObjId(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);


    if (chr == NULL) {
        return objId;
    }
    if (chr->partnerOn == 0) {
        return objId;
    }
    return chr->partnerObjId;
}

/* Pointer n (0..4) of the five at +0x9C of fighter `player`'s object. */
s32 BtlCharApi_GetPlayerObjUnk9C(s32 player, u32 n) {
    BtlCapiChr *chr = BtlChar_Get(player);
    BtlCapiObj *obj;

    if (chr != NULL) {
        obj = BtlChar_GetObj(chr);
        if (obj != NULL) {
            if (n >= 5) {
                return 0;
            }
            return obj->unk9C[n];
        }
        return 0;
    }
    return 0;
}

/* Technique table (object +0x92C, slots 2..4) of fighter `player`. */
void *BtlCharApi_GetPlayerSuperData(s32 player) {
    BtlCapiChr *chr = BtlChar_Get(player);
    BtlCapiObj *obj;

    if (chr != NULL) {
        obj = BtlChar_GetObj(chr);
        if (obj != NULL) {
            return obj->supers;
        }
        return NULL;
    }
    return NULL;
}

/* Skill table (object +0x930, slots 0..1) of fighter `player`. */
void *BtlCharApi_GetPlayerSkillData(s32 player) {
    BtlCapiChr *chr = BtlChar_Get(player);
    BtlCapiObj *obj;

    if (chr != NULL) {
        obj = BtlChar_GetObj(chr);
        if (obj != NULL) {
            return obj->skills;
        }
        return NULL;
    }
    return NULL;
}

/* Word +0x58 of fighter `player`'s object. No caller. */
s32 BtlCharApi_GetPlayerObjUnk58(s32 player) {
    BtlCapiChr *chr = BtlChar_Get(player);
    BtlCapiObj *obj;

    if (chr != NULL) {
        obj = BtlChar_GetObj(chr);
        if (obj != NULL) {
            return obj->unk58;
        }
        return 0;
    }
    return 0;
}

/* Position: for a fighter its position plus the display offset (hover bob and shake), else the object's +0x970. */
void BtlCharApi_GetPos(s32 objId, Vec4 *out) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiObj *obj;

    if (chr != NULL) {
        Vec4_Add(out, &BtlChar_GetPos(chr)->pos, &BtlChar_GetPos(chr)->dispOfs);
        return;
    }
    obj = BtlObj_Get(objId);
    if (obj != NULL) {
        Vec4_Copy(out, &obj->pos);
        return;
    }
    Vec4_SetZeroW1(out);
}

/* Position without the display offset. */
void BtlCharApi_GetBasePos(s32 objId, Vec4 *out) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiObj *obj;

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->pos);
        return;
    }
    obj = BtlObj_Get(objId);
    if (obj != NULL) {
        Vec4_Copy(out, &obj->pos);
        return;
    }
    Vec4_SetZeroW1(out);
}

/* Rotation: the fighter's model rotation, else the object's +0x960. */
void BtlCharApi_GetRot(s32 objId, Vec4 *out) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiObj *obj;

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->rot);
        return;
    }
    obj = BtlObj_Get(objId);
    if (obj != NULL) {
        Vec4_Copy(out, &obj->rot);
        return;
    }
    Vec4_SetZero(out);
}

/* Rotation including the animation: fighter rotation plus root motion rotation, or object rotation plus the Euler
   angles of node 0; wrapped. No caller. */
void BtlCharApi_GetModelRot(s32 objId, Vec4 *out) {
    Quat q;
    Vec4 euler;
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiObj *obj;
    BtlCapiNode *node;

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->rot);
        Vec4_Add(out, out, &BtlChar_GetPos(chr)->rootRot);
        BtlUtil_WrapAngles(out, out);
    } else {
        obj = BtlObj_Get(objId);
        if (obj != NULL) {
            Vec4_Copy(out, &obj->rot);
            node = BtlObj_GetNode(obj, 0);
            if (node != NULL) {
                Vec4_Copy((Vec4 *)&q, (Vec4 *)&node->rot);
                Quat_ToEuler(&euler, &q);
                Vec4_Add(out, out, &euler);
                BtlUtil_WrapAngles(out, out);
            }
        } else {
            Vec4_SetZero(out);
        }
    }
}

/* Heading yaw of a fighter, rotation y of another object. No caller. */
f32 BtlCharApi_GetYaw(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiObj *obj;

    if (chr != NULL) {
        return BtlChar_GetPos(chr)->yaw;
    }
    obj = BtlObj_Get(objId);
    if (obj != NULL) {
        return obj->rot.y;
    }
    return 0.0f;
}

/* Pose +0x30: the fighter's movement over the previous frame (between snapshots 0 and 1). */
void BtlCharApi_GetVelocity(s32 objId, Vec4 *out) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->vel);
        return;
    }
    Vec4_SetZero(out);
}

/* Pose +0x40: the fighter's whole movement of the previous frame. */
void BtlCharApi_GetFrameMove(s32 objId, Vec4 *out) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->move);
        return;
    }
    Vec4_SetZero(out);
}

/* Unit direction of travel. */
void BtlCharApi_GetDir(s32 objId, Vec4 *out) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->dir);
        return;
    }
    Vec4_SetZero(out);
}

/* Speed along the direction of travel, per frame. */
f32 BtlCharApi_GetSpeed(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_GetPos(chr)->speed;
    }
    return 0.0f;
}

/* Vertical speed, positive down. */
f32 BtlCharApi_GetFallSpeed(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_GetPos(chr)->fallSpeed;
    }
    return 0.0f;
}

/* Height (y) of the ground point below the fighter. */
f32 BtlCharApi_GetGroundY(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_GetPos(chr)->ground.y;
    }
    return 0.0f;
}

/* Distance from the fighter down to the ground below it (y is down). */
f32 BtlCharApi_GetAltitude(s32 objId) {
    Vec4 pos;

    BtlCharApi_GetPos(objId, &pos);
    return BtlCharApi_GetGroundY(objId) - pos.y;
}

/* World position of a model node (translation of its matrix); the object position when the node does not exist. */
void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiNode *n;

    if (obj != NULL) {
        n = BtlObj_GetNode(obj, node);
        if (n != NULL) {
            Vec4_Copy(out, (Vec4 *)&n->mtx.m[3]);
            return;
        }
        Vec4_Copy(out, &obj->pos);
    }
    if (obj == NULL) {
        Vec4_SetZeroW1(out);
        return;
    }
}

/* The vector at +0x90 of a model node; the object position when the node does not exist. No caller. */
void BtlCharApi_GetNodeUnk90(s32 objId, s32 node, Vec4 *out) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiNode *n;

    if (obj != NULL) {
        n = BtlObj_GetNode(obj, node);
        if (n != NULL) {
            Vec4_Copy(out, &n->unk90);
            return;
        }
        Vec4_Copy(out, &obj->pos);
    }
    if (obj == NULL) {
        Vec4_SetZeroW1(out);
        return;
    }
}

/* World matrix of a model node, identity when there is none. */
void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiNode *n;

    if (obj != NULL) {
        n = BtlObj_GetNode(obj, node);
        if (n != NULL) {
            Mtx_Copy(out, &n->mtx);
            return;
        }
        Mtx_StoreIdentity(out);
    }
    if (obj == NULL) {
        Mtx_StoreIdentity(out);
        return;
    }
}

/* The second matrix (+0x50) of a model node, identity when there is none. No caller. */
void BtlCharApi_GetNodeMtx50(s32 objId, s32 node, Mtx44 *out) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiNode *n;

    if (obj != NULL) {
        n = BtlObj_GetNode(obj, node);
        if (n != NULL) {
            Mtx_Copy(out, &n->unk50);
            return;
        }
        Mtx_StoreIdentity(out);
    }
    if (obj == NULL) {
        Mtx_StoreIdentity(out);
        return;
    }
}

/* BtlObj_GetNodeVelocity(obj, node, out) on the object, zero for no object. No caller. */
void BtlCharApi_GetNodeRot(s32 objId, s32 node, Vec4 *out) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        BtlObj_GetNodeVelocity(obj, node, out);
        return;
    }
    Vec4_SetZero(out);
}

/* Rotation quaternion (+0xA0) of a model node, identity when there is none. No caller. */
void BtlCharApi_GetNodeQuat(s32 objId, s32 node, Quat *out) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiNode *n;

    if (obj != NULL) {
        n = BtlObj_GetNode(obj, node);
        if (n != NULL) {
            Vec4_Copy((Vec4 *)out, (Vec4 *)&n->rot);
            return;
        }
        Quat_SetIdentity(out);
    }
    if (obj == NULL) {
        Quat_SetIdentity(out);
        return;
    }
}

/* Float +0x5C of what BtlObj_FindBound(obj, n) returns, 1.0 when there is none. */
f32 BtlCharApi_GetPartUnk5C(s32 objId, s32 n) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    BtlCapiPart *part;

    if (obj != NULL) {
        part = BtlObj_FindBound(obj, n);
        if (part != NULL) {
            return part->unk5C;
        }
        return 1.0f;
    }
    return 1.0f;
}

/* The distance under which the opponent counts as close (flags 0x13 and 5): own size + the other's + a margin. */
f32 BtlCharApi_GetCloseRange(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr == NULL) {
        return 0.0f;
    }
    return BtlChar_GetSpacing(chr, 0);
}

/* 1.0 when the object exists, else 0.0. No caller. */
f32 BtlCharApi_ObjExistsF(s32 objId) {
    if (BtlObj_Get(objId) == NULL) {
        return 0.0f;
    }
    return 1.0f;
}

/* Fighter flag 0x13: the opponent is within the close range. No caller in the main executable. */
s32 BtlCharApi_IsClose(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x13);
    }
    return 0;
}

/* Fighter flag 0xBA: the stage blocks the line between the two fighters' heads. */
s32 BtlCharApi_IsSightBlocked(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xBA);
    }
    return 0;
}

/* Fighter flag 0xF: standing on the ground. No caller (the copy at 0x2081E8 is the one used). */
s32 BtlCharApi_IsOnGround(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xF);
    }
    return 0;
}

/* Fighter flag 0x11: the body centre is under water. */
s32 BtlCharApi_IsInWater(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x11);
    }
    return 0;
}

/* Whether the object lacks the "drawn" flag (bit 1 of its flag word). */
s32 BtlCharApi_IsHidden(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        return !((obj->u.flags >> 1) & 1);
    }
    return 0;
}

/* Whether the fighter is frozen by hit-stop. No caller. */
s32 BtlCharApi_IsFrozen(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_IsFrozen(chr);
    }
    return 0;
}

/* Whether any fighter is frozen by hit-stop. No caller. */
s32 BtlCharApi_AnyFrozen(void) {
    s32 i;

    if (gBtlChars != NULL) {
        for (i = 0; i < BtlChar_GetCount(); i++) {
            if (BtlChar_IsFrozen(BtlChar_Get(i))) {
                return 1;
            }
        }
    }
    return 0;
}

/* 1 for a character with parameter flag 0x800 that is not in flag 0xA, else flag 9 (fast vertical flight). */
s32 BtlCharApi_TestStageBreakA(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if ((BtlParam_GetFlags(chr) & 0x800) && !BtlChar_TestFlag(chr, 0xA)) {
            return 1;
        }
        return BtlChar_TestFlag(chr, 9);
    }
    return 0;
}

/* Flag 0xA unless flag 0x21 or 0x22 is up. */
s32 BtlCharApi_TestStageBreakB(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (BtlChar_TestFlag(chr, 0x21)) {
            return 0;
        }
        if (BtlChar_TestFlag(chr, 0x22)) {
            return 0;
        }
        return BtlChar_TestFlag(chr, 0xA);
    }
    return 0;
}

/* How far the fighter has charged: the larger of the attack charge (+0xD78) and the charged ki blast's (+0xDEC), 0..1. */
f32 BtlCharApi_GetChargeRate(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    f32 t = 0.0f;

    if (chr == NULL) {
        return 0.0f;
    }
    if (chr->charge > t) {
        t = chr->charge;
    }
    if (chr->blastCharge > t) {
        t = chr->blastCharge;
    }
    return t;
}

/* A class of the current action for the effect code: 3 fast ascent / leaving switch, 4 fast descent / lift throw,
   5 rush sequence, else 1. */
s32 BtlCharApi_GetActionFxKind(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        switch (BtlAct_GetCurrent(chr)) {
        case 0xBB:
            return 4;
        case 0x17:
            return 3;
        case 0x18:
            return 4;
        case 0xF3:
            return 3;
        case 0x12D:
        case 0x12E:
        case 0x12F:
            return 5;
        default:
            return 1;
        }
    }
    return 0;
}

/* Balance of clash A (roster +0x5C): the lead squashed into -0.8..0.8. */
f32 BtlCharApi_GetClashBias(void) {
    if (gBtlChars != NULL) {
        return gBtlChars->clashBias;
    }
    return 0.0f;
}

/* Fighter counter +0xFFC as a 0..1 ratio of 90. */
f32 BtlCharApi_GetBlindRatio(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr == NULL) {
        return 0.0f;
    }
    return BtlUtil_ClampF((f32)chr->unkFFC / 90.0f, 0.0f, 1.0f);
}

/* During a rush sequence: 0 when the rush has no marked step (attacker: throw word +0x48 / +0x10 - 1; victim: +0x4C /
   +0x1C), 2 before that step, 1 from it on; -1 outside a rush sequence. */
s32 BtlCharApi_GetRushFinishPhase(s32 objId) {
    s32 level = -1;
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr == NULL) {
        return -1;
    }
    if (!BtlCharApi_IsInRushSequence(objId)) {
        return -1;
    }
    if (chr->player == chr->thrAtkSide) {
        if (chr->thrUnk48 != 0) {
            level = chr->thrUnk10 - 1;
        }
    } else {
        if (chr->thrUnk4C != 0) {
            level = chr->thrUnk1C;
        }
    }
    if (level < 0) {
        return 0;
    }
    return (BtlAct_GetMotionLevel(chr, BtlAnim_GetId(chr)) >= level) ? 1 : 2;
}

/* BtlObj_AddPush(obj, (sin yaw, 0, cos yaw) * len, arg). No caller. */
void BtlCharApi_ObjPushYaw(s32 objId, f32 yaw, f32 len, f32 arg) {
    Vec4 v;
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        v.x = Mathf_Sin(yaw) * len;
        v.y = 0.0f;
        v.z = Mathf_Cos(yaw) * len;
        v.w = 0.0f;
        BtlObj_AddPush(obj, &v, arg);
    }
}

/* BtlObj_AddPush(obj, dir scaled to length len, arg); nothing for a direction shorter than 0.0001. No caller. */
void BtlCharApi_ObjPushDir(s32 objId, Vec4 *dir, f32 len, f32 arg) {
    Vec4 v;
    BtlCapiObj *obj = BtlObj_Get(objId);
    f32 l;

    if (obj != NULL) {
        l = Vec3_Length(dir);
        if (l < 0.0001f) {
            return;
        }
        Vec4_Scale(&v, dir, 1.0f / l);
        Vec4_Scale(&v, &v, len);
        BtlObj_AddPush(obj, &v, arg);
    }
}

/* Body radius: the parameter float +0xC when positive, else object +0xFF0 * 1.8. */
f32 BtlCharApi_GetRadius(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);
    f32 r = -1.0f;

    if (obj != NULL) {
        if (obj->param != NULL) {
            r = obj->param->radius;
        }
        if (r <= 0.0f) {
            r = obj->unkFF0 * 1.8f;
        }
        return r;
    }
    return 0.0f;
}

/* Copies the vector the object's +0xFA0 points at (the body position used for pushing apart). */
void BtlCharApi_GetBodyPos(s32 objId, Vec4 *out) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        Vec4_Copy(out, obj->bodyPos);
    }
}

/* Unit direction from `from` to the opponent's model node `node`, with its pitch kept between minPitch and
   maxPitch (a direction steeper than that is flattened, or rebuilt from the model yaw when nearly vertical). A
   technique with flag 0x40000 aims along the heading yaw instead; a direction pointing away from the opponent is
   mirrored; a zero direction becomes the model's facing. */
void BtlCharApi_CalcAimDir(s32 objId, s32 node, Vec4 *from, Vec4 *out, f32 minPitch, f32 maxPitch) {
    Vec4 pos;
    Vec4 delta;
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiPose *pose;
    f32 len;
    f32 lo;
    f32 hi;

    if (chr == NULL) {
        Vec4_SetZero(out);
        return;
    }
    pose = BtlChar_GetPos(chr);
    BtlOpp_GetNodePos(chr, node, &pos);
    Vec4_Sub(out, &pos, from);
    if (BtlCharApi_IsInTechnique(objId)) {
        if (BtlSuper_GetFlags(chr, BtlAct_GetCurrentClass(chr)) & 0x40000) {
            len = BtlUtil_LengthXZ(out);
            out->x = Mathf_Sin(pose->yaw) * len;
            out->z = Mathf_Cos(pose->yaw) * len;
        }
    }
    len = Vec3_Length(out);
    if (len < 0.0001f) {
        out->x = Mathf_Sin(pose->rot.y);
        out->y = 0.0f;
        out->z = Mathf_Cos(pose->rot.y);
        out->w = 0.0f;
        return;
    }
    Vec4_Scale(out, out, 1.0f / len);
    BtlOpp_GetDelta(chr, &delta);
    if (out->x * delta.x + out->z * delta.z < 0.0f) {
        out->x = -out->x;
        out->z = -out->z;
    }
    lo = Mathf_Sin(minPitch);
    hi = Mathf_Sin(maxPitch);
    if (out->y < lo) {
        if (__builtin_fabsf(out->y) < 0.999f) {
            f32 xz = sqrtf(1.0f - lo * lo);
            f32 k = xz / sqrtf(1.0f - out->y * out->y);

            out->y = lo;
            out->x *= k;
            out->z *= k;
        } else {
            f32 xz = sqrtf(1.0f - lo * lo);
            f32 s = Mathf_Sin(pose->rot.y);

            out->y = lo;
            out->x = xz * s;
            out->z = xz * Mathf_Cos(pose->rot.y);
        }
    }
    if (hi < out->y) {
        if (__builtin_fabsf(out->y) < 0.999f) {
            f32 xz = sqrtf(1.0f - hi * hi);
            f32 k = xz / sqrtf(1.0f - out->y * out->y);

            out->y = hi;
            out->x *= k;
            out->z *= k;
        } else {
            f32 xz = sqrtf(1.0f - hi * hi);
            f32 s = Mathf_Sin(pose->rot.y);

            out->y = hi;
            out->x = xz * s;
            out->z = xz * Mathf_Cos(pose->rot.y);
        }
    }
    out->w = 0.0f;
}

/* The same for node 0x11 and 45 degrees up or down. */
void BtlCharApi_CalcAimDir45(s32 objId, Vec4 *from, Vec4 *out) {
    BtlCharApi_CalcAimDir(objId, 0x11, from, out, -0.78539816f, 0.78539816f);
}

/* A random direction for something knocked away by the fighter: its facing turned by two random angles that depend
   on its animation (0xF9, 0xFA, 0xFB / 0x19, 0xFC), else a cone of 0.3 rad round the direction to the opponent.
   Two draws of the fighter generator. */
void BtlCharApi_GetDeflectDir(s32 objId, Vec4 *out) {
    Vec4 v;
    Vec4 side;
    Vec4 fwd;
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    f32 c;

    if (chr == NULL) {
        Vec4_SetZero(out);
        return;
    }
    fwd.x = Mathf_Sin(BtlChar_GetPos(chr)->rot.y);
    fwd.y = 0.0f;
    c = Mathf_Cos(BtlChar_GetPos(chr)->rot.y);
    fwd.z = c;
    fwd.w = 0.0f;
    side.x = fwd.z;
    side.y = 0.0f;
    side.z = -fwd.x;
    side.w = 0.0f;
    Vec4_Copy(&v, &fwd);
    switch (BtlAnim_GetId(chr)) {
    case 0xF9:
        Vec3_RotateAxis(&v, &v, &side, BtlChar_RandF() * 0.3f + 0.3f);
        Vec3_RotateAxis(&v, &v, &fwd, -(BtlChar_RandF() * 0.8f + 0.5f));
        break;
    case 0xFA:
        Vec3_RotateAxis(&v, &v, &side, BtlChar_RandF() * 0.3f + 0.3f);
        Vec3_RotateAxis(&v, &v, &fwd, BtlChar_RandF() * 0.8f + 0.5f);
        break;
    case 0x19:
    case 0xFB:
        Vec3_RotateAxis(&v, &v, &side, BtlChar_RandF() * 0.5f + 2.5f);
        Vec3_RotateAxis(&v, &v, &fwd, BtlChar_RandF() * 1.8f - 0.9f);
        break;
    case 0xFC:
        Vec3_RotateAxis(&v, &v, &side, BtlChar_RandF() * 0.3f + 0.3f);
        Vec3_RotateAxis(&v, &v, &fwd, BtlChar_RandF() * 1.2f - 0.6f);
        break;
    default:
        BtlOpp_GetDelta(chr, &fwd);
        Vec3_Normalize(&fwd, &fwd);
        side.x = fwd.z;
        side.y = 0.0f;
        side.w = 0.0f;
        side.z = -fwd.x;
        Vec3_Normalize(&side, &side);
        Vec4_Copy(&v, &fwd);
        Vec3_RotateAxis(&v, &v, &side, BtlChar_RandF() * 0.6f - 0.3f);
        Vec3_RotateAxis(&v, &v, &fwd, BtlChar_RandF() * 6.2831853f - 3.14159265f);
        break;
    }
    Vec4_Copy(out, &v);
}

/* Fighter flag 5: locked on to the opponent. */
s32 BtlCharApi_IsLockedOn(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 5);
    }
    return 0;
}

/* Fighter flag 0x8E. */
s32 BtlCharApi_IsChargingKiBlast(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x8E);
    }
    return 0;
}

/* Whether the fighter is in a technique action (0x105..0x132). */
s32 BtlCharApi_IsInTechnique(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr));
    }
    return 0;
}

/* In a technique action while the previous action (+0x950) was not one. No caller. */
s32 BtlCharApi_EnteredTechnique(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (BtlAct_IsTechniqueId(BtlAct_GetCurrent(chr)) && !BtlAct_IsTechniqueId(BtlAct_GetPrev(chr))) {
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Fighter flag 0xA1 (raised when the damage of a rush technique is set up). */
s32 BtlCharApi_IsRushConnected(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xA1);
    }
    return 0;
}

/* Whether the fighter is in a rush sequence action (0x12D..0x12F, 0x139..0x13B). */
s32 BtlCharApi_IsInRushSequence(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        switch (BtlAct_GetCurrent(chr)) {
        case 0x12D:
        case 0x12E:
        case 0x12F:
        case 0x139:
        case 0x13A:
        case 0x13B:
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Whether the fighter is in a skill action (0xFD..0x102). */
s32 BtlCharApi_IsInSkill(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return (u32)(BtlAct_GetCurrent(chr) - 0xFD) < 6;
    }
    return 0;
}

/* In a skill action on the frame it was entered (flag 8). No caller. */
s32 BtlCharApi_IsSkillStart(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (BtlCharApi_IsInSkill(objId)) {
            return BtlChar_TestFlag(chr, 8);
        }
        return 0;
    }
    return 0;
}

/* Fighter flag 0x9A (raised on the frame a skill takes effect). */
s32 BtlCharApi_IsSkillApplied(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x9A);
    }
    return 0;
}

/* Whether the fighter is in a clash A action (0x130..0x132). */
s32 BtlCharApi_IsInClashA(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        switch (BtlAct_GetCurrent(chr)) {
        case 0x130:
        case 0x131:
        case 0x132:
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Whether the fighter is in action 0xFA (clash B) or 0xFC (a clash C exchange). */
s32 BtlCharApi_IsInClashBC(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);
    s32 action;

    if (chr != NULL) {
        action = BtlAct_GetCurrent(chr);
        if (action == 0xFA || action == 0xFC) {
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Fighter flag 0xC (raised by the ki charge loop). No caller. */
s32 BtlCharApi_IsChargingKi(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xC);
    }
    return 0;
}

/* Fighter flag 0xD (raised when the ki charge goes on into the powered-up state). No caller. */
s32 BtlCharApi_IsMaxPowerStart(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xD);
    }
    return 0;
}

/* Whether the fighter is transforming, fusing or switching (0xEC..0xF8) or in action 0x103 / 0x104. */
s32 BtlCharApi_IsChanging(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        switch (BtlAct_GetCurrent(chr)) {
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
        case 0xF0:
        case 0xF1:
        case 0xF2:
        case 0xF3:
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7:
        case 0xF8:
        case 0x103:
        case 0x104:
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Bit 4 of the table word of the fighter's animation. */
s32 BtlCharApi_TestAnimFlag10(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return (BtlAnim_GetFlags(BtlAnim_GetId(chr)) >> 4) & 1;
    }
    return 0;
}

/* Whether the fighter is in action 0x33. */
s32 BtlCharApi_IsCircleDash(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlAct_GetCurrent(chr) == 0x33;
    }
    return 0;
}

/* Fighter flag 0x12B (raised when the fighter is bound to its object). */
s32 BtlCharApi_IsModelNew(s32 objId) {
    BtlCapiChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x12B);
    }
    return 0;
}

/* Parameter flag 0x40 with object flags 0x22, or parameter flag 0x80 with object flags 0x42. */
s32 BtlCharApi_HasWeaponOut(s32 objId) {
    BtlCapiObj *obj = BtlObj_Get(objId);

    if (obj != NULL) {
        if (obj->param == NULL) {
            return 0;
        }
        if ((obj->param->flags & 0x40) && (obj->u.flags64 & 0x22) == 0x22) {
            return 1;
        }
        if ((obj->param->flags & 0x80) && (obj->u.flags64 & 0x42) == 0x42) {
            return 1;
        }
        return 0;
    }
    return 0;
}

