#ifndef BT4_TEXTURE_UPDATE_H
#define BT4_TEXTURE_UPDATE_H
#include <SDL3/SDL.h>
#include "bt4_texture_pack.h"
/* root ends in '/'. No archive is loaded or decompressed on this path. */
static int Bt4Texture_NeedsUpdate(const char *root) {
    char path[4096]; SDL_PathInfo info;
    if (SDL_snprintf(path,sizeof(path),"%stextures/%s",root,BT4_TEXTURE_ARCHIVE)>=(int)sizeof(path))return 0;
    if(SDL_GetPathInfo(path,&info)&&info.type==SDL_PATHTYPE_FILE&&info.size==BT4_TEXTURE_ARCHIVE_BYTES)return 0;
    if(SDL_snprintf(path,sizeof(path),"%stextures/%s",root,BT4_TEXTURE_LEGACY_ARCHIVE)>=(int)sizeof(path))return 0;
    return SDL_GetPathInfo(path,&info)&&info.type==SDL_PATHTYPE_FILE&&info.size==BT4_TEXTURE_LEGACY_BYTES;
}
#endif
