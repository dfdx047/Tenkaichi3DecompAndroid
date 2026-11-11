#pragma once
#include "../include/bt4_texture_pack.h"
// Pinned optional packages in the BT4 project repository.
struct LanguagePackage{const char *name;Uint64 bytes;const char *sha256;};
static const char *kLanguageRepository="NauTaro/DBZ-BT4-Tag-Team-Decomp";
static const char *kLanguageRelease="bt4-language-packs-b14rev2-v1";
static const char *kTextureRelease="bt4-hd-textures-v1.0-light";
static const LanguagePackage kTexturePackage={BT4_TEXTURE_ARCHIVE,BT4_TEXTURE_ARCHIVE_BYTES,BT4_TEXTURE_ARCHIVE_SHA256};
static const LanguagePackage kTexturePieces[]={{"BT4-HD-Light-v1.0.zip.001",1270000000ull,"7a5cf9f951526072192ec93306ad8d47b61ea26d0221feeb7bc11224850ae680"},{"BT4-HD-Light-v1.0.zip.002",1267577424ull,"4156af457cc70c7c3d1379459172f9732073f786077ef9c70c7cafa83d801685"}};
static const LanguagePackage kLanguagePackages[]={{"spanish-b14rev2.bt4lang",1403690328ull,"8a23297ad0019e08837360b26da417f717b9dbd52042897bffb87044625579d5"},{"english-b14rev2.bt4lang",1907429425ull,"043dc5bdc88a60fc7af86b36f0d098823960621998c868fa9775926b1d360cf1"}};
static const LanguagePackage kPortuguesePackage={"portuguese-b14rev2.bt4lang",59065456ull,"100f751998cfcf1687e30c9444e13360757cc8fb2fff0d940aa673b5fe9cc8cf"};
