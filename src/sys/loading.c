#include "common.h"
#include "sys/loading.h"
#include "sys/file.h"
#include "sys/job.h"
#include "sys/rand.h"

#ifdef PORT
extern void Port_GsMarker(s32 effect);
extern int Port_PageAspectMilli(void);
#define PORT_2D_LOADING 0x17
#define PORT_2D_END 0x13
#endif

/* libc */
extern void *memset(void *dst, s32 value, u32 size);

extern void Dma_ResetBuffers(void);
extern void Dma_Flush(void);
extern void Gfx_BeginFrame(void);
extern void Gfx_EndFrame(s32 vsyncs);
extern void Snd_Update(void);
extern void Fade_Start(s32 idx, s32 dir, f32 seconds);
extern s32 Fade_IsDone(s32 idx);
extern void Res_RelocateOffsets(void *out, void *base, void *hdr);
extern void File_Stub264D90(void);

extern void PadWatch_SetEnabled(s32 enable);                 /* stores (enable == 0) at +0x20 of the object at gPadWatch */
extern void Fade_ResetAll(void);                       /* resets the three gFade entries */
extern void *Sprite_Unpack(void *src, void *dst, s32 *rawSize); /* wrapper of Bpe_Decode */
extern void Sprite_DrawList(void *res, s32 x, s32 y, LoadSprite *list); /* draws a sprite run at an offset */
extern s32 Rand_IntRange(s32 a, s32 b);                /* random integer in [a, b] */
extern f32 Mathf_Sin(f32 angle);                   /* sine */
extern void Num_ToDigits(u8 *digits, s32 value, s32 count, s32 zeroPad); /* decimal digits, 10 = blank */
extern s32 Num_CountDigits(s32 value);                   /* number of decimal digits */

/* The one pad field read here. sys/pad.h describes the whole struct but was still changing while this file was
   written, so the field is declared by offset. */
typedef struct LoadPad {
    /* 0x000 */ u8 unk0[0x18C];
    /* 0x18C */ u32 gamePressed; /* game-layout buttons that went down this frame */
    /* 0x190 */ u8 unk190[0x30];
} LoadPad; /* 0x1C0 */

extern LoadPad gPad[2];
extern void Pad_Update(void);

#define SPRITE(p, f, X0, Y0, X1, Y1, U0, V0, U1, V1, T, R, G, B, A) \
    (p)->flags = (f); \
    (p)->x0 = (X0); \
    (p)->y0 = (Y0); \
    (p)->x1 = (X1); \
    (p)->y1 = (Y1); \
    (p)->u0 = (U0); \
    (p)->v0 = (V0); \
    (p)->u1 = (U1); \
    (p)->v1 = (V1); \
    (p)->tex = (T); \
    (p)->r = (R); \
    (p)->g = (G); \
    (p)->b = (B); \
    (p)->a = (A); \
    (p)++

#define SPRITE_END(p) SPRITE(p, LOAD_SPR_END, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

/* Shows the loading screen and runs the queued jobs until they are done and the fade-out has finished. */
void Load_RunBlocking(void) {
    s32 busy = 1;
    s32 done = 0;
    s32 prev;

    PadWatch_SetEnabled(0);
    Dma_ResetBuffers();
    Load_InitScreen();
    Fade_ResetAll();
    Fade_Start(0, 1, 1.0f);
    gProgress->flags |= PROGRESS_FLAG_LOADING;
    for (;;) {
        Snd_Update();
        prev = busy;
        Gfx_BeginFrame();
        Pad_Update();
        busy = Job_Run();
        if (prev != busy) {
            Fade_Start(0, 0, 1.0f);
        }
        if (!busy) {
            if (Fade_IsDone(0)) {
                done = 1;
            }
        }
        Load_UpdateScreen();
        Load_DrawScreen();
        Gfx_EndFrame(2);
        Dma_Flush();
        if (done) {
            break;
        }
        Load_ReadInput();
    }
    Dma_ResetBuffers();
    gProgress->flags &= ~PROGRESS_FLAG_LOADING;
    PadWatch_SetEnabled(1);
}

