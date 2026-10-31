# Performance: real-time speed in the window

Opened on 2026-10-13 after the user reported the window as laggy (about 15 frames per second) with the graphics card at about 16% load.

## Requirements (set by the user, 2026-10-13)

- **Fights: a flat 30 pictures per second.** The user confirmed in PCSX2 that the original shows 30 in fights; the fight loop's two-field frame, read from the code, agrees.
- **Menus: a flat 60, without dips.**
- **Never faster than the game:** the run is held at the PS2's field rate, 59.94 per second.
- **No bottleneck from the port** (the user, 2026-10-20: "this is a game from 2007 and the native layer should be light"): neither the processor nor the graphics card may hold the game back, with the enhancements on too. An enhancement whose cost makes pictures late is a fault to fix, not the price of the setting.
- **A mid- to low-end PC must hold these too,** not only the user's machine. The measure for that is the work per picture, not the rate on this machine: a processor half as fast needs the work here to stay under half the budget (about 16 ms of 33.4 ms in fights, about 8 ms of 16.7 ms in menus). Optimizations must remove work or spread it over cores; nothing may rely on instruction sets beyond SSE2.

## How it is measured

`BT2_SAMPLE_PROFILE=<path>` (with `BT2_SAMPLE_FROM=<flip>`) samples where the runner's thread executes and writes module offsets; `python tools/perf/profile_report.py <profile> build/probe/Release/bt2-entry-probe.map` names the functions. For the parts of a flip:

