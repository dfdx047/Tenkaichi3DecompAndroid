#include "battle/tag_clash.h"
#include "common.h"
#include "battle/eft_det_a.h"

/*
 * 0x1AE2A8..0x1B16F0: effect texture tail, volley aim, fighter-against-fighter volumes, the hit detection over
 * the effect hit records, and the ground probe. See include/battle/eft_det_a.h for the layouts.
 *
 * HIT DETECTION, the frame (EftDet_Update, called by Battle_Update between BtlScene_Update, where the projectile
 * tasks publish this frame's records, and BtlScene_PostUpdate, where the tasks read the results):
 *   nothing while the battle is paused (battle flag 0x100), else
 *   1. EftDet_PrepareAll    every record: hitFlags = 0, both distances = FLT_MAX, bounds / movement / zone
 *   2. EftDet_ClashPass     record against record (EftHit_CanHit mode 2)
 *   3. EftDet_StagePass     record against stage  (EftHit_CanHit mode 0)
 *   4. EftDet_FighterPass   record against fighter (EftHit_CanHit mode 1)
 * Every pass walks the list in index order (= the order the tasks created the records). Results go to the owning
 * task with EftHit_SetTaskFlag(index, bit, place): 1 hit a fighter, 2 guarded, 4 hit the stage, 8 lost a clash,
 * 0x20 absorbed, 0x40 deflected, 0x80 reflected, 0x100 beam struggle, 0x8000 multi-hit in contact.
 * The passes do not remove records: a record that lost a clash or hit the stage in this frame is still tested by
 * the later passes unless EftHit_CanHit refuses it because of the task flags the earlier pass set.
 *
 * Callees outside this file are declared here with local views.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern f32 sqrtf(f32 x);
extern f32 powf(f32 x, f32 y);
extern f32 atan2f(f32 y, f32 x);

extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void Dma_AddData(void *src, s32 size);
extern void Dma_AddRef(void *addr, s32 size);
/* Empty (a stripped assert: count <= max). It is defined right before the loaders in the original file (0x1AE140),
   so the compiler knew it touches no memory and kept the pack header and the count in registers across the call;
   the attribute stands in for that (the function is in eft_ae.c, which declares it the same way; the two files were
   not joined at integration because everything matches as it is). */
extern void EftTexSet_CheckCount(s32 count, s32 max) __attribute__((const));

extern void Vec4_Set(EftDetVec *dst, f32 x, f32 y, f32 z, f32 w);
extern void Vec4_Copy(EftDetVec *dst, EftDetVec *src);
extern void Vec4_Add(EftDetVec *dst, EftDetVec *a, EftDetVec *b);
extern void Vec4_Sub(EftDetVec *dst, EftDetVec *a, EftDetVec *b);
extern void Vec4_Scale(EftDetVec *dst, EftDetVec *src, f32 scale);
extern void Vec3_Add(EftDetVec *dst, EftDetVec *a, EftDetVec *b);
extern void Vec3_Sub(EftDetVec *dst, EftDetVec *a, EftDetVec *b);
extern void Vec3_Scale(EftDetVec *dst, EftDetVec *src, f32 scale);
extern void Vec3_Normalize(EftDetVec *dst, EftDetVec *src);
extern void Vec3_Cross(EftDetVec *dst, EftDetVec *a, EftDetVec *b);
extern f32 Vec3_Dot(EftDetVec *a, EftDetVec *b);
extern void Mtx_StoreIdentity(EftDetMtx *m);
extern void Mtx_MulVec4(EftDetVec *dst, EftDetMtx *m, EftDetVec *src);
extern void Mtx_RotateX(EftDetMtx *dst, EftDetMtx *src, f32 angle);          /* rotate about X */
extern void Mtx_RotateY(EftDetMtx *dst, EftDetMtx *src, f32 angle);          /* rotate about Y */
extern void Vec3_Copy(void *dst, EftDetVec *src);                          /* copies x, y, z */
extern void Vec3_Clamp(EftDetVec *dst, EftDetVec *src, f32 lo, f32 hi);     /* clamp x, y, z */
extern f32 Vec3_LengthSq(EftDetVec *v);                                        /* squared length */
extern f32 Vec3_Dist(void *a, EftDetVec *b);                               /* distance between two points */
extern void Vec3_RotateAxis(EftDetVec *out, EftDetVec *v, EftDetVec *axis, f32 angle); /* rotate about an axis */
extern f32 Mathf_SinFast(f32 angle);
extern f32 Mathf_CosFast(f32 angle);
extern f32 Mathf_Asin(f32 x);
extern f32 Mathf_Acos(f32 x);
extern f32 EftMath_WrapAngle(f32 angle);

extern f32 BtlScene_RandRangeF(f32 a, f32 b);
extern s32 BtlScene_RandRange(s32 a, s32 b);

extern s32 BtlCharApi_GetOpponentObjId(s32 objId);
extern void BtlCharApi_GetBasePos(s32 objId, EftDetVec *out);
extern void BtlCharApi_GetFrameMove(s32 objId, EftDetVec *out);
extern f32 BtlCharApi_GetAltitude(s32 objId);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, EftDetVec *out);
extern s32 BtlCharApi_IsSightBlocked(s32 objId);
extern s32 BtlCharApi_IsLockedOn(s32 objId);
extern void BtlCharApi_SetHeldFlagAA(s32 objId);
extern void BtlCharApi_ShakeCamsNear(EftDetVec *pos, f32 near, f32 far, f32 arg3, f32 arg4);
extern void BtlCharApi_RumbleNear(EftDetVec *pos, f32 near, f32 far, f32 power, f32 time);
extern s32 BtlSeq_GetState(void);
extern s32 BattleSide_GetObjId(s32 side);
extern EftDetObj *BtlObj_Get(s32 objId);
extern EftDetNodeMtx *BtlObj_GetNode(EftDetObj *obj, s32 node);

/* Battle work: only the flags. */
typedef struct EftDetBattle {
    /* 0x0000 */ u8 unk0[0x19F0];
    /* 0x19F0 */ u64 flags; /* 0x100: paused */
} EftDetBattle;
extern EftDetBattle *Battle_GetWork(void);

/* The hit record core (eft_a.c). */
extern EftDetList *EftHit_GetList(void);
#ifdef PORT /* PC build: void, as defined; the value EftDet_HitFighterMulti passes on is never used (0 here) */
extern void EftHit_SetTaskFlag(u32 idx, s32 bits, EftDetVec pos);
#define EftHit_SetTaskFlag(idx, bits, pos) (EftHit_SetTaskFlag(idx, bits, pos), 0)
#else
extern s32 EftHit_SetTaskFlag(u32 idx, s32 bits, EftDetVec pos); /* void in eft_a.c; see EftDet_HitFighterMulti */
#endif
extern s32 EftHit_CanHit(EftDetRec *rec, s32 mode);
extern s32 EftHit_IsStoppedByHit(EftDetRec *rec);
extern s32 EftHit_RearmImpact(EftDetRec *rec);
extern s32 EftHit_Clash(EftDetRec *a, EftDetRec *b);
extern s32 EftHit_IsMultiHit(EftDetRec *rec);
extern void EftHit_IncHitCount(EftDetRec *rec);
extern s32 EftHit_GetHitCount(EftDetRec *rec);
extern s32 EftHit_GetMaxHits(EftDetRec *rec);
extern s32 EftHit_GetDefUnkB(EftDetRec *rec);
extern void EftHit_IncCountA(EftDetRec *rec);
extern void EftHit_SetCountA(EftDetRec *rec, s32 value);
extern s32 EftHit_GetCountA(EftDetRec *rec);
extern s32 EftHit_HasDefFlag20(EftDetRec *rec);
extern void EftHit_MarkLastHit(EftDetRec *rec);
extern s32 EftHit_NotifyBlastTask(u32 idx, void *tri);

/* The fighter's answer to a record that touches it (btl_char_coll_b.c). */
extern s32 BtlColl_TryDodge(s32 objId, EftDetRec *hit);
extern s32 BtlColl_TryDeflect(s32 objId, EftDetRec *hit);
extern s32 BtlColl_TryReflect(s32 objId, EftDetRec *hit);
extern s32 BtlColl_TryAbsorb(s32 objId, EftDetRec *hit);
extern s32 BtlColl_TryGuard(s32 objId, EftDetRec *hit);
extern s32 BtlColl_Hit(s32 objId, EftDetRec *hit);
extern void BtlColl_PlayContactSound(EftDetRec *hit);

/* Beam struggle effect (ki blast file, 0x174A70..). */
extern s32 EftStruggle_IsActive(void);                                       /* 1 while a beam struggle is running */
extern s32 EftStruggle_IsActiveFor(s32 actor);
extern void EftStruggle_Start(EftDetVec *pos, EftDetRec *a, EftDetRec *b); /* starts the beam struggle at pos */

