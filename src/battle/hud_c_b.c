#include "common.h"
#include "battle/hud_c.h"
#include "battle/battle.h"
#include "sys/randf.h"
#include "sys/heap.h"

#ifdef PORT
/* PC build, online play: the player who joined is side 1 of the game, and would have their own gauges, team, prompts
   and counters on the right. With the swap (gs/net.c) the two sides change places on THIS copy's screen only: a
   side's data is the same, where it is drawn is the other side's place. HUD_SCR(side) is the place of a side. */
extern int Port_HudSwap(void);
#define HUD_SCR(side) ((side) ^ Port_HudSwap())
#else
#define HUD_SCR(side) (side)
#endif
/*
 * Battle HUD: the combo part, 0x222730-0x224B50 (the fifth part the manager builds, gHud->combo, from sprite
 * sheet 2). Per side it shows
 *   - an event message (one of five pictures, textures 3..7 of the sheet) that slides in, stays 1.2 s and slides out,
 *   - a text line on a grey plate: the name of entry `text` of the fighter's skill script, drawn by the battle text
 *     module at the plate's screen position after the node trees (HudCombo_DrawText),
 *   - the combo damage: a label and five digits; the number counts up to the new total over 0.6 .. 1.5 s and its
 *     last digit is random while it counts,
 *   - the hit counter: a label and three digits that pop (grow by half and shrink back) on every new hit.
 *
 * Like the other parts it is one node tree drawn twice, HudCombo_SelectSide choosing the side: side 0 is placed
 * from the left edge, side 1 from the right one (x' = 372 - x, the text line 512 - x). Hud_PreUpdate feeds it
 * (HudCombo_SetHits / SetDamage / SetText / SetMessage); every animation stands still while the battle is paused.
 *
 * Random numbers: HudCombo_SetDamageDigits draws from libc rand() (Rand_IntRange) while a damage count is rolling.
 * No pad, clock or camera is read.
 */

#define HUDC_PAUSED() (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)

#define HUD_COMBO_SPR_COUNT 12
#define HUD_COMBO_NODE_COUNT 6

/* Sprites. */
#define HUD_COMBO_SPR_MESSAGE 0
#define HUD_COMBO_SPR_PLATE 1     /* untextured 128 x 32 plate of the text line */
#define HUD_COMBO_SPR_DAMAGE 2    /* label; the five digits follow (3..7), units first */
#define HUD_COMBO_SPR_HITS 8      /* label; the three digits follow (9..11), units first */

/* Nodes: [0] root (slide), [1] holder of the four below. */
#define HUD_COMBO_NODE_MESSAGE 2
#define HUD_COMBO_NODE_TEXT 3
#define HUD_COMBO_NODE_DAMAGE 4
#define HUD_COMBO_NODE_HITS 5

/* States of the damage and hit count nodes. */
#define HUD_COMBO_ST_IN 0     /* start sliding in (-> 1 -> 3) */
#define HUD_COMBO_ST_SHOWN 3
#define HUD_COMBO_ST_POP 4    /* start the "value changed" animation (-> 5 -> 3) */
#define HUD_COMBO_ST_OUT 7    /* start sliding out (-> 8 -> 10) */
#define HUD_COMBO_ST_OFF 10

#define HUD_COMBO_DAMAGE_MAX 199998 /* clamp of the stored value; the digits show at most 99999 */
#define HUD_COMBO_HITS_MAX 999

/* Combo part work (gHudCombo, 0x18C bytes). Arrays of two are indexed by side. */
typedef struct HudCombo {
    /* 0x000 */ HudCRes *res;        /* sprite sheet 2 of the HUD file */
    /* 0x004 */ HudCSprite *sprites; /* HUD_COMBO_SPR_COUNT */
    /* 0x008 */ HudCNode *nodes;     /* HUD_COMBO_NODE_COUNT */
    /* 0x00C */ s32 side;            /* side being updated / drawn */
    /* 0x010 */ s32 msgState[2];     /* 0 start, 1 slide in, 2 hold, 3 slide out, 4 (or -1) off */
    /* 0x018 */ s32 dmgState[2];     /* HUD_COMBO_ST_* */
    /* 0x020 */ s32 hitState[2];     /* HUD_COMBO_ST_* */
    /* 0x028 */ s32 textState[2];    /* 0 start, 1 fade in, 2 hold, 3 fade out, 4 (or -1) off */
    /* 0x030 */ s32 msg[2];          /* message picture, 0..4 */
    /* 0x038 */ s32 damage[2];       /* combo damage to show */
    /* 0x040 */ s32 damageFrom[2];   /* value the count started from */
    /* 0x048 */ u32 hits[2];         /* hit count on show (at most 999) */
    /* 0x050 */ s32 hitsRaw[2];      /* hit count as given */
    /* 0x058 */ s32 hitsPrev[2];     /* hit count on show before the last change */
    /* 0x060 */ s32 text[2];         /* entry of the skill script whose name the text line shows */
    /* 0x068 */ Ramp msgRamp[2];
    /* 0x098 */ Ramp textRamp[2];
    /* 0x0C8 */ Ramp dmgRamp[2];
    /* 0x0F8 */ Ramp hitRamp[2];
    /* 0x128 */ Ramp countRamp[2];   /* 0 -> 1 while the damage number counts from damageFrom to damage */
    /* 0x158 */ Ramp hide;           /* 0 on screen .. 1 away: HudCombo_SlideOut / SlideIn */
    /* 0x170 */ u16 lastDigit[2];    /* last random units digit of the rolling damage number */
    /* 0x174 */ s32 textPos[2][2];   /* screen position of the text line, (-1, -1) while its plate is hidden */
    /* 0x184 */ f32 textAlpha[2];
} HudCombo; /* size 0x18C */

