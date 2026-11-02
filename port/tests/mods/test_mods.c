/* The script mods (port/src/gs/mods.c) on their own, without the game: scripts are written to a scratch folder and
 * run through the same calls the game makes once per blank (Mods_Tick) and from the camera hook (Mods_Camera). What
 * the game would hand over (the fight, the camera) comes from this file. Checked: loading, settings and the file they
 * are kept in, the HUD's shapes and text, the look asked of the renderer (bt3.look), the camera (and that a pose that is not a number is refused), require, and
 * that a script cannot reach outside (no io, os, load, string.dump) and is stopped by an error, an endless loop or
 * taking too much memory, without stopping the others. Built with the address and undefined-behaviour sanitizers.
 *
 *   cc -g -O1 -fsanitize=address,undefined -fno-sanitize-recover=all port/tests/mods/test_mods.c port/src/gs/mods_lua.c -lm -lpthread -o mods_test && ./mods_test
 *
 * Ends with "mods OK"; a failed check prints it and exits with 1. */
#include "../../src/gs/mods.c"

#include <sys/stat.h>
#include <unistd.h>

unsigned gPortVBlanks;
volatile int gUiPresentW = 1280, gUiPresentH = 896; /* (448 high in HUD units, 640 wide) */
int Port_NetSession(void) { return 0; }
int Port_NetActive(void) { return 0; }
int Port_HudSwap(void) { return 0; }
const char *MenuTranslation_Text(const char *id) { return strcmp(id, "combo-hits/0/0") == 0 ? "Golpes" : NULL; }
const char *MenuTranslation_Locale(void) { return "es"; }

static FpState sFp;
int PortPanels_Read(FpState *out) { *out = sFp; return 1; }
static ModsGame sG;
int PortMods_Read(ModsGame *out) { *out = sG; return sG.battle; }
static ModsCam sIn, sOut;
static int sSet;
int PortMods_CamGet(ModsCam *out) { *out = sIn; return 1; }
void PortMods_CamSet(const ModsCam *cam) { sOut = *cam; sSet = 1; }

static int sFailed;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #c); sFailed = 1; } } while (0)

static char sDir[256];

static void put(const char *folder, const char *file, const char *text) {
    char path[512];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/%s", sDir, folder);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/%s/%s", sDir, folder, file);
    fp = fopen(path, "w");
    fputs(text, fp);
    fclose(fp);
}

static ModsInfo *mod(const char *folder) {
    int i;
    for (i = 0; i < Mods_Count(); i++) {
        if (strcmp(Mods_Info(i)->folder, folder) == 0) {
            return Mods_Info(i);
        }
    }
    return NULL;
}

static ModsHud sRead;

