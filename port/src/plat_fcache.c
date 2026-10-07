/*
 * The host's open files behind the game's file handles (plat_file.c).
 *
 * A handle the game holds says only which file it is and where its read position is: things that can be saved and
 * restored with the game's state. The operating system's file objects are kept here, outside that state, found by
 * the file's relative path and opened when first needed; a read always says where in the file it reads. So a frame
 * that is run again after a restore (online play rewinds and replays frames) opens and reads the same files and
 * gets the same data, and nothing is opened or closed twice. A few dozen files stay open; the one unused longest
 * is closed when another is needed.
 * (Its variables must NOT be part of the saved state: port/tools/make_state.py leaves this file out.)
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern void Port_StateTouch(void *p, unsigned long n); /* gs/state.c */

extern const char *Port_FileRoot(void); /* plat_file.c: BT3_DATA or "gamedata" */

enum { SLOTS = 32 };
static struct { char rel[128]; FILE *fp; long size; unsigned long used; } sSlot[SLOTS];
static unsigned long sClock;
static volatile int sLock;

static int slot_of(const char *rel) {
    char path[512];
    int i, old = 0;
    FILE *fp;

    for (i = 0; i < SLOTS; i++) {
        if (sSlot[i].fp != NULL && strcmp(sSlot[i].rel, rel) == 0) {
            sSlot[i].used = ++sClock;
            return i;
        }
    }
    {
        /* a stage added from outside the disc: the game asks for a file id past the disc's own, and the file is
           the one the stage list names (plat_stages.c), under the data folder */
        extern int PortStages_Alias(const char *rel, char *out, unsigned n);
        char target[256];
        fp = NULL;
        extern int PortSongs_Alias(const char *rel, char *out, unsigned n); /* plat_songs.c: the same for a song */
        if (PortStages_Alias(rel, target, (unsigned)sizeof(target)) || PortSongs_Alias(rel, target, (unsigned)sizeof(target))) {
            snprintf(path, sizeof(path), "%s/%s", Port_FileRoot(), target);
            fp = fopen(path, "rb");
        }
    }
    if (fp == NULL) { /* a file under mods/ wins over the original */
        snprintf(path, sizeof(path), "%s/mods/%s", Port_FileRoot(), rel);
        fp = fopen(path, "rb");
    }
    if (fp == NULL) {
        snprintf(path, sizeof(path), "%s/%s", Port_FileRoot(), rel);
        fp = fopen(path, "rb");
    }
    if (fp == NULL) {
        fprintf(stderr, "bt3: file not found: %s\n", path);
        return -1;
    }
    for (i = 0; i < SLOTS; i++) {
        if (sSlot[i].fp == NULL) {
            old = i;
            break;
        }
        if (sSlot[i].used < sSlot[old].used) {
            old = i;
        }
    }
    if (sSlot[old].fp != NULL) {
        fclose(sSlot[old].fp);
    }
    snprintf(sSlot[old].rel, sizeof(sSlot[old].rel), "%s", rel);
    sSlot[old].fp = fp;
    fseek(fp, 0, SEEK_END);
    sSlot[old].size = ftell(fp);
    sSlot[old].used = ++sClock;
    return old;
}

/* The file's size in bytes, or -1 if there is no such file. */
long Port_FileSize(const char *rel) {
    long size;
    int i;
    while (__sync_lock_test_and_set(&sLock, 1)) {
    }
    i = slot_of(rel);
    size = i < 0 ? -1 : sSlot[i].size;
    __sync_lock_release(&sLock);
    return size;
}

/* Reads `bytes` bytes at `offset`; returns how many there were. */
size_t Port_FileReadAt(const char *rel, long offset, void *buf, size_t bytes) {
    size_t got = 0;
    int i;
    while (__sync_lock_test_and_set(&sLock, 1)) {
    }
    i = slot_of(rel);
    if (i >= 0 && fseek(sSlot[i].fp, offset, SEEK_SET) == 0) {
        Port_StateTouch(buf, bytes); /* (gs/state.c: file data straight into watched memory) */
        got = fread(buf, 1, bytes, sSlot[i].fp);
    }
    __sync_lock_release(&sLock);
    return got;
}
