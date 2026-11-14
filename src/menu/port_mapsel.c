#include "common.h"
#include "menu/port_mapsel.h"
#include "menu/menu_a.h"
#include "menu/port_screen.h"
#include "sys/loading.h"
#include "plat_stages.h"
#include "plat_songs.h"

#ifdef PORT
#include <stdio.h>
#include <string.h>

typedef struct MsSprite {
    s32 flags, x0, y0, x1, y1, u0, v0, u1, v1, tex;
    s32 r, g, b, a;
} MsSprite;

extern void Sprite_DrawList(void *res, s32 x, s32 y, MsSprite *list);
extern void Sprite_DrawPicture(void *res, s32 x, s32 y, s32 alpha);
extern int PortGame_IsBT4(void);

#define MS_COLS 6
#define MS_LOCKED 0x3E
#define MS_EMPTY 0x3F
#define MS_RANDOM 0x23
#define MS_MUTE_ON 0x22
#define MS_WHOLE_ON 0x24
#define MS_WHOLE_OFF 0x25
extern void Port_GsMarker(s32 effect);

static MsView sView;
static s32 sOn, sMapPhase;

static u32 MsColor(u8 r, u8 g, u8 b, u8 a) {
    return (u32)r | ((u32)g << 8) | ((u32)b << 16) | ((u32)a << 24);
}

static void MsQuad(f32 x0, f32 y0, f32 x1, f32 y1, u32 top, u32 bottom) {
    Ps_ScreenQuad(x0, y0, x1, y0, x0, y1, x1, y1, top, top, bottom, bottom);
}

static void MsFrame(f32 x0, f32 y0, f32 x1, f32 y1, f32 t, u32 color) {
    MsQuad(x0, y0, x1, y0 + t, color, color);
    MsQuad(x0, y1 - t, x1, y1, color, color);
    MsQuad(x0, y0, x0 + t, y1, color, color);
    MsQuad(x1 - t, y0, x1, y1, color, color);
}

/* One textured quad; UVs are source texels and xy is the destination in 512x448 menu units. */
static void MsTex(MTexRes *res, s32 texture, f32 x0, f32 y0, f32 x1, f32 y1,
                  s32 u0, s32 v0, s32 u1, s32 v1, s32 tint, s32 alpha) {
    MsSprite spr[2];
    if (res == NULL || res->tex == NULL) return;
    spr[0].flags = LOAD_SPR_DRAW;
    spr[0].x0 = (s32)x0; spr[0].y0 = (s32)y0;
    spr[0].x1 = (s32)x1; spr[0].y1 = (s32)y1;
    spr[0].u0 = u0; spr[0].v0 = v0; spr[0].u1 = u1; spr[0].v1 = v1;
    spr[0].tex = texture;
    spr[0].r = spr[0].g = spr[0].b = tint; spr[0].a = alpha;
    spr[1].flags = LOAD_SPR_END; spr[1].a = 0;
    Sprite_DrawList(res, 0, 0, spr);
}

static void MsText(f32 x, f32 y, const char *text, f32 scale, f32 width, s32 align, u32 color) {
    Ps_Align(align);
    Ps_TextExOuter(x, y, (char *)text, scale, width, color, color,
                   MsColor(5, 10, 28, 0xD0), MsColor(5, 10, 28, 0x80), 2.0f);
    Ps_Align(0);
}

void PortMapSel_Begin(s32 allow) {
    extern char *getenv(const char *name);
    extern int atoi(const char *text);
    extern int Port_Setting(const char *name, int def);
    const char *env = getenv("BT3_SELECT_CLASSIC");

    sOn = allow && !(env != NULL ? atoi(env) : Port_Setting("select_classic", 0));
    sMapPhase = 0;
}

void PortMapSel_MuteOn(void) {
    Port_GsMarker(MS_MUTE_ON);
}

s32 PortMapSel_Shown(s32 mapPhase) {
    return sOn && mapPhase;
}

void PortMapSel_Frame(const MsView *view, s32 mapPhase) {
    if (view != NULL) sView = *view;
    sMapPhase = mapPhase != 0;
}

static MTexRes *MsStageIcon(s32 id, s32 *index) {
    s32 icon;
    if (id == MS_LOCKED) icon = 37;
    else if (id == MS_EMPTY) icon = 38;
    else if (id >= 0x24 && !PortGame_IsBT4()) icon = 1 + (id - 0x24) % 36;
    else icon = id + 1;
    *index = 0;
    return (MTexRes *)MPACK_AT(sView.stagePack, icon);
}

static const char *MsMapName(char *buf, size_t cap) {
    s32 id = sView.stage;
    if (id >= 0x24 && id - 0x24 < gPortExtraStageCount) {
        return gPortStageNames[id - 0x24];
    }
    snprintf(buf, cap, "%s", id == MS_RANDOM ? "Random stage" : "Stage");
    if (id != MS_RANDOM) snprintf(buf + strlen(buf), cap - strlen(buf), " %02d", (int)id + 1);
    return buf;
}

