/* A spectator's own pause menu (START or Esc while watching a fight): the battle's own pause menu, drawn by its own
   engine (src/battle/hud_0_b.c, BtlMenu_Draw: the box from the battle's sprite sheet, the items in its font and
   colours, the box growing and shrinking over six frames, the yes / no box before leaving) with a menu of ours:
   a note, Continue, View (left / right), Back to the Lobby (the lobby's owner) and Leave / Close the Lobby or Stop
   Watching. Only on this screen: the fight goes on behind it, and nothing of the battle's own menus is touched (the
   engine's update is not run: this file moves the cursor and the box itself; its texts come to the engine through
   BtlMenu_GetItemText, hud_0_b.c, for the numbers PORT_SPEC_TEXT and on).

   Called once a frame from the game's Gfx_EndFrame through PortTbo_SpecDraw (src/menu/port_tbo.c). Only in a fight
   (the battle's menu work and sprite sheet exist then). The pad is read here (Port_PadRead: the spectator's own pad
   plays nobody); Esc reaches it from the window's events (gPortSpecMenuAsk, gs/ui.cpp). Its lines are translated by
   the online menus' file (Port_TrLine; listed by port/tools/tbo_strings.py). This file's variables are not the
   game's state; what it draws goes through the game's draw lists and font, as the spectator's line does. */
#include "types.h"
#include "sys/pad.h"
#include "battle/hud_0.h"
#include "gs/net_lobby.h"
#include <string.h>

extern s32 Snd_PlaySe(u32 mask, s32 id);
extern void BtlMenu_PortDrawMenu(BtlMenu *menu); /* src/battle/hud_0_b.c */
extern void BtlMenu_PortSize(BtlMenu *menu, s32 *w, s32 *h); /* src/battle/hud_0_b.c */
extern s32 BtlMenu_PortLive(void);
extern BtlMenu *PauseMenu_PortConfirmBox(void); /* src/battle/hud_0.c */
extern BtlMenu gPauseMenuVersus;
extern int Port_NetSpectating(void);
extern int Port_NetSpecView(void);
extern void Port_NetSpecViewSet(int view);
extern void Port_NetSpecLeave(void);
extern int Port_PadRead(int socket, unsigned char *data);
extern int Port_TrLine(const char *original, char *out, int size);
extern void PortA11y_Say(const char *text, int flags);
extern volatile int gPortSpecMenuAsk;

#define PORT_SPEC_TEXT 0x7000 /* an item's text number from here on is one of ours (hud_0_b.c) */
enum { IT_NOTE, IT_CONTINUE, IT_VIEW, IT_BACK, IT_LEAVE, IT_YES, IT_NO };

static BtlMenuItem sItem[5], sSureItem[3];
static int sYes = 0; /* the yes / no box's Yes (No is the next) */
static BtlMenu sMain, sSure;
static int sKind[5], sCount, sOpen, sClosing, sLeaveAfter, sBackAfter, sSaid = -1;
static u16 sText[9][96]; /* (8: a lobby's "Back to the Lobby" in the players' menus) */
static PortLobbyView sView;
static unsigned sHeld;
static int sRepeat;

static const char *tr(const char *english, char *buf, int size) {
    if (Port_TrLine(english, buf, size)) {
        return buf;
    }
    return english;
}

static const char *view_name(int view) {
    switch (view) {
    case 1:
        return "Player 2";
    case 2:
        return "Split Screen";
    }
    return "Player 1";
}

static const char *leave_name(void) {
    if (sView.state != 2) {
        return "Stop Watching";
    }
    if (sView.owner) {
        return "Close the Lobby";
    }
    return "Leave the Lobby";
}

/* An item's line, in the chosen language. */
static void item_line(int kind, char *out, int size) {
    char a[96], b[64];
    out[0] = 0;
    switch (kind) {
    case IT_NOTE:
        strncpy(out, tr("Your screen only: the fight goes on", a, sizeof(a)), (size_t)size - 1);
        break;
    case IT_CONTINUE:
        strncpy(out, tr("Continue", a, sizeof(a)), (size_t)size - 1);
        break;
    case IT_VIEW:
        strncpy(out, tr("View: ", a, sizeof(a)), (size_t)size - 1);
        out[size - 1] = 0;
        strncat(out, tr(view_name(Port_NetSpecView() % 3), b, sizeof(b)), (size_t)size - strlen(out) - 1);
        break;
    case IT_BACK:
        strncpy(out, tr("Back to the Lobby", a, sizeof(a)), (size_t)size - 1);
        break;
    case IT_LEAVE:
        strncpy(out, tr(leave_name(), a, sizeof(a)), (size_t)size - 1);
        break;
    case IT_YES:
        strncpy(out, tr("Yes", a, sizeof(a)), (size_t)size - 1);
        break;
    default:
        strncpy(out, tr("No", a, sizeof(a)), (size_t)size - 1);
        break;
    }
    out[size - 1] = 0;
}

