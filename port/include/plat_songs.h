/* Songs added to the music select: a manifest under the data root (<BT3_DATA>/songs/songs.txt), parsed once at
   start-up. A track is a file in the songs folder (an ADX, or an mp3/wav/flac played by miniaudio); the port
   serves it for a file id and the menu lists it (the same scheme as the added stages). */
#ifndef PORT_PLAT_SONGS_H
#define PORT_PLAT_SONGS_H

/* The music list holds offsets into the BGM file range: entry value `n` plays file 0x10B16 + n. Offsets 0x00..0x13
   are the disc's 20 tracks, 0x18 is the game's "random" and 0x19 its "locked" marker; the disc's own raw list also
   carries 0x14..0x17, which the menu drops. BT4 uses native offsets up to 0xE2. Added tracks start at 0x100. */
#define PORT_SONG_FIRST_OFFSET 0x100
#define PORT_SONG_BGM_FIRST 0x816 /* Bgm_Play id of music-list offset 0 */
#define PORT_SONG_MAX 40            /* the menu's list (64 places of ours) less the disc's songs and "random" */
/* Synthetic file indices are beyond BT4's 30,000 native archive-3 entries.
   They are resolved only through the local songs manifest, never the save. */
#define PORT_SONG_FILE_INDEX 73000
/* BT4 routes global IDs above 0x10CC0 to archive 0 and above 0x20000
   to archive 3. Reserve aliases past archive 3's native entries. */
#define PORT_BGM_FILE(n) \
    ((n) >= PORT_SONG_FIRST_OFFSET && (n) < PORT_SONG_FIRST_OFFSET + PORT_SONG_MAX ? 0x20000 + PORT_SONG_FILE_INDEX + ((n) - PORT_SONG_FIRST_OFFSET) \
                                                                                    : PORT_SONG_BGM_FIRST + (n))

void PortSongs_Init(void);
void PortSongs_Refresh(void);       /* the select screens call it as they set up their music list: nothing to do now */
int PortSongs_Count(void);
const char *PortSongs_Name(int index);
int PortSongs_Offset(int index);    /* the music-list entry value of the index-th added track */

/* The game reads these (small data, a low address). */
extern char gPortSongNames[PORT_SONG_MAX][64];
extern int gPortSongCount;
extern int gPortSongOffsets[PORT_SONG_MAX];

/* 1 if the file the game asks for is an added song served from elsewhere; `out` gets its path. */
int PortSongs_Alias(const char *rel, char *out, unsigned n);
int PortSongs_IsFile(const char *path); /* the file of an added song */

#endif
