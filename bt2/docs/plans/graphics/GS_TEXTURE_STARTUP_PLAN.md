# GS texture-state startup

## Actual handoff

VIF configuration and the second GS reset/IMR/CRT call return. FUN_00270FF0 copies 64 bytes from local ELF data into its stack and calls FUN_0028B908 at 0x271044. Native execution stops at the latter's first CHCR poll: address 0x1000A000, PC 0x28B928, SP initial minus 128. The subsequent code polls GIF STAT, stages QWC/MADR, calls the existing cache helper, starts normal DMA with CHCR 0x101 and polls completion.

Local packet inspection establishes packed A+D with three register writes: PABE (0x49), TEX0_1 (0x08), TEX0_2 (0x09). Raw bytes remain ignored. These are texture-state configuration, not geometry or proof of texture loading. The current consumer only understands two 240-byte sprite packets and cannot accept this format. Nothing beyond the initial poll succeeds yet.

## Bounded implementation pass

1. Consult REFERENCE_ARCHITECTURE.md and pinned BT3/PCSX2 GIF, DMA and GS register handling. Adapt the decoder/state separation; do not copy game-specific code.
2. Capture the unique caller return 0x1005D8 and selected actual CHCR/GIF STAT reads, QWC/MADR and stack source bytes. Validate disc/ELF/emulator/BIOS identity and stable publications. Use the existing local capture tooling; do not invent busy timing.
3. Separate packed A+D register decoding from the existing sprite draw recognizer. Retain PABE/TEX0 state for both contexts with documented masks. Reject unsupported packet formats, register effects and draw paths. Do not claim texture sampling or upload from configuration alone.
4. Extend the bounded normal-mode GIF staging path without weakening the first two packet checks. Compare this third source packet and register effects against the local capture; preserve VRAM across reset.
5. Generate/link/run, then follow related graphics setup and reached thread calls through multiple blockers. Gather local evidence before extending contracts. Stop at unsupported actual texture transfer, draw or presentation behavior needing a further scope.
6. Build/test, update PROG and detailed history, commit and push.

This is a new GS packet/texture-state scope. Full GS emulation, VU execution, texture sampling and presentation remain separate. Existing completion snapshots are evidence for their observed requests only.

## Verified completion (2026-10-05)

The third packet completes against independent source, original idle/busy/clear-STR loads and unique caller return captures. Native retains PABE/TEX0 for both contexts; no texture sampling, CLUT transaction or added pixels occur. A separate configuration consumer leaves the prior sprite validator intact. Unsupported register effects and malformed packets reject transactionally.

Continued through disabled Timer 1 configuration, main-thread priority change, generated EE allocator/map-gap discovery and four pool allocations. Pool pointers match a unique retail return capture. The actual next native stop is CreateSema in the second IOP heap RPC path; the retail allocation range conflicts with translated FILEIO. Follow [memory-layout scope](../iop/IOP_HEAP_CAPACITY_PLAN.md). Earlier wording in this plan describes the original handoff.

## References

- **[paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs)** (see [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference)): reference for the GS context register state (TEX0, TEX1, TEXCLUT, TEXA, PABE) that this plan retains. Adapt its behavior; do not copy code.
