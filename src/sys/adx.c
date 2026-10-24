#include "common.h"
#include "sys/adx.h"

/* CRI ADX */
extern void ADXM_ExecMain(void);
extern void ADXT_Stop(ADXT_HN adxt);
extern s32 ADXT_GetStat(ADXT_HN adxt);
extern void ADXT_Pause(ADXT_HN adxt, s32 sw);
extern void ADXT_SetOutVol(ADXT_HN adxt, s32 vol);
extern s32 ADXT_GetOutVol(ADXT_HN adxt);
extern void ADXT_SetOutPan(ADXT_HN adxt, s32 chan, s32 pan);
extern void ADXT_StartFname(ADXT_HN adxt, char *fname);
extern void ADXT_SetOutputMono(s32 flag);

/* game */
extern f32 Mathf_Sin(f32 angle);          /* sinf of the angle wrapped to [-pi, pi] */
extern s32 BtlScript_IsSlotBusy(s32 voice, s32 kind); /* D_00334788[voice][kind] != 0: the voice channel is reserved */

#define ADX_CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : (x) > (hi) ? (hi) : (x))

extern AdxPlayer gAdxPlayerTbl[];
extern s32 gAdxChannelGainTbl[];
extern AdxSaveView *gSaveData;

/* Converts a 0..0x80 volume to the ADXT output level (0.1 dB units, 0 = full, -960 = mute), applying the channel gain and the volume option. */
s32 Adx_CalcOutVol(s32 ch, s32 vol) {
    s32 kind;
    s32 v;

    switch (ch) {
    case ADX_CH_BGM:
    case ADX_CH_MAP:
        kind = ADX_OPT_VOL_MUSIC;
        break;
    default:
        kind = ADX_OPT_VOL_VOICE;
        break;
    }
    vol = vol * gAdxChannelGainTbl[ch < 6 ? ch : ch < 4+PORT_FIGHTER_CAPACITY ? 4 : 2] * gSaveData->volume[kind];
    vol = vol / ADX_VOL_DIVISOR;
    v = ADX_CLAMP(vol, 0, ADX_VOL_MAX);
    vol = v;
    if (vol == 0) {
        vol = ADX_OUTVOL_MUTE;
    } else {
        f32 s = Mathf_Sin((ADX_VOL_MAX - vol) * 0.0078125f);

        vol = s * s * 0.28f * -960.0f;
    }
    return vol;
}

/* Converts a -0x40..0x40 pan to the ADXT range: (pan - 1) / 4, i.e. -16..15 with 0 at the centre. */
s32 Adx_ConvPan(s32 pan) {
    s32 p = ADX_CLAMP(pan, -ADX_PAN_MAX, ADX_PAN_MAX);

    pan = p;
    return (pan - 1) / 4;
}

/* Linear 0..0x80 volume after the channel gain and the volume option (unused). */
s32 Adx_CalcVolume(s32 ch, s32 vol) {
    s32 kind;
    s32 v;

    switch (ch) {
    case ADX_CH_BGM:
    case ADX_CH_MAP:
        kind = ADX_OPT_VOL_MUSIC;
        break;
    default:
        kind = ADX_OPT_VOL_VOICE;
        break;
    }
    vol = vol * gAdxChannelGainTbl[ch < 6 ? ch : ch < 4+PORT_FIGHTER_CAPACITY ? 4 : 2] * gSaveData->volume[kind];
    vol = vol / ADX_VOL_DIVISOR;
    v = ADX_CLAMP(vol, 0, ADX_VOL_MAX);
    vol = v;
    return vol;
}

/* Unused stub that returns 0. */
s32 Adx_Stub265720(void) {
    return 0;
}

/* Stops all six players and blocks until every one reports ADXT_STAT_STOP. */
void Adx_StopAll(void) {
    s32 i;
    s32 done;

    Adx_ResumeSeVoice();
    for (i = 0; i < ADX_PLAYER_COUNT; i++) {
        Adx_Stop(i);
    }
    while (1) {
        done = 1;
        for (i = 0; i < ADX_PLAYER_COUNT; i++) {
            if (Adx_GetStat(i) != ADXT_STAT_STOP) {
                done = 0;
                break;
            }
        }
        if (done) {
            break;
        }
        ADXM_ExecMain();
    }
}