/* Fills the sprite list for the current screen type (and scatters the items of type 2). */
void Load_BuildSprites(void) {
    LoadSprite *spr;
    s32 i;

    gLoadScreen.sprites = gProgress->loadSprites;
    memset(gLoadScreen.sprites, 0, LOAD_SPRITE_COUNT * sizeof(LoadSprite));
    switch (gLoadScreen.type) {
    case LOAD_TYPE_0:
        spr = gLoadScreen.sprites;
        SPRITE(spr, LOAD_SPR_CLEAR, 0, 0, 0x200, 0x1C0, 0, 0, 0, 0, 0, 0x22, 0x49, 0x68, 0x80);
        SPRITE_END(spr);
        SPRITE(spr, LOAD_SPR_DRAW, 0x2D, 0x109, 0x6D, 0x189, 0, 0, 0x40, 0x80, 1, 0x80, 0x80, 0x80, 0x80);
        SPRITE(spr, LOAD_SPR_DRAW, 0x13, 0x13E, 0x93, 0x1A9, 0, 0x15, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0x80);
        SPRITE_END(spr);
        SPRITE(spr, LOAD_SPR_DRAW, 0x71, 0x145, 0x88, 0x156, 0, 0, 0x17, 0x11, 0, 0x80, 0x80, 0x80, 0x80);
        for (i = 0; i < 9; i++) {
            SPRITE(spr, LOAD_SPR_DRAW, 0x71, 0x13F - i * 6, 0x88, 0x148 - i * 6, 0x20, 0, 0x37, 9, 0, 0x80, 0x80, 0x80, 0x80);
        }
        SPRITE_END(spr);
        break;
    case LOAD_TYPE_1:
        spr = gLoadScreen.sprites;
        SPRITE(spr, LOAD_SPR_CLEAR, 0, 0, 0x200, 0x1C0, 0, 0, 0, 0, 0, 0x46, 0x10, 0x12, 0x80);
        SPRITE_END(spr);
        SPRITE(spr, LOAD_SPR_DRAW, 0x1A, 0x129, 0x9A, 0x1A9, 0, 0, 0x80, 0x80, 0, 0x80, 0x80, 0x80, 0x80);
        SPRITE_END(spr);
        SPRITE(spr, LOAD_SPR_DRAW, 0x79, 0x141, 0xBC, 0x180, 0, 0x40, 0x43, 0x7F, 2, 0x80, 0x80, 0x80, 0x80);
        SPRITE_END(spr);
        for (i = 0; i < 3; i++) {
            SPRITE(spr, LOAD_SPR_DRAW, 0x8B + i * 12, 0x151, 0x99 + i * 12, 0x16D, 0x20, 0x1C, 0x30, 0x38, 2, 0x80, 0x80, 0x80, 0x80);
        }
        SPRITE_END(spr);
        memset(&gLoadScreen.pulse, 0, sizeof(LoadPulse));
        *(LoadRect *)gLoadScreen.pulse.rect = *(LoadRect *)&gLoadScreen.sprites[4].x0;
        gLoadScreen.pulse.amp = 0.5f;
        gLoadScreen.pulse.speed = 0.2f;
        break;
    case LOAD_TYPE_2:
        spr = gLoadScreen.sprites;
        SPRITE(spr, LOAD_SPR_CLEAR, 0, 0, 0x200, 0x1C0, 0, 0, 0, 0, 0, 0x1F, 0x52, 0x18, 0x80);
        SPRITE_END(spr);
        SPRITE(spr, LOAD_SPR_DRAW, 0, 0, 0x40, 0x40, 0x40, 0x40, 0x80, 0x80, 1, 0x80, 0x80, 0x80, 0x80);
        SPRITE_END(spr);
        SPRITE(spr, LOAD_SPR_DRAW, 0, 0, 0x40, 0x40, 0x40, 0, 0x80, 0x40, 1, 0x80, 0x80, 0x80, 0x80);
        SPRITE_END(spr);
        SPRITE(spr, LOAD_SPR_DRAW, 0, 0, 0x40, 0x80, 0, 0, 0x40, 0x80, 0, 0x80, 0x80, 0x80, 0x80);
        SPRITE_END(spr);
        memset(gLoadScreen.items, 0, sizeof(gLoadScreen.items));
        for (i = 0; i < LOAD_ITEM_COUNT; i++) {
            gLoadScreen.items[i * LOAD_ITEM_WORDS + LOAD_ITEM_X] = Rand_IntRange(0x10, 0x19C);
            gLoadScreen.items[i * LOAD_ITEM_WORDS + LOAD_ITEM_Y] = Rand_IntRange(0x30, 0x160);
        }
        gLoadScreen.target = Rand_Range(LOAD_ITEM_COUNT);
        break;
    }
}

