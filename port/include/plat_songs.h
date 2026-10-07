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
#define PORT_SONG_MAX 6             /* offsets 0x1A..0x1F: the save's music bits are one 32-bit word */

void PortSongs_Init(void);
int PortSongs_Count(void);
int PortSongs_Offset(int index);    /* the music-list entry value of the index-th added track */

/* The game reads these (small data, a low address). */
extern char gPortSongNames[PORT_SONG_MAX][64];
extern int gPortSongCount;
extern int gPortSongOffsets[PORT_SONG_MAX];

/* 1 if the file the game asks for is an added song served from elsewhere; `out` gets its path. */
int PortSongs_Alias(const char *rel, char *out, unsigned n);

#endif
