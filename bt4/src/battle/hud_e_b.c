#include "common.h"
#include "battle/hud_e.h"
#include "battle/battle.h"
#include "battle/btl_seq.h"
#include "sys/heap.h"
#include "sys/pad.h"

/*
 * Battle HUD, prompt part, 0x22B4F8-0x22EC08. Three things, each a node under the part's root:
 *   - the button prompt (node 24, sprites 0..3): one animated button icon, or the animated picture of kind 12,
 *     with a pulsing base and a glow, shown per side (HudPrompt_SetButton / HudPrompt_ClearButton);
 *   - the cue (node 25, sprites 4, 5; side 0 only): a base with the icon of the pad type of pad 0, shown by the
 *     battle script while an event action is pending (HudPrompt_ShowCue / HideCue / SetCueDim);
 *   - the command row (node 26, sprites 6..21): up to four icons side by side, with the name of the technique
 *     drawn as text above (HudPrompt_SetCommand / ClearCommand / AcceptCommand, HudPrompt_DrawNames).
 * One tree serves both sides: Hud_Draw selects side 0, updates and draws, then side 1 (mirrored, split screen
 * only). Every animation is frozen by the pause flag.
 *
 * Reads the pad: HudPrompt_UpdateCue reads gPad[side].status / lastStatus (the controller type) to choose the
 * icon; nothing else. No random numbers.
 */

#define HUDP_PAUSED() (Battle_GetWork()->flags & BATTLE_FLAG_PAUSE)
/* namePos is always reached through a pointer (the original addresses it as (work + side * 8) + 0x1E8, which a
   plain member access does not produce). */
#define HUDP_NAME_POS(side) (&gHudPrompt->namePos[side])
#define HUDP_MIN(a, b) ((a) < (b) ? (a) : (b))
#define HUDP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define HUDP_MAX2(a, b) ((a) < (b) ? (b) : (a))

HudPrompt *gHudPrompt = NULL;

extern HudPromptIconDef gHudPromptIconDefs[16];
extern HudPromptIconDef gHudPromptIconDefs16[17];
extern HudPromptIconDef gHudPromptCueDefs[4];

/* The side's fighter object: only what HudPrompt_DrawNames reads. */
typedef struct HudPromptObj {
    /* 0x00 */ u8 unk0[0xBC];
    /* 0xBC */ s32 unkBC;        /* handed to BtlMenu_SetScript2 before the name is drawn */
} HudPromptObj;

/* Sprite / node library at 0x224B50.. (neighbouring ranges). */
extern void HudSprite_Show(HudESprite *spr, s32 show);
#ifdef PORT
/* PC build, online play: the player who joined is side 1 of the game, and would have their own gauges, team, prompts
   and counters on the right. With the swap (gs/net.c) the two sides change places on THIS copy's screen only: a
   side's data is the same, where it is drawn is the other side's place. HUD_SCR(side) is the place of a side. */
extern int Port_HudSwap(void);
#define HUD_SCR(side) ((side) ^ Port_HudSwap())
#else
#define HUD_SCR(side) (side)
#endif
extern void HudSprite_SetMirror(HudESprite *spr, s32 mirror);
extern void HudSprite_SetRect(HudESprite *spr, s32 x0, s32 x1, s32 y0, s32 y1); /* screen rectangle */
extern void HudSprite_SetUv(HudESprite *spr, s32 u0, s32 u1, s32 v0, s32 v1); /* texel rectangle */
extern void HudSprite_SetTex(HudESprite *spr, s32 tex, s32 sub);               /* texture entry and sub entry */
extern void HudSprite_SetColor(HudESprite *spr, s32 r, s32 g, s32 b, s32 a);
extern void HudSprite_Move(HudESprite *spr, s32 dx, s32 dy);                 /* moves the rectangle */
extern void HudSprite_Scale(HudESprite *spr, f32 sx, f32 sy);                 /* scales the rectangle */
extern void HudSprite_Center(HudESprite *spr);                                 /* centres the rectangle */
extern void HudSprite_InitTex(HudESprite *spr, HudERes *res, s32 tex, s32 sub); /* sprite of a sheet's texture */
extern void HudSprite_DrawAt(HudESprite *spr, HudERes *res, s32 additive, s32 a, s32 b); /* draw with two GS values */
extern void HudSprite_Draw(HudESprite *spr, HudERes *res, s32 additive);     /* HudSprite_Draw */
extern void HudNode_Show(HudENode *node, s32 show);                        /* HudNode_Show */
extern void HudNode_SetPos(HudENode *node, s32 x, s32 y);                    /* HudNode_SetPos */
extern void BtlMenu_SetScript2(s32 arg);
extern HudPromptObj *BtlCtrl_GetObj(s32 side);
extern HudERes *FontIcon_GetRes(void);
extern HudPromptIconDef *FontIcon_GetDefs(void);
extern void *memset(void *dst, s32 c, u32 n);

/* Root node update: steps the slide once a frame (on the side 0 pass) and moves the part to (-256, 224) * slide. */
void HudPrompt_UpdateRoot(HudENode *node) {
    if (!HUDP_PAUSED() && gHudPrompt->side == 0) {
        Ramp_Step(&gHudPrompt->slide);
    }
    HudNode_SetPos(node, gHudPrompt->slide.value * -256.0f, gHudPrompt->slide.value * 224.0f);
}

/* Clears the mark of one texture entry of the icon sheet. */
void HudPrompt_ClearIconMark(s32 tex) {
    HudETex *t = &gHudPrompt->iconRes->tex[tex];

    t->mark = 0;
}

/* Clears the mark of every texture entry of the icon sheet. */
void HudPrompt_ClearIconMarks(void) {
    s32 n = gHudPrompt->iconRes->texCount;
    s32 i;

    for (i = 0; i < n; i++) {
        HudETex *t = &gHudPrompt->iconRes->tex[i];

        t->mark = 0;
    }
}

/* Rectangles of animation frame f: cells of def->cell, two per row; the picture is the left part of the cell. */
#define HUDP_ICON_RECTS(f) \
    { \
        s32 w = def->width + 1; \
        if (w >= def->cell) { \
            w = def->cell; \
        } \
        pos[0] = ((f) % 2) * def->cell; \
        pos[1] = ((f) % 2) * def->cell + (s32)(w * scale); \
        pos[2] = ((f) / 2) * def->cell; \
        pos[3] = ((f) / 2) * def->cell + def->cell; \
        uv[0] = ((f) % 2) * def->cell; \
        uv[1] = ((f) % 2) * def->cell + w; \
        uv[2] = ((f) / 2) * def->cell; \
        uv[3] = ((f) / 2) * def->cell + def->cell; \
    }

