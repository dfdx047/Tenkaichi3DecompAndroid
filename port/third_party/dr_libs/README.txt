dr_mp3.h: the MP3 decoder of dr_libs, by David Reid (based on minimp3). Used by port/src/gs/song_mp3.c.

Origin
  Repository  https://github.com/mackron/dr_libs
  Commit      51e61d308dde6b437fce0c5fabb32cd86b40f4d7 (2026-08-31, "dr_mp3: Fix a heap-buffer-overflow read when
              parsing Xing/Info tags.")
  File        dr_mp3.h, unchanged: its git blob id is a631ae40f066076e41f2f655ffd2891b41ca3772, the same as the
              file's at that commit (`git hash-object dr_mp3.h`; compared on 2026-10-13 with the id GitHub reports).
  Version     The file calls itself "v0.7.4 - TBD": it is NOT a release. It is the master branch after the last tagged
              one, mp3-0.7.3 (2026-01-17, commit 5690d4671d7ad07ae6021756d7222eb159745f06).

Why not the tagged release
  What the snapshot has over mp3-0.7.3 is, by its own revision history, an overflow fix and validation checks in the
  parsing of "Xing" / "Info" tags, and smaller stack use. The files read here are whatever a player puts in the songs
  folder, so the fixes are wanted. mp3-0.7.3 has the three functions song_mp3.c calls (drmp3_init_file,
  drmp3_read_pcm_frames_s16, drmp3_uninit) with the same signatures and could be put in its place; that was not
  tried. When 0.7.4 is tagged, take that file and update this note.

Licence
  A choice of public domain (Unlicense) or MIT No Attribution, as written at the end of dr_mp3.h. LICENSE in this
  folder is that text (the repository's own LICENSE file is the same but for the year, 2020). Releases carry it as
  licenses/dr_mp3.txt (port/tools/package.py).
