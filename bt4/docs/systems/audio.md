# Audio

Sources: `src/sys/adx.c` (streamed audio), `src/sys/snd.c` (sound effects), `src/sys/file.c`
(player creation). Layouts: `include/sys/adx.h`, `include/sys/snd.h`.

There are two independent paths: streamed audio through CRI ADX, and sample-based sound effects
through the game's own I/O-processor driver `SOUNDS.IRX`.

## Streamed audio: ADX (verified)

Six ADXT players (2 channels each, server frequency 30), created at startup:

| Player | Label | Wrappers | Default volume |
|---|---|---|---|
| 0 | `BGM` | `Bgm_*` | 0x40 |
| 1 | `MAP` | `MapBgm_*` (prefix is a guess: a second music stream) | 0x40 |
| 2, 3 | `SE ` | `StreamSe_*` (prefix is a guess) | 0x40 |
| 4, 5 | `VIC` | `Voice_*` | 0x80 |

- Volume: game range 0..0x80. Linear value `vol * gain[player] * save volume / 0x480`, clamped to
  0..0x80, with gains `{0x3C, 0x58, 0x60, 0x80, 0x46, 0x46}` (0x80 = unity). Players 0-1 use the
  save's music volume, the rest the sound-effect volume (both 0..9).
- Output level: 0 maps to -960 (mute); otherwise `sin((0x80 - v) / 128)^2 * 0.28 * -960`. That
  the unit is 0.1 dB is inferred.
- Pan: -0x40..0x40, converted as `(pan - 1) / 4`.
- `Adx_Play` sets pan and volume, unpauses, then starts the file by id. On players 4 and 5 it
  returns without playing when `func_00259E20(voice, 0)` or `(voice, 1)` is non-zero (a per-side
  mute; what sets it is unknown).
- `*_PlayPaused` starts paused and `*_Resume` releases; `Adx_FadeOutStep` lowers the level by 120
  per call down to -960; `Adx_StopAll` stops all six and pumps the CRI main loop until every
  status is 0.
- Character voice lines: file id `chara * 100 + line + 0x8D4E`, or `+ 0xCC32` when bit 0 of the
  save flags is set (see save_data.md).

For a port: the CRI ADXT library these calls name is replaced by the port's own (`port/src/gs/snd_adx.c`: an ADX
decoder feeding one SDL audio stream per player). A song or sound mod from outside the disc in mp3, wav or flac is
played as it is instead, by miniaudio (`port/src/gs/snd_ma.c`); the file is told apart from the game's own ADX by
its first bytes, so the whole of the disc stays on the ADX path. See docs/port/adding_songs.md.

## Sound effects: SOUNDS.IRX (game side verified; what the driver does is inferred)

### Model

- Eight bank slots, named by mask `1 << slot`. A sound id is the sample index inside its bank,
  0..255.
- Bank file: five words `{?, bodyOfs, hdOfs, seqOfs, endOfs}` then three sections: ADPCM sample
  body (uploaded to sound RAM), a Sony HD header and a third section of unknown content (both
  copied to I/O-processor memory).
- Mask 1 is the common bank (file 0x14B, loaded at boot). Masks 4, 8, 0x10 and 0x20 are loaded
  for a battle; 0x10 and 0x20 are the two sides' voice banks and are replaced in place when a
  character changes.
- One bank uploads at a time; a bank still pending refuses to play.

### Per-frame command queue

Play and stop never talk to the driver directly. They append a 12-byte `SndCmd` to a 32-entry
queue; a 33rd is dropped and the play returns -1. `Snd_Update` sends the queue once per frame,
so a sound starts on the following frame.

`SndCmd`: `u8 type` (0 play, 1 stop by bank+id, 2 stop by handle), `u8 loop`, `u16 handle`,
`u8 bank mask`, `u8 id`, `u8 volume`, `u8 pan`, `s32 pitch` in cents. The handle is a counter
generated on the game side and is what `Snd_PlaySeEx` returns.

`Snd_PlaySeEx(mask, id, volume, pan, pitch)`:
- volume: `volume * bankVolume[slot] * save seVolume / 0x480`, clamped 0..0x7F; bank volumes
  `{0xD8, 0xE0, 0x80, 0x80, 0x80, 0x80, 0x80, 0}`
