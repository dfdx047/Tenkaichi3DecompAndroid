#ifndef BATTLE_HUD_A_H
#define BATTLE_HUD_A_H

#include "types.h"
#include "sys/ramp.h"

/*
 * Battle HUD. Source range 0x2187E0-0x21CA60 (src/battle/hud_a.c .. hud_a_d.c).
 *
 * The HUD is a small scene graph. A HudNode has a position, a rotation, a list of sprites, a list of child
 * nodes, an update callback and a draw callback. HudNode_Update walks a tree calling the update callbacks;
 * HudNode_Draw walks it again, pushing each node's translation and rotation on the VU0 matrix stack and
 * calling the draw callbacks, which draw the node's sprites.
 *
 * Seven parts hang off the manager (gHud): the health / ki gauges, the timer, the team panel, a notice part,
 * the combo counter, the button prompts and a caption part. Each part has its own work, built by its Init from
 * one of five sprite sheets that live in a common file.
 */

/* A rectangle of a sprite: left, right, top, bottom. */
typedef struct HudRect {
    /* 0x00 */ s16 x0;
    /* 0x02 */ s16 x1;
    /* 0x04 */ s16 y0;
    /* 0x06 */ s16 y1;
} HudRect;

/* One 2D sprite (0x1C bytes). Setters are at 0x224B90..0x224CA0 (not in this range). */
typedef struct HudSprite {
    /* 0x00 */ u32 flags;      /* bit 0: hidden (0x224B90(sprite, show)); bit 1: mirrored (0x224BB0) */
    /* 0x04 */ s16 tex;        /* texture entry of the sheet, -1: untextured (0x224C00(sprite, tex, sub)) */
    /* 0x06 */ s16 texSub;     /* added to tex for a second entry (0x225A50) */
    /* 0x08 */ HudRect pos;    /* screen rectangle relative to the node: 0x224BD0(sprite, x0, x1, y0, y1);
                                  0x224CA0(sprite, dx, dy) moves it */
    /* 0x10 */ HudRect uv;     /* texel rectangle: 0x224BE8(sprite, u0, u1, v0, v1) */
    /* 0x18 */ u8 r, g, b, a;  /* 0x224C10(sprite, r, g, b, a); 0x80 is neutral */
} HudSprite; /* size 0x1C */

/* One texture entry of a sprite sheet (0x40 bytes); only what this range touches. */
typedef struct HudTex {
    /* 0x00 */ u8 unk0[0x28];
    /* 0x28 */ s32 mark;       /* cleared for every entry at the end of Hud_Draw */
    /* 0x2C */ u8 unk2C[0x14];
} HudTex;

/* A sprite sheet after Res_RelocateOffsets. */
typedef struct HudRes {
    /* 0x00 */ s32 texCount;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ HudTex *tex;
} HudRes;

typedef struct HudNode HudNode;
struct HudNode {
    /* 0x00 */ u32 flags;      /* HUD_NODE_HIDDEN, HUD_NODE_MIRROR */
    /* 0x04 */ f32 rot;        /* rotation about Z, radians */
    /* 0x08 */ s32 unk8[2];
    /* 0x10 */ s32 x;          /* set with 0x2264C8(node, x, y) */
    /* 0x14 */ s32 y;
    /* 0x18 */ s32 ofsX;       /* added to x / y */
    /* 0x1C */ s32 ofsY;
    /* 0x20 */ u32 spriteCount;
    /* 0x24 */ HudSprite **sprites;
    /* 0x28 */ u32 childCount;
    /* 0x2C */ HudNode **children;
    /* 0x30 */ void (*update)(); /* called without arguments; a callee that wants the node reads $a0, which
                                    still holds it (HudTeam_UpdateRoot) */
    /* 0x34 */ void (*draw)(HudNode *node);
}; /* size 0x38 */

#define HUD_NODE_HIDDEN 1 /* 0x2264A8(node, show) writes !show here */
#define HUD_NODE_MIRROR 2 /* draw mirrored about x = 255.5 (the second player's copy of a part) */

#define HUD_RES_COUNT 5

#define HUD_SHOW_GAUGES 0x01 /* health / ki gauges and the team panel */
#define HUD_SHOW_TIMER 0x02
#define HUD_SHOW_NOTICE 0x04
#define HUD_SHOW_COMBO 0x08
#define HUD_SHOW_PROMPT 0x10

/* HUD manager work (gHud, 0x38 bytes). */
typedef struct Hud {
    /* 0x00 */ HudNode *gauge;   /* built by 0x21FC10 from res[0] */
    /* 0x04 */ HudNode *timer;   /* 0x22F340, res[0] */
    /* 0x08 */ HudNode *team;    /* HudTeam_Init, res[0] */
    /* 0x0C */ HudNode *notice;  /* 0x22AF88, res[1] */
    /* 0x10 */ HudNode *combo;   /* 0x224198, res[2] */
    /* 0x14 */ HudNode *prompt;  /* 0x22E218, res[3] */
    /* 0x18 */ HudNode *caption;    /* HudCaption_Init, res[4] */
    /* 0x1C */ HudRes *res[HUD_RES_COUNT];
    /* 0x30 */ union {
        u32 flags;               /* HUD_SHOW_*: tested as masks by Hud_Draw */
        struct {                 /* written as bit fields by the Hud_Show* setters */
            u32 showGauges : 1;
            u32 showTimer : 1;
            u32 showNotice : 1;
            u32 showCombo : 1;
            u32 showPrompt : 1;
        };
    };
    /* 0x34 */ s32 replayMode;   /* 0, 1, 2: how a replay shows the HUD (Hud_Draw) */
} Hud;

