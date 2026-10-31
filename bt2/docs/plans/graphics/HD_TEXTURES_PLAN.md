# HD textures

Requested by the user on 2026-10-20 as the more important of two enhancements (the other is [60 fps battles](HIGH_FRAME_RATE_PLAN.md)): load HD replacement textures, made for PCSX2, in place of the game's own. Supersedes the "HD textures" section of the [launcher plan](../launcher/LAUNCHER_PLAN.md), which stays the place for the setting.

## The packs (investigated 2026-10-20)

Two local packs, `work/HD_TEXTURES` (never committed; their threads are credited in the README when the feature ships, as the credits files there ask):

| | Pack 1 | Pack 2 |
| --- | --- | --- |
| Files | 1,244 PNG, 191 MB | 15,097 DDS, 8.3 GB |
| Made for | the "DX Mod" of SLUS-21441 (a modified game, per its thread's title) | the retail SLUS-21441 |
| Layout | one `replacements` folder | `replacements` sorted into Characters (bodies 5.6 GB, fight, select and turn pictures, posters), Stages (768), Others (998), Text (266), Tutorial (186), Buttons (50) |
| Format | PNG, mostly 64 to 1024 pixels a side | BC3 (DXGI 77) with full mip chains, mostly 256 to 1024, up to 2048 |
| Names shared with the other pack | 0 | 0 |

**Pack 2 is the one to use** (the user's preference, and the evidence agrees): it is made for the disc we run, covers characters, stages, text and the tutorial instead of a selection, and is already block-compressed with mips, which the GPU samples directly. Pack 1's textures come from a modified game, so their hashes are not expected to match retail data at all; the name check (step 1) confirms or refutes this with numbers.

Pack 2 details that need a decision:

- Its `replacements/Buttons` are the Xbox button art (byte-identical to its `Xbox Layout` folder); `Original Buttons` holds the same 47 names with PlayStation art. Offer it as a setting (PlayStation by default).
- 159 names appear in two subfolders. Which file wins must be fixed (and reported), not left to directory order.
- Almost all textures are 4-bit (12,743) or 8-bit (2,245) paletted; 12 names have no palette part.

## How a texture gets its name

PCSX2's texture replacement (pinned `GSTextureReplacements.cpp` and `GSTextureCache.cpp`, commit `81526d4d`) is the behavioural reference. The behaviour to reproduce, in our own code:

- File name `<texture hash>-<palette hash>-<format word>` in hexadecimal (`<texture hash>-<format word>` without a palette; `-r<w>x<h>` variants for clamped regions, which pack 2 does not use).
- **Texture hash:** XXH3-64 over the texture's GS memory as the GS stores it: the 256-byte blocks of the TW × TH rectangle in block order. A texture smaller than one block, or one whose format does not fill all 32 bits of a pixel (24-bit, the 8H/4HL/4HH formats), is hashed after reading it out to rows instead (indices for paletted formats, 32-bit pixels otherwise), so neighbouring data in the same blocks does not change the name. Mip levels are hashed into the same state only when mipmapping is on.
- **Palette hash:** XXH3-64 over the palette as the GS has loaded it, expanded to 32 bits per entry (16 entries for 4-bit, 256 for 8-bit, from CSA on).
- **Format word:** PSM (6 bits), TW and TH (4 bits each), one unused bit, and for non-paletted formats with alpha expansion TEXA's TA0 (8), AEM (1) and TA1 (8).

XXH3 is a public hash with a BSD-licensed reference implementation (a single-header library); it would be a new external dependency.

## How to draw with them

The GPU renderer is paraLLEl-GS (snapshot `cc6184af`). It keeps a cache of textures: when a draw needs a texture not in the cache, `create_cached_texture` makes an RGBA image of the texture's rectangle and a compute shader fills it from GS memory, palette applied. The ubershader samples these images with normalised coordinates (texel coordinates times the reciprocal of the texture's size), with clamping and region modes in texel units of the original size. So an image with more pixels, holding the same picture, is sampled correctly by the same coordinates. That is the hook:

1. When a texture is created, compute its PCSX2 name; if the pack has it, bind the HD image instead of uploading. Its size stays the original's in the texture's info, so coordinates and clamps are unchanged.
2. Where paraLLEl-GS caches only a sub-rectangle of the texture (clamp regions), the HD image covers the full TW × TH: the offset and scale go into the texture info.
3. Its exact bilinear path (fetching neighbouring texels with a phase table, for clamped textures) steps one original texel; with an HD image it should use the hardware's linear filter instead.

paraLLEl-GS is a vendored snapshot; the change is carried as a patch in the repository (LGPL-3.0 allows it), kept as small as possible.

**The names need GS memory on the CPU.** paraLLEl-GS holds GS memory on the GPU. Reading it back for each new texture would stall the GPU. The CPU GS model already writes image transfers into its own copy of GS memory; it stopped doing so in GPU-only runs for speed ([performance plan](../ee/PERFORMANCE_PLAN.md), "Done"). With HD textures on, it does so again, and hashes from that copy. Textures the game renders itself (render targets read as textures) are not in the copy, and PCSX2 does not replace those either.

**Video memory (the user, 2026-10-20: at most 8 GB of VRAM in all).** Textures stay BC3-compressed on the GPU (one byte per pixel; a 1,024 × 1,024 texture with its mips is about 1.4 MB, 512 × 512 about 0.35 MB). A scene draws a few hundred distinct textures (437 on the Ultimate Battle Z screens and a fight, 633 in a versus battle), under 1 GB even if all were 1,024 × 1,024. The HD textures get a fixed budget, 2 GB by default and adjustable in the settings, with least-recently-used eviction; the pack is never loaded whole. With the renderer's own memory (a few hundred MB, more at 16× super-sampling) the total stays well under 8 GB at the highest settings; the budget and the renderer's use are measured in step 4.

**Loading:** 8.3 GB cannot be resident. An index of names is built at start (15,000 file names, no reads); a texture is loaded on its first use on a worker thread (the game draws the original until it is ready, as PCSX2 does), into GPU memory with a size budget and least-recently-used eviction.

## What it will look like

paraLLEl-GS does super-sampling (2× to 16×), not free upscaling. With 4× or more and `high_resolution_scanout`, it presents at twice the resolution (1,280 × 896) and samples textures at up to four times the density per axis, so HD textures show real detail there. That is the ceiling: output at 1440p or 4K the way PCSX2's hardware renderer upscales is not something paraLLEl-GS does. How far that can be pushed, and widescreen, are the [image quality plan](IMAGE_QUALITY_PLAN.md). Its notes also warn that a game which copies its frame to the display buffer through a texture loses the super-sampled detail; whether BT2 does is checked in step 4.

## The name check (step 1, run 2026-10-20)

`BT2_TEXTURE_NAMES` runs, compared with both packs by `texture_name_check.py`:

| Scene (2,200 to 3,000 flips) | Names drawn | Pack 2: same name | Pack 2: same texture hash | Pack 1: same name |
| --- | --- | --- | --- | --- |
| Ultimate Battle Z screens and a fight | 437 | 188 (Others 95, Text 51, Stages 27, Select pictures 7, Buttons 3, ...) | 225 | 153 |
| Versus battle (character 4 against 70, stage 22) | 633 | 92 | 146 | 78 |
| Story episode 0 (map) | 97 | 41 (Stages 35) | 58 | 5 |

- **Our names are PCSX2's.** Texture and palette hashes agree with pack 2 bit for bit. Pack 2 was dumped by an older PCSX2 that also wrote TCC into bit 14 of the format word; PCSX2 clears that bit when it reads a pack's names, and so does the tool. A name without a palette part stands for every palette, as in PCSX2.
- **Pack 2 is the pack.** Pack 1 also matches (it is not limited to the modified game), but it is a fraction of pack 2's coverage and has no character bodies.
- **The characters' palettes are drawn by the game, and relit all the time.** Each frame it uploads base palettes (8 × 2 pixels at blocks `0x3C40`, `0x3C80`, `0x3B00`, `0x3B40`, ...) and draws the whole palette pages `0x1E0` and `0x1D6` with two 64 × 64 sprites (the second blended in at a fixed weight), from which the body textures load their CLUT (2026-10-21, a fight on stage 8). Read back exactly as drawn, a body texture takes 103 to 107 different palettes in 2,000 flips; pack 2 holds 4 to 14 per texture, and 2 of ours match. So exact names, PCSX2's way, make the bodies HD only when their lighting matches one the pack's author captured; PCSX2 behaves the same. About 44 % of a fight's draws are a 256 × 256 shading texture pack 2 does not hold.
- **Not named:** 16-bit (PSM 2) and depth-format (PSM 0x32) textures, about 7,300 draws in a battle: frame effects the game renders and reads back as textures; no pack holds them and PCSX2 does not replace render targets.

## Drawing with the replacements (step 3, built 2026-10-20)

- **Names in step with the renderer.** The renderer takes a texture from GS memory when a draw needs it, but the GS sink used to hand it whole transfers after reading them, so the names' copy of GS memory ran up to a transfer ahead (BT2 streams about 340 KB of textures a frame through the same memory). With names attached, the sink hands each transfer on in pieces that end before an upload's data and before a CLUT load (`GsCommandSink::forwardBefore`), so the copy and the renderer agree whenever the renderer asks for a name.
- **The renderer patch** (`tools/graphics/gpu/parallel_gs_texture_replacement.patch`, applied by `fetch_parallel_gs.py`, listed in `config/parallel_gs.lock.json`): `GSInterface::texture_replacement` is asked before a texture is cached from GS memory; a block-compressed image it returns is registered with the page tracker like a texture from GS memory (dropped when the game overwrites that memory), addressed as the whole 2^TW × 2^TH texture at the original size, never put back into the renderer's image pool and never copied as a promoted frame. Textures with super-samples are not replaced. (A detail-over-the-game's-colours method, 2026-10-21, showed artifacts in the menus in play and was dropped at the user's direction: replacements are drawn instead of the originals, as PCSX2 does.)
- **Names as PCSX2's (2026-10-21, from Tenkaichi3Decomp's findings on its pack):** a palette texture is looked up first with the palette hash over the entries from the lowest to the highest index the texture uses (older PCSX2 versions, which made pack 2), then over the whole palette (newer ones), then the pack's name without a palette part. Palettes the game draws on the GPU are taken from a copy of their page made after each frame without waiting (`GsTextureNames::drawnPalettePages`, `takeDrawnPage`, `GpuGsBackend::copyDrawnPalettes`): the pages are redrawn whole each frame and change slowly, so this names as exactly as a read that waits (measured: the same 22.2 % of a fight's draws found, against 4.5 % without drawn palettes; the waiting read cost 15 ms a frame at 16x). Left (Tenkaichi3Decomp's rules for drawing replacements, if the user's look in play shows the need): always filtered, no mip levels for 2D art, held to the piece's rectangle of its sheet, alpha kept at the original's largest.
- **A paraLLEl-GS fix in the same patch.** Its whole-loop path for PACKED draws (ST or UV, RGBAQ, XYZ2) took the loop count from the size of the whole transfer instead of what is left of it, so a transfer ending inside a draw packet was read past its end and the renderer lost its place in the GIF stream (garbage registers, a lost device). Transfers that end inside a packet were rare before the lockstep; `bt2-gpu-gs-tests` sends a draw in pieces with poison behind each and fails without the fix.
- **Loading between frames** (`runtime/gs/gpu/hd_texture_pack.*`): the index of the pack's 14,830 names (170 duplicates, the first in path order kept; with PlayStation buttons the `Original Buttons` art) is built at start; a replacement asked for is loaded after the renderer's flush at the next frame boundary (creating images while the renderer takes GS data is avoided), and the original is drawn until then. DDS in BC1, BC2, BC3 or BC7 with their mips, read as UNORM; the budget drops the least recently used.
- **Measured** (Release, `BT2_HD_TEXTURES`, Ultimate Battle Z screens and a fight, 2,000 flips): 57,784 draws replaced, 159 replacements loaded, peak 115 MB of the 2 GB budget, no renderer errors, 25 s (12 s without the pack). Pictures at the same moments with and without the pack: `work/frames/hd-textures/` (the stage's grass, rocks and tree and the select screen's portraits sharper at 640 × 448).

