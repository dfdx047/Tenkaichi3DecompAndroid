# Graphics

Sources: `src/sys/dma.c`, `gfx.c`, `gfx_ot.c`, `fade.c`, `vu1_packet.c`. Layouts and register
macros: `include/sys/dma.h`, `gfx.h`, `gfx_ot.h`, `fade.h`, `vu1_packet.h`.

`gfx_ot.c` and `vu1_packet.c` each have one function still in assembly.

## Display lists (verified; purposes inferred from hardware addresses)

- Two 1 MB display-list buffers; `gDmaCur` is the write cursor. `Dma_Flush` appends FINISH and an
  END tag, sends on VIF1 and swaps buffers.
- `Dma_AddData` copies a ready packet; `Dma_AddRef` / `Dma_AddNext` write chain tags;
  `Dma_BeginDirect` / `Dma_EndDirect` bracket a packet built in place (and flush the cache on
  every packet).
- Register packets, one function each: tex flush, frame, clamp, scissor, xy offset, zbuf (always
  24-bit), tex0, tex1, fba, colclamp, alpha.
- Rectangles and textured quads are drawn as strips of 32-pixel-wide sprites
  (`Dma_AddFillRect`, `Dma_PutTexStrips`).

## Frame buffers (verified)

- Two 512x448 32-bit colour buffers (GS pages 0x00 and 0x70) and one shared 24-bit depth buffer
  (page 0xE0). Larger Z is nearer; the clear writes Z = 0.
- XYOFFSET (1792, 1824); scissor 0..511 by 0..447.
- NTSC interlaced, field mode. `Gfx_Init` first builds a throwaway 640x448 environment only to
  clear video memory at boot.
- Buffer selection uses bit 0 of the frame counter (`gGfx + 0x234`); `gGfx + 0x230` is the
  field parity flag.
- Display: both read circuits show the same buffer one line apart, blended 50/50 (a flicker
  filter; PMODE 0x7F23). MAGH 5 stretches 512 to 2560. `DX = 636 + 4 * screenX`,
  `DY = 50 (51) + 2 * screenY`, with the offsets taken from the save.

## Default render state (verified)

`Gfx_AddDefaultEnv` / `Gfx_PutDefaultEnv` write, for context 1: TEXA TA1 = 0x80; default offset
and full-screen scissor; FRAME = current buffer; TEST 0x50000 (depth test on, GEQUAL, no alpha
test); FBA 0; ZBUF with writes on; ALPHA 0x44 (standard blend); CLAMP clamp/clamp;
PRMODECONT 1; COLCLAMP 1; TEX1 0x60 (bilinear).

`Gfx_ClearScreen(fbmsk, rgba)` fills 512x448 with Z writes off.

## Ordering table (verified layout and order; depth direction inferred)

- 0x1002 slots (a first slot, 0x1000 depth slots, a last slot), two layers each. Each chain
  starts with a head packet setting the blend: layer 0 is 0x44 normal / 0x42 subtractive,
  layer 1 is 0x48 additive / 0x49.
- Packets come from a 0x73000-byte pool.
- `Ot_Draw` links the non-empty chains in table order, sends an environment packet, all the
  data, then a restore packet, and resets the table.