/* Advances the animation of an icon and applies it to a sprite.
   icon->mode picks the frames [lo, hi) that play: 0 = the frames before def->pressed, 1 = from def->pressed to the
   end, 2 (and any other value) = all, 3 = frame 0 only, 4 = none. A frame lasts def->time[frame] calls' worth of
   icon->timer (the caller counts it up); a time of 0, frame 6 or hi ends the run: frame becomes -1 and the last
   frame stays on show. From there the animation restarts at lo when icon->restart is set, else once the timer
   reaches 20. */
void HudPrompt_ApplyIconDef(HudESprite *spr, HudPromptIconDef *def, HudPromptIcon *icon) {
    s32 pos[4];
    s32 uv[4];
    f32 scale = 1.0f;
    s32 lo;
    s32 hi;
    s32 frame;

    switch (icon->mode) {
        case 0:
            lo = 0;
            hi = HUDP_MIN(def->pressed, def->frames);
            break;
        case 4:
            lo = HUDP_MIN(def->pressed, def->frames);
            hi = HUDP_MIN(def->pressed, def->frames);
            break;
        case 1:
            lo = HUDP_MIN(def->pressed, def->frames);
            hi = def->frames + 1;
            break;
        case 3:
            lo = 0;
            hi = 0;
            break;
        case 2:
        default:
            lo = 0;
            hi = def->frames + 1;
            break;
    }
    frame = icon->frame;
    if (frame >= 0 && frame < lo) {
        icon->frame = lo;
        frame = lo;
        icon->timer = 0;
    }
    if (icon->restart) {
        if (frame < 0) {
            icon->frame = lo;
            frame = lo;
            icon->timer = 0;
        }
    }
    if (frame < 0) {
        if (icon->timer >= 20) {
            icon->frame = lo;
            frame = lo;
            icon->timer = 0;
        }
    } else {
        s8 *time = def->time;

        if (icon->timer >= time[frame]) {
            s32 next = frame + 1;

            icon->last = frame;
            icon->frame = next;
            icon->timer = 0;
            if (next >= 6 || next >= hi || time[next] == 0) {
                icon->frame = -1;
                frame = -1;
            } else {
                frame = next;
            }
        }    }
    if (frame < 0) {
        HUDP_ICON_RECTS(icon->last);
    } else {
        HUDP_ICON_RECTS(frame);
    }
    HudSprite_SetTex(spr, def->tex, 0);
    HudSprite_SetRect(spr, pos[0], pos[1], pos[2], pos[3]);
    HudSprite_Center(spr);
    HudSprite_SetUv(spr, uv[0], uv[1], uv[2], uv[3]);
}

/* The same, looking the definition up by the icon number: 0..15 and 16..32 in the two tables of this part, above
   that in the icon sheet's own table. */
void HudPrompt_ApplyIcon(HudESprite *spr, HudPromptIcon *icon) {
    if (icon->icon < 16) {
        HudPrompt_ApplyIconDef(spr, &gHudPromptIconDefs[icon->icon], icon);
    } else if (icon->icon < 33) {
        HudPrompt_ApplyIconDef(spr, &gHudPromptIconDefs16[icon->icon - 16], icon);
    } else {
        HudPrompt_ApplyIconDef(spr, &gHudPrompt->iconDefs[icon->icon], icon);
    }
}

/* Ramp_Step unless the battle is paused; 0 while paused. */
static inline s32 HudPrompt_StepRamp(Ramp *ramp) {
    s32 r;

    if (!HUDP_PAUSED()) {
        r = Ramp_Step(ramp);
    } else {
        r = 0;
    }
    return r;
}

/* The button's icon: tbl row = group, column = picture; the sheet's definition gives the icon number. */
#define HUDP_BTN_SET_ICON() \
    gHudPrompt->btn.icon[side].icon = gHudPrompt->iconDefs[tbl[group * 4 + gHudPrompt->btn.value[side]]].tex

/* Update of node 24: the button prompt of the selected side. Sprites: 0 base, 1 icon, 2 second picture, 3 glow.
   btnState:
     1 -> 2 -> 3      button, animation mode 2 (press and release in a loop); 0.28 s fade-in, then shown
     4 -> 5 -> 6      button held (frame 1)
     7 -> 8 -> 9      button released (frame 0) with the glow dropping onto it every 1.5 s
     10 -> 11 -> 12.. kind 12: an animated picture, textures value .. value + 4 and back, 0.03 s a frame
                      (12..19 count up, 20..27 count down mirrored, then 12 again)
     40 -> 41 -> 42   0.28 s fade-out (HudPrompt_ClearButton)
     anything else    hidden, kind = 13, state = 0 */
