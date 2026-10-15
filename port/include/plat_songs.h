/* Songs added to the music select: a manifest under the data root (<BT3_DATA>/songs/songs.txt), parsed once at
   start-up. Every line `<file>|<display name>` adds one track as the next free music-list entry; the port serves
   its ADX for a file id and the menu lists it (the same scheme as the added stages). */
#ifndef PORT_PLAT_SONGS_H
#define PORT_PLAT_SONGS_H

/* The music list holds offsets into the BGM file range: entry value `n` plays file 0x10B16 + n. Offsets 0x00..0x13
   are the disc's 20 tracks, 0x18 is the game's "random" and 0x19 its "locked" marker; the disc's own raw list also
   carries 0x14..0x17, which the menu drops. Added tracks take 0x1A onward, so they never touch a disc entry. */
#define PORT_SONG_FIRST_OFFSET 0x1A
#define PORT_SONG_BGM_FIRST 0x10B16 /* Bgm_Play id of music-list offset 0 */
#define PORT_SONG_MAX 40            /* the menu's list (64 places of ours) less the disc's songs and "random" */
/* An added song's file. NOT 0x10B16 + its entry: those ids are disc files (0x10B30..0x10B33 are silent, 0x10B34
   and 0x10B35 are the first two lines of the announcer), and the fifth and sixth added song stood in their place
   (releases 0.1.8 to 0.1.11, found 2026-10-08). Past the end of the second archive (65,201 files) instead. The
   limit was 6 for the save's 32 music bits, which an added song is never looked up in. */
#define PORT_SONG_FILE_INDEX 73000
#define PORT_BGM_FILE(n) \
    ((n) >= PORT_SONG_FIRST_OFFSET && (n) < PORT_SONG_FIRST_OFFSET + PORT_SONG_MAX ? 0xD48 + PORT_SONG_FILE_INDEX + ((n) - PORT_SONG_FIRST_OFFSET) \
                                                                                    : PORT_SONG_BGM_FIRST + (n))

void PortSongs_Init(void);
void PortSongs_Refresh(void);       /* before a select screen's music list is made: songs converted from MP3 since */
int PortSongs_Count(void);
int PortSongs_Offset(int index);    /* the music-list entry value of the index-th added track */

/* The game reads these (small data, a low address). */
extern char gPortSongNames[PORT_SONG_MAX][64];
extern int gPortSongCount;
extern int gPortSongOffsets[PORT_SONG_MAX];

/* 1 if the file the game asks for is an added song served from elsewhere; `out` gets its path. */
int PortSongs_Alias(const char *rel, char *out, unsigned n);
int PortSongs_IsFile(const char *path); /* the file of an added song */

#endif