/* Collision primitives (0x230B38..0x239A00, not decompiled). */
extern void ColBox_SetCenterHalf(EftDetBox *box, EftDetVec *center, EftDetVec *extent); /* box = center +- extent */
extern void ColBox_GetCenter(EftDetBox *box, EftDetVec *center);                     /* centre of a box */
extern void ColBox_GetHalf(EftDetBox *box, EftDetVec *extent);                     /* half size of a box */
extern void ColMesh_GetPolyVerts(void *mesh, EftDetPoly *poly, EftDetVec *v0, EftDetVec *v1, EftDetVec *v2);
extern EftDetPoly *ColMesh_GetPoly(void *mesh, s32 poly);
extern s32 ColBox_Overlaps(EftDetBox *a, EftDetBox *b);                             /* 1 when two boxes overlap */
extern s32 ColObb_Overlaps(void *vol, void *partVol);
/* Swept capsule against a triangle. The 64-bit return type is what makes EftDet_StageCb match (with a 32-bit one
   the compiler threads the "return 0" after the test). */
#ifdef PORT /* PC build: the definition's types (the PS2 passes both in one 64-bit register) */
extern s32 ColSweep_TestTri(EftDetStageCtx *sweep, EftDetTri *tri, EftDetVec *hit, f32 *dist);
#else
extern s64 ColSweep_TestTri(EftDetStageCtx *sweep, EftDetTri *tri, EftDetVec *hit, f32 *dist);
#endif
extern s32 ColSphere_ContactObb(EftDetSphere *sphere, void *partVol, EftDetVec *out0, EftDetVec *out1, f32 *t);
extern s32 ColSphere_TestSphere(EftDetSphere *sphere, void *box);                        /* 1 when they overlap */
extern s32 ColSphere_SweepSphere(EftDetSphere *a, void *b, EftDetVec *moveA, EftDetVec *moveB, f32 *t);
extern s32 ColSphere_SeparateSphere(EftDetSphere *a, EftDetSphere *b, EftDetVec *posA, EftDetVec *posB, EftDetVec *nrm);
extern void ColBounds_OfCapsule(EftDetBox *box, void *capsule);                         /* bounds of a capsule */
extern void ColCapsule_Set(EftDetCapsule *cap, void *from, void *to, f32 radius);
extern void ColSweep_FromCapsule(EftDetStageCtx *sweep, EftDetCapsule *cap);
extern void ColSphere_Set(EftDetSphere *sphere, void *center, f32 radius);

/* Stage (stg_a.c, eft_det_b.c). */
extern s32 BtlStage_FindZoneNear(s32 zone, void *pos);
extern void *BtlStage_GetZone(s32 zone);
extern s32 BtlStage_DestroyObj(s32 objId, s32 idx, EftDetVec *hitPos);
extern s32 BtlStage_DamageObj(s32 idx, s32 damage);
extern void StgCol_SetModeAll(void);
extern void StgCol_SetModeObjects(void);
extern s32 StgCol_QueryZone(EftDetResult *result, void *zone, EftDetBox *box, void *ctx, void *callback);
extern s32 StgCol_FirstBit(u64 mask);
extern void StgCol_FighterBreakObj(EftDetObj *obj, s32 idx, EftDetVec *hitPos);

extern EftVram *gEftVram;
extern EftVolleyAimStep *gEftVolleyAimScripts[];
extern void *gStgColMesh;
extern EftDetResult gStgGroundResult;
extern EftDetVec D_002EC2C0; /* {0, 0, 0, 1}: "no movement" */
extern EftDetVec D_002EC2E0; /* {0, 1, 0, 1}: down */
extern const f32 D_002FEAF8[]; /* FLT_MAX */
extern const f32 D_002FEAFC[]; /* FLT_MAX */

#define EFT_DET_DEG(x) ((x) / 180.0f * 3.14159265f)
#define EFT_DET_PI 3.14159265f

/* Builds a texture set of at most 8 entries from a texture pack. */
void EftTexSet_Load8(EftTexSet8 *set, void *pack) {
    EftTexPack *hdr;
    s32 count;
    s32 i;

    memset(set, 0, sizeof(EftTexSet8));
    Res_RelocateOffsets(&hdr, pack, pack);
    count = hdr->count;
    set->count = count;
    EftTexSet_CheckCount(count, 8);
    for (i = 0; i < set->count; i++) {
        set->tex[i].image = &hdr->images[i];
        set->tex[i].tex0 = hdr->images[i].tex0;
    }
}

/* Builds a texture set of at most 16 entries from a texture pack. */
void EftTexSet_Load16(EftTexSet16 *set, void *pack) {
    EftTexPack *hdr;
    s32 count;
    s32 i;

    memset(set, 0, sizeof(EftTexSet16));
    Res_RelocateOffsets(&hdr, pack, pack);
    count = hdr->count;
    set->count = count;
    EftTexSet_CheckCount(count, 16);
    for (i = 0; i < set->count; i++) {
        set->tex[i].image = &hdr->images[i];
        set->tex[i].tex0 = hdr->images[i].tex0;
    }
}

/* Builds a texture set of at most 34 entries from a texture pack. */
void EftTexSet_Load34(EftTexSet34 *set, void *pack) {
    EftTexPack *hdr;
    s32 count;
    s32 i;

    memset(set, 0, sizeof(EftTexSet34));
    Res_RelocateOffsets(&hdr, pack, pack);
    count = hdr->count;
    set->count = count;
    EftTexSet_CheckCount(count, 34);
    for (i = 0; i < set->count; i++) {
        set->tex[i].image = &hdr->images[i];
        set->tex[i].tex0 = hdr->images[i].tex0;
    }
}

/* Returns the number of VRAM blocks the effect textures use (no caller). */
s32 EftVram_GetUsed(void) {
    if (gEftVram == NULL) {
        return 0;
    }
    return gEftVram->next - 0x2B00;
}

/* Returns the number of VRAM blocks the effect textures may use (no caller). */
s32 EftVram_GetCapacity(void) {
    return 0x14FF;
}

/* Queues the upload of a texture's image to VRAM block imageBlock and of its CLUT to clutBlock (< 0: skip). */
#define EFT_VRAM_BITBLTBUF(block, width) (((s64)(block) << 32) | ((u64)(width) << 48))

void EftVram_Upload(EftVramTex *tex, s32 imageBlock, s32 clutBlock) {
    u32 pkt[12] = {
        0x10000002, 0x00000000, 0x00000000, 0x50000002, 0x00008001, 0x10000000,
        0x0000000E, 0x00000000, 0x00000000, 0x00000000, 0x00000050, 0x00000000,
    };
    EftVramImage *img = tex->img;

    if (imageBlock >= 0 && img->imageSize != 0) {
        pkt[8] = EFT_VRAM_BITBLTBUF(imageBlock, img->imageWidth);
        pkt[9] = EFT_VRAM_BITBLTBUF(imageBlock, img->imageWidth) >> 32;
        Dma_AddData(pkt, sizeof(pkt));
        Dma_AddRef(img->image, img->imageSize);
    }
    if (clutBlock >= 0 && img->clutSize != 0) {
        pkt[8] = EFT_VRAM_BITBLTBUF(clutBlock, img->clutWidth);
        pkt[9] = EFT_VRAM_BITBLTBUF(clutBlock, img->clutWidth) >> 32;
        Dma_AddData(pkt, sizeof(pkt));
        Dma_AddRef(img->clut, img->clutSize);
    }
}

/* Returns a + (b - a) * t. */
f32 EftVolleyAim_Lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

/* Returns the squared distance between fighters 0 and 1. */
f32 EftVolleyAim_GetFighterDistSq(void) {
    EftDetVec a;
    EftDetVec b;
    EftDetVec d;

    BtlCharApi_GetBasePos(0, &a);
    BtlCharApi_GetBasePos(1, &b);
    Vec4_Sub(&d, &a, &b);
    return Vec3_Dot(&d, &d);
}

/* Stores the vector from fighter objId to its opponent. */
void EftVolleyAim_GetToOpponent(s32 objId, EftDetVec *out) {
    EftDetVec a;
    EftDetVec b;

    BtlCharApi_GetBasePos(BtlCharApi_GetOpponentObjId(objId), &a);
    BtlCharApi_GetBasePos(objId, &b);
    Vec4_Sub(out, &a, &b);
}

