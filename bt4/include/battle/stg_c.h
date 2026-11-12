#ifndef BATTLE_STG_C_H
#define BATTLE_STG_C_H

#include "types.h"
#include "sys/dma.h"
#include "sys/math3d.h"
#include "sys/ramp.h"

/*
 * Battle screen effects (the "stage effects" group). Source range 0x245F58-0x248F28.
 *
 * Nothing in this range is stage geometry, collision, a destructible object, debris or a rigid body: it is
 * the full-screen post effects the battle draws around the scene, and the manager that creates, resets and
 * draws the whole group (a dozen sub-systems; the others are at 0x102F28-0x10A5A0 and 0x2446B0-0x245F58).
 * Every function only stores drawing parameters or builds GS packets; none reads or writes a fighter, a hit
 * record or a battle object.
 *
 *   0x245F58-0x245FB8  StgFog_*    three accessors of the depth fog filter of the previous module (object at
 *                                   0x2FEC08); names are guesses
 *   0x245FB8-0x2463B0  StgCurve_*  3-key Hermite curve sampled into a 256-entry table (depth -> strength),
 *                                   shared by the depth fog and the depth haze
 *   0x2463B0-0x2473C8  StgHaze_*   depth haze (heat shimmer): the frame is redrawn over itself through a
 *                                   jittering mesh, masked by a depth -> alpha look-up; one per view
 *   0x2473C8-0x247778  StgFx_*     the group manager: init / term / reset and the four draw passes
 *   0x247778-0x247D98  StgTint_*   four full-screen colour layers with linear ramps; slot 0 is the darkening
 *                                   the fighter effect layer starts with request 0x12
 *   0x247D98-0x248F28  StgBlur_*   radial ("zoom") blur of the frame, one per view
 *
 * Callers (src/battle/battle.c): StgFx_Init / StgFx_Term / StgFx_Reset with the battle; inside Battle_Draw
 * and Battle_DrawSplit, in this order:
 *   stage draw, StgFx_DrawPre, BtlObjDraw_Draw, StgFx_DrawNop, BtlScene_Draw + Ot_Draw (per view),
 *   StgFx_DrawPost, BtlGame_Draw (HUD), 0x23A2B8, 0x23D1E0, StgFx_DrawOverlay.
 * The group has no update function: the tint ramps (StgTint_Update) and the haze jitter (StgHaze_Step)
 * advance inside StgFx_DrawPre, once per drawn frame, and test BATTLE_FLAG_PAUSE themselves.
 * StgFx_SetDisabled(1) (stage loading, battle_load.c) makes every pass return at once.
 */

/* One key of a StgCurve. Only x and z of `pos` are used: x = input (0..255), z = output (0..255). */
typedef struct StgCurveKey {
    /* 0x00 */ Vec4 pos;
    /* 0x10 */ Vec4 tanOut; /* towards the next key; written by StgCurve_Build for the middle key only */
    /* 0x20 */ Vec4 tanIn;  /* towards the previous key; same */
    /* 0x30 */ Vec4 unk30;
} StgCurveKey; /* size 0x40 */

/* Three keys; StgCurve_Build turns them into table[0..255] (0..1). */
typedef struct StgCurve {
    /* 0x00 */ StgCurveKey key[3];
    /* 0xC0 */ s32 count; /* table entries written so far (scratch of StgCurve_Build) */
    /* 0xC4 */ s32 padC4[3];
} StgCurve; /* size 0xD0 */

#define STG_HAZE_COLS_MAX 20
#define STG_HAZE_ROWS_MAX 50

/* Depth haze of one view. gStgHaze points at two of them (0x2B00 bytes). */
typedef struct StgHaze {
    /* 0x0000 */ u8 packet[0x490];  /* GS packet template built by GfxClut_InitPacket (not in this range) */
    /* 0x0490 */ u8 *clut;          /* 256 RGBA entries inside the template: depth byte -> alpha */
    /* 0x0494 */ u8 upload[0x20];   /* start of the part sent with Dma_AddData(.., 0x10) */
    /* 0x04B4 */ u16 clutBase;      /* GS block of the CLUT (argument of StgHaze_Create) */
    /* 0x04B6 */ u8 pad4B6[0xA];
    /* 0x04C0 */ s8 targetX[STG_HAZE_ROWS_MAX][STG_HAZE_COLS_MAX]; /* random offsets, 1/16 pixel */
    /* 0x08A8 */ s8 targetY[STG_HAZE_ROWS_MAX][STG_HAZE_COLS_MAX];
    /* 0x0C90 */ s8 curX[STG_HAZE_ROWS_MAX][STG_HAZE_COLS_MAX];    /* follows targetX */
    /* 0x1078 */ s8 curY[STG_HAZE_ROWS_MAX][STG_HAZE_COLS_MAX];
    /* 0x1460 */ s32 cols;          /* mesh columns (20); halved in split screen */
    /* 0x1464 */ s32 rows;          /* mesh rows (50) */
    /* 0x1468 */ s32 ampX;          /* jitter range, +-ampX sixteenths of a pixel (128) */
    /* 0x146C */ s32 ampY;
    /* 0x1470 */ u32 color;         /* RGBA of the redrawn frame (0x80808080) */
    /* 0x1474 */ s32 period;        /* new random targets every `period` frames (1) */
    /* 0x1478 */ s32 frame;         /* frames drawn while not paused */
    /* 0x147C */ f32 follow;        /* cur += (target - cur) * follow each frame */
    /* 0x1480 */ StgCurve curve;    /* depth -> strength */
    /* 0x1550 */ u8 pad1550[0x30];
} StgHaze; /* size 0x1580 */

