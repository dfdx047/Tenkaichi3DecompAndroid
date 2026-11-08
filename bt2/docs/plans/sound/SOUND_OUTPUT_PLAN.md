# Sound output

Make the game audible. Part of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step). Revised 2026-10-13 against the [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md) and the two BT3 references' sound code and notes.

## What the references say

Reference behaviour is adapted, never copied, and nothing about BT3 is assumed for BT2 without BT2 evidence (no addresses, formats, constants or code).

### Tenkaichi3Decomp (native port of the next game on this engine)

From its port log (`docs/port/README.md`) and the names of its port sources (`port/src/gs/snd_se.c`, `snd_adx.c`), snapshot `80e0dd42`:

| Topic | What it reports | Use here |
| --- | --- | --- |
| Effects boundary | `SOUNDS.IRX` is replaced behind the SIF RPC call, the SIF DMA upload and the sound driver's remote call. It captures banks into a 2 MB sound memory and keeps a per-frame command queue: play, stop by number, stop by handle, pause, stop bank, reset, mono, bank volume | The same boundary as our native SOUNDS and sound driver services. The command list is a checklist for decoding BT2's requests |
| What the original uses | Sony's sequencer and synthesizer modules (`modsesq2`, `modhsyn`); the play call takes port, sequence, loop, volume, pan, pitch | BT2's disc holds the same module names and the chunk names `Vagi`, `Setb`, `Sesq` (checked locally, 2026-10-13), so BT2 very likely uses the same stack |
| How little is needed | Over 361 bank files (18,924 effects): every effect is one note-on, timbre *i* uses sample *i*, all timbres share the same parameter bytes. Conclusion: a sampler is enough — PS2 ADPCM decoding, pitch, pan, 48 voices, one 48 kHz output | The target for BT2's effects, **if BT2's banks show the same regularity**. That survey is to be repeated on BT2's banks before the sampler is written |
| Sample data | Rates from 8,000 to 24,000 Hz, almost all 16,000; three samples loop | What to expect; BT2's banks decide |
| Left out there | The envelope's release (an 8 ms fade), the chip's interpolation, reverb | Candidates to leave out here too, but only where a comparison with the original shows no difference above the limits |
| Effects level | 0.25 of full scale, explained as three volumes of 100/127 and the chip's half-scale voice volume (0.244). Chosen with the user's ear: 0.55 was too loud | A lead for our levels, see below. Our stream level is measured, not tuned by ear |
| Music and voices | ADX players for music **and voices**, each on its own output stream: stop, pause, volume in 0.1 dB steps, pan per channel, mono, status. Files are plain ADX at 16, 24 or 48 kHz | Confirms that voices are expected on the stream path, and reads our stream commands: a volume in 0.1 dB steps and a pan per channel are what commands `5` and `9` look like |
| Verification | The ADX decoder against ffmpeg; effects by the user's ear only. Not verified: individual effects against the console, pitch accuracy, timed and looping effects | The same working check we use (the user's ear); our optional comparison step would go further |

### BT3-Recomp (recompilation of the next game)

Its runtime (`ps2xRuntime/src/lib`, snapshot `e22a34be`) routes the sound library's RPC calls to an audio backend (`ps2_iop_audio.cpp`), with a separate ADPCM decoder (`ps2_audio_vag.cpp`), a mixer (`ps2_audio.cpp`) and a host output (`ps2_host_audio.cpp`). Use here: the same separation — service handlers, a decoder, a mixer, a host device — in a recompilation project, which is the structure our `runtime/services/` and `runtime/host/` folders already follow.

### Behavioural references

