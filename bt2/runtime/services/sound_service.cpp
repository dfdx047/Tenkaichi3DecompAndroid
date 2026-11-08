#include "ee/ee_runtime.h"
#include <array>
#include <cstring>
#include <iostream>

// Native sound driver (SDRDRV, 0x80000701) and SOUNDS (0x02000004) services. Replies as the
// original modules returned them (traced 2026-10-11); what is heard comes from `sound_sampler`.
// - SDRDRV: 64-byte commands, 16-byte replies. 0xE620 initializes (zeros); 0x80D0 transfers a
//   sound block from IOP memory to sound RAM and returns its size (word 5); 0x80F0 returns 1.
// - SOUNDS: 256-byte commands, 64-byte replies. 0x10 initializes (zeros). From 0x20 on every reply is
//   the module's status block: words 4 and 12 are 1 and 2, word 5 counts the loaded sound banks, words
//   8 and 13 are module-local values (the worker's IOP thread ID and an IOP address; opaque).
//   PCSX2 call log 2026-10-12 (capture_ee.py --sounds-log, 40 calls from boot into the menus):
//   0x20 loads a bank (word 5 + 1) and starts the worker on the first; 0x30 unloads one (word 5 - 1);
//   0x40 (request a sound), 0x50 and 0x200 leave the block unchanged; so do 0x70 and 0x100, whose
//   handlers (read from the module) return the same block. 0x80 replaces the bank of a slot (handler
//   0xF2C of the module, read 2026-10-19).

namespace {
constexpr std::uint32_t sdr_init=0xE620,sdr_block_transfer=0x80D0,sdr_command=0x80F0;
constexpr std::uint32_t sounds_init=0x10,sounds_bank=0x20,sounds_unload=0x30,sounds_play=0x40,sounds_fade=0x50,sounds_60=0x60,sounds_volume=0x70,sounds_replace=0x80,sounds_stop_all=0x100,sounds_silence=0x200;
constexpr std::array<std::uint32_t,16> sounds_status={0,0,0,0,1,1,0,0,0x20010,0,0,0,2,0x1C2440,0,0};
}

bool EeRuntime::nativeSoundDriverCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=64 || r.recv_size!=16)throw UnsupportedSyscall(0x77,context->pc);
    std::array<std::uint32_t,4> reply{};
    if(on_sound_evidence) {checkMemory(rdram,r.send_source,64);on_sound_evidence("driver",r.rpc_number,std::span<const std::uint8_t>(memory_.data()+r.send_source,64));}
    switch(r.rpc_number) {
    case sdr_init: break;
    case sdr_block_transfer: {
        // {reply buffer, 1, 0, IOP source, sound RAM address, size, ...}: the source must be uploaded data.
        const auto source=read32(rdram,r.send_source+12),size=read32(rdram,r.send_source+20);
        if(!size || source+size>native_iop_ram_.size())throw UnsupportedSyscall(0x77,context->pc);
        const auto address=read32(rdram,r.send_source+16);
        try {sound_sampler.upload(address,std::span<const std::uint8_t>(native_iop_ram_.data()+source,size));}
        catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        if(on_sound_evidence)on_sound_evidence("samples",address,std::span<const std::uint8_t>(native_iop_ram_.data()+source,size));
        reply[0]=size;break;
    }
    case sdr_command: reply[0]=1;break;
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    for(unsigned n=0;n<4;++n)write32(rdram,r.receive+n*4,reply[n]);
    completeNativeCall(rdram,context,r,server);
    ++sound_rpc_calls;++sif_dma_submissions;
    context->r[2].low=sound_rpc_calls==1?0x60002:0x60002+sound_rpc_calls;return true;
}

