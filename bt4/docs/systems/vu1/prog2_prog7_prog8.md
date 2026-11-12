# VU1 programs 2a, 2b, 7 and 8

Listings: `src/vu1/prog2a.vsm`, `prog2b.vsm`, `prog7.vsm`, `prog8.vsm` (annotated; they assemble back to the
original bytes with `scripts/vuasm.py`). Packet builders: `src/sys/vu1_packet.c`.

"Verified" below means read from the listing or from matching C in this repository. "Inferred" means deduced, or
taken from the PC port's notes (`bt3-port/docs/port/README.md`) without being re-checked here.

All four are the same kind of program: one GIF packet per input batch, three quadwords out per vertex
(ST, RGBAQ, XYZ), perspective divide, 12.4 fixed-point conversion, and a clip test that sets the ADC
("do not draw") bit. 2a, 2b and 8 are one piece of code with small changes; 7 is a rigid variant.

| program | file address | instructions | draws | caller |
|---|---|---|---|---|
| 2a | 0x2BFEA0..0x2C01C0 | 99 | fighter model, one flat colour, clipped | `ObjShadow_BeginFlat` (flat shadow) |
| 2b | 0x2C01C0..0x2C04F0 | 101 | fighter model, one flat colour, not clipped | `ObjShadow_LoadProg` (silhouette into the shadow page) |
| 7 | 0x2C2D30..0x2C3080 | 104 | stage debris, rigid, per-mesh alpha | `StgModel_DrawDebris` |
| 8 | 0x2C3080..0x2C3380 | 94 | animated stage objects, two-matrix skinning | `StgModel_DrawAnims` -> `Vu1Node_Draw` |

## Common structure (verified)

- **Three entry points each.** `MSCALF 0` (in the `Vu1Pkt_CallProgN` packet) runs a setup that ends at an E bit.
  The first MSCNT after it resumes two instructions after that E bit, at the batch code. The batch code ends
  with `xgkick`, an E bit, a nop, and then `b batch`, which is where every later MSCNT resumes.
- **Batch code.** Reads the batch at TOP (`xtop`), copies three header quadwords to the output packet, takes the
  vertex count from the low 15 bits of the first word of the last one (the NLOOP field of a GIF tag; the EOP
  bit 15 is masked off with 0x7FFF), and loops over the vertices. The count is tested at the bottom, so it must
  be at least 1.
- **Per vertex.** `screen = S p`, `q = 1 / screen.w`, `screen *= q`, `ftoi4`; texture coordinates `(s, t, 1)`
  times `q`; `clip` on `C p` (x, y, z against +-w); `fcand vi1, 0x3FFFF` = "any clip bit set among the last
  three clip results". If so the 4th word of the stored XYZ becomes 0x8000 (ADC). The clip flags are not reset
  between batches, so for the first two vertices of a batch the older results are the previous batch's.
- **Output packet** at a fixed distance from TOP (0xFC past the first vertex): three header quadwords, then per
  vertex `+0` ST/Q, `+1` colour as integers, `+2` position. In the position, x and y are 12.4 fixed point, z
  goes through the same `ftoi4`, and the 4th word is `ftoi4(w / w)` (16, or 15 after rounding) unless replaced
  by 0x8000.
- **Matrices** are stored as four column quadwords; a transform is `col0 x + col1 y + col2 z + col3` with the
  input's w taken as 1 (`maddw ..., vf0w`).

