#include <stdio.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "../../src/gs/tag_team_ws.h"
int main(int argc,char **argv){
    unsigned char sent[512],got[512];int count=0,reject=argc>2;Uint64 start;
    if(argc<2)return 2;
    SDL_Init(0);memset(sent,0xa7,sizeof(sent));
    if(!TagWs_Open(argv[1],"ABCDEF"))return 3;
    if(!TagWs_Send(sent))return 4;
    memset(sent,0xb7,sizeof(sent));if(!TagWs_Send(sent))return 5;
    start=SDL_GetTicks();
    while(SDL_GetTicks()-start<8000){
        int n=TagWs_Read(got,NULL);
        if(n<0){TagWs_Close();return reject?0:6;}
        if(n>0){
            unsigned char expected=count?0xb7:0xa7;
            for(unsigned i=0;i<sizeof(got);i++)if(got[i]!=expected){TagWs_Close();return 7;}
            if(++count==2){TagWs_Close();return reject?8:0;}
        }else TagWs_Wait(20);
    }
    TagWs_Close();return 9;
}