/* UTF-8 into the engine's 16-bit text. */
static void put_text(u16 *dst, int cap, const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    int n = 0;
    while (*p && n < cap - 1) {
        unsigned c = *p++;
        if (c >= 0xC0 && c < 0xE0 && *p) {
            c = ((c & 0x1F) << 6) | (*p++ & 0x3F);
        } else if (c >= 0xE0 && c < 0xF0 && p[0] && p[1]) {
            c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
            p += 2;
        }
        dst[n++] = (u16)(c == '<' ? '(' : c == '>' ? ')' : c);
    }
    dst[n] = 0;
}

/* The engine asks for our item k's text (hud_0_b.c, BtlMenu_GetItemText). */
u16 *PortSpecMenu_Text(s32 k) {
    if (k == 8) { /* a lobby's match: the owner's way back in the players' pause and result menus (src/battle/hud_0.c) */
        char line[96];
        item_line(IT_BACK, line, sizeof(line));
        put_text(sText[8], 96, line);
    }
    if (k < 0 || k >= 9) {
        k = 0;
    }
    return sText[k];
}

static void item_set(BtlMenuItem *item, int slot, int kind, int disabled) {
    char line[160];
    memset(item, 0, sizeof(*item));
    item->text[0] = item->text[1] = (s16)(kind == IT_NOTE ? 0x6FFF : PORT_SPEC_TEXT + slot); /* (the note: the box's header row) */
    item->id = kind;
    item->type = BTL_MENU_ITEM_RESULT;
    item->disabled = disabled;
    item_line(kind, line, sizeof(line));
    put_text(sText[kind == IT_NOTE ? 7 : slot], 96, line);
}

/* The menus as they are now (the view's name changes; the owner gets Back to the Lobby). The box's look (scale,
   margins) is the battle's own pause menu's. */
static void menus_make(void) {
    int i, cursor = sMain.cursor;
    sCount = 0;
    sKind[sCount++] = IT_NOTE;
    sKind[sCount++] = IT_CONTINUE;
    sKind[sCount++] = IT_VIEW;
    if (sView.state == 2 && sView.owner) {
        sKind[sCount++] = IT_BACK;
    }
    sKind[sCount++] = IT_LEAVE;
    for (i = 0; i < sCount; i++) {
        item_set(&sItem[i], i, sKind[i], sKind[i] == IT_NOTE);
    }
    {   /* the yes / no box: the battle's own ("EXIT ?", Yes, No), else one of ours */
        BtlMenu *game = PauseMenu_PortConfirmBox();
        if (game != NULL) {
            int k;
            for (k = 0; k < 3; k++) {
                sSureItem[k] = game->items[k];
                sSureItem[k].sub = NULL;
                sSureItem[k].func = NULL;
                sSureItem[k].type = BTL_MENU_ITEM_RESULT;
                sSureItem[k].disabled = k == 0;
            }
            sSure.count = 3;
            sYes = 1;
        } else {
            item_set(&sSureItem[0], 5, IT_YES, 0);
            item_set(&sSureItem[1], 6, IT_NO, 0);
            sSure.count = 2;
            sYes = 0;
        }
    }
    for (i = 0; i < sCount; i++) {
        if (sKind[i] == IT_LEAVE) {
            sItem[i].sub = &sSure;
        }
    }
    sMain.items = sItem;
    sMain.count = sCount;
    sSure.items = sSureItem;
    sMain.scale = sSure.scale = gPauseMenuVersus.scale > 0.0f ? gPauseMenuVersus.scale : 1.0f;
    sMain.padX = sSure.padX = gPauseMenuVersus.padX;
    sMain.padY = sSure.padY = gPauseMenuVersus.padY;
    if (cursor < 1 || cursor >= sCount) {
        cursor = 1;
    }
    sMain.cursor = cursor;
}

