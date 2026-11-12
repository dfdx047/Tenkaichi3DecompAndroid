/* Network family and simulation ABI. Independent of installer/menu revision and language.
 * The numeric ID is fixed for this gameplay ABI.
 * Change it when simulation rules or required data change, not for presentation edits.
 */
#ifndef BT4_ONLINE_COMPAT_H
#define BT4_ONLINE_COMPAT_H
#define BT4_ONLINE_COMPAT_NAME "BT4-B14-Rev2-sim6"
#define BT4_ONLINE_COMPAT_ID 0xb33e7efcb2c66312ULL
typedef struct Bt4BaseFile {const char *path; unsigned size[2],crc[2];} Bt4BaseFile;
static const Bt4BaseFile kBt4BaseFiles[]={
{"disc/SLUS_219.78",{2115688,2115696},{0x1456d540u,0x9d2224c1u}},
{"disc/BIN/DBZ4.BIN",{234512,234512},{0xbf6ee25du,0x8a7a0f28u}},
{"disc/BIN/DBZP.BIN",{570836,570836},{0xbda4f48eu,0xe583bbb4u}},
};
#endif
