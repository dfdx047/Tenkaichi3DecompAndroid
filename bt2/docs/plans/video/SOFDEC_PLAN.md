# Opening movie: Sofdec video and ADX audio

Play the game's Sofdec movies natively. Step 5 of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step). Audio output follows the [native services plan](../iop/NATIVE_SERVICES_PLAN.md) (sound and CRI services).

## Evidence (2026-10-11)

- **Files** (ISO, `DATA/`): `LOGO_O.SFD` (2.9 MB), `ZS2USOP.SFD` (opening, 97 MB), `ZS2USED.SFD` (ending, 162 MB). Audio streaming uses the IOP modules `CRI_ADXI.IRX` and `CDVDSTM.IRX`, both replaced by the native CRI service.
- **Video:** the game sets up the IPU (decoder tables, thresholds, reset) before the movie; no IPU decoding command has been reached yet (`runtime/ee/ipu_device.*`).
- **Current stop:** after GIF chain mode, startup runs to 1,259 entered functions and stops at the native CRI service's checked boundary (selector `0x14`, pc `0x32EB88`): the EE's CRI stream buffer (IOP `0x191740`) carries its first commands for the object at `0x1B5E2C`:

  | Offset | Words |
  | --- | --- |
  | `+0x00` | `2` (command count, presumably) |
  | `+0x10` | `8`, `0x1B5E2C`, `1` |
  | `+0x20` | `5`, `0x1B5E2C`, …, `0x65` (at `+0x2C`) |

  Until then every returned stream buffer was empty (sequence counter only). The IOP's answer was captured on 2026-10-12 (see Progress).
- **Capture:** PCSX2 and the BIOS used by the existing captures are available locally, so the original's answer can be captured with `capture_ee.py`.

## Progress (2026-10-12)

- **CRI stream replies:** captured in PCSX2 (`capture_ee.py --capture-point rpc-cri-stream-reply`, unattended: scripted LEFT/CROSS presses answer the absent-card prompt). The IOP returns the buffer with the command count cleared and the records unchanged; the native CRI service now does the same for commands 8 and 5 (`runtime/services/cri_stream.h`) and refuses other codes.
- **Movie path:** the run then reaches 1,590 functions and stops at the first **toIPU DMA** (channel 4, `0x1000B400`, pc `0x2CB384`): the game's own Sofdec library feeds the IPU. Step 3's choice is therefore **native IPU commands** (the game code stays unchanged).
- **Next:** write `docs/plans/video/IPU_PLAN.md` (toIPU/fromIPU DMA, the IPU commands the movie issues, MPEG-1/2 macroblock decoding and CSC), then implement it.

## Progress (2026-10-12, later)

- **More stream commands captured** (`capture_ee.py --capture-point rpc-cri-stream-reply` with the new `--cri-stream-count`, `--cri-stream-code`, `--cri-stream-channel` and `--cri-stream-delay` triggers). All are answered like 8 and 5: the count is cleared and the records are returned unchanged.

  | Channel (EE buffer) | Commands | Meaning (from the arguments) |
  | --- | --- | --- |
  | `0x35FD00` | 8, 4 (`0xBB80`, `0x65`), 9 twice (`0`, `-15`; `1`, `15`) | Audio start: 48,000 Hz, per-channel pan |
  | `0x365BC0` | `0x100` twice ({object, IOP address, `0xA00`}) | Audio data: 2,560 bytes per audio channel written to IOP memory |
  | `0x35FD00` | 2 ({object, 1}) | Start playback, presumably |

- **Presentation:** while a movie plays the game does not call the library flip; it rewrites DISPFB/DISPLAY for both circuits every field. The runtime now counts a frame at the library flip or at the VBLANK ending a field with such an update, and bounds time without a frame (3,600 fields).
- **Current stop:** three pictures are decoded and presented, the audio stream has started (21 commands), then the movie player issues no further IPU command. A capture five seconds into playback (`--cri-stream-delay 300`) shows no status in either stream buffer (only the sequence counters advance), while the EE object next to the second channel (`0x366458`...) holds advancing counters and an IOP read position (`0x17A6AE`).
- **Open:** where the EE obtains audio playback progress (a third transfer from the IOP, an RPC, or its own VBLANK-based estimate) and which value releases the next picture. That decides what the native CRI service must supply.

## Progress (2026-10-12, audio flow)

- **How progress reaches the EE:** the buffer the IOP returns is itself a message list: word 0 counts the IOP's messages and each record is {0, EE object, IOP address, bytes}. The reply handler (`0x3068E0`) dispatches them to the EE object. The earlier captures showed count 0 only because nothing was playing yet.
- **Logged in PCSX2** (`capture_ee.py --cri-stream-log`, which records every non-empty upload and reply between a flagged upload and the halt; read with `read_cri_stream_log.py`):
  - Each field the EE announces the PCM it wrote to the two sources' ring buffers (`0x100` {source, address, bytes}; rings of `0x4000` bytes).
  - From the field after the start command (2 {player, 1}), each returned buffer reports the played data per source, in write order, in blocks of `0x400` bytes: 2, 1, 2, 1, 2, 1, 2, 2, 1, ... blocks per field, i.e. 512-sample blocks at 48 kHz against 800.8 samples per field. A stretch that crosses the ring end is reported as two messages.
  - The stop command (2 {player, 0}) is followed, one field later, by a report of all data still announced.
  - Between the two movies the player is destroyed (RPC `0x409` {player}) and recreated with the same handle.
- **Native model** (`runtime/services/cri_audio.h`): sources (RPC `0x400`, linked to their buffer object and EE object), players over two buffer objects (RPC `0x408`), a logical 48 kHz clock per playing player (one field per VBLANK, phase as logged), and the reports above. A player without announced data reports nothing. No sound is produced.
- **Result:** the logo movie (151 pictures) and the opening movie (3,236 pictures) play to the end. The run then reaches the SOUNDS service with function `0x50`, not yet captured.
- **Movie leader frame:** the logo movie's first picture is a flat green field in a black letterbox; ffmpeg decodes the same picture from `LOGO_O.SFD`. It is part of the stream, not a decoding error.

## Approach

1. **CRI stream replies (done 2026-10-12):** capture EE and IOP RAM in PCSX2 at the first VBlank after these commands are uploaded, and at the next point the EE reads the returned buffer. Derive what the returned buffer holds for commands 8 and 5 (object state, play position, status). Implement those replies in `runtime/services/cri_service.cpp`, one test per command, and keep unknown commands at the checked boundary.
2. **Find the movie path (done: the IPU):** run to the next boundary and record whether the movie reaches IPU commands (EE-side Sofdec decoding through the IPU) and which EE library functions drive it (neutral names).
3. **Choose the boundary** from that evidence:
   - **IPU commands natively** (MPEG-1/2 macroblock decoding, CSC) if the game's own Sofdec library drives the IPU directly: smallest change, game code unchanged.
   - **Replace the Sofdec library entry points** through the function table with a native SFD demuxer and decoder, if that boundary is clear and the IPU path is costly.
4. **ADX audio:** decode ADX natively (CRI ADPCM) behind the CRI service and play it through a host audio backend; keep the stream timing the game observes (returned buffers, positions).

## Verification

- Unit tests per CRI command reply and per IPU command (or per replaced library call).
- Compare EE-visible state with PCSX2 captures at the movie start; compare decoded frames with PCSX2 frame captures.
- The run reaches the title screen with the movie shown, or skipped through the game's own skip path with identical state.

## Stop conditions

A reply or decoding step that cannot be derived from captures or public format knowledge stops at a checked boundary. Never fabricate stream status.