void HudPrompt_UpdateButton(HudENode *node) {
    f32 alpha = 1.0f;
    s32 side = gHudPrompt->side;
    HudESprite *base = &gHudPrompt->spr[0];
    s32 tbl[16] = { 22, 21, 20, 23, 54, 55, 69, 70, 37, 43, 41, 39, 40, 44, 38, 42 };
    HudESprite *icon2;
    Ramp *pulse;
    Ramp *blink;
    s32 *state;
    Ramp *ramp;
    s32 group;
    HudESprite *glow;
    HudESprite *icon;
    f32 t;

    state = &gHudPrompt->btnState[side];
    pulse = &gHudPrompt->btnPulse[side];
    ramp = &gHudPrompt->btnRamp[side];
    blink = &gHudPrompt->btnBlink[side];
    group = gHudPrompt->btnGroup[side];

    HudSprite_InitTex(base, gHudPrompt->res, 0, 0);
    HudSprite_Center(base);
    glow = &base[3];
    icon = &base[1];
    icon2 = &base[2];
    HudSprite_InitTex(glow, gHudPrompt->res, 7, 0);
    HudSprite_Center(glow);
    HudSprite_Move(glow, 0, -24);
    HudNode_Show(node, 1);
    HudSprite_Show(icon, 1);
    HudSprite_Show(icon2, 0);
    HudSprite_Show(base, 1);
    HudSprite_Show(glow, 1);
    switch (*state) {
        case 1:
            HudSprite_Show(glow, 0);
            Ramp_Start(ramp, 0.28f, 0.0f, 1.0f);
            Ramp_Start(pulse, 0.1f, 1.0f, 0.0f);
            Ramp_Start(blink, 1.5f, 1.0f, 0.0f);
            *state = 2;
            HUDP_BTN_SET_ICON();
            gHudPrompt->btn.icon[side].timer = 0;
            gHudPrompt->btn.icon[side].restart = 1;
            gHudPrompt->btn.icon[side].mode = 2;
            /* fall through */
        case 2:
            HUDP_BTN_SET_ICON();
            HudSprite_Show(glow, 0);
            if (HudPrompt_StepRamp(ramp)) {
                *state = 3;
            }
            alpha = ramp->value;
            break;
        case 3:
            HUDP_BTN_SET_ICON();
            HudSprite_Show(glow, 0);
            if (!HUDP_PAUSED()) {
                gHudPrompt->btn.icon[side].timer++;
            }
            break;
        case 4:
            Ramp_Start(ramp, 0.28f, 0.0f, 1.0f);
            Ramp_Start(pulse, 0.1f, 1.0f, 0.0f);
            Ramp_Start(blink, 1.5f, 1.0f, 0.0f);
            *state = 5;
            HUDP_BTN_SET_ICON();
            gHudPrompt->btn.icon[side].timer = 0;
            gHudPrompt->btn.icon[side].frame = 1;
            gHudPrompt->btn.icon[side].restart = 1;
            /* fall through */
        case 5:
            HUDP_BTN_SET_ICON();
            if (HudPrompt_StepRamp(ramp)) {
                *state = 6;
            }
            alpha = ramp->value;
            break;
        case 6:
            HUDP_BTN_SET_ICON();
            break;
        case 7:
            Ramp_Start(ramp, 0.28f, 0.0f, 1.0f);
            Ramp_Start(pulse, 0.1f, 1.0f, 0.0f);
            Ramp_Start(blink, 1.5f, 1.0f, 0.0f);
            *state = 8;
            HUDP_BTN_SET_ICON();
            gHudPrompt->btn.icon[side].timer = 0;
            gHudPrompt->btn.icon[side].frame = 0;
            gHudPrompt->btn.icon[side].restart = 1;
            /* fall through */
        case 8:
            HUDP_BTN_SET_ICON();
            if (HudPrompt_StepRamp(ramp)) {
                *state = 9;
            }
            alpha = ramp->value;
            break;
        case 9:
            HUDP_BTN_SET_ICON();
            if (!HUDP_PAUSED()) {
                if (Ramp_Step(blink)) {
                    Ramp_Start(blink, 1.5f, 1.0f, 0.0f);
                }
            }
            {
                f32 drop = blink->value * 2.6f - 0.8f;
                f32 d;

                if (0.9f < drop) {
                    gHudPrompt->btn.icon[side].frame = 1;
                } else {
                    gHudPrompt->btn.icon[side].frame = 0;
                }
                drop = HUDP_MIN(drop, 1.0f);
                d = (0.0f < drop) ? drop : 0.0f;
                HudSprite_Move(glow, 0, (1.0f - d) * -16.0f);
            }
            break;
        case 10:
            HudSprite_Show(glow, 0);
            Ramp_Start(ramp, 0.28f, 0.0f, 1.0f);
            Ramp_Start(pulse, 0.1f, 1.0f, 0.0f);
            icon->tex = gHudPrompt->btn.value[side];
            *state = 11;
            /* fall through */
        case 11:
            HudSprite_Show(glow, 0);
            if (HudPrompt_StepRamp(ramp)) {
                gHudPrompt->btn.value[side]++;
                icon->tex = gHudPrompt->btn.value[side];
                *state = 12;
            }
            alpha = ramp->value;
            break;
        case 12:
        case 14:
        case 16:
        case 18:
            Ramp_Start(ramp, 0.03f, 0.0f, 1.0f);
            (*state)++;
            /* fall through */
        case 13:
        case 15:
        case 17:
        case 19:
            HudSprite_Show(glow, 0);
            if (HudPrompt_StepRamp(ramp)) {
                gHudPrompt->btn.value[side]++;
                icon->tex = gHudPrompt->btn.value[side];
                (*state)++;
                if (*state == 20) {
                    gHudPrompt->btn.flip[side] = 1;
                }
            }
            break;
        case 20:
        case 22:
        case 24:
        case 26:
            Ramp_Start(ramp, 0.03f, 0.0f, 1.0f);
            (*state)++;
            /* fall through */
        case 21:
        case 23:
        case 25:
        case 27:
            HudSprite_Show(glow, 0);
            if (HudPrompt_StepRamp(ramp)) {
                gHudPrompt->btn.value[side]--;
                icon->tex = gHudPrompt->btn.value[side];
                (*state)++;
                if (*state == 28) {
                    *state = 12;
                    gHudPrompt->btn.flip[side] = 0;
                }
            }
            break;
        case 40:
            Ramp_Start(ramp, 0.28f, 1.0f, 0.0f);
            (*state)++;
            /* fall through */
        case 41:
            HudSprite_Show(glow, 0);
            if (HudPrompt_StepRamp(ramp)) {
                *state = 42;
            }
            alpha = ramp->value;
            break;
        case 42:
            HudNode_Show(node, 0);
            HudSprite_Show(icon, 0);
            HudSprite_Show(icon2, 0);
            HudSprite_Show(base, 0);
            HudSprite_Show(glow, 0);
            gHudPrompt->btn.kind[side] = 13;
            *state = 0;
            break;
        default:
            /* The same block again: the two copies are merged only after register allocation, and the
               allocation matches the original only with both. */
            HudNode_Show(node, 0);
            HudSprite_Show(icon, 0);
            HudSprite_Show(icon2, 0);
            HudSprite_Show(base, 0);
            HudSprite_Show(glow, 0);
            gHudPrompt->btn.kind[side] = 13;
            *state = 0;
            break;
    }
    if (gHudPrompt->btn.kind[side] != 12) {
        HudPrompt_ApplyIconDef(icon, &gHudPrompt->iconDefs[gHudPrompt->btn.icon[side].icon], &gHudPrompt->btn.icon[side]);
    } else if (group != 1) {
        HudSprite_InitTex(icon, gHudPrompt->res, gHudPrompt->btn.value[side], 0);
        HudSprite_Center(icon);
    } else {
        HudSprite_InitTex(icon, gHudPrompt->res, gHudPrompt->btn.value[side] >> 8, 0);
        HudSprite_Center(icon);
        HudSprite_Move(icon, HUD_SCR(gHudPrompt->side) == 0 ? -14 : 14, 0);
        HudSprite_Show(icon, 1);
        HudSprite_InitTex(icon2, gHudPrompt->res, (u8)gHudPrompt->btn.value[side], 0);
        HudSprite_Center(icon2);
        HudSprite_Move(icon2, HUD_SCR(gHudPrompt->side) ? -12 : 12, 0);
        HudSprite_Show(icon2, 1);
    }
    if (HudPrompt_StepRamp(pulse)) {
        if (pulse->value == 1.0f) {
            Ramp_Start(pulse, 0.1f, 1.0f, 0.0f);
        } else if (pulse->value == 0.0f) {
            Ramp_Start(pulse, 0.1f, 0.0f, 1.0f);
        }
    }
    t = pulse->value * 0.4f + 1.2f;
    if (gHudPrompt->btn.kind[side] == 12) {
        t *= 1.2f;
    }
    HudSprite_Scale(base, t, t);
    t = (gHudPrompt->btn.kind[side] != 12) ? 1.44f : 1.0f;
    HudSprite_Scale(icon, t, t);
    HudSprite_SetColor(icon, 0x80, 0x80, 0x80, (u8)(alpha * 128.0f));
    HudSprite_SetColor(icon2, 0x80, 0x80, 0x80, (u8)(alpha * 128.0f));
    HudSprite_SetColor(base, 0x80, 0x80, 0x80, (u8)(alpha * 128.0f));
    {
        f32 drop = blink->value * 2.6f - 0.8f;
        f32 a;

        if (drop < 1.0f) {
            alpha *= drop;
        }
        a = alpha * HUDP_MAX2(0.0f, alpha);
        HudSprite_SetColor(glow, 0x80, 0x80, 0x80, (u8)(a * 128.0f));
    }
    HudSprite_SetMirror(icon, gHudPrompt->btn.flip[side] ^ HUD_SCR(side));
    HudSprite_SetMirror(icon2, gHudPrompt->btn.flip[side] ^ HUD_SCR(side));
    HudSprite_Move(base, 1, -2);
}

