/*
 * Memory card operations (0x116B98..0x1198D8). The file stem is a placeholder: this is system code
 * (proper home src/sys/mcard.c), unrelated to the stage drawing in front of it.
 *
 * Every operation is polled: the caller invokes it once per frame until it returns 1. gMcCardCmd names the
 * operation that owns the step counter gMcCardStep; an operation entered while another one's command is set
 * restarts from step 0. Each step issues one libmc call or polls sceMcSync once. On an error the step simply
 * does not advance and the function keeps returning 0: the caller looks at the port record (ret / result),
 * classifies the code with McCard_IsFatalError and resets the step with McCard_ResetStep.
 * A library call that returns -200 (the library is still busy) is answered with sceMcSync(1, NULL, NULL).
 * sceMcSync(1, ...) returns 0 while the pending call runs; most steps go on for any other value, the steps
 * written `!= 1` only for "finished".
 *
 * Library functions (Sony libmc, not named yet; identified by their arguments):
 *   func_002A1A48 sceMcInit()                         func_002A1E58 sceMcOpen(port, slot, name, mode)
 *   func_002A1F80 sceMcMkdir(port, slot, name)        func_002A1FB8 sceMcClose(fd)
 *   func_002A2078 sceMcSeek(fd, offset, origin)       func_002A2208 sceMcRead(fd, buf, size)
 *   func_002A2320 sceMcWrite(fd, buf, size)           func_002A2498 sceMcSync(mode, cmd, result)
 *   func_002A25B8 sceMcGetInfo(port, slot, type, free, format)
 *   func_002A27A8 sceMcGetDir(port, slot, name, mode, maxent, table)
 *   func_002A2AC8 sceMcFormat(port, slot)
 */
#include "common.h"
#include "sys/heap.h"
#include "battle/stgm_a_b.h"

extern void *memset(void *dst, s32 c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern char *strcpy(char *dst, const char *src);
extern s32 sprintf(char *dst, const char *fmt, ...);

extern s32 func_002A1A48(void);
extern s32 func_002A1E58(s32 port, s32 slot, char *name, s32 mode);
extern s32 func_002A1F80(s32 port, s32 slot, char *name);
extern s32 func_002A1FB8(s32 fd);
extern s32 func_002A2078(s32 fd, s32 offset, s32 origin);
extern s32 func_002A2208(s32 fd, void *buf, s32 size);
extern s32 func_002A2320(s32 fd, void *buf, s32 size);
extern s32 func_002A2498(s32 mode, s32 *cmd, s32 *result);
extern s32 func_002A25B8(s32 port, s32 slot, s32 *type, s32 *free, s32 *format);
extern s32 func_002A27A8(s32 port, s32 slot, char *name, s32 mode, s32 maxent, void *table);
extern s32 func_002A2AC8(s32 port, s32 slot);

#define sceMcInit func_002A1A48
#define sceMcOpen func_002A1E58
#define sceMcMkdir func_002A1F80
#define sceMcClose func_002A1FB8
#define sceMcSeek func_002A2078
#define sceMcRead func_002A2208
#define sceMcWrite func_002A2320
#define sceMcSync func_002A2498
#define sceMcGetInfo func_002A25B8
#define sceMcGetDir func_002A27A8
#define sceMcFormat func_002A2AC8

/* 0x2FE918 / 0x2FE91C: the head of this object's .sdata, in front of its strings. */
s32 gMcCardStep = 0;
s32 gMcCardCmd = 0;
extern McCardPort gMcCardPort[2];
extern McCardIconSys gMcCardIconSys;

/* Sony's 16-byte aligned vectors (sceVu0IVECTOR / sceVu0FVECTOR): the icon.sys constants are locals of these
 * types (they are initialised with aligned 64-bit copies), the fields of icon.sys itself are not aligned. */
typedef s32 McCardIVec[4] __attribute__((aligned(16)));
typedef f32 McCardFVec[4] __attribute__((aligned(16)));

/* An operation entered while another operation's command is set starts over. */
#define MCCARD_ENTER(cmd) \
    if (gMcCardCmd != 0) { \
        gMcCardStep = (gMcCardCmd == (cmd)) ? gMcCardStep : 0; \
    }

/* The step of the running operation. */
s32 McCard_GetStep(void) {
    return gMcCardStep;
}

/* Back to step 0. */
void McCard_ResetStep(void) {
    gMcCardStep = 0;
}

/* Boot: clears the port records, starts libmc and probes port 0. */
void McCard_Init(void) {
    memset(gMcCardPort, 0, sizeof(gMcCardPort));
    sceMcInit();
    McCard_Probe(0);
}

/* Reads type, free space and format state of a card. Step 2 = finished; the caller resets the step. */
s32 McCard_GetInfo(s32 port) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_INFO);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_INFO;
        ret = sceMcGetInfo(port, 0, &gMcCardPort[port].type, &gMcCardPort[port].free, &gMcCardPort[port].format);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 1) {
            gMcCardStep++;
            return 1;
        }
        return 0;
    }
    /* Past step 1 nothing is returned (v0 still holds the 1 of the step comparison). */
}

