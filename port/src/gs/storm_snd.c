/*
 * The thunder of the full power storm (src/battle/stg_c.c, PortStorm_Draw).
 *
 * Four sounds of our own, made by port/tools/make_thunder.py (filtered noise: no recording of anyone's), kept beside
 * the online screens' pictures as tbo/ui/thunder_1.wav .. thunder_4.wav. They are read on a thread of their own when
 * a storm starts and played on a stream of their own on the game's audio device, mixed here (up to four at once), at
 * the effects volume of the settings.
 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../../../include/port_storm.h"

extern SDL_AudioDeviceID Port_AudioDevice(void); /* snd_adx.c */
extern int gPortSePercent;                       /* snd_adx.c: the effects volume, percent */

#define THUNDER_SOUNDS 4
#define THUNDER_VOICES 4
#define THUNDER_RATE 48000
#define THUNDER_GAIN 0.9f

typedef struct {
    int sound; /* -1: free */
    int pos;   /* frames played */
    float left, right;
} ThunderVoice;

static Sint16 *sPcm[THUNDER_SOUNDS]; /* mono, THUNDER_RATE */
static int sFrames[THUNDER_SOUNDS];
static SDL_AtomicInt sState; /* 0 not asked, 1 reading, 2 read */
static SDL_AtomicInt sHeld;  /* 1: the fight is paused, the thunder under way waits */
static SDL_AudioStream *sStream;
static ThunderVoice sVoice[THUNDER_VOICES];

/* The storm's state (src/battle/stg_c.c), here so that it is not one of the game's variables (include/port_storm.h). */
static PortStormState sStorm = {-1, 0.0f, 0, 1200, -1, 0, {0}, {0}, {0}, {0}, -1, -1, 0.0f, 0.0f, 0x2545F491u};
PortStormState *PortStorm_State(void) {
    return &sStorm;
}

/* tbo/ui/<name> beside the program, in the current folder (Android: where GameActivity copies tbo/ui) or in a source
   checkout (as gs_draw.c's pictures) */
static int load_wav(const char *name, SDL_AudioSpec *spec, Uint8 **buf, Uint32 *len) {
    char path[1024], cwd[768];
    const char *base = SDL_GetBasePath();

    snprintf(path, sizeof(path), "%stbo/ui/%s", base != NULL ? base : "", name);
    if (SDL_LoadWAV(path, spec, buf, len)) {
        return 1;
    }
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(path, sizeof(path), "%s/tbo/ui/%s", cwd, name);
        if (SDL_LoadWAV(path, spec, buf, len)) {
            return 1;
        }
    }
    snprintf(path, sizeof(path), "port/tbo/ui/%s", name);
    return SDL_LoadWAV(path, spec, buf, len);
}

static int SDLCALL load_all(void *unused) {
    SDL_AudioSpec want = {SDL_AUDIO_S16, 1, THUNDER_RATE};
    int i, got = 0;

    (void)unused;
    for (i = 0; i < THUNDER_SOUNDS; i++) {
        char name[32];
        SDL_AudioSpec spec;
        Uint8 *buf = NULL, *out = NULL;
        Uint32 len = 0;
        int outLen = 0;

        snprintf(name, sizeof(name), "thunder_%d.wav", i + 1);
        if (!load_wav(name, &spec, &buf, &len)) {
            fprintf(stderr, "bt3: storm: cannot read %s: %s\n", name, SDL_GetError());
            continue;
        }
        if (SDL_ConvertAudioSamples(&spec, buf, (int)len, &want, &out, &outLen)) {
            sPcm[i] = (Sint16 *)out;
            sFrames[i] = outLen / 2;
            got++;
        }
        SDL_free(buf);
    }
    fprintf(stderr, "bt3: storm: %d thunder sounds read\n", got);
    SDL_SetAtomicInt(&sState, 2);
    return 0;
}

void PortThunder_Load(void) {
    SDL_Thread *t;

    if (SDL_GetAtomicInt(&sState) != 0) {
        return;
    }
    SDL_SetAtomicInt(&sState, 1);
    t = SDL_CreateThread(load_all, "thunder", NULL);
    if (t != NULL) {
        SDL_DetachThread(t);
    } else {
        load_all(NULL);
    }
}

