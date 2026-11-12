/* Verified optional language overrides, downloaded from the private BT4 release. */
bool base_english(const std::string &root) {
    size_t n=0; char *p=(char *)SDL_LoadFile((root+"gamedata/.bt4-language").c_str(), &n);
    if(p){bool en=n>=2&&!strncmp(p,"en",2);SDL_free(p);return en;}
    p=(char *)SDL_LoadFile((root+"gamedata/.installed-bt4-b14rev2").c_str(), &n);
    if(p){bool en=std::string(p,n).find("ENG")!=std::string::npos;SDL_free(p);return en;}
    std::vector<Uint8> buf(1024*1024);
    const int last=kVerifyEngCount[0]+kVerifyEngCount[1]+kVerifyEngCount[2]+kVerifyEngCount[3]+7;
    return file_crc(root+"gamedata/disc/SLUS_219.78",kVerifyEng[last][0],buf)==kVerifyEng[last][1];
}
bool english_installed(const std::string &root) {
    const std::string data=root+"gamedata/"+(base_english(root)?"":"languages/en/");
    return SDL_GetPathInfo((data+".installed-bt4-b14rev2").c_str(),NULL) &&
           SDL_GetPathInfo((data+"disc/SLUS_219.78").c_str(),NULL) &&
           SDL_GetPathInfo((data+"disc/BIN/DBZP.BIN").c_str(),NULL);
}
struct EnglishEntry { Uint64 offset=0; Uint32 packed=0,size=0,crc=0; };
static Uint32 pack_u32(const Uint8 *p){return Uint32(p[0])|(Uint32(p[1])<<8)|(Uint32(p[2])<<16)|(Uint32(p[3])<<24);}
static Uint64 pack_u64(const Uint8 *p){return Uint64(pack_u32(p))|(Uint64(pack_u32(p+4))<<32);}
static std::string language_path(int index,const int *counts) {
    for(int part=0;part<4;++part){
        if(index<counts[part]){
            char name[64];snprintf(name,sizeof(name),"pzs4us%d/%05d.bin",part,index);return name;
        }
        index-=counts[part];
    }
    if(index<0||index>=(int)(sizeof(kVerifyLoose)/sizeof(kVerifyLoose[0])))throw Stop{"Invalid English entry."};
    return kVerifyLoose[index];
}
static void install_language(const std::string &root,bool english) {
    const int *counts=english?kVerifyEngCount:kVerifyCount;
    const unsigned int (*verify)[2]=english?kVerifyEng:kVerify;
    const std::string packagePath=download_language(root,english);
    const std::string base=root+"gamedata/", target=base+"languages/"+(english?"en/":"es/");
    if(!SDL_GetPathInfo((base+"disc/SLUS_219.78").c_str(),NULL)||
       !SDL_GetPathInfo((base+"disc/BIN/DBZP.BIN").c_str(),NULL))
        throw Stop{"Install your Spanish BT4 B14 Rev2 ISO first. The included English pack adds a language to that installation."};
    SDL_IOStream *pack=SDL_IOFromFile(packagePath.c_str(),"rb");
    if(!pack)throw Stop{"The bundled English language pack is missing."};
    try {
        Uint8 header[24];
        const int total=counts[0]+counts[1]+counts[2]+counts[3]+sizeof(kVerifyLoose)/sizeof(kVerifyLoose[0]);
        const Sint64 packSize=SDL_GetIOSize(pack);
        if(SDL_ReadIO(pack,header,sizeof(header))!=sizeof(header)||memcmp(header,english?"BT4ENP01":"BT4ESP01",8)||
           pack_u32(header+8)!=1||pack_u64(header+16)!=(Uint64)total)
            throw Stop{"Invalid bundled English pack. Extract the complete ZIP again."};
        const Uint32 records=pack_u32(header+12);
        if(records>(Uint32)total||packSize<24+(Sint64)records*24)throw Stop{"Invalid English pack index."};
        std::vector<EnglishEntry> entries(total);int previous=-1;
        for(Uint32 k=0;k<records;++k){
            Uint8 row[24];if(SDL_ReadIO(pack,row,24)!=24)throw Stop{"Incomplete English pack index."};
            Uint32 i=pack_u32(row);EnglishEntry e;
            e.offset=pack_u64(row+4);e.packed=pack_u32(row+12);e.size=pack_u32(row+16);e.crc=pack_u32(row+20);
            if(i>=(Uint32)total||(int)i<=previous||e.size!=verify[i][0]||e.crc!=verify[i][1]||
               !e.packed||e.packed>e.size+1024*1024||e.offset<24+(Uint64)records*24||
               e.offset>(Uint64)packSize||e.packed>(Uint64)packSize-e.offset)
                throw Stop{"English pack does not match BT4 B14 Rev2."};
            entries[i]=e;previous=(int)i;
        }
        SDL_CreateDirectory(target.c_str());
        for(int part=0;part<4;++part)SDL_CreateDirectory((target+"pzs4us"+std::to_string(part)).c_str());
        SDL_CreateDirectory((target+"disc/BIN").c_str());
        SDL_CreateDirectory((target+"disc/DATA").c_str());
        SDL_CloseIO(pack);pack=NULL;
        say("@activity unpack");
        std::atomic<int> next(0),completed(0);std::atomic<bool> failed(false);
        std::mutex workerLock;std::string firstError;int last=-1;
        auto worker=[&](){
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
#endif
        SDL_IOStream *input=NULL;
        try {
        input=SDL_IOFromFile(packagePath.c_str(),"rb");
        if(!input)throw Stop{"Could not open the included English language pack."};
        std::vector<Uint8> buf(1024*1024),compressed,raw;
        for(int i;(i=next.fetch_add(1))<total&&!failed.load();){
            check_cancel();
            std::string relative=language_path(i,counts),path=target+relative,temp=path+".part";
            Uint32 size=verify[i][0],crc=verify[i][1];
            if(file_crc(path,size,buf)!=crc){
                try {
                    const EnglishEntry &e=entries[i];bool finalized=false;
                    if(e.packed){
                        compressed.resize(e.packed);raw.resize(e.size);
                        if(SDL_SeekIO(input,(Sint64)e.offset,SDL_IO_SEEK_SET)<0||SDL_ReadIO(input,compressed.data(),e.packed)!=e.packed)
                            throw Stop{"English pack is incomplete. Extract the complete ZIP again."};
                        uLongf made=e.size;
                        if(uncompress(raw.data(),&made,compressed.data(),e.packed)!=Z_OK||made!=e.size||SDL_crc32(0,raw.data(),made)!=crc)
                            throw Stop{"Corrupt English pack entry: "+relative};
                        if(!SDL_SaveFile(temp.c_str(),raw.data(),raw.size()))throw Stop{"Could not write the English language data. Check free space."};
                    }else{
                        const std::string shared=base+relative;
                        if(file_crc(shared,size,buf)!=crc)throw Stop{"Spanish base data is missing or incompatible: "+relative+". Repair it with your Spanish B14 Rev2 ISO."};
                        // A new hard link is atomic. Avoid a temporary rename and
                        // recursive directory creation for every common file.
                        if(share_file(shared,path))finalized=true;
                        if(!finalized){
                        SDL_RemovePath(temp.c_str());
                        if(!share_file(shared,temp)){
                            SDL_IOStream *from=SDL_IOFromFile(shared.c_str(),"rb"),*to=SDL_IOFromFile(temp.c_str(),"wb");
                            if(!from||!to){if(from)SDL_CloseIO(from);if(to)SDL_CloseIO(to);throw Stop{"Could not copy the common language files. Check free space."};}
                            bool ok=true;Uint64 remaining=size;
                            while(remaining){size_t n=remaining<buf.size()?(size_t)remaining:buf.size();
                                if(SDL_ReadIO(from,buf.data(),n)!=n||SDL_WriteIO(to,buf.data(),n)!=n){ok=false;break;}remaining-=n;}
                            SDL_CloseIO(from);if(!SDL_CloseIO(to))ok=false;
                            if(!ok)throw Stop{"Could not finish copying the English language data."};
                        }
                        }
                    }
                    if(!finalized&&!SDL_RenamePath(temp.c_str(),path.c_str()))throw Stop{"Could not finalize English file: "+relative};
                }catch(...){SDL_RemovePath(temp.c_str());throw;}
            }
            int done=completed.fetch_add(1)+1,pct=done*100/total;
            {std::lock_guard<std::mutex> lock(workerLock);
            if(pct>last){last=pct;say("@package "+std::to_string(65+done*35/total));
                say("@files "+std::to_string(done)+" "+std::to_string(total));}}
        }
        }catch(const Stop &error){std::lock_guard<std::mutex> lock(workerLock);if(firstError.empty())firstError=error.why;failed=true;
        }catch(...){std::lock_guard<std::mutex> lock(workerLock);if(firstError.empty())firstError="Could not finish installing English. Check free space and run Install.exe again.";failed=true;}
        if(input)SDL_CloseIO(input);
        };
        std::vector<std::thread> workers;
        try{for(int k=0;k<4;++k)workers.emplace_back(worker);}
        catch(...){failed=true;for(auto &thread:workers)thread.join();throw Stop{"Could not start the language installation workers. Try Install.exe again."};}
        for(auto &thread:workers)thread.join();
        if(failed||completed!=total)throw Stop{firstError.empty()?"English installation interrupted.":firstError};
        if(!SDL_SaveFile((target+".bt4-language").c_str(),(english?"en\n":"es\n"),3))throw Stop{"Could not save the English language selection."};
        say("@ok English text and voices installed and verified");
        std::string savedRoot=sDataRoot;bool savedEnglish=sEnglish;
        sDataRoot=target;sEnglish=english;
        try {step_test(root);}catch(...){sDataRoot=savedRoot;sEnglish=savedEnglish;throw;}
        sDataRoot=savedRoot;sEnglish=savedEnglish;
        const char *marker=english?"BT4 B14 Rev2 ENG; SLUS_219.78; native profile 7\n":"BT4 B14 Rev2 LATINO; SLUS_219.78; native profile 7\n";
        if(!SDL_SaveFile((target+".installed-bt4-b14rev2").c_str(),marker,strlen(marker)))throw Stop{"Could not complete English installation."};
    }catch(...){if(pack)SDL_CloseIO(pack);throw;}
    SDL_RemovePath(packagePath.c_str());say("@package 100");
}
