#include "battle/btl_effect_owner.h"
#include "common.h"
#include "battle/eft_q.h"
#include "sys/gfx_ot.h"

/*
 * Effect tasks, 0x170A50..0x174A70. See include/battle/eft_q.h.
 */

typedef struct EftQBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100 = paused */
} EftQBattleWork;

/* The view being drawn (include/battle/btl_cam.h). */
typedef struct EftQView {
    /* 0x000 */ u8 unk0[0x140];
    /* 0x140 */ Mtx44 world2screen;
    /* 0x180 */ u8 unk180[0xA0];
    /* 0x220 */ Vec4 pos;
} EftQView;

extern EftQView *gBtlCamView;

extern void *memset(void *dst, s32 c, u32 n);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vu0Cur_Push(void);            /* VU0 matrix stack push */
extern void Vu0Cur_LoadMtx(Mtx44 *m);        /* load the matrix */
extern void Vu0Cur_Pop(void);            /* pop */
extern void Vec4_Lerp(Vec4 *dst, Vec4 *a, Vec4 *b, f32 t); /* dst = a * t + b * (1 - t) */
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);
#ifdef PORT /* PC build: parameters in the definition's order; the macro reorders the calls of this file */
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, Vec4 *from, f32 len);
#define Vec3_ScaleAdd(dst, dir, len, from) Vec3_ScaleAdd(dst, dir, from, len)
#else
extern void Vec3_ScaleAdd(Vec4 *dst, Vec4 *dir, f32 len, Vec4 *from);     /* dst = from + dir * len */
#endif
extern f32 Vec3_Dist(Vec4 *a, Vec4 *b);                               /* distance */
extern s32 Vu0Cur_ProjectPoints(EftQScr *out, Vec4 *pos, s32 count);             /* projects count points */
extern void ClipVtx_Set(EftQVert *out, Vec4 *pos, Vec4 *uv, Vec4 *col); /* builds one vertex */
extern void EftGfx_DrawPolyAvgZ(EftQVert *verts, s32 arg1, s32 arg2, s32 arg3, s32 flip, u64 tex, s32 zOfs);
extern void EftMath_MtxFromDir(Mtx44 *out, Vec4 *dir, f32 angle);
extern void Mtx_ProjectPoints(EftQScr *out, Mtx44 *m, Vec4 *pos, s32 count);  /* projects count points */
extern s32 IVec4_InGsRange4(EftQScr *a, EftQScr *b, EftQScr *c, EftQScr *d);  /* 1 when the quad is off screen */

extern EftQBattleWork *Battle_GetWork(void);
extern s32 Battle_IsSplitScreen(void);
extern s32 BtlPool_GetCurrent(void);
extern void BtlPool_SetCurrent(s32 slot);
extern void BtlPool_Reset(s32 slot);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern s32 BtlScene_GetCharCount(void);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern s32 *BtlScene_GetCharPackEntry(s32 side, s32 idx);
extern s32 BtlScene_TestCharPackBit(s32 side, s32 bit);
extern s32 BtlScene_IsCharInView(s32 objId);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern void BtlScene_Reset(s32 mode);
extern void *BtlTask_CreateChildList(EftQTask *task, s32 count, s32 workSize);
extern EftQTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern void BtlTask_Kill(EftQTask *task);
extern void BtlTask_SetDead(EftQTask *task);              /* marks the task dead */
extern void BtlTask_SetOwnerTag(EftQTask *task, s32 flag);    /* ors bits into the task's class flags */
extern EftQTask *BtlTask_GetParent(EftQTask *task);         /* parent task */
extern u64 EftVram_AddTex(EftQTex *tex, s32 a, s32 b);   /* GS TEX0 of a texture for this frame */
extern u64 EftVram_AddImage(EftQTex *tex, s32 a, s32 b);   /* the same for an image with a separate palette */
extern u64 EftVram_AddClut(EftQTex *tex);                 /* palette address of a texture */
extern void EftTexSet_Load32(EftQTexSet *set, s32 *entry); /* builds a texture set from a pack entry */

