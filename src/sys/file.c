#include "common.h"
#include "sys/file.h"
#include "sys/heap.h"

/* libc */
extern void *memset(void *dst, s32 value, u32 size);

/* Sony libgraph / libcdvd */
extern void *sceGsSyncVCallback(s32 (*handler)(s32));
extern s32 sceCdGetDiskTypeSafe(void);
extern s32 sceCdStatus(void);
extern s32 sceCdSearchFile(void *file, char *name);

/* CRI ADX */
extern void ADXPS2_ExecVint(s32 mode);
extern void ADXPS2_SetupDvdFs(AdxDvdFsParam *param);
extern void ADXPS2_LoadFcacheDvd(AdxFcacheParam *param);
extern void ADXPS2_SetupThrd(void *param, s32 unk);
extern void ADXM_ExecMain(void);
extern void ADXM_WaitVsync(void);
extern void ADXERR_EntryErrFunc(void (*func)(void *obj, char *msg), void *obj);
extern s32 ADXF_LoadPartitionNw(s32 ptid, char *fname, void *dir, void *ptinfo);
extern s32 ADXF_GetPtStat(s32 ptid);
extern ADXF ADXF_Open(char *fname, void *atr);
extern ADXF ADXF_OpenAfs(s32 ptid, s32 flid);
extern void ADXF_Close(ADXF adxf);
extern s32 ADXF_ReadNw(ADXF adxf, s32 nsct, void *buf);
extern s32 ADXF_Stop(ADXF adxf);
extern s32 ADXF_GetFsizeSct(ADXF adxf);
extern s32 ADXF_GetStat(ADXF adxf);
extern void ADXT_Init(void);
extern ADXT_HN ADXT_Create(s32 maxnch, void *work, s32 worksize);
extern void ADXT_SetReloadSct(ADXT_HN adxt, s32 nsct);
extern void ADXT_SetSvrFreq(ADXT_HN adxt, s32 freq);
extern void ADXT_StartAfs(ADXT_HN adxt, s32 patid, s32 fid);

extern u32 gVsyncCount;
extern AfsPartition gAfsPartitionTbl[];
extern AdxPlayer gAdxPlayerTbl[];
extern char *gDiscExeNameTbl[];
extern AdxDvdFsParam gAdxDvdFsParam;
extern AdxFcacheParam gAdxFcacheParam;
#ifdef PORT
static FileReqQueue nativeFileRequests;
#define gFileReq nativeFileRequests
#else
extern FileReqQueue gFileReq;
#endif
extern char gFileRootDir[];
extern char gFileDirName[];

/* Unused stub that returns 0. */
s32 File_Stub264A50(void) {
    return 0;
}

/* VBlank interrupt handler: counts the frame and runs the CRI vsync work. */
s32 Vsync_Handler(s32 cause) {
    gVsyncCount++;
    ADXPS2_ExecVint(0);
#ifndef PORT
    __asm__ volatile("sync.l\nei");
#endif
    return 0;
}

/* Registers Vsync_Handler as the VBlank callback. */
void Vsync_InstallHandler(void) {
    sceGsSyncVCallback(Vsync_Handler);
}

/* CRI error callback: does nothing (the message printing was stripped). */
void File_AdxErrorCallback(void *obj, char *msg) {
}

/* Starts loading the directory of AFS partition `pt` without blocking. */
s32 File_LoadPartitionNw(s32 pt) {
    AfsPartition *part = &gAfsPartitionTbl[pt];

    return ADXF_LoadPartitionNw(pt, part->name, NULL, part->info);
}

/* Returns 1 once the partition directory has been read. */
s32 File_IsPartitionLoaded(s32 pt) {
    return ADXF_GetPtStat(pt) == ADXF_STAT_READEND;
}

/* Blocks until the partition directory has been read. */
s32 File_WaitPartition(s32 pt) {
    while (ADXF_GetPtStat(pt) != ADXF_STAT_READEND) {
        ADXM_ExecMain();
    }
    return 1;
}

/* Like File_WaitPartition, but gives up after 2400 polls. */
void File_WaitPartitionTimeout(s32 pt) {
    s32 i;

    for (i = 0; i < 2400; i++) {
        if (ADXF_GetPtStat(pt) == ADXF_STAT_READEND) {
            break;
        }
        ADXM_ExecMain();
    }
}

/* Brings up the whole file layer: CRI, partitions 0 and 1, the request queue. */
void File_Init(void) {
    File_InitAdx();
    File_LoadPartition1();
}