/* Picks a screen type different from the last one, loads and unpacks its sprite sheet and sets up its timing. */
void Load_InitScreen(void) {
    memset(&gLoadScreen, 0, sizeof(LoadScreen));
    do {
        gLoadScreen.type = Rand_Range(LOAD_TYPE_COUNT);
    } while (gLoadScreen.type + 1 == gProgress->lastLoadType);
    /* (*&...): the original reloads gProgress after each of these two stores (see the note on LoadScreen). */
    (*&gProgress->lastLoadType) = gLoadScreen.type + 1;
    (*&gProgress->loadPack) = File_LoadSync(gLoadScreen.type + LOAD_FILE_FIRST, gProgress->loadPack, LOAD_PACK_MAX);
    Sprite_Unpack(gProgress->loadPack, gProgress->loadRes, NULL);
    gLoadScreen.res = gProgress->loadRes;
    Res_RelocateOffsets(&gLoadScreen.res, gLoadScreen.res, gLoadScreen.res);
    switch (gLoadScreen.type) {
    case LOAD_TYPE_0:
        gLoadScreen.period = 12;
        gLoadScreen.presses = 1;
        gLoadScreen.cycles = 3;
        break;
    case LOAD_TYPE_1:
        gLoadScreen.period = 0;
        gLoadScreen.presses = 2;
        gLoadScreen.cycles = 1;
        break;
    case LOAD_TYPE_2:
        gLoadScreen.period = 12;
        gLoadScreen.presses = 5;
        gLoadScreen.cycles = 5;
        break;
    }
    Load_BuildSprites();
}

/* Advances the animation of the current screen type by one frame and keeps the score. */
void Load_UpdateScreen(void) {
    switch (gLoadScreen.type) {
    case LOAD_TYPE_0:
        if (gLoadScreen.period < gLoadScreen.timer || gLoadScreen.presses == 0) {
            gLoadScreen.presses = 1;
            gLoadScreen.frame ^= 1;
            gLoadScreen.timer = 0;
            if (gLoadScreen.frame == 0) {
                gLoadScreen.cycles--;
                if (gLoadScreen.cycles == 0) {
                    if (gLoadScreen.count < 100) {
                        gLoadScreen.count++;
                    }
                    gLoadScreen.cycles = 3;
                }
            }
        }
        break;
    case LOAD_TYPE_1:
        if (gLoadScreen.presses == 0) {
            gLoadScreen.frame ^= 1;
            gLoadScreen.presses = gLoadScreen.frame + 2;
            if (gLoadScreen.frame == 0) {
                gLoadScreen.cycles--;
                if (gLoadScreen.cycles == 0) {
                    if (gLoadScreen.count < 999) {
                        gLoadScreen.count++;
                    }
                    gLoadScreen.cycles = 1;
                }
            }
        }
        break;
    case LOAD_TYPE_2:
        if (gLoadScreen.period < gLoadScreen.timer || (gLoadScreen.flags & LOAD_FLAG_PRESSED)) {
            gLoadScreen.timer = 0;
            gLoadScreen.frame ^= 1;
            if (gLoadScreen.flags & LOAD_FLAG_TAKEN) {
                if (gLoadScreen.count < LOAD_ITEM_COUNT) {
                    do {
                        gLoadScreen.target = Rand_Range(LOAD_ITEM_COUNT);
                    } while (gLoadScreen.items[gLoadScreen.target * LOAD_ITEM_WORDS + LOAD_ITEM_FLAGS] & 1);
                } else {
                    gLoadScreen.count = 0;
                    Load_BuildSprites();
                }
                gLoadScreen.flags &= ~LOAD_FLAG_TAKEN;
            }
        }
        if (gLoadScreen.presses == 0) {
            if (gLoadScreen.frame == 1) {
                gLoadScreen.items[gLoadScreen.target * LOAD_ITEM_WORDS + LOAD_ITEM_FLAGS] |= 1;
                gLoadScreen.presses = 5;
                gLoadScreen.flags |= LOAD_FLAG_TAKEN;
                gLoadScreen.count++;
            }
        }
        break;
    }
    gLoadScreen.timer++;
}