| Reference | What it gives |
| --- | --- |
| PCSX2 SPU2 sources, pinned in the reference document (`RegTable.cpp`, `spu2sys.cpp`, `Mixer.cpp`) | The sound chip's behaviour: ADPCM blocks of eight halfwords giving 28 samples, 4.12 fixed-point pitch, end/repeat/loop-start flags, the volume chain from voice to master |
| PS2SDK `libsd` (`freesd.c`, `spu2regs.h`), pinned | Register layout and the library calls the game's sound driver is built on |
| Chronicle's PCSX2 practice | Every comparison records the emulator version, input hash, scenario and observations |
| This project's earlier sound work ([SPU2 voices](SPU2_VOICE_PLAN.md), [SPU2 initialization](SPU2_INITIALIZATION_PLAN.md), [sound driver RPC](SDRDRV_RPC_PLAN.md)) | Written while the IOP was emulated: which SPU2 registers the game's modules write, with comparisons against local PCSX2 captures. The code is gone; the observations about the original remain |

## Evidence from BT2 (2026-10-13)

- **Streams (movies, music):** the game's own CRI ADX library runs on the EE. It decodes there and uploads finished PCM to IOP memory, then announces each piece on a stream channel (command `0x100` {source, IOP address, bytes}). The IOP side only plays: 512 output samples per block at 48 kHz, 16-bit mono per source, two sources per player; a 24 kHz player uses half-size blocks. The native CRI service models this exactly, because the game paces movies and music on the "played" reports ([Sofdec plan](../video/SOFDEC_PLAN.md), `runtime/services/cri_audio.h`). So BT2 needs no native ADX decoder; the reference port needs one only because its game code was rebuilt for the PC.
- **Stream commands** (the report's `cri_stream_commands`; boot run and battle run):
  - `9` {object, channel, pan}: always channel 0 with −15 and channel 1 with +15, the ends of the ADX pan range, so channel 0 is left and channel 1 right.
  - `5` {object, value}: 0 in all 69 uses. Read as the ADX volume in 0.1 dB steps, the game plays every stream without attenuation.
  - `4` {object, rate}: 48000 and 24000. `2` {object, 0 or 1}: stop and start. `3` and `8` {object, 0 or 1}: meaning unknown.
  - The third argument word often holds leftovers of earlier records, so only the arguments named above are evidence.
- **Effects:** a separate path through the SOUNDS module and the sound driver (`runtime/services/sound_service.cpp`). The game uploads sound banks (SOUNDS `0x20` with two IOP blocks; 5 banks loaded by the battle) and requests sounds (`0x40` and others; 399 SOUNDS calls in the 9,000-frame battle run). The driver transfers blocks to sound RAM (`0x80D0`). The disc contains the module names `modhsyn` and `modsesq` and the chunk names `Vagi`, `Setb` and `Sesq`.
- **SOUNDS requests, decoded from the battle run's log** (`BT2_SOUND_LOG`; 256 calls in 7,000 frames):
  - `0x20` load a bank {sound memory address, header block, sequence block}. The sound driver's block transfer (`0x80D0` {.., IOP source, sound memory address, size}) puts the bank's samples at that address just before.
  - `0x30` unload: the bank loaded last goes. Banks form a stack: one system bank from boot, two more in the menus, four in a battle.
  - `0x40` play, 234 of the 256 calls: up to 15 sounds in parallel arrays of 0x20 bytes. Byte 0 is the count; per sound *k* (from 1) the bank slot at +*k*, the sound number at 0x20+*k*, the volume at 0x40+*k* and the pan at 0x60+*k* (both 0 to 127), and a 16-bit pitch offset at 0x80+2*k* (values seen: 0, 1, +200, −200; read as cents).
  - The bank slot is the bank's position in the stack, in the low five bits of its byte. The two bits above are flags (see the module's handlers below); 10 of 288 sounds carry one.
  - `0x50`, `0x100`, `0x200` (10 calls): see the module's handlers below. Their words beyond those named there hold leftovers of earlier requests.
