#include "common.h"
#include "battle/btl_capi_b.h"

/*
 * Fighter interface, second part, 0x208430..0x20BA80. See include/battle/btl_capi_b.h for the groups.
 *
 * This continues src/battle/btl_char_api.c (same accessor style, and the float pool 0x2FE0CC..0x2FE0E0 used here
 * follows that file's run). Callees outside the file are named by address when they have no name yet; what each does
 * is in the comment next to its declaration (read from its code).
 */

extern BtlCapiBMgr *gBtlChars;

extern BtlCapiBChr *BtlChar_Get(s32 player);
extern BtlCapiBChr *BtlChar_FindByObjId(s32 objId);
extern BtlCapiBChr *BtlChar_FindBySide(s32 side);
extern BtlCapiBObj *BtlChar_GetObj(BtlCapiBChr *chr);
extern BtlCapiBPose *BtlChar_GetPos(BtlCapiBChr *chr);
extern s32 BtlChar_TestFlag(BtlCapiBChr *chr, s32 flag);
extern void BtlChar_SetFlag(BtlCapiBChr *chr, s32 flag);
extern void BtlChar_SetHeldFlag(BtlCapiBChr *chr, s32 flag);
extern void BtlChar_ClearFlag(BtlCapiBChr *chr, s32 flag);
extern s32 BtlChar_IsDead(BtlCapiBChr *chr);
extern s32 BtlChar_IsFrozen(BtlCapiBChr *chr);
extern s32 BtlChar_IsStage4Or27(void);
extern s32 BtlUtil_Clamp(s32 v, s32 lo, s32 hi);
extern s32 BtlOpp_GetPlayer(BtlCapiBChr *chr);             /* player index of the opponent */

extern s32 BtlAct_GetCurrent(BtlCapiBChr *chr);            /* action id (+0x948) */
extern s32 BtlAct_IsTechniqueId(s32 action);                  /* action in 0x105..0x132: a technique */
extern s32 BtlAct_GetCurrentClass(BtlCapiBChr *chr);       /* technique class of the current action, 0..4 */
extern s32 BtlAct_TestPoweredSkill(BtlCapiBChr *chr, u32 mask);
extern s32 BtlAnim_GetId(BtlCapiBChr *chr);
extern s32 BtlAnim_GetFlags(s32 anim);

extern BtlCapiBMember *BtlMember_GetActive(BtlCapiBChr *chr);
extern BtlCapiBGauge *BtlMember_GetGauge(BtlCapiBChr *chr, s32 member);
extern BtlCapiBGauge *BtlMember_GetActiveGauge(BtlCapiBChr *chr);
extern s32 BtlMember_CountAlive(BtlCapiBChr *chr);
extern s32 BtlMember_GetActiveIndex(BtlCapiBChr *chr);
extern s32 BtlMember_GetSwitchTarget(BtlCapiBChr *chr);
extern s32 BtlMember_SetSwitchTarget(BtlCapiBChr *chr, s32 n);
extern s32 BtlMember_Damage(BtlCapiBChr *chr, s32 amount, s32 flags);
extern void BtlMember_AddHealth(BtlCapiBChr *chr, s32 amount);
extern void BtlMember_AddKi(BtlCapiBChr *chr, s32 amount);
extern s32 BtlMember_DrainKi(BtlCapiBChr *chr, s32 amount);
extern void BtlMember_AddBlast(BtlCapiBChr *chr, s32 amount);
extern void BtlMember_SubBlast(BtlCapiBChr *chr, s32 amount);
extern void BtlMember_AddMaxPower(BtlCapiBChr *chr, s32 amount);
extern s32 BtlMember_HasAbility(BtlCapiBChr *chr, s32 ability);

extern s32 BtlMove_CanFireBlast(BtlCapiBChr *chr, s32 cls, s32 *outCount);
extern s32 BtlMove_IsBlockedByOpponent(BtlCapiBChr *chr);
extern void BtlChange_SetTaken(void);
extern void BtlChange_SetLoaded(void);
extern void BtlPartner_Attach(BtlCapiBChr *chr, s32 slot, s32 objId);

extern BtlCapiBWork *Battle_GetWork(void);
extern s32 Battle_GetMode(void);
extern s32 Rand_Range(s32 n);

extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);

extern BtlCapiBBlastList *EftHit_GetList(void);             /* the blast list */
extern s32 BtlAct_CanTransform(BtlCapiBChr *chr, u32 slot, s32 needBlast, s32 needAllowed);
extern s32 BtlAct_CanFuse(BtlCapiBChr *chr, s32 slot, s32 needBlast, s32 needAllowed, s32 *partner);
extern s32 BtlAct_CanSwitch(BtlCapiBChr *chr, s32 needGauge, s32 needAllowed);
extern void BtlCharApi_GetPos(s32 objId, Vec4 *out);           /* position of an object */
extern f32 BtlCharApi_GetRadius(s32 objId);                       /* body radius */
extern u32 BtlAtk_GetId(BtlCapiBChr *chr);                /* attack id in use */
extern s32 BtlAtk_GetFlags(BtlCapiBChr *chr);                /* its attribute word */
extern s32 BtlAtk_GetUnk2COf(BtlCapiBChr *chr, s32 level);     /* s8 +0x2C of the record of attack `level` (BtlAtk_GetRecordOf) */
extern s32 BtlParam_GetFlags(BtlCapiBChr *chr);                /* param->flags */
extern s32 BtlParam_GetUnk0(BtlCapiBChr *chr);                /* param->unk0 */
extern s32 BtlParam_GetUnkAC(BtlCapiBChr *chr);                /* param->transformSlot */
extern s32 BtlParam_GetSlotId(BtlCapiBChr *chr, s32 slot);      /* param->transformId[slot] */
extern s32 BtlParam_GetSlotCost(BtlCapiBChr *chr, s32 slot);      /* param->transformCost[slot] * 100000 */
extern s32 BtlParam_CountSlots(BtlCapiBChr *chr);                /* number of transformId[] entries != 0xFF */
extern s32 BtlParam_GetUnkB4(BtlCapiBChr *chr, s32 slot);      /* param->fusionId[slot] */
extern s32 BtlParam_GetCostAE(BtlCapiBChr *chr, s32 slot);      /* param->fusionCost[slot] * 100000 */
extern s32 BtlParam_GetCount80(BtlCapiBChr *chr);                /* param->blastLimit less abilities 0x11 / 0x10 / 0xF */
extern s32 BtlParam_GetUnk82(BtlCapiBChr *chr);                /* param->blastLimitB */
extern s32 BtlParam_GetGaugeB(BtlCapiBChr *chr);                /* param word +0x2C */
extern s32 BtlParam_GetUnk8F(BtlCapiBChr *chr, s32 n);         /* param->unk8F[n] */
extern s32 BtlKiBlast_GetFlagsOfHit(BtlCapiBBlastRec *rec);           /* flag word of the blast's definition */
extern s32 BtlSuper_GetFlags(BtlCapiBChr *chr, s32 cls);       /* skills->flags[cls - 2] */
extern s32 BtlSuper_GetType(BtlCapiBChr *chr, s32 cls);       /* skills->unk13C[cls] */
extern s32 BtlSuper_GetKiCost(BtlCapiBChr *chr, s32 cls);       /* skills->cost[cls], halved with ability 0x2B */
extern s32 BtlSuper_GetPromptRowIndex(BtlCapiBChr *chr, s32 n);         /* skills->unk227[n] */
extern s32 BtlSuper_GetUnk165(BtlCapiBChr *chr, s32 cls);       /* skills->kind[cls] */
extern s32 BtlSkill_GetFlags(BtlCapiBChr *chr, s32 slot);      /* moves->flags[slot] */
extern s32 BtlSkill_GetUnk9E(BtlCapiBChr *chr, s32 slot);      /* moves->kind[slot] */
extern s32 BtlSkill_GetBlastCost(BtlCapiBChr *chr, s32 slot);      /* moves->stock[slot] (-1 with ability 0x15, min 1) * 100000 */

