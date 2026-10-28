#include "common.h"

/*
 * Stage module, second part: 0x242D28..0x245F58 (88 functions). See include/battle/stg_b.h.
 *
 *   0x242D28..0x2435C0  BtlStage_*: readers of the loaded stage data (feature tests, flag word, ambient colour,
 *                       light direction, stage number), the stage timers and BtlStage_Update.
 *   0x2435C0..0x244170  StgAmb_*: the stage's looping background sound and its per-stage volume handler.
 *   0x244170..0x244890  ScrXfade_*: cross-fade from a captured frame.
 *   0x244890..0x245878  ScrWarp_*: screen shock waves (frame redrawn through displaced texture coordinates).
 *   0x245878..0x245F58  StgFog_*: depth look-up table and the depth blur pass (the module continues in stg_c.c).
 *
 * Nothing here answers a collision query and nothing here writes fighter or battle state; the only state with
 * a gameplay reader is the stage flag word (BtlStage_HasMoon) and the stage timers' flag bit.
 */
#include "battle/battle.h"
#include "battle/battle_work.h"
#include "battle/stg_b.h"
#include "battle/btl_cam.h"
#include "sys/adx.h"
#include "sys/common.h"
#include "sys/debug.h"
#include "sys/dma.h"
#include "sys/gfx.h"
#include "sys/heap.h"
#include "sys/mathf.h"
#include "battle/btl_scene.h"
#include "sys/randf.h"
#include "sys/snd.h"

s32 BtlStage_IsReady(void);
f32 Stg_CalcYaw(f32, f32, f32, f32);
void BtlStage_UpdateObjs(void);
void StgModel_UpdateAnims(void);
void StgRigid_Update(void);
void Vec4_Copy(Vec4 *dst, Vec4 *src);
void Mtx_InverseRT(Mtx44 *out, Mtx44 *in);
s32 BtlStage_GetWaterLevel(f32 *out);
f32 Vec3_Dist(Vec4 *a, Vec4 *b);
void *memset(void *, s32, u32);

extern StgAmb gStgAmb;

/* Returns int as in the SDK: with `void` the first call of ScrXfade_StoreHalf does not set v0 and the scheduler
   orders the instructions in front of it differently. */
s32 sceGsSetDefStoreImage(void *sp, s16 sbp, s16 sbw, s16 spsm, s16 x, s16 y, s16 w, s16 h);
s32 sceGsExecStoreImage(void *sp, void *dst);
void FlushCache(s32 mode);
void GfxClut_InitPacket(void *tex, u16 arg);
void StgCurve_Build(StgFogPoint *pts, f32 *curve);
void Vec4_Set(Vec4 *v, f32 x, f32 y, f32 z, f32 w);
void GfxPost_DrawDepthClut(s32 a, s32 tbp, s32 cbp, u64 alpha);
s32 Tex_Log2Size(s32 size);
void Mtx_Copy(Mtx44 *out, Mtx44 *in);
void Vu0Screen_StoreMtx(Mtx44 *out);
void Mtx_ProjectInt(Vec4 *out, Mtx44 *m, Vec4 *in);
void IVec4_ToFloat(Vec4 *out, Vec4 *in);

/* The path of StgAmb_Update10 (16-byte aligned: it is copied by doublewords). */
typedef struct StgAmbPath {
    Vec4 p[8];
} __attribute__((aligned(16))) StgAmbPath;

extern Vec4 D_002C6C10;

/* Tells whether the stage data has feature `kind` (0..13). */
s32 BtlStage_HasFeature(u32 kind) {
    StgData *d;

    if (gBtlStage == NULL || (d = gBtlStage->data) == NULL) {
        return 0;
    }
    switch (kind) {
    case 0:
        if (d->n40 != 0) {
            if (d->sec44[0] & 1) {
                return 1;
            }
        }
        break;
    case 1:
        if (d->n40 > 1) {
            if (d->sec44[4] & 1) {
                return 1;
            }
        }
        break;
    case 2:
        if (d->n48 != 0) {
            if (d->sec4C[0] & 1) {
                return 1;
            }
        }
        break;
    case 3:
        if (d->n50 != 0) {
            if (d->sec54[0] & 1) {
                return 1;
            }
        }
        break;
    case 4:
        if (d->n98 != 0) {
            if (d->sec9C[0] & 1) {
                return 1;
            }
        }
        break;
    case 6:
        if (d->n60 != 0) {
            if (d->sec64[4] & 1) {
                return 1;
            }
        }
        break;
    case 7:
        if (d->n90 != 0) {
            if (d->sec94[0] & 1) {
                return 1;
            }
        }
        break;
    case 8:
        if (d->n58 != 0) {
            if (d->sec5C->unk16 != 0) {
                return 1;
            }
        }
        break;
    case 9:
        if (d->nA0 != 0) {
            if (d->secA4[0] & 1) {
                return 1;
            }
        }
        break;
    case 10:
        if (d->n70 != 0) {
            if (d->flags[0] & STG_FLAG_FX_A) {
                return 1;
            }
        }
        break;
    case 11:
        if (d->n70 != 0) {
            if (d->flags[0] & STG_FLAG_FX_B) {
                return 1;
            }
        }
        break;
    case 12:
        if (d->n70 != 0) {
            if (d->flags[0] & STG_FLAG_FX_C) {
                return 1;
            }
        }
        break;
    case 13:
        return BtlStage_HasFlag8();
    }
    return 0;
}

/* Returns member `index` of the stage file, NULL when it is empty. */
void *BtlStage_GetFileMember(s32 index) {
    u32 *file = ((BattleRes *)gCommonRes->unk20)->stage;
    u32 *ret = NULL;

    if ((index + file)[4] != (index + file)[3]) {
        ret = file + ((index + file)[3] >> 2);
    }
    return ret;
}

/* Stage effect resource A, when the stage flags allow it. */
void *BtlStage_GetFxResA(void) {
    StgData *d;
    void *ret = NULL;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            if (!(d->flags[0] & STG_FLAG_FX_A)) {
                goto fail;
            }
            if (gBtlStageFxRes == NULL) {
                goto fail;
            }
            ret = gBtlStageFxRes->a;
        }
    }
    return ret;
fail:
    return NULL;
}

/* Stage effect resource B, when the stage flags allow it. */
void *BtlStage_GetFxResB(void) {
    StgData *d;
    void *ret = NULL;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            if (!(d->flags[0] & STG_FLAG_FX_B)) {
                goto fail;
            }
            if (gBtlStageFxRes == NULL) {
                goto fail;
            }
            ret = gBtlStageFxRes->b;
        }
    }
    return ret;
fail:
    return NULL;
}

/* Stage effect resource C, when the stage flags allow it. */
void *BtlStage_GetFxResC(void) {
    StgData *d;
    void *ret = NULL;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            if (!(d->flags[0] & STG_FLAG_FX_C)) {
                goto fail;
            }
            if (gBtlStageFxRes == NULL) {
                goto fail;
            }
            ret = gBtlStageFxRes->c;
        }
    }
    return ret;
fail:
    return NULL;
}

