# Menus: how they work, and how to make one

How the game's menus are built, and how the PC port adds an entry to the main menu and a screen behind it (as it
does for Tenkaichi Budokai Online, src/menu/port_tbo.c). A reference for adding menus now, and for the mod
framework later. How each part was found, and what went wrong on the way, is in [README.md](README.md) (section
"Tenkaichi Budokai Online: lettered like the game's menus").

Throughout: the decomp's own files change only inside `#ifdef PORT` (the PS2 matching build must still pass);
nothing from the disc is committed (what is made from it exists in memory only); an online session runs the stock
game.

## 1. How the game's menus work

**Progress modes.** `gProgress->mode` says which screen runs. `Progress_Main` (src/menu/menu_a_b.c) calls that
screen's frame loop until it returns, then reads the next mode. The main menu is mode 4; the versus mode is 38
(its menu) and 39 (its character select). The full list is in `docs/systems/menu_overlay.md` (decomp).

**A screen is a Flash-like movie** from the menu archives (src/sys/gfxm_c.c, `Flash_*`):

- clips are found by name (`Flash_FindLabel`), shown or hidden (`Flash_ClipSetFlags(..., 2, on)`), moved,
  scaled, given another texture (`Flash_ClipSetTex`) or another part of it (`Flash_ClipSetUv`);
- `Flash_Advance` steps the movie (and shows its clips again: anything hidden is hidden after it, each frame);
  `Flash_Draw` draws it into the frame, in call order;
- a movie's shapes take their pictures from the screen's texture list (`flash->tex[i]`); a shape whose texture is
  missing (NULL) is not drawn.

**Words are pictures.** The plates' labels ("Dragon Net Battle", "Options") are painted into texture sheets on the
disc; the game has no font for them. Running text (descriptions, dialogue) is in the game's font
(src/battle/col_c.c, `Font_*`), written by a text box (src/battle/view_b.c, `TextBox_*`) hung on a clip as a
"draw over" callback.

**Two windows every menu shares:**

- the heading window (`IconWin`, src/battle/view_a_c.c): the cloud swirls, the emblem (texture slot 2), the
  heading's words (slot 4), the line under them;
- the message window (`MsgWin`, src/battle/view_a_b.c): the teal box with the meander corners, and the line about
  the item under the cursor, from a text file (`MsgWin_Draw(..., line)`).

**The main menu's plates** (src/menu/menu_a.c, `MainMenu_Draw`): each visible plate shows item *n*

- its label: row `n % 8` (32 pixels high) of label sheet `n / 8`. Sheet 0 holds items 0-7; sheet 1 (512 x 128)
  items 8-10 and one free row. There is a sheet per plate state: `gMainMenu->tex[8]` (as it is) and `tex[12]`
  (chosen); the second sheet is the texture entry after the first (`+ 0x40`). The sheets are 8-bit pictures with
  a 256-colour table, stored the way the GS keeps them (`PortMainMenu_PixelAt`);
- its icon: the 64 x 64 cell `(n % 4, n / 4)` of the icon sheet, and an animated one, `n % 10` of a 48-pixel strip;
- its line in the message window and the guides' voice: line `n * 4 + guide + 0x10` of the screen's text files.

## 2. Making a main-menu entry

All in src/menu/menu_a.c under `#ifdef PORT`, except the label (5).

1. **Its number and place.** `MAINMENU_ITEM_MAX` (include/menu/menu_a.h) counts it. `MainMenu_Init` builds the
   list in a loop over the items: the new item is skipped by the loop (`continue` on its number) and inserted after
   the item it should follow.
2. **Its icon.** `MM_ICON(n)` gives the item whose icon a plate shows: any existing item's.
3. **Its voice and line.** `MAINMENU_VOICED(n)` is false for it (there is no voiced line); `voiceLine` is then -1
   and the message window shows nothing. A line of its own can be written into the window with
   `MsgWin_PortDrawString` (section 3) when the cursor is on it (not done yet).
4. **What choosing it does.** A `case` in `MainMenu_Update`'s switch: `MAINMENU_COUNT(); MAINMENU_CHOOSE(mode);`.
   To open an existing mode, that mode's number is all. To open a screen of its own, see section 3.
