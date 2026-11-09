// PCSX2-compatible texture names (gs/textures/gs_texture_names.h): which bytes are hashed in which order,
// the palette as read at CSA, the format word, and the names of textures drawn after image transfers.
#include "gs/textures/gs_texture_names.h"
#include "gs/cpu/gs_command_sink.h"
#define XXH_INLINE_ALL
#include "xxhash.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <source_location>
#include <vector>

namespace {
void require(bool ok,std::source_location at=std::source_location::current()) {
    if(!ok) {std::cerr<<"texture names: check failed at line "<<at.line()<<'\n';std::exit(1);}
}
std::uint64_t tex0(unsigned tbp,unsigned tbw,unsigned psm,unsigned tw,unsigned th,unsigned cbp=0,unsigned csa=0,unsigned cld=0) {
    return std::uint64_t(tbp)|(std::uint64_t(tbw)<<14)|(std::uint64_t(psm)<<20)|(std::uint64_t(tw)<<26)|(std::uint64_t(th)<<30)
        |(std::uint64_t(cbp)<<37)|(std::uint64_t(csa)<<56)|(std::uint64_t(cld)<<61);
}
// A host->local transfer of w x h pixels in format psm to (bp, bw) at 0,0 with the given data.
void upload(GsTextureNames& names,unsigned bp,unsigned bw,unsigned psm,unsigned w,unsigned h,const std::vector<std::uint8_t>& data) {
    names.reg(0x50,(std::uint64_t(bp)<<32)|(std::uint64_t(bw)<<48)|(std::uint64_t(psm)<<56));
    names.reg(0x51,0);
    names.reg(0x52,std::uint64_t(w)|(std::uint64_t(h)<<32));
    names.reg(0x53,0);
    names.image(data);
}
}

