// The setup's own installer, for a release folder (BT4decomp, BT4decomp.dat and BT4decomp-setup together, no build tools):
// reads the user's disc image directly, checks it, unpacks the game data and runs the self-test. No Python, no 7z.
// It reports through the same event lines as install.py (@step, @note, @ok, @skip, @fail, @stopped, @done), so the
// window (setup.cpp) shows both the same way.
//
//   disc image   ISO 9660: the volume descriptor at sector 16, directory records from the root
//   checks       SHA-1 of BIN/DBZP.BIN and of SLUS_219.78 laid out as a flat image from 0x100000 (the values of
//                config/DBZP.yaml and config/SLUS_219.78.yaml)
//   game data    gamedata/disc/<loose files>, and every DATA/*.AFS archive split into gamedata/<name>/00000.bin ...
//                (the layout of port/tools/extract_disc.py)
//   European     optional, a second image (SLES-54945): 60 entries of DATA/PZS3EU1.AFS for translated menus/HUD,
//                plus entry 1 of DATA/PZS3EU0.AFS for the native language selector and ZS3EUOP.ADX for the
//                EU opening soundtrack (as port/tools/extract_eu.py).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <functional>
#include <stdint.h>
#include <zlib.h>
#define MINIZ_NO_ARCHIVE_WRITING_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#define MINIZ_NO_TIME
#include "../third_party/miniz/miniz.h"
#include "../src/plat_verify_tab.h"
#include "../src/plat_verify_tab_eng.h"
#include "../include/bt4_build_identity.h"
#include "../src/gs/update_sign.h"
#include <SDL3/SDL.h>
#include "language_packages.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include "native.h"

namespace {

const char *kRomSha1 = "2ae9c636de479c01d2b4e9906c25b7ca807e80d9";
const char *kDbzpSha1 = "de91913f40bd675d70ee748aa16a42a96dff5919";
bool sEnglish;
std::string sDataRoot, sDedupeRoot;
const unsigned int (*sVerify)[2] = kVerify;
const int *sVerifyCount = kVerifyCount;
std::vector<Uint8> sPrograms[2];
const Uint32 kRomBase = 0x100000, kRomEnd = 0x2FF180;
#ifdef _WIN32
const char *kGame = "BT4decomp.exe";
#else
const char *kGame = "BT4decomp";
#endif

std::mutex sLock;
std::vector<std::string> sLines;
std::thread sThread;
std::atomic<bool> sRunning(false), sCancel(false);

void say(const std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    sLines.push_back(line);
}

struct Stop { std::string why; };

// ------------------------------------------------------------------------------------------------------- SHA-1
struct Sha1 {
    Uint32 h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    Uint8 block[64];
    Uint64 total = 0;
    size_t fill = 0;

