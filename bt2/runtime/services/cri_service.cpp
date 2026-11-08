#include "ee/ee_runtime.h"
#include "services/cri_stream.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

// Sound of the port's own added to each field before the device has it (Dragon Rage: game/opening_music.h).
std::function<void(std::vector<std::int16_t>&)> host_music_mix;

// Native CRI ADX service (0x90000200) in place of CRI_ADXI. Replies as the original returned them
// (traced 2026-10-11); the handles are opaque IOP addresses the EE keeps and passes back.
// - 2 create channel {index, EE buffer, IOP buffer, 0x880}: channel handle 0x1B58C0 + index*0x44.
// - 0x403, 0x40A: no reply. 0x40C {0xBB80, 0xFFFF, 0xFFFF}: {0, 1, 0}.
// - 0x422 {1, IOP buffer, 0x100}: handle 0x1BBA40 + n*0x30. 0x400 {..., handle, 1, ...}: 0x1BC580 + n*0x14.
// - 0x408 {2, 0, buffer handle, buffer handle}: player 0x1B5DA0 + n*0x1C (lowest free n). 0x409 {player}: destroys
//   a stopped player, no reply (the reuse of its handle is seen in the captures of two successive movies).
// Stream channels: the EE uploads each channel's 0x880-byte buffer, and at the next VBlank the IOP
// returns it with the last word (the EE's sequence counter) advanced by one and its commands consumed
// (cri_stream.h); unknown command codes stop at that VBlank. While a player runs, the returned buffer
// carries its messages about played data (cri_audio.h).

namespace {
constexpr std::uint32_t cri_create=2,cri_403=0x403,cri_40a=0x40A,cri_40c=0x40C,cri_422=0x422,cri_400=0x400,cri_408=0x408,cri_409=0x409;
constexpr std::uint32_t player_base=0x1B5DA0,player_size=0x1C;
constexpr std::uint32_t stream_size=0x880;
}

bool EeRuntime::nativeCriCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    const auto in=[&](unsigned n){if(n*4+4>r.send_size)throw UnsupportedSyscall(0x77,context->pc);return read32(rdram,r.send_source+n*4);};
    const auto reply=[&](std::initializer_list<std::uint32_t> words) {
        if(r.recv_size!=words.size()*4)throw UnsupportedSyscall(0x77,context->pc);
        unsigned n=0;for(auto w:words)write32(rdram,r.receive+4*n++,w);
    };
    const auto none=[&]{if(r.recv_size)throw UnsupportedSyscall(0x77,context->pc);};
    bool create=false;
    switch(r.rpc_number) {
    case cri_create: {
        const auto index=in(0),ee=in(1),iop=in(2),size=in(3);
        if(index>1 || size!=stream_size || native_cri_channels_.size()!=index || iop+size>native_iop_ram_.size())throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,ee,size);
        native_cri_channels_.push_back({ee,iop,false});
        reply({0x1B58C0+index*0x44});create=true;break;
    }
    case cri_403: none();break;
    case cri_40a: if(r.send_size!=12)throw UnsupportedSyscall(0x77,context->pc);none();break;
    case cri_40c: if(in(0)!=0xBB80)throw UnsupportedSyscall(0x77,context->pc);reply({0,1,0});break;
    case cri_422: if(in(0)!=1)throw UnsupportedSyscall(0x77,context->pc);reply({0x1BBA40+native_cri_422s_++*0x30});break;
    case cri_400: {const auto handle=0x1BC580+native_cri_400s_++*0x14;native_cri_audio.sourceCreated(handle,in(1),in(3));reply({handle});break;} // {EE state, 0x422 handle, 1, EE object}.
    case cri_408: {
        if(in(0)!=2 || native_cri_players_==UINT32_MAX)throw UnsupportedSyscall(0x77,context->pc);
        unsigned slot=0;while(native_cri_players_&(1u<<slot))++slot;
        native_cri_players_|=1u<<slot;
        const auto handle=player_base+slot*player_size;native_cri_audio.playerCreated(handle,in(2),in(3));reply({handle});break;
    }
    case cri_409: { // Destroy a stopped player; its slot is free for the next 0x408 (the original reuses the handle).
        const auto handle=in(0);const auto slot=(handle-player_base)/player_size;
        if(r.send_size!=4 || handle<player_base || (handle-player_base)%player_size || slot>=32 || !(native_cri_players_&(1u<<slot)))throw UnsupportedSyscall(0x77,context->pc);
        try {native_cri_audio.playerDestroyed(handle);} catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        native_cri_players_&=~(1u<<slot);none();break;
    }
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    completeNativeCall(rdram,context,r,server);
    ++cri_rpc_calls;++sif_dma_submissions;++native_cri_requests;
    context->r[2].low=create?0x780002:0x780002+cri_rpc_calls-1;return true;
}

