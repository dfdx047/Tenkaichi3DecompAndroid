#include "common.h"
#include "sys/adx.h"
#include "sys/dma.h"
#include "sys/fade.h"
#include "sys/file.h"
#include "sys/gfx.h"
#include "sys/heap.h"
#include "sys/movie.h"
#include "sys/pad.h"

extern void *memset(void *dst, s32 c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);

/* libmpeg */
extern s32 sceMpegInit(void);
extern s32 sceMpegCreate(sceMpeg *mp, u8 *work, s32 size);
extern s32 sceMpegDelete(sceMpeg *mp);
extern s32 sceMpegGetPicture(sceMpeg *mp, u8 *rgb, s32 mbCount);
extern s32 sceMpegReset(sceMpeg *mp);
extern s32 sceMpegIsEnd(sceMpeg *mp);
extern void *sceMpegAddCallback(sceMpeg *mp, s32 type, sceMpegCallback cb, void *data);
extern void *sceMpegAddStrCallback(sceMpeg *mp, s32 strType, s32 ch, sceMpegCallback cb, void *data);
extern s32 sceMpegDemuxPss(sceMpeg *mp, u8 *data, s32 len);

/* CRI */
extern ADXF ADXF_Open(char *fname, void *atr);
extern void ADXF_Close(ADXF adxf);
extern s32 ADXF_ReadNw(ADXF adxf, s32 nsct, void *buf);
extern s32 ADXF_GetStat(ADXF adxf);
extern s32 ADXF_Tell(ADXF adxf);    /* file position in sectors (locked wrapper around adxf_Tell) */
extern s32 ADXT_GetStat(ADXT_HN adxt);
extern void ADXT_Pause(ADXT_HN adxt, s32 sw);
extern s32 func_00277A70(ADXT_HN adxt); /* non-zero while the player's own stream must not be disturbed (inferred) */
extern void ADXM_ExecMain(void);

/* hardware timer helpers (src/sys/timer.c, declared locally): timer 0 counts horizontal blanks */
extern void Timer_Init(s32 ch);      /* sets the timer up */
extern void Timer_Start(s32 ch);     /* starts it and clears the count */
extern void Timer_Reset(s32 ch);     /* clears the count */
extern f32 Timer_GetFrames(s32 ch);  /* time since the count was cleared, in 1/60 s */

extern void Pad_Update(void);

extern AdxPlayer gAdxPlayerTbl[6];
extern s32 gMovieRunCount;
extern u8 *gMovieEsBuf;
extern s32 gMovieEsSize;
extern u8 *gMovieEsWrite;
extern u8 *gMovieEsDmaStart;
extern u8 *gMovieEsDmaEnd;
/* "idle", "nodata" and "full": string literals in the original (the compiler puts strings of 8 bytes or less in
   .sdata, 0x2FE980..0x2FE998, and still addresses them with lui/addiu). Kept as externs until the data is linked. */
extern char gMovieTagIdle[];
extern char gMovieTagNoData[];
extern char gMovieTagFull[];

#ifdef PORT
extern volatile u32 *Port_DmaChcr(s32 channel);
#define D4_CHCR Port_DmaChcr(4)
#else
#define D4_CHCR ((volatile u32 *)0x1000B400)
#endif
#define D4_MADR ((volatile u32 *)0x1000B410)
#define D4_QWC ((volatile u32 *)0x1000B420)

#ifdef PORT
#define UNCACHED(p) ((void *)(p)) /* no uncached mirror of memory on PC */
#else
#define UNCACHED(p) ((void *)(((u32)(p) & 0x0FFFFFFF) | 0x20000000))
#endif

/* Builds the chain that uploads a w x h RGBA32 picture stored as 16x16 macroblocks in column order. */
void MovieTag_BuildImage(u32 *tags, u8 *image, s32 x, s32 y, s32 w, s32 h) {
    MovieTag_BuildBlocks(tags, image, MOVIE_MB_SIZE, x, y, w, h);
}

