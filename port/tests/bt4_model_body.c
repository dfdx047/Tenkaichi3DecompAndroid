/* Requires the user's private B14 import; never embeds disc assets.
   cc -Iport/include port/tests/bt4_model_body.c port/src/plat_game.c -o test */
#include "plat_game.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
const char *Port_FileRoot(void) { return getenv("BT3_DATA"); }
void *Port_LowAlloc(size_t n) { return malloc(n); }
static unsigned char *body(int file) {
    char path[1024];FILE *f;uint32_t h[4];unsigned char *p=malloc(9792);
    snprintf(path,sizeof(path),"%s/pzs4us2/%05d.bin",Port_FileRoot(),file);
    f=fopen(path,"rb");assert(f && fread(h,4,4,f)==4);
    assert((h[3]&~3u)-(h[2]&~3u)==9792);
    assert(!fseek(f,h[2]&~3u,SEEK_SET) && fread(p,1,9792,f)==9792);fclose(f);return p;
}
int main(void) {
    unsigned char *intact=body(2924),*bad=body(3805),*damaged=body(3807),*copy=malloc(9792);
    assert(memcmp(intact,bad,9792));memcpy(copy,bad,9792);
    PortGame_RepairModelBody(bad,9791,7205);assert(!memcmp(copy,bad,9792));
    PortGame_RepairModelBody(bad,9792,7204);assert(!memcmp(copy,bad,9792));
    bad[9000]^=1;memcpy(copy,bad,9792);
    PortGame_RepairModelBody(bad,9792,7205);assert(!memcmp(copy,bad,9792));bad[9000]^=1;
    PortGame_RepairModelBody(bad,9792,7205);assert(!memcmp(intact,bad,9792));
    PortGame_RepairModelBody(bad,9792,7205);assert(!memcmp(intact,bad,9792));
    PortGame_RepairModelBody(damaged,9792,7207);assert(!memcmp(intact,damaged,9792));
    free(intact);free(bad);free(damaged);free(copy);
    puts("PASS body repair matches intact disc shape; other data unchanged");return 0;
}