/* Second pointer of effect resource A, when the first exists. */
void *BtlStage_GetFxResA2(void) {
    StgData *d;
    StgFxRes *res;
    void *ret = NULL;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            if (!(d->flags[0] & STG_FLAG_FX_A)) {
                goto fail;
            }
            res = gBtlStageFxRes;
            if (res == NULL) {
                goto fail;
            }
            if (res->a == NULL) {
                goto fail;
            }
            ret = res->a2;
        }
    }
    return ret;
fail:
    return NULL;
}

/* Second pointer of effect resource B, when the first exists. */
void *BtlStage_GetFxResB2(void) {
    StgData *d;
    StgFxRes *res;
    void *ret = NULL;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            if (!(d->flags[0] & STG_FLAG_FX_B)) {
                goto fail;
            }
            res = gBtlStageFxRes;
            if (res == NULL) {
                goto fail;
            }
            if (res->b == NULL) {
                goto fail;
            }
            ret = res->b2;
        }
    }
    return ret;
fail:
    return NULL;
}

/* Second pointer of effect resource C, when the first exists. */
void *BtlStage_GetFxResC2(void) {
    StgData *d;
    StgFxRes *res;
    void *ret = NULL;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            if (!(d->flags[0] & STG_FLAG_FX_C)) {
                goto fail;
            }
            res = gBtlStageFxRes;
            if (res == NULL) {
                goto fail;
            }
            if (res->c == NULL) {
                goto fail;
            }
            ret = res->c2;
        }
    }
    return ret;
fail:
    return NULL;
}

/* Tests stage flag 8. */
s32 BtlStage_HasFlag8(void) {
    StgData *d;
    s32 ret = 0;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            s32 bits = d->flags[0] & STG_FLAG_8;
            ret = bits != 0;
        }
    }
    return ret;
}

/* Tests stage flag 0x10: the stage has a moon. */
s32 BtlStage_HasMoon(void) {
    StgData *d;
    s32 ret = 0;

    if (gBtlStage != NULL) {
        d = gBtlStage->data;
        if (d != NULL) {
            s32 bits = d->flags[0] & STG_FLAG_MOON;
            ret = bits != 0;
        }
    }
    return ret;
}

/* Reads the stage's ambient colour as four ints; 0x80 grey when there is no stage. */
s32 BtlStage_GetAmbient(s32 *rgba) {
    StgData *d;
    u8 *c;

    if (gBtlStage == NULL) {
        rgba[0] = 0x80;
        rgba[1] = 0x80;
        rgba[2] = 0x80;
        rgba[3] = 0x80;
        return 0;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        rgba[0] = 0x80;
        rgba[1] = 0x80;
        rgba[2] = 0x80;
        rgba[3] = 0x80;
        return 0;
    }
    d = gBtlStage->data;
    if (d == NULL) {
        rgba[0] = 0x80;
        rgba[1] = 0x80;
        rgba[2] = 0x80;
        rgba[3] = 0x80;
        return 0;
    }
    c = d->ambient;
    rgba[0] = c[0];
    rgba[1] = c[1];
    rgba[2] = c[2];
    return 1;
}

/* Writes the stage's ambient colour. */
s32 BtlStage_SetAmbient(s32 *rgba) {
    s32 col[4];
    StgData *d;
    u8 *c;

    *(u64 *)&col[0] = *(u64 *)&rgba[0];
    *(u64 *)&col[2] = *(u64 *)&rgba[2];
    if (gBtlStage == NULL) {
        return 0;
    }
    d = gBtlStage->data;
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return 0;
    }
    if (d == NULL) {
        return 0;
    }
    c = d->ambient;
    c[0] = col[0];
    c[1] = col[1];
    c[2] = col[2];
    return 1;
}

/* Returns the stage number, -1 when there is no stage. */
s32 BtlStage_GetId(void) {
    StgData *d;

    if (gBtlStage == NULL) {
        return -1;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return -1;
    }
    d = gBtlStage->data;
    if (d == NULL) {
        return -1;
    }
    return d->info->id;
}

/* Copies the stage's light direction (and remembers it); the remembered one when there is no stage. */
s32 BtlStage_GetLightDir(Vec4 *out) {
    StgData *d;
    Vec4 *dir;

    if (gBtlStage == NULL) {
        Vec4_Copy(out, &D_002C6C10);
        return -1;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        Vec4_Copy(out, &D_002C6C10);
        return -1;
    }
    d = gBtlStage->data;
    if (d == NULL) {
        Vec4_Copy(out, &D_002C6C10);
        return -1;
    }
    dir = d->lightDir;
    Vec4_Copy(out, dir);
    Vec4_Copy(&D_002C6C10, dir);
    return 1;
}

/* Gives the place stored in section +0x2C: its first point and the yaw from it toward its second point. */
s32 BtlStage_GetPlace2C(Vec4 *pos, Vec4 *rot) {
    StgData *d;
    f32 *b;
    f32 yaw;

    if (gBtlStage == NULL) {
        return 0;
    }
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return 0;
    }
    d = gBtlStage->data;
    if (d == NULL) {
        return 0;
    }
    b = d->place;
    yaw = Stg_CalcYaw(b[0], b[2], b[4], b[6]);
    pos->x = b[0];
    pos->y = 0.0f;
    pos->z = b[2];
    pos->w = 1.0f;
    rot->w = 1.0f;
    rot->y = yaw;
    rot->z = 0.0f;
    rot->x = 0.0f;
    return 1;
}

/* What a stage change leads to: 0xF, 3 or -1 by the stage's change kind. */
s32 BtlStage_GetChangeTarget(void) {
    s32 kind;

    if (!BtlStage_IsReady()) {
        return -1;
    }
#ifdef PORT
    /* B14 stores the actual destination as a signed byte, rather than BT3's
       two-value destruction category. This also covers the added maps. */
    if (PortGame_IsBT4()) {
        kind = ((s8 *)gBtlStage->data->info)[0xD];
        return kind >= 0 && kind < PortGame_StageCount() ? kind : -1;
    }
#endif
    kind = gBtlStage->data->info->changeKind;
    if (kind == 1) {
        return 0xF;
    }
    if (kind == 2) {
        return 3;
    }
    return -1;
}

/* Advances the stage timers; raises bit 2 for the frame a timer wraps. */
void BtlStage_UpdateTimers(void) {
    u32 i;
    StgTimer *base;
    StgTimer *t;

    if (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE) {
        return;
    }
    if (gBtlStage->timerCount == 0) {
        return;
    }
    base = gBtlStage->timers;
    for (i = 0; i < gBtlStage->timerCount; i++) {
        t = &base[i];
        t->time += 0.13333333f;
        t->flags &= ~4;
        if (t->period < t->time) {
            t->flags |= 4;
            t->time = 0.0f;
        }
    }
}

/* Per-frame stage update. */
void BtlStage_Update(void) {
    if (Battle_GetWork()->flags & BATTLE_FLAG_LOADING) {
        return;
    }
    BtlStage_UpdateTimers();
    BtlStage_UpdateObjs();
    StgAmb_Update();
    StgModel_UpdateAnims();
    StgRigid_Update();
}

/* Sets the volume of the stage's looping sound (0..128 before the scale). */
void StgAmb_SetBgmVolume(s32 volume) {
    MapBgm_SetVolume((s32)((f32)volume * gStgAmb.bgmScale * 0.1f));
}

