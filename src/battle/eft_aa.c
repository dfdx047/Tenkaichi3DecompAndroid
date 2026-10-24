#include "common.h"
#include "battle/eft_aa.h"

/*
 * Effect modules, 0x199F28..0x19E0C0. See include/battle/eft_aa.h for the list.
 * Nothing in this file writes a fighter, a battle object, a hit record or stage state: it reads fighters through the
 * BtlCharApi getters and builds display lists, and one module plays sounds.
 */

typedef struct EftAaBattleWork {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags;     /* 0x100: paused */
} EftAaBattleWork;

/* The part of the camera view (gBtlCamView) read here. */
typedef struct EftAaView {
    /* 0x000 */ u8 unk0[0x40];
    /* 0x040 */ f32 m[4][4];    /* view matrix; column 2 is the view axis */
    /* 0x080 */ u8 unk80[0xC0];
    /* 0x140 */ Mtx44 world2screen;
} EftAaView;

typedef struct EftAaOtEntry {
    struct EftAaOtPrim *head;
    struct EftAaOtPrim *tail;
} EftAaOtEntry;

typedef struct EftAaOtPrim {
    u32 tag;
    void *next;
} EftAaOtPrim;

typedef struct EftAaOtSlot {
    EftAaOtEntry layer[2];
} EftAaOtSlot;

extern void *memset(void *dst, s32 c, u32 n);
extern s32 rand(void);

extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 s);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Dot(Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern void Mtx_StoreIdentity(Mtx44 *m);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *src);
extern f32 Rand_Float01(void);
extern void Vu0Cur_ProjectPointsStq(EftAaScr *xyz, Vec4 *stq, Vec4 *pos, Vec4 *uv, s32 n);            /* projects n points with the current matrix */
extern void Mtx_ProjectPointsStq(EftAaScr *xyz, Vec4 *stq, Mtx44 *m, Vec4 *pos, Vec4 *uv, s32 n);  /* projects n points, perspective STQ */
extern void Vec3_Lerp(Vec4 *out, Vec4 *up, Vec4 *dir, f32 angle);                       /* orientation from an up vector, a direction and a tilt */

extern s32 BtlPool_GetCurrent(void);
extern void *BtlPool_Alloc(s32 slot, s32 size);
extern void BtlPool_Free(s32 slot, void *ptr);
extern void *BtlTask_CreateChildList(EftAaTask *task, s32 count, s32 workSize);
extern EftAaTask *BtlTaskList_AddTail(void *list, void *cls, void *arg);
extern s32 BtlScene_IsTimeStopped(void);
extern s32 BtlScene_GetCharCount(void);
extern s32 BtlScene_IsCharStopped(s32 objId);
extern s32 BtlScene_IsEffectStopped(s32 objId, s32 kind);
extern s32 BtlScene_IsEffectHidden(s32 objId, s32 kind);
extern s32 BtlScene_IsCharInView(s32 objId);
extern s32 BtlScene_IsSingleView(void);
extern s32 *BtlScene_GetCommonEntry(s32 idx);
extern EftAaBattleWork *Battle_GetWork(void);
extern s32 Battle_IsSplitScreen(void);
extern s32 BattleReplay_IsActive(void);
extern u8 *BtlStage_GetLightColors(void);
extern s32 Snd_PlaySeEx(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch);

extern s32 BtlCharApi_GetChara(s32 objId);
extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern void BtlCharApi_GetNodeMtx(s32 objId, s32 node, Mtx44 *out);
extern s32 BtlCharApi_IsHidden(s32 objId);
extern f32 BtlCharApi_GetBlindRatio(s32 objId);
extern s32 BtlCharApi_HasWeaponOut(s32 objId);

extern s32 EftCam_IsActive(void);
extern s32 EftBurst_IsBusy(void);
extern void EftGfx_DrawSprite(Vec4 *pos, Vec4 *color, f32 w, f32 h, f32 u0, f32 v0, f32 u1, f32 v1, f32 rot, s32 layer,
                              s32 front, u64 tex0);
extern EftGroundPiece *EftGndDust_AllocPart(List *active, List *free);       /* takes a cleared piece from the free list */
extern void EftSprAnim_Create(void *obj, void *tex, void *res);            /* animated object: create */
extern void EftSprAnim_Destroy(void *obj);                                  /* free */
extern void EftSprAnim_Step(void *obj);                                  /* step */
extern void EftSprAnim_Draw(void *obj);                                  /* draw */
extern s32 EftSprAnim_IsPlaying(void *obj);                                   /* still playing */
extern void EftSprAnim_SetPos(void *obj, Vec4 *pos);
extern void EftSprAnim_SetDir(void *obj, Vec4 *dir);
extern void EftSprAnim_SetScale(void *obj, Vec4 *scale);
extern void EftSprAnim_SetColor(void *obj, Vec4 *color);
extern void EftSprAnim_SetDrawFlags(void *obj, s32 layer);
extern void EftSprAnim_KeepTextures(void *obj);                                  /* end of frame */
extern void BtlTask_SetDead(EftAaTask *task);                            /* kills the task */
extern u64 EftVram_AddTex(void *entry, s32 a1, s32 a2);                 /* advances a texture, returns TEX0 */
extern void EftTexSet_Load16(u8 *res, s32 *entry);                        /* binds a resource set */

extern EftAaView *gBtlCamView;
extern u8 *gOtCur;
extern EftAaOtSlot *gOtZ;
extern EftAaOtSlot *gOtLast;

extern EftGroundWork *gEftGndDust;
extern EftDelaySeMgr *gEftDelaySe;
extern EftCharSlots *gEftCharSlot;
extern EftBladeMgr *gEftBlade;
extern EftBlind *gEftBlind;
extern void *gEftAnimPartList;
extern EftAaOrbTailMgr *gEftOrbTail;

extern EftBladeDef gEftBladeDef[8];
extern u8 gEftBladeColor[0x20];
extern u8 gEftDelaySeClass[0x18];
extern u8 gEftBladeClass[0x18];
extern u8 gEftAnimPartClass[0x18];
extern Vec4 D_002EC2C0;

/* ---- ground effect pieces (tail of the module that starts in eft_z) ------------------------------------- */

/* Moves a node from one list to the end of another and returns the node that followed it. */
ListNode *EftGndDust_MoveNode(List *from, List *to, ListNode *node) {
    ListNode *next = NULL;

    if (node != NULL) {
        next = List_GetNext(node);
        List_Remove(from, node);
        List_PushBack(to, node);
    }
    return next;
}

/* TEX0 of texture slot idx, fetched once per mask reset. */
u64 EftGndDust_GetTex(EftGroundTex *tex, s32 idx) {
    s32 bit = 1 << idx;

    if (!(tex->loaded & bit)) {
        tex->entry[idx].tex0 = EftVram_AddTex(&tex->entry[idx], 1, 0);
        tex->loaded |= bit;
    }
    return tex->entry[idx].tex0;
}

/* The stage's two light colours for the pieces (grey when the stage has none), alpha raised by 8. */
void EftGndDust_GetLightColors(u8 *colA, u8 *colB) {
    u8 *src;

    if (gEftGndDust != NULL) {
        src = BtlStage_GetLightColors();
        if (src != NULL) {
            colA[0] = src[0];
            colA[1] = src[1];
            colA[2] = src[2];
            colA[3] = src[3];
            colB[0] = src[4];
            colB[1] = src[5];
            colB[2] = src[6];
            colB[3] = src[7];
        } else {
            colA[0] = 0x80;
            colA[1] = 0x80;
            colA[2] = 0x80;
            colA[3] = 0x80;
            colB[0] = 0x80;
            colB[1] = 0x80;
            colB[2] = 0x80;
            colB[3] = 0;
        }
        colA[3] += 8;
        colB[3] += 8;
    }
}

/* Starts one piece from a definition block. Two draws of the VU0 generator: speed spread and spin direction. */
EftGroundPiece *EftGndDust_SpawnPiece(EftGroundWork *w, EftGroundDef *def, s32 flags, f32 size, f32 rot) {
    EftGroundPiece *p = EftGndDust_AllocPart(&w->active, w->free);
    f32 spin;

    if (p != NULL) {
        Vec4_Copy(&p->pos, &def->pos);
        Vec4_Copy(&p->dir, &def->dir);
        Vec4_Set(&p->color, 128.0f, 128.0f, 128.0f, 128.0f);
        Vec3_Scale(&p->accel, &def->grav, def->gravity * def->scale + def->speedRand);
        Vec3_Normalize(&p->dir, &p->dir);
        Vec4_Set(&p->size, size, size, 1.0f, 1.0f);
        Vec4_Set(&p->grow, def->grow, def->grow, 1.0f, 1.0f);
        p->colA = def->colA;
        p->colB = def->colB;
        p->speed = (def->speed + def->speedRand * Rand_Float01()) * def->scale;
        p->drag = def->drag;
        p->lifeMax = p->life = (f32)def->life;
        p->fade = p->fadeMax = (f32)def->fade;
        p->unkB4 = 1.0f;
        p->unkB8 = 1.0f;
        p->rot = rot;
        p->tex = def->tex;
        spin = (f32)def->spin * 0.1f;
        p->spin = spin;
        p->spin = Rand_Float01() > 0.5f ? -spin : spin;
        Vec3_Scale(&p->vel, &p->dir, p->speed);
        p->flags = 2;
        if (flags != 0) {
            p->flags = flags | 2;
        }
        gEftGndDust->count++;
        return p;
    }
    return p;
}


