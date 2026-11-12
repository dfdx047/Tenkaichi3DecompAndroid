#ifndef SYS_GFXM_D_H
#define SYS_GFXM_D_H

#include "common.h"

/*
 * Clip-list setters of the Flash-like movie player, 0x10EC18..0x10FB40: the second half of the "sprite instances"
 * group of sys/gfxm_c.c (FlashClipList_*). The types below are LOCAL VIEWS of the ones in sys/gfxm_c.h, with the
 * same field names and a `D` in the type name so that the two headers can be included together; when the files
 * are merged they should be replaced by FlashClip / FlashClipList / FlashRef / FlashTl / FlashProp / Flash.
 */

/* Flash (the movie object), as far as it is read here. */
typedef struct FlashDMovie {
    /* 0x00 */ u8 unk00[0x14];
    /* 0x14 */ s32 speed;        /* frames per tick */
} FlashDMovie;

/* FlashTl: a timeline. */
typedef struct FlashDTl {
    /* 0x00 */ FlashDMovie *owner;
    /* 0x04 */ u8 unk04[0x11];
    /* 0x15 */ u8 state;         /* FLASH_TL_: bit 4 = a jump set the frame, the display list must be rebuilt */
    /* 0x16 */ u8 unk16[2];
} FlashDTl; /* size 0x18 */

#define FLASHD_TL_GOTO 0x04

/* FlashUv without its last word (the four texel coordinates FlashClipList_SetUv copies). */
typedef struct FlashDUv {
    /* 0x00 */ s32 u0;
    /* 0x04 */ s32 v0;
    /* 0x08 */ s32 u1;
    /* 0x0C */ s32 v1;
} FlashDUv;

/* FlashClip.flags */
#define FLASHD_CLIP_FREE_RUN 8 /* keeps advancing while its parent is stopped */

/* FlashProp.flags: the overrides set here. */
#define FLASHD_OV_TEX 0x01
#define FLASHD_OV_POS 0x02
#define FLASHD_OV_UV 0x04
#define FLASHD_OV_ALPHA 0x08
#define FLASHD_OV_SCALE 0x10
#define FLASHD_OV_COLOR 0x20

/* FlashClip: one sprite instance. Instances with the same name follow each other in the list. */
typedef struct FlashDClip {
    /* 0x00 */ char name[0x44];  /* (0x40 bytes of name, then id and index) */
    /* 0x44 */ s32 flags;        /* FLASH_CLIP_ */
    /* 0x48 */ s32 ovFlags;      /* prop.flags: FLASH_OV_ */
    /* 0x4C */ f32 ovX;          /* prop.x */
    /* 0x50 */ f32 ovY;
    /* 0x54 */ f32 ovAlpha;      /* clamped to 0..1 */
    /* 0x58 */ f32 ovScaleX;
    /* 0x5C */ f32 ovScaleY;
    /* 0x60 */ f32 ovColor;      /* clamped to 0..1 */
    /* 0x64 */ FlashDUv ovUv;
    /* 0x74 */ s32 ovTex;
    /* 0x78 */ u8 unk78[8];      /* prop.mtx: flag word, scale x */
    /* 0x80 */ f32 x;            /* prop.mtx translation x */
    /* 0x84 */ u8 unk84[8];
    /* 0x8C */ f32 y;            /* prop.mtx translation y */
    /* 0x90 */ u8 unk90[0x18];
    /* 0xA8 */ f32 alphaMul;     /* prop.cx: alpha multiplier */
    /* 0xAC */ u8 unkAC[6];
    /* 0xB2 */ s16 alphaAdd;     /* prop.cx: alpha term */
    /* 0xB4 */ s32 preDraw;      /* void (*)(void *arg) */
    /* 0xB8 */ s32 postDraw;
    /* 0xBC */ s32 preArg;
    /* 0xC0 */ s32 postArg;
    /* 0xC4 */ s32 drawOver;     /* void (*)(void *arg, FlashProp *prop) */
    /* 0xC8 */ s32 overArg;
    /* 0xCC */ FlashDTl tl;
} FlashDClip; /* size 0xE4 */

typedef struct FlashDClipList {
    /* 0x00 */ FlashDClip *items;
    /* 0x04 */ u32 count;
} FlashDClipList;

/* FlashRef: what Flash_FindLabel returns. */
typedef struct FlashDRef {
    /* 0x00 */ s32 index;        /* first instance; -1 when not found (tested by the Flash_Clip* wrappers) */
    /* 0x04 */ s32 more;         /* how many further instances have the same name */
} FlashDRef;

void FlashClipList_GotoLabel(FlashDClipList *list, FlashDRef *ref, char *label);
void FlashClipList_SetPreDraw(FlashDClipList *list, FlashDRef *ref, s32 fn, s32 arg);
void FlashClipList_SetPostDraw(FlashDClipList *list, FlashDRef *ref, s32 fn, s32 arg);
void FlashClipList_SetDrawOver(FlashDClipList *list, FlashDRef *ref, s32 fn, s32 arg);
void FlashClipList_SetFlags(FlashDClipList *list, FlashDRef *ref, s32 mask, u8 on);
void FlashClipList_SetOverride(FlashDClipList *list, FlashDRef *ref, s32 mask, u8 on);
void FlashClipList_SetOffset(FlashDClipList *list, FlashDRef *ref, s32 x, s32 y);
void FlashClipList_SetScale(FlashDClipList *list, FlashDRef *ref, f32 x, f32 y);
void FlashClipList_SetAlpha(FlashDClipList *list, FlashDRef *ref, f32 alpha);
void FlashClipList_SetColor(FlashDClipList *list, FlashDRef *ref, f32 color);
void FlashClipList_SetTex(FlashDClipList *list, FlashDRef *ref, s32 tex);
void FlashClipList_SetUv(FlashDClipList *list, FlashDRef *ref, FlashDUv *uv);
void FlashClipList_GetPos(FlashDClipList *list, FlashDRef *ref, s32 *x, s32 *y);
f32 FlashClipList_GetAlpha(FlashDClipList *list, FlashDRef *ref);

#endif