extern s32 BtlCharApi_ObjQuery24D610(s32 objId, s32 arg1, s32 arg2);
extern f32 BtlCharApi_ObjGetUnkC78(s32 objId);
extern f32 BtlCharApi_ObjGetUnkC80(s32 objId);

/* Whether object byte +0xCAD is the complement of +0xCAC. */
s32 BtlCharApi_IsUnkCACPaired(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBObj *obj;

    if (chr != NULL) {
        obj = BtlChar_GetObj(chr);
        if (obj != NULL) {
            return obj->unkCAD == ~obj->unkCAC;
        }
        return 0;
    }
    return 0;
}

/* The fighter's action id. */
s32 BtlCharApi_GetAction(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlAct_GetCurrent(chr);
    }
    return 0;
}

/* Fighter word +0xDE0: blasts fired in the current volley. */
s32 BtlCharApi_GetBlastShots(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->blastShots;
    }
    return 0;
}

/* How many more class-0 blasts the fighter may fire: the smaller of limit - shots and limit - blasts alive. */
s32 BtlCharApi_GetBlastRoom(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 count;
    s32 left;
    s32 room;

    if (chr != NULL) {
        left = BtlParam_GetCount80(chr) - chr->blastShots;
        BtlMove_CanFireBlast(chr, 0, &count);
        room = BtlParam_GetCount80(chr) - count;
        if (left < room) {
            room = left;
        }
        return room;
    }
    return 0;
}

/* Whether the character can fire class-0 blasts at all (limit > 0). */
s32 BtlCharApi_HasBlastLimit(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlParam_GetCount80(chr) > 0;
    }
    return 0;
}

/* The same for class-1 blasts: the smaller of limit - shots and class-1 limit - class-1 blasts alive. */
s32 BtlCharApi_GetBlastRoomB(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 count;
    s32 left;
    s32 room;

    if (chr != NULL) {
        left = BtlParam_GetCount80(chr) - chr->blastShots;
        BtlMove_CanFireBlast(chr, 1, &count);
        room = BtlParam_GetUnk82(chr) - count;
        if (left < room) {
            room = left;
        }
        return room;
    }
    return 0;
}

/* Fighter flag 0x66. */
s32 BtlCharApi_TestFlag66(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x66);
    }
    return 0;
}

/* 1 when a blast record is not a threat to the fighter: wrong type for the mode, its own, ignored or already seen. */
s32 BtlCharApi_SkipBlastRec(s32 objId, BtlCapiBBlastRec *rec, s32 mode) {
    if (mode < 2) {
        if (rec->type != 0) {
            return 1;
        }
        if (rec->def == NULL) {
            return 1;
        }
        if (objId == rec->objId) {
            return 1;
        }
    } else {
        if (rec->type != 1) {
            return 1;
        }
        if (rec->src == NULL) {
            return 1;
        }
        if (objId == rec->src->ownerId) {
            return 1;
        }
    }
    if (rec->unk54 == 1) {
        return 1;
    }
    return rec->seen == 1;
}

/* Index of the first blast flying at the fighter and close enough to matter, -1 if none. Mode 0 / 1 look at blasts
   (type 0, reach = 5 frames of travel), mode 2 at type 1 records (reach = 1.9 frames). Mode 1 returns a class instead:
   0 for definition kinds 0, 4, 8, else 2 if bit 0 of the blast's flag word is set, else 1. */
s32 BtlCharApi_FindIncomingBlast(s32 objId, s32 mode) {
    Vec4 pos;
    Vec4 toBlast;
    u64 blastPos[2];
    Vec4 dir;
    Vec4 diff;
    Vec4 step;
    BtlCapiBBlastList *list;
    BtlCapiBBlastRec *rec;
    s32 i;
    f32 dist;
    f32 reach;

    if (mode < 2) {
        reach = 5.0f;
    } else {
        reach = 1.9f;
    }
    BtlCharApi_GetPos(objId, &pos);
    list = EftHit_GetList();
    for (i = 0; i < list->count; i++) {
        rec = &list->rec[i];
        if (BtlCharApi_SkipBlastRec(objId, rec, mode) == 1) {
            continue;
        }
        blastPos[0] = rec->pos[0];
        blastPos[1] = rec->pos[1];
        Vec3_Sub(&toBlast, (Vec4 *)rec->pos, &pos);
        Vec3_Normalize(&toBlast, &toBlast);
        Vec3_Sub(&step, (Vec4 *)rec->pos, &rec->prevPos);
        Vec3_Normalize(&dir, &step);
        if (0.0f < Vec3_Dot(&toBlast, &dir)) {
            continue;
        }
        Vec3_Sub(&diff, (Vec4 *)blastPos, &pos);
        dist = Vec3_Length(&diff) - BtlCharApi_GetRadius(objId) - 10.0f;
        if (Vec3_Length(&step) * reach < dist) {
            continue;
        }
        if (mode != 1) {
            return i;
        }
        switch (rec->def->kind) {
        case 0:
        case 4:
        case 8:
            return 0;
        }
        return (BtlKiBlast_GetFlagsOfHit(rec) & 1) ? 2 : 1;
    }
    return -1;
}

/* 1 when some blast that is not the fighter's own is level with it or moving away from it (no caller). */
s32 BtlCharApi_IsBlastPassing(s32 objId) {
    Vec4 pos;
    Vec4 toBlast;
    Vec4 dir;
    Vec4 step;
    BtlCapiBBlastList *list;
    BtlCapiBBlastRec *rec;
    s32 i;

    list = EftHit_GetList();
    BtlCharApi_GetPos(objId, &pos);
    for (i = 0; i < list->count; i++) {
        rec = &list->rec[i];
        if (rec->type == 0) {
            if (objId == rec->def->ownerId) {
                continue;
            }
        } else {
            if (objId == rec->src->ownerId) {
                continue;
            }
        }
        if (rec->unk54 == 1) {
            continue;
        }
        Vec3_Sub(&toBlast, (Vec4 *)rec->pos, &pos);
        Vec3_Normalize(&toBlast, &toBlast);
        Vec3_Sub(&step, (Vec4 *)rec->pos, &rec->prevPos);
        Vec3_Normalize(&dir, &step);
        if (!(0.0f < Vec3_Dot(&toBlast, &dir))) {
            return 1;
        }
    }
    return 0;
}

/* Marks the blast BtlCharApi_FindIncomingBlast(objId, 0) finds as seen, so it is not reported again. */
void BtlCharApi_MarkIncomingBlast(s32 objId) {
    BtlCapiBBlastList *list = EftHit_GetList();
    s32 i = BtlCharApi_FindIncomingBlast(objId, 0);
    BtlCapiBBlastRec *rec;

    if (i >= 0) {
        rec = list->rec;
        rec += i;
        rec->seen = 1;
    }
}

/* Progress of the technique the fighter is performing: fighter +0x1294 when it is >= 0, else
   (BtlCharApi_ObjQuery24D610(objId, 1, 1) - object +0xC78) / object +0xC80; -1 when there is none. */
