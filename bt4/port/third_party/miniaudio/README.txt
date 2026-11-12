miniaudio.h: the audio device, mixing engine and (mp3, wav, flac) decoders of miniaudio, by David Reid.
Used by port/src/gs/snd_ma.c: a song or sound mod from outside the disc that is not the game's own ADX is played
here as it is, instead of being converted to ADX first.

Origin
  Repository  https://github.com/mackron/miniaudio
  Version     0.11.25 (the release tarball, the version the file's own MINIAUDIO_VERSION_STRING names)
  File        miniaudio.h, unchanged.

What is used
  ma_engine (its own device, beside SDL's for the ADX streams), ma_decoder (one per stream) and ma_sound.
  The build options are set in port/src/gs/snd_ma.c, before the include (manual, section 2.7):
    MA_NO_ENCODING           nothing is written
    MA_NO_GENERATION         no test waveforms
    MA_NO_RESOURCE_MANAGER   a stream is decoded by the ma_decoder this program owns; the default manager
                             would decode through the engine's job thread and add the manager itself
  WAV, FLAC and MP3 are kept: they are the formats a mod is made of.

Licence
  Public domain (Unlicense) or MIT No Attribution, a choice of the two, as written at the top of miniaudio.h.
