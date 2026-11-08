# Native IPU

Decode the opening movie's MPEG video with a native implementation of the IPU's commands, driven unchanged by the game's own Sofdec library. Part of the [Sofdec plan](SOFDEC_PLAN.md) and the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step).

## Evidence (2026-10-12)

- **Setup is modelled:** `runtime/ee/ipu_device.*` handles the registers, the input FIFO and the setup commands (BCLR, SETIQ, SETVQ, SETTH). Decode commands and IPU DMA starts are refused.
- **Current stop:** the first toIPU start (channel 4) at pc `0x2CB384`: CHCR `0x30000105` (source chain from memory; the upper half is the read-only TAG field carried back), TADR `0xBC9880`. The chain is a ring of REF tags, each pointing at a 2 KB bitstream chunk (`0xB49880`, `0xB4A080`, …).
- **The stream:** the first chunk starts with an MPEG sequence header (`00 00 01 B3`): 512×448, frame-rate code 4 (29.97 Hz), with loaded quantiser matrices. Sofdec parses headers itself through the IPU's bit-level commands.

## Progress (2026-10-12)

- **Done (order step 1, input side):** `runtime/ee/ipu_input.*` pulls toIPU source chains on demand into the FIFO and bit buffer (IPU_BP with BP/IFC/FP, IPU_TOP); FDEC; commands without enough input stay pending with BUSY set and complete when a transfer or FIFO write supplies it. Tests in `bt2-ipu-tests` (chain pacing, FDEC across tags, the BUSY wait).
- **Map:** the movie code reached a function whose jump-table cases Ghidra had split off; the candidate export now keeps functions whose holes no other function owns, and the switch prover accepts calls inside case blocks and a last block falling through into the following span (four more functions proven, 9,504 mapped).
- **Also:** a running toIPU channel keeps the FIFO full (filled on start and after every consumption), and clearing STR stops it with MADR/QWC/TADR and the FIFO kept (the movie stops and restarts the chain); 64-bit reads of IPU registers return BUSY in bit 63.
- **Reached:** the movie starts the chain and parses the MPEG sequence header with FDEC (start code, then 32, 32 and 30 bits). It then issues no further command; its decoder thread (7) polls IPU_BP, IPU_CMD and IPU_CTRL (ECD, bit 14) in the wait loop at `0x31A3C8` until the probe's execution budget ends.
- **Next:** find what the wait loop at `0x31A3C8` expects after the header (likely the next FDEC field, an end-code condition or the IPU interrupt), then VDEC and the macroblock commands.

## Progress (2026-10-12, later)

- **The wait was a recompiler defect:** the game reads IPU_CMD as 64 bits and tests BUSY (bit 63) with `bgezl`; the generator emits 32-bit sign tests for BLTZ/BGEZ/BLEZ/BGTZ, so bit 31 of the FDEC result was read as BUSY. `prepare_runtime.py` now widens every such branch to the full register (491 branches in the prepared sources), with a tooling test.
- **Stream:** MPEG-2 (IDP 2, non-linear quantiser scale, intra VLC table one), not MPEG-1. Sofdec loads the quantiser matrix from the sequence header with SETIQ at an unaligned bit position.
- **Commands now native** (`runtime/ee/ipu_vlc.*`, `ipu_block.*`, `ipu_csc.*`, `ipu_device.cpp`):
  - SETIQ/SETVQ read at the bit pointer; SETIQ skips FB bits first.
  - VDEC: macroblock address increment (with escape and MPEG-1 stuffing), macroblock type (I, P, B), motion code, DMV; ECD on an invalid code.
  - BDEC: coded block pattern, DC size and coefficient codes (tables B-9, B-12 to B-15), inverse quantisation, a double-precision inverse DCT, RAW16 output (48 quadwords); then the search for a following start code (SCD/ECD).
  - CSC: RAW8 macroblocks to RGB32 with the SETTH alpha thresholds, resumable per macroblock across input and output.
  - Output FIFO and fromIPU DMA (channel 3, normal mode) to main RAM or the scratchpad; IPU_CTRL OFC, ECD, SCD; writes to read-only IPU_CTRL fields are ignored.