extern s32 BtlCharApi_GetChara(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern s32 BtlCharApi_ObjGetParamFlags0(s32 objId);
extern s32 BtlCharApi_GetAuraType(s32 objId);
extern s32 BtlCharApi_HasKiBlastType2(s32 objId);
extern s32 BtlCharApi_HasKiBlastType3(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out);
extern s32 BtlCharApi_IsHidden(s32 objId);
extern s32 BtlCharApi_IsModelNew(s32 objId);
extern s32 BtlCharApi_ObjTestFlagBit21(s32 objId);
extern s32 BtlCharApi_IsFlag8Action104(s32 objId);
extern s32 BtlCharApi_GetMemberUnk60(s32 objId);
extern s32 BtlCharApi_TestFlag2B(s32 objId);
extern s32 BtlCharApi_IsInRushSequence(s32 objId);
extern s32 BtlCharApi_GetRushFinishPhase(s32 objId);

/* ---- 1. power-up glow: the tasks and manager of the module whose particles are in eft_p_b.c (0x16DCA0..) -- */

extern EftGlowMgr *gEftGlow;      /* 0x2FEA38 */
extern EftGlowCfg *gEftGlowCfg;   /* 0x2FEA3C */
extern EftGlowCfg *gEftGlowCfg2;  /* 0x2FEA40: the same pointer */
extern void *gEftGlowList;        /* 0x2FEA44: the manager's child list */
extern u8 gEftGlowClass[0x18];    /* 0x2C3B68 */

/* eft_p_b.c */
extern void EftGlow_BuildTables(void);                           /* builds the shape tables, links the particle pool */
extern void EftGlow_Begin(EftGlow *w, s32 *arg);                 /* starts the glow (arg[0] = object id) */
extern void EftGlow_Step(EftGlow *w, s32 objId);
extern void EftGlow_FreeParts(s32 objId);                        /* frees the fighter's glow particles */
extern void EftGlow_StepParts(EftGlow *w, s32 objId);
extern void EftGlow_DrawParts(EftGlow *w, s32 objId, f32 alpha);

/* A colour stored as bytes 0..255 to 0..1. The original has one copy of the constant per call site, which a call
 * through an inline function reproduces (a literal argument of a direct call is shared between neighbouring calls
 * and ends up in a saved register). */
static inline void EftGlow_ColorFromBytes(Vec4 *dst, Vec4 *src) {
    Vec4_Scale(dst, src, 0.0039215686f);
}

/* The two base colours of a glow, from the row of its variant. Inline in the original: the flag arrives as a byte
 * (the caller's test and this one are both kept), the constants are not shared, and `colorB.w = 0` is written in
 * both arms (the compiler merges the two tails into one call). */
static inline void EftGlow_SetBaseColors(EftGlow *w, u8 textured, s32 row) {
    if (textured) {
        Vec4_Scale(&w->colorA, &gEftGlowCfg->colorTex[row * EFT_GLOW_ROW], 0.0039215686f);
        Vec4_Scale(&w->colorB, &gEftGlowCfg->colorTex[row * EFT_GLOW_ROW], 0.0039215686f);
        w->colorB.w = 0.0f;
    } else {
        Vec4_Scale(&w->colorA, &gEftGlowCfg->color[w->type * EFT_GLOW_ROW], 0.0039215686f);
        Vec4_Scale(&w->colorB, &gEftGlowCfg->color[w->type * EFT_GLOW_ROW], 0.0039215686f);
        w->colorB.w = 0.0f;
    }
}

/* Renews the GS texture words the glow draws with, and once per frame those of the second texture set. */
void EftGlow_UpdateTextures(EftGlow *w) {
    EftGlowMgr *mgr = gEftGlow;
    EftQTexSet *set = &mgr->res[0].tex;
    EftGlowRes *res;
    s32 i;

    if (w->flags & EFT_GLOW_TEXTURED) {
        u64 tex0 = EftVram_AddImage(&mgr->res[0].tex.entry[8], 1, 0);

        for (i = w->texFirst; i < w->texFirst + w->texCount; i++) {
            EftGlowTexView *v = (EftGlowTexView *)&w->paramFlags;

            v->tex[i - w->texFirst] = tex0 | ((u64)EftVram_AddClut(&set->entry[i]) << 37);
        }
    } else {
        w->tex[0] = EftVram_AddImage(&set->entry[0], 1, 0);
        w->tex[0] |= (u64)EftVram_AddClut(&set->entry[w->type]) << 37;
    }
    mgr = gEftGlow;
    res = &mgr->res[1];
    if (res->ready == 0) {
        EftQTexSet *set2 = &mgr->res[1].tex;

        for (i = 0; i < set2->count; i++) {
            set2->entry[i].tex0 = EftVram_AddTex(&set2->entry[i], 1, 0);
        }
        res->ready = 1;
    }
}

/* Selects the glow's variant and colours from the fighter's aura type. Types 8..10 are the textured variant; they
 * are all turned into type 8 first, so the cases 9 and 10 below never run and `row` stays 0. */
void EftGlow_SetType(EftGlow *w, s32 objId, s32 type, s32 paramFlags) {
    s32 row = 0;
    s32 i;
    s32 textured;

    w->paramFlags = paramFlags;
    w->flags &= ~EFT_GLOW_TEXTURED;
    if ((u32)(type - 8) < 3) {
        w->flags |= EFT_GLOW_TEXTURED;
        type = 8;
        w->texCount = 1;
        w->texFirst = type;
    }
    switch (type) {
    case 8:
        w->texFirst = type;
        w->texCount = 2;
        break;
    case 9:
        w->texFirst = 10;
        w->texCount = 2;
        row = 2;
        break;
    case 10:
        w->texFirst = 12;
        w->texCount = 2;
        row = 3;
        break;
    }
    w->type = type;
    w->type2 = type;
    w->layers = 1;
    textured = w->flags & EFT_GLOW_TEXTURED;
    if (textured) {
        w->layers = 2;
    }
    EftGlow_SetBaseColors(w, textured, row);
    for (i = 0; i < 4; i++) {
        if (w->flags & EFT_GLOW_TEXTURED) {
            EftGlow_ColorFromBytes(&w->color[i], &gEftGlowCfg->colorTex[row * EFT_GLOW_ROW + i + 1]);
        } else {
            s32 j;

            if (w->type < 8) {
                j = 0;
            } else {
                j = i;
            }
            EftGlow_ColorFromBytes(&w->color[i], &gEftGlowCfg->color[w->type * EFT_GLOW_ROW + j + 1]);
        }
    }
}

/* Reads the fighter's aura type, parameter flags and height into the glow. */
void EftGlow_Setup(EftGlow *w, s32 objId) {
    s32 paramFlags = BtlCharApi_ObjGetParamFlags0(objId);
    s32 type = BtlCharApi_GetAuraType(objId);

    EftGlow_SetType(w, objId, type, paramFlags);
    w->scale = BtlCharApi_GetHeight(objId) / 19.35f;
}

/* Init callback of the glow task: arg[0] is the fighter's object id. */
void EftGlowTask_Init(EftQTask *task, s32 *arg) {
    EftGlow *w = task->work;

    memset(w, 0, sizeof(EftGlow));
    EftGlow_Setup(w, arg[0]);
    EftGlow_Begin(w, arg);
    gEftGlow->live++;
}

/* Term callback: frees the particles and the manager's task slot. */
void EftGlowTask_Term(EftQTask *task) {
    EftGlow *w = task->work;
    s32 *objId = &w->objId;

    EftGlow_FreeParts(*objId);
    w->flags = 0;
    gEftGlow->live--;
    gEftGlow->tasks[*objId] = NULL;
}

/* Update callback: re-reads the fighter's type every frame, steps the particles unless paused. */
void EftGlowTask_Update(EftQTask *task) {
    EftGlow *w = task->work;
    s32 *objId = &w->objId;

    if (BtlCharApi_IsModelNew(*objId)) {
        w->flags |= EFT_GLOW_MODELNEW;
    }
    EftGlow_Setup(w, *objId);
    w->flags &= ~EFT_GLOW_MODELNEW;
    if (!(Battle_GetWork()->flags & 0x100)) {
        EftGlow_Step(w, *objId);
        switch (w->step) {
        case 0:
            w->step = 1;
            break;
        case 1:
            w->step = 2;
            break;
        case 2:
            break;
        }
        EftGlow_StepParts(w, *objId);
    }
    if (w->flags & EFT_GLOW_KILL) {
        BtlTask_SetDead(task);
    } else {
        EftGlow_UpdateTextures(w);
    }
}

/* Post-update callback: only fetches the battle work. */
void EftGlowTask_PostUpdate(EftQTask *task) {
    if (Battle_GetWork()->flags & 0x100) {
    }
}

/* Reset callback: kills the task. */
void EftGlowTask_Reset(EftQTask *task) {
    BtlTask_SetDead(task);
}

/* Draw callback: skipped for a hidden fighter; 30% alpha when the camera is on a fighter with object flag bit 21. */
void EftGlowTask_Draw(EftQTask *task) {
    f32 alpha = 1.0f;
    EftGlow *w = task->work;
    s32 *objId = &w->objId;

    if (BtlCharApi_IsHidden(*objId)) {
        return;
    }
    if (BtlScene_IsCharInView(*objId) && BtlCharApi_ObjTestFlagBit21(*objId)) {
        alpha = 0.3f;
    }
    Vu0Cur_Push();
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    EftGlow_DrawParts(w, *objId, alpha);
    Vu0Cur_Pop();
}

/* Manager init: the 0x700-byte work, 70 particles per character, the two texture sets and the settings. */
void EftGlowMgr_Init(EftQTask *task) {
    s32 i;

    gEftGlow = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftGlowMgr));
    memset(gEftGlow, 0, sizeof(EftGlowMgr));
    gEftGlow->charCount = BtlScene_GetCharCount();
    gEftGlow->partMax = gEftGlow->charCount * 70;
    gEftGlow->unkC = 0;
    gEftGlow->unk10 = 0;
    gEftGlow->unk14 = 0;
    gEftGlow->parts = BtlPool_Alloc(BtlPool_GetCurrent(), gEftGlow->partMax * 0xC0);
    memset(gEftGlow->parts, 0, gEftGlow->partMax * 0xC0);
    gEftGlow->tasks = BtlPool_Alloc(BtlPool_GetCurrent(), gEftGlow->charCount * 4);
    memset(gEftGlow->tasks, 0, gEftGlow->charCount * 4);
    gEftGlow->res[0].entry = BtlScene_GetCommonEntry(9);
    gEftGlow->res[1].entry = BtlScene_GetCommonEntry(8);
    for (i = 0; i < 2; i++) {
        EftTexSet_Load32(&gEftGlow->res[i].tex, gEftGlow->res[i].entry);
    }
    gEftGlow->cfg = BtlScene_GetCommonEntry(7);
    gEftGlowCfg2 = gEftGlowCfg = (EftGlowCfg *)gEftGlow->cfg; /* 0x2FEA40 is stored first */
    for (i = 0; i < gEftGlow->charCount; i++) {
        gEftGlow->tasks[i] = NULL;
    }
    EftGlow_BuildTables();
    gEftGlowList = BtlTask_CreateChildList(task, gEftGlow->charCount, sizeof(EftGlow));
}

/* Manager term. */
void EftGlowMgr_Term(EftQTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftGlow->tasks);
    BtlPool_Free(BtlPool_GetCurrent(), gEftGlow->parts);
    BtlPool_Free(BtlPool_GetCurrent(), gEftGlow);
    gEftGlow = NULL;
}

/* Manager update: the textures have to be renewed this frame. */
void EftGlowMgr_Update(EftQTask *task) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gEftGlow->res[i].ready = 0;
    }
}

/* Starts the fighter's glow, or restarts the one it has. Returns 1 unless no task could be made. */
s32 EftGlow_Start(s32 objId) {
    s32 arg[4];
    EftQTask *task;

    if (gEftGlow->tasks[objId] == NULL) {
        arg[0] = objId;
        task = BtlTaskList_AddTail(gEftGlowList, gEftGlowClass, arg);
        if (task == NULL) {
            return 0;
        }
        gEftGlow->tasks[objId] = task;
    } else {
        EftGlow *w = gEftGlow->tasks[objId]->work;

        EftGlow_Begin(w, &w->objId);
    }
    return 1;
}

/* Starts the fade-out of the fighter's glow. Returns 0 when it is not active or already fading. */
s32 EftGlow_FadeOut(s32 objId) {
    EftQTask *task = gEftGlow->tasks[objId];

    if (task != NULL) {
        EftGlow *w = task->work;

        if (!(w->flags & EFT_GLOW_ACTIVE)) {
            return 0;
        }
        if (w->flags & EFT_GLOW_FADING) {
            return 0;
        }
        w->flags |= EFT_GLOW_FADING;
        w->fade = 2;
        w->fadeTime = gEftGlowCfg->fadeOutTime;
        w->fadeFrom = 1.0f;
        w->unk14 = 9;
    }
    return 1;
}

/* Removes the fighter's glow at its next update. */
s32 EftGlow_Kill(s32 objId) {
    EftQTask *task = gEftGlow->tasks[objId];

    if (task != NULL) {
        EftGlow *w = task->work;

        if (!(w->flags & EFT_GLOW_ACTIVE)) {
            return 0;
        }
        w->flags |= EFT_GLOW_KILL;
    }
    return 1;
}

/* Entry point of the fighter effect layer (requests 4 and 5): 0 start, 1 fade out, 2 remove. */
s32 EftGlow_Request(s32 objId, s32 cmd) {
    if (gEftGlow == NULL) {
        return 0;
    }
    switch (cmd) {
    case 0:
        EftGlow_Start(objId);
        break;
    case 1:
        EftGlow_FadeOut(objId);
        break;
    case 2:
        EftGlow_Kill(objId);
        break;
    }
    return 1;
}

/* 1 while the fighter has an active glow. */
s32 EftGlow_IsActive(s32 objId) {
    s32 ret = 1;
    EftQTask *task;

    if (gEftGlow == NULL) {
        return 0;
    }
    task = gEftGlow->tasks[objId];
    if (task != NULL) {
        if (!(((EftGlow *)task->work)->flags & EFT_GLOW_ACTIVE)) {
            ret = 0;
        }
    } else {
        return 0;
    }
    return ret;
}

