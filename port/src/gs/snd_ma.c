/*
 * The streams that are not the game's own ADX: a song or a sound mod from outside the disc, in mp3, wav or flac,
 * decoded and played as it is by miniaudio (port/third_party/miniaudio) instead of being converted to ADX first.
 *
 * The game's own files (the whole of the disc, and everything on the ADX path in gs/snd_adx.c) are untouched:
 * snd_adx.c asks this module, when a stream starts, whether the file is one of these three formats, and only then
 * does the stream live here; every other stream is decoded by snd_adx.c exactly as before. So the ADX path stays
 * what it was and a mod's own file is heard without a conversion pass.
 *
 * One ma_engine owns the mixing and its device, beside SDL's device for the ADX streams; every native stream is one
 * ma_sound over an ma_decoder. Build options (the manual, section 2.7), set before the include:
 *   MA_NO_ENCODING           nothing is written here
 *   MA_NO_GENERATION         no test waveforms
 *   MA_NO_RESOURCE_MANAGER   a stream is decoded by the ma_decoder this program owns; the default manager would
 *                            decode through the engine's job thread and add the manager itself. A stream is then
 *                            created from that data source, the one way left (ma_sound_init_from_data_source).
 * WAV, FLAC and MP3 are kept (their MA_NO_* are not set): they are the formats a mod is made of.
 *
 * Volume: both decoders are given one figure (snd_adx.c's player_gain, from the game's own ADXT_SetOutVol) and
 * miniaudio passes it through untouched: measured, a tone read through this engine comes back at its own level
 * (0.8000 in, 0.8000 out) with pan 0 and spatialization off, and a mono file likewise, so the chain itself does
 * not lose or gain anything. What is left is the file: the game's ADX music is mastered loud and a file dropped in
 * from outside usually is not. So a native stream is lifted by native_lift (1.5 by default) to sit near the game's;
 * BT3_MA_GAIN changes it.
 *
 * Without a sound device the engine is not created and every start fails: as with the ADX path without SDL's
 * device, a run with no sound reports the stream stopped and does not depend on a device being there.
 *
 * The game's player state (volume, pan, pause, "has it ended") is mirrored here per stream, so the rest of the
 * port does not need to know which of the two decoders a stream uses. What is NOT mirrored: the game's speed time
 * stretch (snd_adx.c stretch_feed) and the mono option applied to a stream that is already playing; both are set
 * again the next time the stream starts.
 */
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_IMPLEMENTATION
#include "../../third_party/miniaudio/miniaudio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lossless disc cache: decoder only, with no second audio device, gain, rate
 * conversion or engine. snd_adx.c owns playback, pan, loops and time stretch. */
void *PortPcm_Open(const char *path,unsigned channels,unsigned rate,unsigned long long samples){
    ma_decoder *d=(ma_decoder *)calloc(1,sizeof(*d));ma_uint64 length=0;
    ma_decoder_config config=ma_decoder_config_init(ma_format_s16,0,0);
    if(!d)return NULL;
    if(ma_decoder_init_file(path,&config,d)!=MA_SUCCESS){free(d);return NULL;}
    if(d->outputChannels!=channels||d->outputSampleRate!=rate||
       ma_decoder_get_length_in_pcm_frames(d,&length)!=MA_SUCCESS||length!=samples){
        ma_decoder_uninit(d);free(d);return NULL;
    }
    return d;
}
unsigned PortPcm_Read(void *handle,int16_t *out,unsigned frames){
    ma_uint64 read=0;ma_decoder_read_pcm_frames((ma_decoder *)handle,out,frames,&read);return (unsigned)read;
}
int PortPcm_Seek(void *handle,unsigned frame){return ma_decoder_seek_to_pcm_frame((ma_decoder *)handle,frame)==MA_SUCCESS;}
void PortPcm_Close(void *handle){if(handle){ma_decoder_uninit((ma_decoder *)handle);free(handle);}}

/* PAN_AUTO of snd_adx.c: a channel's pan was never set (the file's own balance is kept). */
#define MA_PAN_AUTO (-128)

typedef struct PortMaSound {
    ma_decoder decoder;
    ma_sound   sound;
    int        loop; /* played round and round, as the disc's looping music is (an added song) */
} PortMaSound;