/* Plays a one-shot stage sound with the ambience scale applied. */
s32 StgAmb_PlaySe(u32 mask, s32 id, s32 volume, s32 pan, s32 pitch) {
    return Snd_PlaySeEx(mask, id, (s32)((f32)volume * gStgAmb.seScale), pan, pitch);
}

/* 1 up to `near`, falling linearly to 0 at `far`. */
f32 StgAmb_Falloff(f32 x, f32 near, f32 far) {
    f32 ret;

    if (far < x) {
        ret = 0.0f;
    } else if (near < x) {
        ret = 1.0f - (x - near) / (far - near);
    } else {
        ret = 1.0f;
    }
    return ret;
}

/* Stage 0: constant volume. */
void StgAmb_Update00(void) {
    StgAmb_SetBgmVolume(0x50);
}

/* Stage 1: constant volume. */
void StgAmb_Update01(void) {
    StgAmb_SetBgmVolume(0x50);
}

/* Stage 2: volume by the camera's height above or below the water level. */
void StgAmb_Update02(void) {
    Mtx44 m;
    f32 h[4];
    View *view = gBtlCamView;

    if (view != NULL) {
        f32 vol;

        Mtx_InverseRT(&m, &view->world2view2);
        BtlStage_GetWaterLevel(h);
        if (m.m[3][1] < h[0]) {
            vol = StgAmb_Falloff(h[0] - m.m[3][1], 20.0f, 180.0f) * 128.0f;
        } else {
            vol = StgAmb_Falloff(m.m[3][1] - h[0], 20.0f, 250.0f) * 96.0f;
        }
        StgAmb_SetBgmVolume((s32)vol);
    }
}

/* Stage 3: random thunder, volume by the camera's distance from the water level. */
void StgAmb_Update03(void) {
    Mtx44 m;
    f32 h[4];
    StgAmb *amb = &gStgAmb;
    View *view = gBtlCamView;

    if (amb->timer == 0) {
        amb->timer = Rand_IntRange(0, 0xFFFF) % 90 + 90;
    }
    amb->timer--;
    if (amb->timer <= 0) {
        s32 vol;
        s32 pan;

        amb->timer = Rand_IntRange(0, 0xFFFF) % 90 + 90;
        vol = Rand_IntRange(0x3F, 0x7F);
        pan = Rand_IntRange(-15, 15);
        StgAmb_PlaySe(8, 10, vol, pan, 0);
    }
    if (view != NULL) {
        f32 x;

        Mtx_InverseRT(&m, &view->world2view2);
        BtlStage_GetWaterLevel(h);
        if (m.m[3][1] < h[0]) {
            x = h[0] - m.m[3][1];
        } else {
            x = m.m[3][1] - h[0];
        }
        StgAmb_SetBgmVolume((s32)(StgAmb_Falloff(x, 0.0f, 700.0f) * 128.0f));
    }
}

/* Stage 4: random thunder at a quarter volume. */
void StgAmb_Update04(void) {
    StgAmb *amb = &gStgAmb;

    if (amb->timer == 0) {
        amb->timer = Rand_IntRange(0, 0xFFFF) % 90 + 90;
    }
    amb->timer--;
    if (amb->timer <= 0) {
        s32 vol;
        s32 pan;

        amb->timer = Rand_IntRange(0, 0xFFFF) % 90 + 90;
        vol = Rand_IntRange(0x3F, 0x7F);
        pan = Rand_IntRange(-15, 15);
        StgAmb_PlaySe(8, 10, vol >> 2, pan, 0);
    }
    StgAmb_SetBgmVolume(0x3F);
}

/* Stage 5: constant volume. */
void StgAmb_Update05(void) {
    StgAmb_SetBgmVolume(0x50);
}

/* Stage 6: constant volume. */
void StgAmb_Update06(void) {
    StgAmb_SetBgmVolume(0x50);
}

/* Stage 7: constant volume. */
void StgAmb_Update07(void) {
    StgAmb_SetBgmVolume(0x40);
}

/* Stage 8: constant volume. */
void StgAmb_Update08(void) {
    StgAmb_SetBgmVolume(0x50);
}

/* Stage 9: constant volume. */
void StgAmb_Update09(void) {
    StgAmb_SetBgmVolume(0x80);
}

/* Stage 10: random thunder; volume by height over the water level and by the distance to a path of 8 points. */
void StgAmb_Update10(void) {
    static const StgAmbPath sPath = { {
        { -986.8f, 0.0f, -199.5f, 1.0f }, { -652.5f, 0.0f, -100.6f, 1.0f },
        { -412.8f, 0.0f, 174.4f, 1.0f },  { -84.31f, 0.0f, 308.1f, 1.0f },
        { 284.91f, 0.0f, 305.2f, 1.0f },  { 549.46f, 0.0f, 98.8f, 1.0f },
        { 793.67f, 0.0f, -139.5f, 1.0f }, { 1017.5f, 0.0f, -331.4f, 1.0f },
    } };
    Mtx44 m;
    StgAmbPath pts;
    Vec4 pos;
    f32 h[4];
    StgAmb *amb = &gStgAmb;
    View *view = gBtlCamView;
    f32 k;
    f32 best;
    f32 d;
    s32 i;

    if (amb->timer == 0) {
        amb->timer = Rand_IntRange(0, 0xFFFF) % 90 + 90;
    }
    amb->timer--;
    if (amb->timer <= 0) {
        s32 vol;
        s32 pan;

        amb->timer = Rand_IntRange(0, 0xFFFF) % 90 + 90;
        vol = Rand_IntRange(0x3F, 0x7F);
        pan = Rand_IntRange(-15, 15);
        StgAmb_PlaySe(8, 10, vol >> 2, pan, 0);
    }
    if (view != NULL) {
        Mtx_InverseRT(&m, &view->world2view2);
        BtlStage_GetWaterLevel(h);
        if (m.m[3][1] < h[0]) {
            k = StgAmb_Falloff(h[0] - m.m[3][1], 20.0f, 180.0f);
        } else {
            k = StgAmb_Falloff(m.m[3][1] - h[0], 20.0f, 250.0f);
        }
        pts = sPath;
        best = 0.0f;
        Vec4_Copy(&pos, (Vec4 *)m.m[3]);
        pos.y = 0.0f;
        for (i = 0; i < 8; i++) {
            d = Vec3_Dist(&pts.p[i], &pos);
            if (i == 0) {
                best = d;
            } else if (d < best) {
                best = d;
            }
        }
        k *= StgAmb_Falloff(best, 280.0f, 550.0f);
        StgAmb_SetBgmVolume((s32)(k * 80.0f));
    }
}

/* Stage 11: louder near the water level. */
void StgAmb_Update11(void) {
    Mtx44 m;
    f32 h[4];
    View *view = gBtlCamView;

    if (view != NULL) {
        f32 x;

        Mtx_InverseRT(&m, &view->world2view2);
        BtlStage_GetWaterLevel(h);
        if (m.m[3][1] < h[0]) {
            x = h[0] - m.m[3][1];
        } else {
            x = m.m[3][1] - h[0];
        }
        StgAmb_SetBgmVolume((s32)(StgAmb_Falloff(x, 20.0f, 150.0f) * 128.0f));
    }
}

