/*
 * Sound effects on PC: the game's own sound driver for the PS2's second processor (SOUNDS.IRX), replaced.
 *
 * What the game does (src/sys/snd.c, the part that runs on the main processor and is compiled unchanged):
 *   - loads "bank" files and hands their three parts over: the sample data goes into the sound chip's memory
 *     (sceSdRemote, a transfer to an address it chose), the two tables go into the second processor's memory
 *     (sceSifSetDma), and command SET_BANK names the three places;
 *   - once per frame sends a queue of at most 32 commands: PLAY (bank, number, volume, pan, pitch in cents, a
 *     handle), STOP by number, STOP by handle; plus a few global commands (reset, pause, stop a bank, mono).
 * SOUNDS.IRX played these through Sony's sequencer (modsesq2) and synthesizer (modhsyn). Measured over all 361
 * bank files of the game (18,924 effects): every effect is a sequence of ONE note (two effects also stop it
 * again after two seconds), every note uses the timbre with its own number, every timbre uses the sample with
 * its own number, and all timbres have the same settings (volume 100, pan centre, base note 60, envelope
 * 0x80FF / 0x5FC0: full level at once, held). So an effect is "play sample N of the bank", and this file is a
 * sampler: it decodes the PS2 sound chip's ADPCM, changes the pitch, and mixes the voices into one SDL stream.
 * The tables are still read (sequence -> timbre -> sample), so a bank that breaks the pattern is honoured.
 *
 * Bank tables (Sony's formats, little-endian, chunk names stored reversed, e.g. "IECSigaV" = SCEI Vagi):
 *   header part  Vagi: +0x0C highest sample number, +0x10 offsets (from the chunk) of 8-byte records
 *                      { u32 offset in the sample data, u16 rate in Hz, u8 loop flag, u8 }
 *                Setb: +0x20 record size, +0x22 highest timbre number, +0x24 records; a record starts with the
 *                      u16 sample number
 *   sequence part  Sesq: +0x34 offsets (from the chunk) of the sequences; a sequence is 0x10 bytes of header
 *                      and then its commands: 00 90 00 <timbre> 64 ... (note on)
 * Sample data: 16-byte blocks: shift / filter, flags (1 = last block, 2 = and loop, 4 = loop start), 28 nibbles.
 *
 * Not reproduced: the envelope's release (a stopped voice fades over a few milliseconds here), the sound chip's
 * interpolation and its reverb (none is set up by these banks). The overall loudness against the music is by
 * ear (SE_GAIN) until it is compared with the console.
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gs_internal.h"

#define SPU_SIZE 0x200000
#define VOICES 48
#define OUT_RATE 48000
/* Loudness of the effects against the music. 0.55 (a guess) was too loud by the user's ear. 0.25 follows the
   chain the console had: sequence volume, note velocity and timbre volume are each 100 of 127 ((100/127)^3 = 0.49)
   and the sound chip's voice volume tops out at half scale. BT3_SE_GAIN=<percent> overrides it for tuning. */
#define SE_GAIN_DEFAULT 0.25f
static float se_gain(void) {
    static float g = -1.0f;
    if (g < 0.0f) {
        g = getenv("BT3_SE_GAIN") != NULL ? (float)atoi(getenv("BT3_SE_GAIN")) / 100.0f : SE_GAIN_DEFAULT;
    }
    return g;
}
extern int gPortSePercent; /* the settings overlay's volume (snd_adx.c); takes effect with the next sound */
#define SE_GAIN (se_gain() * (float)gPortSePercent / 100.0f)

typedef struct Bank {
    int set;
    uint32_t spu;          /* where the sample data is in sSpu */
    const uint8_t *hd, *sq;
    int paused;
    float volume;
} Bank;

typedef struct Voice {
    int on, bank, id, handle;
    uint32_t pos, loopPos;     /* next block / loop start, addresses in sSpu */
    int h1, h2;                /* decoder history */
    int16_t buf[28];
    int have;                  /* samples of buf decoded; 0 = decode the next block first */
    double frac, step;         /* position in buf, input samples per output sample */
    int last, ended;           /* this block is the last one; nothing more to decode */
    int16_t prev;              /* the sample before buf[0], for interpolation */
    float gl, gr;
    float fade;                /* 1 = playing; goes down to 0 after a stop */
    int stopping;
} Voice;

static uint8_t sSpu[SPU_SIZE];
static Bank sBanks[8];
static Voice sVoices[VOICES];
static SDL_AudioStream *sStream;
static int sMono, sTried;