/* Pauses the two SE and the two voice players (music keeps playing). */
void Adx_PauseSeVoice(void) {
    s32 i;
    for (i = 2; i < ADX_PLAYER_COUNT; i++) ADXT_Pause(File_AdxPlayer(i)->adxt, 1);
}

/* Resumes the two SE and the two voice players. */
void Adx_ResumeSeVoice(void) {
    s32 i;
    for (i = 2; i < ADX_PLAYER_COUNT; i++) ADXT_Pause(File_AdxPlayer(i)->adxt, 0);
}

/* Starts file `id` on a player with the given volume and pan; a voice channel that is reserved is left alone. */
void Adx_Play(s32 ch, s32 id, s32 vol, s32 pan) {
    switch (ch) {
    case ADX_CH_VOICE0:
        if (BtlScript_IsSlotBusy(0, 0)) {
            return;
        }
        if (BtlScript_IsSlotBusy(0, 1)) {
            return;
        }
        break;
    case ADX_CH_VOICE1:
        if (BtlScript_IsSlotBusy(1, 0)) {
            return;
        }
        if (BtlScript_IsSlotBusy(1, 1)) {
            return;
        }
        break;
    }
    Adx_SetPan(ch, pan, pan);
    Adx_SetVolume(ch, vol);
    ADXT_Pause(File_AdxPlayer(ch)->adxt, 0);
    File_StartAdxById(File_AdxPlayer(ch)->adxt, id);
}

/* Stops a player. */
void Adx_Stop(s32 ch) {
    ADXT_Stop(File_AdxPlayer(ch)->adxt);
}

/* Starts file `id` on a player in the paused state (Adx_Resume lets it out). */
void Adx_PlayPaused(s32 ch, s32 id, s32 vol, s32 pan) {
    Adx_SetPan(ch, pan, pan);
    Adx_SetVolume(ch, vol);
    ADXT_Pause(File_AdxPlayer(ch)->adxt, 1);
    File_StartAdxById(File_AdxPlayer(ch)->adxt, id);
}

/* Like Adx_PlayPaused, for a file given by name instead of AFS id (the movie soundtracks). */
void Adx_PlayFilePaused(s32 ch, char *fname, s32 vol, s32 pan) {
    Adx_SetPan(ch, pan, pan);
    Adx_SetVolume(ch, vol);
    ADXT_Pause(File_AdxPlayer(ch)->adxt, 1);
    ADXT_StartFname(File_AdxPlayer(ch)->adxt, fname);
}

/* Takes a player out of pause. */
void Adx_Resume(s32 ch) {
    ADXT_Pause(File_AdxPlayer(ch)->adxt, 0);
}

/* Sets a player's volume (0..0x80, before channel gain and option). */
void Adx_SetVolume(s32 ch, s32 vol) {
    ADXT_HN adxt = File_AdxPlayer(ch)->adxt;

    ADXT_SetOutVol(adxt, Adx_CalcOutVol(ch, vol));
}

/* Returns a player's ADXT status. */
s32 Adx_GetStat(s32 ch) {
    return ADXT_GetStat(File_AdxPlayer(ch)->adxt);
}

/* Returns 1 when a player is stopped or has reached the end of its stream. */
s32 Adx_IsStopped(s32 ch) {
    s32 stopped;

    switch (Adx_GetStat(ch)) {
    case ADXT_STAT_STOP:
    case ADXT_STAT_PLAYEND:
        stopped = 1;
        break;
    default:
        stopped = 0;
        break;
    }
    return stopped;
}

/* Sets the pan of both output channels of a player (-0x40..0x40 each). */
void Adx_SetPan(s32 ch, s32 pan0, s32 pan1) {
    ADXT_SetOutPan(File_AdxPlayer(ch)->adxt, 0, Adx_ConvPan(pan0));
    ADXT_SetOutPan(File_AdxPlayer(ch)->adxt, 1, Adx_ConvPan(pan1));
}

