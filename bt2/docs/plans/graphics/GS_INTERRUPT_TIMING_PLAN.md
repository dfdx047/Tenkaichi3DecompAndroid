# Interrupt and video-timing bring-up

## Observed boundary

The native probe stops before writing 4 to INTC_STAT (0x1000F000) at 0x0032F10C, SP 0x01FFEAD0, in FUN_0032F0F0. Inspected generated code polls bit 4 and acknowledges it a second time. The pinned PCSX2 Hw.h identifies this address as INTC_STAT; DMAC_CTRL is 0x1000E000. Keep neutral function names.

Two independent disc-backed captures reach the completed helper and subsequent CSR field read. INTC_STAT is 8 after the helper; the CSR sample is 0x551B6004. Both publications are stable twice, SP 0x01FFEAE0. These are separate runs, not a continuous timing trace or proof of elapsed cycles. Captures/hashes remain ignored.

## Separate implementation scope

1. Consult REFERENCE_ARCHITECTURE.md and its primary references, then pinned PCSX2 INTC acknowledgement, video-event and CSR field behavior. Establish precise bit semantics before implementation.
2. Capture the first acknowledgement, wait entry, event arrival and second acknowledgement with preserved alternative paths. Record scenario, clock/event identity and interrupt-mask state; distinguish status latching from callback delivery.
3. Implement only the reached acknowledgement/event-source contract in an authored component. A write acknowledgement must not itself fabricate the event that satisfies the wait. Use a deterministic, evidence-backed event schedule or explicit diagnostic checkpoint sequence with clearly stated limits.
4. Validate CSR field correlation and any later enable/wait path. Reject unsupported sources, masks, timing modes and service calls. Add bounded execution/poll limits for diagnostics.
5. Test acknowledgement, repeated/absent events and unsupported cases, then rerun through subsequent blockers. Start a separate GIF/renderer plan when actual submission requires it.

Existing GS setup returns use explicit reference checkpoints. They do not reset a native renderer, configure actual display timing or complete native IOP boot. Keep old IOP services invalid. Update PROG.md/history, commit and push after verification.

## Verified diagnostic continuation (2026-10-04)

Four independent disc-backed reached-point captures publish INTC_STAT, INTC_MASK, SP and preceding v0. Corrected instrumentation explicitly forms address 0x1000F000 before loading; an initial signed-offset trap was rejected and all four captures replaced. Status samples are 12 before acknowledgement, 8 after, 12 at event arrival (original masked poll result 4), and 8 after the second acknowledgement. Mask is 0x1802 throughout, SP 0x01FFEAD0. These are not a continuous cycle trace.

The authored IntcCheckpoint component is gated by all four validated publications. It clears only the acknowledged bit, preserves bit 8, allows one unsatisfied poll, then latches one explicit diagnostic Vblank-start checkpoint. Acknowledgement does not inject an event. The chosen two-poll schedule establishes logical ordering, not physical video timing. Mask excludes both sampled bits, so no callback is delivered. Repeated/unsupported acknowledgement, unsupported initialization and an absent-event poll budget fail explicitly.

A separate validated CSR field sample 0x551B6004 allows generated FUN_003104B0 to return. Its alternative SetVSyncFlag path is not reached or implemented. The next major subsystem is [GIF DMA](GIF_DMA_BRINGUP_PLAN.md). Native video clock, event/CSR field correlation and interrupt callback scheduling remain future work.
