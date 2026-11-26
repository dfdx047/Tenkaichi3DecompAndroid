# The Mod Manager's art and data: where it comes from

Everything the Mod Manager draws or reads besides the code, and the scripts that make it. Run them from anywhere
(paths are relative to the repository); they need Python 3 with Pillow, NumPy and SciPy.

## Fonts

- **Tahoma Bold** (`C:/Windows/Fonts/tahomabd.ttf`, Windows' own font, not in the repository). It is the lettering
  of the game's own menu plates, and it is used three times:
  - the **letter sheet** in `gamedata/modmgr/ui.bin` (entry 2, ASCII 32..126, drawn by `modmgr_assets2.py`);
  - the **"Mod Manager" plate** on the main menu (`modmgr_label2.py`);
  - at run time, the screens' lines and headings (`port/src/gs/menu_text.c`, line kind 2: loaded from
    `%WINDIR%/Fonts/tahomabd.ttf`, condensed to 82 %; without it the Menu font of F1 is used).

## The scripts

| Script | Makes | From |
| --- | --- | --- |
| `mm_art.py` | `tbo/ui/mm_*.png` (the screens' pictures, drawn by the renderer like Tenkaichi Budokai Online's) and `include/menu/mm_sizes.h` (their sizes and the corners that keep their shape) | `art_src/`: `bg_sky.png` (the background), `bars_a.jpg`, `bars_b.jpg` (bars, tabs, windows), `icons_b.jpg` (emblem, platform, folders, keys, arrows, ball, lock, pips), `bg_b.jpg` (the dusk background). Google Flow pictures on a flat green screen: the green is keyed out and the pieces of a sheet are told apart by the green between them. `mm_art.py --list` writes a numbered preview of each sheet. |
| `modmgr_assets2.py` | `gamedata/modmgr/ui.bin` (a texture file of the game: 0 a backdrop, 1 the title, 2 the letter sheet, 3 the pieces - bars, panels, buttons, the platform) and `include/menu/port_modart.h` (where each thing is in it) | drawn from code, with Tahoma Bold; uses `modmgr_assets.py` (`entry`, `write`: the file format) and `gsmem.py` (how the GS keeps 8-bit textures) |
| `modmgr_label2.py` | `port/include/port_modmgr_label.h`: "Mod Manager" as runs of palette indices for a row of the main menu's label sheet | `gamedata/pzs3us1/00449.bin` (the main menu's archive, from the player's own game data) for the palette and the size and colour of "Dragon History"; uses `plate_label.py` (draws a plate label in Tahoma Bold), `modmgr_label.py` (reads the label sheets) and `bpe.py` (the archives' packing) |

## The design

`mockups/` holds the 16 approved 16:9 mockups the screens are laid out from (Google Flow). The positions in
`src/menu/port_mmkit.c` and the screens are in these pictures' pixels (1376 x 768).

## Data the Mod Manager reads (`gamedata/modmgr/`)

`gamedata_modmgr/` here is what goes into the game's `gamedata/modmgr/` next to `ui.bin`:

- `z3_paklist.txt` - the names of the files inside the game's packs (what a mod's file replaces);
- `moves.txt`, `items.txt`, `maps.txt`, `songs.txt` - the names the Mod Manager shows for moves, Z-items, stages and
  songs.

These five are plain text lists, kept as they are; no script makes them.

The manager also writes `gamedata/modmgr/cache/` itself (patched copies of fighters' files for fusion animations).