/* Stage 12: louder near the water level. */
void StgAmb_Update12(void) {
    Mtx44 m;
    f32 h[4];
    View *view = gBtlCamView;

    if (view != NULL) {
        f32 x;

        Mtx_InverseRT(&m, &view->world2view2);
        BtlStage_GetWaterLevel(h);
        if (m.m[3][1] < h[0]) {
            x = h[0] - m.m[3][1];
        } else {
            x = m.m[3][1] - h[0];
        }
        StgAmb_SetBgmVolume((s32)(StgAmb_Falloff(x, 20.0f, 300.0f) * 96.0f));
    }
}

/* Stage 13: constant volume. */
void StgAmb_Update13(void) {
    StgAmb_SetBgmVolume(0);
}

/* Stage 14: louder near the water level. */
void StgAmb_Update14(void) {
    Mtx44 m;
    f32 h[4];
    View *view = gBtlCamView;

    if (view != NULL) {
        f32 x;

        Mtx_InverseRT(&m, &view->world2view2);
        BtlStage_GetWaterLevel(h);
        if (m.m[3][1] < h[0]) {
            x = h[0] - m.m[3][1];
        } else {
            x = m.m[3][1] - h[0];
        }
        StgAmb_SetBgmVolume((s32)(StgAmb_Falloff(x, 20.0f, 1200.0f) * 96.0f));
    }
}

/* Stage 15: random thunder. */
void StgAmb_Update15(void) {
    StgAmb *amb = &gStgAmb;

    if (amb->timer == 0) {
        amb->timer = Rand_IntRange(0, 0x10000) % 90 + 90;
    }
    amb->timer--;
    if (amb->timer <= 0) {
        s32 vol;
        s32 pan;

        amb->timer = Rand_IntRange(0, 0x10000) % 90 + 90;
        vol = Rand_IntRange(0x5F, 0x7F);
        pan = Rand_IntRange(-15, 15);
        StgAmb_PlaySe(8, 10, vol, pan, 0);
    }
    StgAmb_SetBgmVolume(0x60);
}

/* Stage 16: constant volume. */
void StgAmb_Update16(void) {
    StgAmb_SetBgmVolume(0x7F);
}

/* Stage 17: constant volume. */
void StgAmb_Update17(void) {
    StgAmb_SetBgmVolume(0x7F);
}

/* Stage 18: constant volume. */
void StgAmb_Update18(void) {
    StgAmb_SetBgmVolume(0x7F);
}

/* Stage 19: constant volume. */
void StgAmb_Update19(void) {
    StgAmb_SetBgmVolume(0x7F);
}

/* Stage 20: constant volume. */
void StgAmb_Update20(void) {
    StgAmb_SetBgmVolume(0x7F);
}

/* Stage 21: constant volume. */
void StgAmb_Update21(void) {
    StgAmb_SetBgmVolume(0x7F);
}

/* Stage 22: constant volume. */
void StgAmb_Update22(void) {
    StgAmb_SetBgmVolume(0x7F);
}

/* Starts the ambience of the current stage: picks the handler and starts the looping sound, silent. */
void StgAmb_Start(void) {
    s32 id;

    memset(&gStgAmb, 0, sizeof(StgAmb));
    gStgAmb.bgmScale = 1.0f;
    gStgAmb.seScale = 1.0f;
    id = BtlStage_GetId();
    switch (id) {
    case 0:
    case 23:
    case 24:
        gStgAmb.update = StgAmb_Update00;
        break;
    case 1:
    case 25:
    case 26:
        gStgAmb.update = StgAmb_Update01;
        break;
    case 2:
        gStgAmb.update = StgAmb_Update02;
        break;
    case 3:
        gStgAmb.update = StgAmb_Update03;
        break;
    case 4:
    case 27:
        gStgAmb.update = StgAmb_Update04;
        break;
    case 5:
        gStgAmb.update = StgAmb_Update05;
        break;
    case 6:
    case 28:
        gStgAmb.update = StgAmb_Update06;
        break;
    case 7:
        gStgAmb.update = StgAmb_Update07;
        break;
    case 8:
        gStgAmb.update = StgAmb_Update08;
        break;
    case 9:
    case 29:
    case 30:
        gStgAmb.update = StgAmb_Update09;
        break;
    case 10:
        gStgAmb.update = StgAmb_Update10;
        break;
    case 11:
        gStgAmb.update = StgAmb_Update11;
        break;
    case 12:
        gStgAmb.update = StgAmb_Update12;
        break;
    case 13:
        gStgAmb.update = StgAmb_Update13;
        break;
    case 14:
        gStgAmb.update = StgAmb_Update14;
        break;
    case 15:
        gStgAmb.update = StgAmb_Update15;
        break;
    case 16:
        gStgAmb.update = StgAmb_Update16;
        break;
    case 17:
        gStgAmb.update = StgAmb_Update17;
        break;
    case 18:
        gStgAmb.update = StgAmb_Update18;
        break;
    case 19:
        gStgAmb.update = StgAmb_Update19;
        break;
    case 20:
        gStgAmb.update = StgAmb_Update20;
        break;
    case 21:
        gStgAmb.update = StgAmb_Update21;
        break;
    case 22:
        gStgAmb.update = StgAmb_Update22;
        break;
    }
    MapBgm_Play(id + DISC_RESOURCE(0x10BA4, 0x916));
    StgAmb_SetBgmVolume(0);
}

/* Runs the stage's ambience handler. */
void StgAmb_Update(void) {
    if (gStgAmb.update != NULL) {
        gStgAmb.update();
    }
}

/* Silences and stops the looping sound. */
void StgAmb_Stop(void) {
    StgAmb_SetBgmVolume(0);
    MapBgm_Stop();
}

/* Drops the handler, then silences and stops the looping sound. */
void StgAmb_Term(void) {
    gStgAmb.update = NULL;
    StgAmb_SetBgmVolume(0);
    MapBgm_Stop();
}

/* Starts the ambience. */
void StgAmb_Init(void) {
    StgAmb_Start();
}

/* Sets the scale of the looping sound's volume. */
void StgAmb_SetBgmScale(f32 scale) {
    gStgAmb.bgmScale = scale;
}

/* Sets the scale of the one-shot sounds' volume. */
void StgAmb_SetSeScale(f32 scale) {
    gStgAmb.seScale = scale;
}

/* Allocates the two capture buffers when they do not exist. */
void ScrXfade_AllocBuffers(void) {
    if (gScrXfade->buf[0] == NULL) {
        gScrXfade->buf[0] = Heap_Alloc(0x60090, 0x20, 0, 2);
        gScrXfade->buf[1] = Heap_Alloc(0x60090, 0x20, 0, 2);
    }
}

/* Frees the two capture buffers. */
void ScrXfade_FreeBuffers(void) {
    if (gScrXfade->buf[0] != NULL) {
        Heap_Free(gScrXfade->buf[0]);
        Heap_Free(gScrXfade->buf[1]);
        gScrXfade->buf[0] = NULL;
        gScrXfade->buf[1] = NULL;
    }
}

/* Reads the frame being shown back from the GS, upper and lower half. */
void ScrXfade_Capture(void) {
    s32 fbp = 0xE00;

    if (!(gGfx.frame & 1)) {
        fbp = 0;
    }
    sceGsSyncPath(0, 0);
    ScrXfade_StoreHalf(fbp, 0x200, 0xE0, 0);
    ScrXfade_StoreHalf(fbp + 0x700, 0x200, 0xE0, 1);
}