/* Starts one piece from explicit values. One draw of the VU0 generator: spin direction. */
EftGroundPiece *EftGndDust_SpawnPieceEx(EftGroundWork *w, Vec4 *pos, Vec4 *dir, EftGroundRgba *colA, EftGroundRgba *colB,
                                       f32 sizeX, f32 sizeY, f32 growX, f32 growY, f32 speed, f32 drag, f32 rot,
                                       f32 spin, s16 life, s16 fade, s16 tex, f32 unkB8, f32 gravity, s32 flags) {
    EftGroundPiece *p = EftGndDust_AllocPart(&w->active, w->free);

    if (p != NULL) {
        Vec4_Copy(&p->pos, pos);
        Vec4_Copy(&p->dir, dir);
        Vec4_Set(&p->color, 128.0f, 128.0f, 128.0f, 128.0f);
        Vec4_Set(&p->accel, 0.0f, -gravity, 0.0f, 1.0f);
        Vec3_Normalize(&p->dir, &p->dir);
        Vec4_Set(&p->size, sizeX, sizeY, 1.0f, 1.0f);
        Vec4_Set(&p->grow, growX, growY, 1.0f, 1.0f);
        p->colA = *colA;
        p->colB = *colB;
        p->speed = speed;
        p->drag = drag;
        p->life = life;
        p->fadeMax = fade;
        p->lifeMax = life;
        p->fade = fade;
        p->unkB4 = 1.0f;
        p->unkB8 = unkB8;
        p->rot = rot;
        p->tex = tex;
        p->spin = spin;
        p->spin = Rand_Float01() > 0.5f ? -spin : spin;
        Vec3_Scale(&p->vel, &p->dir, p->speed);
        p->flags = 2;
        if (flags != 0) {
            p->flags = flags | 2;
        }
        gEftGndDust->count++;
        return p;
    }
    return p;
}


/* Dust rising from a fighter's body: pieces along ten node-to-node segments, one per quarter body height
   (6 units at least), each flying away from node 3. Five libc rand() per piece, two of them unused. */
#if 0 /* NON-MATCHING: 12 of 347 instructions (was 21), register choices only, in the position sum of the piece loop: the original keeps (f32)j in f0 and start.z in f1 (this C the other way round) and has step.y in f6 and the 0.7f constant in f7 (here f7 / f6). Same instructions, same calls, same stack layout.
   Permuter round 2: the definition's scalars are stored in the order fade, grow, drag, gravity (that fixed the order of the 0.1f / 0.01f / 0.8f loads and of the stores: 9 instructions). Without effect on the rest: every order and operand order of the three position lines, a local for (f32)j, the 0.7f added in a second statement or at the use, the life statement moved behind the position lines (67). */
void EftGndDust_SpawnBodyDust(EftGroundWork *w, EftGroundDef *def, f32 scale) {
    EftGroundSeg seg[10] = {
        { 3, -1, 0 },       { 0xB, 8, 0 },      { 0xF, 0xC, 0 },    { 0x15, -1, 0 },    { 0x23, -1, 0 },
        { 0x12, 0x15, 1 },  { 0x20, 0x23, 1 },  { 8, 0x12, 1 },     { 0xC, 0x20, 1 },   { 0x30, -1, 0 },
    };
    EftAaVec pos;
    EftAaVec start;
    EftAaVec step;
    EftAaVec center;
    EftAaVec dir = { 0.0f, -1.0f, 0.0f, 1.0f };
    EftAaVec delta;
    EftAaVec unkE0;
    EftAaVec unkF0;
    f32 h;
    s32 i;
    s32 j;
    s32 n;

    h = BtlCharApi_GetHeight(def->objId) * 0.25f;
    if (h < 6.0f) {
        h = 6.0f;
    }
    BtlCharApi_GetNodePos(def->objId, 3, (Vec4 *)&center);
    for (i = 0; i < 10; i++) {
        BtlCharApi_GetNodePos(def->objId, seg[i].node, (Vec4 *)&pos);
        Vec4_Copy((Vec4 *)&start, (Vec4 *)&pos);
        if (seg[i].end >= 0) {
            f32 len;

            BtlCharApi_GetNodePos(def->objId, seg[i].end, (Vec4 *)&delta);
            Vec4_Sub((Vec4 *)&delta, (Vec4 *)&delta, (Vec4 *)&pos);
            len = Vec3_Length((Vec4 *)&delta);
            if (h < len) {
                n = len / h + 1.0f;
                Vec3_Scale((Vec4 *)&step, (Vec4 *)&delta, 1.0f / (f32)n);
            } else {
                Vec4_Set((Vec4 *)&step, 0.0f, 0.0f, 0.0f, 1.0f);
                n = 1;
                if (seg[i].skipShort != 0) {
                    n = 0;
                }
            }
        } else {
            Vec4_Set((Vec4 *)&step, 0.0f, 0.0f, 0.0f, 1.0f);
            n = 1;
        }
        for (j = 0; j < n; j++) {
            f32 extra;
            f32 life;

            rand();
            extra = ((f32)rand() / 2147483647.0f * 1.5f + h) * scale;
            rand();
            life = (f32)rand() / 2147483647.0f * 0.5f + 0.7f;
            pos.x = start.x + (f32)j * step.x;
            pos.y = start.y + (f32)j * step.y;
            pos.z = start.z + (f32)j * step.z;
            Vec3_Sub((Vec4 *)&dir, (Vec4 *)&pos, (Vec4 *)&center);
            Vec3_Normalize((Vec4 *)&dir, (Vec4 *)&dir);
            Vec4_Set((Vec4 *)&unkE0, 128.0f, 128.0f, 128.0f, 0.0f);
            Vec4_Set((Vec4 *)&unkF0, 128.0f, 128.0f, 128.0f, 0x80 - rand() % 32);
            dir.w = 1.0f;
            pos.w = 1.0f;
            Vec4_Copy(&def->pos, (Vec4 *)&pos);
            Vec4_Copy(&def->dir, (Vec4 *)&dir);
            Vec4_Set(&def->grav, 0.0f, -1.0f, 0.0f, 1.0f);
            def->life = life * 30.0f;
            def->fade = 5;
            def->grow = 0.1f;
            def->drag = 0.8f;
            def->gravity = 0.01f;
            def->spin = 0;
            def->speed = 1.5f;
            def->speedRand = 0.0f;
            EftGndDust_SpawnPiece(w, def, 0, extra, (f32)rand() / 2147483647.0f * 6.2831853f);
        }
    }
}
#else
INCLUDE_RODATA("asm/nonmatchings/battle/eft_aa", D_002ED360);
INCLUDE_RODATA("asm/nonmatchings/battle/eft_aa", D_002ED3E0);
LIT4_WORD(D_002FCE58, 0x4EFFFFFF); /* 2147483647.0f */
LIT4_WORD(D_002FCE5C, 0x3F333333); /* 0.7f */
LIT4_WORD(D_002FCE60, 0x3DCCCCCC); /* 0.1f */
LIT4_WORD(D_002FCE64, 0x3C23D70A); /* 0.01f */
LIT4_WORD(D_002FCE68, 0x3F4CCCCC); /* 0.8f */
LIT4_WORD(D_002FCE6C, 0x40C90FDA); /* 6.2831853f */
INCLUDE_ASM("asm/nonmatchings/battle/eft_aa", EftGndDust_SpawnBodyDust);
#endif

/* One piece next to an impact point: 4..9 units along the (flattened) direction and 7 below. One libc rand(). */
void EftGndDust_SpawnChip(EftGroundWork *w, Vec4 *pos, Vec4 *dir, EftGroundRgba *colA, EftGroundRgba *colB, s32 life,
                         s32 fade) {
    EftAaVec off;
    EftAaVec p;
    EftAaVec d;
    EftAaVec q = { 0 };
    EftAaVec8 up = { 0.0f, -1.0f, 0.0f, 1.0f };

    q.w = 1.0f;
    Vec3_Normalize((Vec4 *)&d, dir);
    d.w = 1.0f;
    d.y = 0.0f;
    Vec4_Scale((Vec4 *)&off, (Vec4 *)&d, (f32)rand() / 2147483647.0f * 5.0f + 4.0f);
    Vec4_Add((Vec4 *)&p, pos, (Vec4 *)&off);
    p.y += 7.0f;
    Vec3_Lerp((Vec4 *)&q, (Vec4 *)&up, (Vec4 *)&d, 0.5f);
    EftGndDust_SpawnPieceEx(w, (Vec4 *)&p, (Vec4 *)&q, colA, colB, 0.8f, 1.0f, 0.0f, 0.2f, 0.0f, 1.0f, 0.0f, 0.0f,
                           (f32)life, (f32)fade, 10, 1.0f, 0.0f, 0x50);
}

