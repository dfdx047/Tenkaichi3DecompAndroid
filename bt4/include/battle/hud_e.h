#ifndef BATTLE_HUD_E_H
#define BATTLE_HUD_E_H

#include "types.h"
#include "sys/ramp.h"

/*
 * Battle HUD continued, 0x22A750-0x22FD10:
 *   src/battle/hud_e.c    0x22A750-0x22B4F8  notice part: announcement 7, the starter, init / term / reset
 *   src/battle/hud_e_b.c  0x22B4F8-0x22EC08  prompt part: button prompt, pad cue, command row
 *   src/battle/hud_e_c.c  0x22EC08-0x22F998  timer part
 *   src/battle/hud_e_d.c  0x22F998-0x22FC40  pause request (not HUD: which pad may pause the battle)
 *   src/battle/hud_e_e.c  0x22FC40-0x22FD10  two list helpers of the stage rigid bodies (head of stg_d.c)
 *
 * The sprite and node layouts are local views that agree with HudSprite / HudNode of include/battle/hud_a.h.
 */

/* A HUD sprite (0x1C bytes). */
typedef struct HudESprite {
    /* 0x00 */ u32 flags;   /* bit 0: hidden, bit 1: mirrored */
    /* 0x04 */ s16 tex;     /* texture entry of the sheet */
    /* 0x06 */ s16 texSub;
    /* 0x08 */ s16 pos[4];  /* screen rectangle x0, x1, y0, y1 relative to the node */
    /* 0x10 */ s16 uv[4];   /* texel rectangle u0, u1, v0, v1 */
    /* 0x18 */ u8 r, g, b, a;
} HudESprite; /* size 0x1C */

/* A HUD node (0x38 bytes). */
typedef struct HudENode {
    /* 0x00 */ union {
        u32 flags;          /* bit 0: hidden, bit 1: mirrored */
        struct {
            u32 hidden : 1;
            u32 mirror : 1;
        };
    };
    /* 0x04 */ f32 rot;
    /* 0x08 */ f32 unk8;
    /* 0x0C */ f32 unkC;
    /* 0x10 */ s32 x;
    /* 0x14 */ s32 y;
    /* 0x18 */ s32 ofsX;
    /* 0x1C */ s32 ofsY;
    /* 0x20 */ u32 sprCount;
    /* 0x24 */ HudESprite **sprList;
    /* 0x28 */ u32 childCount;
    /* 0x2C */ struct HudENode **childList;
    /* 0x30 */ void (*update)();
    /* 0x34 */ void (*draw)();
} HudENode; /* size 0x38 */

/* One texture entry of a sprite sheet (0x40 bytes). */
typedef struct HudETex {
    /* 0x00 */ u8 unk0[0x28];
    /* 0x28 */ s32 mark;
    /* 0x2C */ u8 unk2C[0x14];
} HudETex; /* size 0x40 */

/* A sprite sheet. */
typedef struct HudERes {
    /* 0x00 */ s32 texCount;
    /* 0x04 */ s32 unk4[3];
    /* 0x10 */ HudETex *tex;
} HudERes;

/* ---- Notice part (gHudNotice, 0x74 bytes): the view of hud_e.c. ---- */
typedef struct HudENotice {
    /* 0x00 */ HudERes *res;       /* sprite sheet 1 of the HUD file */
    /* 0x04 */ HudESprite *spr;    /* HUD_NOTICE_SPR_COUNT */
    /* 0x08 */ HudENode *node;     /* HUD_NOTICE_NODE_COUNT: [0] root, [1] the announcement, [2] the band, [3] the
                                      replay mark */
    /* 0x0C */ s32 state;          /* step of the running announcement; -1 at rest */
    /* 0x10 */ s32 unk10;
    /* 0x14 */ Ramp rampA;
    /* 0x2C */ Ramp rampB;
    /* 0x44 */ f32 trail[12];
} HudENotice; /* size 0x74 */

#define HUD_NOTICE_SPR_COUNT 16
#define HUD_NOTICE_NODE_COUNT 4
#define HUD_NOTICE_SPR_STREAK 13
#define HUD_NOTICE_SPR_BAND 14
#define HUD_NOTICE_SPR_REPLAY 15

/* ---- Prompt part (gHudPrompt, 0x2D0 bytes). ---- */

/* Definition of a button icon (12 bytes): a strip of cells, two per row, in a texture of the icon sheet. The same
   layout as FontIconDef (battle/col_c.h); the icon sheet's own table (FontIcon_GetDefs) is read through it. */
