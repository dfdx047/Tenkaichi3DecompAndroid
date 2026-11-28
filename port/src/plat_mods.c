/*
 * Mods of the PC build: the library behind the game's Mod Manager (src/menu/port_modmgr.c and its companions).
 *
 * A mod is a folder in <data>/mods/. Two kinds are understood:
 *   - a character mod in the community's layout: files named after the character and what they are, for example
 *     Goku_3_1p.pak (first costume), Goku_3_1p_dmg.pak (its damaged version), ..._2p / _3p / _4p, Goku_3_anm.pak
 *     (animations), Goku_3_eff.pak (effects), Goku_3_img.cdbt (the large portrait), anywhere under the folder;
 *   - files by number, in the game's own layout: <mod>/pzs3us1/01453.bin.
 * Nothing of the game is changed: a mod that is on answers the file layer's requests (plat_file.c asks
 * Port_ModRedirect for every file of an archive). <data>/mods/enabled.txt lists the mods that are on, the first one
 * winning where two want the same file. A costume or damaged version a mod lacks is answered with the costume it has.
 *
 * What the manager decides about a mod is kept in <mod>/mod.txt (key=value lines):
 *   mode=replace|new   replace a fighter of the roster, or be a fighter of its own on the mod grid
 *   target=<id>        the fighter it replaces (default: the one its files are named after)
 *   template=<id>      for a new fighter: whose voice, statistics and place in the game's tables it borrows
 *   white= / yellow=   its two names on the select screens
 *   chip=x,y,size      the square of its portrait that is its face on the grids
 *   trans<n>= / next=  its transformations (see Port_Trans*)
 *
 * A NEW fighter is a "skin": the game is told it is its template (so every table indexed by character works), and
 * the costume number carries which skin: 8 + skin * 4 + costume. The battle loader asks Port_SkinFile for the file
 * ids of such a fighter and gets ids from 0x100000 up, which only this file answers (they fall into the third
 * archive, far behind its last file).
 *
 * The game's code has 4-byte pointers: what it passes here arrives as 32-bit values.
 */
#include <math.h>
#include <ctype.h>
#include <dirent.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define make_dir_one(p) _mkdir(p)
#else
#define make_dir_one(p) mkdir(p, 0777)
#endif

/* A folder made with the ones above it that are missing (a fresh install has no <data>/mods: the library, the saver and
   the editor's preview all need it). 0 made or there already, -1 could not. */
static int make_dir(const char *path) {
    char tmp[1024];
    size_t n = strlen(path), i;

    if (n == 0 || n >= sizeof(tmp)) {
        return -1;
    }
    memcpy(tmp, path, n + 1);
    for (i = 1; i < n; i++) {
        if ((tmp[i] == '/' || tmp[i] == '\\') && tmp[i - 1] != ':') {
            char c = tmp[i];

            tmp[i] = '\0';
            make_dir_one(tmp);
            tmp[i] = c;
        }
    }
    make_dir_one(tmp);
    return 0;
}

#define MAX_MODS 256
#define MAX_FILES 4096 /* files of pzs3us1 a mod can answer */
#define KIND_COUNT 11  /* 0..3 costumes, 4..7 their damaged versions, 8 animations, 9 effects, 10 portrait */
#define KIND_FACE 11   /* + side: the face of the battle gauge (files by number only) */
#define MODEL_FILE 0x590
#define FACE_FILE 0x2F9
#define DATA_FILE 8
#define US2_FIRST 0xD48        /* the first file id of the third archive */
#define SKIN_FILE 0x100000     /* + skin * 16 + kind */
#define OLD_MODEL_SIZE 0xCE000
#define REF_MOD 0x1000         /* a transformation's target: + mod index (below: a character id; -1: none) */

/* A fighter's transformations and fusions, every field of its parameter block the game reads for them (+0x98..+0xC5,
   see BtlParam in include/battle/btl_tech_a.h): what the Mod Manager's FORMS screen edits. */
typedef struct PortTrans {
    int32_t set;          /* the transformations are set here (0: the fighter's own, as its file has them) */
    int32_t target[4];    /* -1, a character id, or REF_MOD + mod index */
    int32_t cost[4];      /* blast stocks */
    int32_t next;         /* the slot plain R3 takes, -1 none */
    int32_t type[4];      /* +0xA0: 0 power up, 1 back to base, 2 great ape, 3 absorption, 4 machine merge */
    int32_t effect[4];    /* +0xA4: the change's effect, 0..11 (3: needs the moon) */
    int32_t assist[4];    /* +0xA8: the other fighter of its scene (the android Cell absorbs), -1 none */
    int32_t fresh[4];     /* +0xAD bit 4 + n: the new form starts with its clothes whole */
    int32_t fuseSet;      /* the fusions are set here */
    int32_t fuse[3];      /* +0xB4: the result, a character id, -1 none */
    int32_t fuseCost[3];  /* +0xAE */
    int32_t fuseType[3];  /* +0xB1: 1 fusion dance, 2 potara */
    int32_t fuseLead[3];  /* +0xB7: the partner its scene shows */
    int32_t partner[3][4]; /* +0xBA: whom it fuses with (the first of them in the team), -1 none */
    int32_t adLow;        /* the low bits of +0xAD, as the file has them */
    int32_t aura[4];      /* (not used: kept for the layout) */
    int32_t fuseAura[3];  /* (not used) */
    int32_t fuseAnim[3];  /* the scene's motions: 0 by the kind, 1 Goku & Vegeta's dance, 2 Goten & Trunks', 3 the SSJ4
                             pair's, 4 Potara, 5 only what the fighters' files have */
} PortTrans;

typedef struct Mod {
    char name[64];
    char dir[400];
    int chara;                 /* the fighter its files are named after; -1: unknown */
    int on;
    int raw;                   /* has files by number */
    int rawCount;
    int rawIndex[64];
    char rawPath[64][64];      /* relative to dir */
    char file[KIND_COUNT][200]; /* relative to dir; "" = none */
    long size[KIND_COUNT];
    /* mod.txt */
    int isNew;
    int target;                /* -1: chara */
    int tmpl;                  /* -1: chara */
    char white[40], yellow[40];
    int chip[3];               /* x, y, size in the portrait; size 0: the usual square */
    int artSet;                /* it has a portrait made in the manager (<mod>/portrait_custom.bin) */
    int art[4];                /* where its picture sits in the frame: x, y, width and height in percent */
    char artSrc[64];           /* the picture file it is made from (in the mod's folder); "" = the mod's own portrait */
    PortTrans tr;              /* its transformations and fusions (tr.set / tr.fuseSet: set in the manager) */
    char trName[4][64];        /* a transformation's target by mod name ("" = tr.target[k], a character id) */
    char fuseName[3][64];      /* a fusion's result by mod name ("" = tr.fuse[k], a character id) */
    uint8_t *chipData;         /* the chip built last (5504 bytes), NULL = none */
    int voice;                 /* (a new fighter) whose voice and sounds it has, -1 its template's */
    int items[4];              /* (a new fighter) Z-items it always has in a fight */
    int rank;                  /* (on) its place in enabled.txt: the lower wins where two mods want the same file */
    int charaList[8];          /* every fighter it has files of (more than one: a batch of mods in one folder) */
    int charaCount;
    char author[32], version[16], about[200]; /* mod.txt: author= version= description= (from the mod's maker) */
    int problem;               /* PROBLEM_: what is wrong with its files, 0 nothing (looked at when the library is read) */
    int warrior;               /* a warrior of Z Warriors (its folder has a warrior.txt) */
} Mod;

#define PROBLEM_PAK 1       /* a .pak / .unk that is not one (its table of contents does not fit the file) */
#define PROBLEM_BIG 2       /* a file larger than the game's buffers take */
#define PROBLEM_EMPTY 3     /* a file of 0 bytes */
#define PROBLEM_PORTRAIT 4  /* a portrait the select screens cannot take (it is left out) */
#define PROBLEM_BATCH 5     /* files of several fighters in one mod: only the first fighter's are used */
#define MAX_FILE_SIZE 0x900000 /* the model buffers (9 MB each, src/sys/heap.c) */

/* What the game's screens read about a mod (the same layout in src/menu/port_mods.h). */
typedef struct PortModInfo {
    char name[64];
    char target[64];  /* the fighter it replaces, or for a new one its template */
    char white[40];
    char yellow[40];
    int32_t chara;    /* replaced fighter / template */
    int32_t on;
    int32_t kinds;    /* bit per kind present */
    int32_t costumes;
    int32_t large;    /* a model file larger than the original game could load */
    int32_t raw;
    int32_t conflict; /* another mod that is on replaces the same fighter */
    int32_t files;
    int32_t isNew;
    int32_t skin;     /* its place on the mod grid, -1 = not on it */
    int32_t chip[3];
    int32_t named;    /* it has names of its own */
    int32_t art[4];   /* a portrait made in the manager: set, x, y, zoom */
    int32_t rival;    /* another mod that is on and wants the same files of the same fighter, -1 none */
    int32_t wins;     /* (on, with a rival) its files are the ones used */
    int32_t problem;  /* what is wrong with its files: 1 a pak that is not one, 2 too large, 3 empty, 4 bad portrait, 5 several fighters */
    int32_t priority; /* (on) its place in the order mods win in, 1 = first; 0 off */
    int32_t onCount;  /* how many mods are on */
    int32_t charas;   /* how many fighters it has files of (a folder looked at: more than one is a batch) */
    int32_t artH;     /* the portrait's height in percent (art[3] its width) */
    int32_t charaList[8]; /* the fighters it has files of (a batch: each one) */
} PortModInfo;


typedef struct PortDirEntry {
    char name[96];
    int32_t isDir;
    int32_t isMod;   /* (filled by Port_DirLook) */
    int32_t chara;
    int32_t kinds;
} PortDirEntry;

typedef struct DirItem {
    char name[96];
    int isDir;
} DirItem;

static Mod sMod[MAX_MODS];
static int sModCount;
static char *sRedirect[MAX_FILES];
static Mod *sPreview;      /* answers first whatever is on (the manager's model preview) */
static Mod sBrowseMod;     /* a folder the browser is looking at */
static int sScanned;
static int sSkin[MAX_MODS]; /* the mod grid: mod indices in its order */
static int sSkinCount;
static PortTrans sVanilla[161]; /* transformations set for fighters of the roster */
static int sMotionDirty = 1;    /* the fusions changed: which files need the scene's motions is worked out again */
static int sRankLow;       /* the rank a mod switched on gets: below all others, so it wins */
static long sModsStamp;    /* the mods folder as it was when read (mods_stamp) */
static long mods_stamp(void);

static const char *const kPrefix[] = {
#include "plat_mods_prefix.inc"
};
static const char *const kName[] = {
#include "plat_mods_names.inc"
};
#define CHARA_COUNT ((int)(sizeof(kPrefix) / sizeof(kPrefix[0])))


static const char *root(void) {
    const char *r = getenv("BT3_DATA");
    return r != NULL ? r : "gamedata";
}

static void lower(char *dst, const char *src, size_t size) {
    size_t i;
    for (i = 0; i + 1 < size && src[i] != '\0'; i++) {
        dst[i] = (char)tolower((unsigned char)src[i]);
    }
    dst[i] = '\0';
}

static int ends_with(const char *s, const char *tail) {
    size_t a = strlen(s), b = strlen(tail);
    return a >= b && strcmp(s + a - b, tail) == 0;
}

static int chara_of(const char *lname) {
    static char low[200][24]; /* the prefixes in lower case, made once */
    static size_t len[200];
    static int made;
    int best = -1, c;
    size_t bestLen = 0;

    if (!made) {
        for (c = 0; c < CHARA_COUNT && c < 200; c++) {
            lower(low[c], kPrefix[c], sizeof(low[c]));
            len[c] = strlen(low[c]);
        }
        made = 1;
    }
    for (c = 0; c < CHARA_COUNT && c < 200; c++) {
        const char *p = low[c];
        size_t n = len[c];
        if (n > bestLen && strncmp(lname, p, n) == 0 && (lname[n] == '_' || lname[n] == '.')) {
            best = c;
            bestLen = n;
        }
    }
    return best;
}

static int kind_of(const char *l) {
    static const char *const dmg[4][2] = {{"1p_dam", "1p_dmg"}, {"2p_dam", "2p_dmg"}, {"3p_dam", "3p_dmg"}, {"4p_dam", "4p_dmg"}};
    static const char *const cos[4] = {"1p", "2p", "3p", "4p"};
    int k;

    int unk = ends_with(l, ".unk"); /* (the old tools' name for the same files: Goku_0_1p.unk is Goku_0_1p.pak) */

    if (ends_with(l, ".cdbt") || (unk && strstr(l, "_img") != NULL)) {
        return 10;
    }
    if (!ends_with(l, ".pak") && !unk) {
        return -1;
    }
    for (k = 0; k < 4; k++) {
        if (strstr(l, dmg[k][0]) != NULL || strstr(l, dmg[k][1]) != NULL) {
            return 4 + k;
        }
    }
    if (strstr(l, "voice") != NULL) {
        return -1;
    }
    if (strstr(l, "anm") != NULL) {
        return 8;
    }
    if (strstr(l, "eff") != NULL) {
        return 9;
    }
    for (k = 0; k < 4; k++) {
        if (strstr(l, cos[k]) != NULL) {
            return k;
        }
    }
    return -1;
}

static long file_size(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

static int is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* The entries of a folder (not "." / ".."), each with whether it is a folder and its size, read in one go: on Windows
   one call per entry gives all three, where readdir + stat asked the disk twice (a folder of many files stuttered).
   Returns the count; at most `max`. */
typedef struct DirEnt {
    char name[200];
    int isDir;
    long size;
} DirEnt;

static int dir_read(const char *path, DirEnt *out, int max) {
    int n = 0;
#ifdef _WIN32
    char pat[720];
    WIN32_FIND_DATAA f;
    HANDLE h;

    snprintf(pat, sizeof(pat), "%s/*", path);
    h = FindFirstFileA(pat, &f);
    if (h == INVALID_HANDLE_VALUE) {
        return 0;
    }
    do {
        if (f.cFileName[0] == '.' && (f.cFileName[1] == '\0' || (f.cFileName[1] == '.' && f.cFileName[2] == '\0'))) {
            continue;
        }
        if (strlen(f.cFileName) >= sizeof(out[0].name) || (f.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0) {
            continue;
        }
        snprintf(out[n].name, sizeof(out[n].name), "%s", f.cFileName);
        out[n].isDir = (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        out[n].size = (long)f.nFileSizeLow;
        n++;
    } while (n < max && FindNextFileA(h, &f));
    FindClose(h);
#else
    DIR *d = opendir(path);
    struct dirent *e;

    while (d != NULL && n < max && (e = readdir(d)) != NULL) {
        char full[1000];
        struct stat st;

        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 || strlen(e->d_name) >= sizeof(out[0].name)) {
            continue;
        }
        snprintf(full, sizeof(full), "%s/%s", path, e->d_name);
        if (stat(full, &st) != 0) {
            continue;
        }
        snprintf(out[n].name, sizeof(out[n].name), "%s", e->d_name);
        out[n].isDir = S_ISDIR(st.st_mode);
        out[n].size = (long)st.st_size;
        n++;
    }
    if (d != NULL) {
        closedir(d);
    }
#endif
    return n;
}

#define SCAN_BUDGET 3000 /* entries one look at a folder reads at most: a huge folder is not a mod anyway */
static int sScanLeft;

static int kind_by_content(const char *path, const char *lname, long len);
static int chara_of_index(int index);

/* The number of the game's archive a file was taken out under, as the unpacking tools write it in its name
   ("Name(2002).unk"); -1 none. */
static int paren_number(const char *l) {
    const char *open = strrchr(l, '(');
    int n = 0, digits = 0;

    if (open == NULL) {
        return -1;
    }
    for (open++; isdigit((unsigned char)*open) && digits < 6; open++, digits++) {
        n = n * 10 + (*open - '0');
    }
    return digits > 0 && *open == ')' ? n : -1;
}

static void scan_dir(Mod *m, const char *rel, int depth) {
    char path[700];
    DirEnt *ents;
    int count, i;

    snprintf(path, sizeof(path), "%s/%s", m->dir, rel);
    if (sScanLeft <= 0 || (ents = malloc(sizeof(DirEnt) * 1024)) == NULL) {
        return;
    }
    count = dir_read(path, ents, sScanLeft < 1024 ? sScanLeft : 1024);
    sScanLeft -= count;
    for (i = 0; i < count; i++) {
        DirEnt *e = &ents[i];
        char sub[300], l[300];

        if (e->name[0] == '.') {
            continue;
        }
        snprintf(sub, sizeof(sub), "%s%s%s", rel, rel[0] != '\0' ? "/" : "", e->name);
        lower(l, e->name, sizeof(l));
        if (e->isDir) {
            if (depth < 4) {
                scan_dir(m, sub, depth + 1);
            }
            continue;
        }
        if ((ends_with(l, ".bin") || ends_with(l, ".unk")) && strstr(sub, "pzs3us1/") != NULL && isdigit((unsigned char)l[0])) {
            if (m->rawCount < 64 && strlen(sub) < 64) {
                m->rawIndex[m->rawCount] = atoi(l);
                strcpy(m->rawPath[m->rawCount], sub);
                m->rawCount++;
                m->raw = 1;
            }
            continue;
        }
        {
            int k = kind_of(l); /* what its name says */
            int c = chara_of(l);
            int j;
            char full[1000];
            int got;

            snprintf(full, sizeof(full), "%s/%s", m->dir, sub);
            got = kind_by_content(full, l, e->size);
            if (paren_number(l) >= 0) { /* its number in the archive says which file of a fighter it is */
                int num = paren_number(l), id = num + 1, slot;

                if (id < MODEL_FILE || id >= MODEL_FILE + CHARA_COUNT * 10) {
                    continue; /* a file of another kind (a face, a voice, a scene): none a fighter mod replaces */
                }
                slot = (id - MODEL_FILE) % 10; /* 0-3 its costumes, 4-7 damaged, 8 its motions, 9 its effects */
                if (got != (slot < 8 ? 0 : slot)) {
                    continue; /* (its number and what is inside disagree) */
                }
                k = slot;
                if (c < 0) {
                    c = chara_of_index(num);
                }
            } else if (got == 0) { /* a model: the costume (and whether damaged) its name says, else the first one free */
                if (k < 0) { /* (its name read as a pak's, whatever its ending) */
                    char as[310];

                    snprintf(as, sizeof(as), "%s.pak", l);
                    k = kind_of(as);
                }
                if (k < 0 || k > 7) {
                    for (k = 0; k < 4 && m->file[k][0] != '\0'; k++) {
                    }
                    if (k == 4) {
                        continue;
                    }
                }
            } else if (got > 0) {
                k = got;
            } else if (k >= 0 && k <= 9 && !ends_with(l, ".pak") && !ends_with(l, ".unk")) {
                k = -1; /* (a name like one, but not one inside) */
            }
            if (k < 0 || strlen(sub) >= sizeof(m->file[0])) {
                continue;
            }
            if (c >= 0) { /* (every fighter the folder has files of: more than one makes it a batch of mods) */
                for (j = 0; j < m->charaCount && m->charaList[j] != c; j++) {
                }
                if (j == m->charaCount && j < 8) {
                    m->charaList[m->charaCount++] = c;
                }
            }
            if (m->chara < 0 && c >= 0) {
                m->chara = c;
            }
            if (c >= 0 && c != m->chara) {
                continue; /* a second fighter: a batch, which an import splits into a mod per fighter */
            }
            if (m->file[k][0] == '\0') {
                strcpy(m->file[k], sub);
                m->size[k] = e->size;
            }
        }
    }
    free(ents);
}

static int chara_of_index(int index) {
    int id = index + 1;
    if (id >= MODEL_FILE && id < MODEL_FILE + CHARA_COUNT * 10) {
        return (id - MODEL_FILE) / 10;
    }
    if (id >= FACE_FILE && id < FACE_FILE + CHARA_COUNT) {
        return id - FACE_FILE;
    }
    if (id >= DATA_FILE && id < DATA_FILE + CHARA_COUNT * 2) {
        return (id - DATA_FILE) / 2;
    }
    return -1;
}

/* Nothing set: no transformation, no fusion; the types and effects a slot usually has. */
static void trans_clear(PortTrans *t) {
    int k, j;

    memset(t, 0, sizeof(*t));
    t->next = -1;
    for (k = 0; k < 4; k++) {
        t->target[k] = -1;
        t->assist[k] = -1;
        t->type[k] = k == 3 ? 1 : 0;
        t->effect[k] = k == 3 ? 1 : 2;
        t->cost[k] = k == 3 ? 0 : 1;
    }
    for (k = 0; k < 4; k++) {
        t->aura[k] = -1;
    }
    for (k = 0; k < 3; k++) {
        t->fuseAura[k] = -1;
        t->fuse[k] = -1;
        t->fuseLead[k] = -1;
        t->fuseType[k] = 1;
        t->fuseCost[k] = 3;
        for (j = 0; j < 4; j++) {
            t->partner[k][j] = -1;
        }
    }
}

/* A transformation record of mod.txt / transform.txt: cost,type,effect,assist,fresh,<target> (target: an id or
   @<mod name>, last because a name may hold commas); the old one was cost,<target>. name: the mod name, "" none. */
static void trans_parse(char *v, PortTrans *t, int k, char *name) {
    int f[5] = {0, k == 3 ? 1 : 0, k == 3 ? 1 : 2, -1, 0};
    int n = 0;
    char *p = v;

    name[0] = '\0';
    while (n < 5) {
        char *c = strchr(p, ',');
        f[n++] = atoi(p);
        if (c == NULL) {
            p = NULL;
            break;
        }
        p = c + 1;
        if (n == 1 && (*p == '@' || strchr(p, ',') == NULL)) {
            break; /* (the old record: cost, then the target) */
        }
    }
    t->cost[k] = f[0];
    t->type[k] = f[1];
    t->effect[k] = f[2];
    t->assist[k] = f[3];
    t->fresh[k] = f[4] != 0;
    t->target[k] = -1;
    if (p != NULL && *p == '@') {
        snprintf(name, 64, "%.63s", p + 1);
        name[strcspn(name, "|")] = '\0';
    } else if (p != NULL) {
        t->target[k] = atoi(p);
    }
}

static void trans_print(FILE *fp, const PortTrans *t, int k, const char *name) {
    fprintf(fp, "%d,%d,%d,%d,%d,", t->cost[k], t->type[k], t->effect[k], t->assist[k], t->fresh[k]);
    if (name != NULL && name[0] != '\0') {
        fprintf(fp, "@%s", name);
    } else {
        fprintf(fp, "%d", t->target[k] >= REF_MOD ? -1 : t->target[k]);
    }
}

/* A fusion record: cost,type,lead,partner x4,<result> (an id, or @<mod name>, last). name: the mod name, "" none. */
static void fuse_parse(char *v, PortTrans *t, int k, char *name) {
    int f[8], n;
    char *p = v;

    name[0] = '\0';
    for (n = 0; n < 8; n++) {
        if (n == 7 && *p == '@') {
            snprintf(name, 64, "%.63s", p + 1);
            name[strcspn(name, "|")] = '\0';
            f[n] = -1;
            break;
        }
        f[n] = atoi(p);
        p = strchr(p, ',');
        if (p == NULL) {
            break;
        }
        p++;
    }
    if (n < 7) {
        return;
    }
    t->fuseCost[k] = f[0];
    t->fuseType[k] = f[1];
    t->fuseLead[k] = f[2];
    for (n = 0; n < 4; n++) {
        t->partner[k][n] = f[3 + n];
    }
    t->fuse[k] = f[7];
}

static void fuse_print(FILE *fp, const PortTrans *t, int k, const char *name) {
    fprintf(fp, "%d,%d,%d,%d,%d,%d,%d,", t->fuseCost[k], t->fuseType[k], t->fuseLead[k], t->partner[k][0], t->partner[k][1], t->partner[k][2],
            t->partner[k][3]);
    if (name != NULL && name[0] != '\0') {
        fprintf(fp, "@%s", name);
    } else {
        fprintf(fp, "%d", t->fuse[k] >= REF_MOD ? -1 : t->fuse[k]);
    }
}

/* A folder as a mod (not yet in the library's list). Returns whether it holds one. */
static int load_mod(Mod *m, const char *dir, const char *name) {
    int k, any = 0;

    memset(m, 0, sizeof(*m));
    snprintf(m->dir, sizeof(m->dir), "%s", dir);
    snprintf(m->name, sizeof(m->name), "%s", name);
    m->chara = -1;
    m->target = -1;
    m->tmpl = -1;
    trans_clear(&m->tr);
    m->voice = -1;
    sScanLeft = SCAN_BUDGET;
    scan_dir(m, "", 0);
    for (k = 0; k < KIND_COUNT; k++) {
        any |= m->file[k][0] != '\0';
    }
    if (m->raw && m->chara < 0) {
        for (k = 0; k < m->rawCount && m->chara < 0; k++) {
            m->chara = chara_of_index(m->rawIndex[k]);
        }
    }
    return any || m->raw;
}

/* ------------------------------------------------------------------------------------------------- mod.txt */
static void read_settings(Mod *m) {
    char path[600], line[256];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/mod.txt", m->dir);
    fp = fopen(path, "r");
    if (fp == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *v = strchr(line, '=');
        int k;
        line[strcspn(line, "\r\n")] = '\0';
        if (v == NULL) {
            continue;
        }
        *v++ = '\0';
        if (strcmp(line, "mode") == 0) {
            m->isNew = strcmp(v, "new") == 0;
        } else if (strcmp(line, "target") == 0) {
            m->target = atoi(v);
        } else if (strcmp(line, "template") == 0) {
            m->tmpl = atoi(v);
        } else if (strcmp(line, "voice") == 0) {
            m->voice = atoi(v);
        } else if (strcmp(line, "items") == 0) {
            sscanf(v, "%d,%d,%d,%d", &m->items[0], &m->items[1], &m->items[2], &m->items[3]);
        } else if (strcmp(line, "author") == 0) {
            snprintf(m->author, sizeof(m->author), "%s", v);
        } else if (strcmp(line, "version") == 0) {
            snprintf(m->version, sizeof(m->version), "%s", v);
        } else if (strcmp(line, "description") == 0 || strcmp(line, "about") == 0) {
            snprintf(m->about, sizeof(m->about), "%s", v);
        } else if (strcmp(line, "white") == 0) {
            snprintf(m->white, sizeof(m->white), "%s", v);
        } else if (strcmp(line, "yellow") == 0) {
            snprintf(m->yellow, sizeof(m->yellow), "%s", v);
        } else if (strcmp(line, "art") == 0) {
            int n = sscanf(v, "%d,%d,%d,%d", &m->art[0], &m->art[1], &m->art[2], &m->art[3]);

            m->artSet = n >= 3;
            if (n == 3) { /* (from before width and height were apart) */
                m->art[3] = m->art[2];
            }
        } else if (strcmp(line, "artsrc") == 0) {
            snprintf(m->artSrc, sizeof(m->artSrc), "%s", v);
        } else if (strcmp(line, "chip") == 0) {
            sscanf(v, "%d,%d,%d", &m->chip[0], &m->chip[1], &m->chip[2]);
        } else if (strcmp(line, "next") == 0) {
            m->tr.next = atoi(v);
            m->tr.set = 1;
        } else if (strncmp(line, "trans", 5) == 0 && (k = line[5] - '0') >= 0 && k < 4 && line[6] == '\0') {
            m->tr.set = 1;
            trans_parse(v, &m->tr, k, m->trName[k]);
        } else if (strcmp(line, "aura") == 0) {
            sscanf(v, "%d,%d,%d,%d", &m->tr.aura[0], &m->tr.aura[1], &m->tr.aura[2], &m->tr.aura[3]);
        } else if (strcmp(line, "fuseextra") == 0) {
            sscanf(v, "%d,%d,%d,%d,%d,%d", &m->tr.fuseAura[0], &m->tr.fuseAura[1], &m->tr.fuseAura[2], &m->tr.fuseAnim[0], &m->tr.fuseAnim[1],
                   &m->tr.fuseAnim[2]);
        } else if (strncmp(line, "fuse", 4) == 0 && (k = line[4] - '0') >= 0 && k < 3 && line[5] == '\0') {
            m->tr.fuseSet = 1;
            fuse_parse(v, &m->tr, k, m->fuseName[k]);
        }
    }
    fclose(fp);
}

static void write_settings(const Mod *m) {
    char path[600];
    FILE *fp;
    int k;

    snprintf(path, sizeof(path), "%s/mod.txt", m->dir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        return;
    }
    if (m->author[0] != '\0') { /* (the maker's lines are kept) */
        fprintf(fp, "author=%s\n", m->author);
    }
    if (m->version[0] != '\0') {
        fprintf(fp, "version=%s\n", m->version);
    }
    if (m->about[0] != '\0') {
        fprintf(fp, "description=%s\n", m->about);
    }
    fprintf(fp, "mode=%s\n", m->isNew ? "new" : "replace");
    if (m->target >= 0) {
        fprintf(fp, "target=%d\n", m->target);
    }
    if (m->tmpl >= 0) {
        fprintf(fp, "template=%d\n", m->tmpl);
    }
    if (m->voice >= 0) {
        fprintf(fp, "voice=%d\n", m->voice);
    }
    if (m->items[0] || m->items[1] || m->items[2] || m->items[3]) {
        fprintf(fp, "items=%d,%d,%d,%d\n", m->items[0], m->items[1], m->items[2], m->items[3]);
    }
    if (m->white[0] != '\0') {
        fprintf(fp, "white=%s\n", m->white);
    }
    if (m->yellow[0] != '\0') {
        fprintf(fp, "yellow=%s\n", m->yellow);
    }
    if (m->chip[2] > 0) {
        fprintf(fp, "chip=%d,%d,%d\n", m->chip[0], m->chip[1], m->chip[2]);
    }
    if (m->artSrc[0] != '\0') {
        fprintf(fp, "artsrc=%s\n", m->artSrc);
    }
    if (m->artSet) {
        fprintf(fp, "art=%d,%d,%d,%d\n", m->art[0], m->art[1], m->art[2], m->art[3]);
    }
    if (m->tr.set) { /* trans<n>=cost,type,effect,assist,fresh,<target> */
        for (k = 0; k < 4; k++) {
            fprintf(fp, "trans%d=", k);
            trans_print(fp, &m->tr, k, m->trName[k]);
            fprintf(fp, "\n");
        }
        fprintf(fp, "next=%d\n", m->tr.next);
        fprintf(fp, "aura=%d,%d,%d,%d\n", m->tr.aura[0], m->tr.aura[1], m->tr.aura[2], m->tr.aura[3]);
    }
    if (m->tr.fuseSet) { /* fuse<n>=cost,type,lead,partner x4,result */
        for (k = 0; k < 3; k++) {
            fprintf(fp, "fuse%d=", k);
            fuse_print(fp, &m->tr, k, m->fuseName[k]);
            fprintf(fp, "\n");
        }
        fprintf(fp, "fuseextra=%d,%d,%d,%d,%d,%d\n", m->tr.fuseAura[0], m->tr.fuseAura[1], m->tr.fuseAura[2], m->tr.fuseAnim[0], m->tr.fuseAnim[1],
                m->tr.fuseAnim[2]);
    }
    fclose(fp);
}

static int target_of(const Mod *m) {
    if (m->isNew) {
        return m->tmpl >= 0 ? m->tmpl : m->chara;
    }
    return m->target >= 0 ? m->target : m->chara;
}

/* ------------------------------------------------------------------------------------------------- files */
/* The host path of what mod m has for `kind` (0..10, 11 / 12 the gauge faces), "" = nothing. */
static void mod_path(const Mod *m, int kind, char *out, size_t size) {
    int k;

    out[0] = '\0';
    if (kind == 10 && m->artSet) { /* the portrait made in the manager */
        snprintf(out, size, "%s/portrait_custom.bin", m->dir);
        if (file_size(out) > 0) {
            return;
        }
        out[0] = '\0';
    }
    if (kind < KIND_COUNT && m->file[kind][0] != '\0') {
        snprintf(out, size, "%s/%s", m->dir, m->file[kind]);
        return;
    }
    for (k = 0; k < m->rawCount; k++) { /* files by number: what each is, from the fighter they are numbered for */
        int id = m->rawIndex[k] + 1;
        int what = -1;
        if (m->chara >= 0 && id >= MODEL_FILE + m->chara * 10 && id < MODEL_FILE + m->chara * 10 + 10) {
            what = id - (MODEL_FILE + m->chara * 10);
        } else if (m->chara >= 0 && id == FACE_FILE + m->chara) {
            what = 10;
        } else if (m->chara >= 0 && (id == DATA_FILE + m->chara * 2 || id == DATA_FILE + m->chara * 2 + 1)) {
            what = KIND_FACE + (id - DATA_FILE - m->chara * 2);
        }
        if (what == kind) {
            snprintf(out, size, "%s/%s", m->dir, m->rawPath[k]);
            return;
        }
    }
}

/* The same with the usual stand-ins: a costume the mod lacks is its first, a damaged version its costume. */
static void mod_file(const Mod *m, int kind, char *out, size_t size) {
    mod_path(m, kind, out, size);
    if (out[0] == '\0' && kind >= 4 && kind < 8) {
        mod_path(m, kind - 4, out, size);
        if (out[0] == '\0') {
            mod_path(m, 0, out, size);
        }
    } else if (out[0] == '\0' && kind >= 1 && kind < 4) {
        mod_path(m, 0, out, size);
    }
}

/* A file to read. The disc's files by number ("<root>/pzs3us1/01453.bin") are loose only where the disc was
   extracted; the Android app keeps the archives whole (<root>/disc/DATA/PZS3US1.AFS: "AFS", the count, then offset
   and size per entry), so what is not there as a file is read out of the archive. Without this no warrior could be
   made there (its donors "not found on the disc"). On its own, with no lock and no table kept: the mods are scanned
   from inside the file layer's own lock (plat_fcache.c Port_FileSize), so nothing of that layer may be called here. */
static FILE *disc_open(const char *path) {
    FILE *fp = fopen(path, "rb"), *in, *mem;
    char afs[600], part[512];
    uint8_t head[8], entry[8];
    const char *r = root(), *p;
    long base, size, left;
    size_t n = strlen(r), k;
    uint32_t count;
    int index, m, c;

    if (fp != NULL || strncmp(path, r, n) != 0 || path[n] != '/' || strncmp(path + n + 1, "pzs3us", 6) != 0 ||
        (p = strchr(path + n + 1, '/')) == NULL || sscanf(p + 1, "%d.bin", &index) != 1 || index < 0) {
        return fp;
    }
    m = snprintf(afs, sizeof(afs), "%s/disc/DATA/", r);
    for (c = (int)n + 1; path + c < p && m < (int)sizeof(afs) - 5; c++) {
        afs[m++] = (char)toupper((unsigned char)path[c]);
    }
    snprintf(afs + m, sizeof(afs) - (size_t)m, ".AFS");
    if ((in = fopen(afs, "rb")) == NULL) {
        return NULL;
    }
    if (fread(head, 1, 8, in) != 8 || memcmp(head, "AFS", 3) != 0) {
        fclose(in);
        return NULL;
    }
    count = (uint32_t)head[4] | (uint32_t)head[5] << 8 | (uint32_t)head[6] << 16 | (uint32_t)head[7] << 24;
    if ((uint32_t)index >= count || fseek(in, 8 + (long)index * 8, SEEK_SET) != 0 || fread(entry, 1, 8, in) != 8) {
        fclose(in);
        return NULL;
    }
    base = (long)((uint32_t)entry[0] | (uint32_t)entry[1] << 8 | (uint32_t)entry[2] << 16 | (uint32_t)entry[3] << 24);
    size = (long)((uint32_t)entry[4] | (uint32_t)entry[5] << 8 | (uint32_t)entry[6] << 16 | (uint32_t)entry[7] << 24);
    if (size <= 0) {
        fclose(in);
        return NULL;
    }
    mem = fmemopen(NULL, (size_t)size + 1, "w+"); /* (its own memory, freed on fclose) */
    fseek(in, base, SEEK_SET);
    for (left = size; mem != NULL && left > 0; left -= (long)k) {
        k = fread(part, 1, left < (long)sizeof(part) ? (size_t)left : sizeof(part), in);
        if (k == 0 || fwrite(part, 1, k, mem) != k) {
            fclose(mem);
            mem = NULL;
        }
    }
    fclose(in);
    if (mem != NULL) {
        fflush(mem);
        rewind(mem);
    }
    return mem;
}

/* Whether a file is a portrait the select screens can take: packed (unpacked size, packed size, data), no larger
   than their buffers. Anything else would be unpacked over whatever lies behind those buffers. */
static int portrait_ok(const char *path) {
    FILE *fp = disc_open(path);
    uint32_t h[2];
    long len;
    int ok = 0;

    if (fp == NULL) {
        return 0;
    }
    if (fread(h, 4, 2, fp) == 2) {
        fseek(fp, 0, SEEK_END);
        len = ftell(fp);
        ok = h[0] >= 0x100 && h[0] <= 0x20800 && h[1] > 0 && (long)h[1] + 8 <= len && len <= 0x16800;
    }
    fclose(fp);
    return ok;
}

/* The count of parts a file holds if it is a pak (a count, then that many + 1 offsets, the first part right after the
   table), else 0. The rest of the table is not held to order or to the file's end: tools trim the padding at the
   end and the game takes such files. */
static uint32_t pak_count(const char *path, long len) {
    FILE *fp = disc_open(path);
    uint32_t h[2] = {0, 0};
    uint32_t n = 0;

    if (fp == NULL) {
        return 0;
    }
    if (fread(h, 4, 2, fp) == 2 && h[0] >= 1 && h[0] < 4096 && (long)(8 + h[0] * 4) <= len && h[1] >= 4 * (h[0] + 1) && h[1] <= 4 * (h[0] + 2) + 0x800 &&
        (long)h[1] < len) {
        n = h[0];
    }
    fclose(fp);
    return n;
}

static int pak_ok(const char *path, long len) {
    return pak_count(path, len) > 0;
}

/* What a file is by what it holds (not by its name): 0 a costume's model (252 parts), 8 the animations (440), 9 the
   effects (a handful), 10 a portrait; -1 none of them. */
static int kind_by_content(const char *path, const char *lname, long len) {
    static const char *const other[] = {".png", ".jpg", ".jpeg", ".bmp", ".gif", ".txt", ".md", ".ini", ".zip", ".7z", ".rar",
                                        ".exe", ".dll", ".db", ".json", ".xml", ".mp4", ".wav", ".mp3", ".ogg", ".adx", ".afs"};
    uint32_t n;
    size_t k;

    if (len < 16 || len > MAX_FILE_SIZE * 2) {
        return -1;
    }
    for (k = 0; k < sizeof(other) / sizeof(other[0]); k++) {
        if (ends_with(lname, other[k])) {
            return -1;
        }
    }
    if (strcmp(lname, "portrait_custom.bin") == 0 || strcmp(lname, "hud_photo.raw") == 0 || strstr(lname, "voice") != NULL) {
        return -1;
    }
    n = pak_count(path, len);
    if (n >= 200 && n < 320) {
        return 0;
    }
    if (n >= 320 && n < 1024) {
        return 8;
    }
    if (n >= 1 && n <= 32) {
        return 9;
    }
    if (n == 0 && portrait_ok(path)) {
        return 10;
    }
    return -1;
}

/* What is wrong with a mod's files (PROBLEM_), 0 nothing. */
static int check_files(const Mod *m) {
    int k;

    if (m->charaCount > 1) {
        return PROBLEM_BATCH;
    }
    for (k = 0; k < KIND_COUNT; k++) {
        char path[700];
        long len;

        if (m->file[k][0] == '\0') {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", m->dir, m->file[k]);
        len = m->size[k];
        if (len <= 0) {
            return PROBLEM_EMPTY;
        }
        if (len > MAX_FILE_SIZE) {
            return PROBLEM_BIG;
        }
        if (k == 10 ? !portrait_ok(path) : !pak_ok(path, len)) {
            return k == 10 ? PROBLEM_PORTRAIT : PROBLEM_PAK;
        }
    }
    return 0;
}

static void set(int index, const Mod *m, int kind) {
    char path[700];

    if (index < 0 || index >= MAX_FILES || sRedirect[index] != NULL) {
        return;
    }
    mod_file(m, kind, path, sizeof(path));
    if (path[0] != '\0' && (kind != 10 || portrait_ok(path))) {
        sRedirect[index] = strdup(path);
    }
}

static void add_files(const Mod *m) {
    int t = target_of(m);
    int k;

    if (m->raw && m->chara == t) { /* files by number for this very fighter: as they are, whatever they are */
        for (k = 0; k < m->rawCount; k++) {
            char path[700];
            int index = m->rawIndex[k];
            if (index >= 0 && index < MAX_FILES && sRedirect[index] == NULL) {
                snprintf(path, sizeof(path), "%s/%s", m->dir, m->rawPath[k]);
                sRedirect[index] = strdup(path);
            }
        }
    }
    if (t >= 0) {
        int base = MODEL_FILE + t * 10 - 1;
        for (k = 0; k < 10; k++) {
            set(base + k, m, k);
        }
        set(FACE_FILE + t - 1, m, 10);
        set(DATA_FILE + t * 2 - 1, m, KIND_FACE);
        set(DATA_FILE + t * 2, m, KIND_FACE + 1);
    }
}

/* What of its fighter a mod takes over, as bits of kinds 0..12. A mod with any costume answers all eight model
   files (a costume it lacks is its first), so those count as one. */
static int takes(const Mod *m) {
    int k, bits = 0;

    for (k = 0; k < KIND_FACE + 2; k++) {
        char path[700];
        mod_path(m, k, path, sizeof(path));
        if (path[0] != '\0') {
            bits |= 1 << k;
        }
    }
    return (bits & 0xFF) != 0 ? bits | 0xFF : bits;
}

/* Whether two mods that replace a fighter cannot both be used: the same fighter, and files of the same kind (one's
   model with the other's animations would be a mix neither was made for). */
static int clash(const Mod *a, const Mod *b) {
    int t = target_of(a);
    return a != b && !a->isNew && !b->isNew && t >= 0 && t == target_of(b) && (takes(a) & takes(b)) != 0;
}

/* The mods that are on, in the order they win (lowest rank first). Returns how many. */
static int on_order(int *out) {
    int i, j, n = 0;

    for (i = 0; i < sModCount; i++) {
        if (sMod[i].on) {
            for (j = n; j > 0 && sMod[out[j - 1]].rank > sMod[i].rank; j--) {
                out[j] = out[j - 1];
            }
            out[j] = i;
            n++;
        }
    }
    return n;
}

static void rebuild(void) {
    static int order[MAX_MODS];
    static int claimed[161]; /* per fighter: the kinds a mod already answers */
    char path[512], line[128];
    FILE *fp;
    int i, j, n;

    for (i = 0; i < MAX_FILES; i++) {
        free(sRedirect[i]);
        sRedirect[i] = NULL;
    }
    memset(claimed, 0, sizeof(claimed));
    if (sPreview != NULL) {
        static Mod tmp; /* shown in the place of its fighter (target or template), whether it is on or a new one */
        tmp = *sPreview;
        tmp.target = target_of(sPreview); /* the fighter the manager shows it as */
        if (tmp.target < 0) {
            tmp.target = 0; /* files named after no fighter of the roster: shown in the first one's place */
        }
        tmp.isNew = 0;
        add_files(&tmp);
        if (tmp.target < 161) {
            claimed[tmp.target] |= takes(&tmp);
        }
    }
    n = on_order(order);
    for (j = 0; j < n; j++) {
        Mod *m = &sMod[order[j]];
        int t = target_of(m);

        if (m->isNew) {
            continue;
        }
        if (t >= 0 && t < 161) { /* a mod is used whole or not at all: never half of it under another's files */
            int k = takes(m);
            if ((claimed[t] & k) != 0) {
                continue;
            }
            claimed[t] |= k;
        }
        add_files(m);
    }
    /* the mod grid: the order of <data>/mods/roster.txt, then whatever is new and on */
    sSkinCount = 0;
    snprintf(path, sizeof(path), "%s/mods/roster.txt", root());
    fp = fopen(path, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            for (i = 0; i < sModCount; i++) {
                if (sMod[i].on && sMod[i].isNew && target_of(&sMod[i]) >= 0 && strcmp(sMod[i].name, line) == 0) {
                    for (j = 0; j < sSkinCount && sSkin[j] != i; j++) {
                    }
                    if (j == sSkinCount) {
                        sSkin[sSkinCount++] = i;
                    }
                }
            }
        }
        fclose(fp);
    }
    for (i = 0; i < sModCount; i++) {
        if (sMod[i].on && sMod[i].isNew && target_of(&sMod[i]) >= 0) { /* (without a template there is nothing to be) */
            for (j = 0; j < sSkinCount && sSkin[j] != i; j++) {
            }
            if (j == sSkinCount) {
                sSkin[sSkinCount++] = i;
            }
        }
    }
}

static void save(void) {
    char path[512];
    FILE *fp;
    int i, k;

    snprintf(path, sizeof(path), "%s/mods/enabled.txt", root());
    fp = fopen(path, "w");
    if (fp != NULL) {
        static int order[MAX_MODS];
        int n = on_order(order);
        for (i = 0; i < n; i++) { /* (in the order they win) */
            fprintf(fp, "%s\n", sMod[order[i]].name);
        }
        fclose(fp);
    }
    snprintf(path, sizeof(path), "%s/mods/roster.txt", root());
    fp = fopen(path, "w");
    if (fp != NULL) {
        for (i = 0; i < sSkinCount; i++) {
            fprintf(fp, "%s\n", sMod[sSkin[i]].name);
        }
        fclose(fp);
    }
    snprintf(path, sizeof(path), "%s/mods/transform.txt", root());
    fp = fopen(path, "w");
    if (fp != NULL) { /* a line per fighter of the roster set here: <id>|T<set>|<4 records>|N<next>|F<fuseSet>|<3 fusions> */
        for (i = 0; i < 161; i++) {
            const PortTrans *t = &sVanilla[i];
            if (t->set || t->fuseSet) {
                fprintf(fp, "%d|T%d", i, t->set);
                for (k = 0; k < 4; k++) {
                    fprintf(fp, "|");
                    trans_print(fp, t, k, t->target[k] >= REF_MOD && t->target[k] - REF_MOD < sModCount ? sMod[t->target[k] - REF_MOD].name : NULL);
                }
                fprintf(fp, "|N%d|F%d", t->next, t->fuseSet);
                for (k = 0; k < 3; k++) {
                    fprintf(fp, "|");
                    fuse_print(fp, t, k, t->fuse[k] >= REF_MOD && t->fuse[k] - REF_MOD < sModCount ? sMod[t->fuse[k] - REF_MOD].name : NULL);
                }
                fprintf(fp, "|A%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", t->aura[0], t->aura[1], t->aura[2], t->aura[3], t->fuseAura[0], t->fuseAura[1],
                        t->fuseAura[2], t->fuseAnim[0], t->fuseAnim[1], t->fuseAnim[2]);
            }
        }
        fclose(fp);
    }
    sModsStamp = mods_stamp(); /* (what was written here is no change from outside) */
}

static int by_name(const void *a, const void *b) {
    return strcmp(((const Mod *)a)->name, ((const Mod *)b)->name);
}

static int mod_by_name(const char *name) {
    int i;
    for (i = 0; i < sModCount; i++) {
        if (strcmp(sMod[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* What tells a change of the mods folder: its time of last change and its number of entries. */
static long mods_stamp(void) {
    char path[512];
    struct stat st;
    DirEnt *ents = malloc(sizeof(DirEnt) * 512);
    long n = 0;

    snprintf(path, sizeof(path), "%s/mods", root());
    if (ents != NULL) {
        n = dir_read(path, ents, 512);
        free(ents);
    }
    return stat(path, &st) == 0 ? (long)((unsigned long)st.st_mtime * 31u + (unsigned long)n) : n;
}

/* ------------------------------------------------------------------------------------------------- warriors */
/*
 * A custom warrior (the Custom Character System, stage 1): a mod folder holding warrior.txt, from which the mod's
 * files are made out of the disc's fighters. Its lines (a fighter is its id or its roster name):
 *   name=<white name>    title=<yellow name>
 *   body=<fighter>       its models (every costume and damaged version), height, portrait and gauge face
 *   moves=<fighter>      its statistics, melee, ki blasts, techniques, skills, cutscenes, move names and animations
 *   effects=<fighter>    its effect packs (default: moves)
 *   voice=<fighter>      its voice and lip motion (default: moves)
 *   (stage 2: each takes one thing from another fighter in place of the moves fighter's)
 *   blast1_1=<fighter>[:<1|2>]  its first Blast 1 skill: that fighter's first (or second) one, with its animation,
 *   blast1_2=<fighter>[:<1|2>]  effect pack and name; blast1_2 the second (default: that fighter's second)
 *   finishers=<fighter>  its melee finisher set: which finisher each step of the rush chain ends in, the Max Power
 *                        finishers, the finisher bits and the finishers' animations
 *   render=<fighter>     the large portrait of the select screens (default: the body's; a picture of one's own is
 *                        made in the Z Warriors editor as the Mod Manager's portrait maker makes one)
 *   hud=<fighter>        the face of the battle gauge (default: the body's)
 *   colour=<zone>,<r>,<g>,<b>  a colour zone of the body given a new colour (warrior_recolour; up to 8)
 *   style=<fighter>      the fighter the game counts it as (health bars, built-in Z-item stats, AI) and its ki,
 *                        blast gauge and Max Power rates
 *   (stage 3)
 *   blast2_1=<fighter>[:<1|2|3>]  its first Blast 2: that fighter's first Blast 2 (2: its second, 3: its Ultimate),
 *   blast2_2=<fighter>[:<1|2|3>]  with the motions, effect pack, camera work, cutscene placement and name; blast2_2
 *   ultimate=<fighter>[:<1|2|3>]  its second Blast 2 and ultimate its Ultimate (default: the same slot)
 * When warrior.txt is newer than what was made from it (.warrior_built, which lists the files made), the files are
 * made again: <moves prefix>_1p.pak .. _4p_dmg.pak (the body's model files with the other fighters' parts put in),
 * _anm.pak, _eff.pak and _img.cdbt (copies), the gauge faces by number, and mod.txt: a new fighter of the mod grid
 * whose template is the moves fighter, without transformations or fusions unless the manager set some. A warrior
 * made for the first time is switched on. What went wrong is written to warrior_error.txt in the folder.
 *
 * The parts of a model file (252, counted from 0): 17 the parameter block (the moves fighter's, with the body's
 * height and radius at +4 / +8: BobjParam), 18..21 and 23..51 the move data (tables, cutscenes, cameras, names),
 * 52.. the lip motion of the voice lines; everything else is the body (mesh, textures, mouth shapes, eyes, 22).
 */
#define WARRIOR_PARTS 252
#define WARRIOR_VERSION "#warrior 47" /* first line of .warrior_built: a warrior made by an older maker is made again */
#define WARRIOR_ANIM_ROOM 0x160800  /* the animation file's buffer (BTL_RES_ANIM0_SIZE) */
#define WARRIOR_EFF_ROOM 0xCE800    /* the effect file's buffer (BTL_RES_ANIM1_SIZE) */

/* What a warrior is made of: fighter ids, -1 = not given (the moves fighter's own). */
#define WARRIOR_COLOURS 48 /* colour= lines a warrior keeps (ZW_COLOURS of port_zw.h: the same) */
#define WARRIOR_EDITS 32   /* edit= lines */
#ifndef WP_EDIT_WORDS
#define WP_EDIT_WORDS 12
#endif
typedef struct WarriorSpec {
    int body, moves, effects, voice, style, finishers;
    int render, hud;  /* the select screen's large portrait and the battle gauge's face (default: the body's) */
    int skill[2];     /* Blast 1 slot s: the fighter it comes from */
    int skillFrom[2]; /* ... and which of that fighter's two slots */
    int tech[3];      /* technique slot 2 + t (Blast 2, Blast 2, Ultimate): the fighter it comes from */
    int techFrom[3];  /* ... and which of that fighter's three (0, 1 the Blast 2s, 2 the Ultimate) */
    int colours;      /* colour= lines: the zone and the new colour (0xBBGGRR) of each */
    int colourKey[WARRIOR_COLOURS];
    uint32_t colourRgb[WARRIOR_COLOURS];
    int part[12];     /* hair= head= face= chest= waist= upperarms= forearms= hands= hips= shins= feet= accessories=: whose
                         model part (plat_warrior_parts.inc), -1 the body's */
    int size[8];      /* headsize= hairsize= handsize= footsize= arms= torso= legs=: percent, <= 0 as it is */
    int layer[12];    /* +hair= +head= ...: another fighter's part layered over each, -1 none */
    int colourPart[WARRIOR_COLOURS]; /* each colour= line's parts (1 << slot), 0 all: those parts' own colours */
    int stance;       /* stance=: whose standing and idle motions (the menus' pose, the battle idles), -1 the moves fighter's */
    int auraFrom;     /* aurafrom=: whose aura kind (parameter +3: the aura's flames and colours, its light on the body, the
                         ki blasts' trails), -1 the moves fighter's */
    int anim[9];      /* anim_move= .. anim_guard=, anim_intro= anim_victory= anim_taunt=: whose motions of each kind (kWarriorAnimCat), -1 the moves
                         fighter's; what the moves do (move data, damage) stays the moves fighter's */
    int skillFx[2], skillFxFrom[2]; /* blast1_1fx= blast1_2fx=: a Blast 1 skill's look (its effect pack and the skill
                                       table's effect rows) from another's, -1 its own */
    int techFx[3], techFxFrom[3]; /* blast2_1fx= blast2_2fx= ultimatefx=: whose technique's effects each has, -1 its own */
    int edits;        /* edit= lines: the pieces' edits (plat_warrior_parts.inc, WP_EDIT_WORDS each) */
    int edit[WARRIOR_EDITS * WP_EDIT_WORDS];
    char moveName[5][32]; /* blast1_1name= .. ultimatename=: a technique's name of its own, "" its own */
    int techColour[3];    /* blast2_1colour= blast2_2colour= ultimatecolour=: 0x1000000 | 0xBBGGRR, <= 0 its own */
    int skillColour[2];   /* blast1_1colour= blast1_2colour=: a Blast 1 skill's effects' colour, <= 0 their own */
    int bodyScale;        /* bodyscale=: the whole warrior's size in percent (model, motions, its size in the fight), <= 0 100 */
    int auraColour;       /* aura=: the aura's, the ki's and the common effects' colour, 0x1000000 | 0xBBGGRR, <= 0 their own */
    int partCos[12], layerCos[12], bodyCos; /* ", costume N" after a part's, a layer's, the body's fighter: always that
                                               costume of theirs (1..4); 0: the warrior's own costume's */
    char look[4][64]; /* damaged= look2= look3= look4=: another warrior's look (its folder) worn as the damaged look of every
                         costume, as costumes 2..4 (with their own damaged versions); "" its own */
    int xlayer[4], xslot[4], xcos[4]; /* extra1= .. extra4=: more layers (wp_xSlot), -1 none */
    int pose[16][3];                  /* pose=: the stance maker's turns of each bone, degrees */
    int shadeFrom;                    /* shadefrom=: whose shading and glow (each part the same part's), -1 its own */
    int dynShade;                     /* dynamic=: 0 as the shading fighter, 1 on, 2 off */
    char formTo[4][64];               /* form1= .. form4=: the warriors it transforms into ("" none) */
    int formCost[4], formKind[4];     /* their ki bars, kind (0 power up, 1 back to base) */
    int camFrom[4], camSlot[4];       /* cam_blast2_1= cam_blast2_2= cam_ultimate_in= cam_ultimate_loop=: whose technique's
                                         camera (-1 the technique's own) */
    int32_t labSet[5], lab[5][16];    /* lab_blast1_1= .. lab_ultimate=: the Technique Lab (warrior_lab_apply) */
    int32_t texShade[16][2];          /* shade_tex=: shading by texture (wp_txKey) */
    int damage;       /* damage=: <= 0 the fighters' own damaged textures, 1 scuffed, 2 battle-torn (painted over the clean
                         ones: warrior_damage_file), 3 clean */
} WarriorSpec;

static const char *const kWarriorDamage[4] = {"own", "scuffed", "torn", "clean"};

/* The cameras' keys: Blast 2 (1), Blast 2 (2), the Ultimate's opening, the Ultimate's beam / hit cinematic. */
static const char *const kWarriorCamKey[4] = {"cam_blast2_1", "cam_blast2_2", "cam_ultimate_in", "cam_ultimate_loop"};

static int warrior_cam_key(const char *key) {
    int k;

    for (k = 0; k < 4; k++) {
        if (strcmp(key, kWarriorCamKey[k]) == 0) {
            return k;
        }
    }
    return -1;
}

/* ---- the stance maker: each of 16 bones turned (x, y, z degrees, in its own frame) on every key of the standing
   motions (the menus' pose, model part 35; the stance's motions of the animation file), so the stance keeps its
   breathing and sway with the pose on top. A bone those motions leave at rest gets a track of its own (one key). ---- */
static const int kPoseNode[16] = {47, 46, 17, 16, 19, 20, 21, 33, 34, 35, 9, 10, 11, 13, 14, 15};

static int pose_any(const int (*p)[3]) {
    int b;

    for (b = 0; b < 16; b++) {
        if (p[b][0] != 0 || p[b][1] != 0 || p[b][2] != 0) {
            return 1;
        }
    }
    return 0;
}

static void pose_quat_mul(float *o, const float *a, const float *b) { /* (x, y, z, w) */
    float r[4];

    r[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    r[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    r[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    r[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
    memcpy(o, r, sizeof(r));
}

static void pose_unpack(float *v, uint64_t packed) { /* (src/sys/math3d.c Quat_Unpack) */
    int largest = (int)(packed >> 60), i;
    float sum = 0.0f;

    for (i = 0; i < 3; i++) {
        v[i] = ((float)((uint32_t)(packed >> (i * 20)) & 0xFFFFF) * (1.0f / 1048575.0f) - 0.5f) * 1.41421356f;
        sum += v[i] * v[i];
    }
    for (i = 3; i > largest; i--) {
        v[i] = v[i - 1];
    }
    v[largest] = sum < 1.0f ? sqrtf(1.0f - sum) : 0.0f;
}

static uint64_t pose_pack(const float *q) { /* (Quat_Pack) */
    float v[4], best;
    int largest = 0, i, k;
    uint64_t packed = 0;

    memcpy(v, q, sizeof(v));
    best = v[0] < 0.0f ? -v[0] : v[0];
    for (i = 1; i < 4; i++) {
        float a = v[i] < 0.0f ? -v[i] : v[i];

        if (best < a) {
            best = a;
            largest = i;
        }
    }
    if (v[largest] < 0.0f) {
        for (i = 0; i < 4; i++) {
            v[i] = -v[i];
        }
    }
    for (i = largest; i < 3; i++) {
        v[i] = v[i + 1];
    }
    for (i = 0; i < 3; i++) {
        k = (int)((v[i] * 0.70710677f + 0.5f) * 1048575.0f);
        k = k < 0 ? 0 : k > 0xFFFFF ? 0xFFFFF : k;
        packed |= (uint64_t)k << (i * 20);
    }
    return packed | ((uint64_t)largest << 60);
}

/* Motion `an` (uncompressed, size bytes) with the pose on it: malloc'ed, *outSize its size; NULL when it is not one. */
static uint8_t *pose_apply(const uint8_t *an, uint32_t size, const int (*p)[3], uint32_t *outSize) {
    uint8_t *m;
    uint32_t end = (size + 3) & ~3u;
    int b;

    if (size < 0x94 || (m = calloc(1, end + 16 * 16)) == NULL) {
        return NULL;
    }
    memcpy(m, an, size);
    for (b = 0; b < 16; b++) {
        float q[4] = {0.0f, 0.0f, 0.0f, 1.0f}, h[4];
        int ax, node = kPoseNode[b];
        uint16_t off, flags, count, k;
        uint32_t at;

        if (p[b][0] == 0 && p[b][1] == 0 && p[b][2] == 0) {
            continue;
        }
        for (ax = 0; ax < 3; ax++) { /* (x, then y, then z, each about the bone's own axis) */
            float a = (float)p[b][ax] * (3.14159265f / 360.0f);
            float r[4] = {0.0f, 0.0f, 0.0f, cosf(a)};

            r[ax] = sinf(a);
            pose_quat_mul(h, q, r);
            memcpy(q, h, sizeof(q));
        }
        memcpy(&off, m + 6 + node * 2, 2);
        at = (uint32_t)off * 4;
        if (off != 0 && at + 4 <= size) {
            memcpy(&flags, m + at, 2);
            memcpy(&count, m + at + 2, 2);
            for (k = 0; k < count; k++) {
                uint32_t qa = flags & 1 ? at + 4 + k * 8u : at + 4 + k * 0x18u + 0x10;
                uint64_t v;
                float key[4];

                if (qa + 8 > size) {
                    break;
                }
                memcpy(&v, m + qa, 8);
                pose_unpack(key, v);
                pose_quat_mul(h, key, q);
                v = pose_pack(h);
                memcpy(m + qa, &v, 8);
            }
        } else if (end / 4 < 0xFFFF) { /* (at rest in it: a track of its own, one key) */
            uint64_t v = pose_pack(q);
            uint16_t one = 1, zero = 0, word = (uint16_t)(end / 4);

            memcpy(m + end, &one, 2);
            memcpy(m + end + 2, &one, 2);
            memcpy(m + end + 4, &v, 8);
            memcpy(m + end + 12, &zero, 2);
            memcpy(m + 6 + node * 2, &word, 2);
            end += 16;
        }
    }
    *outSize = end;
    return m;
}

/* "form<n>=<cost>,<kind>,<warrior folder>": which (0..3), else -1. */
static int warrior_form_of(const char *key, const char *v, int *cost, int *kind, char *to) {
    int n = 0;

    if (strncmp(key, "form", 4) != 0 || key[4] < '1' || key[4] > '4' || key[5] != '\0' || sscanf(v, "%d,%d,%n", cost, kind, &n) < 2 || n <= 0) {
        return -1;
    }
    snprintf(to, 64, "%s", v + n);
    return key[4] - '1';
}

/* A track's rotation at its first key (identity without one). */
static void pose_first_rot(const uint8_t *an, uint32_t size, int node, float *q) {
    uint16_t off, flags, count;
    uint32_t at;
    uint64_t v;

    q[0] = q[1] = q[2] = 0.0f;
    q[3] = 1.0f;
    memcpy(&off, an + 6 + node * 2, 2);
    at = (uint32_t)off * 4;
    if (off == 0 || at + 4 > size) {
        return;
    }
    memcpy(&flags, an + at, 2);
    memcpy(&count, an + at + 2, 2);
    if (count == 0 || at + (flags & 1 ? 12u : 0x1Cu) > size) {
        return;
    }
    memcpy(&v, an + at + 4 + (flags & 1 ? 0 : 0x10), 8);
    pose_unpack(q, v);
}

/* The pose on another standing motion (a battle idle): each posed bone is turned to where the pose has it on the menus'
   pose `ref` (its first frame, the pose on it), and keeps this motion's own movement from its first frame (its
   breathing): key' = (ref0 * pose) * (first^-1 * key). (On the menus' pose itself: pose_apply.) */
static uint8_t *pose_apply_ref(const uint8_t *an, uint32_t size, const int (*p)[3], const uint8_t *ref, uint32_t refSize, uint32_t *outSize) {
    uint8_t *m;
    uint32_t end = (size + 3) & ~3u;
    int b;

    if (size < 0x94 || refSize < 0x94 || (m = calloc(1, end + 16 * 16)) == NULL) {
        return NULL;
    }
    memcpy(m, an, size);
    for (b = 0; b < 16; b++) {
        float q[4] = {0.0f, 0.0f, 0.0f, 1.0f}, h[4], target[4], r0[4], first[4];
        int ax, node = kPoseNode[b];
        uint16_t off, flags, count, k;
        uint32_t at;

        if (p[b][0] == 0 && p[b][1] == 0 && p[b][2] == 0) {
            continue;
        }
        for (ax = 0; ax < 3; ax++) {
            float a = (float)p[b][ax] * (3.14159265f / 360.0f);
            float r[4] = {0.0f, 0.0f, 0.0f, cosf(a)};

            r[ax] = sinf(a);
            pose_quat_mul(h, q, r);
            memcpy(q, h, sizeof(q));
        }
        pose_first_rot(ref, refSize, node, r0);
        pose_quat_mul(target, r0, q); /* (where the pose has the bone) */
        pose_first_rot(m, size, node, first);
        first[0] = -first[0], first[1] = -first[1], first[2] = -first[2]; /* (its inverse) */
        memcpy(&off, m + 6 + node * 2, 2);
        at = (uint32_t)off * 4;
        if (off != 0 && at + 4 <= size) {
            memcpy(&flags, m + at, 2);
            memcpy(&count, m + at + 2, 2);
            for (k = 0; k < count; k++) {
                uint32_t qa = flags & 1 ? at + 4 + k * 8u : at + 4 + k * 0x18u + 0x10;
                uint64_t v;
                float key[4], d[4];

                if (qa + 8 > size) {
                    break;
                }
                memcpy(&v, m + qa, 8);
                pose_unpack(key, v);
                pose_quat_mul(d, first, key); /* (its own movement since its first frame) */
                pose_quat_mul(h, target, d);
                v = pose_pack(h);
                memcpy(m + qa, &v, 8);
            }
        } else if (end / 4 < 0xFFFF) {
            uint64_t v = pose_pack(target);
            uint16_t one = 1, zero = 0, word = (uint16_t)(end / 4);

            memcpy(m + end, &one, 2);
            memcpy(m + end + 2, &one, 2);
            memcpy(m + end + 4, &v, 8);
            memcpy(m + end + 12, &zero, 2);
            memcpy(m + 6 + node * 2, &word, 2);
            end += 16;
        }
    }
    *outSize = end;
    return m;
}

static int warrior_pose_of(const char *v, int *bone, int *xyz) { /* "<bone>:<x>,<y>,<z>" */
    return sscanf(v, "%d:%d,%d,%d", bone, &xyz[0], &xyz[1], &xyz[2]) == 4 && *bone >= 0 && *bone < 16;
}
static const char *const kWarriorXSlot[13] = {"hair", "head", "face", "chest", "waist", "upperarms", "forearms", "hands", "hips", "shins", "feet",
                                              "accessories", "tail"}; /* (12, the tail: what hangs from a fighter's waist and hips) */

/* "extra1=hair:Goku (End) Super Saiyan 3, costume 2": which more layer (0..3), else -1; its slot, fighter, costume. */
static int warrior_costume_of(char *v);
static int warrior_fighter(const char *v);
static int warrior_xlayer_of(const char *key, char *v, int *slot, int *who, int *cos) {
    char *colon = strchr(v, ':');
    int q;

    if (strncmp(key, "extra", 5) != 0 || key[5] < '1' || key[5] > '4' || key[6] != '\0' || colon == NULL) {
        return -1;
    }
    *colon = '\0';
    *slot = -1;
    for (q = 0; q < 13; q++) {
        if (strcmp(v, kWarriorXSlot[q]) == 0) {
            *slot = q;
        }
    }
    *cos = warrior_costume_of(colon + 1);
    *who = warrior_fighter(colon + 1);
    return *slot >= 0 ? key[5] - '1' : -1;
}

static int warrior_damage_of(const char *v) {
    int q;

    for (q = 0; q < 4; q++) {
        if (strcmp(v, kWarriorDamage[q]) == 0) {
            return q;
        }
    }
    return atoi(v) >= 0 && atoi(v) < 4 ? atoi(v) : 0;
}

static const char *const kWarriorLookKey[4] = {"damaged", "look2", "look3", "look4"};

static int warrior_look_key(const char *key) {
    int q;

    for (q = 0; q < 4; q++) {
        if (strcmp(key, kWarriorLookKey[q]) == 0) {
            return q;
        }
    }
    return -1;
}

static int warrior_look_spec(const char *folder, const WarriorSpec *base, WarriorSpec *out);

static const char *const kWarriorMoveNameKey[5] = {"blast1_1name", "blast1_2name", "blast2_1name", "blast2_2name", "ultimatename"};
static const char *const kWarriorTechColourKey[3] = {"blast2_1colour", "blast2_2colour", "ultimatecolour"};

/* A colour read as rrggbb: 0x1000000 | 0xBBGGRR. */
static int warrior_hex_colour_of(unsigned c) {
    return 0x1000000 | (int)((c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF));
}

/* A colour written rrggbb: 0x1000000 | 0xBBGGRR, 0 not one. */
static int warrior_hex_colour(const char *v) {
    unsigned int c;

    if (sscanf(v, "%6x", &c) != 1) {
        return 0;
    }
    return 0x1000000 | (int)((c & 0xFF) << 16 | (c & 0xFF00) | (c >> 16 & 0xFF));
}

/* The skill table (model part 24, BtlSkillData in include/battle/btl_tech_b.h): a structure of arrays. Each row is
   {offset, element size, count}: `count` arrays of two elements (one per slot) from `offset`, so slot s of array k is
   at offset + (k * 2 + s) * size. (The effect side reads the byte fields 0x42.. and 0x6E.. interleaved by slot:
   include/battle/eft_h.h, EftShotParam.) */
static const uint16_t kWarriorSkill[][3] = {
    {0x00, 4, 1}, {0x08, 4, 1}, {0x10, 2, 1}, {0x14, 2, 1}, {0x18, 4, 1}, {0x20, 4, 1}, {0x28, 4, 1}, {0x30, 4, 1},
    {0x38, 1, 1}, {0x3A, 1, 1}, {0x3C, 1, 1}, {0x3E, 1, 1}, {0x40, 1, 1}, {0x42, 1, 16}, {0x62, 1, 1}, {0x64, 1, 1},
    {0x66, 1, 1}, {0x68, 1, 1}, {0x6A, 1, 1}, {0x6C, 1, 1}, {0x6E, 1, 3}, {0x74, 4, 1}, {0x7C, 4, 1}, {0x84, 4, 1},
    {0x8C, 4, 1}, {0x94, 1, 1}, {0x96, 1, 1}, {0x98, 1, 1}, {0x9A, 1, 1}, {0x9C, 1, 1}, {0x9E, 1, 1}, {0xA0, 4, 1},
    {0xA8, 4, 1}, {0xB0, 4, 1}, {0xB8, 4, 1}, {0xC0, 4, 1},
};

/* The motions of the melee finishers (src/battle/btl_act_a.c: the chargeable finishers' wind-up / hold / release
   triples from 0x41, 0x44, 0x47, 0x56, 0x59, 0x90 and 0x199, and 0x93 of action 0x63). */
static const uint16_t kWarriorFinisherAnim[][2] = {{0x41, 9}, {0x56, 6}, {0x90, 4}, {0x199, 3}};

/* The technique table (model part 23, BtlSuperData): the same kind of rows, arrays of three elements (slots 2..4).
   The effect type (+0x138), the effect's bytes from +0x9F and floats from +0xD4 are interleaved arrays like the
   rest. A technique's motions are the 0x22 from 0x105 + 0x22 * t (start, charge, fire, the rush sequence's attacker
   and victim steps...), its effect pack part 2 + t of the effect file, its camera pack entry t of the common
   pack's sub-part 7, its camera cut t of model part 27, its cutscene placement table model part 40 + t (fighter
   +0x15E8), its name the move list's "!F*" line 2 + t. */
static const uint16_t kWarriorSuper[][3] = {
    {0x000, 4, 1}, {0x00C, 4, 1}, {0x018, 2, 1}, {0x01E, 2, 1}, {0x024, 4, 1}, {0x030, 4, 3}, {0x054, 4, 1},
    {0x060, 4, 1}, {0x06C, 4, 1}, {0x078, 4, 2}, {0x090, 1, 1}, {0x093, 1, 1}, {0x096, 1, 1}, {0x099, 1, 1},
    {0x09C, 1, 1}, {0x09F, 1, 17}, {0x0D4, 4, 8}, {0x134, 1, 1}, {0x138, 1, 1}, {0x13B, 1, 1}, {0x13E, 1, 1},
    {0x141, 1, 1}, {0x144, 1, 1}, {0x147, 1, 1}, {0x14A, 1, 1}, {0x14D, 1, 1}, {0x150, 1, 1}, {0x153, 1, 2},
    {0x159, 1, 1}, {0x15C, 1, 3}, {0x165, 1, 1}, {0x168, 1, 1}, {0x16C, 4, 1}, {0x178, 4, 1}, {0x184, 4, 1},
    {0x190, 4, 1}, {0x19C, 4, 1}, {0x1A8, 4, 1}, {0x1B4, 4, 1}, {0x1C0, 4, 1}, {0x1CC, 4, 1}, {0x1D8, 4, 1},
    {0x1E4, 4, 1}, {0x1F0, 4, 1}, {0x1FC, 4, 1}, {0x208, 2, 1}, {0x20E, 2, 1}, {0x214, 4, 1}, {0x220, 1, 1},
    {0x223, 1, 1}, {0x226, 1, 1}, {0x229, 1, 1}, {0x22C, 4, 1}, {0x238, 4, 1},
};

/* Copies column `from` of a structure-of-arrays table to column `to` (rows as kWarriorSkill / kWarriorSuper). */
static void warrior_column(uint8_t *dst, const uint8_t *src, const uint16_t (*rows)[3], size_t count, uint32_t slots, uint32_t to,
                           uint32_t from) {
    size_t c;
    uint32_t k;

    for (c = 0; c < count; c++) {
        uint32_t o = rows[c][0], e = rows[c][1];

        for (k = 0; k < rows[c][2]; k++) {
            memcpy(dst + o + (k * slots + to) * e, src + o + (k * slots + from) * e, e);
        }
    }
}
#define WARRIOR_TECH_ANIM(t) (0x105 + 0x22 * (t))
#define WARRIOR_ULT_POSE 0x16B /* the Ultimate's opening pose (action 0x105, src/battle/btl_act_f.c) */

static uint8_t *read_all(const char *path, long *len);

typedef struct WarriorPak {
    uint8_t *d;
    long len;
    uint32_t n;
} WarriorPak;

/* A fighter named with ", costume N" after it: N (1..4), the name cut there; 0 none. */
static int warrior_costume_of(char *v) {
    char *at = strstr(v, ", costume ");
    int c;

    if (at == NULL) {
        return 0;
    }
    c = atoi(at + 10);
    *at = '\0';
    return c >= 1 && c <= 4 ? c : 0;
}

/* The model file of a fighter's costume for the warrior's costume k (0..3, 4..7 their damaged ones): its own (cos 0)
   or always costume cos (its damaged one when k is). */
static int warrior_costume_file(int who, int cos, int k) {
    return MODEL_FILE + who * 10 + (cos > 0 ? cos - 1 + (k >= 4 && k < 8 ? 4 : 0) : k);
}

/* ---- mod fighters as sources: a fighter of the mods folder (a Mod Manager mod, another warrior) taken from as a roster
   fighter is - its parts, layers, body, techniques, motions, effects, portrait. warrior.txt names one "mod/<folder>"; in
   here (and in PortWarrior) it is MOD_REF_BASE + its place in sModRef, a table only ever added to (so the number stays
   while the mods move about). Its files: what the mod has of that kind (mod_file), else its fighter's from the disc.
   Their disc file numbers stay apart from the disc's: model files MODEL_FILE + id * 10 + kind (>= 100000 above
   MODEL_FILE), the portrait FACE_FILE + id, the gauge faces DATA_FILE + id * 2 + side; warrior_src finds the real file. */
#define MOD_REF_BASE 10000
#define MOD_REF_MAX 64
static char sModRef[MOD_REF_MAX][64];
static int sModRefN;
static Mod sModRefMod[MOD_REF_MAX]; /* (each read once a build: sMod is being read again while warriors are built) */
static unsigned char sModRefRead[MOD_REF_MAX];
static char sModRefBuilding[64]; /* the warrior being built (it cannot take from itself) */

static int mod_ref_id(const char *name) {
    int i;

    if (name[0] == '\0' || name[0] == '_' || strlen(name) >= sizeof(sModRef[0])) {
        return -1;
    }
    for (i = 0; i < sModRefN; i++) {
        if (strcmp(sModRef[i], name) == 0) {
            return MOD_REF_BASE + i;
        }
    }
    if (sModRefN >= MOD_REF_MAX) {
        return -1;
    }
    snprintf(sModRef[sModRefN], sizeof(sModRef[0]), "%s", name);
    sModRefRead[sModRefN] = 0;
    return MOD_REF_BASE + sModRefN++;
}

static int is_mod_ref(int who) {
    return who >= MOD_REF_BASE && who < MOD_REF_BASE + sModRefN;
}

/* A fighter id a warrior may name: the roster's, or a mod fighter. */
static int chara_ok(int who) {
    return (who >= 0 && who < CHARA_COUNT) || is_mod_ref(who);
}

static void mod_ref_forget(void) {
    memset(sModRefRead, 0, sizeof(sModRefRead));
}

/* The mod of mod fighter `who`, read from its folder; NULL when it is gone (or is the warrior being built). */
static Mod *mod_ref_mod(int who) {
    int r = who - MOD_REF_BASE;
    char dir[600];

    if (!is_mod_ref(who) || strcmp(sModRef[r], sModRefBuilding) == 0) {
        return NULL;
    }
    if (!sModRefRead[r]) {
        sModRefRead[r] = 2;
        snprintf(dir, sizeof(dir), "%s/mods/%s", root(), sModRef[r]);
        memset(&sModRefMod[r], 0, sizeof(sModRefMod[r]));
        sModRefMod[r].target = sModRefMod[r].tmpl = sModRefMod[r].voice = -1;
        if (is_dir(dir) && load_mod(&sModRefMod[r], dir, sModRef[r])) {
            read_settings(&sModRefMod[r]);
            sModRefRead[r] = 1;
        }
    }
    return sModRefRead[r] == 1 ? &sModRefMod[r] : NULL;
}

/* The fighter a mod fighter stands on (its files' fighter), -1 none; a roster fighter: itself. */
static int mod_ref_chara(int who) {
    Mod *m;

    if (!is_mod_ref(who)) {
        return who;
    }
    m = mod_ref_mod(who);
    return m != NULL ? target_of(m) : -1;
}

/* A fighter's name as warrior.txt writes it (NULL: none). */
static const char *warrior_who_name(int who) {
    static char buf[4][80];
    static int n;
    char *out;

    if (who >= 0 && who < CHARA_COUNT) {
        return kName[who];
    }
    if (!is_mod_ref(who)) {
        return NULL;
    }
    out = buf[n++ & 3];
    snprintf(out, sizeof(buf[0]), "mod/%s", sModRef[who - MOD_REF_BASE]);
    return out;
}

/* A fighter's name as the screens show it: a mod fighter's white name, else its folder's. */
static const char *warrior_who_shown(int who) {
    static char buf[4][80];
    static int n;
    char *out;
    Mod *m;

    if (who >= 0 && who < CHARA_COUNT) {
        return kName[who];
    }
    if (!is_mod_ref(who)) {
        return NULL;
    }
    out = buf[n++ & 3];
    m = mod_ref_mod(who);
    (void)m;
    { /* (its folder's name, as the Mod Manager lists it: several mods may all be named "Gohan"; a warrior's without
         "Warrior - ") */
        const char *f = sModRef[who - MOD_REF_BASE];

        if (strncmp(f, "Warrior - ", 10) == 0) {
            f += 10;
        }
        snprintf(out, sizeof(buf[0]), "%s (mod)", f);
    }
    return out;
}

/* The host path of disc file `id` - or of a mod fighter's file under its own number. 0 when there is none. */
static int warrior_src(int id, char *path, size_t size) {
    int who = -1, kind = -1, t;
    Mod *m;

    if (id >= MODEL_FILE + MOD_REF_BASE * 10 && id < MODEL_FILE + (MOD_REF_BASE + MOD_REF_MAX) * 10) {
        who = (id - MODEL_FILE) / 10;
        kind = (id - MODEL_FILE) % 10;
    } else if (id >= FACE_FILE + MOD_REF_BASE && id < FACE_FILE + MOD_REF_BASE + MOD_REF_MAX) {
        who = id - FACE_FILE;
        kind = 10;
    } else if (id >= DATA_FILE + MOD_REF_BASE * 2 && id < DATA_FILE + (MOD_REF_BASE + MOD_REF_MAX) * 2) {
        who = (id - DATA_FILE) / 2;
        kind = KIND_FACE + (id - DATA_FILE) % 2;
    }
    if (who < 0) {
        snprintf(path, size, "%s/pzs3us1/%05d.bin", root(), id - 1);
        return 1;
    }
    path[0] = '\0';
    if ((m = mod_ref_mod(who)) == NULL) {
        return 0;
    }
    mod_file(m, kind, path, size);
    if (path[0] == '\0') {
        if ((t = target_of(m)) < 0 || t >= CHARA_COUNT) { /* (what the mod lacks: its fighter's own) */
            fprintf(stderr, "bt3: mod fighter %s: no file of kind %d, no fighter of its own\n", m->name, kind);
            return 0;
        }
        id = kind < 10 ? MODEL_FILE + t * 10 + kind : kind == 10 ? FACE_FILE + t : DATA_FILE + t * 2 + (kind - KIND_FACE);
        snprintf(path, size, "%s/pzs3us1/%05d.bin", root(), id - 1);
    }
    fprintf(stderr, "bt3: mod fighter %s: kind %d from %s\n", m->name, kind, path);
    return 1;
}

static int warrior_fighter(const char *v) {
    char a[64], b[64];
    int c;

    while (*v == ' ') {
        v++;
    }
    if (strncasecmp(v, "none", 4) == 0 && (v[4] == '\0' || v[4] == '\r' || v[4] == '\n')) {
        return -2; /* (a part taken away: no hair, no accessories) */
    }
    if (strncasecmp(v, "mod/", 4) == 0) { /* (a mod fighter) */
        size_t n;

        snprintf(a, sizeof(a), "%s", v + 4);
        a[strcspn(a, "\r\n")] = '\0';
        for (n = strlen(a); n > 0 && a[n - 1] == ' '; n--) {
            a[n - 1] = '\0';
        }
        return mod_ref_id(a);
    }
    if (isdigit((unsigned char)*v)) {
        c = atoi(v);
        return c >= 0 && c < CHARA_COUNT ? c : -1;
    }
    lower(a, v, sizeof(a));
    a[strcspn(a, "\r\n")] = '\0';
    for (c = 0; c < CHARA_COUNT; c++) {
        lower(b, kName[c], sizeof(b));
        if (strcmp(a, b) == 0) {
            return c;
        }
        lower(b, kPrefix[c], sizeof(b));
        if (strcmp(a, b) == 0) {
            return c;
        }
    }
    return -1;
}

/* Reads disc file `id` as a pak; 0 when it is not one. */
static int warrior_open(WarriorPak *p, int id) {
    char path[600];
    uint32_t i, prev;

    memset(p, 0, sizeof(*p));
    if (!warrior_src(id, path, sizeof(path))) {
        return 0;
    }
    p->d = read_all(path, &p->len);
    if (p->d == NULL || p->len < 8) {
        return 0;
    }
    memcpy(&p->n, p->d, 4);
    if (p->n == 0 || p->n > 4096 || (long)(4 + 4 * (p->n + 1)) > p->len) {
        return 0;
    }
    for (i = 0, prev = 0; i <= p->n; i++) {
        uint32_t o;

        memcpy(&o, p->d + 4 + i * 4, 4);
        if (o < prev || (long)o > p->len) {
            return 0;
        }
        prev = o;
    }
    return 1;
}

static const uint8_t *warrior_part(const WarriorPak *p, uint32_t i, uint32_t *size) {
    uint32_t a, b;

    memcpy(&a, p->d + 4 + i * 4, 4);
    memcpy(&b, p->d + 8 + i * 4, 4);
    *size = b - a;
    return p->d + a;
}

/* Sub-part i of a pak held in memory (a part of a file), NULL when it has none. */
static const uint8_t *warrior_sub(const uint8_t *p, uint32_t len, uint32_t i, uint32_t *size) {
    uint32_t n, a, b;

    if (p == NULL || len < 8) {
        return NULL;
    }
    memcpy(&n, p, 4);
    if (i >= n || n > 4096 || 4 + 4 * (n + 1) > len) {
        return NULL;
    }
    memcpy(&a, p + 4 + i * 4, 4);
    memcpy(&b, p + 8 + i * 4, 4);
    if (a > b || b > len) {
        return NULL;
    }
    *size = b - a;
    return p + a;
}

/* A pak of n parts in memory, laid out as warrior_write_pak writes one (malloc'ed; NULL when out of memory). */
static uint8_t *warrior_make_pak(const uint8_t *const *part, const uint32_t *size, uint32_t n, uint32_t *total) {
    uint32_t head = (4 + 4 * (n + 1) + 0x3F) & ~0x3Fu, o = head, i;
    uint8_t *out;

    for (i = 0; i < n; i++) {
        o += (size[i] + 0x3F) & ~0x3Fu;
    }
    *total = o;
    if ((out = calloc(1, o)) == NULL) {
        return NULL;
    }
    memcpy(out, &n, 4);
    for (i = 0, o = head; i <= n; i++) {
        memcpy(out + 4 + i * 4, &o, 4);
        if (i < n) {
            memcpy(out + o, part[i], size[i]);
            o += (size[i] + 0x3F) & ~0x3Fu;
        }
    }
    return out;
}

/* Writes a pak of n parts: the count, n + 1 offsets, the parts from the next 0x40, each padded to 0x40. */
static int warrior_write_pak(const char *path, const uint8_t *const *part, const uint32_t *size, uint32_t n) {
    static const uint8_t zero[0x40];
    uint32_t head = (4 + 4 * (n + 1) + 0x3F) & ~0x3Fu, o = head, i;
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        return 0;
    }
    fwrite(&n, 4, 1, fp);
    for (i = 0; i <= n; i++) {
        fwrite(&o, 4, 1, fp);
        if (i < n) {
            o += (size[i] + 0x3F) & ~0x3Fu;
        }
    }
    fwrite(zero, 1, head - (4 + 4 * (n + 1)), fp);
    for (i = 0; i < n; i++) {
        fwrite(part[i], 1, size[i], fp);
        fwrite(zero, 1, ((size[i] + 0x3F) & ~0x3Fu) - size[i], fp);
    }
    return fclose(fp) == 0;
}

/* Copies disc file `id` to path. */
static int warrior_copy(int id, const char *path) {
    char src[600];
    long len;
    uint8_t *d;
    FILE *fp;
    int ok;

    if (!warrior_src(id, src, sizeof(src)) || (d = read_all(src, &len)) == NULL || (fp = fopen(path, "wb")) == NULL) {
        free(d);
        return 0;
    }
    ok = fwrite(d, 1, (size_t)len, fp) == (size_t)len;
    ok &= fclose(fp) == 0;
    free(d);
    return ok;
}

/* The origin of a node in a model file's rest pose (the skeleton records of part 2, the chain from header word
   0x6C; BobjBone in include/battle/bobj_a.h). 0 when not found. */
static int warrior_node(const WarriorPak *p, uint16_t want, float *xyz) {
    uint32_t size, o;
    const uint8_t *m = warrior_part(p, 2, &size);
    int guard;

    if (size < 0x70) {
        return 0;
    }
    memcpy(&o, m + 0x6C, 4);
    for (guard = 0; guard < 512 && o + 0x40 <= size; guard++) {
        int32_t next;
        uint16_t last, id;

        memcpy(&next, m + o, 4);
        memcpy(&last, m + o + 6, 2);
        memcpy(&id, m + o + 0xA, 2);
        if (id == want) {
            memcpy(xyz, m + o + 0x10, 12);
            return 1;
        }
        if (last || next <= 0) {
            break;
        }
        o += (uint32_t)next;
    }
    return 0;
}

/* The height of node 3 (the hips) in a model file's rest pose; 0 when not found. */
static float warrior_hip(const WarriorPak *p) {
    float xyz[3];

    return warrior_node(p, 3, xyz) ? xyz[1] : 0.0f;
}

/* Whether a model file's fighter has a tail: nodes 4..7 are a chain behind the hips (node 7 more than a unit away
   from node 3); without a tail they all sit on the hips. */
static int warrior_has_tail(const WarriorPak *p) {
    float h[3], e[3];

    return warrior_node(p, 3, h) && warrior_node(p, 7, e) &&
           (e[0] - h[0]) * (e[0] - h[0]) + (e[1] - h[1]) * (e[1] - h[1]) + (e[2] - h[2]) * (e[2] - h[2]) > 1.0f;
}

static uint8_t *bpe_decode(const uint8_t *d, long len, uint32_t *size);

/* The body's own tail motion for motions made for a fighter without a tail: "TAIL", the length in frames of the
   body's standing motion (0) and its tracks of nodes 4..7, each as its size and bytes (padded to 4). NULL when the
   body's motion has none. */
static uint8_t *warrior_tail_clip(int body, uint32_t *outSize) {
    WarriorPak ap;
    uint32_t raw = 0, size, n, total = 8;
    const uint8_t *packed;
    uint8_t *an = NULL, *out = NULL;
    uint16_t length, off;
    uint32_t at[4], len[4];

    memset(&ap, 0, sizeof(ap));
    if (!warrior_open(&ap, MODEL_FILE + body * 10 + 8) || ap.n < 1) {
        goto done;
    }
    packed = warrior_part(&ap, 0, &size);
    if ((an = bpe_decode(packed, size, &raw)) == NULL || raw < 0x94) {
        goto done;
    }
    memcpy(&length, an + 2, 2);
    for (n = 0; n < 4; n++) {
        uint16_t flags, count;

        memcpy(&off, an + 6 + (4 + n) * 2, 2);
        at[n] = (uint32_t)off * 4;
        len[n] = 0;
        if (off == 0 || at[n] + 4 > raw) {
            goto done;
        }
        memcpy(&flags, an + at[n], 2);
        memcpy(&count, an + at[n] + 2, 2);
        len[n] = 4 + (flags & 1 ? count * 10u : count * 0x18u);
        if (count < 2 || at[n] + len[n] > raw) {
            goto done;
        }
        total += 4 + ((len[n] + 3) & ~3u);
    }
    if ((out = calloc(1, total)) == NULL) {
        goto done;
    }
    memcpy(out, "TAIL", 4);
    size = length;
    memcpy(out + 4, &size, 4);
    for (n = 0, total = 8; n < 4; n++) {
        memcpy(out + total, &len[n], 4);
        memcpy(out + total + 4, an + at[n], len[n]);
        total += 4 + ((len[n] + 3) & ~3u);
    }
    *outSize = total;
done:
    free(an);
    free(ap.d);
    return out;
}

/* The scale for motions made for a fighter whose hips are at hm, played on a body whose hips are at hb (1 when either
   is unknown). */
static float warrior_scale(float hb, float hm) {
    return hb < -0.5f && hm < -0.5f ? hb / hm : 1.0f;
}

/* The translation keys of an uncompressed motion (BobjAnim, include/battle/bobj_a.h: track offsets in words at +6,
   each track {flags (1: rotations only), count, keys of 0x18: x y z frame rotation}) scaled, in place. */
static void warrior_scale_motion(uint8_t *a, uint32_t size, float sc) {
    int n, k, j;

    if (size < 0x94 || (sc > 0.999f && sc < 1.001f)) {
        return;
    }
    for (n = 0; n < 71; n++) {
        uint16_t off, flags, count;
        uint32_t at;

        memcpy(&off, a + 6 + n * 2, 2);
        at = (uint32_t)off * 4;
        if (off == 0 || at + 4 > size) {
            continue;
        }
        memcpy(&flags, a + at, 2);
        memcpy(&count, a + at + 2, 2);
        if (flags & 1) {
            continue;
        }
        for (k = 0; k < count && at + 4 + (uint32_t)(k + 1) * 0x18 <= size; k++) {
            for (j = 0; j < 3; j++) {
                float f;

                memcpy(&f, a + at + 4 + k * 0x18 + j * 4, 4);
                f *= sc;
                memcpy(a + at + 4 + k * 0x18 + j * 4, &f, 4);
            }
        }
    }
}

/* One motion range of part 25: first, count, and the scale for the fighter it comes from (negative when the body's
   tail motion goes into it: the body has a tail clip and that fighter has no tail). Returns the new count. */
typedef struct WarriorRange {
    uint16_t first, count;
    float scale;
} WarriorRange;

static uint32_t warrior_range(WarriorRange *r, uint32_t n, uint32_t first, uint32_t count, float hb, int clip, const WarriorPak *from) {
    r[n].first = (uint16_t)first;
    r[n].count = (uint16_t)count;
    r[n].scale = warrior_scale(hb, warrior_hip(from)) * (clip && !warrior_has_tail(from) ? -1.0f : 1.0f);
    return n + 1;
}

/* Whether the move-list line at d[i] (UTF-16 characters after the byte-order mark) is a technique's: "!F*" and its
   three digits of cost, "!F&086*002" on some fighters (a colour first). 1 it is (*name: where its name starts), -1 the
   techniques are over ("!F$1": the melee's lines follow, their names "!F*000Heavy Finish" too), 0 another line. */
static int ml_tech(const uint16_t *d, uint32_t n, uint32_t i, uint32_t *name) {
    uint32_t t = i + 2;

    if ((i != 0 && d[i - 1] != '\n') || i + 2 >= n || d[i] != '!' || d[i + 1] != 'F') {
        return 0;
    }
    if (d[t] == '$') {
        return t + 1 < n && d[t + 1] != '0' ? -1 : 0;
    }
    if (d[t] == '&' && t + 4 < n) {
        t += 4;
    }
    if (d[t] != '*' || t + 4 > n) {
        return 0;
    }
    *name = t + 4;
    return 1;
}

/* A move-list entry (UTF-16 with a byte-order mark, lines split by '\n', ending in zeros) with its line number
   `line` among the lines that start with "!F*" (0, 1: the Blast 1 skills, 2..4 the techniques) replaced by line
   `from` of the same kind in src. Returns a new entry (padded to 0x40), or NULL when either has no such line. */
static uint8_t *warrior_name_line(const uint8_t *dst, uint32_t dsz, const uint8_t *src, uint32_t ssz, int line, int from,
                                  uint32_t *outSize) {
    const uint16_t *d = (const uint16_t *)(dst + 2), *s = (const uint16_t *)(src + 2);
    uint32_t dn = dsz >= 2 ? (dsz - 2) / 2 : 0, sn = ssz >= 2 ? (ssz - 2) / 2 : 0, i;
    uint32_t da = 0, db = 0, sa = 0, sb = 0;
    int found = 0, k;
    uint8_t *out;

    if (dn == 0 || sn == 0 || dst[0] != 0xFF || dst[1] != 0xFE || src[0] != 0xFF || src[1] != 0xFE) {
        return NULL;
    }
    for (k = 0, i = 0; i < dn && d[i] != 0 && !found; i++) { /* the line in dst: [da, db) */
        uint32_t nm;
        int is = ml_tech(d, dn, i, &nm);

        if (is < 0) {
            break;
        }
        if (is) {
            if (k++ == line) {
                da = i;
                for (db = i; db < dn && d[db] != 0 && d[db] != '\n'; db++) {
                }
                found = 1;
            }
        }
    }
    for (k = 0, i = 0; i < sn && s[i] != 0 && found == 1; i++) { /* the line in src: [sa, sb) */
        uint32_t nm;
        int is = ml_tech(s, sn, i, &nm);

        if (is < 0) {
            break;
        }
        if (is) {
            if (k++ == from) {
                sa = i;
                for (sb = i; sb < sn && s[sb] != 0 && s[sb] != '\n'; sb++) {
                }
                found = 2;
            }
        }
    }
    if (found != 2) {
        return NULL;
    }
    for (i = db; i < dn && d[i] != 0; i++) { /* (the rest of dst's text) */
    }
    *outSize = (2 + (da + (sb - sa) + (i - db)) * 2 + 4 + 0x3F) & ~0x3Fu;
    out = calloc(1, *outSize);
    if (out == NULL) {
        return NULL;
    }
    out[0] = 0xFF;
    out[1] = 0xFE;
    memcpy(out + 2, d, da * 2);
    memcpy(out + 2 + da * 2, s + sa, (sb - sa) * 2);
    memcpy(out + 2 + (da + sb - sa) * 2, d + db, (i - db) * 2);
    return out;
}

/* A move-list entry (as warrior_name_line) with the name of its line `line` among the "!F*" ones (0, 1 the Blast 1
   skills, 2..4 the techniques) made `name` (its "!F*" and cost kept). Returns a new entry, NULL when it has no such line. */
static uint8_t *warrior_rename_line(const uint8_t *dst, uint32_t dsz, int line, const char *name, uint32_t *outSize) {
    const uint16_t *d = (const uint16_t *)(dst + 2);
    uint32_t dn = dsz >= 2 ? (dsz - 2) / 2 : 0, i, da = 0, db = 0, nn = (uint32_t)strlen(name), end;
    int k, found = 0;
    uint8_t *out;
    uint16_t *o;

    if (dn == 0 || dst[0] != 0xFF || dst[1] != 0xFE) {
        return NULL;
    }
    for (k = 0, i = 0; i < dn && d[i] != 0 && !found; i++) {
        uint32_t nm;
        int is = ml_tech(d, dn, i, &nm);

        if (is < 0) {
            break;
        }
        if (is && k++ == line) {
            da = nm; /* (after "!F*" and the three digits of its cost) */
            for (db = da; db < dn && d[db] != 0 && d[db] != '\n'; db++) {
            }
            found = 1;
        }
    }
    if (!found) {
        return NULL;
    }
    for (end = db; end < dn && d[end] != 0; end++) {
    }
    *outSize = (2 + (da + nn + (end - db)) * 2 + 4 + 0x3F) & ~0x3Fu;
    if ((out = calloc(1, *outSize)) == NULL) {
        return NULL;
    }
    out[0] = 0xFF;
    out[1] = 0xFE;
    o = (uint16_t *)(out + 2);
    memcpy(o, d, da * 2);
    for (i = 0; i < nn; i++) {
        o[da + i] = (uint8_t)name[i];
    }
    memcpy(o + da + nn, d + db, (end - db) * 2);
    return out;
}

/* ---- a technique's colours (blast2_1colour= ...) ----
 * A technique's effect pack (model file +9, parts 2..4) is a pack of entries; its colours are in three kinds of them:
 * colour keys (192 or 320 bytes of R G B A floats, 0..255), the V000 animations (32-byte tracks, then 64-byte keys:
 * three x y z w with w 1, then R G B A bytes) and the pictures (texture files as the model's: their palettes). Each
 * colour takes the new hue, keeping how strong and how light it was; whites and greys stay. A colour across the wheel
 * from the technique's own (the red of a blue Kamehameha: blue inverted, for the effects drawn subtracting) is turned
 * as its inverse is, and inverted back. */
typedef struct WarriorTint {
    float h, s, v; /* the new colour's hue (0..6), strength, lightness */
    float own;     /* the technique's own hue (0..6), -1 none */
} WarriorTint;

static void warrior_fx_hsv(float r, float g, float b, float *h, float *s, float *v) {
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b), mn = r < g ? (r < b ? r : b) : (g < b ? g : b); /* (not fmaxf: the C library's,
                                                                                        not the soft-float build's) */

    *v = mx;
    *s = mx > 0.0f ? (mx - mn) / mx : 0.0f;
    *h = 0.0f;
    if (mx > mn) {
        *h = mx == r ? fmodf((g - b) / (mx - mn) + 6.0f, 6.0f) : mx == g ? (b - r) / (mx - mn) + 2.0f : (r - g) / (mx - mn) + 4.0f;
    }
}

/* One colour (0..1 each) turned: in place. */
static void warrior_turn(const WarriorTint *t, float *c) {
    float h, s, v, cc, x, rgb[3], d;
    int hi, q, inv = 0;

    warrior_fx_hsv(c[0], c[1], c[2], &h, &s, &v);
    if (s < 0.12f || v < 0.04f) {
        return; /* (white, grey, black: as they are) */
    }
    if (t->own >= 0.0f) { /* (across the wheel from the technique's own: an inverse) */
        d = fabsf(h - t->own);
        d = d > 3.0f ? 6.0f - d : d;
        inv = d > 2.0f;
    }
    if (inv) {
        for (q = 0; q < 3; q++) {
            c[q] = 1.0f - c[q];
        }
        warrior_fx_hsv(c[0], c[1], c[2], &h, &s, &v);
    }
    s *= t->s < 0.5f ? t->s / 0.5f : 1.0f;
    v *= t->v < 0.5f ? t->v / 0.5f : 1.0f;
    cc = v * s;
    hi = (int)t->h % 6;
    x = cc * (1.0f - fabsf(fmodf(t->h, 2.0f) - 1.0f));
    rgb[0] = hi == 0 || hi == 5 ? cc : hi == 1 || hi == 4 ? x : 0.0f;
    rgb[1] = hi == 1 || hi == 2 ? cc : hi == 0 || hi == 3 ? x : 0.0f;
    rgb[2] = hi == 3 || hi == 4 ? cc : hi == 2 || hi == 5 ? x : 0.0f;
    for (q = 0; q < 3; q++) {
        c[q] = rgb[q] + (v - cc);
        c[q] = inv ? 1.0f - c[q] : c[q];
    }
}

/* The colours of entry e of pack p (n bytes): each one handed to `each` (0..1, in place), written back. */
typedef void (*WarriorColourFn)(void *ctx, float *rgb);

static void warrior_fx_colours(uint8_t *p, uint32_t n, WarriorColourFn each, void *ctx) {
    uint32_t count, e;

    memcpy(&count, p, n >= 4 ? 4 : 0);
    if (n < 8 || count == 0 || count > 4096 || 4 + (count + 1) * 4 > n) {
        return;
    }
    for (e = 0; e < count; e++) {
        uint32_t a, b, size, q, k;
        uint8_t *x;

        memcpy(&a, p + 4 + e * 4, 4);
        memcpy(&b, p + 8 + e * 4, 4);
        if (b <= a || b > n || (a & 3)) {
            continue;
        }
        x = p + a;
        size = b - a;
        if (size == 192 || size == 320) { /* colour keys: floats, every one 0 or 1..255 */
            float *f = (float *)x;
            int ok = 1, any = 0;

            for (q = 0; q < size / 4 && ok; q++) {
                ok = f[q] == 0.0f || (f[q] >= 1.0f && f[q] <= 255.5f);
                any |= f[q] != 0.0f;
            }
            for (k = 0; ok && any && k < size / 16; k++) {
                float c[3] = {f[k * 4] / 255.0f, f[k * 4 + 1] / 255.0f, f[k * 4 + 2] / 255.0f};

                each(ctx, c);
                for (q = 0; q < 3; q++) {
                    f[k * 4 + q] = c[q] * 255.0f;
                    f[k * 4 + q] = f[k * 4 + q] > 0.0f && f[k * 4 + q] < 1.0f ? 1.0f : f[k * 4 + q];
                }
            }
        } else if (size >= 0x20 && memcmp(x, "V000", 4) == 0) { /* an animation: its keys' colours */
            uint32_t at;

            memcpy(&at, x + 8, 4);
            for (; at >= 0x20 && at + 64 <= size; at += 64) {
                float w[3];
                float c[3];

                memcpy(&w[0], x + at + 0x0C, 4);
                memcpy(&w[1], x + at + 0x1C, 4);
                memcpy(&w[2], x + at + 0x2C, 4);
                if (w[0] != 1.0f || w[1] != 1.0f || w[2] != 1.0f) {
                    continue;
                }
                for (q = 0; q < 3; q++) {
                    c[q] = (float)x[at + 0x30 + q] / 255.0f;
                }
                each(ctx, c);
                for (q = 0; q < 3; q++) {
                    x[at + 0x30 + q] = (uint8_t)(c[q] * 255.0f + 0.5f);
                }
            }
        } else if (size >= 0x60) { /* pictures (a texture file: count, its entries at word 8): their palettes */
            uint32_t tc, eo, t;

            memcpy(&tc, x, 4);
            memcpy(&eo, x + 4, 4);
            if (tc == 0 || tc > 64 || eo != 8 || 0x20 + tc * 0x40 > size) {
                continue;
            }
            for (t = 0; t < tc; t++) {
                uint32_t pix, clut, pixSize, clutSize, colours;

                memcpy(&pix, x + 0x20 + t * 0x40, 4);
                memcpy(&clut, x + 0x20 + t * 0x40 + 4, 4);
                memcpy(&pixSize, x + 0x20 + t * 0x40 + 8, 4);
                memcpy(&clutSize, x + 0x20 + t * 0x40 + 12, 4);
                if (pix * 4 + pixSize > size || clut * 4 + clutSize > size || clutSize < 0x60 + 64) {
                    continue;
                }
                colours = clutSize >= 0x60 + 1024 ? 256 : 16;
                for (k = 0; k < colours; k++) {
                    uint8_t *c8 = x + clut * 4 + 0x60 + k * 4;
                    float c[3] = {c8[0] / 255.0f, c8[1] / 255.0f, c8[2] / 255.0f};

                    each(ctx, c);
                    for (q = 0; q < 3; q++) {
                        c8[q] = (uint8_t)(c[q] * 255.0f + 0.5f);
                    }
                }
            }
        }
    }
}

/* (the technique's own hue: the strong colours' hues, summed as directions) */
typedef struct WarriorHueSum {
    float x, y;
} WarriorHueSum;

static void warrior_hue_sum(void *ctx, float *c) {
    WarriorHueSum *m = (WarriorHueSum *)ctx;
    float h, s, v;

    warrior_fx_hsv(c[0], c[1], c[2], &h, &s, &v);
    if (s >= 0.3f && v >= 0.2f) {
        m->x += s * v * cosf(h * 1.0471976f);
        m->y += s * v * sinf(h * 1.0471976f);
    }
}

static void warrior_turn_each(void *ctx, float *c) {
    warrior_turn((const WarriorTint *)ctx, c);
}

static void warrior_tint_fx(uint8_t *p, uint32_t n, uint32_t bgr);

/* The common pack (part 5: a pack of effect packs, the cameras' among them): each effect pack in it turned as a
   technique's is. */
static void warrior_tint_common(uint8_t *p, uint32_t n, uint32_t bgr) {
    uint32_t count, e;

    memcpy(&count, p, n >= 4 ? 4 : 0);
    if (n < 8 || count == 0 || count > 64 || 4 + (count + 1) * 4 > n) {
        return;
    }
    for (e = 0; e < count; e++) {
        uint32_t a, b, c2;

        memcpy(&a, p + 4 + e * 4, 4);
        memcpy(&b, p + 8 + e * 4, 4);
        if (b <= a + 8 || b > n || (a & 3)) {
            continue;
        }
        memcpy(&c2, p + a, 4);
        if (c2 > 0 && c2 <= 4096 && 4 + (c2 + 1) * 4 <= b - a) { /* (a pack itself) */
            warrior_tint_fx(p + a, b - a, bgr);
        }
    }
}

/* The hue (0..6) the common pack's effects are seen in (its fighter's aura, the light of its aura kind), -1 not known:
   an effect whose colours lie across the wheel from it is drawn subtracting (its colours are the inverse of what is
   seen: the charge's), so it is turned as an inverse, not into the opposite colour. */
static float sTintSeen = -1.0f;

static void warrior_tint_fx(uint8_t *p, uint32_t n, uint32_t bgr) {
    WarriorTint t;
    WarriorHueSum m = {0.0f, 0.0f};
    float s, v;

    warrior_fx_hsv((float)(bgr & 0xFF) / 255.0f, (float)(bgr >> 8 & 0xFF) / 255.0f, (float)(bgr >> 16 & 0xFF) / 255.0f, &t.h, &t.s, &t.v);
    warrior_fx_colours(p, n, warrior_hue_sum, &m);
    t.own = -1.0f;
    if (m.x * m.x + m.y * m.y > 1e-6f) {
        t.own = atan2f(m.y, m.x) / 1.0471976f;
        t.own = t.own < 0.0f ? t.own + 6.0f : t.own;
    }
    if (0 && t.own >= 0.0f && sTintSeen >= 0.0f) { /* (an effect taken for one drawn subtracting, turned as an inverse: the charge
                                                     then came out as the opposite colour - red for a cyan aura, blue for a red
                                                     one - so every effect is turned as it is seen) */
        float d = fabsf(t.own - sTintSeen);

        d = d > 3.0f ? 6.0f - d : d;
        if (d > 2.0f) {
            t.own = fmodf(t.own + 3.0f, 6.0f);
        }
    }
    (void)s;
    (void)v;
    warrior_fx_colours(p, n, warrior_turn_each, &t);
}

/* ---- the aura's colour (aura=): read when a fight loads its fighters (Port_SkinVoiceSet), asked for by the aura
   (src/battle/eft_n.c) ---- */
static int sAuraAs[2]; /* per player: 0x1000000 | 0xBBGGRR, 0 its own */
static int sTransAs[2][4]; /* per player: each form's transformation aura colour (formaura<n>=), 0 the game's */
int32_t gPortTfIndex;      /* (btl_char_fx.c: the form a fighter turns into, for its effect) */

int Port_SkinTransColourOf(int player, int index) {
    return player >= 0 && player < 2 && index >= 0 && index < 4 ? sTransAs[player][index] : 0;
}

/* The transformation aura colours of warrior.txt in folder dir (formaura1= ..). */
static void warrior_formaura_of(const char *dir, int *out) {
    char path[600], line[256];
    FILE *fp;

    memset(out, 0, sizeof(int) * 4);
    snprintf(path, sizeof(path), "%s/warrior.txt", dir);
    if ((fp = fopen(path, "r")) == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "formaura", 8) == 0 && line[8] >= '1' && line[8] <= '4' && line[9] == '=') {
            out[line[8] - '1'] = (int)strtoul(line + 10, NULL, 16);
        }
    }
    fclose(fp);
}

/* The aura colour warrior.txt of folder `dir` gives, 0 none. */
static int warrior_aura_of(const char *dir) {
    char path[600], line[256];
    FILE *fp;
    int c = 0;

    snprintf(path, sizeof(path), "%s/warrior.txt", dir);
    if ((fp = fopen(path, "r")) == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "aura=", 5) == 0) {
            c = warrior_hex_colour(line + 5);
        }
    }
    fclose(fp);
    return c;
}

int Port_SkinAuraOf(int player) {
    return player >= 0 && player < 2 ? sAuraAs[player] : 0;
}

/* The donors' model files a warrior takes single parts from (their first costume; .d NULL: not used). */
typedef struct WarriorDonors {
    WarriorPak skill[2], style, fin, tech[3], stance, fx[3], aura, anim[9], sfx[2], cam[4];
} WarriorDonors;

/* The standing motions (the menus' pose 7; the idles of src/battle/btl_act_d.c BtlAct_GetIdleMotion: 0, 1, 0x26, 0x27,
   0x18B), taken from the stance= fighter. */
static const uint16_t kWarriorStance[][2] = {{0, 2}, {7, 1}, {0x26, 2}, {0x18B, 1}};

/* The kinds of motion a warrior can take from other fighters (anim_move= ...), as the actions play them
   (src/battle/btl_act_*.c): {first, count} ranges. Moving & flying: walking, the dashes 0x18..0x1B, jumps 0x1F..0x22,
   hops, ascending and descending 0x2B..0x33, the short rise 0x18C (not the battle idles 0x26 / 0x27: the stance's).
   Charging: ki charge 0x34 start, 0x35 loop, 0x36 powered up. Melee: the rush chain 0x37.., rapid rush 0x3C..0x40,
   smashes and finishers 0x41.., the flying kick and rising attack 0x6B..0x70, the dash attacks 0x8D..0x93. Ki blasts:
   0x7A..0x8C. Grabs & throws: the grab dash 0x94 / 0x95, the throws 0x96..0x9B, the miss 0x9D, the other throw set
   0x186..0x18A, 0x198. Guard & dodge: 0xEC..0xFD (evasions, vanishing, guard). */
static const uint16_t kWarriorAnimCat[9][4][2] = {
    {{0x0E, 0x18}, {0x28, 0x0C}, {0x18C, 1}}, {{0x34, 3}}, {{0x37, 0x3C}, {0x8D, 7}}, {{0x7A, 0x13}},
    {{0x94, 0x0A}, {0x186, 5}, {0x198, 1}}, {{0xEC, 0x12}},
    {{385, 2}}, {{387, 2}}, {{389, 2}}, /* (the intro, the victory, the taunt: each on its own) */
};
static const char *const kWarriorAnimKey[9] = {"anim_move", "anim_charge", "anim_melee", "anim_ki", "anim_grab", "anim_guard",
                                               "anim_intro", "anim_victory", "anim_taunt"};

static int warrior_anim_key(const char *key) {
    int q;

    for (q = 0; q < 9; q++) {
        if (strcmp(key, kWarriorAnimKey[q]) == 0) {
            return q;
        }
    }
    return -1;
}

/* The technique table's effect rows (which effect, its bytes, its floats): a technique's look, from another technique
   of the same effect type (blast2_1fx= ...). */
static const uint16_t kWarriorSuperFx[][3] = {{0x09F, 1, 17}, {0x0D4, 4, 8}, {0x138, 1, 1}};

/* ---- the Technique Lab: a technique's numbers set by hand, over whatever technique it is (lab_blast2_1=0:9000,1:40000)
 * Techniques (Blast 2, Ultimate: model part 23, BtlSuperData in include/battle/btl_tech_b.h, arrays of 3) and skills
 * (Blast 1: part 24, BtlSkillData, arrays of 2). Stats and their units in warrior.txt:
 *   0 damage (health points; the guarded damage keeps its share)   1 cost (ki units, 20000 a bar; a skill: stocks)
 *   2 hits per shot (a beam's multi-hit)    3 shots (a barrage's count)    4 size (percent of the projectile's)
 *   5 speed (10 km/h)    6 homing (degrees a second)    7 range / duration (hundredths of a second)
 *   8 knock-back (tenths, the guarded push keeps its share)    9 charge boost (percent at full charge)
 *   10 recoil (damage to the user)    11 unblockable (1 yes, 0 no: only a super guard stops it)
 *   12 behaves like another technique (fighter * 4 + its technique 0..2: its flags, hit reaction, guard and hit rules)
 *   13 heal (a skill: percent of health)    14 ki given (a skill)
 *   a skill's 3..6: its boosts to attack, defense, ki & speed, Blast power (levels + 100) */
enum { LAB_DAMAGE, LAB_COST, LAB_HITS, LAB_SHOTS, LAB_SIZE, LAB_SPEED, LAB_HOMING, LAB_TIME, LAB_PUSH, LAB_CHARGE, LAB_RECOIL,
       LAB_UNBLOCK, LAB_LIKE, LAB_HEAL, LAB_KI };
/* A Blast 1 skill's boosts (levels added to the fighter's stats while it lasts: BtlSkillData.statLevel*, the stat curves of
   src/battle/btl_char_status.c): they take the places of the shots, size, speed and homing, which skills do not have. */
enum { LAB_ATTACK = LAB_SHOTS, LAB_DEFENSE = LAB_SIZE, LAB_KIBOOST = LAB_SPEED, LAB_BLAST = LAB_HOMING };
static const char *const kWarriorLabKey[5] = {"lab_blast1_1", "lab_blast1_2", "lab_blast2_1", "lab_blast2_2", "lab_ultimate"};
/* The rules of a technique that "behaves like" another takes: the flag words, how its hits are aimed and react, its guard
   kind, its hit sounds. Not its type (its animations' block) nor its effects: those stay its own. */
static const uint16_t kWarriorLabLike[][3] = {{0x000, 4, 1}, {0x00C, 4, 1}, {0x13B, 1, 1}, {0x141, 1, 1}, {0x144, 1, 1},
                                             {0x147, 1, 1}, {0x14A, 1, 1}, {0x150, 1, 1}, {0x153, 1, 2}};

/* "glow_blast1_1" .. "glow_ultimate": which technique's body glow (0..4), else -1. */
static int warrior_glow_key(const char *key) {
    int t;

    for (t = 0; t < 5; t++) {
        if (strncmp(key, "glow_", 5) == 0 && strcmp(key + 5, kWarriorLabKey[t] + 4) == 0) {
            return t;
        }
    }
    return -1;
}

static int warrior_lab_of(const char *key, const char *v, int32_t *set, int32_t (*lab)[16]) {
    int t, id, val, used;

    for (t = 0; t < 5 && strcmp(key, kWarriorLabKey[t]) != 0; t++) {
    }
    if (t == 5) {
        return 0;
    }
    set[t] = 0;
    while (sscanf(v, "%d:%d%n", &id, &val, &used) == 2) {
        if (id >= 0 && id <= LAB_KI && (val >= 0 || id == LAB_HEAL || id == LAB_KI) && (id != LAB_LIKE || val / 4 < CHARA_COUNT)) {
            set[t] |= 1 << id; /* (a value that cannot be one - a negative count, an unknown fighter - is not taken) */
            lab[t][id] = val;
        }
        v += used;
        if (*v == ',') {
            v++;
        }
    }
    return 1;
}

static int32_t lab_rd32(const uint8_t *p) {
    int32_t v;

    memcpy(&v, p, 4);
    return v;
}

static float lab_rdf(const uint8_t *p) {
    float v;

    memcpy(&v, p, 4);
    return v;
}

static void lab_wr32(uint8_t *p, int32_t v) {
    memcpy(p, &v, 4);
}

static void lab_wrf(uint8_t *p, float v) {
    memcpy(p, &v, 4);
}

static int8_t lab_s8(int v) {
    return (int8_t)(v < -128 ? -128 : v > 127 ? 127 : v);
}

/* The Lab's stats written over the technique tables (parts 23, 24) of a model file. */
static void warrior_lab_apply(uint8_t *sup, uint32_t supN, uint8_t *ski, uint32_t skiN, const int32_t *set, const int32_t (*lab)[16]) {
    int t;

    for (t = 2; t < 5; t++) { /* the techniques */
        int c = t - 2, b = set[t];
        const int32_t *v = lab[t];

        if (b == 0 || sup == NULL || supN < 0x244) {
            continue;
        }
        if ((b >> LAB_LIKE) & 1) { /* (first: the stats below go over it) */
            WarriorPak p;
            uint32_t n;
            const uint8_t *q;
            int who = v[LAB_LIKE] >= 0 ? v[LAB_LIKE] / 4 : -1, from = v[LAB_LIKE] >= 0 ? v[LAB_LIKE] % 4 : 3;

            memset(&p, 0, sizeof(p));
            if (who >= 0 && who < CHARA_COUNT && from < 3 && warrior_open(&p, MODEL_FILE + who * 10) && p.n == WARRIOR_PARTS &&
                (q = warrior_part(&p, 23, &n)) != NULL && n >= 0x244) {
                warrior_column(sup, q, kWarriorLabLike, sizeof(kWarriorLabLike) / sizeof(kWarriorLabLike[0]), 3, (uint32_t)c, (uint32_t)from);
            }
            free(p.d);
        }
        if ((b >> LAB_DAMAGE) & 1) {
            int32_t was = lab_rd32(sup + 0x184 + c * 4), g = lab_rd32(sup + 0x190 + c * 4);

            lab_wr32(sup + 0x184 + c * 4, v[LAB_DAMAGE]);
            lab_wr32(sup + 0x190 + c * 4, was > 0 ? (int32_t)((int64_t)g * v[LAB_DAMAGE] / was) : v[LAB_DAMAGE] / 4);
        }
        if ((b >> LAB_COST) & 1) {
            lab_wr32(sup + 0x19C + c * 4, v[LAB_COST]);
        }
        if ((b >> LAB_HITS) & 1) {
            sup[0x9C + c] = (uint8_t)lab_s8(v[LAB_HITS] - 1);
        }
        if ((b >> LAB_SHOTS) & 1) {
            sup[0x93 + c] = (uint8_t)lab_s8(v[LAB_SHOTS]);
        }
        if ((b >> LAB_SIZE) & 1) {
            lab_wrf(sup + 0x3C + c * 4, (float)v[LAB_SIZE] / 100.0f);
        }
        if ((b >> LAB_SPEED) & 1) {
            lab_wrf(sup + 0x54 + c * 4, (float)v[LAB_SPEED]);
        }
        if ((b >> LAB_HOMING) & 1) {
            lab_wrf(sup + 0x60 + c * 4, (float)v[LAB_HOMING]);
        }
        if ((b >> LAB_TIME) & 1) {
            lab_wrf(sup + 0x24 + c * 4, (float)v[LAB_TIME] / 100.0f);
        }
        if ((b >> LAB_PUSH) & 1) {
            float was = lab_rdf(sup + 0x16C + c * 4), g = lab_rdf(sup + 0x178 + c * 4), now = (float)v[LAB_PUSH] / 10.0f;

            lab_wrf(sup + 0x16C + c * 4, now);
            lab_wrf(sup + 0x178 + c * 4, was > 0.0f ? g * now / was : now);
        }
        if ((b >> LAB_CHARGE) & 1) {
            lab_wrf(sup + 0x1D8 + c * 4, (float)v[LAB_CHARGE] / 100.0f);
        }
        if ((b >> LAB_RECOIL) & 1) {
            lab_wr32(sup + 0x1A8 + c * 4, v[LAB_RECOIL]);
        }
        if ((b >> LAB_UNBLOCK) & 1) { /* (the second flag word's bit 1: only a super guard stops it, BtlColl) */
            uint32_t f = (uint32_t)lab_rd32(sup + 0x0C + c * 4);

            lab_wr32(sup + 0x0C + c * 4, (int32_t)(v[LAB_UNBLOCK] ? f | 1u : f & ~1u));
        }
    }
    for (t = 0; t < 2; t++) { /* the skills */
        int b = set[t];
        const int32_t *v = lab[t];

        if (b == 0 || ski == NULL || skiN < 0xC8) {
            continue;
        }
        if ((b >> LAB_DAMAGE) & 1) {
            int32_t was = lab_rd32(ski + 0x84 + t * 4), g = lab_rd32(ski + 0x8C + t * 4);

            lab_wr32(ski + 0x84 + t * 4, v[LAB_DAMAGE]);
            lab_wr32(ski + 0x8C + t * 4, was > 0 ? (int32_t)((int64_t)g * v[LAB_DAMAGE] / was) : v[LAB_DAMAGE] / 4);
        }
        if ((b >> LAB_COST) & 1) {
            ski[0x94 + t] = (uint8_t)lab_s8(v[LAB_COST]);
        }
        if ((b >> LAB_ATTACK) & 1) { /* (attack: melee and ki blasts) */
            ski[0x96 + t] = (uint8_t)lab_s8(v[LAB_ATTACK] - 100);
        }
        if ((b >> LAB_DEFENSE) & 1) { /* (defense: the damage taken) */
            ski[0x9A + t] = (uint8_t)lab_s8(v[LAB_DEFENSE] - 100);
        }
        if ((b >> LAB_KIBOOST) & 1) { /* (ki and speed) */
            ski[0x98 + t] = (uint8_t)lab_s8(v[LAB_KIBOOST] - 100);
        }
        if ((b >> LAB_BLAST) & 1) { /* (Blast 2 and Ultimate power) */
            ski[0x9C + t] = (uint8_t)lab_s8(v[LAB_BLAST] - 100);
        }
        if ((b >> LAB_TIME) & 1) {
            lab_wrf(ski + 0xA0 + t * 4, (float)v[LAB_TIME] / 100.0f);
        }
        if ((b >> LAB_HEAL) & 1) {
            lab_wr32(ski + 0xA8 + t * 4, v[LAB_HEAL]);
        }
        if ((b >> LAB_KI) & 1) {
            lab_wr32(ski + 0xB0 + t * 4, v[LAB_KI]);
        }
        if ((b >> LAB_UNBLOCK) & 1) { /* (a skill's: Max Power - the flag word's 0x200, "enters the powered-up state") */
            uint32_t f = (uint32_t)lab_rd32(ski + 0x08 + t * 4);

            lab_wr32(ski + 0x08 + t * 4, (int32_t)(v[LAB_UNBLOCK] ? f | 0x200u : f & ~0x200u));
        }
    }
}

#include "plat_warrior_parts.inc"
static const char *const kWarriorPartKey[WP_SLOTS] = {"hair",  "head", "face",  "chest", "waist", "upperarms",
                                                      "forearms", "hands", "hips", "shins", "feet", "accessories"};
static const char *const kWarriorSizeKey[WP_SIZES] = {"headsize", "hairsize", "handsize", "footsize", "armbuild", "torsobuild", "legbuild"};

/* A warrior.txt key of the parts: its slot (*n slots from it: the old torso= / arms= / legs= keys name several), -1
   none. */
static int warrior_part_key(const char *key, int *n) {
    int s;

    *n = 1;
    for (s = 0; s < WP_SLOTS; s++) {
        if (strcmp(key, kWarriorPartKey[s]) == 0) {
            return s;
        }
    }
    *n = 2;
    if (strcmp(key, "torso") == 0) {
        return WP_CHEST;
    }
    *n = 3;
    if (strcmp(key, "arms") == 0) {
        return WP_UPPERARMS;
    }
    if (strcmp(key, "legs") == 0) {
        return WP_HIPS;
    }
    return -1;
}

static int warrior_size_key(const char *key) {
    int s;

    for (s = 0; s < WP_SIZES; s++) {
        if (strcmp(key, kWarriorSizeKey[s]) == 0) {
            return s;
        }
    }
    return -1;
}

/* The donors of a look's parts: their model files opened (one per fighter; costume k, else their first), part 2 and
   11 of each slot's. */
typedef struct WarriorParts {
    WarriorPak pak[WP_ORIGINS];
    const uint8_t *dm[WP_ORIGINS], *dt[WP_ORIGINS];
    uint32_t dmn[WP_ORIGINS], dtn[WP_ORIGINS];
    int any;
} WarriorParts;

/* The chains' swing for a look made of several fighters (model part 22: per node id, BObjChainParam of
   include/battle/bobj_b.h: 16 slots of type A, the strands of hair and cloth; 8 of type B, the tails): the body's for
   its own chains, each donor's for the chains that came from it (found by the id they had there, wp_chainOrig) under
   the id they have now (a donor's long hair swings as it did on its fighter, not stiff with the body's settings for
   what had those ids). NULL: nothing to change. */
static void warrior_chain_a(uint8_t *d, int i, const uint8_t *s, int j, int id) {
    static const int f4[] = {0x10, 0x50, 0x90, 0x110, 0x150, 0x1D0, 0x210, 0x260, 0x2A0}; /* (the floats) */
    static const int f2[] = {0xE0, 0x1A0};                                                 /* (the shorts) */
    static const int f1[] = {0xD0, 0x100, 0x190, 0x1C0, 0x250, 0x2E0};                     /* (the bytes) */
    int q;

    d[i] = (uint8_t)id;
    for (q = 0; q < 9; q++) {
        memcpy(d + f4[q] + i * 4, s + f4[q] + j * 4, 4);
    }
    for (q = 0; q < 2; q++) {
        memcpy(d + f2[q] + i * 2, s + f2[q] + j * 2, 2);
    }
    for (q = 0; q < 6; q++) {
        d[f1[q] + i] = s[f1[q] + j];
    }
}

static void warrior_chain_b(uint8_t *d, int i, const uint8_t *s, int j, int id) {
    d[0x2F0 + i] = (uint8_t)id;
    d[0x2F8 + i] = s[0x2F8 + j];
    memcpy(d + 0x300 + i * 4, s + 0x300 + j * 4, 4);
    memcpy(d + 0x320 + i * 4, s + 0x320 + j * 4, 4);
    d[0x340 + i] = s[0x340 + j];
    d[0x348 + i] = s[0x348 + j];
}

static uint8_t *warrior_chain_params(const uint8_t *body, uint32_t n, const WarriorParts *wp, uint32_t *outN) {
    uint8_t *out;
    int id, i, any = 0;

    if (body == NULL || n < 0x350) {
        return NULL;
    }
    for (id = 0x47; id < WP_NODES; id++) {
        any |= wp_chainFrom[id] >= 0 && (wp_chainFrom[id] != WP_BODY || wp_chainOrig[id] != id);
    }
    if (!any || (out = malloc(n)) == NULL) {
        return NULL;
    }
    memcpy(out, body, n);
    for (i = 0; i < 16; i++) { /* (the body's slots for chains it no longer has as its own: free) */
        if (out[i] >= 0x47 && out[i] < WP_NODES && wp_chainFrom[out[i]] != WP_BODY) {
            out[i] = 0;
        }
    }
    for (i = 0; i < 8; i++) {
        if (out[0x2F0 + i] >= 0x47 && out[0x2F0 + i] < WP_NODES && wp_chainFrom[out[0x2F0 + i]] != WP_BODY) {
            out[0x2F0 + i] = 0;
        }
    }
    for (id = 0x47; id < WP_NODES; id++) { /* each donor's chain node: its slot, from its fighter's parameters */
        int f = wp_chainFrom[id], t, j, k;
        const uint8_t *src = NULL;
        uint32_t sn = 0;

        if (f < 0 || f == WP_BODY || f >= WP_BODY) {
            continue;
        }
        for (t = 0; t < WP_BODY && src == NULL; t++) { /* (its fighter's file: opened once, under one of its origins) */
            if (wp->pak[t].d != NULL && wp->dm[t] == wp->dm[f]) {
                src = warrior_part(&wp->pak[t], 22, &sn);
            }
        }
        if (src == NULL || sn < 0x350) {
            continue;
        }
        for (j = 0; j < 16 && src[j] != wp_chainOrig[id]; j++) {
        }
        if (j < 16) {
            for (k = 0; k < 16 && out[k] >= 0x47; k++) {
            }
            if (k < 16) {
                warrior_chain_a(out, k, src, j, id);
            }
            continue;
        }
        for (j = 0; j < 8 && src[0x2F0 + j] != wp_chainOrig[id]; j++) {
        }
        if (j < 8) {
            for (k = 0; k < 8 && out[0x2F0 + k] >= 0x47; k++) {
            }
            if (k < 8) {
                warrior_chain_b(out, k, src, j, id);
            }
        }
    }
    *outN = n;
    return out;
}

static int warrior_parts_open(WarriorParts *wp, const int *part, const int *layer, int k, const int *cos, const int *xl) {
    int s, t, who[WP_ORIGINS], file[WP_ORIGINS];

    memset(wp, 0, sizeof(*wp));
    for (s = 0; s < WP_ORIGINS; s++) { /* the slots' fighters, then the layers' (cos: their costumes, 12 then 12; NULL theirs),
                                          the more layers' (xl: fighters, slots, costumes, 4 each; NULL none) */
        if (s == WP_BODY) {
            who[s] = file[s] = -1;
            continue;
        }
        if (s == WP_SHF) {
            who[s] = xl != NULL && chara_ok(xl[12]) ? xl[12] : -1;
            file[s] = who[s] >= 0 ? warrior_costume_file(who[s], 0, k) : -1;
            continue;
        }
        if (s >= WP_XL) {
            int x = s - WP_XL;

            who[s] = xl != NULL && xl[4 + x] >= 0 && xl[4 + x] <= WP_SLOTS ? xl[x] : -1;
            file[s] = who[s] >= 0 ? warrior_costume_file(who[s], xl[8 + x] > 0 ? xl[8 + x] : 0, k) : -1;
            wp_xTail[x] = who[s] >= 0 && xl[4 + x] == WP_SLOTS; /* (the tail: an accessories layer of only the waist's and hips' chains) */
            wp_xSlot[x] = who[s] < 0 ? -1 : wp_xTail[x] ? WP_EXTRAS : xl[4 + x];
            continue;
        }
        who[s] = s < WP_SLOTS ? (part != NULL ? part[s] : -1) : (layer != NULL ? layer[s - WP_SLOTS] : -1);
        file[s] = who[s] >= 0 ? warrior_costume_file(who[s], cos != NULL ? cos[s] : 0, k) : -1;
    }
    part = who;
    for (s = 0; s < WP_ORIGINS; s++) {
        if (s == WP_BODY) {
            continue;
        }
        if (part[s] == -2) { /* (taken away) */
            wp->dm[s] = wp->dt[s] = wp_empty;
            wp->any = 1;
            continue;
        }
        if (!chara_ok(part[s])) {
            continue;
        }
        wp->any = 1;
        for (t = 0; t < s; t++) {
            if (part[t] == part[s] && file[t] == file[s] && wp->dm[t] != NULL) {
                break;
            }
        }
        if (t < s) { /* (that fighter is open already) */
            wp->dm[s] = wp->dm[t];
            wp->dt[s] = wp->dt[t];
            wp->dmn[s] = wp->dmn[t];
            wp->dtn[s] = wp->dtn[t];
            continue;
        }
        if (!warrior_open(&wp->pak[s], file[s]) || wp->pak[s].n != WARRIOR_PARTS) {
            free(wp->pak[s].d);
            memset(&wp->pak[s], 0, sizeof(wp->pak[s]));
            if (!warrior_open(&wp->pak[s], MODEL_FILE + part[s] * 10) || wp->pak[s].n != WARRIOR_PARTS) {
                return 0;
            }
        }
        wp->dm[s] = warrior_part(&wp->pak[s], 2, &wp->dmn[s]);
        wp->dt[s] = warrior_part(&wp->pak[s], 11, &wp->dtn[s]);
    }
    return 1;
}

static void warrior_parts_close(WarriorParts *wp) {
    int s;

    for (s = 0; s < WP_ORIGINS; s++) {
        free(wp->pak[s].d);
    }
    memset(wp, 0, sizeof(*wp));
}

/* The pak the face slot's donor was opened as (its expression sets), NULL when the face is the body's. */
static const WarriorPak *warrior_parts_face(const WarriorParts *wp, const int *part) {
    int t;

    if (wp->dm[WP_FACE] == NULL) {
        return NULL;
    }
    for (t = 0; t < WP_SLOTS; t++) {
        if (wp->pak[t].d != NULL && part[t] == part[WP_FACE]) {
            return &wp->pak[t];
        }
    }
    for (t = 0; t < WP_SLOTS; t++) { /* (opened as a layer first) */
        if (wp->dm[WP_LAYER + t] == wp->dm[WP_FACE] && wp->pak[WP_LAYER + t].d != NULL) {
            return &wp->pak[WP_LAYER + t];
        }
    }
    return NULL;
}

#include "plat_warrior_paint.inc"

#define WARRIOR_W_BODY 36 /* (PortWarrior's fields, in words: checked where it is defined) */
#define WARRIOR_W_PART 153 /* (57 + 2 * WARRIOR_COLOURS: the colours' keys and values before it) */
#define WARRIOR_W_LAYER 173
#define WARRIOR_W_CPART 185
#define WARRIOR_W_SIZE 165
#define WARRIOR_W_EDITS 240
#define WARRIOR_W_COSTUME 669 /* (partCostume[12], layerCostume[12], bodyCostume) */

static uint8_t sLookFrom[WP_MAX_BATCHES]; /* the look's batches' origins (wp_lastFrom), sLookFroms of them */
static int sLookFroms;
static const int32_t *sLookEdit; /* (the edits whose copies of pieces the next warrior_look makes: WP_DUP; NULL none) */
static int sLookEdits;

/* A warrior's look in its first costume: the body's model (part 2) and textures (part 11) with its part swaps, as
   malloc'ed copies. 1 made, 0 not. */
static int sLookXL[13] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; /* (the more layers of the look being made) */
static int warrior_look(int body, const int32_t *part, const int32_t *layer, const int32_t *cpart, int ncol, const int32_t *sizes, const int32_t *cos,
                        uint8_t **mdl, uint32_t *mdlSize, uint8_t **tex, uint32_t *texSize) {
    static WarriorParts wp;
    WarriorPak b;
    const uint8_t *bm, *bt;
    uint32_t bmn, btn;
    const char *why;
    int r = 0, ok;
    int who[WP_SLOTS], lay[WP_SLOTS], s;

    *mdl = *tex = NULL;
    memset(&b, 0, sizeof(b));
    if (!chara_ok(body) || !warrior_open(&b, warrior_costume_file(body, cos[24], 0)) || b.n != WARRIOR_PARTS) {
        free(b.d);
        memset(&b, 0, sizeof(b));
        if (!chara_ok(body) || !warrior_open(&b, MODEL_FILE + body * 10) || b.n != WARRIOR_PARTS) {
            free(b.d);
            return 0;
        }
    }
    bm = warrior_part(&b, 2, &bmn);
    bt = warrior_part(&b, 11, &btn);
    for (s = 0; s < WP_SLOTS; s++) {
        who[s] = part != NULL ? part[s] : -1;
        lay[s] = layer != NULL ? layer[s] : -1;
    }
    {
        int cs[24];

        for (s = 0; s < 24; s++) {
            cs[s] = cos[s];
        }
        ok = warrior_parts_open(&wp, who, lay, 0, cs, sLookXL);
    }
    if (ok) {
        int sz[8], q;

        for (q = 0; q < 8; q++) {
            sz[q] = sizes != NULL ? sizes[q] : 0;
        }
        r = wp_compose(bm, bmn, bt, btn, wp.dm, wp.dmn, wp.dt, wp.dtn, sizes != NULL ? sz : NULL, mdl, mdlSize, tex, texSize, &why);
    }
    if (ok && r > 0 && sLookEdit != NULL) { /* (pieces copied: as the build makes them) */
        uint8_t *dm;
        uint32_t dmn;

        if (wp_dups(*mdl, *mdlSize, sLookEdit, sLookEdits, 1, &dm, &dmn) == 1) {
            free(*mdl);
            *mdl = dm;
            *mdlSize = dmn;
        }
    }
    sLookFroms = ok && r > 0 ? wp_lastFroms : 0; /* (which batches are a layer's: the workshop's pieces) */
    if (sLookFroms > 0) {
        memcpy(sLookFrom, wp_lastFrom, (size_t)sLookFroms);
    }
    if (ok && r == 0) { /* (no part swapped: the body's own) */
        if ((*mdl = malloc(bmn)) != NULL && (*tex = malloc(btn)) != NULL) {
            uint8_t *dm;
            uint32_t dmn;

            memcpy(*mdl, bm, bmn);
            memcpy(*tex, bt, btn);
            *mdlSize = bmn;
            *texSize = btn;
            r = 1;
            if (sLookEdit != NULL && wp_dups(*mdl, *mdlSize, sLookEdit, sLookEdits, 0, &dm, &dmn) == 1) {
                free(*mdl);
                *mdl = dm;
                *mdlSize = dmn;
            }
        }
    }
    warrior_parts_close(&wp);
    free(b.d);
    if (r != 1) {
        free(*mdl);
        free(*tex);
        *mdl = *tex = NULL;
        return 0;
    }
    if (cpart != NULL) { /* parts with colours of their own */
        int parts[WARRIOR_COLOURS], groups[WP_SPLIT_GROUPS], ng, c;
        uint8_t *sm, *st;
        uint32_t stn;

        for (c = 0; c < ncol && c < WARRIOR_COLOURS; c++) {
            parts[c] = cpart[c];
        }
        ng = wp_colour_groups(parts, ncol < WARRIOR_COLOURS ? ncol : WARRIOR_COLOURS, groups);
        if (ng > 0 && wp_split(*mdl, *mdlSize, *tex, *texSize, groups, ng, &sm, &st, &stn) == 1) {
            free(*mdl);
            free(*tex);
            *mdl = sm;
            *tex = st;
            *texSize = stn;
        }
    }
    return 1;
}

/* The look (model and textures, unpainted) of the warrior the screen asks about, kept while its parts are the same. */
static uint8_t *sLookMdl, *sLookTex;
static uint32_t sLookMdlSize, sLookTexSize;
static int32_t sLookWho[1 + 24 + 1 + WARRIOR_COLOURS + 8 + 25 + 2 + 13]; /* (as warrior_look_cached's who[]: its extra layers too) */
static int sLookGen; /* (counts the looks made: what is kept from one is made again) */

static void warrior_xl_of(const int32_t *w, int *xl);
static int warrior_look_cached(const int32_t *w) {
    enum { W_CP = 26, W_SZ = W_CP + WARRIOR_COLOURS, W_CO = W_SZ + 8, W_SH = W_CO + 25, W_DU = W_SH + 1, W_XL = W_DU + 1 };
    int32_t who[W_XL + 13];
    typedef char who_fits[sizeof(who) == sizeof(sLookWho) ? 1 : -1];
    int c, ncol = w[WARRIOR_W_BODY + 18 + 2], r2, edits = w[WARRIOR_W_EDITS] < 0 ? 0 : w[WARRIOR_W_EDITS] > WARRIOR_EDITS ? WARRIOR_EDITS : w[WARRIOR_W_EDITS];

    who[0] = w[WARRIOR_W_BODY];
    memcpy(who + 1, w + WARRIOR_W_PART, 12 * 4);
    memcpy(who + 13, w + WARRIOR_W_LAYER, 12 * 4);
    who[25] = ncol;
    for (c = 0; c < WARRIOR_COLOURS; c++) { /* (the parts with colours of their own make another look) */
        who[W_CP + c] = c < ncol ? w[WARRIOR_W_CPART + c] : 0;
    }
    memcpy(who + W_SZ, w + WARRIOR_W_SIZE, 8 * 4);
    memcpy(who + W_CO, w + WARRIOR_W_COSTUME, 25 * 4); /* (the parts' costumes, the body's) */
    who[W_SH] = 0;
    who[W_DU] = 0;
    warrior_xl_of(w, (int *)(who + W_XL)); /* (its more layers: in the look too, so their colours can be painted) */
    for (c = 0; c < edits; c++) { /* (shading edits: made as the build makes it, the pieces alike; pieces copied: another look) */
        const int32_t *x = w + WARRIOR_W_EDITS + 1 + c * WP_EDIT_WORDS;

        who[W_SH] |= x[0] == WP_SHADE;
        if (x[0] == WP_DUP) {
            who[W_DU] = (who[W_DU] ^ x[1] ^ x[3] * 131 ^ x[4] * 7919) * 16777619;
            who[W_DU] |= 1;
        }
    }
    if (sLookTex != NULL && memcmp(who, sLookWho, sizeof(who)) == 0) {
        return 1;
    }
    free(sLookMdl);
    free(sLookTex);
    sLookMdl = sLookTex = NULL;
    wp_forceCompose = who[W_SH];
    sLookEdit = who[W_DU] != 0 ? w + WARRIOR_W_EDITS + 1 : NULL;
    sLookEdits = edits;
    memcpy(sLookXL, who + W_XL, sizeof(sLookXL));
    r2 = warrior_look(who[0], who + 1, who + 13, who + W_CP, ncol, who + W_SZ, who + W_CO, &sLookMdl, &sLookMdlSize, &sLookTex, &sLookTexSize);
    memset(sLookXL, 0xFF, sizeof(sLookXL));
    sLookEdit = NULL;
    wp_forceCompose = 0;
    if (!r2) {
        return 0;
    }
    memcpy(sLookWho, who, sizeof(who));
    sLookGen++;
    return 1;
}

/* A warrior's colour groups (the parts with colours of their own), as the builder makes them. */
static int warrior_groups_of(const int32_t *w, int *groups) {
    int parts[WARRIOR_COLOURS], c, ncol = w[WARRIOR_W_BODY + 18 + 2];

    for (c = 0; c < WARRIOR_COLOURS; c++) {
        parts[c] = c < ncol ? w[WARRIOR_W_CPART + c] : 0;
    }
    return wp_colour_groups(parts, ncol < WARRIOR_COLOURS ? ncol : WARRIOR_COLOURS, groups);
}

/* The pieces of a warrior's look (its parts, sizes and builds made, before the edits): up to `max` records of
   {node, piece, slot (-1 the body's always), vertices}, in the model's order. */
static WpPieces sPieces;
static int sPiecesGen = -1;

static int warrior_pieces_cached(const int32_t *w) {
    if (!warrior_look_cached(w)) {
        return 0;
    }
    if (sPiecesGen != sLookGen) {
        wp_pieces_free(&sPieces);
        wp_pieces(sLookMdl, sLookMdlSize, &sPieces, sLookFroms > 0 ? sLookFrom : NULL, sLookFroms);
        sPiecesGen = sLookGen;
    }
    return 1;
}

int Port_WarriorPieces(uint32_t warriorAddr, uint32_t outAddr, int max) {
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    int i;

    if (!warrior_pieces_cached((const int32_t *)(uintptr_t)warriorAddr)) {
        return 0;
    }
    for (i = 0; i < sPieces.n && i < max; i++) {
        out[i * 4] = sPieces.p[i].node;
        out[i * 4 + 1] = sPieces.p[i].comp;
        out[i * 4 + 2] = sPieces.p[i].layer >= 2 ? 25 + sPieces.p[i].layer - 2 : /* (25 + x: extra layer x's) */
                         sPieces.p[i].slot >= 0 && sPieces.p[i].layer ? 13 + sPieces.p[i].slot : sPieces.p[i].slot; /* (13 + slot: a layer's) */
        out[i * 4 + 3] = sPieces.p[i].count;
    }
    return i;
}

/* The warrior's pieces' edits made straight on a model the game has loaded (`mdlAddr`: the model on the screen's, made
   from this look): its vertices first back to the look's, then edited; `extra` (a node, a piece) is hidden besides
   (the screen blinks the piece it is on; -1 none). Nothing when its layout is not the look's. 1 made. */
static int sShapeTouched; /* the model on screen has had edits made on it (so it is put back when they are gone) */
static uint32_t sShapeModel;

int Port_WarriorShapeLive(uint32_t warriorAddr, uint32_t mdlAddr, int extraNode, int extraPiece) {
    const int32_t *w = (const int32_t *)(uintptr_t)warriorAddr;
    uint8_t *m = (uint8_t *)(uintptr_t)mdlAddr;
    static WpRec a[WP_MAX_RECS * 2], b[WP_MAX_RECS * 2];
    static int32_t edit[(WARRIOR_EDITS + 1) * WP_EDIT_WORDS];
    int na, nb, i, k, edits;

    if (mdlAddr != sShapeModel) { /* (another model: as it was read, nothing of ours on it) */
        sShapeModel = mdlAddr;
        sShapeTouched = 0;
    }
    if (m == NULL || (w[WARRIOR_W_EDITS] <= 0 && extraNode == -1000 && !sShapeTouched)) {
        return 0; /* (no edits and none made before on this model: it is left as it was loaded) */
    }
    if (!warrior_pieces_cached(w)) {
        return 0;
    }
    na = wp_records(sLookMdl, sLookMdlSize, a, WP_MAX_RECS * 2);
    nb = wp_records(m, sLookMdlSize, b, WP_MAX_RECS * 2);
    if (na == 0 || na != nb) {
        return 0;
    }
    for (i = 0; i < na; i++) {
        if (a[i].off != b[i].off || a[i].size != b[i].size || a[i].node != b[i].node) {
            return 0; /* (another model) */
        }
    }
    for (i = 0; i < sPieces.n; i++) { /* back to the look's */
        for (k = 0; k < sPieces.p[i].count; k++) {
            uint32_t o = sPieces.off[sPieces.p[i].first + k];

            memcpy(m + o, sLookMdl + o, 7 * 4); /* (place, weight, normal and texture place: a hidden piece's cap sets them; */
            memcpy(m + o + 8 * 4, sLookMdl + o + 8 * 4, 2 * 4); /* word 7 is the game's own once loaded, left alone) */
        }
    }
    for (i = 0; i < na; i++) {
        if (a[i].size > 0x40) {
            memcpy(m + a[i].off + 0x40, sLookMdl + a[i].off + 0x40, 0x20);
        }
    }
    edits = w[WARRIOR_W_EDITS];
    edits = edits < 0 ? 0 : edits > WARRIOR_EDITS ? WARRIOR_EDITS : edits;
    memcpy(edit, w + WARRIOR_W_EDITS + 1, (size_t)edits * WP_EDIT_WORDS * 4);
    if (extraNode != -1000) { /* (the piece under the cursor: swollen a little, not hidden: hiding opened the holes
                                    being looked at) */
        int32_t *e = &edit[edits * WP_EDIT_WORDS];

        memset(e, 0, WP_EDIT_WORDS * 4);
        e[0] = extraNode;
        e[1] = extraPiece;
        e[3] = e[4] = e[5] = 115;
        for (k = 0; k < edits; k++) { /* (on top of its own edit) */
            if (edit[k * WP_EDIT_WORDS] == extraNode && edit[k * WP_EDIT_WORDS + 1] == extraPiece) {
                memcpy(e, &edit[k * WP_EDIT_WORDS], WP_EDIT_WORDS * 4);
                e[3] = e[3] * 115 / 100, e[4] = e[4] * 115 / 100, e[5] = e[5] * 115 / 100;
                edit[k * WP_EDIT_WORDS] = WP_PATCH; /* (that one stands aside for the frame: an edit no piece takes) */
            }
        }
        edits++;
    }
    wp_apply_edits(m, sLookMdl, &sPieces, edit, edits);
    sShapeTouched = edits > 0;
    return 1;
}

/* The effect type of a fighter's technique (slot 2..4: Blast 2, Blast 2, Ultimate), -1 none: a technique's effects can
   come from another of the same type. */
int Port_FighterMoveFx(int chara, int slot) {
    static int cache[200 + MOD_REF_MAX][3];
    static unsigned char have[200 + MOD_REF_MAX];
    WarriorPak p;
    uint32_t n;
    const uint8_t *q;
    int t, c = is_mod_ref(chara) ? 200 + chara - MOD_REF_BASE : chara;

    if (!chara_ok(chara) || (chara < CHARA_COUNT && chara >= 200) || slot < 2 || slot > 4) {
        return -1;
    }
    if (!have[c]) {
        have[c] = 1;
        memset(cache[c], 0xFF, sizeof(cache[c]));
        memset(&p, 0, sizeof(p));
        if (warrior_open(&p, MODEL_FILE + chara * 10) && p.n == WARRIOR_PARTS) {
            q = warrior_part(&p, 23, &n);
            for (t = 0; t < 3 && n >= 0x13B; t++) {
                cache[c][t] = q[0x138 + t];
            }
        }
        free(p.d);
    }
    return cache[c][slot - 2];
}

/* The colour regions of a warrior's look (its body with its parts, first costume) for the Z Warriors colours screen:
   up to `max` records of {key, average colour 0xBBGGRR, palette colours | flags << 16 (1 the face's own, 2 skin, 4
   hair), the parts using it (1 << slot)}. */
int Port_WarriorRegions(uint32_t warriorAddr, uint32_t outAddr, int max) {
    const int32_t *w = (const int32_t *)(uintptr_t)warriorAddr;

    int groups[WP_SPLIT_GROUPS], ng;

    if (!warrior_look_cached(w)) {
        return 0;
    }
    ng = warrior_groups_of(w, groups);
    return warrior_regions(sLookTex, sLookTexSize, sLookMdl, sLookMdlSize, (int32_t *)(uintptr_t)outAddr, max, NULL, groups, ng);
}

/* Which palette colours of a warrior's look are in region `key`: mask[p * 16 + k] = 1 for colour k of palette p (the
   palettes in the texture file's order, PSMT4 ones only, as the game's file of the made warrior has them). Returns
   how many palettes. */
int Port_WarriorZoneMask(uint32_t warriorAddr, int key, int own, uint32_t maskAddr) {
    const int32_t *w = (const int32_t *)(uintptr_t)warriorAddr;
    uint8_t *mask = (uint8_t *)(uintptr_t)maskAddr;
    static int32_t reg[WARRIOR_ZONES * 4];
    static int8_t which[64 * 16];
    uint32_t at[64];
    int n, r, i, groups[WP_SPLIT_GROUPS], ng;

    memset(mask, 0, 64 * 16);
    if (!(key & WARRIOR_KEY) || !warrior_look_cached(w)) {
        return 0;
    }
    ng = warrior_groups_of(w, groups);
    n = warrior_regions(sLookTex, sLookTexSize, sLookMdl, sLookMdlSize, reg, WARRIOR_ZONES, which, groups, ng);
    for (r = 0; r < n && !(reg[r * 4] == key && (reg[r * 4 + 3] >> 16) == own); r++) {
    }
    for (i = 0; r < n && i < 64 * 16; i++) {
        mask[i] = which[i] == r;
    }
    return warrior_palettes(sLookTex, sLookTexSize, at, 64);
}

/* Paints the warrior's colours straight into a texture file the game has loaded (`texAddr`: the model on the screen's,
   made from this look): its palettes and ramps first back to the look's, then painted. Nothing when its layout is not
   the look's. 1 painted. */
static WcInfo sInfo;
static int sInfoGen = -1;
static uint8_t *sPaintBefore;      /* the texture file on screen as it was before the last live paint */
static const uint8_t *sPaintAfter; /* (and that file, painted) */

int Port_WarriorPaintLive(uint32_t warriorAddr, uint32_t texAddr) {
    const int32_t *w = (const int32_t *)(uintptr_t)warriorAddr;
    uint8_t *d = (uint8_t *)(uintptr_t)texAddr;
    static int keys[WARRIOR_COLOURS], parts[WARRIOR_COLOURS];
    static uint32_t rgbs[WARRIOR_COLOURS];
    uint32_t n, i;
    int c, count;

    if (d == NULL || !warrior_look_cached(w)) {
        fprintf(stderr, "bt3: colours: live paint: %s\n", d == NULL ? "no model on screen" : "the look could not be made");
        fflush(stderr);
        return 0;
    }
    memcpy(&n, sLookTex, 4);
    if (memcmp(d, sLookTex, 16) != 0) {
        fprintf(stderr, "bt3: colours: live paint: the model on screen is another file (its texture file differs)\n");
        fflush(stderr);
        return 0; /* (another file) */
    }
    for (i = 0; i < n; i++) { /* the same textures, the same places (the pointers set when it was loaded aside) */
        if (memcmp(d + 0x20 + i * 0x40, sLookTex + 0x20 + i * 0x40, 0x30) != 0) {
            fprintf(stderr, "bt3: colours: live paint: the model on screen is another file (texture %u differs)\n", i);
            fflush(stderr);
            return 0;
        }
    }
    if (sPaintBefore == NULL) {
        sPaintBefore = malloc(sLookTexSize > 0x400000 ? sLookTexSize : 0x400000);
    }
    if (sPaintBefore != NULL && sLookTexSize <= 0x400000) { /* (what the expressions follow: Port_WarriorPaintFollow) */
        memcpy(sPaintBefore, d, sLookTexSize);
    }
    sPaintAfter = NULL;
    for (i = 0; i < n; i++) { /* palettes and ramps back to the look's */
        uint32_t f[4];

        memcpy(f, sLookTex + 0x20 + i * 0x40, 16);
        if (f[1] != 0 && f[1] * 4 + f[3] <= sLookTexSize) {
            memcpy(d + f[1] * 4, sLookTex + f[1] * 4, f[3]);
        }
        if (i >= sLookMdl[0x4F] && f[0] != 0 && f[0] * 4 + f[2] <= sLookTexSize) {
            memcpy(d + f[0] * 4, sLookTex + f[0] * 4, f[2]);
        }
    }
    count = w[WARRIOR_W_BODY + 18 + 2];
    for (c = 0; c < count && c < WARRIOR_COLOURS; c++) {
        keys[c] = w[WARRIOR_W_BODY + 18 + 3 + c];
        rgbs[c] = (uint32_t)w[WARRIOR_W_BODY + 18 + 3 + WARRIOR_COLOURS + c];
        parts[c] = w[WARRIOR_W_CPART + c];
    }
    if (sInfoGen != sLookGen) { /* (the look's analysis, once: reading its textures is what takes time) */
        wc_analyse(sLookTex, sLookTexSize, sLookMdl, sLookMdlSize, &sInfo);
        sInfoGen = sLookGen;
    }
    warrior_recolour_with(d, &sInfo, keys, rgbs, parts, count < WARRIOR_COLOURS ? count : WARRIOR_COLOURS);
    for (i = 0; sPaintBefore != NULL && sLookTexSize <= 0x400000 && i < n; i++) {
        /* the alpha bytes back as the game had them: it writes numbers there once a model is loaded (the parts the
           outline, the glow and the see-through pass read, port/src/gs/gs_draw.c); the file's have none of them */
        uint32_t f[4], q;

        memcpy(f, sLookTex + 0x20 + i * 0x40, 16);
        for (q = f[1] * 4 + 0x60 + 3; f[1] != 0 && q < f[1] * 4 + f[3] && q < sLookTexSize; q += 4) {
            d[q] = sPaintBefore[q];
        }
        for (q = f[0] * 4 + 0x60 + 3; i >= sLookMdl[0x4F] && f[0] != 0 && q < f[0] * 4 + f[2] && q < sLookTexSize; q += 4) {
            d[q] = sPaintBefore[q];
        }
    }
    sPaintAfter = sPaintBefore != NULL && sLookTexSize <= 0x400000 ? d : NULL;
    fprintf(stderr, "bt3: colours: live paint: %d colours painted on the model on screen\n", count);
    fflush(stderr);
    return 1;
}

/* The face's expression sets as the disc has them (parts 12..16 of the face's fighter, its first costume), for the
   look of sExprGen: what the live paint starts from each time. */
static uint8_t *sExpr[5];
static uint32_t sExprSize[5];
static int sExprGen = -1;

static void warrior_expr_orig(void) {
    WarriorPak pk;
    int j, face;

    if (sExprGen == sLookGen) {
        return;
    }
    sExprGen = sLookGen;
    for (j = 0; j < 5; j++) {
        free(sExpr[j]);
        sExpr[j] = NULL;
        sExprSize[j] = 0;
    }
    face = sLookWho[1 + WP_FACE] >= 0 ? sLookWho[1 + WP_FACE] : sLookWho[0];
    if (face < 0 || !warrior_open(&pk, MODEL_FILE + face * 10)) {
        return;
    }
    for (j = 0; j < 5; j++) {
        uint32_t n;
        const uint8_t *q = warrior_part(&pk, 12 + j, &n);

        if (n >= 0x20 && (sExpr[j] = malloc(n)) != NULL) {
            memcpy(sExpr[j], q, n);
            sExprSize[j] = n;
        }
    }
    free(pk.d);
}

/* An expression texture file of the model on screen (its eyes and brows, its mouths: parts 12..16) painted as the
   texture file was just painted: its colours first put back to the disc's (whatever was read or painted before), then
   made to follow the look's colours to the painted ones, as the builder does (warrior_recolour_follow). Never from
   the last paint (that drifted: the brows came and went as the eyes blinked). */
int Port_WarriorPaintFollow(uint32_t exprAddr) {
    uint8_t *e = (uint8_t *)(uintptr_t)exprAddr;
    uint32_t n, i, size;
    int j;

    if (e == NULL || sPaintAfter == NULL) {
        return 0;
    }
    warrior_expr_orig();
    memcpy(&n, e, 4);
    if (n == 0 || n > 64) {
        return 0;
    }
    size = 0x20 + n * 0x40;
    for (i = 0; i < n; i++) {
        uint32_t f[4];

        memcpy(f, e + 0x20 + i * 0x40, 16);
        if (f[0] * 4 + f[2] > size && f[0] * 4 + f[2] < 0x400000) {
            size = f[0] * 4 + f[2];
        }
        if (f[1] * 4 + f[3] > size && f[1] * 4 + f[3] < 0x400000) {
            size = f[1] * 4 + f[3];
        }
    }
    for (j = 0; j < 5; j++) { /* which of the disc's sets it is: the same textures, the same places */
        uint32_t m;

        if (sExpr[j] == NULL) {
            continue;
        }
        memcpy(&m, sExpr[j], 4);
        for (i = 0; m == n && i < n && 0x20 + i * 0x40 + 0x30 <= sExprSize[j]; i++) {
            if (memcmp(e + 0x20 + i * 0x40, sExpr[j] + 0x20 + i * 0x40, 0x30) != 0) {
                break;
            }
        }
        if (m == n && i == n) {
            break;
        }
    }
    if (j == 5) {
        fprintf(stderr, "bt3: colours: an expression set that is not the face's own: left as it is\n");
        fflush(stderr);
        return 0;
    }
    for (i = 0; i < n; i++) { /* its palettes back to the disc's (the game's alpha numbers kept) */
        uint32_t f[4], q;

        memcpy(f, e + 0x20 + i * 0x40, 16);
        for (q = f[1] * 4 + 0x60; f[1] != 0 && q + 3 < f[1] * 4 + f[3] && q + 3 < sExprSize[j] && q + 3 < size; q += 4) {
            memcpy(e + q, sExpr[j] + q, 3);
        }
    }
    {
        static uint8_t was[0x8000];
        uint32_t keep = size < sizeof(was) ? size : sizeof(was), q, changed = 0;

        memcpy(was, e, keep);
        warrior_recolour_follow(e, size, sLookTex, sPaintAfter, sLookTexSize);
        for (q = 0; q < keep; q++) {
            changed += was[q] != e[q];
        }
        fprintf(stderr, "bt3: colours: expression set of %u textures (%u bytes): %u bytes changed\n", n, size, changed);
        fflush(stderr);
    }
    return 1;
}

/* One model file of the warrior: costume kind k (0..7) of the body, with the moves and voice fighters' parts (of the
   same kind, else of their first costume: a costume a fighter lacks is a one-part stand-in on the disc) and the
   single parts of the stage 2 donors. Returns 1 when made, 0 when the body lacks that costume (nothing is written),
   -1 when it failed. */
static const char *sWarriorWhy; /* why the last warrior_model failed, NULL: the usual reason */

static int warrior_paint_apply(uint8_t *tex, uint32_t size, const char *dir);

/* Under the Super Saiyan lighting (the model's flag) the game's transformed fighters have short shading ramps: only the
   side turned furthest from the light shaded (about the first 56 of 256 entries). A long ramp of a base form there
   shades most of the body dark; so each ramp longer than that is pressed into that length, its tones kept. */
static void warrior_ramps_shorten(uint8_t *tex, uint32_t size, const uint8_t *mdl) {
    static WpTex ent[WP_MAX_TEX];
    static uint8_t old[1024];
    int count = wp_textures(tex, size, ent, WP_MAX_TEX), t, j;

    for (t = mdl[0x50]; t < mdl[0x50] + mdl[0x51] && t < count; t++) {
        uint8_t *p = tex + ent[t].pixOfs * 4 + 0x60;
        int last = -1;

        if (ent[t].pixOfs == 0 || ent[t].pixSize < 0x60 + 1024 || ent[t].pixOfs * 4 + 0x60 + 1024 > size) {
            continue;
        }
        for (j = 0; j < 256; j++) {
            if (p[j * 4] | p[j * 4 + 1] | p[j * 4 + 2]) {
                last = j;
            }
        }
        if (last <= 80) {
            continue; /* (short already) */
        }
        memcpy(old, p, 1024);
        {
            int sum[3] = {0, 0, 0}, n = 0, ch; /* (its tone: the average of its shaded part; one band, then half, then none --
                                                  bands pressed together read as metal) */

            for (j = 0; j <= last; j++) {
                for (ch = 0; ch < 3; ch++) {
                    sum[ch] += old[j * 4 + ch];
                }
                n++;
            }
            for (j = 0; j < 256; j++) {
                for (ch = 0; ch < 3; ch++) {
                    int v = sum[ch] / (n > 0 ? n : 1);

                    p[j * 4 + ch] = (uint8_t)(j < 44 ? v : j < 58 ? v / 2 : 0);
                }
            }
        }
    }
}

/* The face texture's 16 colours (model header 0x4C: the record the eye and expression textures stand in for) of a
   model (part 2) and its textures (part 11). 1 when read. */
static int warrior_face_clut(const uint8_t *mdl, uint32_t mn, const uint8_t *tex, uint32_t tn, uint8_t *out) {
    static WpTex ent[WP_MAX_TEX];
    int count;

    if (mdl == NULL || tex == NULL || mn < 0x60 || tn < 0x20) {
        return 0;
    }
    count = wp_textures(tex, tn, ent, WP_MAX_TEX);
    if (mdl[0x4C] >= count || ent[mdl[0x4C]].clutOfs == 0 || ent[mdl[0x4C]].clutOfs * 4 + 0x60 + 64 > tn) {
        return 0;
    }
    memcpy(out, tex + ent[mdl[0x4C]].clutOfs * 4 + 0x60, 64);
    return 1;
}

/* The face's expressions (parts 12..16: the eyes shut as it blinks, the hurt face) take the face's colours as the
   model has them now: each of their colours that was the face's colour before (shading, glow, shading by texture,
   the face's palette changed in any way) becomes that colour now. Without it each blink showed the old colours. */
static void warrior_expr_follow(const uint8_t **part, uint32_t *size, uint8_t **own, const uint8_t *before) {
    static WpTex ent[WP_MAX_TEX];
    uint8_t now[64];
    int i, t, e, count;

    if (!warrior_face_clut(part[2], size[2], part[11], size[11], now) || memcmp(now, before, 64) == 0) {
        return;
    }
    for (i = 12; i <= 16; i++) {
        if (size[i] < 0x20) {
            continue;
        }
        if (own[i] == NULL && (own[i] = malloc(size[i])) != NULL) {
            memcpy(own[i], part[i], size[i]);
        }
        if (own[i] == NULL) {
            continue;
        }
        part[i] = own[i];
        count = wp_textures(own[i], size[i], ent, WP_MAX_TEX);
        for (t = 0; t < count; t++) {
            uint8_t *c = own[i] + ent[t].clutOfs * 4 + 0x60;

            if (ent[t].clutOfs == 0 || ent[t].clutOfs * 4 + 0x60 + 64 > size[i] || ((ent[t].tex0 >> 20) & 0x3F) != 20) {
                continue;
            }
            for (e = 0; e < 16; e++) {
                if (memcmp(c + e * 4, before + e * 4, 4) == 0) {
                    memcpy(c + e * 4, now + e * 4, 4);
                }
            }
        }
    }
}

/* The warrior's techniques that are rushes (effect type 2 or 7 of the technique table, as built by warrior_model): a
   camera of another technique is not put on them (a rush's cinematic is its own; another one broke it). */
static int sTechRush[3];

/* The face's place in a model (its node 48 strips): the corners of the box round them. 1 found. */
static int warrior_face_box(const uint8_t *m, uint32_t n, float *lo, float *hi) {
    static WpRec r[WP_MAX_RECS];
    int nr, i, got = 0;

    if (m == NULL || n < 0x90 || (nr = wp_records(m, n, r, WP_MAX_RECS)) <= 0) {
        return 0;
    }
    for (i = 0; i < nr; i++) {
        uint32_t t = r[i].off + 0x60, end = r[i].off + r[i].size;

        if (r[i].node != 48 || r[i].size <= 0x60) {
            continue;
        }
        for (; t + 4 * 22 <= end; t += 4) {
            const uint32_t *w = (const uint32_t *)(m + t);
            int nv, v, q;

            if (w[0] != WP_VIF_BATCH) {
                continue;
            }
            nv = (int)(w[13] & 0x7FFF);
            if (t + (22 + 12 * (uint32_t)nv) * 4 > end) {
                break;
            }
            for (v = 0; v < nv; v++) {
                const float *x = (const float *)(w + 22 + v * 12);

                for (q = 0; q < 3; q++) {
                    if (!got || x[q] < lo[q]) lo[q] = x[q];
                    if (!got || x[q] > hi[q]) hi[q] = x[q];
                }
                got = 1;
            }
            t += (21 + 12 * (uint32_t)nv) * 4;
        }
    }
    return got;
}

/* The mouth's shapes (parts 3..10: the face's strips as it talks, in the model's space where the face's fighter had
   them) moved onto the face as it is now: the head put on another body, made bigger, moved in the workshop. Each
   vertex goes from the face's old box to its new one (each way as much bigger; only moved when a way changed a lot:
   pieces hidden, the box is not the face's any more). */
static void warrior_shapes_fit(uint8_t *sh, uint32_t n, const float *lo0, const float *hi0, const float *lo1, const float *hi1) {
    float k[3], c0[3], c1[3];
    uint32_t t;
    int q, scale = 1;

    for (q = 0; q < 3; q++) {
        float e0 = hi0[q] - lo0[q], e1 = hi1[q] - lo1[q];

        c0[q] = (lo0[q] + hi0[q]) * 0.5f;
        c1[q] = (lo1[q] + hi1[q]) * 0.5f;
        k[q] = e0 > 1e-3f ? e1 / e0 : 1.0f;
        scale &= k[q] > 0.6f && k[q] < 1.7f;
    }
    for (q = 0; q < 3; q++) {
        k[q] = scale ? k[q] : 1.0f;
    }
    for (t = 0; t + 4 * 22 <= n; t += 4) {
        uint32_t *w = (uint32_t *)(sh + t);
        int nv, v;

        if (w[0] != WP_VIF_BATCH) {
            continue;
        }
        nv = (int)(w[13] & 0x7FFF);
        if (t + (22 + 12 * (uint32_t)nv) * 4 > n) {
            break;
        }
        for (v = 0; v < nv; v++) {
            float *x = (float *)(w + 22 + v * 12);

            for (q = 0; q < 3; q++) {
                x[q] = c1[q] + (x[q] - c0[q]) * k[q];
            }
        }
        t += (21 + 12 * (uint32_t)nv) * 4;
    }
}

static int warrior_model(const WarriorSpec *ws, const WarriorDonors *dn, int k, const char *path) {
    static const uint8_t *part[WARRIOR_PARTS];
    static uint32_t size[WARRIOR_PARTS];
    WarriorPak src[3];
    const int who[3] = {ws->body, ws->moves, ws->voice};
    uint8_t *own[WARRIOR_PARTS]; /* parts made here (freed at the end) */
    float shapeLo[3] = {0, 0, 0}, shapeHi[3] = {0, 0, 0}; /* the face's box the mouth's shapes were made for */
    int shapeRef = 0;
    uint32_t i, n;
    int j, ok = -1, composed = 0; /* composed: part 2 made by wp_compose (wp_lastFrom is its) */
    uint8_t *dropLayout = NULL; /* (an outfit's pieces taken off: the model made first, the pieces numbered on it) */
    uint32_t dropSize = 0;
    uint8_t exprFace[64]; /* (the face's colours as its expressions were made for: warrior_expr_follow) */
    int exprFaceOk = 0;

    memset(src, 0, sizeof(src));
    memset(own, 0, sizeof(own));
    for (j = 0; j < 3; j++) {
        if (!warrior_open(&src[j], j == 0 ? warrior_costume_file(who[j], ws->bodyCos, k) : MODEL_FILE + who[j] * 10 + k) || src[j].n != WARRIOR_PARTS) {
            if (j == 0) {
                ok = 0;
                goto done;
            }
            free(src[j].d);
            if (!warrior_open(&src[j], MODEL_FILE + who[j] * 10) || src[j].n != WARRIOR_PARTS) {
                goto done;
            }
        }
    }
    for (i = 0; i < WARRIOR_PARTS; i++) {
        int from = i >= 52 ? 2 : (i >= 17 && i <= 51 && i != 22) ? 1 : 0;
        part[i] = warrior_part(&src[from], i, &size[i]);
    }
    exprFaceOk = warrior_face_clut(part[2], size[2], part[11], size[11], exprFace);
    shapeRef = warrior_face_box(part[2], size[2], shapeLo, shapeHi); /* (where the mouth's shapes are: the body's own face) */
    if (size[17] >= 0xC6 && (own[17] = malloc(size[17])) != NULL) { /* the parameter block */
        uint8_t *prm = own[17];
        const uint8_t *q;

        memcpy(prm, part[17], size[17]);
        q = warrior_part(&src[0], 17, &n);
        if (n >= 12) {
            memcpy(prm + 4, q + 4, 8); /* the body's height and radius */
        }
        if (dn->style.d != NULL) {
            q = warrior_part(&dn->style, 17, &n);
            if (n >= 0x58) {
                memcpy(prm + 0x28, q + 0x28, 0x58 - 0x28); /* ki, blast gauge and Max Power rates */
            }
        }
        if (dn->aura.d != NULL) {
            q = warrior_part(&dn->aura, 17, &n);
            if (n >= 4) {
                prm[3] = q[3]; /* the aura's kind (BtlCharApi_GetAuraType) */
                prm[0] = (uint8_t)((prm[0] & ~0x0Cu) | (q[0] & 0x0Cu)); /* its lightning (flags0 0x4, 0x8: EftAura_UpdateLightning) */
                prm[1] = (uint8_t)((prm[1] & ~0x40u) | (q[1] & 0x40u)); /* its blue variant (flags0 0x4000) */
            }
        }
        if (dn->fin.d != NULL) {
            q = warrior_part(&dn->fin, 17, &n);
            if (n >= 0x98) {
                uint32_t f, g;

                memcpy(prm + 0x84, q + 0x84, 0x98 - 0x84); /* the finisher of each rush step, the Max Power finishers */
                memcpy(&f, prm + 0x14, 4);
                memcpy(&g, q + 0x14, 4);
                f = (f & ~0x1800u) | (g & 0x1800u); /* flags2: the two finisher bits */
                memcpy(prm + 0x14, &f, 4);
                memcpy(&f, prm + 0x18, 4);
                memcpy(&g, q + 0x18, 4);
                f = (f & ~0x1000u) | (g & 0x1000u); /* flags3: the Max Power finisher */
                memcpy(prm + 0x18, &f, 4);
            }
        }
        part[17] = prm;
    }
    for (j = 0; j < 2; j++) { /* the Blast 1 skills: a column of the skill table and the line of the move list */
        const uint8_t *q;

        if (dn->skill[j].d == NULL) {
            continue;
        }
        q = warrior_part(&dn->skill[j], 24, &n);
        if (own[24] == NULL && size[24] >= 0xC8 && (own[24] = malloc(size[24])) != NULL) {
            memcpy(own[24], part[24], size[24]);
            part[24] = own[24];
        }
        if (own[24] != NULL && n >= 0xC8) {
            warrior_column(own[24], q, kWarriorSkill, sizeof(kWarriorSkill) / sizeof(kWarriorSkill[0]), 2, (uint32_t)j,
                           (uint32_t)ws->skillFrom[j]);
        }
        for (i = 43; i <= 51; i++) {
            uint32_t sz, made;
            const uint8_t *line = warrior_part(&dn->skill[j], i, &sz);
            uint8_t *text = warrior_name_line(part[i], size[i], line, sz, j, ws->skillFrom[j], &made);

            if (text != NULL) {
                free(own[i]);
                own[i] = text;
                part[i] = text;
                size[i] = made;
            }
        }
    }
    for (j = 0; j < 3; j++) { /* the techniques: a column of the technique table, the cutscene placement table and the
                                 line of the move list */
        const WarriorPak *d = &dn->tech[j];
        const uint8_t *q;
        uint32_t from = (uint32_t)ws->techFrom[j];

        if (d->d == NULL) {
            continue;
        }
        q = warrior_part(d, 23, &n);
        if (own[23] == NULL && size[23] >= 0x244 && (own[23] = malloc(size[23])) != NULL) {
            memcpy(own[23], part[23], size[23]);
            part[23] = own[23];
        }
        if (own[23] != NULL && n >= 0x244) {
            warrior_column(own[23], q, kWarriorSuper, sizeof(kWarriorSuper) / sizeof(kWarriorSuper[0]), 3, (uint32_t)j, from);
        }
        part[40 + j] = warrior_part(d, 40 + from, &size[40 + j]);
        q = warrior_part(d, 27, &n); /* its camera cut (cut t of part 27, and the Ultimate's opening pose cut 3) */
        if (own[27] == NULL && size[27] >= 0x24 * 4 && (own[27] = malloc(size[27])) != NULL) {
            memcpy(own[27], part[27], size[27]);
            part[27] = own[27];
        }
        if (own[27] != NULL && n >= 0x24 * 4) {
            memcpy(own[27] + 0x24 * j, q + 0x24 * from, 0x24);
            if (j == 2 && from == 2) {
                memcpy(own[27] + 0x24 * 3, q + 0x24 * 3, 0x24);
            }
        }
        for (i = 43; i <= 51; i++) {
            uint32_t sz, made;
            const uint8_t *line = warrior_part(d, i, &sz);
            uint8_t *text = warrior_name_line(part[i], size[i], line, sz, 2 + j, 2 + (int)from, &made);

            if (text != NULL) {
                free(own[i]);
                own[i] = text;
                part[i] = text;
                size[i] = made;
            }
        }
    }
    for (j = 0; j < 2; j++) { /* a Blast 1 skill's look from another (the skill table's effect rows: EftShotParam) */
        static const uint16_t kSkillFx[][3] = {{0x42, 1, 16}, {0x6E, 1, 3}};
        const uint8_t *q;

        if (dn->sfx[j].d == NULL) {
            continue;
        }
        q = warrior_part(&dn->sfx[j], 24, &n);
        if (own[24] == NULL && size[24] >= 0xC8 && (own[24] = malloc(size[24])) != NULL) {
            memcpy(own[24], part[24], size[24]);
            part[24] = own[24];
        }
        if (own[24] != NULL && n >= 0xC8) {
            warrior_column(own[24], q, kSkillFx, 2, 2, (uint32_t)j, (uint32_t)ws->skillFxFrom[j]);
        }
    }
    for (j = 0; j < 3; j++) { /* a technique's effects from another technique (its effect rows) */
        const uint8_t *q;

        if (dn->fx[j].d == NULL) {
            continue;
        }
        q = warrior_part(&dn->fx[j], 23, &n);
        if (own[23] == NULL && size[23] >= 0x244 && (own[23] = malloc(size[23])) != NULL) {
            memcpy(own[23], part[23], size[23]);
            part[23] = own[23];
        }
        if (own[23] != NULL && n >= 0x244) {
            warrior_column(own[23], q, kWarriorSuperFx, sizeof(kWarriorSuperFx) / sizeof(kWarriorSuperFx[0]), 3, (uint32_t)j, (uint32_t)ws->techFxFrom[j]);
        }
    }
    if (ws->labSet[0] | ws->labSet[1] | ws->labSet[2] | ws->labSet[3] | ws->labSet[4]) { /* the Technique Lab's numbers, last */
        if (own[23] == NULL && size[23] >= 0x244 && (own[23] = malloc(size[23])) != NULL) {
            memcpy(own[23], part[23], size[23]);
            part[23] = own[23];
        }
        if (own[24] == NULL && size[24] >= 0xC8 && (own[24] = malloc(size[24])) != NULL) {
            memcpy(own[24], part[24], size[24]);
            part[24] = own[24];
        }
        warrior_lab_apply(own[23], own[23] != NULL ? size[23] : 0, own[24], own[24] != NULL ? size[24] : 0, ws->labSet,
                          (const int32_t(*)[16])ws->lab);
    }
    for (j = 0; j < 3; j++) { /* a cinematic technique from another fighter: the character its throw brings in (+0x208, and its
                                 alternate +0x20E) is that fighter for "itself" - on the warrior, who the game counts as another
                                 fighter, it brought in a second one of them (Broly's): made the warrior's own */
        int16_t tc, me = (int16_t)(ws->style >= 0 ? ws->style : ws->moves), from = (int16_t)ws->tech[j];
        int q;

        if (ws->tech[j] < 0 || own[23] == NULL || size[23] < 0x244 || from == me) {
            continue;
        }
        for (q = 0; q < 2; q++) {
            memcpy(&tc, own[23] + (q == 0 ? 0x208 : 0x20E) + j * 2, 2);
            if (tc == from) {
                memcpy(own[23] + (q == 0 ? 0x208 : 0x20E) + j * 2, &me, 2);
            }
        }
    }
    for (j = 0; j < 3; j++) { /* (which techniques are rushes now) */
        sTechRush[j] = size[23] >= 0x244 && (part[23][0x138 + j] == 2 || part[23][0x138 + j] == 7);
    }
    for (j = 0; j < 3; j++) { /* a camera of another technique: its camera cut (part 27: cut t; the Ultimate's opening
                                 pose, cut 3). The rush / beam cinematic is the effect file's camera pack (warrior_effects);
                                 the Ultimate's loop (j 3) is that alone */
        const WarriorPak *d = &dn->cam[j];
        uint32_t from = (uint32_t)ws->camSlot[j];
        const uint8_t *q;

        if (d->d == NULL || from > 2 || sTechRush[j]) { /* (a rush keeps its own) */
            continue;
        }
        q = warrior_part(d, 27, &n);
        if (own[27] == NULL && size[27] >= 0x24 * 4 && (own[27] = malloc(size[27])) != NULL) {
            memcpy(own[27], part[27], size[27]);
            part[27] = own[27];
        }
        if (own[27] == NULL || q == NULL || n < 0x24 * 4) {
            continue;
        }
        memcpy(own[27] + 0x24 * j, q + 0x24 * from, 0x24);
        if (j == 2 && from == 2) {
            memcpy(own[27] + 0x24 * 3, q + 0x24 * 3, 0x24);
        }
    }
    for (j = 0; j < 5; j++) { /* the techniques' names of their own: the move list's line, in every language */
        if (ws->moveName[j][0] == 0) {
            continue;
        }
        for (i = 43; i <= 51; i++) {
            uint32_t made;
            uint8_t *text = warrior_rename_line(part[i], size[i], j, ws->moveName[j], &made);

            if (text != NULL) {
                free(own[i]);
                own[i] = text;
                part[i] = text;
                size[i] = made;
            }
        }
    }
    { /* parts of other fighters, sizes */
        static WarriorParts wp;
        uint32_t omn, otn;
        uint8_t *om, *ot;
        int cs[24], r;

        for (j = 0; j < 12; j++) {
            cs[j] = ws->partCos[j];
            cs[12 + j] = ws->layerCos[j];
        }
        int xl[13];

        xl[12] = ws->shadeFrom; /* (the shading fighter) */
        for (j = 0; j < 4; j++) { /* (the more layers: fighter, slot, costume) */
            xl[j] = ws->xlayer[j];
            xl[4 + j] = ws->xslot[j];
            xl[8 + j] = ws->xcos[j];
        }
        r = warrior_parts_open(&wp, ws->part, ws->layer, k, cs, xl) ? 1 : -1;
        wp_txN = 0; /* (shading by texture) */
        for (j = 0; j < 16; j++) {
            if (ws->texShade[j][0] != 0 && ws->texShade[j][1] >= 0x1000000) {
                wp_txKey[wp_txN] = (uint32_t)ws->texShade[j][0] & 0xFFFFFF;
                wp_txRgb[wp_txN++] = (uint32_t)ws->texShade[j][1];
                wp_forceCompose = 1; /* (made even with nothing swapped) */
            }
        }
        { /* (the mouth's shapes, of the face's fighter: wp_compose moves them with the textures) */
            const WarriorPak *mf = r > 0 ? warrior_parts_face(&wp, ws->part) : NULL;

            wp_mouthFrom = mf != NULL ? WP_FACE : WP_BODY;
            for (j = 0; j < 8; j++) {
                uint32_t mn = 0;

                wp_mouthIn[j] = mf != NULL ? warrior_part(mf, 3 + j, &mn) : part[3 + j];
                wp_mouthN[j] = mf != NULL ? mn : size[3 + j];
            }
            if (mf != NULL) { /* (the face's fighter's shapes: made for its face) */
                uint32_t fmn = 0;
                const uint8_t *fm = warrior_part(mf, 2, &fmn);

                shapeRef = warrior_face_box(fm, fmn, shapeLo, shapeHi);
            }
        }

        composed = 0;
        if (r > 0) {
            wp_shade_reset();
            for (wp_skirts = 0, wp_forceCompose = wp_txN > 0, j = 0; j < ws->edits; j++) { /* (shading by texture: made even
                                                                                              with nothing swapped) */ /* (holes patched: a donor's part comes with its own edge) */
                wp_skirts |= ws->edit[j * WP_EDIT_WORDS] == WP_PATCH;
                wp_forceCompose |= ws->edit[j * WP_EDIT_WORDS] == WP_SHADE; /* (shading edits: made even with nothing swapped) */
                if (ws->edit[j * WP_EDIT_WORDS] == WP_SHADE) { /* (a part's shading, and its picks) */
                    int q, e = ws->edit[j * WP_EDIT_WORDS + 1] < 0 ? WP_HAIR * 2 + 1 : ws->edit[j * WP_EDIT_WORDS + 1];

                    if (e < WP_SH_ENTRIES) {
                        wp_shMode[e] = ws->edit[j * WP_EDIT_WORDS + 3];
                        for (q = 0; q < 8; q++) {
                            wp_shPick[e][q] = ws->edit[j * WP_EDIT_WORDS + 4 + q];
                        }
                    }
                    if (e <= WP_GLOW_ALL) { /* (its glow; its outline group; the shading made for it) */
                        wp_shGlow[e] = ws->edit[j * WP_EDIT_WORDS + 2] & 0xFF;
                        wp_shLine[e] = (ws->edit[j * WP_EDIT_WORDS + 2] >> 8) & 0xFF;
                        wp_shStyle[e] = (ws->edit[j * WP_EDIT_WORDS + 2] >> 16) & 0xFF;
                        wp_shSize[e] = ws->edit[j * WP_EDIT_WORDS + 4];
                        wp_shStrength[e] = ws->edit[j * WP_EDIT_WORDS + 5];
                        wp_shTintKind[e] = ws->edit[j * WP_EDIT_WORDS + 6];
                        wp_shTintRgb[e] = (uint32_t)ws->edit[j * WP_EDIT_WORDS + 7];
                        if (wp_shStyle[e] >= 1 && wp_shStyle[e] <= 5 && e < WP_SH_ENTRIES) {
                            memset(wp_shPick[e], 0, sizeof(wp_shPick[e])); /* (its words are the shading's, not picks) */
                        }
                    }
                }
            }
            r = wp_compose(part[2], size[2], part[11], size[11], wp.dm, wp.dmn, wp.dt, wp.dtn, ws->size, &om, &omn, &ot, &otn, &sWarriorWhy);
            if (r > 0 && wp_drop_hidden(om, omn, ws->edit, ws->edits) > 0) { /* (an outfit's pieces taken off: made again without them;
                                                                               the first one kept, the pieces are numbered on it as
                                                                               the workshop's look numbers them) */
                free(ot);
                dropLayout = om;
                dropSize = omn;
                wp_dropOn = 1;
                r = wp_compose(part[2], size[2], part[11], size[11], wp.dm, wp.dmn, wp.dt, wp.dtn, ws->size, &om, &omn, &ot, &otn, &sWarriorWhy);
                wp_dropOn = 0;
            }
            wp_skirts = 0;
            wp_forceCompose = 0;
            wp_shade_reset();
        }
        if (r > 0) {
            const WarriorPak *fp = warrior_parts_face(&wp, ws->part);
            uint8_t *cp;
            uint32_t cpn;

            composed = 1;
            if ((cp = warrior_chain_params(part[22], size[22], &wp, &cpn)) != NULL) { /* the chains' swing: each the
                                                                                   fighter's it came from */
                free(own[22]);
                own[22] = cp;
                part[22] = cp;
                size[22] = cpn;
            }

            free(own[2]);
            free(own[11]);
            own[2] = om;
            own[11] = ot;
            part[2] = om;
            size[2] = omn;
            part[11] = ot;
            size[11] = otn;
            for (i = 3; i <= 10; i++) { /* the mouth's shapes, moved with the textures (wp_compose) */
                if (wp_mouthOut[i - 3] != NULL) {
                    free(own[i]);
                    own[i] = wp_mouthOut[i - 3];
                    wp_mouthOut[i - 3] = NULL;
                    part[i] = own[i];
                    size[i] = wp_mouthN[i - 3];
                }
            }
            if (fp != NULL) { /* (the face's colours as the face fighter's expressions were made for) */
                uint32_t fmn = 0, ftn = 0;
                const uint8_t *fm = warrior_part(fp, 2, &fmn), *ft = warrior_part(fp, 11, &ftn);

                exprFaceOk = warrior_face_clut(fm, fmn, ft, ftn, exprFace);
            }
            for (i = 11; fp != NULL && i <= 16; i++) { /* the face's sets: its eyes and expressions */
                const uint8_t *q;

                if (i == 11) {
                    continue;
                }
                q = warrior_part(fp, i, &n);
                free(own[i]);
                own[i] = NULL;
                if (n > 0 && (own[i] = malloc(n)) != NULL) {
                    memcpy(own[i], q, n);
                }
                part[i] = own[i] != NULL ? own[i] : q;
                size[i] = own[i] != NULL ? n : 0;
            }
        }
        warrior_parts_close(&wp);
        for (j = 0; j < 8; j++) { /* (the shapes given to wp_compose are this build's: not kept for another) */
            wp_mouthIn[j] = NULL;
            free(wp_mouthOut[j]);
            wp_mouthOut[j] = NULL;
        }
        wp_mouthFrom = -1;
        wp_txN = 0;
        if (r < 0) {
            if (sWarriorWhy == NULL || sWarriorWhy[0] == '\0') {
                sWarriorWhy = "A fighter named for a part was not found on the disc.";
            }
            goto done;
        }
    }
    if (ws->edits > 0 && size[2] > 0) { /* pieces copied (the workshop's look makes them the same way) */
        uint8_t *dm;
        uint32_t dmn;

        if (dropLayout != NULL && dropSize == size[2] && wp_dups(dropLayout, dropSize, ws->edit, ws->edits, 0, &dm, &dmn) == 1) {
            free(dropLayout);
            dropLayout = dm;
            dropSize = dmn;
        }
        if (wp_dups(part[2], size[2], ws->edit, ws->edits, composed, &dm, &dmn) == 1) {
            free(own[2]);
            own[2] = dm;
            part[2] = dm;
            size[2] = dmn;
        }
    }
    if (ws->edits > 0 && size[2] > 0) { /* the pieces' edits */
        static WpPieces ps;

        if (own[2] == NULL && (own[2] = malloc(size[2])) != NULL) {
            memcpy(own[2], part[2], size[2]);
            part[2] = own[2];
        }
        const uint8_t *layout = dropLayout != NULL && dropSize == size[2] ? dropLayout : own[2];

        if (own[2] != NULL && wp_pieces((uint8_t *)layout, size[2], &ps, composed ? wp_lastFrom : NULL, composed ? wp_lastFroms : 0) > 0) {
            uint8_t *before = NULL, *pm;
            uint32_t pn;
            int e, reach = 0;

            for (e = 0; e < ws->edits; e++) { /* (the holes patched: the workshop's button) */
                reach = ws->edit[e * WP_EDIT_WORDS] == WP_PATCH ? (ws->edit[e * WP_EDIT_WORDS + 3] > 0 ? ws->edit[e * WP_EDIT_WORDS + 3] : 100) : reach;
            }
            if (reach > 0 && (before = malloc(size[2])) != NULL) {
                memcpy(before, own[2], size[2]);
            }
            wp_apply_edits(own[2], layout, &ps, ws->edit, ws->edits);
            if (before != NULL) { /* (first the holes' sides stretched onto each other; what is still open, bridged) */
                wp_morphOn = 1;
                wp_patch(own[2], size[2], before, composed ? wp_lastFrom : NULL, composed ? wp_lastFroms : 0, 0, (float)reach / 100.0f, &pm, &pn);
                wp_morphOn = 0;
            }
            if (before != NULL && wp_patch(own[2], size[2], before, composed ? wp_lastFrom : NULL, composed ? wp_lastFroms : 0, wp_face_key(own[2], part[11], size[11]),
                                               (float)reach / 100.0f, &pm, &pn) > 0) {
                free(own[2]);
                own[2] = pm;
                part[2] = pm;
                size[2] = pn;
            }
            free(before);
        }
        wp_pieces_free(&ps);
    }
    { /* parts with colours of their own: their textures copied */
        int groups[WP_SPLIT_GROUPS], ng = wp_colour_groups(ws->colourPart, ws->colours, groups);
        uint8_t *sm, *st;
        uint32_t stn;

        if (ng > 0 && wp_split(part[2], size[2], part[11], size[11], groups, ng, &sm, &st, &stn) == 1) {
            free(own[2]);
            free(own[11]);
            own[2] = sm;
            own[11] = st;
            part[2] = sm;
            part[11] = st;
            size[11] = stn;
        }
    }
    if (ws->colours > 0 && size[11] > 0) { /* the body recoloured, its shading ramps with it */
        if (own[11] == NULL && (own[11] = malloc(size[11])) != NULL) {
            memcpy(own[11], part[11], size[11]);
        }
        if (own[11] != NULL) {
            uint8_t *orig = malloc(size[11]);

            if (orig != NULL) {
                memcpy(orig, own[11], size[11]);
            }
            warrior_recolour(own[11], size[11], part[2], size[2], ws->colourKey, ws->colourRgb, ws->colourPart, ws->colours);
            part[11] = own[11];
            for (i = 12; orig != NULL && i <= 16; i++) { /* the face's expressions (eyes, mouths) the same way */
                if (size[i] < 0x20) {
                    continue;
                }
                if (own[i] == NULL && (own[i] = malloc(size[i])) != NULL) {
                    memcpy(own[i], part[i], size[i]);
                }
                if (own[i] != NULL) {
                    warrior_recolour_follow(own[i], size[i], orig, own[11], size[11]);
                    part[i] = own[i];
                }
            }
            free(orig);
        }
    }
    if (shapeRef && size[2] > 0x90) { /* the mouth's shapes onto the face as it is now (a head on another body, sized,
                                         moved: the expressions jumped back to where the face's fighter had it) */
        float lo[3], hi[3];

        if (warrior_face_box(part[2], size[2], lo, hi) &&
            (fabsf(lo[0] - shapeLo[0]) + fabsf(lo[1] - shapeLo[1]) + fabsf(lo[2] - shapeLo[2]) + fabsf(hi[0] - shapeHi[0]) +
             fabsf(hi[1] - shapeHi[1]) + fabsf(hi[2] - shapeHi[2])) > 1e-3f) {
            for (i = 3; i <= 10; i++) {
                if (size[i] < 0x60) {
                    continue;
                }
                if (own[i] == NULL && (own[i] = malloc(size[i])) != NULL) {
                    memcpy(own[i], part[i], size[i]);
                }
                if (own[i] != NULL) {
                    warrior_shapes_fit(own[i], size[i], shapeLo, shapeHi, lo, hi);
                    part[i] = own[i];
                }
            }
        }
    }
    if (size[2] > 0x10 && (ws->dynShade == 1 || ws->dynShade == 2 || ws->shadeFrom >= 0)) { /* the Super Saiyan lighting: the model's
                                                                                                  flag (header byte 0x0F bit 0) */
        int on = ws->dynShade == 1;

        if (ws->dynShade == 0) { /* (as the shading fighter's model has it) */
            WarriorPak sp;
            uint32_t sn;
            const uint8_t *sm;

            memset(&sp, 0, sizeof(sp));
            if (warrior_open(&sp, MODEL_FILE + ws->shadeFrom * 10) && sp.n == WARRIOR_PARTS && (sm = warrior_part(&sp, 2, &sn)) != NULL && sn > 0x10) {
                on = sm[0x0F] & 1;
            } else {
                on = part[2][0x0F] & 1;
            }
            free(sp.d);
        }
        if ((part[2][0x0F] & 1) != on) {
            if (own[2] == NULL && (own[2] = malloc(size[2])) != NULL) {
                memcpy(own[2], part[2], size[2]);
            }
            if (own[2] != NULL) {
                own[2][0x0F] = (uint8_t)((own[2][0x0F] & ~1) | on);
                part[2] = own[2];
            }
        }
    }
    if (size[11] > 0) { /* drawn on and pictures put on its textures (paint/ beside the model file), then the shading made
                           for it from its pictures' colours as they are now */
        char pdir[700];
        const char *slash = strrchr(path, '/');

        if (own[11] == NULL && (own[11] = malloc(size[11])) != NULL) {
            memcpy(own[11], part[11], size[11]);
        }
        if (own[11] != NULL) {
            snprintf(pdir, sizeof(pdir), "%.*s/paint", slash != NULL ? (int)(slash - path) : 0, path);
            warrior_paint_apply(own[11], size[11], pdir);
            wp_shade_regen(own[11], size[11]);

            part[11] = own[11];
            for (i = 12; i <= 16; i++) { /* (the face's expressions: the eyes shut as it blinks, the hurt face) */
                if (size[i] < 0x20) {
                    continue;
                }
                if (own[i] == NULL && (own[i] = malloc(size[i])) != NULL) {
                    memcpy(own[i], part[i], size[i]);
                }
                if (own[i] != NULL) {
                    warrior_paint_apply(own[i], size[i], pdir);
                    part[i] = own[i];
                }
            }
        }
    }
    if (dn->stance.d != NULL) { /* the menus' pose (model part 35: the model's own motion 7), the stance fighter's, scaled to the body */
        uint32_t ps;
        const uint8_t *pose = warrior_part(&dn->stance, 35, &ps);
        float sc = warrior_scale(warrior_hip(&src[0]), warrior_hip(&dn->stance));

        if (ps >= 0x94) {
            free(own[35]);
            own[35] = malloc(ps);
            if (own[35] != NULL) {
                memcpy(own[35], pose, ps);
                warrior_scale_motion(own[35], ps, sc);
                part[35] = own[35];
                size[35] = ps;
            }
        }
    } else if (size[35] >= 0x94 && ws->moves != ws->body) { /* the menus' pose is the moves fighter's (src[1]): scaled to the
                                                                 body too, a taller or smaller body floated or sank in it */
        float sc = warrior_scale(warrior_hip(&src[0]), warrior_hip(&src[1]));

        if (sc < 0.999f || sc > 1.001f) {
            uint8_t *mp35 = malloc(size[35]);

            if (mp35 != NULL) {
                memcpy(mp35, part[35], size[35]);
                warrior_scale_motion(mp35, size[35], sc);
                free(own[35]);
                own[35] = mp35;
                part[35] = mp35;
            }
        }
    }
    if (pose_any(ws->pose) && size[35] >= 0x94) { /* the stance maker's pose on the menus' pose */
        uint32_t ns;
        uint8_t *np = pose_apply(part[35], size[35], ws->pose, &ns);

        if (np != NULL) {
            free(own[35]);
            own[35] = np;
            part[35] = np;
            size[35] = ns;
        }
    }
    if (0 && ws->bodyScale >= 50 && ws->bodyScale <= 250 && ws->bodyScale != 100 && size[2] > 0x60) { /* the whole warrior's size
                                                                       (off: the motions still move the bones their own lengths) */
        float f = (float)ws->bodyScale / 100.0f;
        static WpRec mr[WP_MAX_RECS * 2];
        int mn, q2;

        if (own[2] == NULL && (own[2] = malloc(size[2])) != NULL) {
            memcpy(own[2], part[2], size[2]);
            part[2] = own[2];
        }
        mn = own[2] != NULL ? wp_records(own[2], size[2], mr, WP_MAX_RECS * 2) : 0;
        for (i = 0; i < (uint32_t)mn; i++) { /* (every node and point about the feet's origin) */
            uint8_t *r = own[2] + mr[i].off;
            WpScaleCtx sc;

            for (q2 = 0; q2 < 3; q2++) {
                ((float *)(r + 0x10))[q2] *= f;
                ((float *)(r + 0x20))[q2] *= f;
                ((float *)(r + 0x30))[q2] *= f;
            }
            if (mr[i].size > 0x40) {
                for (q2 = 0; q2 < 3; q2++) {
                    ((float *)(r + 0x40))[q2] *= f;
                }
                for (q2 = 0; q2 < 4; q2++) {
                    ((float *)(r + 0x50))[q2] *= f;
                }
                sc.pivot[0] = sc.pivot[1] = sc.pivot[2] = 0.0f;
                sc.s = f;
                sc.full = 1;
                wp_batches(own[2], &mr[i], wp_scale_batch, &sc);
            }
        }
        if (size[17] >= 12) { /* (its height and radius in the fight: the camera, the hits) */
            float hr[2];

            if (own[17] == NULL && (own[17] = malloc(size[17])) != NULL) {
                memcpy(own[17], part[17], size[17]);
                part[17] = own[17];
            }
            if (own[17] != NULL) {
                memcpy(hr, own[17] + 4, 8);
                hr[0] *= f;
                hr[1] *= f;
                memcpy(own[17] + 4, hr, 8);
            }
        }
        if (size[35] >= 0x94) { /* (the menus' pose) */
            if (own[35] == NULL && (own[35] = malloc(size[35])) != NULL) {
                memcpy(own[35], part[35], size[35]);
                part[35] = own[35];
            }
            if (own[35] != NULL) {
                warrior_scale_motion(own[35], size[35], f);
            }
        }
    }
    { /* part 25 (empty on the disc), read by src/battle/bobj_a.c (PortWarrior_*): "BT3WARRI", the scale of the moves
         fighter's motions, a count, then (first motion, count, scale) for the motions taken from other fighters; a
         scale is how much taller the body is than the fighter the motions were made for (the game scales their
         translations by it), negative when that fighter has no tail and the body has one: the body's own tail
         motion, which follows ("TAIL", warrior_tail_clip), is put into those motions */
        uint8_t *mark;
        float hb = warrior_hip(&src[0]);
        int tail = warrior_has_tail(&src[0]), tailWho = -1;
        uint32_t ranges = 0, c, clipSize = 0;
        uint8_t *clip;

        for (j = 0; j < 4 && !tail; j++) { /* (a Tail layer: the warrior has that fighter's tail, and its tail motion) */
            if (ws->xlayer[j] >= 0 && ws->xslot[j] == WP_SLOTS) {
                WarriorPak tp;

                memset(&tp, 0, sizeof(tp));
                if (warrior_open(&tp, MODEL_FILE + ws->xlayer[j] * 10) && tp.n == WARRIOR_PARTS && warrior_has_tail(&tp)) {
                    tailWho = ws->xlayer[j];
                }
                free(tp.d);
                if (tailWho >= 0) {
                    break;
                }
            }
        }
        clip = tail ? warrior_tail_clip(ws->body, &clipSize) : tailWho >= 0 ? warrior_tail_clip(tailWho, &clipSize) : NULL;
        WarriorRange r[32];

        for (j = 0; j < 9; j++) { /* (each kind of motion from another fighter; first: a later range wins) */
            for (c = 0; dn->anim[j].d != NULL && c < 4 && kWarriorAnimCat[j][c][1] > 0; c++) {
                ranges = warrior_range(r, ranges, kWarriorAnimCat[j][c][0], kWarriorAnimCat[j][c][1], hb, clip != NULL, &dn->anim[j]);
            }
        }
        for (j = 0; j < 2; j++) {
            if (dn->skill[j].d != NULL) {
                ranges = warrior_range(r, ranges, 0x103 + j, 1, hb, clip != NULL, &dn->skill[j]);
            }
        }
        if (dn->fin.d != NULL) {
            for (c = 0; c < sizeof(kWarriorFinisherAnim) / sizeof(kWarriorFinisherAnim[0]); c++) {
                ranges = warrior_range(r, ranges, kWarriorFinisherAnim[c][0], kWarriorFinisherAnim[c][1], hb, clip != NULL, &dn->fin);
            }
        }
        for (j = 0; j < 3; j++) {
            if (dn->tech[j].d != NULL) {
                ranges = warrior_range(r, ranges, WARRIOR_TECH_ANIM(j), 0x22, hb, clip != NULL, &dn->tech[j]);
                if (j == 2 && ws->techFrom[j] == 2) {
                    ranges = warrior_range(r, ranges, WARRIOR_ULT_POSE, 1, hb, clip != NULL, &dn->tech[j]);
                }
            }
        }
        for (c = 0; dn->stance.d != NULL && c < sizeof(kWarriorStance) / sizeof(kWarriorStance[0]); c++) {
            ranges = warrior_range(r, ranges, kWarriorStance[c][0], kWarriorStance[c][1], hb, clip != NULL, &dn->stance);
        }
        size[25] = 16 + ranges * 8 + clipSize;
        if ((mark = calloc(1, size[25])) != NULL) {
            float scale = warrior_scale(hb, warrior_hip(&src[1])) * (clip != NULL && !warrior_has_tail(&src[1]) ? -1.0f : 1.0f);

            if (0 && ws->bodyScale >= 50 && ws->bodyScale <= 250 && ws->bodyScale != 100) { /* (the whole warrior's size: every motion's
                                                                                           steps as much bigger) */
                scale *= (float)ws->bodyScale / 100.0f;
                for (c = 0; c < ranges; c++) {
                    r[c].scale *= (float)ws->bodyScale / 100.0f;
                }
            }

            memcpy(mark, "BT3WARRI", 8);
            memcpy(mark + 8, &scale, 4);
            memcpy(mark + 12, &ranges, 4);
            for (c = 0; c < ranges; c++) {
                memcpy(mark + 16 + c * 8, &r[c].first, 2);
                memcpy(mark + 18 + c * 8, &r[c].count, 2);
                memcpy(mark + 20 + c * 8, &r[c].scale, 4);
            }
            if (clip != NULL) {
                memcpy(mark + 16 + ranges * 8, clip, clipSize);
            }
            own[25] = mark;
            part[25] = mark;
        } else {
            size[25] = 0;
        }
        free(clip);
    }
    if (exprFaceOk) {
        warrior_expr_follow(part, size, own, exprFace);
    }
    ok = warrior_write_pak(path, part, size, WARRIOR_PARTS) ? 1 : -1;
done:
    for (j = 0; j < 3; j++) {
        free(src[j].d);
    }
    free(dropLayout);
    for (i = 0; i < WARRIOR_PARTS; i++) {
        free(own[i]);
    }
    return ok;
}

/* The total size of a pak of these parts as warrior_write_pak writes it. */
static uint32_t warrior_pak_size(const uint32_t *size, uint32_t n) {
    uint32_t total = (4 + 4 * (n + 1) + 0x3F) & ~0x3Fu, i;

    for (i = 0; i < n; i++) {
        total += (size[i] + 0x3F) & ~0x3Fu;
    }
    return total;
}

/* The animation file: the moves fighter's, with the Blast 1 skills' motions (0x103 + slot) and the finishers' from
   their donors. 1 made, 0 failed, -1 too large for the game's buffer. */
static uint8_t *bpe_pack(const uint8_t *data, uint32_t len, uint32_t *outLen);
static int warrior_anims(const WarriorSpec *ws, const char *path) {
    WarriorPak base, skill[2], fin, tech[3];
    const uint8_t **part = NULL;
    uint32_t *size = NULL, i, c;
    int j, ok = 0, posed = 0;
    uint8_t *posedBuf[8];

    memset(&base, 0, sizeof(base));
    memset(skill, 0, sizeof(skill));
    memset(&fin, 0, sizeof(fin));
    memset(tech, 0, sizeof(tech));
    if (!warrior_open(&base, MODEL_FILE + ws->moves * 10 + 8) || base.n < 0x19E || (part = malloc(base.n * sizeof(*part))) == NULL ||
        (size = malloc(base.n * sizeof(*size))) == NULL) {
        goto done;
    }
    for (i = 0; i < base.n; i++) {
        part[i] = warrior_part(&base, i, &size[i]);
    }
    for (j = 0; j < 9; j++) { /* each kind of motion from another fighter (the finishers, stance and techniques
                                 below win where they overlap) */
        static WarriorPak an[9];

        free(an[j].d);
        memset(&an[j], 0, sizeof(an[j]));
        if (ws->anim[j] < 0) {
            continue;
        }
        if (!warrior_open(&an[j], MODEL_FILE + ws->anim[j] * 10 + 8) || an[j].n != base.n) {
            goto done;
        }
        for (c = 0; c < 4 && kWarriorAnimCat[j][c][1] > 0; c++) {
            for (i = kWarriorAnimCat[j][c][0]; i < (uint32_t)(kWarriorAnimCat[j][c][0] + kWarriorAnimCat[j][c][1]) && i < base.n; i++) {
                part[i] = warrior_part(&an[j], i, &size[i]);
            }
        }
    }
    for (j = 0; j < 2; j++) {
        if (ws->skill[j] < 0) {
            continue;
        }
        if (!warrior_open(&skill[j], MODEL_FILE + ws->skill[j] * 10 + 8) || skill[j].n != base.n) {
            goto done;
        }
        part[0x103 + j] = warrior_part(&skill[j], 0x103 + (uint32_t)ws->skillFrom[j], &size[0x103 + j]);
    }
    if (ws->finishers >= 0) {
        if (!warrior_open(&fin, MODEL_FILE + ws->finishers * 10 + 8) || fin.n != base.n) {
            goto done;
        }
        for (c = 0; c < sizeof(kWarriorFinisherAnim) / sizeof(kWarriorFinisherAnim[0]); c++) {
            for (i = kWarriorFinisherAnim[c][0]; i < (uint32_t)(kWarriorFinisherAnim[c][0] + kWarriorFinisherAnim[c][1]); i++) {
                part[i] = warrior_part(&fin, i, &size[i]);
            }
        }
    }
    if (ws->stance >= 0) { /* the standing motions */
        static WarriorPak st;

        free(st.d);
        memset(&st, 0, sizeof(st));
        if (!warrior_open(&st, MODEL_FILE + ws->stance * 10 + 8) || st.n != base.n) {
            goto done;
        }
        for (c = 0; c < sizeof(kWarriorStance) / sizeof(kWarriorStance[0]); c++) {
            for (i = kWarriorStance[c][0]; i < (uint32_t)(kWarriorStance[c][0] + kWarriorStance[c][1]); i++) {
                part[i] = warrior_part(&st, i, &size[i]);
            }
        }
    }
    for (j = 0; j < 3; j++) { /* a technique's 0x22 motions */
        if (ws->tech[j] < 0) {
            continue;
        }
        if (!warrior_open(&tech[j], MODEL_FILE + ws->tech[j] * 10 + 8) || tech[j].n != base.n) {
            goto done;
        }
        for (i = 0; i < 0x22; i++) {
            part[WARRIOR_TECH_ANIM(j) + i] = warrior_part(&tech[j], WARRIOR_TECH_ANIM(ws->techFrom[j]) + i, &size[WARRIOR_TECH_ANIM(j) + i]);
        }
        if (j == 2 && ws->techFrom[j] == 2) { /* the Ultimate's opening pose (action 0x105) */
            part[WARRIOR_ULT_POSE] = warrior_part(&tech[j], WARRIOR_ULT_POSE, &size[WARRIOR_ULT_POSE]);
        }
    }
    if (pose_any(ws->pose)) { /* the stance maker's pose on the standing motions (packed again): on the menus' pose (7) as made
                                 in the stance maker, on the others where it has the bones (pose_apply_ref) */
        uint32_t refRaw = 0;
        uint8_t *ref = NULL;
        WarriorPak mp;

        memset(&mp, 0, sizeof(mp));
        if (warrior_open(&mp, MODEL_FILE + (ws->stance >= 0 ? ws->stance : ws->moves >= 0 ? ws->moves : ws->body) * 10) && mp.n == WARRIOR_PARTS) {
            /* (the stand loop of whoever's standing motions the warrior has -- the stance fighter's, else the moves fighter's --
               as the fight scales those to the body: the body's own was scaled as if it were the moves fighter's) */
            const uint8_t *r35 = warrior_part(&mp, 35, &refRaw); /* (the menus' pose the stance maker shows: uncompressed) */

            if (r35 != NULL && refRaw >= 0x94 && (ref = malloc(refRaw)) != NULL) {
                memcpy(ref, r35, refRaw);
            }
        }
        free(mp.d);
        { /* the battle stances, on the ground (0) and in the air (0x26): the stance maker's stand loop itself, posed -- the
             whole body as it was made there, its breathing with it */
            uint32_t pr = 0, pn = 0;
            uint8_t *posedLoop = ref != NULL ? pose_apply(ref, refRaw, ws->pose, &pr) : NULL;
            uint8_t *pk = posedLoop != NULL ? bpe_pack(posedLoop, pr, &pn) : NULL;

            free(posedLoop);
            if (pk != NULL && posed < 8) {
                posedBuf[posed++] = pk;
                part[0] = pk;
                size[0] = pn;
                if (base.n > 0x26) {
                    part[0x26] = pk;
                    size[0x26] = pn;
                }
            } else {
                free(pk);
            }
        }

        for (c = 0; c < sizeof(kWarriorStance) / sizeof(kWarriorStance[0]); c++) {
            for (i = kWarriorStance[c][0]; i < (uint32_t)(kWarriorStance[c][0] + kWarriorStance[c][1]) && i < base.n; i++) {
                uint32_t raw = 0, ns = 0, pn = 0;
                uint8_t *an, *np = NULL, *pk = NULL;

                if (ref != NULL && (i == 0 || i == 0x26)) {
                    continue; /* (made above: the posed stand loop) */
                }
                an = size[i] > 0 ? bpe_decode(part[i], size[i], &raw) : NULL;

                if (an != NULL && raw >= 0x94 &&
                    (np = ref == NULL || refRaw < 0x94 ? pose_apply(an, raw, ws->pose, &ns) : pose_apply_ref(an, raw, ws->pose, ref, refRaw, &ns)) != NULL &&
                    (pk = bpe_pack(np, ns, &pn)) != NULL &&
                    posed < 8) {
                    posedBuf[posed++] = pk;
                    part[i] = pk;
                    size[i] = pn;
                    pk = NULL;
                }
                free(an);
                free(np);
                free(pk);
            }
        }
        free(ref);
    }
    if (warrior_pak_size(size, base.n) > WARRIOR_ANIM_ROOM) {
        ok = -1;
        goto done;
    }
    ok = warrior_write_pak(path, part, size, base.n);
done:
    while (posed > 0) {
        free(posedBuf[--posed]);
    }
    free(part);
    free(size);
    free(base.d);
    free(skill[0].d);
    free(skill[1].d);
    free(fin.d);
    for (j = 0; j < 3; j++) {
        free(tech[j].d);
    }
    return ok;
}

/* The effect file: the effects fighter's, with the Blast 1 skills' effect packs (parts 0 / 1) from their donors.
   1 made, 0 failed, -1 too large for the game's buffer. */
static int warrior_effects(const WarriorSpec *spec, const char *path) {
    WarriorPak base, skill[2], tech[3];
    static WarriorPak camPak[4]; /* (the cameras' effect files: freed when made again) */
    WarriorSpec own = *spec;
    const WarriorSpec *ws = &own;
    const uint8_t *part[32];
    uint8_t *cams = NULL, *common = NULL; /* (the common pack made again) */
    uint8_t *tinted[6] = {NULL, NULL, NULL, NULL, NULL, NULL}; /* (the techniques' effects, the common pack, in colours of their own) */
    uint32_t size[32], i;
    int j, ok = 0;

    memset(&base, 0, sizeof(base));
    memset(skill, 0, sizeof(skill));
    memset(tech, 0, sizeof(tech));
    if (own.effects != own.moves) { /* the moves fighter's own techniques keep its effect packs and camera work: another
                                       fighter's pack in their slot is another technique's (a camera it lacks crashed) */
        for (j = 0; j < 2; j++) {
            if (own.skill[j] < 0) {
                own.skill[j] = own.moves;
                own.skillFrom[j] = j;
            }
        }
        for (j = 0; j < 3; j++) {
            if (own.tech[j] < 0) {
                own.tech[j] = own.moves;
                own.techFrom[j] = j;
            }
        }
    }
    if (!warrior_open(&base, MODEL_FILE + ws->effects * 10 + 9) || base.n < 5 || base.n > 32) {
        goto done;
    }
    for (i = 0; i < base.n; i++) {
        part[i] = warrior_part(&base, i, &size[i]);
    }
    for (j = 0; j < 2; j++) {
        if (ws->skill[j] < 0) {
            continue;
        }
        if (!warrior_open(&skill[j], MODEL_FILE + ws->skill[j] * 10 + 9) || skill[j].n < 2) {
            goto done;
        }
        part[j] = warrior_part(&skill[j], (uint32_t)ws->skillFrom[j], &size[j]);
    }
    for (j = 0; j < 3; j++) { /* a technique's effect pack (with its camera work) */
        if (ws->tech[j] < 0) {
            continue;
        }
        if (!warrior_open(&tech[j], MODEL_FILE + ws->tech[j] * 10 + 9) || tech[j].n < 5) {
            goto done;
        }
        part[2 + j] = warrior_part(&tech[j], 2 + (uint32_t)ws->techFrom[j], &size[2 + j]);
    }
    for (j = 0; j < 2; j++) { /* a Blast 1 skill's look from another's (its effect pack) */
        static WarriorPak sx[2];

        free(sx[j].d);
        memset(&sx[j], 0, sizeof(sx[j]));
        if (spec->skillFx[j] < 0) {
            continue;
        }
        if (!warrior_open(&sx[j], MODEL_FILE + spec->skillFx[j] * 10 + 9) || sx[j].n < 2) {
            goto done;
        }
        part[j] = warrior_part(&sx[j], (uint32_t)spec->skillFxFrom[j], &size[j]);
    }
    for (j = 0; j < 3; j++) { /* a technique's effects from another (the effect pack; its camera stays the technique's) */
        static WarriorPak fx[3];

        free(fx[j].d);
        memset(&fx[j], 0, sizeof(fx[j]));
        if (spec->techFx[j] < 0) {
            continue;
        }
        if (!warrior_open(&fx[j], MODEL_FILE + spec->techFx[j] * 10 + 9) || fx[j].n < 5) {
            goto done;
        }
        part[2 + j] = warrior_part(&fx[j], 2 + (uint32_t)spec->techFxFrom[j], &size[2 + j]);
    }
    for (j = 0; j < 4; j++) { /* the cameras of their own: the effect file of whose they are */
        free(camPak[j].d);
        memset(&camPak[j], 0, sizeof(camPak[j]));
        if (ws->camFrom[j] >= 0 && j == 3 && !sTechRush[2]) { /* (only the Ultimate's loop: a rush or a beam keeps its own
                                                                    cinematic; a rush Ultimate keeps its loop too) */
            warrior_open(&camPak[j], MODEL_FILE + ws->camFrom[j] * 10 + 9);
        }
    }
    if ((ws->tech[0] >= 0 || ws->tech[1] >= 0 || ws->tech[2] >= 0 || camPak[3].d != NULL) &&
        base.n >= 6) {
        /* part 5, the fighter's common pack: its sub-part 7 holds a camera pack per technique (entry t for
           slots 2..4, read by the technique effects of src/battle/eft_k.c), which go with the techniques */
        const uint8_t *sub[16], *cam[4];
        uint32_t subSize[16], camSize[4], n5 = 0, c;

        memcpy(&n5, part[5], size[5] >= 4 ? 4 : 0);
        if (n5 > 7 && n5 <= 16) {
            for (c = 0; c < n5; c++) {
                sub[c] = warrior_sub(part[5], size[5], c, &subSize[c]);
            }
            for (c = 0; c < 4; c++) {
                cam[c] = sub[7] != NULL ? warrior_sub(sub[7], subSize[7], c, &camSize[c]) : NULL;
            }
            for (j = 0; j < 3; j++) {
                uint32_t s5, s7;
                const uint8_t *p5, *p7, *got;

                if (ws->tech[j] < 0 || tech[j].n < 6) {
                    continue;
                }
                p5 = warrior_part(&tech[j], 5, &s5);
                p7 = warrior_sub(p5, s5, 7, &s7);
                got = p7 != NULL ? warrior_sub(p7, s7, (uint32_t)ws->techFrom[j], &c) : NULL;
                cam[j] = got; /* (entry t: technique t's, as src/battle/eft_k.c reads it -- an empty one is no camera) */
                camSize[j] = got != NULL ? c : 0;
            }
            for (j = 3; j < 4; j++) { /* (the Ultimate's loop camera from another technique) */
                uint32_t s5, s7, to = j == 3 ? 2 : (uint32_t)j;
                const uint8_t *p5, *p7, *got;

                if (camPak[j].d == NULL || camPak[j].n < 6 || ws->camSlot[j] < 0 || ws->camSlot[j] > 2) {
                    continue;
                }
                p5 = warrior_part(&camPak[j], 5, &s5);
                p7 = p5 != NULL ? warrior_sub(p5, s5, 7, &s7) : NULL;
                got = p7 != NULL ? warrior_sub(p7, s7, (uint32_t)ws->camSlot[j], &c) : NULL;
                cam[to] = got;
                camSize[to] = got != NULL ? c : 0;
            }
            for (c = 0; c < 4; c++) {
                if (cam[c] == NULL) {
                    cam[c] = part[5];
                    camSize[c] = 0;
                }
            }
            for (c = 0; c < n5; c++) {
                if (sub[c] == NULL) {
                    sub[c] = part[5];
                    subSize[c] = 0;
                }
            }
            if ((cams = warrior_make_pak(cam, camSize, 4, &c)) != NULL) {
                sub[7] = cams;
                subSize[7] = c;
                if ((common = warrior_make_pak(sub, subSize, n5, &c)) != NULL) {
                    part[5] = common;
                    size[5] = c;
                }
            }
        }
    }
    if (spec->auraColour > 0 && size[5] > 8 && (tinted[3] = malloc(size[5])) != NULL) { /* the common effects in the aura's colour */
        static const uint8_t kSeen[11][3] = {{63, 63, 255}, {255, 63, 255}, {142, 142, 25}, {255, 63, 63}, {63, 255, 63}, {63, 100, 255},
                                             {255, 63, 63}, {128, 63, 255}, {142, 255, 25}, {142, 255, 25}, {142, 255, 25}}; /* (the aura
                                             kinds' light: bobj_a.c BtlObj_SetColorPreset's first table) */
        WarriorPak fm;
        uint32_t pn;
        const uint8_t *prm;
        float sv, vv;

        memset(&fm, 0, sizeof(fm));
        sTintSeen = -1.0f;
        if (warrior_open(&fm, MODEL_FILE + (spec->auraFrom >= 0 ? spec->auraFrom : spec->moves) * 10) && fm.n == WARRIOR_PARTS &&
            (prm = warrior_part(&fm, 17, &pn)) != NULL && pn >= 4 &&
            prm[3] < 11) { /* (the aura kind the warrior fights with - the aura fighter's, else the moves fighter's: the light its
                              charge is seen in) */
            warrior_fx_hsv(kSeen[prm[3]][0] / 255.0f, kSeen[prm[3]][1] / 255.0f, kSeen[prm[3]][2] / 255.0f, &sTintSeen, &sv, &vv);
        }
        free(fm.d);
        memcpy(tinted[3], part[5], size[5]);
        warrior_tint_common(tinted[3], size[5], (uint32_t)spec->auraColour & 0xFFFFFF);
        sTintSeen = -1.0f;
        for (i = 0, j = 0; i < size[5]; i++) {
            j += tinted[3][i] != part[5][i];
        }
        fprintf(stderr, "bt3: warrior effects: common pack, aura colour %08x, %d of %u bytes turned\n", (unsigned)spec->auraColour, j, size[5]);
        part[5] = tinted[3];
    }
    for (j = 0; j < 2; j++) { /* the Blast 1 skills' colours of their own (their effect packs, parts 0 and 1) */
        uint32_t q, turned = 0;

        if (spec->skillColour[j] > 0 && size[j] > 8 && (tinted[4 + j] = malloc(size[j])) != NULL) {
            memcpy(tinted[4 + j], part[j], size[j]);
            warrior_tint_fx(tinted[4 + j], size[j], (uint32_t)spec->skillColour[j] & 0xFFFFFF);
            for (q = 0; q < size[j]; q++) {
                turned += tinted[4 + j][q] != part[j][q];
            }
            part[j] = tinted[4 + j];
            fprintf(stderr, "bt3: warrior effects: Blast 1 %d colour %08x, %u of %u bytes turned\n", j, (unsigned)spec->skillColour[j], turned, size[j]);
        }
    }
    for (j = 0; j < 3; j++) { /* the techniques' colours of their own */
        uint32_t q, turned = 0;

        if (spec->techColour[j] > 0 && size[2 + j] > 8 && (tinted[j] = malloc(size[2 + j])) != NULL) {
            memcpy(tinted[j], part[2 + j], size[2 + j]);
            warrior_tint_fx(tinted[j], size[2 + j], (uint32_t)spec->techColour[j] & 0xFFFFFF);
            for (q = 0; q < size[2 + j]; q++) {
                turned += tinted[j][q] != part[2 + j][q];
            }
            part[2 + j] = tinted[j];
        }
        fprintf(stderr, "bt3: warrior effects: technique %d colour %08x, %u of %u bytes turned\n", j, (unsigned)spec->techColour[j], turned, size[2 + j]);
    }
    if (warrior_pak_size(size, base.n) > WARRIOR_EFF_ROOM) {
        ok = -1;
        goto done;
    }
    ok = warrior_write_pak(path, part, size, base.n);
done:
    for (j = 0; j < 6; j++) {
        free(tinted[j]);
    }
    free(cams);
    free(common);
    free(base.d);
    free(skill[0].d);
    free(skill[1].d);
    for (j = 0; j < 3; j++) {
        free(tech[j].d);
    }
    return ok;
}

static void warrior_fail(const char *dir, const char *what) {
    char path[600];
    FILE *fp;

    snprintf(path, sizeof(path), "%s/warrior_error.txt", dir);
    fp = fopen(path, "w");
    if (fp != NULL) {
        fprintf(fp, "%s\n", what);
        fclose(fp);
    }
}

static int warrior_hud_photo(const char *dir);

/* ---- battle damage painted on: each 4-bit texture of a model file (not the face's, not the hair's) gets dirt (its
   colours a shade darker, in patches) and scratches and tears (its darkest colour, in short strokes; battle-torn: more,
   longer, with a lighter edge), made only of colours its palette has (16 a texture), the same every time. ---- */
static uint32_t wd_hash(uint32_t x, uint32_t y, uint32_t s) {
    uint32_t h = x * 374761393u + y * 668265263u + s * 2246822519u;

    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* The texture's colour numbers idx written back into its pixels (as wc_indices reads them). */
static void wd_put_indices(uint8_t *d, uint32_t size, const WpTex *e, const uint8_t *idx, int w, int h) {
    static uint32_t *mem;
    uint8_t *pix;
    uint32_t upw, uph, dbw = e->pixBlt & 0x3F, tbw = (uint32_t)(e->tex0 >> 14) & 0x3F, x, y;

    if (e->pixOfs == 0 || e->pixOfs * 4 + 0x60 > size) {
        return;
    }
    pix = d + e->pixOfs * 4;
    memcpy(&upw, pix + 0x30, 4);
    memcpy(&uph, pix + 0x34, 4);
    if (upw == 0 || uph == 0 || e->pixOfs * 4 + 0x60 + upw * uph * 4 > size) {
        return;
    }
    if (mem == NULL && (mem = calloc(WC_MEM_WORDS, 4)) == NULL) {
        return;
    }
    for (y = 0; y < uph; y++) {
        for (x = 0; x < upw; x++) {
            uint32_t a = wc_addr32(dbw > 0 ? dbw : 1, x, y);

            if (a < WC_MEM_WORDS) {
                memcpy(&mem[a], pix + 0x60 + (y * upw + x) * 4, 4);
            }
        }
    }
    for (y = 0; y < (uint32_t)h; y++) {
        for (x = 0; x < (uint32_t)w; x++) {
            uint32_t a = wc_addr4(tbw, x, y), sh = (a & 7) * 4;

            if ((a >> 3) < WC_MEM_WORDS) {
                mem[a >> 3] = (mem[a >> 3] & ~(15u << sh)) | ((uint32_t)(idx[y * (uint32_t)w + x] & 15) << sh);
            }
        }
    }
    for (y = 0; y < uph; y++) {
        for (x = 0; x < upw; x++) {
            uint32_t a = wc_addr32(dbw > 0 ? dbw : 1, x, y);

            if (a < WC_MEM_WORDS) {
                memcpy(pix + 0x60 + (y * upw + x) * 4, &mem[a], 4);
            }
        }
    }
}

/* The drawings in folder `dir` put on texture file tex: each texture whose picture (as made: its hash, taken before
   anything is put on) has a drawing gets its colour numbers (stretched if its size changed) and, if the drawing has
   them, its colours. Returns how many. */
static int warrior_paint_apply(uint8_t *tex, uint32_t size, const char *dir) {
    static WpTex ent[WP_MAX_TEX];
    uint32_t hash[WP_MAX_TEX];
    int count, t, n = 0;

    if (dir == NULL || !is_dir(dir) || (count = wp_textures(tex, size, ent, WP_MAX_TEX)) <= 0) {
        return 0;
    }
    for (t = 0; t < count; t++) {
        hash[t] = wc_tex_hash(tex, size, &ent[t]);
    }
    for (t = 0; t < count; t++) {
        char path[900];
        long len = 0;
        uint8_t *d;
        uint32_t hd[4];
        int tw, th, x, y;
        uint8_t *ix, *out;

        snprintf(path, sizeof(path), "%s/%06x.bin", dir, (unsigned)(hash[t] & 0xFFFFFF));
        if ((d = read_all(path, &len)) == NULL) {
            continue;
        }
        memcpy(hd, d, sizeof(hd));
        if (len < 16 || hd[0] != 0x3150575Au || hd[1] == 0 || hd[2] == 0 || hd[1] > 1024 || hd[2] > 1024 ||
            (long)(16 + hd[1] * hd[2] + (hd[3] ? 64 : 0)) > len || (ix = wc_indices(tex, size, &ent[t], &tw, &th)) == NULL) {
            free(d);
            continue;
        }
        out = ix;
        for (y = 0; y < th; y++) {
            for (x = 0; x < tw; x++) {
                out[y * tw + x] = d[16 + (y * (int)hd[2] / th) * (int)hd[1] + x * (int)hd[1] / tw] & 15;
            }
        }
        wd_put_indices(tex, size, &ent[t], out, tw, th);
        free(ix);
        if (hd[3] && ent[t].clutOfs != 0 && ent[t].clutOfs * 4 + 0x60 + 64 <= size) {
            memcpy(tex + ent[t].clutOfs * 4 + 0x60, d + 16 + hd[1] * hd[2], 64);
        }
        free(d);
        n++;
    }
    return n;
}

/* One texture worn: level 1 scuffed, 2 battle-torn; seed makes each texture's own. */
static void wd_wear(uint8_t *ix, int w, int h, const uint8_t *pal, int level, uint32_t seed) {
    int lum[16], solid[16], darker[16], lighter[16], darkest = -1, c, j, x, y, n, cell, strokes;

    for (c = 0; c < 16; c++) {
        lum[c] = pal[c * 4] * 30 + pal[c * 4 + 1] * 59 + pal[c * 4 + 2] * 11;
        solid[c] = pal[c * 4 + 3] >= 0x60;
        if (solid[c] && (darkest < 0 || lum[c] < lum[darkest])) {
            darkest = c;
        }
    }
    if (darkest < 0) {
        return;
    }
    for (c = 0; c < 16; c++) { /* each colour's dirtier one (about 60% of it) and its lighter one (a torn edge) */
        int best = -1, bestD = 0, lit = -1, litD = 0;

        darker[c] = lighter[c] = c;
        for (j = 0; j < 16; j++) {
            int dr, dg, db, dd;

            if (!solid[j] || j == c) {
                continue;
            }
            dr = pal[j * 4] - pal[c * 4] * 6 / 10;
            dg = pal[j * 4 + 1] - pal[c * 4 + 1] * 6 / 10;
            db = pal[j * 4 + 2] - pal[c * 4 + 2] * 6 / 10;
            dd = dr * dr + dg * dg + db * db;
            if (lum[j] < lum[c] * 9 / 10 && lum[j] > lum[c] / 4 && (best < 0 || dd < bestD)) {
                best = j;
                bestD = dd;
            }
            dr = pal[j * 4] - (pal[c * 4] + (255 - pal[c * 4]) / 3);
            dg = pal[j * 4 + 1] - (pal[c * 4 + 1] + (255 - pal[c * 4 + 1]) / 3);
            db = pal[j * 4 + 2] - (pal[c * 4 + 2] + (255 - pal[c * 4 + 2]) / 3);
            dd = dr * dr + dg * dg + db * db;
            if (lum[j] > lum[c] && (lit < 0 || dd < litD)) {
                lit = j;
                litD = dd;
            }
        }
        if (best >= 0 && bestD < 90 * 90) {
            darker[c] = best;
        }
        if (lit >= 0 && litD < 110 * 110) {
            lighter[c] = lit;
        }
    }
    cell = (w > h ? w : h) / 6;
    cell = cell < 6 ? 6 : cell;
    for (y = 0; y < h; y++) { /* dirt: smooth patches (value noise), ragged at their edges */
        for (x = 0; x < w; x++) {
            int gx = x / cell, gy = y / cell, fx = (x % cell) * 256 / cell, fy = (y % cell) * 256 / cell;
            int a = (int)(wd_hash(gx, gy, seed) & 255), b = (int)(wd_hash(gx + 1, gy, seed) & 255);
            int cc = (int)(wd_hash(gx, gy + 1, seed) & 255), dd = (int)(wd_hash(gx + 1, gy + 1, seed) & 255);
            int top = a + (b - a) * fx / 256, bot = cc + (dd - cc) * fx / 256, v = top + (bot - top) * fy / 256;
            int k = ix[y * w + x] & 15;

            v += (int)(wd_hash(x, y, seed ^ 0x51ED) & 31) - 16;
            if (solid[k] && k != darkest && v > (level == 1 ? 196 : 168)) {
                ix[y * w + x] = (uint8_t)darker[k];
            }
        }
    }
    strokes = w * h / (level == 1 ? 1400 : 520);
    strokes = strokes < 2 ? 2 : strokes > 220 ? 220 : strokes;
    for (n = 0; n < strokes; n++) { /* scratches and tears: short strokes, mostly slanting */
        uint32_t r = wd_hash((uint32_t)n, 77, seed);
        int x0 = (int)(r % (uint32_t)w), y0 = (int)((r >> 12) % (uint32_t)h);
        int len = level == 1 ? 3 + (int)((r >> 22) & 3) : 4 + (int)((r >> 22) & 7);
        int dir = (int)((r >> 26) & 3), sx = dir == 3 ? 0 : 1, sy = dir == 0 ? 1 : dir == 1 ? -1 : dir == 2 ? 0 : 1, step;

        for (step = 0; step < len; step++) {
            int px = x0 + sx * step + (int)((wd_hash((uint32_t)n, (uint32_t)step, seed) & 3) == 0), py = y0 + sy * step;
            int k;

            if (px < 0 || py < 0 || px >= w || py >= h) {
                break;
            }
            k = ix[py * w + px] & 15;
            if (!solid[k]) {
                break; /* (not into what is see-through) */
            }
            ix[py * w + px] = (uint8_t)darkest;
            if (level == 2 && py + 1 < h && solid[ix[(py + 1) * w + px] & 15] && (ix[(py + 1) * w + px] & 15) != (uint8_t)darkest) {
                ix[(py + 1) * w + px] = (uint8_t)lighter[ix[(py + 1) * w + px] & 15];
            }
        }
    }
}

/* The model file `path` worn (level 1 scuffed, 2 battle-torn): its textures painted, written again. */
static void warrior_damage_file(const char *path, int level, int which) {
    static WpTex ent[WP_MAX_TEX];
    static WpRec recs[WP_MAX_RECS * 2];
    static uint32_t hairKey[32];
    uint32_t done[WP_MAX_TEX], sz2 = 0, sz11 = 0;
    long len = 0;
    uint8_t *d = (uint8_t *)read_all(path, &len);
    WarriorPak pk;
    const uint8_t *mdl, *tex;
    int count, texCount, t, nrec, hairs = 0, worn = 0, ndone = 0, q;
    FILE *fp;

    if (d == NULL || len < 8) {
        free(d);
        return;
    }
    pk.d = d;
    pk.len = len;
    memcpy(&pk.n, d, 4);
    if (pk.n < 12) {
        free(d);
        return;
    }
    mdl = warrior_part(&pk, 2, &sz2);
    tex = warrior_part(&pk, 11, &sz11);
    if (mdl == NULL || tex == NULL || sz2 < 0x60 || sz11 < 0x20) {
        free(d);
        return;
    }
    count = wp_textures(tex, sz11, ent, WP_MAX_TEX);
    texCount = mdl[0x4F] <= count ? mdl[0x4F] : count;
    nrec = wp_records(mdl, sz2, recs, WP_MAX_RECS * 2);
    hairs = nrec > 0 ? wp_hair_textures(mdl, recs, nrec, hairKey, 32) : 0;
    for (t = 0; t < texCount; t++) {
        int tw, th;
        uint8_t *ix;

        if (t == mdl[0x4C] || wp_is_hair(hairKey, hairs, ent[t].tbpOfs | (ent[t].cbpOfs << 16)) || ent[t].clutOfs == 0 ||
            ent[t].clutOfs * 4 + 0x60 + 64 > sz11) {
            continue; /* (the face, the hair, no palette) */
        }
        for (q = 0; q < ndone && done[q] != ent[t].pixOfs; q++) {
        }
        if (q < ndone) {
            continue; /* (a picture drawn twice: worn once) */
        }
        if ((ix = wc_indices(tex, sz11, &ent[t], &tw, &th)) == NULL) {
            continue;
        }
        if (tw >= 16 && th >= 16) {
            wd_wear(ix, tw, th, tex + ent[t].clutOfs * 4 + 0x60, level, wc_tex_hash(tex, sz11, &ent[t]) ^ (uint32_t)which * 0x9E37u);
            wd_put_indices((uint8_t *)tex, sz11, &ent[t], ix, tw, th);
            worn++;
        }
        free(ix);
        done[ndone++] = ent[t].pixOfs;
    }
    if (worn > 0 && (fp = fopen(path, "wb")) != NULL) {
        fwrite(d, 1, (size_t)len, fp);
        fclose(fp);
    }
    fprintf(stderr, "bt3: warrior damage: %d textures %s in %s\n", worn, level == 1 ? "scuffed" : "torn", path);
    free(d);
}

/* Makes the files of the warrior in mods/<name> when its warrior.txt is new or changed. */
static void warrior_build(const char *dir, const char *name) {
    static const char *const kModel[8] = {"1p", "2p", "3p", "4p", "1p_dmg", "2p_dmg", "3p_dmg", "4p_dmg"};
    char path[600], built[600], line[256], file[700], made[16][96];
    char white[40] = "", yellow[40] = "";
    WarriorSpec ws;
    WarriorDonors dn;
    int k, side, count = 0, first, unknown = 0;
    struct stat a, b;
    FILE *fp;
    Mod tmp;

    snprintf(path, sizeof(path), "%s/warrior.txt", dir);
    snprintf(built, sizeof(built), "%s/.warrior_built", dir);
    if (stat(path, &a) != 0) {
        return;
    }
    snprintf(sModRefBuilding, sizeof(sModRefBuilding), "%s", name); /* (mod fighters read again: they may have changed) */
    mod_ref_forget();
    first = stat(built, &b) != 0;
    if (!first && b.st_mtime >= a.st_mtime && (fp = fopen(built, "r")) != NULL) {
        int current = fgets(line, sizeof(line), fp) != NULL && strncmp(line, WARRIOR_VERSION, strlen(WARRIOR_VERSION)) == 0;

        fclose(fp);
        if (current) {
            return; /* (made from this warrior.txt by this version of the maker) */
        }
    }
    if ((fp = fopen(path, "r")) == NULL) {
        return;
    }
    memset(&ws, 0xFF, sizeof(ws)); /* (every field -1) */
    memset(ws.look, 0, sizeof(ws.look));
    memset(ws.pose, 0, sizeof(ws.pose));
    memset(ws.formTo, 0, sizeof(ws.formTo));
    ws.damage = 0;
    ws.dynShade = 0;
    memset(ws.partCos, 0, sizeof(ws.partCos));
    memset(ws.layerCos, 0, sizeof(ws.layerCos));
    ws.bodyCos = 0;
    ws.colours = 0;
    ws.edits = 0;
    memset(ws.moveName, 0, sizeof(ws.moveName));
    memset(ws.labSet, 0, sizeof(ws.labSet)); /* (the Technique Lab, shading by texture: none until a line says) */
    memset(ws.lab, 0, sizeof(ws.lab));
    memset(ws.texShade, 0, sizeof(ws.texShade));
    memset(ws.skillColour, 0, sizeof(ws.skillColour));
    ws.bodyScale = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *v = strchr(line, '=');

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || v == NULL) {
            continue;
        }
        *v++ = '\0';
        if (strcmp(line, "name") == 0) {
            snprintf(white, sizeof(white), "%s", v);
        } else if (strcmp(line, "title") == 0) {
            snprintf(yellow, sizeof(yellow), "%s", v);
        } else if (strcmp(line, "body") == 0) {
            ws.bodyCos = warrior_costume_of(v);
            ws.body = warrior_fighter(v);
        } else if (strcmp(line, "moves") == 0) {
            ws.moves = warrior_fighter(v);
        } else if (strcmp(line, "effects") == 0) {
            ws.effects = warrior_fighter(v);
        } else if (strcmp(line, "voice") == 0) {
            ws.voice = warrior_fighter(v);
        } else if (strcmp(line, "colour") == 0 || strcmp(line, "color") == 0) {
            int z, r, g, b, parts = 0, con = 0;

            if (ws.colours < WARRIOR_COLOURS && sscanf(v, "%d,%d,%d,%d,%d,%d", &z, &r, &g, &b, &parts, &con) >= 4 && z >= 0 && (z < 48 || (z & WARRIOR_KEY) || (z & WARRIOR_TINT))) {
                ws.colourPart[ws.colours] = parts;
                ws.colourKey[ws.colours] = z;
                ws.colourRgb[ws.colours++] = (uint32_t)(r & 0xFF) | ((uint32_t)(g & 0xFF) << 8) | ((uint32_t)(b & 0xFF) << 16) | ((uint32_t)(con & 0xFF) << 24);
            }
        } else if (warrior_part_key(line + (line[0] == '+'), &side) >= 0) {
            int cos = warrior_costume_of(v), g = warrior_part_key(line + (line[0] == '+'), &side), who = warrior_fighter(v);

            unknown |= who == -1;
            while (side-- > 0) {
                if (line[0] == '+') {
                    ws.layerCos[g] = cos;
                    ws.layer[g++] = who;
                } else {
                    ws.partCos[g] = cos;
                    ws.part[g++] = who;
                }
            }
        } else if (warrior_size_key(line) >= 0) {
            int pct = atoi(v);

            ws.size[warrior_size_key(line)] = pct < 50 ? 50 : pct > 180 ? 180 : pct;
        } else if (strcmp(line, "render") == 0) {
            unknown |= (ws.render = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "hud") == 0) {
            unknown |= (ws.hud = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "style") == 0) {
            unknown |= (ws.style = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "stance") == 0) {
            unknown |= (ws.stance = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "aurafrom") == 0) {
            unknown |= (ws.auraFrom = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "animations") == 0) { /* (the first version: every kind) */
            int q;

            unknown |= (ws.anim[0] = warrior_fighter(v)) < 0;
            for (q = 1; q < 6; q++) {
                ws.anim[q] = ws.anim[0];
            }
        } else if (warrior_anim_key(line) >= 0) {
            unknown |= (ws.anim[warrior_anim_key(line)] = warrior_fighter(v)) < 0;
        } else if (warrior_look_key(line) >= 0) {
            snprintf(ws.look[warrior_look_key(line)], sizeof(ws.look[0]), "%s", v);
        } else if (strcmp(line, "damage") == 0) {
            ws.damage = warrior_damage_of(v);
        } else if (strcmp(line, "pose") == 0) {
            int pb, px[3];

            if (warrior_pose_of(v, &pb, px)) {
                memcpy(ws.pose[pb], px, sizeof(px));
            }
        } else if (strcmp(line, "shadefrom") == 0) {
            unknown |= (ws.shadeFrom = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "dynamic") == 0) {
            ws.dynShade = strcmp(v, "on") == 0 ? 1 : strcmp(v, "off") == 0 ? 2 : 0;
        } else if (strncmp(line, "form", 4) == 0) {
            int fc, fk, f;
            char to[64];

            if ((f = warrior_form_of(line, v, &fc, &fk, to)) >= 0) {
                snprintf(ws.formTo[f], sizeof(ws.formTo[f]), "%s", to);
                ws.formCost[f] = fc;
                ws.formKind[f] = fk;
            }
        } else if (strncmp(line, "extra", 5) == 0) {
            int xs, xw, xc, x = warrior_xlayer_of(line, v, &xs, &xw, &xc);

            if (x >= 0) {
                unknown |= xw < 0;
                ws.xlayer[x] = xw;
                ws.xslot[x] = xs;
                ws.xcos[x] = xc;
            }
        } else if (strcmp(line, "blast1_1fx") == 0 || strcmp(line, "blast1_2fx") == 0) {
            int t = line[7] - '1';
            char *colon = strrchr(v, ':');

            ws.skillFxFrom[t] = t;
            if (colon != NULL && colon[1] >= '1' && colon[1] <= '2') {
                ws.skillFxFrom[t] = colon[1] - '1';
                *colon = '\0';
            }
            unknown |= (ws.skillFx[t] = warrior_fighter(v)) < 0;
        } else if (strstr(line, "name") != NULL && strncmp(line, "blast", 5) == 0 || strcmp(line, "ultimatename") == 0) {
            int t;

            for (t = 0; t < 5 && strcmp(line, kWarriorMoveNameKey[t]) != 0; t++) {
            }
            if (t < 5) {
                snprintf(ws.moveName[t], sizeof(ws.moveName[t]), "%s", v);
            }
        } else if (strstr(line, "colour") != NULL && (strncmp(line, "blast2", 6) == 0 || strncmp(line, "ultimate", 8) == 0)) {
            int t;

            for (t = 0; t < 3 && strcmp(line, kWarriorTechColourKey[t]) != 0; t++) {
            }
            if (t < 3) {
                ws.techColour[t] = warrior_hex_colour(v);
            }
        } else if (strcmp(line, "blast1_1colour") == 0 || strcmp(line, "blast1_2colour") == 0) {
            ws.skillColour[line[7] - '1'] = warrior_hex_colour(v);
        } else if (strcmp(line, "bodyscale") == 0) {
            ws.bodyScale = atoi(v) >= 50 && atoi(v) <= 250 ? atoi(v) : 0;
        } else if (warrior_lab_of(line, v, ws.labSet, ws.lab)) {
            /* (the Technique Lab) */
        } else if (strcmp(line, "shade_tex") == 0) {
            unsigned hk, rgb;
            int q;

            for (q = 0; q < 16 && ws.texShade[q][0] != 0; q++) {
            }
            int g = 0;

            if (q < 16 && sscanf(v, "%x,%x,%d", &hk, &rgb, &g) >= 2 && hk != 0) {
                ws.texShade[q][0] = (int32_t)hk;
                ws.texShade[q][1] = warrior_hex_colour_of(rgb) | (g >= 1 && g <= 9 ? g << 25 : 0); /* (its gradient) */
            }
        } else if (strcmp(line, "aura") == 0) {
            ws.auraColour = warrior_hex_colour(v);
        } else if (strcmp(line, "edit") == 0) {
            int *e = &ws.edit[ws.edits * WP_EDIT_WORDS];

            if (ws.edits < WARRIOR_EDITS && sscanf(v, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &e[0], &e[1], &e[2], &e[3], &e[4], &e[5], &e[6], &e[7], &e[8],
                                                   &e[9], &e[10], &e[11]) == 12) {
                ws.edits++;
            }
        } else if (warrior_cam_key(line) >= 0) {
            int t = warrior_cam_key(line);
            char *colon = strrchr(v, ':');

            ws.camSlot[t] = t < 2 ? t : 2;
            if (colon != NULL && colon[1] >= '1' && colon[1] <= '3') {
                ws.camSlot[t] = colon[1] - '1';
                *colon = '\0';
            }
            unknown |= (ws.camFrom[t] = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "blast2_1fx") == 0 || strcmp(line, "blast2_2fx") == 0 || strcmp(line, "ultimatefx") == 0) {
            int t = line[0] == 'u' ? 2 : line[7] - '1';
            char *colon = strrchr(v, ':');

            ws.techFxFrom[t] = t;
            if (colon != NULL && colon[1] >= '1' && colon[1] <= '3') {
                ws.techFxFrom[t] = colon[1] - '1';
                *colon = '\0';
            }
            unknown |= (ws.techFx[t] = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "finishers") == 0) {
            unknown |= (ws.finishers = warrior_fighter(v)) < 0;
        } else if ((strcmp(line, "blast1_1") == 0 || strcmp(line, "blast1_2") == 0)) {
            int s = line[7] - '1';
            char *colon = strrchr(v, ':');

            ws.skillFrom[s] = s; /* (by default the same slot of that fighter) */
            if (colon != NULL && (colon[1] == '1' || colon[1] == '2')) {
                ws.skillFrom[s] = colon[1] - '1';
                *colon = '\0';
            }
            unknown |= (ws.skill[s] = warrior_fighter(v)) < 0;
        } else if (strcmp(line, "blast2_1") == 0 || strcmp(line, "blast2_2") == 0 || strcmp(line, "ultimate") == 0) {
            int s = line[0] == 'u' ? 2 : line[7] - '1';
            char *colon = strrchr(v, ':');

            ws.techFrom[s] = s; /* (by default the same slot of that fighter) */
            if (colon != NULL && colon[1] >= '1' && colon[1] <= '3') {
                ws.techFrom[s] = colon[1] - '1';
                *colon = '\0';
            }
            unknown |= (ws.tech[s] = warrior_fighter(v)) < 0;
        }
    }
    fclose(fp);
    if (ws.body < 0 || ws.moves < 0 || unknown) {
        warrior_fail(dir, "warrior.txt needs body= and moves=, and every fighter it names must be a roster name (as on the select screen) or an id.");
        return;
    }
    if (is_mod_ref(ws.moves) || is_mod_ref(ws.style) || is_mod_ref(ws.voice)) {
        warrior_fail(dir, "The moves, style and voice fighters must be roster fighters: the game counts the warrior as one of them. A mod fighter can give the look, techniques, motions and effects.");
        return;
    }
    if (ws.effects < 0) {
        ws.effects = ws.moves;
    }
    if (ws.stance < 0) { /* (standing as the body stands: its own pose and idles, not the moves fighter's) */
        ws.stance = ws.body;
    }
    if (ws.voice < 0) {
        ws.voice = ws.moves;
    }
    memset(&dn, 0, sizeof(dn)); /* the stage 2 donors' model files (their first costume) */
    k = 1;
    for (side = 0; side < 2; side++) {
        if (ws.skill[side] >= 0) {
            k &= warrior_open(&dn.skill[side], MODEL_FILE + ws.skill[side] * 10) && dn.skill[side].n == WARRIOR_PARTS;
        }
    }
    if (ws.stance >= 0) {
        k &= warrior_open(&dn.stance, MODEL_FILE + ws.stance * 10) && dn.stance.n == WARRIOR_PARTS;
    }
    for (side = 0; side < 3; side++) {
        if (ws.techFx[side] >= 0) {
            k &= warrior_open(&dn.fx[side], MODEL_FILE + ws.techFx[side] * 10) && dn.fx[side].n == WARRIOR_PARTS;
        }
    }
    for (side = 0; side < 4; side++) {
        if (ws.camFrom[side] >= 0) {
            k &= warrior_open(&dn.cam[side], MODEL_FILE + ws.camFrom[side] * 10) && dn.cam[side].n == WARRIOR_PARTS;
        }
    }
    if (ws.style >= 0) {
        k &= warrior_open(&dn.style, MODEL_FILE + ws.style * 10) && dn.style.n == WARRIOR_PARTS;
    }
    if (ws.auraFrom >= 0) {
        k &= warrior_open(&dn.aura, MODEL_FILE + ws.auraFrom * 10) && dn.aura.n == WARRIOR_PARTS;
    }
    ws.anim[2] = -1; /* (melee's animations: always the moves fighter's, "Melee & rush" -- one row for them, not two) */
    for (side = 0; side < 9; side++) {
        if (ws.anim[side] >= 0) {
            k &= warrior_open(&dn.anim[side], MODEL_FILE + ws.anim[side] * 10) && dn.anim[side].n == WARRIOR_PARTS;
        }
    }
    for (side = 0; side < 2; side++) {
        if (ws.skillFx[side] >= 0) {
            k &= warrior_open(&dn.sfx[side], MODEL_FILE + ws.skillFx[side] * 10) && dn.sfx[side].n == WARRIOR_PARTS;
        }
    }
    if (ws.finishers >= 0) {
        k &= warrior_open(&dn.fin, MODEL_FILE + ws.finishers * 10) && dn.fin.n == WARRIOR_PARTS;
    }
    for (side = 0; side < 3; side++) {
        if (ws.tech[side] >= 0) {
            k &= warrior_open(&dn.tech[side], MODEL_FILE + ws.tech[side] * 10) && dn.tech[side].n == WARRIOR_PARTS;
        }
    }
    if (!k) {
        warrior_fail(dir, "A fighter named by blast1_1, blast1_2, blast2_1, blast2_2, ultimate, style or finishers was not found on the disc.");
        goto out;
    }
    if (!first && (fp = fopen(built, "r")) != NULL) { /* what was made last time goes (the moves fighter may differ) */
        while (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            if (line[0] != '\0' && line[0] != '#' && strstr(line, "..") == NULL) {
                snprintf(file, sizeof(file), "%s/%s", dir, line);
                remove(file);
            }
        }
        fclose(fp);
    }
    { /* (its own colours or drawings, and the fighters' own damaged textures asked for: those textures are not the clean
         ones the colours were picked on - an arm, the eyebrows back to the fighter's colours - so the wear is painted over
         its clean look instead, scuffed) */
        char pd[700];
        int tx = 0;

        snprintf(pd, sizeof(pd), "%s/paint", dir);
        for (k = 0; k < 16; k++) {
            tx |= ws.texShade[k][0] != 0;
        }
        for (k = 0; k < 12; k++) { /* (a layer over the body: its damaged version is not made for that body) */
            tx |= ws.layer[k] >= 0 || (k < 4 && ws.xlayer[k] >= 0);
        }
        if (ws.damage <= 0 && ws.look[0][0] == ' ' && (ws.colours > 0 || tx || is_dir(pd))) {
            ws.damage = 1;
        }
    }
    for (k = 0; k < (name[0] == '_' ? 1 : 8); k++) { /* (a costume the body lacks is left out: the mod's first costume stands in; the
                                                     editor's preview has only the one the screen shows) */
        int got, kk = k, c = k & 3, dmg = k >= 4;
        const WarriorSpec *use = &ws;
        const char *lf = dmg && ws.look[0][0] && ws.damage <= 0 ? ws.look[0] : c > 0 && ws.look[c][0] ? ws.look[c] : NULL;
        static WarriorSpec alt;
        char ownLook[160];

        if (lf != NULL && strcmp(lf, "#own") == 0) { /* (a costume of its own: costumeN/warrior.txt of its folder) */
            snprintf(ownLook, sizeof(ownLook), "%s/costume%d", name, c + 1);
            lf = ownLook;
        }
        if (lf != NULL && name[0] != '_' && warrior_look_spec(lf, &ws, &alt)) { /* (another warrior's look worn: its own
                                                                                  first costume, or its damaged one) */
            use = &alt;
            kk = dmg && lf != ws.look[0] ? 4 : 0;
        }
        if (dmg && ws.damage >= 1 && ws.damage <= 3) { /* (the damaged look from the clean one: wear painted on, or none) */
            kk = use == &ws ? c : 0;
        }
        snprintf(made[count], sizeof(made[0]), "%s_%s.pak", kPrefix[ws.moves], kModel[k]);
        snprintf(file, sizeof(file), "%s/%s", dir, made[count]);
        sWarriorWhy = NULL;
        got = warrior_model(use, &dn, kk, file);
        if (got > 0 && dmg && (ws.damage == 1 || ws.damage == 2)) {
            warrior_damage_file(file, ws.damage, k);
        }
        if (got < 0 || (got == 0 && k == 0)) {
            warrior_fail(dir, sWarriorWhy != NULL ? sWarriorWhy : "A model file could not be made: the disc's files of those fighters were not found or not read.");
            goto out;
        }
        count += got;
    }
    snprintf(made[count], sizeof(made[0]), "%s_anm.pak", kPrefix[ws.moves]);
    snprintf(file, sizeof(file), "%s/%s", dir, made[count++]);
    k = warrior_anims(&ws, file);
    if (k < 0) {
        warrior_fail(dir, "The animations taken from those fighters do not fit in the game's animation buffer together.");
        goto out;
    }
    snprintf(made[count], sizeof(made[0]), "%s_eff.pak", kPrefix[ws.moves]);
    snprintf(file, sizeof(file), "%s/%s", dir, made[count++]);
    side = warrior_effects(&ws, file);
    if (side < 0) {
        warrior_fail(dir, "The effects taken from those fighters do not fit in the game's effect buffer together.");
        goto out;
    }
    k &= side;
    snprintf(made[count], sizeof(made[0]), "%s_img.cdbt", kPrefix[ws.moves]);
    snprintf(file, sizeof(file), "%s/%s", dir, made[count++]);
    k &= warrior_copy(FACE_FILE + (ws.render >= 0 ? ws.render : ws.body), file);
    snprintf(file, sizeof(file), "%s/pzs3us1", dir);
    make_dir(file);
    for (side = 0; side < 2; side++) { /* the gauge faces, numbered for the moves fighter (its template) */
        snprintf(made[count], sizeof(made[0]), "pzs3us1/%05d.bin", DATA_FILE + ws.moves * 2 + side - 1);
        snprintf(file, sizeof(file), "%s/%s", dir, made[count++]);
        k &= warrior_copy(DATA_FILE + (ws.hud >= 0 ? ws.hud : ws.body) * 2 + side, file);
    }
    if (k) {
        warrior_hud_photo(dir); /* (a photo of its face taken in Z Warriors goes over them) */
    }
    if (!k) {
        warrior_fail(dir, "The animations, effects, portrait or gauge faces could not be made from the disc's files.");
        goto out;
    }
    memset(&tmp, 0, sizeof(tmp));
    snprintf(tmp.dir, sizeof(tmp.dir), "%s", dir);
    tmp.target = tmp.tmpl = tmp.voice = -1;
    trans_clear(&tmp.tr);
    read_settings(&tmp); /* (what the manager set for it, such as its chip, portrait or forms, stays) */
    tmp.isNew = 1;
    tmp.target = -1;
    tmp.tmpl = ws.style >= 0 ? ws.style : ws.moves; /* the fighter the game counts it as */
    tmp.voice = ws.voice;
    snprintf(tmp.white, sizeof(tmp.white), "%s", white[0] != '\0' ? white : name);
    snprintf(tmp.yellow, sizeof(tmp.yellow), "%s", yellow[0] != '\0' ? yellow : "Custom Warrior");
    if (!tmp.tr.set) { /* none of the moves fighter's forms */
        trans_clear(&tmp.tr);
        tmp.tr.set = 1;
    }
    if (ws.formTo[0][0] || ws.formTo[1][0] || ws.formTo[2][0] || ws.formTo[3][0]) { /* its forms of Z Warriors: into those
                                                                                       warriors (their mods, by name) */
        trans_clear(&tmp.tr);
        tmp.tr.set = 1;
        for (k = 0; k < 4; k++) {
            if (ws.formTo[k][0] == '\0') {
                tmp.trName[k][0] = '\0';
                continue;
            }
            tmp.tr.cost[k] = ws.formCost[k] < 0 ? 0 : ws.formCost[k] > 7 ? 7 : ws.formCost[k];
            tmp.tr.type[k] = ws.formKind[k] == 1 ? 1 : 0;
            tmp.tr.effect[k] = ws.formKind[k] == 1 ? 1 : 2;
            tmp.tr.assist[k] = -1;
            tmp.tr.fresh[k] = 0;
            tmp.tr.target[k] = -1;
            snprintf(tmp.trName[k], sizeof(tmp.trName[k]), "%s", ws.formTo[k]);
        }
    }
    if (!tmp.tr.fuseSet) {
        tmp.tr.fuseSet = 1;
    }
    write_settings(&tmp);
    if ((fp = fopen(built, "w")) != NULL) {
        fprintf(fp, "%s\n", WARRIOR_VERSION);
        for (k = 0; k < count; k++) {
            fprintf(fp, "%s\n", made[k]);
        }
        fclose(fp);
    }
    snprintf(file, sizeof(file), "%s/warrior_error.txt", dir);
    remove(file);
    if (first && name[0] != '_') { /* a new warrior is switched on (not the editor's preview) */
        snprintf(file, sizeof(file), "%s/mods/enabled.txt", root());
        if ((fp = fopen(file, "a")) != NULL) {
            fprintf(fp, "\n%s\n", name); /* (the file may not end in a newline; an empty line names no mod) */
            fclose(fp);
        }
    }
out:
    free(dn.skill[0].d);
    free(dn.skill[1].d);
    free(dn.style.d);
    free(dn.aura.d);
    for (side = 0; side < 9; side++) {
        free(dn.anim[side].d);
    }
    free(dn.sfx[0].d);
    free(dn.sfx[1].d);
    free(dn.stance.d);
    for (side = 0; side < 3; side++) {
        free(dn.fx[side].d);
    }
    for (side = 0; side < 4; side++) {
        free(dn.cam[side].d);
    }
    free(dn.fin.d);
    for (side = 0; side < 3; side++) {
        free(dn.tech[side].d);
    }
}

/* ---- the warriors for the Z Warriors screens (src/menu/port_zw.c) ----
 * Every folder of <data>/mods with a warrior.txt, whether its files could be made or not. The screens read and write
 * a warrior as a PortWarrior (the same layout in include/menu/port_zw.h); writing it writes warrior.txt (fighters
 * by their roster names) and reads the library again, which makes the warrior's files. */
#define MAX_WARRIORS 128
static void ensure(void);
void Port_ModRescan(void);
static char sWarrior[MAX_WARRIORS][64]; /* folder names */
static int sWarriorCount;

typedef struct PortWarrior {
    char folder[64];
    char name[40], title[40];
    int32_t body, moves, effects, voice, style, finishers, render, hud; /* fighter ids, -1 not given */
    int32_t skill[2], skillFrom[2];
    int32_t tech[3], techFrom[3];
    int32_t mod;      /* its place in the library (Port_ModGetInfo...), -1 when its files are not made */
    int32_t problem;  /* 1: warrior_error.txt says what went wrong */
    int32_t colours;  /* its colour= lines: zone and new colour (0xBBGGRR) */
    int32_t colourKey[WARRIOR_COLOURS];
    uint32_t colourRgb[WARRIOR_COLOURS];
    int32_t part[12]; /* hair, head, face, chest, waist, upper arms, forearms, hands, hips, shins, feet, accessories:
                         whose, -1 the body's */
    int32_t size[8];  /* head, hair, hands, feet, arms, torso, legs (thickness): percent, <= 0 as they are */
    int32_t layer[12]; /* another fighter's part layered over each slot, -1 none */
    int32_t colourPart[WARRIOR_COLOURS]; /* each colour's parts (1 << slot), 0 all: those parts get their own copies to paint */
    int32_t stance;         /* whose standing motions, -1 the moves fighter's */
    int32_t techFx[3], techFxFrom[3]; /* each technique's effects from another's (of the same effect type), -1 its own */
    int32_t edits;          /* the pieces' edits (WP_EDIT_WORDS each) */
    int32_t edit[WARRIOR_EDITS * WP_EDIT_WORDS];
    char moveName[5][32];   /* a technique's name of its own, "" its own */
    int32_t techColour[3];  /* a technique's effects' colour, 0x1000000 | 0xBBGGRR, 0 their own */
    int32_t auraColour;     /* the aura's colour, 0x1000000 | 0xBBGGRR, 0 its own */
    int32_t partCostume[12], layerCostume[12], bodyCostume; /* always that costume (1..4) of a part's, a layer's, the body's
                                                              fighter; 0 the warrior's own costume's */
    int32_t aura;           /* aurafrom=: whose aura kind, -1 the moves fighter's */
    int32_t anim[6];        /* anim_move= .. anim_guard=: whose motions of each kind (kWarriorAnimCat), -1 the moves fighter's */
    int32_t skillFx[2], skillFxFrom[2]; /* blast1_1fx= blast1_2fx=: each Blast 1 skill's look from another's, -1 its own */
    char look[4][64];       /* damaged= look2= look3= look4=: another warrior's look (its folder) worn as the damaged look,
                               costumes 2..4; "" its own; "#own" (costumes 2..4) its own, in costumeN/warrior.txt */
    int32_t damage;         /* damage=: 0 the fighters' own damaged textures, 1 scuffed, 2 battle-torn, 3 clean */
    int32_t xlayer[4], xslot[4], xcos[4]; /* extra1= .. extra4=: more layers, each another fighter's part (slot xslot)
                                              over the look, -1 none; xcos its costume (1..4), 0 the warrior's own */
    int32_t pose[16][3];    /* pose=<bone>:<x>,<y>,<z>: the stance maker's turns (degrees) of each bone (kPoseNode) */
    int32_t shadeFrom;      /* shadefrom=: the fighter whose shading and glow each part takes, -1 its own */
    int32_t dynShade;       /* dynamic=: the Super Saiyan lighting: 0 as the shading fighter (else the body), 1 on, 2 off */
    char formTo[4][64];     /* form1= .. form4=cost,kind,<warrior folder>: its transformations into other warriors */
    int32_t formCost[4], formKind[4];
    int32_t formAura[4];    /* formaura1= ..: each form's aura colour, given to that warrior when this one is saved */
    int32_t camFrom[4], camSlot[4]; /* cam_blast2_1= ..: whose technique's camera, -1 the technique's own */
    int32_t labSet[5];      /* lab_blast1_1= .. lab_ultimate=: the Technique Lab's stats of each technique (bit per LAB_) */
    int32_t lab[5][16];     /* ... and their values (warrior_lab_apply's units) */
    int32_t texShade[16][2]; /* shade_tex=: a texture's shading (its picture hash; 0x1000000 | 0xBBGGRR, 0 none) */
    int32_t skillColour[2];  /* blast1_1colour= blast1_2colour=: a Blast 1 skill's effects' colour, 0 their own */
    int32_t anim2[3];        /* anim_intro= anim_victory= anim_taunt=: whose (kWarriorAnimCat 6..8), -1 the moves fighter's */
    int32_t bodyScale;       /* bodyscale=: the whole warrior's size in percent, 0 as it is */
    int32_t glow[5];         /* glow_blast1_1= .. glow_ultimate=: the body's glow while it does that technique (Kaio-ken
                                like), 0x1000000 | 0xBBGGRR (its brightness the strength), 0 none */
} PortWarrior;
typedef char WarriorLayoutCheck[(offsetof(PortWarrior, body) == WARRIOR_W_BODY * 4 && offsetof(PortWarrior, part) == WARRIOR_W_PART * 4 &&
                                  offsetof(PortWarrior, layer) == WARRIOR_W_LAYER * 4 && offsetof(PortWarrior, colourPart) == WARRIOR_W_CPART * 4 &&
                                  offsetof(PortWarrior, size) == WARRIOR_W_SIZE * 4 && offsetof(PortWarrior, edits) == WARRIOR_W_EDITS * 4 &&
                                  offsetof(PortWarrior, partCostume) == WARRIOR_W_COSTUME * 4) ? 1 : -1];

static void warrior_note(const char *dir, const char *name) {
    char path[600];
    struct stat st;

    snprintf(path, sizeof(path), "%s/warrior.txt", dir);
    if (name[0] == '_') { /* (the editor's preview, never listed) */
        return;
    }
    if (sWarriorCount < MAX_WARRIORS && stat(path, &st) == 0 && strlen(name) < sizeof(sWarrior[0])) {
        strcpy(sWarrior[sWarriorCount++], name);
    }
}

static int warrior_cmp(const void *a, const void *b) {
    return strcasecmp((const char *)a, (const char *)b);
}

/* The library in the order the player reads it: by the warrior's name (then its title, then its folder), not by the
   folder ("Warrior - Kid Buu Vegeta" is named Majin Vegeta). */
typedef struct WarriorKey {
    char key[200];
    char folder[64];
} WarriorKey;

static int warrior_key_cmp(const void *a, const void *b) {
    return strcasecmp(((const WarriorKey *)a)->key, ((const WarriorKey *)b)->key);
}

static void warrior_sort(void) {
    static WarriorKey keys[MAX_WARRIORS];
    int i;

    for (i = 0; i < sWarriorCount; i++) {
        char path[700], line[256], name[64] = "", title[64] = "";
        FILE *fp;

        snprintf(path, sizeof(path), "%s/mods/%s/warrior.txt", root(), sWarrior[i]);
        if ((fp = fopen(path, "r")) != NULL) {
            while (fgets(line, sizeof(line), fp) != NULL) {
                line[strcspn(line, "\r\n")] = '\0';
                if (strncmp(line, "name=", 5) == 0) {
                    snprintf(name, sizeof(name), "%s", line + 5);
                } else if (strncmp(line, "title=", 6) == 0) {
                    snprintf(title, sizeof(title), "%s", line + 6);
                }
            }
            fclose(fp);
        }
        snprintf(keys[i].key, sizeof(keys[i].key), "%s\x01%s\x01%s", name[0] ? name : sWarrior[i], title, sWarrior[i]);
        snprintf(keys[i].folder, sizeof(keys[i].folder), "%s", sWarrior[i]);
    }
    qsort(keys, (size_t)sWarriorCount, sizeof(keys[0]), warrior_key_cmp);
    for (i = 0; i < sWarriorCount; i++) {
        snprintf(sWarrior[i], sizeof(sWarrior[i]), "%s", keys[i].folder);
    }
}

int Port_WarriorCount(void) {
    ensure();
    return sWarriorCount;
}

/* Reads warrior.txt of folder `folder` into w. */
static void warrior_read(const char *folder, PortWarrior *w);

/* Another warrior's look (folder `folder`: its body, parts, layers, sizes, colours, edits, costumes) on everything
   else of `base`: one of `base`'s costumes worn as that warrior looks. 0 when it is not a warrior. */
static int warrior_look_spec(const char *folder, const WarriorSpec *base, WarriorSpec *out) {
    static PortWarrior w;
    int i;

    warrior_read(folder, &w);
    if (w.body < 0) {
        return 0;
    }
    *out = *base;
    out->body = w.body;
    for (i = 0; i < 12; i++) {
        out->part[i] = w.part[i];
        out->layer[i] = w.layer[i];
        out->partCos[i] = w.partCostume[i];
        out->layerCos[i] = w.layerCostume[i];
    }
    out->bodyCos = w.bodyCostume;
    for (i = 0; i < 8; i++) {
        out->size[i] = w.size[i] <= 0 ? w.size[i] : w.size[i] < 50 ? 50 : w.size[i] > 180 ? 180 : w.size[i];
    }
    out->colours = w.colours < 0 ? 0 : w.colours > WARRIOR_COLOURS ? WARRIOR_COLOURS : w.colours;
    for (i = 0; i < WARRIOR_COLOURS; i++) {
        out->colourKey[i] = w.colourKey[i];
        out->colourRgb[i] = w.colourRgb[i];
        out->colourPart[i] = w.colourPart[i];
    }
    out->edits = w.edits < 0 ? 0 : w.edits > WARRIOR_EDITS ? WARRIOR_EDITS : w.edits;
    for (i = 0; i < 4; i++) {
        out->xlayer[i] = w.xlayer[i];
        out->xslot[i] = w.xslot[i];
        out->xcos[i] = w.xcos[i];
    }
    memcpy(out->edit, w.edit, sizeof(out->edit));
    return 1;
}

static void warrior_read(const char *folder, PortWarrior *w) {
    char path[600], line[256];
    FILE *fp;
    int n;

    memset(w, 0, sizeof(*w));
    memset(&w->body, 0xFF, sizeof(int32_t) * 18); /* (every fighter field -1) */
    memset(w->part, 0xFF, sizeof(w->part));
    memset(w->size, 0, sizeof(w->size));
    memset(w->layer, 0xFF, sizeof(w->layer));
    w->stance = -1;
    w->aura = -1;
    memset(w->anim, 0xFF, sizeof(w->anim));
    memset(w->anim2, 0xFF, sizeof(w->anim2));
    memset(w->xlayer, 0xFF, sizeof(w->xlayer));
    w->shadeFrom = -1;
    memset(w->xslot, 0xFF, sizeof(w->xslot));
    memset(w->skillFx, 0xFF, sizeof(w->skillFx));
    memset(w->techFx, 0xFF, sizeof(w->techFx));
    memset(w->camFrom, 0xFF, sizeof(w->camFrom));
    snprintf(w->folder, sizeof(w->folder), "%s", folder);
    snprintf(path, sizeof(path), "%s/mods/%s/warrior.txt", root(), folder);
    if ((fp = fopen(path, "r")) == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *v = strchr(line, '=');

        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || v == NULL) {
            continue;
        }
        *v++ = '\0';
        if (strcmp(line, "name") == 0) {
            snprintf(w->name, sizeof(w->name), "%s", v);
        } else if (strcmp(line, "title") == 0) {
            snprintf(w->title, sizeof(w->title), "%s", v);
        } else if (strcmp(line, "body") == 0) {
            w->bodyCostume = warrior_costume_of(v);
            w->body = warrior_fighter(v);
        } else if (strcmp(line, "moves") == 0) {
            w->moves = warrior_fighter(v);
        } else if (strcmp(line, "effects") == 0) {
            w->effects = warrior_fighter(v);
        } else if (strcmp(line, "voice") == 0) {
            w->voice = warrior_fighter(v);
        } else if (strcmp(line, "style") == 0) {
            w->style = warrior_fighter(v);
        } else if (strcmp(line, "finishers") == 0) {
            w->finishers = warrior_fighter(v);
        } else if (strcmp(line, "stance") == 0) {
            w->stance = warrior_fighter(v);
        } else if (strcmp(line, "aurafrom") == 0) {
            w->aura = warrior_fighter(v);
        } else if (strcmp(line, "animations") == 0) { /* (the first version: every kind) */
            int a = warrior_fighter(v), q;

            for (q = 0; q < 6; q++) {
                w->anim[q] = a;
            }
        } else if (warrior_anim_key(line) >= 0) {
            if (warrior_anim_key(line) >= 6) {
                w->anim2[warrior_anim_key(line) - 6] = warrior_fighter(v);
            } else {
                w->anim[warrior_anim_key(line)] = warrior_fighter(v);
            }
        } else if (warrior_look_key(line) >= 0) {
            snprintf(w->look[warrior_look_key(line)], sizeof(w->look[0]), "%s", v);
        } else if (strcmp(line, "damage") == 0) {
            w->damage = warrior_damage_of(v);
        } else if (strcmp(line, "pose") == 0) {
            int pb, px[3];

            if (warrior_pose_of(v, &pb, px)) {
                memcpy(w->pose[pb], px, sizeof(px));
            }
        } else if (strcmp(line, "shadefrom") == 0) {
            w->shadeFrom = warrior_fighter(v);
        } else if (strcmp(line, "dynamic") == 0) {
            w->dynShade = strcmp(v, "on") == 0 ? 1 : strcmp(v, "off") == 0 ? 2 : 0;
        } else if (strncmp(line, "form", 4) == 0) {
            int fc, fk, f;
            char to[64];

            if (strncmp(line, "formaura", 8) == 0 && line[8] >= '1' && line[8] <= '4') {
                w->formAura[line[8] - '1'] = (int32_t)strtoul(v, NULL, 16);
            } else if ((f = warrior_form_of(line, v, &fc, &fk, to)) >= 0) {
                snprintf(w->formTo[f], sizeof(w->formTo[f]), "%s", to);
                w->formCost[f] = fc;
                w->formKind[f] = fk;
            }
        } else if (strncmp(line, "extra", 5) == 0) {
            int xs, xw, xc, x = warrior_xlayer_of(line, v, &xs, &xw, &xc);

            if (x >= 0) {
                w->xlayer[x] = xw;
                w->xslot[x] = xs;
                w->xcos[x] = xc;
            }
        } else if (strcmp(line, "blast1_1fx") == 0 || strcmp(line, "blast1_2fx") == 0) {
            int t = line[7] - '1';
            char *colon = strrchr(v, ':');

            w->skillFxFrom[t] = t;
            if (colon != NULL && colon[1] >= '1' && colon[1] <= '2') {
                w->skillFxFrom[t] = colon[1] - '1';
                *colon = '\0';
            }
            w->skillFx[t] = warrior_fighter(v);
        } else if (strstr(line, "name") != NULL && strncmp(line, "blast", 5) == 0 || strcmp(line, "ultimatename") == 0) {
            int t;

            for (t = 0; t < 5 && strcmp(line, kWarriorMoveNameKey[t]) != 0; t++) {
            }
            if (t < 5) {
                snprintf(w->moveName[t], sizeof(w->moveName[t]), "%s", v);
            }
        } else if (strstr(line, "colour") != NULL && (strncmp(line, "blast2", 6) == 0 || strncmp(line, "ultimate", 8) == 0)) {
            int t;

            for (t = 0; t < 3 && strcmp(line, kWarriorTechColourKey[t]) != 0; t++) {
            }
            if (t < 3) {
                w->techColour[t] = warrior_hex_colour(v);
            }
        } else if (strcmp(line, "blast1_1colour") == 0 || strcmp(line, "blast1_2colour") == 0) {
            w->skillColour[line[7] - '1'] = warrior_hex_colour(v);
        } else if (strcmp(line, "bodyscale") == 0) {
            w->bodyScale = atoi(v) >= 50 && atoi(v) <= 250 ? atoi(v) : 0;
        } else if (strncmp(line, "glow_", 5) == 0 && warrior_glow_key(line) >= 0) {
            w->glow[warrior_glow_key(line)] = warrior_hex_colour(v);
        } else if (warrior_lab_of(line, v, w->labSet, w->lab)) {
            /* (the Technique Lab) */
        } else if (strcmp(line, "shade_tex") == 0) {
            unsigned hk, rgb;
            int q;

            for (q = 0; q < 16 && w->texShade[q][0] != 0; q++) {
            }
            int g = 0;

            if (q < 16 && sscanf(v, "%x,%x,%d", &hk, &rgb, &g) >= 2 && hk != 0) {
                w->texShade[q][0] = (int32_t)hk;
                w->texShade[q][1] = warrior_hex_colour_of(rgb) | (g >= 1 && g <= 9 ? g << 25 : 0); /* (its gradient) */
            }
        } else if (strcmp(line, "aura") == 0) {
            w->auraColour = warrior_hex_colour(v);
        } else if (strcmp(line, "edit") == 0) {
            int32_t *e = &w->edit[w->edits * WP_EDIT_WORDS];

            if (w->edits < WARRIOR_EDITS && sscanf(v, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &e[0], &e[1], &e[2], &e[3], &e[4], &e[5], &e[6], &e[7], &e[8],
                                                   &e[9], &e[10], &e[11]) == 12) {
                w->edits++;
            }
        } else if (warrior_cam_key(line) >= 0) {
            int t = warrior_cam_key(line);
            char *colon = strrchr(v, ':');

            w->camSlot[t] = t < 2 ? t : 2;
            if (colon != NULL && colon[1] >= '1' && colon[1] <= '3') {
                w->camSlot[t] = colon[1] - '1';
                *colon = '\0';
            }
            w->camFrom[t] = warrior_fighter(v);
        } else if (strcmp(line, "blast2_1fx") == 0 || strcmp(line, "blast2_2fx") == 0 || strcmp(line, "ultimatefx") == 0) {
            int t = line[0] == 'u' ? 2 : line[7] - '1';
            char *colon = strrchr(v, ':');

            w->techFxFrom[t] = t;
            if (colon != NULL && colon[1] >= '1' && colon[1] <= '3') {
                w->techFxFrom[t] = colon[1] - '1';
                *colon = '\0';
            }
            w->techFx[t] = warrior_fighter(v);
        } else if (strcmp(line, "colour") == 0 || strcmp(line, "color") == 0) {
            int z, r, g, b, parts = 0, con = 0;

            if (w->colours < WARRIOR_COLOURS && sscanf(v, "%d,%d,%d,%d,%d,%d", &z, &r, &g, &b, &parts, &con) >= 4) {
                w->colourPart[w->colours] = parts;
                w->colourKey[w->colours] = z;
                w->colourRgb[w->colours++] = (uint32_t)(r & 0xFF) | ((uint32_t)(g & 0xFF) << 8) | ((uint32_t)(b & 0xFF) << 16) | ((uint32_t)(con & 0xFF) << 24);
            }
        } else if (warrior_part_key(line + (line[0] == '+'), &n) >= 0) {
            int cos = warrior_costume_of(v), g = warrior_part_key(line + (line[0] == '+'), &n), who = warrior_fighter(v);

            while (n-- > 0) {
                if (line[0] == '+') {
                    w->layerCostume[g] = cos;
                    w->layer[g++] = who;
                } else {
                    w->partCostume[g] = cos;
                    w->part[g++] = who;
                }
            }
        } else if (warrior_size_key(line) >= 0) {
            w->size[warrior_size_key(line)] = atoi(v);
        } else if (strcmp(line, "render") == 0) {
            w->render = warrior_fighter(v);
        } else if (strcmp(line, "hud") == 0) {
            w->hud = warrior_fighter(v);
        } else {
            int s = -1, n = 2, *who = NULL, *from = NULL;
            char *colon;

            if (strcmp(line, "blast1_1") == 0 || strcmp(line, "blast1_2") == 0) {
                s = line[7] - '1';
                who = &w->skill[s];
                from = &w->skillFrom[s];
            } else if (strcmp(line, "blast2_1") == 0 || strcmp(line, "blast2_2") == 0 || strcmp(line, "ultimate") == 0) {
                s = line[0] == 'u' ? 2 : line[7] - '1';
                who = &w->tech[s];
                from = &w->techFrom[s];
                n = 3;
            }
            if (who != NULL) {
                *from = s;
                colon = strrchr(v, ':');
                if (colon != NULL && colon[1] >= '1' && colon[1] < '1' + n) {
                    *from = colon[1] - '1';
                    *colon = '\0';
                }
                *who = warrior_fighter(v);
            }
        }
    }
    fclose(fp);
}

void Port_WarriorGet(int i, uint32_t outAddr) {
    PortWarrior *w = (PortWarrior *)(uintptr_t)outAddr;
    char path[600];
    struct stat st;

    ensure();
    if (i < 0 || i >= sWarriorCount) {
        memset(w, 0, sizeof(*w));
        return;
    }
    warrior_read(sWarrior[i], w);
    w->mod = mod_by_name(sWarrior[i]);
    snprintf(path, sizeof(path), "%s/mods/%s/warrior_error.txt", root(), sWarrior[i]);
    w->problem = stat(path, &st) == 0;
}

/* What went wrong when its files were made (warrior_error.txt), "" nothing. */
void Port_WarriorError(int i, uint32_t outAddr, int size) {
    char *out = (char *)(uintptr_t)outAddr, path[600];
    FILE *fp;

    out[0] = '\0';
    if (i < 0 || i >= sWarriorCount) {
        return;
    }
    snprintf(path, sizeof(path), "%s/mods/%s/warrior_error.txt", root(), sWarrior[i]);
    if ((fp = fopen(path, "r")) != NULL) {
        if (fgets(out, size, fp) == NULL) {
            out[0] = '\0';
        }
        out[strcspn(out, "\r\n")] = '\0';
        fclose(fp);
    }
}

static int warrior_index(const char *folder) {
    int i;

    for (i = 0; i < sWarriorCount; i++) {
        if (strcmp(sWarrior[i], folder) == 0) {
            return i;
        }
    }
    return -1;
}

/* A folder name in mods/ nobody has yet, from `want` (characters a folder cannot have left out). */
static void warrior_folder(const char *want, char *out, size_t size) {
    char base[48], path[600];
    struct stat st;
    int n, j = 0;
    size_t k;

    for (k = 0; want[k] != '\0' && j + 1 < (int)sizeof(base); k++) {
        if (strchr("\\/:*?\"<>|", want[k]) == NULL && (unsigned char)want[k] >= 32) {
            base[j++] = want[k];
        }
    }
    base[j] = '\0';
    while (j > 0 && (base[j - 1] == ' ' || base[j - 1] == '.')) {
        base[--j] = '\0';
    }
    if (j == 0) {
        strcpy(base, "Warrior");
    }
    for (n = 1; n < 1000; n++) {
        if (n == 1) {
            snprintf(out, size, "Warrior - %s", base);
        } else {
            snprintf(out, size, "Warrior - %s (%d)", base, n);
        }
        snprintf(path, sizeof(path), "%s/mods/%s", root(), out);
        if (stat(path, &st) != 0) {
            return;
        }
    }
}

static void warrior_line(FILE *fp, const char *key, int who, int from, int n) {
    if (who == -2) {
        fprintf(fp, "%s=none\n", key);
        return;
    }
    if (warrior_who_name(who) == NULL) {
        return;
    }
    if (n > 0) {
        fprintf(fp, "%s=%s:%d\n", key, warrior_who_name(who), from + 1);
    } else {
        fprintf(fp, "%s=%s\n", key, warrior_who_name(who));
    }
}

/* A fighter's line with the costume it is always (1..4) after it, ", costume N"; 0: as warrior_line. */
static void warrior_line_cos(FILE *fp, const char *key, int who, int cos) {
    if (cos >= 1 && cos <= 4 && warrior_who_name(who) != NULL) {
        fprintf(fp, "%s=%s, costume %d\n", key, warrior_who_name(who), cos);
    } else {
        warrior_line(fp, key, who, 0, 0);
    }
}

/* Writes warrior i (-1: a new one, in a folder of its own) and reads the library again, which makes its files.
   Returns its index afterwards, -1 when it could not be written. */
static int warrior_write(const char *folder, const PortWarrior *w);

/* ---- drawings on the textures (Z Warriors' colours screen: drawn by hand, a picture put on): paint/<key>.bin in the
   warrior's folder, one per texture, named by the texture's picture as made (wc_tex_hash, six hex digits): 'ZWP1', width,
   height, 1 when its 16 colours follow, the colour numbers (a byte a pixel), the colours (RGBA). While the editor is open
   they are in mods/_zw_paint (copied there when a warrior is opened, back when it is saved). ---- */
#include <SDL3/SDL.h>
static int sPaintActive;
static void remove_tree(const char *path, int depth);
static int copy_file(const char *a, const char *b);

static void paint_copy_dir(const char *from, const char *to) { /* (to: emptied, then from's .bin files) */
    DIR *dp;
    struct dirent *e;

    if (is_dir(to)) {
        remove_tree(to, 0);
    }
    if ((dp = opendir(from)) == NULL) {
        return;
    }
    make_dir(to);
    while ((e = readdir(dp)) != NULL) {
        size_t l = strlen(e->d_name);
        char a[900], b[900];

        if (l < 5 || (strcmp(e->d_name + l - 4, ".bin") != 0 && strcmp(e->d_name + l - 4, ".txt") != 0 &&
                      strcmp(e->d_name + l - 4, ".png") != 0)) {
            continue; /* (the drawings, and the picture layers: their lists and pictures) */
        }
        snprintf(a, sizeof(a), "%s/%s", from, e->d_name);
        snprintf(b, sizeof(b), "%s/%s", to, e->d_name);
        copy_file(a, b);
    }
    closedir(dp);
}

/* The editor opens a warrior (folder "": a new one): its drawings to be worked on. */
void Port_PaintBegin(uint32_t folderAddr) {
    const char *folder = (const char *)(uintptr_t)folderAddr;
    char from[600], to[600];

    ensure();
    snprintf(to, sizeof(to), "%s/mods/_zw_paint", root());
    if (folder != NULL && folder[0] != '\0') {
        snprintf(from, sizeof(from), "%s/mods/%s/paint", root(), folder);
        paint_copy_dir(from, to);
        if (!is_dir(to)) {
            make_dir(to);
        }
    } else {
        if (is_dir(to)) {
            remove_tree(to, 0);
        }
        make_dir(to);
    }
    sPaintActive = 1;
}

static void paint_path(int key, char *out, size_t n) {
    snprintf(out, n, "%s/mods/_zw_paint/%06x.bin", root(), (unsigned)(((uint32_t)key >> 5) & 0xFFFFFF));
}

/* Texture `key` (the colours screen's) drawn: its colour numbers idx (w x h), and its 16 colours (RGBA) when pal is not
   0. 1 written. */
int Port_PaintSave(int key, uint32_t idxAddr, int w, int h, uint32_t palAddr) {
    char path[700];
    FILE *fp;
    uint32_t hd[4], oldPal[16];
    int keepPal = 0;

    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) {
        return 0;
    }
    paint_path(key, path, sizeof(path));
    if (palAddr == 0) { /* (no colours given: those the drawing had, a picture's put on) */
        long len = 0;
        uint8_t *old = read_all(path, &len);
        uint32_t oh[4];

        if (old != NULL && len >= 16) {
            memcpy(oh, old, 16);
            if (oh[0] == 0x3150575Au && oh[3] && (long)(16 + oh[1] * oh[2] + 64) <= len) {
                memcpy(oldPal, old + 16 + oh[1] * oh[2], 64);
                keepPal = 1;
            }
        }
        free(old);
    }
    if ((fp = fopen(path, "wb")) == NULL) {
        return 0;
    }
    hd[0] = 0x3150575Au; /* 'ZWP1' */
    hd[1] = (uint32_t)w;
    hd[2] = (uint32_t)h;
    hd[3] = palAddr != 0 || keepPal;
    fwrite(hd, 4, 4, fp);
    fwrite((const void *)(uintptr_t)idxAddr, 1, (size_t)(w * h), fp);
    if (palAddr != 0) {
        fwrite((const void *)(uintptr_t)palAddr, 4, 16, fp);
    } else if (keepPal) {
        fwrite(oldPal, 4, 16, fp);
    }
    fclose(fp);
    return 1;
}

/* Drawing's undo: op 0 the picture idx (w x h) kept before a change, 1 undo (idx becomes the one before; the one now
   kept for redo), 2 redo, 3 all forgotten (another texture). 1 done. */
#define PAINT_UNDO 24
static uint8_t *sUndo[2][PAINT_UNDO];
static int sUndoN[2], sUndoW, sUndoH;

static void paint_undo_drop(int s) {
    while (sUndoN[s] > 0) {
        free(sUndo[s][--sUndoN[s]]);
    }
}

static void paint_undo_put(int s, const uint8_t *idx, int n) {
    uint8_t *c = malloc((size_t)n);

    if (c == NULL) {
        return;
    }
    memcpy(c, idx, (size_t)n);
    if (sUndoN[s] == PAINT_UNDO) { /* (full: the oldest goes) */
        free(sUndo[s][0]);
        memmove(&sUndo[s][0], &sUndo[s][1], sizeof(sUndo[s][0]) * (PAINT_UNDO - 1));
        sUndoN[s]--;
    }
    sUndo[s][sUndoN[s]++] = c;
}

int Port_PaintUndo(int op, uint32_t idxAddr, int w, int h) {
    uint8_t *idx = (uint8_t *)(uintptr_t)idxAddr;
    int n = w * h, from = op == 1 ? 0 : 1;

    if (op == 3 || w != sUndoW || h != sUndoH) {
        paint_undo_drop(0);
        paint_undo_drop(1);
        sUndoW = w;
        sUndoH = h;
        if (op == 3) {
            return 1;
        }
    }
    if (n <= 0 || idx == NULL) {
        return 0;
    }
    if (op == 0) {
        paint_undo_put(0, idx, n);
        paint_undo_drop(1);
        return 1;
    }
    if (sUndoN[from] <= 0) {
        return 0;
    }
    paint_undo_put(1 - from, idx, n);
    memcpy(idx, sUndo[from][sUndoN[from] - 1], (size_t)n);
    free(sUndo[from][--sUndoN[from]]);
    return 1;
}

/* ---- a picture put on a texture as a sticker: read (Port_DecalLoad: the texture as it is kept under it), shown
   where it is placed (Port_DecalShow: the texture with the picture on it, in the texture's own 16 colours, what is
   see-through in the picture left as it was), done (Port_DecalEnd). ---- */
static uint8_t *sDecal, *sDecalBase;
static int sDecalW, sDecalH, sDecalTW, sDecalTH;
static uint32_t sDecalPal[16];

static uint8_t *sDecalOver;

void Port_DecalEnd(void) {
    free(sDecal);
    free(sDecalBase);
    free(sDecalOver);
    sDecal = sDecalBase = sDecalOver = NULL;
}

/* Returns the picture's width | height << 16, 0 when it cannot be read. */
int Port_DecalLoad(uint32_t pathAddr, uint32_t idxAddr, int w, int h, uint32_t palAddr) {
    SDL_Surface *src, *c;
    int y;

    Port_DecalEnd();
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024 || (src = SDL_LoadSurface((const char *)(uintptr_t)pathAddr)) == NULL) {
        return 0;
    }
    c = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(src);
    if (c == NULL || c->w <= 0 || c->h <= 0 || c->w > 4096 || c->h > 4096) {
        if (c != NULL) {
            SDL_DestroySurface(c);
        }
        return 0;
    }
    sDecalW = c->w;
    sDecalH = c->h;
    sDecal = malloc((size_t)c->w * (size_t)c->h * 4);
    sDecalBase = malloc((size_t)(w * h));
    if (sDecal == NULL || sDecalBase == NULL) {
        SDL_DestroySurface(c);
        Port_DecalEnd();
        return 0;
    }
    for (y = 0; y < c->h; y++) {
        memcpy(sDecal + (size_t)y * (size_t)c->w * 4, (const uint8_t *)c->pixels + (size_t)y * (size_t)c->pitch, (size_t)c->w * 4);
    }
    SDL_DestroySurface(c);
    memcpy(sDecalBase, (const void *)(uintptr_t)idxAddr, (size_t)(w * h));
    memcpy(sDecalPal, (const void *)(uintptr_t)palAddr, sizeof(sDecalPal));
    sDecalTW = w;
    sDecalTH = h;
    return sDecalW | sDecalH << 16;
}

/* Picture img (iw x ih RGBA) stamped on a texture's colour numbers idx (tw x th) in the colours pal: its middle at
   (cx, cy), sw texture pixels across, turned `turn` quarter turns; see-through where the picture is. */
static void decal_stamp(uint8_t *idx, int tw, int th, const uint8_t *img, int iw, int ih, const uint32_t *pal, float cx, float cy, float sw,
                        int turn);

/* (sDecalOver: the drawing made over the picture layers, kept on top while one is edited: 0xFF none) */

/* The texture (into idxAddr) with the picture on it: its middle at (cx, cy), `size` texture pixels across (hundredths),
   turned `turn` quarter turns. */
void Port_DecalShow(int cx100, int cy100, int size100, int turn, uint32_t idxAddr) {
    uint8_t *idx = (uint8_t *)(uintptr_t)idxAddr;
    int i;

    if (sDecal == NULL || sDecalBase == NULL) {
        return;
    }
    memcpy(idx, sDecalBase, (size_t)(sDecalTW * sDecalTH));
    decal_stamp(idx, sDecalTW, sDecalTH, sDecal, sDecalW, sDecalH, sDecalPal, (float)cx100 / 100.0f, (float)cy100 / 100.0f,
                (float)size100 / 100.0f, turn);
    for (i = 0; sDecalOver != NULL && i < sDecalTW * sDecalTH; i++) {
        if (sDecalOver[i] != 0xFF) {
            idx[i] = sDecalOver[i];
        }
    }
}

static void decal_stamp(uint8_t *idx, int tw, int th, const uint8_t *img, int iw, int ih, const uint32_t *pal, float cx, float cy, float sw,
                        int turn) {
    float sh;
    int x, y, x0, y0, x1, y1, q;
    int solid[16];
    int sDecalTW = tw, sDecalTH = th, sDecalW = iw, sDecalH = ih;
    const uint8_t *sDecal = img;
    const uint32_t *sDecalPal = pal;

    sw = sw < 1.0f ? 1.0f : sw;
    sh = sw * (float)sDecalH / (float)sDecalW;
    if (turn & 1) { /* (on its side: across and down change places) */
        float t = sw;

        sw = sh;
        sh = t;
    }
    for (q = 0; q < 16; q++) {
        solid[q] = (sDecalPal[q] >> 24) >= 0x40;
    }
    x0 = (int)(cx - sw / 2.0f);
    y0 = (int)(cy - sh / 2.0f);
    x1 = (int)(cx + sw / 2.0f + 1.0f);
    y1 = (int)(cy + sh / 2.0f + 1.0f);
    for (y = y0 < 0 ? 0 : y0; y < y1 && y < sDecalTH; y++) {
        for (x = x0 < 0 ? 0 : x0; x < x1 && x < sDecalTW; x++) {
            float u = ((float)x + 0.5f - (cx - sw / 2.0f)) / sw, v = ((float)y + 0.5f - (cy - sh / 2.0f)) / sh, t;
            int px, py, best = -1, bd = 1 << 30;
            const uint8_t *c;

            if (u < 0.0f || v < 0.0f || u >= 1.0f || v >= 1.0f) {
                continue;
            }
            switch (turn & 3) { /* (the picture's own place for this pixel) */
            case 1: t = u; u = v; v = 1.0f - t; break;
            case 2: u = 1.0f - u; v = 1.0f - v; break;
            case 3: t = u; u = 1.0f - v; v = t; break;
            }
            px = (int)(u * (float)sDecalW);
            py = (int)(v * (float)sDecalH);
            px = px < 0 ? 0 : px >= sDecalW ? sDecalW - 1 : px;
            py = py < 0 ? 0 : py >= sDecalH ? sDecalH - 1 : py;
            c = sDecal + ((size_t)py * (size_t)sDecalW + (size_t)px) * 4;
            if (c[3] < 128) {
                continue; /* (see-through: the texture as it was) */
            }
            for (q = 0; q < 16; q++) { /* (the texture's own colour nearest to it) */
                int dr = (int)(sDecalPal[q] & 0xFF) - c[0], dg = (int)((sDecalPal[q] >> 8) & 0xFF) - c[1], db = (int)((sDecalPal[q] >> 16) & 0xFF) - c[2];
                int d = dr * dr * 3 + dg * dg * 4 + db * db * 2;

                if (solid[q] && d < bd) {
                    bd = d;
                    best = q;
                }
            }
            if (best >= 0) {
                idx[y * sDecalTW + x] = (uint8_t)best;
            }
        }
    }
}

/* ---- picture layers (Z Warriors' textures): a picture put on a texture stays a layer that can be moved, sized, turned
   or taken off later. In the paint folder beside the drawing <hash>.bin: <hash>_base.bin the drawing before the first
   picture (as Port_PaintSave writes one), <hash>_layers.txt a line per picture (its file, middle x and y and width in
   hundredths of texture pixels, quarter turns; the last on top), <hash>_L<n>.png the pictures. What is drawn over them
   later is kept on top when one is edited. ---- */
#define LAYERS_MAX 16
static int png_write(const char *path, const uint8_t *rgba, int w, int h);
typedef struct PaintLayer {
    char file[64];
    int x, y, w, turn;
} PaintLayer;

static void layer_path(int key, const char *what, char *out, size_t n) {
    snprintf(out, n, "%s/mods/_zw_paint/%06x%s", root(), (unsigned)(((uint32_t)key >> 5) & 0xFFFFFF), what);
}

static int layers_read(int key, PaintLayer *l) {
    char path[700], line[256];
    FILE *fp;
    int n = 0;

    layer_path(key, "_layers.txt", path, sizeof(path));
    if ((fp = fopen(path, "r")) == NULL) {
        return 0;
    }
    while (n < LAYERS_MAX && fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "%63s %d %d %d %d", l[n].file, &l[n].x, &l[n].y, &l[n].w, &l[n].turn) == 5) {
            n++;
        }
    }
    fclose(fp);
    return n;
}

static void layers_write(int key, const PaintLayer *l, int n) {
    char path[700];
    FILE *fp;
    int i;

    layer_path(key, "_layers.txt", path, sizeof(path));
    if (n <= 0) {
        remove(path);
        return;
    }
    if ((fp = fopen(path, "w")) == NULL) {
        return;
    }
    for (i = 0; i < n; i++) {
        fprintf(fp, "%s %d %d %d %d\n", l[i].file, l[i].x, l[i].y, l[i].w, l[i].turn & 3);
    }
    fclose(fp);
}

/* A layer's picture as RGBA (malloc'ed). */
static uint8_t *layer_image(const char *file, int *w, int *h) {
    char path[760];
    SDL_Surface *s, *c;
    uint8_t *out;
    int y;

    snprintf(path, sizeof(path), "%s/mods/_zw_paint/%s", root(), file);
    if ((s = SDL_LoadSurface(path)) == NULL) {
        return NULL;
    }
    c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(s);
    if (c == NULL) {
        return NULL;
    }
    out = malloc((size_t)c->w * (size_t)c->h * 4);
    for (y = 0; out != NULL && y < c->h; y++) {
        memcpy(out + (size_t)y * c->w * 4, (const uint8_t *)c->pixels + (size_t)y * c->pitch, (size_t)c->w * 4);
    }
    *w = c->w;
    *h = c->h;
    SDL_DestroySurface(c);
    return out;
}

/* The drawing under the pictures (w x h colour numbers): 1 read. */
static int layer_base(int key, uint8_t *idx, int w, int h) {
    char path[700];
    long len;
    uint8_t *d;
    uint32_t hd[4];
    int x, y;

    layer_path(key, "_base.bin", path, sizeof(path));
    if ((d = read_all(path, &len)) == NULL) {
        return 0;
    }
    memcpy(hd, d, len >= 16 ? 16 : 0);
    if (len < 16 || hd[0] != 0x3150575Au || hd[1] == 0 || hd[2] == 0 || (long)(16 + hd[1] * hd[2]) > len) {
        free(d);
        return 0;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            idx[y * w + x] = d[16 + (y * (int)hd[2] / h) * (int)hd[1] + x * (int)hd[1] / w];
        }
    }
    free(d);
    return 1;
}

/* The base with layers [0, n) but `skip` stamped (into out). */
static void layers_compose(const PaintLayer *l, int n, int skip, uint8_t *out, int w, int h, const uint32_t *pal) {
    int i;

    for (i = 0; i < n; i++) {
        int iw, ih;
        uint8_t *img;

        if (i == skip || (img = layer_image(l[i].file, &iw, &ih)) == NULL) {
            continue;
        }
        decal_stamp(out, w, h, img, iw, ih, pal, (float)l[i].x / 100.0f, (float)l[i].y / 100.0f, (float)l[i].w / 100.0f, l[i].turn);
        free(img);
    }
}

/* How many pictures texture `key` has as layers. */
int Port_LayerCount(int key) {
    static PaintLayer l[LAYERS_MAX];

    return layers_read(key, l);
}

/* Picture layer i of texture `key` taken up to be placed again (Port_DecalShow / Port_DecalEnd then work on it; idx w x h
   the texture as it is, pal its colours): out = {x, y, width (hundredths), turn, its width | height << 16}. 1 done. */
int Port_LayerEdit(int key, int i, uint32_t idxAddr, int w, int h, uint32_t palAddr, uint32_t outAddr) {
    static PaintLayer l[LAYERS_MAX];
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    uint8_t *idx = (uint8_t *)(uintptr_t)idxAddr, *all;
    int n = layers_read(key, l), k, iw, ih;

    Port_DecalEnd();
    if (i < 0 || i >= n || w <= 0 || h <= 0 || w > 1024 || h > 1024) {
        return 0;
    }
    sDecalBase = malloc((size_t)(w * h));
    sDecalOver = malloc((size_t)(w * h));
    all = malloc((size_t)(w * h));
    if (sDecalBase == NULL || sDecalOver == NULL || all == NULL || !layer_base(key, sDecalBase, w, h) ||
        (sDecal = layer_image(l[i].file, &iw, &ih)) == NULL) {
        free(all);
        Port_DecalEnd();
        return 0;
    }
    memcpy(sDecalPal, (const void *)(uintptr_t)palAddr, sizeof(sDecalPal));
    memcpy(all, sDecalBase, (size_t)(w * h));
    layers_compose(l, n, -1, all, w, h, sDecalPal);
    for (k = 0; k < w * h; k++) { /* (drawn over the pictures since: kept on top) */
        sDecalOver[k] = idx[k] != all[k] ? idx[k] : 0xFF;
    }
    free(all);
    layers_compose(l, n, i, sDecalBase, w, h, sDecalPal);
    sDecalW = iw;
    sDecalH = ih;
    sDecalTW = w;
    sDecalTH = h;
    out[0] = l[i].x;
    out[1] = l[i].y;
    out[2] = l[i].w;
    out[3] = l[i].turn;
    out[4] = iw | ih << 16;
    return 1;
}

/* The picture being placed kept as a layer: i its layer (edited: it goes on top), -1 a new one from file pathAddr (its
   texture before it is the drawing under the pictures if there are none yet). 1 done. */
int Port_LayerKeep(int key, int i, uint32_t pathAddr, int x100, int y100, int w100, int turn) {
    static PaintLayer l[LAYERS_MAX];
    int n = layers_read(key, l);
    PaintLayer e;

    if (i >= 0 && i < n) {
        e = l[i];
        memmove(&l[i], &l[i + 1], sizeof(PaintLayer) * (size_t)(n - i - 1));
        n--;
    } else {
        char to[700], base[700];
        const char *from = (const char *)(uintptr_t)pathAddr;
        int k;

        if (n >= LAYERS_MAX || from == NULL || sDecalBase == NULL) {
            return 0;
        }
        for (k = 0; k < 100; k++) { /* (a file name not taken) */
            FILE *fp;

            snprintf(e.file, sizeof(e.file), "%06x_L%d.png", (unsigned)(((uint32_t)key >> 5) & 0xFFFFFF), k);
            snprintf(to, sizeof(to), "%s/mods/_zw_paint/%s", root(), e.file);
            if ((fp = fopen(to, "rb")) == NULL) {
                break;
            }
            fclose(fp);
        }
        if (k == 100 || !png_write(to, sDecal, sDecalW, sDecalH)) {
            return 0;
        }
        layer_path(key, "_base.bin", base, sizeof(base));
        if (n == 0) { /* (the drawing under the first picture) */
            FILE *fp = fopen(base, "wb");
            uint32_t hd[4] = {0x3150575Au, (uint32_t)sDecalTW, (uint32_t)sDecalTH, 0};

            if (fp != NULL) {
                fwrite(hd, 4, 4, fp);
                fwrite(sDecalBase, 1, (size_t)(sDecalTW * sDecalTH), fp);
                fclose(fp);
            }
        }
    }
    e.x = x100;
    e.y = y100;
    e.w = w100;
    e.turn = turn & 3;
    l[n++] = e;
    layers_write(key, l, n);
    return 1;
}

/* Picture layer i taken off texture `key`: the texture (idx, w x h, colours pal) made again without it, the drawing
   over the pictures kept. 1 done. */
int Port_LayerRemove(int key, int i, uint32_t idxAddr, int w, int h, uint32_t palAddr) {
    static PaintLayer l[LAYERS_MAX];
    uint8_t *idx = (uint8_t *)(uintptr_t)idxAddr, *all, *base;
    int n = layers_read(key, l), k;
    char path[760];

    if (i < 0 || i >= n || w <= 0 || h <= 0 || w > 1024 || h > 1024) {
        return 0;
    }
    all = malloc((size_t)(w * h));
    base = malloc((size_t)(w * h));
    if (all == NULL || base == NULL || !layer_base(key, base, w, h)) {
        free(all);
        free(base);
        return 0;
    }
    memcpy(all, base, (size_t)(w * h));
    layers_compose(l, n, -1, all, w, h, (const uint32_t *)(uintptr_t)palAddr);
    layers_compose(l, n, i, base, w, h, (const uint32_t *)(uintptr_t)palAddr);
    for (k = 0; k < w * h; k++) {
        idx[k] = idx[k] != all[k] ? idx[k] : base[k];
    }
    free(all);
    free(base);
    snprintf(path, sizeof(path), "%s/mods/_zw_paint/%s", root(), l[i].file);
    remove(path);
    memmove(&l[i], &l[i + 1], sizeof(PaintLayer) * (size_t)(n - i - 1));
    layers_write(key, l, n - 1);
    if (n - 1 == 0) {
        layer_path(key, "_base.bin", path, sizeof(path));
        remove(path);
    }
    return 1;
}

/* Texture `key` as it was made (its drawing gone, and its picture layers). */
void Port_PaintClear(int key) {
    static PaintLayer l[LAYERS_MAX];
    char path[760];
    int n = layers_read(key, l), i;

    paint_path(key, path, sizeof(path));
    remove(path);
    for (i = 0; i < n; i++) {
        snprintf(path, sizeof(path), "%s/mods/_zw_paint/%s", root(), l[i].file);
        remove(path);
    }
    layers_write(key, l, 0);
    layer_path(key, "_base.bin", path, sizeof(path));
    remove(path);
}

/* Picture file pathAddr put on texture `key` (w x h): fitted to it, in 16 colours of its own (the most used ones,
   merged), see-through where it is. Its colour numbers and colours to idxAddr / palAddr too. 1 done. */
int Port_PaintImport(int key, uint32_t pathAddr, int w, int h, uint32_t idxAddr, uint32_t palAddr) {
    const char *file = (const char *)(uintptr_t)pathAddr;
    uint8_t *idx = (uint8_t *)(uintptr_t)idxAddr;
    uint32_t *pal = (uint32_t *)(uintptr_t)palAddr;
    SDL_Surface *src, *c;
    static uint32_t px[1024 * 1024];
    static int hist[4096];
    int cw, ch, x, y, i, j, n = 0, clear = 0, cen[16][3], cnt[16];

    if (w <= 0 || h <= 0 || w > 1024 || h > 1024 || (src = SDL_LoadSurface(file)) == NULL) {
        return 0;
    }
    c = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(src);
    if (c == NULL) {
        return 0;
    }
    cw = c->w;
    ch = c->h;
    for (y = 0; y < h; y++) { /* (fitted: the picture stretched to the texture, each pixel the average under it) */
        for (x = 0; x < w; x++) {
            int x0 = x * cw / w, x1 = (x + 1) * cw / w, y0 = y * ch / h, y1 = (y + 1) * ch / h, sum[4] = {0, 0, 0, 0}, k = 0, xx, yy;

            x1 = x1 <= x0 ? x0 + 1 : x1;
            y1 = y1 <= y0 ? y0 + 1 : y1;
            for (yy = y0; yy < y1 && yy < ch; yy++) {
                const uint8_t *row = (const uint8_t *)c->pixels + (size_t)yy * (size_t)c->pitch;

                for (xx = x0; xx < x1 && xx < cw; xx++) {
                    sum[0] += row[xx * 4], sum[1] += row[xx * 4 + 1], sum[2] += row[xx * 4 + 2], sum[3] += row[xx * 4 + 3], k++;
                }
            }
            k = k < 1 ? 1 : k;
            px[y * w + x] = (uint32_t)(sum[0] / k) | (uint32_t)(sum[1] / k) << 8 | (uint32_t)(sum[2] / k) << 16 | (uint32_t)(sum[3] / k) << 24;
        }
    }
    SDL_DestroySurface(c);
    memset(hist, 0, sizeof(hist));
    for (i = 0; i < w * h; i++) {
        if ((px[i] >> 24) < 128) {
            clear = 1;
            continue;
        }
        hist[((px[i] >> 4) & 15) | ((px[i] >> 8) & 0xF0) | ((px[i] >> 12) & 0xF00)]++;
    }
    n = clear ? 15 : 16; /* (the colours: the most used of 4096 boxes, then a few rounds of k-means on the pixels) */
    for (i = 0; i < n; i++) {
        int best = -1;

        for (j = 0; j < 4096; j++) {
            if (hist[j] > 0 && (best < 0 || hist[j] > hist[best])) {
                best = j;
            }
        }
        if (best < 0) {
            break;
        }
        cen[i][0] = (best & 15) * 16 + 8, cen[i][1] = ((best >> 4) & 15) * 16 + 8, cen[i][2] = ((best >> 8) & 15) * 16 + 8;
        for (j = 0; j < 4096; j++) { /* (its near boxes taken too: other colours, not shades of one) */
            int dr = (j & 15) - (best & 15), dg = ((j >> 4) & 15) - ((best >> 4) & 15), db = ((j >> 8) & 15) - ((best >> 8) & 15);

            if (dr * dr + dg * dg + db * db <= 2) {
                hist[j] = 0;
            }
        }
    }
    n = i < 1 ? 1 : i;
    if (i == 0) {
        cen[0][0] = cen[0][1] = cen[0][2] = 128;
    }
    for (j = 0; j < 6; j++) {
        static int sum[16][3];

        memset(sum, 0, sizeof(sum));
        memset(cnt, 0, sizeof(cnt));
        for (i = 0; i < w * h; i++) {
            int r = (int)(px[i] & 0xFF), g = (int)((px[i] >> 8) & 0xFF), b = (int)((px[i] >> 16) & 0xFF), q, best = 0, bd = 1 << 30;

            if ((px[i] >> 24) < 128) {
                continue;
            }
            for (q = 0; q < n; q++) {
                int d = (r - cen[q][0]) * (r - cen[q][0]) + (g - cen[q][1]) * (g - cen[q][1]) + (b - cen[q][2]) * (b - cen[q][2]);

                if (d < bd) {
                    bd = d;
                    best = q;
                }
            }
            sum[best][0] += r, sum[best][1] += g, sum[best][2] += b, cnt[best]++;
        }
        for (i = 0; i < n; i++) {
            if (cnt[i] > 0) {
                cen[i][0] = sum[i][0] / cnt[i], cen[i][1] = sum[i][1] / cnt[i], cen[i][2] = sum[i][2] / cnt[i];
            }
        }
    }
    for (i = 0; i < 16; i++) { /* (colour 15 see-through when the picture has see-through parts) */
        int q = i < n ? i : n - 1;

        pal[i] = (uint32_t)cen[q][0] | (uint32_t)cen[q][1] << 8 | (uint32_t)cen[q][2] << 16 | 0x80000000u;
    }
    if (clear) {
        pal[15] = 0;
    }
    for (i = 0; i < w * h; i++) {
        int r = (int)(px[i] & 0xFF), g = (int)((px[i] >> 8) & 0xFF), b = (int)((px[i] >> 16) & 0xFF), q, best = 0, bd = 1 << 30;

        if (clear && (px[i] >> 24) < 128) {
            idx[i] = 15;
            continue;
        }
        for (q = 0; q < n; q++) {
            int d = (r - cen[q][0]) * (r - cen[q][0]) + (g - cen[q][1]) * (g - cen[q][1]) + (b - cen[q][2]) * (b - cen[q][2]);

            if (d < bd) {
                bd = d;
                best = q;
            }
        }
        idx[i] = (uint8_t)best;
    }
    return Port_PaintSave(key, idxAddr, w, h, palAddr);
}

/* ---- costumes of its own: costumeN/warrior.txt in the warrior's folder (its look's fields; N 2..4) ---- */
static const PortWarrior *sSaveCos; /* (the next Port_WarriorSave's costumes, cos[1..3] where mask has them) */
static int sSaveCosMask;
static void remove_tree(const char *path, int depth);

/* The Z Warriors eyedropper's picture (colour numbers idx, w x h, 16 colours pal) as TBO_PICTURE (gs/tbo_text.c),
   each pixel a square of f x f so it stays sharp drawn large. 1 done. */
int PortTbo_SetPicture(const unsigned int *rgba, int w, int h);

int Port_PictureIndexed(uint32_t idxAddr, int w, int h, uint32_t palAddr) {
    const uint8_t *idx = (const uint8_t *)(uintptr_t)idxAddr;
    const uint32_t *pal = (const uint32_t *)(uintptr_t)palAddr;
    static uint32_t *buf;
    int f, x, y, i, ok;

    if (w <= 0 || h <= 0 || w > 1024 || h > 1024) {
        return 0;
    }
    f = 1024 / w;
    f = 1024 / h < f ? 1024 / h : f;
    f = f < 1 ? 1 : f > 8 ? 8 : f;
    if (buf == NULL && (buf = malloc(1024 * 1024 * 4)) == NULL) {
        return 0;
    }
    for (y = 0; y < h; y++) {
        uint32_t *row = buf + (size_t)(y * f) * (size_t)(w * f);

        for (x = 0; x < w; x++) {
            uint32_t c = pal[idx[y * w + x] & 15];

            for (i = 0; i < f; i++) {
                row[x * f + i] = c;
            }
        }
        for (i = 1; i < f; i++) {
            memcpy(row + (size_t)i * (size_t)(w * f), row, (size_t)(w * f) * 4);
        }
    }
    ok = PortTbo_SetPicture(buf, w * f, h * f);
    return ok;
}

void Port_WarriorCostumesNext(uint32_t cosAddr, int mask) {
    sSaveCos = (const PortWarrior *)(uintptr_t)cosAddr;
    sSaveCosMask = mask;
}

int Port_WarriorCostumeRead(uint32_t folderAddr, int k, uint32_t outAddr) {
    const char *folder = (const char *)(uintptr_t)folderAddr;
    PortWarrior *out = (PortWarrior *)(uintptr_t)outAddr;
    static PortWarrior w;
    char sub[160];

    if (k < 1 || k > 3 || folder[0] == '\0') {
        return 0;
    }
    snprintf(sub, sizeof(sub), "%s/costume%d", folder, k + 1);
    warrior_read(sub, &w);
    if (w.body < 0) {
        return 0;
    }
    out->body = w.body; /* (its look only) */
    out->bodyCostume = w.bodyCostume;
    memcpy(out->part, w.part, sizeof(w.part));
    memcpy(out->partCostume, w.partCostume, sizeof(w.partCostume));
    memcpy(out->layer, w.layer, sizeof(w.layer));
    memcpy(out->layerCostume, w.layerCostume, sizeof(w.layerCostume));
    memcpy(out->size, w.size, sizeof(w.size));
    out->colours = w.colours;
    memcpy(out->colourKey, w.colourKey, sizeof(w.colourKey));
    memcpy(out->colourRgb, w.colourRgb, sizeof(w.colourRgb));
    memcpy(out->colourPart, w.colourPart, sizeof(w.colourPart));
    out->edits = w.edits;
    memcpy(out->edit, w.edit, sizeof(w.edit));
    memcpy(out->xlayer, w.xlayer, sizeof(w.xlayer));
    memcpy(out->xslot, w.xslot, sizeof(w.xslot));
    memcpy(out->xcos, w.xcos, sizeof(w.xcos));
    return 1;
}

int Port_WarriorSave(int i, uint32_t inAddr) {
    const PortWarrior *w = (const PortWarrior *)(uintptr_t)inAddr;
    char folder[64], path[600];

    ensure();
    if (i >= 0 && i < sWarriorCount) {
        snprintf(folder, sizeof(folder), "%s", sWarrior[i]);
    } else {
        warrior_folder(w->name[0] != '\0' ? w->name : "Warrior", folder, sizeof(folder));
        snprintf(path, sizeof(path), "%s/mods/%s", root(), folder);
        make_dir(path);
    }
    if (sPaintActive) { /* (its drawings: what the editor holds, in paint/ of its folder) */
        char from[600], to[600];

        snprintf(from, sizeof(from), "%s/mods/_zw_paint", root());
        snprintf(to, sizeof(to), "%s/mods/%s/paint", root(), folder);
        paint_copy_dir(from, to);
    }
    if (sSaveCos != NULL) { /* (its costumes first: warrior.txt written last, so it is the newest and the files are made again) */
        int k;

        for (k = 1; k < 4; k++) {
            char sub[160];

            snprintf(sub, sizeof(sub), "%s/costume%d", folder, k + 1);
            snprintf(path, sizeof(path), "%s/mods/%s", root(), sub);
            if ((sSaveCosMask >> k) & 1) {
                make_dir(path);
                warrior_write(sub, &sSaveCos[k]);
            } else if (is_dir(path)) {
                remove_tree(path, 0);
            }
        }
        sSaveCos = NULL;
    }
    { /* (its forms' aura colours: into those warriors, which are made again) */
        int f;

        for (f = 0; f < 4; f++) {
            static PortWarrior tw;

            if (1 || w->formTo[f][0] == '\0' || w->formAura[f] == 0 || strcmp(w->formTo[f], folder) == 0) {
                continue; /* (the form's colour is its transformation's aura now: kept in this warrior, not in that one) */
            }
            warrior_read(w->formTo[f], &tw);
            if (tw.body >= 0 && tw.auraColour != w->formAura[f]) {
                tw.auraColour = w->formAura[f];
                warrior_write(w->formTo[f], &tw);
            }
        }
    }
    if (!warrior_write(folder, w)) {
        return -1;
    }
    Port_ModRescan();
    return warrior_index(folder);
}

/* The editor's live preview: the warrior as it is being edited, made in mods/_zw_preview (never listed, never switched
   on) whatever was made there before. Returns that mod's index in the library (Port_ModPreview shows it), -1 none. */
static void remove_tree(const char *path, int depth);

/* ---- the editor's preview: made without its colours, then painted the way the builder paints (warrior_model) into
   its model file, which the screen reads again. A change of colours only paints again (Port_WarriorPreviewPaint),
   quickly: what is on the screen is always a made model, never one painted in memory (the eyes' and mouths' sets
   are not sent to the screen again, the game's numbers in the palettes were lost: those ways are gone). */
static uint8_t *sPrevBase;  /* the preview's first model file as made without colours */
static long sPrevBaseLen;
static char sPrevPath[700];
static PortWarrior sPrevSnap; /* the warrior it was made from, its colours left out */

static void warrior_no_colours(PortWarrior *w) {
    w->colours = 0;
    memset(w->colourKey, 0, sizeof(w->colourKey));
    memset(w->colourRgb, 0, sizeof(w->colourRgb));
    memset(w->colourPart, 0, sizeof(w->colourPart));
}

/* The preview's model file painted with warrior w's colours (from its colourless copy). 1 written. */
static int warrior_preview_paint(const PortWarrior *w) {
    static const uint8_t *part[WARRIOR_PARTS];
    static uint32_t size[WARRIOR_PARTS];
    uint8_t *own[WARRIOR_PARTS];
    WarriorPak pk;
    uint32_t n, i;
    int ok, groups[WP_SPLIT_GROUPS], ng, keys[WARRIOR_COLOURS], parts[WARRIOR_COLOURS], c;
    uint32_t rgbs[WARRIOR_COLOURS];
    const int32_t *wi = (const int32_t *)w;

    if (sPrevBase == NULL) {
        return 0;
    }
    pk.d = sPrevBase;
    pk.len = sPrevBaseLen;
    memcpy(&pk.n, sPrevBase, 4);
    n = pk.n < WARRIOR_PARTS ? pk.n : WARRIOR_PARTS;
    for (i = 0; i < n; i++) {
        part[i] = warrior_part(&pk, i, &size[i]);
        own[i] = NULL;
    }
    for (c = 0; c < w->colours && c < WARRIOR_COLOURS; c++) {
        keys[c] = w->colourKey[c];
        rgbs[c] = w->colourRgb[c];
        parts[c] = w->colourPart[c];
    }
    ng = warrior_groups_of(wi, groups);
    if (ng > 0 && n > 11) { /* parts with colours of their own: their textures copied first (as the builder does) */
        uint8_t *sm, *st;
        uint32_t stn;

        if (wp_split(part[2], size[2], part[11], size[11], groups, ng, &sm, &st, &stn) == 1) {
            own[2] = sm;
            own[11] = st;
            part[2] = sm;
            part[11] = st;
            size[11] = stn;
        }
    }
    if (w->colours > 0 && n > 16 && size[11] > 0) {
        uint8_t *orig = malloc(size[11]);

        if (own[11] == NULL && (own[11] = malloc(size[11])) != NULL) {
            memcpy(own[11], part[11], size[11]);
        }
        if (orig != NULL && own[11] != NULL) {
            memcpy(orig, own[11], size[11]);
            warrior_recolour(own[11], size[11], part[2], size[2], keys, rgbs, parts, w->colours < WARRIOR_COLOURS ? w->colours : WARRIOR_COLOURS);
            part[11] = own[11];
            for (i = 12; i <= 16; i++) { /* the face's expressions (eyes and brows, mouths) the same way */
                if (size[i] < 0x20 || (own[i] = malloc(size[i])) == NULL) {
                    continue;
                }
                memcpy(own[i], part[i], size[i]);
                warrior_recolour_follow(own[i], size[i], orig, own[11], size[11]);
                part[i] = own[i];
            }
        }
        free(orig);
    }
    if (n > 11 && size[11] > 0) { /* (the drawings being made, then the shading made for it, from the colours painted now) */
        if (own[11] == NULL && (own[11] = malloc(size[11])) != NULL) {
            memcpy(own[11], part[11], size[11]);
        }
        if (own[11] != NULL) {
            char pdir[700];

            snprintf(pdir, sizeof(pdir), "%s/mods/_zw_paint", root());
            warrior_paint_apply(own[11], size[11], pdir);
            wp_shade_regen(own[11], size[11]);
            part[11] = own[11];
            for (i = 12; i <= 16 && i < n; i++) { /* (the face's expressions) */
                if (size[i] < 0x20) {
                    continue;
                }
                if (own[i] == NULL && (own[i] = malloc(size[i])) != NULL) {
                    memcpy(own[i], part[i], size[i]);
                }
                if (own[i] != NULL) {
                    warrior_paint_apply(own[i], size[i], pdir);
                    part[i] = own[i];
                }
            }
        }
    }
    ok = warrior_write_pak(sPrevPath, part, size, n);
    for (i = 0; i < n; i++) {
        free(own[i]);
    }
    fprintf(stderr, "bt3: colours: preview painted (%d colours): %s\n", w->colours, ok ? "written" : "NOT written");
    fflush(stderr);
    return ok;
}

/* How many shading ramps of the last preview found no room (the game has 16 per fighter) and borrowed another's. */
int Port_WarriorRampShort(void);

/* How tall mod i's model is against its template fighter's, in percent (the editor's viewer frames a giant - or a
   warrior made bigger - at the size of a fighter: its camera that much further). 100 when not known. */
int Port_WarriorViewScale(int i) {
    static int cache[MAX_MODS];
    static time_t cacheAt[MAX_MODS];
    static long cacheLen[MAX_MODS];
    static int cacheTo[MAX_MODS]; /* (the preview is one mod for every warrior: its file's size and template tell them apart too) */
    char path[700];
    WarriorPak mp, tp;
    struct stat st;
    long len = 0;
    int r = 100;

    if (i < 0 || i >= sModCount) {
        return 100;
    }
    mod_file(&sMod[i], 0, path, sizeof(path));
    if (path[0] == ' ' || stat(path, &st) != 0) {
        return 100;
    }
    if (cache[i] > 0 && cacheAt[i] == st.st_mtime && cacheLen[i] == (long)st.st_size && cacheTo[i] == target_of(&sMod[i])) { /* (made again: read again) */
        return cache[i];
    }
    memset(&mp, 0, sizeof(mp));
    memset(&tp, 0, sizeof(tp));
    if (path[0] != ' ' && (mp.d = read_all(path, &len)) != NULL && len >= 8) {
        mp.len = len;
        memcpy(&mp.n, mp.d, 4);
        if (target_of(&sMod[i]) >= 0 && warrior_open(&tp, MODEL_FILE + target_of(&sMod[i]) * 10)) {
            float hm = warrior_hip(&mp), ht = warrior_hip(&tp);

            if (hm < -0.5f && ht < -0.5f) {
                r = (int)(hm / ht * 100.0f + 0.5f);
                r = r < 30 ? 30 : r > 400 ? 400 : r;
            }
        }
    }
    free(mp.d);
    free(tp.d);
    cache[i] = r;
    cacheAt[i] = st.st_mtime;
    cacheLen[i] = (long)st.st_size;
    cacheTo[i] = target_of(&sMod[i]);
    return r;
}

int Port_WarriorRampShort(void) {
    return wp_rampShort;
}

int Port_WarriorPreview(uint32_t inAddr) {
    const PortWarrior *w = (const PortWarrior *)(uintptr_t)inAddr;
    static PortWarrior bare;
    char path[600];
    DIR *dp;
    struct dirent *e;

    ensure();
    snprintf(path, sizeof(path), "%s/mods/_zw_preview", root());
    remove_tree(path, 0); /* (all of the last preview goes: another body or moves fighter leaves other files) */
    make_dir(path);
    bare = *w;
    warrior_no_colours(&bare);
    wp_rampShort = 0;
    free(sPrevBase);
    sPrevBase = NULL;
    if (!warrior_write("_zw_preview", &bare)) {
        return -1;
    }
    Port_ModRescan(); /* (made: without colours) */
    sPrevPath[0] = '\0';
    if ((dp = opendir(path)) != NULL) { /* its first model file, kept as it is to be painted */
        while ((e = readdir(dp)) != NULL) {
            size_t l = strlen(e->d_name);

            if (l > 7 && strcmp(e->d_name + l - 7, "_1p.pak") == 0) {
                snprintf(sPrevPath, sizeof(sPrevPath), "%s/%s", path, e->d_name);
                break;
            }
        }
        closedir(dp);
    }
    if (sPrevPath[0] != '\0' && (sPrevBase = read_all(sPrevPath, &sPrevBaseLen)) != NULL) {
        sPrevSnap = bare;
        warrior_preview_paint(w);
    }
    fprintf(stderr, "bt3: colours: preview made (mod %d, %d colours)\n", mod_by_name("_zw_preview"), w->colours);
    fflush(stderr);
    return mod_by_name("_zw_preview");
}

/* Only the colours changed since the preview was made: its model file painted again. 1 done (the screen reads it
   again), 0 the preview must be made again (something else changed). */
int Port_WarriorPreviewPaint(uint32_t inAddr) {
    const PortWarrior *w = (const PortWarrior *)(uintptr_t)inAddr;
    static PortWarrior bare;

    bare = *w;
    warrior_no_colours(&bare);
    if (sPrevBase == NULL || memcmp(&bare, &sPrevSnap, sizeof(bare)) != 0) {
        return 0;
    }
    return warrior_preview_paint(w);
}

/* ---- Z Warriors' colours: the look's textures and the eyedropper ---- */
static uint8_t *sTintTex;   /* the look's texture file painted with the warrior's colours (for the screen's pictures) */
static uint32_t sTintSize;

/* The look's texture file painted as warrior w's colours say; 1 when there is one. */
static int warrior_tinted(const int32_t *w) {
    static int keys[WARRIOR_COLOURS], parts[WARRIOR_COLOURS];
    static uint32_t rgbs[WARRIOR_COLOURS];
    int c, count;

    if (!warrior_look_cached(w)) {
        return 0;
    }
    free(sTintTex);
    if ((sTintTex = malloc(sLookTexSize)) == NULL) {
        return 0;
    }
    memcpy(sTintTex, sLookTex, sLookTexSize);
    sTintSize = sLookTexSize;
    count = w[WARRIOR_W_BODY + 18 + 2];
    count = count < 0 ? 0 : count > WARRIOR_COLOURS ? WARRIOR_COLOURS : count;
    for (c = 0; c < count; c++) {
        keys[c] = w[WARRIOR_W_BODY + 18 + 3 + c];
        rgbs[c] = (uint32_t)w[WARRIOR_W_BODY + 18 + 3 + WARRIOR_COLOURS + c];
        parts[c] = w[WARRIOR_W_CPART + c];
    }
    if (count > 0) {
        warrior_recolour(sTintTex, sTintSize, sLookMdl, sLookMdlSize, keys, rgbs, parts, count);
    }
    {
        char pdir[700];

        snprintf(pdir, sizeof(pdir), "%s/mods/_zw_paint", root());
        warrior_paint_apply(sTintTex, sTintSize, pdir);
    }
    return 1;
}

/* The textures of warrior w's look, for the colours screen: up to `max` records of {key (its whole palette: the
   eyedropper's key with colour 16), its most drawn colour now (0xBBGGRR), width | height << 12 | kind << 24 (1 the
   face texture, 2 the hair's), the parts drawing it}; each picture once, the ones drawn nowhere left out. */
int Port_WarriorTextures(uint32_t warriorAddr, uint32_t outAddr, int max) {
    const int32_t *w = (const int32_t *)(uintptr_t)warriorAddr;
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    static WcTexInfo ti;
    static uint32_t hairKey[32];
    static WpRec recs[WP_MAX_RECS * 2];
    int t, n = 0, i, hairs, nrec, expr = (max & 0x10000) != 0, was = (max & 0x20000) != 0; /* (was: each one's colour as the
                                                                                               fighters have it, unpainted) */

    max &= 0xFFFF;
    if (!warrior_tinted(w)) {
        return 0;
    }
    wc_texinfo(sLookTex, sLookTexSize, sLookMdl, sLookMdlSize, &ti);
    nrec = wp_records(sLookMdl, sLookMdlSize, recs, WP_MAX_RECS * 2);
    hairs = nrec > 0 ? wp_hair_textures(sLookMdl, recs, nrec, hairKey, 32) : 0;
    for (t = 0; t < ti.texCount && n < max; t++) {
        int tw, th, kind;
        uint8_t *ix;
        const uint8_t *pal = (was ? sLookTex : sTintTex) + ti.ent[t].clutOfs * 4 + 0x60;

        if (ti.slots[t] == 0 || ti.hash[t] == 0) {
            continue;
        }
        for (i = 0; i < n && (uint32_t)out[i * 4] != (uint32_t)WT_KEY(ti.hash[t], 16); i++) {
        }
        if (i < n) { /* (the same picture again: one row, drawn where both are) */
            out[i * 4 + 3] |= ti.slots[t];
            continue;
        }
        if ((ix = wc_indices(sLookTex, sLookTexSize, &ti.ent[t], &tw, &th)) == NULL) {
            continue;
        }
        free(ix);
        kind = t == sLookMdl[0x4C] ? 1 : wp_is_hair(hairKey, hairs, ti.ent[t].tbpOfs | (ti.ent[t].cbpOfs << 16)) ? 2 : 0;
        out[n * 4] = WT_KEY(ti.hash[t], 16);
        out[n * 4 + 1] = pal[ti.most[t] * 4] | pal[ti.most[t] * 4 + 1] << 8 | pal[ti.most[t] * 4 + 2] << 16;
        out[n * 4 + 2] = (tw & 0xFFF) | (th & 0xFFF) << 12 | kind << 24;
        out[n * 4 + 3] = ti.slots[t];
        n++;
    }
    if (expr) { /* the face's expression sets (parts 12..16: the eyes shut as it blinks, the hurt face..): kind 3 + set */
        static WpTex ent[WP_MAX_TEX];
        int j, cnt;

        warrior_expr_orig();
        for (j = 0; j < 5; j++) {
            cnt = sExpr[j] != NULL ? wp_textures(sExpr[j], sExprSize[j], ent, WP_MAX_TEX) : 0;
            for (t = 0; t < cnt && n < max; t++) {
                uint32_t h = wc_tex_hash(sExpr[j], sExprSize[j], &ent[t]);
                int tw, th, k, most = 0, hist[16];
                uint8_t *ix;
                const uint8_t *pal;

                if (h == 0 || ent[t].clutOfs == 0 || ent[t].clutOfs * 4 + 0x60 + 64 > sExprSize[j]) {
                    continue;
                }
                for (i = 0; i < n && (uint32_t)out[i * 4] != (uint32_t)WT_KEY(h, 16); i++) {
                }
                if (i < n || (ix = wc_indices(sExpr[j], sExprSize[j], &ent[t], &tw, &th)) == NULL) {
                    continue;
                }
                memset(hist, 0, sizeof(hist));
                for (k = 0; k < tw * th; k++) {
                    hist[ix[k] & 15]++;
                }
                for (k = 1; k < 16; k++) {
                    most = hist[k] > hist[most] ? k : most;
                }
                free(ix);
                pal = sExpr[j] + ent[t].clutOfs * 4 + 0x60;
                out[n * 4] = WT_KEY(h, 16);
                out[n * 4 + 1] = pal[most * 4] | pal[most * 4 + 1] << 8 | pal[most * 4 + 2] << 16;
                out[n * 4 + 2] = (tw & 0xFFF) | (th & 0xFFF) << 12 | (3 + j) << 24;
                out[n * 4 + 3] = 1 << 2; /* (the face's) */
                n++;
            }
        }
    }
    return n;
}

/* The picture of the look's texture `key` names (as Port_WarriorTextures gave it): its colours' numbers, one byte a
   pixel, row after row, into idxAddr (room for 256 x 256); its 16 colours now and the 16 it had (0xAABBGGRR) into
   palAddr. Returns width | height << 16, 0 none. */
int Port_WarriorTexturePicture(uint32_t warriorAddr, int key, uint32_t idxAddr, uint32_t palAddr) {
    const int32_t *w = (const int32_t *)(uintptr_t)warriorAddr;
    uint8_t *idx = (uint8_t *)(uintptr_t)idxAddr;
    uint32_t *pal = (uint32_t *)(uintptr_t)palAddr;
    static WcTexInfo ti;
    int t, k;

    if (!warrior_tinted(w)) {
        return 0;
    }
    wc_texinfo(sLookTex, sLookTexSize, sLookMdl, sLookMdlSize, &ti);
    for (t = 0; t < ti.texCount; t++) {
        int tw, th;
        uint8_t *ix;

        if ((uint32_t)WT_KEY(ti.hash[t], 16) != (uint32_t)((key & ~31) | 16) || (ix = wc_indices(sTintTex, sTintSize, &ti.ent[t], &tw, &th)) == NULL) {
            continue;
        }
        if (tw > 256 || th > 256) {
            free(ix);
            return 0;
        }
        memcpy(idx, ix, (size_t)(tw * th));
        free(ix);
        for (k = 0; k < 16; k++) {
            memcpy(&pal[k], sTintTex + ti.ent[t].clutOfs * 4 + 0x60 + k * 4, 4);
            memcpy(&pal[16 + k], sLookTex + ti.ent[t].clutOfs * 4 + 0x60 + k * 4, 4);
        }
        return tw | th << 16;
    }
    { /* one of the face's expressions */
        static WpTex ent[WP_MAX_TEX];
        int j, cnt;

        warrior_expr_orig();
        for (j = 0; j < 5; j++) {
            cnt = sExpr[j] != NULL ? wp_textures(sExpr[j], sExprSize[j], ent, WP_MAX_TEX) : 0;
            for (t = 0; t < cnt; t++) {
                int tw, th;
                uint8_t *ix, *e;
                char pdir[700];

                if ((uint32_t)WT_KEY(wc_tex_hash(sExpr[j], sExprSize[j], &ent[t]), 16) != (uint32_t)((key & ~31) | 16) ||
                    ent[t].clutOfs == 0 || ent[t].clutOfs * 4 + 0x60 + 64 > sExprSize[j] || (e = malloc(sExprSize[j])) == NULL) {
                    continue;
                }
                memcpy(e, sExpr[j], sExprSize[j]);
                if (sTintTex != NULL && sTintSize == sLookTexSize) {
                    warrior_recolour_follow(e, sExprSize[j], sLookTex, sTintTex, sLookTexSize);
                }
                snprintf(pdir, sizeof(pdir), "%s/mods/_zw_paint", root());
                warrior_paint_apply(e, sExprSize[j], pdir);
                if ((ix = wc_indices(e, sExprSize[j], &ent[t], &tw, &th)) == NULL || tw > 256 || th > 256) {
                    free(ix);
                    free(e);
                    return 0;
                }
                memcpy(idx, ix, (size_t)(tw * th));
                free(ix);
                for (k = 0; k < 16; k++) {
                    memcpy(&pal[k], e + ent[t].clutOfs * 4 + 0x60 + k * 4, 4);
                    memcpy(&pal[16 + k], sExpr[j] + ent[t].clutOfs * 4 + 0x60 + k * 4, 4);
                }
                free(e);
                return tw | th << 16;
            }
        }
    }
    return 0;
}

/* (Z Warriors: a line in the log, its test without hands) */
void Port_ZwLogLine(uint32_t lineAddr) {
    fprintf(stderr, "bt3: %s\n", (const char *)(uintptr_t)lineAddr);
    fflush(stderr);
}

/* (Z Warriors: which model the screen shows, for the log) */
void Port_ZwViewLog(int key, int chara, int mod) {
    fprintf(stderr, "bt3: colours: the screen shows %s (key %x, fighter %d, mod %d)\n",
            key >= 0x30000 ? "the body as the disc has it" : key >= 0x20000 ? "the saved warrior" : key >= 0x10000 ? "the preview" : "another model",
            key, chara, mod);
    fflush(stderr);
}

static void warrior_xl_of(const int32_t *w, int *xl) {
    const PortWarrior *pw = (const PortWarrior *)w;
    int x;

    for (x = 0; x < 4; x++) {
        xl[x] = pw->xlayer[x];
        xl[4 + x] = pw->xlayer[x] >= 0 ? pw->xslot[x] : -1;
        xl[8 + x] = pw->xcos[x];
    }
    xl[12] = -1; /* (the shading fighter: not for the colours screen's look) */
}

/* Writes warrior.txt of mods/<folder>. */
static int warrior_write(const char *folder, const PortWarrior *w) {
    char path[600];
    FILE *fp;
    int k;

    snprintf(path, sizeof(path), "%s/mods/%s/warrior.txt", root(), folder);
    if ((fp = fopen(path, "w")) == NULL) {
        return 0;
    }
    fprintf(fp, "# A custom warrior, made in Z Warriors. The game makes this mod's files from the fighters named here.\n");
    fprintf(fp, "name=%s\n", w->name);
    fprintf(fp, "title=%s\n", w->title);
    warrior_line_cos(fp, "body", w->body, w->bodyCostume);
    warrior_line(fp, "moves", w->moves, 0, 0);
    warrior_line(fp, "effects", w->effects, 0, 0);
    warrior_line(fp, "voice", w->voice, 0, 0);
    warrior_line(fp, "style", w->style, 0, 0);
    warrior_line(fp, "finishers", w->finishers, 0, 0);
    warrior_line(fp, "render", w->render, 0, 0);
    warrior_line(fp, "hud", w->hud, 0, 0);
    warrior_line(fp, "blast1_1", w->skill[0], w->skillFrom[0], 1);
    warrior_line(fp, "blast1_2", w->skill[1], w->skillFrom[1], 1);
    warrior_line(fp, "blast2_1", w->tech[0], w->techFrom[0], 1);
    warrior_line(fp, "blast2_2", w->tech[1], w->techFrom[1], 1);
    warrior_line(fp, "ultimate", w->tech[2], w->techFrom[2], 1);
    warrior_line(fp, "blast2_1fx", w->techFx[0], w->techFxFrom[0], 1);
    warrior_line(fp, "blast2_2fx", w->techFx[1], w->techFxFrom[1], 1);
    warrior_line(fp, "ultimatefx", w->techFx[2], w->techFxFrom[2], 1);
    for (k = 0; k < 4; k++) {
        warrior_line(fp, kWarriorCamKey[k], w->camFrom[k], w->camSlot[k], 1);
    }
    warrior_line(fp, "stance", w->stance, 0, 0);
    warrior_line(fp, "aurafrom", w->aura, 0, 0);
    for (k = 0; k < 9; k++) {
        warrior_line(fp, kWarriorAnimKey[k], k < 6 ? w->anim[k] : w->anim2[k - 6], 0, 0);
    }
    for (k = 0; k < 4; k++) {
        if (w->look[k][0] != '\0' && strchr(w->look[k], '/') == NULL && strchr(w->look[k], '\\') == NULL) {
            fprintf(fp, "%s=%s\n", kWarriorLookKey[k], w->look[k]);
        }
    }
    if (w->damage > 0 && w->damage < 4) {
        fprintf(fp, "damage=%s\n", kWarriorDamage[w->damage]);
    }
    warrior_line(fp, "shadefrom", w->shadeFrom, 0, 0);
    if (w->dynShade == 1 || w->dynShade == 2) {
        fprintf(fp, "dynamic=%s\n", w->dynShade == 1 ? "on" : "off");
    }
    for (k = 0; k < 4; k++) { /* (its transformations) */
        if (w->formTo[k][0] != '\0' && strchr(w->formTo[k], '/') == NULL && strchr(w->formTo[k], '\\') == NULL) {
            fprintf(fp, "form%d=%d,%d,%s\n", k + 1, (int)w->formCost[k], (int)w->formKind[k], w->formTo[k]);
            if (w->formAura[k] != 0) {
                fprintf(fp, "formaura%d=%x\n", k + 1, (unsigned)w->formAura[k]);
            }
        }
    }
    for (k = 0; k < 16; k++) { /* (the stance maker's turns) */
        if (w->pose[k][0] != 0 || w->pose[k][1] != 0 || w->pose[k][2] != 0) {
            fprintf(fp, "pose=%d:%d,%d,%d\n", k, (int)w->pose[k][0], (int)w->pose[k][1], (int)w->pose[k][2]);
        }
    }
    for (k = 0; k < 4; k++) { /* (more layers) */
        if (chara_ok(w->xlayer[k]) && w->xslot[k] >= 0 && w->xslot[k] < 13) {
            if (w->xcos[k] >= 1 && w->xcos[k] <= 4) {
                fprintf(fp, "extra%d=%s:%s, costume %d\n", k + 1, kWarriorXSlot[w->xslot[k]], warrior_who_name(w->xlayer[k]), w->xcos[k]);
            } else {
                fprintf(fp, "extra%d=%s:%s\n", k + 1, kWarriorXSlot[w->xslot[k]], warrior_who_name(w->xlayer[k]));
            }
        }
    }
    warrior_line(fp, "blast1_1fx", w->skillFx[0], w->skillFxFrom[0], 1);
    warrior_line(fp, "blast1_2fx", w->skillFx[1], w->skillFxFrom[1], 1);
    for (k = 0; k < 5; k++) {
        if (w->moveName[k][0]) {
            fprintf(fp, "%s=%.31s\n", kWarriorMoveNameKey[k], w->moveName[k]);
        }
    }
    for (k = 0; k < 3; k++) {
        if (w->techColour[k] > 0) {
            fprintf(fp, "%s=%02x%02x%02x\n", kWarriorTechColourKey[k], w->techColour[k] & 0xFF, w->techColour[k] >> 8 & 0xFF, w->techColour[k] >> 16 & 0xFF);
        }
    }
    if (w->bodyScale >= 50 && w->bodyScale <= 250 && w->bodyScale != 100) {
        fprintf(fp, "bodyscale=%d\n", (int)w->bodyScale);
    }
    for (k = 0; k < 5; k++) { /* (the body's glow while a technique is done) */
        if (w->glow[k] > 0) {
            fprintf(fp, "glow_%s=%02x%02x%02x\n", kWarriorLabKey[k] + 4, w->glow[k] & 0xFF, w->glow[k] >> 8 & 0xFF, w->glow[k] >> 16 & 0xFF);
        }
    }
    for (k = 0; k < 2; k++) {
        if (w->skillColour[k] > 0) {
            fprintf(fp, "blast1_%dcolour=%02x%02x%02x\n", k + 1, w->skillColour[k] & 0xFF, w->skillColour[k] >> 8 & 0xFF, w->skillColour[k] >> 16 & 0xFF);
        }
    }
    if (w->auraColour > 0) {
        fprintf(fp, "aura=%02x%02x%02x\n", w->auraColour & 0xFF, w->auraColour >> 8 & 0xFF, w->auraColour >> 16 & 0xFF);
    }
    for (k = 0; k < 16; k++) { /* (shading by texture) */
        if (w->texShade[k][0] != 0 && w->texShade[k][1] >= 0x1000000) {
            fprintf(fp, "shade_tex=%06x,%02x%02x%02x,%d\n", (unsigned)w->texShade[k][0] & 0xFFFFFF, w->texShade[k][1] & 0xFF, w->texShade[k][1] >> 8 & 0xFF,
                    w->texShade[k][1] >> 16 & 0xFF, (int)((w->texShade[k][1] >> 25) & 15));
        }
    }
    for (k = 0; k < 5; k++) { /* (the Technique Lab) */
        int b, first = 1;

        if (w->labSet[k] == 0) {
            continue;
        }
        fprintf(fp, "%s=", kWarriorLabKey[k]);
        for (b = 0; b < 16; b++) {
            if ((w->labSet[k] >> b) & 1) {
                fprintf(fp, first ? "%d:%d" : ",%d:%d", b, (int)w->lab[k][b]);
                first = 0;
            }
        }
        fprintf(fp, "\n");
    }
    for (k = 0; k < w->edits && k < WARRIOR_EDITS; k++) {
        const int32_t *e = &w->edit[k * WP_EDIT_WORDS];

        fprintf(fp, "edit=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", e[0], e[1], e[2], e[3], e[4], e[5], e[6], e[7], e[8], e[9], e[10], e[11]);
    }
    for (k = 0; k < WP_SLOTS; k++) {
        warrior_line_cos(fp, kWarriorPartKey[k], w->part[k], w->partCostume[k]);
    }
    for (k = 0; k < WP_SLOTS; k++) {
        char key[24];

        snprintf(key, sizeof(key), "+%s", kWarriorPartKey[k]);
        warrior_line_cos(fp, key, w->layer[k], w->layerCostume[k]);
    }
    for (k = 0; k < WP_SIZES; k++) {
        if (w->size[k] > 0 && w->size[k] != 100) {
            fprintf(fp, "%s=%d\n", kWarriorSizeKey[k], w->size[k]);
        }
    }
    for (k = 0; k < w->colours && k < WARRIOR_COLOURS; k++) {
        fprintf(fp, "colour=%d,%u,%u,%u", w->colourKey[k], w->colourRgb[k] & 0xFF, (w->colourRgb[k] >> 8) & 0xFF, (w->colourRgb[k] >> 16) & 0xFF);
        if (w->colourRgb[k] >> 24) { /* (a whole texture's contrast, in percent) */
            fprintf(fp, ",%d,%u\n", w->colourPart[k], w->colourRgb[k] >> 24);
        } else {
            fprintf(fp, w->colourPart[k] != 0 ? ",%d\n" : "\n", w->colourPart[k]);
        }
    }
    return fclose(fp) == 0;
}

/* 1 when mod i is a warrior (the mod grid gives its face a halo). */
int Port_ModIsWarrior(int i) {
    ensure();
    return i >= 0 && i < sModCount && sMod[i].warrior;
}

/* A copy of warrior i under "<name> (copy)"; its index afterwards, -1 none. */
int Port_WarriorCopy(int i) {
    PortWarrior w;

    ensure();
    if (i < 0 || i >= sWarriorCount) {
        return -1;
    }
    warrior_read(sWarrior[i], &w);
    if (strlen(w.name) + 7 < sizeof(w.name)) {
        strcat(w.name, " (copy)");
    }
    return Port_WarriorSave(-1, (uint32_t)(uintptr_t)&w);
}

/* Takes warrior i out (its folder goes to <data>/mods_removed, as the Mod Manager's removing does). */
int Port_WarriorRemove(int i) {
    char from[600], to[600];
    int n, ok = 0;

    ensure();
    if (i < 0 || i >= sWarriorCount) {
        return 0;
    }
    snprintf(from, sizeof(from), "%s/mods/%s", root(), sWarrior[i]);
    snprintf(to, sizeof(to), "%s/mods_removed", root());
    make_dir(to);
    for (n = 0; n < 100 && !ok; n++) {
        struct stat st;

        if (n == 0) {
            snprintf(to, sizeof(to), "%s/mods_removed/%s", root(), sWarrior[i]);
        } else {
            snprintf(to, sizeof(to), "%s/mods_removed/%s (%d)", root(), sWarrior[i], n + 1);
        }
        if (stat(to, &st) != 0) {
            ok = rename(from, to) == 0;
            if (!ok) {
                break;
            }
        }
    }
    Port_ModRescan();
    save();
    return ok;
}

/* The name of a fighter's move in slot 0..4 (Blast 1 x2, Blast 2 x2, Ultimate): its line in the English move list
   of the disc's model file, "" when there is none. */
void Port_FighterMove(int chara, int slot, uint32_t outAddr, int size) {
    char *out = (char *)(uintptr_t)outAddr;
    static char cache[200 + MOD_REF_MAX][5][40];
    static unsigned char have[200 + MOD_REF_MAX];
    WarriorPak p;
    uint32_t n, i, k = 0;
    const uint8_t *e;
    int who = chara;

    out[0] = '\0';
    if (!chara_ok(who) || (who < CHARA_COUNT && who >= 200) || slot < 0 || slot > 4) {
        return;
    }
    chara = is_mod_ref(who) ? 200 + who - MOD_REF_BASE : who; /* (its place in the caches) */
    if (!have[chara]) {
        have[chara] = 1;
        memset(cache[chara], 0, sizeof(cache[chara]));
        if (warrior_open(&p, MODEL_FILE + who * 10) && p.n == WARRIOR_PARTS) {
            e = warrior_part(&p, 44, &n);
            for (i = 2; i + 6 <= n && k < 5; i += 2) { /* (UTF-16 after the byte-order mark; "!F*" and 3 digits, then the name) */
                uint32_t t = i + 4;

                if ((i == 2 || e[i - 2] == '\n') && e[i] == '!' && e[i + 2] == 'F' && t + 4 <= n && e[t] == '$' && e[t + 2] != '0') {
                    break; /* ("!F$1": the melee's names follow) */
                }
                if (t + 8 <= n && e[t] == '&') { /* (a colour first on some fighters' lines: "!F&086*002") */
                    t += 8;
                }
                if ((i == 2 || e[i - 2] == '\n') && e[i] == '!' && e[i + 2] == 'F' && t < n && e[t] == '*') {
                    uint32_t j = t + 8;
                    int c = 0;

                    while (j + 1 < n && e[j] != '\n' && e[j] != 0 && c < 39) {
                        cache[chara][k][c++] = e[j + 1] == 0 && e[j] >= 32 && e[j] < 127 ? (char)e[j] : '?';
                        j += 2;
                    }
                    k++;
                }
                if (e[i] == 0 && e[i + 1] == 0) {
                    break;
                }
            }
        }
        free(p.d);
    }
    snprintf(out, (size_t)size, "%s", cache[chara][slot]);
}

void Port_ModRescan(void) {
    char path[512], line[512];
    DIR *d;
    struct dirent *e;
    FILE *fp;
    int i;

    for (i = 0; i < sModCount; i++) {
        free(sMod[i].chipData);
        sMod[i].chipData = NULL;
    }
    sModCount = 0;
    sWarriorCount = 0;
    sPreview = NULL;
    snprintf(path, sizeof(path), "%s/mods", root());
    d = opendir(path);
    while (d != NULL && (e = readdir(d)) != NULL && sModCount < MAX_MODS) {
        char dir[400];

        if (e->d_name[0] == '.' || strlen(e->d_name) >= sizeof(sMod[0].name)) {
            continue;
        }
        snprintf(dir, sizeof(dir), "%s/%s", path, e->d_name);
        if (!is_dir(dir) || strcmp(e->d_name, "pzs3us1") == 0 || strcmp(e->d_name, "pzs3us0") == 0 || strcmp(e->d_name, "pzs3us2") == 0 ||
            strcmp(e->d_name, "disc") == 0) {
            continue; /* (loose files by number straight in mods/ keep working: plat_file.c) */
        }
        warrior_note(dir, e->d_name);
        warrior_build(dir, e->d_name);
        if (load_mod(&sMod[sModCount], dir, e->d_name)) {
            sMod[sModCount].warrior = sWarriorCount > 0 && strcmp(sWarrior[sWarriorCount - 1], e->d_name) == 0;
            read_settings(&sMod[sModCount]);
            sMod[sModCount].problem = check_files(&sMod[sModCount]);
            sModCount++;
        }
    }
    if (d != NULL) {
        closedir(d);
    }
    qsort(sMod, (size_t)sModCount, sizeof(Mod), by_name);
    warrior_sort();
    snprintf(path, sizeof(path), "%s/mods/enabled.txt", root());
    fp = fopen(path, "r");
    sRankLow = 0;
    if (fp != NULL) {
        int rank = 0;
        while (fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            i = mod_by_name(line);
            if (i >= 0 && !sMod[i].on) {
                sMod[i].on = 1;
                sMod[i].rank = rank++;
            }
        }
        fclose(fp);
    }
    for (i = 0; i < 161; i++) {
        trans_clear(&sVanilla[i]);
    }
    snprintf(path, sizeof(path), "%s/mods/transform.txt", root());
    fp = fopen(path, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp) != NULL) {
            char *f[16];
            int c, k, n = 0, old;
            PortTrans *t;
            line[strcspn(line, "\r\n")] = '\0';
            c = atoi(line);
            if (c < 0 || c >= 161) {
                continue;
            }
            for (f[n++] = line; n < 16 && (f[n] = strchr(f[n - 1], '|')) != NULL; n++) {
                *f[n]++ = '\0';
            }
            t = &sVanilla[c];
            old = n < 2 || f[1][0] != 'T'; /* (the old line: <id>|cost,target x4|next) */
            t->set = old ? 1 : atoi(f[1] + 1);
            for (k = 0; k < 4 && 1 + !old + k < n; k++) {
                char name[64];
                trans_parse(f[1 + !old + k], t, k, name);
                if (name[0] != '\0') {
                    i = mod_by_name(name);
                    t->target[k] = i >= 0 ? REF_MOD + i : -1;
                }
            }
            if (old) {
                t->next = n > 5 ? atoi(f[5]) : -1;
                continue;
            }
            t->next = n > 6 ? atoi(f[6] + 1) : -1;
            t->fuseSet = n > 7 ? atoi(f[7] + 1) : 0;
            for (k = 0; k < 3 && 8 + k < n; k++) {
                char name[64];
                fuse_parse(f[8 + k], t, k, name);
                if (name[0] != '\0') {
                    i = mod_by_name(name);
                    t->fuse[k] = i >= 0 ? REF_MOD + i : -1;
                }
            }
            if (n > 11 && f[11][0] == 'A') {
                sscanf(f[11] + 1, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &t->aura[0], &t->aura[1], &t->aura[2], &t->aura[3], &t->fuseAura[0], &t->fuseAura[1],
                       &t->fuseAura[2], &t->fuseAnim[0], &t->fuseAnim[1], &t->fuseAnim[2]);
            }
        }
        fclose(fp);
    }
    sScanned = 1;
    sModsStamp = mods_stamp();
    rebuild();
}

/* 1 when the mods folder changed since the library was read (a mod copied in or deleted by hand): the manager reads
   it again. Asked now and then, not every frame. */
int Port_ModsChanged(void) {
    return sScanned && mods_stamp() != sModsStamp;
}

/* Online play is between two unmodded games: while an online session runs no mod is used at all (no file of a
   mod, no fighter of the mod grid, no transformation a mod set), so both sides compute the game of the disc. */
extern int Port_NetSession(void); /* gs/net.c */

static void ensure(void) {
    if (!sScanned) {
        char path[600];

        snprintf(path, sizeof(path), "%s/mods", root());
        make_dir(path); /* (a fresh install has none) */
        Port_ModRescan();
    }
}

/* ------------------------------------------------------------------------------------------------- skins */
static Mod *skin_mod(int skin) {
    return skin >= 0 && skin < sSkinCount ? &sMod[sSkin[skin]] : NULL;
}

static int skin_of_mod(int mod) {
    int i;
    for (i = 0; i < sSkinCount; i++) {
        if (sSkin[i] == mod) {
            return i;
        }
    }
    return -1;
}

/* A transformation target of a mod, as a mod index (-1: a fighter of the roster or none). */
static int trans_mod(const Mod *m, int k) {
    return m->tr.set && m->trName[k][0] != '\0' ? mod_by_name(m->trName[k]) : -1;
}

/* The skin a fighter that entered as `skin` is while it is character `chara`: itself as its template, or the
   form of its family (the skins tied together by transformations) whose template that is; -1: none of them. */
static int skin_as(int skin, int chara) {
    int group[MAX_MODS], n = 0, i, k, grew = 1;
    Mod *m = skin_mod(skin);

    if (m == NULL) {
        return -1;
    }
    if (target_of(m) == chara) {
        return skin;
    }
    group[n++] = sSkin[skin];
    while (grew) {
        grew = 0;
        for (i = 0; i < sSkinCount; i++) {
            int mi = sSkin[i], in = 0, link = 0, g;
            for (g = 0; g < n; g++) {
                in |= group[g] == mi;
            }
            if (in) {
                continue;
            }
            for (g = 0; g < n && !link; g++) {
                for (k = 0; k < 4; k++) {
                    link |= trans_mod(&sMod[group[g]], k) == mi || trans_mod(&sMod[mi], k) == group[g];
                }
            }
            if (link) {
                group[n++] = mi;
                grew = 1;
            }
        }
    }
    for (i = 1; i < n; i++) {
        if (target_of(&sMod[group[i]]) == chara) {
            return skin_of_mod(group[i]);
        }
    }
    return -1;
}

int Port_SkinCount(void) {
    ensure();
    return sSkinCount;
}

int Port_SkinMod(int skin) {
    ensure();
    return skin >= 0 && skin < sSkinCount ? sSkin[skin] : -1;
}

/* The costume number the rest of the game should see for a costume code. */
int Port_CostumeOf(int code) {
    return code >= 8 ? (code - 8) & 3 : code;
}

/* The file id of a character file for (character, costume code): kind 0 model (variant: damaged), 8 animations,
   9 effects, 10 portrait, 11 / 12 gauge face. Plain ids for plain costumes. */
int Port_SkinFile(int chara, int code, int kind, int variant) {
    int skin, cos;

    ensure();
    cos = Port_CostumeOf(code);
    skin = code >= 8 ? skin_as((code - 8) / 4, chara) : -1;
    { /* (a modified disc image: no warriors, port/src/plat_fprint.c) */
        extern int Port_ZwDataModified(void);

        if (Port_ZwDataModified()) {
            skin = -1;
        }
    }
    if (kind == KIND_FACE || kind == KIND_FACE + 1) {
        fprintf(stderr, "bt3: gauge face asked: fighter %d, costume code %d, side %d -> %s\n", chara, code, kind - KIND_FACE + 1,
                skin >= 0 ? sMod[sSkin[skin]].name : "the disc's");
        fflush(stderr);
    }
    if (skin < 0) {
        switch (kind) {
        case 0: return MODEL_FILE + chara * 10 + cos + (variant ? 4 : 0);
        case 8: case 9: return MODEL_FILE + chara * 10 + kind;
        case 10: return FACE_FILE + chara;
        default: return DATA_FILE + chara * 2 + (kind - KIND_FACE);
        }
    }
    if (kind == 0) {
        kind = cos + (variant ? 4 : 0);
    }
    return SKIN_FILE + skin * 16 + kind;
}

/* The character a model file id belongs to (the game derives it from the id). */
int Port_CharaOfFile(int id) {
    if (id >= SKIN_FILE) {
        Mod *m = skin_mod((id - SKIN_FILE) / 16);
        return m != NULL ? target_of(m) : 0;
    }
    return (id - MODEL_FILE) / 10;
}

/* ---- the motions of a fusion's scene ----
 * The scene's motions are the model file's own (entries 0x1D..0x24: motions 0x19E..0x1A5): the dance (or the Potara)
 * of the fighter and of the partner its scene shows, the arrival of the result. Only the game's fusing fighters have
 * them. For every fusion set in the manager, whatever of them the three fighters' model files lack is taken from the
 * game's fighters of that scene (fuseAnim: which scene), in copies of their files (<data>/modmgr/cache) served in
 * their place. */
static const int kMotionDonor[5][3] = { /* the fighter, the partner, the result */
    {3, 34, 53},  /* 1 Goku (End) & Vegeta (2nd Form): Super Gogeta */
    {46, 44, 48}, /* 2 Goten & Kid Trunks: Gotenks */
    {10, 38, 54}, /* 3 the SSJ4 pair: Gogeta Super Saiyan 4 */
    {3, 34, 51},  /* 4 the Potara: Vegito */
    {-1, -1, -1},
};
static void trans_get(int who, PortTrans *out);
static int sMotionDonor[161];      /* per fighter of the roster: whose motions it takes, -1 none */
static int sMotionModDonor[MAX_MODS]; /* per mod (a new fighter) */

static void motion_mark(int ref, int donor) {
    if (donor < 0 || ref == donor) {
        return;
    }
    if (ref >= 0 && ref < 161 && sMotionDonor[ref] < 0) {
        sMotionDonor[ref] = donor;
    } else if (ref >= REF_MOD && ref - REF_MOD < sModCount && sMotionModDonor[ref - REF_MOD] < 0) {
        sMotionModDonor[ref - REF_MOD] = donor;
    }
}

static void motion_need(void) {
    int i, k, j;

    if (!sMotionDirty) {
        return;
    }
    sMotionDirty = 0;
    for (i = 0; i < 161; i++) {
        sMotionDonor[i] = -1;
    }
    for (i = 0; i < MAX_MODS; i++) {
        sMotionModDonor[i] = -1;
    }
    for (i = 0; i < 161 + sModCount; i++) {
        int who = i < 161 ? i : REF_MOD + (i - 161);
        PortTrans t;

        if (i < 161 ? !sVanilla[i].fuseSet : !sMod[i - 161].tr.fuseSet) {
            continue;
        }
        trans_get(who, &t);
        for (k = 0; k < 3; k++) {
            int set = t.fuseAnim[k];
            int lead = t.fuseLead[k];

            if (t.fuse[k] < 0 || set == 5) {
                continue;
            }
            if (set <= 0 || set > 4) {
                set = t.fuseType[k] == 2 ? 4 : 1;
            }
            for (j = 0; j < 4 && lead < 0; j++) {
                lead = t.partner[k][j];
            }
            motion_mark(who, kMotionDonor[set - 1][0]);
            motion_mark(lead, kMotionDonor[set - 1][1]);
            motion_mark(t.fuse[k], kMotionDonor[set - 1][2]);
        }
    }
}

static uint8_t *read_all(const char *path, long *len);

/* Where entry e of a pak ends: at the next offset above its start (the table may be out of order), else the end. */
static uint32_t pak_end(const uint8_t *d, uint32_t n, long len, uint32_t start) {
    uint32_t best = (uint32_t)len, k, o;

    for (k = 0; k <= n; k++) {
        memcpy(&o, d + 4 + 4 * k, 4);
        if (o > start && o < best) {
            best = o;
        }
    }
    return best;
}

/* `src` (a model file) with the scene motions it lacks taken from `donor`'s, written to `out`. 1 done. */
static int motion_write(const char *src, int donor, const char *out) {
    char dpath[700];
    long slen, dlen;
    uint8_t *s = read_all(src, &slen), *d, *o;
    uint32_t sn, dn, k, first, at;
    int any = 0, ok = 0;

    snprintf(dpath, sizeof(dpath), "%s/pzs3us1/%05d.bin", root(), MODEL_FILE + donor * 10 - 1);
    d = read_all(dpath, &dlen);
    if (s == NULL || d == NULL || slen < 16 || dlen < 16) {
        free(s);
        free(d);
        return 0;
    }
    memcpy(&sn, s, 4);
    memcpy(&dn, d, 4);
    memcpy(&first, s + 4, 4);
    o = sn > 0x24 && sn < 4096 && dn > 0x24 && dn < 4096 ? malloc((size_t)slen + (size_t)dlen + 0x10000) : NULL;
    if (o != NULL) {
        memset(o, 0, first);
        memcpy(o, &sn, 4);
        at = first;
        for (k = 0; k < sn; k++) {
            uint32_t a, b, a2, b2;
            const uint8_t *from = s;
            long flen = slen;
            uint32_t fn = sn;

            memcpy(&a, s + 4 + 4 * k, 4);
            memcpy(&b, s + 8 + 4 * k, 4);
            if (k >= 0x1D && k <= 0x24 && a == b) { /* (empty here: the donor's, if it has it) */
                memcpy(&a2, d + 4 + 4 * k, 4);
                memcpy(&b2, d + 8 + 4 * k, 4);
                if (a2 != b2) {
                    from = d;
                    flen = dlen;
                    fn = dn;
                    a = a2;
                    b = b2;
                    any = 1;
                }
            }
            memcpy(o + 4 + 4 * k, &at, 4);
            if (a != b && (long)a < flen) {
                uint32_t e = pak_end(from, fn, flen, a);

                memcpy(o + at, from + a, e - a);
                at += e - a;
                at = (at + 63) & ~63u;
            }
        }
        memcpy(o + 4 + 4 * sn, &at, 4);
        if (any) {
            FILE *fp = fopen(out, "wb");

            if (fp != NULL) {
                ok = fwrite(o, 1, at, fp) == at;
                fclose(fp);
            }
        }
    }
    free(o);
    free(s);
    free(d);
    return ok;
}

/* The path that answers model file `fileId` of the fighter (ref: a character id or REF_MOD + mod) whose source is
   `src`: a copy with the scene's motions when it needs some, else `src`. */
static const char *motion_path(const char *src, int fileId, int ref) {
    static char out[2][700];
    static int which;
    char dir[600];
    int donor;
    unsigned h = 5381;
    const char *c;
    struct stat a, b;

    motion_need();
    donor = ref >= 0 && ref < 161 ? sMotionDonor[ref] : ref >= REF_MOD && ref - REF_MOD < sModCount ? sMotionModDonor[ref - REF_MOD] : -1;
    if (donor < 0 || src == NULL) {
        return src;
    }
    for (c = src; *c != '\0'; c++) {
        h = h * 33 + (unsigned char)*c;
    }
    snprintf(dir, sizeof(dir), "%s/modmgr/cache", root());
    make_dir(dir);
    which ^= 1;
    snprintf(out[which], sizeof(out[which]), "%s/m%05d_%03d_%08x.bin", dir, fileId, donor, h);
    if (stat(out[which], &a) == 0 && stat(src, &b) == 0 && a.st_mtime >= b.st_mtime) {
        return out[which];
    }
    return motion_write(src, donor, out[which]) ? out[which] : src;
}

/* For the file layer: the host path that answers a file of an archive, or NULL. */
const char *Port_ModRedirect(const char *rel) {
    static char path[700];

    /* (online too: a match is only played between games whose data and mods are the same, Port_ModsFingerprint) */
    ensure();
    if (strncmp(rel, "pzs3us1/", 8) == 0 && isdigit((unsigned char)rel[8])) {
        int index = atoi(rel + 8);
        if (index + 1 >= MODEL_FILE && index + 1 < MODEL_FILE + 161 * 10 && (index + 1 - MODEL_FILE) % 10 < 8) {
            static char disc[700];
            const char *src = index < MAX_FILES && sRedirect[index] != NULL ? sRedirect[index] : NULL;
            const char *got;

            if (src == NULL) {
                snprintf(disc, sizeof(disc), "%s/pzs3us1/%05d.bin", root(), index);
            }
            got = motion_path(src != NULL ? src : disc, index + 1, (index + 1 - MODEL_FILE) / 10);
            if (got != (src != NULL ? src : disc)) {
                return got;
            }
        }
        if (index >= 0 && index < MAX_FILES && sRedirect[index] != NULL) {
            return sRedirect[index];
        }
    }
    if (strncmp(rel, "pzs3us2/", 8) == 0 && isdigit((unsigned char)rel[8])) {
        int id = atoi(rel + 8) + US2_FIRST;
        if (id >= SKIN_FILE) {
            int kind = (id - SKIN_FILE) % 16;
            Mod *m = skin_mod((id - SKIN_FILE) / 16);
            int t;
            if (m == NULL) {
                return NULL;
            }
            t = target_of(m);
            if (t < 0) {
                t = 0;
            }
            mod_file(m, kind, path, sizeof(path));
            if (kind == 10 && path[0] != '\0' && !portrait_ok(path)) {
                path[0] = '\0';
            }
            if (path[0] == '\0') { /* what the mod does not bring is its template's */
                int orig = kind < 10 ? MODEL_FILE + t * 10 + kind : kind == 10 ? FACE_FILE + t : DATA_FILE + t * 2 + (kind - KIND_FACE);
                snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", root(), orig - 1);
            }
            if (kind < 8) { /* (a model: with a fusion scene's motions, when it needs them) */
                return motion_path(path, id, REF_MOD + (int)(m - sMod));
            }
            if (kind == KIND_FACE || kind == KIND_FACE + 1) {
                fprintf(stderr, "bt3: gauge face of %s, side %d: %s\n", m->name, kind - KIND_FACE + 1, path);
                fflush(stderr);
            }
            return path;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------------------------------- for the game's screens */
int Port_ModCount(void) {
    ensure();
    return sModCount;
}

static void fill_info(const Mod *m, PortModInfo *out, int index) {
    int k, j, t = target_of(m);

    memset(out, 0, sizeof(*out));
    snprintf(out->name, sizeof(out->name), "%s", m->name);
    snprintf(out->target, sizeof(out->target), "%s", t >= 0 && t < CHARA_COUNT ? kName[t] : "FIGHTER NOT SET");
    snprintf(out->white, sizeof(out->white), "%s", m->white);
    snprintf(out->yellow, sizeof(out->yellow), "%s", m->yellow);
    out->named = m->white[0] != '\0' || m->yellow[0] != '\0';
    out->chara = t;
    out->on = m->on;
    out->raw = m->raw;
    out->isNew = m->isNew;
    out->skin = index >= 0 ? skin_of_mod(index) : -1;
    memcpy(out->chip, m->chip, sizeof(out->chip));
    out->art[0] = m->artSet;
    out->art[1] = m->art[0];
    out->art[2] = m->art[1];
    out->art[3] = m->artSet ? m->art[2] : 100;
    out->artH = m->artSet ? m->art[3] : 100;
    for (k = 0; k < KIND_COUNT; k++) {
        char path[700];
        mod_path(m, k, path, sizeof(path));
        if (path[0] != '\0') {
            out->kinds |= 1 << k;
            out->files++;
            if (k < 4) {
                out->costumes++;
            }
            if (k < 8 && file_size(path) > OLD_MODEL_SIZE) {
                out->large = 1;
            }
        }
    }
    out->rival = -1;
    out->wins = 1;
    for (j = 0; j < sModCount && index >= 0; j++) { /* (the rival that wins, if one does) */
        if (j != index && sMod[j].on && clash(m, &sMod[j])) {
            if (out->rival < 0 || (m->on && sMod[j].rank < m->rank)) {
                out->rival = j;
            }
            if (m->on && sMod[j].rank < m->rank) {
                out->wins = 0;
            }
        }
    }
    out->conflict = m->on && out->rival >= 0;
    out->problem = m->problem;
    out->charas = m->charaCount;
    memcpy(out->charaList, m->charaList, sizeof(out->charaList));
    for (j = 0; j < sModCount && index >= 0; j++) {
        if (sMod[j].on) {
            out->onCount++;
            if (m->on && (sMod[j].rank < m->rank || j == index)) {
                out->priority++;
            }
        }
    }
}

/* The maker's lines of mod i's mod.txt: author (32 bytes), version (16), description (200) at outAddr, one after
   the other. */
void Port_ModAbout(int i, uint32_t outAddr) {
    char *out = (char *)(uintptr_t)outAddr;

    memset(out, 0, 248);
    if (i >= 0 && i < sModCount) {
        memcpy(out, sMod[i].author, 32);
        memcpy(out + 32, sMod[i].version, 16);
        memcpy(out + 48, sMod[i].about, 200);
    }
}

/* Moves mod i (on) one place earlier (dir -1: it wins over more) or later (+1) in the order mods win in. */
void Port_ModPriority(int i, int dir) {
    static int order[MAX_MODS];
    int n, k, t;

    if (i < 0 || i >= sModCount || !sMod[i].on) {
        return;
    }
    n = on_order(order);
    for (k = 0; k < n && order[k] != i; k++) {
    }
    if (k + dir < 0 || k + dir >= n) {
        return;
    }
    t = sMod[order[k + dir]].rank;
    sMod[order[k + dir]].rank = sMod[i].rank;
    sMod[i].rank = t;
    if (sMod[i].rank == t && t == sMod[order[k + dir]].rank) { /* (equal ranks: made apart) */
        sMod[i].rank += dir;
    }
    rebuild();
    save();
}

/* Opens mod i's folder (-1: the mods folder) in the PC's file explorer. */
void Port_ModShowFolder(int i) {
#ifdef _WIN32
    char path[700], cmd[800];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    int k;

    if (i >= 0 && i < sModCount) {
        snprintf(path, sizeof(path), "%s", sMod[i].dir);
    } else {
        snprintf(path, sizeof(path), "%s/mods", root());
        make_dir(path);
    }
    if (_fullpath(cmd, path, sizeof(cmd)) != NULL) {
        snprintf(path, sizeof(path), "%s", cmd);
    }
    for (k = 0; path[k] != '\0'; k++) {
        if (path[k] == '/') {
            path[k] = '\\';
        }
    }
    snprintf(cmd, sizeof(cmd), "explorer.exe \"%s\"", path);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
#else
    (void)i;
#endif
}

void Port_ModGetInfo(int i, uint32_t outAddr) {
    PortModInfo *out = (PortModInfo *)(uintptr_t)outAddr;

    ensure();
    if (i < 0 || i >= sModCount) {
        memset(out, 0, sizeof(*out));
        return;
    }
    fill_info(&sMod[i], out, i);
}

/* Switches mod i on or off. One switched on goes first in the order, and the mods that are on and want the same
   files of the same fighter are switched off. Returns the first of those, -1 none. */
int Port_ModToggle(int i) {
    int j, off = -1;

    if (i < 0 || i >= sModCount) {
        return -1;
    }
    sMod[i].on = !sMod[i].on;
    if (sMod[i].on) {
        sMod[i].rank = --sRankLow;
        for (j = 0; j < sModCount; j++) {
            if (sMod[j].on && clash(&sMod[i], &sMod[j])) {
                sMod[j].on = 0;
                if (off < 0) {
                    off = j;
                }
            }
        }
    }
    rebuild();
    save();
    return off;
}

/* The mod whose files answer first whatever is on (the manager's model preview); -1: none, -2: the folder the
   browser looked at last. */
void Port_ModPreview(int i) {
    Mod *m = i == -2 ? &sBrowseMod : i >= 0 && i < sModCount ? &sMod[i] : NULL;
    if (sPreview != m) {
        sPreview = m;
        rebuild();
    }
}

/* field: 0 new (1) or replacing (0), 1 template, 2 target, 3..5 the chip's square */
void Port_ModSet(int i, int field, int value) {
    Mod *m;

    if (i < 0 || i >= sModCount) {
        return;
    }
    m = &sMod[i];
    switch (field) {
    case 0: m->isNew = value != 0; break;
    case 1: m->tmpl = value; break;
    case 2: m->target = value; break;
    default:
        if (field >= 3 && field <= 5) {
            m->chip[field - 3] = value;
            free(m->chipData);
            m->chipData = NULL;
        }
        break;
    }
    write_settings(m);
    rebuild();
    save();
}

/* which: 0 the white name, 1 the yellow one, 2 the author, 3 the version, 4 the description */
void Port_ModSetText(int i, int which, uint32_t strAddr) {
    const char *s = (const char *)(uintptr_t)strAddr;

    if (i < 0 || i >= sModCount) {
        return;
    }
    if (which == 0) {
        snprintf(sMod[i].white, sizeof(sMod[i].white), "%s", s);
    } else if (which == 1) {
        snprintf(sMod[i].yellow, sizeof(sMod[i].yellow), "%s", s);
    } else if (which == 2) {
        snprintf(sMod[i].author, sizeof(sMod[i].author), "%s", s);
    } else if (which == 3) {
        snprintf(sMod[i].version, sizeof(sMod[i].version), "%s", s);
    } else if (which == 4) {
        snprintf(sMod[i].about, sizeof(sMod[i].about), "%s", s);
    }
    write_settings(&sMod[i]);
}

void Port_CharaName(int chara, uint32_t outAddr, int size) {
    char *out = (char *)(uintptr_t)outAddr;
    const char *s = warrior_who_shown(chara);

    snprintf(out, (size_t)size, "%s", s != NULL ? s : "?");
}

/* Moves a fighter of the mod grid one place (dir -1 / +1). */
void Port_RosterMove(int skin, int dir) {
    int to = skin + dir, t;

    if (skin < 0 || skin >= sSkinCount || to < 0 || to >= sSkinCount) {
        return;
    }
    t = sSkin[skin];
    sSkin[skin] = sSkin[to];
    sSkin[to] = t;
    save();
}

int Port_ReadFile(uint32_t pathAddr, uint32_t bufAddr, int max) {
    FILE *fp = fopen((const char *)(uintptr_t)pathAddr, "rb");
    long size;
    int got;

    if (fp == NULL) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0 || size > max) {
        fclose(fp);
        return 0;
    }
    got = (int)fread((void *)(uintptr_t)bufAddr, 1, (size_t)size, fp);
    fclose(fp);
    return got;
}

/* Takes mod i out of the library: its folder is moved to <data>/mods_removed/ (nothing is deleted). */
int Port_ModRemove(int i) {
    char to[600];
    int n, ok = 0;

    if (i < 0 || i >= sModCount) {
        return 0;
    }
    snprintf(to, sizeof(to), "%s/mods_removed", root());
    make_dir(to);
    for (n = 0; n < 100 && !ok; n++) {
        struct stat st;

        if (n == 0) {
            snprintf(to, sizeof(to), "%s/mods_removed/%s", root(), sMod[i].name);
        } else {
            snprintf(to, sizeof(to), "%s/mods_removed/%s (%d)", root(), sMod[i].name, n + 1);
        }
        if (stat(to, &st) != 0) {
            ok = rename(sMod[i].dir, to) == 0;
            if (!ok) {
                break;
            }
        }
    }
    Port_ModRescan();
    save();
    return ok;
}

/* ---- the mods taken out of the library (<data>/mods_removed): listed, and put back ---- */
static char sGone[MAX_MODS][64];
static int sGoneCount;

static int gone_cmp(const void *a, const void *b) {
    return strcasecmp((const char *)a, (const char *)b);
}

/* Reads the list again; returns how many there are. */
int Port_RemovedList(void) {
    char path[512];
    DirEnt *ents = malloc(sizeof(DirEnt) * MAX_MODS);
    int n, k;

    sGoneCount = 0;
    snprintf(path, sizeof(path), "%s/mods_removed", root());
    if (ents == NULL) {
        return 0;
    }
    n = dir_read(path, ents, MAX_MODS);
    for (k = 0; k < n; k++) {
        if (ents[k].isDir && strlen(ents[k].name) < 64) {
            snprintf(sGone[sGoneCount++], 64, "%s", ents[k].name);
        }
    }
    free(ents);
    qsort(sGone, (size_t)sGoneCount, sizeof(sGone[0]), gone_cmp);
    return sGoneCount;
}

void Port_RemovedName(int i, uint32_t outAddr) {
    snprintf((char *)(uintptr_t)outAddr, 64, "%s", i >= 0 && i < sGoneCount ? sGone[i] : "");
}

/* Puts removed mod i back into the library (off); returns its index there, -1 when it could not be moved. */
int Port_RemovedRestore(int i) {
    char from[600], to[600], name[64];
    int n;

    if (i < 0 || i >= sGoneCount) {
        return -1;
    }
    snprintf(from, sizeof(from), "%s/mods_removed/%s", root(), sGone[i]);
    for (n = 0; n < 100; n++) {
        if (n == 0) {
            snprintf(name, sizeof(name), "%s", sGone[i]);
        } else {
            snprintf(name, sizeof(name), "%.50s (%d)", sGone[i], n + 1);
        }
        snprintf(to, sizeof(to), "%s/mods/%s", root(), name);
        if (file_size(to) < 0) {
            break;
        }
    }
    if (rename(from, to) != 0) {
        return -1;
    }
    Port_ModRescan();
    Port_RemovedList();
    return mod_by_name(name);
}

/* ------------------------------------------------------------------------------------------------- transformations */
static uint8_t *read_all(const char *path, long *len);

/* The transformation and fusion bytes (+0x98..+0xC5) of the parameter block a model pack holds (its entry 17), into
   t. Returns 1 when the file had one. */
static int trans_of_file(const char *path, PortTrans *t) {
    long len;
    uint8_t *d = read_all(path, &len);
    uint32_t n, a, b;
    int k, j, ok = 0;

    if (d == NULL) {
        return 0;
    }
    memcpy(&n, d, 4);
    if (len >= 8 + 19 * 4 && n > 18 && n < 4096) {
        memcpy(&a, d + 4 + 17 * 4, 4);
        memcpy(&b, d + 4 + 18 * 4, 4);
        if (a < b && b <= (uint32_t)len && b - a >= 0xC6) {
            const uint8_t *q = d + a;
            for (k = 0; k < 4; k++) {
                t->target[k] = q[0x98 + k] == 0xFF ? -1 : q[0x98 + k];
                t->cost[k] = q[0x9C + k];
                t->type[k] = q[0xA0 + k];
                t->effect[k] = q[0xA4 + k];
                t->assist[k] = q[0xA8 + k] == 0xFF ? -1 : q[0xA8 + k];
                t->fresh[k] = (q[0xAD] >> (4 + k)) & 1;
            }
            t->next = (int8_t)q[0xAC];
            t->adLow = q[0xAD] & 0x0F;
            for (k = 0; k < 3; k++) {
                t->fuseCost[k] = q[0xAE + k];
                t->fuseType[k] = q[0xB1 + k];
                t->fuse[k] = q[0xB4 + k] == 0xFF ? -1 : q[0xB4 + k];
                t->fuseLead[k] = q[0xB7 + k] == 0xFF ? -1 : q[0xB7 + k];
                for (j = 0; j < 4; j++) {
                    t->partner[k][j] = q[0xBA + k * 4 + j] == 0xFF ? -1 : q[0xBA + k * 4 + j];
                }
            }
            ok = 1;
        }
    }
    free(d);
    return ok;
}

static int sTransRaw; /* (trans_own: a new fighter's forms as its file has them, not taken away) */

/* What a fighter has before anything is set here: its file's own (a mod's model, or the one of the disc, or of a mod
   that replaces it). who: a character id, or REF_MOD + mod index. */
static void trans_own(int who, PortTrans *t) {
    char path[700];

    trans_clear(t);
    if (who >= REF_MOD && who - REF_MOD < sModCount) {
        Mod *m = &sMod[who - REF_MOD];
        int k;

        mod_file(m, 0, path, sizeof(path));
        if ((path[0] == '\0' || !trans_of_file(path, t)) && target_of(m) >= 0) {
            snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", root(), MODEL_FILE + target_of(m) * 10 - 1);
            trans_of_file(path, t);
        }
        if (m->isNew && !sTransRaw) { /* a new fighter does not take over its template's forms: their targets are the roster's */
            for (k = 0; k < 4; k++) {
                t->target[k] = -1;
            }
            t->next = -1;
        }
    } else if (who >= 0 && who < 161) {
        int index = MODEL_FILE + who * 10 - 1;

        if (index < MAX_FILES && sRedirect[index] != NULL) {
            snprintf(path, sizeof(path), "%s", sRedirect[index]);
        } else {
            snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", root(), index);
        }
        trans_of_file(path, t);
    }
    t->set = 0;
    t->fuseSet = 0;
}

/* The transformation part / the fusion part of one record over another's. */
static void trans_take(PortTrans *to, const PortTrans *from) {
    int k;

    to->set = 1;
    to->next = from->next;
    for (k = 0; k < 4; k++) {
        to->target[k] = from->target[k];
        to->cost[k] = from->cost[k];
        to->type[k] = from->type[k];
        to->effect[k] = from->effect[k];
        to->assist[k] = from->assist[k];
        to->fresh[k] = from->fresh[k];
        to->aura[k] = from->aura[k];
    }
}

static void fuse_take(PortTrans *to, const PortTrans *from) {
    to->fuseSet = 1;
    memcpy(to->fuse, from->fuse, sizeof(to->fuse));
    memcpy(to->fuseCost, from->fuseCost, sizeof(to->fuseCost));
    memcpy(to->fuseType, from->fuseType, sizeof(to->fuseType));
    memcpy(to->fuseLead, from->fuseLead, sizeof(to->fuseLead));
    memcpy(to->partner, from->partner, sizeof(to->partner));
    memcpy(to->fuseAura, from->fuseAura, sizeof(to->fuseAura));
    memcpy(to->fuseAnim, from->fuseAnim, sizeof(to->fuseAnim));
}

static void trans_get(int who, PortTrans *out) {
    trans_own(who, out);
    if (who >= REF_MOD && who - REF_MOD < sModCount) {
        const Mod *m = &sMod[who - REF_MOD];
        if (m->tr.set) {
            int k;
            trans_take(out, &m->tr);
            for (k = 0; k < 4; k++) {
                int t = trans_mod(m, k);
                out->target[k] = t >= 0 ? REF_MOD + t : m->trName[k][0] != '\0' ? -1 : m->tr.target[k];
            }
        }
        if (m->tr.fuseSet) {
            int k;
            fuse_take(out, &m->tr);
            for (k = 0; k < 3; k++) {
                int f = m->fuseName[k][0] != '\0' ? mod_by_name(m->fuseName[k]) : -2;
                out->fuse[k] = f >= 0 ? REF_MOD + f : f == -1 ? -1 : m->tr.fuse[k];
            }
        }
    } else if (who >= 0 && who < 161) {
        if (sVanilla[who].set) {
            trans_take(out, &sVanilla[who]);
        }
        if (sVanilla[who].fuseSet) {
            fuse_take(out, &sVanilla[who]);
        }
    }
}

/* who: a character id, or REF_MOD + mod index. What is set here, over what its file has (set / fuseSet say which
   part is set here). */
void Port_TransGet(int who, uint32_t outAddr) {
    ensure();
    trans_get(who, (PortTrans *)(uintptr_t)outAddr);
}

/* Keeps what the FORMS screen made; in->set 0 / fuseSet 0: that part goes back to the fighter's own. */
void Port_TransSet(int who, uint32_t inAddr) {
    const PortTrans *in = (const PortTrans *)(uintptr_t)inAddr;
    int k;

    if (who >= REF_MOD && who - REF_MOD < sModCount) {
        Mod *m = &sMod[who - REF_MOD];
        m->tr = *in;
        for (k = 0; k < 4; k++) {
            m->trName[k][0] = '\0';
            if (in->target[k] >= REF_MOD && in->target[k] - REF_MOD < sModCount) {
                snprintf(m->trName[k], sizeof(m->trName[k]), "%s", sMod[in->target[k] - REF_MOD].name);
                m->tr.target[k] = -1;
            }
        }
        for (k = 0; k < 3; k++) {
            m->fuseName[k][0] = '\0';
            if (in->fuse[k] >= REF_MOD && in->fuse[k] - REF_MOD < sModCount) {
                snprintf(m->fuseName[k], sizeof(m->fuseName[k]), "%s", sMod[in->fuse[k] - REF_MOD].name);
                m->tr.fuse[k] = -1;
            }
        }
        write_settings(m);
    } else if (who >= 0 && who < 161) {
        sVanilla[who] = *in;
    }
    sMotionDirty = 1;
    save();
}

/* What is set for the fighter of a model file, as the game can take it: `t` with every target and result a character
   id (a new fighter: its template), `ref` the same as set (REF_MOD + mod where a mod), skins[0..3] / [4..6] the skin
   a transformation / fusion leads to (-1: none, the plain fighter). who: the fighter (character id or REF_MOD + mod).
   Returns 0 when nothing is set for it (its file's own stand), 1 when something is. */
static int trans_for_file(int modelFile, PortTrans *t, PortTrans *ref, int *skins, int *who) {
    int k, skin, chara;

    *who = -1;
    for (k = 0; k < 7; k++) {
        skins[k] = -1;
    }
    if (modelFile < MODEL_FILE) {
        return 0;
    }
    skin = modelFile >= SKIN_FILE ? (modelFile - SKIN_FILE) / 16 : -1;
    chara = Port_CharaOfFile(modelFile);
    if (skin >= sSkinCount || chara < 0 || chara >= 161) {
        return 0;
    }
    if (skin >= 0) {
        *who = REF_MOD + sSkin[skin];
    } else {
        if (!sVanilla[chara].set && !sVanilla[chara].fuseSet) {
            *who = chara;
            return 0;
        }
        *who = chara;
    }
    trans_get(*who, t);
    *ref = *t;
    for (k = 0; k < 7; k++) { /* a mod as the target: its template, and the skin it is drawn as */
        int *v = k < 4 ? &t->target[k] : &t->fuse[k - 4];
        if (*v >= REF_MOD) {
            int tm = *v - REF_MOD;
            int sk = tm < sModCount && sMod[tm].on && sMod[tm].isNew ? skin_of_mod(tm) : -1;
            *v = sk >= 0 ? target_of(&sMod[tm]) : -1;
            skins[k] = sk;
        }
    }
    return 1;
}

/* Which slots of which parameter block lead to a new fighter (BtlAct_QueueTransform / QueueFusion ask). */
typedef struct FormSkin {
    uint32_t param;
    int8_t skin[7];
} FormSkin;
static FormSkin sFormSkin[16];
static int sFormSkinNext;

static void form_skin_keep(uint32_t param, const int *skins) {
    int i, k;

    for (i = 0; i < 16 && sFormSkin[i].param != param; i++) {
    }
    if (i == 16) {
        i = sFormSkinNext;
        sFormSkinNext = (sFormSkinNext + 1) % 16;
    }
    sFormSkin[i].param = param;
    for (k = 0; k < 7; k++) {
        sFormSkin[i].skin[k] = (int8_t)skins[k];
    }
}

/* The skin transformation (kind 0) / fusion (kind 1) `index` of the fighter with parameter block `param` turns it
   into, -1 none (the plain fighter of its id). */
int Port_FormSkin(uint32_t param, int kind, int index) {
    int i;

    if (index < 0 || index >= (kind == 0 ? 4 : 3)) {
        return -1;
    }
    for (i = 0; i < 16; i++) {
        if (sFormSkin[i].param == param && param != 0) {
            return sFormSkin[i].skin[(kind == 0 ? 0 : 4) + index];
        }
    }
    return -1;
}

/* Writes what is set for the fighter of a model file into its parameter block as the game reads it (see PortTrans).
   Called when a character pack has been bound, with the id of the model file it came from. */
void Port_TransApplyFile(int modelFile, uint32_t paramAddr) {
    static int none[7] = {-1, -1, -1, -1, -1, -1, -1};
    uint8_t *p = (uint8_t *)(uintptr_t)paramAddr;
    PortTrans t, ref;
    int skins[7], who, k, j;

    ensure();
    if (p == NULL) {
        return;
    }
    form_skin_keep(paramAddr, none);
    if (!trans_for_file(modelFile, &t, &ref, skins, &who)) {
        return;
    }
    form_skin_keep(paramAddr, skins);
    if (who >= REF_MOD && !t.set) { /* a new fighter does not take over its template's forms */
        for (k = 0; k < 4; k++) {
            p[0x98 + k] = 0xFF;
        }
        p[0xAC] = 0xFF;
    }
    if (t.set) {
        int fresh = 0;
        for (k = 0; k < 4; k++) {
            int has = t.target[k] >= 0 && t.target[k] < 161;
            p[0x98 + k] = has ? (uint8_t)t.target[k] : 0xFF;
            p[0x9C + k] = (uint8_t)(t.cost[k] < 0 ? 0 : t.cost[k] > 9 ? 9 : t.cost[k]);
            p[0xA0 + k] = (uint8_t)(t.type[k] < 0 || t.type[k] > 4 ? 0 : t.type[k]);
            p[0xA4 + k] = (uint8_t)(t.effect[k] < 0 || t.effect[k] > 11 ? 1 : t.effect[k]);
            p[0xA8 + k] = t.assist[k] >= 0 && t.assist[k] < 161 ? (uint8_t)t.assist[k] : 0xFF;
            fresh |= t.fresh[k] ? 0x10 << k : 0;
        }
        p[0xAD] = (uint8_t)((p[0xAD] & 0x0F) | fresh);
        p[0xAC] = t.next >= 0 && t.next < 4 && p[0x98 + t.next] != 0xFF ? (uint8_t)t.next : 0xFF;
        if (p[0xAC] == 0xFF) {
            for (k = 0; k < 3; k++) {
                if (p[0x98 + k] != 0xFF) {
                    p[0xAC] = (uint8_t)k;
                    break;
                }
            }
        }
    }
    if (t.fuseSet) {
        for (k = 0; k < 3; k++) {
            int has = t.fuse[k] >= 0 && t.fuse[k] < 161;
            int lead = t.fuseLead[k];
            p[0xAE + k] = (uint8_t)(t.fuseCost[k] < 0 ? 0 : t.fuseCost[k] > 9 ? 9 : t.fuseCost[k]);
            p[0xB1 + k] = (uint8_t)(t.fuseType[k] == 2 ? 2 : 1);
            p[0xB4 + k] = has ? (uint8_t)t.fuse[k] : 0xFF;
            for (j = 0; j < 4; j++) {
                int c = t.partner[k][j];
                p[0xBA + k * 4 + j] = has && c >= 0 && c < 161 ? (uint8_t)c : 0xFF;
                if ((lead < 0 || lead >= 161) && has && c >= 0 && c < 161) {
                    lead = c; /* (no scene partner chosen: the first partner) */
                }
            }
            p[0xB7 + k] = has && lead >= 0 && lead < 161 ? (uint8_t)lead : 0xFF;
        }
    }
}

/* ---- the skill list's Transformation page, written again from what is set ----
 * A fighter's skill list (pause menu, held-charge move list) is a text in its model file: pages "!F$<n><title>",
 * entries "!F*<cost, 3 digits><name>", notes "!F%0<text>", inputs "!D#<PAD=...>" (and "!1#" "!2#" "!4#" "!8#" per
 * controller), the end "!F@". The page "!F$4" lists the transformations and fusions as the file has them; when
 * they are set in the Mod Manager it is written again, so the list says what R3 really does.
 */
#define SK_MAX 12000

static int sk_len;
static uint16_t *sk_out;
static int sk_cap;

static void sk_put(const uint16_t *s, int n) {
    int i;
    for (i = 0; i < n && sk_len < sk_cap; i++) {
        sk_out[sk_len++] = s[i];
    }
}

static void sk_ascii(const char *s) {
    while (*s != '\0' && sk_len < sk_cap) {
        sk_out[sk_len++] = (uint8_t)*s++;
    }
}

static void sk_line(const char *prefix, const uint16_t *wide) {
    sk_ascii(prefix);
    while (wide != NULL && *wide != 0 && sk_len < sk_cap) {
        sk_out[sk_len++] = *wide++;
    }
    sk_ascii("\r\n");
}

/* The input of a slot: R3 alone (the one plain R3 takes) or a direction with it; `hold` adds the hold mark. */
static void sk_input(int dir, int hold) {
    static const uint16_t arrow[4] = {0x2190, 0x2191, 0x2192, 0x2193}; /* left up right down: slots 0..3 */
    static const char *const kinds[4] = {"!1#", "!2#", "!8#", "!4#"};
    uint16_t pad[24];
    int k, n;

    for (k = 0; k < (hold ? 4 : 2); k++) {
        int wii = hold ? k == 1 : k == 1;
        n = 0;
        pad[n++] = '<';
        pad[n++] = 'P';
        pad[n++] = 'A';
        pad[n++] = 'D';
        pad[n++] = '=';
        if (dir >= 0) {
            pad[n++] = arrow[dir];
            pad[n++] = 0xFF0B; /* full-width + */
        }
        if (wii) {
            pad[n++] = 0x2020; /* the Wii's button mark */
            pad[n++] = 0x2460;
        } else {
            pad[n++] = 0xFF32; /* R3 */
            pad[n++] = 0xFF13;
        }
        if (hold) {
            pad[n++] = 0x2642;
        }
        pad[n++] = '>';
        pad[n] = 0;
        sk_line(hold ? kinds[k] : (k == 0 ? "!D#" : "!2#"), pad);
    }
}

/* A fighter's or a mod's name as the list shows it, `base` (the fighter itself) taken off the front of a form of it. */
static void sk_name(int ref, int self, uint16_t *out, int max) {
    char buf[96];
    const char *s;
    int i;

    if (ref >= REF_MOD && ref - REF_MOD < sModCount) {
        const Mod *m = &sMod[ref - REF_MOD];
        s = m->white[0] != '\0' ? m->white : m->name;
    } else if (ref >= 0 && ref < CHARA_COUNT) {
        s = kName[ref];
        if (self >= 0 && self < CHARA_COUNT && ref != self) {
            size_t n = strlen(kName[self]);
            if (strncmp(s, kName[self], n) == 0 && s[n] == ' ') {
                s += n + 1; /* "Goku (End) Super Saiyan 3" for Goku (End): "Super Saiyan 3" */
            }
        }
    } else {
        s = "?";
    }
    snprintf(buf, sizeof(buf), "%s", s);
    for (i = 0; buf[i] != '\0' && i < max - 1; i++) {
        out[i] = (uint8_t)buf[i];
    }
    out[i] = 0;
}

/* The page's entries: the transformations (R3 alone for the one plain R3 takes, else a direction), then the fusions
   (a direction, held). */
static void sk_entries(const PortTrans *t, const PortTrans *ref, int self) {
    uint16_t name[80], full[100];
    char cost[16];
    int k, i, j;

    for (k = 0; k < 4; k++) {
        if (t->target[k] < 0) {
            continue;
        }
        snprintf(cost, sizeof(cost), "!F*%03d", t->cost[k] < 0 ? 0 : t->cost[k] > 9 ? 9 : t->cost[k]);
        sk_name(ref->target[k] >= REF_MOD ? ref->target[k] : t->target[k], self, name, 80);
        sk_line(cost, name);
        sk_input(k == t->next ? -1 : k, 0);
    }
    for (k = 0; k < 3; k++) {
        const char *kind = t->fuseType[k] == 2 ? "Potara [" : "Fusion [";

        if (t->fuse[k] < 0) {
            continue;
        }
        snprintf(cost, sizeof(cost), "!F*%03d", t->fuseCost[k] < 0 ? 0 : t->fuseCost[k] > 9 ? 9 : t->fuseCost[k]);
        sk_name(ref->fuse[k] >= REF_MOD ? ref->fuse[k] : t->fuse[k], -1, name, 80);
        j = 0;
        for (i = 0; kind[i] != '\0'; i++) {
            full[j++] = (uint8_t)kind[i];
        }
        for (i = 0; name[i] != 0 && j < 96; i++) {
            full[j++] = name[i];
        }
        full[j++] = ']';
        full[j] = 0;
        sk_line(cost, full);
        for (i = 0; i < 4 && t->partner[k][i] < 0; i++) {
        }
        if (i < 4) {
            sk_name(t->partner[k][i], -1, name, 80);
            sk_line("!F%0Partner: ", name);
        }
        sk_input(k, 1);
    }
}

/* Writes the fighter of a model file a skill list of its own when its transformations or fusions are set here:
   scriptSlot is the object's pointer to its list (object + 0xBC), buf / words a buffer of the game's to write it in. */
void Port_SkillRewrite(int modelFile, uint32_t scriptSlot, uint32_t bufAddr, int words) {
    static const uint16_t title[] = {'T', 'r', 'a', 'n', 's', 'f', 'o', 'r', 'm', 'a', 't', 'i', 'o', 'n', 0};
    uint32_t *slot = (uint32_t *)(uintptr_t)scriptSlot;
    const uint16_t *src;
    PortTrans t, ref;
    int skins[7], who, self, n, at, skip = 0, done = 0;

    ensure();
    if (slot == NULL || *slot == 0 || bufAddr == 0 || words < 1024) {
        return;
    }
    if (!trans_for_file(modelFile, &t, &ref, skins, &who) || (!t.set && !t.fuseSet)) {
        return;
    }
    self = Port_CharaOfFile(modelFile);
    src = (const uint16_t *)(uintptr_t)*slot;
    for (n = 0; n < SK_MAX - 3; n++) { /* (its end: "!F@") */
        if (src[n] == '!' && src[n + 1] == 'F' && src[n + 2] == '@') {
            break;
        }
    }
    if (n >= SK_MAX - 3) {
        return;
    }
    sk_out = (uint16_t *)(uintptr_t)bufAddr;
    sk_cap = words - 64;
    sk_len = 0;
    for (at = 0; at < n;) { /* line by line; the Transformation page's old entries are left out, new ones written */
        int e = at;
        while (e < n && src[e] != '\n') {
            e++;
        }
        if (e < n) {
            e++;
        }
        if (e - at >= 4 && src[at] == '!' && src[at + 1] == 'F' && src[at + 2] == '$') {
            skip = src[at + 3] == '4';
            sk_put(src + at, e - at);
            if (skip) {
                sk_entries(&t, &ref, self);
                done = 1;
            }
        } else if (!skip) {
            sk_put(src + at, e - at);
        }
        at = e;
    }
    if (!done) { /* (the file had no Transformation page: one at the end) */
        sk_line("!F$4", title);
        sk_entries(&t, &ref, self);
    }
    sk_ascii("!F@\r\n");
    sk_out[sk_len < words ? sk_len : words - 1] = 0;
    *slot = bufAddr;
}

/* ------------------------------------------------------------------------------------------------- pictures */
/* GS memory layout (see gs/gs_core.c): where a pixel of an 8-bit texture and of a 32-bit upload lies. */
static const uint8_t kBlock32[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
static const uint8_t kCol32[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                     {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
static const uint8_t kCol8First[4][16] = {{0, 4, 16, 20, 32, 36, 48, 52, 2, 6, 18, 22, 34, 38, 50, 54}, {8, 12, 24, 28, 40, 44, 56, 60, 10, 14, 26, 30, 42, 46, 58, 62},
                                          {33, 37, 49, 53, 1, 5, 17, 21, 35, 39, 51, 55, 3, 7, 19, 23}, {41, 45, 57, 61, 9, 13, 25, 29, 43, 47, 59, 63, 11, 15, 27, 31}};

static uint32_t addr32(uint32_t bw, uint32_t x, uint32_t y) {
    if (bw == 0) {
        bw = 1;
    }
    return (((y & ~0x1Fu) * bw + ((x >> 1) & ~0x1Fu) + kBlock32[(y >> 3) & 3][(x >> 3) & 7]) << 6) + kCol32[y & 7][x & 7];
}

static uint32_t addr8(uint32_t bw, uint32_t x, uint32_t y) {
    uint32_t col = (y & 15) >> 2, sx = (col & 1) ? ((x & 15) ^ 4) : (x & 15);
    return ((((y >> 1) & ~0x1Fu) * (bw >> 1) + ((x >> 2) & ~0x1Fu) + kBlock32[(y >> 4) & 3][(x >> 4) & 7]) << 8) + kCol8First[y & 3][sx] + col * 64;
}

/* A w x h 8-bit texture <-> the 32-bit rectangle (w/2 x h/2) the game uploads it as. */
static void swizzle8_in(uint8_t *mem, uint8_t *idx, uint8_t *data, int w, int h, int toData) {
    int upw = w / 2, uph = h / 2, x, y;
    uint32_t tbw = (uint32_t)w / 64, dbw = (uint32_t)upw / 64;

    if (toData) {
        memset(mem, 0, sizeof(mem));
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                mem[addr8(tbw, (uint32_t)x, (uint32_t)y) & 0xFFFFF] = idx[y * w + x];
            }
        }
        for (y = 0; y < uph; y++) {
            for (x = 0; x < upw; x++) {
                memcpy(&data[(y * upw + x) * 4], &mem[(addr32(dbw, (uint32_t)x, (uint32_t)y) * 4) & 0xFFFFC], 4);
            }
        }
    } else {
        for (y = 0; y < uph; y++) {
            for (x = 0; x < upw; x++) {
                memcpy(&mem[(addr32(dbw, (uint32_t)x, (uint32_t)y) * 4) & 0xFFFFC], &data[(y * upw + x) * 4], 4);
            }
        }
        for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++) {
                idx[y * w + x] = mem[addr8(tbw, (uint32_t)x, (uint32_t)y) & 0xFFFFF];
            }
        }
    }
}

static void swizzle8(uint8_t *idx, uint8_t *data, int w, int h, int toData) {
    static uint8_t mem[1 << 20];
    swizzle8_in(mem, idx, data, w, h, toData);
}

/* The CLUT's storage order: entries 8..15 and 16..23 of every 32 change places. */
static int csm1(int i) {
    return (i & 0x18) == 8 ? i + 8 : (i & 0x18) == 16 ? i - 8 : i;
}

/* ---- Z Warriors' HD pictures: the game's textures of a warrior's photos are small 8-bit ones; each carries a tag in
   palette entries 252..254 (no pixel uses them) and the renderer draws <warrior>/hd_<kind>.png in its place
   (gs_texpack.c). */
enum { HD_RENDER, HD_CHIP, HD_GAUGE_L, HD_GAUGE_R, HD_STRIP, HD_STRIP_M, HD_REEL };
#define HD_S 8 /* the HD photos: 8 times the game's pictures */
static int png_write(const char *path, const uint8_t *rgba, int w, int h);

static uint32_t hd_key(const char *dir, int kind) {
    const char *b = strrchr(dir, '/'), *c = strrchr(dir, '\\');
    uint32_t h = 2166136261u;

    b = c != NULL && (b == NULL || c > b) ? c : b;
    for (b = b != NULL ? b + 1 : dir; *b != '\0'; b++) {
        h = (h ^ (uint8_t)tolower((unsigned char)*b)) * 16777619u;
    }
    return ((h ^ (h >> 20)) & 0xFFFFF) << 4 | (uint32_t)kind;
}

/* pal: 256 entries in index order (R, G, B, A). */
static void hd_tag(uint8_t *pal, const char *dir, int kind) {
    static uint32_t count;
    uint32_t v[3];
    int i;

    v[0] = (((uint32_t)time(NULL) * 2654435761u + ++count * 40503u) & 0xFFFFFF) | 3u << 24;
    v[1] = 0x015A5748u;
    v[2] = hd_key(dir, kind) | 2u << 24;
    for (i = 0; i < 3; i++) {
        pal[(252 + i) * 4] = (uint8_t)v[i];
        pal[(252 + i) * 4 + 1] = (uint8_t)(v[i] >> 8);
        pal[(252 + i) * 4 + 2] = (uint8_t)(v[i] >> 16);
        pal[(252 + i) * 4 + 3] = (uint8_t)(v[i] >> 24);
    }
}

/* An HD picture written: RGBA with 8-bit alpha in, the game's alpha (0x80 opaque) in the file. */
static int hd_write(const char *dir, int kind, uint8_t *rgba, int w, int h) {
    char path[760];
    int i, ok;

    for (i = 0; i < w * h; i++) {
        rgba[i * 4 + 3] = (uint8_t)((rgba[i * 4 + 3] * 128 + 127) / 255);
    }
    snprintf(path, sizeof(path), "%s/hd_%d.png", dir, kind);
    ok = png_write(path, rgba, w, h);
    fprintf(stderr, "bt3: HD picture %s: %dx%d %s\n", path, w, h, ok ? "written" : "NOT written");
    return ok;
}

/* gs_texpack.c: the file of a tagged texture (any thread: only reads the list of mods). */
int Port_ZwHdPath(uint32_t key, char *out, int max) {
    int i;
    FILE *fp;

    if (key >= 0xFFFFF8u) { /* the main menu's remastered icon sheets (src/menu/menu_a.c; no warrior has pictures 8..10) */
        char cwd[700];
        if (getcwd(cwd, sizeof(cwd)) == NULL) { return 0; } /* (a whole path: SDL on Android takes others as internal) */
        snprintf(out, (size_t)max, "%s/tbo/ui/menu_icons_%u.png", cwd, key - 0xFFFFF8u);
        if ((fp = fopen(out, "rb")) == NULL) { return 0; }
        fclose(fp);
        return 1;
    }
    for (i = 0; i < sModCount; i++) {
        if ((hd_key(sMod[i].dir, 0) >> 4) == (key >> 4)) {
            snprintf(out, (size_t)max, "%s/hd_%u.png", sMod[i].dir, key & 15);
            if ((fp = fopen(out, "rb")) != NULL) {
                fclose(fp);
                return 1;
            }
        }
    }
    return 0;
}

/* A face's shape made whole: each row see-through only outside its first and last shown pixel, and a row with
   nothing shown between two that show takes their span (the disc face's own holes - its outline's gaps, the notch by
   its mouth - are not the photo's). mask: gw x gh of 0 / 255. */
static void hd_mask_fill(uint8_t *mask, int gw, int gh) {
    int x, y, lo[64], hi[64];

    gh = gh > 64 ? 64 : gh;
    for (y = 0; y < gh; y++) {
        lo[y] = gw, hi[y] = -1;
        for (x = 0; x < gw; x++) {
            if (mask[y * gw + x]) {
                lo[y] = x < lo[y] ? x : lo[y];
                hi[y] = x;
            }
        }
    }
    for (y = 0; y < gh; y++) {
        if (hi[y] < 0) { /* (an empty row between shown ones: theirs) */
            int a = y - 1, b = y + 1;

            while (a >= 0 && hi[a] < 0) {
                a--;
            }
            while (b < gh && hi[b] < 0) {
                b++;
            }
            if (a >= 0 && b < gh) {
                lo[y] = lo[a] < lo[b] ? lo[a] : lo[b];
                hi[y] = hi[a] > hi[b] ? hi[a] : hi[b];
            }
        }
        for (x = 0; x < gw; x++) {
            mask[y * gw + x] = (uint8_t)(x >= lo[y] && x <= hi[y] ? 255 : 0);
        }
    }
}

/* The gauge's face window: the game's own faces drawn for it, a pixel in where at least half of the fighters cover it
   (0 / 255; kind 0, 1 the battle gauge's two pictures (64 x 64), 2, 3 the team strip's two (64 x 32)). Made once, from
   the disc. 1 when there is one. */
static int hud_window(int kind, uint8_t *out) {
    static uint8_t win[4][64 * 64];
    static int made[4];
    static uint16_t votes[64 * 64];
    int w = 64, h = kind < 2 ? 64 : 32, c, n = 0, i;

    if (kind < 0 || kind > 3) {
        return 0;
    }
    if (made[kind] == 0) {
        made[kind] = -1;
        memset(votes, 0, sizeof(votes));
        for (c = 0; c < CHARA_COUNT; c++) {
            static uint8_t mIdx[64 * 64];
            static WpTex ent[8];
            WarriorPak p;
            const uint8_t *cl = NULL, *px = NULL;

            memset(&p, 0, sizeof(p));
            if (kind < 2) { /* (part 0 of its model file: the two pictures) */
                uint32_t pn;
                const uint8_t *part;
                int count;

                if (!warrior_open(&p, MODEL_FILE + c * 10) || p.n != WARRIOR_PARTS || (part = warrior_part(&p, 0, &pn)) == NULL) {
                    free(p.d);
                    continue;
                }
                count = wp_textures(part, pn, ent, 8);
                if (count > kind && ((ent[kind].tex0 >> 20) & 0x3F) == 0x13 && (1u << ((ent[kind].tex0 >> 26) & 0xF)) == 64 &&
                    (1u << ((ent[kind].tex0 >> 30) & 0xF)) == 64 && ent[kind].pixOfs * 4 + 0x60 + 4096 <= pn && ent[kind].clutOfs * 4 + 0x60 + 1024 <= pn) {
                    px = part + ent[kind].pixOfs * 4 + 0x60;
                    cl = part + ent[kind].clutOfs * 4 + 0x60;
                }
            } else { /* (the data file of that side: the strip) */
                char path[700];
                long len;
                uint32_t f[4], hn;
                uint64_t tex0;

                snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", root(), DATA_FILE + c * 2 + (kind - 2) - 1);
                if ((p.d = read_all(path, &len)) == NULL || len < 0x60) {
                    free(p.d);
                    continue;
                }
                memcpy(&hn, p.d, 4);
                memcpy(f, p.d + 0x20, 16);
                memcpy(&tex0, p.d + 0x20 + 0x30, 8);
                if (hn == 1 && ((tex0 >> 20) & 0x3F) == 0x13 && (1u << ((tex0 >> 26) & 0xF)) == 64 && (1u << ((tex0 >> 30) & 0xF)) == 32 &&
                    (long)f[0] * 4 + 0x60 + 64 * 32 <= len && (long)f[1] * 4 + 0x60 + 1024 <= len) {
                    px = p.d + f[0] * 4 + 0x60;
                    cl = p.d + f[1] * 4 + 0x60;
                }
            }
            if (px != NULL) {
                swizzle8(mIdx, (uint8_t *)px, w, h, 0);
                for (i = 0; i < w * h; i++) {
                    votes[i] += cl[csm1(mIdx[i]) * 4 + 3] >= 0x40;
                }
                n++;
            }
            free(p.d);
        }
        if (n >= 4) {
            for (i = 0; i < w * h; i++) {
                win[kind][i] = (uint8_t)(votes[i] * 2 >= n ? 255 : 0);
            }
            made[kind] = 1;
        }
        fprintf(stderr, "bt3: hud window %d: from %d fighters\n", kind, n);
    }
    if (made[kind] != 1) {
        return 0;
    }
    memcpy(out, win[kind], (size_t)(w * h));
    return 1;
}

/* A 0/255 mask of gw x gh, read between its pixels at HD pixel (X, Y) of a picture S times bigger. */
static int hd_mask_at(const uint8_t *mask, int gw, int gh, int X, int Y) {
    int px = (2 * X + 1 - HD_S) * 128 / HD_S, py = (2 * Y + 1 - HD_S) * 128 / HD_S;
    int ix = px >= 0 ? px / 256 : -((-px + 255) / 256), iy = py >= 0 ? py / 256 : -((-py + 255) / 256);
    int fx = px - ix * 256, fy = py - iy * 256, x0 = ix, x1 = ix + 1, y0 = iy, y1 = iy + 1, a, b;

    x0 = x0 < 0 ? 0 : x0 > gw - 1 ? gw - 1 : x0;
    x1 = x1 < 0 ? 0 : x1 > gw - 1 ? gw - 1 : x1;
    y0 = y0 < 0 ? 0 : y0 > gh - 1 ? gh - 1 : y0;
    y1 = y1 < 0 ? 0 : y1 > gh - 1 ? gh - 1 : y1;
    a = mask[y0 * gw + x0] * (256 - fx) + mask[y0 * gw + x1] * fx;
    b = mask[y1 * gw + x0] * (256 - fx) + mask[y1 * gw + x1] * fx;
    return (a * (256 - fy) + b * fy) >> 16;
}

/* An RGBA picture made smaller by a whole factor (each pixel the average of what it covers). */
static void hd_down(const uint8_t *src, int sw, int sh, uint8_t *dst, int dw, int dh) {
    int x, y, u, v, c;

    for (y = 0; y < dh; y++) {
        for (x = 0; x < dw; x++) {
            int x0 = x * sw / dw, x1 = (x + 1) * sw / dw, y0 = y * sh / dh, y1 = (y + 1) * sh / dh;
            long sum[4] = {0, 0, 0, 0}, n = 0;

            x1 = x1 <= x0 ? x0 + 1 : x1;
            y1 = y1 <= y0 ? y0 + 1 : y1;
            for (v = y0; v < y1; v++) {
                for (u = x0; u < x1; u++) {
                    for (c = 0; c < 4; c++) {
                        sum[c] += src[((size_t)v * sw + u) * 4 + c];
                    }
                    n++;
                }
            }
            for (c = 0; c < 4; c++) {
                dst[(y * dw + x) * 4 + c] = (uint8_t)(sum[c] / n);
            }
        }
    }
}

/* ---- the gauge faces from a photo of the warrior (Z Warriors): <warrior>/hud_photo.raw, 64 x 32 RGBA, looking
   the way the first side's face looks; the second side has it mirrored. The faces are 64 x 32 PSMT8 textures with a
   256-colour palette (one texture file each). */
#define HUD_W 64
#define HUD_H 32

static int sHudAxis;
static const uint8_t *sHudPx;

static int hud_cmp(const void *a, const void *b) {
    return (int)sHudPx[*(const int *)a * 4 + sHudAxis] - (int)sHudPx[*(const int *)b * 4 + sHudAxis];
}

/* A 64 x 32 RGBA picture in 256 colours (median cut over its pixels). */
#define HUD_PH_H 40 /* the photo: 64 x 40, the shape of the gauge's face (part 0 of the model files) */

static void hud_quantize_n(const uint8_t *rgba, int count, uint8_t *idx, uint8_t *pal) {
    static int ord[64 * 64];
    int start[256], cnt[256], n = 1, i, j, k, c;

    count = count > 64 * 64 ? 64 * 64 : count;
    for (i = 0; i < count; i++) {
        ord[i] = i;
    }
    start[0] = 0;
    cnt[0] = count;
    while (n < 252) { /* the box with the widest spread of one colour, cut at its median (252..254: the HD tag, 255 see-through) */
        int best = -1, range = 0, axis = 0, half;

        for (k = 0; k < n; k++) {
            int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};

            if (cnt[k] < 2) {
                continue;
            }
            for (j = start[k]; j < start[k] + cnt[k]; j++) {
                for (c = 0; c < 3; c++) {
                    int v = rgba[ord[j] * 4 + c];

                    lo[c] = v < lo[c] ? v : lo[c];
                    hi[c] = v > hi[c] ? v : hi[c];
                }
            }
            for (c = 0; c < 3; c++) {
                if (hi[c] - lo[c] > range) {
                    range = hi[c] - lo[c];
                    best = k;
                    axis = c;
                }
            }
        }
        if (best < 0) {
            break;
        }
        sHudAxis = axis;
        sHudPx = rgba;
        qsort(ord + start[best], (size_t)cnt[best], sizeof(int), hud_cmp);
        half = cnt[best] / 2;
        start[n] = start[best] + half;
        cnt[n] = cnt[best] - half;
        cnt[best] = half;
        n++;
    }
    memset(pal, 0, 1024);
    for (k = 0; k < n; k++) {
        long sum[3] = {0, 0, 0};

        for (j = start[k]; j < start[k] + cnt[k]; j++) {
            idx[ord[j]] = (uint8_t)k;
            for (c = 0; c < 3; c++) {
                sum[c] += rgba[ord[j] * 4 + c];
            }
        }
        for (c = 0; c < 3; c++) {
            pal[k * 4 + c] = (uint8_t)(cnt[k] > 0 ? sum[c] / cnt[k] : 0);
        }
        pal[k * 4 + 3] = 0x80;
    }
}

static void hud_quantize(const uint8_t *rgba, uint8_t *idx, uint8_t *pal) {
    hud_quantize_n(rgba, HUD_W * HUD_H, idx, pal);
}

/* The battle gauge's face (part 0 of a model file: two 64 x 64 pictures, the left place's and the right's, mirrored)
   made from the photo (64 x ph RGBA), in place: each picture keeps its own shape (where it is see-through it stays so),
   the photo stretched over that shape. 1 when written. */
static int gauge_write(const char *path, const uint8_t *photo, int ph, const uint8_t *hd, const char *dir) {
    static uint8_t mIdx[64 * 64], img[64 * 64 * 4], idx[64 * 64], pal[1024], mask[64 * 64];
    static WpTex ent[8];
    uint8_t *d, *part;
    long len;
    uint32_t n, a, b;
    int count, t, i, x, y, ok = 0, changed = 0, cx = 0, cy = 0, cw = 64000, ch = 40000;
    FILE *fp;

    if ((d = read_all(path, &len)) == NULL) {
        return 0;
    }
    memcpy(&n, d, 4);
    if (len < 16 || n == 0 || n > 4096) {
        free(d);
        return 0;
    }
    memcpy(&a, d + 4, 4);
    memcpy(&b, d + 8, 4);
    if (b <= a + 0x60 || (long)b > len) {
        free(d);
        return 0;
    }
    part = d + a;
    count = wp_textures(part, b - a, ent, 8);
    for (t = 0; t < count && t < 2; t++) {
        WpTex *e = &ent[t];
        uint8_t *px = part + e->pixOfs * 4 + 0x60, *cl = part + e->clutOfs * 4 + 0x60;
        int x0 = 64, y0 = 64, x1 = -1, y1 = -1;

        if (((e->tex0 >> 20) & 0x3F) != 0x13 || (1u << ((e->tex0 >> 26) & 0xF)) != 64 || (1u << ((e->tex0 >> 30) & 0xF)) != 64 ||
            e->pixOfs * 4 + 0x60 + 4096 > b - a || e->clutOfs * 4 + 0x60 + 1024 > b - a) {
            continue;
        }
        swizzle8(mIdx, px, 64, 64, 0);
        for (i = 0; i < 64 * 64; i++) { /* the shape: where the picture shows */
            if (cl[csm1(mIdx[i]) * 4 + 3] >= 0x40) {
                x = i % 64, y = i / 64;
                x0 = x < x0 ? x : x0, x1 = x > x1 ? x : x1;
                y0 = y < y0 ? y : y0, y1 = y > y1 ? y : y1;
            }
        }
        if (x1 < x0 || y1 < y0) {
            continue;
        }
        { /* the part of the photo the shape's box shows: its own proportions, cut from the middle (not stretched to it:
             that squeezed the face); in thousandths of the photo's pixels */
            int bw = x1 - x0 + 1, bh = y1 - y0 + 1;

            if (bw * ph < bh * 64) {
                cw = ph * bw * 1000 / bh, ch = ph * 1000, cx = (64 * 1000 - cw) / 2, cy = 0;
            } else {
                cw = 64 * 1000, ch = 64 * bh * 1000 / bw, cx = 0, cy = (ph * 1000 - ch) / 2;
            }
        }
        for (y = 0; y < 64; y++) {
            for (x = 0; x < 64; x++) {
                int sx = (cx + (x - x0) * cw / (x1 - x0 + 1)) / 1000, sy = (cy + (y - y0) * ch / (y1 - y0 + 1)) / 1000;

                sx = sx < 0 ? 0 : sx > 63 ? 63 : sx;
                sy = sy < 0 ? 0 : sy > ph - 1 ? ph - 1 : sy;
                if (t == 1) {
                    sx = 63 - sx; /* (the right place's picture looks the other way) */
                }
                memcpy(&img[(y * 64 + x) * 4], &photo[(sy * 64 + sx) * 4], 4);
            }
        }
        hud_quantize_n(img, 64 * 64, idx, pal);
        for (i = 0; i < 64 * 64; i++) {
            mask[i] = cl[csm1(mIdx[i]) * 4 + 3] < 0x40 ? 0 : 255;
        }
        if (!hud_window(t, mask)) { /* (the game's face window; else this file's own shape, made whole) */
            hd_mask_fill(mask, 64, 64);
        }
        for (i = 0; i < 64 * 64; i++) {
            if (mask[i] == 0) {
                idx[i] = 255;
            }
        }
        memset(&pal[255 * 4], 0, 4);
        if (hd != NULL && dir != NULL) { /* the HD face: the big photo over the same shape */
            uint8_t *o = malloc(64 * HD_S * 64 * HD_S * 4);

            if (o != NULL) {
                for (y = 0; y < 64 * HD_S; y++) {
                    for (x = 0; x < 64 * HD_S; x++) {
                        int hx = (cx * HD_S + ((2 * x + 1) - 2 * HD_S * x0) * cw / (2 * (x1 - x0 + 1))) / 1000;
                        int hy = (cy * HD_S + ((2 * y + 1) - 2 * HD_S * y0) * ch / (2 * (y1 - y0 + 1))) / 1000;
                        uint8_t *q = &o[(y * 64 * HD_S + x) * 4];

                        hx = hx < 0 ? 0 : hx > 64 * HD_S - 1 ? 64 * HD_S - 1 : hx;
                        hy = hy < 0 ? 0 : hy > ph * HD_S - 1 ? ph * HD_S - 1 : hy;
                        if (t == 1) {
                            hx = 64 * HD_S - 1 - hx;
                        }
                        memcpy(q, &hd[(hy * 64 * HD_S + hx) * 4], 3);
                        q[3] = (uint8_t)hd_mask_at(mask, 64, 64, x, y);
                    }
                }
                if (hd_write(dir, HD_GAUGE_L + t, o, 64 * HD_S, 64 * HD_S)) {
                    hd_tag(pal, dir, HD_GAUGE_L + t);
                }
                free(o);
            }
        }
        swizzle8(idx, px, 64, 64, 1);
        for (i = 0; i < 256; i++) {
            memcpy(cl + csm1(i) * 4, &pal[i * 4], 4);
        }
        changed++;
    }
    if (changed > 0 && (fp = fopen(path, "wb")) != NULL) {
        ok = fwrite(d, 1, (size_t)len, fp) == (size_t)len;
        ok &= fclose(fp) == 0;
    }
    free(d);
    return ok;
}

/* The gauge face file `path` made the picture (mirrored or not), in place. */
static int hud_write(const char *path, const uint8_t *rgba, int mirror, const uint8_t *hd, int ph, const char *dir) {
    static uint8_t img[HUD_W * HUD_H * 4], idx[HUD_W * HUD_H], pal[1024], mask[HUD_W * HUD_H];
    uint32_t f[4], n;
    uint64_t tex0;
    long len;
    uint8_t *d = read_all(path, &len);
    FILE *fp;
    int x, y, i, ok = 0;

    if (d == NULL) {
        return 0;
    }
    if (len < 0x60) {
        goto out;
    }
    memcpy(&n, d, 4);
    memcpy(f, d + 0x20, 16);
    memcpy(&tex0, d + 0x20 + 0x30, 8);
    if (n != 1 || ((tex0 >> 20) & 0x3F) != 0x13 || (1u << ((tex0 >> 26) & 0xF)) != HUD_W || (1u << ((tex0 >> 30) & 0xF)) != HUD_H ||
        (long)f[0] * 4 + 0x60 + HUD_W * HUD_H > len || (long)f[1] * 4 + 0x60 + 1024 > len) {
        goto out;
    }
    for (y = 0; y < HUD_H; y++) {
        for (x = 0; x < HUD_W; x++) {
            memcpy(&img[(y * HUD_W + x) * 4], &rgba[(y * HUD_W + (mirror ? HUD_W - 1 - x : x)) * 4], 4);
        }
    }
    hud_quantize(img, idx, pal);
    memset(mask, 255, sizeof(mask));
    { /* the game's shape: where the disc's own face of this file is see-through, so is the photo (entry 255) */
        static uint8_t mIdx[HUD_W * HUD_H];
        const char *base = strrchr(path, '/');
        char disc[700];
        long ml;
        uint8_t *md;
        uint32_t mf[4];

        snprintf(disc, sizeof(disc), "%s/pzs3us1/%s", root(), base != NULL ? base + 1 : path);
        if ((md = read_all(disc, &ml)) != NULL) {
            memcpy(mf, md + 0x20, 16);
            if (ml >= 0x60 && (long)mf[0] * 4 + 0x60 + HUD_W * HUD_H <= ml && (long)mf[1] * 4 + 0x60 + 1024 <= ml) {
                swizzle8(mIdx, md + mf[0] * 4 + 0x60, HUD_W, HUD_H, 0);
                for (i = 0; i < HUD_W * HUD_H; i++) {
                    if (md[mf[1] * 4 + 0x60 + csm1(mIdx[i]) * 4 + 3] < 0x40) {
                        mask[i] = 0;
                    }
                }
                if (!hud_window(2 + mirror, mask)) {
                    hd_mask_fill(mask, HUD_W, HUD_H);
                }
                for (i = 0; i < HUD_W * HUD_H; i++) {
                    if (mask[i] == 0) {
                        idx[i] = 255;
                    }
                }
                memset(&pal[255 * 4], 0, 4);
            }
            free(md);
        }
    }
    if (hd != NULL && dir != NULL) { /* the HD strip: the big photo's rows as the strip takes them */
        uint8_t *o = malloc(HUD_W * HD_S * HUD_H * HD_S * 4);

        if (o != NULL) {
            for (y = 0; y < HUD_H * HD_S; y++) {
                for (x = 0; x < HUD_W * HD_S; x++) {
                    int hx = mirror ? HUD_W * HD_S - 1 - x : x, hy = y + (ph - HUD_H) * HD_S / 2; /* (the middle rows: not squeezed) */
                    uint8_t *q = &o[(y * HUD_W * HD_S + x) * 4];

                    hy = hy > ph * HD_S - 1 ? ph * HD_S - 1 : hy;
                    memcpy(q, &hd[(hy * HUD_W * HD_S + hx) * 4], 3);
                    q[3] = (uint8_t)hd_mask_at(mask, HUD_W, HUD_H, x, y);
                }
            }
            if (hd_write(dir, mirror ? HD_STRIP_M : HD_STRIP, o, HUD_W * HD_S, HUD_H * HD_S)) {
                hd_tag(pal, dir, mirror ? HD_STRIP_M : HD_STRIP);
            }
            free(o);
        }
    }
    swizzle8(idx, d + f[0] * 4 + 0x60, HUD_W, HUD_H, 1);
    for (i = 0; i < 256; i++) {
        memcpy(d + f[1] * 4 + 0x60 + csm1(i) * 4, &pal[i * 4], 4);
    }
    if ((fp = fopen(path, "wb")) != NULL) {
        ok = fwrite(d, 1, (size_t)len, fp) == (size_t)len;
        ok &= fclose(fp) == 0;
    }
out:
    free(d);
    return ok;
}

/* The warrior in `dir`'s gauge faces (pzs3us1/<number>.bin, the side by the number) from its hud_photo.raw, when it
   has one. Returns how many were made. */
static int warrior_hud_photo(const char *dir) {
    char path[700];
    long len;
    uint8_t *px;
    DIR *dp;
    struct dirent *e;
    int made = 0;

    static uint8_t strip[HUD_W * HUD_H * 4];
    int ph, y;
    uint8_t *hd;
    long hdLen;

    snprintf(path, sizeof(path), "%s/hud_photo.raw", dir);
    if ((px = read_all(path, &len)) == NULL) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/hud_photo_hd.raw", dir);
    hd = read_all(path, &hdLen);
    if (hd != NULL && hdLen != (long)HUD_W * HD_S * HUD_PH_H * HD_S * 4) {
        free(hd);
        hd = NULL;
    }
    ph = len == HUD_W * HUD_PH_H * 4 ? HUD_PH_H : len == HUD_W * HUD_H * 4 ? HUD_H : 0;
    if (ph == 0) {
        free(px);
        free(hd);
        return 0;
    }
    if (ph != HUD_PH_H) {
        free(hd);
        hd = NULL;
    }
    if ((dp = opendir(dir)) != NULL) { /* the battle gauge's face: part 0 of each model file */
        while ((e = readdir(dp)) != NULL) {
            size_t l = strlen(e->d_name);
            char file[800];

            if ((l > 5 && strcmp(e->d_name + l - 5, "p.pak") == 0) || (l > 9 && strcmp(e->d_name + l - 9, "p_dmg.pak") == 0)) {
                snprintf(file, sizeof(file), "%s/%s", dir, e->d_name);
                made += gauge_write(file, px, ph, hd, dir);
            }
        }
        closedir(dp);
    }
    for (y = 0; y < HUD_H; y++) { /* the team's waiting face (the 64 x 32 strip of the data files) */
        memcpy(strip + y * HUD_W * 4, px + (y + (ph - HUD_H) / 2) * HUD_W * 4, HUD_W * 4); /* (the middle rows: not squeezed) */
    }
    memcpy(px, strip, sizeof(strip));
    len = HUD_W * HUD_H * 4;
    snprintf(path, sizeof(path), "%s/pzs3us1", dir);
    if (len == HUD_W * HUD_H * 4 && (dp = opendir(path)) != NULL) {
        while ((e = readdir(dp)) != NULL) {
            char file[800];
            int num;

            if (strlen(e->d_name) != 9 || sscanf(e->d_name, "%5d.bin", &num) != 1 || num < DATA_FILE - 1) {
                continue;
            }
            snprintf(file, sizeof(file), "%s/%s", path, e->d_name);
            made += hud_write(file, px, (num - DATA_FILE + 1) & 1, hd, HUD_PH_H, dir);
        }
        closedir(dp);
    }
    free(px);
    free(hd);
    return made;
}

extern void GsCapture_PhotoWant(void);
/* (the corners in ten-thousandths: whole numbers, as this soft-float code cannot hand floats to the C++ side) */
extern int GsCapture_PhotoCrop(int x0, int y0, int x1, int y1, uint8_t *out, int ow, int oh);

/* Mod i's gauge faces photographed off the screen (Z Warriors): `start` 1 asks for a photo of the next frame, then
   0 (each frame) gives 0 while it is not there, 1 when the part `from` - `to` (corners x | y << 16, ten-thousandths of
   the game's picture) made its faces, -1 failed; `start` -1 forgets its photo (its faces come back when it is made
   again). (Four numbers: more than four from the game's code do not arrive whole.) */
int Port_WarriorHudPhoto(int i, int start, int from, int to) {
    int x0 = from & 0xFFFF, y0 = (from >> 16) & 0xFFFF, x1 = to & 0xFFFF, y1 = (to >> 16) & 0xFFFF;
    static uint8_t px[HUD_W * HUD_PH_H * 4];
    char path[700];
    FILE *fp;
    int ok;

    ensure();
    if (i < 0 || i >= sModCount) {
        return -1;
    }
    snprintf(path, sizeof(path), "%s/hud_photo.raw", sMod[i].dir);
    if (start < 0) {
        remove(path);
        snprintf(path, sizeof(path), "%s/hud_photo_hd.raw", sMod[i].dir);
        remove(path);
        snprintf(path, sizeof(path), "%s/.warrior_built", sMod[i].dir); /* (made again: the faces back as they were) */
        remove(path);
        return 1;
    }
    if (start > 0) {
        fprintf(stderr, "bt3: hud photo: asked for mod %d (%s)\n", i, sMod[i].dir);
        fflush(stderr);
        GsCapture_PhotoWant();
        return 0;
    }
    {
        static uint8_t hd[HUD_W * HD_S * HUD_PH_H * HD_S * 4];
        char hdPath[720];

        if (!GsCapture_PhotoCrop(x0, y0, x1, y1, hd, HUD_W * HD_S, HUD_PH_H * HD_S)) {
            return 0;
        }
        hd_down(hd, HUD_W * HD_S, HUD_PH_H * HD_S, px, HUD_W, HUD_PH_H);
        snprintf(hdPath, sizeof(hdPath), "%s/hud_photo_hd.raw", sMod[i].dir);
        if ((fp = fopen(hdPath, "wb")) != NULL) {
            fwrite(hd, 1, sizeof(hd), fp);
            fclose(fp);
        }
    }
    if ((fp = fopen(path, "wb")) == NULL) {
        return -1;
    }
    ok = fwrite(px, 1, sizeof(px), fp) == sizeof(px);
    ok &= fclose(fp) == 0;
    fprintf(stderr, "bt3: hud photo: picture kept (%d), making the faces\n", ok);
    fflush(stderr);
    ok = ok ? warrior_hud_photo(sMod[i].dir) : 0;
    fprintf(stderr, "bt3: hud photo: %d faces made\n", ok);
    fflush(stderr);
    return ok > 0 ? 1 : -1;
}

/* Philip Gage's byte-pair expansion (the game's sys/bpe.c): -> malloc'ed data, *size its length; NULL = not that. */
static uint8_t *bpe_decode(const uint8_t *d, long len, uint32_t *size) {
    uint32_t total, pack, n = 0;
    const uint8_t *p, *end;
    uint8_t *out;

    if (len < 8) {
        return NULL;
    }
    memcpy(&total, d, 4);
    memcpy(&pack, d + 4, 4);
    if (total == 0 || total > (8u << 20) || pack + 8 > (uint32_t)len) {
        return NULL;
    }
    out = malloc(total + 16);
    p = d + 8;
    end = p + pack;
    while (p < end && n < total) {
        uint8_t left[256], right[256], stack[512];
        int c = 0, count, sp = 0, i, size16;

        for (i = 0; i < 256; i++) {
            left[i] = (uint8_t)i;
            right[i] = 0;
        }
        count = *p++;
        for (;;) {
            if (count > 127) {
                c += count - 127;
                count = 0;
            }
            if (c >= 256) {
                break;
            }
            for (i = 0; i <= count && c < 256; i++, c++) {
                left[c] = *p++;
                if (c != left[c]) {
                    right[c] = *p++;
                }
            }
            if (c >= 256) {
                break;
            }
            count = *p++;
        }
        size16 = p[0] * 256 + p[1];
        p += 2;
        for (;;) {
            int ch;
            if (sp > 0) {
                ch = stack[--sp];
            } else {
                if (size16-- == 0) {
                    break;
                }
                ch = *p++;
            }
            if (ch == left[ch]) {
                if (n < total) {
                    out[n++] = (uint8_t)ch;
                }
            } else if (sp < 510) {
                stack[sp++] = right[ch];
                stack[sp++] = left[ch];
            }
        }
    }
    *size = total;
    return out;
}

static uint8_t *read_all(const char *path, long *len) {
    FILE *fp = disc_open(path);
    uint8_t *d;

    if (fp == NULL) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    *len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    d = malloc((size_t)*len + 16);
    if (fread(d, 1, (size_t)*len, fp) != (size_t)*len) {
        free(d);
        d = NULL;
    }
    fclose(fp);
    return d;
}

/* A mod's large portrait (256 x 512, 256 colours): its own, else its fighter's. idx: 131072 bytes, pal: 256 RGBA
   (the PS2's alpha, 0x80 = solid). Returns 1 when read. */
static int portrait(const Mod *m, uint8_t *idx, uint8_t *pal) {
    char path[700];
    uint8_t *pk, *raw;
    long len;
    uint32_t size, f[10];
    int i, ok = 0;

    mod_path(m, 10, path, sizeof(path));
    if (path[0] == '\0') {
        int t = target_of(m);
        if (t < 0) {
            return 0;
        }
        snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", root(), FACE_FILE + t - 1);
    }
    pk = read_all(path, &len);
    if (pk == NULL) {
        return 0;
    }
    raw = bpe_decode(pk, len, &size);
    free(pk);
    if (raw == NULL) {
        return 0;
    }
    if (size >= 0x60) {
        memcpy(f, raw + 0x20, sizeof(f));
        if (f[0] * 4 + 0x60 + 131072 <= size && f[1] * 4 + 0x60 + 1024 <= size) {
            swizzle8(idx, raw + f[0] * 4 + 0x60, 256, 512, 0);
            for (i = 0; i < 256; i++) {
                memcpy(&pal[i * 4], raw + f[1] * 4 + 0x60 + csm1(i) * 4, 4);
            }
            ok = 1;
        }
    }
    free(raw);
    return ok;
}

/* The fight panels (gs/ui.cpp, from the render thread): a large portrait file as RGBA, 256 x 512 x 4 bytes, the
   PS2's alpha doubled to 0..255. Its own buffers: nothing shared with the game's thread. Returns 1 when read. */
int Port_PortraitRGBA(const char *path, unsigned char *rgba) {
    uint8_t *pk, *raw, *mem, *idx;
    long len;
    uint32_t size, f[2];
    int i, ok = 0;

    pk = read_all(path, &len);
    if (pk == NULL) {
        return 0;
    }
    raw = bpe_decode(pk, len, &size);
    free(pk);
    if (raw == NULL) {
        return 0;
    }
    mem = malloc(1 << 20);
    idx = malloc(256 * 512);
    if (mem != NULL && idx != NULL && size >= 0x60) {
        memcpy(f, raw + 0x20, sizeof(f));
        if (f[0] * 4 + 0x60 + 131072 <= size && f[1] * 4 + 0x60 + 1024 <= size) {
            uint8_t pal[1024];
            for (i = 0; i < 256; i++) {
                memcpy(&pal[i * 4], raw + f[1] * 4 + 0x60 + csm1(i) * 4, 4);
                pal[i * 4 + 3] = pal[i * 4 + 3] >= 0x80 ? 255 : (uint8_t)(pal[i * 4 + 3] * 2);
            }
            swizzle8_in(mem, idx, raw + f[0] * 4 + 0x60, 256, 512, 0);
            for (i = 0; i < 256 * 512; i++) {
                memcpy(rgba + i * 4, &pal[idx[i] * 4], 4);
            }
            ok = 1;
        }
    }
    free(idx);
    free(mem);
    free(raw);
    return ok;
}

/* Where the head is in a portrait: from the first lines that have anything in them, a square as wide as the head
   and its hair there, a little below the top. */
static void default_square(const uint8_t *idx, const uint8_t *pal, int *x0, int *y0, int *s) {
    int top = -1, x, y, lo = 256, hi = -1, size;

    for (y = 0; y < 400 && top < 0; y++) {
        int n = 0;
        for (x = 0; x < 256; x++) {
            n += pal[idx[y * 256 + x] * 4 + 3] >= 0x40;
        }
        if (n >= 6) {
            top = y;
        }
    }
    if (top < 0) {
        *x0 = 58;
        *y0 = 18;
        *s = 140;
        return;
    }
    for (y = top + 30; y < top + 130 && y < 512; y++) { /* the band where the face is */
        for (x = 0; x < 256; x++) {
            if (pal[idx[y * 256 + x] * 4 + 3] >= 0x40) {
                if (x < lo) {
                    lo = x;
                }
                if (x > hi) {
                    hi = x;
                }
            }
        }
    }
    if (hi < lo) {
        lo = 48;
        hi = 208;
    }
    size = hi - lo + 1;
    if (size > 168) {
        size = 168;
    }
    if (size < 110) {
        size = 110;
    }
    *s = size;
    *x0 = (lo + hi + 1) / 2 - size / 2;
    *y0 = top + 4;
    if (*x0 < 0) {
        *x0 = 0;
    }
    if (*x0 + size > 256) {
        *x0 = 256 - size;
    }
}

/* The chip of mod i (-2: of the folder the browser looked at): a texture file with one 64 x 64 picture of 256
   colours (the layout of the select screens' own chips: 0x20 header, one entry, the pixels' upload packet, the
   CLUT's), cut from its portrait. Written to the game's buffer; returns its size (5504) or 0. */
/* The names of mod i's blasts, from the move list of its first costume (the first language that has one): up to `max`
   lines of 40 bytes at outAddr. Returns how many there are. */
int Port_ModMoves(int i, uint32_t outAddr, int max) {
    char (*out)[40] = (char (*)[40])(uintptr_t)outAddr;
    char path[700];
    uint8_t *d;
    long len;
    uint32_t n, e;
    int count = 0;

    ensure();
    if (i < 0 || i >= sModCount) {
        return 0;
    }
    mod_file(&sMod[i], 0, path, sizeof(path));
    if (path[0] == '\0' || (d = read_all(path, &len)) == NULL) {
        return 0;
    }
    memcpy(&n, d, 4);
    if (len >= 8 && n >= 52 && n < 4096 && (long)(8 + n * 4) <= len) {
        for (e = 43; e < 52 && count == 0; e++) {
            uint32_t a, b, p;

            memcpy(&a, d + 4 + e * 4, 4);
            memcpy(&b, d + 4 + (e + 1) * 4, 4);
            if (a >= b || b > (uint32_t)len) {
                continue;
            }
            p = a;
            while (p + 2 <= b && count < max) { /* a line at a time: "!F*NNNname" is a blast's name */
                uint16_t c[6] = {0, 0, 0, 0, 0, 0};
                uint32_t q = p;
                int k;

                if (d[q] == 0xFF && d[q + 1] == 0xFE) {
                    q += 2;
                }
                for (k = 0; k < 6 && q + (uint32_t)k * 2 + 2 <= b; k++) {
                    memcpy(&c[k], d + q + k * 2, 2);
                }
                if (c[0] == '!' && c[1] == 'F' && c[2] == '*' && isdigit(c[3]) && isdigit(c[4]) && isdigit(c[5])) {
                    int w = 0;

                    for (q += 12; q + 2 <= b && w < 39; q += 2) {
                        uint16_t ch;

                        memcpy(&ch, d + q, 2);
                        if (ch == '\n' || ch == '\r' || ch == 0) {
                            break;
                        }
                        out[count][w++] = ch >= 32 && ch < 127 ? (char)ch : '?';
                    }
                    out[count][w] = '\0';
                    if (w > 0) {
                        count++;
                    }
                }
                for (; q + 2 <= b; q += 2) { /* on to the next line */
                    uint16_t ch;

                    memcpy(&ch, d + q, 2);
                    if (ch == '\n') {
                        break;
                    }
                }
                p = q + 2;
            }
        }
    }
    free(d);
    return count;
}

static int chip_photo(const Mod *m, uint8_t *chip, uint8_t *pal);

/* A chip cut from the portrait (square x0, y0, s of its 256 x 512) made HD from the HD render (<mod>/hd_0.png), as the
   chip and as the reel's: 1 when made (the chip's palette then tagged for it), 0 none. */
static int chip_hd_from_render(const Mod *m, int x0, int y0, int s, uint8_t *pal) {
    char path[760];
    SDL_Surface *src, *c;
    uint8_t *chipHd, *reel;
    int W = 64 * HD_S, k, x, y, ok = 0;

    snprintf(path, sizeof(path), "%s/hd_%d.png", m->dir, HD_RENDER);
    if ((src = SDL_LoadSurface(path)) == NULL) {
        return 0;
    }
    c = SDL_ConvertSurface(src, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(src);
    if (c == NULL) {
        return 0;
    }
    k = c->w / 256; /* (the portrait is 256 x 512) */
    chipHd = k >= 2 ? malloc((size_t)W * W * 4) : NULL;
    reel = chipHd != NULL ? malloc((size_t)W * W * 4) : NULL;
    if (chipHd != NULL && reel != NULL) {
        for (y = 0; y < W; y++) { /* (each HD pixel the render's under it, on the plate's dark blue where it is empty) */
            for (x = 0; x < W; x++) {
                int sx = (x0 * k) + x * s * k / W, sy = (y0 * k) + y * s * k / W;
                uint8_t *q = &chipHd[((size_t)y * W + x) * 4];

                if (sx < 0 || sy < 0 || sx >= c->w || sy >= c->h) {
                    q[0] = 0x0C, q[1] = 0x16, q[2] = 0x48;
                } else {
                    const uint8_t *p = (const uint8_t *)c->pixels + (size_t)sy * c->pitch + (size_t)sx * 4;
                    int a = p[3] > 0x80 ? 0x80 : p[3]; /* (the file's alpha is the game's: 0x80 opaque) */

                    q[0] = (uint8_t)((p[0] * a + 0x0C * (0x80 - a)) / 0x80);
                    q[1] = (uint8_t)((p[1] * a + 0x16 * (0x80 - a)) / 0x80);
                    q[2] = (uint8_t)((p[2] * a + 0x48 * (0x80 - a)) / 0x80);
                }
                q[3] = 255;
            }
        }
        for (y = 0; y < W; y++) { /* the reel's: the face in x 8..55, y 12..51 of the chip, the rest see-through */
            for (x = 0; x < W; x++) {
                uint8_t *q = &reel[((size_t)y * W + x) * 4];
                int lx = x * 64 / W, ly = y * 64 / W;

                if (lx >= 8 && lx < 56 && ly >= 12 && ly < 52) {
                    int sx = (x - 8 * HD_S) * 64 / 48, sy = (y - 12 * HD_S) * 64 / 40;

                    sx = sx < 0 ? 0 : sx > W - 1 ? W - 1 : sx;
                    sy = sy < 0 ? 0 : sy > W - 1 ? W - 1 : sy;
                    memcpy(q, &chipHd[((size_t)sy * W + sx) * 4], 4);
                } else {
                    q[0] = q[1] = q[2] = q[3] = 0;
                }
            }
        }
        ok = hd_write(m->dir, HD_REEL, reel, W, W) && hd_write(m->dir, HD_CHIP, chipHd, W, W);
    }
    free(chipHd);
    free(reel);
    SDL_DestroySurface(c);
    if (ok) {
        hd_tag(pal, m->dir, HD_CHIP);
    }
    return ok;
}

int Port_ModChip(int i, uint32_t outAddr, int max) {
    static uint8_t idx[256 * 512], pal[1024];
    uint8_t chip[64 * 64], *o;
    Mod *m;
    int x0, y0, s, x, y, k;
    uint32_t w[16];

    ensure();
    if (i == -2) {
        m = &sBrowseMod;
    } else if (i >= 0 && i < sModCount) {
        m = &sMod[i];
    } else {
        return 0;
    }
    if (max < 5504) {
        return 0;
    }
    if (m->chipData == NULL && chip_photo(m, chip, pal)) {
        goto made; /* (its own photo: Z Warriors' small portrait) */
    }
    if (m->chipData == NULL) {
        if (!portrait(m, idx, pal)) {
            return 0;
        }
        x0 = m->chip[0];
        y0 = m->chip[1];
        s = m->chip[2];
        if (s <= 0) { /* the usual square: the head, found from where the picture begins */
            default_square(idx, pal, &x0, &y0, &s);
        }
        for (y = 0; y < 64; y++) {
            for (x = 0; x < 64; x++) {
                /* the average of the square's pixels, then the palette's nearest colour */
                int sx0 = x0 + x * s / 64, sx1 = x0 + (x + 1) * s / 64, sy0 = y0 + y * s / 64, sy1 = y0 + (y + 1) * s / 64;
                int r = 0, g = 0, b = 0, a = 0, n = 0, px, py, best = 0;
                long bestD = -1;
                if (sx1 <= sx0) {
                    sx1 = sx0 + 1;
                }
                if (sy1 <= sy0) {
                    sy1 = sy0 + 1;
                }
                for (py = sy0; py < sy1; py++) {
                    for (px = sx0; px < sx1; px++) {
                        if (px >= 0 && px < 256 && py >= 0 && py < 512) {
                            const uint8_t *c = &pal[idx[py * 256 + px] * 4];
                            r += c[0] * c[3];
                            g += c[1] * c[3];
                            b += c[2] * c[3];
                            a += c[3];
                        }
                        n++;
                    }
                }
                if (a > 0) {
                    r /= a;
                    g /= a;
                    b /= a;
                }
                a /= n;
                /* a chip has no see-through parts on every screen: what is empty in the portrait is the plate's dark blue */
                r = (r * a + 0x0C * (0x80 - a)) / 0x80;
                g = (g * a + 0x16 * (0x80 - a)) / 0x80;
                b = (b * a + 0x48 * (0x80 - a)) / 0x80;
                a = 0x80;
                for (k = 0; k < 256; k++) {
                    long d = (long)(pal[k * 4] - r) * (pal[k * 4] - r) + (long)(pal[k * 4 + 1] - g) * (pal[k * 4 + 1] - g) +
                             (long)(pal[k * 4 + 2] - b) * (pal[k * 4 + 2] - b) + (long)(pal[k * 4 + 3] - a) * (pal[k * 4 + 3] - a) * 4;
                    if (bestD < 0 || d < bestD) {
                        bestD = d;
                        best = k;
                    }
                }
                chip[y * 64 + x] = (uint8_t)best;
            }
        }
        /* (the portrait's palette carries the render's HD tag: the chip gets its own HD picture, else no tag) */
        if (pal[253 * 4 + 3] == 0x01 && pal[253 * 4] == 0x48 && !chip_hd_from_render(m, x0, y0, s, pal)) {
            memset(&pal[252 * 4], 0, 12);
        }
    made:
        o = m->chipData = calloc(1, 5504);
        w[0] = 1;
        w[1] = 8;
        memcpy(o, w, 8);
        /* the entry: pixel packet, CLUT packet (offsets in words), sizes, block counts, upload formats, TEX0 */
        w[0] = 0x60 / 4; w[1] = (0x60 + 0x1080) / 4; w[2] = 0x1080; w[3] = 0x480; w[4] = 16; w[5] = 4; w[6] = 0; w[7] = 0; w[8] = 0; w[9] = 16;
        memcpy(o + 0x20, w, 40);
        {
            uint64_t tex0 = (1ull << 14) | (0x13ull << 20) | (6ull << 26) | (6ull << 30) | (1ull << 61);
            memcpy(o + 0x20 + 0x30, &tex0, 8);
        }
        for (k = 0; k < 2; k++) { /* the two upload packets: 32 x 32 of pixels, 16 x 16 of colours */
            uint8_t *p = o + (k == 0 ? 0x60 : 0x60 + 0x1080);
            uint32_t qw = k == 0 ? 0x100 : 0x40, dim = k == 0 ? 32 : 16;
            uint32_t h[24] = {0, 0, 0, 0x50000000u | (qw + 7), 0x8003, 0x10000000, 0xE, 0, 0, 0, 0x51, 0, dim, dim, 0x52, 0, 0, 0, 0x53, 0,
                              0x8000u | qw, 0x08000000, 0, 0};
            uint32_t tail[8] = {0x8001, 0x10000000, 0xE, 0, 0, 0, 0x3F, 0};
            memcpy(p, h, 0x60);
            memcpy(p + 0x60 + qw * 16, tail, 0x20);
        }
        swizzle8(chip, o + 0x60 + 0x60, 64, 64, 1);
        for (k = 0; k < 256; k++) {
            memcpy(o + 0x60 + 0x1080 + 0x60 + csm1(k) * 4, &pal[k * 4], 4);
        }
    }
    memcpy((void *)(uintptr_t)outAddr, m->chipData, 5504);
    return 5504;
}

/* ------------------------------------------------------------------------------------------------- the browser */
static int sDirKind;          /* 0: a mod's files are listed, 1: pictures */
static DirItem sDirAll[512]; /* the folder as it is */
static int sDirAllCount;
static DirItem sDir[512];    /* what the search leaves of it */
static int sDirCount;

static int dir_cmp(const void *a, const void *b) {
    const DirItem *x = a, *y = b;
    if (x->isDir != y->isDir) {
        return y->isDir - x->isDir;
    }
    return strcasecmp(x->name, y->name);
}

/* Lists a folder ("" = the drives): folders first, then the files a mod is made of. Returns the number of entries. */
int Port_DirOpen(uint32_t pathAddr) {
    const char *path = (const char *)(uintptr_t)pathAddr;
    DirEnt *ents;
    int n, k;

    sDirCount = 0;
    if (path[0] == '\0') {
#ifdef _WIN32
        DWORD drives = GetLogicalDrives();
        for (k = 0; k < 26; k++) {
            if (drives & (1u << k)) {
                snprintf(sDir[sDirCount].name, sizeof(sDir[0].name), "%c:", 'A' + k);
                sDir[sDirCount++].isDir = 1;
            }
        }
#else
        strcpy(sDir[0].name, "/");
        sDir[0].isDir = 1;
        sDirCount = 1;
#endif
        memcpy(sDirAll, sDir, sizeof(sDir));
        sDirAllCount = sDirCount;
        return sDirCount;
    }
    ents = malloc(sizeof(DirEnt) * 2048);
    n = ents != NULL ? dir_read(path, ents, 2048) : 0;
    for (k = 0; k < n && sDirCount < 512; k++) {
        char l[300];
        if (ents[k].name[0] == '.' || ents[k].name[0] == '$' || strlen(ents[k].name) >= sizeof(sDir[0].name)) {
            continue;
        }
        lower(l, ents[k].name, sizeof(l));
        if (ents[k].isDir) {
            strcpy(sDir[sDirCount].name, ents[k].name);
            sDir[sDirCount++].isDir = 1;
        } else if (sDirKind == 1 ? (ends_with(l, ".png") || ends_with(l, ".bmp"))
                                 : (ends_with(l, ".pak") || ends_with(l, ".unk") || ends_with(l, ".cdbt") || ends_with(l, ".zip") || ends_with(l, ".7z") ||
                                    ends_with(l, ".rar"))) {
            strcpy(sDir[sDirCount].name, ents[k].name);
            sDir[sDirCount++].isDir = 0;
        }
    }
    free(ents);
    qsort(sDir, (size_t)sDirCount, sizeof(sDir[0]), dir_cmp);
    memcpy(sDirAll, sDir, sizeof(sDir));
    sDirAllCount = sDirCount;
    return sDirCount;
}

/* Keeps of the folder listed last the entries whose name has `text` in it (any case; "" = all). Returns the count. */
int Port_DirFilter(uint32_t textAddr) {
    const char *text = (const char *)(uintptr_t)textAddr;
    char want[64], name[128];
    int i;

    lower(want, text, sizeof(want));
    sDirCount = 0;
    for (i = 0; i < sDirAllCount; i++) {
        lower(name, sDirAll[i].name, sizeof(name));
        if (want[0] == '\0' || strstr(name, want) != NULL) {
            sDir[sDirCount++] = sDirAll[i];
        }
    }
    return sDirCount;
}

void Port_DirGet(int i, uint32_t outAddr) {
    PortDirEntry *out = (PortDirEntry *)(uintptr_t)outAddr;

    memset(out, 0, sizeof(*out));
    out->chara = -1;
    if (i < 0 || i >= sDirCount) {
        return;
    }
    snprintf(out->name, sizeof(out->name), "%s", sDir[i].name);
    out->isDir = sDir[i].isDir;
}

/* Looks at a folder as a mod (the browser's cursor has rested on it): fills the info and remembers it for the preview
   (Port_ModPreview(-2)) and the chip (Port_ModChip(-2)). Returns 1 when it holds a character mod, 2 when it holds
   mods of several fighters (a batch: info.charas says how many; adding it adds a mod per fighter), 0 neither. */
int Port_DirLook(uint32_t pathAddr, uint32_t outAddr) {
    const char *path = (const char *)(uintptr_t)pathAddr;
    const char *base = strrchr(path, '/');
    int is;

    if (sPreview == &sBrowseMod) {
        sPreview = NULL;
        rebuild();
    }
    free(sBrowseMod.chipData);
    sBrowseMod.chipData = NULL;
    /* a mod: it has a first costume (whatever its files are named after) or files by number for a fighter */
    is = is_dir(path) && load_mod(&sBrowseMod, path, base != NULL ? base + 1 : path) && (sBrowseMod.file[0][0] != '\0' || sBrowseMod.chara >= 0);
    if (is && sBrowseMod.charaCount > 1) {
        is = 2;
    }
    sBrowseMod.chipData = NULL;
    if (outAddr != 0) {
        fill_info(&sBrowseMod, (PortModInfo *)(uintptr_t)outAddr, -1);
    }
    return is;
}

/* ---- adding mods ----
 * Runs on a thread of its own (Port_ModImportStart), so the manager keeps drawing and shows how far it is: while it
 * runs the manager touches nothing of the library. The library is read again when it is done, on the game's thread
 * (Port_ModImportPoll). */
static volatile int sJobFiles;  /* files copied so far, */
static volatile int sJobTotal;  /* of how many (0: still counting), */
static volatile int sJobPhase;  /* 0 counting, 1 unpacking an archive, 2 copying */

static int count_files(const char *path, int depth) {
    DirEnt *ents;
    int n, k, count = 0;

    if (depth > 6 || (ents = malloc(sizeof(DirEnt) * 1024)) == NULL) {
        return 0;
    }
    n = dir_read(path, ents, 1024);
    for (k = 0; k < n && count < 100000; k++) {
        if (ents[k].isDir) {
            char sub[900];
            snprintf(sub, sizeof(sub), "%s/%s", path, ents[k].name);
            count += count_files(sub, depth + 1);
        } else {
            count++;
        }
    }
    free(ents);
    return count;
}

static int copy_file(const char *a, const char *b) {
    static uint8_t buf[1 << 16];
    FILE *in = fopen(a, "rb"), *out = in != NULL ? fopen(b, "wb") : NULL;
    size_t n;
    int ok = in != NULL && out != NULL;

    while (ok && (n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            ok = 0;
        }
    }
    if (in != NULL) {
        fclose(in);
    }
    if (out != NULL) {
        fclose(out);
    }
    sJobFiles++;
    return ok;
}

static int copy_tree(const char *from, const char *to, int depth) {
    DirEnt *ents;
    int n, k, ok = 1;

    if ((ents = malloc(sizeof(DirEnt) * 1024)) == NULL) {
        return 0;
    }
    n = dir_read(from, ents, 1024);
    make_dir(to);
    for (k = 0; k < n && ok; k++) {
        char a[900], b[900];
        snprintf(a, sizeof(a), "%s/%s", from, ents[k].name);
        snprintf(b, sizeof(b), "%s/%s", to, ents[k].name);
        if (ents[k].isDir) {
            if (depth < 5) {
                ok = copy_tree(a, b, depth + 1);
            }
        } else {
            ok = copy_file(a, b);
        }
    }
    free(ents);
    return ok;
}

/* What the last import did: the first mod it added (its name), how many it left out as already in the library, and
   whether it was asked to take the library's own folder. */
static char sImportFirst[64];
static char sImportSeen[64]; /* (the first it left out) */
static int sImportSkipped;
static int sImportSelf;

/* The mods one import has added so far, as their files' sizes: a second copy in the same batch is left out too. */
typedef struct ModSig {
    int chara;
    int rawCount;
    long size[KIND_COUNT];
} ModSig;
static ModSig sJobSig[256];
static int sJobSigCount;

static void import_reset(void) {
    sImportFirst[0] = '\0';
    sImportSeen[0] = '\0';
    sImportSkipped = 0;
    sImportSelf = 0;
    sJobSigCount = 0;
}

/* Whether a path is <data>/mods or inside it (the library cannot take its own folders). Not mods_removed. */
static int in_library(const char *path) {
    char a[700], b[700];
    size_t n, i;

    snprintf(b, sizeof(b), "%s/mods", root());
#ifdef _WIN32
    if (_fullpath(a, path, sizeof(a)) == NULL) {
        snprintf(a, sizeof(a), "%s", path);
    }
    {
        char t[700];
        snprintf(t, sizeof(t), "%s", b);
        if (_fullpath(b, t, sizeof(b)) == NULL) {
            snprintf(b, sizeof(b), "%s", t);
        }
    }
#else
    snprintf(a, sizeof(a), "%s", path);
#endif
    for (i = 0; a[i] != '\0'; i++) {
        a[i] = a[i] == '\\' ? '/' : (char)tolower((unsigned char)a[i]);
    }
    for (i = 0; b[i] != '\0'; i++) {
        b[i] = b[i] == '\\' ? '/' : (char)tolower((unsigned char)b[i]);
    }
    n = strlen(b);
    while (n > 0 && b[n - 1] == '/') {
        b[--n] = '\0';
    }
    return strncmp(a, b, n) == 0 && (a[n] == '\0' || a[n] == '/');
}

/* The mod of the library that has the very files `probe` has (same names, same sizes): its index; -2 one this import
   added already; -1 none. */
static int already_have(const Mod *probe) {
    int i, k;

    for (i = 0; i < sJobSigCount; i++) {
        int same = sJobSig[i].chara == probe->chara && sJobSig[i].rawCount == probe->rawCount;
        for (k = 0; k < KIND_COUNT && same; k++) {
            same = sJobSig[i].size[k] == (probe->file[k][0] != '\0' ? probe->size[k] : -1);
        }
        if (same && probe->rawCount == 0) {
            return -2;
        }
    }
    for (i = 0; i < sModCount; i++) {
        const Mod *m = &sMod[i];
        int same = m->rawCount == probe->rawCount && m->chara == probe->chara;
        for (k = 0; k < KIND_COUNT && same; k++) {
            const char *x = strrchr(m->file[k], '/'), *y = strrchr(probe->file[k], '/');
            x = x != NULL ? x + 1 : m->file[k];
            y = y != NULL ? y + 1 : probe->file[k];
            same = strcasecmp(x, y) == 0 && m->size[k] == probe->size[k];
        }
        for (k = 0; k < probe->rawCount && same; k++) { /* files by number: the same numbers, the same sizes */
            char a[700], b[700];
            int j;
            same = 0;
            for (j = 0; j < m->rawCount && !same; j++) {
                if (m->rawIndex[j] == probe->rawIndex[k]) {
                    snprintf(a, sizeof(a), "%s/%s", m->dir, m->rawPath[j]);
                    snprintf(b, sizeof(b), "%s/%s", probe->dir, probe->rawPath[k]);
                    same = file_size(a) == file_size(b);
                }
            }
        }
        if (same) {
            return i;
        }
    }
    return -1;
}

static void remember(const Mod *probe) {
    int k;

    if (sJobSigCount < 256) {
        ModSig *s = &sJobSig[sJobSigCount++];
        s->chara = probe->chara;
        s->rawCount = probe->rawCount;
        for (k = 0; k < KIND_COUNT; k++) {
            s->size[k] = probe->file[k][0] != '\0' ? probe->size[k] : -1;
        }
    }
}

/* Whether a file is an archive the system's tar can unpack. */
static int is_archive(const char *path) {
    char l[700];

    lower(l, path, sizeof(l));
    return ends_with(l, ".zip") || ends_with(l, ".7z") || ends_with(l, ".rar");
}

/* Deletes a folder this file made (the room archives are unpacked in), with all that is in it. */
static void remove_tree(const char *path, int depth) {
    DIR *dir = opendir(path);
    struct dirent *e;

    if (dir == NULL || depth > 12) {
        if (dir != NULL) {
            closedir(dir);
        }
        return;
    }
    while ((e = readdir(dir)) != NULL) {
        char sub[900];

        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
        if (is_dir(sub)) {
            remove_tree(sub, depth + 1);
        } else {
            remove(sub);
        }
    }
    closedir(dir);
    rmdir(path);
}

/* Unpacks an archive into a folder with the tar that comes with Windows (it reads zip, 7z and rar). 1 = done. */
static int unpack_archive(const char *file, const char *to) {
#ifdef _WIN32
    char cmd[2200], sys[300], f[700], t[700];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    int i;

    snprintf(f, sizeof(f), "%s", file);
    snprintf(t, sizeof(t), "%s", to);
    for (i = 0; f[i] != '\0'; i++) {
        if (f[i] == '/') {
            f[i] = '\\';
        }
    }
    for (i = 0; t[i] != '\0'; i++) {
        if (t[i] == '/') {
            t[i] = '\\';
        }
    }
    if (GetSystemDirectoryA(sys, sizeof(sys)) == 0) {
        return 0;
    }
    snprintf(cmd, sizeof(cmd), "\"%s\\tar.exe\" -xf \"%s\" -C \"%s\"", sys, f, t);
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return 0;
    }
    WaitForSingleObject(pi.hProcess, 180000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code == 0;
#else
    (void)file;
    (void)to;
    return 0;
#endif
}

/* Whether a folder's name says nothing of its own: only a fighter's file prefix ("goku", "Goku_0") with copy marks
   and numbers ("goku - Copy (3)"). Such a mod is named after its fighter instead. */
static int generic_name(const char *name) {
    char a[80], b[40];
    int i, j = 0, c;

    lower(a, name, sizeof(a));
    {
        char *cp;
        while ((cp = strstr(a, "copy")) != NULL) {
            memset(cp, ' ', 4);
        }
    }
    for (i = 0; a[i] != '\0' && j < 39; i++) {
        if (a[i] >= 'a' && a[i] <= 'z') {
            b[j++] = a[i];
        }
    }
    b[j] = '\0';
    if (b[0] == '\0') {
        return 1;
    }
    for (c = 0; c < CHARA_COUNT; c++) {
        char p[40], q[40];
        int k = 0;
        lower(p, kPrefix[c], sizeof(p));
        for (i = 0; p[i] != '\0'; i++) {
            if (p[i] >= 'a' && p[i] <= 'z') {
                q[k++] = p[i];
            }
        }
        q[k] = '\0';
        if (strcmp(b, q) == 0) {
            return 1;
        }
    }
    return 0;
}

/* A mod's name in the library: `want` made safe for a folder, and free ("X (2)" when X is taken). */
static void free_name(const char *want, char *name) {
    char clean[64], to[700];
    int i, n;

    snprintf(clean, sizeof(clean), "%.56s", want);
    for (i = 0; clean[i] != '\0'; i++) {
        if (strchr("\\/:*?\"<>|", clean[i]) != NULL) {
            clean[i] = '-';
        }
    }
    for (n = 0; n < 100; n++) {
        if (n == 0) {
            snprintf(name, 64, "%s", clean);
        } else {
            snprintf(name, 64, "%.50s (%d)", clean, n + 1);
        }
        snprintf(to, sizeof(to), "%s/mods/%s", root(), name);
        if (file_size(to) < 0) {
            break;
        }
    }
}

/* What an import of one mod ends in: 1 added, 2 the library had it, 0 not done. */
static int added(const char *name) {
    if (sImportFirst[0] == '\0') {
        snprintf(sImportFirst, sizeof(sImportFirst), "%s", name);
    }
    return 1;
}

static int seen(const Mod *probe, int have, char *name) {
    snprintf(name, 64, "%s", have >= 0 ? sMod[have].name : probe->name);
    if (have >= 0 && sImportSeen[0] == '\0') {
        snprintf(sImportSeen, sizeof(sImportSeen), "%s", name);
    }
    sImportSkipped++;
    return 2;
}

/* The name a mod of folder `base` with fighter `chara` gets: its folder's, unless that says nothing (then the
   fighter's, after the batch it came in when there is one). */
static void mod_name(const char *base, int chara, const char *batch, char *out) {
    char want[160];

    if (!generic_name(base) || chara < 0 || chara >= CHARA_COUNT) {
        snprintf(want, sizeof(want), "%s", base);
    } else if (batch != NULL && batch[0] != '\0') {
        snprintf(want, sizeof(want), "%.40s - %s", batch, kName[chara]);
    } else {
        snprintf(want, sizeof(want), "%s", kName[chara]);
    }
    free_name(want, out);
}

/* Copies a mod's folder into the library; name: the name it got (64 bytes). */
static int import_one(const char *path, char *name, const char *batch) {
    static Mod probe;
    const char *base = strrchr(path, '/');
    char to[700];
    int have;

    base = base != NULL ? base + 1 : path;
    if (!is_dir(path) || base[0] == '\0') {
        return 0;
    }
    if (in_library(path)) {
        sImportSelf = 1;
        return 0;
    }
    if (!load_mod(&probe, path, base)) {
        return 0;
    }
    have = already_have(&probe);
    if (have != -1) { /* (the library has these very files: they are not copied a second time) */
        return seen(&probe, have, name);
    }
    snprintf(to, sizeof(to), "%s/mods", root());
    make_dir(to);
    mod_name(base, probe.chara, batch, name);
    snprintf(to, sizeof(to), "%s/mods/%s", root(), name);
    if (!copy_tree(path, to, 0)) {
        remove_tree(to, 0); /* (half a copy would be a broken mod in the list) */
        return 0;
    }
    remember(&probe);
    return added(name);
}

/* The files lying loose in a batch's folder, a mod per fighter they are named after. Returns how many were added. */
static int import_loose(const char *path, const char *batch) {
    static Mod probe;
    DirEnt *ents;
    int n, k, c, count = 0;
    int charas[8], nc = 0;

    if ((ents = malloc(sizeof(DirEnt) * 1024)) == NULL) {
        return 0;
    }
    n = dir_read(path, ents, 1024);
    for (k = 0; k < n; k++) { /* the fighters there are */
        char l[300];
        int j;
        lower(l, ents[k].name, sizeof(l));
        c = ents[k].isDir || kind_of(l) < 0 ? -1 : chara_of(l);
        for (j = 0; j < nc && charas[j] != c; j++) {
        }
        if (c >= 0 && j == nc && nc < 8) {
            charas[nc++] = c;
        }
    }
    for (c = 0; c < nc; c++) {
        char name[64], to[700];
        int have, ok = 1;

        memset(&probe, 0, sizeof(probe));
        snprintf(probe.dir, sizeof(probe.dir), "%s", path);
        snprintf(probe.name, sizeof(probe.name), "%s", kName[charas[c]]);
        probe.chara = charas[c];
        for (k = 0; k < n; k++) {
            char l[300];
            int kind;
            lower(l, ents[k].name, sizeof(l));
            kind = ents[k].isDir ? -1 : kind_of(l);
            if (kind >= 0 && chara_of(l) == charas[c] && probe.file[kind][0] == '\0') {
                snprintf(probe.file[kind], sizeof(probe.file[kind]), "%s", ents[k].name);
                probe.size[kind] = ents[k].size;
            }
        }
        if (probe.file[0][0] == '\0') {
            continue; /* (no model of this fighter: nothing to be a mod of) */
        }
        have = already_have(&probe);
        if (have != -1) {
            seen(&probe, have, name);
            continue;
        }
        mod_name("", charas[c], batch, name);
        snprintf(to, sizeof(to), "%s/mods/%s", root(), name);
        make_dir(to);
        for (k = 0; k < KIND_COUNT && ok; k++) {
            if (probe.file[k][0] != '\0') {
                char a[900], b[900];
                snprintf(a, sizeof(a), "%s/%s", path, probe.file[k]);
                snprintf(b, sizeof(b), "%s/%s", to, probe.file[k]);
                ok = copy_file(a, b);
            }
        }
        if (!ok) {
            remove_tree(to, 0);
            continue;
        }
        remember(&probe);
        count += added(name);
    }
    free(ents);
    return count;
}

/* Whether a folder is one character mod: it has a first costume, or files by number, and all of one fighter. */
static int folder_is_mod(const char *path, int *charas) {
    static Mod probe;
    const char *base = strrchr(path, '/');
    int is;

    memset(&probe, 0, sizeof(probe));
    is = load_mod(&probe, path, base != NULL ? base + 1 : path) && (probe.file[0][0] != '\0' || probe.chara >= 0);
    *charas = probe.charaCount;
    return is && probe.charaCount <= 1;
}

/* Adds every mod under a folder: the folder itself when it is one, else its folders, and theirs (packs of mods are
   often folders of folders). A folder with files of several fighters is a batch (one fighter's forms, a folder each,
   like "Son Goku"): each of its mods is added on its own, named after the batch and its fighter when its folder's
   name says nothing ("goku - Copy (3)"), and files lying loose in it become a mod per fighter. Returns how many were
   added. */
static int import_under(const char *path, int depth, const char *batch) {
    DirEnt *ents;
    char name[64];
    const char *base = strrchr(path, '/');
    int charas, n, k, count = 0;

    base = base != NULL ? base + 1 : path;
    if (folder_is_mod(path, &charas)) {
        return import_one(path, name, batch) == 1;
    }
    if (charas > 1 && !generic_name(base)) {
        batch = base; /* (its mods are named after it) */
    }
    if (depth >= 4 || (ents = malloc(sizeof(DirEnt) * 1024)) == NULL) {
        return 0;
    }
    n = dir_read(path, ents, 1024);
    for (k = 0; k < n && count < 200; k++) {
        char sub[700];

        if (ents[k].isDir) {
            snprintf(sub, sizeof(sub), "%s/%s", path, ents[k].name);
            count += import_under(sub, depth + 1, batch);
        }
    }
    free(ents);
    if (charas > 1) {
        count += import_loose(path, batch != NULL ? batch : base);
    }
    return count;
}

/* Adds what `path` holds; all: every mod under it (a folder, a batch, an archive, a file of a mod's folder), else the
   one mod that folder is. Returns how many were added. Runs on the import's thread. */
static int import_path(const char *src, int all) {
    char path[700], name[64];
    size_t n;
    int count, i;

    snprintf(path, sizeof(path), "%s", src);
    for (i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            path[i] = '/';
        }
    }
    n = strlen(path);
    while (n > 1 && path[n - 1] == '/') {
        path[--n] = '\0';
    }
    sJobPhase = 0;
    if (!is_dir(path) && is_archive(path)) { /* an archive: unpacked beside the library, added, and the room cleared */
        char tmp[700], in[900], base[64];
        const char *b = strrchr(path, '/');
        char *dot;

        snprintf(base, sizeof(base), "%.56s", b != NULL ? b + 1 : path);
        dot = strrchr(base, '.');
        if (dot != NULL) {
            *dot = '\0';
        }
        snprintf(tmp, sizeof(tmp), "%s/mods_unpack_tmp", root());
        remove_tree(tmp, 0);
        make_dir(tmp);
        snprintf(in, sizeof(in), "%s/%s", tmp, base[0] != '\0' ? base : "mod");
        make_dir(in);
        sJobPhase = 1;
        count = 0;
        if (unpack_archive(path, in)) {
            sJobTotal = count_files(in, 0);
            sJobPhase = 2;
            count = import_under(in, 0, NULL);
        }
        remove_tree(tmp, 0);
        return count;
    }
    if (!is_dir(path)) { /* a file of a mod: the folder it is in */
        char *cut = strrchr(path, '/');

        if (cut == NULL) {
            return 0;
        }
        *cut = '\0';
    }
    /* (not the library's own folder into itself; mods_removed is fine: that is how a removed mod comes back) */
    if (in_library(path)) {
        sImportSelf = 1;
        return 0;
    }
    sJobTotal = count_files(path, 0);
    sJobPhase = 2;
    if (!all) {
        return import_one(path, name, NULL) == 1;
    }
    return import_under(path, 0, NULL);
}

int Port_ModImportInfo(uint32_t skippedAddr);

/* The two calls of before, done at once (the game waits): kept for whoever wants that. */
int Port_ModImport(uint32_t pathAddr) {
    int n;

    import_reset();
    n = import_path((const char *)(uintptr_t)pathAddr, 0);
    Port_ModRescan();
    return n > 0 || sImportSkipped > 0 ? Port_ModImportInfo(0) : -1;
}

int Port_ModImportAll(uint32_t pathAddr) {
    int n;

    import_reset();
    n = import_path((const char *)(uintptr_t)pathAddr, 1);
    Port_ModRescan();
    return n;
}

/* After an import: the first mod it added (-1 none, -2 it was the library's own folder), and through skippedAddr
   (when not 0) how many it left out because the library has them already. */
int Port_ModImportInfo(uint32_t skippedAddr) {
    if (skippedAddr != 0) {
        *(int32_t *)(uintptr_t)skippedAddr = sImportSkipped;
    }
    if (sImportSelf) {
        return -2;
    }
    return sImportFirst[0] != '\0' ? mod_by_name(sImportFirst) : sImportSeen[0] != '\0' ? mod_by_name(sImportSeen) : -1;
}

/* What was dropped on the window (gs_gpu.c hands it over), until the manager asks. */
static char sDropped[700];

void Port_DropPut(const char *path) {
    snprintf(sDropped, sizeof(sDropped), "%s", path);
}

int Port_DropTake(uint32_t outAddr) {
    if (sDropped[0] == '\0') {
        return 0;
    }
    if (outAddr != 0) {
        memcpy((void *)(uintptr_t)outAddr, sDropped, sizeof(sDropped));
    }
    sDropped[0] = '\0';
    return 1;
}

/* The packed portrait file of mod i (its own, else its fighter's) into the game's buffer; returns its size or 0. */
int Port_ModPortrait(int i, uint32_t outAddr, int max) {
    char path[700];
    const Mod *m;
    uint8_t *d;
    long len;

    ensure();
    if (i < 0 || i >= sModCount) {
        return 0;
    }
    m = &sMod[i];
    mod_path(m, 10, path, sizeof(path));
    if (path[0] == '\0') {
        if (target_of(m) < 0) {
            return 0;
        }
        snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", root(), FACE_FILE + target_of(m) - 1);
    }
    if (!portrait_ok(path)) {
        return 0;
    }
    d = read_all(path, &len);
    if (d == NULL) {
        return 0;
    }
    if (len > max) {
        free(d);
        return 0;
    }
    memcpy((void *)(uintptr_t)outAddr, d, (size_t)len);
    free(d);
    return (int)len;
}

/* ------------------------------------------------------------------------------------------------- a portrait of one's own
 * The large picture of a mod (256 x 512, 256 colours, packed: what the select screens show) can be made in the
 * manager: from the mod's own portrait or from a picture file (PNG or BMP), placed and sized inside the frame.
 * mod.txt keeps where (art=x,y,zoom and artsrc=<file in the mod's folder>); the result is <mod>/portrait_custom.bin,
 * which then is the mod's portrait everywhere (and what its chip is cut from).
 */
#include <SDL3/SDL.h>

static void SDLCALL pick_done(void *user, const char *const *files, int filter) {
    (void)user;
    (void)filter;
    if (files != NULL && files[0] != NULL) {
        Port_DropPut(files[0]); /* (taken as if dropped on the window: Port_DropTake) */
    }
}

/* A picture file chosen in the system's open dialog arrives as a dropped one (Port_DropTake), later. */
void Port_PickPicture(void) {
    static const SDL_DialogFileFilter filters[] = {{"Pictures", "png;jpg;jpeg;bmp"}, {"All files", "*"}};

    SDL_ShowOpenFileDialog(pick_done, NULL, NULL, filters, 2, NULL, false);
}

#define ART_W 256
#define ART_H 512
#define ART_PACK_MAX 0x16800 /* what the select screens' buffers take */

/* The picture a mod's portrait is made from, as RGBA (8-bit alpha): its picture file, else its own portrait, else
   its fighter's. malloc'ed; NULL = none. */
static uint8_t *art_source(const Mod *m, int *w, int *h) {
    static uint8_t idx[ART_W * ART_H], pal[1024];
    uint8_t *out;
    int i;

    if (m->artSrc[0] != '\0') {
        char path[700];
        SDL_Surface *s, *c;

        snprintf(path, sizeof(path), "%s/%s", m->dir, m->artSrc);
        s = SDL_LoadSurface(path);
        if (s != NULL) {
            c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(s);
            if (c != NULL && c->w > 0 && c->h > 0 && c->w <= 8192 && c->h <= 8192) {
                int y;
                out = malloc((size_t)c->w * (size_t)c->h * 4);
                for (y = 0; y < c->h; y++) {
                    memcpy(out + (size_t)y * (size_t)c->w * 4, (const uint8_t *)c->pixels + (size_t)y * (size_t)c->pitch, (size_t)c->w * 4);
                }
                *w = c->w;
                *h = c->h;
                SDL_DestroySurface(c);
                return out;
            }
            if (c != NULL) {
                SDL_DestroySurface(c);
            }
        }
    }
    {
        static Mod plain; /* without what was made here before */
        plain = *m;
        plain.artSet = 0;
        if (!portrait(&plain, idx, pal)) {
            return NULL;
        }
    }
    out = malloc(ART_W * ART_H * 4);
    for (i = 0; i < ART_W * ART_H; i++) {
        const uint8_t *c = &pal[idx[i] * 4];
        out[i * 4] = c[0];
        out[i * 4 + 1] = c[1];
        out[i * 4 + 2] = c[2];
        out[i * 4 + 3] = (uint8_t)(c[3] >= 0x80 ? 255 : c[3] * 2);
    }
    *w = ART_W;
    *h = ART_H;
    return out;
}

/* The source in the frame (W x H, the game's 256 x 512 or a multiple): whole and centred at zoom 100, then `zoom`
   percent of that, moved by (ox, oy) of the game's pixels. */
static void art_render_n(const uint8_t *src, int sw, int sh, int ox, int oy, int zoom, int zoomY, uint8_t *out, int W, int H) {
    double base = (double)W / sw < (double)H / sh ? (double)W / sw : (double)H / sh;
    double scale = base * zoom / 100.0, scaleY = base * zoomY / 100.0;
    double left = W / 2.0 - sw * scale / 2.0 + ox * (W / ART_W), top = H / 2.0 - sh * scaleY / 2.0 + oy * (W / ART_W);
    int x, y;

    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            double fx0 = (x - left) / scale, fx1 = (x + 1 - left) / scale, fy0 = (y - top) / scaleY, fy1 = (y + 1 - top) / scaleY;
            int x0 = (int)fx0, x1 = (int)fx1, y0 = (int)fy0, y1 = (int)fy1, px, py;
            unsigned r = 0, g = 0, b = 0, a = 0, n = 0;
            uint8_t *o = &out[((size_t)y * W + x) * 4];

            if (fx1 <= 0 || fy1 <= 0 || fx0 >= sw || fy0 >= sh) {
                o[0] = o[1] = o[2] = o[3] = 0;
                continue;
            }
            if (x1 <= x0) {
                x1 = x0 + 1;
            }
            if (y1 <= y0) {
                y1 = y0 + 1;
            }
            for (py = y0; py < y1; py++) {
                for (px = x0; px < x1; px++) {
                    if (px >= 0 && px < sw && py >= 0 && py < sh) {
                        const uint8_t *c = &src[((size_t)py * (size_t)sw + (size_t)px) * 4];
                        r += c[0] * c[3];
                        g += c[1] * c[3];
                        b += c[2] * c[3];
                        a += c[3];
                    }
                    n++;
                }
            }
            if (a > 0) {
                o[0] = (uint8_t)(r / a);
                o[1] = (uint8_t)(g / a);
                o[2] = (uint8_t)(b / a);
            } else {
                o[0] = o[1] = o[2] = 0;
            }
            o[3] = (uint8_t)(a / n);
        }
    }
}

static void art_render(const uint8_t *src, int sw, int sh, int ox, int oy, int zoom, int zoomY, uint8_t *out) {
    art_render_n(src, sw, sh, ox, oy, zoom, zoomY, out, ART_W, ART_H);
}

/* 256 colours for a picture with see-through edges: `colours` colours (median cut over 5 bits a channel) times
   `levels` steps of alpha, index 0 empty. pal: 256 RGBA with the PS2's alpha. */
typedef struct ArtBox {
    int lo[3], hi[3];
    long count;
} ArtBox;

static void art_quantize(const uint8_t *rgba, uint8_t *idx, uint8_t *pal, int colours, int levels) {
    static int hist[32768];
    static uint8_t boxOf[32768];
    static ArtBox box[256];
    long sum[256][4];
    int n = 1, i, k, c;

    memset(hist, 0, sizeof(hist));
    for (i = 0; i < ART_W * ART_H; i++) {
        if (rgba[i * 4 + 3] >= 24) {
            hist[(rgba[i * 4] >> 3) << 10 | (rgba[i * 4 + 1] >> 3) << 5 | (rgba[i * 4 + 2] >> 3)]++;
        }
    }
    for (c = 0; c < 3; c++) {
        box[0].lo[c] = 0;
        box[0].hi[c] = 31;
    }
    box[0].count = 1;
    while (n < colours) {
        /* the box with most pixels that can still be cut, along its longest side, at its median */
        int best = -1, axis = 0, len = 0, cut, v[3];
        long most = 0, total = 0, half = 0;
        ArtBox *b;

        for (k = 0; k < n; k++) {
            long cnt = 0;
            int lo[3] = {32, 32, 32}, hi[3] = {-1, -1, -1};
            for (v[0] = box[k].lo[0]; v[0] <= box[k].hi[0]; v[0]++) {
                for (v[1] = box[k].lo[1]; v[1] <= box[k].hi[1]; v[1]++) {
                    for (v[2] = box[k].lo[2]; v[2] <= box[k].hi[2]; v[2]++) {
                        int hcount = hist[v[0] << 10 | v[1] << 5 | v[2]];
                        if (hcount != 0) {
                            cnt += hcount;
                            for (c = 0; c < 3; c++) {
                                if (v[c] < lo[c]) {
                                    lo[c] = v[c];
                                }
                                if (v[c] > hi[c]) {
                                    hi[c] = v[c];
                                }
                            }
                        }
                    }
                }
            }
            box[k].count = cnt;
            if (cnt > 0) { /* shrunk to what it holds */
                for (c = 0; c < 3; c++) {
                    box[k].lo[c] = lo[c];
                    box[k].hi[c] = hi[c];
                }
            }
            if (box[k].hi[0] > box[k].lo[0] || box[k].hi[1] > box[k].lo[1] || box[k].hi[2] > box[k].lo[2]) {
                /* (many pixels or a wide spread of colours: a small dark outline is not averaged into its neighbour) */
                long side = box[k].hi[0] - box[k].lo[0], weight;
                for (c = 1; c < 3; c++) {
                    if (box[k].hi[c] - box[k].lo[c] > side) {
                        side = box[k].hi[c] - box[k].lo[c];
                    }
                }
                weight = cnt > 0 ? (cnt / 64 + 1) * side * side : 0;
                if (weight > most) {
                    most = weight;
                    best = k;
                }
            }
        }
        if (best < 0) {
            break;
        }
        b = &box[best];
        for (c = 0; c < 3; c++) {
            if (b->hi[c] - b->lo[c] > len) {
                len = b->hi[c] - b->lo[c];
                axis = c;
            }
        }
        total = b->count;
        cut = b->lo[axis];
        for (cut = b->lo[axis]; cut < b->hi[axis]; cut++) {
            int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
            v[axis] = cut;
            for (v[a1] = b->lo[a1]; v[a1] <= b->hi[a1]; v[a1]++) {
                for (v[a2] = b->lo[a2]; v[a2] <= b->hi[a2]; v[a2]++) {
                    half += hist[v[0] << 10 | v[1] << 5 | v[2]];
                }
            }
            if (half * 2 >= total) {
                break;
            }
        }
        if (cut >= b->hi[axis]) { /* (most of it in the last slice: that slice alone is the second box) */
            cut = b->hi[axis] - 1;
        }
        box[n] = *b;
        b->hi[axis] = cut;
        box[n].lo[axis] = cut + 1;
        n++;
    }
    memset(sum, 0, sizeof(sum));
    memset(boxOf, 0, sizeof(boxOf));
    for (k = 0; k < n; k++) {
        int v[3];
        for (v[0] = box[k].lo[0]; v[0] <= box[k].hi[0]; v[0]++) {
            for (v[1] = box[k].lo[1]; v[1] <= box[k].hi[1]; v[1]++) {
                for (v[2] = box[k].lo[2]; v[2] <= box[k].hi[2]; v[2]++) {
                    boxOf[v[0] << 10 | v[1] << 5 | v[2]] = (uint8_t)k;
                }
            }
        }
    }
    for (i = 0; i < ART_W * ART_H; i++) {
        if (rgba[i * 4 + 3] >= 24) {
            k = boxOf[(rgba[i * 4] >> 3) << 10 | (rgba[i * 4 + 1] >> 3) << 5 | (rgba[i * 4 + 2] >> 3)];
            sum[k][0] += rgba[i * 4];
            sum[k][1] += rgba[i * 4 + 1];
            sum[k][2] += rgba[i * 4 + 2];
            sum[k][3]++;
        }
    }
    memset(pal, 0, 1024);
    for (k = 0; k < n; k++) {
        int l;
        for (l = 0; l < levels; l++) {
            uint8_t *p = &pal[(1 + k * levels + l) * 4];
            if (sum[k][3] > 0) {
                p[0] = (uint8_t)(sum[k][0] / sum[k][3]);
                p[1] = (uint8_t)(sum[k][1] / sum[k][3]);
                p[2] = (uint8_t)(sum[k][2] / sum[k][3]);
            }
            p[3] = (uint8_t)((l + 1) * 0x80 / levels);
        }
    }
    for (i = 0; i < ART_W * ART_H; i++) {
        int l = (rgba[i * 4 + 3] * levels + 127) / 255;
        if (l == 0 || rgba[i * 4 + 3] < 24) {
            idx[i] = 0;
        } else {
            k = boxOf[(rgba[i * 4] >> 3) << 10 | (rgba[i * 4 + 1] >> 3) << 5 | (rgba[i * 4 + 2] >> 3)];
            idx[i] = (uint8_t)(1 + k * levels + (l - 1));
        }
    }
}

/* Philip Gage's byte-pair compression, in the layout bpe_decode (and the game) expands. malloc'ed. */
static uint8_t *bpe_pack(const uint8_t *data, uint32_t len, uint32_t *outLen) {
    static int count[65536];
    uint8_t *out = malloc(len + len / 8 + 4096), *o = out + 8;
    uint32_t at;

    for (at = 0; at < len; at += 4096) {
        uint8_t buf[4096], tmp[4096], left[256], right[256], used[256];
        int n = (int)(len - at < 4096 ? len - at : 4096), i, c;

        memcpy(buf, data + at, (size_t)n);
        memset(used, 0, sizeof(used));
        for (i = 0; i < 256; i++) {
            left[i] = (uint8_t)i;
            right[i] = 0;
        }
        for (i = 0; i < n; i++) {
            used[buf[i]] = 1;
        }
        for (;;) {
            int best = -1, bestN = 3, code = -1, m = 0;

            for (i = 0; i < 256 && code < 0; i++) {
                if (!used[i]) {
                    code = i;
                }
            }
            if (code < 0 || n < 4) {
                break;
            }
            for (i = 0; i + 1 < n; i++) {
                int p = buf[i] << 8 | buf[i + 1];
                if (++count[p] > bestN) {
                    bestN = count[p];
                    best = p;
                }
            }
            for (i = 0; i + 1 < n; i++) {
                count[buf[i] << 8 | buf[i + 1]] = 0;
            }
            if (best < 0) {
                break;
            }
            for (i = 0; i < n; i++) {
                if (i + 1 < n && buf[i] == (best >> 8) && buf[i + 1] == (best & 255)) {
                    tmp[m++] = (uint8_t)code;
                    i++;
                } else {
                    tmp[m++] = buf[i];
                }
            }
            memcpy(buf, tmp, (size_t)m);
            n = m;
            left[code] = (uint8_t)(best >> 8);
            right[code] = (uint8_t)(best & 255);
            used[code] = 1;
        }
        c = 0;
        while (c < 256) { /* the pair table: runs of plain bytes as counts above 127, pairs spelt out */
            int run, b;

            if (left[c] == c) {
                run = 1;
                c++;
                while (run < 127 && c < 256 && left[c] == c) {
                    run++;
                    c++;
                }
                *o++ = (uint8_t)(run + 127);
                run = 0;
                if (c == 256) {
                    break;
                }
            } else {
                run = 0;
                b = c++;
                while (run < 127 && c < 256 && left[c] != c) {
                    run++;
                    c++;
                }
                *o++ = (uint8_t)run;
                c = b;
            }
            for (i = 0; i <= run; i++) {
                *o++ = left[c];
                if (left[c] != c) {
                    *o++ = right[c];
                }
                c++;
            }
        }
        *o++ = (uint8_t)(n >> 8);
        *o++ = (uint8_t)n;
        memcpy(o, buf, (size_t)n);
        o += n;
    }
    {
        uint32_t pack = (uint32_t)(o - out - 8);
        memcpy(out, &len, 4);
        memcpy(out + 4, &pack, 4);
        *outLen = pack + 8;
    }
    return out;
}

/* The unpacked portrait file of a picture: the fighter's own file with the pixels and colours exchanged.
   malloc'ed, *size its length; NULL = no file to start from. */
static uint8_t *art_file(const Mod *m, const uint8_t *idx, const uint8_t *pal, uint32_t *size) {
    char path[700];
    uint8_t *pk, *raw;
    long len;
    uint32_t f[10];
    int t = target_of(m), i;

    snprintf(path, sizeof(path), "%s/pzs3us1/%05d.bin", root(), FACE_FILE + (t >= 0 ? t : 0) - 1);
    pk = read_all(path, &len);
    if (pk == NULL) {
        return NULL;
    }
    raw = bpe_decode(pk, len, size);
    free(pk);
    if (raw == NULL || *size < 0x60) {
        free(raw);
        return NULL;
    }
    memcpy(f, raw + 0x20, sizeof(f));
    if (f[0] * 4 + 0x60 + ART_W * ART_H > *size || f[1] * 4 + 0x60 + 1024 > *size) {
        free(raw);
        return NULL;
    }
    {
        static uint8_t tmp[ART_W * ART_H];
        memcpy(tmp, idx, sizeof(tmp));
        swizzle8(tmp, raw + f[0] * 4 + 0x60, ART_W, ART_H, 1);
    }
    for (i = 0; i < 256; i++) {
        memcpy(raw + f[1] * 4 + 0x60 + csm1(i) * 4, &pal[i * 4], 4);
    }
    return raw;
}

/* The stage of the manager's test fight: the first of the select's grid, grass under an open sky
   ($BT3_TEST_STAGE picks another). */
int Port_ModTestStage(void) {
    const char *s = getenv("BT3_TEST_STAGE");

    return s != NULL ? atoi(s) : 0;
}

static Mod *art_mod(int i) {
    ensure();
    return i >= 0 && i < sModCount ? &sMod[i] : NULL;
}

/* The picture mod i's portrait is made from, whole in the frame (zoom 100, not moved), as an unpacked portrait
   file in the game's buffer: what the manager's screen moves and sizes. Returns its size or 0. */
int Port_ModArtBase(int i, uint32_t outAddr, int max) {
    static uint8_t rgba[ART_W * ART_H * 4], idx[ART_W * ART_H], pal[1024];
    Mod *m = art_mod(i);
    uint8_t *src, *raw;
    uint32_t size;
    int w, h;

    if (m == NULL || (src = art_source(m, &w, &h)) == NULL) {
        return 0;
    }
    art_render(src, w, h, 0, 0, 100, 100, rgba);
    free(src);
    art_quantize(rgba, idx, pal, 84, 3);
    raw = art_file(m, idx, pal, &size);
    if (raw == NULL || (int)size > max) {
        free(raw);
        return 0;
    }
    memcpy((void *)(uintptr_t)outAddr, raw, size);
    free(raw);
    return (int)size;
}

/* Makes mod i's portrait: its picture at (x, y), `zoom` percent wide and `zoomY` high. Returns 1 when
   <mod>/portrait_custom.bin was written. */
int Port_ModArtSet2(int i, int x, int y, int zoom, int zoomY);

/* Mod i's portrait: its picture filling the frame (Port_ModArtSet2 with four numbers: Z Warriors). */
int Port_ModArtFit(int i) {
    return Port_ModArtSet2(i, 0, 0, 100, 100);
}

int Port_ModArtSet2(int i, int x, int y, int zoom, int zoomY) {
    static uint8_t rgba[ART_W * ART_H * 4], idx[ART_W * ART_H], pal[1024];
    static const int tries[4] = {83, 56, 36, 20}; /* (83 x 3 + 1: palette entries 252..254 kept for the HD tag) */
    Mod *m = art_mod(i);
    uint8_t *src;
    int w, h, k, ok = 0, hdOk = 0, hk;

    if (m == NULL || (src = art_source(m, &w, &h)) == NULL) {
        return 0;
    }
    art_render(src, w, h, x, y, zoom, zoomY, rgba);
    hk = h / ART_H; /* the HD render: as many times the game's size as the picture has pixels for (2 to 4) */
    hk = hk > 4 ? 4 : hk;
    if (hk >= 2) {
        uint8_t *big = malloc((size_t)ART_W * hk * ART_H * hk * 4);

        if (big != NULL) {
            art_render_n(src, w, h, x, y, zoom, zoomY, big, ART_W * hk, ART_H * hk);
            hdOk = hd_write(m->dir, HD_RENDER, big, ART_W * hk, ART_H * hk);
            free(big);
        }
    } else {
        char path[760];

        snprintf(path, sizeof(path), "%s/hd_%d.png", m->dir, HD_RENDER);
        remove(path);
    }
    free(src);
    for (k = 0; k < 4 && !ok; k++) { /* fewer colours until the packed file fits the game's buffers */
        uint32_t size, packed;
        uint8_t *raw, *pk;

        art_quantize(rgba, idx, pal, tries[k], 3);
        if (hdOk) {
            hd_tag(pal, m->dir, HD_RENDER);
        }
        raw = art_file(m, idx, pal, &size);
        if (raw == NULL) {
            break;
        }
        pk = bpe_pack(raw, size, &packed);
        free(raw);
        if (packed <= ART_PACK_MAX) {
            char path[700];
            FILE *fp;

            snprintf(path, sizeof(path), "%s/portrait_custom.bin", m->dir);
            fp = fopen(path, "wb");
            if (fp != NULL) {
                ok = fwrite(pk, 1, packed, fp) == packed;
                fclose(fp);
            }
        }
        free(pk);
    }
    if (ok) {
        m->artSet = 1;
        m->art[0] = x;
        m->art[1] = y;
        m->art[2] = zoom;
        m->art[3] = zoomY;
        free(m->chipData);
        m->chipData = NULL;
        write_settings(m);
        rebuild();
    }
    return ok;
}

int Port_ModArtSet(int i, int x, int y, int zoom) {
    return Port_ModArtSet2(i, x, y, zoom, zoom);
}

/* Back to the portrait the mod came with. */
void Port_ModArtReset(int i) {
    Mod *m = art_mod(i);

    if (m != NULL) {
        m->artSet = 0;
        m->artSrc[0] = '\0';
        m->art[0] = m->art[1] = 0;
        m->art[2] = m->art[3] = 100;
        free(m->chipData);
        m->chipData = NULL;
        write_settings(m);
        rebuild();
    }
}

/* A picture file (PNG or BMP) as what mod i's portrait is made from: copied into the mod's folder. */
int Port_ModArtSource(int i, uint32_t pathAddr) {
    const char *path = (const char *)(uintptr_t)pathAddr;
    Mod *m = art_mod(i);
    const char *ext = strrchr(path, '.');
    char to[700], name[64];
    uint8_t *d;
    long len;
    FILE *fp;
    SDL_Surface *s;

    if (m == NULL || ext == NULL || strlen(ext) > 5) {
        return 0;
    }
    s = SDL_LoadSurface(path);
    if (s == NULL) {
        return 0; /* not a picture this build reads */
    }
    SDL_DestroySurface(s);
    d = read_all(path, &len);
    if (d == NULL) {
        return 0;
    }
    snprintf(name, sizeof(name), "portrait_src%s", ext);
    snprintf(to, sizeof(to), "%s/%s", m->dir, name);
    fp = fopen(to, "wb");
    if (fp == NULL) {
        free(d);
        return 0;
    }
    fwrite(d, 1, (size_t)len, fp);
    fclose(fp);
    free(d);
    snprintf(m->artSrc, sizeof(m->artSrc), "%s", name);
    write_settings(m);
    return 1;
}

/* What the browser lists besides folders: 0 the files of a mod, 1 pictures. */
void Port_DirKind(int kind) {
    sDirKind = kind;
}

/* ---- pictures: a PNG file written, a picture brought to a few colours (the renders) ---- */
/* A PNG file, written without packing (stored blocks): every program reads it. */
static uint32_t png_crc(uint32_t crc, const uint8_t *p, size_t n) {
    static uint32_t table[256];
    size_t i;
    int k;

    if (table[1] == 0) {
        for (i = 0; i < 256; i++) {
            uint32_t c = (uint32_t)i;

            for (k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            table[i] = c;
        }
    }
    for (i = 0; i < n; i++) {
        crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static void png_chunk(FILE *fp, const char *type, const uint8_t *data, uint32_t n) {
    uint8_t h[8] = {(uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n, (uint8_t)type[0], (uint8_t)type[1], (uint8_t)type[2], (uint8_t)type[3]};
    uint32_t crc = png_crc(0xFFFFFFFFu, h + 4, 4);
    uint8_t c[4];

    if (n > 0) {
        crc = png_crc(crc, data, n);
    }
    crc ^= 0xFFFFFFFFu;
    c[0] = (uint8_t)(crc >> 24);
    c[1] = (uint8_t)(crc >> 16);
    c[2] = (uint8_t)(crc >> 8);
    c[3] = (uint8_t)crc;
    fwrite(h, 1, 8, fp);
    if (n > 0) {
        fwrite(data, 1, n, fp);
    }
    fwrite(c, 1, 4, fp);
}

static int png_write(const char *path, const uint8_t *rgba, int w, int h) {
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    FILE *fp = fopen(path, "wb");
    uint8_t ihdr[13] = {(uint8_t)(w >> 24), (uint8_t)(w >> 16), (uint8_t)(w >> 8), (uint8_t)w, (uint8_t)(h >> 24), (uint8_t)(h >> 16), (uint8_t)(h >> 8), (uint8_t)h, 8, 6, 0, 0, 0};
    size_t rawLen = (size_t)h * ((size_t)w * 4 + 1), pos = 0, o = 0;
    uint8_t *raw, *z;
    uint32_t a = 1, b = 0;
    int y;
    size_t i;

    if (fp == NULL) {
        return 0;
    }
    raw = malloc(rawLen);
    for (y = 0; y < h; y++) {
        raw[pos++] = 0;
        memcpy(raw + pos, rgba + (size_t)y * w * 4, (size_t)w * 4);
        pos += (size_t)w * 4;
    }
    z = malloc(rawLen + rawLen / 65535 * 5 + 16);
    z[o++] = 0x78;
    z[o++] = 0x01;
    for (pos = 0; pos < rawLen;) {
        size_t n = rawLen - pos > 65535 ? 65535 : rawLen - pos;

        z[o++] = pos + n == rawLen;
        z[o++] = (uint8_t)n;
        z[o++] = (uint8_t)(n >> 8);
        z[o++] = (uint8_t)~n;
        z[o++] = (uint8_t)(~n >> 8);
        memcpy(z + o, raw + pos, n);
        o += n;
        pos += n;
    }
    for (i = 0; i < rawLen; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    z[o++] = (uint8_t)(b >> 8);
    z[o++] = (uint8_t)b;
    z[o++] = (uint8_t)(a >> 8);
    z[o++] = (uint8_t)a;
    fwrite(sig, 1, 8, fp);
    png_chunk(fp, "IHDR", ihdr, 13);
    png_chunk(fp, "IDAT", z, (uint32_t)o);
    png_chunk(fp, "IEND", NULL, 0);
    fclose(fp);
    free(raw);
    free(z);
    return 1;
}

/* `count` colours for n pixels (median cut over 5 bits a channel); see-through pixels take entry `clear` (-1: none). */
static void quantize_any(const uint8_t *rgba, int n, int count, int clear, uint8_t *idx, uint8_t *pal) {
    static int hist[32768];
    static uint8_t boxOf[32768];
    static ArtBox box[256];
    static long sum[256][4];
    int boxes = 1, i, k, c, usable = clear >= 0 ? count - 1 : count;

    memset(hist, 0, sizeof(hist));
    for (i = 0; i < n; i++) {
        if (rgba[i * 4 + 3] >= 128 || clear < 0) {
            hist[(rgba[i * 4] >> 3) << 10 | (rgba[i * 4 + 1] >> 3) << 5 | (rgba[i * 4 + 2] >> 3)]++;
        }
    }
    for (c = 0; c < 3; c++) {
        box[0].lo[c] = 0;
        box[0].hi[c] = 31;
    }
    while (boxes < usable) {
        int best = -1, axis = 0, len = 0, cut, v[3];
        long most = 0, total, half = 0;
        ArtBox *b;

        for (k = 0; k < boxes; k++) {
            long cnt = 0, side, weight;
            int lo[3] = {32, 32, 32}, hi[3] = {-1, -1, -1};

            for (v[0] = box[k].lo[0]; v[0] <= box[k].hi[0]; v[0]++) {
                for (v[1] = box[k].lo[1]; v[1] <= box[k].hi[1]; v[1]++) {
                    for (v[2] = box[k].lo[2]; v[2] <= box[k].hi[2]; v[2]++) {
                        int hc = hist[v[0] << 10 | v[1] << 5 | v[2]];

                        if (hc != 0) {
                            cnt += hc;
                            for (c = 0; c < 3; c++) {
                                if (v[c] < lo[c]) {
                                    lo[c] = v[c];
                                }
                                if (v[c] > hi[c]) {
                                    hi[c] = v[c];
                                }
                            }
                        }
                    }
                }
            }
            box[k].count = cnt;
            if (cnt > 0) {
                for (c = 0; c < 3; c++) {
                    box[k].lo[c] = lo[c];
                    box[k].hi[c] = hi[c];
                }
            }
            side = box[k].hi[0] - box[k].lo[0];
            for (c = 1; c < 3; c++) {
                if (box[k].hi[c] - box[k].lo[c] > side) {
                    side = box[k].hi[c] - box[k].lo[c];
                }
            }
            weight = cnt > 0 && side > 0 ? (cnt / 16 + 1) * side * side : 0;
            if (weight > most) {
                most = weight;
                best = k;
            }
        }
        if (best < 0) {
            break;
        }
        b = &box[best];
        for (c = 0; c < 3; c++) {
            if (b->hi[c] - b->lo[c] > len) {
                len = b->hi[c] - b->lo[c];
                axis = c;
            }
        }
        total = b->count;
        for (cut = b->lo[axis]; cut < b->hi[axis]; cut++) {
            int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;

            v[axis] = cut;
            for (v[a1] = b->lo[a1]; v[a1] <= b->hi[a1]; v[a1]++) {
                for (v[a2] = b->lo[a2]; v[a2] <= b->hi[a2]; v[a2]++) {
                    half += hist[v[0] << 10 | v[1] << 5 | v[2]];
                }
            }
            if (half * 2 >= total) {
                break;
            }
        }
        if (cut >= b->hi[axis]) {
            cut = b->hi[axis] - 1;
        }
        box[boxes] = *b;
        b->hi[axis] = cut;
        box[boxes].lo[axis] = cut + 1;
        boxes++;
    }
    memset(sum, 0, sizeof(sum));
    memset(boxOf, 0, sizeof(boxOf));
    for (k = 0; k < boxes; k++) {
        int v[3];

        for (v[0] = box[k].lo[0]; v[0] <= box[k].hi[0]; v[0]++) {
            for (v[1] = box[k].lo[1]; v[1] <= box[k].hi[1]; v[1]++) {
                for (v[2] = box[k].lo[2]; v[2] <= box[k].hi[2]; v[2]++) {
                    boxOf[v[0] << 10 | v[1] << 5 | v[2]] = (uint8_t)k;
                }
            }
        }
    }
    for (i = 0; i < n; i++) {
        if (rgba[i * 4 + 3] >= 128 || clear < 0) {
            k = boxOf[(rgba[i * 4] >> 3) << 10 | (rgba[i * 4 + 1] >> 3) << 5 | (rgba[i * 4 + 2] >> 3)];
            sum[k][0] += rgba[i * 4];
            sum[k][1] += rgba[i * 4 + 1];
            sum[k][2] += rgba[i * 4 + 2];
            sum[k][3]++;
        }
    }
    memset(pal, 0, (size_t)count * 4);
    for (k = 0; k < boxes; k++) {
        int slot = clear >= 0 && k >= clear ? k + 1 : k;

        if (sum[k][3] > 0 && slot < count) {
            pal[slot * 4] = (uint8_t)(sum[k][0] / sum[k][3]);
            pal[slot * 4 + 1] = (uint8_t)(sum[k][1] / sum[k][3]);
            pal[slot * 4 + 2] = (uint8_t)(sum[k][2] / sum[k][3]);
            pal[slot * 4 + 3] = 0x80;
        }
    }
    for (i = 0; i < n; i++) {
        if (rgba[i * 4 + 3] < 128 && clear >= 0) {
            idx[i] = (uint8_t)clear;
        } else {
            k = boxOf[(rgba[i * 4] >> 3) << 10 | (rgba[i * 4 + 1] >> 3) << 5 | (rgba[i * 4 + 2] >> 3)];
            idx[i] = (uint8_t)(clear >= 0 && k >= clear ? k + 1 : k);
        }
    }
}

/* ---- lists of names (<data>/modmgr/*.txt): z3_paklist.txt the files inside the game's packs, moves.txt the
 * records of a fighter's melee file; items.txt, maps.txt, songs.txt what their names say ---- */
typedef struct TextList {
    char *text;      /* the file, its line ends made string ends */
    char **line;
    int count;
} TextList;

static TextList sList[5];
static const char *kListFile[5] = {"z3_paklist.txt", "moves.txt", "items.txt", "maps.txt", "songs.txt"};
static TextList *text_list(int n) {
    TextList *l = &sList[n];

    if (l->text == NULL) {
        char path[700];
        long len, i;
        int cap = 0;

        snprintf(path, sizeof(path), "%s/modmgr/%s", root(), kListFile[n]);
        l->text = (char *)read_all(path, &len);
        if (l->text == NULL) {
            l->text = calloc(1, 4);
            len = 0;
        } else {
            l->text = realloc(l->text, (size_t)len + 2);
            l->text[len] = '\0';
        }
        { /* (line ends of either kind: one string end per line) */
            long w = 0;

            for (i = 0; i < len; i++) {
                if (l->text[i] != '\r') {
                    l->text[w++] = l->text[i];
                }
            }
            len = w;
            l->text[len] = '\0';
        }
        for (i = 0; i < len; i++) {
            if (i == 0 || l->text[i - 1] == '\0') {
                if (l->count >= cap) {
                    cap = cap ? cap * 2 : 512;
                    l->line = realloc(l->line, sizeof(char *) * (size_t)cap);
                }
                l->line[l->count++] = &l->text[i];
            }
            if (l->text[i] == '\n') {
                l->text[i] = '\0';
            }
        }
    }
    return l;
}

/* Line i of a list (1 moves, 2 items, 3 maps, 4 songs): 48 bytes at outAddr. 1 = there is one. */
int Port_ModListName(int list, int i, uint32_t outAddr) {
    char *out = (char *)(uintptr_t)outAddr;
    TextList *l;

    out[0] = '\0';
    if (list < 1 || list > 4) {
        return 0;
    }
    l = text_list(list);
    if (i < 0 || i >= l->count) {
        return 0;
    }
    snprintf(out, 48, "%s", l->line[i]);
    return out[0] != '\0';
}

/* ---- what the Scenario Builder and the fight panels need of this library ---- */
const char *port_mods_root(void) {
    return root();
}

const char *port_mods_chara_name(int chara) {
    return chara >= 0 && chara < CHARA_COUNT ? kName[chara] : NULL;
}

int port_mods_mkdir(const char *path) {
    return make_dir(path);
}

/* The fighter a costume code plays as in a fight: whose voice and sounds it has (a new fighter's own choice, else
   the character it is). */
int Port_SkinVoice(int chara, int code) {
    Mod *m;

    if (code < 8) {
        return chara;
    }
    ensure();
    m = skin_mod((code - 8) / 4);
    return m != NULL && m->voice >= 0 && m->voice < CHARA_COUNT ? m->voice : chara;
}

/* The Z-items a costume code's fighter always has (4 ids into outAddr, 0 none); how many. */
int Port_SkinItems(int code, uint32_t outAddr) {
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    Mod *m;
    int k, n = 0;

    for (k = 0; k < 4; k++) {
        out[k] = 0;
    }
    if (code < 8) {
        return 0;
    }
    ensure();
    m = skin_mod((code - 8) / 4);
    for (k = 0; m != NULL && k < 4; k++) {
        out[k] = m->items[k];
        n += m->items[k] != 0;
    }
    return n;
}

/* The voice that plays for player `voice` (0, 1) when the game asks for `chara`'s: a fighter of the game that a new
   fighter of a fight stands in for speaks with that one's voice (Port_SkinVoiceSet at the fight's load). */
static int sVoiceAs[2][2] = {{-1, -1}, {-1, -1}}; /* per player: the character asked for, the one heard */

/* Per player: its techniques' sizes in percent (Blast 2 (1), Blast 2 (2), Ultimate: the Fine-tune's size; 0 as the
   game has it), for the effects (src/battle/eft_a.c PortTech_Scale). */
static int sTechSize[2][3];

static void warrior_techsize_of(const char *dir, int *out) {
    static int32_t set[5], lab[5][16];
    char path[600], line[512];
    FILE *fp;
    int t;

    out[0] = out[1] = out[2] = 0;
    snprintf(path, sizeof(path), "%s/warrior.txt", dir);
    if ((fp = fopen(path, "r")) == NULL) {
        return;
    }
    memset(set, 0, sizeof(set));
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq = strchr(line, '=');

        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        line[strcspn(line, "\r\n")] = '\0';
        eq[1 + strcspn(eq + 1, "\r\n")] = '\0';
        warrior_lab_of(line, eq + 1, set, lab);
    }
    fclose(fp);
    for (t = 2; t < 5; t++) {
        if ((set[t] >> LAB_SIZE) & 1) {
            out[t - 2] = lab[t][LAB_SIZE] < 10 ? 10 : lab[t][LAB_SIZE] > 1000 ? 1000 : lab[t][LAB_SIZE];
        }
    }
}

/* Per player: each technique's body glow (0x1000000 | 0xBBGGRR, 0 none), for src/battle/btl_obj.c. */
static int sTechGlow[2][5];

static void warrior_glow_of(const char *dir, int *out) {
    char path[600], line[256];
    FILE *fp;

    memset(out, 0, 5 * sizeof(int));
    snprintf(path, sizeof(path), "%s/warrior.txt", dir);
    if ((fp = fopen(path, "r")) == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *eq = strchr(line, '=');
        int t;

        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        if ((t = warrior_glow_key(line)) >= 0) {
            out[t] = warrior_hex_colour(eq + 1);
        }
    }
    fclose(fp);
}

int Port_SkinTechGlow(int player, int cls) {
    return player >= 0 && player < 2 && cls >= 0 && cls <= 4 ? sTechGlow[player][cls] : 0;
}

int Port_SkinTechSize(int player, int slot) {
    return player >= 0 && player < 2 && slot >= 2 && slot <= 4 ? sTechSize[player][slot - 2] : 0;
}

void Port_SkinVoiceSet(int player, int chara, int code) {
    Mod *m;

    if (player < 0 || player > 1) {
        return;
    }
    sVoiceAs[player][0] = chara;
    sVoiceAs[player][1] = Port_SkinVoice(chara, code);
    m = code >= 8 ? skin_mod((code - 8) / 4) : NULL; /* (and its aura's colour, a warrior's) */
    sAuraAs[player] = m != NULL ? warrior_aura_of(m->dir) : 0;
    if (m != NULL) { /* (its techniques' sizes, their glows) */
        warrior_techsize_of(m->dir, sTechSize[player]);
        warrior_glow_of(m->dir, sTechGlow[player]);
    } else {
        memset(sTechSize[player], 0, sizeof(sTechSize[player]));
        memset(sTechGlow[player], 0, sizeof(sTechGlow[player]));
    }
    if (m != NULL) { /* (its forms' transformation auras) */
        warrior_formaura_of(m->dir, sTransAs[player]);
    } else {
        memset(sTransAs[player], 0, sizeof(sTransAs[player]));
    }
}

int Port_SkinVoiceOf(int player, int chara) {
    if (player >= 0 && player < 2 && sVoiceAs[player][0] == chara && sVoiceAs[player][1] >= 0) {
        return sVoiceAs[player][1];
    }
    return chara;
}

/* ---- adding mods on a thread of their own (see "adding mods" above) ---- */
static volatile int sJobState; /* 0 none, 1 running, 2 finished (the library not read again yet) */
static int sJobAll;
static int sJobResult;
static char sJobPath[700];

static int SDLCALL job_main(void *unused) {
    (void)unused;
    sJobResult = import_path(sJobPath, sJobAll);
    sJobState = 2;
    return 0;
}

/* Starts adding what `path` holds (all: every mod under it, else the one mod that folder is). 0: one is running. */
int Port_ModImportStart(uint32_t pathAddr, int all) {
    SDL_Thread *t;

    if (sJobState != 0) {
        return 0;
    }
    ensure();
    snprintf(sJobPath, sizeof(sJobPath), "%s", (const char *)(uintptr_t)pathAddr);
    sJobAll = all;
    import_reset();
    sJobFiles = 0;
    sJobTotal = 0;
    sJobPhase = 0;
    sJobResult = 0;
    if (sPreview != NULL) { /* (no mod's files are being shown while the library changes) */
        sPreview = NULL;
        rebuild();
    }
    sJobState = 1;
    t = SDL_CreateThread(job_main, "mod import", NULL);
    if (t == NULL) {
        job_main(NULL); /* (no thread: done at once) */
    } else {
        SDL_DetachThread(t);
    }
    return 1;
}

/* How the import is doing: at progAddr (when not 0) three numbers - files copied, of how many (0 still counting),
   and what it does (0 looks, 1 unpacks an archive, 2 copies). Returns 0 none runs, 1 it runs, 2 it has just finished:
   the library has been read again and Port_ModImportResult / Port_ModImportInfo say what came of it. */
int Port_ModImportPoll(uint32_t progAddr) {
    if (progAddr != 0) {
        int32_t *p = (int32_t *)(uintptr_t)progAddr;
        p[0] = sJobFiles;
        p[1] = sJobTotal;
        p[2] = sJobPhase;
    }
    if (sJobState == 2) {
        Port_ModRescan();
        sJobState = 0;
        return 2;
    }
    return sJobState;
}

/* How many mods the last import added. */
int Port_ModImportResult(void) {
    return sJobResult;
}

/* ---- Z Warriors' draft, the suite's version, the bug report (src/menu/port_zw.c) ---- */
#include "port_release.h"

typedef struct WarriorDraftHead {
    uint32_t magic, size; /* 'ZWD1', sizeof(PortWarrior): a draft of another layout is not read */
    char folder[64];
} WarriorDraftHead;

/* The warrior being edited kept as it is while unsaved (a crash or a quit loses nothing): op 0 writes it, 1 reads
   the draft into it when there is one of the same warrior (1 read), 2 removes the draft. */
int Port_WarriorDraft(uint32_t warriorAddr, int op) {
    PortWarrior *w = (PortWarrior *)(uintptr_t)warriorAddr;
    static PortWarrior got;
    WarriorDraftHead h;
    char path[600];
    FILE *fp;
    int ok = 0;

    snprintf(path, sizeof(path), "%s/mods/_zw_draft.bin", root());
    if (op == 2) {
        remove(path);
        return 1;
    }
    if (op == 0) {
        memset(&h, 0, sizeof(h));
        h.magic = 0x3144575Au;
        h.size = (uint32_t)sizeof(PortWarrior);
        snprintf(h.folder, sizeof(h.folder), "%s", w->folder);
        if ((fp = fopen(path, "wb")) != NULL) {
            ok = fwrite(&h, sizeof(h), 1, fp) == 1 && fwrite(w, sizeof(*w), 1, fp) == 1;
            fclose(fp);
        }
        return ok;
    }
    if ((fp = fopen(path, "rb")) == NULL) {
        return 0;
    }
    if (fread(&h, sizeof(h), 1, fp) == 1 && h.magic == 0x3144575Au && h.size == (uint32_t)sizeof(PortWarrior) &&
        strncmp(h.folder, w->folder, sizeof(h.folder)) == 0 && fread(&got, sizeof(got), 1, fp) == 1) {
        got.mod = w->mod; /* (where it is in the library now) */
        got.problem = w->problem;
        *w = got;
        ok = 1;
    }
    fclose(fp);
    return ok;
}

/* "r<release>", and the suite's version when the beta installer put one (suite_version.txt). */
void Port_SuiteVersion(uint32_t outAddr, int size) {
    char *out = (char *)(uintptr_t)outAddr, v[64] = "";
    FILE *fp = fopen("suite_version.txt", "r");

    if (fp != NULL) {
        if (fgets(v, sizeof(v), fp) == NULL) {
            v[0] = '\0';
        }
        v[strcspn(v, "\r\n")] = '\0';
        fclose(fp);
    }
    snprintf(out, (size_t)size, v[0] ? "r%d  -  %s" : "r%d", PORT_RELEASE, v);
}

/* A zip for a bug report in reports/: the log, the crash report, the version, the newest screenshot, the warrior's
   warrior.txt (folder: under mods/, "" none) and the mods enabled; nothing of the disc's. The folder is shown.
   1 = made. */
int Port_MakeReport(uint32_t folderAddr) {
#ifdef _WIN32
    const char *folder = (const char *)(uintptr_t)folderAddr;
    char cmd[4000], sys[300], name[80], shot[300] = "", best[64] = "", path[700];
    const char *maybe[3] = {"bt3_log.txt", "bt3_crash.txt", "suite_version.txt"};
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    struct stat st;
    time_t now = time(NULL);
    DIR *d;
    int k;

    if (GetSystemDirectoryA(sys, sizeof(sys)) == 0) {
        return 0;
    }
    make_dir("reports");
    strftime(name, sizeof(name), "reports\\report_%Y-%m-%d_%H-%M-%S.zip", localtime(&now));
    if ((d = opendir("screenshots")) != NULL) { /* (the newest: their names are their dates) */
        struct dirent *e;

        while ((e = readdir(d)) != NULL) {
            if (strncmp(e->d_name, "BT3_", 4) == 0 && strstr(e->d_name, ".png") != NULL && strcmp(e->d_name, best) > 0 && strlen(e->d_name) < sizeof(best)) {
                snprintf(best, sizeof(best), "%s", e->d_name);
            }
        }
        closedir(d);
        if (best[0]) {
            snprintf(shot, sizeof(shot), " \"screenshots\\%s\"", best);
        }
    }
    snprintf(cmd, sizeof(cmd), "\"%s\\tar.exe\" -a -c -f \"%s\"", sys, name);
    for (k = 0; k < 3; k++) {
        if (stat(maybe[k], &st) == 0) {
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " \"%s\"", maybe[k]);
        }
    }
    snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), "%s", shot);
    snprintf(path, sizeof(path), "%s/mods/enabled.txt", root());
    if (stat(path, &st) == 0) {
        snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " \"%s\"", path);
    }
    if (folder != NULL && folder[0] && strstr(folder, "..") == NULL) {
        snprintf(path, sizeof(path), "%s/mods/%s/warrior.txt", root(), folder);
        if (stat(path, &st) == 0) {
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " \"%s\"", path);
        }
        snprintf(path, sizeof(path), "%s/mods/%s/warrior_error.txt", root(), folder);
        if (stat(path, &st) == 0) {
            snprintf(cmd + strlen(cmd), sizeof(cmd) - strlen(cmd), " \"%s\"", path);
        }
    }
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return 0;
    }
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (code != 0) {
        return 0;
    }
    if (_fullpath(path, "reports", sizeof(path)) != NULL) {
        snprintf(cmd, sizeof(cmd), "explorer.exe \"%s\"", path);
        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    return 1;
#else
    (void)folderAddr;
    return 0;
#endif
}

/* The shading textures of the warrior's preview (wp_shInfo*) of entry e (2 * slot: a part from another fighter, + 1
   its layer): out = {what it stands for's count, its own count, then 8 x 4 colours of each (0xBBGGRR)}; its own count.
   e = -1: out[0..23] each entry's own count. */
int Port_WarriorShades(int e, uint32_t outAddr) {
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    int s, q, c;

    if (e < 0) {
        for (q = 0; q < WP_SH_ENTRIES; q++) {
            out[q] = wp_shInfoN[q][1];
        }
        return WP_SH_ENTRIES;
    }
    if (e >= WP_SH_ENTRIES) {
        return 0;
    }
    out[0] = wp_shInfoN[e][0];
    out[1] = wp_shInfoN[e][1];
    for (s = 0; s < 2; s++) {
        for (q = 0; q < 8; q++) {
            for (c = 0; c < 4; c++) {
                out[2 + s * 32 + q * 4 + c] = (int32_t)wp_shInfoRgb[e][s][q][c];
            }
        }
    }
    return out[1];
}

/* The warrior's shading textures read for the workshop (Port_WarriorShades then has them): its model made once, even
   with nothing swapped, and let go. 1 made. */
int Port_WarriorShadeScan(uint32_t warriorAddr) {
    const int32_t *w = (const int32_t *)(uintptr_t)warriorAddr;
    static uint8_t keepFrom[WP_MAX_BATCHES];
    uint8_t *m = NULL, *t = NULL;
    uint32_t mn = 0, tn = 0;
    int keepN = sLookFroms, ok;

    memcpy(keepFrom, sLookFrom, (size_t)keepN); /* (the workshop's look keeps its own) */
    wp_forceCompose = 1;
    ok = warrior_look(w[WARRIOR_W_BODY], w + WARRIOR_W_PART, w + WARRIOR_W_LAYER, NULL, 0, w + WARRIOR_W_SIZE, w + WARRIOR_W_COSTUME, &m, &mn, &t, &tn);
    wp_forceCompose = 0;
    free(m);
    free(t);
    sLookFroms = keepN;
    memcpy(sLookFrom, keepFrom, (size_t)keepN);
    return ok;
}

/* ---- Z Warriors: the render and the small portrait photographed off the model ----
   Port_WarriorPortraitPhoto(i, step | kind << 8, from, to): kind 1 = the render (256 x 512, cut out: the model is
   photographed on black, then on white, and each pixel's see-through comes from the difference), kind 2 = the small
   portrait (64 x 64, on the plate's dark blue, no cut-out). Steps: 1 asks for the first photo; 2 (each frame) gives 0
   while it is not there, then 1 (the render: ask for the second with 3, then 4 each frame), the small portrait is made
   at 2; 4 makes the render. -1 failed. Step 9 forgets the photo (the render: the picture it came with again). `from`
   and `to` = the frame's corners, x | y << 16 in ten-thousandths of the game's picture. */
int Port_WarriorPortraitPhoto(int i, int step, int from, int to) {
    static uint8_t *shotA, *shotB;
    static int ow, oh; /* (the render: the frame's own pixels, as big as the screen shows it) */
    int kind = (step >> 8) & 0xFF, s = step & 0xFF;
    int x0 = from & 0xFFFF, y0 = (from >> 16) & 0xFFFF, x1 = to & 0xFFFF, y1 = (to >> 16) & 0xFFFF;
    Mod *m;
    char path[700];
    int p;

    ensure();
    if (i < 0 || i >= sModCount || (kind != 1 && kind != 2)) {
        return -1;
    }
    m = &sMod[i];
    if (s == 9) {
        if (kind == 1) {
            Port_ModArtReset(i);
        } else {
            snprintf(path, sizeof(path), "%s/chip_photo.raw", m->dir);
            remove(path);
            free(m->chipData);
            m->chipData = NULL;
            rebuild();
        }
        return 1;
    }
    if (s == 1 || s == 3) {
        GsCapture_PhotoWant();
        return 0;
    }
    if (s != 2 && s != 4) {
        return -1;
    }
    if (s == 2) {
        extern int GsCapture_PhotoSize(int *w, int *h);
        int pw = 0, phh = 0;

        if (!GsCapture_PhotoSize(&pw, &phh)) {
            return 0;
        }
        if (kind == 1) { /* the frame's pixels on the screen (1:2), at least the game's 256 x 512, at most 1024 x 2048 */
            oh = (int)((long)(y1 - y0) * phh / 10000);
            oh = oh < ART_H ? ART_H : oh > 2048 ? 2048 : oh;
            oh &= ~1;
            ow = oh / 2;
        } else {
            ow = oh = 64 * HD_S;
        }
        free(shotA);
        free(shotB);
        shotA = malloc((size_t)ow * oh * 4);
        shotB = malloc((size_t)ow * oh * 4);
        if (shotA == NULL || shotB == NULL) {
            return -1;
        }
    }
    if (shotA == NULL || shotB == NULL || !GsCapture_PhotoCrop(x0, y0, x1, y1, s == 2 ? shotA : shotB, ow, oh)) {
        return 0;
    }
    if (kind == 2) { /* the small portrait: its photo kept, the chip made from it when next asked for */
        static uint8_t small[64 * 64 * 4];
        FILE *fp;
        int ok, x, y;

        hd_down(shotA, ow, oh, small, 64, 64);
        snprintf(path, sizeof(path), "%s/chip_photo.raw", m->dir);
        if ((fp = fopen(path, "wb")) == NULL) {
            return -1;
        }
        ok = fwrite(small, 1, 64 * 64 * 4, fp) == 64 * 64 * 4;
        ok &= fclose(fp) == 0;
        { /* the HD chips: the photo itself, and the reel's (the face in x 8..55, y 12..51, the rest see-through) */
            uint8_t *reel = malloc((size_t)ow * oh * 4);

            if (reel != NULL) {
                for (y = 0; y < oh; y++) {
                    for (x = 0; x < ow; x++) {
                        uint8_t *q = &reel[((size_t)y * ow + x) * 4];
                        int lx = x * 64 / ow, ly = y * 64 / oh;

                        if (lx >= 8 && lx < 56 && ly >= 12 && ly < 52) {
                            int sx = (x - 8 * ow / 64) * 64 / 48, sy = (y - 12 * oh / 64) * 64 / 40;

                            sx = sx < 0 ? 0 : sx > ow - 1 ? ow - 1 : sx;
                            sy = sy < 0 ? 0 : sy > oh - 1 ? oh - 1 : sy;
                            memcpy(q, &shotA[((size_t)sy * ow + sx) * 4], 3);
                            q[3] = 255;
                        } else {
                            q[0] = q[1] = q[2] = q[3] = 0;
                        }
                    }
                }
                hd_write(m->dir, HD_REEL, reel, ow, oh);
                free(reel);
            }
            for (x = 0; x < ow * oh; x++) {
                shotA[x * 4 + 3] = 255;
            }
            hd_write(m->dir, HD_CHIP, shotA, ow, oh);
        }
        free(m->chipData);
        m->chipData = NULL;
        rebuild();
        fprintf(stderr, "bt3: small portrait photo: %s\n", ok ? "kept" : "NOT kept");
        fflush(stderr);
        return ok ? 1 : -1;
    }
    if (s == 2) {
        return 1; /* (the first: now the one on white) */
    }
    for (p = 0; p < ow * oh; p++) { /* the cut-out: on black c = a*f, on white c = a*f + (1-a)*255 */
        uint8_t *a = &shotA[p * 4], *b = &shotB[p * 4];
        int d = ((int)b[0] - a[0] + (int)b[1] - a[1] + (int)b[2] - a[2]) / 3, al = 255 - d, c;

        al = al < 0 ? 0 : al > 255 ? 255 : al;
        for (c = 0; c < 3; c++) {
            int v = al > 0 ? (int)a[c] * 255 / al : 0;

            a[c] = (uint8_t)(v > 255 ? 255 : v);
        }
        a[3] = (uint8_t)(al < 12 ? 0 : al);
    }
    { /* a black outline round the cut-out, as the game's renders have (about 0.3% of the height across: 3 pixels at 1024) */
        int t = oh * 3 / 1000, *dist = malloc((size_t)ow * oh * sizeof(int)), x, y;

        t = t < 1 ? 1 : t;
        if (dist != NULL) {
            for (p = 0; p < ow * oh; p++) { /* (3-4 chamfer distance, in thirds of a pixel, to the shown pixels) */
                dist[p] = shotA[p * 4 + 3] >= 128 ? 0 : 1 << 28;
            }
            for (y = 0; y < oh; y++) {
                for (x = 0; x < ow; x++) {
                    int *d = &dist[y * ow + x];

                    if (x > 0 && dist[y * ow + x - 1] + 3 < *d) *d = dist[y * ow + x - 1] + 3;
                    if (y > 0 && dist[(y - 1) * ow + x] + 3 < *d) *d = dist[(y - 1) * ow + x] + 3;
                    if (x > 0 && y > 0 && dist[(y - 1) * ow + x - 1] + 4 < *d) *d = dist[(y - 1) * ow + x - 1] + 4;
                    if (x < ow - 1 && y > 0 && dist[(y - 1) * ow + x + 1] + 4 < *d) *d = dist[(y - 1) * ow + x + 1] + 4;
                }
            }
            for (y = oh - 1; y >= 0; y--) {
                for (x = ow - 1; x >= 0; x--) {
                    int *d = &dist[y * ow + x];

                    if (x < ow - 1 && dist[y * ow + x + 1] + 3 < *d) *d = dist[y * ow + x + 1] + 3;
                    if (y < oh - 1 && dist[(y + 1) * ow + x] + 3 < *d) *d = dist[(y + 1) * ow + x] + 3;
                    if (x < ow - 1 && y < oh - 1 && dist[(y + 1) * ow + x + 1] + 4 < *d) *d = dist[(y + 1) * ow + x + 1] + 4;
                    if (x > 0 && y < oh - 1 && dist[(y + 1) * ow + x - 1] + 4 < *d) *d = dist[(y + 1) * ow + x - 1] + 4;
                }
            }
            for (p = 0; p < ow * oh; p++) {
                uint8_t *q = &shotA[p * 4];
                int d = dist[p];

                if (d > 0 && d <= t * 3 + 3) { /* (the line, its outer pixel half see-through) */
                    int al = d <= t * 3 ? 255 : 128, c;

                    for (c = 0; c < 3; c++) {
                        q[c] = (uint8_t)(q[c] * q[3] / 255 * (255 - al) / 255 + 12 * al / 255);
                    }
                    q[3] = (uint8_t)(q[3] > al ? q[3] : al);
                }
            }
            free(dist);
        }
    }
    snprintf(path, sizeof(path), "%s/portrait_photo.png", m->dir);
    if (!png_write(path, shotA, ow, oh)) {
        return -1;
    }
    snprintf(m->artSrc, sizeof(m->artSrc), "%s", "portrait_photo.png");
    write_settings(m);
    p = Port_ModArtSet2(i, 0, 0, 100, 100);
    fprintf(stderr, "bt3: render photo: %s\n", p ? "made" : "NOT made");
    fflush(stderr);
    return p ? 1 : -1;
}

/* Motion `anim` of mod i's animation file (its _anm.pak), unpacked into the game's buffer: its size, 0 none / too big. */
int Port_WarriorMotion(int i, int anim, uint32_t outAddr, int max) {
    char path[700];
    uint8_t *d, *raw;
    long len;
    uint32_t n, a, b, size = 0;

    ensure();
    if (i < 0 || i >= sModCount || anim < 0) {
        return 0;
    }
    mod_file(&sMod[i], 8, path, sizeof(path));
    if (path[0] == '\0' || (d = read_all(path, &len)) == NULL) {
        return 0;
    }
    memcpy(&n, d, 4);
    if (len < 16 || (uint32_t)anim >= n || n > 4096) {
        free(d);
        return 0;
    }
    memcpy(&a, d + 4 + anim * 4, 4);
    memcpy(&b, d + 8 + anim * 4, 4);
    raw = b > a && (long)b <= len ? bpe_decode(d + a, (long)(b - a), &size) : NULL;
    free(d);
    if (raw == NULL || size < 0x94 || (int)size > max) {
        free(raw);
        return 0;
    }
    { /* (scaled to the body as the fight scales it, bobj_a.c PortWarrior_ScaleAs: the model file's "BT3WARRI" part, the ratio
         of the fighter each motion was made for -- a pose of a taller fighter floated above the platform, a smaller one's sank) */
        WarriorPak mp;
        long ml = 0;
        uint32_t ps = 0;
        const uint8_t *wpart;

        memset(&mp, 0, sizeof(mp));
        mod_file(&sMod[i], 0, path, sizeof(path));
        if (path[0] != ' ' && path[0] != ' ' && (mp.d = read_all(path, &ml)) != NULL && ml >= 8) {
            mp.len = ml;
            memcpy(&mp.n, mp.d, 4);
            wpart = mp.n > 0x1A && mp.n < 4096 ? warrior_part(&mp, 0x1A, &ps) : NULL;
            if (wpart != NULL && ps >= 16 && memcmp(wpart, "BT3WARRI", 8) == 0) {
                float sc;
                uint32_t ranges, r;

                memcpy(&sc, wpart + 8, 4);
                memcpy(&ranges, wpart + 12, 4);
                for (r = 0; ranges <= 32 && r < ranges && 16 + (r + 1) * 8 <= ps; r++) {
                    uint16_t first, count;

                    memcpy(&first, wpart + 16 + r * 8, 2);
                    memcpy(&count, wpart + 18 + r * 8, 2);
                    if (anim >= first && anim < first + count) {
                        memcpy(&sc, wpart + 20 + r * 8, 4);
                    }
                }
                sc = sc < 0.0f ? -sc : sc;
                if (sc > 0.3f && sc < 3.0f) {
                    warrior_scale_motion(raw, size, sc);
                }
            }
        }
        free(mp.d);
    }
    memcpy((void *)(uintptr_t)outAddr, raw, size);
    free(raw);
    return (int)size;
}

/* Whether mod i has a HUD face photo of its own (Z Warriors). */
int Port_WarriorHasHudPhoto(int i) {
    char path[700];
    FILE *fp;

    ensure();
    if (i < 0 || i >= sModCount) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/hud_photo.raw", sMod[i].dir);
    if ((fp = fopen(path, "rb")) == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

/* Whether mod i has a small portrait photo of its own (Z Warriors). */
int Port_WarriorHasChipPhoto(int i) {
    char path[700];
    FILE *fp;

    ensure();
    if (i < 0 || i >= sModCount) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/chip_photo.raw", sMod[i].dir);
    if ((fp = fopen(path, "rb")) == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

/* The small portrait photo of a mod's folder as a chip's 64 x 64 colour numbers and their palette (PS2 alpha); 0 none. */
static int chip_photo(const Mod *m, uint8_t *chip, uint8_t *pal) {
    static uint8_t big[ART_W * ART_H * 4], idx[ART_W * ART_H];
    char path[700];
    uint8_t *d;
    long len;
    int x, y, k;

    snprintf(path, sizeof(path), "%s/chip_photo.raw", m->dir);
    if ((d = read_all(path, &len)) == NULL) {
        return 0;
    }
    if (len != 64 * 64 * 4) {
        free(d);
        return 0;
    }
    memset(big, 0, sizeof(big));
    for (y = 0; y < 64; y++) {
        memcpy(big + (size_t)y * ART_W * 4, d + y * 64 * 4, 64 * 4);
    }
    free(d);
    art_quantize(big, idx, pal, 200, 1);
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 64; x++) {
            chip[y * 64 + x] = idx[y * ART_W + x];
        }
    }
    for (k = 0; k < 256; k++) {
        pal[k * 4 + 3] = 0x80; /* (a chip is never see-through) */
    }
    snprintf(path, sizeof(path), "%s/hd_%d.png", m->dir, HD_CHIP);
    if ((d = read_all(path, &len)) != NULL) { /* (its HD picture: gs_texpack.c draws it instead) */
        free(d);
        hd_tag(pal, m->dir, HD_CHIP);
    }
    return 1;
}

/* ---- Z Warriors: the Community Workshop, its catalogue ----
   For now a local catalogue: <data>/workshop_local/<id>/ with entry.txt (name, title, creator, bio, tags, downloads,
   likes, uploaded, version) and recipe/ (warrior.txt and what the player made: paint/, costumeN/, the photos). A
   warrior is only its recipe: getting one copies the recipe into mods/ and the game builds it from the player's own
   disc. A server later takes the place of this folder; the game's side stays as it is. */
typedef struct PortWsEntry {
    char id[16];
    char name[40], title[40];
    char creator[32];
    char bio[160];
    char tags[64];
    char uploaded[16];
    int32_t downloads, likes, version, liked, owned;
    int32_t body, render;   /* (its face on the tiles) */
    int32_t built[8], builtN; /* the roster fighters it is made from */
    char line[4][6][56];    /* what it has: OVERVIEW, LOOK, MOVES, FORMS (6 lines each) */
    int32_t mine;
} PortWsEntry;

#define WS_MAX 256
int Port_WsUpload(uint32_t folderAddr, uint32_t bioAddr, uint32_t tagsAddr);
int Port_WsMine(int i);
static char sWsId[WS_MAX][16];
static int sWsN;

static void ws_dir(char *out, size_t n) {
    snprintf(out, n, "%s/workshop_local", root());
}

/* One key of entry.txt of entry `id` (out "" when missing). */
static void ws_key(const char *id, const char *key, char *out, size_t n) {
    char path[700], line[400];
    FILE *fp;
    size_t kl = strlen(key);

    out[0] = '\0';
    snprintf(path, sizeof(path), "%s/workshop_local/%s/entry.txt", root(), id);
    if ((fp = fopen(path, "r")) == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
            line[strcspn(line, "\r\n")] = '\0';
            snprintf(out, n, "%s", line + kl + 1);
            break;
        }
    }
    fclose(fp);
}

static int ws_num(const char *id, const char *key) {
    char v[32];

    ws_key(id, key, v, sizeof(v));
    return atoi(v);
}

/* Writes key=value into entry.txt of `id` (replacing that key). */
static void ws_set(const char *id, const char *key, const char *value) {
    char path[700], line[400];
    static char keep[64][400];
    int n = 0, i, done = 0;
    size_t kl = strlen(key);
    FILE *fp;

    snprintf(path, sizeof(path), "%s/workshop_local/%s/entry.txt", root(), id);
    if ((fp = fopen(path, "r")) != NULL) {
        while (n < 64 && fgets(line, sizeof(line), fp) != NULL) {
            line[strcspn(line, "\r\n")] = '\0';
            if (strncmp(line, key, kl) == 0 && line[kl] == '=') {
                snprintf(keep[n++], sizeof(keep[0]), "%s=%s", key, value);
                done = 1;
            } else if (line[0] != '\0') {
                snprintf(keep[n++], sizeof(keep[0]), "%s", line);
            }
        }
        fclose(fp);
    }
    if (!done && n < 64) {
        snprintf(keep[n++], sizeof(keep[0]), "%s=%s", key, value);
    }
    if ((fp = fopen(path, "w")) != NULL) {
        for (i = 0; i < n; i++) {
            fprintf(fp, "%s\n", keep[i]);
        }
        fclose(fp);
    }
}

/* Whether a warrior of the library came from entry `id` (its folder's .workshop file names it). */
static int ws_owned(const char *id) {
    int i;

    ensure();
    for (i = 0; i < sWarriorCount; i++) {
        char path[700], got[32] = "";
        FILE *fp;

        snprintf(path, sizeof(path), "%s/mods/%s/.workshop", root(), sWarrior[i]);
        if ((fp = fopen(path, "r")) != NULL) {
            if (fgets(got, sizeof(got), fp) != NULL) {
                got[strcspn(got, "\r\n")] = '\0';
            }
            fclose(fp);
            if (strcmp(got, id) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

static const char *ws_me(void) {
    const char *u = getenv("USERNAME");

    return u != NULL && u[0] != '\0' ? u : "You";
}

/* The recipe's files of a warrior folder copied to `to` (warrior.txt, paint/, costumeN/, its photos and picture). */
static int ws_copy_recipe(const char *from, const char *to) {
    static const char *const files[] = {"warrior.txt", "portrait_photo.png", "chip_photo.raw", "hud_photo.raw", "portrait_src.png",
                                        "portrait_src.jpg", "portrait_src.bmp"};
    static const char *const dirs[] = {"paint", "costume1", "costume2", "costume3", "costume4"};
    char a[900], b[900];
    size_t k;
    int ok;

    make_dir(to);
    snprintf(a, sizeof(a), "%s/warrior.txt", from);
    snprintf(b, sizeof(b), "%s/warrior.txt", to);
    ok = copy_file(a, b);
    for (k = 1; k < sizeof(files) / sizeof(files[0]); k++) {
        FILE *fp;

        snprintf(a, sizeof(a), "%s/%s", from, files[k]);
        if ((fp = fopen(a, "rb")) != NULL) {
            fclose(fp);
            snprintf(b, sizeof(b), "%s/%s", to, files[k]);
            copy_file(a, b);
        }
    }
    for (k = 0; k < sizeof(dirs) / sizeof(dirs[0]); k++) {
        snprintf(a, sizeof(a), "%s/%s", from, dirs[k]);
        if (is_dir(a)) {
            snprintf(b, sizeof(b), "%s/%s", to, dirs[k]);
            copy_tree(a, b, 1);
        }
    }
    return ok;
}

/* The catalogue in an order: 0 the newest first, 1 the most downloaded, 2 featured (the most liked), 3 the player's own
   uploads. The first time (no catalogue yet) the library's warriors are put in it, as their maker's. Returns the count. */
int Port_WsScan(int sort) {
    static DirEnt ents[WS_MAX];
    char dir[700];
    int n, i, j;

    ensure();
    ws_dir(dir, sizeof(dir));
    if (!is_dir(dir)) {
        make_dir(dir);
        for (i = 0; i < sWarriorCount; i++) { /* (the first catalogue: the warriors already made) */
            if (sWarrior[i][0] != '_') {
                Port_WsUpload((uint32_t)(uintptr_t)sWarrior[i], (uint32_t)(uintptr_t)"", (uint32_t)(uintptr_t)"");
            }
        }
    }
    n = dir_read(dir, ents, WS_MAX);
    sWsN = 0;
    for (i = 0; i < n; i++) {
        if (ents[i].isDir && strlen(ents[i].name) < sizeof(sWsId[0])) {
            if (sort == 3) {
                char who[32];

                ws_key(ents[i].name, "creator", who, sizeof(who));
                if (strcmp(who, ws_me()) != 0) {
                    continue;
                }
            }
            snprintf(sWsId[sWsN++], sizeof(sWsId[0]), "%s", ents[i].name);
        }
    }
    for (i = 1; i < sWsN; i++) { /* (insertion sort: a few hundred at most) */
        char t[16];
        int a;

        snprintf(t, sizeof(t), "%s", sWsId[i]);
        a = sort == 1 ? ws_num(t, "downloads") : sort == 2 ? ws_num(t, "likes") : 0;
        for (j = i - 1; j >= 0; j--) {
            int b = sort == 1 ? ws_num(sWsId[j], "downloads") : sort == 2 ? ws_num(sWsId[j], "likes") : 0;
            int before = sort == 1 || sort == 2 ? a > b : strcmp(t, sWsId[j]) > 0; /* (ids grow with time: newest first) */

            if (!before) {
                break;
            }
            memcpy(sWsId[j + 1], sWsId[j], sizeof(sWsId[0]));
        }
        memcpy(sWsId[j + 1], t, sizeof(sWsId[0]));
    }
    return sWsN;
}

static void ws_line(char *out, const char *label, int who) {
    const char *s = warrior_who_shown(who);

    snprintf(out, 56, "%s: %s", label, s != NULL ? s : "its own");
}

/* Entry i of the last scan, with what it has (from its recipe). 1 given. */
int Port_WsGet(int i, uint32_t outAddr) {
    PortWsEntry *e = (PortWsEntry *)(uintptr_t)outAddr;
    PortWarrior w;
    char folder[200], v[32];
    int k, n, f;

    if (i < 0 || i >= sWsN) {
        return 0;
    }
    memset(e, 0, sizeof(*e));
    snprintf(e->id, sizeof(e->id), "%s", sWsId[i]);
    ws_key(e->id, "name", e->name, sizeof(e->name));
    ws_key(e->id, "title", e->title, sizeof(e->title));
    ws_key(e->id, "creator", e->creator, sizeof(e->creator));
    ws_key(e->id, "bio", e->bio, sizeof(e->bio));
    ws_key(e->id, "tags", e->tags, sizeof(e->tags));
    ws_key(e->id, "uploaded", e->uploaded, sizeof(e->uploaded));
    e->downloads = ws_num(e->id, "downloads");
    e->likes = ws_num(e->id, "likes");
    e->version = ws_num(e->id, "version");
    ws_key(e->id, "liked", v, sizeof(v));
    e->liked = v[0] == '1';
    e->owned = ws_owned(e->id);
    e->mine = Port_WsMine(i);
    snprintf(folder, sizeof(folder), "../workshop_local/%s/recipe", e->id);
    warrior_read(folder, &w);
    e->body = w.body;
    e->render = w.render >= 0 ? w.render : w.body;
    { /* the fighters it is made from, each once */
        int who[40], m = 0;

        who[m++] = w.body;
        who[m++] = w.moves;
        for (k = 0; k < 12; k++) {
            who[m++] = w.part[k];
            who[m++] = w.layer[k];
        }
        for (k = 0; k < 3; k++) {
            who[m++] = w.tech[k];
        }
        for (k = 0; k < m && e->builtN < 8; k++) {
            int j, seen = 0;

            if (!chara_ok(who[k])) {
                continue;
            }
            for (j = 0; j < e->builtN; j++) {
                seen |= e->built[j] == who[k];
            }
            if (!seen) {
                e->built[e->builtN++] = who[k];
            }
        }
    }
    /* OVERVIEW */
    ws_line(e->line[0][0], "Body", w.body);
    ws_line(e->line[0][1], "Moves", w.moves);
    snprintf(e->line[0][2], 56, "Stance: %s", w.pose[0][0] || w.pose[1][0] || w.pose[2][0] || w.pose[3][1] ? "custom" : w.stance >= 0 && warrior_who_shown(w.stance) != NULL ? warrior_who_shown(w.stance) : "the body's");
    for (n = 0, f = 0; f < 4; f++) {
        n += w.formTo[f][0] != '\0';
    }
    snprintf(e->line[0][3], 56, "Forms: %d", n);
    snprintf(e->line[0][4], 56, "Costumes: %s", w.look[1][0] || w.look[2][0] || w.look[3][0] ? "4" : "1");
    snprintf(e->line[0][5], 56, "Damage: %s", w.damage == 2 ? "battle-torn" : w.damage == 1 ? "scuffed" : w.damage == 3 ? "clean" : "the fighters' own");
    /* LOOK */
    {
        static const char *const slot[6] = {"Hair", "Head", "Face", "Chest", "Hands", "Feet"};
        static const int idx[6] = {0, 1, 2, 3, 7, 10};

        for (k = 0; k < 6; k++) {
            int who = w.part[idx[k]] >= 0 ? w.part[idx[k]] : w.body;

            snprintf(e->line[1][k], 56, "%s: %s%s", slot[k], warrior_who_shown(who) != NULL ? warrior_who_shown(who) : "-", w.layer[idx[k]] >= 0 ? " + layer" : "");
        }
    }
    /* MOVES: the five techniques' names */
    {
        static const char *const label[5] = {"Blast 1", "Blast 1", "Blast 2", "Blast 2", "Ultimate"};

        for (k = 0; k < 5; k++) {
            char mv[48] = "";
            int who = k < 2 ? (w.skill[k] >= 0 ? w.skill[k] : w.moves) : (w.tech[k - 2] >= 0 ? w.tech[k - 2] : w.moves);
            int from = k < 2 ? (w.skill[k] >= 0 ? w.skill[k] : k) : (w.tech[k - 2] >= 0 ? w.techFrom[k - 2] : k - 2);

            if (k < 2 && w.skill[k] >= 0) {
                from = w.skillFrom[k];
            }
            if (w.moveName[k][0] != '\0') {
                snprintf(mv, sizeof(mv), "%s", w.moveName[k]);
            } else if (who >= 0) {
                Port_FighterMove(who, (k < 2 ? 0 : 2) + from, (uint32_t)(uintptr_t)mv, sizeof(mv));
            }
            snprintf(e->line[2][k], 56, "%s: %s", label[k], mv[0] ? mv : "-");
        }
        snprintf(e->line[2][5], 56, "Aura: %s", w.auraColour > 0 ? "a colour of its own" : "its own");
    }
    /* FORMS */
    for (k = 0, f = 0; f < 4 && k < 6; f++) {
        if (w.formTo[f][0] != '\0') {
            const char *to = strncmp(w.formTo[f], "Warrior - ", 10) == 0 ? w.formTo[f] + 10 : w.formTo[f];

            snprintf(e->line[3][k++], 56, "Form %d: %s (%d ki bars)", f + 1, to, (int)w.formCost[f]);
        }
    }
    if (k == 0) {
        snprintf(e->line[3][0], 56, "No transformations");
    }
    return 1;
}

/* Warrior folder `folder` (of mods/) put in the catalogue as a new entry, with its bio and tags. 1 done. */
int Port_WsUpload(uint32_t folderAddr, uint32_t bioAddr, uint32_t tagsAddr) {
    const char *folder = (const char *)(uintptr_t)folderAddr, *bio = (const char *)(uintptr_t)bioAddr, *tags = (const char *)(uintptr_t)tagsAddr;
    char dir[700], from[700], to[760], id[16], day[16];
    PortWarrior w;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    static int seq;
    int ok;

    ws_dir(dir, sizeof(dir));
    make_dir(dir);
    snprintf(id, sizeof(id), "w%08lx%02x", (unsigned long)now, (unsigned)(seq++ & 0xFF));
    snprintf(to, sizeof(to), "%s/%s", dir, id);
    make_dir(to);
    snprintf(from, sizeof(from), "%s/mods/%s", root(), folder);
    snprintf(to, sizeof(to), "%s/%s/recipe", dir, id);
    ok = ws_copy_recipe(from, to);
    warrior_read(folder, &w);
    strftime(day, sizeof(day), "%b %d", t);
    ws_set(id, "name", w.name[0] ? w.name : folder);
    ws_set(id, "title", w.title);
    ws_set(id, "creator", ws_me());
    ws_set(id, "bio", bio);
    ws_set(id, "tags", tags);
    ws_set(id, "downloads", "0");
    ws_set(id, "likes", "0");
    ws_set(id, "uploaded", day);
    ws_set(id, "version", "1");
    fprintf(stderr, "bt3: workshop: %s uploaded as %s (%d)\n", folder, id, ok);
    fflush(stderr);
    return ok;
}

/* Entry i got: its recipe copied into a new warrior folder (built by the rescan). 1 done. */
int Port_WsDownload(int i) {
    char from[700], to[700], folder[96], name[48], path[760], v[16];
    FILE *fp;
    int ok;

    if (i < 0 || i >= sWsN) {
        return 0;
    }
    ws_key(sWsId[i], "name", name, sizeof(name));
    warrior_folder(name[0] ? name : "Warrior", folder, sizeof(folder));
    snprintf(from, sizeof(from), "%s/workshop_local/%s/recipe", root(), sWsId[i]);
    snprintf(to, sizeof(to), "%s/mods/%s", root(), folder);
    ok = ws_copy_recipe(from, to);
    snprintf(path, sizeof(path), "%s/.workshop", to);
    if ((fp = fopen(path, "w")) != NULL) {
        fprintf(fp, "%s\n", sWsId[i]);
        fclose(fp);
    }
    snprintf(v, sizeof(v), "%d", ws_num(sWsId[i], "downloads") + 1);
    ws_set(sWsId[i], "downloads", v);
    fprintf(stderr, "bt3: workshop: got %s as %s (%d)\n", sWsId[i], folder, ok);
    fflush(stderr);
    return ok;
}

/* Entry i liked (or not any more). Returns its likes now. */
int Port_WsLike(int i) {
    char v[16];
    int liked, likes;

    if (i < 0 || i >= sWsN) {
        return 0;
    }
    ws_key(sWsId[i], "liked", v, sizeof(v));
    liked = v[0] == '1';
    likes = ws_num(sWsId[i], "likes") + (liked ? -1 : 1);
    likes = likes < 0 ? 0 : likes;
    snprintf(v, sizeof(v), "%d", likes);
    ws_set(sWsId[i], "likes", v);
    ws_set(sWsId[i], "liked", liked ? "0" : "1");
    return likes;
}

/* Entry i's warrior built for the screen: its recipe (drawings, costumes, photos too) made in mods/_zw_wspreview
   (never listed, never switched on), as the editor's preview is made. Returns that mod's index, -1 none. */
int Port_WsPreview(int i) {
    char from[700], to[700];

    ensure();
    if (i < 0 || i >= sWsN) {
        return -1;
    }
    snprintf(to, sizeof(to), "%s/mods/_zw_wspreview", root());
    remove_tree(to, 0);
    snprintf(from, sizeof(from), "%s/workshop_local/%s/recipe", root(), sWsId[i]);
    if (!ws_copy_recipe(from, to)) {
        return -1;
    }
    Port_ModRescan();
    fprintf(stderr, "bt3: workshop: preview of %s made (mod %d)\n", sWsId[i], mod_by_name("_zw_wspreview"));
    fflush(stderr);
    return mod_by_name("_zw_wspreview");
}

/* Entry i's small portrait (a chip, as Port_ModChip makes one) from its own photo; 0 when it has none (its face then). */
int Port_WsChip(int i, uint32_t outAddr, int max) {
    Mod keep;
    char path[700];
    FILE *fp;
    int got;

    if (i < 0 || i >= sWsN) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/workshop_local/%s/recipe/chip_photo.raw", root(), sWsId[i]);
    if ((fp = fopen(path, "rb")) == NULL) {
        return 0;
    }
    fclose(fp);
    keep = sBrowseMod;
    memset(&sBrowseMod, 0, sizeof(sBrowseMod));
    snprintf(sBrowseMod.dir, sizeof(sBrowseMod.dir), "%s/workshop_local/%s/recipe", root(), sWsId[i]);
    sBrowseMod.chara = -1;
    got = Port_ModChip(-2, outAddr, max);
    free(sBrowseMod.chipData);
    sBrowseMod = keep;
    return got;
}

/* Whether entry i is one of the player's own uploads (only those can be taken down). */
int Port_WsMine(int i) {
    char who[32];

    if (i < 0 || i >= sWsN) {
        return 0;
    }
    ws_key(sWsId[i], "creator", who, sizeof(who));
    return strcmp(who, ws_me()) == 0;
}

/* Entry i taken out of the catalogue (the player's own uploads only); warriors already got from it stay. 1 done. */
int Port_WsDelete(int i) {
    char path[700];

    if (!Port_WsMine(i)) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/workshop_local/%s", root(), sWsId[i]);
    remove_tree(path, 0);
    fprintf(stderr, "bt3: workshop: %s taken down\n", sWsId[i]);
    fflush(stderr);
    memmove(sWsId[i], sWsId[i + 1], (size_t)(sWsN - i - 1) * sizeof(sWsId[0]));
    sWsN--;
    return 1;
}

/* ---- mod fighters for Z Warriors' picker: the mods with a fighter of their own (Mod Manager mods, other warriors) ---- */
static int mod_ref_usable(const Mod *m) {
    char path[700];

    if (m->name[0] == '_' || target_of(m) < 0 || target_of(m) >= CHARA_COUNT) {
        return 0;
    }
    mod_file(m, 0, path, sizeof(path));
    return path[0] != ' ';
}

/* How many mod fighters there are (mod `skip`, the warrior being made, left out). */
int Port_ModRefCount(int skip) {
    int i, n = 0;

    ensure();
    for (i = 0; i < sModCount; i++) {
        n += i != skip && mod_ref_usable(&sMod[i]);
    }
    return n;
}

/* The fighter id of mod fighter j (as Port_ModRefCount counts them): MOD_REF_BASE + its place; -1 none. */
int Port_ModRefAt(int j, int skip) {
    int i;

    ensure();
    for (i = 0; i < sModCount; i++) {
        if (i != skip && mod_ref_usable(&sMod[i]) && j-- == 0) {
            return mod_ref_id(sMod[i].name);
        }
    }
    return -1;
}

/* The mod index of a mod fighter (-1: a roster fighter, or gone). */
int Port_ModRefMod(int who) {
    ensure();
    return is_mod_ref(who) ? mod_by_name(sModRef[who - MOD_REF_BASE]) : -1;
}

/* The fighter a mod fighter stands on (a roster fighter: itself). */
int Port_ModRefChara(int who) {
    return mod_ref_chara(who);
}

/* ---- the Technique Lab's screen (Z Warriors) ---- */
/* Technique t (0, 1 the Blast 1s, 2..4 the Blast 2s and the Ultimate) of warrior w as it is without the Lab: its numbers in
   the Lab's units (LAB_), out[15] its effect type, out[14] the fighter * 4 + technique it is. 1 given. */
int Port_WarriorLabBase(uint32_t warriorAddr, int t, uint32_t outAddr) {
    const PortWarrior *w = (const PortWarrior *)(uintptr_t)warriorAddr;
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    WarriorPak p;
    uint32_t n;
    const uint8_t *q;
    int who, from, ok = 0;

    memset(out, 0, 16 * sizeof(int32_t));
    if (t < 0 || t > 4) {
        return 0;
    }
    if (t >= 2) {
        who = w->tech[t - 2] >= 0 ? w->tech[t - 2] : w->moves;
        from = w->tech[t - 2] >= 0 ? w->techFrom[t - 2] : t - 2;
    } else {
        who = w->skill[t] >= 0 ? w->skill[t] : w->moves;
        from = w->skill[t] >= 0 ? w->skillFrom[t] : t;
    }
    if (!chara_ok(who)) {
        return 0;
    }
    out[14] = (who < CHARA_COUNT ? who : 0) * 4 + from;
    memset(&p, 0, sizeof(p));
    if (warrior_open(&p, MODEL_FILE + who * 10) && p.n == WARRIOR_PARTS) {
        if (t >= 2 && (q = warrior_part(&p, 23, &n)) != NULL && n >= 0x244 && from >= 0 && from < 3) {
            out[LAB_DAMAGE] = lab_rd32(q + 0x184 + from * 4);
            out[LAB_COST] = lab_rd32(q + 0x19C + from * 4);
            out[LAB_HITS] = (int8_t)q[0x9C + from] + 1;
            out[LAB_SHOTS] = (int8_t)q[0x93 + from] < 1 ? 1 : (int8_t)q[0x93 + from];
            out[LAB_SIZE] = (int32_t)(lab_rdf(q + 0x3C + from * 4) * 100.0f + 0.5f);
            out[LAB_SPEED] = (int32_t)(lab_rdf(q + 0x54 + from * 4) + 0.5f);
            out[LAB_HOMING] = (int32_t)(lab_rdf(q + 0x60 + from * 4) + 0.5f);
            out[LAB_TIME] = (int32_t)(lab_rdf(q + 0x24 + from * 4) * 100.0f + 0.5f);
            out[LAB_PUSH] = (int32_t)(lab_rdf(q + 0x16C + from * 4) * 10.0f + 0.5f);
            out[LAB_CHARGE] = (int32_t)(lab_rdf(q + 0x1D8 + from * 4) * 100.0f + 0.5f);
            out[LAB_RECOIL] = lab_rd32(q + 0x1A8 + from * 4);
            out[LAB_UNBLOCK] = lab_rd32(q + 0x0C + from * 4) & 1;
            out[LAB_LIKE] = -1;
            out[15] = (int8_t)q[0x138 + from];
            ok = 1;
        } else if (t < 2 && (q = warrior_part(&p, 24, &n)) != NULL && n >= 0xC8 && from >= 0 && from < 2) {
            out[LAB_DAMAGE] = lab_rd32(q + 0x84 + from * 4);
            out[LAB_COST] = (int8_t)q[0x94 + from];
            out[LAB_ATTACK] = (int8_t)q[0x96 + from] + 100;
            out[LAB_DEFENSE] = (int8_t)q[0x9A + from] + 100;
            out[LAB_KIBOOST] = (int8_t)q[0x98 + from] + 100;
            out[LAB_BLAST] = (int8_t)q[0x9C + from] + 100;
            out[LAB_TIME] = (int32_t)(lab_rdf(q + 0xA0 + from * 4) * 100.0f + 0.5f);
            out[LAB_HEAL] = lab_rd32(q + 0xA8 + from * 4);
            out[LAB_KI] = lab_rd32(q + 0xB0 + from * 4);
            out[LAB_UNBLOCK] = (lab_rd32(q + 0x08 + from * 4) >> 9) & 1; /* (Max Power) */
            out[15] = -1;
            ok = 1;
        }
    }
    free(p.d);
    return ok;
}

/* The techniques a technique of effect type `fx` can behave like (the Lab's "Behaves like"): famous ones, each with rules
   of its own (piercing, unblockable, exploding, barrages). k-th of those of that type: fighter * 4 + technique, -1 none. */
int Port_WarriorLabLike(int fx, int k) {
    static const short kLike[][2] = {
        {23, 0},  /* Piccolo (End): Special Beam Cannon - pierces */
        {91, 0},  /* Frieza 1st Form: Death Beam - thin, piercing */
        {91, 2},  /* Frieza 1st Form: Death Ball */
        {3, 0},   /* Goku (End): Kamehameha */
        {33, 2},  /* Super Vegeta: Final Flash */
        {33, 0},  /* Super Vegeta: Big Bang Attack */
        {128, 2}, /* Metal Cooler: Supernova */
        {117, 2}, /* Kid Buu: Planet Burst */
        {54, 2},  /* Gogeta Super Saiyan 4: X 100 Big Bang Kamehameha */
        {133, 0}, /* Broly Legendary Super Saiyan: Eraser Cannon */
        {4, 1},   /* Goku (End) Super Saiyan: Super Explosive Wave */
        {107, 1}, /* Perfect Cell: Barrage Death Beam */
        {128, 1}, /* Metal Cooler: Finger Blitz Barrage */
        {26, 1},  /* Yamcha: Wolf Fang Fist... (a rush) */
        {3, 1},   /* Goku (End): Meteor Smash */
        {6, 2},   /* Goku (End) Super Saiyan 3: Dragon Fist */
        {23, 2},  /* Piccolo (End): Hellzone Grenade */
        {25, 2},  /* Krillin: Destructo Disc */
        {0, 2},   /* Goku (Early): Spirit Bomb - unblockable */
        {133, 2}, /* Broly Legendary Super Saiyan: Omega Blaster */
    };
    int i, n = 0;

    for (i = 0; i < (int)(sizeof(kLike) / sizeof(kLike[0])); i++) {
        if (Port_FighterMoveFx(kLike[i][0], 2 + kLike[i][1]) == fx && n++ == k) {
            return kLike[i][0] * 4 + kLike[i][1];
        }
    }
    return -1;
}

/* Texture `key` (its picture hash) of the model made last (the preview, or Port_WarriorShadeScan): the shading ramps it
   is drawn with, out = {count, then 4 colours (0xBBGGRR, light to dark) of each}; the count. */
int Port_WarriorTexShade(int key, uint32_t outAddr) {
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    int i, r, c;

    out[0] = 0;
    for (i = 0; i < wp_txiN && wp_txiKey[i] != ((uint32_t)key & 0xFFFFFF); i++) {
    }
    if (i == wp_txiN) {
        return 0;
    }
    out[0] = wp_txiRamps[i];
    for (r = 0; r < wp_txiRamps[i]; r++) {
        for (c = 0; c < 4; c++) {
            out[1 + r * 4 + c] = (int32_t)wp_txiRgb[i][r][c];
        }
    }
    return out[0];
}

/* ---- the character select's CUSTOM tile (menu_c_e.c): Z Warriors' custom characters in the reel ---- */
/* Whether skin `skin` is a warrior of Z Warriors. */
int Port_SkinIsWarrior(int skin) {
    Mod *m;

    ensure();
    m = Port_NetSession() ? NULL : skin_mod(skin);
    return m != NULL && m->warrior;
}

/* The fighter skin `skin` counts as (its files' template), -1 none. */
int Port_SkinChara(int skin) {
    Mod *m;

    ensure();
    m = skin_mod(skin);
    return m != NULL ? target_of(m) : -1;
}

/* The skins a warrior skin powers up into (its transformations: warriors, by name), up to 4; their count. */
int Port_SkinForms(int skin, uint32_t outAddr) {
    int32_t *out = (int32_t *)(uintptr_t)outAddr;
    Mod *m;
    int k, n = 0;

    ensure();
    m = skin_mod(skin);
    if (m == NULL) {
        return 0;
    }
    for (k = 0; k < 4; k++) {
        int mi, sk;

        if (m->trName[k][0] == '\0' || m->tr.type[k] != 0) { /* (a power up; "back to base" is not a form to pick) */
            continue;
        }
        mi = mod_by_name(m->trName[k]);
        sk = mi >= 0 ? skin_of_mod(mi) : -1;
        if (sk >= 0 && sk != skin) {
            out[n++] = sk;
        }
    }
    return n;
}

/* A chip (5504 bytes, as Port_ModChip makes one) made the shape of the character select's: the game's chips draw their
   face in x 8..55, y 12..51 of the 64 x 64 picture, the rest see-through (the reel's frame sits round that). */
static void chip_to_reel(uint8_t *o) {
    static uint8_t idx[64 * 64], out[64 * 64];
    int x, y;

    swizzle8(idx, o + 0xC0, 64, 64, 0);
    memset(out, 255, sizeof(out));
    for (y = 12; y < 52; y++) {
        for (x = 8; x < 56; x++) {
            out[y * 64 + x] = idx[((y - 12) * 64 / 40) * 64 + (x - 8) * 64 / 48];
        }
    }
    swizzle8(out, o + 0xC0, 64, 64, 1);
    memset(o + 0x60 + 0x1080 + 0x60 + csm1(255) * 4, 0, 4); /* (colour 255: see-through; the chips' pictures use 200) */
}

/* Mod i's chip for the character select's reel. */
int Port_ModChipReel(int i, uint32_t outAddr, int max) {
    int got = Port_ModChip(i, outAddr, max);

    if (got >= 5504) {
        uint8_t *o = (uint8_t *)(uintptr_t)outAddr, *e = o + 0x60 + 0x1080 + 0x60;

        chip_to_reel(o);
        if (e[csm1(253) * 4 + 3] == 0x01 && e[csm1(253) * 4] == 0x48 && e[csm1(254) * 4 + 3] == 0x02) {
            e[csm1(254) * 4] = (uint8_t)((e[csm1(254) * 4] & 0xF0) | HD_REEL); /* (the reel's HD picture) */
        }
    }
    return got;
}

/* How many costumes skin `skin` has (its model files 1..4 in a row), at least 1. */
int Port_SkinCostumes(int skin) {
    Mod *m;
    char path[700];
    int k;

    ensure();
    m = skin_mod(skin);
    if (m == NULL) {
        return 1;
    }
    for (k = 1; k < 4; k++) {
        mod_path(m, k, path, sizeof(path));
        if (path[0] == '\0') {
            break;
        }
    }
    return k;
}

/* The CUSTOM tile's chip (<data>/zw_custom/chip_photo.raw, 64 x 64 RGBA), the reel's shape; 0 none. */
int Port_ZwCustomChip(uint32_t outAddr, int max) {
    Mod keep;
    int got;

    ensure();
    keep = sBrowseMod;
    memset(&sBrowseMod, 0, sizeof(sBrowseMod));
    snprintf(sBrowseMod.dir, sizeof(sBrowseMod.dir), "%s/zw_custom", root());
    sBrowseMod.chara = -1;
    got = Port_ModChip(-2, outAddr, max);
    free(sBrowseMod.chipData);
    sBrowseMod = keep;
    if (got >= 5504) {
        chip_to_reel((uint8_t *)(uintptr_t)outAddr);
    }
    return got;
}
/* ------------------------------------------------------------------------------------------------- online
 * What the mods that are on add to the gameplay fingerprint (plat_fprint.c): two games play online only when their
 * fingerprints are the same (gs/net.c), so with the same mods on, warriors among them, and nothing else. Each mod
 * that is on, in its order, by name and by every file the fight reads of it (its fighters' models, motions and
 * effects, its files by number; not its portrait): their bytes. Kept while the mods and their files' sizes are the
 * same. 0 when no mod is on. */
static uint64_t mods_fnv(uint64_t h, const void *p, size_t n) {
    const uint8_t *b = (const uint8_t *)p;
    size_t i;

    for (i = 0; i < n; i++) {
        h = (h ^ b[i]) * 0x100000001B3ull;
    }
    return h;
}

static uint64_t mods_file(uint64_t h, const char *dir, const char *rel) {
    char path[700], buf[65536];
    FILE *fp;
    size_t n;

    snprintf(path, sizeof(path), "%s/%s", dir, rel);
    h = mods_fnv(h, rel, strlen(rel) + 1);
    if ((fp = fopen(path, "rb")) == NULL) {
        return mods_fnv(h, "-", 1);
    }
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        h = mods_fnv(h, buf, n);
    }
    fclose(fp);
    return h;
}

uint64_t Port_ModsFingerprint(void) {
    static uint64_t keptSig, kept;
    static int have;
    int order[MAX_MODS], n = 0, i, j, k;
    uint64_t sig = 0xCBF29CE484222325ull, h;

    ensure();
    for (i = 0; i < sModCount; i++) { /* those that are on, in their order (the rank) */
        if (sMod[i].on) {
            for (j = n; j > 0 && sMod[order[j - 1]].rank > sMod[i].rank; j--) {
                order[j] = order[j - 1];
            }
            order[j] = i;
            n++;
        }
    }
    if (n == 0) {
        return 0;
    }
    for (i = 0; i < n; i++) { /* (what says whether the bytes have to be read again) */
        const Mod *m = &sMod[order[i]];

        sig = mods_fnv(sig, m->name, strlen(m->name) + 1);
        sig = mods_fnv(sig, m->size, sizeof(m->size));
        sig = mods_fnv(sig, &m->rawCount, sizeof(m->rawCount));
    }
    if (have && sig == keptSig) {
        return kept;
    }
    h = 0xCBF29CE484222325ull;
    for (i = 0; i < n; i++) {
        const Mod *m = &sMod[order[i]];

        h = mods_fnv(h, m->name, strlen(m->name) + 1);
        for (k = 0; k < KIND_COUNT - 1; k++) { /* (not the portrait) */
            if (m->file[k][0] != '\0') {
                h = mods_file(h, m->dir, m->file[k]);
            }
        }
        for (k = 0; k < m->rawCount && k < 64; k++) {
            h = mods_file(h, m->dir, m->rawPath[k]);
        }
    }
    keptSig = sig;
    kept = h;
    have = 1;
    fprintf(stderr, "bt3: online: %d mods on, their fingerprint %016llx\n", n, (unsigned long long)h);
    return h;
}

/* ------------------------------------------------------------------------------------------------- sharing
 * A warrior goes from one player to another as a line of text: "ZW1:" and its warrior.txt in base64. The one who
 * gets it makes the warrior from their own disc, byte for byte the same files (the builder computes the same on any
 * machine), so the two games' fingerprints agree and they can play online with it. */
extern void PortZw_ClipCopy(const char *text);  /* gs/gs_input.c */
extern void PortZw_ClipAsk(void);
extern int PortZw_ClipTake(char *out, int size);

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* Warrior i's code into `code`. 1 done. */
int Port_WarriorCode(int i, char *code, int size) {
    unsigned char txt[11000];
    char path[600];
    FILE *fp;
    size_t n, k, o = 4;

    ensure();
    if (i < 0 || i >= sWarriorCount) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/mods/%s/warrior.txt", root(), sWarrior[i]);
    if ((fp = fopen(path, "rb")) == NULL) {
        return 0;
    }
    n = fread(txt, 1, sizeof(txt), fp);
    fclose(fp);
    if (n == 0 || n >= sizeof(txt) || size < (int)((n + 2) / 3 * 4 + 5)) {
        return 0;
    }
    memcpy(code, "ZW1:", 4);
    for (k = 0; k < n; k += 3) {
        uint32_t v = (uint32_t)txt[k] << 16 | (k + 1 < n ? (uint32_t)txt[k + 1] << 8 : 0) | (k + 2 < n ? txt[k + 2] : 0);

        code[o++] = kB64[v >> 18 & 63];
        code[o++] = kB64[v >> 12 & 63];
        code[o++] = k + 1 < n ? kB64[v >> 6 & 63] : '=';
        code[o++] = k + 2 < n ? kB64[v & 63] : '=';
    }
    code[o] = '\0';
    return 1;
}

/* Warrior i's code onto the clipboard. 1 put there. */
int Port_WarriorShare(int i) {
    static char code[16384];

    if (!Port_WarriorCode(i, code, sizeof(code))) {
        return 0;
    }
    PortZw_ClipCopy(code);
    fprintf(stderr, "bt3: warriors: %s shared (%u characters)\n", sWarrior[i], (unsigned)strlen(code));
    return 1;
}

/* Starts reading the clipboard for a warrior's code (Port_WarriorImportPoll takes it). */
void Port_WarriorImportAsk(void) {
    PortZw_ClipAsk();
}

/* A code's warrior made as a warrior of this phone (or found, when one is already the same); its index, -1 not one. */
int Port_WarriorFromCode(const char *clip) {
    static unsigned char txt[11000], have[11000];
    char folder[64], name[48], path[600];
    const char *at, *p;
    size_t n = 0;
    uint32_t v = 0;
    int bits = 0, i;
    FILE *fp;

    if ((at = strstr(clip, "ZW1:")) == NULL) {
        return -1;
    }
    for (p = at + 4; *p != '\0' && n + 1 < sizeof(txt); p++) { /* (spaces and line breaks a chat put in are skipped) */
        const char *c = strchr(kB64, *p);

        if (*p == '=') {
            break;
        }
        if (c == NULL || *p == '\0') {
            if (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') {
                continue;
            }
            break;
        }
        v = v << 6 | (uint32_t)(c - kB64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            txt[n++] = (unsigned char)(v >> bits & 0xFF);
        }
    }
    txt[n] = '\0';
    if (n < 10 || strstr((const char *)txt, "body=") == NULL) {
        return -1;
    }
    snprintf(name, sizeof(name), "%s", "Warrior");
    if ((p = strstr((const char *)txt, "\nname=")) != NULL || strncmp((const char *)txt, "name=", 5) == 0) {
        const char *s = p != NULL ? p + 6 : (const char *)txt + 5;
        size_t k = 0;

        while (s[k] != '\0' && s[k] != '\r' && s[k] != '\n' && k + 1 < sizeof(name)) {
            name[k] = (s[k] == '/' || s[k] == '\\' || s[k] == ':' || s[k] == '*' || s[k] == '?' || s[k] == '"' || s[k] == '<' ||
                       s[k] == '>' || s[k] == '|') ? '_' : s[k];
            k++;
        }
        name[k] = '\0';
    }
    ensure();
    for (i = 0; i < sWarriorCount; i++) {
        size_t m;

        snprintf(path, sizeof(path), "%s/mods/%s/warrior.txt", root(), sWarrior[i]);
        if ((fp = fopen(path, "rb")) == NULL) {
            continue;
        }
        m = fread(have, 1, sizeof(have), fp);
        fclose(fp);
        if (m == n && memcmp(have, txt, n) == 0) {
            return i;
        }
    }
    warrior_folder(name[0] != '\0' ? name : "Warrior", folder, sizeof(folder));
    snprintf(path, sizeof(path), "%s/mods/%s", root(), folder);
    make_dir(path);
    snprintf(path, sizeof(path), "%s/mods/%s/warrior.txt", root(), folder);
    if ((fp = fopen(path, "wb")) == NULL) {
        return -1;
    }
    fwrite(txt, 1, n, fp);
    fclose(fp);
    fprintf(stderr, "bt3: warriors: %s imported from a code\n", folder);
    Port_ModRescan(); /* (its files are made as the library is read) */
    return warrior_index(folder);
}

/* An online rival's warrior is a guest: made for the room and gone with it (nobody keeps another's). A mark file in
   its folder says so, so one left by a crash goes the next time too. A warrior this phone already had is not a guest. */
int Port_WarriorGuest(const char *code) {
    char path[600];
    int before, i;
    FILE *fp;

    ensure();
    before = sWarriorCount;
    i = Port_WarriorFromCode(code);
    if (i >= 0 && i < sWarriorCount && sWarriorCount > before) {
        snprintf(path, sizeof(path), "%s/mods/%s/online_guest", root(), sWarrior[i]);
        if ((fp = fopen(path, "wb")) != NULL) {
            fclose(fp);
        }
    }
    return i;
}

int Port_WarriorIsGuest(int i) {
    char path[600];
    FILE *fp;

    ensure();
    if (i < 0 || i >= sWarriorCount) {
        return 0;
    }
    snprintf(path, sizeof(path), "%s/mods/%s/online_guest", root(), sWarrior[i]);
    if ((fp = fopen(path, "rb")) == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

/* Every guest removed; how many. */
int Port_WarriorDropGuests(void) {
    extern int Port_WarriorRemove(int i);
    char path[600];
    int i, n = 0;

    ensure();
    for (i = sWarriorCount - 1; i >= 0; i--) {
        if (i < sWarriorCount && Port_WarriorIsGuest(i)) {
            snprintf(path, sizeof(path), "%s/mods/%s/online_guest", root(), sWarrior[i]);
            Port_WarriorRemove(i);
            remove(path);
            n++;
        }
    }
    return n;
}

int Port_ModToggle(int i);
/* Warrior i as a fighter of a battle: its character and costume number (switched on if it was not). 1 done. */
int Port_WarriorFight(int i, int *chara, int *costume) {
    int m, k, round;

    ensure();
    if (i < 0 || i >= sWarriorCount || (m = mod_by_name(sWarrior[i])) < 0) {
        return 0;
    }
    for (round = 0; round < 2; round++) {
        for (k = 0; k < sSkinCount; k++) {
            if (sSkin[k] == m) {
                *chara = target_of(&sMod[m]);
                *costume = 8 + k * 4;
                return 1;
            }
        }
        if (!sMod[m].on) {
            Port_ModToggle(m);
        }
    }
    return 0;
}

/* The clipboard's warrior, once it has been read: made as a new warrior; its index, -1 no warrior's code there,
   -2 not read yet. */
int Port_WarriorImportPoll(void) {
    static char clip[16384];

    return PortZw_ClipTake(clip, sizeof(clip)) ? Port_WarriorFromCode(clip) : -2;
}