Inferred for all four: that the model chains hold one MSCNT per batch (the chain data was not inspected; the
port's notes on programs 0 and 4 describe the same chains that way), and that header `+0` is a GIF tag
"one A+D register" whose data is `+1`.

## Programs 2a / 2b: flat-colour fighter draw (the shadow pair)

### Purpose (verified from the callers)

- `Vu1Pkt_LoadProg2(0)` -> 2a. Only caller `ObjShadow_BeginFlat` (`src/sys/gfxm_e_b.c`): the flat shadow. S and
  C are the VU0 screen and clip matrices, each times the matrix that squashes the model onto y = 0 along the
  light; colour = the light's colour.
- `Vu1Pkt_LoadProg2(1)` -> 2b. Only caller `ObjShadow_LoadProg` (from `ObjShadow_BeginRender`): the first shadow
  pass, the silhouette from the shadow camera into the 256 x 256 shadow page. S = C = shadow projection times
  the flattening matrix; colour 128,128,128,128. This is the port's "VU1 program of 101 instructions".
- Both then call `ObjDraw_DrawPartsFlat` -> `ObjDraw_DrawParts(mode 2)` (`src/sys/gfxm_d_c.c`), which queues one
  `Vu1Pkt_CallProg2(part)` per enabled part.
- There is no separate "program 3": the "prog 3" of the header comment in `vu1_packet.c` is 2b.

### Entry points (verified)

| | 2a | 2b |
|---|---|---|
| setup (MSCALF 0) | 0, E at 14 | 0, E at 14 |
| batch (first MSCNT) | 16 | 16 |
| later MSCNTs | 97 (`b 16`) | 99 (`b 16`) |

### Memory map (verified)

`Vu1Pkt_LoadProg2`: UNPACK 0x13 quadwords at 0, BASE 0x13, OFFSET 0x1F6 (buffers at 0x13 and 0x209).
`Vu1Pkt_CallProg2`: UNPACK 0xB quadwords at 0 (0..10), MSCALF 0.

| address | content | filled by |
|---|---|---|
| 0..3 | matrix A | `ObjDrawPkt.mtxA` = node `mtxA` |
| 4..7 | matrix B | `ObjDrawPkt.mtxB` = node `mtxB` |
| 8 | offset before A | `ObjDrawPart.ofsA` |
| 9 | offset before B | `ObjDrawPart.ofsB` |
| 10 | flat colour, floats r g b a | the view's colour (+0x30), a forced to 128.0 |
| 11..14 | matrix S (to GS screen) | caller of `Vu1Pkt_LoadProg2` (`ObjVu1Prog2.screen`) |
| 15..18 | matrix C (clip test) | caller of `Vu1Pkt_LoadProg2` (`ObjVu1Prog2.clip`) |

### Input batch

The chains are the fighter models' (the part's VIF chain at part + 0x60), built for program 0.

| TOP + | use by this program (verified) | meaning (inferred, port notes on program 0) |
|---|---|---|
| 0 | copied to output +0 | GIF tag, one A+D register |
| 1 | copied to output +1 | TEX0_1 of the texture layer |
| 2 | not read | TEX0_2 of the toon layer |
| 3 | not read | primitive tag of the texture layer |
| 4 | copied to output +2; low 15 bits of word 0 = vertex count | primitive tag of the toon layer |
| 5 + 3n | position x y z, w = blend weight | |
| 6 + 3n | loaded into vf21, never used | normal |
| 7 + 3n | s, t, 1 | |

### Output (verified)

GIF packet at TOP + 257: +0, +1, +2 = input +0, +1, +4; then per vertex ST/Q, the flat colour (`ftoi0` of
address 10, converted once at setup), position. Inferred from the addresses: at most 80 vertices per batch fit
in the 502-quadword buffer.

### Algorithm (verified)

1. Setup: load A, B, the offsets; colour -> integers; `vi13 = 0x8000`.
2. Batch: header, count, output pointer.
3. Per vertex: `a = A (pos - ofsA)`, `b = B (pos - ofsB)`, `p = b + (a - b) * pos.w`.
4. `screen = S p / (S p).w`, `st = (s, t, 1) / (S p).w`, clip test on `C p`.
5. Store ST, colour, `ftoi4(screen)`; 2a only: ADC when one of the last three vertices was outside.
6. `xgkick`.