- Low slot index = far (painter's order) is inferred. The table's init/term are called from the
  battle side, so it appears to be battle-lifetime.

## Battle draw passes (verified order)

`Gfx_MarkPass(n)` is an empty debug marker. Order in `Battle_Draw`: stage (0x115950, 0x115DE0);
0x247578; 0x10FF40; 0x247660; default env + scene manager draw (0x12CCD0) + `Ot_Draw`; 0x247688;
HUD (`BtlGame_Draw`); 0x23A2B8; 0x2476D8. In split-screen only the stage pass and the scene
manager pass are repeated per view; everything else is drawn once. What the 0x247xxx passes draw
is weakly supported ("effects").

## Fades (verified)

`gFade[3]`, 0x60 bytes each: current colour, from, to, delta (four floats each), a ramp, flags
(1 fade-out started, 2 fade-in started, 4 done, 8 paused).

- Slot 0 is black; slots 1 and 2 are white. Alpha runs 0..128. The colour belongs to the slot.
- `Fade_Start(slot, dir, seconds)`: dir 0 covers the screen, dir 1 uncovers it.
- A finished fade-out stays opaque until reset or faded in; a finished fade-in turns itself off.
- `Fade_IsDone` returns 1 one frame before the final colour is computed, and also for a slot
  that is off.
- Slots 0 and 1 are drawn by `Gfx_EndFrame`; slot 2 is drawn from the battle side (inferred: the
  white flash on a stage change).
- `Fade_Init` only clears slot 0; the others rely on zeroed memory.

## VU1 packet layer (verified code, guessed names)

Nine VU1 microprograms sit right after the code (0x2BF6B0..0x2C3380): programs 0, 1, 2a, 2b, 4, 5, 6, 7, 8
(there is no program 3). They are assembly listings in `src/vu1/` and are described in
[vu1/README.md](vu1/README.md). `Vu1Pkt_LoadProgN` uploads a microprogram and its constant block;
`Vu1Pkt_CallProgN` builds the call packet in front of an object's vertex chain. `Vu1Node_*`
walk a model node list. Callers are the 3D renderer at 0x111358..0x115DE0 (not decompiled).

## For a port

A renderer needs: 512x448 internal resolution, a greater-or-equal depth test with larger Z
nearer, bilinear filtering by default, four blend modes selected by ordering-table layer, and
painter's-order drawing through the table. The flicker filter and the field handling can be
dropped. The model and texture formats are not understood yet; the microprograms are (see vu1/README.md).

## Movies (`src/sys/movie.c`; verified unless marked)

- A movie is two loose files opened by name, not archive entries: `zs3usop.pss` + `.adx`
  (opening) and `zs3used.pss` + `.adx` (ending). Only the video stream of the `.PSS` is used.
- Path: disc read (0x100 sectors) into one of four buffers, demux with Sony's libmpeg into a
  ring buffer, decode one picture, and upload it straight into the frame buffer as 896
  transfers of 16x16 blocks. The movie is never a textured quad.
- One picture per two vertical blanks. There is no time-stamp sync with the audio: if the
  player waits on the disc for more than four fields it pauses the audio and resumes it after.
- START on pad 1 skips with a 0.5 s fade; pad 2 is not read.
- Buffers (about 3.8 MB) come from the main heap and are freed on close.
- `Movie_PlayEnding` has no direct callers in either binary.
- For a port (inferred): decode the video with any MPEG-2 decoder at 512x448, play the ADX
  from the first frame, and sync video to the audio clock.

## 2D sprites (`src/sys/sprite.c`; verified code, format partly inferred)

A sprite sheet is a BPE-packed file whose word at +0x10 points to 0x40-byte texture entries.
`Sprite_DrawList` draws `LoadSprite` records as blended GS sprites; each quad uploads its
texture first, so one texture is resident at a time. The pixel layout belongs to the uploader
`func_0010A218`, not decompiled.

## Colour overlay (`src/sys/color_fade.c`; verified)

A fourth screen overlay besides the three fade slots: an integer alpha 0..0x80 stepped per
update, drawn as one blended 512x448 sprite. Used only by the trailing memory-card block.

## Controller-removed overlay (`src/sys/pad_watch.c`; verified)

`PadWatch_Update` and `PadWatch_Draw` run every frame from `Gfx_EndFrame`: presence of both
pads is debounced (about 7 frames) and a dimmed screen with a message from the boot file is
drawn when a required pad is missing. Loading screens disable it. It also pauses the battle;
see netplay_notes.md.

## Text printer and button icons (`col_c.c`, `col_c_b.c`, 0x239EA0..0x23D1E8; verified unless marked)

122 of 124 functions matched when first reported; linked since ([upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md) lists what is still assembly). The source file really starts at
0x239BB0 (five helpers at the end of the neighbouring col_b file). Drawing only: no random
draws, no pad, camera or sound input.

- **Queue model**: `Font_Print*` only queue a 0x54-byte command with a copy of the current
  style (120 commands at most, no bound check). `Font_Flush(first)` draws commands
  `first..count` as one GS packet and truncates the queue; `Font_FlushAll` is `Font_Flush(0)`.
  Strings are stored by pointer, so they must stay valid until the flush. Caller idiom:
  `n = Font_GetCmdCount(); Font_PushStyle(); ...; Font_PrintAt(...); Font_PopStyle();
  Font_Flush(n);`.
- **Command kinds**: 16-bit string, 8-bit string, single character, decimal number with a fixed
  digit count. Only 16-bit strings expand tags.
- **Style** (0x48 bytes, five-deep stack): scales, colour, icon colour, spacing, alignment per
  line, flags, shadow mode (1 one copy, 2 eight copies, 3 four copies), clip box, callbacks.
- **Fonts**: seven slots; the boot file's two fonts go to slots 0 and 1. Glyph lookup is a
  binary search over 6-byte records; glyph cells are at most 31 x 31 texels.
- **Tags in text**: `<PAD=names>` draws controller button icons, `<PADS=a,b,c,d>` one list per
  controller type (animated), `<COL=RRGGBBAA>` / `<COL=DEF>` changes or restores the colour,
  `<UB0>` a special character. The icon animation clock (`FontIcon_Tick`) is advanced from
  `Gfx_EndFrame`, once per 30 Hz frame.
- Corrections: `func_0023A458` / `func_0023A2D0` (used in pad_watch.c, btl_seq.c) are
  `Font_GetCmdCount` / `Font_Flush`; the "unidentified inits" at boot, `func_00239FF0` and
  `func_0023D0E0`, are `Font_Init` and `FontIcon_Init`; `func_0023D160` is `FontIcon_Tick`.

## Confirmation dialog (0x268248..0x269228; src/sys/dialog.c, linked; names in config/symbols/dialog.txt)

A two-choice confirmation window ("Yes / No"), one instance, `gDialog` at 0x2FF160 (0x8C bytes
on heap 2). 18 of 19 functions match; `Dialog_SetCursor` differs in registers only and is
INCLUDE_ASM. The `Dialog_` prefix is our choice. The file was written as `lib_a.c`.

- (verified) It is a UI animation object with clips `mc_dummy_text_1..4` (text anchors) and
  `mc_menu_plate_1/2` (the two choices), and labels `fl_window_{s,l}_{in,out,open}`,
  `fl_on_start`, `fl_off_start`, `fl_ok`. Struct layout in include/sys/dialog.h.
- (verified) `Dialog_Input` returns 1 for the first choice, -2 for the second, -1 for cancel,
  0 otherwise. Pad: game-button repeat bits 1 / 2 move the cursor, pressed 0x200 confirms,
  pressed 0x400 cancels. Blocked while `gProgress->flags & 0x100`, while the window is not
  fully open, or when there are no choices.
- (verified) Sound effects, `Snd_PlaySe(1, n)`: 0 cursor, 1 confirm, 2 cancel, 4 open, 5 close.
- (verified) Message tables are `u32` offset tables: text i starts at
  `table + (table[i+1] & ~3)`, UTF-16 strings for the Font module.
- (verified) Resource file: a table of byte offsets; words 1, 4, 2, 3 are texture parts
  (relocated with `Res_RelocateOffsets`), word 5 the animation data, word 6 the title table.
- (inferred) Its callers at 0x119FB8..0x11E1E8 are the memory-card / save confirmation flows.
- Original oddities kept in the matching C: `Dialog_DrawBody` reads its text from the title
  table, not the caller's message table; `Dialog_Term` uses the animation before its NULL
  check.
- Linked layout: `gDialog` is defined in dialog.c as initialised data (`.sdata` 0x2FF160, before
  `"fl_ok"`); `.rodata` at 0x2F35B0, size 0x145. `"fl_on_start"` (0x2F3640) and `"fl_off_start"`
  (0x2F36E8) are the named objects `gDialogLabelOnStart` / `gDialogLabelOffStart`, defined where
  the string pool has them, because the assembly of `Dialog_SetCursor` refers to them by symbol.

## Post-process passes (0x102F28..0x106D60; src/sys/gfxm_a.c, linked; names in config/symbols/gfxm_a.txt)

46 functions, 42 match; `GfxPost_DrawGlow`, `StgPanBlur_DrawView`, `StgPanBlur_UpdateView` and
`StgDepthTint_Draw` are INCLUDE_ASM with attempts (instruction-level differences only). The
module names are readings of the technique, not confirmed on screen. Layouts in
include/sys/gfxm_a.h. Suggested final name: sys/gfx_post.c (with gfxm_b).

- (verified) Post effects read the depth buffer page (0xE0, texture block 0x1C00) as an 8-bit
  texture of its spare top byte (a per-pixel object id / mask), through 256-entry CLUTs built
  at run time. CLUT blocks: 0x3E98 pan blur, 0x3E8C outline, 0x3E88 glare, 0x3E64 depth tint,
  0x3E90 object glow. Work pages 0x150 (texture block 0x2A00) and 0x170 (0x2E00); (inferred)
  0x1F6 / 0x1F8 (64x64) for the glow pass.
- Modules: pan blur (`StgPanBlur_*`, stage feature 0xD; strength built from the camera's
  horizontal movement per frame), glow pass (`GfxPost_DrawGlow`: reduce to 256 / 128 / 64,
  blur, add back), packet helpers (`GfxPost_*`), object outline (`ObjOutline_*`, called by
  `BtlObj_InitDraw / TermDraw / BeginDraw`), sky glare (`StgGlare_*`, stage section +0x48),
  depth tint (`StgDepthTint_*`, stage section +0x40; index 1 when the camera is under the
  water level, never in split screen), object glow (`ObjGlow_*`, per-id alpha table from
  `BtlObj_UploadAlphaTable`).
- (verified) `BtlObj_BeginDraw` runs the outline pass, then the object glow.
- (verified) Stage parameter formats: glare = s32 flags (1 enabled, 2 track), s32 step, u8 max,
  hold, min, alpha (defaults: off, alpha 0x40, step 0x20, max 0x80, hold 0x50, min 4); depth
  tint = 0x10-byte entries: s32 flags (1 enabled, 2 additive, 4 clear entry 0xFF), u8 r, g, b,
  a, three (x, z) byte pairs of curve keys.
- No pad, clock or random draw; reads the camera and `Battle_IsSplitScreen`;
  `BATTLE_FLAG_PAUSE` freezes the pan blur update. Nothing feeds back into the simulation.
- Linked: the file emits no data of its own. `GfxQuad` of include/sys/gfxm_b.h is now an alias of
  `GfxPostQuad`. The declarations in stg_c.c (`StgGlare_Init`, `StgDepthTint_Init`,
  `GfxDepthFog_Init` take a `u16` block; `GfxPost_DrawDepthClut`'s alpha is `u64`) were corrected
  to the definitions; the callers still match.

## Screen passes, texture files, movie data (0x106D60..0x10AD58; src/sys/gfxm_b.c, gfxm_b_b.c, gfxm_b_c.c; linked; names in config/symbols/gfxm_b.txt)

63 functions, 53 match; 10 are INCLUDE_ASM with attempts (`GfxPost_DrawTintRect`,
`GfxLens_DrawAll`, `GfxLens_PutCapture`, `GfxWater_DrawView`, `GfxWater_Draw`,
`GfxPost_ShiftHighWord`, `TexChain_Build`, `TexChain_BuildPair`, `GfxClut_InitPacket`,
`Flash_SkipNamed`). Layouts in include/sys/gfxm_b*.h. Suggested final names: gfx_post_b.c /
gfx_lens.c / gfx_water.c / gfx_depth_fog.c, tex_file.c, flash_data.c.

Depth-buffer tricks (verified):
- The depth buffer is 24-bit (page 0xE0, block 0x1C00); its spare top byte is an 8-bit scratch
  channel. `GfxPost_CopyAlphaToDepth` copies the colour buffer's alpha into it (every frame,
  from `BtlObj_BeginDraw`). `GfxPost_DrawDepthClut(mode, tbp, cbp, alpha)` draws that byte
  through a 256-entry CLUT over the screen (16 strips of 32x448).
- Depth fog (`GfxDepthFog_*`, name inferred): CLUT black with alpha 255 - index, indexed by the
  high depth bits. Alpha key (`GfxAlphaKey_*`, purpose inferred): pixels whose frame alpha is
  0xF4..0xFE get a wash of one of eleven fixed colours (table at 0x2EB710, RGB: 3F3FFF, 8000FF,
  FFFF20, FF0000, 00FF00, 54FDFF, FF00FF, 4000FF, FFFF20, FFFF2A, FFFF34; the first C version had
  five entries wrong, found only by the image compare).
- Lenses (`GfxLens_*`, use unknown): 8 slots that distort a captured copy of the screen around
  a world point; nothing in the decompiled code starts one.
- Underwater wobble (`GfxWater_*`): when the camera is under the stage's water level the view
  is redrawn through a 6 x 10 grid displaced by two cosine waves (speeds 0.06 / 0.04 per
  frame), tinted by the stage's water colour. (from disassembly) original bug: the border test
  uses row index 9 on a 6-row grid, so only the top row is pinned.

Texture files (verified): count at +0, entry-table offset in words at +4, 0x40-byte entries
(`TexEntry`: pixel / CLUT offsets, sizes, block steps, BITBLTBUF bits, TEX0, pointers);
`Res_RelocateOffsets(&file, base, hdr)` returns nothing. Uploads queue the 0x30-byte packet at
0x2C3410 then a DMA REF to the entry's ready packet. `TexFile_UploadOne(file, index, tbp,
cbp)` (0x10A218; btl_obj.c's declaration and comments were corrected: `BtlObjLight_FindRes0 / 1`
upload textures 0 and 1 of the light's texture file and return nothing), `Tex_Upload` (0x10A288;
arguments are block pointers, not x / y), `TexFile_UploadAll` (0x10A480), `Tex_Log2Size`
(0x109F50), `GfxClut_InitPacket` (0x10A5A0: a complete 256-entry CLUT upload packet, CLUT
bytes at +0x70).

Movie ("Flash-like") data (verified code, format names inferred): header bytes 'F' 'O' 'D'
0x11 and the string "LIT"; a list of 8-byte tag headers (code u8, record count u16 +2, size
u32 +4), code 0 ends; named blocks (NUL-terminated name, u16 size, data); a bit reader, most
significant bit first; `Flash_ReadMtx` reads the SWF MATRIX record with an explicit flag byte
(scale and skew / 65536, translation in twentieths).

No pad, clock or random draw anywhere in the range; the lens and water passes read the camera
and the split-screen mode. Nothing feeds back into the simulation.

## Movie clip setters and the battle object renderer (0x10EC18..0x112A30; src/sys/gfxm_d.c, gfxm_d_b.c, gfxm_d_c.c, linked; names in config/symbols/gfxm_d.txt)
Linked layout: gfxm_d_b.c 0x10FB40, assembly (VU0 `ObjSeam_TransformVtx`) 0x10FFD0..0x1101D0, gfxm_d_c.c 0x1101D0;
gfxm_d_c.c `.rodata` 0x2EB7E0 (0x110), `.lit4` 0x2FC2C0 (0xC).

All 38 C functions match; `ObjSeam_TransformVtx` (0x10FFD0..0x1101D0) is hand-written VU0 code
and stays an assembly chunk between gfxm_d_b.c and gfxm_d_c.c. Final names: gfxm_d.c merges
into sys/flash.c (same object as gfxm_c.c: its wrappers tail-call into this file);
gfxm_d_b.c + VU0 chunk + gfxm_d_c.c = battle/btl_obj_draw.c. Layouts in
include/sys/gfxm_d.h, gfxm_d_c.h.

Movie clip setters (0x10EC18..0x10FB40, verified): each `Flash_Clip*` wrapper tail-calls a
`FlashClipList_*` function that acts on clip `ref->index` and on the following clips of the
same name. Overrides sit at clip +0x48: flag 1 texture index, 2 position offset, 4 texture
rectangle, 8 alpha factor (0..1), 0x10 scale, 0x20 colour factor. Alpha 1.0 corresponds to
128. (inferred) +0xB4 / +0xB8 / +0xC4 are pre-draw / post-draw / draw-over callbacks.

Battle object renderer (0x10FB40..0x112A30, verified unless marked):
- `BtlObjDraw_Draw` (0x10FF40; `Battle_Draw` and the character viewer) is the models' whole
  draw pass (the older battle.c comment "a full-screen pass" is wrong): clear the frame's
  alpha, draw every object of every view's list, `BtlObj_BeginDraw` (post passes), the fade
  layer (view flag 8), the shadows (flat or rendered), restore the environment.
- Colour effects on a model are done on its PALETTE: `ObjDraw_AddClutPass` points FRAME_1 at
  the model's CLUT block and blends 64x64 sprites over it for dimming, distance fade (colour
  0x006644FF, strength fade x 70), tint and hit flash (`BtlObjFlash_GetColor`). A port's
  renderer needs an equivalent (palette or shader tint).
- A model's textures are re-uploaded every frame; the face entry is replaced by
  `BtlObj_GetFaceTexture`. Parts are drawn with VU1 program 0 (lit), 1 (fade texture) or
  2 (flat, shadows); the head (node 0x33) is drawn last between two frame-mask packets;
  node 0x30 can swap its chain by `BtlObj_GetSubState`.
- Light direction per view is chosen by the colour flags and stored at the view record +0x10
  (render state only); rim light = normalize(normalize(camPos - node3Pos) + 0.7 * camRight)
  with an original slip (y is replaced by camRight.y, not added).
- "Seams" (name inferred): a model section of two-bone vertices skinned on the CPU by the VU0
  routine and drawn as GS triangles twice (base texture, then a shading ramp with
  u = 0.5 + 0.5 * N.L). Model header fields: +0x4C..+0x53 texture index bytes, +0x58 tbp,
  +0x5C cbp, +0x60 / +0x64 / +0x68 further blocks, +0x70 seam section offset.
- Corrections for btl_obj.h: part record +0xC is a `u16` flags word (bit 0 = fade pass),
  +0x10 / +0x20 the two bind offsets, +0x60 the VIF chain; view record +0x10 is `Vec4
  lightDir`; flag 0x100000 masks all context-1 writes; 0x200000 selects the dim sprite.
- Original quirk: the CLUT range is uploaded `clutCount` times per frame.
- No pad, clock or random draw; nothing feeds back into the simulation.

## Object GS state, model binding, ground shadows, stage relocation (0x112A30..0x115170; src/sys/gfxm_e.c, gfxm_e_b.c, gfxm_e_c.c, gfxm_e_d.c, linked; names in config/symbols/gfxm_e.txt)
Linked layout: gfxm_e.c `.rodata` 0x2EB8F0 (0x80); gfxm_e_b.c `.rodata` 0x2EB970 (0x30), `.lit4` 0x2FC2D0 (0x2C; the word
in front, 0x2FC2CC, belongs to `ObjShadow_BuildPacket` and stays in an assembly chunk).

35 of 36 functions match; `ObjShadow_BuildPacket` (0x113700) is INCLUDE_ASM with an attempt
(a loop-size threshold keeps three constants in the loop in the original; compared with the
disassembly by reading only). Final names: battle/btl_obj_gs.c (probably the tail of the
object-draw file), btl_obj_mdl.c + btl_obj_shadow.c, mdl_tex.c, stg_reloc.c. 0x1131D8 is a
proven object boundary (`beqz` / `beqzl`). Layouts in include/sys/gfxm_e.h.

- (verified) **Model binding** (`BtlObjMdl_Create` 0x113598 / `Destroy` 0x1135F0, called by
  `BtlObj_Setup` / `BtlObj_Destroy`): the first time a model file is bound its texture block
  pointers are rebased (TBP 0x3480, CBP 0x3C00; second TEX0 TBP 0x3D40, CBP + 0x3D00), it
  claims a row of the per-object alpha table, and for each material texture the alpha bytes
  of the first 16 CLUT entries become slot + row x 15 (this is the id byte the post passes
  read back from the depth buffer's spare byte). Bit 31 of header +0x0C marks the file bound.
  One pool part (of 512) per mesh record. A texture reference in a model is a VIF block
  0x6C058000 whose words 5/6 and 9/10 are two TEX0 values; streams end with 0x70000000.
- (verified) **Ground shadow** (`ObjShadow_*`; pool of five, objects with flag bit 28):
  each frame unless paused, the stage triangles under the object's bounds are collected (up
  to 128, alpha fading with height); the object is drawn flattened into a 256x256 page (page
  0x150) from 1000 above with VU1 program 2, then the triangles are drawn in each view with
  that page as a texture (program 6, batches of 18, lifted 0.02 along the normal). Area size
  / texture scale by body scale: <= 23: 18 / 0.055; <= 50: 24.4 / 0.042; <= 70: 50 / 0.021;
  above: 85 / 0.012. Objects with flag bit 29 get a flat squashed shadow instead (purpose
  inferred). Reads `Battle_IsSplitScreen`, the pause and loading flags; visual only.
- (verified) **Stage file relocation** (`BtlStage_Relocate` 0x114C60, called by
  `BtlStage_Init`): every offset is in words from the base; the header holds count / pointer
  pairs at +0x10, +0x18, +0x20, +0x30, +0x38, +0x44, pointers at +0x2C and +0x40, an octree at
  +0x50 (eight children per node), the texture file offset at +0x54. (inferred) what each
  table holds. Original bug: writes `gBtlStage->base` before the NULL test.
- Corrections for btl_obj.h: `BtlObj.unk04` = "built" flag, `unk14` = `BtlResSlot *`,
  `BtlObjBound` records are the mesh list (VIF stream at +0x60), `BtlObjPool3Work` is the
  shadow pool.
- No pad, clock or random draw; nothing feeds back into the simulation.

## Movie player (0x10AD58..0x10EC18; src/sys/gfxm_c.c, linked; names in config/symbols/gfxm_c.txt)
Linked layout: gfxm_c.c `.rodata` 0x2EB740 (0xA0), `.sdata` 0x2FE8E0 (0x34: "pad" .. "end"); gfxm_d.c has no data.
The three flash files (gfxm_b_c.c, gfxm_c.c, gfxm_d.c) are still three objects: the image is the same either way, and
their headers (`Flash*` in gfxm_c.h, `FlashD*` in gfxm_d.h) and the local views in dialog.c / view_a*.c were kept apart.
gfxm_c.c called the clip-list functions by its own names (`FlashClips_*`); they are now gfxm_d.c's `FlashClipList_*`.

73 of 74 functions match; `Flash_Advance` (0x10D6F0) is INCLUDE_ASM, one `move` short, with a
behaviourally exact attempt. Final name sys/flash.c together with gfxm_b_c.c (readers) and
gfxm_d.c (clip setters). Layouts in include/sys/gfxm_c.h (they collide with the local views
in gfxm_d.h, dialog and view_a*: unify before including both).

File format (verified by matching C): a movie is a **converted SWF**. Header 'F' 'O' 'D' 0x11
"LIT\0", then tags (u8 code, u8, u16 record count, u32 size; code 0 ends):
- tag 2 sprite list; tag 3 images (u16 width, height; image i draws with texture entry i);
  tag 4 shapes (u16 count + 0x18-byte quads: s16 image or negative for flat, u32 rgba, s32 x0,
  x1, y0, y1); tag 6 sprite frame records; tag 7 the root's frame record.
- A frame record is a list of named blocks (label, u16 size, frame tags ended by code 1).
  Frame tags keep SWF codes: 4 PlaceObject, 5 RemoveObject, 12 DoAction, 26 PlaceObject2,
  28 RemoveObject2. Actions keep SWF codes: 0x81 GotoFrame, 4 NextFrame, 5 PrevFrame, 6 Play,
  7 Stop, 0x83 GetURL, 0x8B SetTarget, 0x8C GotoLabel.
- **GetURL is the movie-to-game channel**: "pad" "true" / "false" sets / clears `Flash.flags`
  bit 2; "trig" "n" and "se" "n" set bit n of `Flash.trig` / `Flash.se`; "trigger" "end" sets
  flags bit 8. (inferred) "pad" gates input and "se" asks for a sound effect.
- Colour transform: flag byte, 4-bit field width, signed fields, multipliers 8.8.

Runtime (verified): `Flash` is 0x2C bytes (data, tex, flags 1 play / 2 pad / 4 hide / 8 end,
trig, se, speed, offset, root timeline, clip list, shape pool). `Flash_Advance` runs `speed`
frames of the root; a label jump replays frames up to the target with actions suppressed.
Drawing: per depth the shape then the sprite, properties combined with the parent's; each
quad is its own GS packet (12.4 coordinates offset by 0x7000 / 0x7200), textured quads upload
their texture to block 0x3000 every time; alpha 0x44 normal, 0x48 additive, 0x42
subtractive; masks use the frame's alpha plane. Game overrides on a clip: position, scale,
alpha, colour, texture rectangle / index, flips, blend, mask; callbacks preDraw / drawOver /
postDraw. `Flash_FindLabel` (existing name) actually finds a clip instance by name.
No random draw, no pad, clock or camera read. Original bug: a sprite placed by tag 4 has no
name and `FlashClip_Start` calls `strlen(NULL)`.

## Stage radial blur: `StgBlur_Draw` (0x248160; src/battle/stg_c.c; matched 2026-10-05)

Verified by matching C. `StgBlur_Draw(split, view, viewMtx)` blurs one view:
- Returns unless one of the three ring alphas (colours 0 / 1 / 2 of `gStgBlur`) is non-zero.
- Copies the view (512 wide, or 256 in split screen) at half size into work buffer 0 (frame
  page 0x150 = texture block 0x2A00); then `passes` iterations ping-pong between buffers
  0x150 / 0x2A00 and 0x16C / 0x2D80, each a plain copy followed by one blended sprite tinted
  `color3` whose texel rectangle is inset by `grow = (int)(f * scale)` and shifted by
  `f * shift * center` (f = 1, 2, 4, ...): a zoom towards the centre.
- Draws the result back over the screen as two 10-vertex Gouraud strips forming rings around
  the centre: centre (colour 0) to half-way points (colour 1) to the view's corners (colour
  2), so the three alphas are the blur strength at the centre, the middle ring and the edge.
- The centre is `blur->center` in screen space, or rotated by the view matrix (the only camera
  read). No random draw; nothing written outside the packet. No simulation impact.
- Quirks: with `passes <= 0` the final texture block is 0; x truncates `f` before
  multiplying and y does not.
`StgHaze_Draw` (same file, still INCLUDE_ASM) is now 15 stack offsets from matching.
