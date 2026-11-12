# VU1 microprograms

The nine VIF packets right after the code (`.vutext`, 0x2BF6B0..0x2C3380) are kept as assembly listings in
`src/vu1/prog*.vsm`, assembled by `scripts/vuasm.py` and linked through `src/vu1/vutext.s` (see `configure.py`).
The listings assemble to the original bytes (part of the normal gate). Program numbers are the address order;
there is no program 3: the packet the old notes called "prog 3" is the second variant of program 2.

| Program | Packet | Instr. | Draws | Loaded by | Notes |
|---|---|---|---|---|---|
| 0 | 0x2BF6B0 | 127 | fighter model part, lit (texture layer + toon layer) | `Vu1Pkt_LoadProg0` | [prog0_prog1.md](prog0_prog1.md) |
| 1 | 0x2BFAB0 | 124 | fighter model part, fade pass | `Vu1Pkt_LoadProg1` | [prog0_prog1.md](prog0_prog1.md) |
| 2a | 0x2BFEA0 | 99 | fighter model in one flat colour, off-screen triangles dropped (flat shadow) | `Vu1Pkt_LoadProg2(0)` | [prog2_prog7_prog8.md](prog2_prog7_prog8.md) |
| 2b | 0x2C01C0 | 101 | the same without the drop (silhouette into the shadow page) | `Vu1Pkt_LoadProg2(1)` | [prog2_prog7_prog8.md](prog2_prog7_prog8.md) |
| 4 | 0x2C04F0 | 399 | stage geometry, with a triangle clipper | `Vu1Pkt_LoadProg4` | [prog4_prog5.md](prog4_prog5.md) |
| 5 | 0x2C1180 | 429 | program 4 with an object matrix; no caller found | `Vu1Pkt_LoadProg5` | [prog4_prog5.md](prog4_prog5.md) |
| 6 | 0x2C1F00 | 451 | ground shadow: stage triangles under an object, textured with the shadow page | `Vu1Pkt_LoadProg6` | [prog6.md](prog6.md) |
| 7 | 0x2C2D30 | 104 | stage debris (rigid, per-mesh alpha) | `Vu1Pkt_LoadProg7` | [prog2_prog7_prog8.md](prog2_prog7_prog8.md) |
| 8 | 0x2C3080 | 94 | animated stage objects (two matrices per node) | `Vu1Pkt_LoadProg8` | [prog2_prog7_prog8.md](prog2_prog7_prog8.md) |

Common shape (verified in every listing): `MSCALF 0` runs a setup that loads constants into registers; each
`MSCNT` then handles one batch at `xtop` (a few header quadwords, then the vertices) and sends one GIF packet
with `xgkick`. Programs 0, 1, 2, 7 and 8 do not clip: a triangle with a vertex outside the clip volume gets the
"no drawing" bit (2b not even that). Programs 4, 5 and 6 clip such triangles into fans.

Status of the knowledge: the instruction-level description is read from the code; nothing was run on hardware
or an emulator for these notes. What each note marks "(inferred)" is not confirmed. The chains that feed the
programs live in model data, so "one MSCNT per batch" and the exact GIF tags come from the PC port's dumps of
programs 0 and 4, not from this repository.
