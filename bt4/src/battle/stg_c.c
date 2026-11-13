#include "common.h"
#include "battle/battle.h"
#include "battle/btl_cam.h"
#include "battle/btl_demo_cam.h"
#include "battle/stg_c.h"
#include "sys/dma.h"
#include "sys/fade.h"
#include "sys/gfx.h"
#include "sys/heap.h"
#include "sys/math3d.h"
#ifdef PORT
#include "menu/tbo_art.h"
#endif
#include "sys/ramp.h"
#include "sys/randf.h"

/*
 * Battle screen effects, 0x245F58..0x248F28. See include/battle/stg_c.h for the overview.
 * Everything here is drawing: no function reads or writes a fighter, a hit record, a rigid body or
 * stage collision.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern void Vec4_Copy(Vec4 *dst, Vec4 *src);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Set(Vec4 *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern f32 Vec3_Length(Vec4 *v);
extern void Vec3_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Mtx_MulVec4(Vec4 *dst, Mtx44 *m, Vec4 *v);

/* The depth fog filter of the previous module (0x245370..0x245F58); only the fields used here. */
typedef struct StgFogView {
    /* 0x000 */ u8 unk0[0x500];
    /* 0x500 */ StgCurve curve;
    /* 0x5D0 */ u32 color;
    /* 0x5D4 */ s32 clearLast; /* the last CLUT entry is forced to 0 */
} StgFogView;

extern StgFogView *gStgFog;
extern void StgFog_BuildClut(StgFogView *fog, StgCurve *curve, s32 clearLast, f32 scale);

/* Stage parameter file readers (0x242B78..0x242F18, named in config/symbols/stg_a.txt; declared here with
   the types this file needs). */
extern void *BtlStage_GetList50(void);        /* fog section, or NULL */
extern StgHazeParam *BtlStage_GetListA0(void); /* haze section, or NULL */
extern u8 *BtlStage_GetList80(void);          /* tint section: two RGBA byte colours, or NULL */
extern s32 BtlStage_HasFeature(s32 feature);   /* 1 when the stage file turns the feature on */
extern f32 BtlStage_GetTop(void);        /* stage word +4 of the stage block's +0x40 table (see stg_a) */

/* Other members of the group (0x102F28..0x10A5A0, 0x2446B0..0x245F58). */
extern void StgPanBlur_Init(s32 base);
extern void StgPanBlur_Draw(void);
extern void StgPanBlur_Term(void);
extern void StgGlare_Update(void);
extern void StgGlare_Reset(void);
extern void StgGlare_Init(u16 base);
extern void StgGlare_Term(void);
extern void StgGlare_Draw(void);
extern void StgGlare_LoadParams(void);
extern void StgDepthTint_Rebuild(void);
extern void StgDepthTint_Init(u16 base);
extern void StgDepthTint_Term(void);
extern void StgDepthTint_Draw(void);
extern void StgDepthTint_LoadParams(void);
extern void GfxPost_InitNop(void);
extern void GfxPost_TermNop(void);
extern void GfxPost_DrawDepthClut(s32 mode, s32 base, s32 clutBase, u64 alpha);
extern void GfxPost_DrawTintRect(u64 alpha, Vec4 *color); /* blended full-screen rectangle */
extern void GfxLens_Init(void);
extern void GfxLens_Stub107758(void);
extern void GfxLens_Clear(void);
extern void GfxLens_DrawFull(void);
extern void GfxWater_Init(void);
extern void GfxWater_Term(void);
extern void GfxWater_LoadStageColor(void);
extern void GfxWater_Draw(void);
extern void GfxDepthFog_Init(u16 base);
extern void GfxDepthFog_Draw(void);
extern void GfxClut_InitPacket(StgHaze *haze, u16 clutBase);
extern void Mtx_Copy(Mtx44 *dst, Mtx44 *src);
extern void Mtx_ClearTrans(Mtx44 *m);
extern void ScrXfade_Init(void);
extern void ScrXfade_Term(void);
extern void ScrXfade_Reset(void);
extern void ScrXfade_Update(void);
extern void ScrXfade_PostDraw(void);
extern void ScrWarp_Init(void);
extern void ScrWarp_Term(void);
extern void ScrWarp_Reset(void);
extern void ScrWarp_Update(void);
extern void StgFog_Init(s32 base);
extern void StgFog_Term(void);
extern void StgFog_ResetColor(void);
extern void StgFog_Draw(void);
extern void StgFog_SetParams(void *param);

/* 0x2FEC20: the two depth haze views. */
extern StgHaze *gStgHaze;
/* 0x2FEC28: the two radial blur views. */
extern StgBlur *gStgBlur;
/* 0x2FF208: the group's one word of state: non-zero = every draw pass is skipped. */
extern s32 *gStgFx;
/* 0x31C4A0: the four colour layers. */
extern StgTintWork gStgTint;
/* 0x31C630: slot 0's colour as integers (read by the effect code at 0x136FB8 / 0x137F60). */
extern s32 gStgTintColor0[4];

/* Sets the fog colour word. */
void StgFog_SetColor(s32 r, s32 g, s32 b, s32 a) {
    gStgFog->color = r | (g << 8) | (b << 16) | (a << 24);
}

/* Returns the fog's depth curve. */
StgCurve *StgFog_GetCurve(void) {
    return &gStgFog->curve;
}

/* Rebuilds the fog look-up from its curve at full strength. */
void StgFog_RebuildClut(void) {
    StgFogView *fog = gStgFog;

    StgFog_BuildClut(fog, &fog->curve, fog->clearLast, 1.0f);
}

/* Samples the Hermite segment k0 -> k1 at steps + 1 points into table[curve->count...]; returns the last value (0..255). */
f32 StgCurve_AddSegment(StgCurve *curve, f32 *table, StgCurveKey *k0, StgCurveKey *k1, s32 steps) {
    Vec4 p0;
    Vec4 p1;
    Vec4 t0;
    Vec4 t1;
    Vec4 out;
    s32 i;

    Vec4_Copy(&p0, &k0->pos);
    Vec4_Copy(&p1, &k1->pos);
    Vec4_Copy(&t0, &k0->tanOut);
    Vec4_Copy(&t1, &k1->tanIn);
    t0.x = -t0.x;
    t0.y = -t0.y;
    t0.z = -t0.z;
    for (i = 0; i <= steps; i++) {
        Vec3_Hermite(&out, &p0, &p1, &t0, &t1, (f32)i / (f32)steps);
        if (out.x < 0.0f) {
            out.x = 0.0f;
        }
        if (out.x > 255.0f) {
            out.x = 255.0f;
        }
        if (out.z < 0.0f) {
            out.z = 0.0f;
        }
        if (out.z > 255.0f) {
            out.z = 255.0f;
        }
        table[curve->count] = out.z / 255.0f;
        if (curve->count < 0xFF) {
            curve->count++;
        }
    }
    return out.z;
}

/* Computes the middle key's tangents and fills table[0..255] with the curve (0..1). */
void StgCurve_Build(StgCurve *curve, f32 *table) {
    Vec4 d;
    Vec4 t;
    s32 i;
    f32 last;
    StgCurveKey *key;

    key = curve->key;
    for (i = 0; i < 3; i++) {
        if (i != 0) {
            if (i != 2) {
            StgCurveKey *next = &curve->key[i + 1];
            StgCurveKey *prev = &curve->key[i - 1];
            Vec4_Sub(&d, &next->pos, &curve->key[i].pos);
            Vec4_Sub(&t, &next->pos, &prev->pos);
            Vec3_Normalize(&t, &t);
            Vec3_Scale(&t, &t, Vec3_Length(&d));
            Vec4_Copy(&curve->key[i].tanIn, &t);
            Vec4_Sub(&d, &prev->pos, &curve->key[i].pos);
            Vec4_Sub(&t, &prev->pos, &next->pos);
            Vec3_Normalize(&t, &t);
            Vec3_Scale(&t, &t, Vec3_Length(&d));
            Vec4_Copy(&curve->key[i].tanOut, &t);
            }
        }
    }
    curve->count = 0;
    for (i = 0; i < (s32)curve->key[0].pos.x; i++) {
        table[i] = 0.0f;
        curve->count++;
    }
    for (i = 0; i < 2; i++) {
        s32 steps;

        if (i == 0) {
            steps = (s32)curve->key[1].pos.x - (s32)curve->key[0].pos.x;
        } else {
            steps = (s32)curve->key[2].pos.x - (s32)curve->key[1].pos.x;
        }
        last = StgCurve_AddSegment(curve, table, &curve->key[i], &curve->key[i + 1], steps);
    }
    for (i = curve->count; i < 0x100; i++) {
        table[i] = last / 255.0f;
    }
}