/* Update of node 25: the cue of side 0. Sprite 4 is the base (texture 8), sprite 5 the icon of pad 0's controller
   type (gHudPromptCueDefs[gPad[0].status], or lastStatus when the status is 4 or more). cueState: 0 start,
   1 fading in (0.28 s), 2 shown, 3 fading out, anything else hidden (and set to 4). The base pulses between
   1.44 and 1.92 times its size every 0.1 s; with cueDim it stays at 1.2 and the alpha is halved. */
/* INCLUDE_ASM: the C below is the same code except for the registers of four temporaries (14 instructions): the
   original keeps the sprite array pointer in $v1, side * 0x1C0 in $v0, the pad pointer in $a0 and the type in
   $v1; this compiles to $v0, $v1, $v1, $a0. It is a local-allocation order question (which of the two block-local
   values is given $v0 first); no order of the declarations changes it (checked over 400 orders), and computing
   the sprite pointers after the type test gives the original registers but moves them behind the branch. */
#if 0
void HudPrompt_UpdateCue(void) {
    Ramp *ramp = &gHudPrompt->cueRamp;
    s32 *state = &gHudPrompt->cueState;
    f32 alpha = 1.0f;
    s32 side = gHudPrompt->side;
    HudESprite *icon = &gHudPrompt->spr[5];
    HudESprite *base = &gHudPrompt->spr[4];
    s32 *status = &gPad[side].status;
    s32 type = status[0];
    Ramp *pulse = &gHudPrompt->cuePulse;
    HudPromptIconDef *def;
    f32 s;

    if (type >= 4) {
        type = status[1]; /* lastStatus */
    }
    if (side != 0) {
        return;
    }
    def = &gHudPromptCueDefs[type];
    HudSprite_InitTex(icon, gHudPrompt->iconRes, def->tex, 0);
    HudSprite_SetRect(icon, 0, HUDP_MIN(def->width + 1, def->cell), 0, def->cell);
    HudSprite_SetUv(icon, 0, HUDP_MIN(def->width + 1, def->cell), 0, def->cell);
    HudSprite_Center(icon);
    HudSprite_InitTex(base, gHudPrompt->res, 8, 0);
    HudSprite_Center(base);
    HudSprite_Show(icon, 1);
    HudSprite_Show(base, 1);
    switch (*state) {
        case 0:
            Ramp_Start(ramp, 0.28f, 0.0f, 1.0f);
            Ramp_Start(pulse, 0.1f, 1.0f, 0.0f);
            *state = 1;
            /* fall through */
        case 1:
            if (HudPrompt_StepRamp(ramp)) {
                *state = 2;
            }
            alpha = ramp->value;
            break;
        case 2:
            break;
        case 3:
            if (HudPrompt_StepRamp(ramp)) {
                *state = 4;
            }
            alpha = ramp->value;
            break;
        case 4:
        default:
            HudSprite_Show(icon, 0);
            HudSprite_Show(base, 0);
            *state = 4;
            break;
    }
    if (HudPrompt_StepRamp(pulse)) {
        if (pulse->value == 1.0f) {
            Ramp_Start(pulse, 0.1f, 1.0f, 0.0f);
        } else if (pulse->value == 0.0f) {
            Ramp_Start(pulse, 0.1f, 0.0f, 1.0f);
        }
    }
    if (gHudPrompt->cueDim) {
        s = 1.2f;
    } else {
        s = (pulse->value * 0.4f + 1.2f) * 1.2f;
    }
    HudSprite_Scale(base, s, s);
    HudSprite_Scale(icon, 1.2f, 1.2f);
    HudSprite_Move(icon, 2, 0);
    if (gHudPrompt->cueDim) {
        alpha *= 0.5f;
    }
    HudSprite_SetColor(icon, 0x80, 0x80, 0x80, (u8)(alpha * 128.0f));
    HudSprite_SetColor(base, 0x80, 0x80, 0x80, (u8)(alpha * 128.0f));
    HudSprite_Move(base, 1, -2);
}
#endif
LIT4_WORD(D_002FE384, 0x3E8F5C28); /* 0.28f */
LIT4_WORD(D_002FE388, 0x3DCCCCCC); /* 0.1f */
LIT4_WORD(D_002FE38C, 0x3DCCCCCC); /* 0.1f */
LIT4_WORD(D_002FE390, 0x3DCCCCCC); /* 0.1f */
LIT4_WORD(D_002FE394, 0x3F999999); /* 1.2f */
LIT4_WORD(D_002FE398, 0x3ECCCCCC); /* 0.4f */
LIT4_WORD(D_002FE39C, 0x3F999999); /* 1.2f */
LIT4_WORD(D_002FE3A0, 0x3F999999); /* 1.2f */
void HudPrompt_UpdateCue(void);
RODATA_ALIGN16(); /* its jump table is at 0x2F20E0; the table before it ends at 0x2F20D8 */
INCLUDE_ASM("asm/nonmatchings/battle/hud_e_b", HudPrompt_UpdateCue);

/* Draw of node 24: base, icon (from the icon sheet, or the part's own sheet for kind 12), second picture, glow. */
void HudPrompt_DrawButton(void) {
    HudESprite *icon;

    HudPrompt_ClearIconMarks();
    HudSprite_Draw(&gHudPrompt->spr[0], gHudPrompt->res, 0);
    icon = &gHudPrompt->spr[1];
    if (gHudPrompt->btn.kind[gHudPrompt->side] != 12) {
        HudSprite_DrawAt(icon, gHudPrompt->iconRes, 0, 0x2A80, 0x2C90);
    } else {
        HudSprite_Draw(icon, gHudPrompt->res, 0);
    }
    HudSprite_Draw(&gHudPrompt->spr[2], gHudPrompt->res, 0);
    HudSprite_Draw(&gHudPrompt->spr[3], gHudPrompt->res, 0);
}