/* The same, waiting for the result inside the call. */
s32 McCard_GetInfoSync(s32 port) {
    gMcCardPort[port].result = 0;
    gMcCardCmd = MCCARD_CMD_INFO_SYNC;
    gMcCardPort[port].ret = sceMcGetInfo(port, 0, &gMcCardPort[port].type, &gMcCardPort[port].free,
                                         &gMcCardPort[port].format);
    return sceMcSync(0, NULL, &gMcCardPort[port].result);
}

/* Asks for the card state without outputs and waits: only the result code is kept. */
s32 McCard_Probe(s32 port) {
    gMcCardCmd = MCCARD_CMD_INFO_PROBE;
    gMcCardPort[port].result = 0;
    gMcCardPort[port].ret = sceMcGetInfo(port, 0, NULL, NULL, NULL);
    return sceMcSync(0, NULL, &gMcCardPort[port].result);
}

/* The same, polled: 1 when the result is in. */
s32 McCard_CheckChange(s32 port) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_CHANGE);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_CHANGE;
        ret = sceMcGetInfo(port, 0, NULL, NULL, NULL);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        gMcCardStep++;
        return 1;
    }
}

/* Formats the card: 1 when the format finished without error (or when called past its last step). */
s32 McCard_Format(s32 port) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_FORMAT);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_FORMAT;
        ret = sceMcFormat(port, 0);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardStep = 0;
        gMcCardCmd = 0;
        return 1;
    default:
        gMcCardStep = 0;
        gMcCardCmd = 0;
        return 1;
    }
}

/*
 * Creates a save: makes the directory (an existing one is fine), writes icon.sys and the icon file.
 * 1 when the last step finished. The data file is written by McCard_WriteSave.
 */
