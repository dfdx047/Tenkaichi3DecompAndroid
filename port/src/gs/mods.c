/*
 * Script mods (docs/port/mods.md): a folder `mods` next to the game (or the one $BT3_MODS names), one folder per mod
 * with a `mod.lua` in it. Each script runs in a Lua state of its own on the game's thread and returns a table:
 *
 *     return {
 *         name = "My mod", version = "1.0", author = "me", description = "...",
 *         settings = { { id = "size", label = "Size", type = "slider", min = 0, max = 100, default = 50 } },
 *         on_frame  = function(game) end,        -- once per blank (60 a second)
 *         on_hud    = function(hud, game) end,   -- draw over the game's picture
 *         on_camera = function(cam, game) end,   -- change the fight camera as it is shown
 *     }
 *
 * What a script can do is what this file gives it: read the fight (port/src/fight_panels.c, port/src/mods_game.c),
 * draw shapes and text over the picture (gs/mods_ui.cpp draws them), and change the pose of the camera that is
 * drawn. It cannot write the game's memory, draw the game's random numbers or reach a file: its Lua has no io, os,
 * package or debug library (gs/mods_lua.c). So mods change nothing of a fight: an online session stays in step with a
 * player who has none, and a replay plays the same. Blanks run again after a rollback are not given to the scripts.
 *
 * A script that fails (an error, more than its share of time or memory) is stopped and the reason shown in the
 * settings window's Mods tab, where each mod is switched on and off and its settings are changed. Those are kept in
 * bt3_mods.txt (or the file $BT3_MODS_SETTINGS names) as `folder=1` and `folder.setting=value` lines.
 *
 * Nothing in this file is game state (port/tools/make_state.py).
 */
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../third_party/lua/lua.h"
#include "../../third_party/lua/lauxlib.h"
#include "../../third_party/lua/lualib.h"
#include "fight_panels.h"
#include "mods.h"

extern unsigned gPortVBlanks;                     /* plat_stub.c */
extern volatile int gUiPresentW, gUiPresentH;     /* gs_draw.c: the game's picture in the window */
extern int Port_NetSession(void), Port_NetActive(void); /* gs/net.c */

#define MOD_MEMORY ((size_t)64 << 20)     /* a script's Lua memory */
#define MOD_FILE_MAX ((long)4 << 20)      /* the largest script file read */
#define HOOK_STEP 1000                    /* the count hook's period, in Lua instructions */
#define BUDGET_LOAD 200000                /* steps for loading a script (200 million instructions) */
#define BUDGET_CALL 20000                 /* steps for one call (20 million) */

typedef struct Mod {
    lua_State *L;
    size_t mem;
    long steps, budget;
    int table;      /* registry reference of the table the script returned */
    int game;       /* ... of the last blank's game table */
    int dead;
} Mod;

static Mod sMod[MODS_MAX];
static ModsInfo sInfo[MODS_MAX];
static int sCount;
static int sLoaded;
static pthread_mutex_t sLock = PTHREAD_MUTEX_INITIALIZER;
static volatile int sReload, sDirty;
static int sCurrent = -1;           /* the mod whose script is running (its HUD calls, its log lines) */

/* the HUD the scripts draw during a blank, and the copy the render thread reads */
static ModsHud sBuild, sHud;
static volatile unsigned sHudSeq;

void Mods_Lock(void) { pthread_mutex_lock(&sLock); }
void Mods_Unlock(void) { pthread_mutex_unlock(&sLock); }
int Mods_Count(void) { return sCount; }
ModsInfo *Mods_Info(int i) { return i >= 0 && i < sCount ? &sInfo[i] : NULL; }
void Mods_SettingChanged(void) { sDirty = 1; }
void Mods_ReloadAsk(void) { sReload = 1; }

const char *Mods_Folder(void) {
    const char *p = getenv("BT3_MODS");
    return p != NULL && p[0] != '\0' ? p : "mods";
}

static const char *settings_path(void) {
    const char *p = getenv("BT3_MODS_SETTINGS");
    return p == NULL ? "bt3_mods.txt" : p[0] == '\0' ? NULL : p;
}

