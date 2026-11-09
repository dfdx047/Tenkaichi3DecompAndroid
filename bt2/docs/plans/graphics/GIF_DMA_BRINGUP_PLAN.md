# GIF DMA bring-up

## Reached boundary

The checkpoint probe stops at the first 32-bit channel control read, GIF DMA CHCR `0x1000A000`, PC `0x003100E4`, SP `0x01FFEAF0`, in `FUN_003100D0`. The pinned PCSX2 hardware map identifies the channel; generated code tests bit `0x100` before further control/status access. No channel idle value, completion or packet submission has been modeled.

## Implementation scope for the next pass

1. Consult REFERENCE_ARCHITECTURE.md and the pinned BT3 GIF/DMA command-recording architecture, then pinned PCSX2 GIF channel/register/chain behavior. Adapt patterns to the actual BT2 path.
2. Capture the reached CHCR read and subsequent DMA status accesses with the user's local disc. Validate source/prefix/trap/publication, mask/enable state, packet address/count and any completion observations. Preserve alternative paths; a single idle snapshot is not a working DMA implementation.
3. Implement only observed idle/control behavior, then continue generated execution to actual channel programming/submission. Route dynamic word MMIO writes through context-aware checks if needed. Reject unsupported modes, channels and out-of-bounds source ranges.
4. Before acknowledging a transfer, validate its complete packet/chain and define a narrow authored GIF command sink. Keep transfer effects, completion status and rendering separate. Do not fabricate immediate completion or assume BT3 packets match BT2.
5. Test supported and rejected transfers, rerun through subsequent blockers, and start a renderer plan when actual graphics commands require it. Retain reference-checkpoint limitations and pending IOP lifetime.

A plain continue should implement this plan and follow multiple supported blockers. Build/test, update PROG.md and detailed history, then commit/push once at the end.

## Verified staging and drawing boundary (2026-10-04)

Three independent disc-backed points now validate the first helper path: idle read, QWC programming and caller return after submission. Initial CHCR/QWC/MADR are zero, DMAC_CTRL 1, DMAC_STAT 0x800040, DMAC_ENABLER 0x1201. QWC becomes 15. The 240-byte source packet is identical across the three reference captures and stable twice at the idle capture. Returning from the reference helper does not prove transfer completion.

The authored runtime accepts the one reached idle read, QWC write and MADR write in order, with exact PC/SP guards and source bounds/alignment. It compares native source bytes with reference evidence before staging the address. A raw-byte mismatch was investigated: generated FUN_003119E8 retains old stack bits in PRMODECONT/COLCLAMP/DTHE; public GS definitions show only bit 0 is meaningful in those registers. Only those unused bits may differ; tag, register addresses and all other values remain exact, and guest RAM is never rewritten to match a capture.

The first packet includes two XYZ2 kicks. Native execution stops before CHCR start 0x101 at 0x003101A8, with source 0x01FFEB50 /QWC 15. No transfer, completion or render effect is accepted. Follow [the first drawing/renderer plan](GS_RENDERER_BRINGUP_PLAN.md); this is the new major subsystem boundary after staging and validation.
