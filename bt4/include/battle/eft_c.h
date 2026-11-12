#ifndef BATTLE_EFT_C_H
#define BATTLE_EFT_C_H

#include "types.h"
#include "sys/math3d.h"

/*
 * Stage-attached effects: src/battle/eft_c.c = 0x136760..0x13A9D0 (70 functions).
 *
 * Everything here is drawing. Nothing in the range reads or writes a fighter, a battle object, a hit record
 * or a battle event; the inputs from the fight are the pause flag (BattleWork.flags & 0x100) and the camera.
 *
 *   0x136760..0x136CC0  tail of the geyser of eft_b.c (task class gEftGeyserClass 0x2C3508): the smoke and
 *                       steam emitters a column starts when it is created (EftGeyser_StartSmoke / StartSteam).
 *   0x136CC0..0x137BD0  weather particles (rain / snow / ash), state gEftWeather (gp 0x2FE9BC, 0x2C0 bytes),
 *                       task class 0x2C3520 with one child task of class gEftWeatherPtclClass (0x2C3538).
 *   0x137BD0..0x138178  the layer-0 task of the effect scene (class 0x2C3550): the manager of the stage
 *                       effects, state gEftStage (gp 0x2FE9C0, 0x58 bytes). It creates one child task per
 *                       stage effect kind the loaded stage has data for (gEftStageKinds, 0x2C3568).
 *   0x138178..0x13A9D0  animated surfaces of the stage (water, lava, scrolling layers), state gEftSurf
 *                       (gp 0x2FE9C4, 0x1380 bytes), task class 0x2C35B8. The module continues in eft_d.c.
 *
 * Stage effect kinds (gEftStageKinds: {class, kind}; EftStage_HasKind(kind) decides whether the task exists):
 *   0  class 0x2C34D8                 stage pack entry 7 not empty
 *   1  class 0x2C35B8 (surfaces)      stage pack entry 0xD not empty
 *   2  class 0x2C3520 (weather)       stage pack entry 0xB not empty
 *   3  class 0x2C3658                 stage pack entry 0xE not empty
 *   4  class 0x2C3670 (smoke)         BtlStage_GetFxResB2() != NULL
 *   5  class 0x2C3600 (steam)         BtlStage_GetFxResC2() != NULL
 *   6  class 0x2C34F0 (geysers)       BtlStage_GetFxResA2() != NULL
 *   7  classes 0x2C3640 and 0x2C34C0  BtlStage_GetList90() has bit 0 set and stage pack entry 0x12 not empty
 *   8  class 0x2C36A0                 stage pack entries 0x15, 0x16 and 0x17 not empty
 *
 * Callback slots of a task class: {update, init, term, postUpdate, reset, draw}.
 *   weather    {EftWeather_Update, EftWeather_Init, EftWeather_Term}
 *   particles  {EftWeatherPtcl_Update, _Init, _Term, _PostUpdate, _Reset, _Draw}
 *   layer 0    {EftStage_Update, EftStage_Init, EftStage_Term}
 *   surfaces   {EftSurf_Update, EftSurf_Init, EftSurf_Term, EftSurf_PostUpdate, EftSurf_Reset, EftSurf_Draw}
 */

/* A vector as the effect code declares it on the stack: 16-byte aligned (copied with ld / sd). */
typedef struct EftVec {
    /* 0x00 */ f32 x;
    /* 0x04 */ f32 y;
    /* 0x08 */ f32 z;
    /* 0x0C */ f32 w;
} __attribute__((aligned(16))) EftVec;

/* The same as a float array (Sony's sceVu0FVECTOR), which is how the surface code indexes its vectors. */
typedef f32 EftFVec[4] __attribute__((aligned(16)));