static void say(int i, const char *fmt, ...) {
    va_list ap;

    fprintf(stderr, "bt3: mod %s: ", i >= 0 ? sInfo[i].folder : "?");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* A script stopped: why, in the window and in the log. */
static void fail(int i, const char *why) {
    Mods_Lock();
    snprintf(sInfo[i].error, sizeof(sInfo[i].error), "%s", why != NULL ? why : "error");
    Mods_Unlock();
    sMod[i].dead = 1;
    say(i, "stopped: %s", sInfo[i].error);
}

/* ------------------------------------------------------------------------------------------- memory and time */

static void *mod_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    Mod *m = (Mod *)ud;
    size_t old = ptr != NULL ? osize : 0;

    if (nsize == 0) {
        free(ptr);
        m->mem -= old;
        return NULL;
    }
    if (nsize > old && m->mem - old + nsize > MOD_MEMORY) {
        return NULL; /* (Lua raises "not enough memory") */
    }
    ptr = realloc(ptr, nsize);
    if (ptr != NULL) {
        m->mem = m->mem - old + nsize;
    }
    return ptr;
}

static void count_hook(lua_State *L, lua_Debug *ar) {
    Mod *m;

    (void)ar;
    lua_getallocf(L, (void **)&m);
    if (++m->steps > m->budget) {
        luaL_error(L, "the script took too long (an endless loop?)");
    }
}

/* Calls the function on the stack below its nargs arguments, with the time a call has. 0 = it failed. */
static int call(int i, int nargs, int nres) {
    Mod *m = &sMod[i];
    int prev = sCurrent;

    m->steps = 0;
    m->budget = BUDGET_CALL;
    sCurrent = i;
    if (lua_pcall(m->L, nargs, nres, 0) != LUA_OK) {
        sCurrent = prev;
        fail(i, lua_tostring(m->L, -1));
        lua_pop(m->L, 1);
        return 0;
    }
    sCurrent = prev;
    return 1;
}

/* --------------------------------------------------------------------------------------- what scripts are given */

static int l_print(lua_State *L) {
    char line[512];
    size_t at = 0;
    int n = lua_gettop(L), k;

    line[0] = '\0';
    for (k = 1; k <= n && at < sizeof(line) - 1; k++) {
        const char *s = luaL_tolstring(L, k, NULL);
        at += (size_t)snprintf(line + at, sizeof(line) - at, "%s%s", k > 1 ? " " : "", s);
        lua_pop(L, 1);
    }
    say(sCurrent, "%s", line);
    return 0;
}

static ModsSetting *find_setting(int i, const char *id) {
    int k;

    for (k = 0; k < sInfo[i].settings; k++) {
        if (strcmp(sInfo[i].set[k].id, id) == 0) {
            return &sInfo[i].set[k];
        }
    }
    return NULL;
}

/* bt3.setting(id): a toggle's boolean, a slider's number, a choice's position (1 = the first) */
static int l_setting(lua_State *L) {
    const char *id = luaL_checkstring(L, 1);
    int i = (int)lua_tointeger(L, lua_upvalueindex(1));
    ModsSetting *s = find_setting(i, id);

    if (s == NULL) {
        return luaL_error(L, "no setting \"%s\" (the table's settings list names them)", id);
    }
    if (s->kind == MODS_SET_TOGGLE) {
        lua_pushboolean(L, s->value != 0.0);
    } else if (s->kind == MODS_SET_CHOICE) {
        lua_pushinteger(L, (lua_Integer)s->value + 1);
    } else {
        lua_pushnumber(L, s->value);
    }
    return 1;
}

static int byte_of(lua_State *L, int k, lua_Number def) {
    lua_Number v = luaL_optnumber(L, k, def);
    return v < 0 ? 0 : v > 255 ? 255 : (int)v;
}

/* bt3.rgba(r, g, b [, a]): 0..255 each, the colour as the HUD calls take it (0xRRGGBBAA) */
static int l_rgba(lua_State *L) {
    unsigned c = (unsigned)byte_of(L, 1, 0) << 24 | (unsigned)byte_of(L, 2, 0) << 16 | (unsigned)byte_of(L, 3, 0) << 8 |
                 (unsigned)byte_of(L, 4, 255);
    lua_pushinteger(L, (lua_Integer)c);
    return 1;
}

/* require(name): another .lua file of the mod's folder ("a.b" = a/b.lua), run once, its result kept */
static int l_require(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    int i = (int)lua_tointeger(L, lua_upvalueindex(1));
    char path[512], rel[160];
    size_t k;

    for (k = 0; name[k] != '\0' && k < sizeof(rel) - 5; k++) {
        char c = name[k];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.')) {
            return luaL_error(L, "require: \"%s\": a module name is letters, digits, '_' and '.'", name);
        }
        rel[k] = c == '.' ? '/' : c;
    }
    rel[k] = '\0';
    lua_getfield(L, LUA_REGISTRYINDEX, "bt3_modules");
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);
    snprintf(path, sizeof(path), "%s/%s/%s.lua", Mods_Folder(), sInfo[i].folder, rel);
    {
        FILE *fp = fopen(path, "rb");
        long size;
        char *text;
        int r;

        if (fp == NULL) {
            return luaL_error(L, "require: no file %s", path);
        }
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (size < 0 || size > MOD_FILE_MAX || (text = (char *)malloc((size_t)size + 1)) == NULL) {
            fclose(fp);
            return luaL_error(L, "require: %s cannot be read", path);
        }
        size = (long)fread(text, 1, (size_t)size, fp);
        fclose(fp);
        lua_pushfstring(L, "@%s/%s.lua", sInfo[i].folder, rel);
        r = luaL_loadbufferx(L, text, (size_t)size, lua_tostring(L, -1), "t"); /* (text only: no compiled chunks) */
        free(text);
        lua_remove(L, -2);
        if (r != LUA_OK) {
            return lua_error(L);
        }
    }
    lua_call(L, 0, 1);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }
    lua_pushvalue(L, -1);
    lua_setfield(L, -3, name);
    return 1;
}

