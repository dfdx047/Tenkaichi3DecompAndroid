# Maths and random numbers

Sources: `src/sys/math3d.c`, `src/sys/rand.c`. Layouts: `include/sys/math3d.h`,
`include/sys/rand.h`.

## Conventions (verified where a matching function shows them)

- `Vec4 {x, y, z, w}`, 0x10 bytes. Vectors are four floats even when only xyz is used; locals
  sit 0x10 apart and are passed to the VU0 `Vec3_*` routines.
- `Quat {x, y, z, w}`, w at +0xC.
- `Mtx44`, 0x40 bytes, row-vector convention (`v' = v * M`): `Quat_ToMtx` writes
  `m[0][1] = 2(xy + wz)` and `m[1][0] = 2(xy - wz)`. That translation is row 3 is inferred.
- All game maths is single-precision float. `double` would go through software routines.

## Quaternion and spline routines (verified)

`Vec3_Hermite`, `Quat_SetIdentity`, `Quat_FromAxisAngle`, `Quat_FromVectors`, `Quat_LengthSq`,
`Quat_Length`, `Quat_SlerpIdentity`, `Quat_GetAngle`, `Quat_Conjugate`, `Quat_Inverse`,
`Quat_Mul` (Hamilton product), `Quat_Slerp` (shorter arc), `Quat_ConjugateBy`,
`Quat_LimitAngle`, `Quat_Normalize`, `Quat_Pack`, `Quat_Unpack`, `Quat_ToMtx`,
`Quat_FromEuler` (qy * (qx * qz)), `Mtx_ToEuler`, `Quat_ToEuler`, `Quat_FromMtx`.

`Quat_Pack` / `Quat_Unpack`: a quaternion in 64 bits. The largest component is dropped; the
other three are stored as 20-bit fields scaled by sqrt(2) / 1048575; the index of the dropped
component is in bits 60..63. `Quat_Unpack` has 10 call sites (0x24BF58, 0x24C1A8, 0x1FC1E8,
0x1FCEF0), which look like animation key decoding (inferred).

## VU0 vector routines

176 functions at 0x11FA10..0x122940 are the vector library (linked: `src/sys/vu0_a_c*.c`,
`vu0_b_c.c`; see the two sections at the end). Well over half are hand-written VU0 (the PS2's
vector coprocessor) assembly in the original and are carried as assembly blocks inside the C
files. None of the four files emits data: the library's constants are reached as externs. `func_0011F780` and
`func_0011F740` are acos and asin with the argument clamped to [-1, 1].

## Scalar helpers (`src/sys/mathf.c`; verified)

- `Mathf_Sin(a) = sinf(wrap(a))`, `Mathf_Cos(a) = Mathf_Sin(a + pi/2)`: 101 and 78 call sites.
- The wrap (`Mathf_WrapAngle`, hand-written): `r = 2*pi; while (a < r) a += r; while (pi < a)
  a -= r;`. An intended lower bound is overwritten, so every angle below 2 pi is pushed up and
  brought back. (inferred) In-range angles are quantised to the float spacing near 2 pi, and a
  huge angle never terminates.
- `Mathf_SinFast` / `Mathf_CosFast` (18 / 20 sites): the polynomial
  `x + c3 x^3 + c5 x^5 + c7 x^7 + c9 x^9` on the vector unit. Different bits from the pair
  above.
- `Mathf_Tan` wraps correctly by 2 pi steps, then `tanf`. `Mathf_Asin` / `Mathf_Acos` clamp to
  [-1, 1] first. `Mathf_Sqrt` is the vector unit's square root.
- `BtlUtil_WrapAngle` (battle side) adds or subtracts 2 pi once.

## Random numbers

Seven sources; the table with reset points and users is in netplay_notes.md.

1. `BtlChar_Rand` and `BtlScene_Rand`: `(state * 714025 + 4096) % 150889`, separate states,
   reset with the fighters and with the effect scene. (verified)
2. `BtlChar_FrameMod(n)`: the fight's frame counter modulo n. (verified)
3. libc `rand()`: about 490 direct call sites, the main generator for effects; seeded at boot
   from a timer. `Rand_IntRange(a, b) = lo + rand() % (hi - lo + 1)`. (verified)
4. `Rand_Float01` / `Rand_FloatRange`: the vector unit's R register (7 steps, re-init from its
   own output, 7 more steps, minus 1.0, giving [0, 1) in 23 bits). Seeded once at boot from a
   constant, so the stream is the same every boot. (verified) The usual model of the register
   is a 23-bit shift register with feedback from bits 4 and 22. (inferred, not from this
   binary)
5. `Rand_*`: MT19937 seeding and tempering, but the refill only runs the first loop and the
   final word; 396 of 624 state words are never regenerated. Used by the AI and the menus.
   (verified)
6. A second MT19937 copy at 0x252F68, a menu codec seeded per call. (inferred)

## Float constants and matching

The compiler truncates decimal literals: `3.1415925f` gives 0x40490FD9, one bit below the
original's 0x40490FDA; `3.14159265f` is needed. Same for sqrt(2). The diff tool masks constant
relocations, so each emitted constant has to be checked against the original data by value.

## Vector / matrix library, second half (0x121008..0x122940; names in config/symbols/vu0_b.txt)

92 functions, all in `src/sys/vu0_b_c.c` (linked, no gap): 25 are compiled C and match; 67 are
hand-written VU0 code, kept as top-level assembly blocks in that file (three as raw words:
`Vec3_Normalize` and two others the disassembler does not decode). Every one has an exact portable reference
implementation, `Ref_<Name>` in `src/port/vu0_b.c` (not part of the matching build; it has been
compiled and sanity-tested on the host, not checked against a console or emulator). The full
table "address, name, exact semantics" is in the source comments at the top of that file.

