#pragma once
#include <cstdint>
#include <stdexcept>

// EE Timer 2 in logical time. Ticks advance only at explicit scheduling points (one NTSC
// field per logical VBlank); there is no host clock. MODE bits 10 (EQUF) and 11 (OVFF) are
// write-one-to-clear; CMPE (bit 8) and OVFE (bit 9) gate the interrupt request.
class LogicalTimer2 {
public:
    // BUSCLK/256 = 576,000 Hz; NTSC field = 1001/60000 s -> 9,609.6 ticks, kept in tenths.
    static constexpr std::uint64_t tenths_per_field=96096;
    void start(std::uint32_t mode,std::uint32_t count,std::uint32_t comp) {
        if(started_ || (mode&3)!=2 || !(mode&0x80) || count>0xFFFF || comp>0xFFFF)throw std::runtime_error("Unsupported Timer 2 start");
        mode_=mode&0x3FF;count_=count;comp_=comp;started_=true;
    }
    bool started() const {return started_;}
    // Advance one logical field. Latch EQUF when COUNT passes COMP and OVFF on wrap.
    void advanceField() {
        if(!started_)throw std::runtime_error("Timer 2 not started");
        remainder_+=tenths_per_field;const auto ticks=remainder_/10;remainder_%=10;
        if(ticks>=0x10000)throw std::runtime_error("Timer 2 advance exceeds one wrap");
        const std::uint64_t from=count_,to=from+ticks;
        const auto hits=[&](std::uint64_t value){return value>from && value<=to;};
        if(hits(comp_) || hits(comp_+0x10000))mode_|=0x400;
        if(to>0xFFFF) {mode_|=0x800;++wraps;}
        count_=static_cast<std::uint32_t>(to&0xFFFF);++fields;
    }
    std::uint32_t count() const {return count_;}
    std::uint32_t mode() const {return mode_;}
    std::uint32_t comp() const {return comp_;}
    void writeMode(std::uint32_t value) {
        if((value&0x3FF)!=(mode_&0x3FF))throw std::runtime_error("Unsupported Timer 2 mode change");
        mode_&=~(value&0xC00);++mode_writes;
    }
    void writeComp(std::uint32_t value) {
        if(value>0xFFFF)throw std::runtime_error("Invalid Timer 2 compare");
        comp_=value;++comp_writes;
    }
    bool interruptPending() const {return ((mode_&0x500)==0x500) || ((mode_&0xA00)==0xA00);}
    unsigned fields=0,wraps=0,mode_writes=0,comp_writes=0;
private:
    bool started_=false;
    std::uint32_t mode_=0,count_=0,comp_=0;
    std::uint64_t remainder_=0;
};