/* Returns which side a muzzle node is on: 0 for nodes 0x12..0x1F and 0x36, 1 for 0x20..0x2D. */
s32 EftVolleyAim_GetNodeSide(s32 node) {
    s32 side = 0;

    switch (node) {
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x1C:
    case 0x1D:
    case 0x1E:
    case 0x1F:
    case 0x36:
        side = 0;
        break;
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x23:
    case 0x24:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
        side = 1;
        break;
    }
    return side;
}

/* Spreads a new shot's direction and stores its steering script, speed and turn rate. */
void EftVolleyAim_InitShot(s32 objId, EftVolleyAimShot *shot, s32 kind, s32 side, s32 index, s32 count, f32 speed,
                           f32 maxTurn) {
    s32 script = EftVolleyAim_Spread(objId, &shot->dir, kind, BtlCharApi_IsLockedOn(objId), side, index, count);

    shot->speed = speed;
    shot->maxTurn = maxTurn;
    shot->script = script;
}

/* Turns dir by a yaw / pitch offset that depends on the aim kind, the shot's index in the volley and the scene
 * generator. Returns the steering script of the shot: 0 / 1 for the lobbed kinds 7 / 8, else -1. */
s32 EftVolleyAim_Spread(s32 objId, EftDetVec *dir, s32 kind, s32 lockedOn, s32 side, s32 index, s32 count) {
    EftDetMtx m;
    f32 jitV = EFT_DET_DEG(1.0f);
    f32 jitH = jitV;
    s32 script = -1;
    f32 distSq = EftVolleyAim_GetFighterDistSq();
    f32 sprH = EFT_DET_DEG(12.5f);
    f32 sprV = EFT_DET_DEG(10.0f);
    f32 yaw;
    f32 pitch;
    f32 a;
    f32 t;
    s32 half;

    if (kind == 9) {
        return -1;
    }
    if (kind == 7) {
        sprH = EFT_DET_DEG(12.5f) * 0.5f;
        jitH = EFT_DET_DEG(1.0f) * 0.5f;
    } else if (kind == 8) {
        sprH = EFT_DET_DEG(12.5f) * 0.5f;
        jitH = EFT_DET_DEG(1.0f) * 0.5f;
    } else if (kind == 10) {
        sprH = EFT_DET_DEG(12.5f) * 0.02f;
        jitH = EFT_DET_DEG(1.0f) * 0.02f;
        sprV = EFT_DET_DEG(10.0f) * 0.05f;
        jitV = EFT_DET_DEG(1.0f) * 0.05f;
    } else if (kind == 12) {
        jitH = EFT_DET_DEG(1.0f) * 0.05f;
        sprH = EFT_DET_DEG(12.5f) * 0.05f;
        sprV = EFT_DET_DEG(10.0f) * 0.05f;
        jitV = EFT_DET_DEG(1.0f) * 0.05f;
    } else if (index == 0) {
        sprH = EFT_DET_DEG(12.5f) * 0.1f;
        sprV = EFT_DET_DEG(10.0f) * 0.1f;
    } else if (distSq < 250000.0f) {
        if (10000.0f < distSq) {
            t = EftVolleyAim_Lerp(0.5f, 1.0f, (distSq - 10000.0f) / 240000.0f);
        } else {
            t = 0.5f;
        }
        sprH *= t;
        sprV *= t;
    }
    switch (kind) {
    case 0:
        yaw = BtlScene_RandRangeF(-sprH - jitH, sprH + jitH);
        pitch = BtlScene_RandRangeF(-sprV - jitV, sprV + jitV);
        break;
    case 1:
        yaw = sprH * 2.0f / (f32)count * (f32)index - sprH + BtlScene_RandRangeF(-jitH, jitH);
        pitch = BtlScene_RandRangeF((-sprV - jitV) * 0.5f, (sprV + jitV) * 0.5f);
        break;
    case 2:
        if (index >= 4) {
            t = powf(0.5f, (f32)(index / 4));
            sprH *= t;
            sprV *= t;
        }
        yaw = sprH + BtlScene_RandRangeF(-jitH, jitH);
        if (index % 2 != side) {
            yaw = -yaw;
        }
        pitch = BtlScene_RandRangeF(-sprV - jitV, sprV + jitV);
        break;
    case 3:
        a = BtlScene_RandRangeF(-EFT_DET_PI, EFT_DET_PI);
        yaw = sprH * Mathf_SinFast(a) + BtlScene_RandRangeF(-jitH, jitH);
        pitch = sprV * Mathf_CosFast(a) + BtlScene_RandRangeF(-jitV, jitV);
        break;
    case 4:
        a = EFT_DET_PI * 2.0f / (f32)(count + 2);
        half = index / 2;
        if (index % 2 == side) {
            a = a * (f32)(half + 1) + -EFT_DET_PI / 2.0f;
        } else {
            a = EftMath_WrapAngle(-EFT_DET_PI / 2.0f - a * (f32)(half + 1));
        }
        yaw = sprH * Mathf_CosFast(a) + BtlScene_RandRangeF(-jitH, jitH);
        pitch = sprV * Mathf_SinFast(a) + BtlScene_RandRangeF(-jitV, jitV);
        break;
    case 5:
        a = EFT_DET_PI / (f32)(count - 1);
        half = index / 2;
        if (index % 2 == side) {
            a = a * (f32)half;
        } else {
            a = EFT_DET_PI - a * (f32)half;
        }
        yaw = sprH * Mathf_CosFast(a) + BtlScene_RandRangeF(-jitH, jitH);
        pitch = sprV * Mathf_SinFast(a) + BtlScene_RandRangeF(-jitV, jitV);
        break;
    case 6:
        a = EFT_DET_PI / (f32)(count - 1);
        if (index % 2 == side) {
            a = a * (f32)(index / 2);
        } else {
            a = EFT_DET_PI - a * (f32)(index / 2);
        }
        if ((index / 2) % 2 == 1) {
            sprH *= 0.5f;
            sprV *= 0.5f;
        }
        yaw = sprH * Mathf_CosFast(a) + BtlScene_RandRangeF(-jitH, jitH);
        pitch = sprV * Mathf_SinFast(a) + BtlScene_RandRangeF(-jitV, jitV);
        break;
    case 7:
        pitch = EFT_DET_PI / 2.0f;
        script = 0;
        yaw = BtlScene_RandRangeF(-sprH - jitH, sprH + jitH);
        dir->y = 0.0f;
        break;
    case 8:
        pitch = EFT_DET_PI / 4.0f;
        script = 1;
        yaw = BtlScene_RandRangeF(-sprH - jitH, sprH + jitH);
        dir->y = 0.0f;
        break;
    case 10:
        yaw = sprH + jitH;
        pitch = BtlScene_RandRangeF(-sprV - jitV, sprV + jitV);
        if (index & 1) {
            yaw = -yaw;
        }
        break;
    case 11:
        yaw = sprH - sprH * 2.0f / (f32)count * (f32)index + BtlScene_RandRangeF(-jitH, jitH);
        pitch = BtlScene_RandRangeF((-sprV - jitV) * 0.5f, (sprV + jitV) * 0.5f);
        break;
    case 12:
        if (index == 0) {
            yaw = 0.0f;
        } else if (index & 1) {
            yaw = (f32)(index / 2) * sprH + sprH + BtlScene_RandRangeF(0.0f, jitH);
        } else {
            yaw = -((f32)(index / 2) * sprH + sprH + BtlScene_RandRangeF(0.0f, jitH));
        }
        pitch = BtlScene_RandRangeF(-sprV - jitV, sprV + jitV);
        break;
    default:
        yaw = 0.0f;
        pitch = yaw;
        break;
    }
    if (!(kind == 7 || kind == 8)) {
        if (BtlCharApi_GetAltitude(objId) < 30.0f) {
            if (pitch < 0.0f) {
                pitch *= 0.4f;
            }
        }
    }
    {
        EftDetVec fwd = { { 0.0f, 0.0f, 1.0f, 1.0f } };

        f32 rx;
        f32 ry;

        Vec3_Clamp(dir, dir, -1.0f, 1.0f);
        rx = Mathf_Asin(-dir->y);
        ry = atan2f(dir->x, dir->z);
        rx = EftMath_WrapAngle(rx + pitch);
        ry = EftMath_WrapAngle(ry + yaw);
        Mtx_StoreIdentity(&m);
        Mtx_RotateX(&m, &m, rx);
        Mtx_RotateY(&m, &m, ry);
        Mtx_MulVec4(dir, &m, &fwd);
    }
    return script;
}

/* One frame of a homing shot: leads the opponent's node 0x11 (+ offset) by its frame movement, turns the
 * direction towards it by at most shot->maxTurn, then advances the position by shot->speed. */