/* Native extra fighters have separate voice and streamed-effect players. */
#ifdef PORT
static AdxPlayer extraAdxPlayers[ADX_PLAYER_COUNT-6];
#endif
AdxPlayer *File_AdxPlayer(s32 channel) {
#ifdef PORT
    if (channel >= 6) return &extraAdxPlayers[channel - 6];
#endif
    return &gAdxPlayerTbl[channel];
}

/* Sets up the CRI file system and players, loads partition 0 and resets the request queue. */
void File_InitAdx(void) {
    AdxDvdFsParam *dvd = &gAdxDvdFsParam;
    AdxFcacheParam *fc;
    AfsPartition *part;
    AdxPlayer *player;
    s32 i;

    memset(dvd, 0, sizeof(AdxDvdFsParam));
    dvd->rootDir = gFileRootDir;
    ADXPS2_SetupDvdFs(dvd);

    fc = &gAdxFcacheParam;
    memset(fc, 0, sizeof(AdxFcacheParam));
    fc->dirFile = gFileDirName;
    fc->maxFiles = 0x10;
    fc->bufSize = 0x190;
    fc->buf = Heap_Alloc(0x190, 0x20, 0, HEAP_ANY);
    ADXPS2_LoadFcacheDvd(fc);

    ADXPS2_SetupThrd(NULL, 0);
    ADXT_Init();
    Vsync_InstallHandler();
    ADXERR_EntryErrFunc(File_AdxErrorCallback, NULL);

    part = gAfsPartitionTbl;
    for (i = 0; i < FILE_PT_COUNT; i++, part++) {
        part->info = Heap_Alloc(part->infoSize, 0x20, 0, HEAP_ANY);
    }
    for (i = 0; i < ADX_PLAYER_COUNT; i++) {
        player = File_AdxPlayer(i);
#ifdef PORT
        if (i >= 6) *player = gAdxPlayerTbl[i < 4+PORT_FIGHTER_CAPACITY ? 4 : 2];
#endif
        player->work = Heap_Alloc(player->workSize, 0x20, 0, HEAP_ANY);
        player->adxt = ADXT_Create(2, player->work, player->workSize);
        ADXT_SetReloadSct(player->adxt, player->reloadSct);
        ADXT_SetSvrFreq(player->adxt, 30);
    }

    File_LoadPartitionNw(0);
    File_WaitPartition(0);
    File_InitRequests();
}

/* Loads the directory of partition 1 and waits for it. */
void File_LoadPartition1(void) {
    File_LoadPartitionNw(1);
    File_WaitPartition(1);
}

/* Empty stub called once per frame by three frame loops. */
void File_Stub264D90(void) {
}

/* Waits until `count` vblanks have passed since the last call (always at least one), then restarts the count. */
void Vsync_Wait(u32 count) {
    s32 waited = 0;

    while (gVsyncCount < count - 1) {
        ADXM_WaitVsync();
        waited = 1;
    }
    if (gVsyncCount < count || !waited) {
        ADXM_WaitVsync();
    }
    gVsyncCount = 0;
}

/* Identifies the inserted disc by its boot file: 0..2 = Tenkaichi 1/2/3, -1 other PS2 DVD, -2 still detecting, -3 not a PS2 DVD. */
s32 Disc_Identify(void) {
    u8 file[0x24];
    s32 result;
    u32 i;

    switch (sceCdGetDiskTypeSafe()) {
    case 1:
        result = -2;
        break;
    case 0x14:
        result = -1;
        break;
    default:
        result = -3;
        break;
    }
    if (result == -1) {
        for (i = 0; i < 3; i++) {
            if (sceCdSearchFile(file, gDiscExeNameTbl[i]) == 1) {
                result = i;
                break;
            }
        }
    }
    return result;
}

/* Maps the drive status to 0..6 (stop, tray open, spin, read, pause, seek, other) and returns 7 when there is no disc. */
s32 Disc_GetDriveState(void) {
    s32 state;

    switch (sceCdStatus()) {
    case 0:
        state = 0;
        break;
    case 1:
        state = 1;
        break;
    case 2:
        state = 2;
        break;
    case 6:
        state = 3;
        break;
    case 10:
        state = 4;
        break;
    case 18:
        state = 5;
        break;
    case 0x20:
    default:
        state = 6;
        break;
    }
    if (state != 1 && state != 6) {
        if (sceCdGetDiskTypeSafe() == 0) {
            state = 7;
        }
    }
    return state;
}