bool EeRuntime::nativeSoundsCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=256 || r.recv_size!=64)throw UnsupportedSyscall(0x77,context->pc);
    if(on_sound_evidence) {checkMemory(rdram,r.send_source,256);on_sound_evidence("sounds",r.rpc_number,std::span<const std::uint8_t>(memory_.data()+r.send_source,256));}
    switch(r.rpc_number) {
    case sounds_init: break;
    case sounds_bank: {
        // {bank handle, IOP block 1, IOP block 2}: both blocks must have been uploaded to the heap.
        const auto first=read32(rdram,r.send_source+4),second=read32(rdram,r.send_source+8);
        if(!native_heap_.isBlock(first) || !native_heap_.isBlock(second))throw UnsupportedSyscall(0x77,context->pc);
        if(on_sound_evidence) {
            on_sound_evidence("bank-head",first,std::span<const std::uint8_t>(native_iop_ram_.data()+first,native_heap_.size(first)));
            on_sound_evidence("bank-body",second,std::span<const std::uint8_t>(native_iop_ram_.data()+second,native_heap_.size(second)));
        }
        // {sound memory address of the samples, header block, sequence block}.
        try {
            sound_sampler.load(read32(rdram,r.send_source),std::span<const std::uint8_t>(native_iop_ram_.data()+first,native_heap_.size(first)),
                std::span<const std::uint8_t>(native_iop_ram_.data()+second,native_heap_.size(second)));
        } catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        native_sounds_started_=true;++native_sounds_banks_;break;
    }
    case sounds_unload:
        if(!native_sounds_started_ || !native_sounds_banks_ || sound_sampler.banks()!=native_sounds_banks_)throw UnsupportedSyscall(0x77,context->pc);
        sound_sampler.unload();--native_sounds_banks_;break;
    case sounds_play: {
        // Up to 15 sounds in parallel arrays of 0x20 bytes: byte 0 is the count, then per sound k
        // (from 1) the bank slot with its keep and stop flags at +k, the sound number at 0x20+k, the
        // volume at 0x40+k, the pan at 0x60+k and a 16-bit pitch offset in cents at 0x80+2k.
        if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);
        checkMemory(rdram,r.send_source,256);
        const auto* command=memory_.data()+r.send_source;
        const unsigned count=command[0];
        if(count>15)throw UnsupportedSyscall(0x77,context->pc);
        try {
            for(unsigned k=1;k<=count;++k) {
                std::int16_t cents;std::memcpy(&cents,command+0x80+k*2,2);
                sound_sampler.play(command[k],command[0x20+k],command[0x40+k],command[0x60+k],cents);
            }
        } catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        break;
    }
    // The module's handlers for the requests below were read from its code (2026-10-13). Word 3 is a
    // mask of bank slots (the module has eight).
    case sounds_fade:
        // Word 0 set: the masked slots' volume steps down to zero; clear: up to full. The steps are a
        // millisecond apart, so the end value applies at once.
        if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);
        sound_sampler.setVolume(read32(rdram,r.send_source+12)&0xFF,read32(rdram,r.send_source)?0:SoundSampler::full_volume);break;
    case sounds_silence:
        if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);
        sound_sampler.silence(read32(rdram,r.send_source+12)&0xFF);break;
    case sounds_stop_all:
        if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);
        sound_sampler.silence(0xFF);break;
    case sounds_volume: {
        // A 16-bit volume for all eight slots.
        const auto volume=read32(rdram,r.send_source)&0xFFFF;
        if(!native_sounds_started_ || volume>SoundSampler::full_volume)throw UnsupportedSyscall(0x77,context->pc);
        sound_sampler.setVolume(0xFF,volume);break;
    }
    case sounds_60:
        // The synthesizer's output mode (the game's stereo/mono option, sent when a save's options are
        // applied): the module passes 1 for a nonzero first word, else 0, to the synthesizer library.
        // Which of the two is mono is not established, so the mode is recorded and the mix stays stereo.
        if(!native_sounds_started_)throw UnsupportedSyscall(0x77,context->pc);
        sounds_output_mode=read32(rdram,r.send_source)?1:0;++sounds_output_mode_requests;break;
    case sounds_replace: {
        // {sound memory address, header block, sequence block, slot}: the module (handler 0xF2C) loads the
        // bank into that slot as 0x20 does into the next one, and keeps its count of banks (reached by
        // a story mode battle scene).
        const auto first=read32(rdram,r.send_source+4),second=read32(rdram,r.send_source+8),slot=read32(rdram,r.send_source+12);
        if(!native_sounds_started_ || !native_heap_.isBlock(first) || !native_heap_.isBlock(second))throw UnsupportedSyscall(0x77,context->pc);
        try {
            sound_sampler.replace(slot,read32(rdram,r.send_source),std::span<const std::uint8_t>(native_iop_ram_.data()+first,native_heap_.size(first)),
                std::span<const std::uint8_t>(native_iop_ram_.data()+second,native_heap_.size(second)));
        } catch(const std::runtime_error& e) {std::cerr<<e.what()<<std::endl;throw UnsupportedSyscall(0x77,context->pc);}
        ++sounds_bank_replacements;break;
    }
    default: throw UnsupportedSyscall(0x77,context->pc);
    }
    for(unsigned n=0;n<16;++n)write32(rdram,r.receive+n*4,!native_sounds_started_?0u:n==5?native_sounds_banks_:sounds_status[n]);
    completeNativeCall(rdram,context,r,server);
    ++sounds_rpc_calls;++sif_dma_submissions;
    context->r[2].low=sounds_rpc_calls==1?0x60004:0x60004+sounds_rpc_calls;return true;
}