/* Draw of node 25 (side 0 only): base and pad icon. */
void HudPrompt_DrawCue(void) {
    if (gHudPrompt->side == 0) {
        HudPrompt_ClearIconMarks();
        HudSprite_Draw(&gHudPrompt->spr[4], gHudPrompt->res, 0);
        HudSprite_DrawAt(&gHudPrompt->spr[5], gHudPrompt->iconRes, 0, 0x2A80, 0x2C90);
    }
}

/* Update of node 26: the command row of the selected side. Sprites 6..9 are the icons, 10..13 their glows,
   14..17 / 18..21 the enlarged copies drawn when the command is accepted. cmd.state:
     0 -> 1 -> 2  fade in over 0.1 s (the name fades in too), then shown; the glow drops every 1.5 s
     3 -> 4       fade out over 0.3 s (HudPrompt_ClearCommand)
     5 -> 6 -> 7  accepted (HudPrompt_AcceptCommand): 0.8 s with the copies growing and fading, then 0.5 s fade out
     9            off: state = -1 and everything hidden (also for any other value)
   The icons are laid out left to right by their widths; on side 1 the row is moved so that it ends at
   512 - 2 * node->x. The name's place (namePos) is taken from the first icon unless its mode is 3. */
void HudPrompt_UpdateCommand(HudENode *node) {
    s32 side = gHudPrompt->side;
    HudESprite *spr = gHudPrompt->spr;
    HudPromptIcon *icons;
    Ramp *ramp;
    Ramp *name;
    Ramp *blink;
    HudESprite *ghostGlow;
    HudESprite *ghost;
    s32 x = 0;
    s32 *state;
    s32 count;
    HudESprite *glow;
    HudESprite *icon;
    f32 drop;
    s32 i;

    icons = gHudPrompt->row.icon[side];
    ramp = &gHudPrompt->cmdRamp[side];
    name = &gHudPrompt->nameFade[side];
    blink = &gHudPrompt->cmdBlink[side];
    state = &gHudPrompt->cmd.state[side];
    glow = &spr[10];
    count = gHudPrompt->row.count[side];
    ghostGlow = &spr[18];
    icon = &spr[6];
    ghost = &spr[14];

    for (i = 0; i < 4; i++) {
        HudSprite_InitTex(&glow[i], gHudPrompt->res, 7, 0);
        HudSprite_Center(&glow[i]);
        HudSprite_Move(&glow[i], 0, -22);
    }
    for (i = 0; i < 4; i++) {
        if (i < count) {
            HudSprite_Show(&icon[i], 1);
            if (icons[i].icon < 16) {
                if ((u32)icons[i].mode < 2) {
                    HudSprite_Show(&glow[i], 1);
                } else {
                    HudSprite_Show(&glow[i], 0);
                }
            } else {
                HudSprite_Show(&glow[i], 0);
            }
        } else {
            HudSprite_Show(&icon[i], 0);
            HudSprite_Show(&glow[i], 0);
            HudSprite_Show(&ghost[i], 0);
            HudSprite_Show(&ghostGlow[i], 0);
        }
    }
    switch (*state) {
        case 0:
            for (i = 0; i < 4; i++) {
                if (i < count) {
                    icons[i].timer = 0;
                    icons[i].frame = 0;
                    HudSprite_Show(&icon[i], 1);
                    if (icons[i].icon < 16) {
                        if ((u32)icons[i].mode < 2) {
                            HudSprite_Show(&glow[i], 1);
                        } else {
                            HudSprite_Show(&glow[i], 0);
                        }
                    }
                } else {
                    HudSprite_Show(&icon[i], 0);
                    HudSprite_Show(&glow[i], 0);
                }
            }
            Ramp_Start(ramp, 0.1f, 0.0f, 1.0f);
            Ramp_Start(name, 0.1f, 0.0f, 1.0f);
            Ramp_Start(blink, 1.5f, 1.0f, 0.0f);
            (*state)++;
            /* fall through */
        case 1:
            if (!HUDP_PAUSED()) {
                if (Ramp_Step(ramp)) {
                    (*state)++;
                }
            }
            break;
        case 2:
            if (!HUDP_PAUSED()) {
                if (Ramp_Step(blink)) {
                    Ramp_Start(blink, 1.5f, 1.0f, 0.0f);
                }
            }
            if (!HUDP_PAUSED()) {
                for (i = 0; i < count; i++) {
                    if (icons[i].icon < 16) {
                        if (icons[i].mode == 1) {
                            continue;
                        }
                        if (icons[i].mode == 0) {
                            continue;
                        }
                    }
                    icons[i].timer++;
                }
            }
            break;
        case 3:
            Ramp_Start(ramp, 0.3f, 1.0f, 0.0f);
            (*state)++;
            /* The body of case 4 again: the original has cases 5 and 6 between this case and case 4. */
            if (!HUDP_PAUSED()) {
                if (Ramp_Step(ramp)) {
                    *state = 9;
                }
            }
            break;
        case 5:
            Ramp_Start(blink, 0.8f, 1.0f, 0.0f);
            (*state)++;
            /* fall through */
        case 6:
            if (!HUDP_PAUSED()) {
                if (Ramp_Step(blink)) {
                    Ramp_Start(ramp, 0.5f, 1.0f, 0.0f);
                    (*state)++;
                }
            }
            break;
        case 4:
        case 7:
            if (!HUDP_PAUSED()) {
                if (Ramp_Step(ramp)) {
                    *state = 9;
                }
            }
            break;
        case 9:
            *state = -1;
            /* fall through */
        case 8:
        default:
            for (i = 0; i < count; i++) {
                HudSprite_Show(&icon[i], 0);
            }
            for (i = 0; i < 4; i++) {
                HudSprite_Show(&glow[i], 0);
            }
            break;
    }
    drop = blink->value * 2.6f - 0.8f;
    drop = HUDP_MAX2(0.0f, drop);
    drop = HUDP_MIN(drop, 1.0f);
    for (i = 0; i < count; i++) {
        if (icons[i].icon < 16) {
            if (icons[i].mode == 1) {
                HudSprite_SetColor(&glow[i], 0x80, 0x80, 0x80, (u8)(ramp->value * 128.0f));
            } else if (icons[i].mode == 0) {
                if (0.9f < drop) {
                    icons[i].frame = 1;
                } else {
                    icons[i].frame = 0;
                }
                HudSprite_Move(&glow[i], 0, (1.0f - drop) * -16.0f);
                HudSprite_SetColor(&glow[i], 0x80, 0x80, 0x80, (u8)(drop * ramp->value * 128.0f));
            }
        }
    }
    if (!HUDP_PAUSED()) {
        Ramp_Step(name);
    }
    for (i = 0; i < count; i++) {
        HudPrompt_ApplyIcon(&icon[i], &icons[i]);
        if ((u32)(*state - 5) < 2) {
            f32 s = (1.0f - blink->value) * 1.5f + 1.0f;

            ghost[i] = icon[i];
            ghostGlow[i] = glow[i];
            HudSprite_Scale(&ghost[i], s, s);
            HudSprite_SetColor(&ghost[i], 0x80, 0x80, 0x80, (u8)(blink->value * 128.0f));
            HudSprite_Scale(&ghostGlow[i], s, s);
            HudSprite_SetColor(&ghostGlow[i], 0x80, 0x80, 0x80, (u8)(blink->value * 128.0f));
        } else {
            HudSprite_Show(&ghost[i], 0);
            HudSprite_Show(&ghostGlow[i], 0);
        }
        HudSprite_Move(&icon[i], x, 0);
        HudSprite_Move(&glow[i], x, 0);
        HudSprite_Move(&ghost[i], x, 0);
        HudSprite_Move(&ghostGlow[i], x, 0);
        if (i < count - 1) {
            x += icon[i].pos[1] - icon[i].pos[0];
        }
        HudSprite_SetColor(&icon[i], 0x80, 0x80, 0x80, (u8)(ramp->value * 128.0f));
    }
    for (i = 0; i < count; i++) {
        if (HUD_SCR(gHudPrompt->side) != 0) {
            HudSprite_Move(&icon[i], -(node->x * 2) - x + 512, 0);
            HudSprite_Move(&glow[i], -(node->x * 2) - x + 512, 0);
            HudSprite_Move(&ghost[i], -(node->x * 2) - x + 512, 0);
            HudSprite_Move(&ghostGlow[i], -(node->x * 2) - x + 512, 0);
        }
    }
    if (icons[0].mode != 3) {
        if (HUD_SCR(gHudPrompt->side) != 0) {
            HUDP_NAME_POS(gHudPrompt->side)->x = icon[0].pos[1];
        } else {
            HUDP_NAME_POS(gHudPrompt->side)->x = icon[0].pos[0];
        }
        HUDP_NAME_POS(gHudPrompt->side)->y = icon[0].pos[2] - 32;
        *&HUDP_NAME_POS(gHudPrompt->side)->x += node->x;
        *&HUDP_NAME_POS(gHudPrompt->side)->y += node->y;
    }
}