/* The side's fighter object as BtlCtrl_GetObj returns it: only what this file reads. */
typedef struct HudCObj {
    /* 0x00 */ u8 unk0[0xBC];
    /* 0xBC */ void *skillScript; /* given to BtlMenu_SetScript2 before the name is drawn */
} HudCObj;

/* 0x2FEB58: this object's .sdata word (the notice / prompt / timer pointers of hud_e*.c follow it). */
HudCombo *gHudCombo = NULL;

extern void *memset(void *dst, s32 c, u32 n);
extern void Vu0Cur_StoreMtx(f32 (*m)[4]);
extern HudCObj *BtlCtrl_GetObj(s32 side);
extern void BtlMenu_SetScript2(void *script);
extern void BtlText_DrawEntryName(s32 x, s32 y, s32 n, s32 align, f32 alpha);
/* src/battle/hud_d.c */
extern void HudSprite_Draw(HudCSprite *spr, HudCRes *res, s32 ctx2);
extern void HudNode_Show(HudCNode *node, s32 show);
extern void HudNode_SetPos(HudCNode *node, s32 x, s32 y);
extern void HudNode_SetOfs(HudCNode *node, s32 x, s32 y);

/* Zeroes a small local. The original calls memset for 8-byte locals, which this compiler only does when the
   pointer comes through a variable (an inlined helper or constructor in the original). */
static inline void HudCombo_Zero(void *p, u32 size) {
    memset(p, 0, size);
}

void HudCombo_UpdateMessage(HudCNode *node);
void HudCombo_UpdateText(HudCNode *node);
void HudCombo_UpdateDamage(HudCNode *node);
void HudCombo_UpdateHits(HudCNode *node);
void HudCombo_DrawSprites(HudCNode *node);
void HudCombo_StoreTextPos(HudCNode *node);

/* Update of node 0 (root): steps the slide once a frame (on the side 0 pass) and moves the part off its side's
   edge: 256 pixels to the left for side 0, to the right for side 1. */
void HudCombo_UpdateRoot(HudCNode *node) {
    if (!HUDC_PAUSED() && gHudCombo->side == 0) {
        Ramp_Step(&gHudCombo->hide);
    }
    if (HUD_SCR(gHudCombo->side) == 0) {
        HudNode_SetPos(node, gHudCombo->hide.value * -256.0f, gHudCombo->hide.value * 0.0f);
    } else {
        HudNode_SetPos(node, gHudCombo->hide.value * 256.0f, gHudCombo->hide.value * 0.0f);
    }
}

/* Update of the message node: picks the picture (texture 3 + message), slides it in from its own width in 0.1 s,
   holds it 1.2 s and slides it out in 0.1 s. Message 4 sits 310 pixels lower. */
void HudCombo_UpdateMessage(HudCNode *node) {
    f32 pos[2];
    s32 msg = gHudCombo->msg[gHudCombo->side];
    Ramp *ramp = &gHudCombo->msgRamp[gHudCombo->side];
    HudCSprite *spr;
    f32 width;
    s32 done;

    HudCombo_Zero(pos, sizeof(pos));
    spr = &gHudCombo->sprites[HUD_COMBO_SPR_MESSAGE];
    switch (msg) {
    case 0:
        HudSprite_InitTex(spr, gHudCombo->res, 3, 0);
        break;
    case 1:
        HudSprite_InitTex(spr, gHudCombo->res, 4, 0);
        break;
    case 2:
        HudSprite_InitTex(spr, gHudCombo->res, 5, 0);
        break;
    case 3:
        HudSprite_InitTex(spr, gHudCombo->res, 6, 0);
        break;
    case 4:
        HudSprite_InitTex(spr, gHudCombo->res, 7, 0);
        break;
    }
    width = -(f32)(spr->u1 - spr->u0);
    HudSprite_Show(spr, 1);
    switch (gHudCombo->msgState[gHudCombo->side]) {
    case 0:
        Ramp_Start(ramp, 0.1f, 1.0f, 0.0f);
        gHudCombo->msgState[gHudCombo->side]++;
        /* fall through */
    case 1:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            Ramp_Start(ramp, 1.2f, 0.0f, 1.0f);
            gHudCombo->msgState[gHudCombo->side]++;
            pos[0] = 0.0f;
        } else {
            pos[0] = width * ramp->value;
        }
        break;
    case 2:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            Ramp_Start(ramp, 0.1f, 0.0f, 1.0f);
            gHudCombo->msgState[gHudCombo->side]++;
        }
        break;
    case 3:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            HudSprite_Show(spr, 0);
            gHudCombo->msgState[gHudCombo->side]++;
        } else {
            pos[0] = width * ramp->value;
        }
        break;
    case 4:
    default:
        HudSprite_Show(spr, 0);
        return;
    }
    pos[0] += 5.0f;
    if (msg == 4) {
        pos[1] += 310.0f;
    }
    if (HUD_SCR(gHudCombo->side) == 0) {
        HudNode_SetOfs(node, pos[0], pos[1]);
    } else {
        HudNode_SetOfs(node, 372 - (s32)pos[0], pos[1]);
    }
}

