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
    i = gPortExtraStageCount;
    id = PORT_STAGE_FIRST + i;
    snprintf(gPortStageNames[i], 64, "%s", name);
    gPortExtraStages[i] = id;

    /* model: the game asks for file id 0x171 + stage -> partition 1, index 0x170 + stage */
    snprintf(sAliases[sAliasCount].rel, sizeof(sAliases[sAliasCount].rel), "pzs3us1/%05d.bin", 0x170 + id);
    snprintf(sAliases[sAliasCount].target, sizeof(sAliases[sAliasCount].target), "stages/%s", file);
    sAliasCount++;

    /* sound bank: file id 0x14E + stage -> the shared bank of the first stage, so the map has music */
    snprintf(sAliases[sAliasCount].rel, sizeof(sAliases[sAliasCount].rel), "pzs3us1/%05d.bin", 0x14D + id);
    snprintf(sAliases[sAliasCount].target, sizeof(sAliases[sAliasCount].target), "pzs3us1/00333.bin");
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
    snprintf(path, sizeof(path), "%s/stages/maps.txt", data_root());
    fp = fopen(path, "rb");
    if (fp == NULL) {
        from_env();
        if (gPortExtraStageCount > 0) {
            fprintf(stderr, "bt3: stages: %d from BT3_EXTRA_STAGES (no %s)\n", gPortExtraStageCount, path);
        }
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
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
    fclose(fp);
    if (gPortExtraStageCount > 0) {
        fprintf(stderr, "bt3: stages: %d map(s) from %s\n", gPortExtraStageCount, path);
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