/* Changes the type of a running glow. */
void EftGlow_ChangeType(s32 objId, s32 type, s32 paramFlags) {
    if (gEftGlow != NULL && (u32)objId < (u32)gEftGlow->charCount) {
        if (gEftGlow == NULL) {
            return;
        }
        if (type < 0) {
            return;
        }
        if (gEftGlow->tasks[objId] == NULL) {
            return;
        }
        EftGlow_SetType(gEftGlow->tasks[objId]->work, objId, type, paramFlags);
    }
}

/* The colour row of an aura type. No caller. */
Vec4 *EftGlow_GetColors(s32 type, s32 paramFlags) {
    if (gEftGlow == NULL) {
        return NULL;
    }
    if (type < 0) {
        return NULL;
    }
    if (!(paramFlags & 0x5E)) {
        if (type >= 9) {
            return NULL;
        }
        return &gEftGlowCfg->color[type * EFT_GLOW_ROW];
    }
    if (type >= 5) {
        return NULL;
    }
    return &gEftGlowCfg->colorTex[type * EFT_GLOW_ROW];
}

/* ---- 2. rush-finish burst: fighter effect requests 0x28 (start), 0x29 (second stage), 0x2A (end) --------- */

extern void EftEmit_LoadSet(void *arg, EftQEmitSet *set, s32 a2, s32 *pack, s32 a4, s32 a5);
extern void EftEmit_FreeSet(EftQEmitSet *set);
extern void EftEmit_BeginFrame(EftQEmitSet *set);
extern s32 EftEmit_GetEndFrames(EftQEmitSet *set);
extern void EftEmit_InitState(EftQEmitSet *set, EftQEmitState *state);
extern void EftEmit_TermState(EftQEmitSet *set, EftQEmitState *state);
extern s32 EftEmit_GetFlagsFromMask(EftQEmitSet *set, EftQEmitState *state, s32 objId, s32 group, s32 sub, s32 end,
                                    s32 fast, s32 mask);
extern void EftEmit_Spawn(EftQEmitSet *set, EftQEmitState *state, void *nodes, Vec4 *pos, Vec4 *dir, s32 objId, s32 node,
                          s32 arg7, s32 group, s32 sub, s32 flags, f32 scale);
extern void EftEmit_KillAll(EftQEmitSet *set, EftQEmitState *state);
extern s32 EftEmit_UpdateAlive(EftQEmitSet *set, EftQEmitState *state);
extern void EftEmit_SetNode(void *nodes, s32 slot, s32 node, Vec4 *pos);
extern void EftEmit_RefreshFixedNodes(s32 objId, void *nodes);

extern void EftCharSlot_Set2(s32 objId, EftQTask *task);   /* per-fighter task slot of this effect: set */
extern void EftCharSlot_Clear2(s32 objId);                   /* clear */
extern EftQTask *EftCharSlot_Get2(s32 objId);              /* get */

extern u8 gEftRushBurstClass[0x18]; /* 0x2C3B98 */

void EftChar_SetList(s32 objId, s32 kind, void *list);
void *EftChar_GetList(s32 objId, s32 kind);

/* Steps the emitters of the set and spawns their particles at the node, along the direction. */
void EftRushBurst_Emit(s32 objId, EftQTask *task, EftQEmitSet *set) {
    EftRushBurst *w = task->work;
    s32 group;
    s32 sub;

    for (group = 0; group < 0x13; group++) {
        if (*set->mask & (1 << group)) {
            EftQEmitGroupDef *def = set->group[group].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 spawn = EftEmit_GetFlagsFromMask(set, &w->state, objId, group, sub, w->flags & EFT_RUSHBURST_END, 0,
                                                     w->phase);

                if (spawn != 0) {
                    EftEmit_Spawn(set, &w->state, w->nodes, &w->pos, &w->dir, objId, 0, 3, group, sub, spawn, w->scale);
                }
            }
        }
    }
}

/* Init callback: copies the argument, places the burst at the fighter's node and starts the first stage. */
void EftRushBurst_Init(EftQTask *task, EftRushBurstArg *arg) {
    EftQSetMgr *mgr = BtlTask_GetParent(task)->work;
    EftRushBurst *w = task->work;
    Vec4 *dir = &w->dir;
    EftQEmitSet *set = mgr->set;

    memset(w, 0, sizeof(EftRushBurst));
    w->arg = *arg;
    w->flags |= EFT_RUSHBURST_ACTIVE;
    w->scale = arg->scale;
    BtlCharApi_GetNodePos(arg->objId, arg->node, &w->pos);
    Vec4_Copy(dir, (Vec4 *)&arg->dir);
    w->dir.w = 1.0f;
    Vec3_Normalize(dir, dir);
    w->set = set;
    EftEmit_InitState(set, &w->state);
    w->phase |= 1;
    EftEmit_SetNode(w->nodes, 0, arg->node, NULL);
    w->flags |= EFT_RUSHBURST_ALIVE;
    w->life = EftEmit_GetEndFrames(w->set);
    BtlTask_SetOwnerTag(task, BtlScene_CharOwnerTag(arg->objId));
}

/* Term callback: releases the emitter state and the fighter's slot. */
void EftRushBurst_Term(EftQTask *task) {
    EftRushBurst *w = task->work;
    EftRushBurstArg *arg = &w->arg;

    EftEmit_TermState(w->set, &w->state);
    w->flags = 0;
    EftCharSlot_Clear2(arg->objId);
}

/* Update callback: follows the fighter's node, spawns the particles, counts down once the end was asked for. */
void EftRushBurst_Update(EftQTask *task) {
    EftRushBurst *w = task->work;
    EftRushBurstArg *arg = &w->arg;
    void *nodes;

    if (!BtlScene_IsEffectStopped(arg->objId, 3)) {
        if (w->flags & EFT_RUSHBURST_STAGE2) {
            if (!(w->flags & EFT_RUSHBURST_STAGE2D)) {
                w->phase |= 2;
                nodes = w->nodes;
                EftEmit_SetNode(nodes, 1, arg->node, NULL);
                w->flags |= EFT_RUSHBURST_STAGE2D;
            } else {
                nodes = w->nodes;
            }
        } else {
            nodes = w->nodes;
        }
        BtlCharApi_GetNodePos(arg->objId, arg->node, &w->pos);
        EftEmit_RefreshFixedNodes(arg->objId, nodes);
        EftRushBurst_Emit(arg->objId, task, w->set);
        if (w->flags & EFT_RUSHBURST_END) {
            w->timer += 1.0f;
            if (w->timer >= w->life) {
                w->flags |= EFT_RUSHBURST_KILL;
            }
        }
    }
    if (w->flags & EFT_RUSHBURST_KILL) {
        BtlTask_SetDead(task);
    }
}

/* Reset callback: destroys the particles once and kills the task. */
void EftRushBurst_Reset(EftQTask *task) {
    EftRushBurst *w = task->work;

    if (!(w->flags & EFT_RUSHBURST_RESET)) {
        w->flags |= EFT_RUSHBURST_RESET;
        EftEmit_KillAll(w->set, &w->state);
    }
    BtlTask_SetDead(task);
}

/* Post-update callback (from the second frame on): records whether a particle is alive, clears the phase mask. */
void EftRushBurst_PostUpdate(EftQTask *task) {
    EftRushBurst *w = task->work;

    if (w->flags & EFT_RUSHBURST_POSTED) {
        if (!EftEmit_UpdateAlive(w->set, &w->state)) {
            w->flags &= ~EFT_RUSHBURST_ALIVE;
        } else {
            w->flags |= EFT_RUSHBURST_ALIVE;
        }
        w->phase = 0;
    }
    w->flags |= EFT_RUSHBURST_POSTED;
}

/* Draw callback: nothing. */
void EftRushBurst_Draw(EftQTask *task) {
}

/* Manager init (per-character kind 5): loads the emitter set from entry 7 of the character's pack, makes room
 * for four bursts and registers the list. arg = {character, kind}. */
void EftRushBurstMgr_Init(EftQTask *task, s32 *arg) {
    EftQSetMgr *mgr = task->work;
    EftQEmitSet *set;
    s32 *pack;
    void *list;

    mgr->set = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftQEmitSet));
    memset(mgr->set, 0, sizeof(EftQEmitSet));
    set = mgr->set;
    pack = BtlScene_GetCharPackEntry(arg[0], 7);
    set->pack = pack;
    if (pack != NULL) {
        EftEmit_LoadSet(NULL, set, 0, pack, 2, 1);
    }
    list = BtlTask_CreateChildList(task, 4, sizeof(EftRushBurst));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term: releases the set and its memory. */
void EftRushBurstMgr_Term(EftQTask *task) {
    EftQSetMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_FreeSet(mgr->set);
    }
    if (mgr->set != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->set);
    }
}

/* Manager update: steps the set. */
void EftRushBurstMgr_Update(EftQTask *task) {
    EftQSetMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_BeginFrame(mgr->set);
    }
}

/* Fighter effect request 0x28: starts the burst and records its task in the fighter's slot. */
void EftRushBurst_Start(EftRushBurstArg *arg) {
    EftRushBurstArg init;
    void *list;
    EftQTask *task;

    init = *arg;
    list = EftChar_GetList(init.objId, 5);
    if (list != NULL) {
        task = BtlTaskList_AddTail(list, gEftRushBurstClass, &init);
        if (task != NULL) {
            EftCharSlot_Set2(init.objId, task);
        }
    }
}

