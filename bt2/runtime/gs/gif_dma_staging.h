#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>
#include "cpu/gs_command_sink.h"
#include "ee/frame_profile.h"

class GifPacketMismatch : public std::runtime_error {
public:
    GifPacketMismatch(std::span<const std::uint8_t> expected,std::span<const std::uint8_t> actual)
        : std::runtime_error("GIF source packet differs from reference"),
          expected(expected.begin(),expected.end()),actual(actual.begin(),actual.end()) {}
    std::vector<std::uint8_t> expected,actual;
};

// Two observed normal-mode GIF requests with bounded diagnostic completion.
class GifDmaStaging {
public:
    void supply(std::span<const std::uint8_t> expected_packet) {
        if(supplied_ || expected_packet.size()!=240)throw std::runtime_error("Invalid/repeated GIF packet evidence");
        expected_.assign(expected_packet.begin(),expected_packet.end());first_=expected_;supplied_=true;
    }
    // A later identical request pair (same routine, frame and packets) replays the same evidence
    // from a fresh state. Only a completed original pair can be repeated.
    GifDmaStaging repeat() const {
        if(!supplied_ || !sync_ || phase_!=6 || starts!=2 || completions!=2)throw std::runtime_error("GIF request pair not complete");
        GifDmaStaging next;next.expected_=first_;next.first_=first_;next.second_=second_;next.supplied_=true;next.sync_=true;
        next.take_game_packets=take_game_packets;return next;
    }
    std::uint32_t readIdle() {
        if(!supplied_ || (phase_!=0 && !(phase_==6 && starts==1 && !second_.empty())))throw std::runtime_error("Unsupported/repeated GIF idle read");
        if(phase_==6) {expected_=second_;sync_reads=0;packet_verified=false;}
        phase_=1;++idle_reads;return starts?1:0;
    }
    void setCount(std::uint32_t value) {
        if(phase_!=1 || value!=15)throw std::runtime_error("Unsupported GIF count/order");
        qwc=value;phase_=2;++staging_writes;
    }
    void setAddress(std::uint32_t value,std::uint32_t expected_address,std::span<const std::uint8_t> ram) {
        if(phase_!=2 || value!=expected_address || value%16 || value>ram.size()
            || expected_.size()>ram.size()-value)
            throw std::runtime_error("GIF source address/bounds/packet differs");
        auto actual=ram.subspan(value,expected_.size());
        // A packet supplied as zeros was not captured (game/startup_inputs.h): with take_game_packets the
        // packet the game built is the reference, for this pair and its repeats.
        if(take_game_packets && std::all_of(expected_.begin(),expected_.end(),[](std::uint8_t b){return b==0;})) {
            expected_.assign(actual.begin(),actual.end());
            (starts?second_:first_)=expected_;
        }
        unsigned padding_differences=0;
        bool matches=std::equal(expected_.begin(),expected_.begin()+16,actual.begin());
        for(std::size_t offset=16;offset<expected_.size();offset+=16) {
            std::uint64_t expected_value,expected_reg,actual_value,actual_reg;
            std::memcpy(&expected_value,expected_.data()+offset,8);
            std::memcpy(&expected_reg,expected_.data()+offset+8,8);
            std::memcpy(&actual_value,actual.data()+offset,8);
            std::memcpy(&actual_reg,actual.data()+offset+8,8);
            // Pinned GS register definitions: only bit 0 exists in these three.
            // The generated constructor preserves previous stack padding bits.
            const std::uint64_t mask=(expected_reg==0x1A || expected_reg==0x46 || expected_reg==0x45) ? 1 : UINT64_MAX;
            matches=matches && expected_reg==actual_reg && (expected_value&mask)==(actual_value&mask);
            if(mask==1 && expected_value!=actual_value)++padding_differences;
        }
        if(!matches)throw GifPacketMismatch(expected_,actual);
        staged_.assign(actual.begin(),actual.end());
        reserved_difference_words=padding_differences;
        madr=value;phase_=3;++staging_writes;packet_verified=true;
    }
    void start(std::uint32_t value,std::span<const std::uint8_t> ram) {start(value,ram,gs);}
    void start(std::uint32_t value,std::span<const std::uint8_t> ram,GsCommandSink& sink) {
        if(!ready() || value!=0x101)throw std::runtime_error("Unsupported GIF start/order");
        // Revalidate the staged bytes immediately before command consumption.
        if(madr>ram.size() || std::size_t(qwc)*16>ram.size()-madr)throw std::runtime_error("GIF start source bounds changed");
        auto packet=ram.subspan(madr,qwc*16);
        if(!std::equal(staged_.begin(),staged_.end(),packet.begin()))throw GifPacketMismatch(staged_,packet);
        {FrameProfile::Scope timed(frame_profile.gs);sink.consume(packet);}
        chcr=value;phase_=4;++starts;
        madr+=qwc*16;qwc=0;
        // Packet consumption advances MADR/QWC; STR remains set until checked completion.
    }
    void supplySync(std::span<const std::uint8_t> pub) {
        constexpr std::array<std::uint32_t,27> expected={0,0,0x1FFEAF0,0,0x101,0,0x1FFEC40,0x40,0xE00,
            1,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0,
            0x1000A000,0,0x1FFEAF0,0,1,0,0x1FFEC40,0x40,0};
        if(sync_ || pub.size()!=372 || std::memcmp(pub.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("Invalid GIF synchronization evidence");
        constexpr std::array<std::uint32_t,6> idle_return={0,0,0x1FFEAF0,0,0,0x1FFEB00};
        if(std::memcmp(pub.data()+108,idle_return.data(),sizeof(idle_return)))throw std::runtime_error("Invalid VU idle/GS synchronization return evidence");
        second_.assign(pub.begin()+132,pub.end());sync_=true;
    }
    bool syncSupplied() const {return sync_;}
    std::uint32_t readVifIdle() {
        if(!sync_ || phase_!=4 || sync_reads)throw std::runtime_error("Unsupported VIF1 channel read");
        ++sync_reads;return 0;
    }
    std::uint32_t readStarted(bool completion_poll) {
        if(!sync_ || phase_!=4 || sync_reads!=(completion_poll?2u:1u))throw std::runtime_error("Unsupported GIF completion order");
        ++sync_reads;
        if(completion_poll) {phase_=5;chcr=1;++completions;}
        return chcr;
    }
    std::uint32_t readVifStatus() {
        if(!sync_ || phase_!=5 || sync_reads!=3)throw std::runtime_error("Unsupported VIF1 status read");
        ++sync_reads;return 0x40;
    }
    std::uint32_t readVuIdle() {
        if(!sync_ || phase_!=5 || sync_reads!=4 || vu_reads>=starts)throw std::runtime_error("Unsupported VU idle checkpoint/order");
        ++vu_reads;return 0;
    }
    std::uint32_t readGifStatus() {
        if(!sync_ || phase_!=5 || sync_reads!=4 || vu_reads!=starts)throw std::runtime_error("Unsupported GIF status/order");
        phase_=6;++sync_reads;return 0;
    }
    unsigned sync_reads=0,completions=0,vu_reads=0;
    bool take_game_packets=false; // BT2_SELF_STARTUP: packets supplied as zeros come from the game (above).
    GsCommandSink gs;
    unsigned starts=0;
    std::uint32_t chcr=0;
    bool ready() const {return phase_==3 && packet_verified;}
    unsigned idle_reads=0,staging_writes=0,reserved_difference_words=0;
    std::uint32_t qwc=0,madr=0;
    bool packet_verified=false;
private:
    bool supplied_=false,sync_=false;
    unsigned phase_=0;
    std::vector<std::uint8_t> expected_,staged_,second_,first_;
};