/* One fade-out step: lowers a player's output level by 12 dB until it reaches mute. */
void Adx_FadeOutStep(s32 ch) {
    s32 min = ADX_OUTVOL_MUTE;
    s32 vol = ADXT_GetOutVol(File_AdxPlayer(ch)->adxt);

    if (vol > min) {
        vol -= ADX_FADE_STEP;
        if (vol < ADX_OUTVOL_MUTE) {
            vol = ADX_OUTVOL_MUTE;
        }
        ADXT_SetOutVol(File_AdxPlayer(ch)->adxt, vol);
    }
}

/* Switches every ADXT player between mono and stereo output. */
void Adx_SetMono(s32 mono) {
    ADXT_SetOutputMono(mono != 0);
}

/* Plays music file `id` on the BGM player. */
void Bgm_PlayEx(s32 id, s32 vol, s32 pan) {
#ifdef PORT
    /* Original game code and scripts pass the USA music constants. BT4's
       selection menu already passes the new disc file IDs. */
    if (PortGame_IsBT4() && id>=0x10B16 && id<0x10B16+250)
        id=PortGame_MusicFile(id-0x10B16);
#endif
    Adx_Play(ADX_CH_BGM, id, vol, pan);
}

/* Plays music file `id` at the default music volume, centred. */
void Bgm_Play(s32 id) {
    Bgm_PlayEx(id, 0x40, 0);
}

/* Stops the BGM player. */
void Bgm_Stop(void) {
    Adx_Stop(ADX_CH_BGM);
}

/* Sets the BGM volume. */
void Bgm_SetVolume(s32 vol) {
    Adx_SetVolume(ADX_CH_BGM, vol);
}

/* ADXT status of the BGM player. */
s32 Bgm_GetStat(void) {
    return Adx_GetStat(ADX_CH_BGM);
}

/* Returns 1 when the BGM player is stopped or finished. */
s32 Bgm_IsStopped(void) {
    return Adx_IsStopped(ADX_CH_BGM);
}

/* One fade-out step of the BGM player. */
void Bgm_FadeOutStep(void) {
    Adx_FadeOutStep(ADX_CH_BGM);
}

/* Plays file `id` on the second music player ("MAP"). */
void MapBgm_PlayEx(s32 id, s32 vol, s32 pan) {
    Adx_Play(ADX_CH_MAP, id, vol, pan);
}

/* Plays file `id` on the "MAP" player at the default music volume, centred. */
void MapBgm_Play(s32 id) {
    MapBgm_PlayEx(id, 0x40, 0);
}

/* Stops the "MAP" player. */
void MapBgm_Stop(void) {
    Adx_Stop(ADX_CH_MAP);
}

/* Sets the "MAP" player volume. */
void MapBgm_SetVolume(s32 vol) {
    Adx_SetVolume(ADX_CH_MAP, vol);
}

/* ADXT status of the "MAP" player. */
s32 MapBgm_GetStat(void) {
    return Adx_GetStat(ADX_CH_MAP);
}

/* Returns 1 when the "MAP" player is stopped or finished. */
s32 MapBgm_IsStopped(void) {
    return Adx_IsStopped(ADX_CH_MAP);
}

/* One fade-out step of the "MAP" player. */
void MapBgm_FadeOutStep(void) {
    Adx_FadeOutStep(ADX_CH_MAP);
}

/* Plays line `line` of character `chara` on voice player `voice`, from the voice set chosen in the options. */
void Voice_PlayCharaEx(s32 voice, s32 chara, s32 line, s32 vol, s32 pan) {
    s32 id;

#ifdef PORT
    id = PortGame_VoiceLine(chara,line,(gSaveData->flags & SAVE_FLAG_ALT_VOICE)!=0);
#else
    if (gSaveData->flags & SAVE_FLAG_ALT_VOICE) {
        id = VOICE_CHARA_BASE_ALT + (chara * VOICE_CHARA_LINES + line);
    } else {
        id = VOICE_CHARA_BASE + (chara * VOICE_CHARA_LINES + line);
    }
#endif
    Adx_Play(voice + ADX_CH_VOICE0, id, vol, pan);
}

/* Plays a character voice line at full volume, centred. */
void Voice_PlayChara(s32 voice, s32 chara, s32 line) {
    Voice_PlayCharaEx(voice, chara, line, ADX_VOL_MAX, 0);
}