/* Counts a button press from either pad towards the next animation step. */
void Load_ReadInput(void) {
    if (gLoadScreen.flags & LOAD_FLAG_PRESSED) {
        gLoadScreen.flags &= ~LOAD_FLAG_PRESSED;
    }
    if (gPad[0].gamePressed & LOAD_BUTTON) {
        gLoadScreen.presses--;
        gLoadScreen.flags |= LOAD_FLAG_PRESSED;
        if (gLoadScreen.presses < 0) {
            gLoadScreen.presses = 0;
        }
    }
    if (gPad[1].gamePressed & LOAD_BUTTON) {
        gLoadScreen.presses--;
        gLoadScreen.flags |= LOAD_FLAG_PRESSED;
        if (gLoadScreen.presses < 0) {
            gLoadScreen.presses = 0;
        }
    }
}

/* Item words as Load_DrawScreen reads them. The original reaches x and y through a pointer to gLoadScreen + 4
   (base "+4", offsets 0x20 and 0x24) and only the flags through gLoadScreen itself; a plain items[i * 3 + 1]
   folds the constants differently and does not match. */
#define LOAD_POS_VIEW ((LoadScreen *)&gLoadScreen.timer)
#define ITEM_FLAGS(i) (gLoadScreen.items[(i) * LOAD_ITEM_WORDS])
#define ITEM_X(i) (LOAD_POS_VIEW->items[(i) * LOAD_ITEM_WORDS])
#define ITEM_Y(i) (LOAD_POS_VIEW->items[(i) * LOAD_ITEM_WORDS + 1])

#ifdef PORT
/* Spread the items over the wider page, keeping the original left/right margins.
   Only the draw position changes: the stored positions and random sequence stay the same. */
static s32 Load_ItemDrawX(s32 x) {
    s32 aspect = Port_PageAspectMilli();
    s32 extra;

    if (aspect <= 1340) {
        return x;
    }
    extra = (512 * (aspect * 3 - 4000) + 2000) / 4000;
    return x + ((x - 0x10) * extra + (0x19C - 0x10) / 2) / (0x19C - 0x10);
}
#define ITEM_DRAW_X(i) Load_ItemDrawX(ITEM_X(i))
#else
#define ITEM_DRAW_X(i) ITEM_X(i)
#endif

