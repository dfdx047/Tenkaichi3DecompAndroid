# Resume handoff

Updated: 2026-10-21. **No pass in progress.** The last pass (ended at the user's request) removed the CPU renderer, made HD textures a PCSX2 drop-in again and made 60 fps steadier. Next steps are proposed below and start when the user says.

## Where the work stands

The whole game runs without input (all modes, all 200 story parts, 73 towns; [coverage plan](../plans/ee/WHOLE_GAME_COVERAGE_PLAN.md)); what each part was checked on is in [VALIDATION.md](VALIDATION.md). paraLLEl-GS is the only renderer (the CPU renderer was removed 2026-10-21; the GS command sink checks packets and follows registers, and the startup checks read its record of the accepted clears). Settings in the launcher and the in-game menu: HD textures, super-sampling, widescreen, 60 fps battles (all from the next start), integer scaling, window size, fullscreen.

**Performance (this PC, Release, a versus fight in the window):** every enhancement alone holds 30 flips per second, and so do 16x with HD textures and widescreen (about 28 ms of the game thread's 33 a flip) and 60 fps without HD (24 ms). **HD textures with 60 fps do not fit yet:** 22 to 27 flips per second at 4x to 16x (33 to 40 ms). The game's single thread is the limit; the GPU and the other cores are lightly used.

**HD textures — a PCSX2 drop-in** ([plan](../plans/graphics/HD_TEXTURES_PLAN.md)): replacements are drawn instead of the originals (`parallel_gs_texture_replacement.patch`), found by PCSX2's names: the palette hash over the indices the texture uses first (older PCSX2, which made pack 2; found by Tenkaichi3Decomp), then over the whole palette, then the pack's name without a palette. Palettes the game draws on the GPU (the fighters' lit palettes) are copied after each frame without waiting. A fight: 22.2 % of draws replaced (4.5 % before); about 45 % are one shading texture pack 2 does not hold; the fighters' bodies are HD only while their lighting matches one of the pack's variants (their palettes change about a hundred times in a fight against the pack's 4 to 14 per texture), which PCSX2 cannot do better. Dropped on the way: the detail-over-the-game's-colours method (artifacts in the menus), any variant drawn as it is (wrong colours), the nearest learned variant (slow), a palette read that waits (15 ms a frame).

**60 fps battles** ([plan](../plans/graphics/HIGH_FRAME_RATE_PLAN.md), step 3): the fight stays at the game's 30 steps; a second renderer on a worker thread draws a picture between each two fight frames from the main renderer's GPU state and the game's VIF1 lists blended halfway (aligned unpack by unpack; bone and camera axes blended as turning, as Tenkaichi3Decomp does). No in-between picture across a camera cut (a view axis turning by more than 25 degrees); none is waited for (one not ready is left out). Exact by check (`BT2_SIXTY_FPS=check`). Plain fight: 30 flips and about 60 pictures per second (1,269 in-between pictures in 1,500 flips, 188 camera cuts, 22 left out). Left: the cost with HD textures; the camera-cut threshold (188 cuts may be too many); things only the newer frame has (hit flashes) appear one picture early; a state-hash check that the fight is unchanged with the setting on.

**Not seen by the user yet** (built, gate passed): HD textures as a drop-in and the 60 fps rework; the play test fixes of 2026-10-21 (the in-game menu no longer freezes: the picture read for its background flushes the renderer first; input read after the field's wait, a field fresher; stretched pictures enlarge by whole multiples before filtering, so thin lines such as the Cell Games floor no longer pulse; integer scaling with widescreen and 4x or more super-sampling fills the window); the sound fix (speech was heard on the left only); fast forward; saving; special moves; the films with and without a skip; the launcher's screens; the camera in fights.

**For the user's tests now:** HD textures with 60 fps off (menus, stages, text; the bodies switching as in PCSX2); 60 fps with HD textures off (smoothness, no shrunken fighters, no stalls); the fixes above.

## Next (proposed to the user 2026-10-21; start when the user says)

1. **A GS thread** (the user: "the gpu and cpu are not working hard"). The game's thread does everything; the GS work (packet checks, texture naming, feeding paraLLEl-GS: 10 to 14 ms a flip in a fight) moves to its own thread, fed in order by the game's thread, which then only runs the game. Expected: about half of the game thread's time freed, room for HD textures with 60 fps at 16x. Then, with care, VU1 on a worker (the game sometimes waits for VU1's results), and more memory: preload the HD pack, or a fight's textures, into RAM and video memory at a fight's start (no HD pop-in; a few GB of RAM).
2. **60 fps with HD textures:** profile the extra 10 ms (suspected: both renderers name every texture; the second could take the main one's names); tune the camera-cut threshold; the newer frame's own things (hit flashes).
3. **HD coverage beyond exact names: a colour-matched variant for the fighters.** When a body's palette is not in the pack, compare the game's current colours of the texture with each variant's colours at the texture's size (a small summary, once per new palette) and draw the closest variant as a normal replacement, no shader change: the right character and form, with the variant's captured lighting; bodies HD nearly always. (The shading texture the pack lacks stays original.) If the user's look at the menus shows artifacts: Tenkaichi3Decomp's sampling rules for replacements (always filtered, no mip levels for 2D art, held to the piece's rectangle of its sheet, alpha kept at the original's largest).

**After this:** FSR 1 scaling with sharpening and an edge filter ([image quality plan](../plans/graphics/IMAGE_QUALITY_PLAN.md)); the button art choice ([HD textures plan](../plans/graphics/HD_TEXTURES_PLAN.md)); the final battle sweep; the runtime generalization plan's groups 4 to 6 at the very end (the user's decision).

**Not compared with the original yet:** memory card replies, GS readback pixels, 3D frames, VU1 memory. The README's system requirements are estimates from this PC's measurements.

**References checked out locally** (ignored `work/references/`): Tenkaichi3Decomp at `aec1f8a` (`t3d/`: `docs/`, `port/`, `scripts/` only, never its decompiled `src/`), BT3-Recomp at `e22a34b` (`bt3r/`). Their port notes on texture packs and in-between pictures: `t3d/docs/port/README.md`.

## How the user runs things

- **Game folders:** next to `bt2-game.exe` (in `build\probe\Release`), `data` holds the disc image and `data\textures` the texture pack (pack 2 is linked there on this PC); `saves` holds the cards; the settings stay in `user\settings`, the last run's messages in `user\last-run.log`.
- Helper scripts in `work\scripts\` (`work\WORKING_WITHOUT_AI.md`): `play.cmd`, `discover.cmd` (a stop at untranslated code: maps it and rebuilds), `rebuild.cmd`, `gate.cmd`, `show-log.cmd`.
- In the game: Escape, the PS button or both stick buttons open the menu; Tab is fast forward.

## How to run (automated)

- **Gate:** `python tools/gate/run_gate.py --disc <iso>` (builds, unit tests, Python suites, probe runs with the original comparisons, the Release battle run against `5a4960667d4a3dff`).
- **Without input:** `BT2_START_BATTLE`, `BT2_START_SCREEN`, `BT2_START_EPISODE` with `--pad-script ""`; sweeps: `battle_sweep.py`, `story_sweep.py`, `screen_run.py` (at most 6 jobs).
- **Pictures:** `BT2_GPU_GS_SNAPSHOT="<flip>:<absolute path>;…"`. **Audio:** `BT2_AUDIO_WAV=<absolute path>`. **Did the game change:** `BT2_STATE_HASH` on both runs, then `compare_state_hash.py`.
- **Picture settings:** `BT2_HD_TEXTURES`, `BT2_SUPER_SAMPLING`, `BT2_GAME_PATCHES=widescreen`, `BT2_SIXTY_FPS=1` (`check`: blend 1, must equal the main picture; `replay`). **Texture coverage:** `BT2_TEXTURE_NAMES=<file>`, then `tools/graphics/textures/texture_name_check.py <file> <pack>`. **60 fps study:** `BT2_VIF1_DUMP`, `BT2_VIF1_REPLACE`, `BT2_FIGHT_UNCAPPED`. **Cost:** `BT2_VIEWER_STATS`, `BT2_GPU_TIMESTAMPS`, `BT2_SAMPLE_PROFILE` ([TOOLS.md](../guides/TOOLS.md)).
- **Build times:** a runtime header about 16 s; everything about 6 minutes after a change to `ps2_runtime.h`, `ps2_runtime_macros.h` or `ps2_fpu.h`.

## Rules learned the hard way

- **Build the player programs before handing a build over;** a build fails to link while a game window is open.
- **Runs with a window appear on the user's screen** (`BT2_VIEWER_CLOSE=1`); runs with `--sound` play on the user's speakers.
- **Every VU1 or VIF1 change keeps the gate's baseline** (`tools/gate/battle_run.py`).
- **After a change to `prepare_runtime.py`:** run it (and `generate_interpreter.py --reuse`), then build.
- **Shell heredocs corrupt backslashes:** write such files with the file tools.
- **Commits carry no assistant attribution.**