f32 BtlCharApi_GetTechniqueProgress(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 frame;
    f32 start;
    f32 len;

    if (chr == NULL) {
        return 0.0f;
    }
    if (chr->unk1294 >= 0) {
        return chr->unk1294;
    }
    frame = BtlCharApi_ObjQuery24D610(objId, 1, 1);
    if (frame < 0) {
        return -1.0f;
    }
    start = BtlCharApi_ObjGetUnkC78(objId);
    len = BtlCharApi_ObjGetUnkC80(objId);
    if (0.01f < len) {
        return ((f32)frame - start) / len;
    }
    return -1.0f;
}

/* The fighter's technique table (battle object +0x92C). */
BtlCapiBSkills *BtlCharApi_GetSkillTable(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBObj *obj;

    if (chr != NULL) {
        obj = BtlChar_GetObj(chr);
        if (obj != NULL) {
            return obj->skills;
        }
        return NULL;
    }
    return NULL;
}

/* The fighter's move table (battle object +0x930). */
BtlCapiBMoves *BtlCharApi_GetMoveTable(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBObj *obj;

    if (chr != NULL) {
        obj = BtlChar_GetObj(chr);
        if (obj != NULL) {
            return obj->moves;
        }
        return NULL;
    }
    return NULL;
}

/* Attribute word of the attack in use (attack id below 0xA3), else 0. */
s32 BtlCharApi_GetAttackAttr(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (BtlAtk_GetId(chr) < 0xA3) {
            return BtlAtk_GetFlags(chr);
        }
        return 0;
    }
    return 0;
}

/* Health of the active member. */
s32 BtlCharApi_GetHp(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->hp;
    }
    return 0;
}

/* Its maximum. */
s32 BtlCharApi_GetHpMax(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->hpMax;
    }
    return 0;
}

/* Ki of the active member (no caller). */
s32 BtlCharApi_GetKi(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->ki;
    }
    return 0;
}

/* Blast stock of the active member (no caller). */
s32 BtlCharApi_GetBlast(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->blast;
    }
    return 0;
}

/* Its maximum (no caller). */
s32 BtlCharApi_GetBlastMax(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->blastMax;
    }
    return 0;
}

/* Gauge +0x1C of the active member (the powered-up mode's timer). */
s32 BtlCharApi_GetMaxPower(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->maxPower;
    }
    return 0;
}

/* The side's switch gauge, fighter +0x99C (no caller). */
s32 BtlCharApi_GetSwitchGauge(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->switchGauge;
    }
    return 0;
}

/* BtlMove_IsBlockedByOpponent of the fighter (no caller). */
s32 BtlCharApi_IsBlockedByOpponent(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMove_IsBlockedByOpponent(chr);
    }
    return 0;
}

/* Bit 0x80 of the first word of the character's parameter block. */
s32 BtlCharApi_HasParamBit80(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return (BtlParam_GetUnk0(chr) >> 7) & 1;
    }
    return 0;
}

/* Whether the fighter can transform now into the form of its parameter block's AI slot. */
s32 BtlCharApi_CanTransform(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 slot;

    if (chr != NULL) {
        slot = BtlParam_GetUnkAC(chr);
        if (slot < 0) {
            return 0;
        }
        return BtlAct_CanTransform(chr, slot, 1, 1) != 0;
    }
    return 0;
}

/* Whether the fighter can fuse now with any of its three fusion slots. */
s32 BtlCharApi_CanFuse(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 i;

    if (chr == NULL) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        if (BtlAct_CanFuse(chr, i, 1, 1, NULL)) {
            return 1;
        }
    }
    return 0;
}

/* Whether the fighter can switch to another team member now. */
s32 BtlCharApi_CanSwitch(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlAct_CanSwitch(chr, 1, 1) != 0;
    }
    return 0;
}

/* CPU level of the active member. */
s32 BtlCharApi_GetCpuLevel(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActive(chr)->cpuLevel;
    }
    return 0;
}

/* AI type of the active member. */
s32 BtlCharApi_GetAiType(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActive(chr)->aiType;
    }
    return 0;
}

/* Whether fighter word +0x1070 is not -30. */
s32 BtlCharApi_IsUnk1070Set(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->unk1070 != -30;
    }
    return 0;
}

/* Blast stock the AI slot's transformation costs; 0 when the character has none. */
s32 BtlCharApi_GetTransformCost(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 slot;

    if (chr != NULL) {
        if (BtlParam_CountSlots(chr) != 0) {
            slot = BtlParam_GetUnkAC(chr);
            if (slot == -1) {
                return 0;
            }
            return BtlParam_GetSlotCost(chr, slot);
        }
        return 0;
    }
    return 0;
}

/* Picks a fusion slot the side can pay for: the only one, a random one of two, or of three slot 0 / 1 / 2 with
   35 / 20 / 45 %; -1 if none (no caller). Draws Rand_Range(100) always and Rand_Range(2) for two candidates. */
s32 BtlCharApi_PickFusionSlot(s32 objId) {
    s32 list[3];
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 n = 0;
    s32 blast = BtlSide_GetBlast(objId);
    s32 r = Rand_Range(100);
    s32 i;
    s32 cost;

    if (chr == NULL) {
        return 0;
    }
    for (i = 0; i < 3; i++) {
        cost = BtlParam_GetCostAE(chr, i);
        if (cost != 0 && !(blast < cost)) {
            list[n] = i;
            n++;
        }
    }
    switch (n) {
    case 1:
        return list[0];
    case 0:
        return -1;
    case 2:
        return list[Rand_Range(2)];
    case 3:
        if (r < 35) {
            return 0;
        }
        return r < 55 ? 1 : 2;
    }
    return -1;
}

/* Picks a transformation slot the side can pay for (slot 3 also counts with cost 0, and is then taken with 2 %);
   -1 if none (no caller). Draws Rand_Range(100) always and Rand_Range(2) for two candidates without slot 3. */
s32 BtlCharApi_PickTransformSlot(s32 objId) {
    s32 list[4];
    s32 n = 0;
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 has3 = 0;
    s32 blast = BtlSide_GetBlast(objId);
    s32 r = Rand_Range(100);
    s32 i;
    s32 cost;

    if (chr == NULL) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        cost = BtlParam_GetSlotCost(chr, i);
        if (cost == 0 && i != 3) {
            continue;
        }
        if (blast < cost) {
            continue;
        }
        list[n] = i;
        if (i == 3) {
            has3 = 1;
        }
        n++;
    }
    switch (n) {
    case 0:
        break;
    case 1:
        return list[0];
    case 2:
        if (has3) {
            if (r < 2) {
                return 3;
            }
            return list[0];
        }
        return list[Rand_Range(2)];
    case 3:
        if (has3) {
            if (r < 2) {
                return 3;
            }
            if (r < 60) {
                return list[0];
            }
            return list[1];
        }
        if (r < 60) {
            return list[0];
        }
        if (r < 90) {
            return list[1];
        }
        return list[2];
    case 4:
        if (has3) {
            if (r < 2) {
                return 3;
            }
            if (r < 60) {
                return list[0];
            }
            if (r < 90) {
                return list[1];
            }
            return list[2];
        }
        break;
    }
    return -1;
}

/* Whether the fighter is transforming or fusing (action 0xEC..0xF2). */
s32 BtlCharApi_IsChangingForm(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 action;

    if (chr != NULL) {
        action = BtlAct_GetCurrent(chr);
        switch (action) {
        case 0xEC:
        case 0xED:
        case 0xEE:
        case 0xEF:
        case 0xF0:
        case 0xF1:
        case 0xF2:
            return 1;
        }
        return 0;
    }
    return 0;
}

/* Word +0x14 of the character's parameter block. */
s32 BtlCharApi_GetParamUnk14(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_GetObj(chr)->param->unk14;
    }
    return 0;
}

