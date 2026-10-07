/* The manifest of stages added from outside the disc (see plat_stages.h). Runs before the game's menus: the ids,
   the display names and the file aliases the game and the file layer use all come from here. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat_stages.h"

char gPortStageNames[PORT_STAGE_MAX][64];
int gPortExtraStageCount;
int gPortExtraStages[PORT_STAGE_MAX];

typedef struct StageAlias {
    char rel[48];   /* the path the game asks for, under the data root */
    char target[192]; /* where it is served from */
} StageAlias;

static StageAlias sAliases[PORT_STAGE_MAX * 2];
static int sAliasCount;
static int sDone;
static long sLargest; /* the largest added stage file */

/* The game loads every stage of a battle into one buffer of 0x6CB800 bytes (BTL_STAGE_BUF_SIZE), sized for the
   disc's stages on a console with 32 MB. A stage made for the port can be larger: the buffer then is as large as
   the largest added stage, up to PORT_STAGE_BUF_MAX (it comes out of the game's own 28 MB heap, which a battle
   also needs for its fighters). With no larger stage installed, and in an online session (where added stages are
   not offered), it is the game's own size, so nothing about the game's memory changes. BT3_STAGE_BUF=<bytes>
   names a size (testing: a stage put in a disc stage's place through the mods folder). */
#define PORT_STAGE_BUF_GAME 0x6CB800
#define PORT_STAGE_BUF_MAX 0x4000000 /* 64 MB: a larger buffer than the game's is not in its heap (Port_GameBigAlloc) */
static char sDir[256]; /* the folder the stages are in (plat_extras.c) */
static char sFiles[PORT_STAGE_MAX][128];

extern int PortExtras_Dir(const char *kind, const char *env, char *out, unsigned size);
extern int PortExtras_Scan(const char *dir, const char *ext, char names[][128], int max);
extern void PortExtras_NameFromFile(const char *file, char *name, unsigned size);

void PortStages_Init(void);

const char *PortStages_Dir(void) {
    PortStages_Init(); /* (the overlay may ask before the game has started) */
    return sDir;
}

static const char *data_root(void) {
    const char *r = getenv("BT3_DATA");
    return r != NULL ? r : "gamedata";
}

static char *trim(char *s) {
    char *e;
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
        *--e = '\0';
    }
    return s;
}

/* Adds one added stage: its next id, its display name, and the file aliases it needs. */
static void add_stage(const char *file, const char *name) {
    int id, i;

    if (gPortExtraStageCount >= PORT_STAGE_MAX) {
        fprintf(stderr, "bt3: stages: more than %d added maps, the rest are ignored\n", PORT_STAGE_MAX);
        return;
    }
    for (i = 0; i < gPortExtraStageCount; i++) {
        if (strcmp(sFiles[i], file) == 0) {
            return; /* (in the list and found in the folder: once) */
        }
    }
    {
        /* its size decides the buffer the game loads stages into (Port_StageBufSize) */
        char path[512];
        FILE *fp;
        long size = 0;
        snprintf(path, sizeof(path), "%s/%s", sDir, file);
        fp = fopen(path, "rb");
        if (fp != NULL) {
            fseek(fp, 0, SEEK_END);
            size = ftell(fp);
            fclose(fp);
        }
        if (size > PORT_STAGE_BUF_MAX) {
            fprintf(stderr, "bt3: stages: %s is %ld bytes, more than the %d a stage can have here: left out\n", file, size, PORT_STAGE_BUF_MAX);
            return;
        }
        if (size > sLargest) {
            sLargest = size;
        }
    }
    i = gPortExtraStageCount;
    snprintf(sFiles[i], sizeof(sFiles[i]), "%s", file);
    id = PORT_STAGE_FIRST + i;
    snprintf(gPortStageNames[i], 64, "%s", name);
    gPortExtraStages[i] = id;

    /* Its model: the game asks for a file id of the stage's own, 30000 + n past the start of the second archive
       (battle_load.c, PORT_ADDED_STAGE_FILE), where the disc has nothing. (It used to be 0x171 + stage, which is
       also the split-screen model of a disc stage from the fourth added stage on; and a second redirection, for
       a sound bank at 0x14E + stage, took the place of the disc stages' own models, the first added stage that
       of stage 1: with a stage added, a one-player fight on that disc stage was given the wrong file.) */
    snprintf(sAliases[sAliasCount].rel, sizeof(sAliases[sAliasCount].rel), "pzs3us2/%05d.bin", 30000 + i);
    snprintf(sAliases[sAliasCount].target, sizeof(sAliases[sAliasCount].target), "%s/%s", sDir, file);
    sAliasCount++;

    gPortExtraStageCount++;
}

/* No manifest: the old comma-separated BT3_EXTRA_STAGES=0x24,0x25 still names the ids (aliases then come from
   BT3_FILE_ALIAS, which plat_file.c reads). Kept so the tests and a manual run still work. */