void EftVolleyAim_Steer(EftVolleyAimShot *shot, s32 objId, EftDetVec *offset, s32 frontOnly) {
    EftDetVec target;
    EftDetVec move;
    EftDetVec d;
    EftDetVec axis;
    EftDetVec step;
    s32 opp = BtlCharApi_GetOpponentObjId(objId);
    f32 t;
    f32 dot;
    f32 c;

    if (opp < 0) {
        return;
    }
    BtlCharApi_GetNodePos(opp, 0x11, &target);
    if (offset != NULL) {
        Vec3_Add(&target, &target, offset);
    }
    Vec4_Sub(&d, &target, &shot->pos);
    t = sqrtf(Vec3_Dot(&d, &d)) / shot->speed;
    if (15.0f < t) {
        t = 15.0f;
    }
    BtlCharApi_GetFrameMove(opp, &move);
    Vec4_Scale(&move, &move, t * 0.5f);
    Vec4_Add(&target, &target, &move);
    Vec4_Sub(&d, &target, &shot->pos);
    Vec3_Normalize(&d, &d);
    Vec3_Normalize(&shot->dir, &shot->dir);
    dot = Vec3_Dot(&d, &shot->dir);
    if (frontOnly == 0 || (frontOnly == 1 && 0.0f < dot)) {
        if (dot < 0.0f) {
            c = 0.0f;
        } else if (1.0f < dot) {
            c = 1.0f;
        } else {
            c = dot;
        }
        t = Mathf_Acos(c);
        if (shot->maxTurn < t) {
            t = shot->maxTurn;
        }
        Vec3_Cross(&axis, &shot->dir, &d);
        Vec3_Normalize(&axis, &axis);
        Vec3_RotateAxis(&shot->dir, &shot->dir, &axis, t);
        Vec3_Normalize(&shot->dir, &shot->dir);
    }
    Vec4_Scale(&step, &shot->dir, shot->speed);
    Vec4_Add(&shot->pos, &shot->pos, &step);
    shot->pos.w = 1.0f;
}

/* Runs the steering script of a lobbed shot for `time` frames since it was fired (speed, turn rate and target
 * offset by time window), then steers it. `offset` is the shot's own target offset, kept between frames. */
void EftVolleyAim_Update(s32 objId, EftVolleyAimShot *shot, EftDetVec *offset, s32 exact, f32 time, f32 speed,
                         f32 maxTurn) {
    EftDetVec tmp;
    EftVolleyAimStep *p = NULL;
    s32 frontOnly = 0;
    f32 lenSq;
    f32 scale;
    f32 r;
    f32 t;
    f32 a;
    f32 b;

    if ((u32)shot->script < 2) {
        p = gEftVolleyAimScripts[shot->script];
    }
    if (p != NULL) {
        for (; p->op != 0; p++) {
            if ((f32)p->t0 <= time && time <= (f32)p->t1) {
                switch (p->op) {
                case EFT_VOLLEY_OP_TURN:
                    a = maxTurn * (f32)p->v0 * 0.1f;
                    b = maxTurn * (f32)p->v1 * 0.1f;
                    t = 1.0f;
                    if (time != (f32)p->t0) {
                        t = (time - (f32)p->t0) / (f32)(p->t1 - p->t0);
                    }
                    shot->maxTurn = EftVolleyAim_Lerp(a, b, t);
                    break;
                case EFT_VOLLEY_OP_SPEED:
                    a = speed * (f32)p->v0 * 0.1f;
                    b = speed * (f32)p->v1 * 0.1f;
                    t = 1.0f;
                    if (time != (f32)p->t0) {
                        t = (time - (f32)p->t0) / (f32)(p->t1 - p->t0);
                    }
                    shot->speed = EftVolleyAim_Lerp(a, b, t);
                    break;
                case EFT_VOLLEY_OP_SCATTER:
                    if (BtlScene_RandRange(0, p->v1) == 0 || exact) {
                        Vec4_Set(offset, 0.0f, 0.0f, 0.0f, 1.0f);
                        r = (f32)p->v0 * 0.04f;
                    } else if (BtlScene_RandRange(0, p->v1 >> 1) == 0) {
                        Vec4_Set(offset, 0.0f, 0.0f, 0.0f, 1.0f);
                        r = (f32)p->v0 * 0.4f;
                    } else {
                        EftVolleyAim_GetToOpponent(objId, offset);
                        lenSq = Vec3_LengthSq(offset);
                        scale = (lenSq < (f32)(p->v0 * p->v0) ? sqrtf(lenSq) : (f32)p->v0) * -0.5f;
                        Vec3_Normalize(offset, offset);
                        Vec3_Scale(offset, offset, scale);
                        r = (f32)p->v0;
                    }
                    offset->x += BtlScene_RandRangeF(-r, r);
                    offset->y = 0.0f;
                    offset->z += BtlScene_RandRangeF(-r, r);
                    break;
                case EFT_VOLLEY_OP_BEHIND:
                    EftVolleyAim_GetToOpponent(objId, offset);
                    offset->y = 0.0f;
                    Vec3_Normalize(offset, offset);
                    Vec3_Scale(offset, offset, -1000.0f);
                    break;
                case EFT_VOLLEY_OP_ABOVE:
                    EftVolleyAim_GetToOpponent(objId, offset);
                    offset->y = 0.0f;
                    Vec3_Normalize(offset, offset);
                    Vec3_Scale(offset, offset, -1000.0f);
                    Vec3_Normalize(&tmp, &shot->pos);
                    tmp.y = 0.0f;
                    Vec4_Scale(&tmp, &tmp, -400.0f);
                    Vec4_Add(offset, offset, &tmp);
                    offset->y -= 1000.0f;
                    break;
                case EFT_VOLLEY_OP_FRONT:
                    if (p->v0 != 0) {
                        frontOnly = 1;
                    }
                    break;
                }
            }
        }
    }
    EftVolleyAim_Steer(shot, objId, offset, frontOnly);
}

/* Returns 1 when one of the active attack volumes in `work` touches one of the body parts of `body`. */
s32 BtlBodyHit_TestVolumes(EftDetObjWork *work, EftDetPart **body) {
    EftDetVec out0;
    EftDetVec out1;
    f32 t;
    EftDetPart *part;
    s32 i;

    for (i = 0; i < work->atkSphereCount; i++) {
        part = *body;
        do {
            if (ColSphere_ContactObb(&work->atkSphere[i], part->vol, &out0, &out1, &t)) {
                return 1;
            }
        } while (!((part++)->flags & 1));
    }
    for (i = 0; i < work->atkVolCount; i++) {
        part = *body;
        do {
            if (ColObb_Overlaps(work->atkVol[i], part->vol)) {
                return 1;
            }
        } while (!((part++)->flags & 1));
    }
    return 0;
}

#ifdef PORT
static u32 tagContacts[PORT_FIGHTER_CAPACITY];
void TagContact_Clear(s32 actor) {
    if ((u32)actor<PORT_FIGHTER_CAPACITY) tagContacts[actor]=0;
}
void TagContact_Record(s32 attacker,s32 defender) {
    if ((u32)attacker<PORT_FIGHTER_CAPACITY && (u32)defender<PORT_FIGHTER_CAPACITY)
        tagContacts[attacker]|=1u<<defender;
}
s32 TagContact_Has(s32 attacker,s32 defender) {
    if ((u32)attacker>=PORT_FIGHTER_CAPACITY) return 0;
    return defender<0 ? tagContacts[attacker]!=0 :
        (u32)defender<PORT_FIGHTER_CAPACITY && (tagContacts[attacker]&(1u<<defender))!=0;
}
#endif

/* Tests the strike volumes of `atk` against the body of `def` and records the contact in both objects. */
s32 BtlBodyHit_Test(EftDetObj *atk, EftDetObj *def) {
    EftDetObjWork *work;
    EftDetObjWork *defWork;
    EftDetPart **body;

    if (atk->unkCAD < 0) {
        return 0;
    }
    work = atk->work;
    defWork = def->work;
    if (work == NULL) {
        return 0;
    }
    if (defWork == NULL) {
        return 0;
    }
    body = &def->parts;
    if (body == NULL) {
        return 0;
    }
    if (!ColBox_Overlaps(&work->atkBounds, &def->box)) {
        return 0;
    }
    if (!BtlBodyHit_TestVolumes(work, body)) {
        return 0;
    }
#ifdef PORT
    TagContact_Record(atk->id,def->id);
#else
    work->hitFlags |= 1 << (def->id + 0x18);
    defWork->hitFlags |= 1 << (atk->id + 0x1C);
#endif
    return 1;
}

/* Fighter against fighter: the object of side 0 against the object of side 1, then the reverse. */
void BtlBodyHit_Update(void) {
    EftDetObj *a;

    s32 i, j;
    for (i = 0; i < TagTeam_Count(); i++) {
        a = BtlObj_Get(BattleSide_GetObjId(i));
        for (j = 0; j < TagTeam_Count(); j++) {
            if (TagTeam_IsEnemy(i, j) && TagTeam_CanContact(i,j)) BtlBodyHit_Test(a, BtlObj_Get(BattleSide_GetObjId(j)));
        }
    }
}

