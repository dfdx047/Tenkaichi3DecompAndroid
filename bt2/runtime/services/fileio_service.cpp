#include "ee/ee_runtime.h"
#include "services/devices/disc_metadata.h"
#include "services/devices/disc_sectors.h"
#include <array>
#include <cstring>
#include <fstream>
#include <string>

// Native FILEIO service (0x80000001): cdrom0: files read from the local ISO for the C library's
// file API. Protocol as the original boot FILEIO module answers it (traced 2026-10-11):
// - init (0xFF): the EE gives two result buffers; reply {0x30303033, 2}.
// - open/close/read/lseek: the RPC reply is 1 (accepted); the result arrives through an IOP->EE
//   command 0x80000011 whose payload lands in the two result buffers alternately:
//   {tag, command, request pointer, size word, result}. A read also carries the unaligned head
//   and tail bytes: {head size, tail size, head address, tail address, head[64], tail[64]}.
// The rest of the original payload buffer is stale IOP memory; it is zero here.

namespace {
constexpr std::uint32_t fileio_open=0,fileio_close=1,fileio_read=2,fileio_lseek=4,fileio_init=0xFF;
constexpr std::uint32_t fileio_command=0x80000011,fileio_handler=0x333378,fileio_handler_argument=0x3AC780;
constexpr std::uint32_t fileio_version=0x30303033;
}

bool EeRuntime::nativeFileioCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(local_disc_path.empty() || r.send_size>0x800 || r.recv_size!=(r.rpc_number==fileio_init?8u:4u))throw UnsupportedSyscall(0x77,context->pc);
    const auto in=[&](unsigned n){if(n*4+4>r.send_size)throw UnsupportedSyscall(0x77,context->pc);return read32(rdram,r.send_source+n*4);};
    if(r.rpc_number==fileio_init) {
        native_fileio_buffers_={in(0),in(1)};native_fileio_next_buffer_=0;
        write32(rdram,r.receive,fileio_version);write32(rdram,r.receive+4,2);
        completeNativeCall(rdram,context,r,server);finishNativeFileio(context);return true;
    }
    if(!native_fileio_buffers_[0])throw UnsupportedSyscall(0x77,context->pc); // Used before init.
    const std::uint32_t tag=in(0),request=in(1),size_word=in(2);
    std::uint32_t result=0;std::array<std::uint32_t,4> edges{};std::array<std::uint8_t,128> edge_bytes{};bool read=false;
    switch(r.rpc_number) {
    case fileio_open: {
        // {tag, request, size word, mode, ..., path at +20}; read-only cdrom0: files.
        if(r.send_size<21 || in(3)!=1)throw UnsupportedSyscall(0x77,context->pc);
        std::string name;for(unsigned n=20;n<r.send_size;++n){const auto ch=read8(rdram,r.send_source+n);if(!ch)break;name+=static_cast<char>(ch);}
        if(name.rfind("cdrom0:",0))throw UnsupportedSyscall(0x77,context->pc);
        std::optional<std::array<std::uint8_t,32>> found;
        try{found=lookup_disc_file(local_disc_path,name.substr(7));}catch(const std::exception&){throw UnsupportedSyscall(0x77,context->pc);}
        if(!found){result=static_cast<std::uint32_t>(-2);break;} // ENOENT
        NativeFile f;std::memcpy(&f.lba,found->data(),4);std::memcpy(&f.size,found->data()+4,4);
        result=native_next_fd_++;native_files_[result]=f;break;
    }
    case fileio_close: {
        if(!native_files_.erase(in(3)))throw UnsupportedSyscall(0x77,context->pc);
        break;
    }
    case fileio_lseek: {
        const auto file=native_files_.find(in(3));if(file==native_files_.end())throw UnsupportedSyscall(0x77,context->pc);
        auto& f=file->second;const auto offset=static_cast<std::int32_t>(in(4));const auto whence=in(5);
        const std::int64_t base=whence==0?0:whence==1?f.position:whence==2?std::int64_t(f.size):-1;
        if(base<0 || base+offset<0 || base+offset>f.size)throw UnsupportedSyscall(0x77,context->pc);
        f.position=static_cast<std::uint32_t>(base+offset);result=f.position;break;
    }
    case fileio_read: {
        const auto file=native_files_.find(in(3));if(file==native_files_.end())throw UnsupportedSyscall(0x77,context->pc);
        auto& f=file->second;const auto dest=in(4);const auto count=std::min(in(5),f.size-f.position);
        if(count) {
            checkMemory(rdram,dest,count);
            // (the image kept open: services/devices/disc_sectors.h)
            if(!read_iso_bytes(local_disc_path,std::uint64_t(f.lba)*2048+f.position,reinterpret_cast<char*>(memory_.data()+dest),count))
                throw UnsupportedSyscall(0x77,context->pc);
        }
        // The aligned middle travels by DMA; the bytes before the first and after the last 16-byte
        // boundary travel in the completion payload, which the EE handler copies into place.
        const auto head=std::min(count,(16-dest%16)%16),tail=(count-head)%16;
        edges={head,tail,dest,dest+count-tail};
        std::copy_n(memory_.begin()+dest,head,edge_bytes.begin());
        std::copy_n(memory_.begin()+dest+count-tail,tail,edge_bytes.begin()+64);
        f.position+=count;result=count;read=true;break;
    }
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    write32(rdram,r.receive,1); // Request accepted.
    completeNativeCall(rdram,context,r,server);
    // Completion payload into the next result buffer, then the command header to the EE handler.
    const auto buffer=native_fileio_buffers_[native_fileio_next_buffer_];
    const unsigned extra=read?164u:32u;checkMemory(rdram,buffer,extra);
    std::fill_n(memory_.begin()+buffer,extra,std::uint8_t{0});
    const std::array<std::uint32_t,5> words={tag,r.rpc_number,request,size_word,result};
    for(unsigned n=0;n<words.size();++n)write32(rdram,buffer+n*4,words[n]);
    if(read) {
        for(unsigned n=0;n<4;++n)write32(rdram,buffer+20+n*4,edges[n]);
        std::copy(edge_bytes.begin(),edge_bytes.end(),memory_.begin()+buffer+36);
    }
    const std::array<std::uint32_t,4> header={16u|(extra<<8),buffer,fileio_command,native_fileio_next_buffer_};
    for(unsigned n=0;n<4;++n)write32(rdram,sif_receive_buffer+n*4,header[n]);
    native_fileio_next_buffer_^=1;
    deliverEeCommand(rdram,context,fileio_handler,fileio_handler_argument);++fileio_completions;
    finishNativeFileio(context);return true;
}

void EeRuntime::finishNativeFileio(R5900Context* context) {
    ++fileio_calls;++sif_dma_submissions;++native_fileio_requests;context->r[2].low=0x70100+fileio_calls;
}
