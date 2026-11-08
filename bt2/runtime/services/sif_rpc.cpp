#include <iostream>
#include "ee/ee_runtime.h"
#include <array>
#include <cstring>
#include <string>

// Generic SIF RPC transport for native services: decode a sceSifSetDma command packet, route it by
// server, and complete it through the game's own RPC_END handler.

namespace {
constexpr std::uint32_t sif_bind=0x80000009,sif_call=0x8000000A,sif_rpc_end=0x80000008;
constexpr std::uint32_t uncached(std::uint32_t address) {return address&0x1FFFFFFF;}
}

// Server handles are opaque IOP addresses the EE stores in the client and passes back. Native
// services keep the values the original services returned, so EE memory is unchanged.
const EeRuntime::NativeServer* EeRuntime::nativeServerById(std::uint32_t id) const {
    for(const auto& s:native_servers_)if(s.id==id)return &s;
    return nullptr;
}
const EeRuntime::NativeServer* EeRuntime::nativeServerByHandle(std::uint32_t handle) const {
    for(const auto& s:native_servers_)if(s.server==handle)return &s;
    return nullptr;
}

bool EeRuntime::decodeSifRpc(std::uint8_t* rdram,const R5900Context* context,SifRpcRequest& out) {
    // r4: descriptor array {source, IOP destination, size, attribute}; r5: count (1 or 2). The last
    // descriptor carries the command packet (attribute 0x44); a first one carries the send data.
    const auto descriptors=static_cast<std::uint32_t>(context->r[4].low),count=static_cast<std::uint32_t>(context->r[5].low);
    if(count<1 || count>2)return false;
    const auto last=descriptors+(count-1)*16;
    if(read32(rdram,last+12)!=0x44 || read32(rdram,last+8)!=64)return false;
    SifRpcRequest r;r.packet=uncached(read32(rdram,last));
    const auto word=[&](unsigned n){return read32(rdram,r.packet+n*4);};
    r.cid=word(2);
    if(r.cid!=sif_bind && r.cid!=sif_call)return false;
    r.rpc_id=word(6);r.client=word(7);r.rpc_number=word(8);
    if(r.cid==sif_call) {
        r.send_size=word(9);r.receive=word(10);r.recv_size=word(11);r.rmode=word(12);
        if(count==2) {
            r.send_source=read32(rdram,descriptors);
            if(read32(rdram,descriptors+8)!=r.send_size || read32(rdram,descriptors+12))throw UnsupportedSyscall(0x77,context->pc);
        } else if(r.send_size)throw UnsupportedSyscall(0x77,context->pc);
    }
    out=r;return true;
}

bool EeRuntime::handleNativeSifDma(std::uint8_t* rdram,R5900Context* context) {
    SifRpcRequest r;
    if(!decodeSifRpc(rdram,context,r))return handleNativeRawDma(rdram,context);
    const auto* server=r.cid==sif_bind?nativeServerById(r.rpc_number):nativeServerByHandle(read32(rdram,r.client+36));
    if(!server)return false;
    if(r.cid==sif_bind && !server->native_bind)return false; // Bound by an earlier, still emulated, service.
    if(r.cid==sif_bind) {
        // The bind reply carries the server and buffer handles that the RPC_END handler stores.
        const std::array<std::uint32_t,16> reply={0,0,sif_rpc_end,0,0,r.packet|0x20000000,0,r.client,sif_bind,server->server,server->buffer,0,0,0,0,0};
        if(server->id==0x02000004 && ee_queued_sound_rpc) {
            // The SOUNDS bind completes when main next waits (EE sound thread dispatch): queue it.
            std::array<unsigned,16> queued;std::copy(reply.begin(),reply.end(),queued.begin());
            queueSoundsBind(context,queued);return true;
        }
        completeNativeRpc(rdram,context,r,*server,reply);
        ++nativeBindCounter(*server);++sif_dma_submissions;
        context->r[2].low=server->bind_token?server->bind_token:0x70000+dbc_rpc_binds+dbc_rpc_calls;return true;
    }
    return callNativeService(rdram,context,r,*server);
}

