#ifndef BT4_TEXTURE_PACK_H
#define BT4_TEXTURE_PACK_H
/* Original ZIP supplied for R3. Renaming/splitting the download does not alter
   any DDS payload. The renderer opens the archive and loads images on demand. */
#define BT4_TEXTURE_ARCHIVE "BT4-HD-Light-v1.0.zip"
#define BT4_TEXTURE_ROOT "[GrandpaNiki x vini2227 DDS] BT4 PS2 HD Textures v1.0 - Light"
#define BT4_TEXTURE_FILES 48739u
#define BT4_TEXTURE_UNPACKED_BYTES 19814081152ULL
#define BT4_TEXTURE_ARCHIVE_BYTES 2537577424ULL
#define BT4_TEXTURE_ARCHIVE_SHA256 "6f54fc5e362f604de092ff8c46259f8ab3dc175ac3d352c72ca9c2585a9d558a"
/* Only the archive distributed by earlier BT4 installers is managed here.
   A filename and size probe is cheap enough for a menu frame; the installer
   checks SHA-256 before removing the superseded file. Custom packs are kept. */
#define BT4_TEXTURE_LEGACY_ARCHIVE "dbz bt4 HD v1.07.zip"
#define BT4_TEXTURE_LEGACY_BYTES 3690378076ULL
#define BT4_TEXTURE_LEGACY_SHA256 "f129248fcd0d508a5e901e6901109726fd2b38e1d57812b9f8ba743aafdd4129"
#endif
