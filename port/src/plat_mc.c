/*
 * Memory card library (Sony libmc) on PC: slot 1 is a formatted card whose contents are ordinary files in a folder;
 * slot 2 is empty.
 *
 *     saves/card1/BASLUS-21678DBZT3/...     (the folder is `saves` in the current directory, or $BT3_SAVES)
 *
 * The game's paths ("/BASLUS-21678DBZT3/icon.sys") are used as they are below that folder, so a save made here
 * has the same files as one on a console card (icon.sys, the icon, the data file).
 *
 * The library is asynchronous: a call only starts a request and returns 0; sceMcSync reports the request's number
 * and its result. Here every request is carried out at once and sceMcSync hands the stored result over.
 * Results follow libmc (what src/battle/stgm_a_b.c and src/sys/mcflow_a.c test):
 *     sceMcGetInfo   0 = the card seen last time, -1 = a formatted card seen for the first time, -10 = no card;
 *                    type 2 = a PS2 card, free = free clusters of 1 KB, format 1 = formatted
 *     sceMcOpen      a file handle >= 0, or -4 = no such file      (mode: 1 read, 2 write, 0x200 create)
 *     sceMcMkdir     0, or -4 = it exists already
 *     sceMcGetDir    the number of entries that match (the game only asks "is this directory there": 1 or 0)
 *     sceMcRead / sceMcWrite   bytes transferred;   sceMcSeek   the new position;   sceMcClose / sceMcFormat   0
 * The names are the library's addresses in the PS2 executable; the game sources #define the sce names onto them.
 */
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path) /* Windows: no permission bits */
#endif

#define MC_FILES 8
#define MC_FREE_CLUSTERS 8000 /* an empty 8 MB card */

static int sLastCmd, sLastResult;
static int sSeen;             /* slot 1 was reported once: later sceMcGetInfo calls answer "same card" */
/* An open file as the game knows it (part of the game's state): open or not, how it was opened, which host file,
   the position. The host's own file object is beside it (not state) and is brought in line when it is used, so a
   frame that is run again after the state was restored finds the files as that state says. */
#include "port_host.h"
static struct { int open, mode; long pos; char path[600]; } sMc[MC_FILES];
PORT_HOST static FILE *sHost[MC_FILES] = {NULL};
PORT_HOST static char sHostPath[MC_FILES][600] = {{0}};

static FILE *host_file(int fd) {
    if (sHost[fd] != NULL && strcmp(sHostPath[fd], sMc[fd].path) != 0) {
        fclose(sHost[fd]);
        sHost[fd] = NULL;
    }
    if (sHost[fd] == NULL) {
        sHost[fd] = fopen(sMc[fd].path, (sMc[fd].mode & 2) ? "r+b" : "rb");
        snprintf(sHostPath[fd], sizeof(sHostPath[fd]), "%s", sMc[fd].path);
    }
    return sHost[fd];
}

static int request(int cmd, int result) {
    sLastCmd = cmd;
    sLastResult = result;
    return 0;
}

/* The host path of a card path; creates the card's folder on first use. Returns 0 for slot 2 (no card). */
static int host_path(int port, const char *name, char *out, size_t size) {
    const char *root = getenv("BT3_SAVES") != NULL ? getenv("BT3_SAVES") : "saves";
    char dir[512];

    if (port != 0) {
        return 0;
    }
    mkdir(root, 0755);
    snprintf(dir, sizeof(dir), "%s/card1", root);
    mkdir(dir, 0755);
    while (*name == '/') {
        name++;
    }
    if (strstr(name, "..") != NULL) {
        return 0;
    }
    snprintf(out, size, "%s/%s", dir, name);
    return 1;
}

int func_002A1A48(void) { return 0; } /* sceMcInit */

/* sceMcGetInfo: the outputs are filled when the request completes. */
int func_002A25B8(int port, int slot, int *type, int *freeClusters, int *format) {
    char path[600];
    int card = slot == 0 && host_path(port, "", path, sizeof(path));

    if (type != NULL) { *type = card ? 2 : 0; }
    if (freeClusters != NULL) { *freeClusters = card ? MC_FREE_CLUSTERS : 0; }
    if (format != NULL) { *format = card ? 1 : 0; }
    if (!card) {
        return request(1, -10);
    }
    if (!sSeen) {
        sSeen = 1;
        return request(1, -1);
    }
    return request(1, 0);
}