/* The stream's callback (its lock held): the playing voices, mixed, as 16-bit stereo. */
static void SDLCALL mix(void *user, SDL_AudioStream *stream, int additional, int total) {
    Sint16 out[1024 * 2];

    (void)user;
    (void)total;
    if (SDL_GetAtomicInt(&sHeld)) { /* silence, and every voice stays where it is */
        static const Sint16 quiet[512 * 2];

        while (additional > 0) {
            int n = additional > (int)sizeof(quiet) ? (int)sizeof(quiet) : additional & ~3;

            if (n <= 0) {
                break;
            }
            SDL_PutAudioStreamData(stream, quiet, n);
            additional -= n;
        }
        return;
    }
    while (additional > 0) {
        int n = additional / 4 > 1024 ? 1024 : additional / 4;
        int f, v;

        if (n <= 0) {
            break;
        }
        for (f = 0; f < n; f++) {
            float l = 0.0f, r = 0.0f;

            for (v = 0; v < THUNDER_VOICES; v++) {
                ThunderVoice *tv = &sVoice[v];
                float s;

                if (tv->sound < 0) {
                    continue;
                }
                s = (float)sPcm[tv->sound][tv->pos];
                l += s * tv->left;
                r += s * tv->right;
                if (++tv->pos >= sFrames[tv->sound]) {
                    tv->sound = -1;
                }
            }
            out[f * 2] = (Sint16)(l > 32767.0f ? 32767 : l < -32768.0f ? -32768 : (int)l);
            out[f * 2 + 1] = (Sint16)(r > 32767.0f ? 32767 : r < -32768.0f ? -32768 : (int)r);
        }
        SDL_PutAudioStreamData(stream, out, n * 4);
        additional -= n * 4;
    }
}

static int stream_ready(void) {
    static int tried;

    if (sStream == NULL && !tried) {
        SDL_AudioDeviceID dev = Port_AudioDevice();
        SDL_AudioSpec spec = {SDL_AUDIO_S16, 2, THUNDER_RATE};
        int v;

        if (dev == 0) {
            return 0;
        }
        tried = 1;
        for (v = 0; v < THUNDER_VOICES; v++) {
            sVoice[v].sound = -1;
        }
        sStream = SDL_CreateAudioStream(&spec, NULL);
        if (sStream != NULL) {
            SDL_SetAudioStreamGetCallback(sStream, mix, NULL);
            if (!SDL_BindAudioStream(dev, sStream)) {
                SDL_DestroyAudioStream(sStream);
                sStream = NULL;
            }
        }
    }
    return sStream != NULL;
}

/* Sound `which` (0..3) at `volume` (0..1), `pan` -1 left .. 1 right. Nothing until the sounds are read. */
void PortThunder_Play(int which, float volume, float pan) {
    float gain;
    int v, best = 0;

    if (SDL_GetAtomicInt(&sState) != 2 || which < 0 || which >= THUNDER_SOUNDS || sPcm[which] == NULL || !stream_ready()) {
        return;
    }
    gain = volume * THUNDER_GAIN * (float)gPortSePercent / 100.0f;
    if (gain <= 0.0f) {
        return;
    }
    pan = pan < -1.0f ? -1.0f : pan > 1.0f ? 1.0f : pan;
    SDL_LockAudioStream(sStream);
    for (v = 0; v < THUNDER_VOICES; v++) { /* a free voice, or the one furthest along */
        if (sVoice[v].sound < 0) {
            best = v;
            break;
        }
        if (sVoice[v].pos > sVoice[best].pos) {
            best = v;
        }
    }
    sVoice[best].sound = which;
    sVoice[best].pos = 0;
    sVoice[best].left = gain * (pan > 0.0f ? 1.0f - pan * 0.7f : 1.0f);
    sVoice[best].right = gain * (pan < 0.0f ? 1.0f + pan * 0.7f : 1.0f);
    SDL_UnlockAudioStream(sStream);
}

/* The fight paused (1) or going on (0): the thunder under way waits, silent. */
void PortThunder_Hold(int held) {
    SDL_SetAtomicInt(&sHeld, held != 0);
}

/* The fight is over (or a new one begins): no thunder goes on. */
void PortThunder_Stop(void) {
    int v;

    SDL_SetAtomicInt(&sHeld, 0);
    if (sStream == NULL) {
        return;
    }
    SDL_LockAudioStream(sStream);
    for (v = 0; v < THUNDER_VOICES; v++) {
        sVoice[v].sound = -1;
    }
    SDL_UnlockAudioStream(sStream);
}