/* Unused stub that returns 0. */
s32 File_Stub264F70(void) {
    return 0;
}

/* Empties the request queue: every entry goes to the free list. */
void File_InitRequests(void) {
    FileReqQueue *queue = &gFileReq;
    List *free = &queue->free;
    FileReq *req;
    s32 i;

    List_Init(free);
    List_Init(&queue->pending);
    req = queue->entries;
    for (i = 0; i < FILE_REQ_MAX; i++, req++) {
        req->flags = 0;
        List_PushBack(free, &req->node);
    }
    queue->state = FILE_QUEUE_IDLE;
    queue->adxf = NULL;
}

/* Reads a whole file and waits for it; opens by name when id < 0, allocates the buffer when buf is NULL. */
void *File_LoadSyncEx(s32 id, char *name, void *buf, s32 unused) {
    ADXF adxf;
    s32 sectors;
    void *dst;

    do {
        if (id >= 0) {
            adxf = File_OpenById(id);
        } else {
            adxf = ADXF_Open(name, NULL);
        }
    } while (adxf == NULL);

    sectors = ADXF_GetFsizeSct(adxf);
    dst = buf;
    if (buf == NULL) {
        dst = Heap_Alloc(sectors * FILE_SECTOR_SIZE, 0x40, 0, HEAP_ANY);
    }
    while (ADXF_ReadNw(adxf, sectors, dst) != sectors) {
    }
    do {
        ADXM_ExecMain();
    } while (ADXF_GetStat(adxf) != ADXF_STAT_READEND);
    ADXF_Close(adxf);
    buf = dst;
    return buf;
}

/* Returns 1 while the request queue is working on a batch. */
s32 File_IsBusy(void) {
    return gFileReq.state != FILE_QUEUE_IDLE;
}

/* Number of requests not finished yet. */
s32 File_GetPendingCount(void) {
    return List_GetCount(&gFileReq.pending);
}

/* Aborts the batch in progress: stops the read, frees the buffers the queue allocated, drops every pending request. */
void File_CancelRequests(void) {
    FileReqQueue *queue = &gFileReq;
    FileReq *req;

    if (File_IsBusy()) {
        if (queue->adxf != NULL) {
            ADXF_Stop(queue->adxf);
            ADXF_Close(queue->adxf);
        }
        for (req = (FileReq *)List_GetHead(&queue->pending); req != NULL; req = (FileReq *)List_GetNext(&req->node)) {
            if (req->flags & FILE_REQ_OWNS_BUF) {
                Heap_Free(req->buf);
            }
            req->flags = 0;
        }
    }
    queue->state = FILE_QUEUE_IDLE;
    queue->adxf = NULL;
    List_AppendList(&queue->free, &queue->pending);
}

/* Queues an asynchronous read of file `id`; allocates the buffer when buf is NULL. Returns the buffer. */
void *File_Request(s32 id, void *buf) {
    FileReqQueue *queue = &gFileReq;
    s32 flags = 0;
    ADXF adxf;
    s32 sectors;
    void *dst;
    FileReq *req;

    do {
        adxf = File_OpenById(id);
    } while (adxf == NULL);

    sectors = ADXF_GetFsizeSct(adxf);
    dst = buf;
    if (buf == NULL) {
        dst = Heap_Alloc(sectors * FILE_SECTOR_SIZE, 0x40, 0, HEAP_ANY);
        flags = FILE_REQ_OWNS_BUF;
    }
    ADXF_Close(adxf);

    req = (FileReq *)List_PopFront(&queue->free);
#ifdef PORT
    if (req == NULL) {
        extern void Port_TagTeamFatal(const char *);
        Port_TagTeamFatal("file request queue exhausted");
    }
#endif
    req->flags = flags;
    req->id = id;
    req->sectors = sectors;
    req->buf = dst;
    List_PushBack(&queue->pending, &req->node);
    buf = dst;
    return buf;
}