/* Update of the text node: fades the plate in over 0.1 s, holds it 1.2 s and fades it out. */
void HudCombo_UpdateText(HudCNode *node) {
    f32 pos[2];
    s32 alpha = 0x80;
    Ramp *ramp = &gHudCombo->textRamp[gHudCombo->side];
    HudCSprite *spr;
    f32 width;
    s32 done;

    HudCombo_Zero(pos, sizeof(pos));
    spr = &gHudCombo->sprites[HUD_COMBO_SPR_PLATE];
    width = -(f32)(spr->u1 - spr->u0);
    HudSprite_Show(spr, 1);
    switch (gHudCombo->textState[gHudCombo->side]) {
    case 0:
        Ramp_Start(ramp, 0.1f, 0.0f, 1.0f);
        gHudCombo->textState[gHudCombo->side]++;
        /* fall through */
    case 1:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            Ramp_Start(ramp, 1.2f, 0.0f, 1.0f);
            alpha = 0x80;
            gHudCombo->textState[gHudCombo->side]++;
            pos[0] = 0.0f;
        } else {
            pos[0] = width * ramp->value;
            alpha = (u8)(ramp->value * 128.0f);
        }
        break;
    case 2:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            Ramp_Start(ramp, 0.1f, 1.0f, 0.0f);
            gHudCombo->textState[gHudCombo->side]++;
        }
        break;
    case 3:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            HudSprite_Show(spr, 0);
            alpha = 0;
            gHudCombo->textState[gHudCombo->side]++;
        } else {
            pos[0] = width * ramp->value;
            alpha = (u8)(ramp->value * 128.0f);
        }
        break;
    case 4:
    default:
        HudSprite_Show(spr, 0);
        return;
    }
    HudSprite_SetColor(spr, 0x80, 0x80, 0x80, alpha);
    pos[0] += 15.0f;
    if (HUD_SCR(gHudCombo->side) == 0) {
        HudNode_SetOfs(node, pos[0], pos[1]);
    } else {
        HudNode_SetOfs(node, 512 - (s32)pos[0], pos[1]);
    }
}

/* Sets the three digit sprites of the hit count (sprites 1..3 of the node, units first, 22 pixels apart) and hides
   the leading zeros. */
void HudCombo_SetHitDigits(HudCNode *node) {
    u32 pow[6] = { 1, 10, 100, 1000, 10000, 100000 };
    u16 uvs[10][4] = {
        { 0x01, 0x1F, 0x00, 0x20 }, { 0x21, 0x3F, 0x00, 0x20 }, { 0x41, 0x5F, 0x00, 0x20 }, { 0x61, 0x7F, 0x00, 0x20 },
        { 0x01, 0x1F, 0x20, 0x40 }, { 0x21, 0x3F, 0x20, 0x40 }, { 0x41, 0x5F, 0x20, 0x40 }, { 0x61, 0x7F, 0x20, 0x40 },
        { 0x01, 0x1F, 0x40, 0x60 }, { 0x21, 0x3F, 0x40, 0x60 },
    };
    s32 ofs[2];
    u32 hits;
    s32 i;
    HudCSprite *spr;
    u32 digit;

    HudCombo_Zero(ofs, sizeof(ofs));
    hits = gHudCombo->hits[gHudCombo->side];
    ofs[0] = 36;
    ofs[1] = 0;
    for (i = 0; (u32)i < 3; i++) {
        spr = node->sprites[i + 1];
        HudSprite_InitTex(spr, gHudCombo->res, 0, 0);
        HudSprite_SetRect(spr, 1, 30, -32, 0);
        digit = hits % pow[i + 1] / pow[i];
        HudSprite_Show(spr, 1);
        {
            s32 step = -22;

            HudSprite_Move(spr, step * i + ofs[0], ofs[1]);
        }
        HudSprite_SetUv(spr, uvs[digit][0], uvs[digit][1], uvs[digit][2], uvs[digit][3]);
    }
    i = 2;
    do {
        spr = node->sprites[i + 1];
        i--;
        if (spr->u0 != uvs[0][0] || spr->v0 != uvs[0][2]) {
            break;
        }
        HudSprite_Show(spr, 0);
    } while (i > 0);
}