    static Uint32 rol(Uint32 v, int n) { return (v << n) | (v >> (32 - n)); }
    void compress() {
        Uint32 w[80], a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 16; i++) {
            w[i] = (Uint32)block[i * 4] << 24 | (Uint32)block[i * 4 + 1] << 16 | (Uint32)block[i * 4 + 2] << 8 | block[i * 4 + 3];
        }
        for (int i = 16; i < 80; i++) {
            w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        for (int i = 0; i < 80; i++) {
            Uint32 f = i < 20 ? (b & c) | (~b & d) : i < 40 ? b ^ c ^ d : i < 60 ? (b & c) | (b & d) | (c & d) : b ^ c ^ d;
            Uint32 k = i < 20 ? 0x5A827999 : i < 40 ? 0x6ED9EBA1 : i < 60 ? 0x8F1BBCDC : 0xCA62C1D6;
            Uint32 t = rol(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rol(b, 30); b = a; a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    void add(const Uint8 *p, size_t n) {
        total += n;
        while (n > 0) {
            size_t take = 64 - fill < n ? 64 - fill : n;
            memcpy(block + fill, p, take);
            fill += take; p += take; n -= take;
            if (fill == 64) {
                compress();
                fill = 0;
            }
        }
    }
    std::string hex() {
        Uint64 bits = total * 8;
        Uint8 pad = 0x80, zero = 0, len[8];
        char out[41];
        add(&pad, 1);
        while (fill != 56) {
            add(&zero, 1);
        }
        for (int i = 0; i < 8; i++) {
            len[i] = (Uint8)(bits >> (56 - i * 8));
        }
        add(len, 8);
        for (int i = 0; i < 5; i++) {
            snprintf(out + i * 8, 9, "%08x", h[i]);
        }
        return out;
    }
};

// ------------------------------------------------------------------------------------------------- the disc image
struct IsoFile { std::string path; Uint64 offset, size; };

struct Iso {
    SDL_IOStream *io = NULL;
    std::vector<IsoFile> files;

    void read(Uint64 offset, void *buf, size_t n) {
        if (SDL_SeekIO(io, (Sint64)offset, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, buf, n) != n) {
            throw Stop{"The disc image could not be read to its end. It may be incomplete."};
        }
    }
    void walk(Uint32 lba, Uint32 size, const std::string &dir, int depth) {
        std::vector<Uint8> d(size);
        read((Uint64)lba * 2048, d.data(), size);
        for (size_t pos = 0; pos + 34 <= size;) {
            Uint8 len = d[pos];
            if (len == 0) { // records do not cross sectors: the rest of this one is empty
                pos = (pos / 2048 + 1) * 2048;
                continue;
            }
            if (pos + len > size) {
                break;
            }
            Uint32 ext, bytes;
            memcpy(&ext, &d[pos + 2], 4);
            memcpy(&bytes, &d[pos + 10], 4);
            Uint8 flags = d[pos + 25], nameLen = d[pos + 32];
            std::string name((const char *)&d[pos + 33], nameLen);
            pos += len;
            if (nameLen == 1 && (name[0] == 0 || name[0] == 1)) {
                continue; // this directory and its parent
            }
            size_t semi = name.find(';');
            if (semi != std::string::npos) {
                name.resize(semi);
            }
            if (!name.empty() && name.back() == '.') {
                name.pop_back();
            }
            if (flags & 2) {
                if (depth < 8) {
                    walk(ext, bytes, dir + name + "/", depth + 1);
                }
            } else if (!files.empty() && files.back().path == dir + name) {
                files.back().size += bytes; // a file in several extents: they follow each other
            } else {
                files.push_back({dir + name, (Uint64)ext * 2048, bytes});
            }
        }
    }
    void open(const std::string &path) {
        Uint8 pvd[2048];
        Uint32 lba, size;
        io = SDL_IOFromFile(path.c_str(), "rb");
        if (io == NULL) {
            throw Stop{"The disc image could not be opened: " + path};
        }
        if (SDL_SeekIO(io, 16 * 2048, SDL_IO_SEEK_SET) < 0 || SDL_ReadIO(io, pvd, sizeof(pvd)) != sizeof(pvd) || pvd[0] != 1 ||
            memcmp(pvd + 1, "CD001", 5) != 0) {
            throw Stop{"This file is not a disc image (.iso). Compressed images (.chd, .cso, .gz) have to be converted to .iso first."};
        }
        memcpy(&lba, pvd + 156 + 2, 4);
        memcpy(&size, pvd + 156 + 10, 4);
        walk(lba, size, "", 0);
    }
    const IsoFile *find(const char *path) {
        for (auto &f : files) {
            if (SDL_strcasecmp(f.path.c_str(), path) == 0) {
                return &f;
            }
        }
        return NULL;
    }
    ~Iso() {
        if (io != NULL) {
            SDL_CloseIO(io);
        }
    }
};

std::string lower(std::string s) {
    for (auto &c : s) {
        c = (char)tolower((unsigned char)c);
    }
    return s;
}

std::string upper(std::string s) {
    for (auto &c : s) {
        c = (char)toupper((unsigned char)c);
    }
    return s;
}

void check_cancel() {
    if (sCancel) {
        throw Stop{"INSTALL_CANCELLED"};
    }
}

void step(const char *title);
bool installed_language(const std::string &root,bool english);
std::string download_optional(const std::string &root,bool portuguese);
static std::string sha256_file(const std::string &path);

void ensure_directories(const std::string &path) {
    std::string normalized=path;
    for(char &c:normalized)if(c=='\\')c='/';
    std::string current;
    size_t pos=0;
    if(!normalized.empty()&&normalized[0]=='/') {current="/";pos=1;}
    else if(normalized.size()>=3&&normalized[1]==':'&&normalized[2]=='/') {current=normalized.substr(0,2);pos=3;}
    while(pos<=normalized.size()) {
        size_t end=normalized.find('/',pos);
        std::string part=normalized.substr(pos,end==std::string::npos?std::string::npos:end-pos);
        if(!part.empty()) {
            if(!current.empty()&&current.back()!='/')current+='/';
            current+=part;
            SDL_PathInfo info;
            if(!SDL_GetPathInfo(current.c_str(),&info)) {
                if(!SDL_CreateDirectory(current.c_str())&&!SDL_GetPathInfo(current.c_str(),&info))
                    throw Stop{"Could not create directory " + current + ". Is the disk full or the folder read-only?"};
            } else if(info.type!=SDL_PATHTYPE_DIRECTORY) {
                throw Stop{"A file blocks the required directory " + current + "."};
            }
        }
        if(end==std::string::npos)break;
        pos=end+1;
    }
}

std::string normalized_path(std::string path) {
    for(char &c:path) {
        if(c=='\\')c='/';
#ifdef _WIN32
        c=(char)tolower((unsigned char)c);
#endif
    }
    while(path.size()>1&&path.back()=='/')path.pop_back();
    return path;
}

bool valid_zip_relative_path(std::string &path, bool directory) {
    if(path.empty()||path.size()>500)return false;
    for(char &c:path) {
        if(c=='\\')c='/';
        if((unsigned char)c<32||c==':')return false;
    }
    if(path[0]=='/'||path.rfind("//",0)==0)return false;
    while(directory&&!path.empty()&&path.back()=='/')path.pop_back();
    if(path.empty())return directory;
    size_t start=0;
    while(start<=path.size()) {
        size_t end=path.find('/',start);
        std::string part=path.substr(start,end==std::string::npos?std::string::npos:end-start);
        if(part.empty()||part=="."||part=="..")return false;
        if(end==std::string::npos)break;
        start=end+1;
    }
    return true;
}

std::string unique_backup_path(const std::string &root,const std::string &relative,const std::string &folder) {
    std::string base=root+"tbo/setup/"+folder+"/"+relative;
    std::string parent=base.substr(0,base.find_last_of('/'));
    ensure_directories(parent);
    SDL_PathInfo info;
    if(!SDL_GetPathInfo(base.c_str(),&info))return base;
    for(int i=1;i<1000;i++) {
        std::string candidate=base+".backup-"+std::to_string(i);
        if(!SDL_GetPathInfo(candidate.c_str(),&info))return candidate;
    }
    throw Stop{"Could not choose a safe backup path for " + relative + "."};
}

struct ZipReader {
    mz_zip_archive zip{};
    bool open=false;
    explicit ZipReader(const std::string &path,Uint64 offset=0,Uint64 size=0) {
        mz_zip_zero_struct(&zip);
        open=(offset?mz_zip_reader_init_file_v2(&zip,path.c_str(),0,offset,size):mz_zip_reader_init_file(&zip,path.c_str(),0))!=0;
    }
    ~ZipReader(){if(open)mz_zip_reader_end(&zip);}
    ZipReader(const ZipReader&)=delete;
    ZipReader& operator=(const ZipReader&)=delete;
};

struct ZipEntry { mz_zip_archive_file_stat stat{}; std::string path; };

void validate_texture_archive(const std::string &path) {
    SDL_PathInfo fi;
    if(!SDL_GetPathInfo(path.c_str(),&fi)||fi.type!=SDL_PATHTYPE_FILE||fi.size!=kTexturePackage.bytes||
       sha256_file(path)!=kTexturePackage.sha256)
        throw Stop{"TEXTURE_PACK_INVALID"};
    ZipReader reader(path);
    if(!reader.open)throw Stop{"TEXTURE_PACK_INVALID"};
    mz_uint count=mz_zip_reader_get_num_files(&reader.zip);
    if(count<BT4_TEXTURE_FILES||count>BT4_TEXTURE_FILES+1024)throw Stop{"TEXTURE_PACK_INVALID"};
    mz_uint files=0,directories=0,dds=0;Uint64 total=0;
    const std::string prefix=std::string(BT4_TEXTURE_ROOT)+"/";
    for(mz_uint i=0;i<count;i++) {
        mz_zip_archive_file_stat st{};
        if(mz_zip_reader_get_filename(&reader.zip,i,NULL,0)>sizeof(st.m_filename)-1||
           !mz_zip_reader_file_stat(&reader.zip,i,&st)||!st.m_is_supported||st.m_is_encrypted)
            throw Stop{"TEXTURE_PACK_INVALID"};
        std::string name=st.m_filename;
        if(!valid_zip_relative_path(name,st.m_is_directory)||
           (name!=BT4_TEXTURE_ROOT&&name.rfind(prefix,0)!=0))
            throw Stop{"TEXTURE_PACK_INVALID"};
        if(st.m_is_directory){
            if(st.m_uncomp_size!=0||++directories>1024)throw Stop{"TEXTURE_PACK_INVALID"};
            continue;
        }
        ++files;
        size_t dot=name.find_last_of('.');
        std::string ext=dot==std::string::npos?"":name.substr(dot);
        for(char &c:ext)c=(char)tolower((unsigned char)c);
        if(ext==".dds")dds++;
        else throw Stop{"TEXTURE_PACK_INVALID"};
        if(st.m_uncomp_size>50000000ULL||st.m_uncomp_size>BT4_TEXTURE_UNPACKED_BYTES||total>BT4_TEXTURE_UNPACKED_BYTES-st.m_uncomp_size)
            throw Stop{"TEXTURE_PACK_INVALID"};
        total+=st.m_uncomp_size;
    }
    if(files!=BT4_TEXTURE_FILES||dds!=BT4_TEXTURE_FILES||total!=BT4_TEXTURE_UNPACKED_BYTES)throw Stop{"TEXTURE_PACK_INVALID"};
}

void retire_legacy_texture_pack(const std::string &root) {
    const std::string path=root+"textures/"+BT4_TEXTURE_LEGACY_ARCHIVE;
    SDL_PathInfo info;
    if(!SDL_GetPathInfo(path.c_str(),&info)||info.type!=SDL_PATHTYPE_FILE||info.size!=BT4_TEXTURE_LEGACY_BYTES)return;
    // Never remove a customized ZIP just because it has the old pack's name.
    if(sha256_file(path)!=BT4_TEXTURE_LEGACY_SHA256)return;
    if(SDL_RemovePath(path.c_str()))say("@note Superseded HD pack removed after verifying the new installation");
}

void install_texture_pack(const std::string &root,std::string source) {
    step("Texture pack");
    bool downloaded=source=="@download";
    if(downloaded)source=download_optional(root,false);
    say("@activity texture-copy");
    validate_texture_archive(source);
    const std::string relative=kTexturePackage.name;
    const std::string destination=root+"textures/"+relative;
    SDL_PathInfo sourceInfo,destInfo;
    if(!SDL_GetPathInfo(source.c_str(),&sourceInfo)||sourceInfo.type!=SDL_PATHTYPE_FILE)
        throw Stop{"TEXTURE_PACK_INVALID"};
    if(normalized_path(source)==normalized_path(destination)) {
        say("@progress "+std::to_string(sourceInfo.size)+" "+std::to_string(sourceInfo.size)+" 1 1");
        retire_legacy_texture_pack(root);
        say("@ok HD texture pack is already in textures");
        return;
    }
    bool destinationExists=SDL_GetPathInfo(destination.c_str(),&destInfo);
    if(destinationExists&&destInfo.type!=SDL_PATHTYPE_FILE)
        throw Stop{"A non-file already exists at the HD texture pack destination."};
    if(destinationExists&&destInfo.size==sourceInfo.size) {
        try {
            validate_texture_archive(destination);
            say("@progress "+std::to_string(sourceInfo.size)+" "+std::to_string(sourceInfo.size)+" 1 1");
            retire_legacy_texture_pack(root);
            say("@skip BT4 HD v1.0 Light is already installed");
            if(downloaded)SDL_RemovePath(source.c_str());
            return;
        } catch(const Stop &) {
            // The destination has the expected size but is damaged; replace it from the validated source.
        }
    }
    ensure_directories(root+"textures");
    if(downloaded){
        /* Cache and textures share an install root. Move the verified archive
           atomically to avoid needing another 2.54 GB of free space. */
        std::string previous;
        try{
            if(destinationExists){previous=unique_backup_path(root,"textures/"+relative,"texture-pack-v1.0-light-originals");
                if(!SDL_RenamePath(destination.c_str(),previous.c_str()))throw Stop{"Could not preserve the existing HD pack."};}
            if(!SDL_RenamePath(source.c_str(),destination.c_str()))throw Stop{"Could not finish installing the texture pack. Check available disk space."};
        }catch(...){if(!previous.empty())SDL_RenamePath(previous.c_str(),destination.c_str());throw;}
        say("@progress "+std::to_string(sourceInfo.size)+" "+std::to_string(sourceInfo.size)+" 1 1");
        retire_legacy_texture_pack(root);
        say("@ok BT4 HD v1.0 Light installed");return;
    }
    const std::string temp=destination+".part";
    SDL_IOStream *in=SDL_IOFromFile(source.c_str(),"rb");
    SDL_IOStream *out=SDL_IOFromFile(temp.c_str(),"wb");
    if(!in||!out) {
        if(in)SDL_CloseIO(in);if(out)SDL_CloseIO(out);SDL_RemovePath(temp.c_str());
        throw Stop{"Could not open the texture pack or create its destination. Check disk space and permissions."};
    }
    std::vector<Uint8> buffer(1024*1024);
    Uint64 copied=0,lastPct=0;bool ioOk=true;
    try {
        while(copied<sourceInfo.size) {
            size_t n=(size_t)std::min<Uint64>(buffer.size(),sourceInfo.size-copied);
            size_t got=SDL_ReadIO(in,buffer.data(),n);
            if(got!=n||SDL_WriteIO(out,buffer.data(),n)!=n){ioOk=false;break;}
            copied+=n;check_cancel();
            Uint64 pct=copied*100/sourceInfo.size;
            if(pct!=lastPct){lastPct=pct;say("@progress "+std::to_string(copied)+" "+std::to_string(sourceInfo.size)+" 1 1");}
        }
    } catch(...) {SDL_CloseIO(in);SDL_CloseIO(out);SDL_RemovePath(temp.c_str());throw;}
    bool closeIn=SDL_CloseIO(in),closeOut=SDL_CloseIO(out);
    if(!ioOk||!closeIn||!closeOut){SDL_RemovePath(temp.c_str());throw Stop{"Could not copy the HD texture pack. Check available disk space."};}
    std::string backup;
    if(destinationExists) {
        backup=unique_backup_path(root,relative,"texture-pack-v1.0-light-originals");
        if(!SDL_RenamePath(destination.c_str(),backup.c_str())){SDL_RemovePath(temp.c_str());throw Stop{"Could not preserve the existing texture pack before replacing it."};}
    }
    if(!SDL_RenamePath(temp.c_str(),destination.c_str())) {
        if(!backup.empty())SDL_RenamePath(backup.c_str(),destination.c_str());
        SDL_RemovePath(temp.c_str());
        throw Stop{"Could not finish installing the texture pack. Check available disk space."};
    }
    say("@progress "+std::to_string(sourceInfo.size)+" "+std::to_string(sourceInfo.size)+" 1 1");
    retire_legacy_texture_pack(root);
    say("@ok BT4 HD v1.0 Light copied to textures");
}

bool bytes_match_file(const std::string &path,const void *data,size_t size) {
    SDL_PathInfo info;
    if(!SDL_GetPathInfo(path.c_str(),&info)||info.type!=SDL_PATHTYPE_FILE||info.size!=size)return false;
    size_t oldSize=0;void *old=SDL_LoadFile(path.c_str(),&oldSize);
    if(!old)return false;
    bool same=oldSize==size&&(!size||memcmp(old,data,size)==0);
    SDL_free(old);return same;
}

bool allowed_ptbr_path(const std::string &path,bool directory) {
    std::string p=path;for(char &c:p)c=(char)tolower((unsigned char)c);
    if(p=="ptbr"||p.rfind("ptbr/",0)==0||p=="fonts"||p.rfind("fonts/",0)==0||
       p=="gamedata"||p=="gamedata/game-text"||p.rfind("gamedata/game-text/",0)==0||
       p=="textures"||p=="textures/ptbr"||p.rfind("textures/ptbr/",0)==0)return true;
    if(directory)return false;
    return p=="jogar em portugues.cmd"||p=="escolher idioma.cmd"||p=="leia-me - portugues.txt";
}

void install_ptbr_pack(const std::string &root,std::string source) {
    step("Portuguese (Brazil) language pack");
    if(source=="@download")source=download_optional(root,true);
    say("@activity ptbr-unpack");
    if(!installed_language(root,true))throw Stop{"PTBR_REQUIRES_ENGLISH"};
    SDL_PathInfo fi;
    if(!SDL_GetPathInfo(source.c_str(),&fi)||fi.type!=SDL_PATHTYPE_FILE||fi.size>120000000ULL)
        throw Stop{"PTBR_ARCHIVE_INVALID"};
    const std::string marker=root+"tbo/setup/PTBR_v1.2.native-installed";
    Uint64 archiveOffset=0,archiveSize=0;
    { Uint8 header[64];SDL_IOStream *file=SDL_IOFromFile(source.c_str(),"rb");
      if(!file)throw Stop{"PTBR_ARCHIVE_INVALID"};
      size_t count=SDL_ReadIO(file,header,sizeof(header));SDL_CloseIO(file);
      if(count>=8&&!memcmp(header,"BT4PTP01",8)){
          const Uint8 expected[8]={1,0,0,0,64,0,0,0};
          if(count!=64||memcmp(header+8,expected,8)||fi.size!=kPortuguesePackage.bytes||sha256_file(source)!=kPortuguesePackage.sha256)
              throw Stop{"PTBR_ARCHIVE_INVALID"};
          for(int i=0;i<8;++i)archiveSize|=(Uint64)header[16+i]<<(i*8);
          archiveOffset=64;if(archiveSize!=fi.size-64)throw Stop{"PTBR_ARCHIVE_INVALID"};
      }else if(source.size()>=8&&source.substr(source.size()-8)==".bt4lang")throw Stop{"PTBR_ARCHIVE_INVALID"};
    }
    ZipReader reader(source,archiveOffset,archiveSize);
    if(!reader.open)throw Stop{"PTBR_ARCHIVE_INVALID"};
    say("@activity ptbr-unpack");
    mz_uint count=mz_zip_reader_get_num_files(&reader.zip);
    if(count<500||count>1000)throw Stop{"PTBR_ARCHIVE_INVALID"};
    std::vector<ZipEntry> entries;entries.reserve(count);
    Uint64 total=0;unsigned fileCount=0,ptbrData=0,fonts=0,languageText=0,ptbrTextures=0;
    for(mz_uint i=0;i<count;i++) {
        ZipEntry entry;
        if(mz_zip_reader_get_filename(&reader.zip,i,NULL,0)>sizeof(entry.stat.m_filename)-1||
           !mz_zip_reader_file_stat(&reader.zip,i,&entry.stat)||entry.stat.m_is_encrypted||!entry.stat.m_is_supported)
            throw Stop{"PTBR_ARCHIVE_INVALID"};
        entry.path=entry.stat.m_filename;
        if(!valid_zip_relative_path(entry.path,entry.stat.m_is_directory))throw Stop{"PTBR_ARCHIVE_INVALID"};
        if(!allowed_ptbr_path(entry.path,entry.stat.m_is_directory))throw Stop{"PTBR_ARCHIVE_INVALID"};
        if(!entry.stat.m_is_directory) {
            if((entry.stat.m_version_made_by>>8)==3&&(((entry.stat.m_external_attr>>16)&0170000)==0120000))
                throw Stop{"PTBR_ARCHIVE_INVALID"};
            if(entry.stat.m_uncomp_size>32000000ULL||total>250000000ULL-entry.stat.m_uncomp_size)
                throw Stop{"PTBR_ARCHIVE_INVALID"};
            total+=entry.stat.m_uncomp_size;fileCount++;
            std::string lower=entry.path;for(char &c:lower)c=(char)tolower((unsigned char)c);
            if(lower.rfind("ptbr/",0)==0)ptbrData++;
            if(lower.rfind("fonts/",0)==0||lower.rfind("ptbr/fonts/",0)==0)fonts++;
            if(lower.rfind("gamedata/game-text/",0)==0||lower.rfind("ptbr/game-text/",0)==0)languageText++;
            if(lower.rfind("textures/ptbr/",0)==0||lower.rfind("ptbr/textures/",0)==0)ptbrTextures++;
        }
        entries.push_back(entry);
    }
    if(fileCount<500||total<60000000ULL||ptbrData<200||fonts<5||languageText<1||ptbrTextures<200)
        throw Stop{"PTBR_ARCHIVE_INVALID"};

    struct Changed {std::string destination,backup;bool hadOriginal=false;};
    std::vector<Changed> changed;changed.reserve(fileCount);
    Uint64 completed=0;unsigned processed=0;bool packageOk=false;
    try {
        for(mz_uint i=0;i<count;i++) {
            const ZipEntry &entry=entries[i];
            if(entry.stat.m_is_directory) {
                std::string dir=root+entry.path;
                ensure_directories(dir);
                continue;
            }
            check_cancel();
            std::string output=entry.path;
            if(output.rfind("fonts/",0)==0)output="PTBR/"+output;
            else if(output.rfind("textures/PTBR/",0)==0)output="PTBR/textures/"+output.substr(14);
            else if(output!="PTBR/fontes-sem-conflito.txt"&&output.rfind("PTBR/gamedata/mods/",0)!=0&&output.rfind("PTBR/game-text/",0)!=0&&output.rfind("PTBR/fonts/",0)!=0&&output.rfind("PTBR/textures/",0)!=0)continue;
            std::string destination=root+output;
            size_t slash=output.find_last_of('/');
            std::string parent=slash==std::string::npos?root:root+output.substr(0,slash);
            ensure_directories(parent);
            size_t bytes=0;void *data=mz_zip_reader_extract_to_heap(&reader.zip,i,&bytes,0);
            if(!data&&entry.stat.m_uncomp_size!=0)throw Stop{"PTBR_ARCHIVE_INVALID"};
            if(bytes!=entry.stat.m_uncomp_size){if(data)mz_free(data);throw Stop{"PTBR_ARCHIVE_INVALID"};}
            if(bytes_match_file(destination,data,bytes)) {
                if(data)mz_free(data);
            } else {
                std::string temp=destination+".ptbr-v1.2.part";
                if(!SDL_SaveFile(temp.c_str(),data,bytes)) {
                    if(data)mz_free(data);SDL_RemovePath(temp.c_str());
                    throw Stop{"Could not write Portuguese language data. Check disk space and permissions."};
                }
                if(data)mz_free(data);
                Changed change;change.destination=destination;
                SDL_PathInfo oldInfo;
                if(SDL_GetPathInfo(destination.c_str(),&oldInfo)) {
                    if(oldInfo.type!=SDL_PATHTYPE_FILE){SDL_RemovePath(temp.c_str());throw Stop{"A folder blocks Portuguese language data at " + destination + "."};}
                    std::string relative=output;
                    change.backup=unique_backup_path(root,relative,"ptbr-v1.2-originals");
                    if(!SDL_RenamePath(destination.c_str(),change.backup.c_str())) {
                        SDL_RemovePath(temp.c_str());throw Stop{"Could not preserve an existing file before installing Portuguese language data."};
                    }
                    change.hadOriginal=true;
                }
                if(!SDL_RenamePath(temp.c_str(),destination.c_str())) {
                    if(change.hadOriginal)SDL_RenamePath(change.backup.c_str(),destination.c_str());
                    SDL_RemovePath(temp.c_str());throw Stop{"Could not finish writing Portuguese language data."};
                }
                changed.push_back(change);
            }
            completed+=entry.stat.m_uncomp_size;processed++;
            char line[160];snprintf(line,sizeof(line),"@progress %llu %llu %u %u",
                (unsigned long long)completed,(unsigned long long)total,processed,fileCount);say(line);
        }
        ensure_directories(root+"tbo/setup");
        const char *mark="Português (Brasil) v1.2\nRequires BT4 B14 Rev2 English data. Select Portuguese in the game language settings.\n";
        if(!SDL_SaveFile(marker.c_str(),mark,strlen(mark)))throw Stop{"Could not save the Portuguese language pack installation marker."};
        packageOk=true;
    } catch(...) {
        if(!packageOk) {
            for(auto it=changed.rbegin();it!=changed.rend();++it) {
                SDL_RemovePath(it->destination.c_str());
                if(it->hadOriginal)SDL_RenamePath(it->backup.c_str(),it->destination.c_str());
            }
        }
        throw;
    }
    say("@progress "+std::to_string(total)+" "+std::to_string(total)+" "+std::to_string(fileCount)+" "+std::to_string(fileCount));
    say("@ok Português (Brasil) v1.2 installed; select Portuguese in the game language settings");
}

// Copies `size` bytes of the image at `offset` into a new file.
void copy_out(Iso &iso, Uint64 offset, Uint64 size, const std::string &path, std::vector<Uint8> &buf) {
    SDL_IOStream *out = SDL_IOFromFile(path.c_str(), "wb");
    if (out == NULL) {
        throw Stop{"Could not write " + path + ". Is the disk full or the folder read-only?"};
    }
    while (size > 0) {
        size_t n = size < buf.size() ? (size_t)size : buf.size();
        iso.read(offset, buf.data(), n);
        if (SDL_WriteIO(out, buf.data(), n) != n) {
            SDL_CloseIO(out);
            throw Stop{"Could not write " + path + ". Is the disk full?"};
        }
        offset += n;
        size -= n;
    }
    if (!SDL_CloseIO(out)) {
        throw Stop{"Could not write " + path + ". Is the disk full?"};
    }
}

// "@step k n Title": the steps of this run are numbered as the window lists them (setup.cpp, reset_rows).
int sStep, sSteps;
void step(const char *title) {
    char line[96];
    snprintf(line, sizeof(line), "@step %d %d %s", ++sStep, sSteps, title);
    say(line);
}

void step_disc(Iso &iso, const std::string &path) {
    step("Disc image");
    iso.open(path);
    const IsoFile *slus = iso.find("SLUS_219.78"), *dbzp = iso.find("BIN/DBZP.BIN");
    if (slus == NULL || dbzp == NULL) {
        throw Stop{"Esta ISO no corresponde al perfil de Team BT4 B14 Rev2 (falta SLUS_219.78).\n"
                   "Other regions and the Wii version have different programs and are not supported."};
    }
    say("@note checking");
    if (slus->size < 0x34 || slus->size > 16*1024*1024 || dbzp->size > 16*1024*1024) {
        throw Stop{"Unsupported BT4 executable size."};
    }
    std::vector<Uint8> elf((size_t)slus->size), rom(kRomEnd - kRomBase, 0), menu((size_t)dbzp->size);
    iso.read(slus->offset, elf.data(), elf.size());
    iso.read(dbzp->offset, menu.data(), menu.size());
    if (elf.size() > 0x34 && memcmp(elf.data(), "\177ELF", 4) == 0) {
        Uint32 shoff;
        Uint16 shentsize, shnum;
        memcpy(&shoff, &elf[0x20], 4);
        memcpy(&shentsize, &elf[0x2E], 2);
        memcpy(&shnum, &elf[0x30], 2);
        if (shentsize < 24 || (Uint64)shoff + (Uint64)shnum * shentsize > elf.size()) {
            throw Stop{"Invalid BT4 executable section table."};
        }
        for (Uint32 i = 0; i < shnum && (size_t)shoff + (size_t)(i + 1) * shentsize <= elf.size(); i++) {
            Uint32 sh[6]; // name, type, flags, address, offset, size
            memcpy(sh, &elf[shoff + (size_t)i * shentsize], sizeof(sh));
            if ((sh[2] & 2) && sh[1] != 8 && sh[5] != 0 && sh[3] >= kRomBase && (Uint64)sh[3] + sh[5] <= kRomEnd && (Uint64)sh[4] + sh[5] <= elf.size()) {
                memcpy(&rom[sh[3] - kRomBase], &elf[sh[4]], sh[5]);
            }
        }
    }
    Sha1 a, b;
    a.add(rom.data(), rom.size());
    b.add(menu.data(), menu.size());
    const std::string executable = a.hex(), overlay = b.hex();
    sEnglish = executable == "a4776fbadd2d266f42cf559a3530c2e6454d01a3" && overlay == "1d9f8da80c200bb50e7df3210fff30827e5dae9c";
    if (!sEnglish && (executable != kRomSha1 || overlay != kDbzpSha1)) {
        throw Stop{"Unsupported BT4 disc profile. Use the LATINO or English B14 Rev2 ISO.\n"
                   "Both executable and overlay must match one of the verified profiles."};
    }
    sVerify = sEnglish ? kVerifyEng : kVerify;
    sVerifyCount = sEnglish ? kVerifyEngCount : kVerifyCount;
    sPrograms[0] = std::move(rom); sPrograms[1] = std::move(menu);
    say(sEnglish ? "@ok BT4 B14 Rev2 English: verified programs" : "@ok BT4 B14 Rev2 LATINO: verified programs");
}


using ByteCallback = std::function<void(Uint64)>;
// Throttled structured events keep rendering responsive without queuing one
// message per small file. The forced final event follows CRC and atomic rename.
struct ExtractionProgress {
    Uint64 completed=0,total=0,last=0; int files=0,count=0;
    void emit(Uint64 partial=0,bool force=false) {
        Uint64 now=SDL_GetTicks();
        if(!force && now-last<100)return;
        last=now; char line[160];
        snprintf(line,sizeof(line),"@progress %llu %llu %d %d",
                 (unsigned long long)(completed+partial),(unsigned long long)total,files,count);
        say(line);
    }
};
Uint32 file_crc(const std::string &path, Uint64 size, std::vector<Uint8> &buf,
                const ByteCallback &progress={}) {
    SDL_IOStream *f = SDL_IOFromFile(path.c_str(), "rb");
    if (!f) return ~0u;
    if (SDL_GetIOSize(f) != (Sint64)size) { SDL_CloseIO(f); return ~0u; }
    struct Close { SDL_IOStream *file; ~Close(){SDL_CloseIO(file);} } close{f};
    Uint32 crc = 0; Uint64 read=0;
    while (size) {
        size_t n = size < buf.size() ? (size_t)size : buf.size();
        if (SDL_ReadIO(f, buf.data(), n) != n) return ~0u;
        crc = SDL_crc32(crc, buf.data(), n); size -= n; read+=n;
        if(progress)progress(read);check_cancel();
    }
    return crc;
}

// Link only after the source matches the selected disc's size and CRC.
// This keeps shared assets in one physical copy without mixing differing files.
bool share_file(const std::string &source,const std::string &destination) {
#ifdef _WIN32
    int a=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,source.c_str(),-1,NULL,0);
    int b=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,destination.c_str(),-1,NULL,0);
    if(a<1||b<1)return false;
    std::vector<WCHAR> src(a),dst(b);
    MultiByteToWideChar(CP_UTF8,0,source.c_str(),-1,src.data(),a);
    MultiByteToWideChar(CP_UTF8,0,destination.c_str(),-1,dst.data(),b);
    return CreateHardLinkW(dst.data(),src.data(),NULL)!=0;
#else
    return link(source.c_str(),destination.c_str())==0;
#endif
}
void select_data_root(const std::string &root) {
    std::string base=root+"gamedata/";size_t bytes=0;
    char *tag=(char *)SDL_LoadFile((base+".bt4-language").c_str(),&bytes);
    bool baseExists=SDL_GetPathInfo((base+"disc/SLUS_219.78").c_str(),NULL);
    bool baseEnglish=tag&&bytes>=2&&!strncmp(tag,"en",2);SDL_free(tag);
    if(baseExists&&!SDL_GetPathInfo((base+".bt4-language").c_str(),NULL)) {
        tag=(char *)SDL_LoadFile((base+".installed-bt4-b14rev2").c_str(),&bytes);
        baseEnglish=tag&&std::string(tag,bytes).find("ENG")!=std::string::npos;SDL_free(tag);
    }
    sDataRoot=base;sDedupeRoot.clear();
    if(baseExists&&sEnglish!=baseEnglish){
        sDataRoot=base+"languages/"+(sEnglish?"en/":"es/");sDedupeRoot=base;
    }
    SDL_CreateDirectory(sDataRoot.c_str());
    const char *locale=sEnglish?"en\n":"es\n";
    if(!SDL_SaveFile((sDataRoot+".bt4-language").c_str(),locale,3))throw Stop{"Could not save the language profile."};
}

void checked_out(Iso &iso, Uint64 off, Uint64 size, const std::string &path,
                 unsigned expectedSize, unsigned expectedCrc, std::vector<Uint8> &buf,
                 const ByteCallback &progress={}) {
    if (size != expectedSize) throw Stop{"Tamano incompatible en " + path};
    if (file_crc(path, size, buf, progress) == expectedCrc) return;
    std::string temp = path + ".part";
    if(!sDedupeRoot.empty() && path.compare(0,sDataRoot.size(),sDataRoot)==0) {
        std::string shared=sDedupeRoot+path.substr(sDataRoot.size());
        if(file_crc(shared,expectedSize,buf,progress)==expectedCrc) {
            SDL_RemovePath(temp.c_str());
            if(share_file(shared,temp)) {
                if(!SDL_RenamePath(temp.c_str(),path.c_str()))throw Stop{"Could not finalize the shared language asset."};
                return;
            }
        }
    }
    SDL_IOStream *f = SDL_IOFromFile(temp.c_str(), "wb");
    if (!f) throw Stop{"No se puede escribir en " + temp};
    Uint32 crc = 0;
    try {
        while (size) {
            size_t n = size < buf.size() ? (size_t)size : buf.size();
            iso.read(off, buf.data(), n);
            crc = SDL_crc32(crc, buf.data(), n);
            if (SDL_WriteIO(f, buf.data(), n) != n) throw Stop{"No se pudo guardar " + temp};
            off += n; size -= n; if(progress)progress(expectedSize-size);check_cancel();
        }
    } catch (...) { SDL_CloseIO(f); throw; }
    if (!SDL_CloseIO(f)) throw Stop{"No se pudo cerrar " + temp};
    if (crc != expectedCrc) throw Stop{"Los datos de la ISO no coinciden con BT4 B14 Rev2: " + path};
    if (!SDL_RenamePath(temp.c_str(), path.c_str())) throw Stop{"No se pudo finalizar " + path};
}

void step_data(Iso &iso, const std::string &root) {
    step("Game data");
    const IsoFile *index = iso.find("BIN/DBZ4.BIN");
    if (!index || index->size > 16*1024*1024) throw Stop{"Falta el indice DBZ4.BIN de BT4."};
    std::vector<Uint8> db((size_t)index->size), buf(1024*1024);
    iso.read(index->offset, db.data(), db.size());
    Sha1 hash; hash.add(db.data(), db.size());
    const char *indexHash = sEnglish ? "145837274421c39ae7eedc3fa1b697d4212541c2" : "989ee93cbd31508717bc245793c1e89c50109e0f";
    if (hash.hex() != indexHash) throw Stop{"DBZ4.BIN does not match the selected BT4 B14 Rev2 profile."};
    std::string data = sDataRoot.empty() ? root + "gamedata/" : sDataRoot;
    SDL_CreateDirectory(data.c_str());
    int entry = 0;
    ExtractionProgress progress;
    progress.count=sVerifyCount[0]+sVerifyCount[1]+sVerifyCount[2]+sVerifyCount[3]+sizeof(kVerifyLoose)/sizeof(kVerifyLoose[0]);
    for(int i=0;i<progress.count;++i)progress.total+=sVerify[i][0];
    say("@activity extract");progress.emit(0,true);
    auto extract = [&](Uint64 offset,Uint64 bytes,const std::string &path) {
        Uint64 high=0;
        checked_out(iso,offset,bytes,path,sVerify[entry][0],sVerify[entry][1],buf,[&](Uint64 done){
            // A failed cached CRC may require a rewrite. Keep the display
            // monotonic, but credit the final byte only after verification.
            high=std::max(high,std::min(done,bytes?bytes-1:0));progress.emit(high);
        });
        progress.completed+=bytes;++progress.files;
        progress.emit(0,progress.files==progress.count);check_cancel();
    };
    for (int part=0; part<4; part++) {
        char upperName[64], lowerName[32];
        snprintf(upperName,sizeof(upperName),"DATA/PZS4US%d.AFS",part);
        snprintf(lowerName,sizeof(lowerName),"pzs4us%d.afs",part);
        const IsoFile *afs=iso.find(upperName);
        if (!afs) throw Stop{"Falta " + std::string(upperName)};
        auto found=std::search(db.begin(),db.end(),lowerName,lowerName+strlen(lowerName)+1);
        if (found==db.end() || found-db.begin()<16) throw Stop{"Indice BT4 incompleto."};
        size_t at=(size_t)(found-db.begin())-16;
        Uint32 header[4]; memcpy(header,db.data()+at,16);
        Uint32 count=header[2];
        if (header[0]!=0 || count!=(Uint32)sVerifyCount[part] || header[3]!=count || header[1]!=0x11A+count*2 || at+header[1]>db.size())
            throw Stop{"Tabla de sectores BT4 incompatible."};
        std::string dir=data+"pzs4us"+std::to_string(part)+"/";
        SDL_CreateDirectory(dir.c_str());
        Uint64 offset=0;
        for (Uint32 i=0;i<count;i++,entry++) {
            Uint16 sectors; memcpy(&sectors,db.data()+at+0x11A+i*2,2);
            Uint64 bytes=(Uint64)sectors*2048;
            if (offset+bytes>afs->size) {
                if (i!=count-1 || offset>=afs->size || offset+bytes-afs->size>=2048) throw Stop{"Contenedor BT4 truncado."};
                bytes=afs->size-offset;
            }
            char name[32]; snprintf(name,sizeof(name),"%05u.bin",i);
            extract(afs->offset+offset,bytes,dir+name);
            offset+=(Uint64)sectors*2048;
        }
    }
    for (const char *loose:kVerifyLoose) {
        const IsoFile *f=iso.find(loose+5);
        if (!f) throw Stop{"Falta " + std::string(loose+5)};
        std::string path=data+loose;
        SDL_CreateDirectory(path.substr(0,path.rfind('/')).c_str());
        extract(f->offset,f->size,path);
        entry++;
    }
    say("@ok " + std::to_string(progress.files) + " BT4 files extracted and verified with CRC-32");
}

// Recompute only the small native-table fingerprint, using the strictly verified
// programs. Destinations remain those generated for this exact linked executable.
void step_tables(const std::string &root) {
    step("Native data tables");
    size_t bytes = 0;
    Uint8 *loaded = (Uint8 *)SDL_LoadFile((root + "BT4decomp.dat").c_str(), &bytes);
    if (!loaded) throw Stop{"The matching native data table is missing."};
    std::vector<Uint8> table(loaded, loaded + bytes); SDL_free(loaded);
    Uint32 count = 0;
    if (bytes < 16 || memcmp(table.data(), "BT3D", 4)) throw Stop{"Invalid native data table."};
    memcpy(&count, table.data()+4, 4);
    if (count > 100000 || bytes != 16 + (size_t)count * 16) throw Stop{"Invalid native data table length."};
    Uint64 hash = 0xcbf29ce484222325ULL;
    for (Uint32 i=0; i<count; i++) {
        Uint32 record[4]; memcpy(record, table.data()+16+(size_t)i*16, 16);
        if (record[1]>1 || (Uint64)record[2]+record[3]>sPrograms[record[1]].size()) throw Stop{"Native data table exceeds the selected program."};
        for (Uint32 j=0; j<record[3]; j++) hash=(hash^sPrograms[record[1]][record[2]+j])*0x100000001b3ULL;
    }
    memcpy(table.data()+8, &hash, 8);
    std::string temporary = root + "BT4decomp.dat.part";
    if (!SDL_SaveFile(temporary.c_str(), table.data(), table.size()) ||
        !SDL_RenamePath(temporary.c_str(), (root+"BT4decomp.dat").c_str())) throw Stop{"Could not finalize the native data table."};
    say("@ok Native table matches the selected disc profile");
}

void step_test(const std::string &root) {
    std::string exe = root + kGame, saves = root + "gamedata/.selftest";
    size_t oldCrashSize=0;
    char *oldCrash=(char *)SDL_LoadFile((root+"bt3_crash.txt").c_str(),&oldCrashSize);
    std::string previousCrash=oldCrash?std::string(oldCrash,oldCrashSize):"";
    SDL_free(oldCrash);
    const char *args[2] = {exe.c_str(), NULL};
    step("Self-test");
    say("@note the game's demo fight, no window");
    SDL_Environment *env = SDL_CreateEnvironment(true);
    SDL_SetEnvironmentVariable(env, "BT3_DEMO", "1", true);
    SDL_SetEnvironmentVariable(env, "BT3_SETTINGS", "", true);
    SDL_SetEnvironmentVariable(env, "BT3_SAVES", saves.c_str(), true);
    SDL_SetEnvironmentVariable(env, "BT3_GS", "none", true);
    SDL_SetEnvironmentVariable(env, "BT3_NOSOUND", "1", true);
    SDL_SetEnvironmentVariable(env, "BT4_MUSIC_NO_CACHE", "1", true);
    SDL_SetEnvironmentVariable(env, "BT3_DATA", sDataRoot.c_str(), true); // no window: a release program opens one by default
    for (const char *name : {"BT3_REPLAY", "BT3_PAD_PLAY", "BT3_PAD_REC", "BT3_A11Y_LOG"}) {
        SDL_UnsetEnvironmentVariable(env, name);
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)args);
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ENVIRONMENT_POINTER, env);
    SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, root.c_str());
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDIN_NUMBER, SDL_PROCESS_STDIO_NULL);
    SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_APP);
    SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    SDL_Process *proc = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_DestroyEnvironment(env);
    if (proc == NULL) {
#ifdef __APPLE__
        throw Stop{std::string("The game could not be started: ") + SDL_GetError() +
                   " (macOS may still be blocking it: see step 0 of README.txt)"};
#else
        throw Stop{std::string("The game could not be started: ") + SDL_GetError()};
#endif
    }
    size_t size = 0;
    int code = -1;
    char *out = (char *)SDL_ReadProcess(proc, &size, &code);
    std::string text = out != NULL ? std::string(out, size) : "";
    SDL_free(out);
    SDL_DestroyProcess(proc);
    // How the game ended goes into the log too: when it never got as far as printing anything (the system refused
    // to start it, or something stopped it), the exit code is all there is to go on.
    char ended[160];
    const char *why = "";
    switch ((unsigned)code) {
    case 0xC0000135u: why = " (a DLL the program needs was not found)"; break;
    case 0xC0000018u: why = " (the program could not be loaded at its fixed address)"; break;
    case 0xC0000022u: why = " (access denied: often security software blocking the program)"; break;
    case 0xC000007Bu: why = " (a DLL of the wrong kind was loaded)"; break;
    case 0xC0000005u: why = " (crashed: bad memory access)"; break;
    case 0xC0000409u: why = " (stopped by a security check)"; break;
    default: break;
    }
    snprintf(ended, sizeof(ended), "\n[setup] the game exited with code %d (0x%08X)%s\n", code, (unsigned)code, why);
    text += ended;
    {   // a crash report the game wrote itself
        size_t n = 0;
        char *crash = (char *)SDL_LoadFile((root + "bt3_crash.txt").c_str(), &n);
        if (crash != NULL) {
            if(std::string(crash,n)!=previousCrash)text += "[setup] bt3_crash.txt:\n" + std::string(crash, n);
            SDL_free(crash);
        }
    }
    SDL_CreateDirectory((root + "net_session/setup/").c_str());
    SDL_IOStream *log = SDL_IOFromFile((root + "net_session/setup/install.log").c_str(), "wb");
    if (log != NULL) {
        SDL_WriteIO(log, text.data(), text.size());
        SDL_CloseIO(log);
    }
    if (text.find("battle finished") == std::string::npos) {
        size_t end = text.find_last_not_of("\r\n"), begin = end == std::string::npos ? std::string::npos : text.rfind('\n', end);
        std::string last = end == std::string::npos ? "no output" : text.substr(begin == std::string::npos ? 0 : begin + 1, end - (begin == std::string::npos ? 0 : begin + 1) + 1);
        throw Stop{"The game's demo fight did not finish. Its last line was:\n" + last + "\nThe full output is in net_session/setup/install.log."};
    }
    say("@ok the demo fight ran to its end");
}

