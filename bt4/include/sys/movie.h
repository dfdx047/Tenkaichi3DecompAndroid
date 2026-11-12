#ifndef SYS_MOVIE_H
#define SYS_MOVIE_H

#include "types.h"
#include "sys/file.h"

/*
 * Movie player. Source range 0x125568-0x126608 (src/sys/movie.c).
 *
 * A movie is two files in the disc's DATA directory, opened by name (not through the AFS archives):
 *   zs3usop.pss / zs3usop.adx   opening   (Movie_PlayOpening)
 *   zs3used.pss / zs3used.adx   ending    (Movie_PlayEnding)
 * On the PC port, the opening can use the European zs3euop.adx soundtrack while keeping the USA video.
 * The .PSS is an MPEG-2 program stream of which only the video stream is used; the sound is the separate
 * .ADX, played by ADX player 0 (the music player) at volume 0x40, pan 0.
 *
 * Data path of the video:
 *   disc --ADXF_ReadNw, 0x100 sectors a time--> 4 read buffers of 0x80000 bytes (MovieBuf, used in sequence order)
 *        --sceMpegDemuxPss--> Movie_CbVideoData --memcpy--> 0x30000-byte ring buffer (gMovieEs*)
 *        --DMA channel 4, at most 0x1000 bytes a time (Movie_CbNoData)--> IPU
 *        --sceMpegGetPicture(rgb, 0x380 macroblocks)--> 512x448 RGBA32 picture, 16x16 macroblocks in column order
 *        --DMA channel 2 (GIF), a prebuilt chain of 896 host-to-local transfers--> the frame buffer being drawn
 *
 * Timing: one picture per frame, each frame ends with Gfx_EndFrame(2) (two vertical blanks, so 29.97 pictures a
 * second on NTSC). There is no time-stamp comparison between picture and sound: the sound is started when the
 * first frame starts and the two then run freely, except that Movie_WaitRead pauses the ADX player while the
 * player is stuck waiting for the disc for more than 4 fields of a frame, and resumes it when the wait ends.
 *
 * The player ends when sceMpegIsEnd() is true, or when START (PADG_START in gPad[0].gamePressed) was pressed:
 * that starts a 0.5 second fade to black on fade slot 0 and the loop ends 4 frames after the fade is done.
 * Pad 1 is not read. On the way out: Adx_Stop(0), an instant fade to black, two empty frames, Fade_Reset(0).
 *
 * 0x125568-0x1259F0 (MovieTag_*) builds GS packets and may have been a separate source file (it has the shape
 * of the tag helpers of Sony's mpeg samples); nothing in the object code decides it, so it is kept here.
 */

#define MOVIE_SKIP_BUTTON 0x1000 /* PADG_START */

#define MOVIE_WIDTH 0x200
#define MOVIE_HEIGHT 0x1C0
#define MOVIE_MB_SIZE 0x400 /* bytes of one 16x16 RGBA32 macroblock */
#define MOVIE_MB_COUNT ((MOVIE_WIDTH / 16) * (MOVIE_HEIGHT / 16)) /* 0x380 */

#define MOVIE_BUF_COUNT 4
#define MOVIE_BUF_SECTORS 0x100
#define MOVIE_BUF_SIZE (MOVIE_BUF_SECTORS << 11) /* 0x80000 */
#define MOVIE_RGB_SIZE (MOVIE_WIDTH * MOVIE_HEIGHT * 4) /* 0xE0000 */
#define MOVIE_TAG_SIZE 0x15100  /* one upload chain: 5 + 6 * 0x380 quadwords = 0x15050 bytes used */
#define MOVIE_WORK_SIZE 0xFE800 /* libmpeg work area */
#define MOVIE_ES_SIZE 0x30000   /* video elementary stream ring buffer */
#define MOVIE_ES_CHUNK 0x1000   /* most bytes handed to the IPU in one DMA transfer */

#define MOVIE_FRAME_FIELDS 1.8f /* disc reads are started only this early in a frame (unit: 1/60 s) */
#define MOVIE_STALL_FIELDS 4.0f /* a disc wait longer than this pauses the sound */

/* Sony libmpeg decoder object (`sceMpeg`). */
typedef struct sceMpeg {
    /* 0x00 */ s32 width;
    /* 0x04 */ s32 height;
    /* 0x08 */ s32 frameCount;
    /* 0x10 */ s64 pts;
    /* 0x18 */ s64 dts;
    /* 0x20 */ u64 flags;
    /* 0x28 */ s64 pts2nd;
    /* 0x30 */ s64 dts2nd;
    /* 0x38 */ u64 flags2nd;
    /* 0x40 */ void *sys;
} sceMpeg; /* size 0x48 */

