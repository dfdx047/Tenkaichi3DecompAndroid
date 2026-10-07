# Adding stages (maps) to the port

The port can load **stages that are not on the disc**: a map is a `.unk` file (the stage model BT3 modders
release, the same one that is replaced under PCSX2). The port gives it one more cell in the stage grid,
serves its model (and a sound bank) to the game, and draws its name in the menu.

Everything is driven by a **manifest** (`<data>/stages/maps.txt`) that the port reads at start-up. No code
change and no environment variable is needed.


## Where they go (main line)

A folder **`stages`** next to the game, like `textures`. Drop a `.unk` file in and it is used: its name in the
menu is the file's name (`My_Song.unk` is shown as "My Song"). `$BT3_STAGES` names another folder.

- The list (`maps.txt` in that folder, `file|Name In The Menu` per line) is optional: it gives other names and an
  order. Files it does not mention follow, by name without regard to case (the same order on every machine).
- The name strip (`names.rgba`, made by the script below) is optional too: without one for an entry the overlay
  writes the name as text in the same place.
- The folder of that name inside the game's data folder, which is where the pull request put them and what the
  rest of this document describes, is still used when there is none next to the game. The script now writes to
  the folder next to the game (the repository root) unless `--data` names another place.
- Code: `port/src/plat_extras.c` (the folder, the scan), `plat_stages.c`, `plat_songs.c`.
- Tried with a real custom map (a downloaded `.unk`, dropped into `stages`; the user, 2026-10-07): "worked fine".
  Until then only a copy of a disc stage under a new id had been loaded this way.

## File ids and size of an added stage (2026-10-07)

- **A bug of release 0.1.8, fixed.** An added stage's files were asked for under ids that belong to the disc's
  own files. Its sound bank at 0x14E + stage was redirected to the first stage's bank; but 0x14E + 0x24 is 0x172,
  the one-screen MODEL of disc stage 1, so with one stage added a one-player fight on disc stage 1 was given a
  sound bank as its model (the second added stage did the same to stage 2, and so on). And its model at
  0x171 + stage is, from the fourth added stage on, the split-screen model of a disc stage. Found by reading the
  code (the pull request had removed the bank redirection in a later commit of which only the renderer part was
  taken); not reproduced in a run of 0.1.8.
- Now: an added stage's model has a file id of its own, 30000 + n past the start of the second archive
  (`PORT_ADDED_STAGE_FILE` in battle_load.c, served by plat_stages.c), and for its sound bank the first stage's
  is asked for by its real id (`BTL_STAGE_BANK`). No redirection touches a disc file.
- Checked with a stage added, on one screen (`BT3_TEST_SCREEN=0`) and with `BT3_TEST_STAGE`: disc stage 1 opens
  its own model `00369.bin` and bank `00334.bin`; the added stage opens its file and bank `00333.bin`; all fight.
- **Size.** The game loads a battle's stage into one buffer of 0x6CB800 bytes (7.1 MB) and nothing checked a
  file against it: a larger file overran the buffer and the game crashed seconds later in its heap. Now the
  buffer is as large as the largest added stage (`Port_StageBufSize`, up to 64 MB); one larger than the game's
  own comes from the port's memory, not the game's 28 MB heap (`Port_GameBigAlloc`; a 10 MB buffer in the heap
  left too little for a battle's pools). A stage file that does not fit is refused with a message. With no
  larger stage installed, and in an online session, sizes and memory are the game's own.
- Tried with stage 12's split-screen file rebuilt at 2, 3 and 4 times its triangles (63,000 to 126,500; 8.2 to
  11.8 MB), put in its place through `mods/` with `BT3_STAGE_BUF`: all load and play the recorded fight with the
  same fight values as the disc file at each of 12,325 blanks; a frame's work goes from 7.3 to 9.3 ms.
- `BT3_TEST_STAGE=<id>` / `BT3_TEST_SCREEN=<mode>`: every battle on that stage / screen mode (testing).

## The name in the game's own lettering (main line)