/* ---------------------------------------------------------------------------------------------------- the HUD */

static ModsCmd *hud_cmd(lua_State *L, int kind) {
    ModsCmd *c;

    if (sBuild.count >= MODS_HUD_CMDS) {
        return NULL; /* (the rest of this blank's shapes are not drawn) */
    }
    c = &sBuild.cmd[sBuild.count++];
    memset(c, 0, sizeof(*c));
    c->kind = (unsigned char)kind;
    (void)L;
    return c;
}

static unsigned colour(lua_State *L, int k) {
    return (unsigned)luaL_optinteger(L, k, 0xFFFFFFFF);
}

static int l_hud_rect(lua_State *L) {
    ModsCmd *c = hud_cmd(L, MODS_CMD_RECT);
    if (c != NULL) {
        c->a = (float)luaL_checknumber(L, 1); c->b = (float)luaL_checknumber(L, 2);
        c->c = (float)luaL_checknumber(L, 3); c->d = (float)luaL_checknumber(L, 4);
        c->color = colour(L, 5);
        c->e = (float)luaL_optnumber(L, 6, 1.0);
    }
    return 0;
}

static int l_hud_fill(lua_State *L) {
    ModsCmd *c = hud_cmd(L, MODS_CMD_FILL);
    if (c != NULL) {
        c->a = (float)luaL_checknumber(L, 1); c->b = (float)luaL_checknumber(L, 2);
        c->c = (float)luaL_checknumber(L, 3); c->d = (float)luaL_checknumber(L, 4);
        c->color = colour(L, 5);
        c->e = (float)luaL_optnumber(L, 6, 0.0);
    }
    return 0;
}

static int l_hud_line(lua_State *L) {
    ModsCmd *c = hud_cmd(L, MODS_CMD_LINE);
    if (c != NULL) {
        c->a = (float)luaL_checknumber(L, 1); c->b = (float)luaL_checknumber(L, 2);
        c->c = (float)luaL_checknumber(L, 3); c->d = (float)luaL_checknumber(L, 4);
        c->color = colour(L, 5);
        c->e = (float)luaL_optnumber(L, 6, 1.0);
    }
    return 0;
}

static int l_hud_circle(lua_State *L) {
    ModsCmd *c = hud_cmd(L, MODS_CMD_CIRCLE);
    if (c != NULL) {
        c->a = (float)luaL_checknumber(L, 1); c->b = (float)luaL_checknumber(L, 2);
        c->c = (float)luaL_checknumber(L, 3);
        c->color = colour(L, 4);
        c->e = (float)luaL_optnumber(L, 5, 1.0);
    }
    return 0;
}

static int l_hud_disc(lua_State *L) {
    ModsCmd *c = hud_cmd(L, MODS_CMD_DISC);
    if (c != NULL) {
        c->a = (float)luaL_checknumber(L, 1); c->b = (float)luaL_checknumber(L, 2);
        c->c = (float)luaL_checknumber(L, 3);
        c->color = colour(L, 4);
    }
    return 0;
}

/* hud.text(x, y, text [, colour [, size [, align]]]): size in HUD units (16 by default); align "left", "center",
   "right" (the point x is the text's left edge, middle or right edge) */
static int l_hud_text(lua_State *L) {
    size_t n;
    const char *s = luaL_checklstring(L, 3, &n);
    const char *align = luaL_optstring(L, 6, "left");
    ModsCmd *c;

    if (n > 1024) {
        n = 1024;
    }
    if (sBuild.chars + n > MODS_HUD_CHARS || (c = hud_cmd(L, MODS_CMD_TEXT)) == NULL) {
        return 0;
    }
    c->a = (float)luaL_checknumber(L, 1);
    c->b = (float)luaL_checknumber(L, 2);
    c->color = colour(L, 4);
    c->c = (float)luaL_optnumber(L, 5, 16.0);
    c->align = (unsigned char)(align[0] == 'c' ? 1 : align[0] == 'r' ? 2 : 0);
    c->at = sBuild.chars;
    c->text = (unsigned short)n;
    memcpy(sBuild.text + sBuild.chars, s, n);
    sBuild.chars += (unsigned)n;
    return 0;
}

