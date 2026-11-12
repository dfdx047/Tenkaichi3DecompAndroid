# VU1 programs 0 and 1: fighter model parts

Listings: `src/vu1/prog0.vsm` (127 instructions), `src/vu1/prog1.vsm` (124). Numbers in `[..]` are
instruction addresses. C side: `src/sys/vu1_packet.c` (`Vu1Pkt_LoadProg0/1`, `Vu1Pkt_CallProg0/1`),
callers in `src/sys/gfxm_d_c.c` (`BtlObjDraw_DrawModel`, `BtlObjDraw_DrawModelFade`,
`ObjDraw_DrawParts`).

"Verified" below means read from the microcode or the C code in this repository. "Port" means seen
on the PC port's VU memory dumps and reproduced by its shader (`bt3-port/port/src/gs/gs_vu1.c`
`hle_program0`, `shaders/vu0.vert`), which only covers program 0. "Inferred" is neither.

## Shared frame (verified)

- **Load** (`Vu1Pkt_LoadProgN`, once per object): the MPG upload, then UNPACK V4-32 of 0x22
  quadwords to VU address 0, BASE 0x22, OFFSET 0x1EF. No program is started. For program 0 the
  caller fills nothing in that block, for program 1 only `screen` and `clip`; it does not matter,
  every call uploads the block again.
- **Call** (`Vu1Pkt_CallProgN`, once per model part): FLUSHE, UNPACK of the 0x22 constants,
  MSCALF 0, BASE, OFFSET, then a DMA `call` into the part's VIF chain (part record + 0x60; for
  node 0x30 program 0 can get an alternative chain). Both programs are given the same chain.
- **Double buffer**: TOP alternates between 34 and 529. A batch is read at TOP, the GIF packet is
  built at TOP + 257 and sent with one `xgkick`.
- **Control flow**: `[0]` is the setup run by MSCALF 0 and ends with E. Each later run continues
  behind the last E: the first one falls into the batch code, the following ones land on a
  `b batch_entry`. The MSCNT codes themselves are in the model data (Port: one MSCNT per batch).

### Constants (ObjDrawPkt, VU address = (field offset - 0x10) / 16)

| VU addr | field | program 0 | program 1 |
|---|---|---|---|
| 0..3 | `mtxA` | bone matrix A | same |
| 4..7 | `mtxB` | bone matrix B | same |
| 8, 9 | `ofsA`, `ofsB` | pivots of A and B | same |
| 10..13 | `light` | `Mtx_MakeNormalLight(lightDir, 0, 0)`: x of the product = -L . n | normal to camera-aligned frame (see below) |
| 14..17 | `screen` | to GS screen coordinates, before the divide | same |
| 18..21 | `clip` | to clip space, reject test only | same |
| 22 | `color` | layer 0 colour = `view->color` | (128, 128, 128, `view->fade`) |
| 23 | `k` | layer 1 colour = (128, 128, 128, 128) | same |
| 24, 25 | | not read | not read |
| 26 | `tex0`, `texReg` | scratch (see below) | A+D data: TEX0_2 = fade texture |
| 27..33 | | scratch: 26..29 = light x A, 30..33 = light x B | not read |

A matrix is used as `q0*x + q1*y + q2*z + q3` on its four quadwords.

### One batch at TOP

| offset | content | source |
|---|---|---|
| +0 | GIF tag, one A+D register, no EOP | Port; no EOP follows from the kick |
| +1 | A+D data of layer 0 (TEX0_1, the model texture) | Port |
| +2 | A+D data of layer 1 (TEX0_2, the shading ramp); program 1 ignores it | Port / verified |
| +3 | GIF tag of layer 0's strip; low 15 bits = vertex count n | verified (count), Port (rest) |
| +4 | GIF tag of layer 1's strip | Port |
| +5+3i | position xyz, w = weight of bone A | verified |
| +6+3i | normal xyz; program 1 tests the low 16 bits of w of vertex 0 | verified |
| +7+3i | (s, t, 1) | verified (use), Port (the 1) |

Port: the strip tags name ST, RGBAQ, XYZF2, PRIM 0x5C (context 1) and 0x25C (context 2).

### Output packet at TOP + 257 (verified)

`tag(+0), A+D, tag(+3), n x (STQ, RGBA, XYZF)` for layer 0, directly followed by
`tag(+0), A+D, tag(+4), n x (STQ, RGBA, XYZF)` for layer 1. It needs 6 + 6n quadwords of the 238
available, so n <= 38 (arithmetic; no check in the code).

## Program 0: lit draw

Purpose: draws a part twice in one packet, the model texture and on top a ramp texture addressed by
the light, which is the cel shading (Port for the visual result).

Entry points: `[0]` setup, `[36]` batch, `[125]` re-entry.

