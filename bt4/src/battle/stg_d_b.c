/*
 * Stage object animations (0x115170..0x115478), a part of the stage model code (0x114C60..0x115DE0).
 *
 * Member 6 of the stage file is an offset table (StgOfsTable) with three entries per animation: a model
 * resource, its VU1 packet node and a track whose length in frames is the u16 at +0xC. A stage object whose
 * definition has `anim` > 0 plays animation `anim` (1-based) in a loop while it is whole; objects with type bit
 * 0x40000000 keep playing after they are broken. The frame counter lives in the object DEFINITION (inside the
 * stage file), not in the runtime object. The drawing side (0x115478, 0x115DE0) reads it; nothing in the fight
 * simulation does.
 */
#include "common.h"
#include "sys/math3d.h"
#include "sys/randf.h"
#include "sys/vu1_packet.h"
#include "battle/stg_d.h"

/* Offset table of the stage file (StgOfsTable in battle/stg_a.h). */
typedef struct StgAnimTable {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 count;
    /* 0x0C */ s32 relocated;
    /* 0x10 */ s32 entries[1];
} StgAnimTable;

/* Third entry of an animation: only the length is read here. */
typedef struct StgAnimTrack {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ u16 length;     /* frames */
} StgAnimTrack;

/* Local view of a stage object definition (StgObjDef in battle/stg_a.h, where +0x1C is still unknown). */
typedef struct StgAnimObjDef {
    /* 0x00 */ s32 hp;
    /* 0x04 */ s32 type;       /* bit 0x40000000: animated even when broken */
    /* 0x08 */ s32 anim;       /* 1-based animation number, <= 0 none */
    /* 0x0C */ s32 parent;
    /* 0x10 */ u8 unk10[0xC];
    /* 0x1C */ f32 frame;      /* current animation frame, written here at run time */
    /* 0x20 */ u8 unk20[0x20];
} StgAnimObjDef; /* size 0x40 */

/* Local views of the stage parameter block and of the stage object. */
typedef struct StgAnimData {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 unk4;
    /* 0x08 */ s32 objCount;
    /* 0x0C */ StgAnimObjDef *objDefs;
} StgAnimData;

typedef struct StgAnimStage {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ StgAnimData *data;
} StgAnimStage;

/* Local view of the common resources: the stage file pointer (BattleRes.stage). */
typedef struct StgAnimRes {
    /* 0x00 */ u8 unk0[0x24];
    /* 0x24 */ u32 *stage;
} StgAnimRes;

/* Local view of the battle work: only the flag word. */
typedef struct StgAnimWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags;    /* 0x100: paused */
} StgAnimWork;

extern StgAnimRes *gCommonRes;
extern StgAnimStage *gBtlStage;
/* 0x2FE914: this object's only .sdata word (stgm_a.c reads it too). */
StgAnimTable *gStgAnimTable = NULL;

extern StgAnimWork *Battle_GetWork(void);
extern void BtlStage_Init(void *stage, void *arg);
extern s32 BtlStage_IsObjBroken(s32 idx);
extern void StgOfsTable_Relocate(StgAnimTable *t);
extern void *StgOfsTable_Get(StgAnimTable *t, s32 i);
extern s32 StgOfsTable_GetCount(StgAnimTable *t);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);

/* Starts the stage: the stage object is the block at header word 1 of the stage file. */
void StgModel_InitStage(void) {
    u32 *file = gCommonRes->stage;
    void *stage = (u8 *)file + file[1] / 4 * 4;

    BtlStage_Init(stage, stage);
}

/* Gives every animated, whole object a random start frame in its animation. */
void StgModel_ResetAnims(void) {
    StgAnimData *data;
    StgAnimObjDef *def;
    s32 i;
    s32 anim;

    if (gStgAnimTable == NULL) {
        return;
    }
    data = gBtlStage->data;
    def = data->objDefs;
    for (i = 0; i < data->objCount; i++) {
        anim = def->anim;
        if (anim <= 0) {
            def++;
            continue;
        }
        if (!(def->type & 0x40000000) && BtlStage_IsObjBroken(i)) {
            def++;
            continue;
        }
        {
            StgAnimTrack *track = StgOfsTable_Get(gStgAnimTable, anim * 3 - 1);

            def->frame = Rand_FloatRange(0.0f, 1.0f) * (f32)track->length;
        }
        def++;
    }
}

/* Binds the animation table (member 6 of the stage file) and prepares each animation's model and packet. */
void StgModel_BindAnims(void) {
    u32 *file = gCommonRes->stage;
    void *res;
    u32 *node;
    s32 i;
    s32 count;

    if (file == NULL) {
        return;
    }
    if (file[10] == file[9]) {
        gStgAnimTable = NULL;
        return;
    }
    gStgAnimTable = (StgAnimTable *)((u8 *)file + file[9] / 4 * 4);
    StgOfsTable_Relocate(gStgAnimTable);
    count = StgOfsTable_GetCount(gStgAnimTable);
    for (i = 0; i < count; i += 3) {
        res = StgOfsTable_Get(gStgAnimTable, i);
        Res_RelocateOffsets(&res, res, res);
        node = StgOfsTable_Get(gStgAnimTable, i + 1);
        Vu1Node_OrFlags((Vu1Node *)((u8 *)node + node[0] / 4 * 4), 0x3480, 0x3700);
    }
    StgModel_ResetAnims();
}

/* Forgets the animation table. */
void StgModel_ClearAnims(void) {
    gStgAnimTable = NULL;
}

/* Per frame: advances the animation of every animated object that is whole by 2 frames, looping. Not while paused. */
void StgModel_UpdateAnims(void) {
    StgAnimData *data;
    StgAnimObjDef *def;
    s32 i;
    s32 anim;
    StgAnimTrack *track;

    if (gStgAnimTable == NULL) {
        return;
    }
    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    data = gBtlStage->data;
    def = data->objDefs;
    for (i = 0; i < data->objCount; i++) {
        anim = def->anim;
        if (anim <= 0) {
            def++;
            continue;
        }
        if (!(def->type & 0x40000000) && BtlStage_IsObjBroken(i)) {
            def++;
            continue;
        }
        def->frame += 2.0f;
        track = StgOfsTable_Get(gStgAnimTable, anim * 3 - 1);
        if ((f32)track->length < def->frame) {
            def->frame = 0.0f;
        }
        def++;
    }
}
