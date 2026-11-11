# Input

Sources: `src/sys/pad.c`, `src/sys/game_pad.c`. Layouts and bit definitions:
`include/sys/pad.h`, `include/sys/game_pad.h`. The battle input path (`src/battle/btl_input.c`) is linked; see the
note at the end.

## Pad state (verified)

`gPad[2]` at 0x333800, 0x1C0 bytes each. Two ports, slot 0, no multitap. Library: Sony libpad2
on libdbc, libvib for vibration.

| Offset | Field | Meaning |
|---|---|---|
| 0x000 | `work[0x100]` | libpad2 socket work area |
| 0x104 | `state` | 1 = usable |
| 0x108 | `phase` | 0 get vibration profile, 1 get button profile, 2 read |
| 0x114 | `data[18]` | raw report: [0],[1] buttons (active low), [2] RX, [3] RY, [4] LX, [5] LY |
| 0x130 / 0x138 | `left[2]` / `right[2]` | stick x, y as floats |
| 0x148 | `held` | |
| 0x14C | `prev` | |
| 0x150 | `pressed` | `held & ~prev` |
| 0x154 | `repeat` | |
| 0x160 / 0x164 | `vibPower` / `vibSmall` | summed per frame, sent, then cleared |
| 0x180 / 0x184 | `status` / `lastStatus` | 0 = ok, 0xFF = none |
| 0x188 / 0x18C / 0x190 | `gameHeld` / `gamePressed` / `gameRepeat` | menu input word, see below |
| 0x19C / 0x1A0 | `repeatDelay` / `repeatInterval` | init 20 / 1 |
| 0x1A4 / 0x1AC | `gameLeft[2]` / `gameRight[2]` | stick copies |

- There is no "released" word in the pad layer.
- Sticks: `raw - 127.5`, pulled toward 0 by a dead zone of 50 and clamped at 0, divided by 77.5;
  if the vector is longer than 1 it is normalised to the unit circle.
- The sticks also set digital direction bits at a 0.5 threshold on the dominant axis.
- A digital-only pad (profile F9 FF 00 00) is discarded: it reads as no input, status 0xFF.
- A disconnected port resets: buttons 0, raw sticks 0x80, floats 0. `Pad_Reset` does not clear
  `gameLeft` / `gameRight`, so those keep stale values while a pad is unplugged.
- The `Pad` struct must be declared 16-byte aligned to match.

### Raw button bits (`held` / `pressed` / `repeat`)

`((data[1] << 8) | data[0]) ^ 0xFFFF`, plus stick bits.

| Bits | Meaning | Status |
|---|---|---|
| 0x0001..0x0008 | SELECT, L3, R3, START | inferred from the standard DualShock layout |
| 0x0010..0x0080 | UP, RIGHT, DOWN, LEFT | inferred, consistent with the game word |
| 0x0100..0x0800 | L2, R2, L1, R1 | inferred |
| 0x1000..0x8000 | TRIANGLE, CIRCLE, CROSS, SQUARE | inferred |
| 0x10000..0x80000 | left stick left, right, down, up | verified |
| 0x100000..0x800000 | right stick left, right, down, up | verified |

## Menu input word (verified)

`Pad_UpdateGameButtons` builds `gameHeld` from 25 hard-coded tests on `held`. The mapping is
fixed and identical for both ports; there is no key config at this level.

| Bit(s) | Set by |
|---|---|
| 0-3 | d-pad LEFT, RIGHT, DOWN, UP |
| 4-7 | right stick LEFT, RIGHT, DOWN, UP |
| 8-11 | CIRCLE, CROSS, TRIANGLE, SQUARE |
| 12, 13 | START, SELECT |
| 14-17 | L1, L2, R1, R2 |
| 18, 19 | L3, R3 |
| 20 | tested with mask 0, never set |
| 21-24 | d-pad LEFT, RIGHT, DOWN, UP again |

The left stick's digital bits are not mapped; menus use the analog copy.

Repeat (`Pad_CalcRepeat`) works on the whole word: while `held` is non-zero and unchanged, a
timer counts down; when it goes below 0 it reloads with the interval and the whole word is
reported. Any change reloads the timer with the delay. A non-zero `pressed` always wins. Default
delay 20, interval 1. `Pad_SetRepeat` is called by the memory-card block (20, 1 and 40, 3) and
by the overlay.

There are no reader functions: about 90 overlay functions and about 20 in the main executable
read the `gPad` fields directly.

## Battle input (verified by `src/battle/btl_input.c` unless marked)

Fights do not use the menu word. Layouts: `include/battle/btl_input.h`. One function,
`BtlInput_Update`, is still in assembly (six instructions out of order); what it does is known
from its near-matching C.

### Per-frame flow

1. `func_001C2A28` (skipped under battle flags 0x100 or 0x2000) samples each fighter:
   `BtlInput_Sample` copies `gPad[chr->pad]` and builds the record. The pad index is `chr+4`;
   the key-config and replay index is `chr+0`.