/* Advances the request queue by one step; returns 1 when every request has been read. */
static s32 File_UpdateRequestStep(void) {
    FileReqQueue *queue = &gFileReq;
    s32 done = 0;
    List *pending;
    FileReq *req;
    FileReq *entry;
    s32 i;

    if (File_GetPendingCount() == 0) {
        return 1;
    }
    pending = &queue->pending;
    req = (FileReq *)List_GetHead(pending);
    switch (queue->state) {
    case FILE_QUEUE_IDLE:
        queue->state = FILE_QUEUE_OPEN;
        entry = queue->entries;
        for (i = 0; i < FILE_REQ_MAX; i++, entry++) {
            entry->flags &= ~FILE_REQ_DONE;
        }
        /* fallthrough */
    case FILE_QUEUE_OPEN:
        queue->adxf = File_OpenById(req->id);
        if (queue->adxf != NULL) {
            queue->state = FILE_QUEUE_READ;
        }
        break;
    case FILE_QUEUE_READ:
        if (ADXF_ReadNw(queue->adxf, req->sectors, req->buf) == req->sectors) {
            queue->state = FILE_QUEUE_WAIT;
        }
        break;
    case FILE_QUEUE_WAIT:
        if (ADXF_GetStat(queue->adxf) == ADXF_STAT_READEND) {
            ADXF_Close(queue->adxf);
            queue->adxf = NULL;
            queue->state = FILE_QUEUE_NEXT;
        }
        break;
    case FILE_QUEUE_NEXT:
        req->flags |= FILE_REQ_DONE;
        List_Remove(pending, &req->node);
        List_PushBack(&queue->free, &req->node);
        if (File_GetPendingCount() > 0) {
            queue->state = FILE_QUEUE_OPEN;
        } else {
            queue->state = FILE_QUEUE_IDLE;
            done = 1;
        }
        break;
    }
    return done;
}

/* On PC a completed local read needs no further video frames to acknowledge it.
 * Drain only ready work, with both a time and a step limit so the loading
 * minigame still gets its normal input, sound and drawing updates. A read that
 * is genuinely waiting (or failed to open) yields immediately. */
s32 File_UpdateRequests(void) {
#ifdef PORT
    extern s32 Port_FastFileQueue(void);
    extern u32 Port_LoadTimeUs(void);
    if (Port_FastFileQueue()) {
        u32 started = Port_LoadTimeUs();
        s32 step;
        for (step = 0; step < 32; step++) {
            s32 state = gFileReq.state;
            s32 count = File_GetPendingCount();
            if (File_UpdateRequestStep()) return 1;
            if (state == gFileReq.state && count == File_GetPendingCount()) break;
            if ((u32)(Port_LoadTimeUs() - started) >= 2000) break;
        }
        return 0;
    }
#endif
    return File_UpdateRequestStep();
}

/* Size of file `id` in bytes, rounded up to whole sectors. */
s32 File_GetSize(s32 id) {
    ADXF adxf;
    s32 sectors;

    do {
        adxf = File_OpenById(id);
    } while (adxf == NULL);
    sectors = ADXF_GetFsizeSct(adxf);
    ADXF_Close(adxf);
    return sectors * FILE_SECTOR_SIZE;
}

/* Blocking read of a file given by path instead of id. */
void *File_LoadSyncByName(char *name, void *buf, s32 unused) {
    return File_LoadSyncEx(-1, name, buf, unused);
}

/* Blocking read of file `id`; allocates the buffer when buf is NULL. */
void *File_LoadSync(s32 id, void *buf, s32 unused) {
    return File_LoadSyncEx(id, NULL, buf, unused);
}

/* Returns the request queue. */
FileReqQueue *File_GetRequestQueue(void) {
    return &gFileReq;
}

/* Opens file `id`, translating the global id into (partition, index). */
ADXF File_OpenById(s32 id) {
#ifdef PORT
    return ADXF_OpenAfs(PortGame_Partition(id), PortGame_FileIndex(id));
#else
    return ADXF_OpenAfs(id <= 0 ? 0 : id < FILE_PT2_FIRST_ID ? 1 : 2,
                        id <= 0 ? id : id < FILE_PT2_FIRST_ID ? id - FILE_PT1_FIRST_ID : id - FILE_PT2_FIRST_ID);
#endif
}

/* Starts streaming file `id` on an ADXT player, with the same id translation as File_OpenById. */
void File_StartAdxById(ADXT_HN adxt, s32 id) {
#ifdef PORT
    ADXT_StartAfs(adxt, PortGame_Partition(id), PortGame_FileIndex(id));
#else
    ADXT_StartAfs(adxt, id <= 0 ? 0 : id < FILE_PT2_FIRST_ID ? 1 : 2,
                  id <= 0 ? id : id < FILE_PT2_FIRST_ID ? id - FILE_PT1_FIRST_ID : id - FILE_PT2_FIRST_ID);
#endif
}