/* Uploads the haze look-up, writes depth as alpha into the frame, and opens the packet that redraws the frame. */
void StgHaze_BeginDraw(StgHazeDraw *d, StgHaze *haze, s32 x0, s32 x1, s32 width, s32 h, s32 tmpW, s32 tmpH) {
    Dma_AddData(haze->upload, 0x10);
    Dma_AddTexFlush();
    Dma_AddFrame((gGfx.frame & 1) ? 0 : 0x70, 8, 0xFFFFFF);
    Dma_AddZbuf(0xE0, 1);
    GfxPost_DrawDepthClut(0, 0x1C00, haze->clutBase, 0x44);
    Dma_AddZbuf(0xE0, 0);
    d->p = (GsQword *)Dma_BeginDirect();
    Gfx_PutDefaultEnv(&d->p);
    d->p->d[0] = GIF_TAG(6, 0, 1);
    d->p->d[1] = GIF_REG_AD;
    d->p++;
    d->p->d[0] = 0x30000;
    d->p->d[1] = GS_TEST_1;
    d->p++;
    d->p->d[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    d->p->d[1] = GS_ZBUF_1;
    d->p++;
    d->p->d[0] = !(gGfx.frame & 1) ? 0x264020E00 : 0x264020000;
    d->p->d[1] = GS_TEX0_1;
    d->p++;
    d->p->d[0] = 0x80150;
    d->p->d[1] = GS_FRAME_1;
    d->p++;
    d->p->d[0] = GS_SET_SCISSOR(0, tmpW - 1, 0, tmpH - 1);
    d->p->d[1] = GS_SCISSOR_1;
    d->p++;
    d->p->d[0] = GS_SET_CLAMP(2, 2, x0, x1 - 1, 0, h - 1);
    d->p->d[1] = GS_CLAMP_1;
    d->p++;
    Dma_PutTexStrips(&d->p, 0, 0, tmpW, tmpH, 0, 0, x0, 0, width + x0, h, 0, 0, 0x80808080, 0);
    d->p->d[0] = GIF_TAG(5, 0, 1);
    d->p->d[1] = GIF_REG_AD;
    d->p++;
    d->p->d[0] = 0x264022A00;
    d->p->d[1] = GS_TEX0_1;
    d->p++;
    d->p->d[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    d->p->d[1] = GS_FRAME_1;
    d->p++;
    d->p->d[0] = GS_SET_SCISSOR(x0, x1 - 1, 0, h - 1);
    d->p->d[1] = GS_SCISSOR_1;
    d->p++;
    d->p->d[0] = GS_SET_CLAMP(2, 2, 0, tmpW - 1, 0, tmpH - 1);
    d->p->d[1] = GS_CLAMP_1;
    d->p++;
    d->p->d[0] = 0x54;
    d->p->d[1] = GS_ALPHA_1;
    d->p++;
}

/* Closes the packet opened by StgHaze_BeginDraw. */
void StgHaze_EndDraw(StgHazeDraw *d) {
    Dma_EndDirect((u64 *)d->p);
}

/* Starts one textured triangle strip of nVerts vertices in the haze colour. */
void StgHaze_PutStripTag(StgHazeDraw *d, StgHaze *haze, s32 nVerts) {
    d->p->d[0] = 0x2400000000000001;
    d->p->d[1] = 0x10;
    d->p++;
    d->p->d[0] = 0x154;
    d->p->d[1] = (u64)haze->color | ((u64)0xFE00 << 46);
    d->p++;
    d->p->d[0] = (u64)nVerts | ((u64)0x9000 << 46);
    d->p->d[1] = 0x53;
    d->p++;
}

/* Does nothing (end of a strip). */
void StgHaze_EndStrip(StgHazeDraw *d) {
}

/* Appends one strip vertex: texel (u, v) in pixels, position (x, y) in sixteenths. */
void StgHaze_PutVertex(StgHazeDraw *d, s32 u, s32 v, s32 x, s32 y) {
    d->p->d[0] = (u64)(x + 8) | ((u64)(y + 8) << 16);
    d->p->d[1] = (u64)((u << 4) + 0x7000) | ((u64)((v << 4) + 0x7200) << 16);
    d->p++;
}

/* Builds the packet template of one haze view. */
void StgHaze_Create(StgHaze *haze, u16 clutBase) {
    GfxClut_InitPacket(haze, clutBase);
}

/* Fills the look-up: entry i (in CLUT storage order) = black with alpha curve(i) * 128 * scale. */
void StgHaze_BuildClut(StgHaze *haze, StgCurve *curve, f32 scale) {
    f32 table[0x100];
    u8 *clut;
    s32 i;

    clut = haze->clut;
    memset(table, 0, sizeof(table));
    StgCurve_Build(curve, table);
    for (i = 0; i < 0x100; i++) {
        s32 k = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);

        k *= 4;
        clut[k + 0] = 0;
        clut[k + 1] = 0;
        clut[k + 2] = 0;
        clut[k + 3] = (u32)(table[i] * 128.0f * scale);
    }
}

/* Allocates and creates the two haze views. */
void StgHaze_Init(u16 clutBase) {
    s32 i;

    gStgHaze = Heap_Alloc(sizeof(StgHaze) * 2, 0x40, 0, 2);
    memset(gStgHaze, 0, sizeof(StgHaze) * 2);
    for (i = 0; i < 2; i++) {
        StgHaze_Create(&gStgHaze[i], clutBase);
    }
    StgHaze_Reset();
}

/* Frees the haze views. */
void StgHaze_Term(void) {
    if (gStgHaze != NULL) {
        Heap_Free(gStgHaze);
        gStgHaze = NULL;
    }
}

/* Default parameters for both views. */
void StgHaze_Reset(void) {
    StgHaze *haze = gStgHaze;
    s32 i;

    for (i = 0; i < 2; i++, haze++) {
        haze->cols = 20;
        haze->rows = 50;
        haze->ampX = 0x80;
        haze->ampY = 0x80;
        haze->color = 0x80808080;
        haze->period = 1;
        haze->frame = 0;
        haze->follow = 0.1f;
    }
}

/* Rebuilds the look-up for the view's camera height: strength 1 - (cameraY / stageTop) * 0.5, so the haze
   is at full strength on the ground and at half strength at the top of the stage. */
void StgHaze_UpdateClut(StgHaze *haze, s32 split, s32 view) {
    f32 y;
    f32 top;

    if (gBtlCam == NULL) {
        y = gBtlCamView->pos.y;
    } else {
        y = gBtlCam->views[view].pos.y;
    }
    top = -BtlStage_GetTop();
    StgHaze_BuildClut(haze, &haze->curve, 1.0f - -y / top * 0.5f);
}

/* Draws new random targets every `period` frames and moves the mesh offsets towards them. */
void StgHaze_Step(StgHaze *haze, s32 split, s32 view) {
    s32 cols;
    s32 rows;
    s32 i;
    s32 j;
    s8 *tx;
    s8 *ty;
    s8 *cx;
    s8 *cy;

    cols = haze->cols;
    rows = haze->rows;
    if (split) {
        cols /= 2;
    }
    if (haze->frame % haze->period == 0) {
        for (i = 0; i < rows; i++) {
            tx = haze->targetX[i];
            ty = haze->targetY[i];
            for (j = 0; j < cols; j++) {
                *tx++ = Rand_IntRange(-haze->ampX, haze->ampX);
                *ty++ = Rand_IntRange(-haze->ampY, haze->ampY);
            }
        }
    }
    for (i = 0; i < rows; i++) {
        tx = haze->targetX[i];
        ty = haze->targetY[i];
        cx = haze->curX[i];
        cy = haze->curY[i];
        for (j = 0; j < cols; j++) {
            *cx += (s32)((f32)(*tx - *cx) * haze->follow);
            tx++;
            cx++;
            *cy += (s32)((f32)(*ty - *cy) * haze->follow);
            ty++;
            cy++;
        }
    }
    haze->frame++;
}

/*
 * Redraws the frame as rows - 1 triangle strips of cols * 2 vertices. Strip `row` spans the lines
 * row * 448 / (rows - 1) .. (row + 1) * 448 / (rows - 1) of the source (v) and half of that on the
 * target (y, in sixteenths; the target is one field, 224 lines); column `col` is at
 * col * width / (cols - 1). The top vertex is displaced by curX/curY[row][col], the bottom one by
 * curX/curY[row][col + 1] (the next column of the SAME row, not the next row), both clamped to the
 * view.
 * Matching notes: only the packet pointer is in memory (StgHazeDraw is one pointer). Everything the
 * original keeps at sp+4 .. sp+0x30 is a spilled pseudo register, in creation order: the parameter and
 * the declared locals first (haze, width, widthPx, xOffset, cols, y0, y1), then compiler temporaries in
 * the order the statements create them: the quotient of v0 (0x20), `row + 1` (0x24, first written in
 * the y1 statement, which therefore comes BEHIND v0), the quotient of v1 (0x28), and last the registers
 * that later passes create for `rows - 1` (0x2C) and `cols * 2` (0x30).
 * So there are no nextRow / lastRow / nVerts / divisor variables in the source: `rows - 1`, `row + 1`
 * and `cols * 2` are written out at every use. (v0 and v1 themselves disappear: a division always
 * lands in a temporary on this compiler and the copy is propagated.)
 */
void StgHaze_Draw(StgHaze *haze, s32 split, s32 view) {
    StgHazeDraw d;
    s32 width;
    s32 widthPx;
    s32 xOffset;
    s32 cols;
    s32 y0;
    s32 y1;
    s32 v0;
    s32 v1;
    u16 x1;
    s32 rows;
    s32 row;
    s32 col;
    s32 px;
    s32 u;
    s32 x;
    s32 y;
    s32 h = 0xE0;
    s32 srcH = 0x1C0;
    s8 *cx;
    s8 *cy;

    width = 0x200;
    widthPx = 0x200;
    xOffset = 0;
    cols = haze->cols;
    rows = haze->rows;
    x1 = 0x200;
    if (split) {
        width = 0x100;
        xOffset = (view == 1) ? 0x100 : 0;
        x1 = xOffset + 0x100;
        cols /= 2;
        widthPx = 0x100;
    }
    StgHaze_BeginDraw(&d, haze, xOffset, x1, width, srcH, widthPx, h);
    for (row = 0; row < rows - 1; row++) {
        StgHaze_PutStripTag(&d, haze, cols * 2);
        cx = haze->curX[row];
        cy = haze->curY[row];
        y0 = (h * row / (rows - 1)) << 4;
        v0 = srcH * row / (rows - 1);
        y1 = (h * (row + 1) / (rows - 1)) << 4;
        v1 = srcH * (row + 1) / (rows - 1);
        for (col = 0; col < cols; col++) {
            u = col * width / (cols - 1) + xOffset;
            px = (col * widthPx / (cols - 1)) << 4;
            x = px + cx[0];
            y = y0 + cy[0];
            x = (x < 0) ? 0 : ((x > widthPx << 4) ? widthPx << 4 : x);
            y = (y < 0) ? 0 : ((y > h << 4) ? h << 4 : y);
            StgHaze_PutVertex(&d, u, v0, x, y);
            x = px + cx[1];
            y = y1 + cy[1];
            x = (x < 0) ? 0 : ((x > widthPx << 4) ? widthPx << 4 : x);
            y = (y < 0) ? 0 : ((y > h << 4) ? h << 4 : y);
            StgHaze_PutVertex(&d, u, v1, x, y);
            cx++;
            cy++;
        }
        StgHaze_EndStrip(&d);
    }
    StgHaze_EndDraw(&d);
}

/* One view: look-up, jitter (not while paused), draw. */
void StgHaze_DrawView(s32 split, s32 view) {
    StgHaze *haze = &gStgHaze[view];

    StgHaze_UpdateClut(haze, split, view);
    if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
        StgHaze_Step(haze, split, view);
    }
    StgHaze_Draw(haze, split, view);
}

