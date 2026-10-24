#include "battle/tag_clash.h"
#include "battle/tag_position.h"
#include "common.h"
#include "battle/btl_char_ctl.h"
#include "sys/mathf.h"

/*
 * Pose <-> object, placement, holds and per-frame snapshots: 0x1D70E8..0x1D8330.
 *
 * The fighter keeps its own pose block (fighter + 0x10, BtlCtlPose); the battle object it drives keeps the
 * model placement. BtlChar_PoseToObj writes pose -> object (object position = pos + move, less the animation's
 * root translation rotated into the world and scaled), BtlChar_ObjToPose reads it back after the object update.
 *
 * BtlChar_Place is the one function that teleports a fighter: pose, object, fighter camera eye and yaw, and
 * flags 0xCD, 0x24, 0x3F. Seven requests, each a fighter flag tested once per frame in this order by the
 * fighter manager, end in it: 0xF3 (start position, variant 1), 0xF4 (stage position of BtlStage_GetPlace),
 * 0xF6 (the same plus stored offsets), 0xF8 (beside a point of a stage path), 0xF7 (the BtlStage_GetPlace position
 * raised), 0xF5 (the saved placement), 0xFC / 0xFD (warp position / rotation, only in actions 4 and 7..10).
 * None of them clears its flag here except 0xFC / 0xFD.
 *
 * BtlChars_UpdateHold is the roster-level step between the passes that an earlier report called "push-out":
 * it is not a general collision push. It only acts when one fighter has flag 0x8B (holder) and another has
 * flag 0x97 (held), and moves one of them so that the holder's node 0x34 and the held fighter's node
 * obj->holdNode coincide.
 */

extern f32 atan2f(f32 y, f32 x);

extern void Vec4_Set(Vec4 *out, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *out, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *out, Vec4 *v, f32 scale);
extern f32 Vec3_Length(Vec4 *v);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *out, Mtx44 *m, Vec4 *v);
extern void Mtx_RotateZ(Mtx44 *out, Mtx44 *in, f32 angle); /* rotate about z */
extern void Mtx_RotateX(Mtx44 *out, Mtx44 *in, f32 angle); /* rotate about x */
extern void Mtx_RotateY(Mtx44 *out, Mtx44 *in, f32 angle); /* rotate about y */
extern void Vec4_SetZeroW1(Vec4 *v);
extern void Vec4_SetZero(Vec4 *v);

extern BtlCtlRoster *gBtlChars;
extern Vec4 D_002EC2A0; /* (0, 0, 0, 0) */

