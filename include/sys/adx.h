#ifndef SYS_ADX_H
#define SYS_ADX_H

#include "types.h"
#include "sys/file.h"

/* Indices into gAdxPlayerTbl (labels "BGM", "MAP", "SE ", "SE ", "VIC", "VIC"). */
enum {
    ADX_CH_BGM,     /* 0: background music */
    ADX_CH_MAP,     /* 1: second music stream ("MAP") */
    ADX_CH_SE0,     /* 2: streamed sound effect 0 */
    ADX_CH_SE1,     /* 3: streamed sound effect 1 */
    ADX_CH_VOICE0,  /* 4: voice 0 */
    ADX_CH_VOICE1   /* 5: voice 1 */
};

/* CRI ADXT status values used here. */
#define ADXT_STAT_STOP 0
#define ADXT_STAT_PLAYEND 5

#define ADX_VOL_MAX 0x80     /* game volume range is 0..0x80 */
#define ADX_PAN_MAX 0x40     /* game pan range is -0x40..0x40 */
#define ADX_OUTVOL_MUTE (-960) /* ADXT output volume is in 0.1 dB units; -960 is silence */
#define ADX_FADE_STEP 120    /* attenuation added by one fade-out step (12 dB) */
#define ADX_VOL_DIVISOR 0x480 /* 0x80 (channel gain) * 9 (option volume) */

/* Volume option used by a channel (index into AdxSaveView.volume). */
#define ADX_OPT_VOL_MUSIC 1 /* channels 0-1 */
#define ADX_OPT_VOL_VOICE 2 /* channels 2-5 */

/* File ids of the character voice lines: base + character * 100 + line. */
#define VOICE_CHARA_BASE 0x10CC2
#define VOICE_CHARA_BASE_ALT 0x16E6A /* used when AdxSaveView.flags bit 0 is set */
#define VOICE_CHARA_LINES 100

#define SAVE_FLAG_ALT_VOICE 1

/* The part of the 0x4000-byte gSaveData block this module reads (the full layout belongs to the option module). */
typedef struct AdxSaveView {
    /* 0x0000 */ u8 unk0[0x1608];
    /* 0x1608 */ s32 flags;      /* bit 0: alternate voice set */
    /* 0x160C */ u8 unk160C[0x16A4 - 0x160C];
    /* 0x16A4 */ s32 volume[3];  /* 0..9; [1] music streams, [2] voice / SE streams ([0] is not used here) */
} AdxSaveView;

s32 Adx_CalcOutVol(s32 ch, s32 vol);
s32 Adx_ConvPan(s32 pan);
s32 Adx_CalcVolume(s32 ch, s32 vol);
s32 Adx_Stub265720(void);
void Adx_StopAll(void);
void Adx_PauseSeVoice(void);
void Adx_ResumeSeVoice(void);
void Adx_Play(s32 ch, s32 id, s32 vol, s32 pan);
void Adx_Stop(s32 ch);
void Adx_PlayPaused(s32 ch, s32 id, s32 vol, s32 pan);
void Adx_PlayFilePaused(s32 ch, char *fname, s32 vol, s32 pan);
void Adx_Resume(s32 ch);
void Adx_SetVolume(s32 ch, s32 vol);
s32 Adx_GetStat(s32 ch);
s32 Adx_IsStopped(s32 ch);
void Adx_SetPan(s32 ch, s32 pan0, s32 pan1);
void Adx_FadeOutStep(s32 ch);
void Adx_SetMono(s32 mono);

void Bgm_PlayEx(s32 id, s32 vol, s32 pan);
void Bgm_Play(s32 id);
void Bgm_Stop(void);
void Bgm_SetVolume(s32 vol);
s32 Bgm_GetStat(void);
s32 Bgm_IsStopped(void);
void Bgm_FadeOutStep(void);

void MapBgm_PlayEx(s32 id, s32 vol, s32 pan);
void MapBgm_Play(s32 id);
void MapBgm_Stop(void);
void MapBgm_SetVolume(s32 vol);
s32 MapBgm_GetStat(void);
s32 MapBgm_IsStopped(void);
void MapBgm_FadeOutStep(void);

void Voice_PlayCharaEx(s32 voice, s32 chara, s32 line, s32 vol, s32 pan);
void Voice_PlayChara(s32 voice, s32 chara, s32 line);
void Voice_Play(s32 voice, s32 id, s32 vol, s32 pan);
void Voice_PlayDefault(s32 voice, s32 id);
void Voice_PlayPaused(s32 voice, s32 id, s32 vol, s32 pan);
void Voice_PlayPausedDefault(s32 voice, s32 id);
void Voice_Resume(s32 voice);
void Voice_Stop(s32 voice);
s32 Voice_GetStat(s32 voice);
s32 Voice_IsStopped(s32 voice);
void Voice_FadeOutStep(s32 voice);

void StreamSe_Play(s32 se, s32 id, s32 vol, s32 pan);
void StreamSe_PlayDefault(s32 se, s32 id);
void StreamSe_PlayPaused(s32 se, s32 id, s32 vol, s32 pan);
void StreamSe_PlayPausedDefault(s32 se, s32 id);
void StreamSe_Resume(s32 se);
void StreamSe_Stop(s32 se);
s32 StreamSe_GetStat(s32 se);
s32 StreamSe_IsStopped(s32 se);
void StreamSe_FadeOutStep(s32 se);

#endif