extern SDL_AudioDeviceID Port_AudioDevice(void); /* snd_adx.c */

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }

/* The chunk "SCEI<name>" inside a table (searched over at most `limit` bytes). */
static const uint8_t *chunk(const uint8_t *p, const char *name, uint32_t limit) {
    char tag[8] = {'I', 'E', 'C', 'S', name[3], name[2], name[1], name[0]};
    uint32_t o;

    for (o = 0; p != NULL && o + 16 <= limit; o += 4) {
        if (memcmp(p + o, tag, 8) == 0) {
            return p + o;
        }
    }
    return NULL;
}

/* Sequence number -> where its sample starts, its rate. 0 = no such effect. */
static int lookup(const Bank *b, int id, uint32_t *start, uint32_t *rate) {
    const uint8_t *sesq = chunk(b->sq, "Sesq", 0x4000), *setb = chunk(b->hd, "Setb", 0x8000), *vagi = chunk(b->hd, "Vagi", 0x8000);
    uint32_t timbre = (uint32_t)id, vag, o;

    if (vagi == NULL) {
        return 0;
    }
    if (sesq != NULL) {
        const uint8_t *seq = sesq + le32(sesq + 0x34 + 4 * (uint32_t)id);
        if (seq[0x11] == 0x90) { /* note on: 90, 00, timbre, velocity */
            timbre = seq[0x13];
        }
    }
    vag = timbre;
    if (setb != NULL && timbre <= setb[0x22]) {
        vag = le16(setb + 0x24 + timbre * setb[0x20]);
    }
    if (vag > le32(vagi + 0x0C)) {
        return 0;
    }
    o = le32(vagi + 0x10 + 4 * vag);
    *start = b->spu + le32(vagi + o);
    *rate = le16(vagi + o + 4);
    return *start + 16 <= SPU_SIZE && *rate != 0;
}

/* Decodes the voice's next 16-byte block into v->buf. */
static void decode_block(Voice *v) {
    static const int f0[5] = {0, 60, 115, 98, 122}, f1[5] = {0, 0, -52, -55, -60};
    const uint8_t *p;
    int shift, filter, flags, i;

    if (v->last || v->pos + 16 > SPU_SIZE) {
        v->ended = 1;
        v->have = 0;
        return;
    }
    p = &sSpu[v->pos];
    shift = p[0] & 15;
    filter = (p[0] >> 4) & 7;
    flags = p[1];
    if (filter > 4) { filter = 0; }
    if (shift > 12) { shift = 9; } /* what the chip does with the invalid values */
    if (flags & 4) {
        v->loopPos = v->pos;
    }
    v->prev = v->have ? v->buf[27] : 0;
    for (i = 0; i < 28; i++) {
        int nib = (p[2 + i / 2] >> ((i & 1) * 4)) & 15, s;
        if (nib & 8) { nib -= 16; }
        s = ((nib << 12) >> shift) + ((v->h1 * f0[filter] + v->h2 * f1[filter] + 32) >> 6);
        if (s > 32767) { s = 32767; }
        if (s < -32768) { s = -32768; }
        v->buf[i] = (int16_t)s;
        v->h2 = v->h1;
        v->h1 = s;
    }
    v->have = 28;
    v->pos += 16;
    if (flags & 1) {             /* the last block: loop or stop after it */
        if ((flags & 2) && v->loopPos != 0) {
            v->pos = v->loopPos;
        } else {
            v->last = 1;
        }
    }
}

/* SDL's audio thread wants more (the stream is locked while this runs). */
static void SDLCALL mix(void *user, SDL_AudioStream *stream, int additional, int total) {
    int16_t out[512 * 2];
    float acc[512 * 2];
    int frames = additional / 4, n, i, k;

    (void)user; (void)total;
    while (frames > 0) {
        n = frames > 512 ? 512 : frames;
        memset(acc, 0, sizeof(float) * (size_t)n * 2);
        for (k = 0; k < VOICES; k++) {
            Voice *v = &sVoices[k];
            if (!v->on || sBanks[v->bank].paused) {
                continue;
            }
            for (i = 0; i < n; i++) {
                float s, a, b;
                int ip;
                while (v->frac >= (double)v->have) {
                    v->frac -= (double)v->have;
                    decode_block(v);
                    if (v->ended) {
                        break;
                    }
                }
                if (v->ended) {
                    v->on = 0;
                    break;
                }
                ip = (int)v->frac;
                a = (float)(ip == 0 ? v->prev : v->buf[ip - 1]);
                b = (float)v->buf[ip];
                s = (a + (b - a) * (float)(v->frac - (double)ip)) * v->fade;
                acc[i * 2] += s * v->gl;
                acc[i * 2 + 1] += s * v->gr;
                v->frac += v->step;
                if (v->stopping) {
                    v->fade -= 1.0f / (0.008f * OUT_RATE); /* 8 ms */
                    if (v->fade <= 0.0f) {
                        v->on = 0;
                        break;
                    }
                }
            }
        }
        for (i = 0; i < n * 2; i++) {
            float s = acc[i];
            out[i] = (int16_t)(s > 32767.0f ? 32767.0f : s < -32768.0f ? -32768.0f : s);
        }
        SDL_PutAudioStreamData(stream, out, n * 4);
        frames -= n;
    }
}