/* Word +0x10 of the character's parameter block (its flags). */
s32 BtlCharApi_GetParamFlags(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_GetObj(chr)->param->flags;
    }
    return 0;
}

/* Word +0x18 of the character's parameter block. */
s32 BtlCharApi_GetParamUnk18(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_GetObj(chr)->param->unk18;
    }
    return 0;
}

/* Number of team members of the fighter's side (no caller). */
s32 BtlCharApi_GetMemberCount(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->memberCount;
    }
    return 0;
}

/* Index of the member that is fighting (no caller). */
s32 BtlCharApi_GetActiveMember(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetActiveIndex(chr);
    }
    return 0;
}

/* Index of the member a switch would bring in (no caller). */
s32 BtlCharApi_GetSwitchTarget(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlMember_GetSwitchTarget(chr);
    }
    return 0;
}

/* Health of a member as a percentage, at least 1 while it has any (no caller). */
s32 BtlCharApi_GetMemberHpPercent(s32 objId, s32 member) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBGauge *g;
    s32 pct;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        if (g->hpMax > 0) {
            pct = (f32)g->hp / (f32)g->hpMax * 100.0f;
            if (pct <= 0 && g->hp > 0) {
                pct = 1;
            }
            return pct;
        }
        return 0;
    }
    return 0;
}

/* Ki of a member as a percentage, at least 1 while it has any (no caller). */
s32 BtlCharApi_GetMemberKiPercent(s32 objId, s32 member) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBGauge *g;
    s32 pct;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        if (g->kiMax > 0) {
            pct = (f32)g->ki / (f32)g->kiMax * 100.0f;
            if (pct <= 0 && g->ki > 0) {
                pct = 1;
            }
            return pct;
        }
        return 0;
    }
    return 0;
}

/* Kind byte of the technique the opponent is performing, -1 if it is not in a technique action. */
s32 BtlCharApi_GetOppSkillKind(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBChr *opp;

    if (chr == NULL) {
        return -1;
    }
    opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
    if (!BtlAct_IsTechniqueId(BtlAct_GetCurrent(opp))) {
        return -1;
    }
    return BtlSuper_GetUnk165(opp, BtlAct_GetCurrentClass(opp));
}

/* Bit 4 of the flag word of the technique the opponent is performing. */
s32 BtlCharApi_IsOppSkillFlag4(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBChr *opp;

    if (chr != NULL) {
        opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
        if (BtlAct_IsTechniqueId(BtlAct_GetCurrent(opp))) {
            return (BtlSuper_GetFlags(opp, BtlAct_GetCurrentClass(opp)) >> 2) & 1;
        }
        return 0;
    }
    return 0;
}

/* Whether the opponent's technique has flag bit 1, or bit 2 while this fighter's +0xE44 is above 0.9, or byte
   +0x13C of its class is 1 (meaning not known; the AI's condition code reads it). */
s32 BtlCharApi_TestOppSkillFlags(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBChr *opp;
    s32 cls;
    s32 flags;
    s32 unk;

    if (chr != NULL) {
        opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
        if (BtlAct_IsTechniqueId(BtlAct_GetCurrent(opp))) {
            cls = BtlAct_GetCurrentClass(opp);
            flags = BtlSuper_GetFlags(opp, cls);
            unk = BtlSuper_GetType(opp, cls);
            if (flags & 1) {
                return 1;
            }
            if ((flags & 2) && 0.9f < chr->unkE44) {
                return 1;
            }
            return unk == 1;
        }
        return 0;
    }
    return 0;
}

/* Class (0..4) of the technique the opponent is performing, -1 if none. */
s32 BtlCharApi_GetOppSkillClass(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBChr *opp;

    if (chr == NULL) {
        return -1;
    }
    opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
    if (!BtlAct_IsTechniqueId(BtlAct_GetCurrent(opp))) {
        return -1;
    }
    return BtlAct_GetCurrentClass(opp);
}

/* Kind byte of the move the opponent is performing (action 0xFD..0x102), -1 if none. */
s32 BtlCharApi_GetOppMoveKind(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBChr *opp;

    if (chr == NULL) {
        return -1;
    }
    opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
    if ((u32)(BtlAct_GetCurrent(opp) - 0xFD) >= 6) {
        return -1;
    }
    return BtlSkill_GetUnk9E(opp, BtlAct_GetCurrentClass(opp));
}

/* Fighter word +0xE50 (button presses counted in clashes B and C). */
s32 BtlCharApi_GetClashCountB(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->clashCountB;
    }
    return 0;
}

/* Fighter word +0xE4C (the same for clash A). */
s32 BtlCharApi_GetClashCountA(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->clashCountA;
    }
    return 0;
}

/* Whether the effect of move slot 0 / 1 is running: its automatic evasions are left, or its modifier is active. */
s32 BtlCharApi_IsMoveSlotActive(s32 objId, u32 slot) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 n;

    if (chr == NULL) {
        return 0;
    }
    if (slot >= 2) {
        return 0;
    }
    n = slot != 0;
    if (chr->dodgeKind == slot) {
        if (chr->dodges > 0) {
            return 1;
        }
        if (chr->dodgesB > 0) {
            return 1;
        }
    }
    return chr->slotOn[n];
}

/* Bit 0x100 of the flag word of move slot 0 / 1. */
s32 BtlCharApi_IsMoveFlag100(s32 objId, u32 slot) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr == NULL) {
        return 0;
    }
    if (slot >= 2) {
        return 0;
    }
    return (BtlSkill_GetFlags(chr, slot) >> 8) & 1;
}

/* Fighter word +0xFE0: frames of stun left. */
s32 BtlCharApi_GetStunTimer(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->stunTimer;
    }
    return 0;
}

/* Buttons the open prompt (fighter +0x1594 >= 2) wants, as bits of the fighter's button word: 4 with flag 0xA3, else
   0x08 plus the direction bit 0x10 / 0x20 / 0x40 / 0x80 of the prompt's entry; 0 with flag 0xA2 or no prompt. */
s32 BtlCharApi_GetPromptButtons(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBPrompt *p;

    if (chr == NULL) {
        return 0;
    }
    if (chr->switchPrompt < 2) {
        return 0;
    }
    p = &gBtlChars->prompts[BtlSuper_GetPromptRowIndex(chr, chr->switchPrompt)];
    if (BtlChar_TestFlag(chr, 0xA2)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xA3)) {
        return 4;
    }
    switch (p->button) {
    case 0:
        return 0x18;
    case 1:
        return 0x28;
    case 2:
        return 0x48;
    case 3:
        return 0x88;
    }
    return 0;
}

/* Fighter word +0x1290 in story mode (Battle_GetMode() == 1), else 0. */
s32 BtlCharApi_GetUnk1290(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (Battle_GetMode() != 1) {
            return 0;
        }
        return chr->unk1290;
    }
    return 0;
}

/* Whether fighter word +0x106C is below -29. */
s32 BtlCharApi_IsUnk106CLow(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->unk106C < -29;
    }
    return 0;
}

/* Which of the fighter's attacks 0..4 is the first strong enough for the opponent's armour. An armour level is
   built from both characters' parameter flags 4 / 8, powered skills (0x40 on the opponent, 0x100 on the fighter),
   abilities (0x46 on the opponent; 0x47, or 0x70 with flag 6, on the fighter) and the opponent's +0xE14 timer; the
   first attack record whose byte +0x2C is not below it gives 0 (attacks 0, 1) or its id - 1; 4 when none is. */