s32 McCard_CreateSave(s32 port, McCardFile *file) {
    McCardIVec bgColor[4] = {
        { 0xFF, 0x80, 0, 0 }, { 0xFF, 0x80, 0, 0 }, { 0x80, 0x80, 0x80, 0 }, { 0x80, 0x80, 0x80, 0 },
    };
    McCardFVec lightDir[3] = {
        { 0.5f, 0.5f, 0.5f, 0.0f }, { 0.5f, 0.5f, 0.5f, 0.0f }, { 0.5f, 0.5f, 0.5f, 0.0f },
    };
    McCardFVec lightColor[3] = {
        { 0.3f, 0.3f, 0.03f, 0.0f }, { 0.3f, 0.2f, 0.2f, 0.0f }, { 0.14f, 0.14f, 0.2f, 0.0f },
    };
    McCardFVec ambient = { 0.25f, 0.25f, 0.25f, 0.0f };
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_CREATE);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_CREATE;
        ret = sceMcMkdir(port, 0, file->dir);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            if (gMcCardPort[port].result != -4) {
                return 0;
            }
            gMcCardPort[port].result = 0;
        }
        gMcCardStep++;
        return 0;
    case 2:
        gMcCardPort[port].ret = sceMcOpen(port, 0, file->sysPath, 0x202);
        if (gMcCardPort[port].ret < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 3:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 4:
        memset(&gMcCardIconSys, 0, sizeof(McCardIconSys));
        strcpy(gMcCardIconSys.head, "PS2D");
        strcpy(gMcCardIconSys.title, file->title);
        gMcCardIconSys.titleBreak = file->titleBreak;
        gMcCardIconSys.transRate = 0x60;
        memcpy(gMcCardIconSys.bgColor, bgColor, sizeof(bgColor));
        memcpy(gMcCardIconSys.lightDir, lightDir, sizeof(lightDir));
        memcpy(gMcCardIconSys.lightColor, lightColor, sizeof(lightColor));
        memcpy(gMcCardIconSys.ambient, ambient, sizeof(ambient));
        strcpy(gMcCardIconSys.viewName, file->iconName[0]);
        strcpy(gMcCardIconSys.copyName, file->iconName[1]);
        strcpy(gMcCardIconSys.delName, file->iconName[2]);
        gMcCardPort[port].ret = sceMcWrite(gMcCardPort[port].fd, &gMcCardIconSys, sizeof(McCardIconSys));
        if (gMcCardPort[port].ret < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 5:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 6:
        gMcCardPort[port].ret = sceMcClose(gMcCardPort[port].fd);
        if (gMcCardPort[port].ret < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 7:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 8:
        gMcCardPort[port].ret = sceMcOpen(port, 0, file->iconPath[0], 0x202);
        if (gMcCardPort[port].ret < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 9:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 10:
        gMcCardPort[port].ret = sceMcWrite(gMcCardPort[port].fd, file->icon, file->iconSize);
        if (gMcCardPort[port].ret < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 11:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 12:
        gMcCardPort[port].ret = sceMcClose(gMcCardPort[port].fd);
        if (gMcCardPort[port].ret < 0) {
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 13:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardStep++;
        return 1;
    default:
        gMcCardStep = 0;
        gMcCardCmd = 0;
        return 1;
    }
}

/* The two recurring steps: wait for the pending call, and wait for it and require a result >= 0. */
#define MCCARD_WAIT() \
    if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) { \
        return 0; \
    }
#define MCCARD_WAIT_OK() \
    MCCARD_WAIT(); \
    if (gMcCardPort[port].result < 0) { \
        return 0; \
    }
/* Issue a call; a negative return value leaves the step where it is. */
#define MCCARD_CALL(call) \
    gMcCardPort[port].ret = (call); \
    if (gMcCardPort[port].ret < 0) { \
        return 0; \
    }

/*
 * Writes the data file of a save: opens it write-only with the create flag (0x202) and, when that fails,
 * once more without the flag (2); writes `size` bytes and requires that many to be reported (result -8
 * otherwise). Then the icon file: opened read-write (3); when it is missing (-4) it is created, when its length
 * (seek to the end) differs from the request's icon size it is rewritten from the start, otherwise it is only
 * closed. 1 when the last step finished.
 */
s32 McCard_WriteSave(s32 port, void *data, s32 size, McCardFile *file) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_SAVE);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_SAVE;
        ret = sceMcOpen(port, 0, file->dataPath, 0x202);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        MCCARD_WAIT();
        if (gMcCardPort[port].result < 0) {
            gMcCardPort[port].result = 0;
            gMcCardStep = 2;
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep = 4;
        return 0;
    case 2:
        gMcCardPort[port].ret = sceMcOpen(port, 0, file->dataPath, 2);
        if (gMcCardPort[port].ret < 0) {
            if (gMcCardPort[port].ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 3:
        MCCARD_WAIT_OK();
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 4:
        MCCARD_CALL(sceMcWrite(gMcCardPort[port].fd, data, size));
        gMcCardStep++;
        return 0;
    case 5:
        MCCARD_WAIT_OK();
        if (gMcCardPort[port].result != size) {
            gMcCardPort[port].result = -8;
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 6:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 7:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 8:
        MCCARD_CALL(sceMcOpen(port, 0, file->iconPath[0], 3));
        gMcCardStep++;
        return 0;
    case 9:
        MCCARD_WAIT();
        if (gMcCardPort[port].result < 0) {
            if (gMcCardPort[port].result == -4) {
                gMcCardPort[port].result = 0;
                gMcCardStep++;
            }
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep = 12;
        return 0;
    case 10:
        MCCARD_CALL(sceMcOpen(port, 0, file->iconPath[0], 0x202));
        gMcCardStep++;
        return 0;
    case 11:
        MCCARD_WAIT_OK();
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep = 16;
        return 0;
    case 12:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 2));
        gMcCardStep++;
        return 0;
    case 13:
        MCCARD_WAIT_OK();
        if (file->iconSize != gMcCardPort[port].result) {
            gMcCardStep++;
            return 0;
        }
        gMcCardStep = 18;
        return 0;
    case 14:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 0));
        gMcCardStep++;
        return 0;
    case 15:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 16:
        MCCARD_CALL(sceMcWrite(gMcCardPort[port].fd, file->icon, file->iconSize));
        gMcCardStep++;
        return 0;
    case 17:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 18:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 19:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 1;
    default:
        gMcCardStep = 0;
        gMcCardCmd = 0;
        return 1;
    }
}

/*
 * Reads `size` bytes of a save's data file into `buf`. The port's flags tell whether exactly that many bytes
 * came back. A failed step resets the operation to step 0. 1 when finished.
 */
s32 McCard_ReadFile(s32 port, void *buf, s32 size, McCardFile *file) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_READ);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].flags = 0;
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_READ;
        ret = sceMcOpen(port, 0, file->dataPath, 1);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        MCCARD_WAIT();
        if (gMcCardPort[port].result < 0) {
            gMcCardStep = 0;
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 2:
        MCCARD_CALL(sceMcRead(gMcCardPort[port].fd, buf, size));
        gMcCardStep++;
        return 0;
    case 3:
        MCCARD_WAIT();
        if (gMcCardPort[port].result < 0) {
            gMcCardStep = 0;
            return 0;
        }
        if (gMcCardPort[port].result != size) {
            gMcCardPort[port].flags |= MCCARD_F_DIFF;
        } else {
            gMcCardPort[port].flags |= MCCARD_F_SAME;
        }
        gMcCardStep++;
        return 0;
    case 4:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 5:
        MCCARD_WAIT();
        if (gMcCardPort[port].result < 0) {
            gMcCardStep = 0;
            return 0;
        }
        gMcCardStep++;
        return 1;
    default:
        return 1;
    }
}