5. **Its words.** The label is written into the free row of label sheet 1, in both states, put together from the
   disc's own letters so it matches the other plates:
   - `MenuLabels_MainItem` (port/src/gs/menu_labels.c) reads archives 448 and 449 from the player's game data
     (size and CRC checked), cuts the main menu's ten labels into letters with the cutter of port/src/gs/namefont.c,
     lends the letters they lack from the stage select's names (resized), and composes the text, `kMlItem`, in
     both states (512 x 32 RGBA). It runs in a thread started with the first frame (port/src/gs/menu_text.c).
   - `Port_MainItemLabel` (menu_text.c) turns each pixel into the nearest colour of the sheet's own table
     (`MenuLabels_ItemPixels`), and `PortMainMenu_WriteDisc` writes the values into the row.
   - For the Menu font setting (F1 > UI), which finds a sheet by a hash of all its pixels: the list of labels
     writes the item into the sheet the same way (`ml_item_into`), hashes that, and lists the item as the sheet's
     twelfth label. `MENU_LABELS_VERSION` (menu_labels.h) is raised whenever what the list writes changes.
   - Every letter has to be on the main menu's labels or the stage names; a missing one makes the composition
     fail (the log says so). Without the USA disc's archives a fixed picture is written instead
     (`port/include/port_tbo_label.h`, which says "Tenkaichi Budokai Online").

**Checks:**

- the item in both plate states (not chosen / under the cursor), listed once;
- with a Menu font chosen: the item and the three other labels on its sheet (Character Reference, Options,
  Shenron) in that font;
- the screen reader's log (`BT3_A11Y_LOG=<file>`), if the entry opens a screen of its own (section 3);
- the PC build's output free of `error:`; the PS2 matching build (`build/SLUS_216.78.ok`, `build/DBZP.ok`).

## 3. Making the screen behind it

**Reaching it.** A new movie cannot be made (nothing writes the movie format), and the game has no mode to spare,
so the port's screens have a progress mode of their own: **80** (`MPROG_MODE_PORT_SCREEN`,
include/menu/port_screen.h), handled in `Progress_Main` by `PortScreen_Main` (src/menu/port_screen.c). Which
screen it runs is `gPortScreen` (`PORT_SCREEN_*`). An entry sets it and chooses the mode:

```c
gPortScreen = PORT_SCREEN_TBO;
MAINMENU_CHOOSE(MPROG_MODE_PORT_SCREEN);
```

