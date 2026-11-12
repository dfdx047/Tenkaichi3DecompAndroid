# VU1 program 6: ground shadow of a battle object

Listing: `src/vu1/prog6.vsm` (451 instructions, microprogram data `D_002C1F00..D_002C2D30`).
Numbers below are instruction addresses in decimal; the listing's header gives the address of
every label.

"Verified" here means read directly from the listing or from matching C. Nothing was run.

## Purpose

- (verified) The only user is `ObjShadow_Draw` (`src/sys/gfxm_e_b.c`), called per view from
  `src/sys/gfxm_d_b.c`. It draws the stage triangles collected under an object
  (`StgShadow_Collect`), textured with the object's shadow picture: the 256x256 work page
  (texture block 0x2A00, set with `ObjGs_AddShadowTexEnv`) that the object was drawn into from
  above with program 2.
- (verified) No stage draw group uses it. The stage groups of `docs/systems/effects_stage.md`
  use programs 4, 7 and 8; `src/battle/stgm_a.c` and `src/sys/gfxm_d_c.c` contain no call of
  `Vu1Pkt_LoadProg6` / `Vu1Pkt_CallProg6`.
- (verified) It is program 4 with texture coordinates generated from the position, a per-vertex
  "no draw" flag, and no kick of the batch's two header quadwords. See "Differences".

## How it is loaded and run

- (verified) `Vu1Pkt_LoadProg6(view, mtx, vec)`: references the microprogram, then unpacks 17
  quadwords at address 0 and sets BASE 0x92, OFFSET 0x1B2.
- (verified) `Vu1Pkt_CallProg6(chain)`: MSCALF 0, BASE / OFFSET again, then calls the chain.
- (verified) The chain (`ObjShadow_BuildPacket`): one quadword unpacked at TOPS and MSCNT; then
  per batch three header quadwords at TOPS + 0, 12 quadwords per triangle at TOPS + 3, and MSCNT.
  A batch holds at most 18 triangles (54 vertices, 219 quadwords).

| Entry | Reached by | Does |
|---|---|---|
| 0 `entry_init` | MSCALF 0 | matrices 0x000, 0x004, 0x00C to vf1..vf8, vf13..vf16; vi14 = 0x400 |
| 15 `entry_fan_tag` | first MSCNT | copies the quadword at TOP to 0x01E; vf31 = 0 |
| 21 `batch_start` | second MSCNT | one batch |
| 104 `entry_next_batch` | later MSCNTs | branches to 21 |

(verified) Each entry ends with an `[E]` instruction (13, 19, 102); the next MSCNT continues two
instructions later. The registers set by entries 0 and 15 are not reloaded per batch.

## VU memory (quadwords)

| Address | Content | Source |
|---|---|---|
| 0x000..0x003 | world -> screen matrix (vf1..vf4) | `Mtx_Mul(view + 0x80, view + 0x40)` |
| 0x004..0x007 | world -> clip matrix (vf5..vf8) | `Mtx_Mul(view + 0xC0, view + 0x40)` |
| 0x008..0x00B | clip -> screen matrix (vf9..vf12, loaded in `emit_fan`) | `view + 0x100` |
| 0x00C..0x00F | world -> shadow camera matrix (vf13..vf16) | `ObjShadowWork.view` |
| 0x010 | x = texture scale; y, z, w not read | `ObjShadowWork.scale` |
| 0x011..0x01D | not used | |
| 0x01E | GIF tag of a clipped triangle's fan, then its vertices (3 quadwords each, room up to 0x03C = 10 vertices) | entry 15 / `emit_fan` |
| 0x03D..0x064 | clipper polygon buffer A | |
| 0x065..0x08C | clipper polygon buffer B | |
| 0x08D..0x091 | not used | |
| TOP (0x092 or 0x244) | input batch | VIF double buffering |
| TOP + 0xDD | output packet (2 + 1 + 3 per vertex) | |
| 0x3F7..0x3FF | clipper save area, 9 quadwords pushed down from 0x400 | vi14 |

- (verified) What the program does with each matrix: 0x000 gives the XYZ sent to the GS, 0x004
  gives the positions that are clip-tested and clipped, 0x008 takes the clipper's output to the
  screen, 0x00C and 0x010 give the texture coordinates.
