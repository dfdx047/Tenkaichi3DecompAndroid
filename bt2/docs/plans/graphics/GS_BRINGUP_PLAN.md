# Scoped GS bring-up plan

## Evidence and boundary

The checked native path reaches mapped FUN_0030FDC0 from 0x0010026C, resume 0x00100274. Local generated code calls FUN_0030FF50, then includes a 64-bit write of 0x200 and a 64-bit read at 0x12001000. This is inspected code ahead of the current stop, not an executed GS transaction. Keep function names neutral.

Consult [the reference architecture](../../reference/REFERENCE_ARCHITECTURE.md) and pinned BT3 GS interface first. Its command recording/presentation separation is guidance for later rendering; do not import its game patches, renderer or platform dependencies wholesale.

## Next pass

1. Select exact reached map entries, compile/link and run until the actual first GS/runtime boundary.
2. Collect local disc-backed PCSX2 observations of the reached register writes/readbacks and any required syscall. Preserve original alternative paths when instrumenting.
3. Implement a small authored register interface with explicit supported addresses, widths, side effects and evidence. Distinguish captured reference readbacks from native simulated behavior. Reject unknown accesses; do not invent reset values or timing.
4. Test meaningful supported/unsupported behavior and rerun through subsequent blockers. Add GIF/DMA handling only when execution requires it and evidence is available.
5. Stop for a separate rendering/presentation plan when draw submission becomes the actual blocker. A register model is not a rendered frame.

## Remaining IOP limitation

Current readiness continuation replays only the tested 0x40000 bit from a reached reference branch. The full flag value, native IOP loader/import resolution, scheduling and new service routing remain unimplemented. Live post-reset IOP RAM is diagnostic evidence, not a coherent execution state to restore wholesale. Keep the old service lifetime invalid and reject unsupported new service calls.

After verification, update PROG.md/history, review staging, commit and push.

## Initial setup result

The reached GS reset/readback, interrupt-mask and CRT calls now return with explicit matching local reference checkpoints. Several generated RAM helpers run, using bounded halfword access and 128-bit OR. Every selected extra entry now receives deterministic tail-call routing and source/prepared hash provenance. This permits the next missing entry to become a checked execution boundary without a native linker stub.

The next actual stop is INTC_STAT acknowledgement/event waiting in FUN_0032F0F0. Follow [the separate interrupt/video-timing plan](GS_INTERRUPT_TIMING_PLAN.md). Acknowledging an event and waiting for the next one requires more than storing the written value. No GIF transfers or renderer introduced.

## References

- **[paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs)** (see [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference)): reference for GS privileged register state (CSR, PMODE, SMODE, DISPFB, DISPLAY) reached by this setup. Adapt its behavior; do not copy code.
