#include "../../src/gs/adx_loop.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void put32(uint8_t *p,uint32_t v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
int main(void) {
    uint8_t h[64];uint32_t a,b;int checks=0;
    for(int version=3;version<=4;++version) for(int channels=1;channels<=2;++channels) {
        size_t base=version==3?0x14:0x20;
        memset(h,0,sizeof(h));h[0]=0x80;h[3]=60;h[7]=channels;h[0x12]=version;
        put32(h+base+4,1);put32(h+base+8,32);put32(h+base+16,320);
        assert(bt4_adx_loop(h,sizeof(h),320,&a,&b)&&a==32&&b==320);checks++;
        assert(!bt4_adx_loop(h,base+19,320,&a,&b)&&!a&&!b);checks++;
        put32(h+base+16,321);assert(!bt4_adx_loop(h,sizeof(h),320,&a,&b));checks++;
        put32(h+base+16,32);assert(!bt4_adx_loop(h,sizeof(h),320,&a,&b));checks++;
        put32(h+base+16,320);put32(h+base+4,0);assert(!bt4_adx_loop(h,sizeof(h),320,&a,&b));checks++;
        put32(h+base+4,1);h[3]=20;assert(!bt4_adx_loop(h,sizeof(h),320,&a,&b));checks++;
    }
    memset(h,0,sizeof(h));assert(!bt4_adx_loop(h,sizeof(h),320,&a,&b));checks++;
    printf("ADX v3/v4 loop metadata, one-shot and truncated/invalid loop bounds: %d checks PASS\n",checks);
    return 0;
}