/*
 * Reads a save's whole data file into a heap block (heap 2, 32-byte aligned) that it allocates itself and
 * leaves in the port record. 1 when finished. Nothing calls it.
 */
s32 McCard_LoadFile(s32 port, McCardFile *file) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_LOAD);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].buf = NULL;
        gMcCardPort[port].flags = 0;
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_LOAD;
        ret = sceMcOpen(port, 0, file->dataPath, 1);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        MCCARD_WAIT_OK();
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 2:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 2));
        gMcCardStep++;
        return 0;
    case 3:
        MCCARD_WAIT_OK();
        gMcCardPort[port].size = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 4:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 0));
        gMcCardStep++;
        return 0;
    case 5:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 6:
        gMcCardPort[port].buf = Heap_Alloc(gMcCardPort[port].size, 0x20, 0, 2);
        MCCARD_CALL(sceMcRead(gMcCardPort[port].fd, gMcCardPort[port].buf, gMcCardPort[port].size));
        gMcCardStep++;
        return 0;
    case 7:
        MCCARD_WAIT_OK();
        if (gMcCardPort[port].result != gMcCardPort[port].size) {
            gMcCardPort[port].flags |= MCCARD_F_DIFF;
        }
        gMcCardStep++;
        return 0;
    case 8:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 9:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 1;
    default:
        return 1;
    }
}

/*
 * Opens and closes the data file, icon.sys and the icon file of a save: 1 when all three could be opened.
 * Matching note: the two later open steps share one variable (`r`), written `x = r = f()` in the first and
 * `r = f(); x = r` in the second, and every "library busy" test has the shape
 * `if (v == -200) { sync; return 0; } return 0;`. With that the compiler merges their error paths as the
 * original has them; other spellings change the registers or the merge.
 */
s32 McCard_CheckFiles(s32 port, McCardFile *file) {
    s32 ret;
    s32 r;

    MCCARD_ENTER(MCCARD_CMD_CHECK);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_CHECK;
        ret = sceMcOpen(port, 0, file->dataPath, 1);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) != 1) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 2:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 3:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 4:
        gMcCardPort[port].ret = r = sceMcOpen(port, 0, file->sysPath, 1);
        if (r < 0) {
            if (r == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 5:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) != 1) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 6:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 7:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 8:
        r = sceMcOpen(port, 0, file->iconPath[0], 1);
        gMcCardPort[port].ret = r;
        if (r < 0) {
            if (r == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 9:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) != 1) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 10:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 11:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 1;
    default:
        return 1;
    }
}

/*
 * Fills the names of a save request: the directory "BASLUS-21678<name>" (with two digits appended when `num`
 * is not negative), the paths of icon.sys, of the icon (three identical entries) and of the data file, which
 * has the directory's own name, and the icon file names for icon.sys. `replay` selects dbzsmr.ico.
 */