static float hud_width(void) {
    int w = gUiPresentW, h = gUiPresentH;
    return w > 0 && h > 0 ? MODS_HUD_H * (float)w / (float)h : MODS_HUD_H * 4.0f / 3.0f;
}

static void push_hud(lua_State *L) {
    static const luaL_Reg fns[] = {{"rect", l_hud_rect}, {"fill", l_hud_fill}, {"line", l_hud_line}, {"circle", l_hud_circle},
                                   {"disc", l_hud_disc}, {"text", l_hud_text}, {NULL, NULL}};
    lua_getfield(L, LUA_REGISTRYINDEX, "bt3_hud");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        luaL_newlib(L, fns);
        lua_pushvalue(L, -1);
        lua_setfield(L, LUA_REGISTRYINDEX, "bt3_hud");
    }
    lua_pushnumber(L, hud_width());
    lua_setfield(L, -2, "w");
    lua_pushnumber(L, MODS_HUD_H);
    lua_setfield(L, -2, "h");
}

int Mods_HudRead(ModsHud *out) {
    unsigned a, b;
    int tries, n;

    for (tries = 0; tries < 64; tries++) {
        a = __atomic_load_n(&sHudSeq, __ATOMIC_ACQUIRE);
        if (a & 1) {
            continue;
        }
        out->w = sHud.w;
        n = sHud.count;
        out->chars = sHud.chars;
        if (n < 0 || n > MODS_HUD_CMDS || out->chars > MODS_HUD_CHARS) {
            continue;
        }
        out->count = n;
        memcpy(out->cmd, sHud.cmd, (size_t)n * sizeof(ModsCmd));
        memcpy(out->text, sHud.text, out->chars);
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        b = __atomic_load_n(&sHudSeq, __ATOMIC_ACQUIRE);
        if (a == b) {
            return n > 0;
        }
    }
    return 0;
}

static void hud_publish(void) {
    __atomic_add_fetch(&sHudSeq, 1, __ATOMIC_ACQ_REL);
    sHud.w = sBuild.w;
    sHud.count = sBuild.count;
    sHud.chars = sBuild.chars;
    memcpy(sHud.cmd, sBuild.cmd, (size_t)sBuild.count * sizeof(ModsCmd));
    memcpy(sHud.text, sBuild.text, sBuild.chars);
    __atomic_add_fetch(&sHudSeq, 1, __ATOMIC_ACQ_REL);
}

/* ------------------------------------------------------------------------------------------------ the game table */

static void set_num(lua_State *L, const char *k, double v) { lua_pushnumber(L, v); lua_setfield(L, -2, k); }
static void set_int(lua_State *L, const char *k, long long v) { lua_pushinteger(L, (lua_Integer)v); lua_setfield(L, -2, k); }
static void set_bool(lua_State *L, const char *k, int v) { lua_pushboolean(L, v); lua_setfield(L, -2, k); }

static void set_vec(lua_State *L, const char *k, const float *v) {
    lua_createtable(L, 0, 3);
    set_num(L, "x", v[0]);
    set_num(L, "y", v[1]);
    set_num(L, "z", v[2]);
    lua_setfield(L, -2, k);
}