/* Fighter effect request 0x29: asks the fighter's burst for its second stage. Returns 1 when one is running. */
s32 EftRushBurst_Stage2(s32 objId) {
    EftQTask *task = EftCharSlot_Get2(objId);
    EftRushBurst *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RUSHBURST_ACTIVE)) {
        return 0;
    }
    w->flags |= EFT_RUSHBURST_STAGE2;
    return 1;
}

/* Fighter effect request 0x2A: asks the fighter's burst to end. Returns 1 when one is running. */
s32 EftRushBurst_Stop(s32 objId) {
    EftQTask *task = EftCharSlot_Get2(objId);
    EftRushBurst *w;

    if (task == NULL) {
        return 0;
    }
    w = task->work;
    if (!(w->flags & EFT_RUSHBURST_ACTIVE)) {
        return 0;
    }
    w->flags |= EFT_RUSHBURST_END;
    return 1;
}

/* Nothing. No caller. */
void EftRushBurst_Nop(void) {
}

/* ---- 3. per-character effect root = scene layer 2 (btl_scene.c creates it from class 0x2C3BB0) ----------- */

extern EftCharRoot *gEftChar;        /* 0x2FEA48 */
extern u8 gEftCharClass[0x18];       /* 0x2C3BC8 */
extern void *gEftCharKindClass[11];  /* 0x2C3BE0: manager class of each kind */
extern u8 gEftKiBombMgrClass[0x18];          /* manager class used for kind 2 when the fighter has a type 3 ki blast */
extern u8 D_002C3E00[0x18];          /* manager class used for kind 1 when the fighter has a type 2 ki blast */

void EftChar_SetPool(s32 chr);
void EftChar_ResetPool(s32 chr);
void EftChar_CreateKind(s32 chr, s32 kind, s32 mode);

/* Root init: selects pool 6, makes the root's work and one task per character. */
void EftCharRoot_Init(EftQTask *task) {
    s32 i;
    s32 size;

    BtlPool_SetCurrent(6);
    gEftChar = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftCharRoot));
    memset(gEftChar, 0, sizeof(EftCharRoot));
    gEftChar->count = BtlScene_GetCharCount();
    size = gEftChar->count * sizeof(EftCharEntry);
    gEftChar->chars = BtlPool_Alloc(BtlPool_GetCurrent(), size);
    memset(gEftChar->chars, 0, size);
    gEftChar->list = BtlTask_CreateChildList(task, gEftChar->count, 0);
    for (i = 0; i < gEftChar->count; i++) {
        EftCharEntry *c = &gEftChar->chars[i];

        c->task = BtlTaskList_AddTail(gEftChar->list, gEftCharClass, (void *)i);
    }
}

/* Root term: frees its work and empties pools 6, 7 and 8. */
void EftCharRoot_Term(EftQTask *task) {
    BtlPool_SetCurrent(6);
    BtlPool_Free(BtlPool_GetCurrent(), gEftChar->chars);
    BtlPool_Free(BtlPool_GetCurrent(), gEftChar);
    gEftChar = NULL;
    BtlPool_Reset(6);
    BtlPool_Reset(7);
    BtlPool_Reset(8);
}

/* Root update: nothing. */
void EftCharRoot_Update(EftQTask *task) {
}

/* Character task init: selects the character's pool and creates the manager of every kind the character's pack
 * has. A fighter with a type 2 / type 3 ki blast gets kind 1 / 2 with another class, whatever the pack says. */
void EftChar_Init(EftQTask *task, s32 chr) {
    EftCharEntry *c;
    s32 kind;
    s32 on;
    s32 mode;

    EftChar_SetPool(chr);
    task->step = chr;
    c = &gEftChar->chars[chr];
    c->list = BtlTask_CreateChildList(task, EFT_CHAR_KINDS, 4);
    for (kind = 0; kind < EFT_CHAR_KINDS; kind++) {
        mode = 0;
        if (kind == 3) {
            on = BtlScene_TestCharPackBit(chr, 3);
        } else if (kind == 6) {
            on = 0;
        } else {
            on = BtlScene_TestCharPackBit(chr, kind + 1);
        }
        if (BtlCharApi_HasKiBlastType2(chr) && kind == 1) {
            on = 1;
            mode = 2;
        }
        if (BtlCharApi_HasKiBlastType3(chr) && kind == 2) {
            on = 1;
            mode = 4;
        }
        if (BtlCharApi_HasKiBlastType3(chr) && kind == 3) {
            on = 0;
        }
        if (on) {
            EftChar_CreateKind(chr, kind, mode);
        }
    }
}

/* Character task term: forgets the kind lists and empties the character's pool. */
void EftChar_Term(EftQTask *task) {
    s32 chr = task->step;
    s32 i;

    if (gEftChar != NULL) {
        EftCharEntry *c = &gEftChar->chars[chr];

        for (i = EFT_CHAR_KINDS - 1; i >= 0; i--) {
            c->lists[i] = NULL;
        }
    }
    EftChar_ResetPool(chr);
}

/* Character task update: nothing. */
void EftChar_Update(EftQTask *task) {
}

/* Makes the character's pool (7 for character 0, 8 for the others) the current one. */
void EftChar_SetPool(s32 chr) {
    BtlPool_SetCurrent(chr < 2 ? 7 + chr : 9 + (PORT_FIGHTER_CAPACITY-2) + chr - 2);
}

/* Empties the character's pool. */
void EftChar_ResetPool(s32 chr) {
    BtlPool_Reset(chr < 2 ? 7 + chr : 9 + (PORT_FIGHTER_CAPACITY-2) + chr - 2);
}

/* Creates the manager task of one kind for a character; its init gets {character, kind}. */
void EftChar_CreateKind(s32 chr, s32 kind, s32 mode) {
    s32 arg[2];
    EftCharEntry *c = &gEftChar->chars[chr];
    void *cls = NULL;

    if (mode != 0) {
        if (mode & 2) {
            cls = D_002C3E00;
        }
        if (mode & 4) {
            cls = gEftKiBombMgrClass;
        }
    } else {
        cls = gEftCharKindClass[kind];
    }
    c->lists[kind] = NULL;
    arg[0] = chr;
    arg[1] = kind;
    BtlTaskList_AddTail(c->list, cls, arg);
}

/* Removes a character's effects (BtlScene_FreeCharLayer2, when the character is streamed out): resets the scene
 * tasks of that character and kills its task. */
void EftChar_Kill(s32 chr) {
    EftCharEntry *c = &gEftChar->chars[chr];

    if (c->task != NULL) {
        BtlScene_ResetCharacter(chr);
        EftChar_SetPool(chr);
        BtlTask_Kill(c->task);
        c->task = NULL;
    }
}

/* Creates a character's task again (BtlScene_CreateCharLayer2, when the character is streamed in). */
void EftChar_Create(s32 chr) {
    EftCharEntry *c = &gEftChar->chars[chr];

    if (c->task == NULL) {
        c->task = BtlTaskList_AddTail(gEftChar->list, gEftCharClass, (void *)chr);
    }
}

/* A kind's manager registers the list its tasks go into. */
void EftChar_SetList(s32 chr, s32 kind, void *list) {
    EftCharEntry *c = &gEftChar->chars[chr];

    c->lists[kind] = list;
}

/* The list of one kind of a character, or NULL when the character does not have that kind. */
void *EftChar_GetList(s32 chr, s32 kind) {
    EftCharEntry *c = &gEftChar->chars[chr];

    return c->lists[kind];
}

/* ---- 4. placeholder kind (kinds 6, 8 and 10 of the table) ------------------------------------------------- */

extern u8 gEftCharNullClass[0x18];   /* 0x2C3C28 */

/* Adds a placeholder task to kind 0's list of a character (the task kills itself in its first update). No caller. */
EftQTask *EftCharNull_Start(s32 chr) {
    void *list = EftChar_GetList(chr, 0);

    if (list != NULL) {
        return BtlTaskList_AddTail(list, gEftCharNullClass, (void *)chr);
    }
}

/* Manager init: four bytes of work from the pool, a list for one task. */
void EftCharNullMgr_Init(EftQTask *task, s32 *arg) {
    void **mgr = task->work;
    void *list;

    *mgr = NULL;
    *mgr = BtlPool_Alloc(BtlPool_GetCurrent(), 4);
    memset(*mgr, 0, 4);
    list = BtlTask_CreateChildList(task, 1, 4);
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term. */
void EftCharNullMgr_Term(EftQTask *task) {
    void **mgr = task->work;

    if (*mgr != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), *mgr);
        *mgr = NULL;
    }
}

/* Manager update: nothing. */
void EftCharNullMgr_Update(EftQTask *task) {
}

/* Task init. */
void EftCharNull_Init(EftQTask *task, s32 chr) {
    s32 *w = task->work;

    *w = 0;
    BtlTask_SetOwnerTag(task, BtlScene_CharOwnerTag(chr));
}

/* Task term: nothing. */
void EftCharNull_Term(EftQTask *task) {
}

/* Task reset: kills the task. */
void EftCharNull_Reset(EftQTask *task) {
    BtlTask_SetDead(task);
}