/* A task of the effect scene's task tree (0x1AD150..0x1ADC00). Only the fields used here. */
typedef struct EftTask {
    /* 0x00 */ u8 flags; /* bit 0: dead */
    /* 0x04 */ u8 unk4[0x24];
    /* 0x28 */ struct EftTaskClass *cls;
    /* 0x2C */ u8 unk2C[0xC];
    /* 0x38 */ void *work; /* work block of the size given to BtlTask_CreateChildList */
    /* 0x3C */ s32 unk3C;
} EftTask; /* size 0x40 */

typedef struct EftTaskClass {
    /* 0x00 */ void (*update)(EftTask *task);
    /* 0x04 */ void (*init)(EftTask *task, void *arg);
    /* 0x08 */ void (*term)(EftTask *task);
    /* 0x0C */ void (*postUpdate)(EftTask *task);
    /* 0x10 */ void (*reset)(EftTask *task);
    /* 0x14 */ void (*draw)(EftTask *task);
} EftTaskClass; /* size 0x18 */

/* ---- geyser (work block of gEftGeyserClass, eft_b.c; only what the four functions here use) ---- */

typedef struct EftGeyserView {
    /* 0x00 */ u8 unk0[0x70];
    /* 0x70 */ Vec4 pos;
    /* 0x80 */ u8 unk80[0x30];
    /* 0xB0 */ f32 unkB0; /* size or strength; scales both emitters */
    /* 0xB4 */ f32 unkB4;
    /* 0xB8 */ u8 unkB8[0x34];
    /* 0xEC */ s32 smoke; /* emitter task from EftSmoke_Create */
    /* 0xF0 */ s32 steam; /* emitter task from EftSteam_Create */
} EftGeyserView;

/* Argument of EftSmoke_Create. */
typedef struct EftGeyserSmokeArg {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ EftVec colorA;
    /* 0x20 */ EftVec colorB;
    /* 0x30 */ f32 unk30;
    /* 0x34 */ f32 unk34;
    /* 0x38 */ f32 unk38;
    /* 0x3C */ f32 unk3C;
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 unk44;
    /* 0x48 */ s32 unk48;
    /* 0x4C */ s32 unk4C;
} EftGeyserSmokeArg; /* size 0x50 */

/* Argument of EftSteam_Create. */
typedef struct EftGeyserSteamArg {
    /* 0x00 */ EftVec pos;
    /* 0x10 */ EftVec unk10;
    /* 0x20 */ EftVec color;
    /* 0x30 */ f32 unk30;
    /* 0x34 */ f32 gravity; /* 9.8 / 30 */
    /* 0x38 */ f32 unk38;
    /* 0x3C */ s32 unk3C;
    /* 0x40 */ f32 unk40;
    /* 0x44 */ f32 unk44;
    /* 0x48 */ f32 unk48;
    /* 0x4C */ f32 unk4C;
    /* 0x50 */ s32 unk50;
    /* 0x54 */ s32 unk54[3];
} EftGeyserSteamArg; /* size 0x60 */

/* ---- weather ---- */

#define EFT_WEATHER_PTCL_MAX 30

/* One weather particle; positions are relative to the camera. */
typedef struct EftWeatherPtcl {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 vel;   /* random walk, each component clamped to +-0.4 */
    /* 0x20 */ Vec4 color; /* r, g, b, alpha */
    /* 0x30 */ s32 wrap;   /* 3 after the particle wrapped around the view volume, counts down: fade in */
    /* 0x34 */ s32 tex;    /* index into EftWeather.tex */
    /* 0x38 */ s32 unk38[2];
} EftWeatherPtcl; /* size 0x40 */

/* Texture table filled by EftTexSet_Load32. */
typedef struct EftTexEntry {
    /* 0x00 */ u64 tex0;
    /* 0x08 */ struct EftTexDef *def;
    /* 0x0C */ s32 unkC;
} EftTexEntry; /* size 0x10 */

typedef struct EftTexTbl {
    /* 0x000 */ EftTexEntry tex[32];
    /* 0x200 */ s32 count;
    /* 0x204 */ s32 unk204;
} EftTexTbl; /* size 0x208 */