int main(void) {
    char settings[300];
    FILE *fp;
    ModsInfo *m;
    int i;

    snprintf(sDir, sizeof(sDir), "/tmp/bt3_mods_test_XXXXXX");
    if (mkdtemp(sDir) == NULL) {
        return 1;
    }
    setenv("BT3_MODS", sDir, 1);
    snprintf(settings, sizeof(settings), "%s.txt", sDir);
    setenv("BT3_MODS_SETTINGS", settings, 1);
    fp = fopen(settings, "w");
    fputs("other.x=5\nhud.size=70\n", fp); /* (a line of a mod not installed is kept when the file is written) */
    /* (a mod starts switched off until the player switches it on: the test mods switched on) */
    fputs("bad_return=1\ncamera=1\ncamera_nan=1\nerr=1\nhud=1\nlook_a=1\nlook_b=1\nloop=1\nmemory=1\nnot_a_mod=1\n"
          "sandbox=1\nshapes=1\nsyntax=1\n", fp);
    fclose(fp);

    put("hud", "mod.lua",
        "local util = require('lib.util')\n"
        "return { name = 'HUD test', version = '1.0', author = 'tests',\n"
        "  settings = { { id = 'size', label = 'Size', type = 'slider', min = 0, max = 100, default = 50 },\n"
        "               { id = 'show', type = 'toggle', default = true },\n"
        "               { id = 'mode', type = 'choice', choices = { 'a', 'b', 'c' }, default = 2 } },\n"
        "  on_hud = function(hud, game)\n"
        "    if not bt3.setting('show') or bt3.setting('mode') ~= 2 then error('settings') end\n"
        "    hud.fill(10, 20, bt3.setting('size'), 8, bt3.rgba(255, 0, 0, 128), 2)\n"
        "    hud.text(hud.w / 2, 30, util.label(game), 0xFFFFFFFF, 20, 'center')\n"
        "  end }\n");
    put("hud/lib", "util.lua", "return { label = function(g) return 'P1 ' .. g.fighters[1].hp end }\n");
    put("camera", "mod.lua",
        "return { on_camera = function(cam, game)\n"
        "  if cam.cut then return end\n"
        "  cam.pos.x = cam.pos.x + 1; cam.rot.z = 0.25\n"
        "end }\n");
    put("camera_nan", "mod.lua", "return { on_camera = function(cam) return { pos = { x = 0/0, y = 0, z = 0 }, rot = cam.rot } end }\n");
    put("err", "mod.lua", "return { on_frame = function() local t = nil; return t.x end }\n");
    put("loop", "mod.lua", "return { on_frame = function() while true do end end }\n");
    put("memory", "mod.lua", "local t = {} return { on_frame = function() for i = 1, 1e6 do t[#t + 1] = string.rep('x', 1e5) .. i end end }\n");
    put("sandbox", "mod.lua",
        "assert(io == nil and os == nil and package == nil and debug == nil, 'libraries')\n"
        "assert(load == nil and loadfile == nil and dofile == nil and string.dump == nil, 'loaders')\n"
        "assert(not pcall(require, '../escape'), 'require outside')\n"
        "return { on_frame = function(game) assert(game.battle and game.fighters[2].pos.x == 3) end }\n");
    put("bad_return", "mod.lua", "x = 1\n");
    put("syntax", "mod.lua", "return { on_frame = function( }\n");
    put("not_a_mod", "readme.txt", "no script here\n");
    put("look_a", "mod.lua", "return { on_frame = function() bt3.look{ shading = 150, vivid = 40, rim = 30, rim_color = 0x11223344 } end }\n");
    put("look_b", "mod.lua", "return { on_frame = function() bt3.look{ vivid = 70.4, ambient = 0/0, energy = -5 } end }\n");
    put("shapes", "mod.lua",
        "local img, iw, ih = bt3.image('art/frame.png')\n"
        "assert(img == 1 and iw == 300 and ih == 20, 'image size')\n"
        "assert(bt3.image('missing.png') == nil, 'missing image')\n"
        "assert(not pcall(bt3.image, '../x.png'), 'image outside')\n"
        "return { on_hud = function(hud, game)\n"
        "  assert(game.timer and game.timer.value == 99 and game.timer.clock, 'timer')\n"
        "  local f = game.fighters[1]\n"
        "  assert(f.status.attack == 1 and f.status.speed == -1 and f.status.ki == 0, 'status')\n"
        "  assert(f.hud_hits == 7 and f.message == 2, 'combo feed')\n"
        "  hud.hide_game('gauges', 'combo')\n"
        "  hud.poly({ 0, 0, 10, 0, 10, 10 }, 0x11223344)\n"
        "  hud.path({ { 0, 0 }, { 5, 5 }, { 9, 1 } }, 0xFFFFFFFF, 2, true)\n"
        "  hud.gradient(0, 0, 10, 10, 1, 2, 3, 4)\n"
        "  hud.text(1, 2, 'hi', 0xFFFFFFFF, 20, 'right', { outline = 0xFF00FFFF, shadow = 0x000000C0, font = 'ui', fit = 50 })\n"
        "  hud.image(img, 0, 0, 30, 2, { flip = true, tint = 0x80FFFFFF })\n"
        "  hud.portrait(2, 0, 0, 40, 40, { head = 1.5, band = 'white' })\n"
        "  hud.clip(0, 0, 5, 5) hud.unclip()\n"
        "  hud.band({ 0, 0, 10, 1, 20, 3 }, { 0, 5, 10, 6, 20, 7 }, { 0xFF0000FF, 0x00FF00FF }, 0x0000FFFF)\n"
        "  hud.glow(5, 5, 30, 0x000000A0)\n"
        "  hud.text(2, 9, '7', 0xFFFFFFFF, 30, 'left', { bottom = 0xFFAA00FF, lean = 0.16, baseline = true })\n"
        "  hud.text(4, 9, bt3.label('combo-hits/0/0') or '?', 0xFFFFFFFF, 12, 'left', { after = true })\n"
        "  hud.portrait(1, 100, 92, 236, 0, { face = true, flip = true, fade = { x = 108, y = 82, r0 = 52, r1 = 116 } })\n"
        "  assert(bt3.locale() == 'es' and bt3.label('nothing') == nil and type(game.time) == 'number', 'labels')\n"
        "end }\n");
    {
        static const unsigned char png[24] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n', 0, 0, 0, 13, 'I', 'H', 'D', 'R',
                                               0, 0, 0x01, 0x2C, 0, 0, 0, 20};
        char path[512];
        snprintf(path, sizeof(path), "%s/shapes/art", sDir);
        mkdir(path, 0755);
        snprintf(path, sizeof(path), "%s/shapes/art/frame.png", sDir);
        fp = fopen(path, "wb");
        fwrite(png, 1, sizeof(png), fp);
        fclose(fp);
    }

    sFp.battle = 1;
    sG.battle = 1;
    sFp.side[0].count = 1;
    sFp.side[0].m[0].hp = 12345;
    sFp.side[1].count = 1;
    sG.f[1].pos[0] = 3.0f;
    sG.f[0].found = sG.f[1].found = 1;
    sFp.timer = 99;
    sFp.timerClock = 1;
    sFp.timerSet = 1;
    sFp.side[0].statMask = 1 | 2;
    sFp.side[0].statUp = 1;
    sFp.side[0].hudHits = 7;
    sFp.side[0].msg = 2;

    Mods_Tick();
    CHECK(Mods_Count() == 12); /* (every folder with a mod.lua; not not_a_mod) */
    {   /* the look: the strongest each mod asked for, within 0..100 (NaN: 0) */
        const ModsLook *lk = Mods_Look();
        CHECK(mod("look_a")->error[0] == '\0' && mod("look_b")->error[0] == '\0');
        CHECK(lk->shading == 100 && lk->vivid == 70 && lk->rim == 30 && lk->ambient == 0 && lk->energy == 0 &&
              lk->energy_light == 0 && lk->rim_color == 0x11223344u);
    }
    CHECK(mod("shapes") != NULL && mod("shapes")->error[0] == '\0');
    CHECK(Mods_HudHidden() == (1 | 4));
    {
        char path[400];
        CHECK(Mods_ImagePath(1, path, sizeof(path)) != NULL && strstr(path, "shapes/art/frame.png") != NULL);
        CHECK(Mods_ImagePath(2, path, sizeof(path)) == NULL);
    }
    m = mod("hud");
    CHECK(m != NULL && strcmp(m->name, "HUD test") == 0 && m->error[0] == '\0' && m->settings == 3);
    CHECK(m != NULL && m->set[0].value == 70.0); /* (from the file) */
    CHECK(Mods_HudRead(&sRead) && sRead.count == 2 + 13);
    {   /* the shapes mod's, after the hud mod's two (folders in order) */
        const ModsCmd *c = &sRead.cmd[2];
        CHECK(c[0].kind == MODS_CMD_POLY && c[0].n == 3 && c[0].color == 0x11223344u && sRead.pts[c[0].at * 2 + 2] == 10.0f);
        CHECK(c[1].kind == MODS_CMD_PATH && c[1].n == 3 && (c[1].flags & MODS_F_CLOSED) && sRead.pts[(c[1].at + 1) * 2] == 5.0f);
        CHECK(c[2].kind == MODS_CMD_GRADIENT && c[2].color == 1 && c[2].color4 == 4);
        CHECK(c[3].kind == MODS_CMD_TEXT && c[3].align == 2 && c[3].font == 1 && c[3].color2 == 0xFF00FFFFu &&
              c[3].color3 == 0xC0u && c[3].f[6] == 50.0f);
        CHECK(c[4].kind == MODS_CMD_IMAGE && c[4].i0 == 1 && (c[4].flags & MODS_F_FLIP) && c[4].color == 0x80FFFFFFu);
        CHECK(c[5].kind == MODS_CMD_PORTRAIT && c[5].i0 == 1 && c[5].i1 == -1 && c[5].i2 == 2 && (c[5].flags & MODS_F_HEAD) &&
              c[5].f[4] == 1.5f);
        CHECK(c[6].kind == MODS_CMD_CLIP && c[7].kind == MODS_CMD_UNCLIP);
        CHECK(c[8].kind == MODS_CMD_BAND && c[8].n == 3 && sRead.pts[(c[8].at + 3) * 2 + 1] == 5.0f &&
              sRead.cols[c[8].i0 + 1] == 0x00FF00FFu && sRead.cols[c[8].i0 + 2] == 0x00FF00FFu && sRead.cols[c[8].i0 + 5] == 0x0000FFFFu);
        CHECK(c[9].kind == MODS_CMD_GLOW && c[9].f[2] == 30.0f && c[9].color2 == 0x000000A0u);
        CHECK(c[10].kind == MODS_CMD_TEXT && (c[10].flags & MODS_F_GRAD) && (c[10].flags & MODS_F_BASELINE) && c[10].f[7] == 0.16f &&
              c[10].color4 == 0xFFAA00FFu);
        CHECK(c[11].kind == MODS_CMD_TEXT && (c[11].flags & MODS_F_AFTER) && c[11].n == 6);
        CHECK(c[12].kind == MODS_CMD_PORTRAIT && (c[12].flags & MODS_F_FACE) && (c[12].flags & MODS_F_FLIP) && c[12].f[10] == 52.0f &&
              c[12].f[4] == 100.0f - 236.0f && c[12].f[7] == 92.0f + 472.0f);
    }
    CHECK(sRead.cmd[0].kind == MODS_CMD_FILL && sRead.cmd[0].f[2] == 70.0f && sRead.cmd[0].color == 0xFF000080u && sRead.cmd[0].f[4] == 2.0f);
    CHECK(sRead.cmd[1].kind == MODS_CMD_TEXT && sRead.cmd[1].align == 1 && sRead.cmd[1].n == 8 &&
          memcmp(sRead.text + sRead.cmd[1].at, "P1 12345", 8) == 0);
    CHECK(fabsf(sRead.w - 640.0f) < 0.01f); /* (448 * 1280 / 896) */
    CHECK(mod("sandbox") != NULL && mod("sandbox")->error[0] == '\0');
    CHECK(mod("err") != NULL && strstr(mod("err")->error, "attempt to index") != NULL);
    CHECK(mod("loop") != NULL && strstr(mod("loop")->error, "too long") != NULL);
    CHECK(mod("memory") != NULL && strstr(mod("memory")->error, "memory") != NULL);
    CHECK(mod("bad_return") != NULL && strstr(mod("bad_return")->error, "return") != NULL);
    CHECK(mod("syntax") != NULL && mod("syntax")->error[0] != '\0');

    /* the camera: the first mod moves it, the second's pose is not a number and is refused */
    CHECK(Mods_WantCamera());
    memset(&sIn, 0, sizeof(sIn));
    sIn.pos[0] = 10.0f;
    sIn.rot[1] = 1.0f;
    Mods_Camera();
    CHECK(sSet && sOut.pos[0] == 11.0f && sOut.rot[2] == 0.25f && sOut.rot[1] == 1.0f);
    CHECK(mod("camera_nan")->error[0] == '\0');
    sSet = 0;
    sIn.cut = 1; /* (a cut: the camera mod leaves it) */
    Mods_Camera();
    CHECK(!sSet);

    /* switched off from the window: no shapes; the choice is written to the file with the other mod's line */
    Mods_Lock();
    mod("hud")->enabled = 0;
    mod("hud")->set[0].value = 33.0;
    Mods_Unlock();
    Mods_SettingChanged();
    Mods_Tick();
    CHECK(Mods_HudRead(&sRead) && sRead.count == 13 && sRead.cmd[0].kind == MODS_CMD_POLY); /* (only the shapes mod's) */
    Mods_Lock();
    mod("look_a")->enabled = 0;
    Mods_Unlock();
    Mods_Tick(); /* (the look follows: what look_a asked for is gone, look_b's stays) */
    CHECK(Mods_Look()->shading == 0 && Mods_Look()->vivid == 70 && Mods_Look()->rim == 0 && Mods_Look()->rim_color == 0xFFECD6FFu);
    Mods_Lock();
    mod("look_b")->enabled = 0;
    Mods_Unlock();
    Mods_Tick();
    CHECK(memcmp(Mods_Look(), &kLookNone, sizeof(ModsLook)) == 0);
    {
        long size = 0;
        char *text = read_file(settings, &size);
        CHECK(text != NULL && strstr(text, "other.x=5") != NULL && strstr(text, "hud=0") != NULL && strstr(text, "hud.size=33") != NULL);
        free(text);
    }

    /* loaded again: the choices come back from the file, the stopped scripts run again (and stop again) */
    Mods_ReloadAsk();
    Mods_Tick();
    CHECK(Mods_Count() == 12 && mod("hud")->enabled == 0 && mod("hud")->set[0].value == 33.0);
    CHECK(strstr(mod("loop")->error, "too long") != NULL);

    /* a long run of blanks: nothing grows */
    Mods_Lock();
    mod("hud")->enabled = 1;
    Mods_Unlock();
    for (i = 0; i < 2000; i++) {
        gPortVBlanks++;
        Mods_Tick();
        Mods_Camera();
    }
    CHECK(Mods_HudRead(&sRead) && sRead.count == 2 + 13);
    for (i = 0; i < Mods_Count(); i++) {
        CHECK(sMod[i].L == NULL || sMod[i].mem < ((size_t)8 << 20) || strcmp(sInfo[i].folder, "memory") == 0);
    }

    /* the examples shipped with the source (run from the repository's root): they load and run without stopping
       (switched on: a mod starts off) */
    setenv("BT3_MODS", "port/examples/mods", 1);
    {
        char ex[300];
        FILE *ef;
        snprintf(ex, sizeof(ex), "%s-examples.txt", sDir);
        ef = fopen(ex, "w");
        fputs("example_camera=1\nexample_hud=1\nmodern_camera=1\nmodern_hud=1\nmodern_look=1\nskill_list=1\n", ef);
        fclose(ef);
        setenv("BT3_MODS_SETTINGS", ex, 1);
    }
    sFp.gauges = 1;
    sFp.side[0].comboHits = 3;
    sFp.side[1].poweredUp = 1;
    Mods_ReloadAsk();
    {
    int moved = 0;
    for (i = 0; i < 100; i++) {
        Mods_Tick();
        sIn.cut = i % 40 == 39;
        sIn.rot[1] = (float)i * 0.01f;
        sSet = 0;
        Mods_Camera();
        CHECK(!(sIn.cut && sSet));
        moved += sSet; /* (not on the first frame or after a cut: the pose is then the game's, nothing to write) */
    }
    CHECK(moved >= 90);
    }
    CHECK(Mods_Count() == 6); /* (example_camera, example_hud, modern_camera, modern_hud, modern_look, skill_list) */
    CHECK(Mods_Look()->shading == 100 && Mods_Look()->energy_light == 0); /* (modern_look at its defaults) */
    for (i = 0; i < Mods_Count(); i++) {
        CHECK(Mods_Info(i)->error[0] == '\0');
    }
    CHECK(Mods_HudRead(&sRead) && sRead.count >= 4);

    Mods_Lock();
    unload_all();
    Mods_Unlock();
    if (sFailed) {
        return 1;
    }
    printf("mods OK\n");
    return 0;
}