/* Task update: kills the task. */
void EftCharNull_Update(EftQTask *task) {
    BtlTask_SetDead(task);
}

/* Task post-update: nothing. */
void EftCharNull_PostUpdate(EftQTask *task) {
}

/* Task draw: nothing. */
void EftCharNull_Draw(EftQTask *task) {
}

/* ---- 5. full-screen flash --------------------------------------------------------------------------------- */

extern EftFlashMgr *gEftFlash;       /* 0x2FEA4C */
extern u8 gEftFlashClass[0x18];      /* 0x2C3C58 */

void EftFlash_Update(EftQTask *task);
s32 EftFlash_GetRushState(void);

/* Starts a flash that belongs to nobody (the owner in the argument is overwritten). Technique modules call it. */
EftQTask *EftFlash_Start(EftFlashArg *arg) {
    arg->objId = -1;
    return BtlTaskList_AddTail(gEftFlash->list, gEftFlashClass, arg);
}

/* Starts a flash and keeps the owner. No caller. */
EftQTask *EftFlash_StartOwned(EftFlashArg *arg) {
    return BtlTaskList_AddTail(gEftFlash->list, gEftFlashClass, arg);
}

/* The phase of a live flash task (1 in, 2 hold, 4 out), else 0. No caller. */
s32 EftFlash_GetPhase(EftQTask *task) {
    if (task == NULL) {
        return 0;
    }
    if (task->cls[0] != EftFlash_Update) {
        return 0;
    }
    if (EFTQ_TASK_FLAGS(task)->dead) {
        return 0;
    }
    return ((EftFlash *)task->work)->phase;
}

/* Freezes or releases a live flash task. No caller. */
void EftFlash_SetPause(EftQTask *task, s32 pause) {
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftFlash_Update) {
        return;
    }
    if (EFTQ_TASK_FLAGS(task)->dead) {
        return;
    }
    ((EftFlash *)task->work)->pause = pause;
}

/* Manager init. */
void EftFlashMgr_Init(EftQTask *task) {
    gEftFlash = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftFlashMgr));
    memset(gEftFlash, 0, sizeof(EftFlashMgr));
    gEftFlash->list = BtlTask_CreateChildList(task, 1, sizeof(EftFlash));
}

/* Manager term. */
void EftFlashMgr_Term(EftQTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftFlash);
    gEftFlash = NULL;
}

/* Manager update: starts the white flash of a fighter in action 0x104 with object flag 8 (one flash at a time),
 * and while a flash runs keeps the rush-finish state it waits on. */
void EftFlashMgr_Update(EftQTask *task) {
    s32 count = BtlScene_GetCharCount();
    s32 i;

    if (gEftFlash == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        if (BtlCharApi_IsFlag8Action104(i)) {
            EftFlashArg arg = { { 255.0f, 255.0f, 255.0f, 128.0f }, 0.5f, 1.0f, 0.5f, -1, 0 };

            if (!(gEftFlash->flags & 1)) {
                EftFlash_Start(&arg);
                if (i == 0) {
                    gEftFlash->flags |= 2;
                } else {
                    gEftFlash->flags |= 4;
                }
            }
        }
    }
    if (gEftFlash->flags & 1) {
        gEftFlash->rushState = EftFlash_GetRushState();
    }
}

/* Manager reset. */
void EftFlashMgr_Reset(EftQTask *task) {
    if (gEftFlash != NULL) {
        gEftFlash->flags = 0;
    }
}

/* Task init: copies the argument and turns its three times into frames. */
void EftFlash_Init(EftQTask *task, EftFlashArg *arg) {
    EftFlash *w = task->work;

    memset(w, 0, sizeof(EftFlash));
    w->arg = *arg;
    w->arg.in *= 30.0f;
    w->arg.hold *= 30.0f;
    w->arg.out *= 30.0f;
    w->inMax = w->arg.in;
    w->outMax = w->arg.out;
    w->fade = 1.0f;
    w->phase = 1;
    gEftFlash->flags = 1;
    gEftFlash->rushBits = 0;
    gEftFlash->rushState = 0;
}

/* Task term. */
void EftFlash_Term(EftQTask *task) {
    gEftFlash->flags = 0;
}

/* Task reset: kills the task. */
void EftFlash_Reset(EftQTask *task) {
    BtlTask_SetDead(task);
}

/* Task update: fade in, hold, fade out. The hold lasts until its time has run AND, for a flash started by the
 * manager, the fighter's member flag (BtlCharApi_GetMemberUnk60) is set and the rush finish is in state 0 or 2;
 * a waiting flash (arg.wait) ends its hold when the rush finish reaches state 2. 180 frames end it regardless. */
void EftFlash_Update(EftQTask *task) {
    EftFlash *w = task->work;

    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    if (w->pause != 0) {
        return;
    }
    switch (task->step) {
    case 0:
        w->arg.in -= 1.0f;
        w->phase = 1;
        w->fade = w->arg.in / w->inMax;
        if (w->arg.in <= 0.0f) {
            w->arg.in = 0.0f;
            task->step = 1;
        }
        break;
    case 1:
        w->frames++;
        if (w->frames >= 180) {
            task->step = 2;
        }
        if (w->arg.wait != 0) {
            if (gEftFlash->rushState == 2) {
                task->step = 2;
                break;
            }
        } else {
            if (gEftFlash->rushState != 0 && gEftFlash->rushState != 2) {
                break;
            }
            if (gEftFlash->flags & 2) {
                if (!BtlCharApi_GetMemberUnk60(0)) {
                    break;
                }
            }
            if (gEftFlash->flags & 4) {
                if (!BtlCharApi_GetMemberUnk60(1)) {
                    break;
                }
            }
        }
        w->arg.hold -= 1.0f;
        w->phase = 2;
        if (w->arg.hold <= 0.0f) {
            w->arg.hold = 0.0f;
            task->step = 2;
        }
        break;
    case 2:
        w->arg.out -= 1.0f;
        w->phase = 4;
        w->fade = 1.0f - w->arg.out / w->outMax;
        if (w->arg.out <= 0.0f) {
            w->arg.out = 0.0f;
            BtlTask_SetDead(task);
        }
        break;
    }
}

/* Task draw: one sprite over the whole screen, queued in the slot drawn after every depth slot. The colour is the
 * argument's colour blended towards transparent white by `fade`. */
