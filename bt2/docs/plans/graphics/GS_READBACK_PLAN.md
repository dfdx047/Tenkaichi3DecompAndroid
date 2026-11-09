# GS readback (local→host transfers)

The game reads a rectangle of GS memory back into EE memory. Part of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step); the pixels come from the GS backends ([GPU rendering plan](GPU_RENDERING_PLAN.md)).

## Evidence (2026-10-13)

- **Where:** battle loading, after character and map selection. Main calls the GS library's store-image routine (`0x310D08`), reached through `0x27A4E8`. It was first seen as a stop at EE syscall `0x70` (GsGetIMR), which the kernel model did not answer.
- **The sequence the routine drives** (read from the disassembly of the game's own copy of the library, not from a capture):
  1. Wait until VIF1's DMA channel is idle (`D1_CHCR` bit 8).
  2. `GsGetIMR`, then `GsPutIMR(mask | 0x200)`: FINISH interrupts off. The routine keeps PutIMR's result and puts it back at the end, so PutIMR returns the previous mask.
  3. Write 2 to `GS_CSR` (clear the FINISH event), then send a 7-quadword packet through VIF1 in normal DMA mode (`D1_QWC` 7, `D1_MADR`, `D1_CHCR` `0x101`). The packet holds VIF codes (among them MSKPATH3 and DIRECT) and a GIF packet with BITBLTBUF, TRXPOS, TRXREG, FINISH and TRXDIR = 1.
  4. Wait for the channel, then for `GS_CSR` bit 1 (FINISH).
  5. `VIF1_STAT` = `0x800000` (FDR: the FIFO runs from the GS to the EE) and `GS_BUSDIR` = 1.
  6. Take the pixels: whole 8-quadword groups by a VIF1 DMA to memory (`D1_CHCR` `0x100`), the rest quadword by quadword from the VIF1 FIFO (`0x10005000`) whenever `VIF1_STAT`'s FQC field is nonzero. If the size is not a whole number of quadwords the routine first rounds the height in TRXREG up, so the transfer always ends on a quadword; the extra quadwords are read and dropped.
  7. `VIF1_STAT` = 0, `GS_BUSDIR` = 0, `GsPutIMR(previous)`, clear FINISH again, and write one quadword of VIF codes to the FIFO (MSKPATH3 off).

## Design

- **GS model (`GsCommandSink`):** TRXDIR = 1 records the source rectangle as the pending download; each FINISH write counts one FINISH event. `readDownload` hands out the transfer's bytes in order. The CPU model reads PSMCT32 and PSMCT24 buffers from its own VRAM and refuses other formats by name; with the GPU renderer drawing it only tracks the position.
- **GPU renderer:** paraLLEl-GS performs the local→host transfer itself when it sees TRXDIR = 1; `GpuGsBackend::readFifo` returns its data (`read_transfer_fifo`). In GPU-only mode the probe connects it as the runtime's `gs_download_source`.
- **EE side (`runtime/ee/ee_gs_readback.cpp`):** the registers of the sequence, valid in the frame phase on main:
  - `D1_MADR`, and `D1_CHCR` `0x101` (VIF codes from memory, checked as a whole before they run) and `0x100` (download data to memory);
  - `VIF1_STAT` writes (FDR only) and reads (FDR, FQC = quadwords left, at most 16);
  - `GS_CSR` bit 1: set by a FINISH write, cleared by writing 2; `GS_BUSDIR`;
  - VIF1 FIFO reads (download data) and writes (VIF codes);
  - syscalls `0x70` and `0x71` (the GS interrupt mask).
- **VIF1:** MSKPATH3 sets a register bit. Transfers complete synchronously, so nothing is ever in flight on PATH3; a GIF channel start while the bit is set is refused instead of being queued.
- **Time:** every step completes at once. The status reads of the routine's wait loops see the finished state on their first read.

## Verification

- Unit tests: the sink's download (size, order, format refusal, FINISH count), the VIF1 MSKPATH3 bit, and the EE sequence end to end against a seeded CPU VRAM.
- Still open: a PCSX2 capture of the destination buffer after the routine returns, to compare the pixels the GPU renderer returns with the original.

## Stop conditions

A download format the CPU model does not read (when it draws), a scratchpad destination, a FIFO read without data, a `VIF1_STAT` or `GS_BUSDIR` value outside the sequence, or a GIF channel start while PATH3 is masked stops at a checked boundary.