## Order of work

1. **Name check (decides everything after it) — done 2026-10-20** (above); left from it: the drawn palettes. A diagnostic in tool runs: for each texture the GPU renderer creates, compute the PCSX2 name and report how many are found in each pack, per screen (title, menus, a battle, story). Expect most of pack 2's matching textures to be found on the screens that use them, and next to none of pack 1's. Compare a few names by hand with PCSX2's own dump of the same screen if the counts are low.
2. **XXH3 and the hasher** as a runtime unit with tests (block order per format, small-texture and partial-format paths, palette expansion, format word), checked against names in pack 2.
3. **The paraLLEl-GS hook — built 2026-10-20** (above); left: the drawn character palettes, the button art setting. (above) with BC3 images and their mips; the button-art choice; the duplicate rule.
4. **Super-sampling setting** (off, 4×, 8×, 16×) and high-resolution output, measured in a battle in Release (GPU and processor time per picture).
5. **The user's inspection:** pictures of the same scenes with and without the pack in `work/frames/`.
6. **Settings — built 2026-10-20** (HD textures and super-sampling under Display, the pack in `data/textures`; the button art choice still to come): the launcher's and the overlay's "HD textures" entry becomes available, with the pack folder `data/textures` next to the programs (like the disc image) and the button art choice.