extern s32 BtlChar_GetCount(void);
extern BtlCtlChr *BtlChar_Get(s32 index);
extern BtlCtlObj *BtlChar_GetObj(BtlCtlChr *chr);
extern BtlCtlPose *BtlChar_GetPos(BtlCtlChr *chr);
extern s32 BtlChar_IsFrozen(BtlCtlChr *chr);
extern s32 BtlChar_TestFlag(BtlCtlChr *chr, s32 bit);
extern void BtlChar_SetFlag(BtlCtlChr *chr, s32 bit);
extern void BtlChar_SetHeldFlag(BtlCtlChr *chr, s32 bit);
extern void BtlChar_ClearFlag(BtlCtlChr *chr, s32 bit);
extern f32 BtlUtil_WrapAngle(f32 angle);
extern void BtlUtil_WrapAngles(Vec4 *dst, Vec4 *src);
extern s32 BtlOpp_GetPlayer(BtlCtlChr *chr); /* player index of the other fighter */
extern f32 BtlOpp_GetHeight(BtlCtlChr *chr); /* size of the other fighter */
extern s32 BtlAct_GetCurrent(BtlCtlChr *chr); /* chr + 0x948: action id */
extern f32 BtlCharApi_GetHeight(s32 objId);        /* obj + 0xFF4 (10.0 without an object): the fighter's size */
extern f32 BtlCharApi_GetCenterHeight(s32 objId);
extern f32 BtlCharApi_GetRadius(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlSuper_GetStagePlacement(BtlCtlChr *target, s32 arg1, s32 arg2, Vec4 *pos, Vec4 *rot);
extern s32 BtlStage_FindZoneNear(s32 arg0, Vec4 *pos);
extern f32 BtlStage_GetTop(void);
extern BtlCtlPath *BtlStage_GetPath(s32 arg0);
extern s32 BtlStage_GetStartPlace(s32 player, Vec4 *pos, Vec4 *rot, s32 restart);
extern s32 BtlStage_GetPlace(Vec4 *pos, Vec4 *rot);
extern void BtlObjBody_Reset(BtlCtlObj *obj);
extern void BtlObjXf_Update(BtlCtlObj *obj);
extern void BtlObjPose_CalcMatrices(BtlCtlObj *obj);
extern BtlCtlNode *BtlObj_GetNode(BtlCtlObj *obj, s32 node);

/* Reads the object's position, rotation and root node back into the pose. */
void BtlChar_ObjToPose(BtlCtlChr *chr) {
    BtlCtlObj *obj = BtlChar_GetObj(chr);
    BtlCtlPose *pose = BtlChar_GetPos(chr);
    BtlCtlNode *node;
    Quat q;
    extern f32 TagGiant_ObjectScale(void *);
    f32 scale=TagGiant_ObjectScale(obj);
    Vec4 previous=pose->pos;

    Vec4_Sub(&pose->pos, &obj->outPos, &pose->move);
    /* The enlarged skeleton must not multiply locomotion/root displacement.
       This also prevents charge loops slowly dragging giants across the map. */
    if(scale!=1) {
        pose->pos.x=previous.x+(pose->pos.x-previous.x)/scale;
        pose->pos.y=previous.y+(pose->pos.y-previous.y)/scale;
        pose->pos.z=previous.z+(pose->pos.z-previous.z)/scale;
    }
    Vec4_Copy(&pose->rot, &obj->rot);
    node = BtlObj_GetNode(obj, 0);
    if (node != NULL) {
        Vec4_Copy(&pose->rootPos, &node->pos);
        Vec4_Copy((Vec4 *)&q, (Vec4 *)&node->rot);
        Quat_ToEuler(&pose->rootRot, &q);
    } else {
        Vec4_SetZeroW1(&pose->rootPos);
        Vec4_SetZero(&pose->rootRot);
    }
}

/* Writes the pose to the object (position less the rotated root motion) and updates the object. */
void BtlChar_PoseToObj(BtlCtlChr *chr, s32 keepRoot) {
    Vec4 old;
    Mtx44 m;
    Vec4 v;
    BtlCtlObj *obj = BtlChar_GetObj(chr);
    BtlCtlPose *pose = BtlChar_GetPos(chr);
    s32 apply;

    Vec4_Copy(&old, &obj->pos);
    Vec4_Add(&obj->pos, &pose->pos, &pose->move);
    Vec4_Copy(&obj->rot, &pose->rot);
    {
        extern f32 TagGiant_ObjectScale(void *);
        obj->scale = chr->scale*TagGiant_ObjectScale(obj);
    }
    if (BtlChar_TestFlag(chr, 0x2B)) {
        apply = 0;
    } else {
        apply = 1;
    }
    if (keepRoot == 0) {
        apply = 1;
    }
    if (apply) {
        Mtx_StoreIdentity(&m);
        Mtx_RotateY(&m, &m, 3.14159265f);
        Mtx_RotateY(&m, &m, obj->rot.y);
        Mtx_RotateX(&m, &m, obj->rot.x);
        Mtx_RotateZ(&m, &m, obj->rot.z);
        Mtx_MulVec4(&v, &m, &pose->rootPos);
        Vec4_Scale(&v, &v, obj->scale);
        Vec4_Sub(&obj->pos, &obj->pos, &v);
    }
    Vec4_Sub((Vec4 *)&m, &obj->pos, &old);
    if (Vec3_Length((Vec4 *)&m) < 0.001f) {
        Vec4_Copy(&obj->pos, &old);
    }
    BtlObjXf_Update(obj);
}

/* Under flag 0x33 (and not 0x94): turns the yaw by the root node's yaw and scales the speed by its cosine. */
void BtlChar_ApplyRootYaw(BtlCtlChr *chr) {
    BtlCtlPose *pose = BtlChar_GetPos(chr);

    if (BtlChar_TestFlag(chr, 0x33) && !BtlChar_TestFlag(chr, 0x94)) {
        f32 yaw = pose->rootRot.y;

        pose->heading = pose->rot.y = BtlUtil_WrapAngle(yaw + pose->rot.y);
        pose->speed *= Mathf_Cos(yaw);
    }
}

/* Builds the object's lean quaternion from the two lean angles of the pose. */
void BtlChar_UpdateLean(BtlCtlChr *chr) {
    BtlCtlPose *pose = BtlChar_GetPos(chr);
    BtlCtlObj *obj = BtlChar_GetObj(chr);
    Vec4 angles;

    Vec4_Set(&angles, -pose->leanX, 0.0f, pose->leanZ, 0.0f);
    Quat_FromEuler(&obj->lean, &angles);
}

/* Puts the fighter at a position and rotation: pose, object, camera eye and yaw; raises flags 0xCD, 0x24, 0x3F. */
void BtlChar_Place(BtlCtlChr *chr, Vec4 *pos, Vec4 *rot, s32 area) {
    BtlCtlPose *pose = BtlChar_GetPos(chr);
    BtlCtlObj *obj = BtlChar_GetObj(chr);

    Vec4_Copy(&pose->pos, pos);
    Vec4_Copy(&pose->rot, rot);
    Vec4_Copy(&pose->unk30, &D_002EC2A0);
    Vec4_Copy(&pose->unk40, &D_002EC2A0);
    pose->heading = rot->y;
    pose->speed = 0.0f;
    pose->unk98 = 0.0f;
    pose->unk9C = 0.0f;
    Vec4_Copy(&obj->pos, pos);
    Vec4_Copy(&obj->outPos, pos);
    Vec4_Copy(&obj->rot, rot);
    obj->area = area;
    chr->camEye[0] = pos->x;
    chr->camEye[1] = pos->y - BtlCharApi_GetHeight(chr->objId);
    chr->camEye[2] = pos->z;
    chr->camYaw = rot->y;
    pose->dir.x = Mathf_Sin(rot->y);
    pose->dir.y = 0.0f;
    pose->dir.z = Mathf_Cos(rot->y);
    pose->dir.w = 0.0f;
    BtlChar_SetFlag(chr, 0xCD);
    BtlChar_SetFlag(chr, 0x24);
    BtlChar_SetFlag(chr, 0x3F);
    BtlObjBody_Reset(obj);
}

/* Places the fighter at its player's start position of the stage. */
void BtlChar_PlaceAtStart(BtlCtlChr *chr) {
    Vec4 pos;
    Vec4 rot;

    BtlChar_Place(chr, &pos, &rot, BtlStage_GetStartPlace(chr->player, &pos, &rot, 0));
}

/* Flag 0xF3: places the fighter at its player's second start position. */
void BtlChar_PlaceRestart(BtlCtlChr *chr) {
    Vec4 pos;
    Vec4 rot;

    if (BtlChar_TestFlag(chr, 0xF3)) {
        s32 area=BtlStage_GetStartPlace(chr->player,&pos,&rot,1);
        TagPosition_Place(chr->player,&pos,&rot,area,1);
    }
}

/* Flag 0xF4: places the fighter at the stage position BtlStage_GetPlace gives. */
void BtlChar_PlaceCenter(BtlCtlChr *chr) {
    Vec4 pos;
    Vec4 rot;

    if (BtlChar_TestFlag(chr, 0xF4)) {
        s32 area=BtlStage_GetPlace(&pos,&rot);
        TagPosition_Place(chr->player,&pos,&rot,area,1);
    }
}

/* Flag 0xF6: the same position plus the offsets stored by BtlChar_RequestPlaceRelative. */
void BtlChar_PlaceRelative(BtlCtlChr *chr) {
    Vec4 pos;
    Vec4 rot;

    if (BtlChar_TestFlag(chr, 0xF6)) {
        s32 area = BtlStage_GetPlace(&pos, &rot);

        Vec4_Add(&pos, &pos, &chr->relPos);
        Vec4_Add(&rot, &rot, &chr->relRot);
        BtlUtil_WrapAngles(&rot, &rot);
        TagPosition_Place(chr->player,&pos,&rot,area,2);
    }
}

/* Flag 0xF8: places the fighter beside a point of a stage path, player 0 on one side and the others opposite. */
void BtlChar_PlaceOnPath(BtlCtlChr *chr) {
    Vec4 side;
    Vec4 delta;
    Vec4 pos;
    Vec4 rot;
    s32 *work = BtlClash_Exchange(chr->player);

    if (BtlChar_TestFlag(chr, 0xF8)) {
        BtlCtlPath *path = BtlStage_GetPath(work[2]);

        if (path != NULL && path->count > path->first) {
            s32 index = work[3] % (path->count - path->first) + path->first;
            BtlCtlPathPoint *first = path->points;
            BtlCtlPathPoint *point = &first[index];
            f32 len;
            f32 margin;

            delta.x = point->x - first->x;
            delta.y = point->y - first->y;
            delta.z = point->z - first->z;
            delta.w = 0.0f;
            side.x = delta.z;
            side.y = 0.0f;
            side.z = -delta.x;
            side.w = 0.0f;
            len = Vec3_Length(&side);
            if (len < 0.001f) {
                return;
            }
            margin = 30.0f;
            Vec4_Scale(&side, &side, (BtlCharApi_GetRadius(chr->objId) + margin) / len);
            pos.x = point->x;
            pos.y = point->y + BtlCharApi_GetCenterHeight(chr->objId);
            pos.z = point->z;
            pos.w = 1.0f;
            rot.x = 0.0f;
            rot.y = 0.0f;
            rot.z = 0.0f;
            rot.w = 0.0f;
            if (BtlClash_Side(chr->player) == 0) {
                Vec4_Sub(&pos, &pos, &side);
                rot.y = atan2f(side.x, side.z);
            } else {
                Vec4_Add(&pos, &pos, &side);
                rot.y = atan2f(-side.x, -side.z);
            }
            BtlChar_Place(chr, &pos, &rot, BtlStage_FindZoneNear(-1, &pos));
        }
    }
}

/* Flag 0xF7: the BtlStage_GetPlace position at a height of BtlStage_GetTop() plus half the fighter's height. */
void BtlChar_PlaceCenterHigh(BtlCtlChr *chr) {
    Vec4 pos;
    Vec4 rot;

    if (BtlChar_TestFlag(chr, 0xF7)) {
        s32 area = BtlStage_GetPlace(&pos, &rot);
        f32 y = BtlStage_GetTop();

        pos.y = y + BtlCharApi_GetHeight(chr->objId) * 0.5f;
        TagPosition_Place(chr->player,&pos,&rot,area,3);
    }
}

/* Flag 0xF5: restores the saved placement and the saved state of flags 0xF and 0xE. */
s32 BtlChar_PlaceSaved(BtlCtlChr *chr) {
    if (BtlChar_TestFlag(chr, 0xF5)) {
        BtlChar_Place(chr, &chr->savedPos, &chr->savedRot, chr->savedArea);
        if (chr->savedFlagF) {
            BtlChar_SetHeldFlag(chr, 0xF);
        } else {
            BtlChar_ClearFlag(chr, 0xF);
        }
        if (chr->savedFlagE) {
            BtlChar_SetHeldFlag(chr, 0xE);
        } else {
            BtlChar_ClearFlag(chr, 0xE);
        }
        if (chr->savedFlagF) {
            BtlChar_SetFlag(chr, 0x10);
        }
    }
}

/* In actions 4 and 7..10: flags 0xFC / 0xFD replace the position / rotation by the warp vectors (and are cleared). */
void BtlChar_PlaceWarp(BtlCtlChr *chr) {
    Vec4 pos;
    Vec4 rot;
    s32 changed = 0;
    s32 area;

    switch (BtlAct_GetCurrent(chr)) {
    case 4:
    case 7:
    case 8:
    case 9:
    case 10:
        Vec4_Copy(&pos, &BtlChar_GetPos(chr)->pos);
        Vec4_Copy(&rot, &BtlChar_GetPos(chr)->rot);
        area = BtlChar_GetObj(chr)->area;
        if (BtlChar_TestFlag(chr, 0xFC)) {
            BtlChar_ClearFlag(chr, 0xFC);
            changed = 1;
            Vec4_Copy(&pos, &chr->warpPos);
            area = BtlStage_FindZoneNear(-1, &pos);
        }
        if (BtlChar_TestFlag(chr, 0xFD)) {
            BtlChar_ClearFlag(chr, 0xFD);
            changed = 1;
            Vec4_Copy(&rot, &chr->warpRot);
        }
        if (changed) {
            BtlChar_Place(chr, &pos, &rot, area);
        }
        break;
    }
}

/* Returns own size (obj + 0xFF4) + the other fighter's size + a margin: 30, or 40 in mode 0 under flag 0x13. */
f32 BtlChar_GetSpacing(BtlCtlChr *chr, s32 mode) {
    f32 r = 0.0f;

    r += BtlCharApi_GetHeight(chr->objId);
    r += BtlOpp_GetHeight(chr);
    switch (mode) {
    case 0:
        if (BtlChar_TestFlag(chr, 0x13)) {
            r += 40.0f;
        } else {
            r += 30.0f;
        }
        break;
    case 1:
        r += 30.0f;
        break;
    case 2:
        r += 30.0f;
        break;
    }
    return r;
}

/* Keeps a held fighter (flag 0x97) attached to its holder (flag 0x8B): moves one so that the two hold nodes meet. */
void BtlChars_UpdateHold(void) {
    Vec4 holderPos;
    Vec4 heldPos;
    Vec4 delta;
    BtlCtlChr *held = NULL;
    BtlCtlChr *holder = NULL;
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlCtlChr *chr = BtlChar_Get(i);

        if (!BtlChar_IsFrozen(chr)) {
            if (BtlChar_TestFlag(chr, 0x8B)) {
                holder = chr;
            }
            if (BtlChar_TestFlag(chr, 0x97)) {
                held = chr;
            }
        }
    }
    if (holder != NULL && held != NULL && holder != held) {
        BtlCtlObj *holderObj = BtlChar_GetObj(holder);
        BtlCtlObj *heldObj = BtlChar_GetObj(held);
        s32 node = heldObj->holdNode;

        BtlCharApi_GetNodePos(holder->objId, 0x34, &holderPos);
        BtlCharApi_GetNodePos(held->objId, node, &heldPos);
        if (BtlChar_TestFlag(holder, 0x9B)) {
            Vec4_Sub(&delta, &heldPos, &holderPos);
            if (BtlChar_TestFlag(held, 0x34)) {
                Vec4_Scale(&delta, &delta, 1.0f - heldObj->unkC84);
            }
            Vec4_Add(&BtlChar_GetPos(holder)->pos, &BtlChar_GetPos(holder)->pos, &delta);
            BtlChar_PoseToObj(holder, 0);
            BtlObjPose_CalcMatrices(holderObj);
            BtlChar_ObjToPose(holder);
        } else {
            Vec4_Sub(&delta, &holderPos, &heldPos);
            if (BtlChar_TestFlag(held, 0x34)) {
                Vec4_Scale(&delta, &delta, 1.0f - heldObj->unkC84);
            }
            Vec4_Add(&BtlChar_GetPos(held)->pos, &BtlChar_GetPos(held)->pos, &delta);
            BtlChar_PoseToObj(held, 0);
            BtlObjPose_CalcMatrices(heldObj);
            BtlChar_ObjToPose(held);
            BtlChar_SetFlag(held, 0x23);
        }
    }
}