s32 BtlCharApi_GetArmorBreakLevel(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    BtlCapiBChr *opp;
    s32 level;
    s32 i;

    if (chr == NULL) {
        return 4;
    }
    opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
    level = 0;
    if (BtlParam_GetFlags(opp) & 4) {
        level = 1;
    }
    if (BtlParam_GetFlags(opp) & 8) {
        level--;
    }
    if (BtlParam_GetFlags(chr) & 4) {
        level--;
    }
    if (BtlParam_GetFlags(chr) & 8) {
        level++;
    }
    if (BtlAct_TestPoweredSkill(opp, 0x40)) {
        level++;
    }
    level += opp->unkE14 > 0;
    if (BtlMember_HasAbility(opp, 0x46)) {
        level++;
    }
    if (BtlAct_TestPoweredSkill(chr, 0x100)) {
        level--;
    }
    if (BtlMember_HasAbility(chr, 0x47)) {
        level--;
    } else if (BtlMember_HasAbility(chr, 0x70)) {
        if (BtlChar_TestFlag(chr, 6)) {
            level--;
        }
    }
    for (i = 0; i < 5; i++) {
        if (level - BtlAtk_GetUnk2COf(chr, i) <= 0) {
            if (i < 2) {
                return 0;
            }
            return i - 1;
        }
    }
    return 4;
}

/* Byte +2 of the character's parameter block. */
s32 BtlCharApi_GetParamByte2(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_GetObj(chr)->param->unk2;
    }
    return 0;
}

/* Float at fighter +0xE44 (a second copy of BtlCharApi_GetUnkE44). */
f32 BtlCharApi_GetUnkE44B(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->unkE44;
    }
    return 0.0f;
}

/* Fighter +0xD74 minus +0xD70, or -1 when +0xD74 is 0 (no caller). */
s32 BtlCharApi_GetUnkD74Diff(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        if (chr->unkD74 == 0) {
            return -1;
        }
        return chr->unkD74 - chr->unkD70;
    }
    return 0;
}

/* Whether the fighter's animation has flag 0x2000 or 0x800 and not 0x8000. */
s32 BtlCharApi_IsAnimFlag2800(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);
    s32 flags;

    if (chr != NULL) {
        flags = BtlAnim_GetFlags(BtlAnim_GetId(chr));
        if (!(flags & 0x2800)) {
            return 0;
        }
        if (flags & 0x8000) {
            return 0;
        }
        return 1;
    }
    return 0;
}

/* Top bit of pose byte +0xD0 (fighter +0xE0); with always == 0 only on stages 4 and 27. */
s32 BtlCharApi_TestPoseBit80(s32 objId, s32 always) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr == NULL) {
        return 0;
    }
    if (always == 0 && !BtlChar_IsStage4Or27()) {
        return 0;
    }
    return BtlChar_GetPos(chr)->unkD0_7;
}

/* Fighter flag 0x98. */
s32 BtlCharApi_TestFlag98(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0x98);
    }
    return 0;
}

/* Fighter flag 0xBE (out of ki: the state that leads to action 0xEA). */
s32 BtlCharApi_TestFlagBE(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xBE);
    }
    return 0;
}

/* Fighter word +0xE40. */
s32 BtlCharApi_GetUnkE40(s32 objId) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return chr->unkE40;
    }
    return 0;
}

/* Byte 0x8F + n of the character's parameter block. */
s32 BtlCharApi_GetParamByte8F(s32 objId, s32 n) {
    BtlCapiBChr *chr = BtlChar_FindByObjId(objId);

    if (chr != NULL) {
        return BtlParam_GetUnk8F(chr, n);
    }
    return 0;
}

/* Sequence poses. Asks for the entrance pose: one-frame flag 0xEF forces action 1 (animations 0x180 -> 0x181). */
void BtlCtrl_StartEntrance(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetFlag(chr, 0xEF);
    }
}

/* Ends the entrance pose: flag 0xF0 sends action 1 to action 0xB. */
void BtlCtrl_EndEntrance(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetFlag(chr, 0xF0);
    }
}

/* Asks for the win pose: flag 0xF1 forces action 2 (animations 0x182 -> 0x183). */
void BtlCtrl_StartWinPose(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetFlag(chr, 0xF1);
    }
}

/* Asks for the lose pose: flag 0xF2 forces action 3 (animation 0x184). */
void BtlCtrl_StartLosePose(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetFlag(chr, 0xF2);
    }
}

/* Whether the pose has settled: animation 0x181 or 0x183 (the loops), or 0x184 played to its end (flag 0x31). */
s32 BtlCtrl_IsPoseReached(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr == NULL) {
        return 0;
    }
    switch (BtlAnim_GetId(chr)) {
    case 0x181:
    case 0x183:
        return 1;
    case 0x184:
        if (BtlChar_TestFlag(chr, 0x31)) {
            return 1;
        }
        break;
    }
    return 0;
}

/* Scripted motion: stores the animation, blend time and loop mode of action 4 and holds flag 0xFA, which makes the
   fighter enter (or restart) action 4. */
void BtlCtrl_PlayMotion(s32 player, s32 motion, s32 loop, f32 blend) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        chr->motion = motion;
        chr->motionBlend = blend;
        chr->motionLoop = loop;
        BtlChar_SetHeldFlag(chr, 0xFA);
    }
}

/* Stores a position to warp to and holds flag 0xFC (taken by the placement code). */
void BtlCtrl_SetPos(s32 player, Vec4 *pos) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        Vec4_Copy(&chr->warpPos, pos);
        BtlChar_SetHeldFlag(chr, 0xFC);
    }
}

/* Copies the fighter's position (no caller). */
void BtlCtrl_GetPos(s32 player, Vec4 *out) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->pos);
    }
}

/* Stores a rotation to take and holds flag 0xFD. */
void BtlCtrl_SetRot(s32 player, Vec4 *rot) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        Vec4_Copy(&chr->warpRot, rot);
        BtlChar_SetHeldFlag(chr, 0xFD);
    }
}

/* Copies the fighter's rotation (no caller). */
void BtlCtrl_GetRot(s32 player, Vec4 *out) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        Vec4_Copy(out, &BtlChar_GetPos(chr)->rot);
    }
}

/* Index of the side's team member that is fighting. */
s32 BtlCtrl_GetActiveMember(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        return BtlMember_GetActiveIndex(chr);
    }
    return 0;
}

/* Whether the fighter is in action 4, the scripted motion. */
s32 BtlCtrl_IsAction4(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        return BtlAct_GetCurrent(chr) == 4;
    }
    return 0;
}

/* 1 while the scripted motion has NOT reached its end: always for a looping one, else while flag 0x31 is clear. */
s32 BtlCtrl_IsMotionPlaying(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        if (chr->motionLoop == 1) {
            return 1;
        }
        return BtlChar_TestFlag(chr, 0x31) == 0;
    }
    return 0;
}

/* Ends the scripted motion: drops the request flag 0xFA and raises 0xFB, on which action 4 leaves. */
void BtlCtrl_StopMotion(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_ClearFlag(chr, 0xFA);
        BtlChar_SetFlag(chr, 0xFB);
    }
}

/* Holds flag 0xFE and drops 0xFF: action 4 then sets effect request 0x13 every frame (aura at full level). */
void BtlCtrl_SetAuraOn(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0xFE);
        BtlChar_ClearFlag(chr, 0xFF);
    }
}

/* Drops flag 0xFE and holds 0xFF: action 4 then sets effect request 0x14 every frame (aura off). */
void BtlCtrl_SetAuraOff(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_ClearFlag(chr, 0xFE);
        BtlChar_SetHeldFlag(chr, 0xFF);
    }
}

/* Holds or drops flag 0x101: while held, action 4 sets effect request 7 (keeps the charge effect) and plays common
   sound BtlParam_GetChargeLoopSound every frame. Declared int with no return statement: the original does not tail-call. */