/* Per-view camera tracking of the weather. */
typedef struct EftWeatherView {
    /* 0x00 */ Vec4 camPos[2]; /* camera position of the previous frame, per view */
    /* 0x20 */ s32 moveCnt[2]; /* 0..15: rises while the camera moves fast, fades the particles */
} EftWeatherView;

typedef struct EftWeather {
    /* 0x000 */ void *list;       /* child task list holding the particle task */
    /* 0x004 */ s32 unk4;
    /* 0x008 */ EftTexTbl tex;    /* stage pack entry 0xB */
    /* 0x210 */ s32 color[4];     /* 0x80 each */
    /* 0x220 */ Vec4 wind;        /* from the stage's weather record */
    /* 0x230 */ Vec4 windNow;     /* wind plus a slow sine on x and z */
    /* 0x240 */ EftWeatherView view;
    /* 0x268 */ u8 unk268[0x38];
    /* 0x2A0 */ s32 angle;        /* degrees, 0..359 */
    /* 0x2A4 */ s32 count;        /* particles drawn (halved per view in split-screen) */
    /* 0x2A8 */ s32 flags;        /* bit 0: fade near y = 0 (water level); bit 1: stay above the ground */
    /* 0x2AC */ f32 size;
    /* 0x2B0 */ s32 enabled;
    /* 0x2B4 */ s32 unk2B4[3];
} EftWeather; /* size 0x2C0 */

/* The stage's weather record (BtlStage_GetList60). */
typedef struct StgWeatherRec {
    /* 0x00 */ f32 wind[3];
    /* 0x0C */ f32 size;
    /* 0x10 */ s32 flags; /* bit 0: weather on */
    /* 0x14 */ u16 ptclFlags;
    /* 0x16 */ u16 count;
} StgWeatherRec;

/* ---- stage effect manager (layer 0 of the effect scene) ---- */

#define EFT_STAGE_FLAG_DRAW 2 /* EftStage_SetDrawOn */
#define EFT_STAGE_FLAG_SUN 4  /* stage pack entry 0x14 exists (one sprite texture) */

typedef struct EftStage {
    /* 0x00 */ void *list;  /* child list, one task per stage effect kind */
    /* 0x04 */ void *task;  /* the layer-0 task */
    /* 0x08 */ s32 flags;   /* EFT_STAGE_FLAG_* */
    /* 0x0C */ s32 unkC;
    /* 0x10 */ EftTexEntry sprite; /* filled by EftTexSet_Load4 from pack entry 0x14 */
    /* 0x20 */ u8 unk20[0x38];
} EftStage; /* size 0x58 */

typedef struct EftStageKind {
    /* 0x00 */ EftTaskClass *cls;
    /* 0x04 */ s32 kind; /* EftStage_HasKind */
} EftStageKind;

/* ---- surfaces ---- */

/* Texture record of the surface file. */
typedef struct EftSurfTex {
    /* 0x00 */ s32 ofs0; /* file offsets of two blocks */
    /* 0x04 */ s32 ofs4;
    /* 0x08 */ u8 unk8[0x20];
    /* 0x28 */ u64 tex0;  /* written every frame by EftSurf_UpdateTextures */
    /* 0x30 */ u64 unk30;
    /* 0x38 */ s32 ptr0;  /* ofs0 / ofs4 as pointers */
    /* 0x3C */ s32 ptr4;
} EftSurfTex; /* size 0x40 */

#define EFT_SURF_NO_DEPTH_TEST 0x8 /* drawn in layer 1 */
#define EFT_SURF_LAYER2 0x10       /* drawn in layer 2 (else 0) */
#define EFT_SURF_HIDDEN 0x60   /* either bit: not drawn */
#define EFT_SURF_WAVE 0x80     /* u wobbles with a sine, v scrolls */
#define EFT_SURF_PULSE 0x100   /* like WAVE, and unk18 bounces between 0.95 and 1 */
#define EFT_SURF_NEAR_ONLY 0x200 /* fades between 200 and 400 units from the camera */
#define EFT_SURF_CLIP 0x400
#define EFT_SURF_REFLECT 0x800 /* uses the rendered reflection textures */