The game has no font for these names: every stage name of the disc is a picture. `port/src/gs/namefont.c` reads
those pictures from the stage select's own pack when the menu is set up (36 stage names in `gCharSel->tex[39]`,
24 song names in `tex[19]` and again in the lit colours in `tex[20]`; 512 x 256 sheets, 8-bit with a table),
cuts them into letters in memory and writes an added name with them. Nothing made from the game's art is stored
or shipped; the letters exist while the program runs, made from the player's own disc.

- Fill letters are cut from the pictures; the outline and the shadow are drawn again around the composed name
  (neighbouring outlines run into each other on the disc). Spacing is solved from the disc names' own gaps.
- Characters the disc's names have: most letters, `' - !`. None of `J Q X Y Z j q x`, no digits, no other
  punctuation. A letter missing in one size is taken from the other and resampled. A name with a character
  that exists nowhere is written as plain styled text instead, whole.
- A strip made by the script (`names.rgba`) still comes first.
- The tables of what each picture says are in namefont.c; a disc whose pictures do not have those letter counts
  (another edition or language) leaves the names as plain text.
- `BT3_NAMEFONT_TEST="Stage Name|Song Name" BT3_NAMEFONT_OUT=<folder>` writes test pictures and the coverage.
- Checked: every disc name put together again from the kept letters against its own picture (mean difference
  under 1 of 128); an added stage's name seen in the menu under Vulkan and once under OpenGL. Not seen: an added
  song's name in the menu, in either of its two states (the lit one while the music list is open).

> **How this differs from the pull request it came from** (main line, 2026-10-07):
> - Nothing is written to the save: added stages are appended after the game has applied the save's unlocks to its
>   own list, and no unlock bit is set for them. A save stays valid for a copy of the game without them.
> - In an online session (Dragon Net Battle) added stages are not offered: both players must have the same list.
> - With nothing installed the game is unchanged.
> - The disc's own stages keep their lighter split-screen model; only an added stage (one model) is loaded as it
>   is for split screen too.
> - The styled name over the menu is drawn by the Vulkan renderer only; under OpenGL the game's plain font is used.

---

## Quick start (script)

From the repository root:

```sh
# install a map (the name is the one shown in the menu)
port/tools/add_stage.py add "/path/My Map.unk" "My Map"

# list the installed ones (with their id)
port/tools/add_stage.py list

# remove one
port/tools/add_stage.py remove "My Map"

# regenerate only the name strip (after a font/style change)
port/tools/add_stage.py rebuild
```

`add` copies the `.unk` into `gamedata/stages/`, adds the line to the manifest and **regenerates the name
strip** (`names.rgba`) so the name comes out in the game's style.

Options:

| option | effect |
|---|---|
| `--data <dir>` | data folder (default `gamedata`, or `$BT3_DATA`) |
| `--font <ttf>` | font for the name strip (default `$BT3_STAGE_FONT`, or `port/tools/compacta.ttf`, or a system one) |
| `--no-strip` | only edit the manifest; leave the strip alone |

### The font of the names

The game draws its stage names as **pre-rendered images** (it does not use a font with glyphs). So that the
added maps look the same, the name strip is **generated** from a TTF. Without Pillow or without a font the
manifest still works (the maps appear and are playable) but the name is drawn with the game's own plain font.

Put the TTF at `port/tools/compacta.ttf` (or pass it with `--font`) for the reference look. The style
(cream→orange fill, brown outline, shadow) lives in the constants of `port/tools/add_stage.py`.

---

## Manual use

1. Copy the `.unk` into `gamedata/stages/`.
2. Add a line to `gamedata/stages/maps.txt`:
   ```
   my_map.unk|Name In The Menu
   ```
3. Regenerate the strip: `port/tools/add_stage.py rebuild`.

Every **non-comment** line (`#`) adds a map; the order in the file is the order of the ids and of the strip's
images.

---

## How it works

### Stage ids

The game has 36 stages on the disc, ids `0x00`–`0x23`. The added ones take ids **`0x24`, `0x25`, …** (the
unlock field `stageBits` is 64 bits, so up to `0x3F`; the port uses 26 as the cap, `0x24`–`0x3D`). So that
those ids are selectable the port moves the grid's sentinels:

```
STGGRID_ID_LOCKED 0x24 -> 0x3E      STGGRID_ID_EMPTY 0x25 -> 0x3F   (PORT only)
```