/* Shape type 0: bounds of the sweep from sphere b to sphere a, its movement and direction, and the zone. */
s32 EftDet_PrepareSpheres(EftDetShape *shape) {
    EftDetCapsule cap;
    EftDetSphere *a = shape->a;
    EftDetSphere *b = shape->b;

    ColCapsule_Set(&cap, b, a, a->radius);
    ColBounds_OfCapsule(&shape->bounds, &cap);
    Vec3_Sub(&shape->delta, &a->pos, &b->pos);
    Vec3_Normalize(&shape->dir, &shape->delta);
    shape->zone = BtlStage_FindZoneNear(shape->zone, a);
    return 1;
}

/* Shape type 1: bounds of capsule a, its axis as movement and direction, and the zone of its end. */
s32 EftDet_PrepareCapsule(EftDetShape *shape) {
    EftDetCapsule *a = shape->a;
    EftDetVec *end = &a->end;

    ColBounds_OfCapsule(&shape->bounds, a);
    Vec3_Sub(&shape->delta, end, &a->start);
    Vec3_Normalize(&shape->dir, &shape->delta);
    shape->zone = BtlStage_FindZoneNear(shape->zone, end);
    return 1;
}

/* Shape types 2..6: nothing to prepare. */
s32 EftDet_PrepareNone(EftDetShape *shape) {
    return 1;
}

/* Resets the detection fields of every record and prepares its shape. */
void EftDet_PrepareAll(void) {
    s32 (*prepare[7])(EftDetShape *) = {
        EftDet_PrepareSpheres, EftDet_PrepareCapsule, EftDet_PrepareNone, EftDet_PrepareNone,
        EftDet_PrepareNone,    EftDet_PrepareNone,    EftDet_PrepareNone,
    };
    EftDetList *list;
    s32 i;

    list = EftHit_GetList();
    for (i = 0; i < list->count; i++) {
        list->rec[i].shape.hitFlags = 0;
        list->rec[i].shape.stageDist = D_002FEAF8[0];
        list->rec[i].shape.dist = D_002FEAF8[0];
        prepare[list->rec[i].shape.type](&list->rec[i].shape);
    }
}

/* The hit detection of one frame; nothing while the battle is paused. */
void EftDet_Update(void) {
    if (Battle_GetWork()->flags & 0x100) {
        return;
    }
    EftDet_PrepareAll();
    EftDet_ClashPass();
    EftDet_StagePass();
    EftDet_FighterPass();
}

/* Shape type 0 against a fighter: sphere b swept along delta against every body part; the earliest contact wins.
 * A part the sphere already overlaps at the start ends the search. */
s32 EftDet_SpheresVsFighter(EftDetObj *obj, EftDetShape *shape) {
    f32 t;
    EftDetPart **body = &obj->parts;
    s32 hits = 0;
    f32 best = 1.0f;
    EftDetSphere *b = shape->b;
    EftDetPart *part;

    if (!ColBox_Overlaps(&shape->bounds, &obj->hitBox)) {
        return 0;
    }
    for (part = *body;; part++) {
        if (ColSphere_SweepSphere(b, part->box, &shape->delta, &D_002EC2C0, &t)) {
            if (t <= 0.000001f || 1.0f <= t) {
                if (ColSphere_TestSphere(b, part->box)) {
                    Vec4_Copy(&shape->hitPos, &b->pos);
                    Vec4_Copy(&shape->contact, &b->pos);
                    hits++;
                    break;
                }
            } else if (t < best) {
                hits++;
                shape->hitPos.x = b->pos.x + t * shape->delta.x;
                shape->hitPos.y = b->pos.y + t * shape->delta.y;
                shape->hitPos.z = b->pos.z + t * shape->delta.z;
                shape->hitPos.w = 1.0f;
                shape->contact.x = shape->hitPos.x + b->radius * shape->dir.x;
                shape->contact.y = shape->hitPos.y + b->radius * shape->dir.y;
                shape->contact.z = shape->hitPos.z + b->radius * shape->dir.z;
                best = t;
            }
        }
        if (part->flags & 1) {
            break;
        }
    }
    if (hits != 0) {
        shape->dist = Vec3_Dist(b, &shape->hitPos);
        return 1;
    }
    return 0;
}

/* Shape type 1 against a fighter: a sphere at the start of capsule a swept along the capsule's axis. */
s32 EftDet_CapsuleVsFighter(EftDetObj *obj, EftDetShape *shape) {
    EftDetSphere sphere;
    f32 t;
    EftDetPart **body = &obj->parts;
    s32 hits = 0;
    f32 best = 1.0f;
    EftDetCapsule *a = shape->a;
    EftDetPart *part;

    if (!ColBox_Overlaps(&shape->bounds, &obj->hitBox)) {
        return 0;
    }
    ColSphere_Set(&sphere, a, a->radius);
    for (part = *body;; part++) {
        if (ColSphere_SweepSphere(&sphere, part->box, &shape->delta, &D_002EC2C0, &t)) {
            if (t <= 0.000001f || 1.0f <= t) {
                if (ColSphere_TestSphere(&sphere, part->box)) {
                    hits++;
                    Vec4_Copy(&shape->hitPos, &sphere.pos);
                    Vec4_Copy(&shape->contact, &sphere.pos);
                    break;
                }
            } else if (t < best) {
                hits++;
                shape->hitPos.x = sphere.pos.x + t * shape->delta.x;
                shape->hitPos.y = sphere.pos.y + t * shape->delta.y;
                shape->hitPos.z = sphere.pos.z + t * shape->delta.z;
                shape->hitPos.w = 1.0f;
                shape->contact.x = shape->hitPos.x + sphere.radius * shape->dir.x;
                shape->contact.y = shape->hitPos.y + sphere.radius * shape->dir.y;
                shape->contact.z = shape->hitPos.z + sphere.radius * shape->dir.z;
                shape->contact.w = 1.0f;
                best = t;
            }
        }
        if (part->flags & 1) {
            break;
        }
    }
    if (hits != 0) {
        shape->dist = Vec3_Dist(a, &shape->hitPos);
        return 1;
    }
    return 0;
}

/* Shape types 2..6 never touch a fighter. */
s32 EftDet_NoneVsFighter(EftDetObj *obj, EftDetShape *shape) {
    return 0;
}

/* A single-hit record touches fighter `target`: guard (task flag 2) or hit (task flag 1). */
void EftDet_HitFighter(EftDetRec *rec, s32 target, s32 idx) {
    if (BtlColl_TryGuard(target, rec)) {
        if (EftHit_RearmImpact(rec)) {
            EftHit_SetTaskFlag(idx, 2, rec->shape.contact);
        }
    } else if (BtlColl_Hit(target, rec)) {
        if (EftHit_RearmImpact(rec)) {
            EftHit_SetTaskFlag(idx, 1, rec->shape.contact);
        }
    }
}

/* A multi-hit record touches fighter `target`. The task counts the frames in contact (counter A) and the hits
 * landed: past the hit limit it only reports a hit; between two hits (fewer frames than the definition's interval
 * since the last one) it reports "in contact" (0x8000); else guard or hit as above, counting the hit.
 * The first exit is a tail call and the others are not, which this compiler only does for a non-void function
 * that returns the callee's value there, hence the types (the value is never used). */
s32 EftDet_HitFighterMulti(EftDetRec *rec, s32 target, s32 idx) {
    s32 interval;
    s32 maxHits;
    s32 frames;
    s32 hits;

    EftHit_IncCountA(rec);
    interval = EftHit_GetDefUnkB(rec);
    maxHits = EftHit_GetMaxHits(rec);
    frames = EftHit_GetCountA(rec);
    hits = EftHit_GetHitCount(rec);
    if (maxHits < hits) {
        return EftHit_SetTaskFlag(idx, 1, rec->shape.contact);
    }
    if (hits > 0 && frames < interval) {
        if (!EftHit_HasDefFlag20(rec)) {
            EftHit_SetTaskFlag(idx, 0x8000, rec->shape.contact);
        }
    } else if (BtlColl_TryGuard(target, rec)) {
        EftHit_IncHitCount(rec);
        EftHit_SetCountA(rec, 0);
        if (!EftHit_HasDefFlag20(rec)) {
            EftHit_SetTaskFlag(idx, 0x8000, rec->shape.contact);
        }
        if (EftHit_RearmImpact(rec)) {
            EftHit_SetTaskFlag(idx, 2, rec->shape.contact);
        }
    } else if (BtlColl_Hit(target, rec)) {
        EftHit_IncHitCount(rec);
        EftHit_SetCountA(rec, 0);
        if (!EftHit_HasDefFlag20(rec)) {
            EftHit_SetTaskFlag(idx, 0x8000, rec->shape.contact);
        }
        if (EftHit_RearmImpact(rec)) {
            EftHit_SetTaskFlag(idx, 1, rec->shape.contact);
        }
    }
}

