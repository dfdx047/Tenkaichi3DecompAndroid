# IOP SPU2 voice transfers

## Why this is a new subsystem

After its first file loads, main uploads a 0x16640-byte sound block into the IOP sound buffer (`0x28BA18`: one SIF DMA, then a status poll; modelled) and sends sound-driver command `0x80D0` to the registered service `0x80000701` (client `0x3A5C80`, 64-byte request, 16-byte reply). The original handler reaches LIBSD's transfer routine (`0x921D0`), which moves data from IOP RAM into SPU2 sound RAM:

- **Idle check:** core ATTR (`0x1F90019A` + core × 0x400) DMA bits and DMA4 CHCR (`0x1F8010C8`) busy bit 24 at `0x92250`.
- **Setup:** transfer start address (`0x1F9001A8`/`0x1F9001AA`), then ATTR DMA mode (`|0x20` for writes) inside a suspended-interrupt section.
- **DMA4 programming:** MADR `0x1F8010C0`, BCR `0x1F8010C4` = 16 words per block, `0x1F8010C6` = blocks, CHCR `0x1F8010C8` = `0x01000201` (to SPU2, start).
- **Completion:** LIBSD's IRQ 36 handler `0x927E8` (already registered at startup) clears the ATTR DMA bits and signals the waiting thread; the wait mode (`0x923F4`) polls CHCR and the I/O status (`0x1F900344` bit 0x80).

The owned SPU2 device models only the observed CRI AutoDMA startup and FIFO checkpoints. Registered RPC handlers cannot suspend on an event wait in this runtime, so completion must be observable before any blocking wait.

## Bounded design

1. A general owned SPU2 DMA write for cores 0/1 (DMA4/DMA7): reads/writes of TSA, ATTR, ADMA status, MADR/BCR/CHCR follow register semantics instead of startup checkpoints, outside the CRI AutoDMA owner.
2. At CHCR start: check MADR/size against owned IOP RAM, copy the bytes to `spu2_sound_ram` at TSA (halfword addressed) and complete the transfer in logical time (CHCR bit 24 clear).
3. Deliver IRQ 36/40 through the registered LIBSD handler with the IOP interrupt ABI, before the issuing thread's next wait or status poll, when CPU interrupts are enabled; fail closed otherwise.
4. Enable `sound_voice_transfers_supported` for the later sound commands and compare the transferred bytes with the uploaded IOP buffer.
5. Verify: unit tests for the register sequence, bounds, busy/idle and handler ordering; normal startup; capture the original after the command if a later consumer depends on SPU2 state.

## Limits and stop conditions

No audio output, voice playback or SPU2 timing is claimed. Do not fabricate completion for read transfers (SPU2 → IOP) or AutoDMA outside its owner. Stop if a reached handler requires a suspended RPC frame, or if transfer sizes/addresses cannot be validated against owned memory.

## Outcome (2026-10-07)

Steps 1–4 are implemented. While a later sound command runs, the owned SPU2 device accepts LIBSD's write transfers on DMA4/DMA7 (core 1 for the reached commands; DMA7 at `0x1F801500`), copies the IOP bytes into `spu2_sound_ram` at the transfer address and runs LIBSD's IRQ 36/40 handler at once on a stack below the live stack pointer. `QueryIntrContext` is provided. Two sound-driver commands (`0x80D0` and its follow-up) complete through the original handlers; a unit test covers the register sequence, bounds and handler ordering. Main's later SOUNDS command `0x20` starts the SOUNDS worker, which allocates an IOP hardware timer: see [IOP_HARDWARE_TIMER_PLAN.md](../iop/IOP_HARDWARE_TIMER_PLAN.md).