/* Draws the haze for whatever views are on screen. */
void StgHaze_DrawAll(void) {
    if (gBtlCam == NULL) {
        StgHaze_DrawView(0, 0);
        return;
    }
    if (Battle_IsSplitScreen()) {
        if (DemoCam_IsActive()) {
            StgHaze_DrawView(0, 0);
        } else if (gBtlCam->cur->split == 0) {
            StgHaze_DrawView(0, gBtlCam->cur->index);
        } else {
            StgHaze_DrawView(1, 0);
            StgHaze_DrawView(1, 1);
        }
    } else {
        StgHaze_DrawView(0, gBtlCam->cur->index);
    }
}

/* Loads the stage's haze parameters into both views (defaults when the stage has none). */
void StgHaze_SetParams(StgHazeParam *param) {
    StgHaze *haze;
    s32 i;
    u8 *key;

    haze = gStgHaze;
    if (param == NULL) {
        for (i = 0; i < 2; i++, haze++) {
            Vec4_Set(&haze->curve.key[0].pos, 0.0f, 0.0f, 128.0f, 1.0f);
            Vec4_Set(&haze->curve.key[1].pos, 200.0f, 0.0f, 128.0f, 1.0f);
            Vec4_Set(&haze->curve.key[2].pos, 255.0f, 0.0f, 255.0f, 1.0f);
        }
    } else {
        key = param->key;
        for (i = 0; i < 2; i++) {
            StgHaze_SetGrid(i, param->cols, param->rows);
            StgHaze_SetAmplitude(i, param->ampX, param->ampY);
            StgHaze_SetColor(i, param->color[0], param->color[1], param->color[2], param->color[3]);
            StgHaze_SetSpeed(i, param->period, param->follow);
            Vec4_Set(&gStgHaze->curve.key[0].pos, key[0], 0.0f, key[1], 1.0f);
            Vec4_Set(&gStgHaze->curve.key[1].pos, key[2], 0.0f, key[3], 1.0f);
            Vec4_Set(&gStgHaze->curve.key[2].pos, key[4], 0.0f, key[5], 1.0f);
        }
    }
}

/* Sets the mesh size of a view. */
void StgHaze_SetGrid(s32 view, s32 cols, s32 rows) {
    gStgHaze[view].cols = cols;
    gStgHaze[view].rows = rows;
}

/* Sets the jitter range of a view. */
void StgHaze_SetAmplitude(s32 view, s32 ampX, s32 ampY) {
    gStgHaze[view].ampX = ampX;
    gStgHaze[view].ampY = ampY;
}

/* Sets the colour of a view. */
void StgHaze_SetColor(s32 view, s32 r, s32 g, s32 b, s32 a) {
    gStgHaze[view].color = r | (g << 8) | (b << 16) | (a << 24);
}

/* Sets how often new targets are drawn and how fast the mesh follows them. */
void StgHaze_SetSpeed(s32 view, s32 period, f32 follow) {
    gStgHaze[view].period = period;
    gStgHaze[view].follow = follow;
}

