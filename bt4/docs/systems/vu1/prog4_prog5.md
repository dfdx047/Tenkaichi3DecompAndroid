# VU1 programs 4 and 5: static geometry with a triangle clipper

Listings: `src/vu1/prog4.vsm` (399 instructions, 0x2C04F0..0x2C1180), `src/vu1/prog5.vsm` (429 instructions,
0x2C1180..0x2C1F00). C side: `src/sys/vu1_packet.c` (`Vu1Pkt_LoadProg4/5`, `Vu1Pkt_CallProg4/5`), caller
`StgModel_Draw` in `src/battle/stgm_a.c`. Port: `bt3-port/port/src/gs/shaders/vu4.vert`, `hle_program4` in
`bt3-port/port/src/gs/gs_vu1.c`.

"Verified" below means read from the instructions of the listing or from matching C. "Inferred" means deduced
from how the value is used; the data itself (stage chains) was not inspected for this note.

## Program 4

### Purpose (verified)
Draws triangle strips that carry a position, a colour and a texture coordinate per vertex. No lighting, no fog.
Per vertex: transform to the screen, divide by w, convert. Every triangle of the strip is tested against the
clip volume; a triangle that crosses it is removed from the strip and drawn again as a clipped polygon (a fan).
Used for the stage groups drawn by `STGM_DRAW_GROUP` and for group 2 meshes with `prog == 0`.

### Packets (verified, vu1_packet.c)
- `Vu1Pkt_LoadProg4(mtx)`: "ref" to the program, then FLUSHE, UNPACK V4-32 of 12 quadwords to VU address 0,
  BASE 0x8D, OFFSET 0x1B4. The 12 quadwords: `Vu0Screen_StoreMtx`, `Vu0Clip_StoreMtx`, `mtx`.
  `StgModel_Draw` passes the view's matrix at +0x100, which `View_BuildProjection` (btl_cam.c) builds as
  diag(sx, sy, (zMin - zMax) / 2, 1) with last column (2048, 2048, (zMax + zMin) / 2, 1): the map from clip
  space to GS screen coordinates.
- `Vu1Pkt_CallProg4(chain)`: FLUSHE, MSCALF 0, BASE / OFFSET, DMA "call" of the mesh's chain. The chain holds
  the UNPACKs to the double buffer and one MSCNT per unit.

### Entry points (verified)
| address | reached by | does |
|---|---|---|
| 0 `init` | MSCALF 0 | vf1..vf12 = the three matrices, vi14 = 0x400 (stack). Ends at 13. |
| 15 `object_header` | first MSCNT | copies the quadword at TOP to VU 30, vf31 = 0. Ends at 19. |
| 21 `batch` | second MSCNT | one batch. Ends at 67. |
| 69 `batch_resume` | every later MSCNT | `b batch`. |

### VU memory (verified unless marked)
| quadwords | content |
|---|---|
| 0x000..0x003 | screen matrix (position -> screen coordinates times w), vf1..vf4 |
| 0x004..0x007 | clip matrix (position -> clip space), vf5..vf8 |
| 0x008..0x00B | clip -> screen matrix, vf9..vf12; only `emit_fan` uses it |
| 0x00C..0x01D | not read or written by the program |
| 0x01E | GIF tag of the fan packet: the object header. The program only writes its first word (0x8000, then 0x8000 + vertex count). Inferred: PRIM = triangle fan, registers ST, RGBAQ, XYZ2 (the polygon is convex and its vertices are stored in ring order with that format, so a fan is the only primitive that draws it right). |
| 0x01F..0x03C | fan vertices, 3 quadwords each: room for 10; a triangle cut by six planes has at most 9 |
| 0x03D..0x064 | clipper polygon A: per vertex clip position, colour (floats), texture coordinate; 40 quadwords, at most 10 entries (9 vertices + the repeated first) are used |
| 0x065..0x08C | clipper polygon B, same |
| 0x08D.. | first input buffer (TOP = BASE); the second is at 0x241 (BASE + OFFSET). Input batch at TOP, output packet at TOP + 0xDD |
| ..0x3FF | stack, down from 0x400: 4 float quadwords + 2 quadwords of integer registers per clipped triangle (not nested) |

Arithmetic on these addresses (not checked against the data): the input may be 0xDD quadwords (3 + 3 x 72
vertices); the output of the first buffer has 0x241 - 0x16A = 215 quadwords before the second buffer's input
(3 + 3 x 70), the output of the second 0x3FA - 0x31E = 220 before the stack.