/* Builds the matrix of a quad lying along dir and turned towards the camera, placed at pos. */
void EftGndDust_MakeFacingMtx(Mtx44 *out, Vec4 *dir, Vec4 *pos) {
    EftAaVec v;
    EftAaVec x;
    EftAaVec y;
    EftAaVec z;
    EftAaVec cam;
    f32 d;
    f32 half = 0.0f;

    cam.x = gBtlCamView->m[0][2];
    cam.y = gBtlCamView->m[1][2];
    cam.z = gBtlCamView->m[2][2];
    cam.w = 1.0f;
    d = Vec3_Dot((Vec4 *)&cam, dir);
    if (d < 0.0f) {
        d = -d;
        v.x = -cam.x;
        v.y = -cam.y;
        v.z = -cam.z;
        v.w = 1.0f;
        half = d * 0.5f;
    } else {
        half = d * 0.5f;
        v.x = cam.x;
        v.y = cam.y;
        v.z = cam.z;
        v.w = 1.0f;
    }
    Vec4_Sub((Vec4 *)&v, dir, (Vec4 *)&v);
    Vec3_Normalize((Vec4 *)&v, (Vec4 *)&v);
    Vec4_Scale((Vec4 *)&v, (Vec4 *)&v, half);
    z.x = dir->x + v.x;
    z.y = dir->y + v.y;
    z.z = dir->z + v.z;
    z.w = 1.0f;
    Vec3_Normalize((Vec4 *)&z, (Vec4 *)&z);
    Vec3_Cross((Vec4 *)&x, (Vec4 *)&cam, (Vec4 *)&z);
    Vec3_Normalize((Vec4 *)&x, (Vec4 *)&x);
    Vec3_Cross((Vec4 *)&y, (Vec4 *)&x, (Vec4 *)&z);
    Vec3_Normalize((Vec4 *)&y, (Vec4 *)&y);
    Mtx_StoreIdentity(out);
    out->m[0][0] = x.x;
    out->m[1][0] = y.x;
    out->m[2][0] = z.x;
    out->m[0][1] = x.y;
    out->m[1][1] = y.y;
    out->m[2][1] = z.y;
    out->m[0][2] = x.z;
    out->m[1][2] = y.z;
    out->m[2][2] = z.z;
    out->m[3][0] = pos->x;
    out->m[3][1] = pos->y;
    out->m[3][2] = pos->z;
}

/* 1 when a projected point can be drawn. */
s32 EftGndDust_IsOnScreen(EftAaScr *p) {
    s32 r;

    if (p->z <= 0) {
        return 0;
    }
    if (p->x > 0xFFEF) {
        return 0;
    }
    if (p->x <= 0) {
        return 0;
    }
    r = p->y > 0;
    if (p->y > 0xFFEF) {
        r = 0;
    }
    return r;
}

/* Queues one textured quad in the plane of mtx's X and Z axes: X from -width / 2 to width / 2, Z from z0 to z1,
   all scaled. Nothing is drawn during the stage-change transition, with zero alpha, or when a corner is off screen. */
void EftGndDust_DrawQuad(Vec4 *pos, Vec4 *color, Mtx44 *mtx, u64 tex0, f32 scale, f32 z0, f32 z1, f32 width, f32 u0,
                        f32 v0, f32 u1, f32 v1, u8 layer) {
    EftAaVec c[4];
    EftAaVec stq[4];
    EftAaVec uv[4];
    EftAaScr scr[4];
    EftGroundQuadPkt *p;
    EftAaOtEntry *e;
    f32 hw;
    f32 n;
    f32 f;
    s32 i;
    s32 l;
    s32 z;

    if (EftBurst_IsBusy()) {
        return;
    }
    if (color->w <= 0.0f) {
        return;
    }
    hw = scale * width * 0.5f;
    n = scale * z0;
    f = scale * z1;
    Vec4_Set((Vec4 *)&c[0], -hw, 0.0f, n, 1.0f);
    Vec4_Set((Vec4 *)&c[1], hw, 0.0f, n, 1.0f);
    Vec4_Set((Vec4 *)&c[2], -hw, 0.0f, f, 1.0f);
    Vec4_Set((Vec4 *)&c[3], hw, 0.0f, f, 1.0f);
    Vec4_Set((Vec4 *)&uv[0], u0, v0, 1.0f, 0.0f);
    Vec4_Set((Vec4 *)&uv[1], u1, v0, 1.0f, 0.0f);
    Vec4_Set((Vec4 *)&uv[2], u0, v1, 1.0f, 0.0f);
    Vec4_Set((Vec4 *)&uv[3], u1, v1, 1.0f, 0.0f);
    for (i = 0; i < 4; i++) {
        Mtx_MulVec4((Vec4 *)&c[i], mtx, (Vec4 *)&c[i]);
        Vec4_Add((Vec4 *)&c[i], (Vec4 *)&c[i], pos);
        c[i].w = 1.0f;
    }
    Vu0Cur_ProjectPointsStq(scr, (Vec4 *)stq, (Vec4 *)c, (Vec4 *)uv, 4);
    for (i = 0; i < 4; i++) {
        if (!EftGndDust_IsOnScreen(&scr[i])) {
            return;
        }
    }
    p = (EftGroundQuadPkt *)gOtCur;
    gOtCur = (u8 *)(p + 1);
    if (p == NULL) {
        return;
    }
    p->dmaTag = 0x20000008;
    p->vif1 = 0x50000008;
    p->prim = 0x5C;
    p->vif0 = 0x10000000;
    p->gifTag = 0xE400000000008001;
    p->regs = 0x42142142142160;
    p->next = NULL;
    p->v[0].rgbaq.r = color->x;
    p->v[0].rgbaq.g = color->y;
    p->v[0].rgbaq.b = color->z;
    p->v[0].rgbaq.a = color->w;
    p->v[0].rgbaq.q = stq[0].z;
    p->v[1].rgbaq.r = color->x;
    p->v[1].rgbaq.g = color->y;
    p->v[1].rgbaq.b = color->z;
    p->v[1].rgbaq.a = color->w;
    p->v[1].rgbaq.q = stq[1].z;
    p->v[2].rgbaq.r = color->x;
    p->v[2].rgbaq.g = color->y;
    p->v[2].rgbaq.b = color->z;
    p->v[2].rgbaq.a = color->w;
    p->v[2].rgbaq.q = stq[2].z;
    p->v[3].rgbaq.r = color->x;
    p->v[3].rgbaq.g = color->y;
    p->v[3].rgbaq.b = color->z;
    p->v[3].rgbaq.a = color->w;
    l = layer;
    if (l >= 2) {
        l -= 2;
    }
    p->v[3].rgbaq.q = stq[3].z;
    p->v[0].st.s = stq[0].x;
    p->v[0].st.t = stq[0].y;
    p->v[1].st.s = stq[1].x;
    p->v[1].st.t = stq[1].y;
    p->v[2].st.s = stq[2].x;
    p->v[2].st.t = stq[2].y;
    p->v[3].st.s = stq[3].x;
    p->v[3].st.t = stq[3].y;
    p->v[0].xyz.x = scr[0].x;
    p->v[0].xyz.y = scr[0].y;
    p->v[0].xyz.z = scr[0].z;
    p->v[0].xyz.f = 0xFF;
    p->v[1].xyz.x = scr[1].x;
    p->v[1].xyz.y = scr[1].y;
    p->v[1].xyz.z = scr[1].z;
    p->v[1].xyz.f = 0xFF;
    p->v[2].xyz.x = scr[2].x;
    p->v[2].xyz.y = scr[2].y;
    p->v[2].xyz.z = scr[2].z;
    p->v[2].xyz.f = 0xFF;
    p->v[3].xyz.x = scr[3].x;
    p->v[3].xyz.y = scr[3].y;
    p->v[3].xyz.z = scr[3].z;
    p->v[3].xyz.f = 0xFF;
    z = (scr[0].z + scr[1].z + scr[2].z + scr[3].z) >> 10;
    p->tex0 = tex0;
    if (z < 0) {
        e = &gOtZ[0].layer[l];
    } else if (z >= 0x1000) {
        e = &gOtZ[0xFFF].layer[l];
    } else {
        e = &gOtZ[z].layer[l];
    }
    e->tail->next = p;
    e->tail = (EftAaOtPrim *)p;
}

/* Draws one piece: a quad lying along dir, or a camera-facing sprite. */
#if 0 /* NON-MATCHING: 2 of 57 instructions: the original sets up f15 (width) before a3 (tex0) for the EftGndDust_DrawQuad call, this C after. No parameter order of either function changes it.
   Cleanup E: all 1287 interleavings of EftGndDust_DrawQuad's five integer and eight float parameters were compiled; none gives the
   original order (175 give these 2, the rest more). It is the first scheduling pass: with `$a3 = tex0`, `$f13 = h`
   and `$f15 = w` all ready, it takes the one that comes first in the instruction stream; the original took the two
   float moves first, i.e. the tex0 move ranked lower there (as if tex0 did not die in it). */