/* A projectile hits the first hostile body in its path, independent of lock-on.
 * A contact that lies behind a stage contact of the same record
 * (found by EftDet_StagePass, which ran before) is dropped. Then the fighter answers, first yes wins: dodge
 * (nothing reported), deflect (0x40), reflect (0x80), absorb (0x20), then guard (2) or hit (1).
 * (The mix of `rec->` and `list->rec[i].` is what reproduces the original's two induction pointers.) */
void EftDet_FighterPass(void) {
    s32 (*test[7])(EftDetObj *, EftDetShape *) = {
        EftDet_SpheresVsFighter, EftDet_CapsuleVsFighter, EftDet_NoneVsFighter, EftDet_NoneVsFighter,
        EftDet_NoneVsFighter,    EftDet_NoneVsFighter,    EftDet_NoneVsFighter,
    };
    EftDetList *list;
    EftDetRec *rec;
    s32 i;

    list = EftHit_GetList();
    for (i = 0; i < list->count; i++) {
        rec = &list->rec[i];
        if (EftHit_CanHit(rec, 1)) {
            s32 target = TagTeam_Target(list->rec[i].objId);
            s32 ok;
            EftDetObj *obj;

            if (TagTeam_Active()) {
                extern s32 TagTeam_IsAlive(s32);
                EftDetShape original=rec->shape, nearest=rec->shape;
                s32 candidate;
                f32 distance=1.0e30f;
                target=-1;
                for (candidate=0;candidate<TagTeam_Count();candidate++) {
                    EftDetShape probe=original;
                    if (!TagTeam_IsEnemy(rec->objId,candidate) || !TagTeam_IsAlive(candidate) || !TagTeam_CanContact(rec->objId,candidate)) continue;
                    obj=BtlObj_Get(BattleSide_GetObjId(candidate));
                    if (test[probe.type](obj,&probe) && probe.dist<distance) {
                        target=candidate; nearest=probe; distance=probe.dist;
                    }
                }
                if (target<0) continue;
                rec->shape=nearest;
            }

            obj = BtlObj_Get(BattleSide_GetObjId(target));
            if (TagTeam_Active() || test[list->rec[i].shape.type](obj, &rec->shape)) {
                ok = 0;
                if (!(list->rec[i].shape.hitFlags & EFT_DET_HIT_STAGE) ||
                    list->rec[i].shape.dist < list->rec[i].shape.stageDist) {
                    ok = 1;
                }
                if (ok) {
                    list->rec[i].shape.hitFlags |= EFT_DET_HIT_FIGHTER;
                    EftHit_MarkLastHit(rec);
                    if (BtlColl_TryDodge(target, rec)) {
                        continue;
                    }
                    if (BtlColl_TryDeflect(target, rec)) {
                        EftHit_SetTaskFlag(i, 0x40, rec->shape.contact);
                    } else if (BtlColl_TryReflect(target, rec)) {
                        EftHit_SetTaskFlag(i, 0x80, rec->shape.contact);
                    } else if (BtlColl_TryAbsorb(target, rec)) {
                        EftHit_SetTaskFlag(i, 0x20, rec->shape.contact);
                    } else if (EftHit_IsMultiHit(rec)) {
                        EftDet_HitFighterMulti(rec, target, i);
                    } else {
                        EftDet_HitFighter(rec, target, i);
                    }
                }
            }
        }
    }
}

/* Two type 0 shapes: both spheres swept over the frame. On contact stores each one's centre and contact point. */
s32 EftDet_SpheresVsSpheres(EftDetShape *a, EftDetShape *b) {
    EftDetVec nrm;
    f32 t;
    EftDetSphere *sa = a->b;
    EftDetSphere *sb = b->b;

    if (!ColBox_Overlaps(&a->bounds, &b->bounds)) {
        return 0;
    }
    if (ColSphere_SweepSphere(sa, sb, &a->delta, &b->delta, &t)) {
        if (t <= 0.000001f || 1.0f <= t) {
            if (ColSphere_SeparateSphere(sa, sb, &a->hitPos, &b->hitPos, &nrm)) {
                a->contact.x = a->hitPos.x + nrm.x * sa->radius;
                a->contact.y = a->hitPos.y + nrm.y * sa->radius;
                a->contact.z = a->hitPos.z + nrm.z * sa->radius;
                a->contact.w = 1.0f;
                Vec4_Copy(&b->contact, &a->contact);
                return 1;
            }
        } else {
            a->hitPos.x = sa->pos.x + t * a->delta.x;
            a->hitPos.y = sa->pos.y + t * a->delta.y;
            a->hitPos.z = sa->pos.z + t * a->delta.z;
            a->hitPos.w = 1.0f;
            a->contact.x = a->hitPos.x + sa->radius * a->dir.x;
            a->contact.y = a->hitPos.y + sa->radius * a->dir.y;
            a->contact.z = a->hitPos.z + sa->radius * a->dir.z;
            a->contact.w = 1.0f;
            b->hitPos.x = sb->pos.x + t * b->delta.x;
            b->hitPos.y = sb->pos.y + t * b->delta.y;
            b->hitPos.z = sb->pos.z + t * b->delta.z;
            b->hitPos.w = 1.0f;
            b->contact.x = b->hitPos.x + sb->radius * b->dir.x;
            b->contact.y = b->hitPos.y + sb->radius * b->dir.y;
            b->contact.z = b->hitPos.z + sb->radius * b->dir.z;
            b->contact.w = 1.0f;
            return 1;
        }
    }
    return 0;
}

/* Two type 1 shapes: a sphere at the start of each capsule, swept along each capsule's axis. */
s32 EftDet_CapsuleVsCapsule(EftDetShape *a, EftDetShape *b) {
    EftDetSphere sa;
    EftDetSphere sb;
    EftDetVec nrm;
    f32 t;
    EftDetCapsule *ca = a->a;
    EftDetCapsule *cb = b->a;

    ColSphere_Set(&sa, ca, ca->radius);
    ColSphere_Set(&sb, cb, cb->radius);
    if (!ColBox_Overlaps(&a->bounds, &b->bounds)) {
        return 0;
    }
    if (ColSphere_SweepSphere(&sa, &sb, &a->delta, &b->delta, &t)) {
        if (t <= 0.000001f || 1.0f <= t) {
            if (ColSphere_SeparateSphere(&sa, &sb, &a->hitPos, &b->hitPos, &nrm)) {
                a->contact.x = a->hitPos.x + nrm.x * sa.radius;
                a->contact.y = a->hitPos.y + nrm.y * sa.radius;
                a->contact.z = a->hitPos.z + nrm.z * sa.radius;
                a->contact.w = 1.0f;
                Vec4_Copy(&b->contact, &a->contact);
                return 1;
            }
        } else {
            a->hitPos.x = sa.pos.x + t * a->delta.x;
            a->hitPos.y = sa.pos.y + t * a->delta.y;
            a->hitPos.z = sa.pos.z + t * a->delta.z;
            a->hitPos.w = 1.0f;
            a->contact.x = a->hitPos.x + sa.radius * a->dir.x;
            a->contact.y = a->hitPos.y + sa.radius * a->dir.y;
            a->contact.z = a->hitPos.z + sa.radius * a->dir.z;
            a->contact.w = 1.0f;
            b->hitPos.x = sb.pos.x + t * b->delta.x;
            b->hitPos.y = sb.pos.y + t * b->delta.y;
            b->hitPos.z = sb.pos.z + t * b->delta.z;
            b->hitPos.w = 1.0f;
            b->contact.x = b->hitPos.x + sb.radius * b->dir.x;
            b->contact.y = b->hitPos.y + sb.radius * b->dir.y;
            b->contact.z = b->hitPos.z + sb.radius * b->dir.z;
            b->contact.w = 1.0f;
            return 1;
        }
    }
    return 0;
}

