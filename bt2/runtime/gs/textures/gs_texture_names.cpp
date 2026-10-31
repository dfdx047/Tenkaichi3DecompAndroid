#include "gs/textures/gs_texture_names.h"
#include "gs/cpu/gs_command_sink.h"
#define XXH_INLINE_ALL
#include "xxhash.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
constexpr unsigned psm_ct32=0x00,psm_ct24=0x01,psm_t8=0x13,psm_t4=0x14,psm_t8h=0x1B,psm_t4hl=0x24,psm_t4hh=0x2C;
constexpr std::size_t memory_words=1024*1024,max_blocks=16384;

// What naming needs of a texture format: block size, palette entries, whether its bits fill a
// 32-bit pixel (else it is hashed as read out), and the page geometry of its blocks.
struct Format {unsigned block_w,block_h,palette;bool full;unsigned page_w,page_h,blocks_w,blocks_h;};
std::optional<Format> format(unsigned psm) {
    switch(psm) {
    case psm_ct32:return Format{8,8,0,true,64,32,8,4};
    case psm_ct24:return Format{8,8,0,false,64,32,8,4};
    case psm_t8:return Format{16,16,256,true,128,64,8,4};
    case psm_t4:return Format{32,16,16,true,128,128,4,8};
    case psm_t8h:return Format{8,8,256,false,64,32,8,4};
    case psm_t4hl:case psm_t4hh:return Format{8,8,16,false,64,32,8,4};
    default:return std::nullopt;
    }
}
// A block's place in its page (public GS layout, as gs_command_sink.h places it): pages of 8 x 4 blocks
// for the 32-bit and 8-bit formats, 4 x 8 for 4-bit.
unsigned blockInPage(unsigned psm,unsigned bx,unsigned by) {
    if(psm==psm_t4)return (by&1)|((bx&1)<<1)|((by&2)<<1)|((bx&2)<<2)|((by&4)<<2);
    return (bx&1)|((by&1)<<1)|((bx&2)<<1)|((by&2)<<2)|((bx&4)<<2);
}
unsigned log2(unsigned v) {unsigned n=0;while(v>1){v>>=1;++n;}return n;}

// Each format's place within one page (word, byte or nibble), from the sink's own layout functions.
// An address is the start block's units, plus whole pages, plus this: the layouts are linear in them.
struct PageLayouts {
    std::array<std::uint16_t,32*64> ct32;std::array<std::uint16_t,64*128> t8;std::array<std::uint16_t,128*128> t4;
    PageLayouts() {
        for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)ct32[y*64+x]=std::uint16_t(GsCommandSink::ct32Word(0,1,x,y));
        for(unsigned y=0;y<64;++y)for(unsigned x=0;x<128;++x)t8[y*128+x]=std::uint16_t(GsCommandSink::t8Byte(0,2,x,y));
        for(unsigned y=0;y<128;++y)for(unsigned x=0;x<128;++x)t4[y*128+x]=std::uint16_t(GsCommandSink::t4Nibble(0,2,x,y));
    }
};
const PageLayouts page_layouts; // Built at start: no initialization check on each pixel.
const PageLayouts& layouts() {return page_layouts;}
}

std::size_t GsTextureNames::ct32Word(unsigned bp,unsigned bw,unsigned x,unsigned y) {
    return (std::size_t(bp)*64+(std::size_t(y/32)*bw+x/64)*2048+layouts().ct32[(y%32)*64+x%64])%memory_words;
}
std::size_t GsTextureNames::t8Byte(unsigned bp,unsigned bw,unsigned x,unsigned y) {
    return (std::size_t(bp)*256+(std::size_t(y/64)*(bw/2)+x/128)*8192+layouts().t8[(y%64)*128+x%128])%(memory_words*4);
}
std::size_t GsTextureNames::t4Nibble(unsigned bp,unsigned bw,unsigned x,unsigned y) {
    return (std::size_t(bp)*512+(std::size_t(y/128)*(bw/2)+x/128)*16384+layouts().t4[(y%128)*128+x%128])%(memory_words*8);
}

std::string GsTextureNames::Name::textUsed() const {
    Name used=*this;used.palette=palette_used;
    return used.text();
}