void EftFlash_Draw(EftQTask *task) {
    EftFlash *w = task->work;
    EftQVec col;
    EftQVec clear = { 255.0f, 255.0f, 255.0f, 0.0f };
    EftQSpritePkt *p;
    OtEntry *e;

    Vec4_Lerp((Vec4 *)&col, (Vec4 *)&clear, (Vec4 *)&w->arg.color, w->fade);
    p = (EftQSpritePkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    p->dmaTag = 0x20000003;
    p->vif1 = 0x50000003;
    p->prim = 0x46;
    p->vif0 = 0x10000000;
    p->gifTag = 0x4400000000008001;
    p->regs = 0x4410;
    p->next = NULL;
    p->rgbaq.r = col.x;
    p->rgbaq.g = col.y;
    p->rgbaq.b = col.z;
    p->rgbaq.a = col.w;
    p->xyz0.x = 0x7000;
    p->xyz0.y = 0x7200;
    p->xyz1.x = -0x7000;
    p->xyz1.y = -0x7200;
    p->xyz0.z = 0xFFFFFF;
    p->xyz1.z = 0xFFFFFF;
    p->rgbaq.q = 1.0f;
    p->xyz0.f = 0xFF;
    p->xyz1.f = 0xFF;
    e = &gOtLast->layer[1];
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
}

/* What the rush finish is doing, for the flash that waits on it: 0 nothing, 1 a fighter is in rush-finish phase 1,
 * 2 that fighter has flag 0x2B (when both fighters are in phase 1: both have it). Also keeps gEftFlash->rushBits. */
s32 EftFlash_GetRushState(void) {
    s32 i = 0;
    s32 ret = 0;
    s32 count = BtlScene_GetCharCount();

    for (; i < count; i++) {
        if (BtlCharApi_IsInRushSequence(i)) {
            s32 phase = BtlCharApi_GetRushFinishPhase(i);

            /* The original tests 1, 0, 2, -1 in this order with the bodies behind the tests; neither a switch
               (the compiler would start with 0) nor an if / else chain (bodies in line) gives that. */
            if (phase == 1) {
                goto phase1;
            }
            if (phase == 0) {
                continue;
            }
            if (phase == 2) {
                goto phase2;
            }
            if (phase == -1) {
                goto phaseNone;
            }
            continue;
        phase1:
            if (i == 0) {
                gEftFlash->rushBits |= 1;
            } else {
                gEftFlash->rushBits |= 2;
            }
            continue;
        phase2:
            if (i == 0) {
                gEftFlash->rushBits |= 4;
            } else {
                gEftFlash->rushBits |= 8;
            }
            continue;
        phaseNone:
            gEftFlash->rushBits = 0;
        } else {
            gEftFlash->rushBits = 0;
        }
    }
    if (gEftFlash->rushBits == 0) {
        ret = 0;
    } else if ((gEftFlash->rushBits & 3) == 1) {
        ret = 1;
        if (BtlCharApi_TestFlag2B(0)) {
            ret = 2;
        }
    } else if ((gEftFlash->rushBits & 3) == 2) {
        ret = 1;
        if (BtlCharApi_TestFlag2B(1)) {
            ret = 2;
        }
    } else if ((gEftFlash->rushBits & 3) == 3) {
        ret = 1;
        if (BtlCharApi_TestFlag2B(0)) {
            gEftFlash->rushBits |= 4;
        }
        if (BtlCharApi_TestFlag2B(1)) {
            gEftFlash->rushBits |= 8;
        }
        if ((gEftFlash->rushBits & 0xC) == 0xC) {
            ret = 2;
        }
    }
    return ret;
}

/* ---- 6. trails (no caller starts one) ------------------------------------------------------------------------ */

extern EftTrailMgr *gEftTrail;     /* 0x2FEA50 */
extern u8 gEftTrailClass[0x18];     /* 0x2C3C88 */
extern Vec4 gEftTrailOrigin;        /* 0x2C3CA0: (0, 0, 0, 1) */

void EftTrail_Update(EftQTask *task);
void EftTrail_UpdateTextures(EftTrail *w);
void EftTrail_SetTextures(EftTrail *w, EftQTex *src);
void EftTrail_DrawSprite(u8 r, u8 g, u8 b, u8 a, f32 x, f32 y, f32 z, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 scale,
                         f32 rot, s32 layer, u64 tex0);

/* Starts a trail at pos that is pulled towards *target, using the manager's own texture table. */
void EftTrail_Start(Vec4 *pos, Vec4 *dir, s32 life, Vec4 *target, f32 speed, f32 accel, f32 turn, f32 turnAccel) {
    EftTrail init;
    s32 i;

    memset(&init, 0, sizeof(EftTrail));
    init.count = EFT_TRAIL_POINTS;
    if (Battle_IsSplitScreen()) {
        init.count = 15;
    }
    Vec4_Copy(&init.pos, pos);
    for (i = 0; i < init.count; i++) {
        Vec4_Copy(&init.pts[i], pos);
    }
    Vec4_Copy(&init.dir, dir);
    Vec3_Normalize(&init.dir, &init.dir);
    init.speed = speed;
    init.accel = accel;
    init.turn = turn;
    init.turnAccel = turnAccel;
    init.life = life;
    init.target = target;
    init.texCount = 4;
    init.tbl = &gEftTrail->tbl;
    init.texFirst = 0;
    BtlTaskList_AddTail(gEftTrail->list, gEftTrailClass, &init);
}

/* Starts an endless trail owned by an object, with the caller's textures and colour. */
void EftTrail_StartOwned(u8 kind, s32 objId, Vec4 *pos, Vec4 *color, EftTrailTexTbl *tbl, f32 width) {
    EftTrail init;
    Vec4 dir;
    s32 i;

    memset(&dir, 0, sizeof(Vec4));
    dir.w = 1.0f;
    memset(&init, 0, sizeof(EftTrail));
    init.objId = objId;
    init.kind = kind;
    init.count = EFT_TRAIL_POINTS;
    if (Battle_IsSplitScreen()) {
        init.count = 8;
    }
    Vec4_Copy(&init.pos, pos);
    for (i = 0; i < init.count; i++) {
        Vec4_Copy(&init.pts[i], pos);
    }
    Vec4_Copy(&init.dir, &dir);
    Vec3_Normalize(&init.dir, &init.dir);
    Vec4_Copy(&init.color, color);
    init.speed = 1.0f;
    init.accel = 1.0f;
    init.turn = 0.2f;
    init.turnAccel = 0.01f;
    init.width = width;
    init.width0 = width;
    init.life = -1;
    init.tbl = tbl;
    init.texFirst = 0;
    init.texCount = 4;
    init.target = &gEftTrailOrigin;
    EftTrail_SetTextures(&init, tbl->tex);
    BtlTaskList_AddTail(gEftTrail->list, gEftTrailClass, &init);
}

/* Starts the fade-out of a trail over the given number of frames. */
void EftTrail_End(EftQTask *task, s32 frames) {
    EftTrail *w;

    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftTrail_Update) {
        return;
    }
    w = task->work;
    w->fadeFrames = frames;
    w->flags |= 1;
}

/* Sets a trail's colour. */
void EftTrail_SetColor(EftQTask *task, Vec4 *color) {
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftTrail_Update) {
        return;
    }
    Vec4_Copy(&((EftTrail *)task->work)->color, color);
}

/* Reads a trail's head position. */
void EftTrail_GetPos(EftQTask *task, Vec4 *out) {
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftTrail_Update) {
        return;
    }
    Vec4_Copy(out, &((EftTrail *)task->work)->pos);
}

/* Moves a trail's head. */
void EftTrail_SetPos(EftQTask *task, Vec4 *pos) {
    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftTrail_Update) {
        return;
    }
    Vec4_Copy(&((EftTrail *)task->work)->pos, pos);
}

/* Sets a trail's width. */
void EftTrail_SetWidth(EftQTask *task, f32 width) {
    EftTrail *w;

    if (task == NULL) {
        return;
    }
    if (task->cls[0] != EftTrail_Update) {
        return;
    }
    w = task->work;
    w->width = width;
    w->width0 = width;
}

/* Manager init: room for 15 trails. */
void EftTrailMgr_Init(EftQTask *task) {
    gEftTrail = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftTrailMgr));
    memset(gEftTrail, 0, sizeof(EftTrailMgr));
    gEftTrail->list = BtlTask_CreateChildList(task, 15, sizeof(EftTrail));
}

/* Manager term. */
void EftTrailMgr_Term(EftQTask *task) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftTrail);
    gEftTrail = NULL;
}

/* Manager update: the shared texture words have to be renewed this frame. */
void EftTrailMgr_Update(EftQTask *task) {
    gEftTrail->tbl.loaded = 0;
}

/* Task init: the argument is the whole work. */
void EftTrail_Init(EftQTask *task, EftTrail *arg) {
    EftTrail *w = task->work;

    *w = *arg;
}

/* Task term: nothing. */
void EftTrail_Term(EftQTask *task) {
}

/* Task reset: kills the task. */
void EftTrail_Reset(EftQTask *task) {
    BtlTask_SetDead(task);
}

/* Task update: shifts the point history and adds the head position, turns the direction towards the target, runs
 * the fade and the life. The direction and speed are computed but never move the head (EftTrail_SetPos does). */
void EftTrail_Update(EftQTask *task) {
    EftTrail *w = task->work;
    Vec4 v;
    s32 i;

    if (!BtlScene_IsEffectStopped(w->objId, w->kind)) {
        if (!(w->flags & 1)) {
            Vec4 *pos;
            Vec4 *dir;

            for (i = w->count - 1; i > 0; i--) {
                Vec4_Copy(&w->pts[i], &w->pts[i - 1]);
            }
            pos = &w->pos;
            Vec3_Sub(&v, w->target, pos);
            dir = &w->dir;
            Vec3_Normalize(&v, &v);
            Vec3_Scale(&v, &v, w->turn);
            Vec3_Add(dir, dir, &v);
            Vec3_Normalize(dir, dir);
            Vec4_Scale(&v, dir, w->speed);
            Vec4_Copy(&w->pts[0], pos);
        }
        w->speed += w->accel;
        if (w->speed > 100.0f) {
            w->speed = 100.0f;
        }
        w->turn += w->turnAccel;
        if (w->flags & 1) {
            w->width -= w->width0 / (f32)w->fadeFrames;
            if (w->width <= 0.1f) {
                BtlTask_SetDead(task);
            }
        }
        if (w->life >= 0) {
            if (--w->life <= 0) {
                BtlTask_SetDead(task);
            }
        }
    }
    EftTrail_UpdateTextures(w);
}

/* Task draw: the point history as a camera-facing strip (two triangles per segment; the head, middle and tail
 * segments have their own textures). Where the strip bends sharply (|cos| of the angle between the side vectors of
 * two segments below 0.82) and the points are more than 0.8 apart, three glow sprites cover the joint and the
 * segment starts transparent.
 *
 * Matched in cleanup W1. What it took:
 *   - the next point is `p + 1` / `p[1]` everywhere, not a variable;
 *   - the three glow sprites are plain calls (no inline wrapper: the scale `w->width` is loaded during the
 *     argument set-up, behind the store of the stack argument in front of it), each preceded by `sz = 9.45943f;`:
 *     a variable assigned three times is not a loop invariant, so every call loads its own copy of the constant
 *     (three .lit4 words) while the 0.0 / 1.0 literals are hoisted and shared;
 *   - the -7 added to the glow colour is an int variable that gets no register: reload replaces it by its
 *     constant and the int-to-float conversion reads it from a constant-pool word in .sdata (0x2FEA54, the former
 *     "gEftTrailGlowBias"; the same thing as the 50 in EftSmoke_Draw, eft_g.c);
 *   - declaration order `i`, `first`, `limit`, the bias, `w`, `width` (spill slots of i / first, and the order of
 *     the entry block). */