// The European release's archive entries: the battle HUD (5 to 9, one per language: English, Spanish, German,
// French, Italian) and the menu archives (USA 448 + k is 451 + k + 31 per language).
std::vector<Uint32> eu_entries() {
    std::vector<Uint32> out;
    for (Uint32 bank = 0; bank < 5; bank++) {
        out.push_back(5 + bank);
        for (Uint32 usa = 448; usa < 479; usa++) {
            out.push_back(451 + (usa - 448) + bank * 31);
        }
    }
    // Story dialogue: 50 episodes for each of the five European languages.
    for (Uint32 entry = 656; entry < 906; entry++) out.push_back(entry);
    return out;
}

void step_eu(const std::string &root, const std::string &path) {
    Iso iso;
    std::string dir = root + "gamedata/pzs3eu1/";
    step("European languages");
    iso.open(path);
    const IsoFile *sles = iso.find("SLES_549.45"), *afs = iso.find("DATA/PZS3EU1.AFS"), *selector = iso.find("DATA/PZS3EU0.AFS");
    const IsoFile *openingAudio = iso.find("DATA/ZS3EUOP.ADX");
    if (sles == NULL || afs == NULL || selector == NULL || openingAudio == NULL) {
        throw Stop{"This is not a disc image of the European release of Budokai Tenkaichi 3 (no SLES_549.45 on it).\n"
                   "The languages come from the European release (SLES-54945); the game itself is set up from the USA one."};
    }
    Uint8 head[8];
    Uint32 count;
    iso.read(afs->offset, head, 8);
    memcpy(&count, head + 4, 4);
    if (memcmp(head, "AFS\0", 4) != 0 || count == 0 || (Uint64)count * 8 + 8 > afs->size) {
        throw Stop{"An archive on the European disc image (DATA/PZS3EU1.AFS) is damaged."};
    }
    std::vector<Uint32> table((size_t)count * 2);
    iso.read(afs->offset + 8, table.data(), table.size() * 4);
    std::vector<Uint8> buf(1 << 20);
    std::vector<Uint32> want = eu_entries();
    SDL_CreateDirectory(dir.c_str());
    int done = 0;
    for (Uint32 e : want) {
        char name[32], line[64];
        if (e >= count || (Uint64)table[e * 2] + table[e * 2 + 1] > afs->size) {
            throw Stop{"An archive on the European disc image (DATA/PZS3EU1.AFS) is damaged or not the European release's."};
        }
        snprintf(name, sizeof(name), "%05u", e);
        copy_out(iso, afs->offset + table[e * 2], table[e * 2 + 1], dir + name + ".part", buf);
        if (!SDL_RenamePath((dir + name + ".part").c_str(), (dir + name + ".bin").c_str())) {
            throw Stop{"Could not write " + dir + name + ".bin."};
        }
        snprintf(line, sizeof(line), "@note %d of %d", ++done, (int)want.size());
        say(line);
        check_cancel();
    }
    iso.read(selector->offset, head, 8);
    memcpy(&count, head + 4, 4);
    if (memcmp(head, "AFS\0", 4) != 0 || count < 2 || selector->size < 24) {
        throw Stop{"The language selector archive on the European disc image (DATA/PZS3EU0.AFS) is damaged."};
    }
    Uint32 selectorEntry[2];
    iso.read(selector->offset + 16, selectorEntry, sizeof(selectorEntry));
    if (selectorEntry[1] == 0 || selectorEntry[1] > (1u << 20) ||
        (Uint64)selectorEntry[0] + selectorEntry[1] > selector->size) {
        throw Stop{"The language selector archive on the European disc image (DATA/PZS3EU0.AFS) is damaged."};
    }
    std::string selectorDir = root + "gamedata/pzs3eu0/";
    SDL_CreateDirectory(selectorDir.c_str());
    copy_out(iso, selector->offset + selectorEntry[0], selectorEntry[1], selectorDir + "00001.part", buf);
    if (!SDL_RenamePath((selectorDir + "00001.part").c_str(), (selectorDir + "00001.bin").c_str())) {
        throw Stop{"Could not write " + selectorDir + "00001.bin."};
    }
    std::string movieDir = root + "gamedata/disc/DATA/";
    SDL_CreateDirectory((root + "gamedata/disc/").c_str());
    SDL_CreateDirectory(movieDir.c_str());
    Uint8 magic[2];
    if (openingAudio->size < 2 || openingAudio->size > (16u << 20)) {
        throw Stop{"The European opening soundtrack has an invalid size."};
    }
    iso.read(openingAudio->offset, magic, sizeof(magic));
    if (magic[0] != 0x80 || magic[1] != 0) {
        throw Stop{"The European opening soundtrack has an invalid header."};
    }
    std::string dest = movieDir + "ZS3EUOP.ADX";
    copy_out(iso, openingAudio->offset, openingAudio->size, dest + ".part", buf);
    if (!SDL_RenamePath((dest + ".part").c_str(), dest.c_str())) {
        throw Stop{"Could not write " + dest + "."};
    }
    say("@ok European languages and opening soundtrack: the game makes the languages at its next start");
}