std::string GsTextureNames::Name::text() const {
    char buffer[64];
    if(has_palette)std::snprintf(buffer,sizeof(buffer),"%llx-%llx-%08x",static_cast<unsigned long long>(texture),static_cast<unsigned long long>(palette),format);
    else std::snprintf(buffer,sizeof(buffer),"%llx-%08x",static_cast<unsigned long long>(texture),format);
    return buffer;
}

void GsTextureNames::reg(unsigned reg,std::uint64_t value) {
    if(reg>=regs_.size())return;
    if(reg==0x16 || reg==0x17) {
        // TEX2: PSM and the CLUT fields of TEX0 (CBP, CPSM, CSM, CSA, CLD).
        constexpr std::uint64_t mask=(std::uint64_t(63)<<20)|(~std::uint64_t(0)<<37);
        const unsigned tex0=reg-0x16+6;
        regs_[reg]=value;reg=tex0;value=(regs_[tex0]&~mask)|(value&mask);
    }
    regs_[reg]=value;
    if(reg==0x53) {startTransfer(value);return;}
    if(reg==0x06 || reg==0x07) {
        // CLD: 1 to 3 load the CLUT (2 and 3 also keep CBP), 4 and 5 load when CBP differs from the kept one.
        const unsigned cld=unsigned((value>>61)&7);const std::uint32_t cbp=std::uint32_t((value>>37)&0x3FFF);
        const bool load=(cld>=1 && cld<=3) || (cld==4 && cbp!=cbp_[0]) || (cld==5 && cbp!=cbp_[1]);
        if(cld==2 || cld==4)cbp_[0]=cbp;
        if(cld==3 || cld==5)cbp_[1]=cbp;
        if(load)loadClut(value);
        return;
    }
    if(reg==0x00)vertex_count_=0; // PRIM starts a new primitive.
    if(reg==0x04 || reg==0x05) {vertex(value);kick();}
}

void GsTextureNames::startTransfer(std::uint64_t dir) {
    writePending(); // An incomplete transfer: what arrived is in memory.
    transfer_=false;partial_=0;partial_bytes_=0;direct_=false;
    const unsigned direction=unsigned(dir&3);
    if(direction==2) {++local_copies;all_written_=++generation_;return;}
    if(direction!=0)return;
    const auto bitblt=regs_[0x50],pos=regs_[0x51],size=regs_[0x52];
    psm_=unsigned((bitblt>>56)&63);bp_=unsigned((bitblt>>32)&0x3FFF);bw_=unsigned((bitblt>>48)&63);
    x0_=unsigned((pos>>32)&0x7FF);y0_=unsigned((pos>>48)&0x7FF);w_=unsigned(size&0xFFF);h_=unsigned((size>>32)&0xFFF);x_=y_=0;
    const bool written=psm_==psm_ct32 || psm_==psm_ct24 || psm_==psm_t8 || psm_==psm_t4 || psm_==psm_t8h || psm_==psm_t4hl || psm_==psm_t4hh;
    if(!written || ((pos>>59)&3) || !w_ || !h_) {++unsupported_uploads;all_written_=++generation_;return;}
    transfer_=true;++generation_;++uploads;
    const unsigned bits=psm_==psm_ct32?32:psm_==psm_ct24?24:(psm_==psm_t8 || psm_==psm_t8h)?8:4;
    transfer_bytes_=(std::size_t(w_)*h_*bits+7)/8;
    upload_key_={bitblt,pos,size};
}

void GsTextureNames::finishUpload() {
    const auto hash=XXH3_64bits(pending_.data(),pending_.size());
    if(const auto found=uploads_.find(upload_key_);found!=uploads_.end() && found->second.hash==hash && found->second.generation>=all_written_
       && std::all_of(found->second.pages.begin(),found->second.pages.end(),[&](std::uint16_t page){return page_written_[page]==found->second.generation;})) {
        ++repeated_uploads;uploaded_pixels+=std::uint64_t(w_)*h_;
        pending_.clear();transfer_=false;
        return;
    }
    written_pages_.clear();
    writeBytes(pending_);
    pending_.clear();
    std::sort(written_pages_.begin(),written_pages_.end());
    written_pages_.erase(std::unique(written_pages_.begin(),written_pages_.end()),written_pages_.end());
    uploads_[upload_key_]={hash,generation_,written_pages_};
}