void EftGndDust_DrawPiece(Vec4 *pos, Vec4 *color, Vec4 *dir, s32 layer, u64 tex0, s32 along, f32 w, f32 h, f32 rot) {
    Mtx44 m;

    if (along != 0) {
        EftGndDust_MakeFacingMtx(&m, dir, &D_002EC2C0);
        EftGndDust_DrawQuad(pos, color, &m, tex0, 1.0f, h, 0.0f, w, 0.0f, 0.0f, 1.0f, 1.0f, layer);
    } else {
        EftGfx_DrawSprite(pos, color, w, h, 0.0f, 0.0f, 1.0f, 1.0f, rot, layer, 0, tex0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/battle/eft_aa", EftGndDust_DrawPiece);
#endif

/* ---- delayed sound effects ------------------------------------------------------------------------------ */

typedef struct EftDelaySeArg {
    /* 0x0 */ s32 objId;
    /* 0x4 */ struct {
        EftDelaySeDef def;
        s32 unk8;
    } s;
} EftDelaySeArg;

/* Starts one countdown task per {sound, frames} entry for a fighter. */
void EftDelaySe_Start(s32 objId, EftDelaySeDef *def, s32 count) {
    EftDelaySeArg arg;
    s32 i;

    for (i = 0; i < count; i++) {
        memset(&arg.s, 0, sizeof(arg.s));
        arg.objId = objId;
        arg.s.def = def[i];
        BtlTaskList_AddTail(gEftDelaySe->list, gEftDelaySeClass, &arg);
    }
}

/* Manager init: a list of five tasks. */
void EftDelaySeMgr_Init(EftAaTask *task) {
    gEftDelaySe = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftDelaySeMgr));
    gEftDelaySe->list = NULL;
    gEftDelaySe->list = BtlTask_CreateChildList(task, 5, sizeof(EftDelaySe));
}

/* Manager term. */
void EftDelaySeMgr_Term(void) {
    if (gEftDelaySe != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftDelaySe);
        gEftDelaySe = NULL;
    }
}

/* Manager update: nothing. */
void EftDelaySeMgr_Update(void) {
}

/* Task init: copies {fighter, sound, frames}. */
void EftDelaySe_Init(EftAaTask *task, EftDelaySe *arg) {
    EftDelaySe *w = task->work;

    memset(w, 0, sizeof(EftDelaySe));
    *w = *arg;
}

/* Task term: nothing. */
void EftDelaySe_Term(void) {
}

/* Task update: counts down while the fighter is not stopped, then plays the sound and ends. */
void EftDelaySe_Update(EftAaTask *task) {
    EftDelaySe *w = task->work;

    if (!BtlScene_IsCharStopped(w->objId)) {
        if (--w->delay <= 0) {
            Snd_PlaySeEx(4, w->seId, 0x7F, 0x40, 0);
            BtlTask_SetDead(task);
        }
    }
}

/* Task reset: a pending sound is dropped. */
void EftDelaySe_Reset(EftAaTask *task) {
    BtlTask_SetDead(task);
}

/* ---- per-fighter effect slots --------------------------------------------------------------------------- */

/* Manager init: the cleared slot table. */
void EftCharSlotMgr_Init(void) {
    gEftCharSlot = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftCharSlots));
    memset(gEftCharSlot, 0, sizeof(EftCharSlots));
}

/* Manager term. */
void EftCharSlotMgr_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftCharSlot);
    gEftCharSlot = NULL;
}

/* Manager update: nothing. */
void EftCharSlotMgr_Update(void) {
}

/* Manager reset: nothing (the tasks clear their own slots). */
void EftCharSlotMgr_Reset(void) {
}

/* Records the kind-0 effect task of a fighter. */
void EftCharSlot_Set0(s32 objId, void *task) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind0[objId];

    if (task != NULL) {
        s->task = task;
        s->on = 1;
    }
}

/* Empties the kind-0 slot of a fighter. */
void EftCharSlot_Clear0(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind0[objId];

    s->task = NULL;
    s->on = 0;
}

/* The kind-0 effect task of a fighter, or NULL. */
void *EftCharSlot_Get0(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return NULL;
    return gEftCharSlot->kind0[objId].task;
}

/* Records the kind-1 effect task of a fighter. */
void EftCharSlot_Set1(s32 objId, void *task) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind1[objId];

    if (task != NULL) {
        s->task = task;
        s->on = 1;
    }
}

/* Empties the kind-1 slot of a fighter. */
void EftCharSlot_Clear1(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind1[objId];

    s->task = NULL;
    s->on = 0;
}

/* The kind-1 effect task of a fighter, or NULL. */
void *EftCharSlot_Get1(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return NULL;
    EftCharSlot *s = &gEftCharSlot->kind1[objId];

    return s->task;
}

/* Records the kind-2 effect task of a fighter. */
void EftCharSlot_Set2(s32 objId, void *task) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind2[objId];

    if (task != NULL) {
        s->task = task;
        s->on = 1;
    }
}

/* Empties the kind-2 slot of a fighter. */
void EftCharSlot_Clear2(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind2[objId];

    s->task = NULL;
    s->on = 0;
}

/* The kind-2 effect task of a fighter, or NULL. */
void *EftCharSlot_Get2(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return NULL;
    EftCharSlot *s = &gEftCharSlot->kind2[objId];

    return s->task;
}

/* Records the kind-3 effect task of a fighter. */
void EftCharSlot_Set3(s32 objId, void *task) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind3[objId];

    if (task != NULL) {
        s->task = task;
        s->on = 1;
    }
}

/* Empties the kind-3 slot of a fighter. */
void EftCharSlot_Clear3(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->kind3[objId];

    s->task = NULL;
    s->on = 0;
}

/* The kind-3 effect task of a fighter, or NULL. */
void *EftCharSlot_Get3(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return NULL;
    EftCharSlot *s = &gEftCharSlot->kind3[objId];

    return s->task;
}

/* Records the drain glow task of a fighter (EftAbsorb_Start). */
void EftCharSlot_SetAbsorb(s32 objId, void *task) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->absorb[objId];

    if (task != NULL) {
        s->task = task;
        s->on = 1;
    }
}

/* Empties the drain glow slot of a fighter (EftAbsorb_Term). */
void EftCharSlot_ClearAbsorb(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return ;
    EftCharSlot *s = &gEftCharSlot->absorb[objId];

    s->task = NULL;
    s->on = 0;
}

/* The drain glow task of a fighter, or NULL (EftAbsorb_Start / _Stop). */
void *EftCharSlot_GetAbsorb(s32 objId) {
    if (gEftCharSlot == NULL || (u32)objId >= EFT_FIGHTER_CAPACITY) return NULL;
    EftCharSlot *s = &gEftCharSlot->absorb[objId];

    return s->task;
}

/* ---- blade trail ---------------------------------------------------------------------------------------- */

/* Creates the trail task of a fighter. One libc rand(), for a value nothing here reads. */
void EftBlade_Create(s32 objId) {
    EftBladeTrail arg;

    memset(&arg, 0, sizeof(arg));
    arg.objId = objId;
    arg.unk1284 = rand() % 30 + 60;
    BtlTaskList_AddTail(gEftBlade->list, gEftBladeClass, &arg);
}

/* Manager init: fills the table of the eight characters that have a trail and creates both fighters' tasks. */
void EftBladeMgr_Init(EftAaTask *task) {
    EftBladeDef *d = gEftBladeDef;
    s32 i, count = BtlScene_GetCharCount();

    Vec4_Set(&d[0].base, 0.0f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[0].tip, 0.0f, 0.0f, -10.5f, 1.0f);
    Vec4_Set(&d[0].color, 53.0f, 147.0f, 242.0f, 219.0f);
    d[0].chara = 0x3E;
    d[0].layer = 1;
    Vec4_Set(&d[1].base, -1.5f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[1].tip, -1.5f, 0.0f, -14.5f, 1.0f);
    Vec4_Set(&d[1].color, 220.0f, 0.0f, 150.0f, 215.0f);
    d[1].layer = 0;
    d[1].chara = 0x8A;
    Vec4_Set(&d[2].base, 0.0f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[2].tip, 0.0f, 0.0f, -8.1f, 1.0f);
    Vec4_Set(&d[2].color, 53.0f, 147.0f, 242.0f, 219.0f);
    d[2].layer = 1;
    d[2].chara = 0x27;
    Vec4_Set(&d[3].base, 0.0f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[3].tip, 0.0f, 0.0f, -8.1f, 1.0f);
    Vec4_Set(&d[3].color, 53.0f, 147.0f, 242.0f, 219.0f);
    d[3].layer = 1;
    d[3].chara = 0x28;
    Vec4_Set(&d[4].base, 0.0f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[4].tip, 0.0f, 0.0f, -8.5f, 1.0f);
    Vec4_Set(&d[4].color, 220.0f, 220.0f, 240.0f, 255.0f);
    d[4].layer = 0;
    d[4].chara = 0x3C;
    Vec4_Set(&d[5].base, 0.0f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[5].tip, 0.0f, 0.0f, -10.5f, 1.0f);
    Vec4_Set(&d[5].color, 220.0f, 220.0f, 240.0f, 255.0f);
    d[5].layer = 0;
    d[5].chara = 0x94;
    Vec4_Set(&d[6].base, 0.0f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[6].tip, 0.0f, 0.0f, -16.0f, 1.0f);
    Vec4_Set(&d[6].color, 0.0f, 0.0f, 0.0f, 230.0f);
    d[6].layer = 0;
    d[6].chara = 0x6F;
    Vec4_Set(&d[7].base, 0.0f, 0.0f, -1.5f, 1.0f);
    Vec4_Set(&d[7].tip, 0.0f, 0.0f, -16.0f, 1.0f);
    Vec4_Set(&d[7].color, 0.0f, 0.0f, 0.0f, 230.0f);
    d[7].layer = 0;
    d[7].chara = 0x61;
    gEftBlade = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftBladeMgr));
    memset(gEftBlade, 0, sizeof(EftBladeMgr));
    gEftBlade->color = gEftBladeColor;
    if (count > EFT_FIGHTER_CAPACITY) count = EFT_FIGHTER_CAPACITY;
    gEftBlade->list = BtlTask_CreateChildList(task, count, sizeof(EftBlade));
    for (i = 0; i < count; i++) {
        gEftBlade->chara[i] = BtlCharApi_GetChara(i);
        EftBlade_Create(i);
    }
}