/* Argument of a stream callback (`sceMpegCbDataStr`). */
typedef struct sceMpegCbDataStr {
    /* 0x00 */ s32 type;
    /* 0x04 */ u8 *header;
    /* 0x08 */ u8 *data;
    /* 0x0C */ u32 len;
    /* 0x10 */ s64 pts;
    /* 0x18 */ s64 dts;
} sceMpegCbDataStr;

typedef s32 (*sceMpegCallback)(sceMpeg *mp, void *cbData, void *anyData);

/* sceMpegAddCallback types used here. */
#define SCE_MPEG_CB_ERROR 0
#define SCE_MPEG_CB_NODATA 1
#define SCE_MPEG_CB_BACKGROUND 4
/* sceMpegAddStrCallback stream type. */
#define SCE_MPEG_STR_M2V 0

/* One disc read buffer (0x1C bytes). */
typedef struct MovieBuf {
    /* 0x00 */ u8 *data;     /* MOVIE_BUF_SIZE bytes */
    /* 0x04 */ s32 used;     /* 1 from the read request until the demuxer has taken every byte */
    /* 0x08 */ s32 startSct; /* file position (sectors) when the read was requested */
    /* 0x0C */ s32 sectors;  /* sectors requested (what ADXF_ReadNw accepted) */
    /* 0x10 */ s32 bytes;    /* sectors << 11 */
    /* 0x14 */ s32 pos;      /* bytes already demuxed */
    /* 0x18 */ s32 seq;      /* read order */
} MovieBuf;

/* Player state (gMovie, 0xF8 bytes, Heap_Alloc'ed by Movie_Open). */
typedef struct Movie {
    /* 0x00 */ ADXF adxf;      /* the .PSS file */
    /* 0x04 */ ADXT_HN adxt;   /* ADX player 0, playing the .ADX */
    /* 0x08 */ s32 audioOn;    /* set when the sound has been released; Movie_WaitRead only pauses it after that */
    /* 0x0C */ s32 unkC;       /* never written after the memset */
    /* 0x10 */ sceMpeg mpeg;
    /* 0x58 */ MovieBuf buf[MOVIE_BUF_COUNT];
    /* 0xC8 */ s32 unkC8;      /* never written after the memset */
    /* 0xCC */ s32 seq;        /* next MovieBuf.seq */
    /* 0xD0 */ u8 *rgb;        /* decoded picture, MOVIE_RGB_SIZE */
    /* 0xD4 */ s32 rgbSize;
    /* 0xD8 */ u32 *tags[2];   /* upload chains: [0] to frame buffer rows 0..447, [1] to rows 448..895 */
    /* 0xE0 */ s32 tagSize;
    /* 0xE4 */ u8 *work;       /* libmpeg work area */
    /* 0xE8 */ s32 workSize;
    /* 0xEC */ u8 *es;         /* ring buffer */
    /* 0xF0 */ s32 esSize;
    /* 0xF4 */ s32 unkF4;
} Movie; /* size 0xF8 */

extern Movie *gMovie;

void MovieTag_BuildImage(u32 *tags, u8 *image, s32 x, s32 y, s32 w, s32 h);
void MovieTag_BuildFill(u32 *tags, u8 *image, s32 x, s32 y, s32 w, s32 h);
void MovieTag_BuildBlocks(u32 *tags, u8 *image, s32 step, s32 x, s32 y, s32 w, s32 h);
void MovieTag_SetDmaTag(u64 *p, s32 spr, s32 addr, s32 irq, s32 id, s32 pce, s32 qwc);
void MovieTag_SetGifTag(u32 *p, u64 regs, s32 nreg, s32 flg, s32 prim, s32 pre, s32 eop, s32 nloop);
void MovieTag_SetReg(u32 *p, s32 reg, u64 value);
void MovieTag_SetBitBltBuf(u32 *p, s32 dbp, s32 dbw, s32 dpsm);
void MovieTag_SetTrxPos(u32 *p, s32 dir, s32 x, s32 y);
void MovieTag_SetTrxReg(u32 *p, s32 w, s32 h);
void MovieTag_SetTrxDir(u32 *p, s32 dir);
void MovieTag_Send(u32 *tags);
void Movie_Init(void);
void Movie_Term(void);
void Movie_Open(char *pss, char *adx);
void Movie_Close(void);
void Movie_PlayOpening(void);
void Movie_PlayEnding(void);
void Movie_Run(void);
s32 Movie_CbVideoData(sceMpeg *mp, sceMpegCbDataStr *str, void *data);
s32 Movie_CbNoData(sceMpeg *mp, void *cbData, void *data);
s32 Movie_CbBackground(sceMpeg *mp, void *cbData, void *data);
s32 Movie_CbError(sceMpeg *mp, void *cbData, void *data);
MovieBuf *Movie_FindBuf(s32 full);
s32 Movie_ReadBuf(MovieBuf *buf, char *tag);
void Movie_WaitRead(s32 sector);
s32 Movie_ReadAhead(char *tag);
s32 Movie_FillBufs(void);
s32 Movie_Demux(void);

#endif