static void from_env(void) {
    const char *p = getenv("BT3_EXTRA_STAGES");

    while (p != NULL && *p != '\0' && gPortExtraStageCount < PORT_STAGE_MAX) {
        char *end;
        long id = strtol(p, &end, 0);
        if (end == p) {
            break;
        }
        gPortExtraStages[gPortExtraStageCount] = (int)id;
        gPortStageNames[gPortExtraStageCount][0] = '\0';
        gPortExtraStageCount++;
        p = end;
        while (*p == ',' || *p == ' ') {
            p++;
        }
    }
}

void PortStages_Init(void) {
    char path[512];
    char line[256];
    FILE *fp;

    if (sDone) {
        return;
    }
    sDone = 1;
    if (!PortExtras_Dir("stages", "BT3_STAGES", sDir, sizeof(sDir))) {
        from_env();
        if (gPortExtraStageCount > 0) {
            fprintf(stderr, "bt3: stages: %d from BT3_EXTRA_STAGES (no stages folder)\n", gPortExtraStageCount);
        }
        return;
    }
    /* the list first (its names and its order), then whatever else is in the folder, by name */
    snprintf(path, sizeof(path), "%s/maps.txt", sDir);
    fp = fopen(path, "rb");
    while (fp != NULL && fgets(line, sizeof(line), fp) != NULL) {
        char *bar, *file;
        if (line[0] == '\0' || line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        bar = strchr(line, '|');
        if (bar == NULL) {
            continue;
        }
        *bar = '\0';
        file = trim(line);
        add_stage(file, trim(bar + 1));
    }
    if (fp != NULL) {
        fclose(fp);
    }
    {
        static char found[PORT_STAGE_MAX][128];
        int n = PortExtras_Scan(sDir, ".unk", found, PORT_STAGE_MAX), k;
        for (k = 0; k < n; k++) {
            char name[64];
            PortExtras_NameFromFile(found[k], name, sizeof(name));
            add_stage(found[k], name);
        }
    }
    if (gPortExtraStageCount > 0) {
        fprintf(stderr, "bt3: stages: %d added from %s\n", gPortExtraStageCount, sDir);
    }
}

int PortStages_Count(void) {
    return gPortExtraStageCount;
}

const char *PortStages_Name(int index) {
    if (index < 0 || index >= gPortExtraStageCount) {
        return "";
    }
    return gPortStageNames[index];
}

int PortStages_Alias(const char *rel, char *out, unsigned n) {
    int i;
    for (i = 0; i < sAliasCount; i++) {
        if (strcmp(sAliases[i].rel, rel) == 0) {
            snprintf(out, n, "%s", sAliases[i].target);
            return 1;
        }
    }
    return 0;
}

int Port_StageBufSize(void) {
    extern int Port_NetSession(void); /* gs/net.c */
    const char *e = getenv("BT3_STAGE_BUF");
    long size = PORT_STAGE_BUF_GAME;

    PortStages_Init();
    if (e != NULL) {
        size = strtol(e, NULL, 0);
    } else if (!Port_NetSession() && sLargest > size) {
        size = sLargest;
    }
    if (size < PORT_STAGE_BUF_GAME) {
        size = PORT_STAGE_BUF_GAME;
    }
    if (size > PORT_STAGE_BUF_MAX) {
        size = PORT_STAGE_BUF_MAX;
    }
    return (int)((size + 0x7FF) & ~0x7FFL); /* whole sectors, as the reads are */
}

/* A stage file about to be read: 0 if it does not fit the buffer (the caller says so and stops; reading it
   would overrun the buffer and corrupt the game's heap, which showed as a crash seconds later). */
int Port_StageFits(const char *rel, long size) {
    int index = -1;
    if (strncmp(rel, "pzs3us2/", 8) == 0 && sscanf(rel + 8, "%d", &index) == 1 && index >= 30000) {
        return size <= Port_StageBufSize(); /* an added stage */
    }
    if (strncmp(rel, "pzs3us1/", 8) != 0 || sscanf(rel + 8, "%d", &index) != 1) {
        return 1;
    }
    /* the disc's stage models: 368..402 for one screen, 407..441 split (a file in mods/ may stand in for one) */
    if ((index >= 368 && index <= 402) || (index >= 407 && index <= 441)) {
        return size <= Port_StageBufSize();
    }
    return 1;
}

/* BT3_TEST_STAGE=<id>: every battle is set up on that stage, whatever the menus chose (testing: a recorded
   session can then be played on any stage, an added one too: 0x24 is the first). -1 without it. */
int Port_TestStage(void) {
    const char *e = getenv("BT3_TEST_STAGE");
    return e != NULL ? (int)strtol(e, NULL, 0) : -1;
}

/* BT3_TEST_SCREEN=<mode>: the same for the screen mode (0 one screen, 1 split): a two-player recording then
   loads the one-screen model of its stage. */
int Port_TestScreen(void) {
    const char *e = getenv("BT3_TEST_SCREEN");
    return e != NULL ? (int)strtol(e, NULL, 0) : -1;
}