/* Builds the same chain with every block reading the same 16x16 pixels. */
void MovieTag_BuildFill(u32 *tags, u8 *image, s32 x, s32 y, s32 w, s32 h) {
    MovieTag_BuildBlocks(tags, image, 0, x, y, w, h);
}

/* Builds a GIF DMA chain that copies 16x16 blocks, columns first, from `image` to frame buffer page 0 at (x, y). */
void MovieTag_BuildBlocks(u32 *tags, u8 *image, s32 step, s32 x, s32 y, s32 w, s32 h) {
    GsQword *p = UNCACHED(tags);
    s32 cols = w >> 4;
    s32 rows = h >> 4;
    s32 i;
    s32 j;
    s32 eop;

    MovieTag_SetDmaTag(&p->d[0], 0, 0, 0, 1, 0, 3);
    p++;
    MovieTag_SetGifTag(p->w, GIF_REG_AD, 1, 0, 0, 0, 0, 2);
    p++;
    MovieTag_SetBitBltBuf(p->w, 0, 8, 0);
    p++;
    MovieTag_SetTrxReg(p->w, 16, 16);
    p++;
    for (i = 0; i < cols; i++) {
        for (j = 0; j < rows; j++) {
            eop = (j == rows - 1) && (i == cols - 1);
            MovieTag_SetDmaTag(&p->d[0], 0, 0, 0, 1, 0, 4);
            p++;
            MovieTag_SetGifTag(p->w, GIF_REG_AD, 1, 0, 0, 0, 0, 2);
            p++;
            MovieTag_SetTrxPos(p->w, 0, x + i * 16, y + j * 16);
            p++;
            MovieTag_SetTrxDir(p->w, 0);
            p++;
            MovieTag_SetGifTag(p->w, 0, 0, 2, 0, 0, eop, 0x40);
            p++;
#ifdef PORT
            MovieTag_SetDmaTag(&p->d[0], 0, (u32)image, 0, 3, 0, 0x40); /* the pointer itself, see DMA_PHYS */
#else
            MovieTag_SetDmaTag(&p->d[0], 0, (u32)image & 0x0FFFFFFF, 0, 3, 0, 0x40);
#endif
            p++;
            image += step;
        }
    }
    MovieTag_SetDmaTag(&p->d[0], 0, 0, 0, 7, 0, 0);
    p++;
}

/* Writes one DMA source-chain tag. */
void MovieTag_SetDmaTag(u64 *p, s32 spr, s32 addr, s32 irq, s32 id, s32 pce, s32 qwc) {
    *p = ((u64)spr << 63) | ((u64)(addr & ~0xF) << 32) | ((u64)(u32)irq << 31) | ((u64)(u32)id << 28) |
         ((u64)(u32)pce << 26) | (u64)(u32)qwc;
}

/* Writes one GIF tag. */
void MovieTag_SetGifTag(u32 *p, u64 regs, s32 nreg, s32 flg, s32 prim, s32 pre, s32 eop, s32 nloop) {
    p[0] = (eop << 15) | nloop;
    p[1] = (pre << 14) | (prim << 15) | (flg << 26) | (nreg << 28);
    p[2] = regs & 0xFFFFFFFF;
    p[3] = regs >> 32;
}

/* Writes one A+D quadword: a GS register value and the register number. */
void MovieTag_SetReg(u32 *p, s32 reg, u64 value) {
    p[2] = reg;
    p[1] = value >> 32;
    p[0] = value & 0xFFFFFFFF;
    p[3] = 0;
}

/* Writes BITBLTBUF: destination buffer pointer, width and pixel format. */
void MovieTag_SetBitBltBuf(u32 *p, s32 dbp, s32 dbw, s32 dpsm) {
    MovieTag_SetReg(p, 0x50, ((u64)dpsm << 56) | ((u64)dbw << 48) | ((u64)dbp << 32));
}

