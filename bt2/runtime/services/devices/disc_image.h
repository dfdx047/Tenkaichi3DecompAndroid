#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>
#if defined(__ANDROID__)
#include <sys/stat.h>
#include <unistd.h>
#endif

// The disc image the app opened for the engine comes as /proc/self/fd/<n>: it is read through that descriptor, where it
// lies, with pread (no shared position, so one reader does not hold up another). Opening the path again repeats the
// storage's permission check, which an app without storage permission fails for a document in Download/ (an "msf:"
// one). Elsewhere, and for a plain path, -1: the callers read it as a file.
inline int disc_image_fd(const std::string& path) {
#if defined(__ANDROID__)
    static const char prefix[]="/proc/self/fd/";
    if(path.compare(0,sizeof(prefix)-1,prefix)!=0)return -1;
    char* end=nullptr;
    const long fd=std::strtol(path.c_str()+sizeof(prefix)-1,&end,10);
    return end!=path.c_str()+sizeof(prefix)-1 && *end=='\0' && fd>=0?static_cast<int>(fd):-1;
#else
    (void)path;return -1;
#endif
}

inline bool disc_image_read(int fd,std::uint64_t offset,void* out,std::uint64_t size) {
#if defined(__ANDROID__)
    auto* at=static_cast<char*>(out);
    while(size>0) {
        const ssize_t n=pread(fd,at,static_cast<std::size_t>(size),static_cast<off_t>(offset));
        if(n<=0)return false;
        at+=n;offset+=static_cast<std::uint64_t>(n);size-=static_cast<std::uint64_t>(n);
    }
    return true;
#else
    (void)fd;(void)offset;(void)out;return size==0;
#endif
}

// The image's length and time (seconds): what tells one image from another.
inline bool disc_image_stat(int fd,std::uint64_t& length,long long& seconds) {
#if defined(__ANDROID__)
    struct stat st{};
    if(fstat(fd,&st)!=0)return false;
    length=static_cast<std::uint64_t>(st.st_size);seconds=static_cast<long long>(st.st_mtime);
    return true;
#else
    (void)fd;(void)length;(void)seconds;return false;
#endif
}