/* Manager term. */
void EftBladeMgr_Term(void) {
    if (gEftBlade != NULL) {
        BtlPool_Free(BtlPool_GetCurrent(), gEftBlade);
        gEftBlade = NULL;
    }
}

/* Manager update: nothing. */
void EftBladeMgr_Update(void) {
}

/* Task init: copies the creation block. Two libc rand(), for a vector nothing here reads. */
void EftBlade_Init(EftAaTask *task, EftBladeTrail *arg) {
    EftBlade *w = task->work;

    memset(w, 0, sizeof(EftBlade));
    w->t = *arg;
    w->unk12A0.x = (f32)rand() / 2147483647.0f * 20.0f;
    w->unk12A0.y = -10.0f;
    w->unk12A0.z = (f32)rand() / 2147483647.0f * 20.0f;
    w->unk12A0.w = 1.0f;
}

/* Task term: nothing. */
void EftBlade_Term(void) {
}

/* Task reset: nothing. */
void EftBlade_Reset(void) {
}

#define EFT_BLADE_NEXT()                                                                                              \
    head = (head + 1) & 0x7F;                                                                                         \
    if (head == tail) {                                                                                               \
        tail = (tail + 1) & 0x7F;                                                                                     \
    }

#define EFT_BLADE_SPLINE(f, a)                                                                                        \
    EftBlade_Spline(&t->edgeA[head], t->lastA, f);                                                                    \
    EftBlade_Spline(&t->edgeB[head], t->lastB, f);                                                                    \
    t->alpha[head] = a;                                                                                               \
    EFT_BLADE_NEXT()

/* Appends this frame's blade ends (a = base, b = tip) to the ring while the weapon is out. Once five entries
   exist and the base moved more than 1 unit, the previous entry is replaced by eleven: Catmull-Rom points at
   0.1 .. 0.9 through the last four frames, then the previous entry again. */
#if 0 /* NON-MATCHING: same length (385 instructions) and same calls, but the register allocation differs throughout: the original keeps the addresses of both vector copies in s7 / fp and spills p1, p3, p4 to the stack; this C keeps p1 in a register and spills the address of b. The allocator priorities of p1 and of the two addresses are within 2% of each other (8 / 89 against 27 / 304 and 27 / 313), so the original source differs in some detail that lengthens p1 by a few instructions or adds one use of a and b. */
void EftBlade_AddPoint(EftBladeTrail *t, EftAaVec a, EftAaVec b, s32 objId) {
    s32 head = t->head;
    s32 p4 = (head + 0x7C) & 0x7F;
    s32 p3 = (head + 0x7D) & 0x7F;
    s32 p1 = (head + 0x7F) & 0x7F;
    s32 p2 = (head + 0x7E) & 0x7F;
    s32 tail = t->tail;
    s32 i;

    if (t->wasOut != BtlCharApi_HasWeaponOut(objId) && t->wasOut == 0) {
        for (i = 0; i < 4; i++) {
            Vec4_Copy((Vec4 *)&t->lastA[i], (Vec4 *)&a);
            Vec4_Copy((Vec4 *)&t->lastB[i], (Vec4 *)&b);
        }
    }
    t->wasOut = BtlCharApi_HasWeaponOut(objId);
    if (BtlCharApi_HasWeaponOut(objId)) {
        for (i = 0; i < 3; i++) {
            Vec4_Copy((Vec4 *)&t->lastA[i], (Vec4 *)&t->lastA[i + 1]);
            Vec4_Copy((Vec4 *)&t->lastB[i], (Vec4 *)&t->lastB[i + 1]);
        }
        Vec4_Copy((Vec4 *)&t->lastA[3], (Vec4 *)&a);
        Vec4_Copy((Vec4 *)&t->lastB[3], (Vec4 *)&b);
    }
    if (head == tail || p1 == tail || p2 == tail || p3 == tail || p4 == tail) {
        if (BtlCharApi_HasWeaponOut(objId)) {
            Vec4_Copy((Vec4 *)&t->edgeA[head], (Vec4 *)&a);
            Vec4_Copy((Vec4 *)&t->edgeB[head], (Vec4 *)&b);
            t->alpha[head] = 0x51;
            EFT_BLADE_NEXT()
        }
    } else {
        if (BtlCharApi_HasWeaponOut(objId)) {
            EftAaVec sa;
            EftAaVec sb;
            EftAaVec d;

            Vec4_Sub((Vec4 *)&d, (Vec4 *)&t->lastA[3], (Vec4 *)&t->lastA[2]);
            if (!(Vec3_Length((Vec4 *)&d) <= 1.0f)) {
                Vec4_Copy((Vec4 *)&sa, (Vec4 *)&t->edgeA[p1]);
                Vec4_Copy((Vec4 *)&sb, (Vec4 *)&t->edgeB[p1]);
                EftBlade_Spline(&t->edgeA[p1], t->lastA, 0.1f);
                EftBlade_Spline(&t->edgeB[p1], t->lastB, 0.1f);
                t->alpha[p1] = 0x4C;
                EFT_BLADE_SPLINE(0.2f, 0x4E)
                EFT_BLADE_SPLINE(0.3f, 0x50)
                EFT_BLADE_SPLINE(0.4f, 0x52)
                EFT_BLADE_SPLINE(0.5f, 0x54)
                EFT_BLADE_SPLINE(0.6f, 0x56)
                EFT_BLADE_SPLINE(0.7f, 0x58)
                EFT_BLADE_SPLINE(0.8f, 0x5A)
                EFT_BLADE_SPLINE(0.9f, 0x5C)
                Vec4_Copy((Vec4 *)&t->edgeA[head], (Vec4 *)&sa);
                Vec4_Copy((Vec4 *)&t->edgeB[head], (Vec4 *)&sb);
                t->alpha[head] = 0x5E;
                EFT_BLADE_NEXT()
            }
            Vec4_Copy((Vec4 *)&t->edgeA[head], (Vec4 *)&a);
            Vec4_Copy((Vec4 *)&t->edgeB[head], (Vec4 *)&b);
            t->alpha[head] = 0x60;
            EFT_BLADE_NEXT()
        }
    }
    t->tail = tail;
    t->head = head;
}
#else
LIT4_WORD(D_002FCE84, 0x3DCCCCCC); /* 0.1f */
LIT4_WORD(D_002FCE88, 0x3E4CCCCC); /* 0.2f */
LIT4_WORD(D_002FCE8C, 0x3E999999); /* 0.3f */
LIT4_WORD(D_002FCE90, 0x3ECCCCCC); /* 0.4f */
LIT4_WORD(D_002FCE94, 0x3F199999); /* 0.6f */
LIT4_WORD(D_002FCE98, 0x3F333333); /* 0.7f */
LIT4_WORD(D_002FCE9C, 0x3F4CCCCC); /* 0.8f */
LIT4_WORD(D_002FCEA0, 0x3F666666); /* 0.9f */
INCLUDE_ASM("asm/nonmatchings/battle/eft_aa", EftBlade_AddPoint);
#endif

/* Task update: fades the ring (not while paused) and adds this frame's blade position. */
void EftBlade_Update(EftAaTask *task) {
    EftBladeTrail *t = task->work;
    Mtx44 m;
    EftAaVec a;
    EftAaVec b;
    EftAaVec blade[2] = { { 0.0f, 0.0f, -1.5f, 1.0f }, { 0.0f, 0.0f, -10.5f, 1.0f } };
    s32 head = t->head;
    s32 tail = t->tail;
    s32 i;
    s32 j;

    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    for (i = tail; i != head; i = (i + 1) & 0x7F) {
        if (t->alpha[i] > 0) {
            t->alpha[i] -= 0x14;
            if (t->alpha[i] <= 0) {
                t->alpha[i] = 0;
                t->tail = i;
            }
        }
    }
    if (BtlCharApi_HasWeaponOut(t->objId)) {
        for (j = 0; j < 8; j++) {
            if (gEftBladeDef[j].chara == BtlCharApi_GetChara(t->objId)) {
                Vec4_Copy((Vec4 *)&blade[0], &gEftBladeDef[j].base);
                Vec4_Copy((Vec4 *)&blade[1], &gEftBladeDef[j].tip);
            }
        }
        BtlCharApi_GetNodeMtx(t->objId, 0x40, &m);
        Mtx_MulVec4((Vec4 *)&a, &m, (Vec4 *)&blade[0]);
        Mtx_MulVec4((Vec4 *)&b, &m, (Vec4 *)&blade[1]);
        EftBlade_AddPoint(t, a, b, t->objId);
    } else {
        t->wasOut = 0;
    }
}

