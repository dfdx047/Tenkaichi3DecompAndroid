#include "common.h"
#include "battle/eft_l.h"

/*
 * Speed-line spawners, 0x15EF18..0x15F728: the first two functions of the speed-line module (class D_002C3A18,
 * state gEftSpdLine; the rest of it is eft_m.c). Both only create draw segments; positions come from libc rand().
 *
 * .rodata of this file, 0x2ECBA0..0x2ECC20, byte-identical to the original: the node table and the two `up`
 * vectors are function-local static constants. The stack copy of `up` is only scheduled as in the original when
 * its source is a constant defined in this file (an extern gives a swapped pair of instructions).
 */

extern s32 rand(void);

extern void Vec4_Add(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Sub(Vec4 *dst, Vec4 *a, Vec4 *b);
extern void Vec4_Scale(Vec4 *dst, Vec4 *src, f32 scale);
extern void Vec3_Normalize(Vec4 *dst, Vec4 *src);
extern void Vec3_Cross(Vec4 *dst, Vec4 *a, Vec4 *b);
extern f32 Vec3_Length(Vec4 *v);
extern f32 Mathf_Cos(f32 angle);
extern f32 Mathf_Sin(f32 angle);

extern f32 BtlCharApi_GetHeight(s32 objId);
extern void BtlCharApi_GetDir(s32 objId, Vec4 *out);
extern void BtlCharApi_GetNodePos(s32 objId, s32 node, Vec4 *out);
extern f32 BtlCharApi_GetPartUnk5C(s32 objId, s32 part);

#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftSpdLine_AddTrail(s32 objId, PortVec4 pos, PortVec4 dir);
#define EftSpdLine_AddTrail(objId, pos, dir) EftSpdLine_AddTrail(objId, *(PortVec4 *)(pos), *(PortVec4 *)(dir))
#else
extern void EftSpdLine_AddTrail(s32 objId, Vec4 *pos, Vec4 *dir);
#endif
#ifdef PORT /* PC build: by value, as the definition takes it (the PS2 passes a struct of this size by address) */
extern void EftSpdLine_AddStreak(s32 objId, PortVec4 from, PortVec4 to, PortVec4 dir, f32 width, f32 life, f32 fade);
#define EftSpdLine_AddStreak(objId, from, to, dir, width, life, fade) EftSpdLine_AddStreak(objId, *(PortVec4 *)(from), *(PortVec4 *)(to), *(PortVec4 *)(dir), width, life, fade)
#else
extern void EftSpdLine_AddStreak(s32 objId, Vec4 *from, Vec4 *to, Vec4 *dir, f32 width, f32 life, f32 fade);
#endif

extern void *gEftSpdLine;

/* A body node a trail starts from, with a second node (or -1): when the two are more than 6 apart (a long limb)
 * extra trails are added. */
typedef struct EftSpdLineNode {
    s32 node;
    s32 node2;
} EftSpdLineNode;


#define EFT_RANDF() ((f32)rand() / 2147483647.0f)

/* Fighter effect request 0xB (moving faster than half the reference speed): one trail behind each of eleven body
 * nodes, at a random point of a disc of radius height * 0.05 across the direction of movement. */
s32 EftSpdLine_SpawnBodyTrails(s32 objId) {
    f32 extra = 0.0f;
    Vec4 pos;
    Vec4 p0;
    Vec4 back;
    Vec4 dir;
    static const EftSpdLineNode sNodes[11] = { /* .rodata 0x2ECBA0 */
        { 0x0A, 0x08 }, { 0x08, -1 },   { 0x0E, 0x0C }, { 0x0C, -1 },   { 0x13, 0x14 }, { 0x14, 0x15 },
        { 0x21, 0x22 }, { 0x22, 0x23 }, { 0x10, 0x11 }, { 0x11, 0x30 }, { 0x30, -1 },
    };
    static const EftVec sUp = { 0.0f, -1.0f, 0.0f, 1.0f }; /* .rodata 0x2ECC00 */
    EftVec up = sUp;
    Vec4 up2;
    Vec4 side;
    Vec4 p1;
    u32 i;
    s32 j;
    f32 angle;
    f32 radius;
    f32 len;

    if (gEftSpdLine == NULL) {
        return 0;
    }
    BtlCharApi_GetDir(objId, &dir);
    Vec4_Scale(&back, &dir, -1.0f);
    Vec3_Normalize(&back, &back);
    Vec3_Cross(&side, &back, (Vec4 *)&up);
    Vec3_Normalize(&side, &side);
    Vec3_Cross(&up2, &back, &side);
    Vec3_Normalize(&up2, &up2);
    for (i = 0; i < 11; i++) {
        BtlCharApi_GetNodePos(objId, sNodes[i].node, &p0);
        if (sNodes[i].node2 >= 0) {
            BtlCharApi_GetNodePos(objId, sNodes[i].node2, &p1);
            Vec4_Sub(&p1, &p1, &p0);
            len = Vec3_Length(&p1);
            if (len > 6.0f) {
                extra = len / 6.0f;
            }
        }
        angle = EFT_RANDF() * 6.2831853f;
        radius = EFT_RANDF() + 0.0f;
        Vec4_Scale(&dir, &side, Mathf_Cos(angle) * radius * (BtlCharApi_GetHeight(objId) * 0.05f));
        Vec4_Add(&pos, &p0, &dir);
        Vec4_Scale(&dir, &up2, Mathf_Sin(angle) * radius * (BtlCharApi_GetHeight(objId) * 0.05f));
        Vec4_Add(&pos, &pos, &dir);
        Vec4_Scale(&dir, &back, (EFT_RANDF() * 5.0f + 1.5f) * (BtlCharApi_GetHeight(objId) * 0.05f));
        Vec4_Add(&pos, &pos, &dir);
        EftSpdLine_AddTrail(objId, &pos, &back);
        if (extra > 0.0f) {
            for (j = 0; j < (s32)extra; j++) {
                angle = EFT_RANDF() * 6.2831853f;
                radius = EFT_RANDF() + 0.0f;
                Vec4_Scale(&dir, &side, Mathf_Cos(angle) * radius * (BtlCharApi_GetHeight(objId) * 0.05f));
                Vec4_Add(&pos, &p0, &dir);
                Vec4_Scale(&dir, &up2, Mathf_Sin(angle) * radius * (BtlCharApi_GetHeight(objId) * 0.05f));
                Vec4_Add(&pos, &pos, &dir);
                Vec4_Scale(&dir, &back, (EFT_RANDF() * 5.0f + 1.5f) * (BtlCharApi_GetHeight(objId) * 0.05f));
                Vec4_Add(&pos, &pos, &dir);
                EftSpdLine_AddTrail(objId, &pos, &back);
            }
        }
    }
    return 1;
}
/* Animation event 8: fifteen streaks around one model part, flying against `dir`. The radius is the part's size
 * factor * 0.4, or height * 0.04 when that is under 0.8. */
s32 EftSpdLine_SpawnPartStreaks(s32 objId, Vec4 *dir, s32 part) {
    Vec4 pos;
    Vec4 end;
    Vec4 back;
    Vec4 tmp;
    static const EftVec sUp = { 0.0f, -1.0f, 0.0f, 1.0f }; /* .rodata 0x2ECC10 */
    EftVec up = sUp;
    Vec4 up2;
    Vec4 side;
    f32 size;
    f32 angle;
    f32 radius;
    f32 lead;
    f32 width;
    s32 i;

    if (gEftSpdLine == NULL) {
        return 0;
    }
    size = BtlCharApi_GetPartUnk5C(objId, part) * 0.4f;
    if (size < 0.8f) {
        size = BtlCharApi_GetHeight(objId) * 0.05f * 0.8f;
    }
    Vec4_Scale(&back, dir, -1.0f);
    Vec3_Normalize(&back, &back);
    Vec3_Cross(&side, &back, (Vec4 *)&up);
    Vec3_Normalize(&side, &side);
    Vec3_Cross(&up2, &back, &side);
    Vec3_Normalize(&up2, &up2);
    for (i = 0; i < 15; i++) {
        BtlCharApi_GetNodePos(objId, part, &pos);
        angle = EFT_RANDF() * 6.2831853f;
        radius = EFT_RANDF() * 1.8f + 0.1f;
        lead = EFT_RANDF() * 2.0f + 1.5f;
        radius *= size;
        lead *= size;
        Vec4_Scale(&tmp, &side, Mathf_Cos(angle) * radius);
        Vec4_Add(&pos, &pos, &tmp);
        Vec4_Scale(&tmp, &up2, Mathf_Sin(angle) * radius);
        Vec4_Add(&pos, &pos, &tmp);
        Vec4_Scale(&tmp, &back, lead);
        Vec4_Sub(&pos, &pos, &tmp);
        Vec4_Scale(&tmp, &back, (EFT_RANDF() * 3.0f + 3.0f) * size);
        Vec4_Add(&end, &pos, &tmp);
        width = (EFT_RANDF() * 2.0f + 1.0f) * (BtlCharApi_GetHeight(objId) * 0.05f);
        EftSpdLine_AddStreak(objId, &pos, &end, &back, width, EFT_RANDF() * 0.09f + 0.06f, 0.2f);
    }
    return 1;
}