/* The box grows from `from` (w, h) to its own size over six frames, or shrinks back; as the engine does. */
static void box_start(BtlMenu *m, f32 fw, f32 fh, int closing) {
    s32 iw, ih;
    f32 w, h;
    BtlMenu_PortSize(m, &iw, &ih);
    w = (f32)iw;
    h = (f32)ih;
    m->frame = 0;
    if (!closing) {
        m->state = BTL_MENU_STATE_OPENING;
        m->w = fw;
        m->h = fh;
        m->dw = (w - fw) / 6.0f;
        m->dh = (h - fh) / 6.0f;
        Snd_PlaySe(1, 4);
    } else {
        m->state = BTL_MENU_STATE_CLOSING;
        m->w = w;
        m->h = h;
        m->dw = (fw - w) / 6.0f;
        m->dh = (fh - h) / 6.0f;
        Snd_PlaySe(1, 5);
    }
}

/* One frame of the box's animation; 1 when a closing box has gone. */
static int box_step(BtlMenu *m) {
    if (m->state != BTL_MENU_STATE_OPENING && m->state != BTL_MENU_STATE_CLOSING) {
        return 0;
    }
    m->w += m->dw;
    m->h += m->dh;
    if (++m->frame >= 6) {
        s32 iw, ih;
        if (m->state == BTL_MENU_STATE_CLOSING) {
            return 1;
        }
        BtlMenu_PortSize(m, &iw, &ih);
        m->w = (f32)iw;
        m->h = (f32)ih;
        m->state = BTL_MENU_STATE_ACTIVE;
    }
    return 0;
}

/* The PC's pad this frame: new presses, and the directions again while held. */
#define B_START 0x0008
#define B_UP 0x0010
#define B_RIGHT 0x0020
#define B_DOWN 0x0040
#define B_LEFT 0x0080
#define B_TRIANGLE 0x1000
#define B_CIRCLE 0x2000
#define B_CROSS 0x4000
#define B_DIRS (B_UP | B_RIGHT | B_DOWN | B_LEFT)
static unsigned pad(int vsyncs) {
    unsigned char d[18];
    unsigned held, pressed;
    memset(d, 0, sizeof(d));
    d[0] = d[1] = 0xFF;
    d[2] = d[3] = d[4] = d[5] = 0x80;
    Port_PadRead(0, d);
    held = ~((unsigned)d[0] | ((unsigned)d[1] << 8)) & 0xFFFF;
    if (d[4] < 0x40) { held |= B_LEFT; } else if (d[4] > 0xC0) { held |= B_RIGHT; }
    if (d[5] < 0x40) { held |= B_UP; } else if (d[5] > 0xC0) { held |= B_DOWN; }
    pressed = held & ~sHeld;
    if ((held & B_DIRS) != 0 && (held & B_DIRS) == (sHeld & B_DIRS)) {
        sRepeat += vsyncs;
        if (sRepeat >= 24) {
            sRepeat -= 6;
            pressed |= held & B_DIRS;
        }
    } else {
        sRepeat = 0;
    }
    sHeld = held;
    return pressed;
}

static void say(void) {
    char line[200], item[160];
    int sure = sMain.open >= 0;
    int what = !sOpen ? -1 : sure ? 100 + sSure.cursor - sYes : sKind[sMain.cursor] * 4 + Port_NetSpecView() % 3;
    if (what == sSaid) {
        return;
    }
    line[0] = 0;
    if (what >= 0) {
        if (sSaid < 0) {
            item_line(IT_NOTE, line, sizeof(line));
            strcat(line, ". ");
        }
        if (sure) {
            item_line(IT_LEAVE, item, sizeof(item));
            strncat(line, item, sizeof(line) - strlen(line) - 3);
            strcat(line, "? ");
            item_line(sSure.cursor == sYes ? IT_YES : IT_NO, item, sizeof(item));
        } else {
            item_line(sKind[sMain.cursor], item, sizeof(item));
        }
        strncat(line, item, sizeof(line) - strlen(line) - 1);
        PortA11y_Say(line, 1);
    }
    sSaid = what;
}

/* The yes / no box grows from the menu's box, and shrinks back to it. */
static void main_box(BtlMenu *m, int closing) {
    s32 w, h;
    BtlMenu_PortSize(&sMain, &w, &h);
    box_start(m, (f32)w, (f32)h, closing);
}

static void close_menu(void) {
    if (sMain.open >= 0) {
        sMain.open = -1;
        sSure.state = BTL_MENU_STATE_OPEN;
    }
    box_start(&sMain, 0.0f, 0.0f, 1);
    sClosing = 1;
}

