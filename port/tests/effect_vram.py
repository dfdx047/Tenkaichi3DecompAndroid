"""Run the real native effect allocator against a crowded clash's uploads.

Run with Python and a C compiler (CC defaults to cc). No disc data is needed.
"""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
source = (ROOT / 'src/battle/eft_ae.c').read_text()
start = source.index('void EftVram_Init(')
end = source.index('/* Empty: a stripped assert', start)
body = source[start:end]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef int32_t s32; typedef uint32_t u32; typedef uint64_t u64;
typedef struct { s32 imageBlocks,clutBlocks; } EftAeImage;
typedef struct { u64 tex0; EftAeImage *image; } EftAeTex;
typedef struct { EftAeTex *tex; s32 imageBlock,clutBlock,flags; } EftAeVramEntry;
typedef struct { EftAeVramEntry *entry; u32 count,capacity; s32 imageNext,clutNext; } EftAeVram;
typedef struct { EftAeTex tex[32];s32 count; } EftAeTexSet32;
typedef EftAeTexSet32 EftAeTexSet4,EftAeTexSet8,EftAeTexSet16;
static EftAeVram *gEftVram;
static void *Heap_Alloc(u32 n,s32 a,s32 b,s32 c) { (void)a;(void)b;(void)c;return calloc(1,n); }
static void Heap_Free(void *p) {free(p);}
static unsigned char occupied[0x4000];static int uploaded;
static void reserve(int first,int size) {
    int i;assert(first>=0x2A00 && first+size<=0x4000);
    for(i=first;i<first+size;i++) {assert(!occupied[i]);occupied[i]=1;}
}
static void EftVram_Upload(EftAeTex *t,s32 image,s32 clut) {
    if(image>=0)reserve(image,t->image->imageBlocks);
    if(clut>=0)reserve(clut,t->image->clutBlocks);
    uploaded++;
}
'''
tests = r'''
int main(void) {
    EftAeTex textures[193],duplicate; EftAeImage images[193];u64 words[193];int i,frame;
    EftVram_Init();
    for(frame=0;frame<4;frame++) {
        memset(occupied,0,sizeof occupied);uploaded=0;
        for(i=0;i<193;i++) {
            images[i].imageBlocks=4+(i%3)*4;images[i].clutBlocks=4;
            textures[i].image=&images[i];textures[i].tex0=0x123400000;
            words[i]=EftVram_AddTex(&textures[i],1,0);
        }
        /* One source shared by 10 actors and four viewports allocates once. */
        for(i=0;i<40;i++) {
            duplicate=textures[37];
            assert(EftVram_AddTex(&duplicate,1,0)==words[37]);
        }
        assert(gEftVram->count==193);
        EftVram_UploadAll();assert(uploaded==193);EftVram_Reset();
        assert(gEftVram->count==0);
    }
    /* Image-only followed by a palette request retains both original bases. */
    words[0]=EftVram_AddImage(&textures[0],1,0);
    words[1]=EftVram_AddClut(&textures[0]);
    words[2]=EftVram_AddTex(&textures[0],1,0);
    assert((words[0]&0x3fff)==(words[2]&0x3fff));
    assert(words[1]==((words[2]>>37)&0x3fff));assert(gEftVram->count==1);
    memset(occupied,0,sizeof occupied);EftVram_UploadAll();EftVram_Term();
    puts("Effect textures: crowded palettes, shared uploads, partial requests and resets passed");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='bt3-effect-vram-') as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test'
    c.write_text(prefix + body + tests)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
