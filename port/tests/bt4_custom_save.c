/* Run as separate write/read processes with BT3_DATA and an isolated BT3_SAVES.
 * cc -Iport/include port/tests/bt4_custom_save.c port/src/plat_game.c -o test */
#include "plat_game.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
const char *Port_FileRoot(void) { return getenv("BT3_DATA"); }
void *Port_LowAlloc(size_t size) { return malloc(size); }
int main(int argc,char **argv) {
    unsigned char original[97*56+32];
    int slots[]={0,96,97,154,249},i,j,write;
    assert(argc==2);write=!strcmp(argv[1],"write");
    memset(original,0xa5,sizeof(original));
    if(!PortGame_IsBT4()) {
        assert(PortGame_CustomSlot(original,96)==original+96*56);
        assert(PortGame_SaveCustom());
        puts("PASS BT3 original equipment layout");return 0;
    }
    for(i=0;i<5;i++) {
        unsigned char *slot=PortGame_CustomSlot(original,slots[i]);
        if(slots[i]<97) {
            assert(slot==original+slots[i]*56);
            continue;
        }
        for(j=0;j<56;j++) {
            unsigned char value=(unsigned char)(slots[i]+j);
            if(write)slot[j]=value;
            else assert(slot[j]==value);
        }
    }
    for(i=0;i<(int)sizeof(original);i++)assert(original[i]==0xa5);
    assert(PortGame_SaveCustom());
    { extern void PortGame_OnlineCustom(int);
      PortGame_OnlineCustom(1);PortGame_OnlineCustom(1);
      for(i=2;i<5;i++) {
          unsigned char *slot=PortGame_CustomSlot(original,slots[i]);
          for(j=0;j<56;j++)assert(slot[j]==0);
          memset(slot,0x77,56);
      }
      assert(PortGame_SaveCustom()); /* Must not write temporary online gear. */
      PortGame_OnlineCustom(0);
      for(i=2;i<5;i++) {
          unsigned char *slot=PortGame_CustomSlot(original,slots[i]);
          for(j=0;j<56;j++)assert(slot[j]==(unsigned char)(slots[i]+j));
      }
      PortGame_OnlineCustom(0);
    }
    puts(write?"PASS BT4 equipment write":"PASS BT4 equipment reload");return 0;
}