- **The SOUNDS module's request handlers, read from the module's code** (the local `SOUNDS.IRX`, disassembled on 2026-10-13; a small unoptimized module that calls Sony's sequencer and synthesizer modules):
  - **Play (`0x40`):** the module keeps a table of 48 sounds that can be stopped later. Slot flag `0x80` puts the sound in the table (a full table drops the sound). Slot flag `0x40` does not play: it stops the table's sound with the same slot and number. Entries whose sound has ended are freed at each play request.
  - **Fade (`0x50`)** {direction, .., .., slot mask}: steps the volume of the masked slots from full (`0x100`) to zero when the direction word is set, from zero to full when it is clear, in four steps a millisecond apart. The game mutes slots 0 to 4 before it loads the battle's banks and restores them after.
  - **Volume (`0x70`)** {16-bit volume}: the same volume for all eight slots. Not requested in the runs so far.
  - **Silence (`0x200`)** {.., .., .., slot mask}: all sound off for the masked slots.
  - **Stop all (`0x100`):** stops every sound in the table, all sound off for all eight slots, clears the table.
  - **Unload (`0x30`)** only lowers the bank count. **`0x60`** selects the synthesizer's output mode (mono or stereo; recorded, the mix stays stereo). **`0x80`** {sound memory address, header block, sequence block, slot} loads a bank into an existing slot and keeps the bank count (the module's handler, read 2026-10-19); implemented, first reached by a story mode scene.
  - A slot is the synthesizer port its bank was loaded on, so the slot masks address banks.
  - The three looping samples are in the battle's common bank; the run requests one of them (sound 22) with the keep flag and stops it with the stop flag.
- **Bank layout, read from the eight banks the run loads:** Sony's chunked sound data. The header block has `SCEI` chunks `Vers`, `Head`, `Vagi` (per sample: offset, sampling rate, loop flag) and `Setb` (timbres of 0x32 bytes: sample index, volume, pan). The sequence block has `Vers`, `Sequ` and `Sesq` (per sound: volume, pan and seven data bytes).
- **Bank survey:** 233 sounds in the eight banks. Every one is a single note-on at time zero naming one timbre, and every timbre's parameters are identical (volume 100, pan centre). Sequence volume and note velocity are 100, the set volume 127. Rates are 11,000, 11,025, 16,000, 24,000 and 48,000 Hz. Three samples loop. This is the regular shape the reference port found in BT3, so a sampler without a sequencer is enough.
- **Comparison with the original** (a PCSX2 recording by the user: boot without input through both movies, against the port's boot run; `tools/audio/compare_audio.py`):
  - Timing: constant offset over each movie, no drift over 80 s.
  - Channels: the left/right balance tracks the recording with correlation 0.994.
  - Level: the unscaled PCM was 2.53 to 2.58 times too loud at every frequency, a level of 0.388 to 0.395. With a stream voice output at 25/64 (0.391) the RMS ratio is 1.00 to 1.08 and the peaks agree.
  - Waveform: alike in short windows only (coherence about 0.97, sample correlation about 0.5), as a time-stretched recording would be. Not settled.
  - The recording's emulator version and audio settings were not noted. Later references record them.
- **A lead for the stream level:** one volume of 100/127 times the chip's half-scale voice volume is 0.394, inside the measured range; the reference port explains its effects level with the same two factors (three such volumes there). This is a hypothesis: it is confirmed only when the values the original's modules write are read (the optional comparison step).
- **The user's listening checks:** before the level correction, music too loud, voices too quiet, effects too loud (stream audio only at that time); after it, better. On 2026-10-13 the user listened to the battle run with the one-shot effects (menus and a fight): it sounds good and the effects are present.
- **Not known:** where the original sets the stream level; whether BT2's voices are streams, as in the reference port; the pan law and the envelope the original applies (a stopped sound's release in particular).

## Design

Sound is produced where the game asks for it. Two producers feed one mixer.

