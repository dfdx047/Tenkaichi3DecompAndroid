# 3D pipeline: VIF1 display lists and VU1

Run the game's 3D drawing path natively: VIF1 display lists unpack vertex data into VU1 memory and start VU1 microprograms, which transform the geometry and send GS packets (PATH1). Part of the [implementation plan](../../project/IMPLEMENTATION_PLAN.md#current-next-step); rendering itself stays with the GS backends ([GPU rendering plan](GPU_RENDERING_PLAN.md)).

## Evidence (2026-10-12)

- **Current stop:** after the title screen's New Game choice (scripted Start presses), main starts VIF1 (channel 1, `0x10009000`) with CHCR `0x145` at pc `0x28B3E8`: source chain with **TTE** (bit 6), so the upper 64 bits of every DMA tag go to VIF1 as two VIFcodes. The runtime models only chain mode without TTE and VIF1 DIRECT data.
- **Before it** the game already uses DMA CALL/RET tags in VIF1 chains (now walked, two return levels), reads VPU_STAT from several threads (idle: nothing is started yet) and runs VU0 **macro-mode** code inline (matrix and perspective routines: VADD/VSUB/VMUL with field masks, ACC, Q, VSQI/VLQD on VU0 data memory, and the status flags of its clip tests).
- **Whole-game surface of generated VU0 macro code** (surveyed in the generated sources): `vu0_vf` (5,791 uses), `vu0_acc` (1,428), `vu0_q` (105), `vu0_r` (30), status flags (12, in clip tests), clip flags (3), four arithmetic helpers and eight VU0 memory instructions. VU0 micro mode (VCALLMS) has not been seen.
- **Not yet known:** which VIFcodes the lists use, how many VU1 microprograms the game has and how large they are, and whether VU1 output reaches the GS only through XGKICK.

## Progress (2026-10-13)

Steps 1 to 3 of the order below are done and the first 3D frames are presented (step 4 without the PCSX2 comparison):

- **Display lists:** the first list uses MPG, BASE, OFFSET, UNPACK V4-32 (plain write cycle), MSCALF, MSCNT, FLUSHE and DIRECT. It was the idle demo fight's list, not New Game's.
- **Code:** `runtime/vu/vif1.*` (VIFcodes; a transfer is checked as a whole before it runs) and `runtime/vu/vu1.*` (memory, interpreter, XGKICK packets). `EeRuntime::frameVif1Start` walks the DMA chain and feeds the words; XGKICK and DIRECT data reach the GS in list order.
- **Microprograms:** five in the first list (101 to 451 instructions), more later (48,963 uploads in the long run). Modelled instructions: the set those programs use, listed in `vu1.cpp`; every other one stops by name.
- **Pipeline model:** one cycle per instruction pair; Q is visible 7 cycles after DIV, the clip flags 4 cycles after CLIP; no stall cycles. Reads that fall inside such a delay are counted (`vu1_early_q_reads`, `vu1_early_clip_reads` in the probe report): both are 0 over 4.43 billion instructions, so the programs reached so far do not depend on stall timing. A branch on an integer register written by the previous instruction is refused unless that instruction is a flag instruction (the programs pair `FCOR` with `IBNE` directly).
- **Rendering:** GPU renderer only. The CPU GS model refuses the 3D drawing states; in GPU-only mode it checks packet framing only.
- **Battle (2026-10-13):** a 1P-vs-COM duel runs on the same interpreter (5.77 billion instructions in the 9,000-frame run, no early Q or clip read) at about 19 presented frames per second of wall-clock time, so step 5 (translation of the hot microprograms) is needed before live play.
- **Open:** VU1 memory against a PCSX2 capture, a frame comparison with the original, the CPU renderer's 3D states, and measured frame time (the 9,000-frame long run takes 214 s with the interpreter).

## Design

1. **VIF1 interpreter** (`runtime/gs/` or a new `runtime/vu/` folder, decided when the code exists): VIFcodes from chain data and, with TTE, from the tags. State: CYCLE (STCYCL), MASK/ROW/COL (STMASK, STROW, STCOL), MODE (STMOD), BASE/OFST/TOPS/TOP (BASE, OFFSET, the double-buffer swap at MSCAL), ITOPS/ITOP, MARK. Commands: NOP, MSKPATH3, FLUSH/FLUSHE/FLUSHA, MPG (microprogram upload), UNPACK (all formats the lists use, with masking and addition modes), MSCAL/MSCALF/MSCNT, DIRECT/DIRECTHL (existing path). Each unsupported code or UNPACK format stops by name.
2. **VU1 memory:** 16 KB data and 16 KB microprogram memory owned by the runtime; UNPACK and MPG write them.
3. **VU1 execution:** start with an **interpreter** of the VU instruction set as the reference (upper and lower instruction per cycle, the pipeline delays that programs can observe: flag latency, Q/P, branch delay slots, XGKICK). It defines correct behaviour and gives the tests. Then, where measured Release performance requires it, translate the game's microprograms to native code (they are a small fixed set uploaded by MPG), validated against the interpreter. No general dynamic recompiler.
4. **PATH1:** XGKICK sends the GIF packet at the given VU1 memory address to the GS backend through the existing validated GIF path (`GsCommandSink`), in order with PATH2/PATH3 data.
5. **Status:** VPU_STAT and the COP2 condition report a running VU1 only while a microprogram executes inside MSCAL; with synchronous execution they stay idle between VIF commands.

## Order

1. Log the VIFcodes of the first display lists (a trace at the checked boundary) and list the UNPACK formats and microprogram uploads; record microprogram sizes and hashes (local, ignored).
2. VIF1 with TTE, the state commands, MPG and UNPACK into VU1 memory, with unit tests per code and UNPACK format; MSCAL stops at a checked boundary.
3. VU interpreter: instruction decoding and the instructions the uploaded microprograms use, with unit tests per instruction; XGKICK to the GS.
4. First 3D frame: compare with a PCSX2 screenshot or GS dump; the user confirms the picture.
5. Measure Release frame time; translate hot microprograms if needed.

## Verification

- Unit tests per VIFcode, UNPACK format and VU instruction (synthetic data only).
- VU1 memory after a display list compared with a PCSX2 capture at the same point (a VU1 memory capture point is to be added to `capture_ee.py`).
- Frames: PCSX2 comparison plus the user's inspection.

## Stop conditions

A VIFcode, UNPACK format or VU instruction outside the public documentation's description, or a microprogram whose behaviour the interpreter cannot reproduce against a capture, stops at a checked boundary.