/* Sets the five digit sprites of the combo damage (sprites 1..5 of the node, units first, 12 pixels apart) and
   hides the leading zeros. The number is damageFrom + (damage - damageFrom) * countRamp, at most 99999. While the
   count is running (and the number is 100..99998) the units digit is a random digit that differs from the one
   shown the frame before: one or more draws from libc rand() per side per drawn frame, none while paused. */
void HudCombo_SetDamageDigits(HudCNode *node) {
    Ramp *ramp = &gHudCombo->countRamp[gHudCombo->side];
    u32 pow[6] = { 1, 10, 100, 1000, 10000, 100000 };
    u16 uvs[10][4] = {
        { 0x00, 0x20, 0x00, 0x20 }, { 0x20, 0x40, 0x00, 0x20 }, { 0x40, 0x60, 0x00, 0x20 }, { 0x60, 0x80, 0x00, 0x20 },
        { 0x00, 0x20, 0x20, 0x40 }, { 0x20, 0x40, 0x20, 0x40 }, { 0x40, 0x60, 0x20, 0x40 }, { 0x60, 0x80, 0x20, 0x40 },
        { 0x00, 0x20, 0x40, 0x60 }, { 0x20, 0x40, 0x40, 0x60 },
    };
    s32 ofs[2];
    u32 value;
    s32 i;
    HudCSprite *spr;
    u32 digit;

    HudCombo_Zero(ofs, sizeof(ofs));
    if (!HUDC_PAUSED()) {
        Ramp_Step(ramp);
    }
    if (gHudCombo->damage[gHudCombo->side] < 2) {
        value = gHudCombo->damage[gHudCombo->side];
    } else {
        value = gHudCombo->damageFrom[gHudCombo->side];
        value = (f32)value +
                (f32)(gHudCombo->damage[gHudCombo->side] - gHudCombo->damageFrom[gHudCombo->side]) * ramp->value;
    }
    if (value > 99999) {
        value = 99999;
    }
    ofs[0] = 96;
    ofs[1] = 0;
    for (i = 0; (u32)i < 5; i++) {
        spr = node->sprites[i + 1];
        HudSprite_InitTex(spr, gHudCombo->res, 1, 0);
        HudSprite_SetRect(spr, 1, 31, 1, 31);
        digit = value % pow[i + 1] / pow[i];
        if (!HUDC_PAUSED() && i == 0 && value - 100 <= 99898 && ramp->value != ramp->target) {
            do {
                digit = Rand_IntRange(0, 9);
            } while (digit == gHudCombo->lastDigit[gHudCombo->side]);
            gHudCombo->lastDigit[gHudCombo->side] = digit;
        }
        HudSprite_Show(spr, 1);
        {
            s32 step = -12;

            HudSprite_Move(spr, step * i + ofs[0], ofs[1]);
        }
        HudSprite_SetUv(spr, uvs[digit][0] + 1, uvs[digit][1] - 1, uvs[digit][2] + 1, uvs[digit][3] - 1);
    }
    i = 4;
    do {
        spr = node->sprites[i + 1];
        i--;
        if (spr->u0 != 1 || spr->v0 != 1) {
            break;
        }
        HudSprite_Show(spr, 0);
    } while (i > 0);
}

/* Update of the damage node: sets the digits and runs the node's state: slide in (0.1 s), a 10 pixel nudge out and
   back when the value changes (0.1 s each way), slide out. */