static int stream_ready(void) {
    if (sStream == NULL && !sTried) {
        SDL_AudioDeviceID dev = Port_AudioDevice();
        SDL_AudioSpec spec;

        if (dev == 0) {
            if (GsGpu_Enabled() || gGsFrame > 2) {
                sTried = 1; /* decided: no device */
            }
            return 0;
        }
        sTried = 1;
        spec.format = SDL_AUDIO_S16;
        spec.channels = 2;
        spec.freq = OUT_RATE;
        sStream = SDL_CreateAudioStream(&spec, NULL);
        if (sStream != NULL) {
            SDL_SetAudioStreamGetCallback(sStream, mix, NULL);
            SDL_BindAudioStream(dev, sStream);
        }
    }
    return sStream != NULL;
}

static int slot_of(int mask) {
    int i;

    for (i = 0; i < 8; i++) {
        if (mask & (1 << i)) {
            return i;
        }
    }
    return -1;
}

static void play(int mask, int id, int volume, int pan, int pitch, int handle) {
    if (gPortResim) { /* a frame that is only being re-run: its sounds were heard the first time */
        return;
    }
    int slot = slot_of(mask), k, pick = -1;
    uint32_t start, rate;
    double theta;
    Voice *v;

    if (slot < 0 || !sBanks[slot].set || !lookup(&sBanks[slot], id, &start, &rate)) {
        return;
    }
    for (k = 0; k < VOICES; k++) {
        if (!sVoices[k].on) {
            pick = k;
            break;
        }
    }
    if (pick < 0) { /* all busy: take one that is being stopped, else the first */
        for (k = 0; k < VOICES && !sVoices[k].stopping; k++) {
        }
        pick = k < VOICES ? k : 0;
    }
    v = &sVoices[pick];
    memset(v, 0, sizeof(*v));
    v->bank = slot;
    v->id = id;
    v->handle = handle;
    v->pos = start;
    v->step = (double)rate * pow(2.0, (double)pitch / 1200.0) / (double)OUT_RATE;
    v->frac = 0.0;
    v->fade = 1.0f;
    theta = (sMono ? 0.5 : (double)pan / 127.0) * 1.5707963267948966;
    v->gl = (float)(cos(theta) * 1.41421356) * ((float)volume / 127.0f) * sBanks[slot].volume * SE_GAIN;
    v->gr = (float)(sin(theta) * 1.41421356) * ((float)volume / 127.0f) * sBanks[slot].volume * SE_GAIN;
    v->on = 1;
    if (getenv("BT3_SND_VERBOSE") != NULL) {
        fprintf(stderr, "se: play bank %d effect %d: %u Hz, volume %d, pan %d, pitch %d, handle %d\n", slot, id, rate, volume, pan, pitch, handle);
    }
}

/* Stops voices: by bank mask (id < 0, handle < 0), by bank and number, or by handle. */
static void stop(int mask, int id, int handle) {
    int k;

    for (k = 0; k < VOICES; k++) {
        Voice *v = &sVoices[k];
        if (v->on && ((handle >= 0 && v->handle == handle) || (handle < 0 && (mask & (1 << v->bank)) && (id < 0 || v->id == id)))) {
            v->stopping = 1;
        }
    }
}

/* ------------------------------------------------------------ what the game calls (Sony's library functions) */

/* the game's structure: its two pointers are 4 bytes wide in every build (PS2 layout) */
typedef struct SifDmaData { uint32_t data, addr; int32_t size, mode; } SifDmaData;