void McCardFile_SetNames(McCardFile *file, char *name, s32 num, s32 unused, s32 replay) {
    if (num >= 0) {
        memset(file->dir, 0, 0x80);
        sprintf(file->dir, "%s%s%02d", "BASLUS-21678", name, num);
    } else {
        memset(file->dir, 0, 0x80);
        sprintf(file->dir, "%s%s", "BASLUS-21678", name);
    }
    memset(file->sysPath, 0, 0x80);
    sprintf(file->sysPath, "/%s/icon.sys", file->dir);
    memset(file->iconPath[0], 0, 0x80);
    if (replay == 0) {
        sprintf(file->iconPath[0], "/%s/dbzsm.ico", file->dir);
    } else {
        sprintf(file->iconPath[0], "/%s/dbzsmr.ico", file->dir);
    }
    memset(file->iconPath[1], 0, 0x80);
    if (replay == 0) {
        sprintf(file->iconPath[1], "/%s/dbzsm.ico", file->dir);
    } else {
        sprintf(file->iconPath[1], "/%s/dbzsmr.ico", file->dir);
    }
    memset(file->iconPath[2], 0, 0x80);
    if (replay == 0) {
        sprintf(file->iconPath[2], "/%s/dbzsm.ico", file->dir);
    } else {
        sprintf(file->iconPath[2], "/%s/dbzsmr.ico", file->dir);
    }
    memset(file->dataPath, 0, 0x80);
    sprintf(file->dataPath, "/%s/%s", file->dir, file->dir);
    memset(file->iconName[0], 0, 0x40);
    if (replay == 0) {
        strcpy(file->iconName[0], "dbzsm.ico");
    } else {
        strcpy(file->iconName[0], "dbzsmr.ico");
    }
    memset(file->iconName[1], 0, 0x40);
    if (replay == 0) {
        strcpy(file->iconName[1], "dbzsm.ico");
    } else {
        strcpy(file->iconName[1], "dbzsmr.ico");
    }
    memset(file->iconName[2], 0, 0x40);
    if (replay == 0) {
        strcpy(file->iconName[2], "dbzsm.ico");
    } else {
        strcpy(file->iconName[2], "dbzsmr.ico");
    }
}

/* Tells the result codes the caller gives up on: -19..-11, -49..-40, -59..-50 and -79..-70. */
s32 McCard_IsFatalError(s32 code) {
    if (code >= -19 && code <= -11) {
        return 1;
    }
    if (code >= -49 && code <= -40) {
        return 1;
    }
    if (code >= -59 && code <= -50) {
        return 1;
    }
    if (code >= -79 && code <= -70) {
        return 1;
    }
    return 0;
}

/* Lists the card's root directory into `table`. 1 when finished. Nothing calls it. */
s32 McCard_ReadDir(s32 port, s32 maxEntries, void *table) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_DIR);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_DIR;
        ret = sceMcGetDir(port, 0, "*", 0, maxEntries, table);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 0) {
            return 0;
        }
        gMcCardStep = 0;
        gMcCardCmd = 0;
        return 1;
    default:
        gMcCardStep = 0;
        gMcCardCmd = 0;
        return 1;
    }
}

/* Looks the system save's directory up (the result is the number of entries, or an error). Step 2 = finished. */
s32 McCard_FindSystemSave(s32 port) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_FIND);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_FIND;
        ret = sceMcGetDir(port, 0, "BASLUS-21678DBZT3", 0, -1, NULL);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 1) {
            gMcCardStep++;
            return 1;
        }
        return 0;
    }
    /* Past step 1 nothing is returned (v0 still holds the 1 of the step comparison). */
}

/* The same for the directory of a save request. */
s32 McCard_FindSave(s32 port, McCardFile *file) {
    s32 ret;

    MCCARD_ENTER(MCCARD_CMD_FIND);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_FIND;
        ret = sceMcGetDir(port, 0, file->dir, 0, -1, NULL);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) == 1) {
            gMcCardStep++;
            return 1;
        }
        return 0;
    }
    /* Past step 1 nothing is returned. */
}