static void push_game(lua_State *L, const FpState *fp, const ModsGame *g) {
    int side, k;

    lua_createtable(L, 0, 8);
    set_int(L, "frame", gPortVBlanks);
    set_bool(L, "battle", fp->battle && g->battle);
    set_bool(L, "hud_shown", fp->gauges);
    set_bool(L, "online", Port_NetSession() || Port_NetActive());
    set_bool(L, "split", g->split);
    lua_createtable(L, 2, 0);
    for (side = 0; side < 2 && fp->battle && g->battle; side++) {
        const FpSide *s = &fp->side[side];
        const ModsFighter *f = &g->f[side];
        lua_createtable(L, 0, 24);
        set_int(L, "side", side + 1);
        set_int(L, "active", s->active + 1);
        set_int(L, "count", s->count);
        if (s->next >= 0) {
            set_int(L, "next", s->next + 1);
        }
        if (s->active >= 0 && s->active < FP_MEMBERS) {
            set_int(L, "chara", s->m[s->active].chara);
            set_int(L, "costume", s->m[s->active].costume);
            set_int(L, "hp", s->m[s->active].hp);
            set_int(L, "hp_max", s->m[s->active].hpMax);
        }
        set_int(L, "ki", s->ki);
        set_int(L, "ki_max", s->kiMax);
        set_int(L, "blast", s->blast);
        set_int(L, "blast_max", s->blastMax);
        set_int(L, "max_power", s->maxPower);
        set_bool(L, "powered_up", s->poweredUp);
        set_int(L, "combo_hits", s->comboHits);
        set_int(L, "combo_damage", s->comboDamage);
        set_vec(L, "pos", f->pos);
        set_num(L, "yaw", f->yaw);
        lua_createtable(L, s->count, 0);
        for (k = 0; k < s->count && k < FP_MEMBERS; k++) {
            lua_createtable(L, 0, 5);
            set_bool(L, "present", s->m[k].present);
            set_int(L, "chara", s->m[k].chara);
            set_int(L, "costume", s->m[k].costume);
            set_int(L, "hp", s->m[k].hp);
            set_int(L, "hp_max", s->m[k].hpMax);
            lua_rawseti(L, -2, k + 1);
        }
        lua_setfield(L, -2, "members");
        lua_rawseti(L, -2, side + 1);
    }
    lua_setfield(L, -2, "fighters");
}

/* ------------------------------------------------------------------------------------------------ loading */

static char *read_file(const char *path, long *size) {
    FILE *fp = fopen(path, "rb");
    char *text = NULL;

    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (*size >= 0 && *size <= MOD_FILE_MAX && (text = (char *)malloc((size_t)*size + 1)) != NULL) {
        *size = (long)fread(text, 1, (size_t)*size, fp);
        text[*size] = '\0';
    }
    fclose(fp);
    return text;
}

static void copy_field(lua_State *L, int t, const char *k, char *out, size_t size) {
    lua_getfield(L, t, k);
    if (lua_type(L, -1) == LUA_TSTRING || lua_type(L, -1) == LUA_TNUMBER) {
        snprintf(out, size, "%s", lua_tostring(L, -1));
    }
    lua_pop(L, 1);
}

static double num_field(lua_State *L, int t, const char *k, double def) {
    double v = def;
    lua_getfield(L, t, k);
    if (lua_type(L, -1) == LUA_TNUMBER) {
        v = lua_tonumber(L, -1);
    } else if (lua_type(L, -1) == LUA_TBOOLEAN) {
        v = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);
    return v;
}

