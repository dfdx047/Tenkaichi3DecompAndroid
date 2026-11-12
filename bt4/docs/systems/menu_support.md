# Menu support code in the main executable (0x25C2A8..0x2600B0)

Source (linked): src/battle/view_a.c .. view_a_e.c, include/battle/view_a.h, names in
config/symbols/view_a.txt (all marked guess). 80 functions, 77 match; `Num_DrawEx`,
`ChrGrid_Build` and `BgmList_ApplyUnlocks` (view_a_e.c) are INCLUDE_ASM with attempts. None of
this is battle code: it is called almost entirely from the menu overlay DBZP.BIN. Suggested
final names: menu/get_win.c, msg_win.c, icon_win.c, chr_view.c, menu_util.c, text_box.c.
view_a_e.c is one object that continues past 0x2600B0: the TextBox head (written as view_a_f.c)
was merged into it at link time, because as two files the powers-of-ten table landed at 0x2F3248
instead of 0x2F3250 (the object's read-only data is 16-byte aligned by TextBox_Init's jump table).

| File | Range | Module |
|---|---|---|
| view_a.c | 0x25C2A8..0x25CFC0 | `GetWin`: reward window ("title_get", money digits, item lines) |
| view_a_b.c | ..0x25D290 | `MsgWin`: message window sliding in from left or right |
| view_a_c.c | ..0x25D468 | `IconWin`: window with an icon |
| view_a_d.c | ..0x25DE68 | `ChrView`: character model viewer |
| view_a_e.c | ..0x2600B0 | `Progress_*`, `FlashAnim_*`, `Num_*`, `ChrGrid_*`, `StgGrid_*`, `BgmList_*`; from 0x25FE00 `TextBox`: head of the text box module |

Verified by matching C:
- **Character viewer**: reuses the battle object, stage, scene and ordering-table modules
  outside a battle (`ChrView_Init / Update / Term`, own frame, not `Battle_Loop`).
  `BtlObj_Create` type 3 is a viewer model, played with model animation 7. Character model
  files are `0x590 + chara * 10 + costume`, +4 for the damaged model; file 0x197 is the
  backdrop (kept at `gCommonRes + 0x24`); common file 4 section 1 holds 0x3C-byte character
  entries (+0x30 viewer camera target height, +0x34 distance). Frame order: passes 1, 3, 2,
  0, 4, 0 through `Gfx_MarkPass`. `OrbitCam_Update(1)` reads pad 0.
- **`gProgress`** is 0x7FC bytes; `Progress_ClearSession` clears +0x30 (0x4C), +0x7C (0x3C4),
  +0x440 (0x1F4), +0x634 (0x58), +0x68C (0x144), +0x7D0, +0x7D4 (0x28). Flag 0x100 freezes
  the menu clip animations.