/* A health value and its maximum, by side. */
typedef struct HudTeamHp {
    /* 0x00 */ s32 cur[2];
    /* 0x08 */ s32 max[2];
} HudTeamHp;

/* Team panel work (gHudTeam, 0x148 bytes). Arrays of two are indexed by side. The pairs are nested structures
   in the original as well: their members are addressed as (work + (side * 4 + offset of the pair)) + offset of
   the member, which this compiler only emits for a member of a nested structure. */
typedef struct HudTeam {
    /* 0x000 */ HudRes *res;
    /* 0x004 */ HudSprite *sprites;  /* HUD_TEAM_SPR_COUNT */
    /* 0x008 */ HudNode *nodes;      /* HUD_TEAM_NODE_COUNT */
    /* 0x00C */ s32 side;            /* side being updated / drawn */
    /* 0x010 */ HudTeamHp hp;        /* stand-by member's health and its maximum */
    /* 0x020 */ struct {
        s32 gauge[2];                /* switch gauge, HUD_TEAM_GAUGE_FULL = full */
        s32 reserve[2];              /* 0x028: members in reserve (alive members - 1) */
    } sw;
    /* 0x030 */ HudRes *faceRes[2];  /* face sheet of the member being shown (BattleMember + 0x58) */
    /* 0x038 */ Ramp flash[2];       /* blink of the "full" marker of the switch gauge */
    /* 0x068 */ s32 shown[2];        /* reserve count != 0 */
    /* 0x070 */ Ramp slide[2];       /* 0 = in place, 1 = slid off the screen (while switching) */
    /* 0x0A0 */ s32 switching[2];
    /* 0x0A8 */ Ramp flipA[2];       /* face change animation */
    /* 0x0D8 */ Ramp flipB[2];
    /* 0x108 */ s32 flipDone[2];
    /* 0x110 */ struct {
        s32 cur[2];                  /* member index of the stand-by member (-1: none) */
        s32 prev[2];                 /* 0x118: the one shown before the last flip */
    } target;
    /* 0x120 */ HudTeamHp prevHp;    /* copy of hp taken when a flip starts */
    /* 0x130 */ Ramp hide;           /* Hud_SlideOut / Hud_SlideIn */
} HudTeam;

#define HUD_TEAM_SPR_COUNT 12
#define HUD_TEAM_NODE_COUNT 9
#define HUD_TEAM_GAUGE_FULL 100000

/* Caption part work (gHudCaption, 0x24 bytes). */
typedef struct HudCaption {
    /* 0x00 */ HudRes *res;
    /* 0x04 */ HudSprite *sprites; /* 3 */
    /* 0x08 */ HudNode *nodes;     /* 2 */
    /* 0x0C */ Ramp pulse;
} HudCaption;

/* Health gauge work (gHudGauge): only the fields used before 0x21CA60. The full layout is HudBWork in
 * src/battle/hud_b.c, whose matching setters fix which word is which: HudGauge_SetHp writes +0x30. */
typedef struct HudGauge {
    /* 0x00 */ HudRes *res;
    /* 0x04 */ HudSprite *sprites;
    /* 0x08 */ u8 unk8[0x1C];
    /* 0x24 */ s32 side;        /* side being updated / drawn */
    /* 0x28 */ s32 hpShown[2];  /* the health the trailing (damage) bar shows; falls towards cur.hp */
    /* 0x30 */ struct {         /* a nested structure (addressed like the pairs of HudTeam) */
        s32 hp[2];              /* health now (HudGauge_SetHp) */
    } cur;
} HudGauge;

#define HUD_GAUGE_BAR 10000  /* health per bar */

extern Hud *gHud;
extern HudTeam *gHudTeam;
extern HudCaption *gHudCaption;
extern HudGauge *gHudGauge;

/* hud_a.c */
void Hud_ClearTexMarks(void);
void HudNode_Update(HudNode *node);
void HudNode_Draw(HudNode *node, s32 mirror);
void Hud_SetReplayMode(s32 mode);
void Hud_ShowAll(s32 on);
void Hud_ShowGauges(s32 on);
void Hud_ShowTimer(s32 on);
void Hud_ShowNotice(s32 on);
void Hud_ShowCombo(s32 on);
void Hud_ShowPrompt(s32 on);
void Hud_Init(void);
void Hud_Term(void);
void Hud_PreUpdate(void);
void Hud_Reset(void);
void Hud_SlideOut(f32 seconds);
void Hud_SlideIn(f32 seconds);
void Hud_Draw(void);

/* hud_a_b.c */
void HudTeam_SelectSide(s32 side);
void HudTeam_SetTargetHp(s32 side, s32 hp);
void HudTeam_SetTargetHpMax(s32 side, s32 hpMax);
void HudTeam_SetSwitchGauge(s32 side, s32 gauge);
void HudTeam_SetTarget(s32 side, s32 member);
void HudTeam_SetReserveCount(s32 side, s32 count);
void HudTeam_SlideOut(f32 seconds);
void HudTeam_SlideIn(f32 seconds);
void HudTeam_SetSwitching(s32 side, s32 on);
void HudTeam_StartFlipNext(s32 side);
void HudTeam_StartFlipPrev(s32 side);
void HudTeam_Init(HudNode **out, HudRes *res);
void HudTeam_Term(void);
void HudTeam_Reset(void);

/* hud_a_c.c */
void HudCaption_Init(HudNode **out, HudRes *res);
void HudCaption_Term(void);
void HudCaption_Reset(void);

#endif
