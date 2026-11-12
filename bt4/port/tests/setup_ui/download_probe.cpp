// Verify the production HTTPS downloader and native PT-BR ZIP reader without
// requiring an ISO, opening a window or starting a game.
#include "../../setup/native.cpp"
#include <cassert>

int main(int argc,char **argv){
    if(argc!=2)return 2;
    SDL_Init(0);const std::string root=std::string(argv[1])+"/";
    ensure_directories(root);
    const std::string path=root+kPortuguesePackage.name;
    std::atomic<bool> done(false);std::exception_ptr error;
    std::thread download([&]{try{download_public_asset(root,kLanguageRelease,kPortuguesePackage.name,path,kPortuguesePackage.bytes);}catch(...){error=std::current_exception();}done=true;});
    int percent=-1;
    while(!done){
        std::string line;
        while(Native_Poll(line))if(line.rfind("@package ",0)==0){
            const int next=atoi(line.c_str()+9);
            if(next>=percent+10){percent=next;printf("download progress=%d/55\n",percent);fflush(stdout);}
        }
        SDL_Delay(50);
    }
    download.join();if(error)std::rethrow_exception(error);
    assert(sha256_file(path)==kPortuguesePackage.sha256);
    Uint8 header[64];SDL_IOStream *in=SDL_IOFromFile(path.c_str(),"rb");assert(in);
    assert(SDL_ReadIO(in,header,sizeof(header))==sizeof(header));SDL_CloseIO(in);
    assert(!memcmp(header,"BT4PTP01",8));
    ZipReader reader(path,64,kPortuguesePackage.bytes-64);
    assert(reader.open && mz_zip_reader_get_num_files(&reader.zip)>500);
    assert(mz_zip_validate_archive(&reader.zip,0));
    printf("PASS: public PT-BR download, SHA-256, native package header and all ZIP entry CRCs\n");
    SDL_Quit();return 0;
}