void EftTrail_Draw(EftQTask *task) {
    s32 i;
    s32 first = 1;
    f32 limit = 0.82f;
    s32 ibias = -7;
    EftTrail *w = task->work;
    f32 width = w->width;
    EftQVert v[9];
    Vec4 side;
    Vec4 prevSide;
    Vec4 mid;
    Vec4 color;
    Vec4 camPos;
    Vec4 toCam;
    Vec4 quad[4];
    Vec4 prev[4];
    EftQScr scr[4];
    EftQVec uv[4] = { { 1.0f, 0.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f },
                      { 0.0f, 1.0f, 1.0f, 1.0f } };
    Vec4 *p;
    s32 j;
    s32 seg;
    s32 z;

    if (BtlScene_IsEffectHidden(w->objId, w->kind)) {
        return;
    }
    Vu0Cur_Push();
    p = &w->pts[0];
    i = 0;
    Vu0Cur_LoadMtx(&gBtlCamView->world2screen);
    Vec4_Copy(&camPos, &gBtlCamView->pos);
    for (; i < w->count - 1; i++) {
        seg = 1;
        if (i == 0) {
            seg = 2;
        }
        if (i == w->count - 2) {
            seg = 0;
        }
        Vec4_Copy(&color, &w->color);
        Vec4_Sub(&side, p + 1, p);
        Vec4_Sub(&toCam, p, &camPos);
        Vec3_Cross(&side, &side, &toCam);
        Vec3_Normalize(&side, &side);
        if (first) {
            Vec3_ScaleAdd(&quad[0], &side, width, p);
            Vec3_ScaleAdd(&quad[1], &side, -width, p);
            first = 0;
        } else {
            if (Vec3_Dot(&side, &prevSide) < 0.0f) {
                Vec3_Scale(&side, &side, -1.0f);
            }
            if (__builtin_fabsf(Vec3_Dot(&side, &prevSide)) < limit) {
                color.w = 0.0f;
                if (w->flags & 1) {
                    color.w = w->color.w;
                }
                if (Vec3_Dist(p, p + 1) > 0.8f) {
                    f32 bias = ibias;
                    f32 sz;

                    mid.x = (p->x + p[1].x) * 0.5f;
                    mid.y = (p->y + p[1].y) * 0.5f;
                    mid.z = (p->z + p[1].z) * 0.5f;
                    sz = 9.45943f;
                    EftTrail_DrawSprite(w->color.x + bias, w->color.y + bias, w->color.z + bias, w->color.w * 0.8f, p->x, p->y, p->z, sz, sz, 0.0f, 0.0f, 1.0f, 1.0f, w->width, 0.0f, 1, w->tex0[3]);
                    sz = 9.45943f;
                    EftTrail_DrawSprite(w->color.x + bias, w->color.y + bias, w->color.z + bias, w->color.w * 0.8f, mid.x, mid.y, mid.z, sz, sz, 0.0f, 0.0f, 1.0f, 1.0f, w->width, 0.0f, 1, w->tex0[3]);
                    sz = 9.45943f;
                    EftTrail_DrawSprite(w->color.x + bias, w->color.y + bias, w->color.z + bias, w->color.w * 0.8f, p[1].x, p[1].y, p[1].z, sz, sz, 0.0f, 0.0f, 1.0f, 1.0f, w->width, 0.0f, 1, w->tex0[3]);
                }
            }
            Vec4_Copy(&quad[0], &prev[0]);
            Vec4_Copy(&quad[1], &prev[1]);
        }
        Vec3_ScaleAdd(&quad[2], &side, width, p + 1);
        Vec3_ScaleAdd(&quad[3], &side, -width, p + 1);
        Vec4_Copy(&prevSide, &side);
        Vec4_Copy(&prev[0], &quad[2]);
        Vec4_Copy(&prev[1], &quad[3]);
        quad[3].w = 1.0f;
        quad[2].w = 1.0f;
        quad[1].w = 1.0f;
        quad[0].w = 1.0f;
        Vu0Cur_ProjectPoints(scr, quad, 4);
        z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
        for (j = 0; j < 2; j++) {
            ClipVtx_Set(&v[0], &quad[j], (Vec4 *)&uv[j], &color);
            ClipVtx_Set(&v[1], &quad[j + 1], (Vec4 *)&uv[j + 1], &color);
            ClipVtx_Set(&v[2], &quad[j + 2], (Vec4 *)&uv[j + 2], &color);
            EftGfx_DrawPolyAvgZ(v, 1, 0, 0, 0, w->tex0[seg], z);
        }
        p++;
    }
    Vu0Cur_Pop();
}

/* Fetches this frame's GS texture words: the first trail that uses a part of the shared table renews it, the
 * others copy from the table. */
void EftTrail_UpdateTextures(EftTrail *w) {
    s32 i;

    if (!(w->tbl->loaded & (1 << w->texFirst))) {
        for (i = 0; i < w->texCount; i++) {
            w->tex0[i] = EftVram_AddTex(&w->tex[i], 1, 0);
            w->tbl->tex[w->texFirst + i].tex0 = w->tex0[i];
        }
        w->tbl->loaded |= 1 << w->texFirst;
    } else {
        for (i = 0; i < w->texCount; i++) {
            w->tex0[i] = w->tbl->tex[w->texFirst + i].tex0;
        }
    }
}

/* Copies four texture entries into a trail. */
void EftTrail_SetTextures(EftTrail *w, EftQTex *src) {
    s32 i;

    for (i = 0; i < 4; i++) {
        w->tex[i] = src[i];
    }
}

/* Queues a camera-facing textured quad at (x, y, z): half sizes w x h in sixteenths of a unit times scale, turned
 * by rot about the view direction. Sorted by depth; layer 2 / 3 select layer 0 / 1 of the ordering table. */
void EftTrail_DrawSprite(u8 r, u8 g, u8 b, u8 a, f32 x, f32 y, f32 z, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 scale,
                         f32 rot, s32 layer, u64 tex0) {
    EftQScr scr[4];
    Vec4 c[4];
    Vec4 pos;
    Vec4 dir;
    Mtx44 m;
    EftQQuadPkt *p;
    OtEntry *e;
    s32 i;
    s32 l;
    s32 oz;
    f32 hh;
    f32 ww;

    if (rot > 3.14159265f) {
        rot -= 6.2831853f;
    } else if (rot < -3.14159265f) {
        rot += 6.2831853f;
    }
    ww = w * scale;
    hh = h * scale;
    ww *= 0.0625f;
    hh *= 0.0625f;
    Vec4_Set(&pos, x, y, z, 1.0f);
    Vec4_Sub(&dir, &gBtlCamView->pos, &pos);
    Vec3_Normalize(&dir, &dir);
    EftMath_MtxFromDir(&m, &dir, rot);
    Vec4_Set(&c[0], -ww, -hh, 0.0f, 1.0f);
    Vec4_Set(&c[1], ww, -hh, 0.0f, 1.0f);
    Vec4_Set(&c[2], -ww, hh, 0.0f, 1.0f);
    Vec4_Set(&c[3], ww, hh, 0.0f, 1.0f);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4(&c[i], &m, &c[i]);
        Vec3_Add(&c[i], &c[i], &pos);
        c[i].w = 1.0f;
    }
    Mtx_ProjectPoints(scr, &gBtlCamView->world2screen, c, 4);
    if (IVec4_InGsRange4(&scr[0], &scr[1], &scr[2], &scr[3])) {
        return;
    }
    p = (EftQQuadPkt *)gOtCur;
    gOtCur = (u32 *)(p + 1);
    p->prim = 0x54;
    p->dmaTag = 0x20000007;
    p->vif0 = 0x10000000;
    p->vif1 = 0x50000007;
    p->gifTag = 0xC400000000008001;
    p->regs = 0xF42424242160;
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    p->rgbaq.r = r;
    p->rgbaq.g = g;
    p->rgbaq.b = b;
    p->rgbaq.a = a;
    p->rgbaq.q = 1.0f;
    p->tex0 = tex0;
    p->next = NULL;
    p->st0.s = u0;
    p->st0.t = v0;
    p->st1.s = u1;
    p->st1.t = v0;
    p->st2.s = u0;
    p->st2.t = v1;
    p->st3.s = u1;
    p->st3.t = v1;
    p->xyz0.x = scr[0].x;
    p->xyz0.y = scr[0].y;
    p->xyz0.z = scr[0].z;
    p->xyz0.f = 0xFF;
    p->xyz1.x = scr[1].x;
    p->xyz1.y = scr[1].y;
    p->xyz1.z = scr[1].z;
    p->xyz1.f = 0xFF;
    p->xyz2.x = scr[2].x;
    p->xyz2.y = scr[2].y;
    p->xyz2.z = scr[2].z;
    p->xyz2.f = 0xFF;
    p->xyz3.x = scr[3].x;
    p->xyz3.y = scr[3].y;
    p->xyz3.z = scr[3].z;
    p->xyz3.f = 0xFF;
    oz = scr[0].z >> 8;
    if (oz < 0) {
        e = &gOtZ[0].layer[l];
    } else if (oz >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[oz].layer[l];
    }
    e->tail->next = (OtPrim *)p;
    e->tail = (OtPrim *)p;
}


/* ---- 7. character body effect: characters 0x37, 0x98 and 0x99 (fighter effect request 0x3A) ---------------- */