/* Haze section of the stage parameter file (returned by BtlStage_GetListA0). */
typedef struct StgHazeParam {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ u8 color[4];
    /* 0x08 */ u8 key[6]; /* x0, z0, x1, z1, x2, z2 of the curve */
    /* 0x0E */ u8 cols;
    /* 0x0F */ u8 rows;
    /* 0x10 */ u8 ampX;
    /* 0x11 */ u8 ampY;
    /* 0x12 */ u8 period;
    /* 0x13 */ u8 pad13;
    /* 0x14 */ f32 follow;
} StgHazeParam;

/* Packet cursor of one StgHaze_Draw call. As with StgBlurDraw, only the pointer is in memory; what
   used to be listed here as further members (haze, width, widthPx, xOffset, cols, y0, y1, v0, nextRow,
   v1, lastRow, nVerts at +4 .. +0x30) are StgHaze_Draw's ordinary locals in their spill slots. */
typedef struct StgHazeDraw {
    /* 0x00 */ GsQword *p;
} StgHazeDraw;

/* StgTint.flags */
#define STG_TINT_ON 1
#define STG_TINT_OUT 2  /* started with dir 0 (colour A -> colour B) */
#define STG_TINT_IN 4   /* started with dir 1 (back to colour A): the slot is released when the ramp ends */
#define STG_TINT_DONE 8 /* a STG_TINT_IN ramp ended; StgTint_Release turns the slot off */

/* One full-screen colour layer; the same layout as Fade (sys/fade.h) with byte flags. */
typedef struct StgTint {
    /* 0x00 */ Vec4 color; /* from + delta * ramp.value; r, g, b 0..255, alpha 0..128 */
    /* 0x10 */ Vec4 from;
    /* 0x20 */ Vec4 to;
    /* 0x30 */ Vec4 delta;
    /* 0x40 */ Ramp ramp;  /* 0 -> 1 */
    /* 0x58 */ u8 flags;
    /* 0x59 */ u8 pad59[7];
} StgTint; /* size 0x60 */

#define STG_TINT_COUNT 4

typedef struct StgTintWork {
    /* 0x000 */ StgTint slot[STG_TINT_COUNT];
    /* 0x180 */ s32 unk180;
    /* 0x184 */ s32 pad184[3];
} StgTintWork; /* size 0x190 */

/* One end colour of a slot. The table of StgTint_Start (0x2F2370, [slot][dir]) is copied with 64-bit moves,
   so the type is 8- or 16-byte aligned in the original. */
typedef struct StgTintColor {
    f32 x, y, z, w;
} __attribute__((aligned(16))) StgTintColor;

/* Radial blur of one view. gStgBlur points at two of them (0xA0 bytes). */
typedef struct StgBlur {
    /* 0x00 */ s32 unk0;     /* cleared by StgBlur_Reset, not read */
    /* 0x04 */ s32 pad4[3];
    /* 0x10 */ Vec4 center;  /* unit vector; screenSpace != 0: x, y used as they are, else rotated by the view first */
    /* 0x20 */ f32 scale;    /* growth per pass (2.0) */
    /* 0x24 */ s32 passes;   /* 3 */
    /* 0x28 */ f32 shiftX;   /* 2.0 */
    /* 0x2C */ f32 shiftY;   /* 2.0 */
    /* 0x30 */ u64 color01;  /* low word: colour at the blur centre; high word: colour of the middle ring */
    /* 0x38 */ u32 color2;   /* colour at the screen edge. The blur is off while all three alphas are 0 */
    /* 0x3C */ u32 color3;   /* colour of the second draw of each blur pass */
    /* 0x40 */ s32 screenSpace;
    /* 0x44 */ s32 pad44[3];
} StgBlur; /* size 0x50 */