/* sceMcOpen */
int func_002A1E58(int port, int slot, char *name, int mode) {
    char path[600];
    FILE *f;
    int fd;

    if (slot != 0 || !host_path(port, name, path, sizeof(path))) {
        return request(2, -10);
    }
    for (fd = 0; fd < MC_FILES && sMc[fd].open; fd++) {
    }
    if (fd == MC_FILES) {
        return request(2, -7); /* too many open files */
    }
    f = fopen(path, (mode & 2) ? "r+b" : "rb");
    if (f == NULL && (mode & 0x200)) {
        f = fopen(path, "w+b");
    }
    if (f == NULL) {
        return request(2, -4);
    }
    if (sHost[fd] != NULL) {
        fclose(sHost[fd]);
    }
    sHost[fd] = f;
    snprintf(sHostPath[fd], sizeof(sHostPath[fd]), "%s", path);
    sMc[fd].open = 1;
    sMc[fd].mode = mode;
    sMc[fd].pos = 0;
    snprintf(sMc[fd].path, sizeof(sMc[fd].path), "%s", path);
    return request(2, fd);
}

/* sceMcMkdir */
int func_002A1F80(int port, int slot, char *name) {
    char path[600];

    if (slot != 0 || !host_path(port, name, path, sizeof(path))) {
        return request(11, -10);
    }
    return request(11, mkdir(path, 0755) == 0 ? 0 : -4);
}

/* sceMcClose */
int func_002A1FB8(int fd) {
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open) {
        return request(3, -4);
    }
    if (sHost[fd] != NULL) {
        fclose(sHost[fd]);
        sHost[fd] = NULL;
    }
    sMc[fd].open = 0;
    return request(3, 0);
}

/* sceMcSeek: origin 0 = start, 1 = current, 2 = end */
int func_002A2078(int fd, int offset, int origin) {
    FILE *f;
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(4, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    fseek(f, offset, origin == 0 ? SEEK_SET : origin == 1 ? SEEK_CUR : SEEK_END);
    sMc[fd].pos = ftell(f);
    return request(4, (int)sMc[fd].pos);
}

/* sceMcRead */
int func_002A2208(int fd, void *buf, int size) {
    FILE *f;
    int n;
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(5, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    n = (int)fread(buf, 1, (size_t)size, f);
    sMc[fd].pos += n;
    return request(5, n);
}

/* sceMcWrite */
int func_002A2320(int fd, void *buf, int size) {
    int n;

    FILE *f;
    if (fd < 0 || fd >= MC_FILES || !sMc[fd].open || (f = host_file(fd)) == NULL) {
        return request(6, -4);
    }
    fseek(f, sMc[fd].pos, SEEK_SET);
    n = (int)fwrite(buf, 1, (size_t)size, f);
    fflush(f);
    sMc[fd].pos += n;
    return request(6, n);
}

/* sceMcGetDir: the number of entries matching `name` ("*" = everything in the root, otherwise one name). The
   table is not filled: nothing in the game reads it. */
int func_002A27A8(int port, int slot, char *name, int mode, int maxent, void *table) {
    char path[600];
    struct stat st;
    int n = 0;

    (void)mode; (void)maxent; (void)table;
    if (slot != 0 || !host_path(port, strcmp(name, "*") == 0 ? "" : name, path, sizeof(path))) {
        return request(13, -10);
    }
    if (strcmp(name, "*") == 0) {
        DIR *d = opendir(path);
        struct dirent *e;
        while (d != NULL && (e = readdir(d)) != NULL) {
            if (e->d_name[0] != '.') {
                n++;
            }
        }
        if (d != NULL) {
            closedir(d);
        }
    } else {
        n = stat(path, &st) == 0 ? 1 : 0;
    }
    return request(13, n);
}

/* sceMcFormat: the folder is always "formatted"; nothing is erased. */
int func_002A2AC8(int port, int slot) {
    (void)slot;
    return request(16, port == 0 ? 0 : -10);
}

/* sceMcSync: 1 = the request has finished (its number and result are returned), -1 = nothing was pending. */
int func_002A2498(int mode, int *cmd, int *result) {
    (void)mode;
    if (sLastCmd == 0) {
        return -1;
    }
    if (cmd != NULL) { *cmd = sLastCmd; }
    if (result != NULL) { *result = sLastResult; }
    sLastCmd = 0;
    return 1;
}