/* A type 1 shape (a) against a type 0 shape (b). */
s32 EftDet_CapsuleVsSpheres(EftDetShape *a, EftDetShape *b) {
    EftDetSphere sa;
    EftDetVec nrm;
    f32 t;
    EftDetCapsule *ca = a->a;
    EftDetSphere *sb = b->b;

    ColSphere_Set(&sa, ca, ca->radius);
    if (!ColBox_Overlaps(&a->bounds, &b->bounds)) {
        return 0;
    }
    if (ColSphere_SweepSphere(&sa, sb, &a->delta, &b->delta, &t)) {
        if (t <= 0.000001f || 1.0f <= t) {
            if (ColSphere_SeparateSphere(&sa, sb, &a->hitPos, &b->hitPos, &nrm)) {
                a->contact.x = a->hitPos.x + nrm.x * sa.radius;
                a->contact.y = a->hitPos.y + nrm.y * sa.radius;
                a->contact.z = a->hitPos.z + nrm.z * sa.radius;
                a->contact.w = 1.0f;
                Vec4_Copy(&b->contact, &a->contact);
                return 1;
            }
        } else {
            a->hitPos.x = sa.pos.x + t * a->delta.x;
            a->hitPos.y = sa.pos.y + t * a->delta.y;
            a->hitPos.z = sa.pos.z + t * a->delta.z;
            a->hitPos.w = 1.0f;
            a->contact.x = a->hitPos.x + sa.radius * a->dir.x;
            a->contact.y = a->hitPos.y + sa.radius * a->dir.y;
            a->contact.z = a->hitPos.z + sa.radius * a->dir.z;
            a->contact.w = 1.0f;
            b->hitPos.x = sb->pos.x + t * b->delta.x;
            b->hitPos.y = sb->pos.y + t * b->delta.y;
            b->hitPos.z = sb->pos.z + t * b->delta.z;
            b->hitPos.w = 1.0f;
            b->contact.x = b->hitPos.x + sb->radius * b->dir.x;
            b->contact.y = b->hitPos.y + sb->radius * b->dir.y;
            b->contact.z = b->hitPos.z + sb->radius * b->dir.z;
            b->contact.w = 1.0f;
            return 1;
        }
    }
    return 0;
}

/* Record against record: every pair i < count - 1, j >= i of records of different owners. Record i is asked
 * once whether it may still clash (EftHit_CanHit mode 2), before its inner loop; record j for every pair.
 * EftHit_Clash is called for the pair whether or not the shapes touch (it has side effects: a head-on pair of
 * techniques asks both technique timers to stop); its result is used only when they touch:
 * bit 0 -> record i lost (task flag 8), bit 1 -> record j lost (3 = both), 4 -> beam struggle: both lose when
 * the owner of i has no line of sight or the battle sequence is at state 4 or later; else both fighters get held
 * flag 0xAA and, unless a struggle is already running, both tasks get flag 0x100 and the struggle effect starts
 * at record i's contact centre. Only record i gets EFT_DET_HIT_CLASH. */
