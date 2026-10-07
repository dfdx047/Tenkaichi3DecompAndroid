/* The manifest of songs added to the music select (see plat_songs.h). Runs at start-up: it gives the game the
   added tracks' list offsets and names, and the file layer where their ADX files live. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "plat_songs.h"

char gPortSongNames[PORT_SONG_MAX][64];
int gPortSongCount;
int gPortSongOffsets[PORT_SONG_MAX];

typedef struct SongAlias {
    char rel[48];     /* the path the game asks for, under the data root */
    char target[192]; /* where it is served from */
} SongAlias;

static SongAlias sAliases[PORT_SONG_MAX];
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

void PortSongs_Init(void) {
    char path[512];
    char line[256];
    FILE *fp;
    int i;

    if (sDone) {
        return;
    }
    sDone = 1;
    snprintf(path, sizeof(path), "%s/songs/songs.txt", data_root());
    fp = fopen(path, "rb");
    if (fp == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), fp) != NULL && gPortSongCount < PORT_SONG_MAX) {
        char *bar, *file;
        int offset, index;

        if (line[0] == '\0' || line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        bar = strchr(line, '|');
        if (bar == NULL) {
            continue;
        }
        *bar = '\0';
        file = trim(line);
        i = gPortSongCount;
        offset = PORT_SONG_FIRST_OFFSET + i;
        index = PORT_SONG_BGM_FIRST + offset - 0xD48; /* the game's id -> partition 2 index */

        gPortSongOffsets[i] = offset;
        snprintf(gPortSongNames[i], 64, "%s", trim(bar + 1));
        snprintf(sAliases[sAliasCount].rel, sizeof(sAliases[sAliasCount].rel), "pzs3us2/%05d.bin", index);
        snprintf(sAliases[sAliasCount].target, sizeof(sAliases[sAliasCount].target), "songs/%s", file);
        sAliasCount++;
        gPortSongCount++;
    }
    fclose(fp);
    if (gPortSongCount > 0) {
        fprintf(stderr, "bt3: songs: %d added track(s) from %s\n", gPortSongCount, path);
    }
}

int PortSongs_Count(void) {
    return gPortSongCount;
}

int PortSongs_Offset(int index) {
    if (index < 0 || index >= gPortSongCount) {
        return 0;
    }
    return gPortSongOffsets[index];
}

int PortSongs_Alias(const char *rel, char *out, unsigned n) {
    int i;
    for (i = 0; i < sAliasCount; i++) {
        if (strcmp(sAliases[i].rel, rel) == 0) {
            snprintf(out, n, "%s", sAliases[i].target);
            return 1;
        }
    }
    return 0;
}