/* The settings list of the table at index t into the mod's info. */
static void read_settings(int i, lua_State *L, int t) {
    ModsInfo *in = &sInfo[i];
    int n, k, c;

    lua_getfield(L, t, "settings");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    n = (int)lua_rawlen(L, -1);
    for (k = 1; k <= n && in->settings < MODS_SETTINGS; k++) {
        ModsSetting *s = &in->set[in->settings];
        char type[16] = "slider";
        int e;

        lua_rawgeti(L, -1, k);
        e = lua_gettop(L);
        if (!lua_istable(L, e)) {
            lua_pop(L, 1);
            continue;
        }
        memset(s, 0, sizeof(*s));
        copy_field(L, e, "id", s->id, sizeof(s->id));
        copy_field(L, e, "label", s->label, sizeof(s->label));
        copy_field(L, e, "tip", s->tip, sizeof(s->tip));
        copy_field(L, e, "type", type, sizeof(type));
        if (s->id[0] == '\0' || find_setting(i, s->id) != NULL) {
            say(i, "setting %d has no id, or the id of another", k);
            lua_pop(L, 1);
            continue;
        }
        if (s->label[0] == '\0') {
            snprintf(s->label, sizeof(s->label), "%s", s->id);
        }
        s->kind = strcmp(type, "toggle") == 0 ? MODS_SET_TOGGLE : strcmp(type, "choice") == 0 ? MODS_SET_CHOICE : MODS_SET_SLIDER;
        if (s->kind == MODS_SET_CHOICE) {
            lua_getfield(L, e, "choices");
            c = lua_istable(L, -1) ? (int)lua_rawlen(L, -1) : 0;
            for (s->choices = 0; s->choices < c && s->choices < 8; s->choices++) {
                lua_rawgeti(L, -1, s->choices + 1);
                snprintf(s->choice[s->choices], sizeof(s->choice[0]), "%s", lua_isstring(L, -1) ? lua_tostring(L, -1) : "?");
                lua_pop(L, 1);
            }
            lua_pop(L, 1);
            if (s->choices == 0) {
                snprintf(s->choice[0], sizeof(s->choice[0]), "-");
                s->choices = 1;
            }
            s->min = 0;
            s->max = s->choices - 1;
            s->step = 1;
            s->def = num_field(L, e, "default", 1) - 1;
        } else if (s->kind == MODS_SET_TOGGLE) {
            s->min = 0;
            s->max = 1;
            s->step = 1;
            s->def = num_field(L, e, "default", 0) != 0;
        } else {
            s->min = num_field(L, e, "min", 0);
            s->max = num_field(L, e, "max", 100);
            s->step = num_field(L, e, "step", 0);
            if (s->max < s->min) {
                s->max = s->min;
            }
            s->def = num_field(L, e, "default", s->min);
        }
        s->def = s->def < s->min ? s->min : s->def > s->max ? s->max : s->def;
        s->value = s->def;
        in->settings++;
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

/* The player's choices (bt3_mods.txt) for the mods just loaded. */
static void settings_load(void) {
    const char *path = settings_path();
    FILE *fp = path != NULL ? fopen(path, "r") : NULL;
    char line[256], key[160];
    double v;
    int i;

    while (fp != NULL && fgets(line, sizeof(line), fp) != NULL) {
        char *dot;
        if (sscanf(line, " %159[^=\n]=%lf", key, &v) != 2) {
            continue;
        }
        dot = strchr(key, '.');
        if (dot != NULL) {
            *dot++ = '\0';
        }
        for (i = 0; i < sCount; i++) {
            if (strcmp(sInfo[i].folder, key) != 0) {
                continue;
            }
            if (dot == NULL) {
                sInfo[i].enabled = v != 0.0;
            } else {
                ModsSetting *s = find_setting(i, dot);
                if (s != NULL) {
                    s->value = v < s->min ? s->min : v > s->max ? s->max : v;
                }
            }
        }
    }
    if (fp != NULL) {
        fclose(fp);
    }
}

/* Writes the choices: those of the mods loaded, and the lines of other mods kept as they were. */
static void settings_save(void) {
    const char *path = settings_path();
    FILE *fp;
    char *old = NULL, *line, *next, key[160];
    long size = 0;
    int i, k;

    if (path == NULL) {
        return;
    }
    old = read_file(path, &size);
    fp = fopen(path, "w");
    if (fp == NULL) {
        free(old);
        return;
    }
    for (line = old; line != NULL && *line != '\0'; line = next) {
        int ours = 0;
        next = strchr(line, '\n');
        if (next != NULL) {
            *next++ = '\0';
        }
        if (sscanf(line, " %159[^=.\n]", key) == 1) {
            for (i = 0; i < sCount; i++) {
                ours |= strcmp(sInfo[i].folder, key) == 0;
            }
        }
        if (!ours && line[0] != '\0') {
            fprintf(fp, "%s\n", line);
        }
    }
    free(old);
    Mods_Lock();
    for (i = 0; i < sCount; i++) {
        fprintf(fp, "%s=%d\n", sInfo[i].folder, sInfo[i].enabled);
        for (k = 0; k < sInfo[i].settings; k++) {
            fprintf(fp, "%s.%s=%.6g\n", sInfo[i].folder, sInfo[i].set[k].id, sInfo[i].set[k].value);
        }
    }
    Mods_Unlock();
    fclose(fp);
}

static void open_libs(lua_State *L, int i) {
    static const char *const drop[] = {"dofile", "loadfile", "load", NULL};
    int k;

    luaL_requiref(L, LUA_GNAME, luaopen_base, 1);
    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_settop(L, 0);
    for (k = 0; drop[k] != NULL; k++) { /* (no other files, and no compiled chunks: those can break the interpreter) */
        lua_pushnil(L);
        lua_setglobal(L, drop[k]);
    }
    lua_getglobal(L, LUA_STRLIBNAME);
    lua_pushnil(L);
    lua_setfield(L, -2, "dump");
    lua_pop(L, 1);
    lua_pushcfunction(L, l_print);
    lua_setglobal(L, "print");
    lua_pushinteger(L, i);
    lua_pushcclosure(L, l_require, 1);
    lua_setglobal(L, "require");
    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, "bt3_modules");

    lua_createtable(L, 0, 6);
    set_int(L, "api", 1);
    lua_pushstring(L, sInfo[i].folder);
    lua_setfield(L, -2, "folder");
    lua_pushinteger(L, i);
    lua_pushcclosure(L, l_setting, 1);
    lua_setfield(L, -2, "setting");
    lua_pushcfunction(L, l_rgba);
    lua_setfield(L, -2, "rgba");
    lua_pushcfunction(L, l_print);
    lua_setfield(L, -2, "log");
    lua_setglobal(L, "bt3");
}

static int has_fn(lua_State *L, int t, const char *k) {
    int r;
    lua_getfield(L, t, k);
    r = lua_isfunction(L, -1);
    lua_pop(L, 1);
    return r;
}

/* Loads the script of mods/<folder> into the next slot. 1 = the slot is used (a mod, running or stopped). */
static int load_one(const char *folder) {
    int i = sCount;
    Mod *m = &sMod[i];
    ModsInfo *in = &sInfo[i];
    char path[512];
    long size = 0;
    char *text;
    int r, t;

    memset(m, 0, sizeof(*m));
    memset(in, 0, sizeof(*in));
    snprintf(in->folder, sizeof(in->folder), "%s", folder);
    snprintf(in->name, sizeof(in->name), "%s", folder);
    in->enabled = 1;
    snprintf(path, sizeof(path), "%s/%s/mod.lua", Mods_Folder(), folder);
    text = read_file(path, &size);
    if (text == NULL) {
        return 0; /* (a folder without a script: not a script mod) */
    }
    m->table = m->game = LUA_NOREF;
    m->L = lua_newstate(mod_alloc, m);
    if (m->L == NULL) {
        free(text);
        fail(i, "no memory for its Lua");
        return 1;
    }
    open_libs(m->L, i);
    lua_sethook(m->L, count_hook, LUA_MASKCOUNT, HOOK_STEP);
    lua_pushfstring(m->L, "@%s/mod.lua", folder);
    r = luaL_loadbufferx(m->L, text, (size_t)size, lua_tostring(m->L, -1), "t");
    free(text);
    lua_remove(m->L, -2);
    if (r != LUA_OK) {
        fail(i, lua_tostring(m->L, -1));
        lua_settop(m->L, 0);
        return 1;
    }
    m->steps = 0;
    m->budget = BUDGET_LOAD;
    sCurrent = i;
    r = lua_pcall(m->L, 0, 1, 0);
    sCurrent = -1;
    if (r != LUA_OK) {
        fail(i, lua_tostring(m->L, -1));
        lua_settop(m->L, 0);
        return 1;
    }
    if (!lua_istable(m->L, -1)) {
        fail(i, "mod.lua must end with `return { ... }` (the mod's table)");
        lua_settop(m->L, 0);
        return 1;
    }
    t = lua_gettop(m->L);
    copy_field(m->L, t, "name", in->name, sizeof(in->name));
    copy_field(m->L, t, "version", in->version, sizeof(in->version));
    copy_field(m->L, t, "author", in->author, sizeof(in->author));
    copy_field(m->L, t, "description", in->about, sizeof(in->about));
    read_settings(i, m->L, t);
    in->hooks = (has_fn(m->L, t, "on_frame") ? MODS_HOOK_FRAME : 0) | (has_fn(m->L, t, "on_hud") ? MODS_HOOK_HUD : 0) |
                (has_fn(m->L, t, "on_camera") ? MODS_HOOK_CAMERA : 0);
    m->table = luaL_ref(m->L, LUA_REGISTRYINDEX);
    lua_settop(m->L, 0);
    say(i, "loaded:%s%s%s", in->hooks & MODS_HOOK_FRAME ? " on_frame" : "", in->hooks & MODS_HOOK_HUD ? " on_hud" : "",
        in->hooks & MODS_HOOK_CAMERA ? " on_camera" : "");
    return 1;
}

static int by_name(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

static void unload_all(void) {
    int i;

    for (i = 0; i < sCount; i++) {
        if (sMod[i].L != NULL) {
            lua_close(sMod[i].L);
        }
    }
    memset(sMod, 0, sizeof(sMod));
    sCount = 0;
}

static void load_all(void) {
    static char names[MODS_MAX][64];
    DIR *d;
    struct dirent *e;
    int n = 0, i;

    Mods_Lock();
    unload_all();
    d = opendir(Mods_Folder());
    while (d != NULL && (e = readdir(d)) != NULL && n < MODS_MAX) {
        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(names[0])) {
            continue;
        }
        snprintf(names[n++], sizeof(names[0]), "%s", e->d_name);
    }
    if (d != NULL) {
        closedir(d);
    }
    qsort(names, (size_t)n, sizeof(names[0]), by_name); /* (the same order on every machine) */
    Mods_Unlock();
    for (i = 0; i < n; i++) { /* (the lock is not held while a script runs: the window would wait for it; it sees
                                    a mod once it is loaded) */
        if (load_one(names[i])) {
            Mods_Lock();
            sCount++;
            Mods_Unlock();
        }
    }
    Mods_Lock();
    settings_load();
    Mods_Unlock();
    sLoaded = 1;
}

/* ----------------------------------------------------------------------------------------------- the hooks */

static int runs(int i, int hook) {
    return sMod[i].L != NULL && !sMod[i].dead && sInfo[i].enabled && (sInfo[i].hooks & hook) != 0;
}

/* Pushes the mod's function `name` (from its table); 0 = it has none now. */
static int push_fn(int i, const char *name) {
    lua_State *L = sMod[i].L;

    lua_rawgeti(L, LUA_REGISTRYINDEX, sMod[i].table);
    lua_getfield(L, -1, name);
    lua_remove(L, -2);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    return 1;
}

static void push_last_game(int i) {
    lua_State *L = sMod[i].L;

    if (sMod[i].game != LUA_NOREF) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, sMod[i].game);
    } else {
        lua_newtable(L);
    }
}