static ma_engine sEngine;
static int sEngineState; /* 0 not looked at yet, 1 created, -1 could not be */
static int sMono;        /* the options' sound mode, as snd_adx.c's ADXT_SetOutputMono is told it */

/* The lift a native stream is played at beside the game's own volume (the comment at the top has the measurement).
   A multiplier: 1 is off, 2 is twice as loud (+6 dB). BT3_MA_GAIN sets it; 1.5 (+3.5 dB) when it says nothing or
   says something that is not a number in range. Above about 2 a file already near full scale clips, as the engine
   does not compress. */
#define MA_LIFT_DEFAULT 1.5f
static float sLift; /* read from the environment the first time a stream starts */
static int sLiftRead;

static float native_lift(void) {
    if (!sLiftRead) {
        const char *e = getenv("BT3_MA_GAIN");
        float g = e != NULL && e[0] != '\0' ? (float)atof(e) : MA_LIFT_DEFAULT;
        sLift = g > 0.0f && g <= 16.0f ? g : MA_LIFT_DEFAULT;
        sLiftRead = 1;
    }
    return sLift;
}

/* As the program ends: the engine's device is stopped, which waits for its thread to be out of the decoders. The
   port leaves with exit() while that thread still runs, and on Windows the C library closes every open file in
   exit(): the thread would read a song from a closed file in the moment before the process is gone. (Stopped, not
   freed: the game's thread may still ask after a stream until then.) */
static void engine_stop(void) {
    if (sEngineState == 1) {
        ma_engine_stop(&sEngine);
    }
}
/* The engine, made the first time a native stream starts (so a run without one never touches a device here). */
static int engine(void) {
    if (sEngineState == 0) {
        if (ma_engine_init(NULL, &sEngine) == MA_SUCCESS) {
            sEngineState = 1;
            atexit(engine_stop);
            fprintf(stderr, "bt3: sound: miniaudio %s (%u Hz), gain x%.2f\n", ma_version_string(), ma_engine_get_sample_rate(&sEngine), native_lift());
        }
        else {
            sEngineState = -1;
            fprintf(stderr, "bt3: sound: no miniaudio device\n");
        }
    }
    return sEngineState == 1 ? 1 : 0;
}

/* Whether `path` ends in `ext` ("mp3"), without regard to case. */
static int ends_in(const char *path, const char *ext) {
    size_t pl = strlen(path), el = strlen(ext);

    if (pl <= el || path[pl - el - 1] != '.') {
        return 0;
    }
    {
        size_t i;
        for (i = 0; i < el; i++) {
            char a = path[pl - el + i], b = ext[i];
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (a != b) {
                return 0;
            }
        }
    }
    return 1;
}

/* The first bytes of a file, or 0 when it cannot be read: enough for the container's mark. */
static int magic(const char *path, uint8_t *out, size_t n) {
    FILE *fp = fopen(path, "rb");
    size_t got;

    if (fp == NULL) {
        return 0;
    }
    got = fread(out, 1, n, fp);
    fclose(fp);
    return got == n;
}

/*
 * 1 when the file is an mp3, wav or flac stream: by its name (".mp3" and the like, in any case) or, for a mod that
 * keeps another name (a `.bin` in the mods folder, say), by its first bytes ("RIFF" for wav, "fLaC", "ID3" or a
 * bare MP3 frame for mp3). A file that is neither is left to the ADX path, which reports it as it did before.
 */
int PortMa_Format(const char *path) {
    uint8_t h[4];

    if (ends_in(path, "mp3") || ends_in(path, "wav") || ends_in(path, "flac")) {
        return 1;
    }
    if (!magic(path, h, sizeof(h))) {
        return 0;
    }
    if (memcmp(h, "RIFF", 4) == 0 || memcmp(h, "fLaC", 4) == 0 || memcmp(h, "ID3", 3) == 0) {
        return 1;
    }
    return h[0] == 0xFF && (h[1] & 0xE0) == 0xE0;
}

/* The options' stereo/mono, taken over at the next start (a stream already here keeps its own). */
void PortMa_SetMono(int mono) {
    sMono = mono != 0;
}

/* Starts `path` here. `loop`: play it round and round, as an added song is (the disc's music loops by itself).
   Returns the stream's handle, or NULL when it cannot be read or played. */