1. **Streams — done.** The CRI flow passes on each stretch of PCM a playing player consumed (`on_audible`). Data that a stop only reports back, and data of a player destroyed while playing, is not heard. Each source is a mixer voice: mono, routed left (the player's first buffer) or right (its second), upsampled to 48 kHz by an integer factor with linear interpolation, at 25/64 of its PCM level. A voice starts with one block of silence, because data arrives in 512-sample blocks while a field takes 800.8 samples; with that its queue cannot run dry while the player plays. Still to add when a run needs them: a stream's volume and pan other than the values seen so far (commands `5` and `9`), refused by name until then.
2. **Effects — done:** a native sampler behind SOUNDS and the sound driver, the shape both BT3 references use (`runtime/services/sound_sampler.h`, `sound_bank.h`, `spu_adpcm.h`).
   - **Sound memory:** 2 MB; the driver's block transfers fill it.
   - **Banks:** parsed at load into sounds (sample offset, rate, loop flag, the volumes and pans along the way). A bank outside the regular shape is refused by name at the load, so nothing plays wrongly.
   - **Play:** each sound of a request starts a voice. The sample is decoded from sound memory (SPU2 ADPCM) and played at its rate, shifted by the pitch offset in cents, with linear interpolation. A sample that ends with a repeat flag loops from its loop start. Up to 48 voices; the oldest gives way.
   - **Level:** the product of the request's volume, the set's, the sequence's, the note's and the timbre's (each of 127), the chip's half-scale voice volume and the slot's volume (of `0x100`, set by the fade and volume requests). At a request volume of 127 and a full slot that is 0.244, the reference port's figure. The pan is constant-power, an assumption.
   - **Stopping:** the keep and stop flags, the silence request and the stop-all request end voices as the module's handlers do. A stopped voice fades out over 10 ms; that stands in for the release of the envelope, which is not modelled.
   - **Not modelled:** the envelope, reverb, the chip's interpolation. Refused by name until a run needs them: requests `0x60` and `0x80`, a request that keeps and stops at once.
   - The sampler does not emulate the sequencer and synthesizer modules or their voice allocation; nothing the game can observe depends on them so far.
3. **Mixer** (`runtime/services/audio_mixer.h`): 48 kHz stereo, one field of output per VBLANK (800.8 samples, the fraction carried), saturating. Streams and sampler voices add here. Streams use the measured level; effects start from the level the expected volume chain gives and are adjusted with the user's listening.
4. **Output:** the runtime hands each field's samples to `on_audio_field`. The probe writes them to a WAV file (`BT2_AUDIO_WAV=<path>`) and plays them on the default audio device (`--sound`, `BT2_AUDIO_DEVICE`; `runtime/host/audio_device.h`, Win32 waveOut). The run is paced by the clock (`runtime/host/frame_pacer.h`, see the [performance plan](../ee/PERFORMANCE_PLAN.md)); the pacer lengthens or shortens the field period by up to 1% to keep the device's queue near 50 ms. Pacing on the device's queue itself (the first version) made pictures arrive unevenly. A run that is slower lets the device run dry, heard as gaps and counted. Not done: user volume settings for music and effects, as the reference port has.

## Order of work

1. **Done:** streams, one-shot effects (heard by the user), looping effects and the stop, fade and silence requests, the host audio device.
2. **The user listens** to the new `work/audio/battle-run.wav`: do the looping sounds in the fight start and end where they should?
3. **The user plays with sound** (`run_startup_probe.py --viewer --sound`): is the sound in step with the picture where the run keeps the game's speed?
4. **Later:** volume settings; gaps in fights end with the performance work (the fight runs below the game's speed).
5. **Optional, at the end: comparison with the original** (see Verification). It includes explaining the stream level from the original's settings and the byte-for-byte check of stream PCM.

## Verification

**Working check (what each step needs to count as done):** unit tests for the code, the run's counters, and the user's listening to the WAV files in `work/audio/`. This is how sound moves forward; a missing comparison with the original does not hold a step back. What the user heard is recorded as the user's judgement, kept apart from anything measured.

**So far:**
- Unit tests: the flow's audible stretches (stop and destroy cases), the mixer (start silence, interpolation, routing, level, field length, saturation, a dry voice, refused rates, extra sources), the ADPCM decoder, the bank parser on a synthetic bank, the sampler (level, pan, pitch, voice end, looping, keep and stop, slot volume, silencing, refusals, unload), the comparison tool.
- Measured (9,000-frame battle run): 14,334 fields give 239.1 s of audio from 29.4 MB of stream PCM. 457 effects played, at most 16 voices at once, none dropped. 9 sounds were kept and each was followed by its stop request: 7 stops ended a playing voice, 2 came after the sound had ended by itself. The looping sound played 3 times and was stopped each time. The silence requests ended 2 voices.
- The user's listening (2026-10-13, the earlier 7,000-frame run with one-shot effects): sounds good, effects present. The looping and stopped sounds have not been listened to. Nothing of the effects is compared with the original.
- Audio device, measured: a standalone check queued 120 fields (2.0 s) in 1.93 s, so the device paces the writer. A 900-frame boot run with the device played all 1,283 fields without running dry and waited for the device 1,301 times, so boot and movies run faster than the game's speed and are held to it. Not measured with the device: menus and the fight (the fight runs at about two thirds of the game's speed, so gaps are expected there). The user has not yet listened live.
- Against a PCSX2 recording, for the two boot movies: timing, channel order and level agree (see the evidence above); the waveform is not shown identical.
- The user's listening: recognizable; better after the level correction; accuracy not something the ear can confirm.

### Optional final step: comparison with the original

Decided by the user on 2026-10-13: judging sound by comparison instead of by ear is worth doing, but as a final step, not as a condition for progress. When it is taken up, each stage of the sound path is compared where an exact reference exists:

| Stage | What is compared | Reference in the original | Exact? |
| --- | --- | --- | --- |
| Requests | The stream commands and SOUNDS calls the game makes: kind, arguments, order (timing within a tolerance, since loading time differs) | PCSX2 call logs (`capture_ee.py --sounds-log`, `--cri-stream-log`) | Yes |
| Sample data | The PCM the game uploads for streams; the bank blocks it uploads for effects | PCSX2 IOP memory at the same point (the `rpc-cri-stream-reply` capture point holds a stream's address and size) | Byte for byte |
| Voice parameters | Per started voice: pitch, left and right volume, envelope settings; the master volume. This is also where the measured stream level of 25/64 would be replaced by the original's settings | The SPU2 registers read from PCSX2 when a voice is keyed on (a capture to be added); their meaning from the pinned PCSX2 SPU2 sources and PS2SDK `spu2regs.h` | Yes |
| Output | The mixed signal, with `tools/audio/compare_audio.py` | A PCSX2 recording of the same stretch | Within the limits below |

- **A usable output recording:** PCSX2's audio synchronisation set to none (no time stretching); the same input on both sides (nothing for input-free stretches, otherwise one input recording played back in PCSX2 and converted to a pad script, with a converter to be written); logged with the PCSX2 version, its audio settings, the disc hash and the scenario.
- **Output limits** (starting values): offset drift below 5 ms over the stretch; level within 1 dB per frequency band; short-window coherence above 0.95 where the signal is not silent; for effects, onset time, level and pitch of each detected sound. The output cannot be bit-exact: the chip resamples and interpolates, and its reverb is not modelled.
- Captures and recordings are proprietary-derived and stay local; documents record numbers only.

Status does not wait for this step: sound that works in the runs and that the user has heard is marked Operational, like the other subsystems.

## Stop conditions

A sampling rate that does not divide 48 kHz, PCM outside IOP memory, a bank layout or a SOUNDS request whose meaning BT2's evidence does not settle: stop at a checked boundary, or stay silent and say so. No invented sound, and no BT3 constant used without BT2 evidence. A level set with the user's ear is allowed and is recorded as such.

**One-channel streams (2026-10-20).** A stream's player has two buffers with a source each. Music and films give both sources data; speech (the fight's announcer) gives data to one only. Such a source is heard in both ears (`cri_audio.h`, `paired`: a partner that received data within the last 60 fields makes a pair). Before, it was heard on its buffer's side only: the user heard voices on the left only. Checked with recordings of a fight, the Ultimate Battle Z screens and story mode (no one-sided 0.1-second stretch; left and right within 1%).