/* Reads w x h pixels at frame block `sbp` into buffer `index` and builds the packet that uploads them again. */
void ScrXfade_StoreHalf(s16 sbp, u16 w, u16 h, s32 index) {
    u8 si[0x70];
    ScrXfade *xf = gScrXfade;
    u64 *p = xf->buf[index];
    u8 *e = (u8 *)xf + index * sizeof(ScrXfadeChain);
    u32 *tag = (u32 *)(e + 0x10); /* chain[index].ref */
    s32 qwc;

    sceGsSetDefStoreImage(si, sbp, w >> 6, 1, 0, 0, w, h);
    FlushCache(0);
    sceGsExecStoreImage(si, (u8 *)p + 0x70);
    sceGsSyncPath(0, 0);
    Dbg_AfterStoreImage();
    qwc = w * h * 3 / 16;
    tag[0] = (qwc + 9) | DMA_TAG_REF;
    tag[1] = (u32)p;
    tag[2] = 0;
    tag[3] = 0;
    tag = (u32 *)(e + 0x20); /* chain[index].end */
    tag[0] = DMA_TAG_END;
    tag[1] = 0;
    tag[2] = 0;
    tag[3] = 0;
    ((u32 *)p)[3] = (qwc + 8) | VIF_DIRECT;
    ((u32 *)p)[0] = 0;
    ((u32 *)p)[1] = 0;
    ((u32 *)p)[2] = 0;
    p += 2;
    p[0] = GIF_TAG(4, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[1] = 0x50; /* BITBLTBUF: filled in by ScrXfade_Draw */
    p[0] = 0;
    p += 2;
    p[1] = 0x51; /* TRXPOS */
    p[0] = 0;
    p += 2;
    p[0] = (u64)w | ((u64)h << 32);
    p[1] = 0x52; /* TRXREG */
    p += 2;
    p[1] = 0x53; /* TRXDIR: host to local */
    p[0] = 0;
    p += 2;
    p[0] = (u64)(u32)(qwc | GIF_EOP) | (2ULL << 58); /* image data */
    p[1] = 0;
    p += 2;
    p += qwc * 2;
    p[0] = GIF_TAG(1, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[1] = GS_TEXFLUSH;
    p[0] = 0;
}

/* Uploads the captured halves as a 512 x 448 texture at block `tbp` and draws it over the frame. */
void ScrXfade_Draw(s32 unused, s32 tbp, u8 alpha) {
    u64 *p;
    s32 i;
    s32 dbp;
    s32 bw;
    s32 w, h;

    /* w and h as variables: `w * h / 64` keeps 0x700 in a register (hoisted by the second loop pass) */
    w = 0x200;
    h = 0xE0;
    dbp = 0;
    bw = w >> 6;
    for (i = 0; i < 2; i++) {
        switch (i) {
        case 0:
            dbp = tbp;
            break;
        case 1:
            dbp = tbp + w * h / 64;
            break;
        }
        gScrXfade->buf[i][4] = ((u64)dbp << 32) | ((u64)bw << 48) | 0x0100000001000000ULL;
        Dma_AddData(&gScrXfade->chain[i], 0x10);
    }
    p = Dma_BeginDirect();
    Gfx_PutDefaultEnv((GsQword **)&p);
    p[0] = GIF_TAG(5, 1, 1);
    p[1] = GIF_REG_AD;
    p += 2;
    p[0] = 0x30000;
    p[1] = GS_TEST_1;
    p += 2;
    p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    p[1] = GS_ZBUF_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEX1_1;
    p += 2;
    p[0] = 0;
    p[1] = GS_TEXFLUSH;
    p += 2;
    p[0] = (u64)(u32)tbp | ((u64)bw << 14) | (0xA641ULL << 20);
    p[1] = GS_TEX0_1;
    p += 2;
    Dma_PutTexStrips((GsQword **)&p, 0, 0, 0x200, 0x1C0, 0, 0, 0, 0, 0x200, 0x1C0, 8, 8, (alpha << 24) | 0x808080, 1);
    Dma_EndDirect(p);
}

/* Allocates and clears the cross-fade state. */
void ScrXfade_Init(void) {
    gScrXfade = Heap_Alloc(sizeof(ScrXfade), 0x20, 0, 2);
    memset(gScrXfade, 0, sizeof(ScrXfade));
    ScrXfade_Reset();
}

/* Frees the buffers and the state. */
void ScrXfade_Term(void) {
    if (gScrXfade != NULL) {
        ScrXfade_FreeBuffers();
        Heap_Free(gScrXfade);
        gScrXfade = NULL;
    }
}

/* Stops any fade and frees the buffers. */
void ScrXfade_Reset(void) {
    gScrXfade->alpha = 0;
    gScrXfade->request = 0;
    Ramp_Stop(&gScrXfade->ramp);
    ScrXfade_FreeBuffers();
}

/* Steps the fade (not while paused) and draws the captured frame while its alpha is not 0. */
void ScrXfade_Update(void) {
    if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
        if (Ramp_Step(&gScrXfade->ramp)) {
            gScrXfade->alpha = 0;
            gScrXfade->request = 0;
            ScrXfade_FreeBuffers();
        } else {
            gScrXfade->alpha = 128.0f - gScrXfade->ramp.value * 128.0f;
        }
    }
    if (gScrXfade->alpha != 0) {
        ScrXfade_Draw(0, 0x1C00, gScrXfade->alpha);
    }
}

/* Captures the finished frame when a capture was asked for. */
void ScrXfade_PostDraw(void) {
    if (gScrXfade->request == 1) {
        ScrXfade_AllocBuffers();
        ScrXfade_Capture();
        gScrXfade->request = 0;
    }
}

/* Starts the fade ramp over `seconds` and stores the request word. */
void ScrXfade_Start(s32 request, f32 seconds) {
    Ramp_Start(&gScrXfade->ramp, seconds, 0.0f, 1.0f);
    gScrXfade->request = request;
}

/* Asks for a capture at the end of this frame. */
void ScrXfade_RequestCapture(void) {
    gScrXfade->request = 1;
}

/* Tells whether the captured frame is being drawn. */
s32 ScrXfade_IsActive(void) {
    return gScrXfade->alpha != 0;
}

/* Starts the packet: copies the frame region x0..x1 at half size into the work buffer at block 0x2A00, then
   points the frame, scissor, clamp and texture registers at drawing that copy back over the region. */
void ScrWarp_BeginPacket(u64 **pp, s32 x0, s32 x1, s32 w, s32 h, s32 tw, s32 th) {
    *pp = Dma_BeginDirect();
    Gfx_PutDefaultEnv((GsQword **)pp);
    (*pp)[0] = GIF_TAG(7, 0, 1);
    (*pp)[1] = GIF_REG_AD;
    *pp += 2;
    (*pp)[0] = 0x30000;
    (*pp)[1] = GS_TEST_1;
    *pp += 2;
    (*pp)[0] = GS_SET_SCISSOR(0, tw - 1, 0, th - 1);
    (*pp)[1] = GS_SCISSOR_1;
    *pp += 2;
    (*pp)[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
    (*pp)[1] = GS_ZBUF_1;
    *pp += 2;
    (*pp)[0] = GS_SET_FRAME(0x150, 4, 0, 0);
    (*pp)[1] = GS_FRAME_1;
    *pp += 2;
    (*pp)[0] = GS_SET_CLAMP(2, 2, x0, x1 - 1, 0, h - 1);
    (*pp)[1] = GS_CLAMP_1;
    *pp += 2;
    (*pp)[0] = 0x60;
    (*pp)[1] = GS_TEX1_1;
    *pp += 2;
    (*pp)[0] = !(gGfx.frame & 1) ? 0x264020E00ULL : 0x264020000ULL;
    (*pp)[1] = GS_TEX0_1;
    *pp += 2;
    Dma_PutTexStrips((GsQword **)pp, 0, 0, tw, th, 0, 0, x0, 0, w + x0, h, 8, 8, 0x80808080, 0);
    (*pp)[0] = GIF_TAG(4, 0, 1);
    (*pp)[1] = GIF_REG_AD;
    *pp += 2;
    (*pp)[0] = !(gGfx.frame & 1) ? 0x80070 : 0x80000;
    (*pp)[1] = GS_FRAME_1;
    *pp += 2;
    (*pp)[0] = GS_SET_SCISSOR(x0, x1 - 1, 0, h - 1);
    (*pp)[1] = GS_SCISSOR_1;
    *pp += 2;
    (*pp)[0] = GS_SET_CLAMP(2, 2, 0, tw - 1, 0, th - 1);
    (*pp)[1] = GS_CLAMP_1;
    *pp += 2;
    (*pp)[0] = 0x220012A00ULL;
    (*pp)[1] = GS_TEX0_1;
    *pp += 2;
}

/* Sends the packet built by the ScrWarp_Put* helpers. */
void ScrWarp_EndPacket(u64 **pp) {
    Dma_EndDirect((*pp));
}

/* Starts a textured, blended triangle strip of `count` vertices. */
void ScrWarp_BeginStrip(u64 **pp, s32 count) {
    (*pp)[0] = GIF_TAG(1, 0, 1);
    (*pp)[1] = GIF_REG_AD;
    (*pp) += 2;
    (*pp)[0] = 0x15C; /* triangle strip, gouraud, textured, blended, UV coordinates */
    (*pp)[1] = GS_PRIM;
    (*pp) += 2;
    (*pp)[0] = GIF_TAG_EX(count, 1, GIF_FLG_REGLIST, 4);
    (*pp)[1] = 0x531F; /* NOP, RGBAQ, UV, XYZ2 */
    (*pp) += 2;
}

/* Ends a strip: nothing to do. */
void ScrWarp_EndStrip(u64 **pp) {
}

/* Adds one strip vertex: frame pixel, texel and colour. */
void ScrWarp_PutVertex(u64 **pp, s32 x, s32 y, s32 u, s32 v, s32 rgba) {
    (*pp)[0] = 0;
    (*pp)[1] = (u64)rgba | (0x3F800000ULL << 32);
    (*pp) += 2;
    (*pp)[0] = GS_SET_UV(u << 4, v << 4);
    (*pp)[1] = GS_SET_UV((x << 4) + 0x7000, (y << 4) + 0x7200);
    (*pp) += 2;
}

/* Takes a free object and counts it for `view`. */
ScrWarpObj *ScrWarp_Alloc(s32 view) {
    ScrWarp *mgr = gScrWarp;
    ScrWarpObj *found = NULL;
    ScrWarpObj *o;
    s32 i;

    o = mgr->objs;
    for (i = 0; i < SCRWARP_MAX; i++, o++) {
        if (o->flags == 0) {
            found = o;
            break;
        }
    }
    if (found != NULL) {
        if (view == 0) {
            mgr->count[0]++;
        } else {
            mgr->count[1]++;
        }
    }
    return found;
}

/* Releases an object. */
void ScrWarp_Free(ScrWarpObj *o) {
    o->flags = 0;
    if (o->view == 0) {
        gScrWarp->count[0]--;
    } else {
        gScrWarp->count[1]--;
    }
}

/* Tells whether a projected point is in front of the camera and inside the 512 x 448 frame. */
s32 ScrWarp_IsOnScreen(Vec4 *screen) {
    if (screen->z <= 0.0f) {
        return 0;
    }
    if (2304.0f <= screen->x) {
        return 0;
    }
    if (screen->x <= 1792.0f) {
        return 0;
    }
    if (2272.0f <= screen->y) {
        return 0;
    }
    if (screen->y <= 1824.0f) {
        return 0;
    }
    return 1;
}

/* One frame of a shock wave: grows the ring and rebuilds its vertices; frees it when its time or size is up. */
void ScrWarp_Step(ScrWarpObj *o) {
    f32 r0;
    f32 r1;
    f32 r2;
    f32 rj;
    f32 *dir;
    s32 i;

    o->life -= 1.0f;
    if (o->life <= 0.0f) {
        ScrWarp_Free(o);
        return;
    }
    if (600.0f < o->radius) {
        ScrWarp_Free(o);
        return;
    }
    r0 = o->radius + o->speed;
    o->radius = r0;
    r2 = r0 + o->width;
    r1 = r0 + o->width * 0.5f;
    dir = gScrWarp->dir[0];
    for (i = 0; i < SCRWARP_DIRS; i++, dir += 2) {
        o->uv[i][0][0] = (o->screen.x + dir[0] * r0) * 0.5f;
        o->uv[i][0][1] = (o->screen.y + dir[1] * r0) * 0.5f;
        o->uv[i][1][0] = (o->screen.x + dir[0] * r1) * 0.5f;
        o->uv[i][1][1] = (o->screen.y + dir[1] * r1) * 0.5f;
        o->uv[i][2][0] = (o->screen.x + dir[0] * r2) * 0.5f;
        o->uv[i][2][1] = (o->screen.y + dir[1] * r2) * 0.5f;
        rj = r0 + o->width * o->jitter[i];
        o->xy[i][0][0] = o->cx + dir[0] * r0;
        o->xy[i][0][1] = o->cy + dir[1] * r0;
        o->xy[i][1][0] = o->cx + dir[0] * rj;
        o->xy[i][1][1] = o->cy + dir[1] * rj;
        o->xy[i][2][0] = o->cx + dir[0] * r2;
        o->xy[i][2][1] = o->cy + dir[1] * r2;
        if (o->uv[i][0][0] < 2.0f) {
            o->uv[i][0][0] = 2.0f;
        }
        if (o->uv[i][0][1] < 0.0f) {
            o->uv[i][0][1] = 0.0f;
        }
        if (o->uv[i][1][0] < 2.0f) {
            o->uv[i][1][0] = 2.0f;
        }
        if (o->uv[i][1][1] < 0.0f) {
            o->uv[i][1][1] = 0.0f;
        }
        if (o->uv[i][2][0] < 2.0f) {
            o->uv[i][2][0] = 2.0f;
        }
        if (o->uv[i][2][1] < 0.0f) {
            o->uv[i][2][1] = 0.0f;
        }
    }
    o->speed += o->speedAcc;
    o->width += o->widthVel;
    if (o->life <= 6.0f) {
        o->alpha += o->alphaVel;
        if (o->alpha < 0.0f) {
            o->alpha = 0.0f;
        }
    }
}

/* Draws the rings of one view (or of both when not split): two strips of 21 vertex pairs per ring. */
void ScrWarp_Draw(s32 split, s32 view) {
    s32 alpha[3];
    u64 *p;
    ScrWarpObj *o;
    s32 x0;
    s32 x1;
    s32 w;
    s32 tw;
    s32 left;
    s32 rgb;
    s32 j;
    s32 jn;
    s32 k;
    s32 m;
    s32 j1 = 0; /* dead initialiser: keeps j1 a separate register from jn (the original has `move s5,s3`) */
    s32 n;

    x0 = 0;
    w = 0x200;
    tw = 0x100;
    x1 = 0x200;
    if (split) {
        w /= 2;
        x0 = w;
        tw /= 2;
        if (view != 1) {
            x0 -= w; /* not `x0 = 0`: the original copies w and then clears it with movn */
        }
        x1 = x0 + w;
    }
    ScrWarp_BeginPacket(&p, x0, x1, w, 0x1C0, tw, 0xE0);
    o = gScrWarp->objs;
    for (left = SCRWARP_MAX - 1; left >= 0; left--, o++) {
        if (o->flags == 0) {
            continue;
        }
        if (split != 0 && o->view != view) {
            continue;
        }
        j = 0; /* j and n are set ahead of the alpha values: decides the registers of n and the alpha pointers */
        n = SCRWARP_DIRS;
        rgb = 0x808080;
        alpha[1] = o->alpha * 255.0f;
        alpha[0] = alpha[1] / 8;
        alpha[2] = alpha[1] / 8;
        for (; j < 2; j++) {
            jn = j + 1;
            ScrWarp_BeginStrip(&p, (SCRWARP_DIRS + 1) * 2);
            for (k = 0; k < SCRWARP_DIRS + 1; k++) {
                s32 *a0 = &alpha[j];
                s32 *a1 = &alpha[jn];
                j1 = jn;

                m = k % n;
                ScrWarp_PutVertex(&p, o->xy[m][j][0], o->xy[m][j][1], o->uv[m][j][0], o->uv[m][j][1],
                                  (*a0 << 24) | rgb);
                ScrWarp_PutVertex(&p, o->xy[m][j1][0], o->xy[m][j1][1], o->uv[m][j1][0], o->uv[m][j1][1],
                                  (*a1 << 24) | rgb);
            }
            ScrWarp_EndStrip(&p);
        }
    }
    ScrWarp_EndPacket(&p);
}

/* Allocates the manager and its objects and fills the direction table. */
void ScrWarp_Init(void) {
    s32 i;
    f32 a;

    gScrWarp = Heap_Alloc(sizeof(ScrWarp), 0x20, 0, 2);
    memset(gScrWarp, 0, sizeof(ScrWarp));
    a = -3.14159265f;
    gScrWarp->objs = Heap_Alloc(SCRWARP_MAX * sizeof(ScrWarpObj), 0x20, 0, 2);
    memset(gScrWarp->objs, 0, SCRWARP_MAX * sizeof(ScrWarpObj));
    for (i = 0; i < SCRWARP_DIRS; i++) {
        gScrWarp->dir[i][0] = Mathf_Sin(a);
        gScrWarp->dir[i][1] = Mathf_Cos(a);
        a += 0.314159265f;
        if (3.14159265f <= a) {
            a -= 6.2831853f;
        }
    }
}

/* Frees the objects and the manager. */
void ScrWarp_Term(void) {
    if (gScrWarp != NULL) {
        Heap_Free(gScrWarp->objs);
        Heap_Free(gScrWarp);
        gScrWarp = NULL;
    }
}

/* Releases every object. */
void ScrWarp_Reset(void) {
    ScrWarpObj *o = gScrWarp->objs;
    s32 i;

    for (i = 0; i < SCRWARP_MAX; i++, o++) {
        o->flags = 0;
    }
    gScrWarp->count[0] = 0;
    gScrWarp->count[1] = 0;
}

/* Projects new objects with their view's camera (dropping those off screen) and steps all of them. */
void ScrWarp_UpdateAll(void) {
    Mtx44 m;
    Vec4 v;
    ScrWarpObj *o = gScrWarp->objs;
    s32 i;
    f32 x;
    f32 y;

    for (i = 0; i < SCRWARP_MAX; i++, o++) {
        if (o->flags == 0) {
            continue;
        }
        if (!(o->flags & 2)) {
            if (Battle_IsSplitScreen() && !BtlScene_IsSingleView()) {
                Mtx_Copy(&m, &BtlCam_GetNativeView(o->view)->view.world2screen);
            } else {
                Vu0Screen_StoreMtx(&m);
            }
            Mtx_ProjectInt(&v, &m, &o->pos);
            IVec4_ToFloat(&o->screen, &v);
            if (!ScrWarp_IsOnScreen(&o->screen)) {
                ScrWarp_Free(o);
                continue;
            }
            o->screen.x -= 1792.0f;
            y = o->screen.y - 1824.0f;
            o->screen.y = y;
            x = o->screen.x;
            o->cy = y;
            o->cx = x;
            if (Battle_IsSplitScreen() && !BtlScene_IsSingleView() && o->view == 1) {
                o->screen.x -= 256.0f;
            }
            o->flags |= 2;
        }
        ScrWarp_Step(o);
    }
}

/* Draws the objects: one pass full screen, or one pass per view that has objects. */
void ScrWarp_DrawAll(void) {
    if (!Battle_IsSplitScreen() || BtlScene_IsSingleView()) {
        ScrWarp_Draw(0, 0);
    } else {
        if (gScrWarp->count[0] > 0) {
            ScrWarp_Draw(1, 0);
        }
        if (gScrWarp->count[1] > 0) {
            ScrWarp_Draw(1, 1);
        }
    }
}

/* Per-frame entry: steps (not while paused) and draws while any object is in use. */
void ScrWarp_Update(void) {
    if (gScrWarp->count[0] > 0 || gScrWarp->count[1] > 0) {
        if (!(Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)) {
            ScrWarp_UpdateAll();
        }
        ScrWarp_DrawAll();
    }
}

#define SCRWARP_ALPHA (128.0f / 255.0f)

/* Starts a shock wave at a world position for one view. Returns 0 when all 10 objects are in use. */
s32 ScrWarp_Spawn(s32 view, Vec4 *pos, f32 seconds, f32 radius, f32 width, f32 speed, f32 jitter) {
    ScrWarpObj *o = ScrWarp_Alloc(view);
    s32 i;
    f32 base;

    if (o == NULL) {
        return 0;
    }
    o->view = view;
    o->flags |= 1;
    o->life = seconds * 30.0f;
    Vec4_Copy(&o->pos, pos);
    o->radius = radius;
    o->alpha = SCRWARP_ALPHA;
    o->alphaVel = -SCRWARP_ALPHA / 6.0f;
    o->width = width;
    o->speed = speed;
    o->speedAcc = -(speed * 0.3f / o->life);
    o->widthVel = width * 3.2f / o->life;
    base = 0.5f - jitter * 0.5f;
    for (i = 0; i < SCRWARP_DIRS; i++) {
        o->jitter[i] = base + Rand_Float01() * jitter;
    }
    return 1;
}

/* Sets up the texture object of the table and remembers where its entries are. */
void StgFog_SetupTex(StgFog *tone, u16 arg) {
    GfxClut_InitPacket(tone, arg);
    tone->clut = tone->clutSrc;
}

/* Builds the alpha table from the curve through the control points. */
void StgFog_BuildClut(StgFog *tone, StgFogPoint *pts, s32 clearLast, f32 scale) {
    f32 curve[256];
    u8 *clut = tone->clut;
    s32 i;

    memset(curve, 0, sizeof(curve));
    StgCurve_Build(pts, curve);
    for (i = 0; i < 256; i++) {
        s32 idx = (i & 0xE7) | ((i & 8) << 1) | ((i & 0x10) >> 1);

        clut[(idx << 2) + 0] = 0;
        clut[(idx << 2) + 1] = 0;
        clut[(idx << 2) + 2] = 0;
        clut[(idx << 2) + 3] = (u32)(curve[i] * 128.0f * scale);
    }
    if (clearLast) {
        clut[0x3FC] = 0;
        clut[0x3FD] = 0;
        clut[0x3FE] = 0;
        clut[0x3FF] = 0;
    }
}

/* Allocates the depth tone state. */
void StgFog_Init(u16 arg) {
    gStgFog = Heap_Alloc(sizeof(StgFog), 0x20, 0, 2);
    memset(gStgFog, 0, sizeof(StgFog));
    StgFog_SetupTex(gStgFog, arg);
    StgFog_ResetColor();
}

/* Frees the depth tone state. */
void StgFog_Term(void) {
    if (gStgFog != NULL) {
        Heap_Free(gStgFog);
        gStgFog = NULL;
    }
}

/* Sets the draw colour back to neutral grey. */
void StgFog_ResetColor(void) {
    gStgFog->color = 0x80808080;
}

/* Draws the depth tone: the table turns depth into the frame's alpha, the frame is copied at half size into the
   work buffer, and the copy is blended back over the frame through that alpha (a blur that grows with depth). */
/* Emits two local tables in .sdata: 0x2FEC10 = {0x2A00, 0x2D80} (the work buffers as textures) and
   0x2FEC18 = {0x150, 0x16C} (as frame buffers).
   Matching notes: as in StgBlur_Draw the sizes are VARIABLES set at the top (`width`, `half`, `srcH`, `h`) and
   passed to both Dma_PutTexStrips calls. All four are needed: they are what occupies the saved registers the
   way the original has it (448 in s8; 0x4E, 0x30000, 0x47 and the second TEX0 value pushed into s3 / s5 / s6 /
   s7 by local allocation; 0xE re-materialised twice). With literals, or with only some of the four as
   variables, the same instructions come out in another schedule (88 of 230). The TEX0 word is
   `tbp | fbw << 14 | tw << 26 | th << 30` in that order, with tw declared before th. */
#ifdef PORT
extern void Port_GsMarker(s32 effect); /* port/src/gs_marker.c */
/* The PC renderer draws page 0x150 at the PS2's resolution between these (gs_draw.c target_lowres): the blur is the
   half-size copy stretched back, and at a higher internal resolution the copy kept the detail and blurred nothing. */
#define PORT_LOWRES_BEGIN 0x20
#define PORT_LOWRES_END 0x21
#endif
void StgFog_Draw(void) {
    StgFog *tone = gStgFog;
    u64 *p;
    s32 width = 0x200;
    s32 half = 0x100;
    s32 srcH = 0x1C0;
    s32 h = 0xE0;

#ifdef PORT
    Port_GsMarker(PORT_LOWRES_BEGIN);
#endif
    Dma_AddData(tone->load, 0x10);
    Dma_AddTexFlush();
    Dma_AddFrame(!(gGfx.frame & 1) ? 0x70 : 0, 8, 0xFFFFFF);
    Dma_AddZbuf(0xE0, 1);
    GfxPost_DrawDepthClut(0, 0x1C00, tone->cbp, (0x80ULL << 32) | 0x48);
    Dma_AddZbuf(0xE0, 0);
    {
        u32 tbp[2] = { 0x2A00, 0x2D80 };
        u32 fbp[2] = { 0x150, 0x16C };
        s32 tw;
        s32 th;
        u64 fbw = 4;

        tw = Tex_Log2Size(0x100);
        th = Tex_Log2Size(0xE0);

        p = Dma_BeginDirect();
        Gfx_PutDefaultEnv((GsQword **)&p);
        p[0] = GIF_TAG(4, 0, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = (u64)fbp[0] | (fbw << 16);
        p[1] = GS_FRAME_1;
        p += 2;
        p[0] = GS_SET_ZBUF(0xE0, GS_PSMZ24, 1);
        p[1] = GS_ZBUF_1;
        p += 2;
        p[0] = 0x30000;
        p[1] = GS_TEST_1;
        p += 2;
        p[0] = !(gGfx.frame & 1) ? 0xA64020E00ULL : 0xA64020000ULL;
        p[1] = GS_TEX0_1;
        p += 2;
        Dma_PutTexStrips((GsQword **)&p, 0, 0, half, h, 0, 0, 0, 0, width, srcH, 0, 0, 0x80808080, 1);
        p[0] = GIF_TAG(3, 0, 1);
        p[1] = GIF_REG_AD;
        p += 2;
        p[0] = !(gGfx.frame & 1) ? 0xFF00000000080070ULL : 0xFF00000000080000ULL;
        p[1] = GS_FRAME_1;
        p += 2;
        p[0] = (0x80ULL << 32) | 0x54;
        p[1] = GS_ALPHA_1;
        p += 2;
        p[0] = (u64)tbp[0] | (fbw << 14) | ((u64)tw << 26) | ((u64)th << 30);
        p[1] = GS_TEX0_1;
        p += 2;
        Dma_PutTexStrips((GsQword **)&p, 0, 0, width, srcH, 0, 0, 0, 0, half, h, 8, 8, 0x80808080, 1);
        Dma_EndDirect(p);
    }
#ifdef PORT
    Port_GsMarker(PORT_LOWRES_END);
#endif
}

/* Takes the three control points from the stage's parameters (defaults when NULL) and rebuilds the table. */
void StgFog_SetParams(StgFogParams *prm) {
    u8 *pt = prm->pt;

    if (prm == NULL) {
        gStgFog->clearLast = 1;
        Vec4_Set(&gStgFog->pt[0].v, 0.0f, 0.0f, 0.0f, 1.0f);
        Vec4_Set(&gStgFog->pt[1].v, 222.0f, 0.0f, 23.0f, 1.0f);
        Vec4_Set(&gStgFog->pt[2].v, 255.0f, 0.0f, 255.0f, 1.0f);
    } else {
        gStgFog->clearLast = (prm->flags >> 1) & 1;
        Vec4_Set(&gStgFog->pt[0].v, pt[0], 0.0f, pt[1], 1.0f);
        Vec4_Set(&gStgFog->pt[1].v, pt[2], 0.0f, pt[3], 1.0f);
        Vec4_Set(&gStgFog->pt[2].v, pt[4], 0.0f, pt[5], 1.0f);
    }
    StgFog_BuildClut(gStgFog, gStgFog->pt, gStgFog->clearLast, 1.0f);
}