`BT2_VIEWER_STATS=1` with the viewer prints every 120 flips: flips and fields per second of wall-clock time, the slowest single flip and how many took more than 20.9 ms, the work per flip (its time without the pacer's waiting), and the time per flip spent on reading the picture back from the GPU, on showing it, on VIF1 unpacking, on VU1 (with the GS packets it sends), on GS packet handling from all sources and on the GPU renderer's flush. The rest of a flip is the recompiled game code and the services. Timers are in `runtime/ee/frame_profile.h`.

**Scene:** the scripted path into a 1P-vs-COM fight (`pad-fight.txt`), Release build, the user's 10-core machine. One scene on one machine; other fights and stages are not measured.

**Equality of output:** the report's `vu1_kick_hash` is a hash over every GS packet VU1 sends, in order. Equal hashes on the same scripted run mean a change to VU1 or VIF1 left the output bit-identical. Baseline for 5,100 frames of the fight script: 784,729,536 instruction pairs, hash `f3da74e43010e423`.

## Findings (2026-10-13)

**1. Menus and movies were slow because of waiting, not work.** The window and the audio device both paced the run, each with sleeps at Windows' default granularity of about 15 ms, so a flip took about 28 to 31 ms although the work needed under 10 ms. Fixed: the audio device is the only pacer when it is on, and sleeps are 1 ms. Measured after: menus hold 60 flips per second (16.7 ms per flip); the movies' 30 flips per second are the movies' own rate.

**2. The fight is limited by the processor, not the graphics card.** Time per flip in the fight, three windows of 120 flips:

| Part | At the start | After the changes below |
| --- | --- | --- |
| Whole flip | 58.7 to 73.1 ms | 31.7 to 37.7 ms |
| VU1 with its GS packets | 35.9 to 50.3 ms | 16.8 to 23.5 ms |
| GS packet handling (all sources; mostly inside the VU1 figure) | 7.3 to 8.0 ms | 5.8 to 6.5 ms, of it 3.9 to 4.5 ms in the GPU renderer |
| VIF1 (unpacking and direct GS data) | 4.5 to 4.9 ms | 2.8 to 3.1 ms |
| Picture readback and showing | 2.8 to 3.9 ms | 2.7 to 3.9 ms |
| GPU flush | 0.1 to 0.2 ms | 0.1 to 0.3 ms |
| Game code and services (the remainder) | about 12 to 14 ms | about 8 ms |

The three windows are the same 360 flips of the run each time; the scene gets lighter across them, so the ranges are comparable between columns but not within one. With the first figure of the right column the fight ran at 53 fields per second of wall-clock time, with the last at 63 (60 is the game's speed).

VU1 runs about 1.1 to 1.3 million instruction pairs per flip in this scene. The GPU's low load is consistent with this: it waits for the processor.

**3. The fight shows 30 pictures per second.** The fight's main loop (the functions at `0x102E80` and `0x102F30`) starts Timer 1 on the H-BLANK clock at the top of each frame and, before it waits for the vertical blank, waits until that timer has counted 400 (`0x28C790`). 400 lines are about 1.5 fields, so each frame takes two fields. The menu loops call a frame end without this wait and run at 60. The user confirmed 30 in fights on the original in PCSX2.

**4. Menus averaged 60 but were not flat.** With the sound device's queue as the pacer, 15 to 20 of every 120 pictures took more than 20.9 ms (the slowest 26 to 30 ms), because the device releases its queue in coarse steps. With the clock-based pacer (below) 0 or 1 of 120 are late and the slowest steady picture is about 20 ms; the work per picture is 6 to 7.5 ms.

**5. Single hitches at screen changes** (0.11 to 0.3 s, and 0.2 s entering the fight): the game loads data and the port does all of it within one field. Not addressed yet.

**6. A longer fight is heavier than the first 20 seconds.** Over about 100 seconds of the scripted fight, with the interpreter, the work per picture was 25 to 31 ms in most stretches and 35 to 39 ms in the heaviest (flips 5,280 to 5,880), where the rate fell to 23 to 28 pictures per second; VU1 with its packets took 23 to 24.5 ms there.

**7. With native VU1 programs the fight averages 30 throughout.** Every 120-flip window of the same run holds 29.8 to 30.0 pictures per second. The work per picture is 18 to 27 ms in most stretches and 29 to 31.5 ms in the heaviest; VU1 with its packets is 7 to 12 ms and 15 to 17 ms. Single pictures still take 37 to 59 ms now and then (the slowest per window), so it is not yet even; and the heaviest stretch leaves almost no margin on this machine, which a slower processor does not have.

## Done

All with the same instruction count, call count and output hash on the fight run as the baseline.

- **Pacing:** one pacer, 1 ms sleeps (`runtime/host/audio_device.h`, `viewer_window.h`).
- **VU1 arithmetic with SSE:** the float mode of a run (round toward zero, denormals as zero) is set in MXCSR and the four lanes of an arithmetic instruction are computed at once, with the model's clamp of infinities and NaNs as integer operations. The XGKICK packet buffer is reused.
- **VU1 pairs decoded once:** an instruction pair is decoded into a table entry the first time it runs (an entry is current while its words equal program memory, so nothing has to invalidate it). Arithmetic, the plain loads and stores and the no-operations run from the entry; everything else goes through the general path, which also refuses what is not modelled.
- **No CPU copy of texture uploads in GPU-only mode:** the CPU GS model no longer writes host-to-local image data into its own memory when the GPU renderer draws alone.
- **Main RAM first in word loads and stores:** the game's 32-bit memory accesses reach RAM before the chain of device and checkpoint cases, none of which is a RAM address.
- **Call bookkeeping:** the sets of entered and completed functions are skipped for a function recorded just before (a small table in front of them).
- **A sampling profiler** (`BT2_SAMPLE_PROFILE`; `tools/perf/profile_report.py` by function with the callers of library code, `profile_lines.py` by source line) to find the next places.
- **The VIF1 list buffer is reused** instead of being built in a new zero-filled vector for every list.
- **The EE FPU helpers set the rounding mode in MXCSR** for their one operation instead of through the C library.
- **The CPU GS model follows only A+D registers in GPU-only mode** (transfers, FINISH); vertices, primitives and CLUT loads are the renderer's. Its primitive counters stay at zero in that mode.
- **Frame timers cost nothing unless the statistics are on.**
- **VU1 microprograms as native code.** A census of the extended fight run found 14 distinct programs with 1,720 instruction pairs in all (a program is the set of pairs reachable from a start address, `runtime/vu/vu1_program.h`). `tools/vu1/translate_programs.py` turns each into a C++ function: arithmetic, loads and stores are written out with their operands as constants, every other instruction calls the interpreter's own code, and branches become labels and gotos. A start looks its program up by key when program memory has changed; a program without a translation, or a jump out of a translated one, runs in the interpreter. On the fight script all 2,501,695 starts ran native code with the same instruction count and output hash. The translations are derived from the game's programs: generated locally into the ignored `game/generated/vu1/`, like the EE code.
- **A clock-based pacer** (`runtime/host/frame_pacer.h`): each field has a deadline one period after the last and is waited for once, when its picture is shown or at its end, with a high-resolution timer. The period follows the sound device's queue within 1%, so the device no longer paces the run and its queue stays near 50 ms.

## Next, in order of expected gain

Against the requirements the fight needs its work cut to about 16 ms per picture, and its slow single pictures explained. The numbers above were measured before the fixes of 2026-10-13 to the game's random numbers and float sign tests, which changed what happens in the scripted fight; the first step is to measure again.

1. **Hand-written native VU1 programs** (decided by the user on 2026-10-13; next section).
2. **The slow single pictures in the fight** (37 to 59 ms while the average is in budget): find whether it is the work of single frames (uploads, the renderer) or the pacing.
3. **Graphics on its own thread.** VIF1, VU1 and the GS packets do not feed back into the game except at a few points (path synchronization, readback, the flip). Running them on a second thread takes about half of the fight's work off the game's thread. Needs an ordered queue for all GS-bound data and those synchronization points.
4. **Presenting without readback — done 2026-10-20.** The window shows the renderer's image through a swapchain on the renderer's own device (`runtime/gs/gpu/gpu_window_presenter.cpp`, Granite's WSI; `host/window_presenter.h` keeps Vulkan out of the window): the scanout is blitted to the window, filtered where the stretch is not a whole multiple; pictures drawn by the processor (the menus) are uploaded and blitted; the frame rate text is a small picture blitted on top. The picture is read back only for snapshots and when the in-game menu opens. `BT2_VIEWER_GDI=1` keeps the old path. Measured in a versus fight in the window (1x): scanout 0.1 ms instead of 2.2 to 3.8 ms per picture, work per picture 15.7 to 18 ms instead of 20; GDI's filtered stretch, which the presenter replaces, took about 12 ms.
4b. **Resource use, measured 2026-10-20** (paced versus fight in the window): the processor 0.42 cores plain and 0.55 with every enhancement, memory 680 to 843 MB, GPU memory 831 to 1,180 MB; GPU time per frame 5.9 ms plain (at low clocks), 7.7 ms at 16x, 10.1 ms with 16x, pack 2 and widescreen, under a third of a fight frame's 33 ms. The README's system requirements are estimated from these figures.
4a. **Enhancements' cost (measured 2026-10-20, versus fight in the window, RX 6800 XT, i9-10900K):** super-sampling 1x, 4x, 8x, 16x: work per picture 15 to 16.5, 18 to 20, 21 to 23, 22.5 to 26 ms, all at 30 per second; the time in the renderer's intake on the game's thread grows with the samples (6.5 to 7, 9.3 to 10.5, 12.5 to 13.5, 13 to 16.5 ms), i.e. the game's thread waits for the graphics card's work. HD textures (pack 2) at 1x: GS handling 15 to 16 ms against the renderer's 5.5: about 10 ms of naming textures on the game's thread. HD at 16x in the window: 22 to 25 pictures per second (work 37 to 39 ms, the window waiting 6 to 10 ms for the card). **Fixed the same day:** (1) the waits were frames in flight, not the graphics card: the card's 3D engine was at 53% on average (69% at most) while the game's thread spent 34% of its time waiting for a frame context; the renderer and the window each use one per picture, and with 4 the game waited for work two pictures old. With 8 frame contexts, 16x with pack 2 holds 30 per second in the window: work per picture 17 to 21 ms (from 30 to 39), the renderer's intake 5 to 6 ms (from 11 to 15), the window's wait 0.5 ms (from 5 to 10). (2) The texture naming, profiled in the same fight (pack 2, 1x, 1,000 flips; 343,373 uploads of 455 million pixels and 3.6 million CLUT loads): naming per draw is counted only for the naming log (the renderer asks for names itself); GS addresses come from a table of each page's layout (checked against the sink's functions on 200,000 random cases); uploads are written and textures read out row by row; an upload or CLUT load that repeats the last one into unchanged memory is skipped (5% and 22% of them). The naming's share of the game thread's samples fell from about 34% to about 13%, the run's samples from 12,434 to 9,772, with the same 77,300 draws replaced; in the window GS handling went from 15 to 16 ms to 10.6 to 11.3 ms per picture at 1x.
5. **Game code and services** (about 5 to 7 ms): the other access widths and the per-call checks, guided by a new profile.
6. **Loading hitches at screen changes:** spread the load over fields or do it off the game's thread.

Each step: measure with `BT2_VIEWER_STATS` over the extended fight run (7,400 frames of `pad-fight.txt`), compare `vu1_kick_hash` and the run's counters with the baseline (5,100 frames, in the handoff), run the gate, record before and after in the monthly history.

## Hand-written native programs

The translations run a program pair by pair: every register lives in memory, every pair keeps the interpreter's bookkeeping, and integer instructions, branches, the division and the conversions still call the interpreter. A program written by hand as what it does (a loop over vertices: transform, perspective division, clipping test, conversion, output list) keeps its values in SSE registers, has no per-pair bookkeeping and can process the vertices of a list without the program's own pipelining. That is the route to the low-end target for VU1, and it is the project's own code: it is committed, unlike the translations.

- **Where the time goes** (extended fight, measured per program with `BT2_VU1_CENSUS`, which now writes `cost.txt`: key, pairs, starts, pairs run, milliseconds including the output packets): of 14 programs five carry about 94% of VU1's time. Two have about 90 pairs each and run 1.2 and 0.9 billion pairs; two have about 377 pairs and run 0.86 and 0.71 billion; one of 89 pairs runs 0.23 billion. The small start stubs (6 to 36 pairs) cost almost nothing. The fight starts a VU1 program about 12 million times in 7,400 frames.
- **Reading a program:** `python tools/vu1/disassemble.py <census file>` lists it as text. The listing is the game's program: read locally, never committed.
- **What a hand-written program must keep:** the output packets byte for byte (`vu1_kick_hash`), and every piece of state a later program can read: the float and integer registers it leaves behind, ACC, Q, the clipping flags, data memory, the instruction count. Where the original leaves the values of its last loop pass in registers, the native version leaves the same values.
- **Checking:** the verification mode (`BT2_VU1_VERIFY=1`, `Vu1::verify_native`; done) runs the interpreter and the native program on the same state for every start and compares the packets and all of that state afterwards, so a difference is reported at the first start that shows it, with the program's key and what differs. On the translations it passes for all 2,140,195 starts of the baseline run. Then the run's hash and counters against the baseline, with and without `BT2_VU1_INTERPRET`.
- **Selection:** as now, by the key of the program reachable from the start address. A hand-written program is registered under its key in the runtime (a key is a number, not content) and takes precedence over a translation; a program without one keeps its translation, and without either the interpreter.
- **Done 2026-10-13:** three hand-written programs cover six keys (each program is started at its first pair or through a two-pair jump, which makes two keys): skinned vertices from two bones to two output lists, the same to one list, and the triangle strip with the view volume test, including its clipping of a triangle against the six planes. Files: `runtime/vu/vu1_handwritten_registry.cpp` (keys), `vu1_handwritten_helpers.h`, `vu1_skinned_vertex_programs.cpp`, `vu1_triangle_strip_program.cpp`, `vu1_triangle_clipper.cpp`. Registered programs are looked up before the translations (`Vu1::handWrittenPrograms`; `BT2_VU1_NO_HAND_WRITTEN=1` leaves them out; the report counts `vu1_hand_written_runs`). The verification mode and the registry have unit tests with programs of the test's own.
- **Why they are fast:** in the run's float mode (round toward zero) sums and products of finite values stay finite, so the model's treatment of infinities and NaNs is needed only once per value read from memory, not around every operation; values live in SSE registers; the Q and clipping pipelines are resolved by reading the program (no read falls inside a pipeline's latency).
- **Measured** (scripted fight, 5,100 frames, headless, the developer's PC; all with hash `5a4960667d4a3dff`): the VU1 programs with their packets cost 6.07 s before and 2.07 s now, of which about 1.5 s is the GS taking the packets; VU1's own work fell from about 4.1 s to about 0.55 s. The whole run takes 20.3 s interpreted, 19.0 s with the translations alone and 14.2 to 14.7 s with the hand-written programs. Verification passes on all 2,140,195 starts (1,638,737 hand-written; the triangle clipping ran 107,223 times).
- **Two general gains found on the way:** (1) the compiler does not inline a function into code compiled under other float settings, so the interpreter called every small helper; its float pragmas now come before the project's headers. (2) An XGKICK packet is handed on in place (a span of data memory) unless it wraps around the end of memory; before, it was copied quadword by quadword.
- **Left to the translations:** a variant of the strip program (428 pairs, 720 starts in the run) and the small start stubs; together under 0.3 s.
- **Where the run's time is now** (`BT2_PROFILE_TOTALS=1`, same run): the GPU renderer's intake of GIF packets 3.7 s, its flush 0.9 s, this runtime's GS packet checking 0.65 s, VIF1 without VU1 and GS about 0.6 s, VU1 about 0.75 s; the remaining 8 s are game code, the films and startup. The renderer's intake is the library's own work per packet (this runtime calls its GIF entry point directly); taking it off the game's thread is the graphics thread item below, not a rewrite.

## Native VU1 programs: how they are made

1. **Census:** run with `BT2_VU1_CENSUS=<existing folder>`; every distinct program the run starts is written there (`<key>.vu1`). `work/perf/vu1-census/` holds the fight run's.
2. **Translate:** `python tools/vu1/translate_programs.py <census folder> game/generated/vu1/vu1_native.cpp`. A program the translator cannot follow exactly (a branch or an E bit in a delay slot, a debug bit) is left to the interpreter, with the reason printed.
3. **Build:** configure again so CMake picks the file up (it is chosen when it exists), then build.
4. **Check:** the report's `vu1_native_runs` against `vu1_runs`, and `vu1_kick_hash` against a run with `BT2_VU1_INTERPRET=1`.

A program met later that is not in the census is simply interpreted; `vu1_runs` minus `vu1_native_runs` shows how much that is. New programs are added by a new census.

## Verification

- Unit tests for VU1 and VIF1 (`bt2-vu1-tests`), the gate's frame comparisons and renders.
- The output hash and instruction count on the fight script.
- The user's observation in the window, with sound: no gaps where the run holds the game's speed.