#include "private_packages.inc"
#include "language_install.inc"

std::string download_optional(const std::string &root,bool portuguese){
    const std::string folder=root+"tbo/setup/cache/";
    ensure_directories(folder);
    const std::string name=portuguese?kPortuguesePackage.name:kTexturePackage.name,path=folder+name;
    const char *digest=portuguese?kPortuguesePackage.sha256:kTexturePackage.sha256;
    const Uint64 bytes=portuguese?kPortuguesePackage.bytes:kTexturePackage.bytes;
    SDL_PathInfo info;
    if(SDL_GetPathInfo(path.c_str(),&info)&&info.size==bytes&&sha256_file(path)==digest)return path;
    say("@activity download");say("@package 0");
    if(portuguese){
        download_public_asset(root,kLanguageRelease,name,path,bytes);
    }else{
        const std::string temp=path+".part";SDL_IOStream *output=SDL_IOFromFile(temp.c_str(),"wb");
        if(!output)throw Stop{"Could not create the texture download. Check free disk space."};
        try{
            for(int part=1;part<=2;++part){
                check_cancel();const LanguagePackage &pieceInfo=kTexturePieces[part-1];
                const std::string piece=folder+pieceInfo.name;
                download_public_asset(root,kTextureRelease,pieceInfo.name,piece,pieceInfo.bytes,(part-1)*27,part==1?27:28);
                if(sha256_file(piece)!=pieceInfo.sha256){SDL_RemovePath(piece.c_str());throw Stop{"LANGUAGE_VERIFY_FAILED"};}
                SDL_IOStream *input=SDL_IOFromFile(piece.c_str(),"rb");if(!input)throw Stop{"LANGUAGE_DOWNLOAD_INCOMPLETE"};
                std::vector<Uint8> buffer(1024*1024);Sint64 remaining=SDL_GetIOSize(input);
                try{while(remaining>0){check_cancel();size_t n=(size_t)std::min<Sint64>(remaining,buffer.size());
                    if(SDL_ReadIO(input,buffer.data(),n)!=n||SDL_WriteIO(output,buffer.data(),n)!=n)throw Stop{"LANGUAGE_DOWNLOAD_INCOMPLETE"};remaining-=n;}}
                catch(...){SDL_CloseIO(input);throw;}
                SDL_CloseIO(input);SDL_RemovePath(piece.c_str());
            }
            bool ok=SDL_CloseIO(output);output=NULL;if(!ok)throw Stop{"LANGUAGE_DOWNLOAD_INCOMPLETE"};
            if(!SDL_RenamePath(temp.c_str(),path.c_str()))throw Stop{"LANGUAGE_DOWNLOAD_INCOMPLETE"};
        }catch(...){if(output)SDL_CloseIO(output);SDL_RemovePath(temp.c_str());throw;}
    }
    if(!SDL_GetPathInfo(path.c_str(),&info)||info.size!=bytes||sha256_file(path)!=digest){SDL_RemovePath(path.c_str());throw Stop{"LANGUAGE_VERIFY_FAILED"};}
    return path;
}


