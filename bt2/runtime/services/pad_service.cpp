#include "ee/ee_runtime.h"
#include "services/pad_block.h"
#include <array>
#include <cstdio>
#include <cstdlib>

// Native controller service in place of DBCMAN (servers 0x80001300 main, 0x8000131C, 0x8000131E
// port 1, 0x8000131F port 0). One DualShock 2 is connected to port 0 and port 1 is empty. All
// requests carry 144 bytes (36 words) in and out; rules as DBCMAN answered them (traced 2026-10-11):
// - 0x80001363 init: {0x310}.
// - 0x80001304 init: {1, table}; a 128-byte socket table is written to the EE table address.
// - 0x80001301 open slot: echo, with word 9 = slot (word 2). Words 10/11 are the slot's two EE
//   receive buffers.
// - 0x80001303 attach: echo, with word 1 cleared.
// - 0x8000131A device query {port, query, ...}: port 0 answers word 2 = 1 and word 3 = 3 (query
//   0x1038002) or 1 (query 0x101800C); port 1 is unchanged. Word 35 is the result.
// - port requests (services 0x8000131E/F) {port, ...}: echo; word 35 is the result.
// Results: 1 for the connected port 0, -23 for the empty port 1.
// Online play (pad_second, set by the library through the plugin API): a second DualShock 2 on port 1 that answers every request as port 0
// does (the game's pad library treats its two sockets alike: it copies the socket table, opens socket k only when word k of it is 1, then reads
// the slot's two buffers; docs/plans/online/LOCKSTEP_PLAN.md). The other player's controller is written to slot 1's buffers like port 0's.
// Each VBlank, once slot 0 is open and DBCMAN's detection has run (27 fields), the pad state is
// written to the slot's two buffers alternately: a 128-byte block in the DualShock 2 report layout.

struct NativePadState {
    DualShock2 pad,pad2;
    std::array<std::uint32_t,2> buffers{},buffers2{};std::uint32_t table=0;bool open=false,open2=false;
    unsigned fields_open=0,transfers=0,transfers2=0;
};

namespace {
constexpr std::uint32_t dbc_main=0x80001300,dbc_port1=0x8000131E,dbc_port0=0x8000131F;
constexpr std::uint32_t dbc_init_version=0x80001363,dbc_init_table=0x80001304,dbc_open=0x80001301,dbc_attach=0x80001303,dbc_query=0x8000131A;
constexpr std::uint32_t dbc_version=0x310,dbc_connected=1,dbc_no_device=static_cast<std::uint32_t>(-23);
constexpr unsigned dbc_words=36,dbc_result=35;
constexpr unsigned pad_detection_fields=27; // DBCMAN's detection and DualShock 2 mode setup.
// DBCMAN's initial socket table; 0x82640 is an IOP handle the game keeps and passes back.
constexpr std::array<std::uint32_t,32> dbc_initial_table={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0x82640,0xFFFFFFFF,0xFFFFFFFF,0,0,0,0,1,1,0,0x82640};
// The table once DBCMAN has detected the DualShock 2 (written with the first pad transfer); 0x82290
// is another kept IOP handle.
constexpr std::array<std::uint32_t,32> dbc_detected_table={1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,1,0,0x82640,0,0,0xFFFFFF00,0xFFFFFFFF,0x82290,1,1,1,0,0x82640};
}