/* One surface: a list of triangles sharing a texture and a scroll. */
typedef struct EftSurfMesh {
    /* 0x00 */ s32 tex;      /* index into EftSurfFile.texs */
    /* 0x04 */ s32 triCount;
    /* 0x08 */ struct EftSurfTri *tris;
    /* 0x0C */ f32 du;       /* added to u per frame */
    /* 0x10 */ f32 dv;
    /* 0x14 */ f32 u;
    /* 0x18 */ f32 v;
    /* 0x1C */ s32 zBias;
    /* 0x20 */ s32 flags;    /* EFT_SURF_* */
    /* 0x24 */ s32 unk24[3];
    /* 0x30 */ Vec4 boxMin;
    /* 0x40 */ Vec4 boxMax;
    /* 0x50 */ Vec4 center;
} EftSurfMesh; /* size 0x60 */

typedef struct EftSurfVtx {
    /* 0x00 */ EftFVec pos;
    /* 0x10 */ EftFVec st;    /* s, t, q, ? */
    /* 0x20 */ EftFVec color; /* r, g, b, alpha */
} EftSurfVtx; /* size 0x30 */

/* A projected vertex: GS screen coordinates (12.4 fixed point) and depth. */
typedef struct EftScreenPos {
    /* 0x00 */ s32 x;
    /* 0x04 */ s32 y;
    /* 0x08 */ s32 z;
    /* 0x0C */ s32 w;
} EftScreenPos; /* size 0x10 */

typedef struct EftSurfTri {
    /* 0x00 */ EftSurfVtx v[3];
} EftSurfTri; /* size 0x90 */

typedef struct EftSurfFile {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s32 version; /* 4 */
    /* 0x08 */ s32 unk8;
    /* 0x0C */ s32 unkC;    /* offset, made a pointer */
    /* 0x10 */ s32 texCount;
    /* 0x14 */ EftSurfTex *texs;
    /* 0x18 */ s32 meshCount;
    /* 0x1C */ EftSurfMesh *meshes;
} EftSurfFile;

/* Parameters of the reflecting surface: the first 0x40 bytes of stage pack entry 0xD, overridden by the
   stage's record (BtlStage_GetList58). */
typedef struct EftSurfParam {
    /* 0x00 */ f32 colorA[4];
    /* 0x10 */ f32 colorB[4];
    /* 0x20 */ Vec4 lightDir;
    /* 0x30 */ f32 unk30;
    /* 0x34 */ s32 animSpeed;  /* animSpeed / animCount is added to the frame accumulator each update */
    /* 0x38 */ f32 fadeDist;   /* 1000 when the data says 0 */
    /* 0x3C */ f32 unk3C;
} EftSurfParam; /* size 0x40 */

/* The stage's record for the reflecting surface (BtlStage_GetList58). */
typedef struct StgSurfRec {
    /* 0x00 */ f32 lightDir[3];
    /* 0x0C */ f32 unkC;
    /* 0x10 */ f32 fadeDist;
    /* 0x14 */ u16 animSpeed;
    /* 0x16 */ u8 unk16;
    /* 0x17 */ u8 colorA[4];
    /* 0x1B */ u8 colorB[4];
} StgSurfRec;

/* A 256-colour palette. The reflecting surface is an 8-bit texture whose palette holds normals; lighting the
   256 palette entries gives the palettes the surface is drawn with. */
typedef struct EftSurfClut {
    /* 0x00 */ u32 color[0x100];
} EftSurfClut; /* size 0x400 */

