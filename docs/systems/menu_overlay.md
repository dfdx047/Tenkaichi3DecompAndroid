# Menu overlay (DBZP.BIN)

Loaded at 0x334C00; code 0x334C00..0x3B0E04 (737 functions), data from 0x3B0E80. Built with
-G0 (verified: no gp-relative access anywhere). Data is laid out per object as `.data` then
`.rodata`. Source goes in src/menu/. Brief and chunk table: [upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/briefs_menu_overlay.md).

## Link state (eleventh step, 2026-10-05): the whole overlay is linked from C

build/DBZP.BIN is byte-identical from 69 C files (the 93 chunk files after 24 merges of head / tail halves);
no assembly code chunk is left, only the zero padding between `.text` and the data (0x3B0E04..0x3B0E80). 728 of
the 737 functions are C; 9 are INCLUDE_ASM inside linked files ([upstream notes](https://github.com/z3xox/Tenkaichi3Decomp/blob/ec3c85a98cffd9cd22c22ac14cd62fb584d23664/docs/open_questions.md)). The file names in the
chunk sections below are the ORIGINAL chunk files; where a section says "append to" / "merge with", that is done
and the tail file is gone:

| Object (source file now) | Range | Was |
|---|---|---|
| menu_a_d.c `ModeMenu` | 0x338020..0x33A360 | menu_a_d + menu_b |
| menu_b_d.c `HistSel` | 0x33CFC8..0x33F700 | menu_b_d + menu_c |
| menu_c_e.c `CharSel` | 0x342190..0x348D78 | menu_c_e + menu_d + menu_e |
| menu_e_b.c `TeamSel` | 0x348D78..0x351C38 | menu_e_b + menu_f |
| menu_g_c.c `DuelMenu` | 0x352EC0..0x3562B8 | menu_g_c + menu_h |
| menu_h_d.c `Train` | 0x359358..0x35D660 | menu_h_d + menu_i |
| menu_i_d.c `EntrySel` | 0x35E0F8..0x3623A8 | menu_i_d + menu_j |
| menu_j_b.c `TourMenu` | 0x3623A8..0x364DA8 | menu_j_b + menu_k |
| menu_k_f.c `Bracket` logic | 0x368068..0x36B3E0 | menu_k_f + menu_l |
| menu_l_b.c `SoloSel` | 0x36B3E0..0x36E028 | menu_l_b + menu_m |
| menu_m_b.c `UbTeamSel` | 0x36E028..0x372560 | menu_m_b + menu_n |
| menu_n_d.c `UbResult` | 0x3760C8..0x3782A8 | menu_n_d + menu_o |
| menu_o_d.c `MisSel` | 0x379F58..0x37B7C0 | menu_o_d + menu_p |
| menu_p_d.c `UbScore` | 0x37EE18..0x37F850 | menu_p_d + menu_q |
| menu_q_b.c `SimDay` | 0x37F850..0x3851B0 | menu_q_b + menu_r |
| menu_r_d.c `SimTop` | 0x387880..0x388EE0 | menu_r_d + menu_s |
| menu_s_d.c sim events 0..27 | 0x38C360..0x3900D0 | menu_s_d + menu_t |
| menu_u_h.c `EvoZ_Refresh` .. `EvoZ_Load` | 0x393C58..0x396838 | menu_u_h + menu_v |
| menu_v_d.c `Shop` | 0x399790..0x39E940 | menu_v_d + menu_w |
| menu_w_c.c `EvoTop` | 0x39EB08..0x39FAA8 | menu_w_c + menu_x |
| menu_x_c.c `Option` | 0x39FBB8..0x3A65E8 | menu_x_c + menu_y |
| menu_y_b.c `DcList` | 0x3A65E8..0x3A9850 | menu_y_b + menu_z |
| menu_z_d.c `DcPass` | 0x3AAF30..0x3AE648 | menu_z_d + menu_za |

Facts the link established (verified by the identical image):
- menu_u_g.c (`EvoZ_Init` .. `EvoZ_Run`, 0x392F10..0x393C58) and menu_u_h.c are TWO source files
  ("mc_ability_limit_up" exists twice), not one as the chunk notes assumed. menu_k_b .. menu_k_e are separate
  objects from the Bracket logic file.
- `CharSel_Input`, `Train_Update`, `Train_Input` and `DcList_Draw` match as C once their file is whole;
  `SimEv28` is C (its file is assembled with -G8, see docs/decomp_guide.md).
- `.data` (all defined in C now): every screen with a global work struct has one pointer, in link order:
  0x3B0E80 `gMainMenu`, `gMenuArc0..12`, `gTitle`; 0x3B12F0 ModeMenu, ModeBg, HistOutro, HistSel, HistResult,
  HistSave; 0x3B38D4 CharSel, TeamSel, (pad), `gItemPanel[2]`, DuelMenu; 0x3B4854 CharRef; 0x3B4BA8 Train;
  0x3B5908 BootCard, Logo, EntrySel, TourMenu, Bracket (defined in menu_k_b.c), TourBg; 0x3B7348 SoloSel,
  UbTeamSel, UbzSel, UbRank, UbResult, DiscFusion, MisSel + `gMisSelFaceTex[5]`, MisResult, UbMenu, SimDay,
  `gSimEvent[37]` (the event handler table, `SimEv00`..`SimEv36` in order), SimResult, SimTop +
  `gSimTopFaceTex[5]`, SurvSel, SurvResult; 0x3BB140 EvoZ, ItemHelp, Shop; 0x3BC364 Option; 0x3BC928
  `gDcPassKeys[2][5][14]`, `gPassWin`, `gPassKeys[4][14]`, `gReplayMenu`, `gDcSave`. `EvoTop`, `DcList`, `DcMenu`
  and `DcPass` keep their work struct on the stack of their `_Run` function (no pointer).
- `gMenuArc12` (0x3B0EB4, file baseFile + 0x18) is the wish screen's archive: defined by the overlay, loaded and
  freed by the main executable (src/sys/late_a.c).
- Overlay globals in the main executable's `.bss` / `.sbss` are fixed-address symbols
  (build/DBZP_extern_syms.ld, from the symbol files). The overlap at 0x31EA80..0x31EAA4 is REAL: CharRef's
  nine-entry state table `gCharRefState` covers the words UbMenu (0x31EA90), the three sim trainings
  (0x31EA94..0x31EA9C) and the card game (0x31EAA0) use. How the original linker came to overlap them
  (inferred: common symbols) is not established; the screens never run at the same time.

## Entry and mode dispatcher (src/menu/menu_a_b.c; verified by matching C)

`Game_Main` calls `Progress_Main` (0x336A90) after `Overlay_Load(0)`; when it returns,
`Game_Main` runs `Battle_Main(0)`. `Progress_Main` loops `switch (gProgress->mode)` through
the 70-entry table at 0x3B1130 until a handler returns non-zero.

| Mode | Handler | Ends the loop |
|---|---|---|
| 1 | `Title_Run(1)` (0x337D70) | on result 1 (attract demo set up) |
| 2 | opening movie, then mode 1 | no |
| 4 | `MainMenu_Run(2)` (0x336838) | no |
| 6..10 | 0x33CBF8 | if non-zero |
| 13..30 | 0x379A58 | if non-zero |
| 33..35 | 0x362160 | if non-zero |
| 38..41 | 0x352CB8 | if non-zero |
| 44..45 | 0x3591C8 | if non-zero |
| 48..50 | 0x39E940 | never |
| 53..56 | 0x3A9850 | if non-zero |
| 60 | 0x3590A8 | never |
| 62 | 0x39FAA8 | never |
| 70 | `Shen_Main` (main executable 0x2BD230) | never |
| others | nothing: the loop spins forever |

Main-menu item -> mode: 0 -> 6, 1 -> 13, 2 -> 33, 3 -> 38, 5 -> 48, 6 -> 44, 7 -> 53, 8 -> 60,
9 -> 62, 10 -> 70 (item 10 only with all seven dragon balls, `gSaveData->unlockFlags & 0x7F`).
(inferred) which game mode each is.

Before the loop: `Pad_SetRepeat(0x28, 3)`; first-run partition load (0x35DF70) when
`gProgress->flags & 0x40`; the return screen after a battle is chosen from
`BattleResult_GetReason()` when `BattleResult_GetFlags() & 8` (bit 8 -> mode 4; 0x10 -> 39 /
40 / 45 by current mode; 0x80 -> 38; 0x800 -> 6; 0x1000 -> 56); sound bank file 0x14C into
bank 2. Twelve archive pointers at 0x3B0E84..0x3B0EB0, one per group of modes, freed on exit.

`gProgress` fields (verified): +0x04 base file id; +0x14 flags (0x40 first run, 0x100 freeze,
8 title left with result 0); +0x18 mode; +0x28 attract-demo counter; +0x2C last main-menu
item; +0x34 sub menu; +0x38 last sub-menu item.

## Screens of chunk 1 (0x334C00..0x339610; all 25 functions match)

- **Title** (mode 1, menu_a_c.c, `gTitle` 0xD0 bytes): still picture 300 frames, then the
  movie (intro, "press start", menu of 1 or 2 items). Item 0 runs `McFlow_Start(3)`. After
  1800 idle frames: `Demo_SetupBattle()` and leave (attract demo), twice, then the opening
  movie replays. That call is the only battle hand-off in this chunk.
- **MainMenu** (mode 4, menu_a.c, `gMainMenu` 0x188 bytes): a ring of items with six plates
  shown, four guide characters, the seven dragon balls from `unlockFlags`.
- **ModeMenu** (menu_a_d.c, head only; continues in chunk 2; `gModeMenu` 0x33C bytes): the
  sub menu chosen by `gProgress->subMenu` (0..7), up to 16 items, a picture per item loaded
  in the background; `gSaveData->slot[n].val[0..2]` = listed / cleared / new bit sets.
- Common: pad 0 only (`gameRepeat` 8 up, 4 down; `gamePressed` 0x200 confirm, 0x400 cancel,
  0x1000 start); `gProgress->flags & 0x100` freezes update and input; Title and MainMenu call
  `Gfx_EndFrame(1)`. Random draws all from `Rand_Range` (the shared Mersenne Twister): title
  voice choice, guide choice at every cursor move, blink timers, `FlashAnim_Blink / Talk`.
  No clock reads.
- Menu asset layout: a pack is a table of byte offsets; a screen's section is compressed
  (`Sprite_Unpack`) into an inner pack of texture lists, one movie, text and subtitles.
- Original oddity: an unhandled mode value hangs `Progress_Main` in a busy loop.

## Story mode ("Hist"), modes 6..10 and 3 (chunk 2, 0x339610..0x33E108; src/menu/menu_b*.c; all 22 functions match)

Names are guesses from behaviour. Files: menu_b.c = ModeMenu tail (must be appended to
menu_a_d.c: one source file, shared string; merged test in build/scratch_menu_b/), menu_b_b.c
= `ModeBg` (scrolling backdrop), menu_b_c.c = `HistOutro` + `Hist_Main`, menu_b_d.c = head of
`HistSel` (continues in chunk 3).

`Hist_Main` (0x33CBF8; verified) loads archive 2 (file baseFile + 0xC) and runs:

| Mode | Screen | Next |
|---|---|---|
| 6 | saga select (`HistSel`, 0x33F440) | 0 -> mode 4; 1 -> 7; 2 -> 3 |
| 7 | episode list (`ModeMenu_Run`) | non-zero -> mode 8 and RETURN 1 (battle); 0 -> 6 |
| 8 | result (`HistResult_Run`) | 0 -> 7; 1 -> 10; 2 -> 9 |
| 9 | saga outro (`HistOutro_Run`) | 0 -> 10; 1 -> 3 |
| 10 | save (`HistSave_Run`) | 0 -> 6; 1 -> 7 |
| 3 | ending movie | back to 6 or 10 |

`gProgress->subMenu` is the saga (0..7; episodes per saga 3, 4, 7, 5, 16, 5, 4, 4 = 48).

**Battle hand-off (verified)**: `BattleSetup_Clear()` then
`BattleSetup_SetScript(ModeMenu_GetLine(saga, episode))` (`setup.script` = the episode's
running index + 1). Nothing else is written to the battle setup: a story battle is defined
entirely by its script number. The episode's "new" bit (`slot[saga].val[2]`) is cleared.

Other verified facts: saga pack = file baseFile + 0x10 + saga (sections: 1 picture, 2/3
textures, 4 message text, 5 subtitles, 6 description table of 0x20-byte entries, 7/8 guide
faces, 9 movie, 10 backdrop textures); episode pictures = file imageBase + episode.
`SaveSlot.flags`: 1 saga unlocked, 4 outro seen, 0x10 outro script started, 0x20 excluded
from the random event, 0x40 first-visit greeting; `slot[n].val[1]` cleared bits give the
completion percentage. Saga select random event: `Rand_Range(100) < 4` per eligible saga per
visit when `unlockFlags & 0x80`. All random draws are `Rand_Range`; pad 0 only.

Correction to the note above: the overlay's `.rodata` is per source file, but its `.data`
(the work pointers) is grouped per group of modules (0x3B12F0..0x3B1310 holds the six story
work pointers; 0x3B0E80 those of MainMenu / Title and the archive pointers).

## Story mode continued (chunk 3, 0x33E108..0x342588; src/menu/menu_c*.c; all 29 functions match)

Files: menu_c.c = tail of `HistSel` (same object as menu_b_d.c: merge them and delete the
stub copy of `HistSel_PlayVoice`), menu_c_b.c = `HistGuide` (the saga-select guide scripts: a
switch with 363 case labels), menu_c_c.c = `HistResult` (mode 8), menu_c_d.c = `HistSave`
(mode 10), menu_c_e.c = head of the `CharSel` object (continues in chunk 4). menu_c.h's
`HistSel` (full layout) replaces the head-only view in menu_b.h.

Verified by matching C:
- Saga select (`HistSel`, mode 6): Goku guides; a two-item menu (saga list / level); leaving
  with a saga writes `gProgress->subMenu` and `gProgress + 0x3C` = the save's level (inferred:
  difficulty). Nothing here touches the battle setup.
- Result screen (`HistResult`, mode 8): win = `BattleResult_GetFlags() & 1`; a win sets the
  episode's cleared bit. Points = `gProgress->reward.points[level]`, doubled while item 0x88
  is owned, paid with `Save_AddMoney` 33 a frame. `gProgress + 0x40` reward block:
  `points[3]`, `item[3]` (+0x4C), `chara[3]` (+0x58), `stage[3]` (+0x64), `episode[3]` (+0x70),
  -1 = none (filled by the battle script). Rewards go into the save at once (`Save_AddItem`,
  `charaBits`, `stageBits`, new saga / episode bits, dragon ball from `BattleResult.unk44`).
- Save screen (`HistSave`, mode 10): `McFlow_Start(0)` on a black screen.
- `SaveSlot.flags`: 1 listed, 2 new, 4 outro seen, 8 guide introduced, 0x20 event done,
  0x40 new episode; `unlockFlags` 0x80 first-visit explanation seen, 0x100 100 % speech,
  0x200 new saga.
- **Save layout**: `SaveData` is `{ s32 sum[2]; struct { s32 unlockFlags; s32 level;
  SaveSlot slot[9]; ... } body; }`: the nested body reproduces the original address
  arithmetic (`MSave` / `MSAVE` in include/menu/menu_c.h; should move to include/sys/save.h).
- Random draws: `Rand_Range` only (greeting line, ambient timer, result picture
  0x3FE + `Rand_Range(3)`).
- (inferred) The mode is "Dragon History"; `GetWin_IsAnimating` may really mean "animation
  finished".

## Character select hand-off and team select (chunk 5, 0x348710..0x34D368; src/menu/menu_e.c, menu_e_b.c; all 19 functions match)

One object runs 0x342190..0x351C38: `CharSel` (chunks 3 tail, 4, and `CharSel_Run` here), then
`TeamSel` (head here, rest in chunk 6). Names are guesses. Layouts in include/menu/menu_e.h
(`TeamSel` 0x3EA4 bytes; menu_f.h has a second view to unify).

**Versus battle hand-off (`CharSel_Run`, 0x348710; verified by matching C).** Called by the
handlers of modes 38..41 and 44..45. On leaving, the choices go to `gProgress` (+0x440 /
+0x530 the two sides' 0x30-byte records, +0x628 stage cursor, +0x62C music id), then:
1. Stage 0x23 = random: `stageIds[Rand_Range(stageCount - 1)]` until below 0x23. Music 0x18 =
   random: `Rand_Range(9) + 8`. (Shared Mersenne Twister, drawn before the battle starts.)
2. `gProgress->mode == 45`: battle mode 6, no time limit, announcer 1, cpuLevel -1. Otherwise
   battle mode 0, screenMode = (players == 1), and from `gSaveData->rule[0..5]`: time limit
   (0 -> 1, 1 -> 2, 2 -> 3, 3 -> 4, 4 -> 0), `CpuLevel_FromSetting(rule[1])`, announcer,
   two side flags, one more flag.
3. `Battle_ClearWork()`, `BattleSetup_SetRule(screenMode, mode, bgm, timeLimit, announcer,
   stage, unk10)`.
4. `BattleSetup_SetSide` by `players`: 0 = pad vs CPU, 1 = pad vs pad, 2 = CPU vs CPU
   (control 0 = pad, 2 = CPU).
5. Per side: `BattleSetup_SetMember(side, 0, rec->chara, rec->color, 0, cpuLevel, 100.0f,
   rec->items)`; then `BattleSetup_Finish()`.
This is the complete set of inputs a versus fight starts from (with the random stage / music
resolved in the menu): what netplay peers must agree on before the simulation starts.

Team select (`TeamSel`, verified unless marked): screen pack = a section of `gMenuArc5` with
57 sections (textures, movies, stage id list, master character grid, 165 chip texture lists,
item panel data, music id list); portraits = file 0x2F9 + character, stage pictures = file
0x39D + stage. Restores both teams (five 0x30-byte records each), players (+0x620), battle
type (+0x624), stage, music and DP level (+0x630) from `gProgress`. DP battles: limit 10 /
15 / 20 by level, `TeamSel_FitsDp` = cost + `ChrTbl_GetCost(chara)` <= limit;
`TeamSel_IsCharaFree` refuses a character `ChrTbl_IsRelated` to another member. Member record
(0x30 bytes): +0x18 costume, +0x1C character id (-1 none), +0x20 `BattleItemSet`.

## Character / stage / music select (chunk 4, 0x342588..0x348710; src/menu/menu_d.c; 11 of 12 match)

`CharSel_Input` (0x3456A8, 0x3064 bytes) matches as C since the merge (in the stand-alone chunk file it differed
in 3 of 3097 instructions), so what is said about input below is verified. `gCharSel` = 0x3B38D4, 0x3A4C bytes; layouts in include/menu/menu_d.h. The file only
lays out correctly merged behind menu_c_e.c (one object 0x342190..0x348D78).

- (verified) Per-side pick record kept in `gProgress` (+0x440 / +0x530, first 0x30 bytes of a
  0xF0-byte side record): col, row, form, customCol, customRow, item set, costume, picked
  character id, items[8]. `gProgress + 0x620` pad mode (1 = one pad per side; 0 or 2 = pad 0
  picks both sides in turn).
- (verified) Grid markers as this screen uses them: 0xA1 random, 0xA2 locked, 0xA3 saved
  custom characters, 0xA4 filler (**view_a.h has 0xA1 / 0xA3 swapped**). Music list id 0x18 =
  random, 0x19 = locked (`bgmBits` confirmed as the music unlock bits).
- (from the attempt) Side states: 0 grid, 1 form reel, 2 item set, 3 item panel, 4 item info,
  5 costume, 6 saved custom characters, 7 done; then stage (8) and music (9) on pad 0.
  Confirming the costume of the random cell draws `Rand_Range(masterCount)` until a real
  character, then `Rand_Range(formCount)`: drawn on the frame the button is pressed.
- (from the attempt) Item sets come from `gSaveData->custom[col + row * 7].item[set - 1]`
  (97 entries of 0x38 bytes, per base character, 3 sets of 8 u16) or `gSaveData->rec[n]`.
- Original oddities: cancelling the stage phase returns a custom-list side to the wrong state.

## Team select input and hand-off (chunk 6, 0x34D368..0x351C38; src/menu/menu_f.c; all 3 functions match)

`TeamSel_Update`, `TeamSel_Input` (16.5 KB) and `TeamSel_Run`. menu_e_b.c + menu_f.c are ONE
source file (the TeamSel object 0x348D78..0x351C38: `TeamSel_Input` only matches with three
of its callees defined above it; merged and linked now). CharSel and TeamSel
are separate files (duplicate strings), contrary to the note above.

- (verified) Mode 40 = `TeamSel_Run(2)`, called by `Duel_Main` (0x352CB8, modes 38..41): mode
  38 is the duel menu, which picks 39 (`CharSel_Run`, single characters) or 40 (teams) by
  `gProgress + 0x624`.
- (verified) **Team battle hand-off**: as `CharSel_Run` (random stage / music resolved with
  `Rand_Range`, rules from `gSaveData->rule[]`, battle mode 0, split screen only with two
  pads), but `BattleSetup_SetSide(side, control, pad, memberCount, ...)` and one
  `BattleSetup_SetMember(side, j, chara, color, 0, cpuLevel, 100.0f, items)` per member of
  each team.
- (verified) Per-side states: 0 character grid, 1 form reel, 2 item-set plates, 3 item panel,
  4 help window, 5 costume plates, 6 member list, 7 saved-custom list, 8 done; then the stage
  grid (9) and music list (10) on pad 0. Item sets: plate 0 = none; a saved custom character's
  set is the first 16 bytes of its `SaveRec` (`gSaveData + 0x2D40 + idx * 0x1C`); otherwise
  `gSaveData + 0x1808 + cell * 0x38 + (plate - 1) * 16`.
- (verified) Grid cell 0xA1 = random, 0xA3 = saved custom characters (the headers agree now).
- (verified) Random member: `Rand_Range(masterCount)` until a real character, then
  `Rand_Range(formCount)`, repeated until `TeamSel_IsCharaFree`.
- Original bugs: a random member is never checked against the DP limit, so a DP team can
  exceed it through the random cell; the random stage draw uses `Rand_Range(stageCount - 1)`,
  so the last stage in the list can never be drawn; the CPU level is given to pad-controlled
  members too.

## Duel (versus) mode, modes 38..41 (chunk 7, 0x351C38..0x356090; src/menu/menu_g.c, menu_g_b.c, menu_g_c.c; all 22 functions match)

- (verified) `Duel_Main` (0x352CB8), main-menu item 3; archive 5 = file `gProgress->baseFile`:
  mode 38 `DuelMenu_Run(1)` (non-zero -> mode 40 if `gProgress + 0x624` else 39; zero -> mode
  4); 39 `CharSel_Run(2)`; 40 `TeamSel_Run(2)` (non-zero from either = leave the overlay for
  the battle; zero -> 38); 41 a stub. The battle setup is written by the two selects only.
- (verified) `DuelMenu` (`gDuelMenu`, 0x194 bytes): levels 0 top (four plates; plate 1 needs
  `gPad[1].status != 0xFF`), 1 battle type (3 rows), 2 DP limit (3 rows), 3 settings, 4 value
  picker. Its three cursors are `gProgress + 0x620 / 0x624 / 0x630` (players, battle type, DP
  level). (inferred) plates = 1P vs COM / 1P vs 2P / COM vs COM / settings; types = single /
  team / DP.
- (verified) **`gSaveData->rule[0..5]` are edited only here**: settings rows 0, 1, 2, 3 (rule
  3 and 4, per side), 4 (rule 5); value counts 5, 5, 7, 2 + 2, 2; reset by `Save_ResetRules`.
  From the hand-off code: rule 0 time limit, 1 CPU level, 2 announcer, 3 / 4 per-side option,
  5 a battle flag.
- (verified) `ItemPanel` (0x351C38..0x352CB8; `gItemPanel[side]`, 0x6C8 bytes): the
  equipped-item panel of one side of the character selects; `ItemPanel_Input` returns the
  item id under the cursor (1-based), 0 on an empty row, -1 on cancel.
- (verified) The `.data` of four objects (CharSel, TeamSel, ItemPanel, DuelMenu) is contiguous
  at 0x3B38D4..0x3B38F0 and their `.rodata` follows in the same order up to 0x3B4854: the
  layout rule is per link group.

## Modes 13..30 group: selects (chunk 13, 0x36DBE8..0x372148; src/menu/menu_m.c, menu_m_b.c; all 17 functions match)

menu_m.c = tail of `SoloSel` (appends to menu_l_b.c; object 0x36B3E0..0x36E028); menu_m_b.c =
head of `UbTeamSel` (object 0x36E028..0x372560, continues in chunk 14). "Ub" is a placeholder
prefix for this mode family (main-menu item 1); its game name is not established.

Handler 0x379A58 (FROM DISASSEMBLY, not matched yet: chunk 15 owns it), archive 3 = file
baseFile + 3: mode 13 is a group menu leading to four sub-families:
- 14 -> 15 (select: `UbTeamSel` if `gProgress + 0x644` teamSize >= 2, else `SoloSel_Run`)
  -> battle -> 16 (result) -> 14;
- 17 -> 18 (`SoloSel_Run`) -> battle -> 19 -> 17;
- 20 -> 21 (`SoloSel_Run`) -> 22 -> battle -> 23 -> 20 or 22 (a counter at `gProgress + 0x674`
  advances: a ladder);
- 24 -> 25 / 28 -> 26 / 29 -> battle -> 27 / 30.

(verified) The selects do NOT write the battle setup: they copy the choice to
`gProgress + 0x440` (one 0x30-byte member record, or the whole 0xF0-byte team with
`gProgress + 0x644` = member count). Member record: +0 col, +4 row, +8 form, +0x14 item-set
plate, +0x18 costume, +0x1C character id, +0x20 eight u16 item ids. (from disassembly) the
handler then calls `BattleSetup_SetSide(0, 0, 0, teamSize, 1, 1, 0, 0)` /
`BattleSetup_SetMember(0, i, chara, color, 0, 0, 100.0f, items)` / `BattleSetup_Finish()` for
the player's side only; the rule and the opposing side are set by the preceding menu screens.

`UbTeamSel` (verified): a one-side team select like TeamSel (same DP rule 10 / 15 / 20 from
`gProgress + 0x648`, same related-character refusal), guides Android 17 / 18 (clip names),
voice base 0x8765; unlike TeamSel it has no random-cell draw. Random draws: blink timers and
`FlashAnim_*` only. Pad 0 only.

## Character reference (mode 60) and training handler (modes 44..45) (chunk 8, 0x356090..0x35A558; src/menu/menu_h*.c; all 41 functions match)

Files: menu_h.c = `DuelMenu_Run` (last function of the duel menu object), menu_h_b.c =
`CharRef` (a whole object), menu_h_c.c = the two handlers, menu_h_d.c = head of `Train`
(continues in chunk 9; merge with menu_i's first file).

- (verified) **Mode 60 = Character Reference** (`CharRefMode_Main` 0x3590A8; archive 9 = file
  baseFile + 15): Chi-Chi guides ("mc_guide_a/b_chichi"); a list of the characters set in
  `gSaveData->charaBits`; per character a profile, a voice sample
  (`Voice_PlayChara(0, chara, rand() % 2)`: libc `rand()`), her comment, and the model viewer
  (`ChrView_Show(chara, costume, 0)`; the frame loop runs `ChrView_Update` in that state).
  Never starts a battle; writes nothing to the save. Pack: 29 sections (table in the source).
- (verified) **Modes 44..45 = training** (`TrainMode_Main` 0x3591C8; archive 6 = file
  baseFile + 13; names from clip strings "mc_icon_training_clear", "fl_class_menu_*"): mode 44
  `Train_Run(1)` (0 -> mode 4; 1 -> leave for the battle; 45 -> mode 45); mode 45
  `CharSel_Run(2)` (the battle mode 6 branch of the versus hand-off). The lesson battle
  hand-off is inside `Train_Run` (chunk 9).
- (verified) `gSaveData + 0xE0C`: `s32 trainClear[3]`, one "lesson cleared" bit per lesson per
  class (three classes of up to 15 lessons).
- (verified) `DuelMenu_Run` copies `gDuelMenu->pick[0..2]` to `gProgress + 0x620 / 0x624 /
  0x630` on leaving.
- (verified) `gCharRefState` (0x31EA80) is in the MAIN executable's `.bss`: an uninitialised
  global of overlay source placed as a common symbol, so the overlay was linked together with
  the main executable.
- `Snd_PlaySe` returns `s32` (menu_a.h says so since the eleventh step).

## Dragon World Tour, modes 33..35: tournament menu tail and bracket screen (chunk 11, 0x364358..0x368C18; src/menu/menu_k*.c; all 16 functions match)

`Tour_Main` (0x362160, chunk 10) is main-menu item 2: mode 33 tournament menu (`TourMenu`),
34 entrant select, 35 bracket screen (`Bracket`, `gBracket` 0x3B5918, 0x2090 bytes).
menu_k.c appends to menu_j_b.c (one source file); menu_k_f.c (`Bracket_Load`) is the head of
the object that continues in menu_l (it starts at 0x368068, not 0x3673F8 as menu_l's notes
say). Module names "Bracket" / "TourBg" are guesses.

Verified by matching C:
- The tournament menu writes `gProgress + 0x80` (0 real tournament, 1 free play), `+0x84`
  tournament 0..4 (World / Big / Cell Games / Otherworld / Yamcha Game: names from music ids
  and clip names, partly inferred), `+0x88` level 0..2, `+0x8C` entrants 1..8.
- Bracket state lives in `gProgress`: +0x90 round, +0x94 match, +0x98 17 entrants (0x28
  each), +0x340 16 matches (0x10 each: flags 4 winner to left slot / 8 right / 0x10 final,
  `ent[2]`, next, pos). `gProgress->flags` 0x10 = a tournament is in progress.
- New tournament: fill entrants, shuffle, build matches; back from a battle: apply the result,
  play out the CPU-only matches, advance to the next match with a player entrant.
- **Battle hand-off**: `Bracket_Run` result 1 writes the bracket back to `gProgress` and calls
  `Bracket_SetupBattle` (0x36A720, chunk 12): the only place this mode writes the battle
  setup. Result 0 clears the bracket (tournament over).
- Winning the real tournament (or finishing runner-up) gives prizes (`Bracket_GivePrizes`,
  chunk 12), shows reward windows, then runs the save flow `McFlow_Start(0)`.
- Assets: pack = file baseFile + 6 + tournament; backdrop = file 0x3C9 + tournament.
- **Random draws**: a cheer line from `Rand_Range(14)` (7 for the Big Tournament) on the
  shared Mersenne Twister immediately before a battle hand-off; `rand() & 1` (libc) for a
  round-announcement line; `FlashAnim_*` every frame.
- Pad 0 only; `gameHeld` scrolls the tree.

## Dragon World Tour: handler, entrant select, tournament menu (chunk 10, 0x35F650..0x364358; src/menu/menu_j.c, menu_j_b.c; all 14 functions match)

menu_j.c is the second half of the `EntrySel` object (0x35E0F8..0x3623A8; merge behind
menu_i_d.c, recipe in the agent's notes at the top of the file); menu_j_b.c is the head of
the `TourMenu` object (continues in menu_k.c).

Verified by matching C:
- `Tour_Main` (0x362160): mode 33 `TourMenu_Run(1)` (BGM 0x10B19; zero -> mode 4), 34
  `EntrySel_Run(2)` (BGM 0x10B1B; zero -> 33), 35 `Bracket_Run(0)` (non-zero = leave for the
  battle; zero -> 33 and the tournament clock `gSaveData->unkA0C` advances by one, wrapping
  at 24). Archive 4 = file baseFile + 2.
- **Tournament clock**: `unkA0C` is shown as an hour ("mc_timer"); `TourMenu_CheckInvite`
  opens tournament 0 at hours 7..12, 1 at 13..18, 2 at 19..23, 3 at 0..4, 4 at 5..6 (one of
  `unkA08` bits 0..4) with a level `unkA10 = Rand_Range(3)`. The same counter is bumped by
  every main-menu confirm (chunk 1), so it is the game's shared "time of day".
- `gProgress + 0x7C` is the tournament session block (0x3C4 bytes, what
  `Progress_ClearSession` clears): +0x80 entry, +0x84 tournament, +0x88 level, +0x8C entrant
  count, +0x90 round, +0x94 match, +0x98 `TourEntrant[17]` (0x28 each: +0 u16 flags 1 player
  / 0x10 saved custom character / 0x20 has items; +4 chara; +8 costume; +0xC index; +0x18
  u16 item[8]), +0x340 match table.
- Entrant select (`gEntrySel`, 0x1C48 bytes): per entrant grid -> form -> item set -> item
  panel / help -> costume; in the Yamcha Game the entrants are drawn at random by Init and
  the player only confirms. Random cell: `Rand_Range(gridCount)` until a real character, then
  `Rand_Range(formCount)`, at costume confirm.
- Original oddity: a saved custom character entered with the "no items" plate loses its
  custom flag as well as its items.

## Tournament logic and battle hand-off; solo select (chunk 12, 0x368C18..0x36DBE8; src/menu/menu_l.c, menu_l_b.c; all 30 functions match)

menu_l.c = tail of the Bracket object (append behind menu_k's files); menu_l_b.c = `SoloSel`
(object 0x36B3E0..0x36E028, the one-character select of modes 15, 18, 21, 25, 29; mode 21
forbids item sets).

Verified by matching C:
- **Tree**: 17 entrants, 16 matches: a 16-entrant knock-out (matches 0..14) whose winner meets
  a seeded boss (entrant 16) in match 15. CPU entrants are drawn from the grid by cost (level
  0: cost < 4; 1: < 8; 2: >= 7), no duplicates; boss by tournament (0: 0x42 or 0x38; 1: 0x37;
  2: 0x6B or 0x6C; 3: 0x3D or 6; 4: 0x1A). 16 swaps shuffle the entrants (biased). CPU-only
  matches are decided by `Rand_Range(2)`.
- **Tournament battle hand-off (`Bracket_SetupBattle`, 0x36A720)**: `Battle_ClearWork()`; a
  player entrant is always battle side 0 on pad 0 (two players: side 1 on pad 1, split
  screen; otherwise side 1 is CPU, control 2); `BattleSetup_SetRule(screenMode, 4, bgm, 0,
  announcer, stage, 1)` (battle mode 4, no time limit); stage fixed or drawn with
  `Bracket_PickStage` (`Rand_Range` over the unlocked stages of a fixed list), music
  `Rand_Range(9) + 8` (tournament 0: 0x12), stage draw before music draw; per side
  `BattleSetup_SetMember(side, 0, chara, costume, 0, cpu[level * 5 + round].cpuLevel, health,
  items)`: a CPU's items and level come from the pack's `[level][round]` table (section 31,
  0x14-byte records), a player's items from the entrant record; health 100 except in the
  Cell Games (tournament 2), where a player carries over `health + 20` capped at 100 from
  the previous round. `BattleSetup_Finish()`.
- After the battle `Bracket_ApplyResult` reads `BattleResult_GetFlags()` (bit 0 side 0 won,
  bit 1 side 1 won) and stores the winner's `BattleResult.health[side]`.
- Prizes from `prize[tour].prize[kind][level]` (section 41); dragon ball chance in percent by
  level: winner 20 / 30 / 40, runner-up 5 / 10 / 15; Yamcha Game 40 / 50 / 60 and 15 / 20 /
  25; `Rand_Range(100) < chance`, then `Rand_Range(missing)`.
- All random draws are `Rand_Range` (shared Mersenne Twister): boss, entrants, shuffle, CPU
  matches, stage, music, dragon ball.

## Training menu, boot sequence, entrant select head (chunk 9, 0x35A558..0x35F650; src/menu/menu_i*.c; 33 of 34 match)

`Train_BuildLists` (0x35A610) is INCLUDE_ASM (11 of 118 instructions, a folded compare).
`Train_Update` and `Train_Input` match only when compiled in one file with menu_h_d.c (one
object, 0x359358..0x35D660; standalone they differed in 1 and 4 instructions): merged and matching now, with
menu_i.h's complete `Train` (0x1BE0 bytes). menu_i_d.c is the head of the
`EntrySel` object (continues in menu_j.c).

Verified by matching C:
- **First run** (`FirstRun_Main`, 0x35DF70, when `gProgress->flags & 0x40`): start loading
  partition 2 in the background; wait for pad 0 to be connected; `BootCard_Run(2)` (the boot
  save check: `McFlow_Start(2)` on a black screen); the six logo pictures (`Logo_ShowAll`,
  120 frames each with 60-frame fades, skippable with start / confirm except the last); wait
  for partition 2 with the loading screen if needed; `Movie_PlayOpening()`. Archive 0 = file
  baseFile + 0x19.
- **Training menu** (mode 44, `Train_Run`): levels 0 top (0 = character select -> mode 45,
  1 = lessons), 1 class, 2 lesson list, 3 the guide's introduction, 4 explanation pages
  (picture files 0x402 + page id), 5 "cleared" sequence (sets bit `1 << lesson` in
  `gSaveData + 0xE0C[class]`, the only save write). Lessons are data-driven from pack section
  22 (0x10-byte records `{u16 id; u8 pageNum; u8 flags; s32 firstPage; s32 voice; u8 stage;
  u8 bgm; u8 chara[2]}`; 13 + 11 + 14 lessons). `gProgress` training block: +0x7D4 flags
  (1 tutorial, 2 battle set up, 4 first clear pending, 8 already cleared, 0x10 left for the
  character select), +0x7DC cursors, +0x7F4 tutorial number.
- **Training battle hand-off** (`Train_Leave`, lesson flag 2): `Battle_ClearWork()`;
  `BattleSetup_SetRule(0, 5, rec->bgm, 0, 7, rec->stage, 0)` (battle mode 5, no time limit);
  side 0 pad, side 1 CPU; `BattleSetup_SetMember(i, 0, rec->chara[i], 0, 0, cpuLevel, 100.0f,
  items)` for both sides with the same values: cpuLevel -999 and no items by default, or from
  an 8-row table keyed by lesson id (cpuLevel -1 or 15, one item 0x89 / 0x99 / 0x9A in slot
  7); `BattleSetup_Finish()`. Lesson flag 1 instead starts a scripted tutorial through
  `gProgress + 0x7F4` (inferred meaning).
- **Development asset names**: the Train object carries 20 unreferenced host paths
  ("host:data/test/ut/..."), one per pack section, e.g. `UltimateTraining_top_PS2_.fod` (so
  `.fod` is the movie format), `ut_guide_saiyaman` / `ut_guide_bidel` (.dbt textures),
  `utraining_lips` (lip data), `font_Training_JP` (.pak). Guides: Great Saiyaman and Videl.
- Entrant select head: in the Yamcha Game every entrant is drawn with `Rand_Range` at Init
  (cell until a real character, item set of 4, form, costume).
- Original bug: the training idle-line end test compares against a NEW random draw instead
  of the line that was said.

## Sim sub-mode (modes 20..23): day screen and battle hand-off; score sheet tail (chunk 17, 0x37F430..0x3840E0; src/menu/menu_q.c, menu_q_b.c; all 33 functions match)

menu_q.c = tail of the `UbScore` object (score sheet shared by the result screens of modes
16, 19, 23, 27 / 30; starts in chunk 16). menu_q_b.c = head of `SimDay` (mode 22; work
`gSimDay` 0x3B7384, 0xBBC bytes; continues in chunk 18). (inferred) the family is "Ultimate
Battle" and modes 20..23 its "Sim Dragon": 20 top, 21 character select, 22 day screen, 23
result.

Verified by matching C:
- **Sim state** is a nested struct at `gProgress + 0x64C`: level 0..6; `stat[4]` = attack,
  defence, health %, points / 100 (names from clip names); `item[3]` ring of carried item
  ids; `turn` (+0x674; / 10 = round, % 10 = turn, turn 9 is the fight); `wait`; `off` (greyed
  board buttons).
- The day screen: board buttons run event scripts chosen by `Rand_Range(256)` (training row,
  or a weighted draw over 32 event rows); stat changes are queued and animated (attack /
  defence in steps of `Rand_Range(2) + 1`; health floored at 20, capped at 100; points give
  level-ups against a table); items are drawn with `Rand_Range(ownCount)`.
- **Battle hand-off (`SimDay_SetupBattle`, 0x37F978)**, tables from the mode's pack (section
  25 rounds, 26 enemies, 27 pools; 998 = random, 999 = none): announcer `Rand_Range(8)`, stage
  `Rand_Range(0x23)` when random (music "random" is passed on as id 0x18 unresolved);
  `Battle_ClearWork()`; `BattleSetup_SetRule(0, 2, bgm, rd->timeLimit, announcer, stage,
  flag)` (battle mode 2); opponent from the enemy record or `Rand_Range` over a pool; **the
  player's attack and defence are passed as item ids 0xC4 + attack and 0x110 + defence**,
  plus the carried items; `BattleSetup_SetSide(0, 0, 0, ...)` / `(1, 2, 1, ...)`;
  `BattleSetup_SetMember(0, 0, chara, costume, 0, 0, (f32)health%, items)` (the player starts
  at the board's health per cent); `BattleSetup_SetMember(1, 0, enemy, color, 0,
  en->cpuLevel, 100.0f, enemyItems)`; `BattleSetup_FinishEx(1)`. Draw order: announcer,
  stage, opponent. Back from the battle, health = `BattleResult->health[0]`, at least 1.
- Save records written by the score code: `gSaveData + 0x210` ranking of 10 {chara, score,
  cleared}; `+0x28C` `mission[100]` (0xC bytes: cleared, rank, h, m, s, total); `+0x73C` and
  `+0x780` two more best-record tables.

## Ladder and course modes (modes 24..30) (chunk 14, 0x372148..0x376920; src/menu/menu_n*.c; all 27 functions match)

Files: menu_n.c = tail of `UbTeamSel` (append to menu_m_b.c), menu_n_b.c = `UbzSel` (course
select, mode 28), menu_n_c.c = `UbRank` (ranking ladder, mode 26), menu_n_d.c = head of
`UbResult` (modes 27 / 30; prepend to menu_o.c). Mode 24 (chunk 15) sets `gProgress + 0x640`:
0 = ladder (25 -> 26 -> 27), 1 = courses (28 -> 29 -> 30). (inferred) these are the Disc
Fusion modes "Ultimate Battle" and "Ultimate Battle Z".

Verified by matching C:
- **Ladder hand-off (`UbRank_SetupBattle`, 0x373A68; battle mode 2)**: rule record (0x1C
  bytes: announcer, flag, timeLimit, stage, bgm, flag, foe index; 998 = draw: announcer
  `Rand_Range(8)`, stage `stageList[Rand_Range(28)]`, bgm `bgmList[Rand_Range(11)]`);
  `Battle_ClearWork()`; `BattleSetup_SetRule(0, 2, bgm, timeLimit, announcer, stage, flag)`;
  side 0 pad, side 1 CPU; player = `gProgress + 0x45C` chara / `+0x458` costume / `+0x460`
  items at 100 health; opponent from the 0x2C-byte table entry {chara, costume, cpuLevel,
  items (stored minus one; 999 = none)}; `BattleSetup_Finish()`.
- **Course hand-off (battle mode 3), split in two**: `UbzSel_SetupBattle` (0x372560) writes
  the rule, both sides, the first opponent and, per opponent (up to 8, ended by chara 999),
  `BattleSetup_SetPoolMember(count, i, chara, color, 0, cpuLevel, 100.0f, items)`; after the
  character select of mode 29, `Ub_SetupSolo2` (0x379A10, chunk 15) writes the player's member
  and `BattleSetup_Finish()`. Stage 998 -> `Rand_Range(35)`; music 998 is passed on as 0x18
  unresolved.
- Ladder screen: 100 places; only the place directly above the player (or any below) can be
  challenged; a win does `gSaveData->rank--` (`gSaveData + 0x77C`, 99 at start, 0 = first).
  **Intruder**: on confirm, `Rand_Range(0xFFFF) < 0x199A` (10 %) replaces the opponent with
  intruder `Rand_Range(37)` for places 10 and lower. (inferred) an intruder win is worth six
  places. `gProgress + 0x684` bits: 4 battle started, 8 upward challenge, 0x10 intruder;
  `+0x688` the place or course chosen.
- Save: `+0x780` five 12-byte course records {cleared, rank, time[3], score}; `+0x208` bit
  0x10 = all-courses reward given. Rewards: `UbScore_GetRewardItem(5)` for clearing all five
  courses, `(4)` for first place on the ladder.
- Random: `Rand_Range` for the draws above; libc `rand() % 32` seeds the blink timers.
- Original bugs: in a course battle side 1's member 0 gets costume 0 and no items while pool
  entry 0 (the same opponent) gets the table's; the course guide's line plays with a NULL
  subtitle table.

## Sim mode continued: board input, result screen, top screen head (chunk 18, 0x3840E0..0x388618; src/menu/menu_r*.c; all 18 functions match)

Files: menu_r.c = `SimDay` tail (appends to menu_q_b.c; carries stand-ins for six head
functions; `SimDay_PickEvent` must be declared with one unused parameter at the merge),
menu_r_b.c = `SimEvent_Run` (runs one of 37 event scripts from the `.data` table 0x3B7388),
menu_r_c.c = `SimResult` (mode 23, a whole object), menu_r_d.c = head of `SimTop` (mode 20;
continues in chunk 19).

Verified by matching C:
- The ladder is seven rounds of ten turns: nine turns of training / events, a fight on the
  tenth. `SimTop_Init` resets the run (level 0, attack 0, defence 0, health 100, points 0,
  no items, turn 0).
- `SimDay_Input` calls `SimDay_SetupBattle()` on the frame the fight turn's event script
  finishes, BEFORE the player confirms the versus picture: the battle setup's random draws
  happen then; one more `Rand_Range(3)` (closing line) is drawn at the confirm.
- Result screen: outcome from `UbScore_Fill(0, ...)` (0 won, 1 lost, 2 aborted); "continue"
  writes the total back to `gProgress + 0x65C` and returns to the board; otherwise the total
  goes into the ranking (`UbScore_AddRanking`) and into `gSaveData->money` (+0x3028, capped at
  9999999). `cleared` = turn >= 69.
- Random: `Rand_Range` (lines) and libc `rand()` (blink timer, two result lines).
- **Original bug**: on the result screen of the last fight (turn >= 69) the clear flag
  (`gSaveData + 0x288`) and the reward item are given with no test of the fight's outcome: a
  lost or aborted last fight still grants the item.
(from disassembly) mode 22 result 1 -> mode 23 and the battle; 0 -> mode 13; mode 23 result
0 -> turn += 1 and mode 22; 1 -> mode 20.

## `Ub_Main`: the modes 13..30 handler, Disc Fusion, mission select head (chunk 15, 0x376920..0x37AFF8; src/menu/menu_o*.c; all 26 functions match)

Files: menu_o.c = `UbResult` tail (behind menu_n_d.c), menu_o_b.c = `DiscFusion` (mode 24),
menu_o_c.c = `Ub_Main` + three hand-off functions, menu_o_d.c = head of `MisSel` (mode 14;
continues in chunk 16).

**Mode table of `Ub_Main` (0x379A58; VERIFIED by matching C; replaces the from-disassembly
sketches above)**; archive 3 = file baseFile + 3:

| Mode | Screen | Next |
|---|---|---|
| 13 | `UbMenu_Run(1)` | 0 -> mode 4; 1 -> 20; 2 -> 14; 3 -> 17; 4 -> 24 |
| 14 | `MisSel_Run(2)` | non-zero -> 15; 0 -> 13 |
| 15 | `UbTeamSel_Run(4)` if team size >= 2 else `SoloSel_Run(4)` | non-zero -> `Ub_SetupTeam()`, mode 16, battle; 0 -> 14 |
| 16 | `MisResult_Run(3)` | 14 |
| 17 | `SurvSel_Run(2)` | non-zero -> 18; 0 -> 13 |
| 18 | `SoloSel_Run(4)` | non-zero -> `Ub_SetupSolo()`, mode 19, battle; 0 -> 17 |
| 19 | `SurvResult_Run(3)` | 17 |
| 20 | `SimTop_Run(5)` | non-zero -> 21; 0 -> 13 |
| 21 | `SoloSel_Run(4)` | non-zero -> 22; 0 -> 20 |
| 22 | `SimDay_Run(0)` | non-zero -> mode 23, battle; 0 -> 13 |
| 23 | `SimResult_Run(3)` | non-zero -> 20; 0 -> 22, turn + 1 |
| 24 | `DiscFusion_Run(6)` | 0 -> 13; else `gProgress + 0x640` 0 -> 25, 1 -> 28 |
| 25 | `SoloSel_Run(4)` | non-zero -> 26; 0 -> 24 |
| 26 | `UbRank_Run(0)` | non-zero -> mode 27, battle; 0 -> 24 |
| 27 | `UbResult_Run(3)` | 26 |
| 28 | `UbzSel_Run(0)` | non-zero -> 29; 0 -> 24 |
| 29 | `SoloSel_Run(4)` | non-zero -> `Ub_SetupSolo2()`, mode 30, battle; 0 -> 28 |
| 30 | `UbResult_Run(3)` | 28 |

(inferred from labels) the family is the "Ultimate Battle" menu: 14..16 Mission 100
("mc_mission_plate", "fl_mission100_menu_cansel"), 17..19 Survival, 20..23 Sim Dragon,
24 Disc Fusion ("mc_disk_plate") with its ladder (25..27) and course (28..30) modes.

- **Mission battle hand-off (verified), in two halves**: `MisSel_SetupBattle` (0x37A060) when
  a mission is confirmed: `Battle_ClearWork()`; `BattleSetup_SetRule(0, 2, bgm, timeLimit,
  announcer, stage, flag)` from the 0x34-byte mission entry (0x3E6 = random: announcer
  `Rand_Range(8)`, stage `Rand_Range(35)`, music passed on as 0x18; 0x3E7 = none);
  `BattleSetup_SetSide(1, 2, 1, count, flag, 1, 0, NULL)` and up to five
  `BattleSetup_SetMember(1, i, chara, costume, 0, cpuLevel, 100.0f, items)` from the 0x2C-byte
  opponent entries; `gProgress + 0x644` = the player's team size (by mission kind),
  `+0x648` = DP rule. Then, after the character select, `Ub_SetupTeam` (0x379908):
  `BattleSetup_SetSide(0, 0, 0, teamSize, 1, 1, 0, NULL)`, one `BattleSetup_SetMember(0, i,
  ...)` per member (CPU level 0, health 100), `BattleSetup_Finish()`.
- Survival (18) and courses (29): the handler only adds the player's member and finishes; the
  rest is written by `SurvSel` / `UbzSel` (battle mode 3).
- **Disc Fusion (verified)**: a dialog state machine over `Disc_GetDriveState` and
  `Disc_Identify` (swap in an earlier game's disc, then this game's back); "recognised" bits
  are `gProgress + 0x684` bits 1 / 2, NOT saved (cleared with the session). A PC port has no
  disc drive: this gate needs a replacement.
- Result screen pay-out: `UbScore_Transfer` into `gSaveData->money`, capped at 9,999,999.
- Save: `+0x20C` mission pages shown; `+0x28C` 100 mission records of 12 bytes (rank, three
  separate time bytes, score). Here the save must be viewed FLAT (the nested `MSAVE` view
  breaks these accesses).
- Random: `Rand_Range` (announcer, stage, a guide line); libc `rand() % 32` seeds blink
  timers.
- Original oddities: `Ub_SetupSolo` and `Ub_SetupSolo2` are byte-identical; mission kinds 4,
  5, 8, 9 give team size 0.

## Survival (modes 17..19), sim top tail, first sim trainings (chunk 19, 0x388618..0x38CB38; src/menu/menu_s*.c; all 27 functions match)

Files: menu_s.c = `SimTop` tail (appends to menu_r_d.c; the merged file needs three
file-scope const tables at 0x3B9ED0 / 0x3B9ED8 / 0x3B9EE8 above its first function),
menu_s_b.c = `SurvSel` (mode 17), menu_s_c.c = `SurvResult` (mode 19), menu_s_d.c = sim
event handlers 0..2 (the three trainings; head of an object that continues in chunk 20).

Verified by matching C:
- **Survival battle hand-off (`SurvSel_SetupBattle`, 0x388EE0; battle mode 3)**: announcer
  998 -> `Rand_Range(8)`, stage 998 -> `Rand_Range(35)`, music 998 -> 0x18 unresolved;
  `Battle_ClearWork()`; `BattleSetup_SetRule(0, 3, bgm, timeLimit, announcer, stage, flag)`;
  side 0 pad with one member, side 1 CPU with a placeholder member (character 0); then FIFTY
  `BattleSetup_SetPoolMember(50, i, chara, costume, 0, cpuLevel, 100.0f, items)` from the
  course's `opp[50]` list (0xE0-byte course record; opponent character 998 ->
  `randomChara[Rand_Range(102)]`, drawn in index order). The player's member is added by
  `Ub_SetupSolo` after the character select, which also calls `BattleSetup_Finish()`. Every
  random draw happens in the menu at confirm time.
- Survival records: `gSaveData + 0x73C`, 12 bytes each {s32 defeated, s32 score, u8 rank, u8
  h, m, s}; `gProgress + 0x640` = the course; `gSaveData + 0x208` bit 8 = survival reward
  (item 0x6A, for 10 or more beaten) given. (inferred) `BattleResult + 0x18` is the number of
  opponents beaten in battle mode 3.
- Sim trainings (`gSimEvent[0..2]`): `Rand_Range(101)` against a weight row picks outcome
  0..5, then attack / defence changes drawn from min..max and a fixed health change.
  Original oddity: the draw is 0..100 inclusive against weights that presumably sum to 100,
  so outcome 5 has an extra 1-in-101 chance.
- The address range 0x31EA80..0x31EAA4 in the main executable's `.bss` is shared by several
  overlay modules' uninitialised globals (`gCharRefState` as declared by chunk 8 overlaps
  UbMenu's word, three sim outcome words and `gSimCardShown`): chunk 8's nine-pointer reading
  of it is RIGHT (its nine stores are verified by the identical image): the overlap is real, see "Link state".

## Mission select tail, mission result, family top menu, score sheet head (chunk 16, 0x37AFF8..0x37F430; src/menu/menu_p*.c; all 30 functions match)

Files: menu_p.c = `MisSel` tail (appends to menu_o_d.c), menu_p_b.c = `MisResult` (mode 16),
menu_p_c.c = `UbMenu` (mode 13), menu_p_d.c = head of `UbScore` (menu_q.c appends; both
define `gUbRewardItems`: keep one).

Verified by matching C:
- `UbMenu` (mode 13): four plates, guides Android 17 / 18; cursor in `gProgress + 0x638`;
  plate 3 (mode 17; inferred Survival) is closed until `gSaveData + 0x208` bit 0, which 30
  cleared missions set.
- `MisSel` (mode 14): 100 missions as pages of five; choice in `gProgress + 0x63C` (page) and
  `+0x640` (plate); it calls `MisSel_SetupBattle` after the fade-out.
- `MisResult` (mode 16): rewards by missions cleared: 30 -> `+0x208 |= 1`; 50 -> an item;
  100 -> an item and `+0x208 |= 4`. Money gained = the sheet total, capped at 9999999.
- **Score sheet `UbScore`** (0x2A8 bytes; shared by all result screens of the family):
  `UbScore_Fill(kind, ...)` reads `BattleResult_GetPtr()`: outcome 0 won (`winner & 1`), 1
  lost, 2 aborted; lines = remaining health, `result + 0x24` (capped at 100), `result + 0x1C`
  (capped at 99990), and for kind 2 `result + 0x18`; **one bonus per set bit 0..47 of
  `BattleResult.eventSummary`**; prices from the screen pack's section 13 (0x18-byte entries);
  rank 1..4 by total against {1599, 2199, 2799, 9999} (or {2099, 2499, 3199, 9999} for kinds
  2 and 4). So the simulation's outputs that the menus consume are: winner, abort flag,
  health, three counters, the event summary bits and the battle clock.
- `Snd_PlaySe` is non-void in the original (several pad handlers depend on it).

## Evolution Z (modes 48..50): customising screen second file, item help, shop head (chunk 22, 0x395E30..0x39A978; src/menu/menu_v*.c; all 28 functions match)

Files: menu_v.c = `EvoZ_Load`, last function of the customising screen's first source file
(0x392F10..0x396838, rest in chunk 21); menu_v_b.c = its second file (input, dialog, status,
item-set edits); menu_v_c.c = `ItemHelp` (the item details page used by every character
select and the wish screen: `ItemHelp_Init` 0x399240, `_Term` 0x399430, `_Draw(item)`
0x399478, `_Open` 0x399730, `_Close` 0x399760; replaces the `func_00399...` placeholders,
also in config/linker_script_extra.ld); menu_v_d.c = head of `Shop` (continues in chunk 23).

(from disassembly, handler `EvoZ_Main` 0x39E940 in chunk 23) mode 48 top menu -> 49
`EvoZ_Run(3)` (customising) or 50 `Shop_Run(2)`; archive 7 = file baseFile + 4; never starts
a battle.

Verified by matching C:
- **Customising screen save writes** (cell = col + row * 7 of the character grid):
  `custom[cell].set[set].id[slot] = item + 1` on equip; remove / remove-all zero ids;
  paying Z-points does `money -= zp`, `custom[cell].exp += zp`, then `custom[cell].level++`
  while `exp >= ChrTbl_GetExp`; opening an item's details clears its "new" bit
  (`item[i] &= ~2`). Each edit sets `gProgress->flags |= 1` (inferred: "save data changed",
  which triggers a save flow on leaving).
- **Equip rule (`EvoZ_CanEquip`)**: owned, `ItemTbl_CanEquip`, fits the character's slot
  capacity (`ChrTbl_GetLevel`), and no other slot holds an item with the same type and group.
- Item table entry additions: +1 group, +2 picture, +8 stockLevel; flags 1|4 hidden, 0x20
  sold by the shop, 0x40 listed, 0x100 not counted for the collection percentage.
- Shop: sells items with flag 0x20 whose `stockLevel <= gSaveData->unk100C` (the shop's stock
  level, 0..5); `Shop_CheckStockLevel` raises it when exactly ONE countable item is missing
  (an original off-by-one) and sets `unk1008 |= 3`. Guide = `Rand_Range(2)` (clip names:
  Launch and Oolong).
- `SaveCustom.unk30` is the character's experience.
- Here both save views are needed in one file (nested for most accesses, flat for three).

## Sim day events 3..30 (chunk 20, 0x38CB38..0x3911A8; src/menu/menu_t.c, menu_t_b.c; 31 of 32 match)

28 of the 37 day-event handlers (`gSimEvent[3..30]`); menu_t.c appends to menu_s_d.c.
`SimEv28` (the card game) was INCLUDE_ASM for a TOOLCHAIN reason, not a source one (resolved at the eleventh
step: src/menu/menu_t_b.c is assembled with -G8, `AS_G_FLAGS` in configure.py, and the function is C): its
compiler output is right, but the modern assembler under -G0 moves the `lw %lo(sym)(reg)` of
a `symbol(reg)` load into the `jal`'s delay slot where the original assembler kept a `nop`
(3 of 546 instructions). The same output assembled with -G8 is byte-identical. It is the only
such site in the overlay; the fix is a rule in include/gcc_prelude.inc or a per-file
exception (the file has no floats); the per-file exception was chosen.

Verified by matching C: every event is a `switch (day->seq)` script, one step per frame;
effects go through `SimDay_AddChange` (stats 0 attack, 1 defence, 2 health %, 3 points); the
per-event table is in the source header (e.g. event 4 health to 100; 13..17 deals for 10000
points; 23 / 24 force the next training's outcome; 27 attack and defence +15 for half the
health). The card game shuffles with ten libc `rand() % 10` swaps; every other amount is
`Rand_Range`. Only event 17 writes the save (`gSaveData + 0x20C` += 1, no upper bound).
Six card-game globals sit in the main executable's `.bss` (0x31EAA0..0x31EAF9): common
symbols of overlay source again.

## Evolution Z handler, shop tail, top menu head (chunk 23, 0x39A978..0x39EFC0; src/menu/menu_w*.c; all 14 functions match)

Files: menu_w.c = `Shop` tail (one source file with menu_v_d.c: merged test matches),
menu_w_b.c = `EvoMode_Main` (0x39E940), menu_w_c.c = head of `EvoTop` (mode 48; continues in
chunk 24; menu_x.c's stand-in for `EvoTop_Wrap` is not the matching form).

Verified by matching C:
- **Modes 48..50 = "Evolution Z"** (a development path in the data says
  `host:data/ps2/test/main/evoZ/EvoZ_TEX_PS2_.dbt`): 48 `EvoTop_Run(1)` (1 -> 49; 2 -> 50;
  0 -> mode 4), 49 `EvoZ_Run(3)` (customising), 50 `Shop_Run(2)`; archive 7 = file
  baseFile + 4; BGM 0x10B18; never touches the battle setup.
- Shop: levels 0 two plates (buy / collection), 1 buy list, 2 collection list, 3 item
  details, 4 confirmation, 5 "got it". Buying = `Save_AddItem(item)` then
  `money -= items[item].price` (item table +4 = s32 price); `gProgress->flags |= 1` = "the
  save changed in Evolution Z" (the top menu starts the save flow on leaving when set).
  `gSaveData->unk100C` = the shop's stock level; `unk1008` bit 0 = "stock grew, announce it",
  bit 1 = collection bonus given. Guides: Launch (two personalities, swapped after 3600 idle
  frames) and Oolong (clip names).
- Original slip: page-up in the buy list reads the collection list's top row.

## Sim day events 31..36 and the customising screen head (chunk 21, 0x3911A8..0x395E30; src/menu/menu_u*.c; 16 of 17 match)

`SimEv34` (the shell game, 0x392190) is INCLUDE_ASM: 15 of 551 instructions (registers and
store order in the block that starts the game); its description below is from the attempt.
Files: menu_u.c .. menu_u_f.c = one event each (`gSimEvent[31..36]`); menu_u_g.c / menu_u_h.c
= the customising screen's first source file pair (`EvoZ_Init / Term / Draw / Update / Run`
and `EvoZ_Refresh`; `EvoZ_Load` in menu_v.c ends that file).

Verified by matching C:
- Events: 31 yes / no offer (+1000 points or defence -5); 32 the rock mini game (training 1,
  `gSimTrain1`, attack: rocks cross at `speed * 3 + 3` units a frame, confirm swings, a hit
  needs the rock at x -412..-342); 33 attack and defence +10 for 30 % of health; 35 robbery
  (lose an item, or a tenth of the points, or defence -5); 36 +5000 points. Most scripts end
  with two `Rand_Range(3)` losses (attack, then defence).
- (from the attempt) 34 the shell mini game (training 2, `gSimTrain2`, defence): mark =
  `Rand_Range(5)`; per swap `Rand_Range(256) < 170` picks a random first figure, the second
  is redrawn until it differs from the first and from the mark.
- Mini-game state is uninitialised overlay globals in the main executable's `.bss`
  (0x31EAFC..0x31EB3C).
- Customising screen (`gEvoZ` 0x3BB140, 0x3AA8 bytes): `EvoZ_Update` converts Z points to
  experience at 33 per frame (`gSaveData->money` +0x3028 down, `custom[cell].exp` +0x1838 +
  cell * 0x38 up, `custom[cell].level` +0x183C up at each threshold: "ability limit up").
  Save facts: `custom[97]` at +0x1808 = {u16 set[3][8]; s32 exp; u16 level}; `item[350]` at
  +0x2EC8 (bit 0 owned, bit 1 new); money at +0x3028 (inferred: "Z points").
- Matching lesson: write every `uv.x0 / y0 / x1 / y1` group in field order and let the
  scheduler shuffle (540 -> 20 differing instructions in `EvoZ_Refresh`).

## Evolution Z top menu tail; Options (mode 62) (chunk 24, 0x39EFC0..0x3A3848; src/menu/menu_x*.c; all 14 functions match)

Files: menu_x.c = `EvoTop` tail (appends to menu_w_c.c; merged test matches all 16),
menu_x_b.c = `OptMode_Main` (0x39FAA8), menu_x_c.c = head of `Option` (`Option_Init`,
`Option_Run`, `Option_Input`; continues in chunk 25; `gOption` 0x3BC364, 0x22C bytes).

Verified by matching C:
- `EvoTop` (mode 48; guide Krillin by clip names): three plates; cancel returns 0 and, when
  `gProgress->flags & 1`, first runs the save flow (`DcSave_Start` -> `McFlow_Start(0)`;
  "DcSave" is a misnomer: a shared save-on-leaving helper at 0x3B0C08..0x3B0E04).
- **Mode 62 = Options** (main-menu item 9; archive 10 = file baseFile + 5; guide Dende; never
  starts a battle). Settings are written live into `gSaveData`; the card is written by the
  Save row (`McFlow_Start(0)`), Load is `McFlow_Start(1)`, and leaving when dirty runs
  `McFlow_Start(4)`.
- **Save option fields (the only place they are edited)**: `+0x1608` flags (bit 0
  `SAVE_FLAG_VOICE`; bits 1 / 2 and 3 / 4 per-controller toggles; inferred vibration and
  custom keys on); `+0x160C` `key[pad][0..7]` (what the battle reads: a copy of `keyEdit`
  when the per-pad bit `8 << pad` is on, else the default {2, 1, 0, 3, 4, 5, 6, 7});
  `+0x164C` `keyEdit[pad][0..3]` (a permutation of 0..3, swapped on change); `+0x1694` (0..2)
  and `+0x1698` (0 / 1), copied per side into the battle option block (meaning unknown);
  `+0x169C` / `+0x16A0` screen position (-16..16); `+0x16A4` sound mode (stereo / mono, then
  `SndOpt_Apply()`); `+0x16A8` / `+0x16AC` BGM / SE volume (0..9).
- Sound test: `Bgm_Play(id + 0x10B16)`, gated by `bgmBits`.
- Original bugs: both key pages compare the ADDRESSES of two tables instead of their
  contents, so they always mark the save dirty; the screen position counts as changed only
  if both axes changed.
For the port / netplay: the key assignment table is an input-mapping layer in front of the
pad, per controller; peers must apply their own mapping before inputs are exchanged.

## Data Center (modes 53..56) (chunk 26, 0x3A7D98..0x3AC440; src/menu/menu_z*.c; 62 of 68 match)

Wrapped up early (budget): NO relocated byte compare was run on this chunk, so strings and
call targets were checked by reading only (the link has since verified all of them; `DcList_Draw` matches
since the merge; `DcMenu_Init` lacked its twelve development path strings and two inline helpers were defined in
the wrong place for the string order: fixed at the link). Six INCLUDE_ASM with attempts (`DcList_Draw`,
`Dc_Main`, `DcMenu_DrawPlates`, `DcPass_WrapPos`, `DcPass_Decode`, `DcPass_DrawStatus`; notes
in the source). Files: menu_z.c = `DcList` tail (saved custom characters, mode 55; head in
chunk 25), menu_z_b.c = `Dc_Main` (0x3A9850), menu_z_c.c = `DcMenu` (mode 53), menu_z_d.c =
head of `DcPass` (password screen, mode 54; continues in chunk 27).

- (dev paths "host:data/ps2/test/main/DC/"; guide Bulma) Main-menu item 7 is the **Data
  Center**; archive 8 = file baseFile + 0xB.
- (from the `Dc_Main` attempt, flow identical to the original) mode 53 `DcMenu_Run(1)` (0 ->
  mode 4; else 54 / 55 / 56); 54 `DcPass_Run(3)`; 55 `DcList_Run(2)`; 56 `ReplayMenu_Run(4)`:
  0 -> 53; **1 -> leave the overlay: the replay starts**; other -> that mode.
- (verified) Save: `+0x1208` bit 0 = introduction heard; deleting a custom character sets
  `rec[n].chara = -1`; storing a decoded password writes the whole 0x1C-byte `rec` entry
  ({u16 item[8]; s32; u16 level (= the password's extraSlots); u16; s32 chara}).
- (verified, `DcPass_Decode` from the attempt) Password entry: 34 cells; 32 characters ->
  `OldPass_DecodeChar`, validity check, conversion; 34 -> `ChrPass_Decode`, validity check.
- Random: `Rand_Range` only (greeting lines, blink). Pad 0 only.

## Options tail and Data Center character list (chunk 25, 0x3A3848..0x3A7D98; src/menu/menu_y.c, menu_y_b.c; 42 of 43 match)

`Option_Draw` (0x3A3848, 2153 instructions) is INCLUDE_ASM: 14 instructions differ, register
choices only; menu_y.c was link-checked at the eleventh step (strings and call targets right). menu_y.c appends to
menu_x_c.c; menu_y_b.c is the head of `DcList` (continues in menu_z.c).
- (verified) `Option_UpdateReset` (the "reset?" dialogs): screen position and +0x1694 /
  +0x1698 to 0; sound mode 0, both volumes 9; controller: flags bits 8 / 0x10 cleared, bits
  2 / 4 set, `keyEdit` = {2, 1, 0, 3, 4, 5, 6, 7}.
- (verified) Data Center list: the 14 saved custom characters `gSaveData->rec[14]` (+0x2D40,
  0x1C each: u16 item[8], s32, u16 level, s32 chara, negative = empty); details menu: item
  list, password (the password builder 0x3AEA28), delete.

## Password screens, replay menu, save helper (chunk 27, 0x3AC440..0x3B0E04; src/menu/menu_za*.c; 75 of 76 match)

`DcPass_DrawRows` (0x3AC858) is INCLUDE_ASM (saved registers). Files: menu_za.c = `DcPass`
tail (behind menu_z_d.c), menu_za_b.c = `PassWin`, menu_za_c.c = `PassChk`, menu_za_d.c =
`ReplayMenu` (mode 56), menu_za_e.c = `DcSave`. Done at the eleventh step: `li.d` is in
include/gcc_prelude.inc (the project's first `double` constant, in `ReplayMenu_Draw`); the compiler's soft-float
calls `litodp` / `dptoli` are aliased to `__floatsidf` / `__fixdfsi` (config/linker_script_extra_dbzp.ld);
`gPassChk` is in the main executable's `.sbss` (a fixed-address symbol).

**How a replay is started from the menu** (ReplayMenu verified by matching C; `Dc_Main` from
its attempt; McFlow / `BattleReplay_Load` from linked main-executable source):
1. Mode 56: `Dc_Main` calls `ReplayMenu_Run(4)`. Init runs `McFlow_Init`, registers the
   per-mode callbacks (McFlow modes 5..9) and `Progress_ClearTeams()`; the first frame starts
   `McFlow_Start(7)`, the card scan, which fills `gProgress + 0x69C` (7 slots of 0x2C bytes:
   flags bit 0 = has a replay, `chara[2][5]`).
2. Confirm on a slot with a replay: `McFlow_SetSlot(cursor)`, `McFlow_Start(6)`.
3. McFlow mode 6 reads the 0x1AC00-byte file, checks its checksum, then `Battle_ClearWork()`
   and `BattleReplay_Load(buf + 0x38, 0x1ABA8)`: this copies the recorded 0x5A8-byte battle
   setup into the battle setup, copies the block into `gBattleReplay` and sets
   `active = loaded = 1`.
4. The done callback sets result 1; after a 30-frame fade `ReplayMenu_Run` returns 1,
   `Dc_Main` returns 1 with mode still 56, `Progress_Main` returns and `Game_Main` runs
   `Battle_Main(0)`.
**The menu writes nothing to the battle setup: the hand-off is entirely the loaded replay
block** (setup + recorded inputs). This is the path a headless simulation would be validated
through: load a 0x1ABA8-byte replay block, run the battle, compare.

**Where a replay is saved**: the mode-0 result menu's "save replay" item sets
`BattleResult_Set(8, 0x1000)`; `Progress_Main` then sets mode 56 and `gProgress + 0x68C |= 1`;
the same `ReplayMenu` opens in save mode (`McFlow_Init` snapshots `gBattleReplay`); confirm
on a slot: `McFlow_SetSlot`, `McFlow_Start(5)` writes the file to "BASLUS-21678DBZT3Rnn";
afterwards the mode becomes 39 or 40 (the character or team select reopens).

Other verified facts: `PassChk_IsValid` (character id <= 160; item ids < 350, no gaps, no
duplicates, no two of the same type and kind; a type 2 item only in the eighth place; slot
sums within the character's slots plus extra); old 32-character passwords are converted to
one of 6 x 3 preset item sets (the old items are not carried over); `PassWin_Open` calls
`ChrPass_Encode` (two libc `rand()` draws); `DcSave` = `McFlow_Start(0)` on leaving a mode
whose `gProgress->flags` bit 0 says the save changed.
