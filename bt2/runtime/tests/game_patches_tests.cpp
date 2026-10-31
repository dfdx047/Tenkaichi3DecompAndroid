// Optional game patches (runtime/ee/game_patches.h) on a synthetic memory image (no game data).
#include "ee/game_patches.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"Game patch check failed: "<<what<<'\n';std::exit(1);}}
template<class F> bool throws(F f) {try{f();}catch(const std::runtime_error&){return true;}return false;}
std::uint32_t word(const std::vector<std::uint8_t>& ram,std::uint32_t at) {std::uint32_t v;std::memcpy(&v,ram.data()+at,4);return v;}
void put(std::vector<std::uint8_t>& ram,std::uint32_t at,std::uint32_t v) {std::memcpy(ram.data()+at,&v,4);}
}

int main() {
    const auto& wide=gamePatch("widescreen");
    check(wide.words.size()==9,"widescreen has its nine words");
    std::vector<std::uint8_t> ram(32u<<20);
    for(const auto& w:wide.words)put(ram,w.address,w.original);
    applyGamePatch(ram,wide);
    bool all=true;for(const auto& w:wide.words)all=all && word(ram,w.address)==w.patched;
    check(all,"every word patched");
    applyGamePatch(ram,wide); // Applied again: already patched words stay.
    check(word(ram,wide.words[0].address)==wide.words[0].patched,"applying twice keeps the patch");
    // A word that is neither the original nor the patched one is another build: refused, nothing guessed.
    put(ram,wide.words[3].address,0x12345678u);
    check(throws([&]{applyGamePatch(ram,wide);}),"a foreign word is refused");
    check(throws([&]{gamePatch("sixty");}),"an unknown name is refused");
    check(parseGamePatches("").empty(),"an empty list applies none");
    check(parseGamePatches("widescreen,widescreen").size()==1,"a name given twice applies once");
    check(throws([&]{parseGamePatches("widescreen,");}),"an empty name is refused");
    std::vector<std::uint8_t> small(16);
    check(throws([&]{applyGamePatch(small,wide);}),"an address outside the memory is refused");
    std::cout<<"Game patch tests passed\n";
    return 0;
}
