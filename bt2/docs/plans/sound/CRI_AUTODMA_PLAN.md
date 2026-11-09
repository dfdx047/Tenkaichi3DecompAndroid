# CRI first AutoDMA transfer boundary

## Reached evidence

The 0x40C handler now starts its higher-priority child, which parks at the original first WaitSema. LIBSD then reaches its first core-0 transfer setup. Independent isolated captures preserve original StartThread and WaitSema calls, observe StartThread return 0, and record the original CHCR load at retail 0x84E80 (native 0x92250): register 0xBF8010C8, value 0. Transfer arguments are core 0, mode 0xC000, buffer 0xC6EF0, size 0x1000. The relocated native buffer is 0x176FC0; both observed 4096-byte buffers contain zeros, SHA-256 ad7facb2586fc6e966c004d7d1d16b024f5805ff7cb47c7a85dabd8b48892ca7.

A second terminal observer reaches the original requested ADMAS halfword store at retail 0x859E8 / native 0x92DB8: address 0xBF9001B0, value 1. The observer records the operands without issuing the store. Native owned configuration remains ADMAS 0 and rejects the same write. No AutoDMA transaction, completion, IRQ, CRI 0x40C reply or native audio playback is supplied.

## Required transfer design/evidence

- Consult REFERENCE_ARCHITECTURE.md, pinned BT3 IOP/audio ownership patterns, pinned PCSX2 SPU2 Dma/ReadInput/RegTable and public PS2SDK LIBSD/DMA contracts. Adapt behavior to the actual BT2 setup; do not copy a renderer/audio engine or infer mode semantics from BT3.
- Capture the original subsequent MADR/BCR/CHCR programming, interrupt enable state, actual transfer API return and first completion/callback. Distinguish initial configuration from active transport and continuous refill.
- Define ownership of the static initial buffer, transfer byte count, halves/loop behavior if confirmed, and SPU2 input destination. Keep IOP RAM, SPU2 storage and host audio output separate. Do not equate a logical transfer or zero-filled initial buffer with audible game content.
- Implement the bounded initial accepted transaction and its interrupt/callback lifecycle. Preserve nested interrupt state, the parked CRI child, pending RPC parent and server queue. Reject unsupported transfer modes, out-of-range buffers and later refill until observed.
- Complete original 0x40C return/EE cleanup only after its real handler returns; then follow captured 0x422 and further startup requests at subsystem scale.

The existing eight-halfword FIFO initialization is separate from this first AutoDMA path. General playback, voice mixing, audio timing, child semaphore wake and cross-CPU scheduling remain incomplete.

## Verified first-half acceptance and constructor batch (2026-10-06)

The exported `sceSdBlockTrans` call is now independently captured: core 0, **mode 0x10**, source 0xC6EF0, 4096 bytes, actual return **2048**. The earlier 0xC000 value was the retained core ATTR in an internal DMA-status helper; it was incorrectly described as the exported transfer mode above/in earlier history. Keep that distinction when interpreting those observations.

Original LIBSD enables ADMAS 1, programs MADR 0xC6EF0, BCR 0x00200010 and CHCR 0x01000201, and immediately reads the same active CHCR. BCR represents 32 blocks of 16 words, **2048 bytes**: the first half of the loop buffer. Another independent terminal capture reaches the first real DMA handler entry with MADR 0xC76F0, CHCR 0x201 and descriptor mode 0x1600. This establishes the observed completion frontier, not its callback return, subsequent half refill or physical timing. Two original parameter stores then set direct-input volumes at 0xBF90076C/0xBF90076E to 0x7FFF.

Native ownership retains the configured register sequence and an immutable submitted 2048-byte source snapshot. The transfer remains **pending**, with CHCR 0x01000201: no input consumption, DMA completion, IRQ, child wake/refill or host audio has been supplied. The real translated 0x40C handler nevertheless returns `[0, 1, 0]`, and generated EE RPC_END/cleanup completes. A nonblocking API/RPC return is separate from audio-transfer completion.

The following first-channel constructor batch now completes **sixteen 0x422/0x400 pairs** through original IOP handlers and generated EE cleanup. Requests are restricted by phase, exact extents, observed object/buffer strides and owned heap ranges; unsupported further calls reject. Independently captured constructor fields for all sixteen records match after internal pointer normalization. This establishes object construction, not streaming or sample decoding.

Reproduction: `capture_ee.py --capture-point rpc-cri-third-before --capture-cri-thread program|transfer|irq|volume` with the existing local PCSX2/BIOS/disc and absent-card options. `--capture-point rpc-cri-nth-before --cri-call-index 10` or `36` captures later original request boundaries without reviewing each function. All output stays ignored. `python tools/capture_ee/verify_cri_autodma.py --native work/bt2-generation/entry-stop.json` verifies programming, API return, volumes, original results and the completed batch.

The next handoff is [second-channel heap/layout ownership](CRI_SECOND_CHANNEL_PLAN.md). The parked child's current relocated entry is 0x1AC890, GP 0x1BCDF0 and WaitSema PC 0x1B363C. The zero-filled buffer is now 0x1B5FC0. Earlier native placements above are historical. General AutoDMA consumption/completion still needs the transfer design/evidence listed above.