/* Returns a view's depth curve. */
StgCurve *StgHaze_GetCurve(s32 view) {
    return &gStgHaze[view].curve;
}

/* Rebuilds a view's look-up. */
void StgHaze_RebuildClut(s32 view) {
    StgHaze_UpdateClut(&gStgHaze[view], 0, 0);
}

/* Turns every draw pass of the group off (1) or on (0). */
void StgFx_SetDisabled(s32 disabled) {
    *gStgFx = disabled;
}

/* Resets every member of the group and reloads the stage's fog and haze parameters. */
void StgFx_Reset(void) {
    *gStgFx = 0;
    StgHaze_Reset();
    StgBlur_Reset();
    ScrXfade_Reset();
    StgFog_ResetColor();
    ScrWarp_Reset();
    StgDepthTint_Rebuild();
    StgGlare_Reset();
    GfxLens_Clear();
    StgTint_Reset();
    StgHaze_SetParams(BtlStage_GetListA0());
    StgFog_SetParams(BtlStage_GetList50());
    StgDepthTint_LoadParams();
    GfxWater_LoadStageColor();
    StgGlare_LoadParams();
}

/* Creates the group. The arguments are GS texture blocks reserved for each member. */
void StgFx_Init(void) {
    gStgFx = Heap_Alloc(4, 0x20, 0, 2);
    *gStgFx = 0;
    GfxPost_InitNop();
    StgHaze_Init(0x3E9C);
    StgBlur_Init();
    ScrXfade_Init();
    StgFog_Init(0x3E84);
    ScrWarp_Init();
    GfxWater_Init();
    GfxDepthFog_Init(0x3E80);
    StgDepthTint_Init(0x3E60);
    StgGlare_Init(0x3E88);
    GfxLens_Init();
    StgPanBlur_Init(0x3E98);
    StgTint_Init();
    StgFx_Reset();
}

/* Destroys the group. */
void StgFx_Term(void) {
    StgHaze_Term();
    StgBlur_Term();
    ScrXfade_Term();
    StgFog_Term();
    ScrWarp_Term();
    GfxWater_Term();
    StgDepthTint_Term();
    StgGlare_Term();
    GfxLens_Stub107758();
    StgPanBlur_Term();
    GfxPost_TermNop();
#ifdef PORT
    { extern void PortThunder_Stop(void); PortThunder_Stop(); } /* (the fight is over: its storm's thunder too) */
#endif
    Heap_Free(gStgFx);
    gStgFx = NULL;
}

/* First draw pass (after the scene): steps the tints, then the depth-based filters the stage turns on. */
void StgFx_DrawPre(void) {
    if (*gStgFx == 0) {
        StgTint_Update();
        GfxDepthFog_Draw();
        StgDepthTint_Draw();
        if (!(Battle_GetWork()->flags & 0x400000000000000)) {
            StgGlare_Update();
            StgGlare_Draw();
        }
        if (BtlStage_HasFeature(3) && !(Battle_GetWork()->flags & 0x400000000000000)) {
            StgFog_Draw();
        }
        if (BtlStage_HasFeature(0xD)) {
            StgPanBlur_Draw();
        }
        if (BtlStage_HasFeature(9)) {
            StgHaze_DrawAll();
        }
        StgTint_DrawBack();
    }
}

/* Second draw pass: nothing. */
void StgFx_DrawNop(void) {
    if (*gStgFx == 0) {
        StgTint_Nop();
    }
}

/* Third draw pass: the white stage-change fade, then the blur and its neighbours. */
void StgFx_DrawPost(void) {
    Fade_DrawSlot2();
    if (*gStgFx == 0) {
        GfxWater_Draw();
        StgBlur_DrawAll();
        GfxLens_DrawFull();
        ScrWarp_Update();
    }
}

/* Fourth draw pass: the front tints, then releases finished tints. */
void StgFx_DrawOverlay(void) {
    if (*gStgFx == 0) {
        ScrXfade_PostDraw();
        ScrXfade_Update();
        StgTint_DrawFront();
        StgTint_Release();
    }
}

/* The first pass without the stage-feature and flag tests (and without haze and back tints). */
void StgFx_DrawPreNoCheck(void) {
    if (*gStgFx == 0) {
        StgTint_Update();
        GfxDepthFog_Draw();
        StgDepthTint_Draw();
        StgGlare_Update();
        StgGlare_Draw();
        StgFog_Draw();
    }
}

/* color = from + delta * ramp.value. */
void StgTint_CalcColor(StgTint *tint) {
    f32 t = tint->ramp.value;

    tint->color.x = tint->from.x + t * tint->delta.x;
    tint->color.y = tint->from.y + t * tint->delta.y;
    tint->color.z = tint->from.z + t * tint->delta.z;
    tint->color.w = tint->from.w + t * tint->delta.w;
}

#ifdef PORT
/*
 * The port's storm (BT3's, Dragon Rage), in BT4's one-on-one fights: offline, Quick Match and Ranked (not in Tag Team): while any fighter is in the
 * full power mode (fighter flag 6, the MAX POWER of a full ki charge) the sky clouds over, dark and blue, and now and then lightning strikes far away, with thunder a moment later
 * (port/src/gs/storm_snd.c). One storm for the whole fight however many fighters are powered up. It clears when
 * nobody is. The flash lights the whole picture.
 *
 * Far away: the bolts are drawn with the depth test against Z = 0, the value the frame starts with. The sky (stage group 0) is drawn without depth writes and everything else writes depth, so they show only
 * where the sky shows, behind the far hills, never over the arena, the fighters or what flies.
 *
 * Only drawing and sound: nothing here is read by the fight, its timing is the wall clock (not the game's frames,
 * which a rollback runs again) and its random numbers are its own (the game's rand() is left alone), so an online
 * match is not touched. Port_Setting("storm", 1): 0 turns it off. Its state is kept outside the game's variables (include/port_storm.h).
 */
extern s32 BtlCharApi_PortFlag(s32 objId, s32 flag);
extern s32 TagBattle_Active(void); /* tag_battle.c: a Tag Team battle (local or online) */
extern void Ps_SpriteScreen(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 v0, f32 u1, f32 v1, u32 tint, s32 alpha);
extern u32 Ps_Uv(f32 t);
extern int Port_TboArtState(int id);
extern int Port_Setting(const char *name, int def);
extern int Port_PageAspectMilli(void);
extern u64 SDL_GetTicks(void);
extern void PortThunder_Load(void);
extern void PortThunder_Play(int which, float volume, float pan);
extern void PortThunder_Hold(int held);
extern void PortThunder_Stop(void);

#include "port_storm.h"
#define STORM (*PortStorm_State())

static u32 PortStorm_Rand(u32 n) {
    STORM.seed = STORM.seed * 1664525u + 1013904223u;
    return (STORM.seed >> 8) % n;
}