- **Character grid**: 7 columns, 0x24-byte cells {id, formCount, form[7]}; ids 0..0xA0
  character, 0xA1 random, 0xA2 locked, 0xA3 saved custom characters, 0xA4 filler (0xA1 and 0xA3 were
  swapped here and in view_a.h until the overlay's character select showed which is which). **Stage grid**: 6 columns,
  0x24 locked, 0x25 filler; `gSaveData->stageBits` bit = unlocked.
- **Random draws**: `FlashAnim_Blink` (`Rand_Range(12)`) and `FlashAnim_Talk`
  (`Rand_Range(3)`) draw from the shared Mersenne Twister whenever a menu portrait animates,
  so menu time before a match changes that generator's state (netplay: reseed at match start).
- Original quirks: `StgGrid_MoveRight` wraps at 7 in a 6-column grid; the three window `Term`
  functions use the pointer before the NULL test; `FlashAnim_ShowNext` and `ShowNext2` are
  identical.

Inferred (from the non-matching attempts or names): `ChrGrid_Build` filters by
`gSaveData->charaBits` and by `gProgress->mode` and lists the 14 saved custom characters;
`bgmBits` gates a 25-entry list; what `GetWin` kinds 0, 1, 7, 8, 9 announce; `TextBox` field
meanings.

Linked layout: each file defines its global pointer `= NULL` (`gGetWin` 0x2FF0D0, `gMsgWin`
0x2FF0D8, `gIconWin` 0x2FF0F0, `gChrView` 0x2FF108, `gProgress` 0x2FF10C), which with the short
label strings reproduces `.sdata` 0x2FF0D0..0x2FF110; `.rodata` 0x2F30E0 (view_a.c), 0x2F3218
(view_a_b.c), 0x2F3250 (view_a_e.c); `.lit4` 0x2FE7A4 (view_a_d.c). `Num_ToDigits` and
`Res_RelocateOffsets` return nothing; the declarations in sys/loading.c and the window files
were corrected.

## Text box tail, character / item tables, menu utilities, dragon scene (0x2600B0..0x263098; src/battle/view_b.c .. view_b_e.c, linked; names in config/symbols/view_b.txt)
Linked layout: view_b.c 0x2600B0, view_b_b.c 0x260D20, view_b_c.c 0x2614B0, view_b_d.c 0x261ED8 (it starts with
`ShenScene_StepSeq`, still assembly), view_b_e.c 0x262FF0. `.rodata` 0x2F3290 / 0x2F32D0 / 0x2F33D0 (b / b_c / b_d),
`.sdata` 0x2FF110 (b_c) and 0x2FF118 (b_d). view_b.c stays an object of its own (same image as appended to view_a_e.c).

68 functions, 66 match; `TextBox_DrawClip` (0x260158) and `ShenScene_StepSeq` (0x261ED8) are
INCLUDE_ASM with behaviourally exact attempts. Final names: view_b.c appends to view_a_e.c
(text_box.c); menu/chr_table.c; menu/menu_util.c; menu/shen_scene.c; view_b_e.c (20 empty
debug stubs) prepends to sys/debug.c. Layouts in include/battle/view_b.h.

Verified by matching C:
- **Common file 4** = header {?, charaOffset, itemOffset}; character entries 0x3C bytes
  (`ChrTblEntry`: +0 aiType, +8 flags, +0xA costumes, +0xC cost, +0xE baseLevel, +0x10
  exp[7], +0x2C link[4], +0x30 / +0x34 viewer camera) and item entries 0x28 bytes
  (`ItemTblEntry`: +0 type 1 slot item / 2 AI item, +3 slots, +0xC s16 stat[4], +0x14 flags,
  +0x18 s32 ability[4]). Field meanings are inferred from how the overlay uses them.
- **`ItemSet_GetStats(ids, stats, ability, chara)` (0x261130) is what `BtlMember_ApplyItems`
  calls**: four stat sums, OR of the ability bits, `stats[4]` = AI type (item id - 0x87 for a
  type 2 item, else the character's own). This is part of the battle set-up the simulation
  starts from.
- **`Demo_SetupBattle`** (0x2617F0): nine fixed pairings {chara, chara, stage}, picked with
  `Rand_Range(9)` until different from the last one, one more `Rand_Range(9)` for the music;
  `BattleSetup_SetRule(0, 7, bgm, 5, 4, stage, 0)`, both sides CPU level 13, one member each.
- Difficulty settings 0..4 map to CPU levels 0, 6, 13, 21, 29 (`CpuLevel_FromSetting`).
- Menu voice: `Voice_Play(0, base + line, 0x80, 0)`, + 0x55C when save flag bit 0 is set
  (second language); mouth movement is key data {u16 frame, u16 open} advanced once per call
  after the voice starts, frozen by progress flag 0x100.
- Text box: a line hangs on a movie clip through the clip's "draw over" callback; flags
  1 clip, 2 colour, 4 shadow colour, 8 max width, 0x10 max height, 0x20 per-row y offsets,
  0x40 spacing; default clip is the 512x448 screen. Text files: word[n+1] = offset of line n;
  16-bit characters, high byte first.
- `ShenScene_*` (0x261ED8..0x262FF0): the 3D backdrop of the dragon wish screen, a second
  user of the battle object / stage / scene modules outside a battle (backdrop file 0x194 +
  dragon; models 0xD3C / 0xD3D / 0xD45). Step contents come from the attempt.
- Original quirks: `TextBox_DrawClip` accumulates position and tint in place (a line must be
  re-attached before every draw); `ItemSet_GetStats` ORs the ability words four times.

## Dragon wish screen (0x2BD230..0x2BF6B0; src/sys/late_a.c, late_a_b.c, late_a_c.c, linked; names in config/symbols/late_a.txt)
Linked layout (built with -G0): late_a.c 0x2BD230, late_a_b.c 0x2BEB20, late_a_c.c 0x2BF370; `.rodata` 0x2FBDA8 (0x347)
and 0x2FC0F0 (0x18E); `.data` 0x2EB34C (late_a_b.c) and 0x2EB350 (late_a_c.c). The overlay symbols it uses
(`D_003B0EB4`, `func_00399240` ..) are defined in config/linker_script_extra.ld.

40 of 42 functions match; `Shen_DrawList` (38 of 229) and `Shen_BuildList` (4 of 121,
registers) are INCLUDE_ASM with attempts (not run through the differential interpreter).
**Built with -G0** like src/cri/ and the overlay (scripts/fdiff.py handles `src/sys/late_a*`;
configure.py's G_FLAGS needs the same for linking). This is game code linked after the SDK
libraries: the wish screen (`gProgress->mode` 70) and its save screen (mode 71), not
"memory-card menu UI" as older notes say. Final names menu/shenron.c, shenron_confirm.c,
shenron_save.c.

Verified by matching C:
- `Shen_Main` (0x2BD230) runs mode 70 (`Shen_Run`, loop at 0x2BEA48, `Gfx_EndFrame(2)`) then
  mode 71 (`ShenSave_Run`, loop at 0x2BF588, `Gfx_EndFrame(1)`, `McFlow_Start(0)` after the
  fade-in), then sets mode 4.
- Wishes: a circular list, 4 rows visible; kinds item (`Save_AddItem`), stage (`stageBits`),
  character (`charaBits`; character 0xA0 shows a second reward window), money
  (`Save_AddMoney`). `Shen_Init` clears `gSaveData->unlockFlags` bits 0..6 (the seven dragon
  balls). `GetWin_Setup` kinds seen here: 0 character, 1 stage, 2 item, 6 money.
- Pack sections: messages, wish file (`ShenWishFile` 0x78: lists of 4, 7 and 4 eight-byte
  entries), movie, textures, confirmation pack, item table, IconWin, MsgWin, Dialog, GetWin,
  row text. Debug strings in the read-only data give original file names
  (`shenron_list_PS2_.dat`, `zitem_parameter_PS2_.dat`, ...).
- Random draws: `Rand_Range` only (greeting line, grant line).

From the non-matching `Shen_BuildList` attempt: one `Rand_Range(100)` picks the dragon: with
`slot[8].flags & 1`, r < 40 dragon 0, 40..59 dragon 1, else dragon 2; without it, r < 50
dragon 0, else dragon 1. Dragon 1 lists 7 wishes and grants 3; the others list 4 and grant 1.
(inferred) dragon 1 is Porunga.
