/* The script mods (port/src/gs/mods.c) on their own, without the game: scripts are written to a scratch folder and
 * run through the same calls the game makes once per blank (Mods_Tick) and from the camera hook (Mods_Camera). What
 * the game would hand over (the fight, the camera) comes from this file. Checked: loading, settings and the file they
 * are kept in, the HUD's shapes and text, the camera (and that a pose that is not a number is refused), require, and
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

    sFp.battle = 1;
    sG.battle = 1;
    sFp.side[0].count = 1;
    sFp.side[0].m[0].hp = 12345;
    sFp.side[1].count = 1;
    sG.f[1].pos[0] = 3.0f;
    sG.f[0].found = sG.f[1].found = 1;

    Mods_Tick();
    CHECK(Mods_Count() == 9); /* (every folder with a mod.lua; not not_a_mod) */
    m = mod("hud");
    CHECK(m != NULL && strcmp(m->name, "HUD test") == 0 && m->error[0] == '\0' && m->settings == 3);
    CHECK(m != NULL && m->set[0].value == 70.0); /* (from the file) */
    CHECK(Mods_HudRead(&sRead) && sRead.count == 2);
    CHECK(sRead.cmd[0].kind == MODS_CMD_FILL && sRead.cmd[0].c == 70.0f && sRead.cmd[0].color == 0xFF000080u && sRead.cmd[0].e == 2.0f);
    CHECK(sRead.cmd[1].kind == MODS_CMD_TEXT && sRead.cmd[1].align == 1 && sRead.cmd[1].text == 8 &&
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
    CHECK(!Mods_HudRead(&sRead));
    {
        long size = 0;
        char *text = read_file(settings, &size);
        CHECK(text != NULL && strstr(text, "other.x=5") != NULL && strstr(text, "hud=0") != NULL && strstr(text, "hud.size=33") != NULL);
        free(text);
    }

    /* loaded again: the choices come back from the file, the stopped scripts run again (and stop again) */
    Mods_ReloadAsk();
    Mods_Tick();
    CHECK(Mods_Count() == 9 && mod("hud")->enabled == 0 && mod("hud")->set[0].value == 33.0);
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
    CHECK(Mods_HudRead(&sRead) && sRead.count == 2);
    for (i = 0; i < Mods_Count(); i++) {
        CHECK(sMod[i].L == NULL || sMod[i].mem < ((size_t)8 << 20) || strcmp(sInfo[i].folder, "memory") == 0);
    }

    /* the examples shipped with the source (run from the repository's root): they load and run without stopping */
    setenv("BT3_MODS", "port/examples/mods", 1);
    setenv("BT3_MODS_SETTINGS", "", 1); /* (no file) */
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
    CHECK(Mods_Count() == 2);
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
