#include "ee/game_patches.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace {
// Widescreen 16:9 from PCSX2's patch database (patches/SLUS-21441_FE961D28.pnach): the projections'
// width (433 to about 577) and the two horizontal scales next to them (7/6 to 14/9), the 0.75 loaded at
// 0x12BA08 (to 0.5625) and the 1.0 loaded at 0x25B694 (to 0.75). The port adds the story map's
// projection at 0x386D94, which the PCSX2 patch leaves at 4:3 (the map then shows stretched). The same
// words are in config/game_patches.json, where the generator takes the two instructions from.
constexpr GamePatchWord widescreen[]{
    {0x0012BA08u,0x3C013F40u,0x3C013F10u},
    {0x0025B694u,0x3C013F80u,0x3C013F40u},
    {0x00386D94u,0x43D88000u,0x4410553Eu},
    {0x00386DA0u,0x43D88000u,0x4410553Eu},
    {0x00386DA4u,0x3F955555u,0x3FC71C55u},
    {0x00386DC8u,0x3F955555u,0x3FC71C55u},
    {0x00386DD8u,0x43D88000u,0x4410553Eu},
    {0x00386DF4u,0x43D88000u,0x4410553Eu},
    {0x00386E00u,0x43D88000u,0x4410553Eu},
};
constexpr GamePatch patches[]{
    {"widescreen","16:9 picture (PCSX2 patch database, SLUS-21441 Widescreen 16:9)",widescreen},
};
}

std::span<const GamePatch> gamePatches() {return patches;}

const GamePatch& gamePatch(std::string_view name) {
    for(const auto& patch:patches)if(patch.name==name)return patch;
    std::string known;
    for(const auto& patch:patches)known+=(known.empty()?"":", ")+std::string(patch.name);
    throw std::runtime_error("Unknown game patch \""+std::string(name)+"\" (there are: "+known+")");
}

void applyGamePatch(std::span<std::uint8_t> memory,const GamePatch& patch) {
    for(const auto& word:patch.words) {
        if(word.address%4 || word.address+4>memory.size())throw std::runtime_error("Game patch "+std::string(patch.name)+" names an address outside main RAM");
        std::uint32_t value;std::memcpy(&value,memory.data()+word.address,4);
        if(value==word.patched)continue;
        if(value!=word.original) {
            std::ostringstream message;
            message<<"Game patch "<<patch.name<<": the word at 0x"<<std::hex<<word.address<<" is 0x"<<value<<", not the original 0x"<<word.original;
            throw std::runtime_error(message.str());
        }
        std::memcpy(memory.data()+word.address,&word.patched,4);
    }
}

std::vector<const GamePatch*> parseGamePatches(std::string_view list) {
    std::vector<const GamePatch*> chosen;
    if(list.empty())return chosen;
    for(;;) {
        const auto comma=list.find(',');
        const auto name=list.substr(0,comma);
        if(name.empty())throw std::runtime_error("BT2_GAME_PATCHES has an empty name");
        const GamePatch* patch=&gamePatch(name);
        if(std::find(chosen.begin(),chosen.end(),patch)==chosen.end())chosen.push_back(patch);
        if(comma==std::string_view::npos)return chosen;
        list=list.substr(comma+1);
    }
}