extern void EftCharSlot_Set3(s32 objId, EftQTask *task);   /* per-fighter task slot of this effect: set */
extern void EftCharSlot_Clear3(s32 objId);                   /* clear */
extern EftQTask *EftCharSlot_Get3(s32 objId);              /* get */

extern u8 gEftCharaFxClass[0x18];    /* 0x2C3CC8 */

/* Steps the emitters of one state and spawns their particles at pos, along the effect's direction. */
void EftCharaFx_Emit(s32 objId, EftQTask *task, EftQEmitSet *set, EftQEmitState *state, Vec4 *pos) {
    EftCharaFx *w = task->work;
    s32 group;
    s32 sub;

    for (group = 0; group < 0x13; group++) {
        if (*set->mask & (1 << group)) {
            EftQEmitGroupDef *def = set->group[group].def;

            for (sub = 0; sub < def->count; sub++) {
                s32 spawn = EftEmit_GetFlagsFromMask(set, state, objId, group, sub, w->flags & EFT_CHARAFX_END, 0, w->phase);

                if (spawn != 0) {
                    EftEmit_Spawn(set, state, w->nodes, pos, &w->dir, objId, 0, 6, group, sub, spawn, w->scale);
                }
            }
        }
    }
}

/* Init callback: picks the model nodes by character id and places the effect. */
void EftCharaFx_Init(EftQTask *task, EftCharaFxArg *arg) {
    EftQSetMgr *mgr = BtlTask_GetParent(task)->work;
    EftCharaFx *w = task->work;
    Vec4 *dir = &w->dir;
    EftQEmitSet *set = mgr->set;
    Vec4 from;
    s32 chara;

    memset(w, 0, sizeof(EftCharaFx));
    w->arg = *arg;
    w->flags |= EFT_CHARAFX_ACTIVE;
    w->scale = arg->scale;
    chara = BtlCharApi_GetChara(arg->objId);
    if (chara == 0x37) {
        w->nodeA = 0x37;
        w->offset = 1.5f;
        w->node = 0x38;
        w->nodeB = 0x38;
        w->flags |= EFT_CHARAFX_PAIR;
    } else if (chara == 0x98) {
        w->node = 0x74;
        w->nodeA = 0x74;
        w->nodeB = 0x75;
    } else if (chara == 0x99) {
        w->node = 0x3D;
        w->nodeA = 0x3D;
        w->nodeB = 0x3E;
        w->scale += w->scale;
    }
    BtlCharApi_GetNodePos(arg->objId, w->node, &w->pos[0]);
    BtlCharApi_GetNodePos(arg->objId, w->node, &w->pos[1]);
    BtlCharApi_GetNodePos(arg->objId, w->nodeB, &from);
    BtlCharApi_GetNodePos(arg->objId, w->nodeA, dir);
    Vec3_Sub(dir, &from, dir);
    w->dir.w = 1.0f;
    Vec3_Normalize(dir, dir);
    w->set = set;
    EftEmit_InitState(set, &w->state[0]);
    EftEmit_InitState(w->set, &w->state[1]);
    w->phase |= 1;
    EftEmit_SetNode(w->nodes, 0, w->node, NULL);
    w->flags |= EFT_CHARAFX_ALIVE;
    w->life = EftEmit_GetEndFrames(w->set);
    BtlTask_SetOwnerTag(task, BtlScene_CharOwnerTag(arg->objId));
}

/* Term callback: releases both emitter states and the fighter's slot. */
void EftCharaFx_Term(EftQTask *task) {
    EftCharaFx *w = task->work;
    EftCharaFxArg *arg = &w->arg;

    EftEmit_TermState(w->set, &w->state[0]);
    EftEmit_TermState(w->set, &w->state[1]);
    w->flags = 0;
    EftCharSlot_Clear3(arg->objId);
}

/* Update callback: follows the fighter's nodes and spawns the particles; once asked to end it destroys the
 * particles and kills the task in the same update. */
void EftCharaFx_Update(EftQTask *task) {
    EftCharaFx *w = task->work;
    EftCharaFxArg *arg = &w->arg;
    Vec4 from;
    Mtx44 m;
    Vec4 side;
    s32 i;

    if (!BtlScene_IsEffectStopped(arg->objId, 6)) {
        Vec4 *dir;

        BtlCharApi_GetNodePos(arg->objId, w->node, &w->pos[0]);
        dir = &w->dir;
        BtlCharApi_GetNodePos(arg->objId, w->node, &w->pos[1]);
        BtlCharApi_GetNodePos(arg->objId, w->nodeB, &from);
        BtlCharApi_GetNodePos(arg->objId, w->nodeA, dir);
        Vec3_Sub(dir, &from, dir);
        w->dir.w = 1.0f;
        Vec3_Normalize(dir, dir);
        EftEmit_RefreshFixedNodes(arg->objId, w->nodes);
        if (w->flags & EFT_CHARAFX_PAIR) {
            BtlCharApi_GetNodeMtx(arg->objId, w->node, &m);
            Vec4_Set(&side, m.m[0][0], m.m[0][1], m.m[0][2], 1.0f);
            Vec3_Normalize(&side, &side);
            Vec3_Scale(&side, &side, w->offset);
            for (i = 0; i < 2; i++) {
                if (i == 0) {
                    Vec3_Add(&w->pos[0], &w->pos[0], &side);
                } else {
                    Vec3_Sub(&w->pos[i], &w->pos[i], &side);
                }
            }
        }
        for (i = 0; i < 2; i++) {
            if ((w->flags & EFT_CHARAFX_PAIR) || i <= 0) {
                EftCharaFx_Emit(arg->objId, task, w->set, &w->state[i], &w->pos[i]);
            }
        }
        if (w->flags & EFT_CHARAFX_END) {
            w->flags |= EFT_CHARAFX_KILL;
            if (!(w->flags & EFT_CHARAFX_KILLED)) {
                w->flags |= EFT_CHARAFX_KILLED;
                EftEmit_KillAll(w->set, &w->state[0]);
                EftEmit_KillAll(w->set, &w->state[1]);
            }
        }
    }
    if (w->flags & EFT_CHARAFX_KILL) {
        BtlTask_SetDead(task);
    }
}

/* Reset callback: destroys the particles once and kills the task. */
void EftCharaFx_Reset(EftQTask *task) {
    EftCharaFx *w = task->work;

    if (!(w->flags & EFT_CHARAFX_KILLED)) {
        w->flags |= EFT_CHARAFX_KILLED;
        EftEmit_KillAll(w->set, &w->state[0]);
        EftEmit_KillAll(w->set, &w->state[1]);
    }
    BtlTask_SetDead(task);
}

/* Post-update callback (from the second frame on): steps both states' particle lists, clears the phase mask. */
void EftCharaFx_PostUpdate(EftQTask *task) {
    EftCharaFx *w = task->work;

    if (w->flags & EFT_CHARAFX_POSTED) {
        if (!EftEmit_UpdateAlive(w->set, &w->state[0])) {
            w->flags &= ~EFT_CHARAFX_ALIVE;
        } else {
            w->flags |= EFT_CHARAFX_ALIVE;
        }
        EftEmit_UpdateAlive(w->set, &w->state[1]);
        w->phase = 0;
    }
    w->flags |= EFT_CHARAFX_POSTED;
}

/* Draw callback: nothing. */
void EftCharaFx_Draw(EftQTask *task) {
}

/* Manager init (per-character kind 7): loads the emitter set from entry 9 of the character's pack, makes room for
 * four tasks and registers the list. arg = {character, kind}. */
void EftCharaFxMgr_Init(EftQTask *task, s32 *arg) {
    EftQSetMgr *mgr = task->work;
    EftQEmitSet *set;
    s32 *pack;
    void *list;

    mgr->set = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftQEmitSet));
    memset(mgr->set, 0, sizeof(EftQEmitSet));
    set = mgr->set;
    pack = BtlScene_GetCharPackEntry(arg[0], 9);
    set->pack = pack;
    if (pack != NULL) {
        EftEmit_LoadSet(NULL, set, 0, pack, 2, 1);
    }
    list = BtlTask_CreateChildList(task, 4, sizeof(EftCharaFx));
    EftChar_SetList(arg[0], arg[1], list);
}

/* Manager term: releases the set and its memory. */
void EftCharaFxMgr_Term(EftQTask *task) {
    EftQSetMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_FreeSet(mgr->set);
    }
    if (mgr->set != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), mgr->set);
    }
}

/* Manager update: steps the set. */
void EftCharaFxMgr_Update(EftQTask *task) {
    EftQSetMgr *mgr = task->work;

    if (mgr->set->pack != NULL) {
        EftEmit_BeginFrame(mgr->set);
    }
}

/* Starts the body effect unless the fighter already has one. Returns 1 when a task was created. */
s32 EftCharaFx_Start(EftCharaFxArg *arg) {
    EftCharaFxArg init;
    void *list;
    EftQTask *task;

    init = *arg;
    if (EftCharSlot_Get3(init.objId) != NULL) {
        return 0;
    }
    list = EftChar_GetList(init.objId, 7);
    if (list == NULL) {
        return 0;
    }
    task = BtlTaskList_AddTail(list, gEftCharaFxClass, &init);
    if (task == NULL) {
        return 0;
    }
    EftCharSlot_Set3(init.objId, task);
    return 1;
}
