#pragma once
#include <cstdint>
#include <fstream>
#include <string>
#include <stdexcept>
#include <vector>

// A presented image in BGRA (host order for Win32 DIBs).
struct DisplayFrame {
    unsigned width=0,height=0;
    std::vector<std::uint32_t> bgra;
    unsigned unwritten=0;
};

// Writes a frame as a binary PPM (diagnostic snapshots; rendered frames stay in ignored work/).
inline void writePpm(const std::string& path,const DisplayFrame& f) {
    std::ofstream out(path,std::ios::binary);
    out<<"P6\n"<<f.width<<' '<<f.height<<"\n255\n";
    for(const auto p:f.bgra) {const char rgb[3]={char((p>>16)&255),char((p>>8)&255),char(p&255)};out.write(rgb,3);}
    if(!out)throw std::runtime_error("PPM write failed: "+path);
}