/* Ps_SpriteScreen with the same depth test: a part of a picture over a box of the screen, only over the sky. */
static void PortStorm_SkySprite(s32 id, f32 x0, f32 y0, f32 x1, f32 y1, f32 u0, f32 u1, s32 alpha) {
    u64 *p;

    if (alpha <= 0 || x1 <= x0 || y1 <= y0) {
        return;
    }
    if (alpha > 0x80) {
        alpha = 0x80;
    }
    p = Dma_BeginDirect();
    p[0] = 4 | (1ULL << 15) | (1ULL << 60); /* A+D x 4 */
    p[1] = 0xE;
    p += 2;
    p[0] = 0x44;
    p[1] = 0x42; /* ALPHA_1 */
    p += 2;
    p[0] = 0x60;
    p[1] = 0x14; /* TEX1_1: bilinear */
    p += 2;
    p[0] = 0x50000;
    p[1] = 0x47; /* TEST_1: depth GEQUAL against Z = 0 */
    p += 2;
    p[0] = 5;
    p[1] = 0x08; /* CLAMP_1 */
    p += 2;
    p[0] = 1 | (1ULL << 15) | (1ULL << 58) | (8ULL << 60);
    p[1] = 0x535306E1; /* RGBAQ, (A+D: nothing), TEX0_1, PRIM, UV, XYZ2, UV, XYZ2 */
    p += 2;
    p[0] = 0x808080 | ((u64)alpha << 24);
    p[1] = 0;
    p += 2;
    p[0] = 1ULL | (1ULL << 14) | ((u64)TBO_PSM << 20) | (10ULL << 26) | (10ULL << 30) | (1ULL << 34) | ((u64)(id & 0x3FFF) << 37);
    p[1] = 0x156; /* PRIM: sprite, textured, blended, UV */
    p += 2;
    p[0] = (u64)Ps_Uv(u0) | ((u64)Ps_Uv(0.0f) << 16);
    p[1] = (u64)((s32)(x0 * 16.0f) + 0x7000) | ((u64)((s32)(y0 * 16.0f) + 0x7200) << 16);
    p += 2;
    p[0] = (u64)Ps_Uv(u1) | ((u64)Ps_Uv(1.0f) << 16);
    p[1] = (u64)((s32)(x1 * 16.0f) + 0x7000) | ((u64)((s32)(y1 * 16.0f) + 0x7200) << 16);
    p += 2;
    Dma_EndDirect(p);
}

static void PortStorm_Draw(void) {
    /* the flash, a step every 17 ms: on, nearly out, on again, then fading */
    static const f32 flash[16] = {1.0f, 0.85f, 0.15f, 0.05f, 0.7f, 0.55f, 0.4f, 0.3f, 0.22f, 0.16f, 0.11f, 0.07f, 0.04f, 0.02f, 0.01f, 0.0f};
    Vec4 c;
    s32 on = 0;
    s32 dt;
    s32 i;
    f32 f;
    u64 now;

    if (STORM.on < 0) {
        STORM.on = Port_Setting("storm", 1) != 0;
    }
    if (!STORM.on || TagBattle_Active()) {
        return;
    }
    now = SDL_GetTicks();
    dt = STORM.last == 0 || now < STORM.last ? 0 : now - STORM.last > 100 ? 100 : (s32)(now - STORM.last);
    STORM.last = now;
    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        dt = 0;
    }
    PortThunder_Hold(dt == 0 && (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)); /* (a thunder under way waits in the pause) */
    if (Port_TboArtState(TBO_STORM_BOLT) == 0) {
        /* the bolts' picture asked for from the fight's first frames (a dot, alpha 1): read then, not in the middle
           of the fight when the first fighter powers up */
        Ps_SpriteScreen(TBO_STORM_BOLT, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.01f, 0.01f, 0x808080, 1);
    }
    if (dt > 0) {
        for (i = 0; i < 16 && !on; i++) {
            if (BtlCharApi_PortFlag(i, 6)) {
                on = 1;
            }
        }
        if (on && STORM.level <= 0.0f) {
            PortThunder_Load(); /* (read on a thread of its own, long before the first strike) */
            STORM.next = 1200;
        }
        STORM.level += (f32)dt * (on ? 1.0f / 1250.0f : -1.0f / 2000.0f);
        if (STORM.level > 1.0f) {
            STORM.level = 1.0f;
        }
        if (STORM.level < 0.0f) {
            STORM.level = 0.0f;
        }
        if (STORM.flash >= 0) {
            STORM.flash = STORM.flash + dt >= 16 * 17 ? -1 : STORM.flash + dt;
        }
        if (on && STORM.level > 0.7f) {
            STORM.next -= dt;
            if (STORM.next <= 0) {
                STORM.next = 1500 + (s32)PortStorm_Rand(3500); /* 1.5 to 5 seconds */
                STORM.flash = 0;
                STORM.bolts = PortStorm_Rand(4) == 0 ? 2 : 1;
                for (i = 0; i < STORM.bolts; i++) {
                    STORM.kind[i] = (s32)PortStorm_Rand(8); /* which of the four drawings, mirrored or not */
                    STORM.h[i] = 120.0f + (f32)PortStorm_Rand(110);
                    STORM.x[i] = 30.0f + (f32)PortStorm_Rand(452);
                    STORM.y[i] = -10.0f + (f32)PortStorm_Rand(40);
                }
                /* far away: the thunder comes a second or two later, softer the later it comes */
                STORM.thunderIn = 700 + (s32)PortStorm_Rand(1600);
                STORM.thunderVol = 1.0f - (f32)(STORM.thunderIn - 700) / 1600.0f * 0.4f;
                STORM.thunderPan = (STORM.x[0] - 256.0f) / 256.0f * 0.6f;
            }
        }
        if (STORM.thunderIn >= 0) {
            STORM.thunderIn -= dt;
            if (STORM.thunderIn < 0) {
                s32 which = (s32)PortStorm_Rand(4);

                if (which == STORM.thunderLast) {
                    which = (which + 1 + (s32)PortStorm_Rand(3)) & 3;
                }
                STORM.thunderLast = which;
                PortThunder_Play(which, STORM.thunderVol, STORM.thunderPan);
            }
        }
    }
    if (STORM.level <= 0.0f) {
        return;
    }
    /* the sky clouds over: the whole picture a little darker and bluer (the stage's own tints do the same) */
    c.x = 34.0f;
    c.y = 40.0f;
    c.z = 58.0f;
    c.w = 80.0f * STORM.level;
    GfxPost_DrawTintRect(0x44, &c);
    if (STORM.flash < 0) {
        return;
    }
    f = flash[STORM.flash / 17 > 15 ? 15 : STORM.flash / 17] * STORM.level;
    if (f <= 0.0f) {
        return;
    }
    /* the flash lights the whole picture */
    c.x = 215.0f;
    c.y = 225.0f;
    c.z = 255.0f;
    c.w = 60.0f * f;
    GfxPost_DrawTintRect(0x44, &c);
    Gfx_AddDefaultEnv();
    Dma_AddZbuf(GFX_ZBP, 1); /* (no depth writes) */
    if (Port_TboArtState(TBO_STORM_BOLT) > 0 && f > 0.1f) {
        /* each drawing is a quarter of the picture, 256 x 1024 texels; the page is 512 x 448 stretched over the screen */
        s32 aspect = Port_PageAspectMilli() > 0 ? Port_PageAspectMilli() : 1333;

        for (i = 0; i < STORM.bolts; i++) {
            f32 u0 = (f32)(STORM.kind[i] & 3) * 0.25f;
            f32 u1 = u0 + 0.25f;
            f32 w = STORM.h[i] * 0.25f * (512.0f / 448.0f) * (1000.0f / (f32)aspect);

            if (STORM.kind[i] & 4) { /* mirrored */
                f32 t = u0;
                u0 = u1;
                u1 = t;
            }
            PortStorm_SkySprite(TBO_STORM_BOLT, STORM.x[i] - w * 0.5f, STORM.y[i], STORM.x[i] + w * 0.5f, STORM.y[i] + STORM.h[i], u0,
                                u1, (s32)(f * 128.0f));
        }
    }
    Dma_AddZbuf(GFX_ZBP, 0);
}
#endif

/* Turns every slot off. */
void StgTint_Reset(void) {
    StgTintWork *work = &gStgTint;
    s32 i;

    work->unk180 = 0;
    for (i = 0; i < STG_TINT_COUNT; i++) {
        StgTint *tint = &work->slot[i];

        tint->flags = 0;
        Ramp_Stop(&tint->ramp);
    }
#ifdef PORT
    STORM.level = 0.0f;
    STORM.flash = -1;
    STORM.thunderIn = -1;
    PortThunder_Stop();
#endif
}

/* Clears the work block. */
void StgTint_Init(void) {
    memset(&gStgTint, 0, sizeof(gStgTint));
    StgTint_Reset();
}