/* Draw of node 26: icons and glows, then the enlarged copies. */
void HudPrompt_DrawCommand(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        HudESprite *spr = &gHudPrompt->spr[6 + i];

        HudPrompt_ClearIconMark(spr->tex);
        HudSprite_DrawAt(spr, gHudPrompt->iconRes, 0, 0x2A80, 0x2C90);
        HudSprite_Draw(&gHudPrompt->spr[10 + i], gHudPrompt->res, 0);
        spr = &gHudPrompt->spr[14 + i];
        HudPrompt_ClearIconMark(spr->tex);
        HudSprite_DrawAt(spr, gHudPrompt->iconRes, 0, 0x2A80, 0x2C90);
        HudSprite_Draw(&gHudPrompt->spr[18 + i], gHudPrompt->res, 0);
    }
}

/* Selects the side to update / draw; side 1 is drawn mirrored (node 24's mirror bit). */
#ifdef PORT
/* Widescreen on PC: tells the renderer where the 2D pieces that follow belong (port/src/gs_marker.c). */
#define PORT_2D_LEFT 0x10
#define PORT_2D_RIGHT 0x11
#define PORT_2D_CENTER 0x12
#define PORT_2D_END 0x13
extern void Port_GsMarker(s32 effect);
#endif

void HudPrompt_SelectSide(s32 side) {
    HudENode *node;

    gHudPrompt->side = side;
#ifdef PORT
    Port_GsMarker(HUD_SCR(side) ? PORT_2D_RIGHT : PORT_2D_LEFT);
#endif
    node = &gHudPrompt->node[HUD_PROMPT_NODE_BUTTON];
    node->mirror = HUD_SCR(side) != 0;
}

/* Shows the cue: restarts it unless it is already coming in or shown. */
void HudPrompt_ShowCue(void) {
    if ((u32)gHudPrompt->cueState >= 3) {
        gHudPrompt->cueState = 0;
    }
}

/* Hides the cue: starts the 0.28 s fade-out if it is coming in or shown. */
void HudPrompt_HideCue(void) {
    Ramp *ramp = &gHudPrompt->cueRamp;

    if ((u32)gHudPrompt->cueState < 3) {
        Ramp_Start(ramp, 0.28f, 1.0f, 0.0f);
        gHudPrompt->cueState = 3;
    }
}

/* The cue is drawn dim while the fighter cannot act. */
void HudPrompt_SetCueDim(s32 canAct) {
    gHudPrompt->cueDim = canAct == 0;
}

/* Asks for the button prompt of a side. `button` = column + 4 * style (style 0: press and release, 1: held,
   2: released with the glow), or 12 for the animated picture; `group` is the row of the icon table, and for
   button 12 group 1 means "leave the picture alone". Ignored while the same prompt is already on show or the
   side's command row is active. */
void HudPrompt_SetButton(s32 side, s32 button, s32 group) {
    HudESprite *icon = &gHudPrompt->spr[1];

    gHudPrompt->btnGroup[side] = group;
    if ((u32)(gHudPrompt->btnState[side] - 1) < 38 && gHudPrompt->btn.kind[side] == button) {
        return;
    }
    if (gHudPrompt->cmd.state[side] >= 0) {
        return;
    }
    gHudPrompt->btn.kind[side] = button;
    switch (button) {
        case 2:
            gHudPrompt->btnState[side] = 1;
            gHudPrompt->btn.value[side] = 2;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 3:
            gHudPrompt->btnState[side] = 1;
            gHudPrompt->btn.value[side] = 3;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 1:
            gHudPrompt->btnState[side] = 1;
            gHudPrompt->btn.value[side] = 1;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 0:
            gHudPrompt->btnState[side] = 1;
            gHudPrompt->btn.value[side] = 0;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 6:
            gHudPrompt->btnState[side] = 4;
            gHudPrompt->btn.value[side] = 2;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 7:
            gHudPrompt->btnState[side] = 4;
            gHudPrompt->btn.value[side] = 3;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 5:
            gHudPrompt->btnState[side] = 4;
            gHudPrompt->btn.value[side] = 1;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 4:
            gHudPrompt->btnState[side] = 4;
            gHudPrompt->btn.value[side] = 0;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 10:
            gHudPrompt->btnState[side] = 7;
            gHudPrompt->btn.value[side] = 2;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 11:
            gHudPrompt->btnState[side] = 7;
            gHudPrompt->btn.value[side] = 3;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 9:
            gHudPrompt->btnState[side] = 7;
            gHudPrompt->btn.value[side] = 1;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 8:
            gHudPrompt->btnState[side] = 7;
            gHudPrompt->btn.value[side] = 0;
            gHudPrompt->btn.flip[side] = 0;
            break;
        case 12:
            if (group != 1) {
                HudSprite_InitTex(icon, gHudPrompt->res, 1, 0);
                gHudPrompt->btn.value[side] = 1;
                HudSprite_Center(icon);
                gHudPrompt->btnState[side] = 10;
            }
            gHudPrompt->btn.flip[side] = 0;
            break;
    }
}