- (inferred) The meaning of the view's fields: +0x40 world-to-view (it is that in
  `GfxLensView`), +0x80 view-to-screen, +0xC0 view-to-clip, +0x100 clip-to-screen (the matrix
  `stgm_a.c` gives program 4). The view type `ObjShadow_Draw` receives is only known as a
  scissor rectangle at +0x200.
- (verified) Polygon buffer entry: clip-space position, colour as floats, (s, t, 1, 0).

## Input batch (at TOP)

| Offset | Content |
|---|---|
| +0 | GIF tag: NLOOP 1, EOP, 1 register = 6 (TEX0_1) |
| +1 | its data, all zero |
| +2 | GIF tag: NLOOP = vertex count, EOP, PRE, PRIM 0x5C (triangle strip, gouraud, textured, STQ, alpha blend), registers ST, RGBAQ, XYZF2 |
| +3 + 4i | x, y, z floats; w = integer no-draw flag |
| +4 + 4i | triangle normal, w = 1.0. Not read by the program |
| +5 + 4i | colour r, g, b, a floats (the light colour, alpha times the shadow's alpha) |
| +6 + 4i | 0, 0, 1, 0; overwritten by the program with s, t, 1, 0 |

- (verified, from the `#if 0` C of `ObjShadow_BuildPacket`, which is described as behaviourally
  exact but is not the matching build) The flag is 1 on the first two vertices of every triangle
  except the batch's first. Positions are already lifted 0.02 along the normal.
- (verified) The chain's first quadword (to 0x01E) is the same tag as +2 with NLOOP 0 and PRIM
  0x5D (triangle fan).

## Output

- (verified) At TOP + 0xDD: copies of batch +0 and +1 (never sent), the tag of batch +2, then per
  vertex `s/w, t/w, 1/w` / colour as integers / x, y, z in 12.4 fixed point. One `xgkick` per
  batch, from TOP + 0xDF (101).
- (verified) Bit 15 of the XYZF2 quadword's w word (ADC) is set on a vertex when the strip
  triangle ending there must not be drawn: flagged in the input, wholly outside one clip plane,
  or replaced by a fan.
- (verified) Each clipped triangle is one more packet kicked from 0x01E: the fan tag and up to
  10 vertices in the same three-quadword form.

## Algorithm

1. (verified) Set-up (21..44): copy the three header quadwords to the output, read the vertex
   count (low 15 bits of the tag), start the screen transform and divide of vertex 0, load its
   flag into vi13. vi4, vi5, vi6 = -2.
2. (verified) Per vertex (45..100):
   - clip position = matrix 0x004 x (x, y, z, 1); it enters a three-deep history vf22..vf24, and
     all three are `clip`ped so the 24-bit clip flag holds the last three vertices;
   - screen position = matrix 0x000 x (x, y, z, 1), times 1/w, `ftoi4`;
   - c = matrix 0x00C x (x, y, z, 1); s = (c.x * scale + 1) / 2, t = (c.y * scale + 1) / 2;
     (s, t, 1, 0) is written back to the batch (+6 + 4i) and (s, t, 1) / w goes to the output;
   - colour `ftoi0`;
   - vi4 = number of the last three vertices outside the clip volume. If positive: go to 3.
     Otherwise add the vertex's flag; if the result is positive set ADC.
3. (verified) `tri_reject_test` (106..123): six `fcor` tests; if the three vertices are all
   outside the same plane, only ADC is set. Otherwise the triangle is clipped (4) and ADC is set.
4. (verified) Clip (124..163, `clip_tri` 164..319): save registers; grow the triangle by a
   factor 1.005 about its centroid in clip space; build the closed polygon v0 v1 v2 v0 in buffer
   A with colours read back from the output packet and s, t read back from the batch; six passes
   (z > -w, z < w, x > -w, x < w, y > -w, y < w) of Sutherland-Hodgman clipping between buffers
   A and B (`clip_edge`, `intersect`, `poly_close`), returning early when a pass leaves nothing.
   Crossing point: d = component - sign x w at both ends, t = |d_cur / (d_next - d_cur)|,
   position, colour and s, t interpolated linearly.
5. (verified) `emit_fan` (409..450): the polygon in buffer A through matrix 0x008, times 1/w,
   to 0x01E as a fan, and `xgkick`. If vi13 (the flag of the triangle's last vertex) is not
   zero, ADC is set on every fan vertex.
6. (verified) After the last vertex: `xgkick` of the strip, end.

- (verified) The clipper does not look at the no-draw flag before running. The two strip
  triangles that join consecutive input triangles are therefore clipped too when they cross the
  volume; their fans are sent with ADC on every vertex.
- (inferred) The 1.005 growth is there to hide cracks between a fan and its neighbours.
- (inferred) `emit_fan` first kicks the tag with NLOOP 0 so that the previous fan has left the
  buffer before it is rewritten (`xgkick` waits for the previous transfer).

## Differences from program 4

Compared by the instruction columns of `src/vu1/prog4.vsm` (399 instructions) and this listing.
Everything not listed is identical apart from branch targets.

1. Entry 0 loads 0x00C..0x00F into vf13..vf16 instead of 0x008..0x00B into vf9..vf12. The four
   loads of vf9..vf12 are at the top of `emit_fan` (409..412, +4 instructions), because the vertex
   loop uses vf9 as scratch.
2. w of the input position: program 4 multiplies the fourth matrix row by the batch's w
   (`vf20w`); program 6 uses `vf0w` = 1 in the three transforms of the vertex loop (33, 48, 87).
   `emit_fan` still uses the polygon's w, as in program 4.
3. vi13: in program 4 the constant 0x8000 (set once in the set-up). In program 6 the no-draw
   flag of the current vertex (`ilwr.w` at 44, 95, 158), and 0x8000 is rebuilt in vi11 where it is
   needed (159..160, 417..418, 421..422, 428..430).
4. Program 4 kicks the first two output quadwords (the TEX0_1 write) right after copying them;
   program 6 has no such `xgkick`, so they are never sent.
5. Vertex stride 4 instead of 3. Program 4: position, colour, s t. Program 6: position, normal
   (unused), colour, s t slot. The clipper's read-back offsets of s, t change from -7 / -4 / -1 to
   -9 / -5 / -1.
6. Texture coordinates: program 4 reads s, t from the batch. Program 6 inserts 28 instructions
   (51..78, 16 of them `nop | nop`) that compute them and write them back to the batch.
7. After the outside test, program 6 adds the flag test (94..97) that sets ADC; program 4 goes
   straight to the loop end. The reject / clipped path of both sets ADC.
8. The save frame around `clip_tri`: program 4 pushes vf20, vf23, vf24, vf25 and zeroes vf31
   after the call; program 6 pushes vf20, vf21, vf22, vf23, vf24, vf25, vf31 and pops them.
9. `emit_fan`: besides 1 and 3, the loop is reordered and gains the conditional ADC store
   (434, 442..444).
10. Small reorderings in the vertex loop (the first `div` moved after the header stores, the
    stores and integer adds of 82..90 shuffled, branch delay slots that held work in program 4
    hold `nop`).
11. Outside the microprogram: 17 constant quadwords instead of 12, BASE / OFFSET 0x92 / 0x1B2
    instead of 0x8D / 0x1B4, and the screen and clip matrices come from the view block instead
    of `Vu0Screen_StoreMtx` / `Vu0Clip_StoreMtx`.

## Open questions

- Why the texture-coordinate block is spread over 28 instructions with long runs of nops. The
  arithmetic needs far fewer.
- Why the batch still carries the TEX0_1 header and the normal when the program uses neither
  (inferred: the format was kept from program 4's batch and from the collected triangle).
- The extra saves of vf21, vf22 and vf31 look unnecessary (vf21 and vf22 are rewritten before
  their next use, the clipper never writes vf31); not checked on an emulator.
- BASE moved from 0x8D to 0x92 although nothing in the program uses 0x08D..0x091.
- The exact latency assumptions (clip flag read 4 instructions after the last `clip`, Q read 7
  instructions after `div`) were checked by counting only.
- The colour's range and the shadow camera's convention (which way s and t run on the page) come
  from the C side and were not traced here.
- Program 6 has no rewrite in the PC port yet; `hle_program4` and `vu4.vert` in `bt3-port` do not
  apply to it as they stand (different stride, generated s, t, ADC flags).