int main() {
    {
        // A 4-bit 64 x 32 texture is four 32 x 16 blocks, hashed in block order: blocks 0 and 2 across the
        // first block row (4-bit pages place blocks 0, 2, 8, 10 across), then 1 and 3.
        GsTextureNames names;
        std::vector<std::uint8_t> pixels(64*32/2);
        for(std::size_t i=0;i<pixels.size();++i)pixels[i]=std::uint8_t(i*7+3);
        upload(names,0x100,2,0x14,64,32,pixels);
        require(names.uploaded_pixels==64*32 && !names.unsupported_uploads);
        const auto& m=names.memory();
        std::vector<std::uint8_t> blocks;
        for(const unsigned b:{0u,2u,1u,3u}) {
            const auto* p=reinterpret_cast<const std::uint8_t*>(&m[(0x100+b)*64]);
            blocks.insert(blocks.end(),p,p+256);
        }
        const auto n=names.name(tex0(0x100,2,0x14,6,5),0);
        require(n && n->has_palette);
        require(n->texture==XXH3_64bits(blocks.data(),blocks.size()));
        require(n->format==(0x14u|(6u<<6)|(5u<<10)));
        // The index of pixel (5, 9) is where the 4-bit layout puts it.
        const auto nibble=GsCommandSink::t4Nibble(0x100,2,5,9);
        const unsigned byte=pixels[(9*64+5)/2];
        require(((m[nibble/8]>>(nibble%8*4))&15u)==((9*64+5)%2==0?byte&15u:byte>>4));
    }
    {
        // An 8 x 8 4-bit texture is smaller than a block: hashed as 64 index bytes, row by row.
        GsTextureNames names;
        std::vector<std::uint8_t> pixels(32);
        for(std::size_t i=0;i<pixels.size();++i)pixels[i]=std::uint8_t(i*13+1);
        upload(names,0x200,2,0x14,8,8,pixels);
        std::vector<std::uint8_t> indices;
        for(unsigned i=0;i<64;++i)indices.push_back(std::uint8_t(i%2?pixels[i/2]>>4:pixels[i/2]&15));
        const auto n=names.name(tex0(0x200,2,0x14,3,3),0);
        require(n && n->texture==XXH3_64bits(indices.data(),indices.size()));
    }
    {
        // The palette: a 4-bit texture's 16 entries loaded (CLD 1) from 8 x 2 32-bit pixels into CSA 3's
        // place and hashed as 32-bit entries; the name reads them at CSA 3.
        GsTextureNames names;
        std::vector<std::uint8_t> colors(16*4);
        for(std::size_t i=0;i<colors.size();++i)colors[i]=std::uint8_t(i*5+11);
        upload(names,0x300,1,0x00,8,2,colors);
        names.reg(0x06,tex0(0x100,2,0x14,6,5,0x300,3,1));
        const auto n=names.name(tex0(0x100,2,0x14,6,5,0x300,3),0);
        require(n && n->palette==XXH3_64bits(colors.data(),colors.size()));
        // Another CSA reads other entries.
        require(names.name(tex0(0x100,2,0x14,6,5,0x300,4),0)->palette!=n->palette);
        require(names.clut()[3*16]==std::uint16_t(colors[0]|(colors[1]<<8)) && names.clut()[3*16+256]==std::uint16_t(colors[2]|(colors[3]<<8)));
    }
    {
        // A 24-bit texture is read out with TEXA's alpha, and TEXA is part of its format word; 32-bit
        // ones keep neither. Names print without leading zeros except the format word.
        GsTextureNames names;
        const auto n=names.name(tex0(0,1,0x01,3,3),0x0000003000008040ull);
        require(n && !n->has_palette && n->format==(0x01u|(3u<<6)|(3u<<10)|(0x40u<<15)|(1u<<23)|(0x30u<<24)));
        require(names.name(tex0(0,1,0x00,3,3),0x0000003000008040ull)->format==(3u<<6|3u<<10));
        GsTextureNames::Name text;text.texture=0x1abc;text.palette=0x2f;text.format=0x1a53;text.has_palette=true;
        require(text.text()=="1abc-2f-00001a53");
        text.has_palette=false;require(text.text()=="1abc-00001a53");
    }
    {
        // A draw names its texture once: TME primitives after a TEX0 write; again after an upload.
        GsTextureNames names;
        names.reg(0x06,tex0(0x100,2,0x14,6,5));
        names.reg(0x00,0x16); // Triangle strip with TME.
        names.reg(0x05,0);names.reg(0x05,0);names.reg(0x05,0);
        require(names.drawn.size()==1 && names.drawn.begin()->second==1);
        upload(names,0x100,2,0x14,64,32,std::vector<std::uint8_t>(1024,0x11));
        names.reg(0x05,0);
        require(names.drawn.size()==2);
        names.reg(0x00,0x06); // Without TME: nothing named.
        upload(names,0x100,2,0x14,64,32,std::vector<std::uint8_t>(1024,0x22));
        names.reg(0x05,0);
        require(names.drawn.size()==2);
        // Without count_drawn (HD textures without the naming log) draws are not named.
        names.count_drawn=false;
        names.reg(0x00,0x16);upload(names,0x100,2,0x14,64,32,std::vector<std::uint8_t>(1024,0x33));
        names.reg(0x05,0);
        require(names.drawn.size()==2);
    }
    {
        // A transfer equal to the last one into its pages is not written again; the name stays. Other data,
        // or a write into its pages in between, is written.
        GsTextureNames names;
        const unsigned psm=0x00; // 32-bit, 64 x 32: one page.
        const std::uint64_t tex0=0x400|(std::uint64_t(1)<<14)|(std::uint64_t(6)<<26)|(std::uint64_t(5)<<30);
        std::vector<std::uint8_t> a(64*32*4),b(64*32*4);
        for(std::size_t i=0;i<a.size();++i) {a[i]=std::uint8_t(i*7);b[i]=std::uint8_t(i*13+1);}
        upload(names,0x400,1,psm,64,32,a);
        const auto first=names.name(tex0,0)->texture;
        upload(names,0x400,1,psm,64,32,a);
        require(names.repeated_uploads==1 && names.name(tex0,0)->texture==first);
        upload(names,0x400,1,psm,64,32,b);
        const auto second=names.name(tex0,0)->texture;
        require(names.repeated_uploads==1 && second!=first);
        upload(names,0x400,1,psm,64,32,a); // Equal to an earlier transfer, but the page changed since: written.
        require(names.repeated_uploads==1 && names.name(tex0,0)->texture==first);
        // In two pieces, with a name asked in between: the first piece is written at once.
        names.reg(0x50,(std::uint64_t(0x400)<<32)|(std::uint64_t(1)<<48));names.reg(0x51,0);names.reg(0x52,64|(std::uint64_t(32)<<32));names.reg(0x53,0);
        names.image(std::span(b).first(b.size()/2));
        (void)names.name(tex0,0);
        names.image(std::span(b).subspan(b.size()/2));
        require(names.name(tex0,0)->texture==second);
        // The same CLUT load again from unchanged memory is not read again.
        const std::uint64_t t8=0x2000|(std::uint64_t(2)<<14)|(std::uint64_t(0x13)<<20)|(std::uint64_t(4)<<26)|(std::uint64_t(4)<<30)
            |(std::uint64_t(0x400)<<37)|(std::uint64_t(1)<<61);
        names.reg(0x06,t8);const auto loads=names.repeated_cluts;
        names.reg(0x06,t8);
        require(names.repeated_cluts==loads+1);
        upload(names,0x400,1,psm,64,32,a); // The CLUT's memory changed: read again.
        names.reg(0x06,t8);
        require(names.repeated_cluts==loads+1);
    }
    {
        // The page-table addresses equal the sink's for any start block, width and position.
        std::uint32_t seed=12345;
        auto next=[&]{seed=seed*1103515245u+12345u;return seed>>8;};
        bool same=true;
        for(int i=0;i<200000 && same;++i) {
            const unsigned bp=next()%16384,bw=1+next()%32,x=next()%2048,y=next()%2048;
            same=GsTextureNames::ct32Word(bp,bw,x,y)==GsCommandSink::ct32Word(bp,bw,x,y)
                && GsTextureNames::t8Byte(bp,bw*2,x,y)==GsCommandSink::t8Byte(bp,bw*2,x,y)
                && GsTextureNames::t4Nibble(bp,bw*2,x,y)==GsCommandSink::t4Nibble(bp,bw*2,x,y)
                && GsTextureNames::t8Byte(bp,bw,x,y)==GsCommandSink::t8Byte(bp,bw,x,y);
        }
        require(same);
    }
    std::cout<<"Texture name checks passed."<<std::endl;
}