The registers vf1-8 hold A/B while the next vertex's skinning runs and S/C while the current one is projected;
they are reloaded from memory twice per vertex. The loop is software-pipelined (the A half of vertex n + 1 is
computed at the bottom of vertex n's pass).

### Exact differences between 2a and 2b (verified by comparing the instruction lists)

- Addresses 0..78 are identical.
- 2b has three extra `nop | nop` instructions, at its addresses 79, 87 and 88.
- 2b does not have 2a's instruction 91, `nop | isw.w vi13, -1(vi7)` (the ADC store).
- Removing those three from 2b and that one from 2a leaves identical lists (apart from the addresses the
  branches encode). 99 + 3 - 1 = 101.

Effect: 2a drops triangles that have a vertex outside the clip volume; 2b draws every triangle. In 2b the
`clip`, the `fcand` into vi1, `vi13` and the `ibeq vi1, vi0` (which now jumps to the instruction it would reach
anyway) are still there but have no effect, and matrix C is computed with but never matters.

## Program 7: stage debris

### Purpose (verified)

`StgModel_DrawDebris` (`src/battle/stgm_a.c`), draw group 6: `Vu1Pkt_LoadProg7()` once, then per visible mesh
`Vu0Cur_LoadMtx(mesh.mtx)` and `Vu1Pkt_CallProg7(mesh.chain, (f32)mesh.alpha)`. Rigid meshes with vertex
colours; the alpha of every vertex is replaced by the mesh's (the debris fade, 0..128).

### Entry points (verified)

Setup 0 (E at 50), batch 52, later MSCNTs 102 (`b 52`).

### Memory map (verified)

Both builders: UNPACK 0xD quadwords at 0, BASE 0xD, OFFSET 0x1F9 (buffers at 0xD and 0x206). Only the call
packet has MSCALF 0.

| address | content | filled by |
|---|---|---|
| 0..3 | matrix L, mesh local-to-world | `Vu1Pkt_CallProg7` (current VU0 matrix vf16-19) |
| 4..7 | matrix S (to GS screen) | both builders (VU0 vf24-27) |
| 8..11 | matrix C (clip test) | both builders (VU0 vf20-23) |
| 12 | only w is read: alpha as a float | `Vu1Pkt_CallProg7` (x, y, z are never written) |

### Input batch

| TOP + | use (verified) | meaning |
|---|---|---|
| 0 | copied to output +0 | (inferred) GIF tag, one A+D register |
| 1 | copied to output +1 | (inferred) its data: TEX0 |
| 2 | copied to output +2; low 15 bits of word 0 = vertex count | (inferred) primitive tag ST, RGBAQ, XYZ |
| 3 + 3n | position x y z; low 16 bits of the w word = integer flag | flag non-zero: do not draw (inferred: strip restarts) |
| 4 + 3n | colour r g b a, floats (a replaced) | |
| 5 + 3n | s, t, 1 | |

### Output (verified)

GIF packet at TOP + 255: the three header quadwords, then per vertex ST/Q, colour (`ftoi0`, a = address 12's
w), position. Inferred: at most 82 vertices per batch fit in the 505-quadword buffer.

### Algorithm (verified)

1. Setup: `SL = S L` into vf1-4 and `CL = C L` into vf13-16 (column by column); vf5 = address 12;
   `vi13 = 0x8000`.
2. Batch: header, count, output pointer; transform vertex 0 and start its divide.
3. Per vertex: `screen = SL p / (SL p).w`, `st = (s, t, 1) / w`, clip test on `CL p`;
   `vi1 = clip result + vertex flag`.
4. Colour: take the vertex colour, overwrite w with the mesh alpha (`move.w vf23, vf5`), `ftoi0`.
5. Store ST, colour (in the delay slot of the branch), position; ADC when `vi1 != 0`.
6. `xgkick`.

The transform and divide of vertex n + 1 run inside vertex n's pass.

## Program 8: animated stage objects

### Purpose (verified)

`StgModel_DrawAnims` (`src/battle/stgm_a.c`): `Vu1Pkt_LoadProg8()` once (the caller stores the VU0 screen and
clip matrices at 10..17), then per object `Vu1Node_Animate` and `Vu1Node_Draw`, which queues one
`Vu1Pkt_CallProg8(node)` per node with a mesh. The code is program 2a with the stage batch header (no skipped
quadwords) and the colour taken from each vertex instead of a constant.

### Entry points (verified)

Setup 0 (E at 13), batch 15, later MSCNTs 92 (`b 15`).

### Memory map (verified)

`Vu1Pkt_LoadProg8`: UNPACK 0x12 quadwords at 0, BASE 0x12, OFFSET 0x1F7 (buffers at 0x12 and 0x209).
`Vu1Pkt_CallProg8`: UNPACK 0xA quadwords at 0 (0..9), MSCALF 0.

| address | content | filled by |
|---|---|---|
| 0..3 | matrix A | `Vu1Node.world` (after the node's own position / rotation) |
| 4..7 | matrix B | `Vu1Node.parent` (matrix on entry to the node) |
| 8 | offset before A | `Vu1Node.unkD0` (copy of `unk30`) |
| 9 | offset before B | `Vu1Node.unkE0` (copy of `unk40`) |
| 10..13 | matrix S (to GS screen) | `StgModel_DrawAnims` (VU0 vf24-27) |
| 14..17 | matrix C (clip test) | `StgModel_DrawAnims` (VU0 vf20-23) |

Inferred: `unk30` / `unk40` are the rest-pose pivots of the node and of its parent (they play the role of
`ofsA` / `ofsB` of the fighter parts).

### Input batch

| TOP + | use (verified) | meaning |
|---|---|---|
| 0 | copied to output +0 | (inferred) GIF tag, one A+D register |
| 1 | copied to output +1 | TEX0: `Vu1Node_OrFlags` ORs the texture / CLUT block numbers (0x3480, 0x3700 << 5) into its first two words (verified) |
| 2 | copied to output +2; low 15 bits of word 0 = vertex count | (inferred) primitive tag ST, RGBAQ, XYZ |
| 3 + 3n | position x y z, w = blend weight (1 = all A) | |
| 4 + 3n | colour r g b a, floats | |
| 5 + 3n | s, t, 1 | |

The header is the block `Vu1Node_OrFlags` searches for: VIF code 0x6C038000, UNPACK V4-32 of 3 quadwords at
TOPS + 0.

### Output (verified)

GIF packet at TOP + 255: the three header quadwords, then per vertex ST/Q, colour (`ftoi0` of the vertex
colour), position. Inferred: at most 81 vertices per batch fit in the 503-quadword buffer.

### Algorithm (verified)

Same six steps as program 2a, with the colour of step 5 read from the vertex (`lq vf21, 1(vi3)` then `ftoi0`).
Unlike 2a the setup converts nothing.

### Differences from 2a (verified)

- Constants: no colour quadword, so S is at 10..13 and C at 14..17 (2a: 11..14 and 15..18).
- Batch header: three quadwords, vertices at TOP + 3 (2a: five, two skipped, vertices at TOP + 5).
- Colour per vertex instead of per part.
- Fewer filler nops; the order of the remaining instructions is the same.

## Open questions

- The chain data itself was not inspected: MSCNT per batch, the exact GIF tags (primitive type, register
  list) and the reading of program 7's vertex flag as a strip-restart marker are inferred.
- Why 2b carries three extra nops and a dead branch (it reads as 2a with the ADC store taken out, but nothing in
  the code says which was written first or why the padding stayed).
- Whether skipping the clip test in 2b is the reason for a separate program is an inference from use: the
  silhouette pass renders with its own projection into the shadow page, and S = C there.
- The per-batch vertex limits (80 / 82 / 81) are derived from the buffer addresses only.
- `Vu1Node.unk30` / `unk40` as pivots: named by analogy, not confirmed from the model format.
- Programs 2a/2b pass the input's third texture-coordinate component through unchanged apart from the divide;
  the value is assumed to be 1 (port notes on program 0).