s32 BtlCtrl_SetChargeFx(s32 player, s32 on) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        if (on) {
            BtlChar_SetHeldFlag(chr, 0x101);
        } else {
            BtlChar_ClearFlag(chr, 0x101);
        }
    }
}

/* Holds flag 0x102: action 4 then drops 0x101 / 0x102 and sets effect requests 7 and 8 (the burst) with sound
   BtlParam_GetMaxPowerSound, once. */
void BtlCtrl_BurstChargeFx(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0x102);
    }
}

/* Drops flag 0x100. While it is held action 4 raises fighter flag 0x30, which clears bit 2 of the object's flags
   (the same flag the animation events 0x2000000 / 0x4000000 set and clear: the model is hidden). */
void BtlCtrl_ClearHidden(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_ClearFlag(chr, 0x100);
    }
}

/* Holds flag 0x100. */
void BtlCtrl_SetHidden(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0x100);
    }
}

/* Holds flag 0x103 + member: the fighter update re-reads that member's bonus levels from the setup. */
void BtlCtrl_ReloadMemberBonus(s32 player, u32 member) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL && member < 5) {
        BtlChar_SetHeldFlag(chr, member + 0x103);
    }
}

/* Holds flag 0x108 + member: the fighter update re-reads that member's ability words. */
void BtlCtrl_ReloadMemberAbilities(s32 player, u32 member) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL && member < 5) {
        BtlChar_SetHeldFlag(chr, member + 0x108);
    }
}

/* Holds flag 0x10D. */
void BtlCtrl_SetFlag10D(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_SetHeldFlag(chr, 0x10D);
    }
}

/* Drops flag 0x10D. */
void BtlCtrl_ClearFlag10D(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlChar_ClearFlag(chr, 0x10D);
    }
}

/* Adds percent % of a member's maximum to its health. The active member heals through BtlMember_AddHealth or is
   damaged through BtlMember_Damage (flags 0x42B: no defense, exact, no combo hit, no timer, cannot kill); a benched
   member is written directly. Int with no return statement, like the next two Add functions and SetPowerUp. */
s32 BtlCtrl_AddHp(s32 player, s32 member, f32 percent) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;
    s32 amount;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        amount = g->hpMax * percent * 0.01f;
        if (member == BtlMember_GetActiveIndex(chr)) {
            if (amount >= 0) {
                BtlMember_AddHealth(chr, amount);
            } else {
                BtlMember_Damage(chr, -amount, 0x42B);
            }
        } else {
            g->hp = BtlUtil_Clamp(g->hp + amount, 0, g->hpMax);
        }
    }
}

/* Raises a member's health to at least percent % of its maximum (at least 1 for a positive percentage). */
void BtlCtrl_RaiseHp(s32 player, s32 member, f32 percent) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;
    s32 target;
    s32 max;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        max = g->hpMax;
        target = max * percent * 0.01f;
        if (target <= 0 && 0.0f < percent) {
            target = 1;
        }
        g->hp = BtlUtil_Clamp(target, g->hp, max);
    }
}

/* Lowers a member's health to at most percent % of its maximum (at least 1 for a positive percentage). */
void BtlCtrl_LowerHp(s32 player, s32 member, f32 percent) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;
    s32 target;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        target = g->hpMax * percent * 0.01f;
        if (target <= 0 && 0.0f < percent) {
            target = 1;
        }
        g->hp = BtlUtil_Clamp(target, 0, g->hp);
    }
}

/* Adds bars * 20000 to a member's ki (the active member through BtlMember_AddKi / BtlMember_DrainKi). */
s32 BtlCtrl_AddKi(s32 player, s32 member, s32 bars) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;
    s32 amount;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        amount = bars * 20000;
        if (member == BtlMember_GetActiveIndex(chr)) {
            if (amount >= 0) {
                BtlMember_AddKi(chr, amount);
            } else {
                BtlMember_DrainKi(chr, -amount);
            }
        } else {
            g->ki = BtlUtil_Clamp(g->ki + amount, 0, g->kiMax);
        }
    }
}

/* Raises a member's ki to at least bars * 20000. */
void BtlCtrl_RaiseKi(s32 player, s32 member, s32 bars) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        g->ki = BtlUtil_Clamp(bars * 20000, g->ki, g->kiMax);
    }
}

/* Lowers a member's ki to at most bars * 20000. */
void BtlCtrl_LowerKi(s32 player, s32 member, s32 bars) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        g->ki = BtlUtil_Clamp(bars * 20000, 0, g->ki);
    }
}

/* Adds stocks * 100000 to a member's blast stock (the active member through BtlMember_AddBlast / SubBlast). */
s32 BtlCtrl_AddBlast(s32 player, s32 member, s32 stocks) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;
    s32 amount;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        amount = stocks * 100000;
        if (member == BtlMember_GetActiveIndex(chr)) {
            if (amount >= 0) {
                BtlMember_AddBlast(chr, amount);
            } else {
                BtlMember_SubBlast(chr, -amount);
            }
        } else {
            g->blast = BtlUtil_Clamp(g->blast + amount, 0, g->blastMax);
        }
    }
}

/* Raises a member's blast stock to at least stocks * 100000. */
void BtlCtrl_RaiseBlast(s32 player, s32 member, s32 stocks) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        g->blast = BtlUtil_Clamp(stocks * 100000, g->blast, g->blastMax);
    }
}

/* Lowers a member's blast stock to at most stocks * 100000. */
void BtlCtrl_LowerBlast(s32 player, s32 member, s32 stocks) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;

    if (chr != NULL) {
        g = BtlMember_GetGauge(chr, member);
        g->blast = BtlUtil_Clamp(stocks * 100000, 0, g->blast);
    }
}

/* Turns the powered-up mode on (ki filled, its timer set to 30000, flag 6 held) or off (flag 6 dropped). */
s32 BtlCtrl_SetMaxPower(s32 player, s32 on) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        if (on) {
            BtlMember_AddKi(chr, 100000);
            BtlMember_AddMaxPower(chr, 30000);
            BtlChar_SetHeldFlag(chr, 6);
        } else {
            BtlChar_ClearFlag(chr, 6);
        }
    }
}

/* Makes the fighter transform into character `id`: finds it among its four transformation slots, checks
   BtlAct_CanTransform without the cost, raises the blast stock to the cost and holds flag 0x116 + slot. */
s32 BtlCtrl_Transform(s32 player, s32 id) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    s32 slot;
    s32 i;
    BtlCapiBGauge *g;

    if (chr != NULL) {
        slot = -1;
        for (i = 0; i < 4; i++) {
            if (BtlParam_GetSlotId(chr, i) == id) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            return 0;
        }
        if (!BtlAct_CanTransform(chr, slot, 0, 0)) {
            return 0;
        }
        g = BtlMember_GetActiveGauge(chr);
        g->blast = BtlUtil_Clamp(BtlParam_GetSlotCost(chr, slot), g->blast, g->blastMax);
        BtlChar_SetHeldFlag(chr, slot + 0x116);
        BtlChar_ClearFlag(chr, 0xBE);
        chr->stunTimer = 0;
        return 1;
    }
    return 0;
}

/* Makes the fighter fuse into character `id`: the same over its three fusion slots (flag 0x11A + slot); it also
   refills the health of the fighter and of the fusion partner. */