/* Saves the current placement and the state of flags 0xF and 0xE for a later flag 0xF5 restore. */
void BtlChar_SavePlacement(BtlCtlChr *chr) {
    BtlCtlObj *obj = BtlChar_GetObj(chr);

    Vec4_Copy(&chr->savedPos, &BtlChar_GetPos(chr)->pos);
    Vec4_Copy(&chr->savedRot, &BtlChar_GetPos(chr)->rot);
    Vec4_Copy(&chr->unk1310, obj->unkFA0);
    chr->savedArea = BtlChar_GetObj(chr)->area;
    chr->savedFlagF = BtlChar_TestFlag(chr, 0xF);
    chr->savedFlagE = BtlChar_TestFlag(chr, 0xE);
}

/* Sets the saved placement from arguments. */
void BtlChar_SetSavedPlacement(BtlCtlChr *chr, Vec4 *pos, Vec4 *rot, Vec4 *unk1310, s32 flagF, s32 flagE) {
    Vec4_Copy(&chr->savedPos, pos);
    Vec4_Copy(&chr->savedRot, rot);
    Vec4_Copy(&chr->unk1310, unk1310);
    chr->savedArea = BtlStage_FindZoneNear(-1, pos);
    chr->savedFlagF = flagF;
    chr->savedFlagE = flagE;
}

