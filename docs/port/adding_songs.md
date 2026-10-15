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
- **An `.mp3` works too** (2026-10-12, pull request 70; changed 2026-10-13, below): at start-up every `name.mp3`
  that needs it is converted once and written as `name.adx` next to it; the next start finds the ADX and does
  nothing. The MP3's own rate and channels are kept. A file that cannot be read or written (not an MP3, more than
  two channels, a folder that cannot be written to) is left out: one line in the log and a notice over the picture
  ("Song x.mp3 could not be converted and is left out."), and the game goes on. `songs.txt` may name the `.mp3`;
  its `.adx` is used. Code: `port/src/gs/song_mp3.c` (dr_mp3 from `port/third_party/dr_libs`, public domain /
  MIT-0, see the README.txt there for the exact origin, and an ADX encoder written against the decoder in
  `snd_adx.c`).
  Checked with the macOS build (branch macos-port, where it was written): a 6 s and an 8 s MP3 converted (decoded
  with the port's own rounding, 21 dB and 55 dB against the MP3's samples; ffmpeg's `adpcm_adx` files of the same
  MP3s give 16.5 dB and 30 dB there, why was not looked into), not converted again at the next start; a file of
  junk named `.mp3` left out with the log line; the demo fight ran to its end. The release container compiles it
  for Linux (`clang64`, linked) and Windows (`win64`, the file compiled). Not checked: the notice on screen, the
  songs played in a fight, the program run on Linux or Windows.
- **How the conversion decides and when it runs** (2026-10-13, the top of `song_mp3.c` has the whole of it):
  - The ADX written carries a mark in its header (0x60 bytes; at 0x30 `BT3:MP3\x01`, then the MP3's size and its
    time of last change, 8 bytes each). No `name.adx`: converted. Mark with the MP3's size and time: nothing. Mark
    with another size or time: converted again (a replaced MP3, whatever its date). **No mark: the ADX is somebody
    else's (made by hand, by ffmpeg, by `add_song.py`) and is never written over**; the MP3 is left alone, the log
    says so. An ADX written by the pull request before this change has no mark and counts as made by hand: delete
    it to have it made again.
  - An MP3 that cannot be decoded is written down in `mp3-failed.txt` in the folder (size, time, name, reason) and
    not tried or reported again until the file changes or its line is deleted. A failure to write (the folder, the
    disk) is not written down and is tried at every start.
  - It runs in a thread of its own; the program does not wait. The song list is made at start from the ADX files
    there are; a song converted later joins the END of the list when a character select is next set up
    (`PortSongs_Refresh`, called from `menu_c_e.c` and `menu_e_b.c`), so its place can differ from the next start's
    (there the list's order is `songs.txt`, then by name). Not in an online session. With `BT3_PAD_REC`,
    `BT3_PAD_PLAY`, `BT3_PAD_TABLE` or `BT3_SYNCTEST` the conversion is done at start before the list is made, so
    that such a run's list depends on the folder only. `BT3_TEST_SONGS_SLOW=<ms>` makes every conversion wait
    first (testing).
  - The MP3 is decoded and written 8192 samples at a time: about 14 MB for the whole program in a test whatever the
    length (an hour of music took 622 MB decoded whole).
  - Names: `.mp3` in any case of the letters; `a.b.mp3` gives `a.b.adx`; names of 128 bytes and more are left out
    (the list does not take them either); of two MP3 files that give the same ADX name the first by name is
    converted and the other left out, the same one at every start. On Windows a name with characters outside the
    system's code page cannot be opened by this program (files are opened by the code page's names everywhere in
    the port) and is left out with a line in the log; names inside the code page (accents on a western system) work.
  - Deleting an MP3 does not delete its ADX: the song stays until the ADX is deleted too.

## The name in the game's own lettering (main line)

The game has no font for these names: every song name of the disc is a picture. `port/src/gs/namefont.c` reads
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
> - Nothing is written to the save: added songs are appended after the game has applied the save's unlocks to its
>   own list, and no unlock bit is set for them. A save stays valid for a copy of the game without them.
> - In an online session (Dragon Net Battle) added songs are not offered: both players must have the same list.
> - With nothing installed the game is unchanged.
> - The styled name over the menu is drawn by the Vulkan renderer only; under OpenGL the game's plain font is used.

---

## Team battles, and playing round (2026-10-10)

Two reports, both true:

- **Added songs were missing in team and DP battles.** The team select (src/menu/menu_e_b.c) builds its own music
  list, and only the duel's select (menu_c_e.c) had been given the added songs. It has them now, the same way:
  appended after the disc's and before "random", their own file numbers for the preview (`PORT_BGM_FILE`), their
  names drawn by the port or, without the overlay, in the game's plain font.
- **An added song played once and then the fight was silent.** The port plays a track round only if the ADX file
  has loop points (the disc's music has them: flag at 0x24, start and end samples at 0x28 and 0x30). A file made
  from an mp3 has none. Such a file is now played again from its start, when it is the file of an added song
  (`PortSongs_IsFile`, port/src/gs/snd_adx.c `start`); the game is told it never ends, as for the disc's music. A
  file that has loop points of its own keeps them.

Checked: a 2.7-second file without loop points, installed as a song and forced as a fight's music
(`BT3_TEST_BGM=26`, a dummy sound device): it is started once as a looping track and not again in a minute's
fight; the replay check is unchanged. NOT seen: the team select's music list with added songs in it (no recorded
session reaches that screen).

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

- **40 added songs** at most (see the end of this file; it was 6).
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

## The added songs' file numbers were disc files; 40 songs (2026-10-08)

An added song was played as file 0x10B16 + its list entry, entries 0x1A..0x1F. Those are disc files: 0x10B30 to
0x10B33 are silent files of 30,720 bytes, but 0x10B34 and 0x10B35 are the first two lines of the announcer
(`HudNotice_GetVoiceBase`, the voice set used when `SAVE_FLAG_VOICE` is clear). With five or six songs added,
those two lines were answered with the songs (releases 0.1.8 to 0.1.11).

- Now an added song's file is 0xD48 + 73000 + n, past the end of the second archive (`PORT_BGM_FILE` in
  port/include/plat_songs.h; used where the fight and the duel's music select play an entry).
- The limit was 6 "because the save's music bits are one 32-bit word". An added song is never looked up there
  (the unlocks are applied to the disc's list before the added ones are appended), and the entry is an s32
  everywhere it is kept. The limit is 40: the list the menu is given has 72 places.
- `BT3_TEST_BGM=<entry>`: every battle plays that entry (0x1A is the first added song).
- Checked with 45 files in the folder: 40 are taken (the rest named in the log), the music select's list has 61
  entries with "random" last, and a fight set to entries 0x1A, 0x1E, 0x1F and 0x41 opens the 1st, 5th, 6th and 40th
  file; a disc entry opens none of them. Not checked: the forty names on screen, and scrolling the list by hand.
