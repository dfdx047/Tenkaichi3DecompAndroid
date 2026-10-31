// VIF1: interprets the VIFcodes of a display list (docs/plans/graphics/VU1_PIPELINE_PLAN.md).
// The caller passes the words of one DMA transfer in order (with tag transfer: each tag's upper two
// words, then its data). UNPACK and MPG fill VU1 memory, MSCAL/MSCALF/MSCNT run a microprogram to
// its end, DIRECT/DIRECTHL hand PATH2 data to the GS. Only the reached codes and UNPACK forms are
// modelled; anything else is refused by name.
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include "vu1.h"

// A VIFcode (or form of one) that is not modelled, at word `index` of the transfer.
struct Vif1Unsupported : std::runtime_error {
    Vif1Unsupported(std::uint32_t code,std::size_t index,const std::string& what):std::runtime_error(what),code(code),index(index) {}
    std::uint32_t code;std::size_t index;
};

class Vif1 {
public:
    struct Registers {
        std::uint32_t cycle=0,mode=0,mask=0,base=0,ofst=0,tops=0,top=0,itops=0,itop=0,mark=0;
        std::array<std::uint32_t,4> row{},col{};
        bool dbf=false; // Double buffer flag: TOPS is BASE+OFST.
        bool mskpath3=false; // MSKPATH3: transfers on PATH3 (the GIF channel) are held.
    } r;
    Vu1 vu;
    unsigned codes=0,unpacks=0,programs=0,calls=0,directs=0;
    std::uint64_t unpacked_qwords=0,direct_qwords=0;

    // Throws Vif1Unsupported for the first code that is not modelled; nothing changes.
    void check(std::span<const std::uint32_t> words) const {Registers copy=r;process(words,copy,nullptr,nullptr,nullptr);}
    // Runs a checked transfer. `direct` receives PATH2 data, `kick` the XGKICK packets (PATH1), in order.
    void run(std::span<const std::uint32_t> words,const Vu1::Kick& direct,const Vu1::Kick& kick) {process(words,r,this,&direct,&kick);}

private:
    // One pass over the codes; `self` is null for the check, which only follows the registers.
    static void process(std::span<const std::uint32_t> words,Registers& r,Vif1* self,const Vu1::Kick* direct,const Vu1::Kick* kick);
};
