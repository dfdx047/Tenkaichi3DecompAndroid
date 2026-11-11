#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include "../../src/gs/voice_audio_sdl.inc"
int main(void){
    if(!SDL_Init(SDL_INIT_AUDIO))return 2;
    {
        VoiceAudio input(true),output(false);
        if(input.open("@none"))return 3;
        if(!input.open("")||!output.open(""))return 4;
        short pcm[320]={0};for(int i=0;i<30;i++)output.write(pcm);
        Uint64 begin=SDL_GetTicks();int bytes=0;
        while(SDL_GetTicks()-begin<1500&&bytes!=640){bytes=input.read(pcm);SDL_Delay(2);}
        if(bytes!=640)return 5;
        input.close();output.close();if(input||output)return 6;
    }
    SDL_Quit();return 0;
}