### Input batch at TOP (layout verified from the reads; meaning of +0/+1 from the port)
- +0 GIF tag with one A+D register and EOP; +1 its data (the texture). The program only copies them.
- +2 GIF tag of the strip: bits 0..14 of the first word = vertex count n. Copied as is.
- +3 n vertices of three quadwords: position (x, y, z, w); colour as floats; texture coordinate (s, t, q).
  Inferred: w = 1, q = 1, colour 0..255 (the port's shader assumes this).

### Output (verified)
- TOP+0xDD +0,+1: the A+D tag and data, kicked first (EOP in the tag ends that packet).
- +2: the strip tag; +3: per vertex ST = (s, t, q) x 1/w, RGBAQ = ftoi0(colour), XYZ2 = ftoi4(screen / w)
  for all four fields. The GIF's packed XYZ2 takes z from bit 4 of the third field, so z ends up as the
  integer part. The fourth field is ftoi4(1) = 16; bit 15 of it is ADC ("do not draw"), which the clipper
  sets. Kicked from +2 after the last vertex.
- VU 0x1E: one packet per clipped triangle, kicked while the strip is still being built, so a fan reaches the
  GS before the strip it belongs to.

### Algorithm (verified)
1. Batch setup: copy the three header quadwords to the output, kick the texture packet, start the transform of
   vertex 0, clear the history (clip positions = (0,0,0,1), outside flags = -2).
2. `vertex_loop` (20 instructions per vertex, software-pipelined: the screen transform and the divide of a
   vertex are started in the round before):
   - clip = clip matrix x position, pushed into a three-deep history vf22, vf23, vf24;
   - `clip` on the three history entries in order, so the clipping flags hold the whole triangle
     (bits 12..17 oldest, 6..11 middle, 0..5 newest);
   - store ST, RGBAQ, XYZ2 of the vertex;
   - `fcand 0x3F` = 1 if the newest vertex is outside any plane; vi4 = that flag summed over the last three
     vertices. The -2 start values keep vi4 <= 0 until a third vertex exists.
   - vi4 > 0: `triangle_outside`.
3. `triangle_outside`: six `fcor` tests, one per plane (order y < -w, y > +w, x < -w, x > +w, z < -w, z > +w),
   each true when all three vertices are outside that plane. If any is true: `triangle_drop`.
   Otherwise `triangle_clip`: centroid = (v0 + v1 + v2) / 3 in clip space, push vf20, vf23, vf24, vf25 and
   vi2, vi3, vi5, vi6, vi7, vi10, call `clip_triangle`, pop, then `triangle_drop` as well.
4. `triangle_drop`: writes 0x8000 into the fourth word of the XYZ2 just stored (ADC), so this vertex does not
   complete a triangle. The strip continues.
5. `clip_triangle`:
   - each vertex is moved to centroid + 1.005 x (vertex - centroid): the triangle is enlarged by 0.5% in clip
     space. Inferred purpose: overlap with the neighbours so that no gap shows along the shared edges, since
     the fan is rasterised from different vertices than the strip.
   - polygon A is filled with v0, v1, v2, v0: the enlarged clip positions, the colours read back from the
     output packet (integers, `itof0`: so the truncated colours), and the texture coordinates read from the input.
   - six passes of Sutherland-Hodgman, source and destination swapping between A and B:

     | pass | plane | flag mask first / second vertex | component (mr32 count) | sign |
     |---|---|---|---|---|
     | 1 | z = -w | 0x800 / 0x20 | z (2) | -1 |
     | 2 | z = +w | 0x400 / 0x10 | z (2) | +1 |
     | 3 | x = -w | 0x080 / 0x02 | x (0) | -1 |
     | 4 | x = +w | 0x040 / 0x01 | x (0) | +1 |
     | 5 | y = -w | 0x200 / 0x08 | y (1) | -1 |
     | 6 | y = +w | 0x100 / 0x04 | y (1) | +1 |

     Each pass calls `clip_edge` once per edge (3 for the first pass, then the vertex count of the previous
     pass), then `close_polygon` to repeat the first output vertex at the end; if the pass wrote no vertex
     the routine returns without drawing.
   - `clip_edge` (edge A -> B): `clip` A, `clip` B, `fcget`; with the pass's two masks:
     in/in writes A; in/out writes A and the intersection; out/in writes the intersection; out/out nothing.
   - `intersect`: d = c - sign x w for both ends, where c is the pass's component (brought to x by `mr32`);
     t = |dA / (dB - dA)|; vertex = A + t x (B - A) for the four components of position, colour and
     texture coordinate. The interpolation is linear in clip space, before any divide, so it is
     perspective-correct.
   - after pass 6 the polygon is in A: `emit_fan`.
6. `emit_fan`: writes the tag's first word as 0x8000 (NLOOP 0, EOP) and kicks it, then writes 0x8000 + count.
   Per polygon vertex: screen = (clip -> screen matrix) x clip position, x 1/w, ftoi4; colour ftoi0; texture
   coordinate x 1/w; stored as ST, RGBAQ, XYZ2 from VU 0x1F. Then XGKICK 0x1E.
   Inferred: the kick of the empty tag is there to wait until the previous fan's transfer from this same
   buffer has ended before the buffer is rewritten (XGKICK waits for the one before it).

The names -w / +w refer to the VU `clip` instruction (compare with |w|). Which of z = -w / z = +w is the near
plane depends on the clip matrix and was not worked out here.

## Program 5

### Purpose
Verified: the same program with an object matrix in front. Its builders `Vu1Pkt_LoadProg5` and
`Vu1Pkt_CallProg5` have no caller (no reference in `src/` or `asm/`), so the game never runs it.
Inferred: it is the general version (static geometry placed by a matrix) that program 4 was specialised and
tightened from, or a sibling kept for objects that were dropped; nothing in the code says which.

### Packets (verified)
- `Vu1Pkt_LoadProg5(mtx)`: 16 quadwords to VU 0: identity, screen matrix, clip matrix, `mtx`. BASE 0x8D,
  OFFSET 0x1B4.
- `Vu1Pkt_CallProg5(chain, mtx)`: UNPACK of `mtx` to VU 0..3 (replaces the identity), MSCALF 0, BASE / OFFSET,
  the chain.

### Entry points (verified)
0 `init` (MSCALF 0, ends at 38), 40 `object_header` (first MSCNT, ends at 44), 46 `batch` (ends at 95),
97 `batch_resume`.

### VU memory (verified)
0..3 object matrix, 4..7 screen matrix, 8..11 clip matrix, 12..15 clip -> screen matrix; 0x1E fan tag,
0x1F fan vertices, 0x3D / 0x65 polygons A / B as in program 4; output packet at TOP + 0xC0; stack 9 quadwords
per clipped triangle. Input and output formats are those of program 4.

### Differences from program 4 (verified, by comparing the listings)
| | program 4 | program 5 |
|---|---|---|
| constants | 12 quadwords: screen, clip, clip -> screen | 16: object, screen, clip, clip -> screen |
| `init` | loads vf1..vf12 | vf1..vf4 = screen x object, vf5..vf8 = clip x object (32 multiply-add instructions), vf9..vf12 loaded |
| output packet | TOP + 0xDD | TOP + 0xC0 (input at most 3 + 3 x 63 vertices by the same arithmetic) |
| vertex loop | 20 instructions, divide in the branch delay slot | 24 instructions, same operations in another order, two nops |
| 0x8000 constant | kept in vi13 for the whole batch | built in vi11 where needed (`triangle_drop`, `emit_fan`) |
| before `clip_triangle` | computes the centroid; pushes vf20, vf23, vf24, vf25 | no centroid; pushes vf20..vf25 and vf31 |
| after `clip_triangle` | vf31 zeroed again with `mulx` | vf31 popped |
| triangle given to the clipper | enlarged by 0.5% about the centroid | as it is |
| `emit_fan` | pre-increments the cursor (stores at -4, -3, -2) | stores RGBAQ at 0, ST at -1, then advances; reads the colour into vf20 |
| return to the loop from `triangle_drop` | `vertex_next` = the loop branch (divide in its delay slot) | `vertex_next` = the divide, then the branch |

The six clip passes, `clip_edge`, `close_polygon` and `intersect` are instruction-for-instruction the same
(only the addresses differ). Results differ only by the object matrix and the missing enlargement.

## Open questions
- The object header (first MSCNT) is taken to be the fan's GIF tag because it is stored at VU 0x1E and only its
  first word is rewritten; its PRIM and register list were not read from stage data.
- Why program 4 enlarges the clipped triangle and program 5 does not (crack hiding is the inferred reason).
- The purpose of the empty-tag XGKICK in `emit_fan` (inferred: synchronisation with the previous kick). The tag
  word is rewritten two instructions after that kick; whether the hardware has read it by then was not checked.
- The port notes that its interpreter drew wrong triangles through this clipper when the camera was close to
  geometry; whether that is the interpreter (XGKICK timing, clip flag pipeline) or original behaviour is not known.
- Actual vertex counts per batch in the stage data against the buffer arithmetic above.
- vf31 is assumed to stay 0 between `object_header` and the batches (nothing in these programs writes it, but
  a chain that skipped the header MSCNT would leave it undefined, and `clip_edge` uses it as zero).
- Which side of z is the near plane; what w and q hold in the vertex data (taken as 1).