/* GS PRIM value: primitive type, alpha blending, context. */
static inline u64 EftBlade_MakePrim(s32 type, s32 abe, s32 ctx) {
    return ((u64)ctx << 9) | ((u64)abe << 6) | type;
}

/* Task draw: the ribbon as two gouraud triangles per ring step, in the character's colour, alpha from the ring. */
void EftBlade_Draw(EftAaTask *task) {
    EftBladeTrail *t = task->work;
    EftAaVec uv[3] = { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } };
    EftAaVec pos[3];
    EftAaScr scr[3];
    EftAaVec stq[3];
    EftAaIVec col;
    s32 alpha[3];
    s32 head = t->head;
    s32 p3 = (head + 0x7D) & 0x7F;
    s32 p2 = (head + 0x7E) & 0x7F;
    s32 p1 = (head + 0x7F) & 0x7F;
    s32 p4 = (head + 0x7C) & 0x7F;
    s32 i = t->tail;
    EftBladeDef *def;
    f32 aScale;
    s32 k;

    if (BtlCharApi_IsHidden(t->objId)) {
        return;
    }
    if (head == i || p1 == i || p2 == i || p3 == i || p4 == i) {
        return;
    }
    EftBlade_GetColor(&col);
    def = EftBlade_FindDef(t->objId);
    if (def == NULL) {
        return;
    }
    col.x = def->color.x;
    col.y = def->color.y;
    col.z = def->color.z;
    col.w = def->color.w;
    aScale = (f32)col.w / 255.0f;
    while (i != head) {
        i = (i + 1) & 0x7F;
        if (i == p1) {
            break;
        }
        if (t->alpha[i] == 0) {
            continue;
        }
        for (k = 0; k < 2; k++) {
            EftBladeTriPkt *p;
            EftAaOtEntry *e;
            s32 abe = 1;
            s32 layer;
            s32 l;
            s32 z;

            if (k & 1) {
                Vec4_Copy((Vec4 *)&pos[0], (Vec4 *)&t->edgeA[i]);
                Vec4_Copy((Vec4 *)&pos[1], (Vec4 *)&t->edgeB[i]);
                Vec4_Copy((Vec4 *)&pos[2], (Vec4 *)&t->edgeA[(i + 1) & 0x7F]);
                uv[0].x = 1.0f;
                uv[0].y = 0.0f;
                uv[1].x = 0.0f;
                uv[1].y = 0.0f;
                uv[2].x = 1.0f;
                uv[2].y = 0.0f;
                alpha[1] = t->alpha[i];
                alpha[0] = 0;
                alpha[2] = 0;
            } else {
                Vec4_Copy((Vec4 *)&pos[0], (Vec4 *)&t->edgeB[i]);
                Vec4_Copy((Vec4 *)&pos[1], (Vec4 *)&t->edgeA[(i + 1) & 0x7F]);
                Vec4_Copy((Vec4 *)&pos[2], (Vec4 *)&t->edgeB[(i + 1) & 0x7F]);
                uv[0].x = 0.0f;
                uv[0].y = 0.0f;
                uv[1].x = 1.0f;
                uv[1].y = 0.0f;
                uv[2].x = 0.0f;
                uv[2].y = 0.0f;
                alpha[0] = t->alpha[i];
                alpha[2] = t->alpha[(i + 1) & 0x7F];
                alpha[1] = 0;
            }
            pos[0].w = 1.0f;
            pos[1].w = 1.0f;
            pos[2].w = 1.0f;
            Mtx_ProjectPointsStq(scr, (Vec4 *)stq, &gBtlCamView->world2screen, (Vec4 *)pos, (Vec4 *)uv, 3);
            if (EftBlade_IsClipped(&scr[0])) {
                continue;
            }
            if (EftBlade_IsClipped(&scr[1])) {
                continue;
            }
            if (EftBlade_IsClipped(&scr[2])) {
                continue;
            }
            p = (EftBladeTriPkt *)gOtCur;
            gOtCur = (u8 *)(p + 1);
            layer = def->layer;
            p->prim = EftBlade_MakePrim(0xB, abe, layer >= 2);
            p->dmaTag = 0x20000005;
            p->vif1 = 0x50000005;
            p->vif0 = 0x10000000;
            p->gifTag = 0x8400000000008001;
            p->regs = 0xF4141410;
            p->next = NULL;
            p->v[0].rgbaq.r = col.x;
            p->v[0].rgbaq.g = col.y;
            p->v[0].rgbaq.b = col.z;
            p->v[0].rgbaq.a = (f32)alpha[0] * aScale;
            p->v[0].rgbaq.q = 1.0f;
            p->v[1].rgbaq.r = col.x;
            p->v[1].rgbaq.g = col.y;
            p->v[1].rgbaq.b = col.z;
            p->v[1].rgbaq.a = (f32)alpha[1] * aScale;
            p->v[1].rgbaq.q = 1.0f;
            p->v[2].rgbaq.r = col.x;
            p->v[2].rgbaq.g = col.y;
            p->v[2].rgbaq.b = col.z;
            p->v[2].rgbaq.a = (f32)alpha[2] * aScale;
            l = layer;
            p->v[2].rgbaq.q = 1.0f;
            if (l >= 2) {
                l -= 2;
            }
            p->v[0].xyz.x = scr[0].x;
            p->v[0].xyz.y = scr[0].y;
            p->v[0].xyz.z = scr[0].z;
            p->v[0].xyz.f = 0xFF;
            p->v[1].xyz.x = scr[1].x;
            p->v[1].xyz.y = scr[1].y;
            p->v[1].xyz.z = scr[1].z;
            p->v[1].xyz.f = 0xFF;
            p->v[2].xyz.x = scr[2].x;
            p->v[2].xyz.y = scr[2].y;
            p->v[2].xyz.z = scr[2].z;
            p->v[2].xyz.f = 0xFF;
            z = scr[0].z >> 8;
            if (z < 0) {
                e = &gOtZ[0].layer[l];
            } else if (z >= 0x1000) {
                e = &gOtZ[0xFFF].layer[l];
            } else {
                e = &gOtZ[z].layer[l];
            }
            e->tail->next = p;
            e->tail = (EftAaOtPrim *)p;
        }
    }
}

/* Catmull-Rom point between p[1] and p[2] at parameter t (Hermite with tangents (p[2] - p[0]) / 2, (p[3] - p[1]) / 2). */
void EftBlade_Spline(EftAaVec *out, EftAaVec *p, f32 t) {
    EftAaVec m0;
    EftAaVec m1;
    EftAaVec a;
    EftAaVec b;
    f32 t2 = t * t;
    f32 t3;

    Vec4_Sub((Vec4 *)&a, (Vec4 *)&p[1], (Vec4 *)&p[0]);
    Vec4_Sub((Vec4 *)&b, (Vec4 *)&p[2], (Vec4 *)&p[1]);
    Vec4_Add((Vec4 *)&m0, (Vec4 *)&a, (Vec4 *)&b);
    Vec4_Sub((Vec4 *)&m1, (Vec4 *)&p[3], (Vec4 *)&p[2]);
    Vec4_Add((Vec4 *)&m1, (Vec4 *)&b, (Vec4 *)&m1);
    t3 = t2 * t;
    Vec4_Scale((Vec4 *)&m0, (Vec4 *)&m0, 0.5f);
    Vec4_Scale((Vec4 *)&m1, (Vec4 *)&m1, 0.5f);
    Vec4_Scale((Vec4 *)&a, (Vec4 *)&p[1], 2.0f * t3 - 3.0f * t2 + 1.0f);
    Vec4_Scale((Vec4 *)&m0, (Vec4 *)&m0, t3 - 2.0f * t2 + t);
    Vec4_Scale((Vec4 *)&m1, (Vec4 *)&m1, t3 - t2);
    Vec4_Add((Vec4 *)&m0, (Vec4 *)&a, (Vec4 *)&m0);
    Vec4_Add((Vec4 *)&m0, (Vec4 *)&m0, (Vec4 *)&m1);
    Vec4_Scale((Vec4 *)&a, (Vec4 *)&p[2], -2.0f * t3 + 3.0f * t2);
    Vec4_Add((Vec4 *)out, (Vec4 *)&a, (Vec4 *)&m0);
}

/* The default trail colour (RGBA bytes), or NULL before the manager exists. */
u8 *EftBlade_GetColorPtr(void) {
    u8 *r = NULL;

    if (gEftBlade != NULL) {
        r = gEftBlade->color;
    }
    return r;
}

/* The default trail colour as four ints; returns its alpha as 0..1. */
/* `c` is initialised to NULL at its declaration and assigned again behind the test: a variable with two sets is
   a pseudo of its own, so the tested value stays in v0 and its copy goes to a1 in the delay slot (the original's
   `lw v0,0xC(v0) / bnez v0 / move a1,v0`). With a single assignment cse folds the copy into the tested register
   and the load goes straight to a1 (17 of 31 instructions). This is the "surviving copy" of the ribbon placers
   in its simplest form. (decomp-permuter found it as a read of the uninitialised `c` in the default arm.) */