void GsTextureNames::writePending() {
    if(pending_.empty())return;
    direct_=true;
    writeBytes(pending_);
    pending_.clear();
}

void GsTextureNames::image(std::span<const std::uint8_t> data) {
    if(!transfer_)return;
    if(direct_) {writeBytes(data);return;}
    const std::size_t take=std::min(data.size(),transfer_bytes_-pending_.size());
    pending_.insert(pending_.end(),data.begin(),data.begin()+std::ptrdiff_t(take));
    if(pending_.size()==transfer_bytes_)finishUpload();
}

void GsTextureNames::writeBytes(std::span<const std::uint8_t> data) {
    ++generation_; // A hash taken between two pieces of a transfer is not kept past the next one.
    const auto& tables=layouts();
    auto* const bytes=reinterpret_cast<std::uint8_t*>(memory_.data()); // GS memory is little-endian words.
    std::size_t at=0,last_page=~std::size_t(0);
    const auto mark=[&](std::size_t word) {
        const std::size_t page=word/2048;
        if(page==last_page)return;
        last_page=page;
        if(page_written_[page]!=generation_) {page_written_[page]=generation_;written_pages_.push_back(std::uint16_t(page));}
    };
    // The next pixel of the stream; a pixel split between two pieces waits in partial_ (for 4-bit
    // formats partial_ holds the second pixel of a byte).
    const auto take=[&](std::uint32_t& value)->bool {
        switch(psm_) {
        case psm_ct32:case psm_ct24: {
            const unsigned need=psm_==psm_ct32?4:3;
            if(!partial_bytes_ && at+need<=data.size()) {value=0;std::memcpy(&value,data.data()+at,need);at+=need;return true;}
            while(partial_bytes_<need) {
                if(at==data.size())return false;
                partial_|=std::uint32_t(data[at++])<<(8*partial_bytes_++);
            }
            value=partial_;partial_=0;partial_bytes_=0;return true;
        }
        case psm_t8:case psm_t8h:
            if(at==data.size())return false;
            value=data[at++];return true;
        default: // Two pixels a byte, the low nibble first.
            if(partial_bytes_) {value=partial_;partial_bytes_=0;return true;}
            if(at==data.size())return false;
            value=data[at]&15u;partial_=data[at++]>>4;partial_bytes_=1;return true;
        }
    };
    while(transfer_) {
        // A row's place: its pages' start and its line of each page's layout (gs_command_sink.h's
        // functions, as ct32Word, t8Byte and t4Nibble above).
        const unsigned y=(y0_+y_)%2048;
        const std::size_t ct32_row=std::size_t(bp_)*64+std::size_t(y/32)*bw_*2048;const auto* ct32_line=&tables.ct32[(y%32)*64];
        const std::size_t t8_row=std::size_t(bp_)*256+std::size_t(y/64)*(bw_/2)*8192;const auto* t8_line=&tables.t8[(y%64)*128];
        const std::size_t t4_row=std::size_t(bp_)*512+std::size_t(y/128)*(bw_/2)*16384;const auto* t4_line=&tables.t4[(y%128)*128];
        for(;x_<w_;++x_) {
            std::uint32_t value;
            if(!take(value))return;
            const unsigned x=(x0_+x_)%2048;
            switch(psm_) {
            case psm_t8: {
                const std::size_t byte=(t8_row+std::size_t(x/128)*8192+t8_line[x%128])%(memory_words*4);
                bytes[byte]=std::uint8_t(value);mark(byte/4);
                break;
            }
            case psm_t4: {
                const std::size_t nibble=(t4_row+std::size_t(x/128)*16384+t4_line[x%128])%(memory_words*8);
                auto& byte=bytes[nibble/2];
                byte=std::uint8_t(nibble%2?(byte&0x0Fu)|(value<<4):(byte&0xF0u)|value);mark(nibble/8);
                break;
            }
            default: {
                const std::size_t at_word=(ct32_row+std::size_t(x/64)*2048+ct32_line[x%64])%memory_words;
                auto& word=memory_[at_word];
                switch(psm_) {
                case psm_ct32:word=value;break;
                case psm_ct24:word=(word&0xFF000000u)|(value&0xFFFFFFu);break;
                case psm_t8h:word=(word&0x00FFFFFFu)|(value<<24);break;
                case psm_t4hl:word=(word&0xF0FFFFFFu)|(value<<24);break;
                default:word=(word&0x0FFFFFFFu)|(value<<28);break; // T4HH
                }
                mark(at_word);
            }
            }
            ++uploaded_pixels;
        }
        x_=0;
        if(++y_==h_)transfer_=false;
    }
}