void HudCombo_UpdateDamage(HudCNode *node) {
    f32 pos[2];
    Ramp *ramp = &gHudCombo->dmgRamp[gHudCombo->side];
    HudCSprite *spr;
    f32 width;
    s32 done;

    HudCombo_Zero(pos, sizeof(pos));
    spr = &gHudCombo->sprites[HUD_COMBO_SPR_DAMAGE];
    width = -(f32)(spr->u1 - spr->u0);
    HudCombo_SetDamageDigits(node);
    HudNode_Show(node, 1);
    switch (gHudCombo->dmgState[gHudCombo->side]) {
    case 0:
        HudNode_Show(node, 1);
        Ramp_Start(ramp, 0.1f, 1.0f, 0.0f);
        gHudCombo->dmgState[gHudCombo->side]++;
        /* fall through */
    case 1:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            gHudCombo->dmgState[gHudCombo->side] = HUD_COMBO_ST_SHOWN;
            pos[0] = 0.0f;
        } else {
            pos[0] = width * ramp->value;
        }
        break;
    case 4:
        Ramp_Start(ramp, 0.1f, 0.0f, 1.0f);
        gHudCombo->dmgState[gHudCombo->side]++;
        /* fall through */
    case 5:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            if (ramp->value == 1.0f) {
                Ramp_Start(ramp, 0.1f, 1.0f, 0.0f);
            } else if (ramp->value == 0.0f) {
                gHudCombo->dmgState[gHudCombo->side] = HUD_COMBO_ST_SHOWN;
            }
        }
        pos[0] -= ramp->value * 10.0f;
        break;
    case 7:
        Ramp_Start(ramp, 0.1f, 0.0f, 1.0f);
        gHudCombo->dmgState[gHudCombo->side]++;
        /* fall through */
    case 8:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            HudNode_Show(node, 0);
            gHudCombo->dmgState[gHudCombo->side] = HUD_COMBO_ST_OFF;
        } else {
            pos[0] = width * ramp->value;
        }
        break;
    case 10:
        HudNode_Show(node, 0);
        gHudCombo->damageFrom[gHudCombo->side] = 0;
        gHudCombo->damage[gHudCombo->side] = 0;
        return;
    case 2:
    case 3:
    case 6:
    case 9:
        break;
    }
    pos[0] += 5.0f;
    if (HUD_SCR(gHudCombo->side) == 0) {
        HudNode_SetOfs(node, pos[0], pos[1]);
    } else {
        HudNode_SetOfs(node, 372 - (s32)pos[0], pos[1]);
    }
}

/* Update of the hit count node: sets the digits and runs the node's state: slide in, a pop of the digits (their
   height grows to 1.5 and back, 0.1 s each way) when the count changes, slide out. */
void HudCombo_UpdateHits(HudCNode *node) {
    f32 pos[2];
    f32 scale = 1.0f;
    Ramp *ramp = &gHudCombo->hitRamp[gHudCombo->side];
    HudCSprite *spr;
    f32 width;
    s32 i;
    s32 done;

    HudCombo_Zero(pos, sizeof(pos));
    spr = &gHudCombo->sprites[HUD_COMBO_SPR_HITS];
    width = -(f32)(spr->u1 - spr->u0);
    HudCombo_SetHitDigits(node);
    HudNode_Show(node, 1);
    switch (gHudCombo->hitState[gHudCombo->side]) {
    case 0:
        HudNode_Show(node, 1);
        Ramp_Start(ramp, 0.1f, 1.0f, 0.0f);
        gHudCombo->hitState[gHudCombo->side]++;
        /* fall through */
    case 1:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            Ramp_Start(ramp, 1.2f, 0.0f, 1.0f);
            gHudCombo->hitState[gHudCombo->side] = HUD_COMBO_ST_SHOWN;
            pos[0] = 0.0f;
        } else {
            pos[0] = width * ramp->value;
        }
        break;
    case 4:
        Ramp_Start(ramp, 0.1f, 0.0f, 1.0f);
        gHudCombo->hitState[gHudCombo->side]++;
        /* fall through */
    case 5:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            if (ramp->value == 1.0f) {
                Ramp_Start(ramp, 0.1f, 1.0f, 0.0f);
            } else if (ramp->value == 0.0f) {
                gHudCombo->hitState[gHudCombo->side] = HUD_COMBO_ST_SHOWN;
            }
        }
        scale = ramp->value * 0.5f + 1.0f;
        break;
    case 7:
        Ramp_Start(ramp, 0.1f, 0.0f, 1.0f);
        gHudCombo->hitState[gHudCombo->side]++;
        /* fall through */
    case 8:
        if (!HUDC_PAUSED()) {
            done = Ramp_Step(ramp);
        } else {
            done = 0;
        }
        if (done) {
            HudNode_Show(node, 0);
            gHudCombo->hitState[gHudCombo->side] = HUD_COMBO_ST_OFF;
        } else {
            pos[0] = width * ramp->value;
        }
        break;
    case 10:
        HudNode_Show(node, 0);
        return;
    case 2:
    case 3:
    case 6:
    case 9:
        break;
    }
    pos[0] += 5.0f;
    if (HUD_SCR(gHudCombo->side) == 0) {
        HudNode_SetOfs(node, pos[0], pos[1]);
    } else {
        HudNode_SetOfs(node, 372 - (s32)pos[0], pos[1]);
    }
    for (i = 0; i < 3; i++) {
        HudSprite_Scale(node->sprites[i + 1], 1.0f, scale);
    }
}

/* Draw callback: every sprite of the node. */
void HudCombo_DrawSprites(HudCNode *node) {
    u32 i;

    for (i = 0; i < node->spriteCount; i++) {
        HudSprite_Draw(node->sprites[i], gHudCombo->res, 0);
    }
}

/* Draw callback of the text node: draws nothing; it notes where the plate is on the screen (the translation of the
   current VU0 matrix) and its alpha for HudCombo_DrawText, or (-1, -1) while the plate is hidden. */