/* Work buffer set up by GfxClut_InitPacket. */
typedef struct EftSurfBuf {
    /* 0x000 */ u8 unk0[0x490];
    /* 0x490 */ EftSurfClut *clut;
    /* 0x494 */ u8 unk494[0x2C];
} EftSurfBuf; /* size 0x4C0 */

typedef struct EftTexData {
    /* 0x00 */ u8 unk0[0x60];
    /* 0x60 */ EftSurfClut clut;
} EftTexData;

typedef struct EftTexDef {
    /* 0x00 */ u8 unk0[0x3C];
    /* 0x3C */ EftTexData *data;
} EftTexDef;

/* Texture table filled by EftTexSet_Load34. */
typedef struct EftSurfTexTbl {
    /* 0x000 */ EftTexEntry tex[34];
    /* 0x220 */ s32 count;
    /* 0x224 */ s32 unk224;
} EftSurfTexTbl; /* size 0x228 */

#define EFT_SURF_RT_READY 0x1   /* buffers and textures are set up */
#define EFT_SURF_RT_STEPPED 0x2 /* the animation advanced this frame */
#define EFT_SURF_RT_BUILT 0x4   /* the palettes were built this frame */
#define EFT_SURF_RT_ON 0x8

/* Reflection renderer: gEftSurf + 0x240. */
typedef struct EftSurfRt {
    /* 0x0000 */ EftSurfBuf buf[3];   /* [0] normals of the current frame, [1] / [2] the two lit palettes */
    /* 0x0E40 */ Vec4 light;          /* light direction in the surface's frame */
    /* 0x0E50 */ EftSurfTexTbl tex;   /* stage pack entry 0xA */
    /* 0x1078 */ EftTexEntry *texTbl; /* = tex.tex */
    /* 0x107C */ s32 unk107C;
    /* 0x1080 */ EftSurfParam param;
    /* 0x10C0 */ Mtx44 frame;         /* built by EftMath_CalcTangentFrame from two triples of points */
    /* 0x1100 */ s32 animFrame;       /* 0..animCount-1: which normal map */
    /* 0x1104 */ s32 animCount;       /* tex.count - 4 */
    /* 0x1108 */ s32 texA;            /* tex.count - 4: first of two alternating result textures */
    /* 0x110C */ s32 texB;            /* tex.count - 2 */
    /* 0x1110 */ s32 flip;            /* 0 / 1, toggled each time the palettes are built */
    /* 0x1114 */ s32 flags;           /* EFT_SURF_RT_* */
    /* 0x1118 */ f32 animAcc;
} EftSurfRt;

typedef struct EftSurfSlot {
    /* 0x00 */ u64 unk0;
    /* 0x08 */ EftSurfTex *def;
    /* 0x0C */ s32 unkC;
} EftSurfSlot; /* size 0x10 */

typedef struct EftSurf {
    /* 0x0000 */ EftSurfParam *param; /* stage pack entry 0xD */
    /* 0x0004 */ EftSurfFile *file;   /* param + 1 */
    /* 0x0008 */ union {
        struct {
            /* 0x0008 */ s32 size;
            /* 0x000C */ s32 unkC;
            /* 0x0010 */ Vec4 *planes; /* five clip planes (EftGfx_GetClipPlanes) */
            /* 0x0014 */ s32 unk14;
        } s;
        /* 0x0008 */ EftSurfSlot slot[35]; /* [i + 1] belongs to texture i of the file */
    } u;
    /* 0x0238 */ s32 unk238[2];
    /* 0x0240 */ EftSurfRt rt;
    /* 0x1360 */ u8 unk1360[0x20];
} EftSurf; /* size 0x1380 */

extern EftWeather *gEftWeather;
extern EftStage *gEftStage;
extern EftSurf *gEftSurf;

s32 EftGeyser_GetSmokePos(EftTask *task);
s32 EftGeyser_GetSteamWork(EftTask *task);
void EftGeyser_StartSmoke(EftTask *task);
void EftGeyser_StartSteam(EftTask *task);