s32 BtlCtrl_Fuse(s32 player, s32 id) {
    s32 partner;
    BtlCapiBChr *chr = BtlChar_Get(player);
    s32 slot;
    s32 i;
    BtlCapiBGauge *g;

    if (chr != NULL) {
        slot = -1;
        for (i = 0; i < 3; i++) {
            if (BtlParam_GetUnkB4(chr, i) == id) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            return 0;
        }
        if (!BtlAct_CanFuse(chr, slot, 0, 0, &partner)) {
            return 0;
        }
        g = BtlMember_GetActiveGauge(chr);
        g->blast = BtlUtil_Clamp(BtlParam_GetCostAE(chr, slot), g->blast, g->blastMax);
        BtlChar_SetHeldFlag(chr, slot + 0x11A);
        BtlChar_ClearFlag(chr, 0xBE);
        chr->stunTimer = 0;
        g->hp = g->hpMax;
#ifdef PORT
        if(TagTeam_Active()) {
            BtlCapiBGauge *pg=BtlMember_GetActiveGauge(BtlChar_Get(partner));
            pg->hp=pg->hpMax;
        } else
#endif
        BtlMember_GetGauge(chr, partner)->hp = BtlMember_GetGauge(chr, partner)->hpMax;
        return 1;
    }
    return 0;
}

/* Makes the side switch to team member `member`: fills the switch gauge and holds flag 0x11D, or for a fighter
   that is down holds flag 0x10E (which lets story mode replace it). */
s32 BtlCtrl_ChangeMember(s32 player, s32 member) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr == NULL) {
        return 0;
    }
    if (member == BtlMember_GetActiveIndex(chr)) {
        return 0;
    }
    if (!BtlChar_IsDead(chr)) {
        if (!BtlAct_CanSwitch(chr, 0, 0)) {
            return 0;
        }
    }
    if (!BtlMember_SetSwitchTarget(chr, member)) {
        return 0;
    }
    if (!BtlChar_IsDead(chr)) {
        chr->switchGauge = 100000;
        BtlChar_SetHeldFlag(chr, 0x11D);
        BtlChar_ClearFlag(chr, 0xBE);
        chr->stunTimer = 0;
    } else {
        BtlChar_SetHeldFlag(chr, 0x10E);
    }
    return 1;
}

/* Forces action 5 (kind 0, flag 0x110) or action 6 (kind 1, flag 0x111). */
s32 BtlCtrl_ForceFlag11x(s32 player, s32 kind) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    s32 flag;

    if (chr != NULL) {
        switch (kind) {
        case 0:
            flag = 0x110;
            break;
        case 1:
            flag = 0x111;
            break;
        default:
            flag = 0x110;
            break;
        }
        BtlChar_SetHeldFlag(chr, flag);
        BtlChar_ClearFlag(chr, 0xBE);
        chr->stunTimer = 0;
        return 1;
    }
    return 0;
}

/* Makes the fighter use a technique: kind 0 / 1 a move slot (blast stock raised to its cost; lock-on flag 5 if the
   move's flag bit 1 is set), kind 2..4 a skill slot (ki raised to its cost, flags 0x11F and 5; kind 4 also starts the
   powered-up mode with full ki). Holds the input flag 0x120 + kind and 0x11E (pad input off). */
s32 BtlCtrl_UseTechnique(s32 player, s32 kind) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    BtlCapiBGauge *g;

    if (chr == NULL) {
        return 0;
    }
    g = BtlMember_GetActiveGauge(chr);
    switch (kind) {
    case 0:
    case 1:
        g->blast = BtlUtil_Clamp(BtlSkill_GetBlastCost(chr, kind), g->blast, g->blastMax);
        if (BtlSkill_GetFlags(chr, kind) & 1) {
            BtlChar_SetHeldFlag(chr, 5);
        }
        break;
    case 2:
    case 3:
    case 4:
        g->ki = BtlUtil_Clamp(BtlSuper_GetKiCost(chr, kind), g->ki, g->kiMax);
        if (kind == 4) {
            BtlChar_SetHeldFlag(chr, 6);
            g->maxPower = 30000;
            g->ki = g->kiMax;
        }
        BtlChar_SetHeldFlag(chr, 0x11F);
        BtlChar_SetHeldFlag(chr, 5);
        break;
    default:
        return 0;
    }
    BtlChar_SetHeldFlag(chr, kind + 0x120);
    BtlChar_SetHeldFlag(chr, 0x11E);
    BtlChar_ClearFlag(chr, 0xBE);
    chr->stunTimer = 0;
    chr->unkE40 = 0;
    return 1;
}

/* Forces one of the waiting actions 7..10 (flag 0x112 + kind): the fighter stands and faces the opponent. */
s32 BtlCtrl_ForceReaction(s32 player, u32 kind) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr == NULL) {
        return 0;
    }
    if (kind >= 4) {
        return 0;
    }
    BtlChar_SetHeldFlag(chr, kind + 0x112);
    BtlChar_ClearFlag(chr, 0xBE);
    chr->stunTimer = 0;
    return 1;
}

/* Whether the fighter is in an action a script (or the object loader) may interrupt: not a throw / clash / move /
   technique / form change / member switch action. */
s32 BtlCtrl_IsInterruptible(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    s32 action;

    if (chr != NULL) {
        action = BtlAct_GetCurrent(chr);
        switch (action) {
        case 0xB7:
        case 0xB8:
        case 0xB9:
        case 0xBA:
        case 0xBB:
        case 0xBC:
        case 0xBE:
        case 0xBF:
        case 0xFA:
        case 0xFB:
        case 0xFC:
        case 0x104:
        case 0x105:
        case 0x130:
        case 0x131:
        case 0x132:
            return 0;
        }
        if ((u32)(action - 0xFD) < 6) {
            return 0;
        }
        if ((u32)(action - 0x106) < 0x36) {
            return 0;
        }
        if ((u32)(action - 0xEC) < 0xE) {
            return 0;
        }
        return 1;
    }
    return 0;
}

/* Whether the fighter can take a forced action now: not frozen, not stunned, no reaction pending, flags 0xAF and
   0xB9 clear, and standing (action 0xB, 0xD, 0xE, 0x36) or, when down, lying (0xD8, 0xEB). */
s32 BtlCtrl_CanAct(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    s32 action;

    if (chr == NULL) {
        return 0;
    }
    if (BtlChar_IsFrozen(chr)) {
        return 0;
    }
    if (chr->stunTimer > 0) {
        return 0;
    }
    if (chr->reactId >= 3) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xAF)) {
        return 0;
    }
    if (BtlChar_TestFlag(chr, 0xB9)) {
        return 0;
    }
    action = BtlAct_GetCurrent(chr);
    switch (action) {
    case 0xB:
    case 0xD:
    case 0xE:
    case 0x36:
        return 1;
    default:
        if (BtlChar_IsDead(chr)) {
            switch (action) {
            case 0xD8:
            case 0xEB:
                return 1;
            }
        }
        break;
    }
    return 0;
}

/* Whether the character-change request being served is of type 0, for this player, and waits for the loader. */
s32 BtlChange_IsPendingType0(s32 player) {
    BtlCapiBChangeQueue *q;

    if (gBtlChars == NULL) {
        return 0;
    }
    q = &gBtlChars->change;
    if (q->cur == NULL) {
        return 0;
    }
    if (q->cur->type != 0) {
        return 0;
    }
    if (q->cur->player != player) {
        return 0;
    }
    return q->state == 1;
}

/* The same for a request of type 1. */
s32 BtlChange_IsPendingType1(s32 player) {
    BtlCapiBChangeQueue *q;
    s32 type;

    if (gBtlChars == NULL) {
        return 0;
    }
    q = &gBtlChars->change;
    if (q->cur == NULL) {
        return 0;
    }
    type = q->cur->type;
    if (type != 1) {
        return 0;
    }
    if (q->cur->player != player) {
        return 0;
    }
    return q->state == type;
}