/* Plays file `id` on voice player `voice` (0 or 1). */
void Voice_Play(s32 voice, s32 id, s32 vol, s32 pan) {
    Adx_Play(voice + ADX_CH_VOICE0, id, vol, pan);
}

/* Plays file `id` on a voice player at full volume, centred. */
void Voice_PlayDefault(s32 voice, s32 id) {
    Voice_Play(voice, id, ADX_VOL_MAX, 0);
}

/* Starts file `id` on a voice player in the paused state. */
void Voice_PlayPaused(s32 voice, s32 id, s32 vol, s32 pan) {
    Adx_PlayPaused(voice + ADX_CH_VOICE0, id, vol, pan);
}

/* Starts file `id` paused on a voice player at full volume, centred. */
void Voice_PlayPausedDefault(s32 voice, s32 id) {
    Voice_PlayPaused(voice, id, ADX_VOL_MAX, 0);
}

/* Takes a voice player out of pause. */
void Voice_Resume(s32 voice) {
    Adx_Resume(voice + ADX_CH_VOICE0);
}

/* Stops a voice player. */
void Voice_Stop(s32 voice) {
    Adx_Stop(voice + ADX_CH_VOICE0);
}

/* ADXT status of a voice player. */
s32 Voice_GetStat(s32 voice) {
    return Adx_GetStat(voice + ADX_CH_VOICE0);
}

/* Returns 1 when a voice player is stopped or finished. */
s32 Voice_IsStopped(s32 voice) {
    return Adx_IsStopped(voice + ADX_CH_VOICE0);
}

/* One fade-out step of a voice player. */
void Voice_FadeOutStep(s32 voice) {
    Adx_FadeOutStep(voice + ADX_CH_VOICE0);
}

/* Plays file `id` on streamed-SE player `se` (0 or 1). */
void StreamSe_Play(s32 se, s32 id, s32 vol, s32 pan) {
    Adx_Play((se < 2 ? se + ADX_CH_SE0 : 4 + PORT_FIGHTER_CAPACITY + se - 2), id, vol, pan);
}

/* Plays file `id` on a streamed-SE player at volume 0x40, centred. */
void StreamSe_PlayDefault(s32 se, s32 id) {
    StreamSe_Play(se, id, 0x40, 0);
}

/* Starts file `id` on a streamed-SE player in the paused state. */
void StreamSe_PlayPaused(s32 se, s32 id, s32 vol, s32 pan) {
    Adx_PlayPaused((se < 2 ? se + ADX_CH_SE0 : 4 + PORT_FIGHTER_CAPACITY + se - 2), id, vol, pan);
}

/* Starts file `id` paused on a streamed-SE player at volume 0x40, centred. */
void StreamSe_PlayPausedDefault(s32 se, s32 id) {
    StreamSe_PlayPaused(se, id, 0x40, 0);
}

/* Takes a streamed-SE player out of pause. */
void StreamSe_Resume(s32 se) {
    Adx_Resume((se < 2 ? se + ADX_CH_SE0 : 4 + PORT_FIGHTER_CAPACITY + se - 2));
}

/* Stops a streamed-SE player. */
void StreamSe_Stop(s32 se) {
    Adx_Stop((se < 2 ? se + ADX_CH_SE0 : 4 + PORT_FIGHTER_CAPACITY + se - 2));
}

/* ADXT status of a streamed-SE player. */
s32 StreamSe_GetStat(s32 se) {
    return Adx_GetStat((se < 2 ? se + ADX_CH_SE0 : 4 + PORT_FIGHTER_CAPACITY + se - 2));
}

/* Returns 1 when a streamed-SE player is stopped or finished. */
s32 StreamSe_IsStopped(s32 se) {
    return Adx_IsStopped((se < 2 ? se + ADX_CH_SE0 : 4 + PORT_FIGHTER_CAPACITY + se - 2));
}

/* One fade-out step of a streamed-SE player. */
void StreamSe_FadeOutStep(s32 se) {
    Adx_FadeOutStep((se < 2 ? se + ADX_CH_SE0 : 4 + PORT_FIGHTER_CAPACITY + se - 2));
}
