# Bounded SPU2 initialization

Continue the actual CRI_ADXI -> LIBSD sceSdInit path. Existing library registration does not initialize SPU2 or establish audio playback.

1. Correct the initial diagnostic: PC 0x91614 writes halfword 0x0900 to physical 0x1F9007C6 (SPDIF mode); it is not a read.
2. Consult pinned SDK register definitions and PCSX2 handling, then retain only reached configuration writes with their actual semantics. Unknown widths/addresses/readback remain rejected.
3. Run after each meaningful stage. At status/transfer boundaries, capture the actual reached local hardware value or transition with guarded instrumentation; never return arbitrary ready/zero values.
4. Complete the bounded initialization sequence where evidence permits, retain owned interrupt/DMA/callback state, and compare local startup effects before acknowledging the pending module.
5. Continue later startup blockers in the same scope. Sample processing/playback, unsupported transfers and new subsystems need separate evidence/plans. Test, update progress/history, commit and push at the end.

All local captures, instrumented binaries and generated retail-derived source remain ignored. Adapt public/reference patterns, do not copy implementations.


## Verified boundary

The earlier first-FIFO boundary at 0x940CC is resolved. Eight halfwords are stored in owned sound RAM at word addresses 0x2800..0x2807, TSA advances to 0x2808, and the captured first status poll returns 0x80. LIBSD returns 0 after voice/core configuration; 394 stable configuration registers match an independent initialized reference snapshot. Dynamic ENDX values are excluded from that comparison, with only explicit immediate clear readbacks supported.

CRI_ADXI returns resident result 2. Relocated code, complete normalized data/BSS and its Vblank/RPC workers match a local return capture. No sample processing, general sound transfers, IRQ delivery or audio playback is established. Startup now reaches the separate [file I/O RPC boundary](../iop/IOP_HEAP_STARTUP_PLAN.md).

## Sound RAM transaction pass

1. Capture the first reached I/O-mode status poll after the eight FIFO halfwords; publish original status/ATTR/TSA and compare source with terminal diagnostic destination readback.
2. Adapt documented halfword memory storage and TSA advancement under disabled IRQ/ADMA prerequisites. Keep sound RAM owned separately from IOP RAM. Unsupported modes/IRQs/transfers fail.
3. Validate the I/O-mode transition and reached status against capture; return completion state only from an accepted transaction, not arbitrary readiness.
4. Continue through voice/core register setup, IRQ registration and later startup where reference behavior and local evidence permit. Validate pending module residency before acknowledging it. Test/update history/commit/push at pass end.