/* Stores a vector at +0x1310 and raises flag 0x55. */
void BtlChar_SetUnk1310(BtlCtlChr *chr, Vec4 *v) {
    Vec4_Copy(&chr->unk1310, v);
    BtlChar_SetFlag(chr, 0x55);
}

/* Reloads +0x1310 from the object's vector. */
void BtlChar_ResetUnk1310(BtlCtlChr *chr) {
    BtlCtlObj *obj = BtlChar_GetObj(chr);

    Vec4_Copy(&chr->unk1310, obj->unkFA0);
}

/* Computes the relative placement offsets (from this fighter or from BtlOpp_GetPlayer's fighter) and raises flag 0xF6. */
void BtlChar_RequestPlaceRelative(BtlCtlChr *chr, s32 arg1, s32 arg2, s32 self) {
    BtlCtlChr *target = chr;

    if (!self) {
        target = BtlChar_Get(BtlOpp_GetPlayer(chr));
    }
    BtlSuper_GetStagePlacement(target, arg1, arg2, &chr->relPos, &chr->relRot);
    BtlChar_SetFlag(chr, 0xF6);
}

/* Raises flag 0xF8 (placement on the stage path). */
void BtlChar_RequestPlaceOnPath(BtlCtlChr *chr) {
    BtlChar_SetFlag(chr, 0xF8);
}