/* Steps the ramp of every slot that is on. Slots 0..2 wait while the battle is paused; slot 3 does not. */
void StgTint_Update(void) {
    StgTint *tint = gStgTint.slot;
    s32 i;

    for (i = 0; i < STG_TINT_COUNT; i++, tint++) {
        if (i != 3 && (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            continue;
        }
        if (tint->flags & STG_TINT_ON) {
            if (Ramp_Step(&tint->ramp) == 1) {
                if (tint->flags & STG_TINT_IN) {
                    tint->flags |= STG_TINT_DONE;
                    Ramp_Stop(&tint->ramp);
                }
            }
        }
    }
}

/* Turns off the slots whose way back has ended. */
void StgTint_Release(void) {
    StgTint *tint = gStgTint.slot;
    s32 i;

    for (i = 0; i < STG_TINT_COUNT; i++, tint++) {
        if ((tint->flags & STG_TINT_ON) && (tint->flags & STG_TINT_DONE)) {
            tint->flags = 0;
            Ramp_Stop(&tint->ramp);
        }
    }
}

/* Draws slots 0 and 1 over the scene and publishes slot 0's colour as integers. */
void StgTint_DrawBack(void) {
    StgTint *tint = &gStgTint.slot[0];

    if (tint->flags & STG_TINT_ON) {
        StgTint_CalcColor(tint);
        GfxPost_DrawTintRect(0x44, &tint->color);
        gStgTintColor0[0] = tint->color.x;
        gStgTintColor0[1] = tint->color.y;
        gStgTintColor0[2] = tint->color.z;
        gStgTintColor0[3] = tint->color.w;
    } else {
        gStgTintColor0[0] = 0;
        gStgTintColor0[1] = 0;
        gStgTintColor0[2] = 0;
        gStgTintColor0[3] = 0;
    }
    tint = &gStgTint.slot[1];
    if (tint->flags & STG_TINT_ON) {
        StgTint_CalcColor(tint);
        GfxPost_DrawTintRect(0x44, &tint->color);
    }
#ifdef PORT
    PortStorm_Draw();
#endif
}

/* Does nothing. */
void StgTint_Nop(void) {
}

/* Draws slots 3 and 2 over everything, each only when its alpha is above 0.001. */
void StgTint_DrawFront(void) {
    StgTint *tint = &gStgTint.slot[3];

    StgTint_CalcColor(tint);
    if (tint->color.w > 0.001f) {
        GfxPost_DrawTintRect(0x44, &tint->color);
    }
    tint--;
    StgTint_CalcColor(tint);
    if (tint->color.w > 0.001f) {
        GfxPost_DrawTintRect(0x44, &tint->color);
    }
}

/* Starts a slot's ramp: dir 0 goes to the slot's second colour, dir 1 back to the first. */
/* Starts a slot's ramp: dir 0 goes to the slot's second colour, dir 1 back to the first. A slot that is off
   ignores dir 1; a slot already going the asked way is left alone; a running slot restarts from its current
   colour. Slot 0's two colours come from the stage file when it has a tint section. */
void StgTint_Start(u32 slot, s32 dir, f32 seconds) {
    StgTintColor colors[STG_TINT_COUNT][2] = {
        { { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 100.0f } },
        { { 255.0f, 255.0f, 255.0f, 0.0f }, { 255.0f, 255.0f, 255.0f, 128.0f } },
        { { 255.0f, 255.0f, 255.0f, 0.0f }, { 255.0f, 255.0f, 255.0f, 128.0f } },
        { { 0.0f, 0.0f, 0.0f, 128.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },
    };
    u8 *stage;
    StgTint *tint;

    stage = BtlStage_GetList80();
    if (stage != NULL) {
        colors[0][0].x = stage[0];
        colors[0][0].y = stage[1];
        colors[0][0].z = stage[2];
        colors[0][0].w = stage[3];
        colors[0][1].x = stage[4];
        colors[0][1].y = stage[5];
        colors[0][1].z = stage[6];
        colors[0][1].w = stage[7];
    }
    if (slot >= STG_TINT_COUNT) {
        slot = 0;
    }
    tint = &gStgTint.slot[slot];
    if (!(tint->flags & STG_TINT_ON)) {
        if (dir == 1) {
            return;
        }
        tint->flags = STG_TINT_ON | STG_TINT_OUT;
        Vec4_Copy(&tint->from, (Vec4 *)&colors[slot][dir]);
        Vec4_Copy(&tint->to, (Vec4 *)&colors[slot][dir ^ 1]);
        Vec4_Sub(&tint->delta, &tint->to, &tint->from);
    } else {
        if (dir == 0) {
            if (tint->flags & STG_TINT_OUT) {
                return;
            }
            tint->flags = STG_TINT_ON | STG_TINT_OUT;
        } else {
            if (tint->flags & STG_TINT_IN) {
                return;
            }
            tint->flags = STG_TINT_ON | STG_TINT_IN;
        }
        Vec4_Copy(&tint->from, &tint->color);
        Vec4_Copy(&tint->to, (Vec4 *)&colors[slot][dir ^ 1]);
        Vec4_Sub(&tint->delta, &tint->to, &tint->from);
    }
    Ramp_Start(&tint->ramp, seconds, 0.0f, 1.0f);
}

/* Copies slot 0's integer colour. */
void StgTint_GetColor0(s32 *out) {
    out[0] = gStgTintColor0[0];
    out[1] = gStgTintColor0[1];
    out[2] = gStgTintColor0[2];
    out[3] = gStgTintColor0[3];
}

/* Returns the work block. */
StgTintWork *StgTint_GetWork(void) {
    return &gStgTint;
}

/* 1 when the slot is on. */
s32 StgTint_IsOn(u32 slot) {
    if (slot >= STG_TINT_COUNT) {
        slot = 0;
    }
    return gStgTint.slot[slot].flags & STG_TINT_ON;
}

/* Opens the blur packet. */
void StgBlur_BeginDraw(StgBlurDraw *d, s32 x0, s32 srcW, s32 width, s32 srcH, s32 dstW, s32 dstH) {
    d->p = (GsQword *)Dma_BeginDirect();
    Gfx_PutDefaultEnv(&d->p);
    d->p->d[0] = GIF_TAG(6, 0, 1);
    d->p->d[1] = GIF_REG_AD;
    d->p++;
    d->p->d[0] = 0x30000;
    d->p->d[1] = GS_TEST_1;
    d->p++;
    d->p->d[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    d->p->d[1] = GS_ZBUF_1;
    d->p++;
    d->p->d[0] = !(gGfx.frame & 1) ? 0x264020E00 : 0x264020000;
    d->p->d[1] = GS_TEX0_1;
    d->p++;
    d->p->d[0] = 0x40150;
    d->p->d[1] = GS_FRAME_1;
    d->p++;
    d->p->d[0] = GS_SET_SCISSOR(0, dstW - 1, 0, dstH - 1);
    d->p->d[1] = GS_SCISSOR_1;
    d->p++;
    d->p->d[0] = GS_SET_CLAMP(2, 2, 0, srcW - 1, 0, srcH - 1);
    d->p->d[1] = GS_CLAMP_1;
    d->p++;
}

/* Closes the blur packet. */
void StgBlur_EndDraw(StgBlurDraw *d) {
    Dma_EndDirect((u64 *)d->p);
}

/* Starts one textured Gouraud triangle strip of nVerts vertices (colour, texel, position per vertex).
   Both callers pass 1 as a third argument, which is not used. */
void StgBlur_PutStripTag(StgBlurDraw *d, s32 nVerts, s32 unused) {
    d->p->d[0] = 0x1000000000000001;
    d->p->d[1] = 0xE;
    d->p++;
    d->p->d[0] = 0x15C;
    d->p->d[1] = 0;
    d->p++;
    d->p->d[0] = (u64)(nVerts | 0x8000) | ((u64)0x8800 << 47);
    d->p->d[1] = 0x531F;
    d->p++;
}

/* Does nothing (end of a strip; StgBlur_Draw calls it with its packet cursor). */
void StgBlur_Nop(StgBlurDraw *d) {
}

/* Allocates the two blur views. */
void StgBlur_Init(void) {
    StgBlur *blur = Heap_Alloc(sizeof(StgBlur) * 2, 0x20, 0, 2);

    gStgBlur = blur;
    memset(blur, 0, sizeof(StgBlur) * 2);
    StgBlur_Reset();
}

/* Frees the blur views. */
void StgBlur_Term(void) {
    if (gStgBlur != NULL) {
        Heap_Free(gStgBlur);
        gStgBlur = NULL;
    }
}

/* Defaults: 3 passes, scale 2, grey corners with alpha 0 (off). */
void StgBlur_Reset(void) {
    StgBlur *blur = gStgBlur;
    s32 i;

    for (i = 0; i < 2; i++) {
        blur->unk0 = 0;
        Vec4_Set(&blur->center, 0.0f, 0.0f, 1.0f, 0.0f);
        blur->passes = 3;
        blur->scale = 2.0f;
        blur->shiftX = 2.0f;
        blur->shiftY = 2.0f;
        blur->screenSpace = 1;
        blur++;
        StgBlur_SetColor0Rgba(i, 0x80, 0x80, 0x80, 0);
        StgBlur_SetColor1Rgba(i, 0x80, 0x80, 0x80, 0);
        StgBlur_SetColor2Rgba(i, 0x80, 0x80, 0x80, 0);
        StgBlur_SetColor3Rgba(i, 0x80, 0x80, 0x80, 0);
    }
}

/* One strip vertex of the final rings: colour (q = 1), texel (u, v) of the work buffer, position (x, y) in view pixels. */
static inline void StgBlur_PutVertex(StgBlurDraw *d, u32 rgba, s32 x, s32 y, s32 u, s32 v) {
    d->p->d[0] = 0;
    d->p->d[1] = (u64)rgba | ((u64)0xFE00 << 46);
    d->p++;
    d->p->d[0] = (u64)((u << 4) + 8) | ((u64)((v << 4) + 8) << 16);
    d->p->d[1] = (u64)((x << 4) + 0x7000) | ((u64)((y << 4) + 0x7200) << 16);
    d->p++;
}

/*
 * Radial blur of one view. Nothing is drawn unless one of the three ring colours (centre, middle, edge)
 * has a non-zero alpha.
 *   1. View rectangle: 512 x 448 at x 0, or 256 x 448 at x 0 / 256 in split screen. The work buffers are
 *      half that size (256 or 128 x 224).
 *   2. Centre: blur->center as it is when blur->screenSpace is set; otherwise the view matrix (third
 *      argument) is copied, its translation cleared, and blur->center (a world direction) rotated by it.
 *      Only x and y are used.
 *   3. StgBlur_BeginDraw, then the view's part of the frame is copied at half size into work buffer 0
 *      (frame block 0x150 = texture block 0x2A00).
 *   4. `passes` times, with f = 1, 2, 4, ...: buffer (i & 1) is copied into the other buffer (half a texel
 *      off, so it is filtered), then drawn over that copy again as a blended sprite in colour `color3`
 *      whose texels are pulled in by grow = (int)(f * scale) on every side and shifted by
 *      (int)((float)(int)f * shiftX * center.x) and (int)(f * shiftY * center.y): a positive shift moves
 *      the near edge of the texel rectangle, a negative one the far edge.
 *   5. The frame buffer, the view's scissor and the last written work buffer (as texture) are selected and
 *      two rings of ten-vertex Gouraud strips are drawn around the point (0.5 + 0.5 * center) * size:
 *      centre (colour 0) -> points half way to the corners (colour 1), then those -> the corners
 *      (colour 2). The alpha of the three colours is the strength of the blur at each ring.
 * With passes <= 0 the texture of step 5 is block 0 (lastTbp keeps its initial 0); the game never sets that.
 * Emits two local tables and one constant in .sdata: 0x2FEC30 = {0x2A00, 0x2D80}, 0x2FEC38 = {0x150, 0x16C},
 * 0x2FEC40 = 448 (the integer the compiler converts to float for the centre's y).
 * Matching notes: only `p` is in memory (StgBlurDraw is one pointer); blur, width, half, xOffset and x1
 * are plain locals that the allocator leaves on the stack, which is what the 0x134..0x150 slots are.
 */
void StgBlur_Draw(s32 split, s32 view, Mtx44 *viewMtx) {
    Vec4 c;
    Mtx44 m;
    StgBlurDraw d;
    StgBlur *blur;
    s32 width;   /* of the view, pixels */
    s32 half;    /* of the work buffers */
    s32 xOffset; /* left edge of the view */
    s32 srcH;
    s32 h;
    s32 lastTbp; /* texture block of the work buffer written last */
    s32 x1;      /* right edge of the view */
    s32 u1;
    s32 i;
    f32 f;

    blur = &gStgBlur[view];
    width = 0x200;
    half = 0x100;
    srcH = 0x1C0;
    h = 0xE0;
    lastTbp = 0;
    xOffset = 0;
    x1 = 0x200;
    if ((blur->color01 & 0xFF000000FF000000) == 0 && (blur->color2 & 0xFF000000) == 0) {
        return;
    }
    if (split) {
        width = 0x100;
        half = 0x80;
        xOffset = (view == 1) ? 0x100 : 0;
        x1 = xOffset + width;
        u1 = x1;
    } else {
        u1 = 0x200;
    }
    if (blur->screenSpace) {
        Vec4_Copy(&c, &blur->center);
    } else {
        Mtx_Copy(&m, viewMtx);
        Mtx_ClearTrans(&m);
        Mtx_MulVec4(&c, &m, &blur->center);
    }
    StgBlur_BeginDraw(&d, xOffset, x1, width, srcH, half, h);
    {
        u32 tbp[2] = { 0x2A00, 0x2D80 }; /* the two work buffers as textures ... */
        u32 fbp[2] = { 0x150, 0x16C };   /* ... and as frame buffers */

        Dma_PutTexStrips(&d.p, 0, 0, half, h, 0, 0, xOffset, 0, u1, srcH, 0, 0, 0x80808080, 0);
        f = 1.0f;
        for (i = 0; i < blur->passes; i++, f += f) {
            s32 grow;
            s32 dx;
            s32 dy;
            s32 nx;
            s32 ny;
            s32 ua;
            s32 ub;
            s32 vb;
            s32 va;

            grow = f * blur->scale;
            dx = (f32)(s32)f * blur->shiftX * c.x;
            dy = f * blur->shiftY * c.y;
            nx = 0;
            ny = 0;
            if (dx < 0) {
                nx = -dx;
                dx = 0;
            }
            if (dy < 0) {
                ny = -dy;
                dy = 0;
            }
            ua = grow + dx;
            ub = grow + nx;
            va = grow + dy;
            vb = grow + ny;
            d.p->d[0] = GIF_TAG(2, 0, 1);
            d.p->d[1] = GIF_REG_AD;
            d.p++;
            d.p->d[0] = (u64)(tbp[i & 1] | 0x20010000) | ((u64)8 << 30); /* 256 x 256, 4 pages wide */
            d.p->d[1] = GS_TEX0_1;
            d.p++;
            d.p->d[0] = (u64)(fbp[(i & 1) ? 0 : 1] | 0x40000);
            d.p->d[1] = GS_FRAME_1;
            d.p++;
            lastTbp = tbp[(i & 1) ? 0 : 1];
            Dma_PutTexStrips(&d.p, 0, 0, half, h, 0, 0, 0, 0, half, h, 8, 8, 0x80808080, 0);
            d.p->d[0] = GIF_TAG(2, 0, 1);
            d.p->d[1] = GIF_REG_AD;
            d.p++;
            d.p->d[0] = 0x156; /* sprite, textured, blended, UV */
            d.p->d[1] = GS_PRIM;
            d.p++;
            d.p->d[0] = (u64)blur->color3 | ((u64)0xFE00 << 46);
            d.p->d[1] = GS_RGBAQ;
            d.p++;
            d.p->d[0] = 0x4400000000008001; /* REGLIST, one loop of UV, XYZ2, UV, XYZ2 */
            d.p->d[1] = 0x5353;
            d.p++;
            d.p->d[0] = (u64)((ua << 4) + 8) | ((u64)((va << 4) + 8) << 16);
            d.p->d[1] = 0x72007000;
            d.p++;
            d.p->d[0] = (u64)(((half - ub) << 4) + 8) | ((u64)(((h - vb) << 4) + 8) << 16);
            d.p->d[1] = (u64)((half + 0x700) << 4) | ((u64)0x8000 << 16);
            d.p++;
        }
        d.p->d[0] = GIF_TAG(4, 0, 1);
        d.p->d[1] = GIF_REG_AD;
        d.p++;
        d.p->d[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
        d.p->d[1] = GS_FRAME_1;
        d.p++;
        d.p->d[0] = GS_SET_SCISSOR(xOffset, x1 - 1, 0, srcH - 1);
        d.p->d[1] = GS_SCISSOR_1;
        d.p++;
        d.p->d[0] = (u64)(lastTbp | 0x20010000) | ((u64)8 << 30);
        d.p->d[1] = GS_TEX0_1;
        d.p++;
        d.p->d[0] = GS_SET_CLAMP(2, 2, 0, half - 1, 0, h - 1);
        d.p->d[1] = GS_CLAMP_1;
        d.p++;
        {
            s32 in[4][4];  /* x, y, u, v of the points half way between the centre and each corner */
            s32 out[4][4]; /* x, y, u, v of the view's corners */
            s32 cx;
            s32 cy;
            s32 hx;
            s32 hy;
            s32 rx;
            s32 ry;
            s32 ru;
            s32 rv;

            cx = (c.x * 0.5f + 0.5f) * (f32)width;
            cy = (c.y * 0.5f + 0.5f) * (f32)srcH;
            hx = cx / 2;
            hy = cy / 2;
            rx = width - cx;
            ry = srcH - cy;
            ru = half - hx;
            rv = h - hy;
            out[0][0] = 0;
            out[0][1] = 0;
            out[0][2] = 0;
            out[0][3] = 0;
            out[1][0] = width;
            out[1][1] = 0;
            out[1][2] = half;
            out[1][3] = 0;
            out[2][0] = width;
            out[2][1] = srcH;
            out[2][2] = half;
            out[2][3] = h;
            out[3][0] = 0;
            out[3][1] = srcH;
            out[3][2] = 0;
            out[3][3] = h;
            in[0][0] = hx;
            in[0][1] = hy;
            in[0][2] = hx / 2;
            in[0][3] = hy / 2;
            in[1][0] = cx + rx / 2;
            in[1][1] = hy;
            in[1][2] = hx + ru / 2;
            in[1][3] = hy / 2;
            in[2][0] = cx + rx / 2;
            in[2][1] = cy + ry / 2;
            in[2][2] = hx + ru / 2;
            in[2][3] = hy + rv / 2;
            in[3][0] = hx;
            in[3][1] = cy + ry / 2;
            in[3][2] = hx / 2;
            in[3][3] = hy + rv / 2;
            StgBlur_PutStripTag(&d, 10, 1);
            for (i = 0; i < 5; i++) {
                StgBlur_PutVertex(&d, (u32)blur->color01, cx + xOffset, cy, hx, hy);
                StgBlur_PutVertex(&d, ((u32 *)&blur->color01)[1], in[i % 4][0] + xOffset, in[i % 4][1], in[i % 4][2],
                                  in[i % 4][3]);
            }
            StgBlur_Nop(&d);
            StgBlur_PutStripTag(&d, 10, 1);
            for (i = 0; i < 5; i++) {
                StgBlur_PutVertex(&d, ((u32 *)&blur->color01)[1], in[i % 4][0] + xOffset, in[i % 4][1], in[i % 4][2],
                                  in[i % 4][3]);
                StgBlur_PutVertex(&d, blur->color2, out[i % 4][0] + xOffset, out[i % 4][1], out[i % 4][2],
                                  out[i % 4][3]);
            }
            StgBlur_Nop(&d);
        }
    }
    StgBlur_EndDraw(&d);
}

/* Draws the blur for whatever views are on screen. */
void StgBlur_DrawAll(void) {
    if (gBtlCam == NULL) {
        StgBlur_Draw(0, 0, &gBtlCamView->world2view2);
        return;
    }
    if (Battle_IsSplitScreen()) {
        if (DemoCam_IsActive()) {
            StgBlur_Draw(0, 0, &gBtlCam->views[0].view.world2view2);
            return;
        }
        if (gBtlCam->cur->split == 0) {
            StgBlur_Draw(0, gBtlCam->cur->index, &gBtlCam->views[gBtlCam->cur->index].view.world2view2);
            return;
        }
        StgBlur_Draw(1, 0, &gBtlCam->views[0].view.world2view2);
        StgBlur_Draw(1, 1, &gBtlCam->views[1].view.world2view2);
        return;
    }
    if (DemoCam_IsActive()) {
        StgBlur_Draw(0, 0, &gBtlCam->views[0].view.world2view2);
        return;
    }
    StgBlur_Draw(0, gBtlCam->cur->index, &gBtlCam->views[gBtlCam->cur->index].view.world2view2);
}

/* Sets the number of passes. */
void StgBlur_SetPasses(s32 view, s32 passes) {
    gStgBlur[view].passes = passes;
}

/* Sets the growth per pass and the two shift factors. */
void StgBlur_SetScale(s32 view, f32 scale, f32 shiftX, f32 shiftY) {
    gStgBlur[view].scale = scale;
    gStgBlur[view].shiftX = shiftX;
    gStgBlur[view].shiftY = shiftY;
}

/* Normalises `center` in place and selects how StgBlur_Draw reads the stored centre. */
void StgBlur_SetCenter(s32 view, Vec4 *center, s32 screenSpace) {
    Vec3_Normalize(&gStgBlur[view].center, center);
    gStgBlur[view].screenSpace = screenSpace;
}

/* Corner colour 0 from components. */
void StgBlur_SetColor0Rgba(s32 view, u8 r, u8 g, u8 b, u8 a) {
    StgBlur_SetColor0(view, r | (g << 8) | (b << 16) | (a << 24));
}

/* Corner colour 0. */
void StgBlur_SetColor0(s32 view, u32 rgba) {
    *(u32 *)&gStgBlur[view].color01 = rgba;
}

/* Corner colour 1 from components. */
void StgBlur_SetColor1Rgba(s32 view, u8 r, u8 g, u8 b, u8 a) {
    StgBlur_SetColor1(view, r | (g << 8) | (b << 16) | (a << 24));
}

/* Corner colour 1. */
void StgBlur_SetColor1(s32 view, u32 rgba) {
    ((u32 *)&gStgBlur[view].color01)[1] = rgba;
}

/* Corner colour 2 from components. */
void StgBlur_SetColor2Rgba(s32 view, u8 r, u8 g, u8 b, u8 a) {
    StgBlur_SetColor2(view, r | (g << 8) | (b << 16) | (a << 24));
}

/* Corner colour 2. */
void StgBlur_SetColor2(s32 view, u32 rgba) {
    gStgBlur[view].color2 = rgba;
}

/* Corner colour 3 from components. */
void StgBlur_SetColor3Rgba(s32 view, u8 r, u8 g, u8 b, u8 a) {
    StgBlur_SetColor3(view, r | (g << 8) | (b << 16) | (a << 24));
}

/* Corner colour 3. */
void StgBlur_SetColor3(s32 view, u32 rgba) {
    gStgBlur[view].color3 = rgba;
}