static int frame(int vsyncs);

/* Once a frame (src/menu/port_tbo.c, PortTbo_SpecDraw). Returns 1 while the menu is up. */
int PortSpecMenu_Frame(int vsyncs) {
    if (!Port_NetSpectating() || !BtlMenu_PortLive()) {
        sOpen = 0;
        sSaid = -1;
        gPortSpecMenuAsk = 0;
        sHeld = 0;
        return 0;
    }
    return frame(vsyncs);
}

static int frame(int vsyncs) {
    unsigned p;
    int ask;

    vsyncs = vsyncs < 1 ? 1 : vsyncs > 4 ? 4 : vsyncs;
    Port_LobbyView(&sView);
    p = pad(vsyncs);
    ask = gPortSpecMenuAsk;
    gPortSpecMenuAsk = 0;
    if (!sOpen) {
        if (!(p & B_START) && !ask) {
            return 0;
        }
        sOpen = 1;
        sClosing = 0;
        sLeaveAfter = sBackAfter = 0;
        sMain.cursor = 1;
        sMain.open = -1;
        menus_make();
        box_start(&sMain, 0.0f, 0.0f, 0);
        p = 0;
        ask = 0;
    }
    menus_make();
    if (sClosing) {
        if (box_step(&sMain)) {
            sOpen = 0;
            sSaid = -1;
            if (sBackAfter) {
                Port_LobbyDo(LOBBY_BACK, 0, 0);
            }
            if (sLeaveAfter) {
                if (sView.state == 2) {
                    Port_LobbyDo(LOBBY_LEAVE, 0, 0);
                } else {
                    Port_NetSpecLeave();
                }
            }
            return 0;
        }
    } else if (sMain.open >= 0) { /* the yes / no box */
        if (sSure.state == BTL_MENU_STATE_CLOSING) {
            if (box_step(&sSure)) {
                sMain.open = -1;
                sSure.state = BTL_MENU_STATE_OPEN;
            }
        } else if (sSure.state != BTL_MENU_STATE_ACTIVE) {
            box_step(&sSure);
        } else if (p & B_DIRS) {
            sSure.cursor = sSure.cursor == sYes ? sYes + 1 : sYes;
            Snd_PlaySe(1, 0);
        } else if ((p & B_CROSS) && sSure.cursor == sYes) {
            Snd_PlaySe(1, 1);
            sLeaveAfter = 1;
            close_menu();
        } else if ((p & (B_CROSS | B_TRIANGLE | B_CIRCLE)) || ((p & B_START) || ask)) {
            main_box(&sSure, 1);
        }
    } else if (sMain.state != BTL_MENU_STATE_ACTIVE) {
        box_step(&sMain);
    } else if (p & B_DOWN) {
        sMain.cursor = sMain.cursor + 1 >= sCount ? 1 : sMain.cursor + 1; /* (the note is not chosen) */
        Snd_PlaySe(1, 0);
    } else if (p & B_UP) {
        sMain.cursor = sMain.cursor - 1 < 1 ? sCount - 1 : sMain.cursor - 1;
        Snd_PlaySe(1, 0);
    } else if ((p & (B_LEFT | B_RIGHT)) && sKind[sMain.cursor] == IT_VIEW) {
        Port_NetSpecViewSet((Port_NetSpecView() + ((p & B_RIGHT) ? 1 : 2)) % 3);
        Snd_PlaySe(1, 0);
    } else if (p & B_CROSS) {
        switch (sKind[sMain.cursor]) {
        case IT_CONTINUE:
            Snd_PlaySe(1, 1);
            close_menu();
            break;
        case IT_VIEW:
            Port_NetSpecViewSet((Port_NetSpecView() + 1) % 3);
            Snd_PlaySe(1, 0);
            break;
        case IT_BACK:
            Snd_PlaySe(1, 1);
            sBackAfter = 1;
            close_menu();
            break;
        default: /* its yes / no box, No under the cursor */
            sMain.open = sMain.cursor;
            menus_make();
            sSure.cursor = sYes + 1; /* (No) */
            main_box(&sSure, 0);
            break;
        }
    } else if ((p & (B_TRIANGLE | B_CIRCLE | B_START)) || ask) {
        close_menu();
    }
    menus_make();
    BtlMenu_PortDrawMenu(&sMain); /* (drawn by the battle's own menu engine, as BtlMenu_DrawAll draws its menus) */
    say();
    return 1;
}