/* Packet cursor of one StgBlur_Draw call. Only the pointer is in memory: StgBlur_Draw keeps the view's
   rectangle in ordinary locals (the matching C verifies it: they are spilled registers, reloaded by the
   compiler where it pleases, which a structure member would not be). */
typedef struct StgBlurDraw {
    /* 0x00 */ GsQword *p;
} StgBlurDraw;

void StgFog_SetColor(s32 r, s32 g, s32 b, s32 a);
StgCurve *StgFog_GetCurve(void);
void StgFog_RebuildClut(void);

f32 StgCurve_AddSegment(StgCurve *curve, f32 *table, StgCurveKey *k0, StgCurveKey *k1, s32 steps);
void StgCurve_Build(StgCurve *curve, f32 *table);

void StgHaze_BeginDraw(StgHazeDraw *d, StgHaze *haze, s32 x0, s32 x1, s32 width, s32 w, s32 srcW, s32 h);
void StgHaze_EndDraw(StgHazeDraw *d);
void StgHaze_PutStripTag(StgHazeDraw *d, StgHaze *haze, s32 nVerts);
void StgHaze_EndStrip(StgHazeDraw *d);
void StgHaze_PutVertex(StgHazeDraw *d, s32 u, s32 v, s32 x, s32 y);
void StgHaze_Create(StgHaze *haze, u16 clutBase);
void StgHaze_BuildClut(StgHaze *haze, StgCurve *curve, f32 scale);
void StgHaze_Init(u16 clutBase);
void StgHaze_Term(void);
void StgHaze_Reset(void);
void StgHaze_UpdateClut(StgHaze *haze, s32 split, s32 view);
void StgHaze_Step(StgHaze *haze, s32 split, s32 view);
void StgHaze_Draw(StgHaze *haze, s32 split, s32 view);
void StgHaze_DrawView(s32 split, s32 view);
void StgHaze_DrawAll(void);
void StgHaze_SetParams(StgHazeParam *param);
void StgHaze_SetGrid(s32 view, s32 cols, s32 rows);
void StgHaze_SetAmplitude(s32 view, s32 ampX, s32 ampY);
void StgHaze_SetColor(s32 view, s32 r, s32 g, s32 b, s32 a);
void StgHaze_SetSpeed(s32 view, s32 period, f32 follow);
StgCurve *StgHaze_GetCurve(s32 view);
void StgHaze_RebuildClut(s32 view);

void StgFx_SetDisabled(s32 disabled);
void StgFx_Reset(void);
void StgFx_Init(void);
void StgFx_Term(void);
void StgFx_DrawPre(void);
void StgFx_DrawNop(void);
void StgFx_DrawPost(void);
void StgFx_DrawOverlay(void);
void StgFx_DrawPreNoCheck(void);

void StgTint_CalcColor(StgTint *tint);
void StgTint_Reset(void);
void StgTint_Init(void);
void StgTint_Update(void);
void StgTint_Release(void);
void StgTint_DrawBack(void);
void StgTint_Nop(void);
void StgTint_DrawFront(void);
void StgTint_Start(u32 slot, s32 dir, f32 seconds);
void StgTint_GetColor0(s32 *out);
StgTintWork *StgTint_GetWork(void);
s32 StgTint_IsOn(u32 slot);

void StgBlur_BeginDraw(StgBlurDraw *d, s32 x0, s32 x1, s32 width, s32 h, s32 srcW, s32 srcH);
void StgBlur_EndDraw(StgBlurDraw *d);
void StgBlur_PutStripTag(StgBlurDraw *d, s32 nVerts, s32 unused);
void StgBlur_Nop(StgBlurDraw *d);
void StgBlur_Init(void);
void StgBlur_Term(void);
void StgBlur_Reset(void);
void StgBlur_Draw(s32 split, s32 view, Mtx44 *viewMtx);
void StgBlur_DrawAll(void);
void StgBlur_SetPasses(s32 view, s32 passes);
void StgBlur_SetScale(s32 view, f32 scale, f32 shiftX, f32 shiftY);
void StgBlur_SetCenter(s32 view, Vec4 *center, s32 screenSpace);
void StgBlur_SetColor0Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
void StgBlur_SetColor0(s32 view, u32 rgba);
void StgBlur_SetColor1Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
void StgBlur_SetColor1(s32 view, u32 rgba);
void StgBlur_SetColor2Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
void StgBlur_SetColor2(s32 view, u32 rgba);
void StgBlur_SetColor3Rgba(s32 view, u8 r, u8 g, u8 b, u8 a);
void StgBlur_SetColor3(s32 view, u32 rgba);

#endif