s32 EftWeather_ClipCode(Vec4 *v);
void EftWeather_Init(EftTask *task);
void EftWeather_Term(void);
void EftWeather_Update(void);
void EftWeatherPtcl_Init(EftTask *task);
void EftWeatherPtcl_Term(void);
void EftWeather_Move(EftWeatherPtcl *p, EftWeatherView *view, s32 count);
void EftWeatherPtcl_Update(void);
void EftWeatherPtcl_PostUpdate(void);
void EftWeatherPtcl_Reset(EftTask *task);
void EftWeather_DrawPtcls(EftWeatherPtcl *p, s32 count);
void EftWeatherPtcl_Draw(EftTask *task);
void EftWeather_SetCount(s32 count);
s32 EftWeather_GetCount(void);
void EftWeather_SetFlags(s32 flags);
s32 EftWeather_GetFlags(void);
Vec4 *EftWeather_GetWindPtr(void);
void EftWeather_SetSize(f32 size);
f32 EftWeather_GetSize(void);
void EftWeather_SetColor(u8 r, u8 g, u8 b);
void EftWeather_GetColor(s32 *out);
void EftWeather_SetWind(f32 x, f32 y, f32 z);
void EftWeather_GetWind(Vec4 *out);
void EftWeather_SetCount2(s32 count);
s32 EftWeather_GetCount2(void);
void EftWeather_SetFlags2(s32 flags);
s32 EftWeather_GetFlags2(void);
void EftWeather_SetEnabled(s32 on);

void EftStage_Recreate(void);
void EftStage_ResetAll(void);
s32 EftStage_IsDrawOn(void);
void EftStage_SetDrawOn(s32 on);
void EftStage_Init(EftTask *task);
void EftStage_Term(void);
void EftStage_Update(void);
void EftStage_FreeKinds(void);
void EftStage_CreateKinds(void);
s32 EftStage_HasKind(s32 kind);
void EftStage_Nop(void);
f32 EftStage_GetTintScale(void);
s32 EftStage_LoadSprite(void);
void EftStage_UpdateSprite(void);
void EftStage_DrawSprite(Vec4 *pos);

void EftSurf_Init(void);
void EftSurf_Term(void);
void EftSurf_Update(void);
void EftSurf_Draw(void);
void EftSurf_PostUpdate(void);
void EftSurf_Reset(void);
void EftSurf_Relocate(EftSurfFile *file);
void EftSurf_Unrelocate(EftSurfFile *file);
void EftSurf_UpdateTextures(EftSurfFile *file);
void EftSurf_Animate(EftSurfFile *file);
s32 EftSurf_IsBoxVisible(Vec4 *min, Vec4 *max);
void EftSurf_DrawMeshes(void);
void EftSurf_PutTri(s32 *xyz0, s32 *xyz1, s32 *xyz2, Vec4 *st0, Vec4 *st1, Vec4 *st2, Vec4 *rgba0, Vec4 *rgba1,
                    Vec4 *rgba2, u64 tex0);
void EftSurf_BeginDraw(s32 blend, s32 noDepthWrite);
void EftSurf_EndDraw(s32 arg);
void EftSurf_DrawTri(EftSurfVtx *tri, u64 tex0);
void EftSurf_DrawTriClipped(EftSurfVtx *tri, u64 tex0);
void EftSurf_DrawReflectTri(EftSurfVtx *tri, u64 *tex, Vec4 *fog);
void EftSurf_DrawReflectTriClipped(EftSurfVtx *tri, u64 *tex, Vec4 *fog);
void EftSurf_DrawTriOtClipped(EftSurfVtx *tri, s32 layer, s32 unused, s32 flipZ, u64 tex0, s32 zBias);
void EftSurf_BuildPalettes(void);
void EftSurf_StepAnim(void);
void EftSurf_RenderPalettes(void);

#endif