/* Writes TRXPOS: destination position. */
void MovieTag_SetTrxPos(u32 *p, s32 dir, s32 x, s32 y) {
    MovieTag_SetReg(p, 0x51, ((u64)dir << 59) | ((u64)y << 48) | ((u64)x << 32));
}

/* Writes TRXREG: size of the transferred rectangle. */
void MovieTag_SetTrxReg(u32 *p, s32 w, s32 h) {
    MovieTag_SetReg(p, 0x52, ((u64)h << 32) | (u64)(u32)w);
}

/* Writes TRXDIR: 0 starts a host-to-local transfer. */
void MovieTag_SetTrxDir(u32 *p, s32 dir) {
    MovieTag_SetReg(p, 0x53, (u64)(u32)dir);
}

/* Starts an upload chain on the GIF DMA channel without waiting for it. */
void MovieTag_Send(u32 *tags) {
#ifdef PORT
    *D2_TADR = (u32)tags;
#else
    *D2_TADR = (u32)tags & 0x0FFFFFFF;
#endif
    *D2_QWC = 0;
    *D2_CHCR = 0x105;
}

/* Initialises libmpeg; called once at boot. */
void Movie_Init(void) {
    sceMpegInit();
}

/* Closes the movie if one is open. */
void Movie_Term(void) {
    Movie_Close();
}

/* Opens a movie: the .PSS video file and the .ADX sound file (started paused), all buffers and the decoder. */
void Movie_Open(char *pss, char *adx) {
    ADXF adxf;
    Movie *m;
    MovieBuf *buf;
    s32 i;

#ifdef PORT
    adxf = ADXF_Open(pss, NULL);
    /* An incomplete import must not trap the native application in a retry
       loop. Leave the movie closed and continue to the title/menu. */
    if (adxf == NULL) return;
#else
    do {
        adxf = ADXF_Open(pss, NULL);
    } while (adxf == NULL);
#endif
    Adx_PlayFilePaused(ADX_CH_BGM, adx, 0x40, 0);
    gMovie = Heap_Alloc(sizeof(Movie), 0x20, 0, 2);
    memset(gMovie, 0, sizeof(Movie));
    m = gMovie;
    m->adxf = adxf;
    m->adxt = gAdxPlayerTbl[ADX_CH_BGM].adxt;
    m->rgbSize = MOVIE_RGB_SIZE;
    m->tagSize = MOVIE_TAG_SIZE;
    m->workSize = MOVIE_WORK_SIZE;
    m->esSize = MOVIE_ES_SIZE;
    m->rgb = Heap_Alloc(MOVIE_RGB_SIZE, 0x40, 0, 2);
    m->tags[0] = Heap_Alloc(m->tagSize, 0x40, 0, 2);
    m->tags[1] = Heap_Alloc(m->tagSize, 0x40, 0, 2);
    m->work = Heap_Alloc(m->workSize, 0x40, 0, 2);
    m->es = Heap_Alloc(m->esSize, 0x40, 0, 2);
    for (i = 0, buf = m->buf; i < MOVIE_BUF_COUNT; i++, buf++) {
        buf->data = Heap_Alloc(MOVIE_BUF_SIZE, 0x40, 0, 2);
    }
    sceMpegCreate(&m->mpeg, m->work, m->workSize);
    sceMpegAddStrCallback(&m->mpeg, SCE_MPEG_STR_M2V, 0, (sceMpegCallback)Movie_CbVideoData, NULL);
    sceMpegAddCallback(&m->mpeg, SCE_MPEG_CB_NODATA, Movie_CbNoData, NULL);
    sceMpegAddCallback(&m->mpeg, SCE_MPEG_CB_BACKGROUND, Movie_CbBackground, NULL);
    sceMpegAddCallback(&m->mpeg, SCE_MPEG_CB_ERROR, Movie_CbError, NULL);
    MovieTag_BuildImage(m->tags[0], m->rgb, 0, 0, MOVIE_WIDTH, MOVIE_HEIGHT);
    MovieTag_BuildImage(m->tags[1], m->rgb, 0, MOVIE_HEIGHT, MOVIE_WIDTH, MOVIE_HEIGHT);
}

