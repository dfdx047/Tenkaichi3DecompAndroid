# Adding songs to the music select

The port can **add tracks** to the music ("BGM") list shown in the menu. A song is an audio file (mp3, wav,
ogg, flac…): the port serves it as the ADX the game plays and gives it its own entry in the list, with its
name.

Everything is driven by a **manifest** (`<data>/songs/songs.txt`) that the port reads at start-up. No code
change and no environment variable is needed.


## Where they go (main line)

A folder **`songs`** next to the game, like `textures`. Drop a `.adx` file in and it is used: its name in the
menu is the file's name (`My_Song.adx` is shown as "My Song"). `$BT3_SONGS` names another folder.

- The list (`songs.txt` in that folder, `file|Name In The Menu` per line) is optional: it gives other names and an
  order. Files it does not mention follow, by name without regard to case (the same order on every machine).
- The name strip (`names.rgba`, made by the script below) is optional too: without one for an entry the overlay
  writes the name as text in the same place.
- The folder of that name inside the game's data folder, which is where the pull request put them and what the
  rest of this document describes, is still used when there is none next to the game. The script now writes to
  the folder next to the game (the repository root) unless `--data` names another place.
- Code: `port/src/plat_extras.c` (the folder, the scan), `plat_stages.c`, `plat_songs.c`.

> **How this differs from the pull request it came from** (main line, 2026-10-07):
> - Nothing is written to the save: added songs are appended after the game has applied the save's unlocks to its
>   own list, and no unlock bit is set for them. A save stays valid for a copy of the game without them.
> - In an online session (Dragon Net Battle) added songs are not offered: both players must have the same list.
> - With nothing installed the game is unchanged.
> - The styled name over the menu is drawn by the Vulkan renderer only; under OpenGL the game's plain font is used.

---

## Quick start (script)

From the repository root:

```sh
# install a song (the name is the one shown in the menu)
port/tools/add_song.py add "/path/Song.mp3" "Song Name"

# list the installed ones (with their list offset)
port/tools/add_song.py list

# remove one
port/tools/add_song.py remove "Song Name"

# regenerate only the name strip (after a font/style change)
port/tools/add_song.py rebuild
```

`add` **converts the audio to ADX** with `ffmpeg` (24000 Hz, stereo — like the game's own tracks), copies it
into `gamedata/songs/`, adds the line to the manifest and **regenerates the name strip** (`names.rgba`).

| option | effect |
|---|---|
| `--data <dir>` | data folder (default `gamedata`, or `$BT3_DATA`) |
| `--font <ttf>` | font for the name strip (default `$BT3_STAGE_FONT`, or `port/tools/compacta.ttf`, or a system one) |
| `--ffmpeg <path>` | the ffmpeg binary (default `ffmpeg`) |
| `--rate <hz>` | the ADX sample rate (default 24000) |
| `--no-strip` | only edit the manifest; leave the strip alone |

### The font of the names

The songs' names are **pre-rendered images** in the game (like the stages), so the name strip is **generated**
from a TTF. Without Pillow or without a font the manifest still works (the songs appear and play) but the name
is drawn with the game's own plain font.

Put the TTF at `port/tools/compacta.ttf` (or pass it with `--font`). The style (cream→orange fill, brown
outline, shadow, left-aligned) lives in the constants of `port/tools/add_song.py`.

---

## Manual use

1. Convert the audio to ADX: `ffmpeg -i Song.mp3 -ar 24000 -ac 2 -c:a adpcm_adx my_song.adx`.
2. Copy the `.adx` into `gamedata/songs/`.
3. Add a line to `gamedata/songs/songs.txt`:
   ```
   my_song.adx|Name In The Menu
   ```
4. Regenerate the strip: `port/tools/add_song.py rebuild`.

---

## How it works

### The music list

The menu keeps the music as **offsets** into a range of BGM files:

```
BGM id = 0x10B16 + offset      ->      pzs3us2/<64974 + offset>.bin
```

- Offsets `0x00..0x13` (20) = the disc's tracks (0x10B16..0x10B29).
- `0x14..0x17` exist in the raw list but the menu drops them (a gap).
- `0x18` = **Random** (a marker), `0x19` = **Locked** (a marker).
- Offsets `0x1A` onward = **free** (their files, `0x10B30+`, are 30 KB stubs).

The added songs take `0x1A`, `0x1B`, … The port:

1. **Aliases** the file `pzs3us2/<64974 + 0x1A + i>.bin` → `songs/<file>` (`plat_songs.c`; `plat_file.c`
   consults it both for the normal load and for the **streamed** ADX).
2. **Extends the list**: copies `bgmIds` into a buffer, inserts the new offsets **before** Random and raises
   `bgmCount` (in `menu_c_e.c`, `#ifdef PORT` only). It sets their bits in `gSaveData->bgmBits` (a u32, so
   offsets `0x1A..0x1F` fit).
3. **Draws the name**: the music strip is fixed; the port draws it as an image (`gamedata/songs/names.rgba`)
   over the frame (the overlay, on the **Vulkan/SDL_GPU** back end). Without the overlay (e.g. **OpenGL**)
   the name is printed with the **game's font**.

### The name (overlay)

`names.rgba`: a header `w`, `h`, `count` (u32) and then `count` images of `w×h` in RGBA, one per song in the
manifest's order. The port draws them at the **clip's real position** (`mc_bgm_now`, parent + child of the
game), mapped through the letterbox.

## Known issue: the name's position

In the music select the game shows the name in **two** different places:

- over the **reel** (the entry being scrolled; the "selection"),
- in the **bottom bar** (the track already chosen; the "selected").

It is the **same clip** (`mc_bgm_now`), moved by each screen/state, so the port follows it and draws the name
where the game would. The **selected** one (the bar) lands in place; the **selection** one (over the reel) can
sit **a bit high** compared with the game (the clip is animated and its position is not final). Refining it is
still pending. The same mechanism serves the **Map Select**, where the name goes in the bar like the stages.

---

## Replacing a disc track

That already works through the mods mechanism: just put the ADX at `gamedata/mods/pzs3us2/<index>.bin` (e.g.
`64974.bin` is track 0). Nothing from the port is needed.

## Limits and notes

- **6 added songs** at most (offsets `0x1A..0x1F`; `bgmBits` is a u32).
- The audio is converted to **ADX** (ffmpeg `adpcm_adx`). The game plays it with its ADX decoder; other
  formats make no sound.
- The music list is a **reel**: the added ones appear before Random.
- The overlay runs on **Vulkan (SDL_GPU)**. On OpenGL the game font's text is used.

## Checking it

```sh
port/tools/add_song.py list
BT3_STAGE_FONT=/path/to/compacta.ttf port/tools/add_song.py rebuild
BT3_64=1 port/run.sh menu     # Duel -> Character/Stage Select -> BGM Select
```

At start-up the port logs how many songs it read:

```
bt3: songs: 1 added track(s) from gamedata/songs/songs.txt
bt3: song-name overlay: 1 names, 1024x64 each
```