void EftDet_ClashPass(void) {
    EftDetList *list;
    s32 i;
    s32 j;

    list = EftHit_GetList();
    for (i = 0; i < list->count - 1; i++) {
        if (EftHit_CanHit(&list->rec[i], 2)) {
            for (j = i; j < list->count; j++) {
                s32 hit = 0;
                s32 result;

                if (TagTeam_IsEnemy(list->rec[i].objId, list->rec[j].objId)) {
                    if (EftHit_CanHit(&list->rec[j], 2)) {
                        if (list->rec[i].shape.type == 0 && list->rec[j].shape.type == 0) {
                            if (EftDet_SpheresVsSpheres(&list->rec[i].shape, &list->rec[j].shape)) {
                                hit = 1;
                            }
                        } else if (list->rec[i].shape.type == 1 && list->rec[j].shape.type == 1) {
                            if (EftDet_CapsuleVsCapsule(&list->rec[i].shape, &list->rec[j].shape)) {
                                hit = 1;
                            }
                        } else if ((list->rec[i].shape.type == 1 && list->rec[j].shape.type == 0) ||
                                   (list->rec[i].shape.type == 0 && list->rec[j].shape.type == 1)) {
                            if (list->rec[i].shape.type == 1) {
                                if (EftDet_CapsuleVsSpheres(&list->rec[i].shape, &list->rec[j].shape)) {
                                    hit = 1;
                                }
                            } else if (EftDet_CapsuleVsSpheres(&list->rec[j].shape, &list->rec[i].shape)) {
                                hit = 1;
                            }
                        }
                        result = EftHit_Clash(&list->rec[i], &list->rec[j]);
                        if (hit) {
                            list->rec[i].shape.hitFlags |= EFT_DET_HIT_CLASH;
                            if (result & 1) {
                                EftHit_SetTaskFlag(i, 8, list->rec[i].shape.hitPos);
                            }
                            if (result & 2) {
                                EftHit_SetTaskFlag(j, 8, list->rec[j].shape.hitPos);
                            }
                            if (result & 4) {
                                if (BtlCharApi_IsSightBlocked(list->rec[i].objId)) {
                                    EftHit_SetTaskFlag(i, 8, list->rec[i].shape.hitPos);
                                    EftHit_SetTaskFlag(j, 8, list->rec[j].shape.hitPos);
                                } else if (BtlSeq_GetState() >= 4) {
                                    EftHit_SetTaskFlag(i, 8, list->rec[i].shape.hitPos);
                                    EftHit_SetTaskFlag(j, 8, list->rec[j].shape.hitPos);
                                } else {
                                    if (!EftStruggle_IsActiveFor(list->rec[i].objId) &&
                                        !EftStruggle_IsActiveFor(list->rec[j].objId) &&
                                        BtlClash_Join(list->rec[i].objId, list->rec[j].objId)) {
                                        BtlCharApi_SetHeldFlagAA(list->rec[i].objId);
                                        BtlCharApi_SetHeldFlagAA(list->rec[j].objId);
                                        EftHit_SetTaskFlag(i, 0x100, list->rec[i].shape.hitPos);
                                        EftHit_SetTaskFlag(j, 0x100, list->rec[j].shape.hitPos);
                                        EftStruggle_Start(&list->rec[i].shape.hitPos, &list->rec[i], &list->rec[j]);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/* Mesh callback of the stage test: the record's swept volume against one triangle; keeps the nearest contact in
 * the record's shape (centre, contact point pushed back by the radius along the plane normal, the triangle). */
s32 EftDet_StageCb(EftDetNode *node, EftDetStageCtx *ctx) {
    EftDetTri tri;
    EftDetVec hit;
    f32 dist;
    EftDetPoly *poly;

    memset(&hit, 0, sizeof(hit));
    hit.w = 1.0f;
    poly = ColMesh_GetPoly(gStgColMesh, node->poly);
    if (poly->flags & 0x8000000) {
        return 0;
    }
    ColMesh_GetPolyVerts(gStgColMesh, poly, &tri.v[0], &tri.v[1], &tri.v[2]);
    Vec4_Copy(&tri.nrm, &poly->nrm);
    if (ColSweep_TestTri(ctx, &tri, &hit, &dist)) {
        if (dist < ctx->best) {
            ctx->best = dist;
            ctx->hit = 1;
            Vec4_Copy(&ctx->shape->hitPos, &hit);
            ctx->shape->tri = tri;
            if (ctx->best < 0.01f) {
                Vec4_Copy(&ctx->shape->contact, &hit);
            } else {
                ctx->shape->contact.x = hit.x - tri.nrm.x * ctx->radius;
                ctx->shape->contact.y = hit.y - tri.nrm.y * ctx->radius;
                ctx->shape->contact.z = hit.z - tri.nrm.z * ctx->radius;
                ctx->shape->contact.w = 1.0f;
            }
            return 1;
        }
        return 0;
    }
    return 0;
}

/* One record against the stage collision of its zone (none when the zone is < 0). The volume is a capsule from
 * the previous to the current position (type 0: sphere centres; type 1: capsule ends) with the radius times
 * shape->scale (not scaled for type 0 when objectsOnly), at least 0.08. Returns 1 when it hit something solid
 * that stays: sets EFT_DET_HIT_STAGE and stageDist; hitPos / contact / tri were filled by the callback.
 * When the walk touched an unbroken stage object, the one with the lowest index takes damage (1 from a ki blast,
 * 999999 from a technique); if that breaks it, it is destroyed and the record goes on as if nothing was hit
 * (returns 0 although hitPos / contact / tri were written). */
s32 EftDet_TestStage(EftDetShape *shape, s32 isBlast, s32 objectsOnly) {
    EftDetStageCtx ctx;
    EftDetResult result;
    EftDetBox box;
    EftDetCapsule cap;
    void *zone;
    s32 idx;

    if (shape->zone < 0) {
        return 0;
    }
    memset(&ctx, 0, sizeof(ctx));
    ctx.shape = shape;
    switch (shape->type) {
    case 0:
        ColCapsule_Set(&cap, shape->b, shape->a, ((EftDetSphere *)shape->a)->radius);
        if (!objectsOnly) {
            cap.radius *= shape->scale;
        }
        if (cap.radius < 0.08f) {
            cap.radius = 0.08f;
        }
        ColSweep_FromCapsule(&ctx, &cap);
        ColBounds_OfCapsule(&box, &cap);
        if (ctx.length > cap.radius) {
            ctx.best = ctx.length + 0.001f;
        } else {
            ctx.best = cap.radius + 0.001f;
        }
        break;
    case 1:
        ColCapsule_Set(&cap, &((EftDetCapsule *)shape->b)->end, &((EftDetCapsule *)shape->a)->end,
                      ((EftDetCapsule *)shape->a)->radius);
        cap.radius *= shape->scale;
        if (cap.radius < 0.08f) {
            cap.radius = 0.08f;
        }
        ColSweep_FromCapsule(&ctx, &cap);
        ColBounds_OfCapsule(&box, &cap);
        ctx.best = ctx.length + 0.001f;
        break;
    case 2:
        break;
    }
    zone = BtlStage_GetZone(shape->zone);
    StgCol_SetModeAll();
    if (objectsOnly) {
        StgCol_SetModeObjects();
    }
    if (!StgCol_QueryZone(&result, zone, &box, &ctx, EftDet_StageCb)) {
        return 0;
    }
    if (result.obj != 0) {
        idx = StgCol_FirstBit(result.objMask);
        if (idx >= 0) {
            if (BtlStage_DamageObj(idx, isBlast == 1 ? 1 : 999999)) {
                BtlStage_DestroyObj(-1, idx, &ctx.unk50);
                return 0;
            }
        }
    }
    shape->hitFlags |= EFT_DET_HIT_STAGE;
    switch (shape->type) {
    case 0:
        shape->stageDist = Vec3_Dist(shape->b, &shape->hitPos);
        break;
    case 1:
        shape->stageDist = Vec3_Dist(shape->a, &shape->hitPos);
        break;
    case 2:
        break;
    }
    return 1;
}

/* Record against stage. A rush-type technique (class 0 kind 4, or another class and kind 9) only meets unbroken
 * stage objects. A record that is stopped by the hit reports task flag 4 with the contact point, hands the
 * triangle to its blast task and plays the contact sound; a technique also shakes cameras and pads nearby. */
void EftDet_StagePass(void) {
    EftDetList *list;
    EftDetRec *rec;
    s32 i;

    list = EftHit_GetList();
    for (i = 0; i < list->count; i++) {
        rec = &list->rec[i];
        if (EftHit_CanHit(rec, 0)) {
            s32 objectsOnly = 0;

            if (rec->src != NULL) {
                EftDetDef *def = rec->src->def;

                if (def->cls == 0) {
                    if (def->kind == 4) {
                        objectsOnly = 1;
                    }
                } else {
                    objectsOnly = def->kind == 9;
                }
            }
            if (EftDet_TestStage(&rec->shape, rec->type == 0, objectsOnly)) {
                if (EftHit_IsStoppedByHit(rec)) {
                    EftHit_SetTaskFlag(i, 4, list->rec[i].shape.contact);
                    EftHit_NotifyBlastTask(i, &rec->shape.tri);
                    BtlColl_PlayContactSound(rec);
                    if (rec->type == 1) {
                        BtlCharApi_ShakeCamsNear(&rec->pos, 100.0f, 1500.0f, 3.0f, 0.5f);
                        BtlCharApi_RumbleNear(&rec->pos, 100.0f, 1000.0f, 1.0f, 0.3f);
                    }
                }
            }
        }
    }
}

/* Stretches a box 8000 downwards (+Y is down), keeping its top. */
void StgGround_ExtendBoxDown(EftDetBox *box) {
    EftDetVec center;
    EftDetVec extent;

    ColBox_GetCenter(box, &center);
    ColBox_GetHalf(box, &extent);
    extent.y = (extent.y * 2.0f + 8000.0f) * 0.5f;
    center.y = box->min[1] + extent.y;
    ColBox_SetCenterHalf(box, &center, &extent);
}

/* Tests the point (out->x, out->z) against a triangle seen from above. When it is inside and the plane's height
 * there is below minY and above the best so far, stores the height, the plane and the polygon flags. */
s32 StgGround_TestTri(StgGroundOut *out, EftDetTri *tri, s32 flags, f32 minY) {
    s32 result = 0;
    s32 inside = 0;
    f32 x0 = tri->v[0].x;
    f32 z0 = tri->v[0].z;
    f32 px = out->x;
    f32 pz = out->z;
    f32 x2 = tri->v[2].x;
    f32 z2 = tri->v[2].z;
    f32 y = (tri->nrm.w - tri->nrm.x * px - tri->nrm.z * pz) / tri->nrm.y;
    f32 x1 = tri->v[1].x;
    f32 z1 = tri->v[1].z;

    f32 c;

    c = (x2 - x0) * (pz - z0) - (z2 - z0) * (px - x0);
    if (c <= 0.001f) {
        c = (x1 - x2) * (pz - z2) - (z1 - z2) * (px - x2);
        if (c <= 0.001f) {
            c = (x0 - x1) * (pz - z1) - (z0 - z1) * (px - x1);
            if (c <= 0.001f) {
                inside = 1;
            }
        }
    }
    if (minY < y && y < out->y && inside) {
        out->y = y;
        out->flags = flags;
        Vec4_Copy(&out->nrm, &tri->nrm);
        result = 1;
    }
    return result;
}

/* Mesh callback of the ground probe: solid polygons that face up only. */
s32 StgGround_Cb(EftDetNode *node, StgGroundCtx *ctx) {
    EftDetTri tri;
    EftDetPoly *poly = ColMesh_GetPoly(gStgColMesh, node->poly);

    if (poly->flags & 0x8000000) {
        return 0;
    }
    if (!(Vec3_Dot(&D_002EC2E0, &poly->nrm) < 0.0f)) {
        return 0;
    }
    ColMesh_GetPolyVerts(gStgColMesh, poly, &tri.v[0], &tri.v[1], &tri.v[2]);
    Vec4_Copy(&tri.nrm, &poly->nrm);
    return StgGround_TestTri(ctx->out, &tri, poly->flags, ctx->minY) != 0;
}

/* The ground probe: of the upward-facing solid triangles of the zone (static mesh, unbroken objects, remains of
 * broken ones) that lie under the point (out->x, out->z), the one nearest below the height minY (+Y is down:
 * the smallest plane height that is still greater than minY). `box` is only the search volume (stretched 8000
 * down) and gives the zone. out->y comes back as the ground height, or FLT_MAX when there is none; out->nrm and
 * out->flags describe the triangle. What the walk touched is left in gStgGroundResult. */
void StgGround_Probe(s32 zone, EftDetBox *box, StgGroundOut *out, f32 minY) {
    StgGroundCtx ctx;
    EftDetBox b;
    EftDetVec center;
    void *z;

    if (out != NULL) {
        out->y = D_002FEAFC[0];
    }
    ColBox_GetCenter(box, &center);
    b = *box;
    StgGround_ExtendBoxDown(&b);
    *(u64 *)&ctx = 0;
    ctx.out = out;
    ctx.minY = minY;
    z = BtlStage_GetZone(BtlStage_FindZoneNear(zone, &center));
    StgCol_SetModeAll();
    StgCol_QueryZone(&gStgGroundResult, z, &b, &ctx, StgGround_Cb);
}

/* Per-frame ground query of a fighter object: probes below the body sphere's height at the object's position and
 * stores the result in the object's work buffer. When the probe touched an unbroken stage object and the ground
 * found is above the model's root node, the fighter is under / inside that object: break test on it. */
void StgGround_UpdateFighter(EftDetObj *obj) {
    EftDetBox box;
    EftDetVec pos;
    EftDetVec extent = { { 0.1f, 1.0f, 0.1f, 1.0f } };
    EftDetVec hitPos = { { 1.0f, 0.0f, 1.0f, 1.0f } };
    u8 *blk = obj->unk950;
    EftDetBodyPos *body = obj->bodyPos;
    EftDetObjWork *work = obj->work;

    if (body == NULL || work == NULL) {
        return;
    }
    Vec4_Copy(&pos, &obj->pos);
    pos.y = body->y;
    ColBox_SetCenterHalf(&box, &pos, &extent);
    Vec3_Copy(&work->ground, &obj->pos);
    StgGround_Probe(*(s32 *)(blk + 0xD4), &box, &work->ground, body->y);
    if (gStgGroundResult.obj != 0) {
        if (work->ground.y < BtlObj_GetNode(obj, 0)->mtx[3][1]) {
            StgCol_FighterBreakObj(obj, StgCol_FirstBit(gStgGroundResult.objMask), &hitPos);
        }
    }
}
