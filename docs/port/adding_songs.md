# Custom music

Create a `songs` folder next to the game executable and add `.mp3`, `.wav`,
`.flac` or `.adx` files. Restart the game to refresh the music list.
`BT3_SONGS` can select another folder.

## Names and ordering

File names become menu names, with underscores replaced by spaces. An optional
`songs.txt` manifest sets display names and ordering:

```text
track.mp3|Track Name
another-track.adx|Another Track
```

Unlisted files follow in alphabetical order. The runtime supports up to 40 added
songs. Native audio formats use miniaudio; ADX uses the game's decoder.

## Import tool

From the repository root:

```sh
python port/tools/add_song.py add "/path/Track.mp3" "Track Name"
python port/tools/add_song.py list
python port/tools/add_song.py remove "Track Name"
python port/tools/add_song.py rebuild
```

The import tool can convert audio to ADX using FFmpeg and generate a menu-name
strip. Directly adding supported files to the `songs` folder does not require conversion.

| Option | Purpose |
| --- | --- |
| `--data <dir>` | Select the data directory |
| `--font <ttf>` | Select the font for generated name strips |
| `--ffmpeg <path>` | Select the FFmpeg executable |
| `--rate <hz>` | Set ADX output sample rate; default 24000 |
| `--no-strip` | Update the manifest without regenerating name strips |

## Playback and menu labels

Added tracks loop during battles. Native streams receive a default gain of 1.5;
`BT3_MA_GAIN` selects another multiplier. High gain can clip loud source files.

Menu labels use generated strips, the game's lettering where available, or
plain text fallback. Added resources are subject to the runtime's online
compatibility rules and may be unavailable during network sessions.

Custom tracks use reserved BT4 file IDs, independently of the installed Spanish
or English data. With **F1 > Audio > Music follows game speed** disabled,
music retains its original tempo at 114%, including ADX tracks without loop
metadata. The F1 language setting does not change audio timing.