typedef struct HudPromptIconDef {
    /* 0x00 */ u8 tex;        /* texture entry of the icon sheet; for the sheet's own table also the number of the
                                 definition that draws it (HudPrompt_UpdateButton looks the icon up twice) */
    /* 0x01 */ u8 cell;       /* cell size in texels: 32 */
    /* 0x02 */ u8 width;      /* width of the picture inside the cell, minus 1 */
    /* 0x03 */ u8 pressed;    /* first frame of the second half of the animation ("pressed") */
    /* 0x04 */ u16 frames;    /* last frame */
    /* 0x06 */ s8 time[6];    /* how long each frame is held (in counts of HudPromptIcon.timer); 0 ends the run */
} HudPromptIconDef; /* size 0xC */

/* State of one shown icon (0x18 bytes). */
typedef struct HudPromptIcon {
    /* 0x00 */ s32 icon;      /* icon number: 0..15 gHudPromptIconDefs, 16..32 gHudPromptIconDefs16, above that the
                                 definitions of the icon sheet (FontIcon_GetDefs) */
    /* 0x04 */ s32 frame;     /* animation frame, -1: the run ended (the last frame stays on show) */
    /* 0x08 */ s32 timer;     /* counted up by the owner while the icon animates */
    /* 0x0C */ s32 mode;      /* 0..4: which frames play (HudPrompt_ApplyIconDef) */
    /* 0x10 */ s32 last;      /* frame shown after the run ended */
    /* 0x14 */ s32 restart;   /* restart at once when the run ends (else when timer reaches 20) */
} HudPromptIcon; /* size 0x18 */

/* A screen position. */
typedef struct HudPromptPos {
    s32 x;
    s32 y;
} HudPromptPos;

typedef struct HudPrompt {
    /* 0x000 */ HudERes *res;            /* sprite sheet 3 of the HUD file */
    /* 0x004 */ HudESprite *spr;         /* HUD_PROMPT_SPR_COUNT */
    /* 0x008 */ HudENode *node;          /* HUD_PROMPT_NODE_COUNT are allocated; only [23]..[26] are used */
    /* 0x00C */ HudERes *iconRes;        /* button icon sheet (FontIcon_GetRes) */
    /* 0x010 */ HudPromptIconDef *iconDefs; /* its definitions (FontIcon_GetDefs) */
    /* 0x014 */ s32 side;                /* side being updated / drawn */
    /* 0x018 */ s32 btnState[2];         /* step of the button prompt (HudPrompt_UpdateButton); 0 / -1 off */
    /* 0x020 */ Ramp btnPulse[2];        /* 1 <-> 0 every 0.1 s: the base's size */
    /* 0x050 */ Ramp btnRamp[2];         /* fade / frame timer of the step */
    /* 0x080 */ struct {
        s32 value[2];                    /* column of the icon table; kind 12: texture of the picture */
        HudPromptIcon icon[2];           /* 0x088 */
        s32 flip[2];                     /* 0x0B8: the picture is mirrored (second half of kind 12's animation) */
        s32 kind[2];                     /* 0x0C0: button asked for (HudPrompt_SetButton), 13 none */
    } btn;
    /* 0x0C8 */ Ramp slide;              /* 0 on screen .. 1 away */
    /* 0x0E0 */ struct {
        s32 on[2];                       /* a command row is asked for */
        s32 state[2];                    /* 0x0E8: step of the command row (HudPrompt_UpdateCommand); -1 off */
    } cmd;
    /* 0x0F0 */ Ramp cmdRamp[2];         /* fade of the row */
    /* 0x120 */ struct {
        s32 count[2];                    /* icons in the row, at most 4 */
        HudPromptIcon icon[2][4];        /* 0x128 */
    } row;
    /* 0x1E8 */ HudPromptPos namePos[2]; /* where HudPrompt_DrawNames puts the technique name */
    /* 0x1F8 */ s32 nameIdx[2];          /* entry of the name list, -1 none */
    /* 0x200 */ Ramp btnBlink[2];        /* 1 -> 0 every 1.5 s: the glow of the button prompt */
    /* 0x230 */ Ramp cmdBlink[2];        /* the same for the command row; 0.8 s once when the command is accepted */
    /* 0x260 */ Ramp nameFade[2];        /* alpha of the name */
    /* 0x290 */ s32 cueState;            /* 0 start, 1 fading in, 2 shown, 3 fading out, 4 / -1 off */
    /* 0x294 */ Ramp cueRamp;
    /* 0x2AC */ Ramp cuePulse;
    /* 0x2C4 */ s32 cueDim;              /* the cue is drawn at half alpha, not pulsing */
    /* 0x2C8 */ s32 btnGroup[2];         /* third argument of HudPrompt_SetButton: row of the icon table */
} HudPrompt; /* size 0x2D0 */

