#ifndef SYS_FILE_H
#define SYS_FILE_H

#include "types.h"
#include "sys/list.h"

#define FILE_SECTOR_SIZE 0x800
#ifdef PORT
#define FILE_REQ_MAX 128
#else
#define FILE_REQ_MAX 32
#endif

/* File ids: one global number space spread over three AFS partitions. */
#define FILE_PT1_FIRST_ID 1     /* ids <= 0 are entries of partition 0 */
#define FILE_PT2_FIRST_ID 0xD48 /* ids 1..0xD47 are partition 1, the rest partition 2 */
#define FILE_PT_COUNT 3

#ifdef PORT
#define ADX_PLAYER_COUNT (2 + 2 * PORT_FIGHTER_CAPACITY)
#else
#define ADX_PLAYER_COUNT 6
#endif

/* CRI ADXF status values (file handle and partition load share them). */
#define ADXF_STAT_STOP 1
#define ADXF_STAT_READING 2
#define ADXF_STAT_READEND 3
#define ADXF_STAT_ERROR 4

typedef void *ADXF; /* CRI ADXF file handle */
typedef void *ADXT_HN; /* CRI ADXT player handle (see cri/adxt.h for the struct) */

/* One AFS partition (gAfsPartitionTbl, 3 entries). */
typedef struct AfsPartition {
    /* 0x00 */ char *name;   /* "pzs3usN.afs", relative to the root directory "data/" */
    /* 0x04 */ s32 infoSize; /* size of the ADXF partition info work area */
    /* 0x08 */ void *info;   /* allocated by File_InitAdx */
} AfsPartition;

/* One streamed-audio player (gAdxPlayerTbl, 6 entries). */
typedef struct AdxPlayer {
    /* 0x00 */ char *label;   /* "BGM", "MAP", "SE ", "VIC" */
    /* 0x04 */ s32 workSize;  /* ADXT work area size */
    /* 0x08 */ s32 reloadSct; /* passed to ADXT_SetReloadSct */
    /* 0x0C */ void *work;
    /* 0x10 */ ADXT_HN adxt;
} AdxPlayer;

AdxPlayer *File_AdxPlayer(s32 channel);

/* Argument of ADXPS2_SetupDvdFs (0x14 bytes are cleared; only the first word is set). */
typedef struct AdxDvdFsParam {
    /* 0x00 */ char *rootDir;
    /* 0x04 */ s32 unk4[4];
} AdxDvdFsParam;

/* Argument of ADXPS2_LoadFcacheDvd: directory listing used as a file name cache. */
typedef struct AdxFcacheParam {
    /* 0x00 */ char *dirFile; /* "pzs3us.dir" */
    /* 0x04 */ s32 maxFiles;
    /* 0x08 */ void *buf;
    /* 0x0C */ s32 bufSize;
} AdxFcacheParam;

#define FILE_REQ_OWNS_BUF 0x01 /* buffer was allocated by File_Request; File_CancelRequests frees it */
#define FILE_REQ_DONE 0x10     /* read finished (cleared on every entry when a new batch starts) */

/* One queued asynchronous read. */
typedef struct FileReq {
    /* 0x00 */ ListNode node;
    /* 0x08 */ s32 flags;
    /* 0x0C */ s32 id;
    /* 0x10 */ s32 sectors; /* file size in 0x800-byte sectors */
    /* 0x14 */ void *buf;
} FileReq;

enum {
    FILE_QUEUE_IDLE,  /* nothing in progress; the next update starts a batch */
    FILE_QUEUE_OPEN,  /* open the head of the pending list */
    FILE_QUEUE_READ,  /* start the non-blocking read */
    FILE_QUEUE_WAIT,  /* wait for ADXF_STAT_READEND, then close */
    FILE_QUEUE_NEXT   /* move the entry back to the free list, pick the next one */
};

/* Asynchronous read queue (gFileReq, 0x320 bytes). */
typedef struct FileReqQueue {
    /* 0x00 */ s32 state;
    /* 0x04 */ ADXF adxf;    /* handle of the read in progress, NULL otherwise */
    /* 0x08 */ List free;
    /* 0x14 */ List pending;
    /* 0x20 */ FileReq entries[FILE_REQ_MAX];
} FileReqQueue;

s32 Vsync_Handler(s32 cause);
void Vsync_InstallHandler(void);
void Vsync_Wait(u32 count);

void File_Init(void);
void File_InitAdx(void);
void File_LoadPartition1(void);
s32 File_LoadPartitionNw(s32 pt);
s32 File_IsPartitionLoaded(s32 pt);
s32 File_WaitPartition(s32 pt);
void File_WaitPartitionTimeout(s32 pt);

s32 Disc_Identify(void);
s32 Disc_GetDriveState(void);

void File_InitRequests(void);
s32 File_IsBusy(void);
s32 File_GetPendingCount(void);
void File_CancelRequests(void);
void *File_Request(s32 id, void *buf);
s32 File_UpdateRequests(void);
FileReqQueue *File_GetRequestQueue(void);

void *File_LoadSyncEx(s32 id, char *name, void *buf, s32 unused);
void *File_LoadSyncByName(char *name, void *buf, s32 unused);
void *File_LoadSync(s32 id, void *buf, s32 unused);
s32 File_GetSize(s32 id);
ADXF File_OpenById(s32 id);
void File_StartAdxById(ADXT_HN adxt, s32 id);

#endif