and the grid becomes **6×N** (`stageRows = ceil(count/6)`), filling the last row with "empty" cells.

### Files the game asks for

For a stage of id `S` the game asks for (see `docs/systems/files_and_assets.md`):

| asset | id | where the added one comes from |
|---|---|---|
| model (single screen) | `0x171 + S` | **the map's `.unk`** |
| model (split screen) | `0x198 + S` | the *single* model (the port never uses the split) |
| sound bank | `0x14E + S` | the first stage's bank (borrowed) |
| menu thumbnail | `0x39D + S` | an existing thumbnail (high ids fall on the loading screens) |
| name | movie atlas | drawn by the port (see below) |

The port turns those requests into disc files through an **alias table** built by `port/src/plat_stages.c`
from the manifest; `port/src/plat_file.c` consults it in `open_rel` (and in `Port_FilePath`, the streamed
path).

**The id ranges are packed and overlap.** File ids are contiguous per asset type and the next range starts
right after the disc's 36, so:

* the added stages' *split* model (`0x198 + S`) falls on the **menu archives** (`baseFile = 0x1C1`) and the
  transitions (`0x1BF`, `0x1C0`). That is why the port **always loads the *single* model** in battle
  (`battle_load.c`, under `#ifdef PORT`): the *split* range is never asked for, so there is no collision. In
  split screen the full model is used.
* the thumbnail (`0x39D + S`, with `S >= 0x24`) falls on the loading screens (`LOAD_FILE_FIRST = 0x3C1`). So
  an existing thumbnail is reused (`menu_d.h`, `CHARSEL_STAGE_FILE_ID`).

### The name in the menu

The movie's name atlas only covers the disc's 36 stages. For the added ones the clip is hidden and the name
is drawn by the **port** (`ui.cpp`) as an **image** over the frame:

* `gamedata/stages/names.rgba`: a header `w`, `h`, `count` (u32) and then `count` images of `w×h` in RGBA,
  one per map in the manifest's order.
* The menu (`menu_c_e.c`) publishes each frame the clip's rectangle in game pixels (512×448) and which image;
  `gs_gpu.c` publishes the presented picture's rectangle (the letterbox, in window pixels); `ui.cpp` combines
  them and draws with Dear ImGui.

When the overlay is not available (no `names.rgba`, or the **OpenGL** back end), the menu falls back to
printing the name with the **game's own font** (the name is placed from `gPortStageNames`, in
`port/src/plat_stages.c`).

### Compatibility with the earlier prototype

For testing, the prototype's environment variables still work:

* `BT3_EXTRA_STAGES="0x24,0x25"` — used **only when there is no manifest**; adds ids without names.
* `BT3_FILE_ALIAS="pzs3us1/00404.bin=/path/kaio.unk;…"` — a manual alias by relative path.
* `BT3_STAGE_REPLACE="0x1b=0x24,…"` — replaces the id of an existing cell with another (without adding).

---

## Limits and notes

* **26 maps** at most (`0x24`–`0x3D`). Beyond that neither `stageBits` (64 bits) nor the sentinels allow it.
* The added stages' **sound bank** is the first stage's (borrowed). For their own, a free id would have to be
  given or an existing bank replaced.
* The model is loaded into the game's buffer (`BTL_STAGE_BUF_SIZE = 0x6CB800`, ~7.1 MB). The original models
  reach ~7.12 MB, so the mods' `.unk` files fit; a **larger** one would be truncated.
* The **thumbnail** (the grid's small preview) is a reused game image: it is not the map's. Generating it
  would need its own art.
* The name overlay runs on **Vulkan (SDL_GPU)**. On OpenGL the game font's text is used.

## Checking it

```sh
port/tools/add_stage.py list
BT3_STAGE_FONT=/path/to/compacta.ttf port/tools/add_stage.py rebuild
BT3_64=1 port/run.sh menu     # go to Duel -> Character/Stage Select
```

At start-up the port logs how many maps it read:

```
bt3: stages: 11 map(s) from gamedata/stages/maps.txt
bt3: stage-name overlay: 11 names, 1024x128 each
```