/* Draws the loading screen for the current type. */
void Load_DrawScreen(void) {
    u8 digits[16];
    s32 i;

#ifdef PORT
    /* Preserve sprite proportions and the left margin, independently of the battle HUD layout. */
    Port_GsMarker(PORT_2D_LOADING);
#endif
    switch (gLoadScreen.type) {
    case LOAD_TYPE_0:
        Sprite_DrawList(NULL, 0, 0, gLoadScreen.sprites);
        gLoadScreen.sprites[2].u0 = gLoadScreen.frame << 6;
        gLoadScreen.sprites[2].u1 = gLoadScreen.sprites[2].u0 + 0x40;
        Sprite_DrawList(gLoadScreen.res, 0, 0, &gLoadScreen.sprites[2]);
        for (i = 0; i < 10; i++) {
            gLoadScreen.sprites[5 + i].flags = 0;
        }
        for (i = 0; i < gLoadScreen.count % 10; i++) {
            gLoadScreen.sprites[5 + i].flags = LOAD_SPR_DRAW;
        }
        Sprite_DrawList(gLoadScreen.res, 0, 0, &gLoadScreen.sprites[5]);
        for (i = 0; i < 10; i++) {
            gLoadScreen.sprites[5 + i].flags = LOAD_SPR_DRAW;
        }
        for (i = 0; i < gLoadScreen.count / 10; i++) {
            Sprite_DrawList(gLoadScreen.res, 30 + i * 25, 60, &gLoadScreen.sprites[5]);
        }
        break;
    case LOAD_TYPE_1:
        Sprite_DrawList(NULL, 0, 0, gLoadScreen.sprites);
        gLoadScreen.sprites[2].tex = gLoadScreen.frame;
        Sprite_DrawList(gLoadScreen.res, 0, 0, &gLoadScreen.sprites[2]);
        gLoadScreen.pulse.size += Mathf_Sin(gLoadScreen.pulse.angle) * gLoadScreen.pulse.amp;
        gLoadScreen.pulse.angle += gLoadScreen.pulse.speed;
        /* The compiler truncates float literals: these give 0x40490FDA and 0x40C90FDA, as in the original .lit4. */
        if (gLoadScreen.pulse.angle >= 3.14159265f) {
            gLoadScreen.pulse.angle -= 6.2831853f;
        }
        gLoadScreen.sprites[4].y0 = gLoadScreen.pulse.rect[1] - (s32)gLoadScreen.pulse.size;
        gLoadScreen.sprites[4].y1 = gLoadScreen.pulse.rect[3] + (s32)gLoadScreen.pulse.size;
        gLoadScreen.sprites[4].x0 = gLoadScreen.pulse.rect[0] - (s32)gLoadScreen.pulse.size;
        gLoadScreen.sprites[4].x1 = gLoadScreen.pulse.rect[2] + (s32)gLoadScreen.pulse.size;
        Sprite_DrawList(gLoadScreen.res, 0, 0, &gLoadScreen.sprites[4]);
        if (gLoadScreen.count != 0) {
            Num_ToDigits(digits, gLoadScreen.count, 3, 0);
            for (i = 0; i < 3; i++) {
                LoadSprite *spr = &gLoadScreen.sprites[6 + i];

                spr->flags = digits[i] != 10;
                spr->v0 = (digits[i] >> 3) * 28;
                spr->v1 = spr->v0 + 28;
                spr->u0 = (digits[i] & 7) << 4;
                spr->u1 = spr->u0 + 16;
            }
            switch (Num_CountDigits(gLoadScreen.count)) {
            default:
                Sprite_DrawList(gLoadScreen.res, 0, 0, &gLoadScreen.sprites[6]);
                break;
            case 1:
                Sprite_DrawList(gLoadScreen.res, -14, 0, &gLoadScreen.sprites[6]);
                break;
            case 2:
                Sprite_DrawList(gLoadScreen.res, -7, 0, &gLoadScreen.sprites[6]);
                break;
            }
        } else {
            Sprite_DrawList(gLoadScreen.res, 0, 0, &gLoadScreen.sprites[6]);
        }
        break;
    case LOAD_TYPE_2:
        Sprite_DrawList(NULL, 0, 0, gLoadScreen.sprites);
        for (i = 0; i < LOAD_ITEM_COUNT; i++) {
            if (gLoadScreen.target != i && !(ITEM_FLAGS(i) & 1) &&
                ITEM_Y(i) + 0x20 <= ITEM_Y(gLoadScreen.target) + 0x40) {
                Sprite_DrawList(gLoadScreen.res, ITEM_DRAW_X(i), ITEM_Y(i), &gLoadScreen.sprites[4]);
            }
        }
        if (gLoadScreen.flags & LOAD_FLAG_TAKEN) {
            gLoadScreen.sprites[6].u0 = 0;
            gLoadScreen.sprites[6].u1 = gLoadScreen.sprites[6].u0 + 0x40;
            gLoadScreen.sprites[6].tex = 1;
            Sprite_DrawList(gLoadScreen.res, ITEM_DRAW_X(gLoadScreen.target), ITEM_Y(gLoadScreen.target), &gLoadScreen.sprites[2]);
        } else {
            gLoadScreen.sprites[6].u0 = gLoadScreen.frame << 6;
            gLoadScreen.sprites[6].u1 = gLoadScreen.sprites[6].u0 + 0x40;
            gLoadScreen.sprites[6].tex = 0;
        }
        Sprite_DrawList(gLoadScreen.res, ITEM_DRAW_X(gLoadScreen.target), ITEM_Y(gLoadScreen.target) - 0x40, &gLoadScreen.sprites[6]);
        for (i = 0; i < LOAD_ITEM_COUNT; i++) {
            if (gLoadScreen.target != i && !(ITEM_FLAGS(i) & 1) &&
                ITEM_Y(i) + 0x20 > ITEM_Y(gLoadScreen.target) + 0x40) {
                Sprite_DrawList(gLoadScreen.res, ITEM_DRAW_X(i), ITEM_Y(i), &gLoadScreen.sprites[4]);
            }
        }
        break;
    }
#ifdef PORT
    Port_GsMarker(PORT_2D_END);
#endif
}

/* Shows the loading screen until the file request queue has been read. No callers. */
void Load_RunFileQueue(void) {
    PadWatch_SetEnabled(0);
    Dma_ResetBuffers();
    Load_InitScreen();
    for (;;) {
        Gfx_BeginFrame();
        Pad_Update();
        Load_UpdateScreen();
        Load_DrawScreen();
        Gfx_EndFrame(2);
        Dma_Flush();
        File_Stub264D90();
        gLoadScreen.frames++;
        if (File_UpdateRequests()) {
            break;
        }
        Load_ReadInput();
    }
    Dma_ResetBuffers();
    PadWatch_SetEnabled(1);
}
