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
    {
        const auto at16x9=screenShapeWords(1778);
        check(at16x9.size()==wide.words.size(),"the screen's shape changes the widescreen patch's words");
        for(std::size_t n=0;n<at16x9.size();n++)check(at16x9[n].patched>>16==wide.words[n].patched>>16,"at 16:9 the words are about the widescreen patch's");
        const auto at20x9=screenShapeWords(2222);
        std::vector<std::uint8_t> shaped(32u<<20);
        for(const auto& w:at20x9)put(shaped,w.address,w.original);
        applyGamePatch(shaped,GamePatch{"screen","",at20x9});
        check(word(shaped,0x0012BA08u)==0x3C013EE6u,"at 20:9 the 0.75 loaded becomes 0.45");
        for(const auto& w:screenShapeWords(1333))if(w.address>=0x00300000u)check(w.patched>>16==w.original>>16,"at 4:3 the data stays about the original");
    }
    std::cout<<"Game patch tests passed\n";
    return 0;
}