Steps (verified):
1. Setup `[0..35]`: store light x A at 26..28 and light x B at 30..32 (the three rotation columns);
   29 and 33 get light's own fourth column. Keep A in vf1..4, the pivots in vf30/31,
   int(k) in vf18, 0x8000 in vi13.
2. Batch header `[36..57]`: read the five header quadwords, n = tag(+3) & 0x7FFF, copy the two
   layer headers to the output.
3. Per vertex `[62..121]`, with w = position.w:
   - p = pB + (pA - pB) * w, pA = A * (pos - ofsA, 1), pB = B * (pos - ofsB, 1)
   - l = lB + (lA - lB) * w, lA = (light x A) * (normal, 1), lB likewise; u = 0.5 + 0.5 * l.x
   - s' = screen * (p, 1), c = clip * (p, 1), q = 1 / s'.w
   - layer 0: STQ = (s, t, 1) * q, RGBA = int(color)
   - layer 1: STQ = (u, 0, 1) * q, RGBA = int(k)
   - both: XYZF = ftoi4(s' * q) (x, y in 12.4; the fourth word is 16)
   - `clip` c against |c.w|; if any flag of this vertex or the two before it is set, the fourth
     word of XYZF becomes 0x8000 in both layers (ADC: the triangle is not drawn). No real clipping.
4. `xgkick`, end.

Details worth knowing (verified):
- The loop is software-pipelined: the transform of the next vertex by A starts in the tail
  `[103..116]`, so one position past the batch is read and discarded.
- The clip flags are not reset per batch; the first two vertices of a strip are judged together
  with the previous strip's last ones, which has no effect on a strip.
- n = 0 would wrap the counter; the data must not contain it.

## Program 1: fade pass

Purpose: the same parts again (only those with part flag bit 0), layer 0 with the model texture
and layer 1 with the model's fade texture looked up by the normal's direction relative to the
camera. On the GS side (`ObjGs_AddFadeEnv`, `ObjGs_AddFadeTex`) context 1 writes alpha only, behind
an alpha test against `view->fade`, and context 2 uses ALPHA 0x58 with clamped coordinates. What it
looks like on screen has not been checked.

Entry points: `[0]` setup, `[15]` batch, `[111]` re-entry after a drawn batch, `[122]` after a
skipped one.

Differences from program 0 (verified):
- Setup precomputes nothing; constants 10..13 are used directly on the normal, so the normal is
  taken through one matrix only and is not blended between the bones. C builds that matrix as
  `Mtx_Mul(facing^-1, rotation of mtxA)` with `facing` from `ObjDraw_MakeFacingMtx(view direction)`.
- Layer 1 coordinates are two-dimensional: (u, v) = 0.5 + 0.5 * (light * (normal, 1)).xy
  (program 0: u from x only, v = 0).
- Layer 1's A+D data comes from constant 26 (TEX0_2 = fade texture, written by C), not from
  batch +2.
- Batch skip `[19..22]`: if the low 16 bits of the w field at TOP + 6 (first vertex's normal) are
  0, nothing is drawn; `[113..121]` writes 0x8000 (NLOOP 0, EOP) into the first word at TOP + 255
  and kicks that empty packet.
- Colours: both are converted per vertex (vf9, vf10); vf18 is used for the constant (1, 1, 1, 0).
- Position, screen transform, divide, XYZF and the reject test are instruction for instruction
  the same as program 0.

## About the labels

The raw listings had a label on most lines. `scripts/vuasm.py --disasm` computes a branch target
from the low 11 bits of every lower instruction before it checks the opcode, so non-branches
produced targets too. The real branch targets are: program 0 `[36]`, `[62]`, `[120]`; program 1
`[15]`, `[46]`, `[106]`, `[113]`. The other labels were removed; `setup`, `batch_resume` and
`skip_resume` were added for the entry points.

## Inferred / open

- Meaning of the mark in normal.w of a batch's first vertex (program 1's skip test). Inferred: the
  model data marks the batches that carry the fade layer. Needs a look at a model file.
- Why a skipped batch kicks an empty packet instead of just ending; the other three words of that
  quadword are stale memory.
- Layer 1's strip tag carrying the EOP that ends the kick: required for the packet to work, not
  read from data here.
- The fourth XYZF word being 16 means fog coefficient 1 if the register really is XYZF2 (Port);
  the PRIM values seen have fog off.
- Argument order of `Mtx_Mul` for program 1's matrix (written above as facing^-1 times the bone
  rotation because that is the order that makes sense for a normal); `Mtx_Mul` itself was not
  re-read.
- The second and third rows of program 0's light matrix come from normalising zero vectors; the
  program never uses the y and z of the product, so their content does not matter.
- Program 0's setup also computes int(color) into vf23, which the loop recomputes: dead work in
  the original.