/* sceSifSetDma: main memory -> the second processor's memory. Here both are ordinary memory. */
uint32_t sceSifSetDma(SifDmaData *d, int count) {
    int i;

    for (i = 0; i < count; i++) {
        if (d[i].addr != 0 && d[i].data != 0 && d[i].size > 0) {
            memcpy((void *)(uintptr_t)d[i].addr, (void *)(uintptr_t)d[i].data, (size_t)d[i].size);
        }
    }
    return 1;
}

/* sceSdRemote (0x2967C0 in the PS2 executable). Used for two things: 0x80D0 = transfer `size` bytes at `iopAddr`
   into the sound chip's memory at `spuAddr`; 0x80F0 = is the transfer over (always: it is done at once). */
int func_002967C0(int block, int cmd, int core, int mode, void *iopAddr, uint32_t spuAddr, uint32_t size) {
    (void)block; (void)core; (void)mode;
    if (cmd == 0x80D0) {
        if (iopAddr != NULL && spuAddr < SPU_SIZE && size <= SPU_SIZE - spuAddr) {
            if (sStream != NULL) { SDL_LockAudioStream(sStream); }
            memcpy(&sSpu[spuAddr], iopAddr, size);
            if (sStream != NULL) { SDL_UnlockAudioStream(sStream); }
        }
        return 0;
    }
    return cmd == 0x80F0 ? 1 : 0;
}

/* sceSifCallRpc to SOUNDS.IRX (the only server the game calls this way): one command, answered at once. */
int sceSifCallRpc(void *client, int fno, int mode, void *send, int sendSize, void *recv, int recvSize, void *cb, void *arg) {
    const int32_t *w = send;
    int ready = stream_ready(), i;

    (void)client; (void)mode; (void)sendSize; (void)cb; (void)arg;
    if (recv != NULL && recvSize >= 4) {
        memset(recv, 0, 4); /* the result word */
    }
    if (sStream != NULL) { SDL_LockAudioStream(sStream); }
    switch (fno) {
    case 0x0: /* INIT */
    case 0x2: /* RESET */
        for (i = 0; i < VOICES; i++) { sVoices[i].on = 0; }
        break;
    case 0x3: { /* SET_BANK: mask, where the samples are, the header table, the sequence table */
        int slot = slot_of(w[0]);
        if (slot >= 0) {
            sBanks[slot].set = 1;
            sBanks[slot].spu = (uint32_t)w[1];
            sBanks[slot].hd = (const uint8_t *)(uintptr_t)(uint32_t)w[2];
            sBanks[slot].sq = (const uint8_t *)(uintptr_t)(uint32_t)w[3];
            sBanks[slot].paused = 0;
            if (sBanks[slot].volume == 0.0f) { sBanks[slot].volume = 1.0f; }
        }
        break;
    }
    case 0x4: /* FREE_BANK */
        for (i = 0; i < 8; i++) {
            if (w[0] & (1 << i)) {
                sBanks[i].set = 0;
            }
        }
        for (i = 0; i < VOICES; i++) {
            if (w[0] & (1 << sVoices[i].bank)) { sVoices[i].on = 0; }
        }
        break;
    case 0x5: sMono = w[0] != 0; break;
    case 0x6: /* SET_PAUSE: mask, on */
        for (i = 0; i < 8; i++) {
            if (w[0] & (1 << i)) { sBanks[i].paused = w[1] != 0; }
        }
        break;
    case 0x7: /* SET_VOLUME: mask, 0..0x7F */
        for (i = 0; i < 8; i++) {
            if (w[0] & (1 << i)) { sBanks[i].volume = (float)w[1] / 127.0f; }
        }
        break;
    case 0xA: stop(w[0], -1, -1); break; /* STOP_BANK */
    case 0xD: { /* QUEUE: count, then 12-byte commands { type, loop, u16 handle, bank, id, volume, pan, s32 pitch } */
        const uint8_t *q = send;
        uint32_t count = le32(q);
        for (i = 0; ready && i < (int)count && i < 32; i++) {
            const uint8_t *c = q + 4 + i * 12;
            if (c[0] == 0) {
                play(c[4], c[5], c[6], c[7], (int32_t)le32(c + 8), (int)le16(c + 2));
            } else if (c[0] == 1) {
                stop(c[4], c[5], -1);
            } else if (c[0] == 2) {
                stop(0, -1, (int)le16(c + 2));
            }
        }
        break;
    }
    default: break; /* TICK, FIGHTERS and the commands the game never sends */
    }
    if (sStream != NULL) { SDL_UnlockAudioStream(sStream); }
    return 0;
}