- pan: `pan + 0x40`, clamped 0..0x7F
- pitch: `pitch * 18.75` cents, clamped to +-1200 (64 units per octave)
- returns -1 when the bank is not loaded or pending, when the mask is 0x10 or 0x20 and the
  per-side mute is set, when the queue is full, or when an argument is out of range.

`Snd_PlaySe(mask, id)` is `Snd_PlaySeEx(mask, id, 0x40, 0, 0)`.

### RPC protocol

One SIF RPC client bound to server id 0x2000004. Every call blocks. Send buffer 0x188 bytes.

| fno | Size | Payload | Use |
|---|---|---|---|
| 0x0 | 4 | | init |
| 0x1 | 4 | | term (never called) |
| 0x2 | 4 | | reset / stop all |
| 0x3 | 0x10 | mask, spuAddr, hdIopAddr, seqIopAddr | bank data is in place |
| 0x4 | 4 | mask | bank freed |
| 0x5 | 4 | 0 stereo / 1 mono | |
| 0x6 | 8 | mask, on | pause / resume (battle pause sends 4,1 and 4,0) |
| 0x7 | 8 | mask, volume | only sent as (0xFF, 0x7F) at init |
| 0x9 | 4 | | per-frame tick, sent before the queue |
| 0xA | 4 | mask | stop the voices of those banks |
| 0xD | 0x184 | `SndQueue` | the per-frame commands |
| 0xF | 0x18 | count, ids | both sides' fighter ids, every battle frame |
| 0x8, 0xB, 0xC, 0xE | | | never sent |

Sample upload does not go through SOUNDS.IRX: it uses Sony's libsdr remote calls directly.

### For a port

The sound-effect layer is a thin command protocol, so it can be replaced by a mixer that
consumes the same queued commands. Not yet known: the HD header format, the third bank section,
and why the driver is sent the fighter ids every frame.

## Sound RAM allocator (`src/sys/spu_heap.c`; verified)

Sample bodies are placed in sound RAM by a game-side address allocator covering
0x5010..0x1F5FE0: a table of 17 descriptors `{state, addr, size}`, first fit in address order,
16-byte rounding, merge on free. There is no failure path (exhaustion dereferences NULL). It is
bookkeeping only, so a port's mixer does not need it. I/O-processor memory for bank headers
comes from bare wrappers of Sony's IOP heap calls (`src/sys/iop_heap.c`).

Correction: `Snd_SendFighters` (command 0xF) sends each side's four live sound handles, not
fighter ids (`BtlCharApi_GetSoundCount` / `GetSound`). The name is misleading.

## Movie sound (verified)

A movie's sound is a separate `.ADX` file played on ADX player 0 at volume 0x40, started paused
and released just before the first video frame. See graphics.md for the video side.

## CRI ADX library extent (found 2026-10-04)

(verified by strings) 0x269228..0x26C050 is CRI ADX `adx_fs.c` (the ADXF file layer), library
version "ADXT/PS2EE Ver.9.71 Build:Dec 13 2005 21:36:32", built with "GCC2096 SCE3020"; error
strings carry the function names. 82 functions named in config/symbols/cri_adxf.txt (many marked
guess), none decompiled: it is third-party library code. (inferred) 0x26C050..0x273CA0 is the
rest of the same library (lock pair, ADXT, ADXSJD, ADXSTM), so the game code of that assembly
range ends at 0x269228. Three names that were on the wrong half of a lock / worker pair are
fixed: 0x26B580 `ADXF_Tell` / 0x26B5B8 `adxf_Tell`; 0x26BE30 `ADXF_SetReqRdSct` / 0x26BE70
`adxf_SetReqRdSct`; 0x269EE0 `ADXF_GetPtStat` / 0x269F18 `adxf_GetPtStat` (a bare tail call) and
0x269F30 `ADXF_GetPtStatEx` / 0x269F68 `adxf_GetPtStatEx` (the real worker; its error strings
say "(adxf_GetPtStat)", so the Ex name is a guess from the pairing).