void GsTextureNames::vertex(std::uint64_t xyz) {
    vertices_[vertex_count_++%3]=xyz;
    if(clut_pages_.none())return;
    const auto prim=(regs_[0x1A]&1)?regs_[0x00]:regs_[0x1B];
    const unsigned type=unsigned(prim&7),context=unsigned((prim>>9)&1);
    // Whether this vertex completes a primitive (vertices count from the last PRIM write), and how many
    // vertices it has: 1 for points, 2 for lines and sprites, 3 for triangles.
    const unsigned count=type==0?1u:(type==6 || type==1 || type==2)?2u:3u;
    const bool draws=type==0 || ((type==1 || type==6)?vertex_count_%2==0:type==3?vertex_count_%3==0:vertex_count_>=count);
    if(!draws || vertex_count_<count)return;
    const auto frame=regs_[0x4C+context];
    const unsigned fbp=unsigned(frame&0x1FF),fbw=unsigned((frame>>16)&63),fpsm=unsigned((frame>>24)&63);
    unsigned page_w=64,page_h;
    if(fpsm==0x00 || fpsm==0x01 || fpsm==0x30 || fpsm==0x31)page_h=32;
    else if(fpsm==0x02 || fpsm==0x0A || fpsm==0x32 || fpsm==0x3A)page_h=64;
    else return;
    const auto offset=regs_[0x18+context];
    const int ofx=int(offset&0xFFFF),ofy=int((offset>>32)&0xFFFF);
    int x0=INT32_MAX,y0=INT32_MAX,x1=INT32_MIN,y1=INT32_MIN;
    for(unsigned k=0;k<count;++k) {
        const auto v=vertices_[(vertex_count_-1-k)%3];
        const int x=(int(v&0xFFFF)-ofx)>>4,y=(int((v>>16)&0xFFFF)-ofy)>>4;
        x0=std::min(x0,x);y0=std::min(y0,y);x1=std::max(x1,x);y1=std::max(y1,y);
    }
    if(x1<0 || y1<0)return;
    x0=std::max(x0,0);y0=std::max(y0,0);
    if(x1-x0>2048 || y1-y0>2048)return;
    for(int py=y0/int(page_h);py<=y1/int(page_h);++py)
        for(int px=x0/int(page_w);px<=x1/int(page_w);++px) {
            const unsigned page=(fbp+unsigned(py)*std::max(fbw,1u)+unsigned(px))%512;
            if(clut_pages_[page])page_drawn_[page]=++generation_;
        }
}

std::vector<unsigned> GsTextureNames::drawnPalettePages() const {
    std::vector<unsigned> pages;
    for(unsigned page=0;page<512;++page)if(clut_pages_[page] && pageDrawn(page))pages.push_back(page);
    return pages;
}

void GsTextureNames::takeDrawnPage(unsigned page,const std::uint32_t* words) {
    if(drawn_copy_.empty())drawn_copy_.resize(memory_.size());
    std::copy_n(words,2048,drawn_copy_.begin()+std::ptrdiff_t(std::size_t(page%512)*2048));
    copied_.set(page%512);++generation_;
}

