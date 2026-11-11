/* Cache v1 contains the native decoder's PCM, without gain or resampling.
 * Source bytes key each file so changing a disc track cannot reuse stale audio. */
#ifndef BT4_MUSIC_CACHE_H
#define BT4_MUSIC_CACHE_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
static uint64_t bt4_music_key(const void *data,size_t bytes){
    const unsigned char *p=(const unsigned char *)data;uint64_t h=UINT64_C(14695981039346656037);
    while(bytes--){h^=*p++;h*=UINT64_C(1099511628211);}return h;
}
static void bt4_music_path(char *out,size_t n,uint64_t key){
    snprintf(out,n,"tbo/music/v1/%016llx.flac",(unsigned long long)key);
}
#endif