`PortScreen_Main` reads what the screens need (the versus mode's archive, `gMenuArc5`, for the game's windows),
runs the screen, and turns what it returns into the next mode (for Tenkaichi Budokai Online: back to the main
menu, or the Ultimate Training's fight of "Train While Waiting"). A new screen is a `PORT_SCREEN_` number and a
`case` there. No game mode is borrowed: an online session's character select is the versus mode's own.

**The frame loop** is the game's own kind (src/menu/port_tbo.c, `PortTbo_Run`):

```
Gfx_BeginFrame, Pad_Update, Snd_Update, ColorFade_Update
    the page's update (cursor, choices)          Tb_*Update
    what the page says to the screen reader      Tb_Speak
    drawing                                      Tb_Backdrop, IconWin_Draw, Tb_Draw, Tb_DrawText, prompts
ColorFade_Draw, Gfx_EndFrame, Dma_Flush
```

It fades in at the start, fades out when left, and returns a value for the caller.

**The game's windows.** Made from a menu's data as that menu makes them (the versus menu's: archive 448, member 1,
sections 21 to 24; `Tb_WinInit` / `Tb_WinTerm` in port_tbo.c):

- heading window: `IconWin_Init`, `IconWin_Open`, `IconWin_Draw` each frame. `IconWin_PortHideWords` empties the
  texture slots of the words and the emblem, so the screen's own title stands in their place;
- message window: `MsgWin_Init`, `MsgWin_Open`, and each frame `MsgWin_PortDrawString(str)`: a line of the
  screen's own, written by the game's text box in the game's font at the game's place. `str` is the game's kind of
  text: 16-bit, `0xFEFF` first, `\n` between lines, no `<` (it starts a tag). Lines are wrapped with the font's own
  widths (`Font_GetWidth`) up to the step of the box's top right corner (`MsgWin_PortLineRoom`);
  `MsgWin_PortSameTop` starts one and two lines at the same height. A changed line is handed over in another
  buffer (the screen reader tells lines apart by where they are).

**What the screen draws itself,** with the kit in src/menu/port_screen.c (`Ps_*`, declared in
include/menu/port_screen.h):

- pictures: PNG files kept in `port/tbo/ui/` (with CREDITS.md; `tbo/ui/` next to the program in a release, which a
  folder there also overrides), named in include/menu/tbo_art.h, drawn as sprites with the reserved format
  `TBO_PSM` (port/src/gs/gs_draw.c, `tbo_texture`) at their own resolution; their width : height comes from
  include/menu/tbo_sizes.h (`python3 port/tools/tbo_sizes.py` after a picture of another size);
- text: lines in the Menu font (port/src/gs/tbo_text.c), the title in the main menu's heading style
  (`MenuText_HeadingRender`);
- lines of letters (`Ps_Text` in five styles, `Ps_TextEx`, `Ps_Heading`, `Ps_Msg`; `Ps_Align`, `Ps_TextTop`);
- shapes: plain quads (`Ps_Rect`, `Ps_Quad`, `Ps_Disc`), the windows' frames from a picture stretched by its
  corners (`Ps_Panel`), bars (`Ps_Pill`), pictures standing or centred (`Ps_Stand`, `Ps_Icon`);
- the pad: up and down (`Ps_Move`), left and right (`Ps_Side`), the buttons' pictures and the prompts at the foot
  of the screen (`Ps_Prompt`, `Ps_PromptsDraw`, cleared each frame with `Ps_PromptsClear`).

Still in port_tbo.c, to move into the kit next: the keypads (`Tb_Keypad`, `Tb_PadUpdate`), the notes (`Tb_Note`),
the pages and their slide-in (`Tb_SetMode`, `Tb_In`), the speech (`Tb_Speak`), the windows (`Tb_WinInit`).

**Coordinates.** A page is 4:3 in the middle of the window (`Ps_PageSet` once a frame; `Ps_X`, `Ps_Y` take the
game's 512 x 448 units of the page; `Ps_Left` is the window's left edge on it). The game's movies, the windows included, cover the whole screen: where the screen's own things
meet theirs, convert between the two.

**Drawing order** is call order: what is drawn later covers what is drawn earlier (the message box covers the
hosts; it is see-through).

**Opening a page.** `Tb_SetMode` changes the page and starts its slide in from the left (`Tb_In`, given to the kit
with `Ps_SetShift`); the title, the player card and the box stay.

**The screen reader** (port/src/a11y_game.c, port/src/gs/a11y.c) reads the game's own screens from their state,
and cannot read the screen's own text (it is pictures). The line in the message window is read because it is the
game's text box. Everything else the screen says itself with `PortA11y_Say` (`Tb_Speak`): the title as a page
opens, the item under the cursor with its value, a keypad's key, what was typed, notes; what changed in one frame
as one line. `BT3_A11Y_LOG=<file>` writes what is said, without a speech library.

**An online match started from the screen.** The session puts the game back to its first moment and goes to the
character select; when it ends, the game is put back to the moment the match was asked for. The port's own
variables are not put back. So a screen that starts matches notes `Port_NetSessionsEnded()` (port/src/gs/net.c)
when it asks for a match; when the count has changed, the match is over: it goes to its first page. (The session's
character select is the versus mode's own, not the screen's mode, so it never opens the screen again.)

## 4. Limits, and what is possible

| | Now | For a mod framework |
|---|---|---|
| A new main-menu entry | one, in code | a manifest entry: its text, its place, its icon, what it opens; the label from the disc's letters, a font where letters are missing |
| More than one new entry | no: one free label row (item 12 would read past sheet 1) | a larger or third label sheet the plates are pointed at: not looked into yet |
| Its icon | any existing item's | the same, or a new one painted into the icon sheet |
| Its line in the message window | none yet | `MsgWin_PortDrawString` when the cursor is on it |
| Entries in other menus | not tried | the same parts (list, label sheet, choice, line) per menu; each menu's list and sheets mapped first |
| A screen of its own | in code, in the port's screen mode (80), with the kit of port_screen.c | screens described as data (rows, keypads, windows, prompts, speech) |
| The game's look | the heading window, the message box, the game's font in the box | any menu's windows (each menu's archive sections listed) |
| A list in the game's own plates | not done | an existing list movie reused, its labels made from disc letters |
| New movies | not possible | (nothing writes the format) |
| Online | a session runs the stock game | mods' entries and screens off in a session, as all mods |

A first step for mods: a "PC settings" entry opening a screen of rows, which needs only what is described here.