- **Reported words follow PCSX2's IPU where the public documentation is silent** (VDEC result layout: the code length only for B-picture macroblock types and motion codes, the frame-motion flag `0x80`, sign-extended motion codes). Deviations from that reference, chosen from the MPEG specification: MPEG-1 results are made odd for every coefficient; no fast path for DC-only blocks. MPEG-2 mismatch control is not applied (as in the reference).
- **Code tables** are checked at compile time (complete prefix codes) and table B-15 was cross-checked against the reference's lookup tables.
- **Reference:** PCSX2 `pcsx2/IPU/` at the pinned revision `81526d4dc7cc70e4ae75abb35a789417456c6d43` (kept in ignored `work/references/pcsx2-ipu/`), read for behaviour only.
- **Around the IPU:** toSPR DMA in source-chain mode; MMI instructions the movie's motion compensation uses (PADDH, PMAXH, PMINH, PPACB, PEXTLB, PEXTUB, QFSRV) and the trapping ADD/ADDI; GIF IMAGE data continuing in later transfers.
- **Reached:** the movie decodes an I, a P and a B picture (1,088 coded macroblocks, three CSC passes of 896), uploads 16×448 strips straight to the displayed framebuffer and presents them every field. Then no further IPU command is issued: the player waits for audio playback progress ([Sofdec plan](SOFDEC_PLAN.md#progress-2026-10-12-later)).
- **Not yet:** IDEC, PACK, RGB16 output with dithering, resuming BDEC when the input ends inside a macroblock (refused by name); Release decode timing.
- **Later the same day:** with the native audio flow ([Sofdec plan](SOFDEC_PLAN.md#progress-2026-10-12-audio-flow)) both movies decode completely: 3,387 pictures, 2.46 million BDEC macroblocks, 10.1 million VDEC codes, P and B pictures through the game's own motion compensation (one more MMI helper, PCGTH). Spot checks: the logo's first picture equals ffmpeg's decode within one colour step, a mid-logo frame shows the logo of a PCSX2 memory capture, the opening's first picture matches ffmpeg's. A frame-by-frame comparison against ffmpeg (local, from the user's disc image) is the next verification step.

## IPU behavior to implement

Public IPU semantics (register layout, command set, MPEG-1/2 decoding from ISO/IEC 11172-2 and 13818-2). Adapt reference behavior; do not copy PCSX2's or libmpeg2's code.

| Part | Behavior |
| --- | --- |
| Input | toIPU DMA (normal and source chain, via `runtime/ee/dma_chain.*`) feeds the 8-quadword input FIFO **on demand**: as commands consume bits, the next quadwords are taken from the running transfer. CHCR STR, MADR, QWC and TADR advance as on hardware, so the game sees the same refill points. |
| Bit reader | IPU_BP (bit pointer, FIFO quadword count, FP), IPU_TOP (peek of the next 32 bits), BCLR. |
| FDEC / VDEC | Fixed-length and variable-length header fields: macroblock address increment, macroblock type (I, P, B; MPEG-1 and MPEG-2 tables), motion code, DMV, coded block pattern. Results in IPU_CMD. |
| IDEC / BDEC | Intra and block decoding: DC/AC coefficient VLC (tables zero and one), inverse quantisation with the SETIQ matrices and QSC, alternate scan, IDCT, saturation; output RAW8/RAW16 blocks or, for IDEC, CSC to RGB32/RGB16 with optional dithering (DTE) and the threshold alpha (SETTH). |
| CSC / PACK | Colour conversion of RAW8 macroblocks to RGB32/RGB16 and the 4-bit indexed PACK through the SETVQ table. |
| Output | fromIPU DMA (channel 3, normal mode) drains the output FIFO to main RAM. |
| Status | Commands complete synchronously (BUSY never observed set); ECD/SCD and error bits in IPU_CTRL as the stream defines them; the IPU interrupt only if the game enables it. |

## Order

1. **toIPU input and the bit reader:** on-demand FIFO filling from the chain, IPU_BP/IPU_TOP, then FDEC and VDEC with the header tables, until the game reaches its first macroblock command. Unit tests with synthetic bitstreams.
2. **Macroblock decoding:** the command the movie uses first (IDEC or BDEC), with IDCT and dequantisation, and fromIPU output. Tests against reference blocks computed from the MPEG specification.
3. **CSC / PACK** as reached.
4. **Frames:** compare decoded movie frames with PCSX2 frame captures (visual first, then per-pixel where the IPU is exact).

Each refused command or mode names itself (command code, field) at a checked boundary.

## Verification

- Unit tests per command and VLC table, with synthetic streams (no game data).
- The run advances through the movie; the presented frames match PCSX2 captures.
- Release timing of the decode on the movie (time per frame), reported separately from startup.

## Stop conditions

A command or mode the specification and captures cannot pin down stops at a checked boundary.