void HudCombo_StoreTextPos(HudCNode *node) {
    f32 m[4][4];
    HudCSprite *spr = node->sprites[0];
    f32 alpha = spr->a / 128.0f;

    if (spr->flags & HUDC_SPR_HIDDEN) {
        gHudCombo->textPos[gHudCombo->side][0] = -1;
        gHudCombo->textPos[gHudCombo->side][1] = -1;
    } else {
        Vu0Cur_StoreMtx(m);
        gHudCombo->textAlpha[gHudCombo->side] = alpha;
        gHudCombo->textPos[gHudCombo->side][0] = m[3][0];
        gHudCombo->textPos[gHudCombo->side][1] = m[3][1];
    }
}

/* Selects the side the next update / draw of the tree shows. */
#ifdef PORT
/* Widescreen on PC: tells the renderer where the 2D pieces that follow belong (port/src/gs_marker.c). */
#define PORT_2D_LEFT 0x10
#define PORT_2D_RIGHT 0x11
#define PORT_2D_CENTER 0x12
#define PORT_2D_END 0x13
extern void Port_GsMarker(s32 effect);
#endif

void HudCombo_SelectSide(s32 side) {
    gHudCombo->side = side;
#ifdef PORT
    Port_GsMarker(HUD_SCR(side) ? PORT_2D_RIGHT : PORT_2D_LEFT);
#endif
}

/* Starts message picture `message` (0..4) on a side. */
void HudCombo_SetMessage(s32 side, s32 message) {
    gHudCombo->msgState[side] = 0;
    gHudCombo->msg[side] = message;
}

/* Starts the text line on a side: the name of entry `text` of the fighter's skill script. */
void HudCombo_SetText(s32 side, s32 text) {
    gHudCombo->textState[side] = 0;
    gHudCombo->text[side] = text;
}

/* Gives the side's combo damage; negative: slide the number out. The number counts from where the last count
   stands (from 0 when `isNew` and the hit count did not grow, i.e. a new combo) to the new value in 0.6 s, or in
   difference / 9500 s when the difference exceeds 6000, at most 1.5 s. */
void HudCombo_SetDamage(s32 side, s32 damage, s32 isNew) {
    HudCombo *w; /* the original reads gHudCombo once in front of the test and again (as written here) after it */
    f32 seconds;

    if (damage > HUD_COMBO_DAMAGE_MAX) {
        damage = HUD_COMBO_DAMAGE_MAX;
    }
    w = gHudCombo;
    if (damage < 0) {
        if ((u32)(w->dmgState[side] - HUD_COMBO_ST_OUT) < 4) {
            return;
        }
        w->dmgState[side] = HUD_COMBO_ST_OUT;
    } else {
        seconds = 0.6f;
        if (isNew && gHudCombo->hitsRaw[side] <= gHudCombo->hitsPrev[side]) {
            gHudCombo->damageFrom[side] = 0;
        } else if (gHudCombo->countRamp[side].value != gHudCombo->countRamp[side].target) {
            gHudCombo->damageFrom[side] += (s32)(gHudCombo->countRamp[side].value *
                                                 (f32)(gHudCombo->damage[side] - gHudCombo->damageFrom[side]));
        } else {
            gHudCombo->damageFrom[side] = gHudCombo->damage[side];
        }
        gHudCombo->damage[side] = damage;
        if (gHudCombo->damage[side] - gHudCombo->damageFrom[side] > 6000) {
            seconds = (f32)(gHudCombo->damage[side] - gHudCombo->damageFrom[side]) / 9500.0f;
        }
        if (seconds > 1.5f) {
            seconds = 1.5f;
        }
        Ramp_Start(&gHudCombo->countRamp[side], seconds, 0.0f, 1.0f);
        if ((u32)gHudCombo->dmgState[side] < 6) {
            gHudCombo->dmgState[side] = HUD_COMBO_ST_POP;
        } else {
            gHudCombo->dmgState[side] = HUD_COMBO_ST_IN;
        }
    }
    HudSprite_Show(&gHudCombo->sprites[HUD_COMBO_SPR_DAMAGE], 1);
}

/* Gives the side's hit count. Below 2 the counter slides out; a count lower than the one on show slides it in
   anew, a higher one pops the digits. */
void HudCombo_SetHits(s32 side, u32 hits) {
    gHudCombo->hitsRaw[side] = hits;
    if (hits > HUD_COMBO_HITS_MAX) {
        hits = HUD_COMBO_HITS_MAX;
    }
    if (hits < 2) {
        if ((u32)(gHudCombo->hitState[side] - HUD_COMBO_ST_OUT) < 4) {
            return;
        }
        gHudCombo->hitState[side] = HUD_COMBO_ST_OUT;
    } else {
        if (hits < gHudCombo->hits[side]) {
            gHudCombo->hitState[side] = HUD_COMBO_ST_IN;
        } else if ((u32)gHudCombo->hitState[side] < 6) {
            gHudCombo->hitState[side] = HUD_COMBO_ST_POP;
        } else {
            gHudCombo->hitState[side] = HUD_COMBO_ST_IN;
        }
        gHudCombo->hitsPrev[side] = gHudCombo->hits[side];
        gHudCombo->hits[side] = hits;
    }
    HudSprite_Show(&gHudCombo->sprites[HUD_COMBO_SPR_HITS], 1);
}

