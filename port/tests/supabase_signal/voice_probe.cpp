/* Two native processes, synthetic microphone and dummy playback only. */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#define JSON_NOEXCEPTION
#include "../../third_party/voice/json.hpp"
#include "../../src/gs/supabase_signal.h"
#include "../../src/gs/voice.h"
using J=nlohmann::json;
extern "C" int Port_Setting(const char *key,int fallback){
    if(!strcmp(key,"voice_enabled")||!strcmp(key,"voice_mode"))return 1;
    if(!strcmp(key,"voice_threshold"))return 1;return fallback;
}
extern "C" void Port_SettingSave(const char*,int){}
extern "C" void Port_SettingsWrite(void){}
extern "C" void Port_SettingText(const char*,const char *fallback,char *out,int size){SDL_strlcpy(out,fallback,size);}
extern "C" void Port_SettingTextSave(const char*,const char*){}
extern "C" SDL_AudioDeviceID Port_AudioPeekDevice(void){return 0;}
int main(int argc,char **argv){
    if(argc<3)return 2;SDL_Init(0);bool host=!strcmp(argv[1],"host");FILE *log=fopen(argv[2],"w");if(!log)return 3;
    auto rpc=[](unsigned id,const char *op,const J &args){return Bt4Signal_Command(0,id,op,args.dump().c_str());};
    bool manual=getenv("BT4_VOICE_MANUAL_PROBE")!=nullptr;
    if(manual){unsigned char key[32]={};const char *hex=getenv("BT4_VOICE_PROBE_KEY");if(!hex||strlen(hex)!=64)return 8;
        for(int i=0;i<32;++i){char byte[3]={hex[i*2],hex[i*2+1],0};key[i]=(unsigned char)strtoul(byte,nullptr,16);}
        // Simulates the shared key already delivered by the native UDP greeting.
        rpc(1,"leave",J::object());PortVoice_DuelKey(key,host?0:1,host?"Voice A":"Voice B");
        if(host){fprintf(log,"CODE MANUAL\n");fflush(log);}
    }else{
        rpc(1,"leave",J::object());
        if(host)rpc(2,"create",{{"mode","tag"},{"wire","voice-native-test"},{"content","5454000754540008"},{"rules","voice-test"},{"name","Voice A"},{"capacity",2}});
        else rpc(2,"join",{{"code",argc>3?argv[3]:""},{"wire","voice-native-test"},{"content","5454000754540008"},{"name","Voice B"}});
    }
    Uint64 begin=SDL_GetTicks(),mediaAt=0;bool printed=false,disabled=false,enabled=false,cut=false;unsigned beforeToggle=0,beforeCut=0;std::string generation;
    while(SDL_GetTicks()-begin<35000){
        char bytes[24577];while(Bt4Signal_Read(0,bytes,sizeof(bytes))>0){auto e=J::parse(bytes,nullptr,false);if(!e.is_object())continue;
            auto event=e.value("event",std::string());auto body=e.find("body");if(body==e.end()||!body->is_object())continue;
            if(event=="reply"&&e.value("serial",0)==2&&body->contains("error")){fprintf(log,"ERROR %s\n",body->dump().c_str());fflush(log);return 4;}
            if((event=="bt4.room"||event=="reply")&&body->contains("members")){
                generation=body->value("generation",std::string());PortVoice_P2PSession(2,body->dump().c_str(),nullptr,0);
                if(host&&!printed){fprintf(log,"CODE %s\n",body->value("code",std::string()).c_str());fflush(log);printed=true;}
            }
        }
        PortVoiceState s={};PortVoice_State(&s);Uint64 now=SDL_GetTicks();
        if(!mediaAt&&s.direct>30&&s.decoded>20){mediaAt=now;fprintf(log,"MEDIA initial direct=%u relay=%u decoded=%u\n",s.direct,s.relayed,s.decoded);fflush(log);}
        if(mediaAt){Uint64 elapsed=now-mediaAt;
            if(!host&&elapsed>=2000&&!disabled){beforeToggle=s.direct;PortVoiceOptions o;PortVoice_Options(&o);o.enabled=0;PortVoice_OptionsSet(&o);disabled=true;fprintf(log,"DISABLED\n");fflush(log);}
            if(!host&&elapsed>=4000&&!enabled){PortVoiceOptions o;PortVoice_Options(&o);o.enabled=1;PortVoice_OptionsSet(&o);enabled=true;fprintf(log,"ENABLED\n");fflush(log);}
            if(elapsed>=10000&&!cut){beforeCut=s.direct;Bt4Signal_Close();cut=true;fprintf(log,"SIGNAL_DISCONNECTED direct=%u\n",s.direct);fflush(log);}
            if(elapsed>=14000){bool ok=s.direct>beforeCut+80&&s.relayed==0&&s.decoded>100&&(host||s.direct>beforeToggle+100);
                fprintf(log,"DONE ok=%d direct=%u relay=%u decoded=%u\n",ok,s.direct,s.relayed,s.decoded);fflush(log);
                PortVoice_Shutdown();Bt4Signal_Close();fclose(log);SDL_Quit();return ok?0:5;}
        }
        SDL_Delay(10);
    }
    PortVoiceState s;PortVoice_State(&s);fprintf(log,"TIMEOUT sent=%u received=%u direct=%u relay=%u peers=%d status=%s\n",s.sent,s.received,s.direct,s.relayed,s.count,s.status);fclose(log);PortVoice_Shutdown();Bt4Signal_Close();SDL_Quit();return 6;
}