void Mods_Tick(void) {
    FpState fp;
    ModsGame g;
    int i;

    if (sReload || !sLoaded) {
        sReload = 0;
        load_all();
    }
    if (sDirty) {
        sDirty = 0;
        settings_save();
    }
    if (sCount == 0) {
        if (sHud.count != 0) { /* (the last mod gone: its shapes too) */
            sBuild.count = 0;
            sBuild.chars = 0;
            hud_publish();
        }
        return;
    }
    if (!PortPanels_Read(&fp)) {
        memset(&fp, 0, sizeof(fp));
    }
    PortMods_Read(&g);
    sBuild.count = 0;
    sBuild.chars = 0;
    sBuild.w = hud_width();
    for (i = 0; i < sCount; i++) {
        lua_State *L = sMod[i].L;
        if (L == NULL || sMod[i].dead || !sInfo[i].enabled) {
            continue;
        }
        push_game(L, &fp, &g);
        luaL_unref(L, LUA_REGISTRYINDEX, sMod[i].game);
        lua_pushvalue(L, -1);
        sMod[i].game = luaL_ref(L, LUA_REGISTRYINDEX);
        if (runs(i, MODS_HOOK_FRAME) && push_fn(i, "on_frame")) {
            lua_pushvalue(L, -2);
            call(i, 1, 0);
        }
        if (runs(i, MODS_HOOK_HUD) && push_fn(i, "on_hud")) {
            push_hud(L);
            lua_pushvalue(L, -3);
            call(i, 2, 0);
        }
        lua_settop(L, 0);
    }
    hud_publish();
}

