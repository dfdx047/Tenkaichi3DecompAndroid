#pragma once
#include <cstdint>
#include <stdexcept>

// Logical EE Timer 1 with the H-BLANK clock (MODE CLKS=3: COUNT advances once per HBLANK while CUE
// (0x80) is set) or, in the frame phase, BUSCLK/256 (CLKS=2: 9,609.6 ticks per NTSC field, kept exact
// with an integer remainder). Logical display time is kept in half-lines: an NTSC field is 262.5 lines (525
// half-lines) and an HBLANK falls every two half-lines. Field boundaries come from the logical
// VBlank model (`field`); a polled COUNT read waits one line (`line`).
class LogicalTimer1 {
public:
    static constexpr std::uint64_t field_halflines=525;
    void configure(std::uint32_t value) {
        if(value!=3)throw std::runtime_error("Unsupported Timer 1 mode");
        mode_=value;configured_=true;
    }
    void writeMode(std::uint32_t value) {
        // Only CUE may change after configuration (start/stop of the H-BLANK delay).
        if(!configured_ || (value&~0x80u)!=(mode_&~0x80u))throw std::runtime_error("Unsupported Timer 1 mode change");
        mode_=value;
    }
    // Frame-phase configuration: H-BLANK or BUSCLK/256, CUE on or off; no gate, zero return or
    // interrupts (so a COUNT wrap raises no flag).
    void reconfigure(std::uint32_t value) {
        value&=~0xC00u; // Writing 1 to the equal/overflow flags (bits 10, 11) acknowledges them; no flag is modelled.
        if(!configured_ || (value!=0x02 && value!=0x03 && value!=0x82 && value!=0x83))throw std::runtime_error("Unsupported Timer 1 mode");
        if((value&3)!=(mode_&3))busclk_remainder_=0;
        mode_=value;
    }
    void setCount(std::uint32_t value) {count_=value&0xFFFF;}
    void writeCount(std::uint32_t value) {
        if(value || counting())throw std::runtime_error("Unsupported Timer 1 count write");
        count_=0;
    }
    // Field boundary reached through the VBlank model: credit the remaining HBLANKs of this field.
    void field() {advanceTo((halflines_/field_halflines+1)*field_halflines);}
    // One polled line: the caller takes `nextLine()`, delivers the VBlank first when that line
    // crosses a field boundary (`crossesField`), then completes the line with `lineTo`.
    std::uint64_t nextLine() const {return halflines_+2;}
    bool crossesField(std::uint64_t target) const {return target/field_halflines>halflines_/field_halflines;}
    void lineTo(std::uint64_t target) {advanceTo(target);}
    std::uint32_t mode() const {return mode_;}
    std::uint32_t count() const {return count_;}
    bool counting() const {return configured_ && (mode_&0x80);}
    std::uint64_t halflines() const {return halflines_;}
private:
    void advanceTo(std::uint64_t next) {
        if(next<halflines_)return; // Already at or past this point (a field was delivered first).
        if(counting() && (mode_&3)==2) {
            // BUSCLK/256: 96,096 tenth-ticks per 525-half-line field = 96,096/5,250 ticks per half-line.
            busclk_remainder_+=(next-halflines_)*96096;
            count_=static_cast<std::uint32_t>((count_+busclk_remainder_/5250)&0xFFFF);busclk_remainder_%=5250;
        } else if(counting()) {
            const auto add=next/2-halflines_/2;
            // COUNT is 16 bits and wraps; MODE 3 enables neither the overflow nor the compare
            // interrupt, so a wrap raises no flag or interrupt.
            count_=static_cast<std::uint32_t>((count_+add)&0xFFFF);
        }
        halflines_=next;
    }
    std::uint32_t mode_=0,count_=0;bool configured_=false;std::uint64_t halflines_=0,busclk_remainder_=0;
};
