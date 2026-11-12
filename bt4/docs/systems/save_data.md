# Save data

Source: `src/sys/save.c`. Layout: `include/sys/save.h` (`SaveData`, exactly 0x4000 bytes).

`gSaveData` points at a 0x4000-byte heap block that is the whole persistent save, not just the
options: the memory card code copies 0x4000 bytes into it after a load and passes it whole when
saving.

## Layout (verified by matching code unless marked)

| Offset | Field | Notes |
|---|---|---|
| 0x0008 | `unlockFlags` | 7 bits (plus bit 9 used by the menu); meaning unknown |
| 0x0010 | `slot[9]`, 0x10 each | flags + three values; probably per-mode progress (inferred) |
| 0x0C10 | `charaBits[3]` (u64) | 161 character unlock bits |
| 0x0C28 | `stageBits` (u64) | 35 stage unlock bits |
| 0x0C30 | `bgmBits` | 20 bits of a 25-entry list; "BGM" is a guess |
| 0x0C34 | `rule[6]` | "battle rules" is a guess; defaults 3, 2, 2, 0, 0, 0 |
| 0x1608 | `flags` | see below |
| 0x160C | `key[2][8]` | battle button assignment per player |
| 0x164C | `keyEdit[2][8]` | the copy the controller menu edits |
| 0x1694, 0x1698 | unknown | copied per side into the battle option block |
| 0x169C, 0x16A0 | `screenX`, `screenY` | display position |
| 0x16A4 | `soundMode` | 0 stereo, 1 mono |
| 0x16A8 | `bgmVolume` | 0..9 |
| 0x16AC | `seVolume` | 0..9 |
| 0x1808 | `custom[97]`, 0x38 each | three sets of seven item slots, plus a level |
| 0x2D40 | `rec[14]`, 0x1C each | level and character (-1 = empty); saved custom characters (inferred) |
| 0x2EC8 | `item[350]` | bit 0 owned, bit 1 set on first acquisition ("new" is a guess) |
| 0x3028 | `money` | 0..9,999,999 |

Everything else is unidentified.

`flags` at 0x1608:
- bit 0: voice set. Set selects the file base 0xCC32, clear 0x8D4E. It is on by default, so set
  is presumably English in this build (inferred).
- bits 1-2: one per controller, default on; read by battle fighter init. Vibration is a guess.
- bits 3-4: one per controller, default off; meaning unknown.

## A new save (verified)

- Characters locked at the start (ids): 7-10, 0x18, 0x26, 0x36, 0x45-0x48, 0x4B, 0x4C, 0x61,
  0x6E, 0x78, 0x91, 0x95-0xA0. All other ids 0..160 are unlocked.
- Stages locked at the start: 0x11-0x16, 0x1F, 0x20 (of 35).
- Items: every item whose table entry has flag bit 1 is given (table in common file 4).
- Key tables `{2, 1, 0, 3, 4, 5, 6, 7}` for both pads; both volumes 9; flags |= 7.

## Functions

| Name | What it does |
|---|---|
| `Save_Init` | allocates the block, zeroes it, writes defaults |
| `Save_SetDefaults` | writes a new save; also called by the memory card code |
| `Save_UnlockAll` | no callers: sets every character and stage bit, grants items, money 4,850,000 |
| `Save_AddItem(idx)` | marks an item owned and new (name is a guess) |
| `Save_AddMoney(n)` | adds, clamped to 0..9,999,999 (name is a guess) |
| `Save_ResetRules` | restores the six rule defaults (name is a guess) |

## Notes

- About 210 references to the save block in the overlay are not symbolised (they appear as
  `lui 0x30` / `lw -0xD74`), so grepping for the name misses most menu uses.
- `func_002BD950`..`func_002BECC0` in the trailing block of the main executable also grant items
  and money (a reward path); not analysed.

## Memory-card flows (0x1198D8..0x11EC10; src/sys/mcflow_a.c, linked; names in config/symbols/mcflow_a.txt)
Linked layout: `.rodata` 0x2EBD30 (0x518), `.sdata` 0x2FE940 (`gMcFlow`, defined in the file, then "DBZT3" / "DBZT3R").
The file's private aliases for the card layer are gone: it calls the `McCard_*` names of stgm_a_b.c.

All 22 functions match. One module (`McFlow_*`, final name sys/mc_flow.c): a heap work area
`gMcFlow` (0x2FE940, 0x39F90 bytes; layout in include/sys/mcflow_a.h) and eight state machines
chosen by `McFlow_Start(mode)`: 0 / 4 save, 1 load, 2 boot load, 3 new save, 5 replay save,
6 replay load, 7 / 9 replay scan, 8 replay question. `McFlow_Update` runs one frame and
returns the state (0 = finished). The full state tables are in the comments of mcflow_a.c.

Verified by matching C:
- The system save is the raw 0x4000-byte `SaveData`; its first 8 bytes are a checksum (bytes
  from +8 summed, odd positions shifted left by 8, 16-bit end-around carry; word 0 = sum >> 8,
  word 1 = sum & 0xFF), written just before saving and verified after loading. After a load
  the block is copied into `*gSaveData`; the menu load also calls `Progress_ClearSession()`.
- Card directories: "DBZT3" (system) and "DBZT3R" + slot (replays); 7 replay slots. Free
  space needed: 61 clusters for the system save, 160 for a replay.