Register conventions (read from disassembly): vf0 = (0,0,0,1); vf1 / vf2 / vf3 = the z / y / x
unit vectors, set at boot by 0x120088; vf16..19 the current matrix with a stack in VU0 memory;
vf20..23, vf24..27, vf28..31 three camera matrices (inferred: world-to-clip, world-to-screen,
world-to-view).

Exact semantics that matter (read from disassembly unless marked):
- dot = (x*x' + y*y') + vf3.x * (z*z'); `Vec3_Length` / `Vec3_Dist` use the VU0 square root.
- Every ".xyz" routine stores four components: out.w = the first input's w, except
  `Vec3_ScaleAdd` / `ScaleSub` (the second vector's w), `Vec3_Copy` (keeps the destination's w),
  `Vec3_Normalize` and `Vec3_Cross` (w = 0).
- `Vec3_Normalize`, `Vec4_Div` / `Vec3_Div` and every projection multiply by a VU0 reciprocal
  instead of dividing. **A zero vector normalises to (0,0,0,0) on the PS2** (division by zero
  gives a large finite number, not NaN); 312 call sites.
- `Vec4_Lerp(dst, a, b, t)` = a*t + b*(1-t), so t = 1 gives a. `Vec4_SetZeroW1` writes
  (0,0,0,1), not zero.
- (verified) `Vec3_RotateAxis` / `RotateX` / `RotateY` are Rodrigues rotations with
  `Mathf_Cos` / `Mathf_Sin`; they copy w from the source after writing the result, so with
  out == v (which callers do) w is not preserved: an original quirk to reproduce.
- (verified) `ClipPoly_ClipPlane`: Sutherland-Hodgman against one plane, at most 9 vertices, no
  bound check.
- Projection output is GS coordinates (x, y in 12.4 fixed point); "on screen" is 0 < x, y <
  4096 and w > 0, strict.
- Trigonometry comes from three different sources depending on the routine: libm `sinf` /
  `cosf`, the game's `Mathf_Sin` / `Mathf_Cos`, and a VU0 polynomial.

Where the VU0 differs from IEEE (inferred, from emulator documentation; which routines are
affected is from the disassembly): rounding is toward zero; no NaN, infinity or denormals
(overflow clamps, underflow gives zero); division by zero gives +/- max; no fused
multiply-add; float-to-int truncates and saturates; min / max compare bit patterns. A port
must choose between soft-float (bit-exact with the PS2) and host arithmetic (all peers must
then use identical modes and compiler flags; PS2 replays would not reproduce).

VU0 R register (the generator behind `Rand_Float01`), rule from emulator documentation:
init R = 0x3F800000 | (bits(x) & 0x7FFFFF); next: b = ((R >> 4) ^ (R >> 22)) & 1;
R = 0x3F800000 | (((R << 1) | b) & 0x7FFFFF). `Vu0_Init` seeds it from 0.1234141f at boot.

## Vector / matrix library, first half (0x11FA10..0x121008; names in config/symbols/vu0_a.txt)

84 functions. All 84 match in three files (`src/sys/vu0_a_c*.c`, linked): 36 are
compiled C and 48 are the original hand-written VU0 routines kept as top-level assembly blocks
inside those files. Exact portable references are in `src/port/vu0_a.c`: each routine executes
the original instruction sequence on a register model (vf0..31, ACC, Q, vi, flags, VU0 memory),
with arithmetic through an integer-only model of PS2 floats that gives the same bits on every
host. Compiled and self-tested on the host; not validated against a console or emulator.

- (verified) The library is Sony's libvu0 under other names plus a matrix stack: the camera,
  light, view-screen, drop-shadow and rotation matrix builders matched from the Sony source.
- (verified) Matrices are row-vector with the translation in row 3. `Mtx_Mul(dst, a, b)`: the
  FIRST argument is the destination, and b is applied first. `Mtx_RotateX/Y/Z(dst, src, angle)`
  = src x R. Euler order: `RotateZXY` applies Z, X, Y; `RotateXYZ` applies X, Y, Z.
- (verified) The current matrix is vf16..19 with a stack of 64 matrices in VU0 data memory and
  no overflow check. Identity matrices are written from registers vf1..vf3, which hold the unit
  axes only because `Vu0_InitAxisRegs` ran at boot.
- (verified) **Rotation sine / cosine come from `Vu0_SinCos`**: a degree-9 polynomial for the
  cosine, sine = sqrt(1 - cos^2) with the angle's sign, and no range reduction (callers must
  pass |angle| <= pi). It gives different bits from `Mathf_Sin` and from libm, and the sine is
  coarsely quantised near 0 and +-pi.
- (verified) The "scale" routines multiply only the three diagonal elements (not a scale on a
  rotated matrix); 15 call sites. `IVec4_InGsRange4` tests its third point twice and never
  reads the fourth.
- (inferred, emulator knowledge) PS2 float rules beyond rounding toward zero: add / subtract
  keep one guard bit and no sticky bit, so they can differ from IEEE round-toward-zero by one
  unit; the multiplier can be one unit low on real hardware (NOT modelled: x * 1.0 may not be
  exactly x, which touches every rotation and dot product); the overflow / divide-by-zero
  value is 0x7FFFFFFF or 0x7F7FFFFF (unsettled). These need an emulator or console trace to
  settle before the reference can be called bit-exact.