/* Stops the sound, deletes the decoder, closes the file and frees everything Movie_Open allocated. */
void Movie_Close(void) {
    Movie *m = gMovie;
    MovieBuf *buf;
    s32 i;

    if (m != NULL) {
        Adx_Stop(ADX_CH_BGM);
        sceMpegReset(&m->mpeg);
        sceMpegDelete(&m->mpeg);
        ADXF_Close(m->adxf);
        Heap_Free(m->rgb);
        Heap_Free(m->tags[0]);
        Heap_Free(m->tags[1]);
        Heap_Free(m->work);
        Heap_Free(m->es);
        for (i = 0, buf = m->buf; i < MOVIE_BUF_COUNT; i++, buf++) {
            Heap_Free(buf->data);
        }
        Heap_Free(m);
        gMovie = NULL;
    }
}

/* Plays the opening movie from start to end (or until it is skipped). */
void Movie_PlayOpening(void) {
#ifdef PORT
    extern s32 PortGame_IsBT4(void);
    if (PortGame_IsBT4()) Movie_Open("zs4usop.pss", "zs4usop.adx");
    else
#endif
    Movie_Open("zs3usop.pss", "zs3usop.adx");
    if (!gMovie) return;
    Movie_Run();
    Movie_Close();
}

/* Plays the ending movie. */
void Movie_PlayEnding(void) {
#ifdef PORT
    extern s32 PortGame_IsBT4(void);
    if (PortGame_IsBT4()) Movie_Open("zs4used.pss", "zs4used.adx");
    else
#endif
    Movie_Open("zs3used.pss", "zs3used.adx");
    if (!gMovie) return;
    Movie_Run();
    Movie_Close();
}

/* The player's frame loop: decodes and shows one picture per frame until the stream ends or START skips it. */
void Movie_Run(void) {
    Movie *m;
    sceMpeg *mp;
    s32 wait = 0;
    s32 skip = 0;

    gMovieRunCount++;
    m = gMovie;
    mp = &m->mpeg;
    gMovieEsBuf = m->es;
    gMovieEsSize = m->esSize;
    gMovieEsWrite = gMovieEsBuf + 0x10; /* the first 16 bytes stay unused */
    gMovieEsDmaEnd = gMovieEsWrite;
    gMovieEsDmaStart = gMovieEsBuf;
    Timer_Init(0);
    Movie_FillBufs();
    Movie_Demux();
    Movie_FillBufs();
    sceGsSyncV(0);
    Timer_Start(0);
    m->audioOn = 1;
    Adx_Resume(ADX_CH_BGM);
    while (!sceMpegIsEnd(mp)) {
        Timer_Reset(0);
        Gfx_BeginFrame();
        Pad_Update();
        if (skip) {
            if (Fade_IsDone(0)) {
                if (wait >= 3) {
                    break;
                }
                wait++;
            }
        } else if (gPad[0].gamePressed & MOVIE_SKIP_BUTTON) {
            Fade_Start(0, FADE_OUT, 0.5f);
            skip = 1;
        }
        sceMpegGetPicture(mp, m->rgb, MOVIE_MB_COUNT);
        sceGsSyncPath(0, 0);
        MovieTag_Send(gGfx.field ? m->tags[0] : m->tags[1]);
        sceGsSyncPath(0, 0);
        Dma_Flush();
        do {
            if (!(Timer_GetFrames(0) < MOVIE_FRAME_FIELDS)) {
                break;
            }
            if (ADXF_GetStat(m->adxf) != ADXF_STAT_READING && func_00277A70(m->adxt) == 0) {
                Movie_ReadAhead(gMovieTagIdle);
            }
        } while (Movie_FindBuf(0) != NULL);
        Gfx_EndFrame(2);
    }
    Adx_Stop(ADX_CH_BGM);
    Fade_Start(0, FADE_OUT, 0.0f);
    Gfx_BeginFrame();
    Dma_Flush();
    Gfx_EndFrame(1);
    Gfx_BeginFrame();
    Dma_Flush();
    Gfx_EndFrame(1);
    Fade_Reset(0);
}

