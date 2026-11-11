#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include "../../src/gs/supabase_signal.h"
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
using Json=nlohmann::json;
static bool next(unsigned serial,Json &body){
    Uint64 until=SDL_GetTicks()+15000;
    while(SDL_GetTicks()<until){char bytes[24577];if(Bt4Signal_Read(0,bytes,sizeof(bytes))>0){
        Json reply=Json::parse(bytes,nullptr,false);if(reply.is_object()&&reply.contains("serial")&&reply["serial"]==serial){body=reply["body"];return true;}}
        SDL_Delay(5);
    }return false;
}
int main(){
    if(!SDL_Init(0))return 1;
    if(!Bt4Signal_Open())return 2;
    Uint64 end=SDL_GetTicks()+15000;while(Bt4Signal_Status()==0&&SDL_GetTicks()<end)SDL_Delay(10);
    if(Bt4Signal_Status()!=1){fprintf(stderr,"Native private inbox connection failed\n");Bt4Signal_Close();return 3;}
    Json reply;
    Bt4Signal_Command(0,1,"leave","{}");if(!next(1,reply))return 4;
    const char *profile="{\"mode\":\"tag\",\"wire\":\"bt4-p2p-1\",\"content\":\"5454000754540008\",\"rules\":\"native-probe\",\"name\":\"Native probe\",\"capacity\":4}";
    Bt4Signal_Command(0,2,"create",profile);
    if(!next(2,reply)||!reply.contains("room")){fprintf(stderr,"Native create failed: %s\n",reply.dump().c_str());Bt4Signal_Close();return 5;}
    Bt4Signal_Command(0,3,"get","{}");Json same;
    if(!next(3,same)||same["room"]!=reply["room"])return 6;
    Bt4Signal_Command(0,4,"leave","{}");if(!next(4,reply)||reply["state"]!="left")return 7;
    Bt4Signal_Close();SDL_Quit();puts("PASS native HTTPS authentication, private Realtime inbox, room create/get/leave");return 0;
}