/* Moves the part off the screen over `seconds`. */
void HudCombo_SlideOut(f32 seconds) {
    Ramp_Start(&gHudCombo->hide, seconds, 0.0f, 1.0f);
}

/* Brings it back over `seconds`. */
void HudCombo_SlideIn(f32 seconds) {
    Ramp_Start(&gHudCombo->hide, seconds, 1.0f, 0.0f);
}

/* Draws both sides' text lines: the name of entry text[side] of the side's skill script at the position
   HudCombo_StoreTextPos noted, aligned by side. Called by Hud_Draw after the combo trees. */
void HudCombo_DrawText(void) {
    s32 side;

    for (side = 0; side < 2; side++) {
        s32 *pos = gHudCombo->textPos[side];
        f32 alpha = gHudCombo->textAlpha[side];

        if (pos[0] >= 0 && pos[1] >= 0) {
#ifdef PORT
            /* Widescreen: the text belongs to its side's panel. Both lines are drawn here, after both combo
               trees, so the marker was still side 2's: player 1's technique name was narrowed about the right
               edge and stood where it stands in 4:3, away from its panel (seen by the user). */
            Port_GsMarker(HUD_SCR(side) ? PORT_2D_RIGHT : PORT_2D_LEFT);
#endif
            BtlMenu_SetScript2(BtlCtrl_GetObj(side)->skillScript);
            BtlText_DrawEntryName(pos[0], pos[1], gHudCombo->text[side], HUD_SCR(side), alpha); /* (the 4th: left or right aligned) */
        }
    }
}