/*
 * Checksum of a save block: the first two words of the block hold it. The sum runs over the bytes after the
 * first 8, even bytes counted once and odd bytes times 256, kept to 16 bits. `write` == 1 stores the high
 * byte in word 0 and the low byte in word 1 and returns 0; otherwise the stored values are compared and 1
 * means "bad".
 */
s32 McCard_Checksum(u32 *buf, s32 size, s32 write) {
    u32 sum = 0;
    u8 *p;
    s32 i;

    if (size <= 0) {
        return 1;
    }
    p = (u8 *)buf + 8;
    size -= 8;
    if (write == 1) {
        buf[0] = 0;
        buf[1] = 0;
    }
    for (i = 0; i < size; i++) {
        if (i & 1) {
            sum += *p << 8;
        } else {
            sum += *p;
        }
        p++;
        sum -= (sum > 0xFFFF) * 0x10000;
    }
    if (write == 1) {
        buf[0] = sum >> 8;
        buf[1] = sum & 0xFF;
    } else {
        if (buf[0] != sum >> 8 && buf[1] != (sum & 0xFF)) {
            return 1;
        }
    }
    return 0;
}

/*
 * Works out how many clusters a save still needs: for each of its three files that exists, the file's size in
 * clusters (1024 bytes) is taken off that file's budget (system save: data 16, icon.sys 1, icon 40; replay:
 * 107, 1, 48) and the remainders are summed in the port's `size`. A file that cannot be opened stops the
 * operation at that step (the caller adds the missing budgets). 1 when finished.
 */
s32 McCard_CalcNeed(s32 port, McCardFile *file, s32 replay) {
    s32 ret;
    s32 needData;
    s32 needSys;
    s32 needIcon;

    if (replay == 0) {
        needData = 0x10;
        needSys = 1;
        needIcon = 0x28;
    } else {
        needData = 0x6B;
        needSys = 1;
        needIcon = 0x30;
    }
    MCCARD_ENTER(MCCARD_CMD_NEED);
    switch (gMcCardStep) {
    case 0:
        gMcCardPort[port].size = 0;
        gMcCardPort[port].result = 0;
        gMcCardCmd = MCCARD_CMD_NEED;
        ret = sceMcOpen(port, 0, file->dataPath, 1);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 1:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) != 1) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 2:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 2));
        gMcCardStep++;
        return 0;
    case 3:
        MCCARD_WAIT_OK();
        gMcCardPort[port].size += needData - (gMcCardPort[port].result + 0x3FF) / 0x400;
        gMcCardStep++;
        return 0;
    case 4:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 0));
        gMcCardStep++;
        return 0;
    case 5:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 6:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 7:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 8:
        ret = sceMcOpen(port, 0, file->sysPath, 1);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 9:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) != 1) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 10:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 2));
        gMcCardStep++;
        return 0;
    case 11:
        MCCARD_WAIT_OK();
        gMcCardPort[port].size += needSys - (gMcCardPort[port].result + 0x3FF) / 0x400;
        gMcCardStep++;
        return 0;
    case 12:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 0));
        gMcCardStep++;
        return 0;
    case 13:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 14:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 15:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 16:
        ret = sceMcOpen(port, 0, file->iconPath[0], 1);
        gMcCardPort[port].ret = ret;
        if (ret < 0) {
            if (ret == -200) {
                sceMcSync(1, NULL, NULL);
                return 0;
            }
            return 0;
        }
        gMcCardStep++;
        return 0;
    case 17:
        if (sceMcSync(1, NULL, &gMcCardPort[port].result) != 1) {
            return 0;
        }
        if (gMcCardPort[port].result < 0) {
            return 0;
        }
        gMcCardPort[port].fd = gMcCardPort[port].result;
        gMcCardStep++;
        return 0;
    case 18:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 2));
        gMcCardStep++;
        return 0;
    case 19:
        MCCARD_WAIT_OK();
        gMcCardPort[port].size += needIcon - (gMcCardPort[port].result + 0x3FF) / 0x400;
        gMcCardStep++;
        return 0;
    case 20:
        MCCARD_CALL(sceMcSeek(gMcCardPort[port].fd, 0, 0));
        gMcCardStep++;
        return 0;
    case 21:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 0;
    case 22:
        MCCARD_CALL(sceMcClose(gMcCardPort[port].fd));
        gMcCardStep++;
        return 0;
    case 23:
        MCCARD_WAIT_OK();
        gMcCardStep++;
        return 1;
    default:
        return 1;
    }
}