bool installed_language(const std::string &root,bool english){
    bool base=base_english(root);std::string data=root+"gamedata/"+(base==english?"":english?"languages/en/":"languages/es/");
    if(base!=english&&!SDL_GetPathInfo((data+".installed-bt4-b14rev2").c_str(),NULL))return false;
    const int *counts=english?kVerifyEngCount:kVerifyCount;
    const unsigned int (*verify)[2]=english?kVerifyEng:kVerify;
    int offset=counts[0]+counts[1]+counts[2]+counts[3],checked=0;std::vector<Uint8> buf(1024*1024);
    for(int i=0;i<(int)(sizeof(kVerifyLoose)/sizeof(kVerifyLoose[0]));++i){
        std::string relative=kVerifyLoose[i];
        if(relative=="disc/SLUS_219.78"||relative=="disc/BIN/DBZ4.BIN"||relative=="disc/BIN/DBZP.BIN"){
            if(file_crc(data+relative,verify[offset+i][0],buf)!=verify[offset+i][1])return false;++checked;
        }
    }
    return checked==3;
}
void work(std::string root, std::string path, std::string eu, std::string texturePack, std::string ptbrPack) {
    try {
        sStep=0;sSteps=(path.empty()?0:4)+((sInstallMask==3||!ptbrPack.empty())?2:0)+(!texturePack.empty()?1:0)+(!ptbrPack.empty()?1:0);
        if(!path.empty()){
            Iso iso;step_disc(iso,path);check_cancel();
            if(sInstallMask==0)sInstallMask=sEnglish?2:1;
            const bool portugueseCanAddEnglish=!ptbrPack.empty()&&sInstallMask==2&&!sEnglish;
            if(sInstallMask!=3&&sInstallMask!=(sEnglish?2:1)&&!portugueseCanAddEnglish)throw Stop{"LANGUAGE_ISO_MISMATCH"};
            select_data_root(root);step_data(iso,root);check_cancel();step_tables(root);check_cancel();step_test(root);
            const char *profile=sEnglish?"BT4 B14 Rev2 ENG; SLUS_219.78; native profile 7\n":"BT4 B14 Rev2 LATINO; SLUS_219.78; native profile 7\n";
            if(!SDL_SaveFile((sDataRoot+".installed-bt4-b14rev2").c_str(),profile,strlen(profile)))throw Stop{"INSTALL_STATE_FAILED"};
        }else if(!Native_Installed(root))throw Stop{"ISO_REQUIRED"};
        bool base=base_english(root);
        if(sInstallMask==3||!ptbrPack.empty()){
            const bool targetEnglish=sInstallMask==3?!base:true;
            if(!installed_language(root,targetEnglish))install_language(root,targetEnglish);
            else {
                step("Language package");say("@package 100");say("@skip verified language is installed");
                step("Self-test");say("@skip verified language needs no additional self-test");
            }
        }else if(!installed_language(root,sInstallMask==2))throw Stop{"ISO_REQUIRED"};
        if(!texturePack.empty())install_texture_pack(root,texturePack);
        if(!ptbrPack.empty())install_ptbr_pack(root,ptbrPack);
        if(texturePack=="@download")SDL_RemovePath((root+"tbo/setup/cache/"+kTexturePackage.name).c_str());
        if(ptbrPack=="@download")SDL_RemovePath((root+"tbo/setup/cache/"+kPortuguesePackage.name).c_str());
        say("@activity finalize");
        SDL_CreateDirectory((root+"tbo").c_str());
        std::string options=std::to_string(sInstallMask|((!ptbrPack.empty()||SDL_GetPathInfo((root+"tbo/setup/PTBR_v1.2.native-installed").c_str(),NULL))?4:0))+"\n";
        if(!ptbrPack.empty()){
            const char *settings="pt-BR";
            if(!SDL_SaveFile((root+"tbo/setup/selected-game-language").c_str(),settings,strlen(settings)))throw Stop{"INSTALL_STATE_FAILED"};
        }
        if(!SDL_SaveFile((root+"tbo/languages.ini").c_str(),options.data(),options.size()))throw Stop{"INSTALL_STATE_FAILED"};
        const char *identity=BT4_BUILD_IDENTITY;
        SDL_SaveFile((root+BT4_BUILD_MARKER).c_str(),identity,strlen(identity));
        say("@done "+root+kGame);
    }catch(const Stop &e){
        SDL_CreateDirectory((root+"tbo/setup").c_str());
        SDL_SaveFile((root+"tbo/setup/install.log").c_str(),e.why.data(),e.why.size());
        std::string why=e.why;for(size_t i=0;(i=why.find('\n',i))!=std::string::npos;)why.replace(i,1,"\\n");
        say("@fail");say("@stopped "+why);
    }catch(...){say("@fail");say("@stopped INSTALL_FAILED");}
    sRunning=false;
}