int Mods_WantCamera(void) {
    int i;

    if (!sLoaded) {
        return 0; /* (the first blank loads them) */
    }
    for (i = 0; i < sCount; i++) {
        if (runs(i, MODS_HOOK_CAMERA)) {
            return 1;
        }
    }
    return 0;
}

static int read_vec(lua_State *L, int t, const char *k, float *out) {
    float v[3];
    int ok = 1, j;
    static const char *const xyz[] = {"x", "y", "z"};

    lua_getfield(L, t, k);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    for (j = 0; j < 3; j++) {
        lua_getfield(L, -1, xyz[j]);
        v[j] = (float)lua_tonumber(L, -1);
        ok &= lua_type(L, -1) == LUA_TNUMBER && isfinite(v[j]) && fabsf(v[j]) < 1.0e7f;
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    if (ok) {
        memcpy(out, v, sizeof(v));
    }
    return ok;
}

void Mods_Camera(void) {
    ModsCam cam, orig;
    int i, changed = 0;

    PortMods_CamGet(&cam);
    orig = cam;
    for (i = 0; i < sCount; i++) {
        lua_State *L = sMod[i].L;
        int t;
        if (!runs(i, MODS_HOOK_CAMERA) || !push_fn(i, "on_camera")) {
            continue;
        }
        lua_pop(L, 1);
        lua_settop(L, 0);
        lua_createtable(L, 0, 5); /* (1: the camera, kept to read back what the script changed in it) */
        t = lua_gettop(L);
        set_int(L, "side", cam.side + 1);
        set_bool(L, "cut", cam.cut);
        set_vec(L, "pos", cam.pos);
        set_vec(L, "rot", cam.rot);
        set_vec(L, "target", cam.target);
        push_fn(i, "on_camera");
        lua_pushvalue(L, t);
        push_last_game(i);
        if (call(i, 2, 1)) { /* on_camera(cam, game): it changes cam, or returns a camera table */
            int r = lua_istable(L, -1) ? lua_gettop(L) : t;
            float p[3], q[3];
            if (read_vec(L, r, "pos", p) && read_vec(L, r, "rot", q)) {
                memcpy(cam.pos, p, sizeof(p));
                memcpy(cam.rot, q, sizeof(q));
                changed = 1;
            }
        }
        lua_settop(L, 0);
    }
    if (changed && memcmp(&cam, &orig, sizeof(cam)) != 0) {
        PortMods_CamSet(&cam);
    }
}