2. `func_001C2B30` later calls `BtlInput_Update`, which fetches the record through the ring,
   applies gating, and derives held / previous / pressed / released words and frame counters.

### Input record

`BtlInputRecord`, 16 bytes at `chr+0x938`:

| Offset | Field | Content |
|---|---|---|
| +0 | tag | 'O','P','R','T' |
| +4 | player | |
| +5 | | always 0 |
| +6, +7 | stickX, stickY | 0..0xFE, 0x7F neutral; forced to 0 / 0xFF when a d-pad bit is down |
| +8 | buttons | u32 |
| +0xC | commands | u32 |

### Button word

"Action k" is the physical button `i` with `gSaveData->key[player][i] == k`, over (Circle, Cross,
Square, Triangle, L1, L2, R1, R2). The default config is `{2, 1, 0, 3, 4, 5, 6, 7}`. The mask
table is built once per fighter reset, not per frame.

| Bit | Source | Default button |
|---|---|---|
| 0 | action 2 | Circle |
| 1 | action 1 | Cross |
| 2 | action 3 | Triangle |
| 3 | action 0 | Square |
| 4-7 | up, down, left, right: d-pad, else the left stick's dominant axis | |
| 8, 10 | right stick left, right | |
| 9 | action 5 | L2 |
| 11 | action 6 | R1 |
| 12 | action 7 | R2 |
| 13 | R3 | |
| 14 | action 4 | L1 |
| 15 | L3 and R3 together | |
| 16-19 | right stick up, down, left, right | |
| 20 | any of actions 0-3 | |
| 21, 22 | bit 11 / bit 12 pressed again within 7 frames (double tap) | |
| 23 | SELECT | |
| 24 | action 3 again | |
| 25, 26 | right stick up, down again | |
| 27-30 | physical Circle, Cross, Triangle, Square (not remapped) | |

The gameplay names in the header (`BTLB_GUARD`, `DASH`, `BLAST`, `RUSH`, `CHARGE`, ...) are
guesses from the default layout; the bit sources are verified.

### Command word

Built from the button word and its history by `BtlInput_BuildCommands`: combinations such as
"b1 held without b9", "b1 pressed while b9 held", "b3 released under 7 frames after its press",
"b3 held exactly 6 frames", plus direction qualifier bits. The full table is in the header. It
also depends on three pieces of fighter state (a frame counter block, `chr+0x1594`, and a
per-fighter table entry), so it is a function of button history plus fighter state, and the
game recomputes it rather than recording it.

### Ring buffer

Eight entries `{buttons, commands, stick[2]}` at `chr+0x8C0`. `BtlInput_Fetch` is the only user:
it pushes the record and pops immediately, so the ring is always empty between frames and there
is never any delay. The public push / pop / count wrappers have no callers.

### Gating after the ring

Not part of the record:
- input is forced neutral under battle flag 0x200, or fighter flags 0x11E, 0x11F, 0xB1;
- under battle flag 0x400 (the Ready state) it is neutral on stages 4 and 27, otherwise masked
  to movement and guard (buttons 0x008F18F3, commands 0x3C010011).

### CPU input

`chr+0x1278` is set at fighter reset when the side is CPU-controlled. The CPU then writes
buttons and stick floats to `chr+0x127C..0x1284` through `func_00208198`, whose only caller is
`func_001B6CD8` (the AI, inferred). Injected input skips key config and double-tap detection
and then goes through the same stick merge, command building and replay hook.

### Replay hooks (read from disassembly; not decompiled)

- Recorder `func_001D8388`: appends `{stick[2], buttons}` per fighter per frame; ignores the
  command word. Stops at 9000 frames (5 minutes at 30 Hz).
- Player `func_001D8470`: reads the next frame; past the end it returns neutral. The command
  word is rebuilt from the replayed buttons.
- Buffer per player (0xD2F8 bytes): `u8 stick[9000][2]`, `u32 buttons[9000]`, count, position.
- The hook only runs when the fighter is taking input (fighter flag 2 or 3 set, flag 0x136
  clear); otherwise the record is neutral and nothing is recorded or consumed.

### Direct pad readers in battle-side code

- `DbgCam_Update` (was `func_0023F0F0`): L1 and the sticks. No callers in either binary: dead
  debug code (inferred from the absence of any call or table reference).
- `OrbitCam_Update` (was `func_0023F708`): sticks of pad 0, for an orbiting viewer camera whose
  callers look like a viewer screen, not the battle (inferred).
- `func_001D8590`: pad 0 up/down. Not examined.

## Fighter input reference (verified)

- `BtlInput_TestAction(chr, id, want)` and the stick readers belong to the same original source
  file as `btl_input.c`. It evaluates 114 numbered input conditions; the table is in
  `include/battle/btl_char_ctl.h` and summarised in combat.md.
- The two per-frame bit sets at fighter +0x918 record which conditions were queried this frame.
- The decision code that maps conditions to actions is 0x2012D8..0x204E00 (being decompiled).