void texture_work(std::string root) {
    try {
        sStep=0;sSteps=1;
        // Use a verified installed archive directly; do not download it again.
        const std::string installed=root+"textures/"+BT4_TEXTURE_ARCHIVE;
        bool valid=false;
        try{validate_texture_archive(installed);valid=true;}catch(const Stop &){}
        install_texture_pack(root,valid?installed:"@download");
        say("@done "+root+kGame);
    }catch(const Stop &e){
        ensure_directories(root+"tbo/setup");
        SDL_SaveFile((root+"tbo/setup/install.log").c_str(),e.why.data(),e.why.size());
        std::string why=e.why;for(size_t i=0;(i=why.find('\n',i))!=std::string::npos;)why.replace(i,1,"\\n");
        say("@fail");say("@stopped "+why);
    }catch(...){say("@fail");say("@stopped INSTALL_FAILED");}
    sRunning=false;
}

} // namespace

void Native_Start(const std::string &root, const std::string &iso, const std::string &eu,int mask,
                  const std::string &texturePack,const std::string &ptbrPack) {
    if (sThread.joinable()) {
        sThread.join();
    }
    sInstallMask=mask>=0&&mask<=3?mask:1;
    sCancel = false;
    sRunning = true;
    sThread = std::thread(work, root, iso, eu, texturePack, ptbrPack);
}

