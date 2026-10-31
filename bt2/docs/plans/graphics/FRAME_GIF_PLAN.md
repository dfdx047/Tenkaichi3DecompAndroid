# Frame GIF DMA and GS register state

## Why this is a new subsystem

The `DBZP.BIN` overlay's frame path now runs natively up to its first draw submission. Its first display flip (the GS library's `0x311860`) matches the original, including the recorded privileged registers (`verify_overlay_dispenv.py`). The flip is followed by the library's draw-environment send (`0x3100D0`). That send:
1. waits for GIF DMA (channel 2) to be idle;
2. sets QWC from the packet's GIFtag NLOOP + 1;
3. sets MADR (scratchpad addresses get the SPR bit);
4. starts the channel in normal mode.

Every GIF transfer so far was a startup request checked against captured packets (`gif_dma_staging.h`, `gs_command_sink.h`). Per-frame drawing needs a general channel-2 model and a GS register state that consumes ordinary packets.

## Evidence (2026-10-07)

- **Draw environments.** The two double-buffer draw environments sit in the display block at `0x3B0850` (offsets `+80` and `+320`). The block matches the original at `rpc-overlay-dispenv`.
- **Display.** 512×448 interlaced: PMODE `0x66`, DISPFB2 FBW 8 / FBP 0, DISPLAY2 height 448.
- **Next stop.** The checked boundary is the GIF CHCR idle read at `0x3100E4` in main's thread.

## Bounded design

1. **Channel 2, normal mode.** Accept main-thread CHCR/MADR/QWC writes for normal-mode transfers. Bound and check the source range in RAM or scratchpad, consume synchronously, and report completion (STR clear) on the next poll. Chain mode and interrupts on completion stay checked boundaries until they are reached.
2. **GIF parser.** Parse PACKED (A+D and the primitive registers), REGLIST and IMAGE per public GIF documentation (PCSX2 GIF/GS sources as reference). Do not copy them.
3. **GS register state.** Keep the general register file for both contexts; FRAME, ZBUF, XYOFFSET, SCISSOR, TEST and so on are recorded, not rendered. A primitive kick is recorded as a draw request with its vertices. Rasterization stays a later, separate step.
4. **Verification.** Compare each packet's source bytes with an original capture at the send. Compare register outcomes with the packet contents.
5. **Follow the frame.** Further frame work (chain DMA, VIF1/PATH2, VU1 microprograms, PATH3 images, presentation of DISPFB2) gets its own plan when it becomes the blocker.

## References

- **[paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs)** (see [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference)): reference for the GS side of this plan: GIF packet parsing per path (`gif_transfer`), the GS context register file and primitive kicks. The DMA channel itself (normal and chain mode) follows PCSX2 `Dmac.cpp`/`Gif.cpp`, which paraLLEl-GS does not cover. Adapt its behavior; do not copy code.

## Limits and stop conditions

- No presentation or window output is claimed until a flip presents real VRAM contents.
- Unknown GIF modes, primitive types or register numbers stop as checked boundaries.
- Packets and captures stay local and ignored.

## Progress (2026-10-07)

Steps 1–3 are implemented for the reached shapes:
- channel-2 normal mode;
- the PACKED/REGLIST parser, with IMAGE refused;
- the general register file;
- flat untextured sprite drawing with dry-run validation.

The first frame's draw-environment send clears 512×448. The path sync and the game's VIF1 render kick are modeled for idle paths. The VIF1 interpreter accepts NOP only; the first frame's chain is empty. Real geometry (VIF1 UNPACK/MSCAL, VU1 microprograms, XGKICK) has not been reached yet. When it is, it needs its own plan.

The second frame's render kick carries the first VIF1 chain with content: PATH2 DIRECT texture/CLUT uploads and a textured sprite, without VU1. It is refused as a checked boundary at the channel start; continued in [GS_TEXTURE_PLAN.md](GS_TEXTURE_PLAN.md).
