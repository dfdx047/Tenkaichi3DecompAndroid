#include "../../src/gs/menu_hd.cpp"
#include <cassert>
#include <vector>
#include <cstdio>

int main() {
    std::vector<uint32_t> seed(32*32,0x80000000u);
    for(unsigned i=0;i<seed.size();++i)seed[i]|=(i*13791)&0xffffff;
    assert(Bt4MenuHD_Request(seed.data(),32,32,2,1,nullptr)==0);
    uint64_t deadline=SDL_GetTicks()+10000;
    while(Bt4MenuHD_Request(nullptr,32,32,2,1,nullptr)==0 && SDL_GetTicks()<deadline)SDL_Delay(1);
    assert(Bt4MenuHD_Request(nullptr,32,32,2,1,nullptr)==1);
    std::vector<uint32_t> pixels(1024*1024);
    for(unsigned i=0;i<pixels.size();++i)pixels[i]=0x80000000u|((i*13791)&0xffffff);
    assert(Bt4MenuHD_Request(pixels.data(),1024,1024,3,2,nullptr)==0);
    bool running=false;
    while(SDL_GetTicks()<deadline){
        SDL_LockMutex(artLock);
        for(auto &item:artwork)if(item.key==2 && item.state==2)running=true;
        SDL_UnlockMutex(artLock);
        if(running)break;
        SDL_Delay(1);
    }
    assert(running);
    std::puts("Artwork shutdown while a real xBRZ job is running");
    // Returning runs the actual C++ destructors and registered worker shutdown.
}