f32 EftBlade_GetColor(EftAaIVec *out) {
    u8 *c = NULL;

    if (gEftBlade == NULL || gEftBlade->color == NULL) {
        out->x = 0xF0;
        out->y = 0xF0;
        out->z = 0xF0;
        out->w = 0x80;
        return 1.0f;
    }
    c = gEftBlade->color;
    out->x = c[0];
    out->y = c[1];
    out->z = c[2];
    out->w = c[3];
    return (f32)out->w / 255.0f;
}

/* The trail definition of a fighter's character, or NULL when it has none. */
EftBladeDef *EftBlade_FindDef(s32 objId) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (gEftBladeDef[i].chara == BtlCharApi_GetChara(objId)) {
            return &gEftBladeDef[i];
        }
    }
    return NULL;
}

/* 1 when a projected point cannot be drawn. */
s32 EftBlade_IsClipped(EftAaScr *p) {
    s32 r;

    if (p->z <= 0) {
        return 1;
    }
    if (p->x > 0xFFEF) {
        return 1;
    }
    if (p->x <= 0) {
        return 1;
    }
    r = p->y < 1;
    if (p->y > 0xFFEF) {
        r = 1;
    }
    return r;
}

/* ---- blinding overlay ----------------------------------------------------------------------------------- */

#define EFT_BLIND_CLAMP(x) ((x) < 0.0f ? 0.0f : ((x) > 255.0f ? 255.0f : (x)))

/* Init callback. */
void EftBlind_Init(void) {
    gEftBlind = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftBlind));
    memset(gEftBlind, 0, sizeof(EftBlind));
    gEftBlind->flags |= EFT_BLIND_ACTIVE;
}

/* Update callback: reads each fighter's blind level (fighter +0xFFC / 90) into an alpha. Object 1 is read only in
   split screen. Skipped while time is stopped. */
void EftBlind_Update(void) {
#ifdef PORT
    if (TagTeam_Active()) {
        s32 i;
        if (BtlScene_IsTimeStopped()) return;
        for (i = 0; i < EFT_FIGHTER_CAPACITY; i++)
            gEftBlind->alpha[i] = EFT_BLIND_CLAMP(BtlCharApi_GetBlindRatio(i) * 255.0f);
        return;
    }
#endif
    if (BtlScene_IsTimeStopped()) {
        return;
    }
    gEftBlind->alpha[0] = EFT_BLIND_CLAMP(BtlCharApi_GetBlindRatio(0) * 255.0f);
    if (gEftBlind->alpha[0] <= 0.0f) {
        gEftBlind->flags &= ~EFT_BLIND_SHOW0;
    } else {
        gEftBlind->flags |= EFT_BLIND_SHOW0;
    }
    if (Battle_IsSplitScreen()) {
        gEftBlind->alpha[1] = EFT_BLIND_CLAMP(BtlCharApi_GetBlindRatio(1) * 255.0f);
        if (gEftBlind->alpha[1] <= 0.0f) {
            gEftBlind->flags &= ~EFT_BLIND_SHOW1;
        } else {
            gEftBlind->flags |= EFT_BLIND_SHOW1;
        }
    }
}

/* Draw callback: object 0's overlay over the whole screen, or each object's over its half in split screen. */
void EftBlind_Draw(void) {
#ifdef PORT
    if (TagTeam_Active()) {
        extern s32 TagTeam_CurrentViewActor(void);
        extern EftAaView *BtlCam_GetNativeView(s32);
        s32 actor = TagTeam_CurrentViewActor(), view;
        if (BattleReplay_IsActive() || (u32)actor >= EFT_FIGHTER_CAPACITY) return;
        if (gEftBlind->alpha[actor] <= 0) return;
        for (view = 0; view < TagTeam_ViewCount(); view++)
            if (gBtlCamView == BtlCam_GetNativeView(view)) {
                EftBlind_DrawOverlay(view, BtlScene_IsSingleView(), gEftBlind->alpha[actor]);
                break;
            }
        return;
    }
#endif
    if (BattleReplay_IsActive()) {
        return;
    }
    if (Battle_IsSplitScreen()) {
        if (BtlScene_IsSingleView()) {
            return;
        }
        if (gEftBlind->flags & EFT_BLIND_SHOW0) {
            EftBlind_DrawOverlay(0, 0, gEftBlind->alpha[0]);
        }
        if (gEftBlind->flags & EFT_BLIND_SHOW1) {
            EftBlind_DrawOverlay(1, 0, gEftBlind->alpha[1]);
        }
    } else {
        if (gEftBlind->flags & EFT_BLIND_SHOW0) {
            EftBlind_DrawOverlay(0, 1, gEftBlind->alpha[0]);
        }
    }
}

/* Term callback. */
void EftBlind_Term(void) {
    BtlPool_Free(BtlPool_GetCurrent(), gEftBlind);
    gEftBlind = NULL;
}

/* Fills the two corners of the overlay sprite: z = 0xFFFFFF, fog 0xFF. */
static inline void EftBlind_SetRect(EftBlindPkt *p, s32 x0, s32 y0, s32 x1, s32 y1) {
    p->xyz0.x = x0;
    p->xyz0.y = y0;
    p->xyz1.x = x1;
    p->xyz1.y = y1;
    p->xyz0.z = 0xFFFFFF;
    p->xyz1.z = 0xFFFFFF;
    p->xyz0.f = 0xFF;
    p->xyz1.f = 0xFF;
}

/* Queues a grey blended sprite in the last ordering table slot: the whole screen, or the left (view 0) or
   right (view 1) half. */
void EftBlind_DrawOverlay(s32 view, s32 full, f32 alpha) {
    EftAaVec c;
    EftBlindPkt *p;
    EftAaOtEntry *e;

    Vec4_Set((Vec4 *)&c, 128.0f, 128.0f, 128.0f, alpha);
    p = (EftBlindPkt *)gOtCur;
    gOtCur = (u8 *)(p + 1);
    p->prim = 0x46;
    p->dmaTag = 0x20000003;
    p->vif1 = 0x50000003;
    p->vif0 = 0x10000000;
    p->gifTag = 0x4400000000008001;
    p->regs = 0x4410;
    p->next = NULL;
    p->rgbaq.r = c.x;
    p->rgbaq.g = c.y;
    p->rgbaq.b = c.z;
    p->rgbaq.a = c.w;
    p->rgbaq.q = 1.0f;
#ifdef PORT
    if (TagTeam_Active()) {
        extern void TagTeam_ViewRect(s32,s32 *);
        s32 rect[4] = {0,0,512,448};
        if (!full) TagTeam_ViewRect(view,rect);
        EftBlind_SetRect(p, 0x7000 + rect[0]*16, 0x7200 + rect[1]*16,
                        0x7000 + (rect[0]+rect[2])*16, 0x7200 + (rect[1]+rect[3])*16);
    } else
#endif
    if (full == 0) {
        if (view == 0) {
            EftBlind_SetRect(p, 0x7000, 0x7200, -0x8000, -0x7200);
        } else {
            EftBlind_SetRect(p, -0x8000, 0x7200, -0x7000, -0x7200);
        }
    } else {
        EftBlind_SetRect(p, 0x7000, 0x7200, -0x7000, -0x7200);
    }
    e = &gOtLast[0].layer[1];
    e->tail->next = p;
    e->tail = (EftAaOtPrim *)p;
}

/* ---- effect pack part kind 14 --------------------------------------------------------------------------- */

/* Creates a part; returns its task as the handle. */
EftAaTask *EftAnimPart_Create(EftAnimPartArg *arg) {
    return BtlTaskList_AddTail(gEftAnimPartList, gEftAnimPartClass, arg);
}

/* Asks a part to end: hold, fade, then die. */
s32 EftAnimPart_Stop(EftAaTask *task) {
    if (EftAnimPart_IsValid(task)) {
        ((EftAnimPart *)task->work)->flags |= EFT_ANIMPART_ENDING;
        return 1;
    }
    return 0;
}

/* Kills a part at its next update. */
s32 EftAnimPart_Kill(EftAaTask *task) {
    if (EftAnimPart_IsValid(task)) {
        ((EftAnimPart *)task->work)->flags |= EFT_ANIMPART_DEAD;
        return 1;
    }
    return 0;
}

/* Sets the effect kind used for the stop / hide tests. */
s32 EftAnimPart_SetType(EftAaTask *task, s32 type) {
    if (EftAnimPart_IsValid(task)) {
        ((EftAnimPart *)task->work)->type = type;
        return 1;
    }
    return 0;
}

/* Moves a part. */
s32 EftAnimPart_SetPos(EftAaTask *task, Vec4 *pos) {
    if (EftAnimPart_IsValid(task)) {
        EftSprAnim_SetPos(task->work, pos);
        return 1;
    }
    return 0;
}

/* Scales a part uniformly. */
s32 EftAnimPart_SetSize(EftAaTask *task, f32 size) {
    EftAaVec v;

    if (EftAnimPart_IsValid(task)) {
        EftAnimPart *w = task->work;

        Vec4_Set((Vec4 *)&v, size, size, size, 1.0f);
        EftSprAnim_SetScale(w, (Vec4 *)&v);
        return 1;
    }
    return 0;
}

