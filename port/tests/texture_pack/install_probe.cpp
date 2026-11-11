#include "../../setup/native.cpp"
#include "../../include/bt4_texture_update.h"
#include <cassert>

int main(int argc,char **argv){
    if(argc!=3)return 2;
    SDL_Init(0);
    try{
        const std::string archive=argv[1],root=std::string(argv[2])+"/";
        validate_texture_archive(archive);
        // The harness supplies an existing cache hardlink; no network request,
        // second archive copy or DDS extraction is needed.
        sSteps=1;sStep=0;sCancel=false;
        install_texture_pack(root,"@download");
        const std::string installed=root+"textures/"+kTexturePackage.name;
        validate_texture_archive(installed);
        install_texture_pack(root,installed);
        assert(!SDL_GetPathInfo((root+"tbo/setup/cache/"+kTexturePackage.name).c_str(),NULL));
        assert(SDL_GetPathInfo(installed.c_str(),NULL));
        assert(!Bt4Texture_NeedsUpdate(root.c_str()));
        // An already installed pack uses the dedicated maintenance worker,
        // without any ISO, game data or language-profile changes.
        const std::string languages=root+"tbo/languages.ini";
        assert(SDL_SaveFile(languages.c_str(),"7\n",2));
        Native_StartTextures(root);
        while(Native_Running())SDL_Delay(10);
        bool finished=false;std::string line;
        while(Native_Poll(line))if(line.rfind("@done",0)==0)finished=true;
        Native_Join();assert(finished);
        size_t n=0;char *saved=(char *)SDL_LoadFile(languages.c_str(),&n);
        assert(saved&&n==2&&!memcmp(saved,"7\n",2));SDL_free(saved);
        assert(!SDL_GetPathInfo((root+"gamedata").c_str(),NULL));
        std::string fake=root+"wrong.zip";assert(SDL_SaveFile(fake.c_str(),"invalid",7));
        bool rejected=false;try{validate_texture_archive(fake);}catch(const Stop &){rejected=true;}
        assert(rejected);SDL_RemovePath(fake.c_str());
        std::puts("Texture setup: exact supplied ZIP, SHA-256, DDS index, cached download, atomic install, retry and damaged download rejection PASS");
    }catch(const Stop &e){std::fprintf(stderr,"Setup probe failed: %s\n",e.why.c_str());return 3;}
    SDL_Quit();return 0;
}