/* libmpeg stream callback: appends demuxed video data to the ring buffer; returns 0 when it does not fit. */
s32 Movie_CbVideoData(sceMpeg *mp, sceMpegCbDataStr *str, void *data) {
    s32 space;
    s32 over;
    u32 len;

    space = gMovieEsDmaStart - gMovieEsWrite;
    if (space < 0) {
        space += gMovieEsSize;
    }
    len = str->len;
    if (space == 0 && gMovieEsDmaStart == gMovieEsDmaEnd) {
        space = gMovieEsSize;
    }
    if ((u32)space < len) {
        return 0;
    }
    over = (gMovieEsWrite - gMovieEsBuf) + len - gMovieEsSize;
    if (over > 0) {
        memcpy(UNCACHED(gMovieEsWrite), str->data, len - over);
        memcpy(UNCACHED(gMovieEsBuf), str->data + (str->len - over), over);
        gMovieEsWrite = gMovieEsBuf + over;
    } else {
        memcpy(UNCACHED(gMovieEsWrite), str->data, len);
        gMovieEsWrite = gMovieEsWrite + str->len;
    }
    return 1;
}

/* libmpeg "no data" callback: makes sure up to 0x1000 bytes are queued and sends them to the IPU by DMA. */
s32 Movie_CbNoData(sceMpeg *mp, void *cbData, void *data) {
    s32 avail;
    s32 size;
    u8 *start;
    u8 *end;
    u8 *limit;

    gMovieEsDmaStart = gMovieEsDmaEnd;
    avail = gMovieEsWrite - gMovieEsDmaEnd;
    if (avail < 0) {
        avail += gMovieEsSize;
    }
    while (avail < MOVIE_ES_CHUNK) {
        if (Movie_FindBuf(1) == NULL) {
            Movie_ReadAhead(gMovieTagNoData);
        }
        if (Movie_Demux() == 0) {
            avail += 0xF;
            break;
        }
        avail = gMovieEsWrite - gMovieEsDmaEnd;
        if (avail < 0) {
            avail += gMovieEsSize;
        }
    }
    size = avail & ~0xF;
    if (size > MOVIE_ES_CHUNK) {
        size = MOVIE_ES_CHUNK;
    }
    start = gMovieEsDmaStart;
    end = start + size;
    limit = gMovieEsBuf + gMovieEsSize;
    if (limit < end) {
        size = limit - start;
        end = start + size;
    }
    *D4_QWC = size / 16;
    *D4_MADR = (u32)start;
    *D4_CHCR = 0x101;
    gMovieEsDmaEnd = end;
    if (!(gMovieEsDmaEnd < limit)) {
        gMovieEsDmaEnd -= gMovieEsSize;
    }
    return 1;
}

/* libmpeg "background" callback, called while the decoder is busy: demuxes and reads ahead. */
s32 Movie_CbBackground(sceMpeg *mp, void *cbData, void *data) {
    Movie *m = gMovie;

    Movie_Demux();
    if (ADXF_GetStat(m->adxf) != ADXF_STAT_READING && func_00277A70(m->adxt) == 0) {
        Movie_ReadAhead("background");
    }
    return 1;
}

/* libmpeg error callback: carries on. */
s32 Movie_CbError(sceMpeg *mp, void *cbData, void *data) {
    return 1;
}