/* Turns a part. */
s32 EftAnimPart_SetDir(EftAaTask *task, Vec4 *dir) {
    if (EftAnimPart_IsValid(task)) {
        EftSprAnim_SetDir(task->work, dir);
        return 1;
    }
    return 0;
}

/* Sets the frames before the part starts. */
s32 EftAnimPart_SetDelay(EftAaTask *task, f32 frames) {
    if (EftAnimPart_IsValid(task)) {
        ((EftAnimPart *)task->work)->delay = frames;
        return 1;
    }
    return 0;
}

/* Sets the frames between the end and the fade. */
s32 EftAnimPart_SetHold(EftAaTask *task, f32 frames) {
    if (EftAnimPart_IsValid(task)) {
        ((EftAnimPart *)task->work)->hold = frames;
        return 1;
    }
    return 0;
}

/* Sets the length of the fade. */
s32 EftAnimPart_SetFade(EftAaTask *task, f32 frames) {
    if (EftAnimPart_IsValid(task)) {
        EftAnimPart *w = task->work;

        w->fade = frames;
        w->fadeTime = frames;
        return 1;
    }
    return 0;
}

/* 1 while the handle is a live part. */
s32 EftAnimPart_IsAlive(EftAaTask *task) {
    return EftAnimPart_IsValid(task) != 0;
}

/* Manager init: a list of 32 parts. */
void EftAnimPartMgr_Init(EftAaTask *task) {
    gEftAnimPartList = BtlTask_CreateChildList(task, 0x20, sizeof(EftAnimPart));
}

/* Manager term: nothing. */
void EftAnimPartMgr_Term(void) {
}

/* Manager update: nothing. */
void EftAnimPartMgr_Update(void) {
}

/* Task init: creates the animated object and places it. */
void EftAnimPart_Init(EftAaTask *task, EftAnimPartArg *arg) {
    EftAnimPart *w = task->work;
    EftAaVec v;

    memset(w, 0, sizeof(EftAnimPart));
    w->arg = *arg;
    EftSprAnim_Create(w, w->arg.tex, w->arg.res);
    w->taskId = task->id;
    w->flags |= EFT_ANIMPART_VALID;
    if (arg->life > 0.0f) {
        w->life = arg->life * 30.0f;
        w->flags |= EFT_ANIMPART_TIMED;
    }
    Vec4_Set((Vec4 *)&v, w->arg.size, w->arg.size, w->arg.size, 1.0f);
    EftSprAnim_SetPos(w, (Vec4 *)&w->arg.pos);
    EftSprAnim_SetDir(w, (Vec4 *)&w->arg.dir);
    EftSprAnim_SetScale(w, (Vec4 *)&v);
}

/* Task term: frees the animated object. */
void EftAnimPart_Term(EftAaTask *task) {
    EftSprAnim_Destroy(task->work);
}

/* Task reset: the part is dropped. */
void EftAnimPart_Reset(EftAaTask *task) {
    BtlTask_SetDead(task);
}

/* Task update: delay, then steps the object until its animation or its life ends; then hold and fade. */
void EftAnimPart_Update(EftAaTask *task) {
    EftAnimPart *w = task->work;
    f32 a = 0.0f;
    EftAaVec c;

    if (!BtlScene_IsEffectStopped(w->arg.chr, w->type)) {
        if (w->delay <= 0.0f) {
            EftSprAnim_Step(w);
            w->flags |= EFT_ANIMPART_STARTED;
            if (w->flags & EFT_ANIMPART_TIMED) {
                w->life -= 1.0f;
                if (w->life <= 0.0f) {
                    w->flags |= EFT_ANIMPART_ENDING;
                }
            }
            if (!EftSprAnim_IsPlaying(w)) {
                w->flags &= ~EFT_ANIMPART_STARTED;
                w->flags |= EFT_ANIMPART_ENDING;
            }
        } else {
            w->delay -= 1.0f;
        }
        if (w->flags & EFT_ANIMPART_ENDING) {
            if (w->hold > 0.0f) {
                w->hold -= 1.0f;
            } else {
                if (w->fadeTime > 0.0f) {
                    a = w->fade / w->fadeTime;
                    w->fade -= 1.0f;
                }
                if (w->fade <= 0.0f) {
                    w->flags |= EFT_ANIMPART_DEAD;
                    w->flags &= ~EFT_ANIMPART_VALID;
                    a = 0.0f;
                }
                Vec4_Set((Vec4 *)&c, 1.0f, 1.0f, 1.0f, a);
                EftSprAnim_SetColor(w, (Vec4 *)&c);
            }
        }
    }
    if (w->flags & EFT_ANIMPART_DEAD) {
        BtlTask_SetDead(task);
    } else {
        EftSprAnim_KeepTextures(w);
    }
}

/* Task draw: picks the draw layer from the part's mode; some modes are not drawn in a view that does not
   show the owner (unless a technique camera cut is running). */
/* FAKE MATCH (permuter): `other++; other--;` in front of the switch. The pair combines to a self-move of
   `other` that is deleted after register allocation; it only gives `other` two more references (one more set,
   one more use). Without it the code is the same with one register exchange (16 of 67 instructions: `other` in
   s3 and `w` in s2). Global allocation orders the two by log2(refs) * refs / live length: w has 7 references
   over 43 instructions (14 / 43), other 5 over 38 (10 / 38), and the original needs `other` first: 7 references
   to `other` (14 / 39), or 5 to `w`. The dummy stands in for those two references, wherever the original had
   them; the forms of the `other` assignment (if / else, &&, |=, a second statement, narrower types), separate
   case bodies, the mode in a local and other pointers for the two draw calls were tried without effect. (The
   permuter also reused `other` for the constant of the flags test; that half is not needed.) */
void EftAnimPart_Draw(EftAaTask *task) {
    s32 layer = 0;
    EftAnimPart *w = task->work;
    s32 chr = w->arg.chr;
    s32 other = 0;

    if (BtlScene_IsEffectHidden(chr, w->type)) {
        return;
    }
    if (!EftCam_IsActive()) {
        other = BtlScene_IsCharInView(chr) == 0;
    }
    other++;
    other--;
    switch (w->arg.mode) {
    case 1:
    case 3:
        if (other) {
            return;
        }
        layer = 5;
        break;
    case 4:
        layer = 10;
        break;
    case 5:
        layer = 6;
        break;
    case 6:
        if (!other) {
            layer = 5;
            break;
        }
    case 0:
    case 2:
        layer = 9;
        break;
    case 7:
        layer = 0;
        break;
    case 8:
        if (other) {
            return;
        }
        layer = 0x11;
        break;
    case 9:
        layer = 0x12;
        break;
    }
    if (w->flags & EFT_ANIMPART_STARTED) {
        EftSprAnim_SetDrawFlags(w, layer);
        EftSprAnim_Draw(w);
    }
}

/* 1 when a handle still names a live part: the task is not killed, is of this class, has the id the part was
   created with, and the part has not finished fading. */
s32 EftAnimPart_IsValid(EftAaTask *task) {
    EftAnimPart *w;

    if (task == NULL) {
        return 0;
    }
    if ((u8)(task->flags & 1)) {
        return 0;
    }
    if (task->cls[0] != EftAnimPart_Update) {
        return 0;
    }
    w = task->work;
    if (w->taskId != task->id) {
        return 0;
    }
    if (w->flags & EFT_ANIMPART_VALID) {
        return 1;
    }
    return 0;
}

/* ---- next module's manager ------------------------------------------------------------------------------ */

/* Init callback of the manager whose tasks follow this file: binds its common entries and creates its task list. */
void EftOrbTailMgr_Init(EftAaTask *task) {
    s32 i;

    gEftOrbTail = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftAaOrbTailMgr));
    gEftOrbTail->w = NULL;
    gEftOrbTail->w = BtlPool_Alloc(BtlPool_GetCurrent(), sizeof(EftAaOrbTailWork));
    memset(gEftOrbTail->w, 0, sizeof(EftAaOrbTailWork));
    EftTexSet_Load16(gEftOrbTail->w->res0, BtlScene_GetCommonEntry(0x259));
    gEftOrbTail->w->unk0 = BtlScene_GetCommonEntry(0x25B);
    gEftOrbTail->w->unk4 = BtlScene_GetCommonEntry(0x25A);
    {
        s32 idsC[7] = { 0x261, 0x262, 0x263, 0x264, 0x265, 0x266, 0x267 };
        s32 idsB[3] = { 0x25D, 0x25F, 0x260 };
        s32 idsA[3] = { 0x25C, 0x25E, 0x25E };

        for (i = 0; i < 3; i++) {
            EftTexSet_Load16(gEftOrbTail->w->res[i], BtlScene_GetCommonEntry(idsA[i]));
        }
        for (i = 0; i < 3; i++) {
            gEftOrbTail->w->unk428[i] = BtlScene_GetCommonEntry(idsB[i]);
        }
        for (i = 0; i < 7; i++) {
            gEftOrbTail->w->unk434[i] = BtlScene_GetCommonEntry(idsC[i]);
        }
    }
    gEftOrbTail->w->list = BtlTask_CreateChildList(task, BtlScene_GetCharCount(), 0x1E0);
}