bool EeRuntime::nativeCriStreamUpload(std::uint8_t* rdram,R5900Context* context,std::uint32_t source,std::uint32_t destination,std::uint32_t size) {
    const auto channel=std::find_if(native_cri_channels_.begin(),native_cri_channels_.end(),[&](const auto& c){return c.ee==source && c.iop==destination;});
    if(channel==native_cri_channels_.end())return false;
    if(size!=stream_size)throw UnsupportedSyscall(0x77,context->pc);
    channel->pending=true;
    ++cri_stream_transfers;++sif_dma_submissions;context->r[2].low=0x790000+cri_stream_transfers;
    return true;
}

void EeRuntime::nativeCriVblank(const R5900Context* interrupted) {
    // The IOP consumes each uploaded buffer and returns it with the sequence counter advanced.
    if(on_audio_field && !native_cri_audio.on_audible) {
        native_cri_audio.on_audible=[this](std::uint32_t source,unsigned side,std::uint32_t hertz,std::uint32_t address,std::uint32_t bytes) {
            if(address>native_iop_ram_.size() || bytes>native_iop_ram_.size()-address)throw std::runtime_error("CRI audio data outside IOP memory");
            const bool single=!native_cri_audio.paired(source);
            audio_mixer.push(source,side,hertz,std::span<const std::uint8_t>(native_iop_ram_.data()+address,bytes));audio_stream_bytes+=bytes;if(single)audio_single_stream_bytes+=bytes;
        };
        native_cri_audio.on_silenced=[this](std::uint32_t source) {audio_mixer.drop(source);};
    }
    native_cri_audio.field(); // The IOP's player advances one field.
    for(auto& c:native_cri_channels_) {
        if(!c.pending)continue;
        const auto channel=unsigned(&c-native_cri_channels_.data());
        const auto stream=std::span<std::uint8_t>(native_iop_ram_.data()+c.iop,stream_size);
        // Commands are consumed (count cleared); unknown command codes stop at this VBlank.
        // Then the buffer carries the player's messages for the data played since the last return.
        try {native_cri_commands+=consumeCriStreamCommands(stream,&native_cri_audio,channel);writeCriStreamMessages(stream,native_cri_audio.played(channel));}
        catch(const std::runtime_error& e) {std::cerr<<e.what()<<" (channel EE 0x"<<std::hex<<c.ee<<", IOP 0x"<<c.iop<<std::dec<<")"<<std::endl;throw UnsupportedSyscall(0x14,interrupted->pc);}
        std::copy_n(native_iop_ram_.begin()+c.iop,stream_size,memory_.begin()+c.ee);
        std::uint32_t counter;std::memcpy(&counter,memory_.data()+c.ee+stream_size-4,4);++counter;
        std::memcpy(memory_.data()+c.ee+stream_size-4,&counter,4);
        c.pending=false;++native_cri_returns;
    }
    if(on_audio_field) {
        auto pcm=audio_mixer.field([this](std::span<std::int32_t> mix){sound_sampler.render(mix);});
        if(host_music_mix)host_music_mix(pcm);
        on_audio_field(pcm);++audio_fields;
    }
}