void EeRuntime::completeNativeRpc(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server,
                                   const std::array<std::uint32_t,16>& reply) {
    for(unsigned n=0;n<16;++n)write32(rdram,sif_receive_buffer+n*4,reply[n]);
    // A blocking client waits on its semaphore (client+8); a no-wait client (-1) runs its end function.
    const auto semaphore=read32(rdram,r.client+8);
    // The completion and an end function run below the caller's stack pointer; that window is restored.
    constexpr unsigned window=256;
    const auto scratch=static_cast<std::uint32_t>(context->r[29].low)-window;
    checkMemory(rdram,scratch,window);std::array<std::uint8_t,window> saved;
    std::copy_n(memory_.begin()+scratch,window,saved.begin());
    try{deliverRpcEnd(rdram,context,r.client,semaphore==UINT32_MAX?0:semaphore,server.server,server.buffer);}
    catch(...){std::copy(saved.begin(),saved.end(),memory_.begin()+scratch);throw;}
    std::copy(saved.begin(),saved.end(),memory_.begin()+scratch);
}

void EeRuntime::completeNativeCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    // Call reply: received size and buffer, then the RPC_END header naming the client.
    const std::array<std::uint32_t,16> reply={r.recv_size?r.recv_size<<8:0u,r.recv_size?r.receive:0u,sif_rpc_end,0,0,0,0,r.client,sif_call,0,0,0,0,0,0,0};
    completeNativeRpc(rdram,context,r,server,reply);
}

unsigned& EeRuntime::nativeBindCounter(const NativeServer& server) {
    switch(server.id) {
    case 0x80000597: return disc_rpc_binds;
    case 0x80000595: return ncmd_rpc_binds;
    case 0x80000001: return fileio_binds;
    case 0x80000400: return mc_rpc_binds;
    case 0x80001300: case 0x8000131C: case 0x8000131E: case 0x8000131F: return dbc_rpc_binds;
    case 0x80000003: return heap_binds;
    case 0x80000701: return sound_rpc_binds;
    case 0x02000004: return sounds_rpc_binds;
    case 0x90000200: return cri_rpc_binds;
    default: return native_other_binds;
    }
}

// Returns false for a request the native service does not own yet; it then stays on its emulated route.
bool EeRuntime::callNativeService(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    // A refused request names itself: server, function number, sizes and the first send words.
    try {
        switch(server.id) {
        case 0x80000597: case 0x80000595: case 0x80000593: return nativeDiscCall(rdram,context,r,server);
        case 0x80000001: return nativeFileioCall(rdram,context,r,server);
        case 0x80000400: return nativeCardCall(rdram,context,r,server);
        case 0x80001300: case 0x8000131E: case 0x8000131F: return nativePadCall(rdram,context,r,server);
        case 0x80000003: return nativeHeapCall(rdram,context,r,server);
        case 0x80000006: return nativeModuleCall(rdram,context,r,server);
        case 0x80000701: return nativeSoundDriverCall(rdram,context,r,server);
        case 0x02000004: return nativeSoundsCall(rdram,context,r,server);
        case 0x90000200: return nativeCriCall(rdram,context,r,server);
        }
        throw UnsupportedSyscall(0x77,context->pc);
    } catch(const UnsupportedSyscall&) {
        std::cerr<<"Native service refused: server 0x"<<std::hex<<server.id<<" function 0x"<<r.rpc_number<<" send 0x"<<r.send_size<<" receive 0x"<<r.recv_size<<" words";
        for(std::uint32_t n=0;n<8 && n*4+4<=r.send_size && r.send_source;++n)std::cerr<<" "<<read32(rdram,r.send_source+n*4);
        std::cerr<<std::dec<<std::endl;
        throw;
    }
}