bool EeRuntime::nativePadCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    if(r.send_size!=dbc_words*4 || r.recv_size!=dbc_words*4 || r.receive!=r.send_source)throw UnsupportedSyscall(0x77,context->pc);
    auto& state=nativePadState();
    std::array<std::uint32_t,dbc_words> w;for(unsigned n=0;n<dbc_words;++n)w[n]=read32(rdram,r.send_source+n*4);
    const bool second=pad_second.load(std::memory_order_relaxed);
    const auto connected=[&](std::uint32_t port){return port==0 || (port==1 && second);};
    const auto port_result=[&](std::uint32_t port){return connected(port)?dbc_connected:dbc_no_device;};
    bool startup=false;
    if(server.id==dbc_main) {
        switch(r.rpc_number) {
        case dbc_init_version: w={};w[0]=dbc_version;startup=true;break;
        case dbc_init_table: {
            const auto table=w[1];checkMemory(rdram,table,128);state.table=table;
            for(unsigned n=0;n<dbc_initial_table.size();++n)write32(rdram,table+n*4,dbc_initial_table[n]);
            w[0]=1;startup=true;break;
        }
        case dbc_open:
            if(w[2]>1)throw UnsupportedSyscall(0x77,context->pc);
            w[9]=w[2];
            if(w[2]==0){if(!state.table)throw UnsupportedSyscall(0x77,context->pc);state.buffers={w[10],w[11]};state.open=true;}
            else if(second){state.buffers2={w[10],w[11]};state.open2=true;std::fprintf(stderr,"Pad: slot 1 opened (buffers 0x%X 0x%X)\n",w[10],w[11]);}
            startup=true;break;
        case dbc_attach: w[1]=0;break;
        case dbc_query:
            if(w[1]!=0x1038002 && w[1]!=0x101800C)throw UnsupportedSyscall(0x77,context->pc);
            if(connected(w[0])){w[2]=1;w[3]=w[1]==0x1038002?3:1;}
            w[dbc_result]=port_result(w[0]);break;
        default: throw UnsupportedSyscall(0x77,context->pc);
        }
    } else if(server.id==dbc_port0 || server.id==dbc_port1) {
        if(w[0]>1)throw UnsupportedSyscall(0x77,context->pc);
        w[dbc_result]=port_result(w[0]);
    } else throw UnsupportedSyscall(0x77,context->pc);
    for(unsigned n=0;n<dbc_words;++n)write32(rdram,r.receive+n*4,w[n]);
    completeNativeCall(rdram,context,r,server);
    ++sif_dma_submissions;++native_pad_requests;
    if(startup){++dbc_rpc_calls;context->r[2].low=0x70000+dbc_rpc_binds+dbc_rpc_calls;}
    else if(server.id==dbc_main){++dbc_main_calls;context->r[2].low=0x7C0000+dbc_main_calls;}
    else {++dbc_port_calls;context->r[2].low=0x7B0000+dbc_port_calls;}
    return true;
}

NativePadState& EeRuntime::nativePadState() {
    if(!native_pad_) {
        native_pad_=std::make_shared<NativePadState>();
        // Repeatable input (pad_script.h); live input takes its place, except on Android, where a script asked for
        // ("pad_script" in the app's file: the same fight in every measurement) plays over the touch controls.
        const bool over_live=std::getenv("BT2_PAD_SCRIPT_OVER_LIVE")!=nullptr;
        if(const char* script=std::getenv("BT2_PAD_SCRIPT");script && (!on_pad_input || over_live))native_pad_->pad.script=PadScript::parse(script);
    }
    return *native_pad_;
}

void EeRuntime::nativePadVblank(std::uint8_t* rdram) {
    auto& state=nativePadState();
    state.pad.advanceField(); // Scripted input changes at field starts; fields count every VBlank.
    const bool second=pad_second.load(std::memory_order_relaxed);
    bool exchanged=false;
    if(second && on_pad_exchange) {
        // Online play: the controller of this field goes to the other side and what both players made comes back (it may wait for it).
        // Until the exchange begins (the start-up walks the menus by script) the script holds player 1's buttons and player 2 is neutral.
        const PadInput local=on_pad_input?on_pad_input():PadInput{};
        PadInput first,other;
        if((exchanged=on_pad_exchange(local,first,other))){state.pad.hold(first);state.pad2.hold(other);}
        else {state.pad2.hold(PadInput{});if(state.pad.script.empty())state.pad.hold(local);}
    } else if(on_pad_input && !state.pad.script.empty()) on_pad_input(); // (the session's pause and settings still run)
    else if(on_pad_input) {
        const auto before=state.pad.buttons;state.pad.hold(on_pad_input());
        if(on_pad_change && state.pad.buttons!=before)on_pad_change(state.pad.field,std::uint16_t(~state.pad.buttons));
    }
    if(!state.open || ++state.fields_open<pad_detection_fields)return;
    if(!state.transfers) {
        // Detection complete: the socket table reports the connected device.
        checkMemory(rdram,state.table,128);
        for(unsigned n=0;n<dbc_detected_table.size();++n)write32(rdram,state.table+n*4,dbc_detected_table[n]);
        if(second)write32(rdram,state.table+4,1); // word k of the table: socket k has a device (the game's pad library checks it before opening socket k)
    }
    if(second && state.open2) {
        const auto buffer2=state.buffers2[state.transfers2%2];checkMemory(rdram,buffer2,128);
        const auto block2=nativePadBlock(state.pad2,state.transfers2);
        for(unsigned n=0;n<block2.size();++n)write32(rdram,buffer2+n*4,block2[n]);
        ++state.transfers2;
    }
    const auto buffer=state.buffers[state.transfers%2];checkMemory(rdram,buffer,128);
    const auto block=nativePadBlock(state.pad,state.transfers);
    for(unsigned n=0;n<block.size();++n)write32(rdram,buffer+n*4,block[n]);
    ++state.transfers;++native_pad_transfers;
}