/* Battle start: allocates the work, 12 sprites and 6 nodes and builds the tree; returns the root through `out`. */
void HudCombo_Init(HudCNode **out, HudCRes *res) {
    s32 i;
    HudCSprite *spr;
    HudCNode *node;

    gHudCombo = Heap_Alloc(sizeof(HudCombo), 0x20, 0, 2);
    memset(gHudCombo, 0, sizeof(HudCombo));
    gHudCombo->sprites = Heap_Alloc(HUD_COMBO_SPR_COUNT * sizeof(HudCSprite), 0x20, 0, 2);
    memset(gHudCombo->sprites, 0, HUD_COMBO_SPR_COUNT * sizeof(HudCSprite));
    gHudCombo->nodes = Heap_Alloc(HUD_COMBO_NODE_COUNT * sizeof(HudCNode), 0x20, 0, 2);
    memset(gHudCombo->nodes, 0, HUD_COMBO_NODE_COUNT * sizeof(HudCNode));
    for (i = 0; i < 2; i++) {
        gHudCombo->msg[i] = -1;
        gHudCombo->msgState[i] = -1;
        gHudCombo->textState[i] = -1;
        gHudCombo->dmgState[i] = HUD_COMBO_ST_OFF;
        gHudCombo->hitState[i] = HUD_COMBO_ST_OFF;
    }
    gHudCombo->res = res;

    spr = &gHudCombo->sprites[0];
    HudSprite_InitTex(spr, res, 2, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[1];
    HudSprite_InitPlain(spr, 0x80, 0x80, 0x80, 0x80);
    HudSprite_SetRect(spr, 0, 128, 0, 32);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[2];
    HudSprite_InitTex(spr, res, 2, 0);
    HudSprite_SetRect(spr, 0, 128, 0, 32);
    HudSprite_SetUv(spr, 0, 128, 32, 64);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[3];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[4];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[5];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[6];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[7];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[8];
    HudSprite_InitTex(spr, res, 2, 0);
    HudSprite_SetRect(spr, 0, 128, -30, 2);
    HudSprite_SetUv(spr, 0, 128, 2, 32);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[9];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[10];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    spr = &gHudCombo->sprites[11];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Show(spr, 0);

    node = &gHudCombo->nodes[HUD_COMBO_NODE_MESSAGE];
    HudNode_SetPos(node, 0, 16);
    node->spriteCount = 1;
    node->sprites = Heap_Alloc(node->spriteCount * sizeof(HudCSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudCSprite *));
    node->sprites[0] = &gHudCombo->sprites[0];
    node->update = HudCombo_UpdateMessage;
    node->draw = HudCombo_DrawSprites;

    node = &gHudCombo->nodes[HUD_COMBO_NODE_TEXT];
    HudNode_SetPos(node, 0, 4);
    node->spriteCount = 1;
    node->sprites = Heap_Alloc(node->spriteCount * sizeof(HudCSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudCSprite *));
    node->sprites[0] = &gHudCombo->sprites[1];
    node->update = HudCombo_UpdateText;
    node->draw = HudCombo_StoreTextPos;

    node = &gHudCombo->nodes[HUD_COMBO_NODE_DAMAGE];
    HudNode_SetPos(node, 0, 80);
    node->spriteCount = 6;
    node->sprites = Heap_Alloc(node->spriteCount * sizeof(HudCSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudCSprite *));
    node->sprites[0] = &gHudCombo->sprites[2];
    node->sprites[1] = &gHudCombo->sprites[3];
    node->sprites[2] = &gHudCombo->sprites[4];
    node->sprites[3] = &gHudCombo->sprites[5];
    node->sprites[4] = &gHudCombo->sprites[6];
    node->sprites[5] = &gHudCombo->sprites[7];
    node->update = HudCombo_UpdateDamage;
    node->draw = HudCombo_DrawSprites;
    HudNode_Show(node, 0);

    node = &gHudCombo->nodes[HUD_COMBO_NODE_HITS];
    HudNode_SetPos(node, 0, 80);
    node->spriteCount = 4;
    node->sprites = Heap_Alloc(node->spriteCount * sizeof(HudCSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudCSprite *));
    node->sprites[0] = &gHudCombo->sprites[8];
    node->sprites[1] = &gHudCombo->sprites[9];
    node->sprites[2] = &gHudCombo->sprites[10];
    node->sprites[3] = &gHudCombo->sprites[11];
    node->update = HudCombo_UpdateHits;
    node->draw = HudCombo_DrawSprites;
    HudNode_Show(node, 0);

    node = &gHudCombo->nodes[1];
    HudNode_SetPos(node, 0, 85);
    node->spriteCount = 0;
    node->childCount = 4;
    node->update = NULL;
    node->draw = NULL;
    node->children = Heap_Alloc(node->childCount * sizeof(HudCNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudCNode *));
    node->children[0] = &gHudCombo->nodes[HUD_COMBO_NODE_MESSAGE];
    node->children[1] = &gHudCombo->nodes[HUD_COMBO_NODE_TEXT];
    node->children[2] = &gHudCombo->nodes[HUD_COMBO_NODE_DAMAGE];
    node->children[3] = &gHudCombo->nodes[HUD_COMBO_NODE_HITS];

    node = &gHudCombo->nodes[0];
    HudNode_SetPos(node, 0, 0);
    node->spriteCount = 0;
    node->childCount = 1;
    node->update = HudCombo_UpdateRoot;
    node->draw = NULL;
    node->children = Heap_Alloc(node->childCount * sizeof(HudCNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudCNode *));
    node->children[0] = &gHudCombo->nodes[1];

    *out = gHudCombo->nodes;
}

/* Battle end: frees the sprites, every node's sprite and child lists, the nodes and the work. */
void HudCombo_Term(void) {
    s32 i;

    if (gHudCombo->sprites != NULL) {
        Heap_Free(gHudCombo->sprites);
    }
    for (i = 0; i < HUD_COMBO_NODE_COUNT; i++) {
        if (gHudCombo->nodes[i].sprites != NULL) {
            Heap_Free(gHudCombo->nodes[i].sprites);
        }
        if (gHudCombo->nodes[i].children != NULL) {
            Heap_Free(gHudCombo->nodes[i].children);
        }
    }
    if (gHudCombo->nodes != NULL) {
        Heap_Free(gHudCombo->nodes);
    }
    if (gHudCombo != NULL) {
        Heap_Free(gHudCombo);
    }
}

/* Round reset: everything off, values and ramps zeroed. */
void HudCombo_Reset(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gHudCombo->msgState[i] = -1;
        gHudCombo->dmgState[i] = HUD_COMBO_ST_OFF;
        gHudCombo->hitState[i] = HUD_COMBO_ST_OFF;
        gHudCombo->textState[i] = -1;
        gHudCombo->msg[i] = -1;
        gHudCombo->damage[i] = 0;
        gHudCombo->damageFrom[i] = 0;
        gHudCombo->hits[i] = 0;
        gHudCombo->hitsRaw[i] = 0;
        gHudCombo->hitsPrev[i] = 0;
        gHudCombo->text[i] = 0;
        memset(&gHudCombo->msgRamp[i], 0, sizeof(Ramp));
        memset(&gHudCombo->textRamp[i], 0, sizeof(Ramp));
        memset(&gHudCombo->dmgRamp[i], 0, sizeof(Ramp));
        memset(&gHudCombo->hitRamp[i], 0, sizeof(Ramp));
        memset(&gHudCombo->countRamp[i], 0, sizeof(Ramp));
    }
    memset(&gHudCombo->hide, 0, sizeof(Ramp));
}