void *PortMa_Start(const char *path, int loop) {
    PortMaSound *s;

    if (!engine()) {
        return NULL;
    }
    s = (PortMaSound *)calloc(1, sizeof(*s));
    if (s == NULL) {
        return NULL;
    }
    {
        ma_decoder_config config = ma_decoder_config_init_default();
        if (sMono) {
            config.channels = 1;
        }
        if (ma_decoder_init_file(path, &config, &s->decoder) != MA_SUCCESS) {
            fprintf(stderr, "bt3: miniaudio: cannot read %s\n", path);
            free(s);
            return NULL;
        }
    }
    if (ma_sound_init_from_data_source(&sEngine, &s->decoder, 0, NULL, &s->sound) != MA_SUCCESS) {
        fprintf(stderr, "bt3: miniaudio: cannot play %s\n", path);
        ma_decoder_uninit(&s->decoder);
        free(s);
        return NULL;
    }
    s->loop = loop != 0;
    ma_sound_set_looping(&s->sound, s->loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_spatialization_enabled(&s->sound, MA_FALSE); /* music and effects, not placed in a room */
    ma_sound_set_volume(&s->sound, 1.0f);                     /* the game's volume follows (PortMa_SetVolume) */
    ma_sound_start(&s->sound);
    return s;
}

/* Stops and frees a stream of PortMa_Start. */
void PortMa_Stop(void *handle) {
    PortMaSound *s = (PortMaSound *)handle;

    if (s == NULL) {
        return;
    }
    ma_sound_uninit(&s->sound); /* stops it and detaches it from the engine */
    ma_decoder_uninit(&s->decoder);
    free(s);
}

/* The volume to play at: the game's own 0..1 gain, as snd_adx.c computes it for an SDL stream, times the lift. */
void PortMa_SetVolume(void *handle, float gain) {
    PortMaSound *s = (PortMaSound *)handle;

    if (s != NULL) {
        float v = gain * native_lift();
        ma_sound_set_volume(&s->sound, v < 0.0f ? 0.0f : v);
    }
}

/* The game's per-input-channel pan (-15..15 each, MA_PAN_AUTO when it was never set): one figure for the file's
   own stereo balance. A stream is almost always centred, so an average is enough. */
void PortMa_SetPan(void *handle, int pan0, int pan1) {
    PortMaSound *s = (PortMaSound *)handle;
    float pan;

    if (s == NULL) {
        return;
    }
    if (pan0 == MA_PAN_AUTO) {
        pan0 = 0;
    }
    if (pan1 == MA_PAN_AUTO) {
        pan1 = 0;
    }
    pan = (float)(pan0 + pan1) / 2.0f / 15.0f;
    if (pan < -1.0f) {
        pan = -1.0f;
    }
    if (pan > 1.0f) {
        pan = 1.0f;
    }
    ma_sound_set_pan(&s->sound, pan);
}

/* Pauses (1) or resumes a stream where it was. */
void PortMa_Pause(void *handle, int paused) {
    PortMaSound *s = (PortMaSound *)handle;

    if (s == NULL) {
        return;
    }
    if (paused) {
        ma_sound_stop(&s->sound);
    }
    else {
        ma_sound_start(&s->sound);
    }
}

/* 1 when a stream that is not looped has reached its end; 0 for a looped one, which is never "ended" (the game
   counts on the disc's music going on for as long as the fight does). */
int PortMa_AtEnd(void *handle) {
    PortMaSound *s = (PortMaSound *)handle;

    if (s == NULL) {
        return 1;
    }
    if (s->loop) {
        return 0;
    }
    return ma_sound_at_end(&s->sound) ? 1 : 0;
}

/* How long a native file lasts, in vertical blanks (as adx_blanks does for an ADX); 0 when it cannot be read. */
int PortMa_Blanks(const char *path) {
    ma_decoder decoder;
    ma_uint64 frames = 0;
    ma_uint32 rate;

    if (!engine()) {
        return 0;
    }
    if (ma_decoder_init_file(path, NULL, &decoder) != MA_SUCCESS) {
        return 0;
    }
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) != MA_SUCCESS) {
        frames = 0;
    }
    rate = decoder.outputSampleRate;
    ma_decoder_uninit(&decoder);
    if (rate == 0) {
        return 0;
    }
    return (int)((frames * 60000 + (ma_uint64)rate * 1001 - 1) / ((ma_uint64)rate * 1001)) + 1;
}