void GsTextureNames::loadClut(std::uint64_t tex0) {
    const unsigned psm=unsigned((tex0>>20)&63),cbp=unsigned((tex0>>37)&0x3FFF),cpsm=unsigned((tex0>>51)&15);
    const unsigned csm=unsigned((tex0>>55)&1),csa=unsigned((tex0>>56)&31);
    const auto f=format(psm);
    if(!f || !f->palette)return;
    ++clut_loads;
    writePending();
    const unsigned first_page=(cbp/32)%512,last_page=((cbp+3)/32)%512;
    clut_pages_.set(first_page);clut_pages_.set(last_page);
    const bool from_drawn=pageDrawn(first_page) || pageDrawn(last_page);
    // The same load as the last one from memory nothing wrote since: the CLUT already holds it.
    const std::uint64_t key=std::uint64_t(f->palette)|(std::uint64_t(cbp)<<9)|(std::uint64_t(cpsm)<<23)|(std::uint64_t(csm)<<27)|(std::uint64_t(csa)<<28);
    if(!from_drawn && key==clut_key_ && all_written_<=clut_generation_ && page_written_[first_page]<=clut_generation_ && page_written_[last_page]<=clut_generation_) {
        ++repeated_cluts;return;
    }
    ++generation_;
    clut_key_=key;clut_generation_=generation_;
    // CSM1 with 32-bit entries: 16 entries as 8 x 2 pixels at CSA's 16; 256 as 16 x 16 pixels with index
    // bits 3 and 4 swapped in the layout, groups of 16 from CSA's on.
    if(csm || (cpsm!=0 && cpsm!=1)) {++unsupported_cluts;return;}
    auto put=[&](unsigned entry,std::uint32_t value){clut_[entry]=std::uint16_t(value);clut_[entry+256]=std::uint16_t(value>>16);};
    auto word=[&](std::size_t w)->std::uint32_t {
        const unsigned page=unsigned(w/2048);
        return from_drawn && pageDrawn(page) && copied_[page]?drawn_copy_[w]:memory_[w]; // A drawn page: its latest copy.
    };
    if(f->palette==16) {
        for(unsigned k=0;k<16;++k)put((csa&15)*16+k,word(ct32Word(cbp,1,k%8,k/8)));
    } else {
        for(unsigned i=(csa&15)*16;i<256;++i) {
            const unsigned s=(i&~0x18u)|((i&0x08u)<<1)|((i&0x10u)>>1);
            put(i,word(ct32Word(cbp,1,s%16,s/16)));
        }
    }
}

void GsTextureNames::kick() {
    if(!count_drawn)return;
    const auto prim=(regs_[0x1A]&1)?regs_[0x00]:regs_[0x1B];
    if(!((prim>>4)&1))return; // TME
    const unsigned context=unsigned((prim>>9)&1);
    const auto tex0=regs_[6+context]&~(std::uint64_t(7)<<61),texa=regs_[0x3B];
    if(named_generation_[context]==generation_ && named_tex0_[context]==tex0 && named_texa_[context]==texa)return;
    named_generation_[context]=generation_;named_tex0_[context]=tex0;named_texa_[context]=texa;
    if(const auto n=name(tex0,texa))++drawn[n->has_palette?n->textUsed()+"/"+n->text():n->text()];
}

std::optional<GsTextureNames::Name> GsTextureNames::name(std::uint64_t tex0,std::uint64_t texa) {
    writePending();
    const unsigned psm=unsigned((tex0>>20)&63);
    const unsigned tw=unsigned((tex0>>26)&15),th=unsigned((tex0>>30)&15);
    const unsigned cpsm=unsigned((tex0>>51)&15),csa=unsigned((tex0>>56)&31);
    const auto f=format(psm);
    if(!f || tw>10 || th>10 || (f->palette && cpsm!=0 && cpsm!=1)) {++unsupported_textures;++unsupported_formats[psm];return std::nullopt;}
    const unsigned ta0=unsigned(texa&0xFF),aem=unsigned((texa>>15)&1),ta1=unsigned((texa>>32)&0xFF);
    Name n;unsigned lowest=0,highest=255;
    n.texture=textureHash(tex0,texa,&lowest,&highest);
    n.format=psm|(tw<<6)|(th<<10);
    if(!f->palette && psm!=psm_ct32)n.format|=(ta0<<15)|(aem<<23)|(ta1<<24);
    if(f->palette) {
        std::array<std::uint32_t,256> entries{};
        auto entry=[&](unsigned i){return std::uint32_t(clut_[i])|(std::uint32_t(clut_[i+256])<<16);};
        if(f->palette==16)for(unsigned k=0;k<16;++k)entries[k]=entry((csa&15)*16+k);
        else for(unsigned i=0;i<256;i+=16) {
            // Groups past the last one clamp to it instead of wrapping.
            const unsigned from=std::min(i+(csa&15)*16,240u);
            for(unsigned k=0;k<16;++k)entries[i+k]=entry(from+k);
        }
        n.palette=XXH3_64bits(entries.data(),std::size_t(f->palette)*4);n.has_palette=true;
        highest=std::min(highest,f->palette-1);lowest=std::min(lowest,highest);
        n.palette_used=XXH3_64bits(entries.data()+lowest,std::size_t(highest-lowest+1)*4);
    }
    return n;
}

