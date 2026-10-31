# VIF1 DIRECT, GS image transfers and the first textured sprite

## Why this is a new subsystem

The overlay's second frame starts its render kick (`0x28B3E8`, second VIF1 chain start) with real content. The VIF1 interpreter accepted only NOP. The chain is now refused as a checked boundary at the channel start (`frame_vif1.unsupported`, no partial effect).

The first non-empty chain uses no VU1 at all. It is PATH2 GS traffic that needs:
- host→local image transfers;
- 4-bit texture addressing;
- CLUT loads;
- textured sprite drawing.

None of this exists in the GS model yet, and together it is the GS texturing subsystem.

## Evidence (2026-10-07, native report at the boundary)

The chain at `0x430600` has 8 DMA tags (5 CNT, 2 REF, 1 END). Its VIF codes are FLUSHE ×3, NOP ×14 and DIRECT ×7, covering 2,081 quadwords. There is no UNPACK, MPG or MSCAL. The DIRECT payloads are:

1. **Draw state (A+D):** ALPHA_1 `0x44` (Cs−Cd)·As+Cd, TEX1_1 `0x60` (linear MMAG/MMIN), TEST_1 `0x30000` (Z test ALWAYS).
2. **Texture upload:**
   - BITBLTBUF: DBP `0x3000`, DBW 4, DPSM CT32.
   - A REF to `0x8D53A0`: TRXPOS, TRXREG 256×32, TRXDIR host→local, then IMAGE 2,048 qwords and TEXFLUSH.
3. **CLUT upload:** BITBLTBUF DBP `0x30C0` CT32, then a REF to `0x8DD420`: TRXREG 8×2, IMAGE 4 qwords, TEXFLUSH.
4. **TEXFLUSH.**
5. **Draw (REGLIST, 8 registers):**
   - RGBAQ `0x80808080`;
   - TEX0_1: TBP0 `0x3000`, TBW 8, PSMT4, 512×128, TCC 1, TFX modulate, CBP `0x30C0`, CPSM CT32, CSM1, CLD 1;
   - PRIM `0x156`: sprite, TME, ABE, FST;
   - UV/XYZ2 ×2: a 512×128 sprite at (1792,1908)–(2304,2036), UV 0–512 / 0–128.

So the frame draws one 512×128 4-bit CLUT image, uploaded as 256×32 CT32 words. This relies on GS memory swizzling to reinterpret the layout. That makes exact PSMCT32 write and PSMT4 read addressing a requirement, not an optimization.

## Bounded design

1. **VIF1 command parser.** Parse codes with their real lengths: NOP; FLUSHE, FLUSH and FLUSHA as no-ops while no VU1 program runs; DIRECT/DIRECTHL handing quadword-aligned payloads to the general GS transfer. All other codes (UNPACK, MPG, MSCAL and so on) stay checked boundaries. Keep the whole-chain dry run, so a refused chain has no effects.
2. **Host→local transfer.** Handle BITBLTBUF, TRXPOS, TRXREG and TRXDIR (direction 0), then IMAGE data written pixel by pixel in the destination format. Start with PSMCT32 destinations, using the existing CT32 swizzle. Other destination formats, local→host and local→local stop as boundaries.
3. **Texture addressing.** Add PSMT4 (and PSMT8 when reached) read addressing over the same word memory, expressed from the public GS page, block and column layout. Do not copy reference tables.
4. **CLUT.** Implement the TEX0 CLD load into a CLUT buffer for CSM1 and CPSM CT32 (16 entries for T4), with CSA. Other CLUT modes stay boundaries.
5. **Textured sprite.** For the reached state: FST UVs; modulate with TCC; bilinear MMAG/MMIN with clamp behavior per CLAMP_1 (record CLAMP and refuse unknown modes); alpha blend per ALPHA_1, with FBA, PABE and DTHE off.
6. **Verification.**
   - Unit-test the swizzles on round trips (CT32 written, T4 read) against independently derived expectations.
   - Compare the uploaded VRAM pages with the original. A capture point after the frame's VIF1 transfer can read GS memory through the PCSX2 debug interface if available; otherwise compare the EE source images.
   - Compare the drawn framebuffer region with a PCSX2 GS dump or screenshot where obtainable. Without one, state that limit.

## References

- **[paraLLEl-GS](https://github.com/Arntzen-Software/parallel-gs)** (see [reference architecture](../../reference/REFERENCE_ARCHITECTURE.md#parallel-gs-gs-rendering-reference)): use it as the GS rendering reference as new formats, primitives, blending and presentation modes are reached, and as the architecture reference for a later GPU renderer. Adapt its behavior; do not copy code.
- PCSX2's GS (`GSLocalMemory`, `GSTables`, drawing environment) remains the reference for memory layout and register defaults.

## Limits and stop conditions

- No presentation is claimed until DISPFB contents reach a window.
- UNPACK, VU1 microprograms and XGKICK remain a separate later plan.
- Stop if the drawn result needs GS features beyond the reached registers (fog, mipmapping, other CLUT modes).

## Progress (2026-10-08)

Steps 1–5 are implemented for the reached state:
- **VIF1:** commands are parsed with their real lengths; FLUSH variants are no-ops; DIRECT payloads reach the GS.
- **GS transactions:** a whole chain's GS work is one transaction, with state, CLUT, VRAM and counters rolled back on any refusal.
- **GIF parsing:** packets after an EOP in the same transfer are parsed, as on the GS.
- **Image transfers:** host→local transfers into CT32. A new TRXDIR write abandons an incomplete transfer.
- **Texture addressing:** PSMT4 uses the public column layout, written as bit placement and checked against the reference layout at all 512 block positions.
- **CLUT:** CSM1 loads with CT32 entries.
- **Sprites:** textured and blended (modulate/decal, nearest or bilinear, REPEAT/CLAMP, the general blend equation with COLCLAMP and PABE).

**Result.** Normal startup draws the controller-missing screen every frame. Rendered from the owned VRAM model (`render_gs_frame.py`), the displayed framebuffer is fully written and the text is legible. It matches the game's message.

**Not yet verified.** Bilinear rounding against GS output, and a framebuffer comparison with an original GS dump.

The next blocker is controller input: [CONTROLLER_PLAN.md](../input/CONTROLLER_PLAN.md).

## Progress (2026-10-08, unverified)

- **Triangles:** the first gameplay-phase packet draws a textured, blended triangle strip (PRIM `0x154`). The GS model now rasterizes triangles, strips and fans:
  - pixels whose integer position lies inside the 12.4 triangle;
  - a top-left rule on shared edges;
  - flat or Gouraud color, affine FST UVs and Z.
- **Shared path:** sprites and triangles share validation (`prepare`) and per-pixel shading (`plot`).
- **Not modelled yet:** STQ (perspective) texturing, lines and points, and depth tests other than ALWAYS.