/* Forgets this frame's snapshots. */
void BtlChar_ClearSnapshots(BtlCtlChr *chr) {
    chr->snapMask = 0;
}

/* Stores a position and rotation as snapshot n and as the latest snapshot. */
void BtlChar_Snapshot(BtlCtlChr *chr, Vec4 *pos, Vec4 *rot, s32 n) {
    Vec4_Copy(&chr->snapPos[n], pos);
    Vec4_Copy(&chr->snapRot[n], rot);
    chr->snapMask |= 1 << n;
    Vec4_Copy(&chr->snapPos[BTL_SNAP_LAST], pos);
    Vec4_Copy(&chr->snapRot[BTL_SNAP_LAST], rot);
    chr->snapMask |= 1 << BTL_SNAP_LAST;
}

/* Takes snapshot n of every fighter's pose. */
void BtlChars_Snapshot(s32 n) {
    s32 i;

    for (i = 0; i < BtlChar_GetCount(); i++) {
        BtlCtlChr *chr = BtlChar_Get(i);
        BtlCtlPose *pose = BtlChar_GetPos(chr);

        BtlChar_Snapshot(chr, &pose->pos, &pose->rot, n);
    }
}

/* Gives the position of snapshot n, or the current position when it was not taken. */
void BtlChar_GetSnapPos(BtlCtlChr *chr, Vec4 *out, s32 n) {
    if (chr->snapMask & (1 << n)) {
        Vec4_Copy(out, &chr->snapPos[n]);
    } else {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->pos);
    }
}

/* Gives the rotation of snapshot n, or the current rotation when it was not taken. */
void BtlChar_GetSnapRot(BtlCtlChr *chr, Vec4 *out, s32 n) {
    if (chr->snapMask & (1 << n)) {
        Vec4_Copy(out, &chr->snapRot[n]);
    } else {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->rot);
    }
}

/* Gives the movement between two snapshots. */
void BtlChar_GetSnapDelta(BtlCtlChr *chr, Vec4 *out, s32 from, s32 to) {
    Vec4 a;
    Vec4 b;

    BtlChar_GetSnapPos(chr, &a, from);
    BtlChar_GetSnapPos(chr, &b, to);
    Vec4_Sub(out, &b, &a);
}

/* Gives the movement since snapshot n. */
void BtlChar_GetMoveSince(BtlCtlChr *chr, Vec4 *out, s32 n) {
    Vec4 a;

    BtlChar_GetSnapPos(chr, &a, n);
    Vec4_Sub(out, &BtlChar_GetPos(chr)->pos, &a);
}
