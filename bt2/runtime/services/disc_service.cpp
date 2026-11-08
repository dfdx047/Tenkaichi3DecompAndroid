#include "ee/ee_runtime.h"
#include "services/devices/disc_metadata.h"
#include "services/devices/disc_sectors.h"
#include <array>
#include <cstring>
#include <string>

// Native CDVD service: file search (0x80000597), N-commands (0x80000595) and S-commands
// (0x80000593), answered from the local ISO. Results match what CDVDFSV/CDVDMAN left in EE memory.

namespace {
constexpr std::uint32_t search_server=0x80000597,ncmd_server=0x80000595,scmd_server=0x80000593;
constexpr std::uint32_t ncmd_read=1,ncmd_ready=14,scmd_error=4;
constexpr std::uint32_t drive_ready=2,completion_record_size=144;
}

bool EeRuntime::nativeDiscCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(local_disc_path.empty())throw UnsupportedSyscall(0x77,context->pc);
    const auto reply_word=[&](std::uint32_t value) {
        if(r.recv_size!=4)throw UnsupportedSyscall(0x77,context->pc);
        write32(rdram,r.receive,value);
    };
    if(server.id==search_server && r.rpc_number==0) {
        // sceCdSearchFile: 300-byte request; NUL-terminated path at +36; EE result address at +292.
        // The 32-byte sceCdlFILE record is written to the result address, followed by request bytes 32..35.
        if(r.send_size!=300)throw UnsupportedSyscall(0x77,context->pc);
        std::array<std::uint8_t,300> in;for(unsigned n=0;n<in.size();++n)in[n]=read8(rdram,r.send_source+n);
        const auto end=std::find(in.begin()+36,in.begin()+292,std::uint8_t{0});
        if(end==in.begin()+292)throw UnsupportedSyscall(0x77,context->pc);
        const std::string path(in.begin()+36,end);std::uint32_t result_address;std::memcpy(&result_address,in.data()+292,4);
        std::optional<std::array<std::uint8_t,32>> record;
        try{record=lookup_disc_file(local_disc_path,path);}
        catch(const std::exception&){throw UnsupportedSyscall(0x77,context->pc);}
        if(record) {
            for(unsigned n=0;n<32;++n)write8(rdram,result_address+n,(*record)[n]);
            for(unsigned n=32;n<36;++n)write8(rdram,result_address+n,in[n]);
        }
        reply_word(record?1u:0u);completeNativeCall(rdram,context,r,server);
        ++disc_rpc_calls;++sif_dma_submissions;++native_disc_requests;context->r[2].low=0x740002;return true;
    }
    if(server.id==ncmd_server && r.rpc_number==ncmd_ready) {
        // Drive readiness query: an idle, ready drive.
        if(r.send_size)throw UnsupportedSyscall(0x77,context->pc);
        reply_word(drive_ready);completeNativeCall(rdram,context,r,server);
        ++ncmd_ready_calls;++sif_dma_submissions;++native_disc_requests;context->r[2].low=0x760001+ncmd_ready_calls;return true;
    }
    if(server.id==ncmd_server && r.rpc_number==ncmd_read) {
        // Sector read (no-wait): {LBN, count, EE destination, read mode, completion record, progress word}.
        if(r.send_size!=24 || r.recv_size)throw UnsupportedSyscall(0x77,context->pc);
        std::array<std::uint32_t,6> in;for(unsigned n=0;n<6;++n)in[n]=read32(rdram,r.send_source+n*4);
        const auto [lbn,count,dest,mode,record,progress]=in;
        // Read mode: try count, spindle control, data pattern. Only 2048-byte sectors (pattern 0).
        if(!count || count>512 || dest%64 || (mode>>16)&0xFF)throw UnsupportedSyscall(0x77,context->pc);
        const auto bytes=count*2048;checkMemory(rdram,dest,bytes);
        std::vector<std::uint8_t> payload;
        try{payload=read_iso_sectors(local_disc_path,lbn,count);}
        catch(const std::exception&){throw UnsupportedSyscall(0x77,context->pc);}
        std::copy(payload.begin(),payload.end(),memory_.begin()+dest);
        write32(rdram,progress,bytes);
        // Completion record: {status 0, 0, first byte, end}; the remaining words are zero.
        for(unsigned n=0;n<completion_record_size;n+=4)write32(rdram,record+n,0);
        write32(rdram,record+8,dest);write32(rdram,record+12,dest+bytes);
        completeNativeCall(rdram,context,r,server);
        ++ncmd_read_calls;++sif_dma_submissions;++native_disc_requests;context->r[2].low=0x760002+ncmd_read_calls*2;return true;
    }
    if(server.id==scmd_server && r.rpc_number==scmd_error) {
        // sceCdGetError: no error.
        if(r.send_size)throw UnsupportedSyscall(0x77,context->pc);
        reply_word(0);completeNativeCall(rdram,context,r,server);
        ++scmd_calls;++sif_dma_submissions;++native_disc_requests;context->r[2].low=0x7A0000+scmd_calls;return true;
    }
    // S-command media mode (34) is replayed with the emulated CD initialization for now.
    if(server.id==scmd_server)return false;
    throw UnsupportedSyscall(0x77,context->pc);
}