#define HUD_PROMPT_SPR_COUNT 22
#define HUD_PROMPT_NODE_COUNT 27
#define HUD_PROMPT_NODE_ROOT 23
#define HUD_PROMPT_NODE_BUTTON 24
#define HUD_PROMPT_NODE_CUE 25
#define HUD_PROMPT_NODE_COMMAND 26

/* ---- Timer part (gHudTimer, 0x4C bytes). ---- */
typedef struct HudTimer {
    /* 0x00 */ HudERes *res;      /* sprite sheet 0 of the HUD file */
    /* 0x04 */ HudESprite *spr;   /* 6: [0] the frame, [1] ones, [2] tens, [3] hundreds, [4] the mode 3 label,
                                     [5] the mark */
    /* 0x08 */ HudENode *node;    /* 2: [0] root, [1] the clock */
    /* 0x0C */ s32 value;         /* seconds left, or the count */
    /* 0x10 */ s32 prev;          /* value of the previous frame */
    /* 0x14 */ Ramp pulse;        /* 1 -> 0 in 0.5 s each time the value changes below 4 */
    /* 0x2C */ Ramp slide;        /* 0 on screen .. 1 away */
    /* 0x44 */ s32 isClock;       /* 1: a clock (digits coloured by the time left), 0: a count */
    /* 0x48 */ s32 mark;          /* sprite 5 shown */
} HudTimer; /* size 0x4C */

/* ---- Pause request (gBtlPause, 8 bytes). ---- */
typedef struct BtlPause {
    /* 0x00 */ s32 padCount;      /* pads that may pause: 2 in split screen, else 1 */
    /* 0x04 */ s32 pad;           /* the pad that paused / owns the pause menu */
} BtlPause;

extern HudPrompt *gHudPrompt;
extern HudTimer *gHudTimer;

/* hud_e.c */
s32 HudNotice_GetVoiceBase(void);
void HudNotice_Show(s32 id);
void HudNotice_ShowReplayMark(s32 on);
void HudNotice_Init(HudENode **out, HudERes *res);
void HudNotice_Term(void);
void HudNotice_Reset(void);

/* hud_e_b.c */
void HudPrompt_SelectSide(s32 side);
void HudPrompt_ShowCue(void);
void HudPrompt_HideCue(void);
void HudPrompt_SetCueDim(s32 canAct);
void HudPrompt_SetButton(s32 side, s32 button, s32 group);
void HudPrompt_SetCommand(s32 side, s32 count, s32 *icons, s32 *modes, s32 nameIdx);
void HudPrompt_ClearCommand(s32 side);
void HudPrompt_AcceptCommand(s32 side);
void HudPrompt_SlideOut(f32 seconds);
void HudPrompt_SlideIn(f32 seconds);
void HudPrompt_ClearButton(s32 side);
void HudPrompt_DrawNames(void);
void HudPrompt_Init(HudENode **out, HudERes *res);
void HudPrompt_Term(void);
void HudPrompt_Reset(void);

/* hud_e_c.c */
void HudTimer_SlideOut(f32 seconds);
void HudTimer_SlideIn(f32 seconds);
void HudTimer_Init(HudENode **out, HudERes *res);
void HudTimer_Term(void);
void HudTimer_SetTime(s32 seconds);
void HudTimer_SetCount(s32 count);
void HudTimer_ShowMark(void);
void HudTimer_ShowMark2(void);
void HudTimer_Reset(void);

/* hud_e_d.c */
BtlPause *BtlPause_GetWork(void);
void BtlPause_Reset(void);
void BtlPause_Init(s32 padCount);
void BtlPause_Term(void);
s32 BtlPause_CheckOpen(void);
s32 BtlPause_CheckStart(void);
s32 BtlPause_GetPadCount(void);
void BtlPause_SetPad(s32 pad);
s32 BtlPause_GetMenuPad(void);
s32 BtlPause_GetPad(void);

#endif