- **Replay file on the card: 0x1AC00 bytes** = 0x38 header (`sum[2]` over the file from +8,
  `hdrSum[2]` over +0x10..+0x38, `chara[2][5]` character ids with 0xA4 = empty), then the
  0x1ABA8-byte `BtlReplay` block, then 0x20 unused. Replay load calls `Battle_ClearWork()`
  then `BattleReplay_Load(buf + 0x38, 0x1ABA8)`. `McFlow_Init` snapshots the current replay
  (`BattleReplay_GetBuffer`) into the image to save.
- `gProgress + 0x69C`: replay slot list, 0x2C per slot (flag word, 2x5 character ids).
  `gProgress->flags` bit 2 = "continue without saving" chosen at boot; bit 3 = a save was
  loaded at boot.
- Timing is frame counts only (question answers act after 12 frames, busy states wait 120);
  pad 0 confirm (`gamePressed & 0x200`) and `Dialog_Input`; no random draw, no clock.
- Original oddities: two-armed tests with identical arms; `McFlow_Term` reads the work before
  its NULL check; the caller passes an argument `McFlow_Init` does not take.

Inferred: what each message index says (the text was not extracted); the names of the card
library calls in 0x116BA0..0x1190C8 (the neighbouring range).

For linking: define `McFlow *gMcFlow = NULL;` at the top of the file (it is `.sdata` at
0x2FE940 before "DBZT3" / "DBZT3R"); `.rodata` 0x2EBD30..0x2EC248.

## Memory-card layer (0x116B98..0x1198D8; src/battle/stgm_a_b.c, linked; final name sys/mcard.c)
Linked layout: `.rodata` 0x2EBA80 (0x2B0), `.sdata` 0x2FE918 (`gMcCardStep`, `gMcCardCmd`, defined in the file, then its
strings from 0x2FE920).

All 20 functions match (`McCard_*`, names in config/symbols/stgm_a.txt). Polled operations
over Sony libmc: each call makes one library call or one `sceMcSync` poll; state in
`gMcCardStep` (0x2FE918) / `gMcCardCmd` (0x2FE91C); per-port record `gMcCardPort[2]` at
0x331D80 (0x24 bytes). Called by `main()` (`McCard_Init`) and by `McFlow_*`.

- (verified) A save is a directory "BASLUS-21678DBZT3" (system) or "BASLUS-21678DBZT3Rnn"
  (replay) holding icon.sys (built in code), dbzsm.ico / dbzsmr.ico, and a data file named
  like the directory. Saving writes the data file first and repairs the icon only if it is
  missing or has the wrong length.
- (verified) Space budget in 1024-byte units: 16 + 1 + 40 (system), 107 + 1 + 48 (replay).
- (verified) **Original bug**: the checksum verify joins its two byte comparisons with `&&`,
  so a block is rejected only when BOTH stored bytes are wrong. Also: two operations share
  command code 23; three functions fall off the end without a return value.
- (inferred) The libmc function identities (0x2A1A48 sceMcInit .. 0x2A2AC8 sceMcFormat, table
  in the source header), from their arguments.
- For linking: define `s32 gMcCardStep = 0; s32 gMcCardCmd = 0;` first in the file; `.rodata`
  at 0x2EBA80, `.sdata` from 0x2FE918.

## Character password codec (0x252F68..0x254A20; src/sys/misc_a.c, misc_a_b.c, linked; names in config/symbols/misc_a.txt)
Linked layout: misc_a.c `.rodata` 0x2F2AA0, `.sdata` 0x2FEC50; misc_a_b.c `.rodata` 0x2F2CB0, `.sdata` 0x2FEE58 (0x20A and
0x202 bytes each).

All 35 functions match. Two builds of one codec, called only by the menu overlay: `OldPass_*`
(0x252F68, 32- and 20-character texts) and `ChrPass_*` (0x253ED8, the 34-character text).
Final names menu/pass_old.c, menu/pass.c. Most of the range is dead code (old encoders, the
20-character format, random test fillers, "No Debug\n" getters).

Verified by matching C:
- A password is a 32-byte bit buffer, least significant bit first, 6 bits per character
  through a 64-character table (current table
  `QRST2VWX1JKLMN5P0BCD3FGHYZ4bcd9fwxyz!#$%6pqrst8vgh7jklmn&@-+*()?`; characters not in the
  table are silently skipped on decode).
- Bits 0..31 are a seed in clear. Bytes from 4 on are XORed with the low bytes of tempered
  MT19937 state words. The state comes from `init_by_array({s, 2s, 3s, 4s})` (without the
  reference code's final `state[0] = 0x80000000`), is read from index 1 and is never twisted.
- Current format (34 characters): charId at bit 0x20 (8 bits), item[8] from bit 0x28 (9 bits
  each), extraSlots at 0x70 (4 bits), one more bit at 0x74; a bit-count checksum at 0x77, an
  XOR parity block (bytes 16..23 = bytes 0..7 ^ bytes 8..15), a second bit count at 0xC4.
  The old 32-character layout has 7 items of 10 bits.
- Each codec owns its state (0x31C7E0 / 0x31D1F0, index at 0x2FEC50 / 0x2FEE58), reseeded on
  every call: independent of `gRandState` and of the simulation. **But `ChrPass_Encode` draws
  libc `rand()` twice** to make the seed (menu only).
- Original bugs: the last checksum nibble test in `ChrPass_Decode` can never fail; the old
  decoder does not clear the bit buffer first.

Inferred: "Old" = the previous game's password format (the overlay still accepts and converts
it); item ids valid up to 350, character ids 0..160 (from overlay disassembly).