/* Copies the seven parameter words of the request being served (any pointer may be NULL). */
void BtlChange_GetArgs(s32 *a, s32 *b, s32 *c, s32 *d, s32 *e, s32 *f, s32 *g) {
    BtlCapiBChangeQueue *q = &gBtlChars->change;

    if (q->cur == NULL) {
        return;
    }
    if (a != NULL) {
        *a = q->cur->arg[0];
    }
    if (b != NULL) {
        *b = q->cur->arg[1];
    }
    if (c != NULL) {
        *c = q->cur->arg[2];
    }
    if (d != NULL) {
        *d = q->cur->arg[3];
    }
    if (e != NULL) {
        *e = q->cur->arg[4];
    }
    if (f != NULL) {
        *f = q->cur->arg[5];
    }
    if (g != NULL) {
        *g = q->cur->arg[6];
    }
}

/* The loader has taken the request (BtlChange_SetTaken). */
void BtlChange_NotifyTaken(void) {
    BtlChange_SetTaken();
}

/* The request's files are loaded (BtlChange_SetLoaded). */
void BtlChange_NotifyLoaded(void) {
    BtlChange_SetLoaded();
}

/* Whether the request has reached state 4 and the battle is not paused. */
s32 BtlChange_IsReady(void) {
    BtlCapiBChangeQueue *q = &gBtlChars->change;

    if (q->cur == NULL) {
        return 0;
    }
    if (Battle_GetWork()->flags & 0x100) {
        return 0;
    }
    return q->state == 4;
}

/* Attaches a loaded partner object to the player's fighter (BtlPartner_Attach). */
void BtlCtrl_AttachPartner(s32 player, s32 slot, s32 objId) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        BtlPartner_Attach(chr, slot, objId);
    }
}

/* Icon of bit `bit` of the fighter's button word, for a button prompt: 0x10..0x13 for bits 4..7 (up, down, left,
   right), else by the pad button the fighter's key table gives that bit: 0x1000 (triangle) -> 0, 0x8000 (square)
   -> 1, 0x2000 (circle) -> 2, 0x4000 (cross) -> 3; -1 otherwise. The first argument is the fighter, not an id. */
s32 BtlCharApi_GetButtonIcon(BtlCapiBChr *chr, u32 bit) {
    u32 *mask;

    switch (bit) {
    case 4:
        return 0x10;
    case 5:
        return 0x11;
    case 6:
        return 0x12;
    case 7:
        return 0x13;
    }
    mask = chr->keyMask;
    mask += bit;
    switch (*mask) {
    case 0x1000:
        return 0;
    case 0x2000:
        return 2;
    case 0x4000:
        return 3;
    case 0x8000:
        return 1;
    }
    return -1;
}

/* Health of the side's active member. */
s32 BtlSide_GetHp(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->hp;
    }
    return 0;
}

/* Ki of the side's active member. */
s32 BtlSide_GetKi(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->ki;
    }
    return 0;
}

/* Blast stock of the side's active member. */
s32 BtlSide_GetBlast(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->blast;
    }
    return 0;
}

/* Its maximum. */
s32 BtlSide_GetBlastMax(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->blastMax;
    }
    return 0;
}

/* Gauge +0x1C of the side's active member (the powered-up mode's timer). */
s32 BtlSide_GetMaxPower(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlMember_GetActiveGauge(chr)->maxPower;
    }
    return 0;
}

/* The side's switch gauge. */
s32 BtlSide_GetSwitchGauge(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return chr->switchGauge;
    }
    return 0;
}

/* Index of the side's member that is fighting, -1 for no fighter. */
s32 BtlSide_GetActiveMember(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr == NULL) {
        return -1;
    }
    return BtlMember_GetActiveIndex(chr);
}

/* Index of the member a switch would bring in, -1 with fewer than two members alive. */
s32 BtlSide_GetSwitchTarget(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr == NULL) {
        return -1;
    }
    if (BtlMember_CountAlive(chr) < 2) {
        return -1;
    }
    return BtlMember_GetSwitchTarget(chr);
}

/* Number of the side's members that are alive, -1 for no fighter. */
s32 BtlSide_CountAlive(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr == NULL) {
        return -1;
    }
    return BtlMember_CountAlive(chr);
}

/* Health of the member a switch would bring in, 0 with fewer than two alive. */
s32 BtlSide_GetSwitchTargetHp(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlMember_CountAlive(chr) < 2) {
            return 0;
        }
        return BtlMember_GetGauge(chr, BtlMember_GetSwitchTarget(chr))->hp;
    }
    return 0;
}

/* Its maximum. */
s32 BtlSide_GetSwitchTargetHpMax(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        if (BtlMember_CountAlive(chr) < 2) {
            return 0;
        }
        return BtlMember_GetGauge(chr, BtlMember_GetSwitchTarget(chr))->hpMax;
    }
    return 0;
}

/* Word +0x2C of the side's character parameter block. */
s32 BtlSide_GetParamUnk2C(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlParam_GetGaugeB(chr);
    }
    return 0;
}

/* Fighter flag 6: the side is in the powered-up mode. */
s32 BtlSide_IsPoweredUp(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 6);
    }
    return 0;
}

/* Fighter flag 0xBE. */
s32 BtlSide_TestFlagBE(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 0xBE);
    }
    return 0;
}

/* Whether the player's active member has no health left. */
s32 BtlCtrl_IsActiveDead(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        return BtlChar_IsDead(chr);
    }
    return 0;
}

/* Whether every member of the player's team has no health left. */
s32 BtlCtrl_IsTeamDead(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);
    s32 dead = 1;
    s32 i;

    if (chr != NULL) {
        for (i = 0; i < chr->memberCount; i++) {
            if (BtlMember_GetGauge(chr, i)->hp > 0) {
                dead = 0;
                break;
            }
        }
        return dead;
    }
    return 0;
}

/* Fighter flag 7 (ring out: the end-of-battle check gives the other side the win). */
s32 BtlCtrl_TestFlag7(s32 player) {
    BtlCapiBChr *chr = BtlChar_Get(player);

    if (chr != NULL) {
        return BtlChar_TestFlag(chr, 7);
    }
    return 0;
}

/* Hits of the combo the side is dealing (counted on the opponent). */
s32 BtlSide_GetComboHits(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_Get(BtlOpp_GetPlayer(chr))->comboHits;
    }
    return 0;
}

/* Damage of the combo the side is dealing, -1 while the opponent has flag 0xED. */
s32 BtlSide_GetComboDamage(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);
    BtlCapiBChr *opp;

    if (chr != NULL) {
        opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
        if (BtlChar_TestFlag(opp, 0xED)) {
            return -1;
        }
        return opp->comboDamage;
    }
    return 0;
}

/* Whether the combo display timer of the opponent is running. */
s32 BtlSide_IsComboShown(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);

    if (chr != NULL) {
        return BtlChar_Get(BtlOpp_GetPlayer(chr))->comboTimer > 0;
    }
    return 0;
}

/* Whether a counted hit landed on the opponent this frame (0 while it is frozen or the battle is loading). */
s32 BtlSide_IsComboHitNew(s32 side) {
    BtlCapiBChr *chr = BtlChar_FindBySide(side);
    BtlCapiBChr *opp;

    if (chr != NULL) {
        opp = BtlChar_Get(BtlOpp_GetPlayer(chr));
        if (BtlChar_IsFrozen(opp)) {
            return 0;
        }
        if (Battle_GetWork()->flags & 0x2000) {
            return 0;
        }
        return opp->comboNewHit;
    }
    return 0;
}