/* Asks for a command row: `count` icons with their animation modes, and the entry of the name list to draw
   above it (-1: none; a name on show then fades out over 0.3 s). Ignored while the side's button prompt is
   active, and when the row on show is the same. An icon below 16 with mode 2 restarts its animation at once. */
void HudPrompt_SetCommand(s32 side, s32 count, s32 *icons, s32 *modes, s32 nameIdx) {
    s32 i;

    if (count <= 0) {
        return;
    }
    if (gHudPrompt->btnState[side] > 0) {
        return;
    }
    if (gHudPrompt->cmd.on[side] == 1) {
        s32 same = 1;

        for (i = 0; i < count; i++) {
            if (icons[i] != gHudPrompt->row.icon[side][i].icon) {
                same = 0;
            }
            if (modes[i] != gHudPrompt->row.icon[side][i].mode) {
                same = 0;
            }
        }
        if (nameIdx != gHudPrompt->nameIdx[side]) {
            same = 0;
        }
        if (same) {
            return;
        }
    }
    if (nameIdx < 0) {
        Ramp *name = &gHudPrompt->nameFade[side];

        if (0.0f < name->value) {
            Ramp_Start(name, 0.3f, 1.0f, 0.0f);
        } else {
            gHudPrompt->nameIdx[side] = nameIdx;
        }
    } else {
        gHudPrompt->nameIdx[side] = nameIdx;
    }
    if (gHudPrompt->cmd.on[side] != 1) {
        gHudPrompt->cmd.state[side] = 0;
    }
    gHudPrompt->cmd.on[side] = 1;
    gHudPrompt->row.count[side] = count;
    for (i = 0; i < 4; i++) {
        if (i < count) {
            gHudPrompt->row.icon[side][i].icon = icons[i];
            gHudPrompt->row.icon[side][i].frame = 0;
            gHudPrompt->row.icon[side][i].timer = 0;
            gHudPrompt->row.icon[side][i].mode = modes[i];
            gHudPrompt->row.icon[side][i].restart = 0;
            if ((u32)icons[i] < 16 && modes[i] == 2) {
                gHudPrompt->row.icon[side][i].restart = 1;
            }
        } else {
            memset(&gHudPrompt->row.icon[side][i], 0, sizeof(HudPromptIcon));
        }
    }
}

/* Takes the command row away: 0.3 s fade-out of the row and of the name; while the battle is paused it goes at
   once. */
void HudPrompt_ClearCommand(s32 side) {
    Ramp *name = &gHudPrompt->nameFade[side];

    if (gHudPrompt->cmd.on[side]) {
        gHudPrompt->cmd.on[side] = 0;
        if (HUDP_PAUSED()) {
            gHudPrompt->cmd.state[side] = 9;
            Ramp_Stop(name);
        } else {
            gHudPrompt->cmd.state[side] = 3;
            if (0.0f < name->value) {
                Ramp_Start(name, 0.3f, 1.0f, 0.0f);
                return;
            }
        }
        gHudPrompt->nameIdx[side] = -1;
    }
}

/* The command was performed: plays the "accepted" animation of the row (state 5) and fades the name out. */
void HudPrompt_AcceptCommand(s32 side) {
    s32 on = gHudPrompt->cmd.on[side];
    Ramp *name = &gHudPrompt->nameFade[side];

    if (on) {
        gHudPrompt->cmd.on[side] = 0;
        gHudPrompt->cmd.state[side] = 5;
        if (0.0f < name->value) {
            Ramp_Start(name, 0.3f, 1.0f, 0.0f);
            return;
        }
        gHudPrompt->nameIdx[side] = -1;
    }
}

/* Slides the part off the screen over `seconds`. */
void HudPrompt_SlideOut(f32 seconds) {
    Ramp_Start(&gHudPrompt->slide, seconds, 0.0f, 1.0f);
}

/* Slides it back. */
void HudPrompt_SlideIn(f32 seconds) {
    Ramp_Start(&gHudPrompt->slide, seconds, 1.0f, 0.0f);
}

/* Takes the button prompt away: starts its fade-out (state 40) if it is on show. */
void HudPrompt_ClearButton(s32 side) {
    if ((u32)(gHudPrompt->btnState[side] - 1) < 38) {
        gHudPrompt->btnState[side] = 40;
    }
}

/* Draws the technique name above each side's command row (text, after the sprites): entry nameIdx of the list
   BtlMenu_SetScript2 selects for the side's fighter, at namePos, with nameFade as alpha. A name whose alpha reached 0
   is dropped. */
void HudPrompt_DrawNames(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        Ramp *fade = &gHudPrompt->nameFade[i];
        f32 alpha = fade->value;
        HudPromptPos *pos = &gHudPrompt->namePos[i];
        s32 idx = gHudPrompt->nameIdx[i];

        if (alpha <= 0.0f) {
            gHudPrompt->nameIdx[i] = -1;
            idx = -1;
        }
        if (idx >= 0) {
#ifdef PORT
            Port_GsMarker(HUD_SCR(i) ? PORT_2D_RIGHT : PORT_2D_LEFT); /* (widescreen: each name with its side, as HudCombo_DrawText) */
#endif
            BtlMenu_SetScript2(BtlCtrl_GetObj(i)->unkBC);
            BtlText_DrawEntryName(pos->x, pos->y, idx, HUD_SCR(i), alpha); /* (the 4th: left or right aligned) */
        }
    }
}

/* A white 32x32 sprite, hidden: the icons and glows of the command row are filled in by HudPrompt_UpdateCommand. */
#define HUDP_INIT_ROW_SPRITE(n) \
    spr = &gHudPrompt->spr[n]; \
    HudSprite_SetColor(spr, 0xFF, 0xFF, 0xFF, 0x80); \
    HudSprite_SetRect(spr, 0, 32, 0, 32); \
    HudSprite_Show(spr, 0)

/* Builds the part: the work, 22 sprites and 27 nodes of which four are used: node 23 the root, node 24 (64, 384)
   the button prompt, node 25 (256, 384) the cue, node 26 (60, 400) the command row. */