/* Returns the oldest filled read buffer (once its data has arrived) or the first empty one; NULL if none. */
MovieBuf *Movie_FindBuf(s32 full) {
    Movie *m = gMovie;
    MovieBuf *best = NULL;
    MovieBuf *buf = m->buf;
    s32 i;
    s32 end;

    if (full) {
        for (i = 0; i < MOVIE_BUF_COUNT; i++, buf++) {
            if (buf->used) {
                if (best == NULL || buf->seq < best->seq) {
                    best = buf;
                }
            }
        }
        if (best != NULL) {
            end = best->startSct + best->sectors;
            if (ADXF_Tell(m->adxf) < end) {
                Movie_WaitRead(end);
            }
        }
    } else {
        for (i = 0; i < MOVIE_BUF_COUNT; i++, buf++) {
            if (!buf->used) {
                best = buf;
                break;
            }
        }
    }
    return best;
}

/* Starts reading the next 0x100 sectors of the file into a buffer; returns the bytes requested. */
/* The six header stores sit in a `do { } while (0)` (presumably a statement macro in the original). Written as
   plain statements, every order of them compiles to the same wrong store order with `n << 11` computed late: the
   order comes from the scheduler, and the loop notes of the wrapper change it (found by decomp-permuter). */
s32 Movie_ReadBuf(MovieBuf *buf, char *tag) {
    Movie *m = gMovie;
    s32 pos;
    s32 n;

    if (buf == NULL) {
        return 0;
    }
    Movie_WaitRead(-1);
    pos = ADXF_Tell(m->adxf);
    n = ADXF_ReadNw(m->adxf, MOVIE_BUF_SECTORS, buf->data);
    if (n == 0) {
        return n;
    }
    ADXM_ExecMain();
    do {
        buf->used = 1;
        buf->startSct = pos;
        buf->pos = 0;
        buf->sectors = n;
        buf->bytes = n << 11;
        buf->seq = m->seq;
    } while (0);
    m->seq++;
    return n << 11;
}

/* Waits for the disc read (to a sector, or to its end with -1), pausing the sound if the wait drags on. */
void Movie_WaitRead(s32 sector) {
    Movie *m = gMovie;

    do {
        if (m->audioOn) {
            switch (ADXT_GetStat(m->adxt)) {
            case 3:
            case 4:
                if (Timer_GetFrames(0) > MOVIE_STALL_FIELDS) {
                    ADXT_Pause(m->adxt, 1);
                }
                break;
            }
        }
    } while (sector < 0 ? ADXF_GetStat(m->adxf) == ADXF_STAT_READING : ADXF_Tell(m->adxf) < sector);
    if (m->audioOn) {
        ADXT_Pause(m->adxt, 0);
    }
}

/* Starts a read into the first empty buffer, if there is one. */
s32 Movie_ReadAhead(char *tag) {
    MovieBuf *buf = Movie_FindBuf(0);
    s32 ret = 0;

    if (buf != NULL) {
        ret = Movie_ReadBuf(buf, tag);
    }
    return ret;
}

/* Starts reads until no buffer is empty; returns the bytes requested. */
s32 Movie_FillBufs(void) {
    s32 total = 0;
    MovieBuf *buf;

    while ((buf = Movie_FindBuf(0)) != NULL) {
        total += Movie_ReadBuf(buf, gMovieTagFull);
    }
    return total;
}

/* Feeds the filled buffers to the demuxer in read order until it stops taking data; returns the bytes taken. */
s32 Movie_Demux(void) {
    MovieBuf *buf;
    s32 len;
    s32 n;
    s32 total;
    sceMpeg *mp;

    mp = &gMovie->mpeg;
    total = 0;
    do {
        buf = Movie_FindBuf(1);
        if (buf == NULL) {
            break;
        }
        len = buf->bytes - buf->pos;
        n = sceMpegDemuxPss(mp, buf->data + buf->pos, len);
        total += n;
        if (n >= len) {
            buf->used = 0;
            continue;
        }
        buf->pos += n;
        break;
    } while (1);
    return total;
}