static const char *MsSongName(char *buf, size_t cap) {
    s32 id = sView.bgmId;
    if (id >= PORT_SONG_FIRST_OFFSET && id < PORT_SONG_FIRST_OFFSET + gPortSongCount) {
        return gPortSongNames[id - PORT_SONG_FIRST_OFFSET];
    }
    if (id == 0x18) return "Random music";
    snprintf(buf, cap, "Battle music %02d", (int)id + 1);
    return buf;
}

void PortMapSel_Draw(void) {
    s32 r, c, id, icon, selected;
    s32 top = sView.windowRow;
    char mapBuf[40], songBuf[64];
    const char *mapName = MsMapName(mapBuf, sizeof(mapBuf));
    const char *songName = MsSongName(songBuf, sizeof(songBuf));
    s32 musicId = sView.bgmId;

    Ps_PageSet();
    Port_GsMarker(0x23); /* end suppression of the original menu display list */
    Port_GsMarker(MS_WHOLE_ON);
    if (sView.preview != NULL) Sprite_DrawPicture(sView.preview, 0, 0, 0x80);

    /* The disc's names are already a texture atlas: four 512x64 strips per texture. */
    if (sView.stageNames != NULL && sView.stage < 0x24) {
        MsTex(sView.stageNames, sView.stage / 4, 112, 23, 350, 49,
              0, (sView.stage % 4) * 64, 512, (sView.stage % 4 + 1) * 64, 0x80, 0xFF);
        mapName = NULL;
    }

    /* Translucent title and three-row map window over the full-size preview. */
    MsQuad(14, 12, 352, 56, MsColor(5, 18, 52, 0xD0), MsColor(8, 38, 86, 0xB8));
    MsFrame(14, 12, 352, 56, 2, MsColor(105, 205, 255, 0xE0));
    if (mapName != NULL) MsText(27, 19, mapName, 0.9f, 320, 0, MsColor(255, 235, 175, 0xFF));

    MsQuad(12, 67, 354, 286, MsColor(5, 12, 34, 0xD8), MsColor(6, 22, 55, 0xD8));
    MsFrame(12, 67, 354, 286, 2, MsColor(90, 177, 238, 0xE0));
    for (r = 0; r < 3; r++) for (c = 0; c < MS_COLS; c++) {
        s32 idx = (top + r) * MS_COLS + c;
        id = idx < sView.stageCount ? sView.windowIds[r * MS_COLS + c] : MS_EMPTY;
        selected = (top + r == sView.row && c == sView.col);
        {
            f32 x = 20.0f + c * 54.0f, y = 76.0f + r * 67.0f;
            MTexRes *res = MsStageIcon(id, &icon);
            if (selected) {
                MsQuad(x - 2, y - 2, x + 49, y + 58, MsColor(255, 219, 96, 0x78), MsColor(255, 164, 34, 0x28));
                MsFrame(x - 2, y - 2, x + 49, y + 58, 2, MsColor(255, 224, 125, 0xFF));
            }
            MsTex(res, icon, x, y, x + 45, y + 54, 0, 0, 64, 64, 0x80, 0xFF);
        }
    }

    /* Show the native music strip even while the game cursor is on a map cell; arrows mirror its music column. */
    MsQuad(14, 304, 498, 362, MsColor(5, 18, 52, 0xE0), MsColor(8, 38, 86, 0xD0));
    MsFrame(14, 304, 498, 362, 2, sView.col == 6 ? MsColor(255, 210, 90, 0xFF) : MsColor(95, 190, 250, 0xE0));
    MsText(34, 316, "<", 1.0f, 20, 1, MsColor(255, 236, 175, 0xFF));
    MsText(478, 316, ">", 1.0f, 20, 1, MsColor(255, 236, 175, 0xFF));
    if (sView.songNames != NULL && musicId >= 0 && musicId < PORT_SONG_FIRST_OFFSET) {
        MsTex(sView.songNames, musicId / 8, 60, 316, 452, 346,
              0, (musicId % 8) * 32, 512, (musicId % 8 + 1) * 32, 0x80, 0xFF);
    } else {
        MsText(256, 315, songName, 0.72f, 390, 1, MsColor(235, 244, 255, 0xFF));
    }
    MsText(18, 418, "CONFIRM  /  MUSIC", 0.48f, 210, 0, MsColor(255, 255, 255, 0xE0));
    MsText(494, 418, "BACK", 0.48f, 0, 2, MsColor(255, 255, 255, 0xE0));
    Font_FlushAll();
    Port_GsMarker(MS_WHOLE_OFF);
}

void PortMapSel_End(void) {
    sOn = 0;
    sMapPhase = 0;
    memset(&sView, 0, sizeof(sView));
}
#endif