void HudPrompt_Init(HudENode **out, HudERes *res) {
    HudESprite *spr;
    HudENode *node;
    s32 i;
    s32 y;
    s32 rowCount;

    gHudPrompt = Heap_Alloc(sizeof(HudPrompt), 0x20, 0, 2);
    memset(gHudPrompt, 0, sizeof(HudPrompt));
    gHudPrompt->spr = Heap_Alloc(HUD_PROMPT_SPR_COUNT * sizeof(HudESprite), 0x20, 0, 2);
    memset(gHudPrompt->spr, 0, HUD_PROMPT_SPR_COUNT * sizeof(HudESprite));
    gHudPrompt->node = Heap_Alloc(HUD_PROMPT_NODE_COUNT * sizeof(HudENode), 0x20, 0, 2);
    memset(gHudPrompt->node, 0, HUD_PROMPT_NODE_COUNT * sizeof(HudENode));
    for (i = 0; i < 2; i++) {
        gHudPrompt->btnState[i] = -1;
        gHudPrompt->cmd.state[i] = -1;
    }
    y = 384;
    rowCount = 8;
    gHudPrompt->cueState = -1;
    gHudPrompt->res = res;
    gHudPrompt->iconRes = FontIcon_GetRes();
    gHudPrompt->iconDefs = FontIcon_GetDefs();

    spr = &gHudPrompt->spr[0];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudPrompt->spr[4];
    HudSprite_InitTex(spr, res, 8, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudPrompt->spr[1];
    HudSprite_InitTex(spr, res, 1, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudPrompt->spr[5];
    HudSprite_InitTex(spr, res, 1, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudPrompt->spr[2];
    HudSprite_InitTex(spr, res, 1, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    spr = &gHudPrompt->spr[3];
    HudSprite_InitTex(spr, res, 7, 0);
    HudSprite_Center(spr);
    HudSprite_Show(spr, 0);

    HUDP_INIT_ROW_SPRITE(6);
    HUDP_INIT_ROW_SPRITE(7);
    HUDP_INIT_ROW_SPRITE(8);
    HUDP_INIT_ROW_SPRITE(9);
    HUDP_INIT_ROW_SPRITE(10);
    HUDP_INIT_ROW_SPRITE(11);
    HUDP_INIT_ROW_SPRITE(12);
    HUDP_INIT_ROW_SPRITE(13);

    node = &gHudPrompt->node[HUD_PROMPT_NODE_BUTTON];
    node->x = 64;
    node->y = y;
    node->sprCount = 4;
    node->sprList = Heap_Alloc(4 * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudPrompt->spr[0];
    node->sprList[1] = &gHudPrompt->spr[1];
    node->sprList[2] = &gHudPrompt->spr[2];
    node->sprList[3] = &gHudPrompt->spr[3];
    node->update = HudPrompt_UpdateButton;
    node->draw = HudPrompt_DrawButton;

    node = &gHudPrompt->node[HUD_PROMPT_NODE_CUE];
    node->x = 256;
    node->y = y;
    node->sprCount = 2;
    node->sprList = Heap_Alloc(2 * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudPrompt->spr[4];
    node->sprList[1] = &gHudPrompt->spr[5];
    node->update = HudPrompt_UpdateCue;
    node->draw = HudPrompt_DrawCue;

    node = &gHudPrompt->node[HUD_PROMPT_NODE_COMMAND];
    node->x = 60;
    node->y = 400;
    node->sprCount = rowCount;
    node->sprList = Heap_Alloc(8 * sizeof(HudESprite *), 0x20, 0, 2);
    memset(node->sprList, 0, node->sprCount * sizeof(HudESprite *));
    node->sprList[0] = &gHudPrompt->spr[6];
    node->sprList[1] = &gHudPrompt->spr[7];
    node->sprList[2] = &gHudPrompt->spr[8];
    node->sprList[3] = &gHudPrompt->spr[9];
    node->sprList[4] = &gHudPrompt->spr[10];
    node->sprList[5] = &gHudPrompt->spr[11];
    node->sprList[6] = &gHudPrompt->spr[12];
    node->sprList[7] = &gHudPrompt->spr[13];
    node->update = HudPrompt_UpdateCommand;
    node->draw = HudPrompt_DrawCommand;

    node = &gHudPrompt->node[HUD_PROMPT_NODE_ROOT];
    node->x = 0;
    node->y = 0;
    node->childCount = 3;
    node->childList = Heap_Alloc(3 * sizeof(HudENode *), 0x20, 0, 2);
    memset(node->childList, 0, node->childCount * sizeof(HudENode *));
    node->childList[0] = &gHudPrompt->node[HUD_PROMPT_NODE_BUTTON];
    node->childList[1] = &gHudPrompt->node[HUD_PROMPT_NODE_CUE];
    node->childList[2] = &gHudPrompt->node[HUD_PROMPT_NODE_COMMAND];
    node->draw = NULL;
    node->update = HudPrompt_UpdateRoot;

    *out = &gHudPrompt->node[HUD_PROMPT_NODE_ROOT];
}

/* Frees the part. */
void HudPrompt_Term(void) {
    s32 i;

    if (gHudPrompt->spr != NULL) {
        Heap_Free(gHudPrompt->spr);
    }
    for (i = 0; i < HUD_PROMPT_NODE_COUNT; i++) {
        if (gHudPrompt->node[i].sprList != NULL) {
            Heap_Free(gHudPrompt->node[i].sprList);
        }
        if (gHudPrompt->node[i].childList != NULL) {
            Heap_Free(gHudPrompt->node[i].childList);
        }
    }
    if (gHudPrompt->node != NULL) {
        Heap_Free(gHudPrompt->node);
    }
    if (gHudPrompt != NULL) {
        Heap_Free(gHudPrompt);
    }
}

/* Round reset: everything off. (nameFade is not stopped.) */
void HudPrompt_Reset(void) {
    s32 i;

    for (i = 0; i < 2; i++) {
        gHudPrompt->btnState[i] = -1;
        Ramp_Stop(&gHudPrompt->btnPulse[i]);
        Ramp_Stop(&gHudPrompt->btnRamp[i]);
        Ramp_Stop(&gHudPrompt->cmdRamp[i]);
        Ramp_Stop(&gHudPrompt->btnBlink[i]);
        Ramp_Stop(&gHudPrompt->cmdBlink[i]);
        gHudPrompt->btn.value[i] = 0;
        gHudPrompt->btn.flip[i] = 0;
        gHudPrompt->btn.kind[i] = 0;
        gHudPrompt->cmd.state[i] = -1;
        gHudPrompt->row.count[i] = 0;
        gHudPrompt->cmd.on[i] = 0;
        gHudPrompt->nameIdx[i] = -1;
        HUDP_NAME_POS(i)->x = -1;
        HUDP_NAME_POS(i)->y = -1;
        gHudPrompt->cueState = -1;
        Ramp_Stop(&gHudPrompt->cuePulse);
        Ramp_Stop(&gHudPrompt->cueRamp);
        memset(&gHudPrompt->btn.icon[i], 0, sizeof(HudPromptIcon));
        memset(gHudPrompt->row.icon[i], 0, 4 * sizeof(HudPromptIcon));
    }
    Ramp_Stop(&gHudPrompt->slide);
}
