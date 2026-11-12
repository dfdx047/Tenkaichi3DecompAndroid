#include "common.h"
#include "battle/hud_a.h"
#include "sys/heap.h"
#include "sys/mathf.h"

/*
 * HUD caption part. Source range 0x21BCA0-0x21C0E0.
 *
 * Two sprites from the fifth HUD sheet: texture 0 at the top of the screen (240, 19), and texture 1 at the
 * bottom left (0, 386) whose alpha pulses. Hud_Draw draws this part in the attract demo (battle mode 7) and,
 * with replay HUD mode 0, during a replay; the pulsing sprite is left out while a replay is played.
 *
 * Nodes: 0 root (child 1, no callbacks); 1 holds sprites 1 and 2 (HudCaption_Update / HudCaption_Draw).
 * Sprite 0 is allocated and never used.
 */

extern void *memset(void *dst, s32 c, u32 n);
extern s32 BtlGame_IsReplay(void); /* returns gBtlGameReplayActive */

/* The caption work. Defined here: this object's .sdata (0x2FEB44). */
HudCaption *gHudCaption = NULL;

extern void HudSprite_Show(HudSprite *spr, s32 show);
extern void HudSprite_SetColor(HudSprite *spr, s32 r, s32 g, s32 b, s32 a);
extern void HudSprite_Move(HudSprite *spr, s32 dx, s32 dy);
extern void HudSprite_InitTex(HudSprite *spr, HudRes *res, s32 tex, s32 sub);
extern void HudSprite_Draw(HudSprite *spr, HudRes *res, s32 additive);

#define HUD_CAPTION_SPR_COUNT 3
#define HUD_CAPTION_NODE_COUNT 2

/* Draws the pulsing sprite (not during a replay) and the top sprite. */
void HudCaption_Draw(void) {
    if (!BtlGame_IsReplay()) {
        HudSprite_Draw(&gHudCaption->sprites[2], gHudCaption->res, 0);
    }
    HudSprite_Draw(&gHudCaption->sprites[1], gHudCaption->res, 0);
}

/* Runs the one-second ramp over and over and sets the pulsing sprite's alpha to 64 + 64 * (sin(2 pi t) + 1) / 2.
   The ramp is stepped whether or not the battle is paused. */
void HudCaption_Update(void) {
    HudSprite *spr = &gHudCaption->sprites[2];

    if (Ramp_Step(&gHudCaption->pulse)) {
        Ramp_Start(&gHudCaption->pulse, 1.0f, 0.0f, 1.0f);
    }
    HudSprite_SetColor(spr, 0xFF, 0xFF, 0xFF,
                  (u8)((Mathf_Sin(gHudCaption->pulse.value * 3.14159265f * 2.0f) + 1.0f) * 0.5f * 64.0f + 64.0f));
}

/* Builds the part; *out receives the root node. */
void HudCaption_Init(HudNode **out, HudRes *res) {
    HudSprite *spr;
    HudNode *node;

    gHudCaption = Heap_Alloc(sizeof(HudCaption), 0x20, 0, 2);
    memset(gHudCaption, 0, sizeof(HudCaption));
    gHudCaption->sprites = Heap_Alloc(HUD_CAPTION_SPR_COUNT * sizeof(HudSprite), 0x20, 0, 2);
    memset(gHudCaption->sprites, 0, HUD_CAPTION_SPR_COUNT * sizeof(HudSprite));
    gHudCaption->nodes = Heap_Alloc(HUD_CAPTION_NODE_COUNT * sizeof(HudNode), 0x20, 0, 2);
    memset(gHudCaption->nodes, 0, HUD_CAPTION_NODE_COUNT * sizeof(HudNode));
    gHudCaption->res = res;

    spr = &gHudCaption->sprites[1];
    HudSprite_InitTex(spr, res, 0, 0);
    HudSprite_Move(spr, 240, 19);
    HudSprite_Show(spr, 1);

    spr = &gHudCaption->sprites[2];
    HudSprite_InitTex(spr, res, 1, 0);
    HudSprite_Move(spr, 0, 386);
    HudSprite_Show(spr, 1);

    node = &gHudCaption->nodes[1];
    node->spriteCount = 2;
    node->x = 0;
    node->y = 0;
    node->sprites = Heap_Alloc(2 * sizeof(HudSprite *), 0x20, 0, 2);
    memset(node->sprites, 0, node->spriteCount * sizeof(HudSprite *));
    node->sprites[0] = &gHudCaption->sprites[1];
    node->sprites[1] = &gHudCaption->sprites[2];
    node->update = HudCaption_Update;
    node->draw = (void (*)(HudNode *))HudCaption_Draw;

    node = &gHudCaption->nodes[0];
    node->childCount = 1;
    node->x = 0;
    node->y = 0;
    node->children = Heap_Alloc(1 * sizeof(HudNode *), 0x20, 0, 2);
    memset(node->children, 0, node->childCount * sizeof(HudNode *));
    node->children[0] = &gHudCaption->nodes[1];
    node->update = NULL;
    node->draw = NULL;

    *out = gHudCaption->nodes;
}

/* Frees the sprites, the nodes' lists, the nodes and the work. */
void HudCaption_Term(void) {
    s32 i;

    if (gHudCaption->sprites != NULL) {
        Heap_Free(gHudCaption->sprites);
    }
    for (i = 0; i < HUD_CAPTION_NODE_COUNT; i++) {
        if (gHudCaption->nodes[i].sprites != NULL) {
            Heap_Free(gHudCaption->nodes[i].sprites);
        }
        if (gHudCaption->nodes[i].children != NULL) {
            Heap_Free(gHudCaption->nodes[i].children);
        }
    }
    if (gHudCaption->nodes != NULL) {
        Heap_Free(gHudCaption->nodes);
    }
    if (gHudCaption != NULL) {
        Heap_Free(gHudCaption);
    }
}

/* Zeroes the pulse ramp. */
void HudCaption_Reset(void) {
    memset(&gHudCaption->pulse, 0, sizeof(Ramp));
}