std::uint64_t GsTextureNames::textureHash(std::uint64_t tex0,std::uint64_t texa,unsigned* lowest,unsigned* highest) {
    const unsigned psm=unsigned((tex0>>20)&63),tbp=unsigned(tex0&0x3FFF),tbw=unsigned((tex0>>14)&63);
    const unsigned tw=unsigned((tex0>>26)&15),th=unsigned((tex0>>30)&15);
    const auto f=*format(psm);
    const unsigned width=1u<<tw,height=1u<<th;
    const unsigned ta0=unsigned(texa&0xFF),aem=unsigned((texa>>15)&1);
    // The pages the texture lies in: rows of pages TBW apart (half for 8-bit and 4-bit), each as many
    // pages across as the width needs plus one when TBP is not at a page's start.
    const unsigned pages_across=tbw>>(log2(f.page_w)-6);
    auto pages=[&](auto&& each) {
        const unsigned rows=(height+f.page_h-1)/f.page_h,across=(width+f.page_w-1)/f.page_w+(tbp%32?1:0);
        for(unsigned r=0;r<rows;++r)for(unsigned c=0;c<across;++c)each((tbp/32+r*std::max(pages_across,1u)+c)%512);
    };
    const auto key=std::make_pair(tex0&0x3FFFFFFFFull,psm==psm_ct24?texa&0x80FFull:0ull);
    if(auto found=hashed_.find(key);found!=hashed_.end() && found->second.generation>=all_written_) {
        bool clean=true;
        pages([&](unsigned page){if(page_written_[page]>found->second.generation)clean=false;});
        if(clean) {
            if(lowest)*lowest=found->second.lowest;
            if(highest)*highest=found->second.highest;
            return found->second.hash;
        }
    }
    // The palette indices the texture uses (for the used entries' palette hash): a texture hashed by whole
    // blocks holds exactly its pixels' indices in them (taken while hashing); others are read pixel by pixel.
    unsigned low=0,high=255;
    const bool whole_blocks=width>=f.block_w && height>=f.block_h && f.full;
    if(f.palette && whole_blocks) {low=255;high=0;}
    if(f.palette && !whole_blocks) {
        const auto& tables=layouts();
        const auto* const bytes=reinterpret_cast<const std::uint8_t*>(memory_.data());
        low=255;high=0;
        for(unsigned y=0;y<height;++y) {
            const std::size_t ct32_row=std::size_t(tbp)*64+std::size_t(y/32)*tbw*2048;const auto* ct32_line=&tables.ct32[(y%32)*64];
            const auto word=[&](unsigned x){return memory_[(ct32_row+std::size_t(x/64)*2048+ct32_line[x%64])%memory_words];};
            const std::size_t t8_row=std::size_t(tbp)*256+std::size_t(y/64)*(tbw/2)*8192;const auto* t8_line=&tables.t8[(y%64)*128];
            const std::size_t t4_row=std::size_t(tbp)*512+std::size_t(y/128)*(tbw/2)*16384;const auto* t4_line=&tables.t4[(y%128)*128];
            for(unsigned x=0;x<width;++x) {
                unsigned index;
                switch(psm) {
                case psm_t8:index=bytes[(t8_row+std::size_t(x/128)*8192+t8_line[x%128])%(memory_words*4)];break;
                case psm_t4: {
                    const std::size_t nibble=(t4_row+std::size_t(x/128)*16384+t4_line[x%128])%(memory_words*8);
                    index=(bytes[nibble/2]>>(nibble%2*4))&15;break;
                }
                case psm_t8h:index=word(x)>>24;break;
                case psm_t4hl:index=(word(x)>>24)&15;break;
                default:index=word(x)>>28;break; // T4HH
                }
                low=std::min(low,index);high=std::max(high,index);
            }
        }
    }
    if(lowest)*lowest=low;
    if(highest)*highest=high;
    XXH3_state_t state;XXH3_64bits_reset(&state);
    if(width<f.block_w || height<f.block_h || !f.full) {
        // Read out row by row: the index for palette formats, RGBA otherwise (rows placed as in writeBytes).
        const auto& tables=layouts();
        const auto* const bytes=reinterpret_cast<const std::uint8_t*>(memory_.data());
        std::vector<std::uint8_t> row(std::size_t(width)*(f.palette?1:4));
        for(unsigned y=0;y<height;++y) {
            const std::size_t ct32_row=std::size_t(tbp)*64+std::size_t(y/32)*tbw*2048;const auto* ct32_line=&tables.ct32[(y%32)*64];
            const auto word=[&](unsigned x){return memory_[(ct32_row+std::size_t(x/64)*2048+ct32_line[x%64])%memory_words];};
            switch(psm) {
            case psm_t8: {
                const std::size_t t8_row=std::size_t(tbp)*256+std::size_t(y/64)*(tbw/2)*8192;const auto* t8_line=&tables.t8[(y%64)*128];
                for(unsigned x=0;x<width;++x)row[x]=bytes[(t8_row+std::size_t(x/128)*8192+t8_line[x%128])%(memory_words*4)];
                break;
            }
            case psm_t4: {
                const std::size_t t4_row=std::size_t(tbp)*512+std::size_t(y/128)*(tbw/2)*16384;const auto* t4_line=&tables.t4[(y%128)*128];
                for(unsigned x=0;x<width;++x) {
                    const std::size_t nibble=(t4_row+std::size_t(x/128)*16384+t4_line[x%128])%(memory_words*8);
                    row[x]=std::uint8_t((bytes[nibble/2]>>(nibble%2*4))&15);
                }
                break;
            }
            case psm_t8h:for(unsigned x=0;x<width;++x)row[x]=std::uint8_t(word(x)>>24);break;
            case psm_t4hl:for(unsigned x=0;x<width;++x)row[x]=std::uint8_t((word(x)>>24)&15);break;
            case psm_t4hh:for(unsigned x=0;x<width;++x)row[x]=std::uint8_t(word(x)>>28);break;
            default:
                for(unsigned x=0;x<width;++x) {
                    std::uint32_t pixel=word(x);
                    if(psm==psm_ct24) {const std::uint32_t rgb=pixel&0xFFFFFFu;pixel=rgb|((aem && !rgb)?0u:std::uint32_t(ta0)<<24);}
                    std::memcpy(&row[std::size_t(x)*4],&pixel,4);
                }
            }
            XXH3_64bits_update(&state,row.data(),row.size());
        }
    } else {
        // Whole blocks in block order: rows of blocks, each left to right; pages follow each other across
        // and TBW pages (half of it for the 8-bit and 4-bit formats) down.
        for(unsigned by=0;by<height/f.block_h;++by)
            for(unsigned bx=0;bx<width/f.block_w;++bx) {
                const std::size_t block=(tbp+(by/f.blocks_h)*32*pages_across+(bx/f.blocks_w)*32
                    +blockInPage(psm,bx%f.blocks_w,by%f.blocks_h))%max_blocks;
                XXH3_64bits_update(&state,&memory_[block*64],256);
                if(f.palette) {
                    const auto* b=reinterpret_cast<const std::uint8_t*>(&memory_[block*64]);
                    std::uint8_t lo=255,hi=0;
                    if(psm==psm_t8)for(unsigned k=0;k<256;++k){lo=std::min(lo,b[k]);hi=std::max(hi,b[k]);}
                    else for(unsigned k=0;k<256;++k){const std::uint8_t l=b[k]&15,h=b[k]>>4;lo=std::min({lo,l,h});hi=std::max({hi,l,h});}
                    low=std::min(low,unsigned(lo));high=std::max(high,unsigned(hi));
                }
            }
    }
    const auto hash=XXH3_64bits_digest(&state);
    hashed_[key]={hash,generation_,low,high};
    return hash;
}
