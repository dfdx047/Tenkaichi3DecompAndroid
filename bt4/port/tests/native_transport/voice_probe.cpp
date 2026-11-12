#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "../../src/gs/voice_transport_curl.inc"
int main(int argc,char **argv){
    if(argc<2)return 2;
    SDL_Init(0);unsigned char audio[512];memset(audio,0xb7,sizeof(audio));
    VoiceWs ws(std::string(argv[1])+"/voice",std::string(64,'a'),0,"NauTaro Test");
    ws.text("ping");ws.send(audio,sizeof(audio),true);
    Uint64 begin=SDL_GetTicks();bool text=false,binary=false;
    while(SDL_GetTicks()-begin<8000){
        if(ws.status<0)return 3;
        VoiceWsMessage message;
        while(ws.read(message)){
            if(message.binary){if(message.bytes.size()!=512||message.bytes[0]!=0xb7)return 4;binary=true;}
            else {if(std::string(message.bytes.begin(),message.bytes.end())!="ping")return 5;text=true;}
        }
        if(text&&binary)return 0;
        SDL_Delay(1);
    }
    return 6;
}