void Native_StartTextures(const std::string &root) {
    if(sThread.joinable())sThread.join();
    {std::lock_guard<std::mutex> lock(sLock);sLines.clear();}
    sCancel=false;sRunning=true;
    sThread=std::thread(texture_work,root);
}

bool Native_Poll(std::string &line) {
    std::lock_guard<std::mutex> lock(sLock);
    if (sLines.empty()) {
        return false;
    }
    line = sLines.front();
    sLines.erase(sLines.begin());
    return true;
}

bool Native_Running() { return sRunning; }

void Native_Cancel() { sCancel = true; }

void Native_Join() {
    sCancel = true;
    if (sThread.joinable()) {
        sThread.join();
    }
}

bool Native_Installed(const std::string &root) {
    SDL_PathInfo info;
    if(!SDL_GetPathInfo((root+"gamedata/disc/SLUS_219.78").c_str(),&info))return false;
    if(SDL_GetPathInfo((root+"gamedata/.installed-bt4-b14rev2").c_str(),&info))return true;
    // Older portable installations can add English without extracting their ISO again.
    std::vector<Uint8> buf(1024*1024);
    for(int en=0;en<2;++en){
        const int *counts=en?kVerifyEngCount:kVerifyCount;
        const unsigned int (*verify)[2]=en?kVerifyEng:kVerify;
        int off=counts[0]+counts[1]+counts[2]+counts[3];
        if(file_crc(root+"gamedata/disc/SLUS_219.78",verify[off+7][0],buf)==verify[off+7][1]&&
           file_crc(root+"gamedata/disc/BIN/DBZP.BIN",verify[off+1][0],buf)==verify[off+1][1])return true;
    }
    return false;
}

bool Native_EuInstalled(const std::string &root) { return true; }
bool Native_BundledEnglish(const std::string &root) { return false; }
bool Native_EnglishInstalled(const std::string &root) { return english_installed(root); }
bool Native_SpanishInstalled(const std::string &root) { return installed_language(root,false); }
