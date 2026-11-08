// The microprogram a VU1 start runs, as far as it can be told from program memory alone: the
// instruction pairs reachable from the start address. Used to tell programs apart (a key over the
// reachable pairs) for the program census and for native translations.
// Control flow as the interpreter runs it: a pair is followed by the next one; a branch takes effect
// after the pair that follows it; a pair with the E bit ends the program after the pair that follows
// it. A register jump (JR) can go anywhere, so the return addresses of the program's own calls (BAL)
// are taken as its targets and `indirect` is set.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

struct Vu1Program {
    struct Pair {std::uint32_t address,low,high;};
    std::uint32_t start=0;
    std::vector<Pair> pairs; // By address.
    bool indirect=false;     // Contains a register jump.
    std::uint64_t key=0;     // Over the start and the pairs: equal keys, equal programs.

    static Vu1Program reachable(std::span<const std::uint8_t> program,std::uint32_t start) {
        constexpr std::uint32_t mask=0x7FF;
        const auto word=[&](std::uint32_t address,unsigned half) {std::uint32_t w;std::memcpy(&w,program.data()+(address&mask)*8+half*4,4);return w;};
        // `seen`: the pair belongs to the program. `followed`: what comes after it does too. The pair
        // after an E bit or after an unconditional branch runs, but nothing follows it from there.
        std::array<bool,mask+1> seen{},followed{};std::vector<std::uint32_t> work,returns;
        Vu1Program result;result.start=start&mask;
        const auto visit=[&](std::uint32_t address) {address&=mask;seen[address]=true;if(!followed[address]) {followed[address]=true;work.push_back(address);}};
        const auto last=[&](std::uint32_t address) {seen[address&mask]=true;};
        visit(start);
        for(std::size_t done=0;;) {
            if(done==work.size()) {
                // Register jumps reach every return address found; that may uncover more code.
                const auto before=work.size();
                if(result.indirect)for(const auto r:returns)visit(r);
                if(work.size()==before)break;
            }
            const auto at=work[done++];
            const auto low=word(at,0),high=word(at,1);
            const bool ending=(high>>30)&1,immediate=high>>31;
            const unsigned op=low>>25;
            const auto target=(at+1+(std::uint32_t(low&0x7FF)-((low&0x400)<<1)))&mask;
            if(ending) {last(at+1);continue;} // The pair after the E bit runs, then the program ends.
            if(immediate) {visit(at+1);continue;} // The lower word is a float for I.
            switch(op) {
                case 0x20:last(at+1);visit(target);break;                                    // B: the delay pair, then the target.
                case 0x21:last(at+1);visit(target);returns.push_back((at+2)&mask);break;       // BAL.
                case 0x24:last(at+1);result.indirect=true;break;                               // JR.
                case 0x28:case 0x29:case 0x2C:case 0x2D:case 0x2E:case 0x2F:last(at+1);visit(target);visit(at+2);break; // Conditional branches: the target or the pair after the delay pair.
                default:visit(at+1);
            }
        }
        std::uint64_t key=0xCBF29CE484222325ull;
        const auto mix=[&](std::uint32_t v) {key=(key^v)*0x100000001B3ull;};
        mix(result.start);
        for(std::uint32_t address=0;address<=mask;++address)if(seen[address]) {
            const Pair pair{address,word(address,0),word(address,1)};
            result.pairs.push_back(pair);mix(pair.address);mix(pair.low);mix(pair.high);
        }
        result.key=key;
        return result;
    }
};
