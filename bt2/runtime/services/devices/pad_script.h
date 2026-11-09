#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Deterministic controller input for repeatable probe runs: a list of "FIELD:buttons" steps, each
// holding its buttons from that logical IOP field until the next step. Buttons are joined with '+';
// '-' releases everything. Example: BT2_PAD_SCRIPT="700:left,706:-,720:cross,726:-".
class PadScript {
public:
    // Wire button bits (active-low in the reply): byte 3 then byte 4 of a poll reply.
    static constexpr std::array<std::string_view,16> names{"select","l3","r3","start","up","right","down","left",
        "l2","r2","l1","r1","triangle","circle","cross","square"};
    static PadScript parse(std::string_view text) {
        PadScript script;unsigned previous=0;
        while(!text.empty()) {
            const auto comma=text.find(',');const auto step=text.substr(0,comma);
            text=comma==std::string_view::npos?std::string_view{}:text.substr(comma+1);
            const auto colon=step.find(':');
            if(colon==std::string_view::npos || !colon)throw std::runtime_error("Pad script step needs FIELD:buttons: "+std::string(step));
            unsigned field=0;
            for(char c:step.substr(0,colon)) {
                if(c<'0' || c>'9' || field>100000000)throw std::runtime_error("Invalid pad script field: "+std::string(step));
                field=field*10+unsigned(c-'0');
            }
            if(!field || field<=previous)throw std::runtime_error("Pad script fields must start at 1 and increase: "+std::string(step));
            std::uint16_t pressed=0;auto buttons=step.substr(colon+1);
            if(buttons!="-") {
                while(!buttons.empty()) {
                    const auto plus=buttons.find('+');const auto name=buttons.substr(0,plus);
                    buttons=plus==std::string_view::npos?std::string_view{}:buttons.substr(plus+1);
                    unsigned bit=0;while(bit<names.size() && names[bit]!=name)++bit;
                    if(bit==names.size())throw std::runtime_error("Unknown pad button: "+std::string(name));
                    pressed|=std::uint16_t(1u<<bit);
                }
            }
            script.steps_.emplace_back(field,pressed);previous=field;
        }
        return script;
    }
    // Pressed-button mask (active-high) starting at this field, if a step begins there.
    std::optional<std::uint16_t> at(unsigned field) const {
        for(const auto& [start,pressed]:steps_)if(start==field)return pressed;
        return std::nullopt;
    }
    bool empty() const {return steps_.empty();}
    std::size_t size() const {return steps_.size();}
private:
    std::vector<std::pair<unsigned,std::uint16_t>> steps_;
};
