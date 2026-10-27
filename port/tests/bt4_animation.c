/* Standalone check of decoded-animation bounds; uses no copyrighted assets.
 * cc -Iport/include port/tests/bt4_animation.c port/src/plat_game.c -o test */
#include "plat_game.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
const char *Port_FileRoot(void) { return getenv("BT3_DATA"); }
void *Port_LowAlloc(size_t n) { return malloc(n); }
static void w16(unsigned char *p,unsigned v) {p[0]=v;p[1]=v>>8;}
int main(void) {
    unsigned char data[0x10020],copy[0x10020];
    unsigned char *b=data+16;void *a,*a2,*other;int i;
    memset(data,0,sizeof(data));memset(data,0xa5,16);memset(b+0x10000,0xa5,16);
    /* One two-key rotation track; its valid neighbours must survive a bad one. */
    w16(b+2,30);w16(b+6,148/4);w16(b+148,1);w16(b+150,2);
    b[159]=b[167]=0x30;w16(b+170,30);
    memcpy(copy,data,sizeof(data));assert(PortGame_CheckAnimation(b,172)==0);
    assert(!memcmp(data,copy,sizeof(data)));
    w16(b+8,16400);assert(PortGame_CheckAnimation(b,172)==1);assert(!b[8]&&!b[9]);
    w16(b+8,148/4);w16(b+150,65535);assert(PortGame_CheckAnimation(b,172)==2);
    memcpy(data,copy,sizeof(data));b[159]=0xf0;
    assert(PortGame_CheckAnimation(b,172)==1);assert(!b[6]&&!b[7]);
    memcpy(data,copy,sizeof(data));b[1]=255;assert(PortGame_CheckAnimation(b,172)==-1);
    memcpy(data,copy,sizeof(data));assert(PortGame_CheckAnimation(b,147)==-1);
    assert(PortGame_CheckAnimation(b,0x10001)==-1);
    for(i=0;i<16;i++)assert(data[i]==0xa5 && b[0x10000+i]==0xa5);
    if(PortGame_IsBT4()) {
        assert(PortGame_StreamFile(0x10BC7)==0x14E);
        assert(PortGame_StreamFile(0x10BCA)==0x151);
        assert(PortGame_StreamFile(0x10BDB)==0x158);
        assert(PortGame_StreamFile(0x10BE1)==0x15E);
        assert(PortGame_StreamFile(0x151)==0x151);
        assert(PortGame_StreamFile(0x10BCB)==0x10BCB);
        assert(PortGame_AnimationCapacity()==0x10000);
        assert(PortGame_CheckAnimation(b,53632)==0);
        assert(PortGame_VoiceLine(165,0,0)==0x10CC2+16500);
        assert(PortGame_VoiceLine(165,99,1)==0x16E6A+16500+99);
        /* English B14 has an invalid alternate Time Patroller bank. Spanish
           B14 provides both banks, so it must retain its chosen audio. */
        if(strcmp(PortGame_DefaultLanguage(),"es"))
            assert(PortGame_Voice(246,1)==PortGame_Voice(246,0));
        else {
            assert(PortGame_Voice(246,0)==0x1D012+246);
            assert(PortGame_Voice(246,1)==0x1D10C+246);
        }
        a=PortGame_AnimationBuffer(b);a2=PortGame_AnimationBuffer(b);
        other=PortGame_AnimationBuffer(copy);assert(a==a2 && a!=other && a!=b);
        memset(a,1,0x10000);memset(other,2,0x10000);
        PortGame_ResetAnimations();assert(PortGame_AnimationBuffer(copy)==a);
    } else {
        assert(PortGame_StreamFile(0x10BC7)==0x10BC7);
        assert(PortGame_StreamFile(0x10BE1)==0x10BE1);
        assert(PortGame_AnimationBuffer(b)==b);
        assert(PortGame_VoiceLine(5,17,0)==0x8D4E + 517);
        assert(PortGame_VoiceLine(5,17,1)==0xCC32+517);
    }
    puts("PASS animation bounds, buffer reuse and disc voice profiles");return 0;
}
