#include "ipu_block.h"
#include <cmath>
#include <cstddef>
#include <string>

namespace {
struct Coefficient {std::uint16_t code;std::uint8_t length,run,level;};
// Table B-14 (DCT coefficients table zero) without the sign bit, shortest codes first. End of block is
// '10', escape '0000 01'; a block's first coefficient codes run 0 / level 1 as '1'.
constexpr Coefficient coefficients[]={
    {3,2,0,1},{3,3,1,1},{4,4,0,2},{5,4,2,1},{5,5,0,3},{7,5,3,1},{6,5,4,1},
    {6,6,1,2},{7,6,5,1},{5,6,6,1},{4,6,7,1},{6,7,0,4},{4,7,2,2},{7,7,8,1},{5,7,9,1},
    {0x26,8,0,5},{0x21,8,0,6},{0x25,8,1,3},{0x24,8,3,2},{0x27,8,10,1},{0x23,8,11,1},{0x22,8,12,1},{0x20,8,13,1},
    {0x0A,10,0,7},{0x0C,10,1,4},{0x0B,10,2,3},{0x0F,10,4,2},{0x09,10,5,2},{0x0E,10,14,1},{0x0D,10,15,1},{0x08,10,16,1},
    {0x1D,12,0,8},{0x18,12,0,9},{0x13,12,0,10},{0x10,12,0,11},{0x1B,12,1,5},{0x14,12,2,4},{0x1C,12,3,3},{0x12,12,4,3},
    {0x1E,12,6,2},{0x15,12,7,2},{0x11,12,8,2},{0x1F,12,17,1},{0x1A,12,18,1},{0x19,12,19,1},{0x17,12,20,1},{0x16,12,21,1},
    {0x1A,13,0,12},{0x19,13,0,13},{0x18,13,0,14},{0x17,13,0,15},{0x16,13,1,6},{0x15,13,1,7},{0x14,13,2,5},{0x13,13,3,4},
    {0x12,13,5,3},{0x11,13,9,2},{0x10,13,10,2},{0x1F,13,22,1},{0x1E,13,23,1},{0x1D,13,24,1},{0x1C,13,25,1},{0x1B,13,26,1},
    {0x1F,14,0,16},{0x1E,14,0,17},{0x1D,14,0,18},{0x1C,14,0,19},{0x1B,14,0,20},{0x1A,14,0,21},{0x19,14,0,22},{0x18,14,0,23},
    {0x17,14,0,24},{0x16,14,0,25},{0x15,14,0,26},{0x14,14,0,27},{0x13,14,0,28},{0x12,14,0,29},{0x11,14,0,30},{0x10,14,0,31},
    {0x18,15,0,32},{0x17,15,0,33},{0x16,15,0,34},{0x15,15,0,35},{0x14,15,0,36},{0x13,15,0,37},{0x12,15,0,38},{0x11,15,0,39},
    {0x10,15,0,40},{0x1F,15,1,8},{0x1E,15,1,9},{0x1D,15,1,10},{0x1C,15,1,11},{0x1B,15,1,12},{0x1A,15,1,13},{0x19,15,1,14},
    {0x13,16,1,15},{0x12,16,1,16},{0x11,16,1,17},{0x10,16,1,18},{0x14,16,6,3},{0x1A,16,11,2},{0x19,16,12,2},{0x18,16,13,2},
    {0x17,16,14,2},{0x16,16,15,2},{0x15,16,16,2},{0x1F,16,27,1},{0x1E,16,28,1},{0x1D,16,29,1},{0x1C,16,30,1},{0x1B,16,31,1}};
// Table B-15 (DCT coefficients table one, intra blocks with intra_vlc_format): the codes of up to ten
// bits; longer codes are those of table zero. End of block is '0110', escape '0000 01'.
constexpr Coefficient coefficients_one[]={
    {0x2,2,0,1},{0x2,3,1,1},{0x6,3,0,2},{0x7,4,0,3},{0x5,5,2,1},{0x6,5,1,2},{0x7,5,3,1},{0x1C,5,0,4},{0x1D,5,0,5},
    {0x4,6,0,7},{0x5,6,0,6},{0x6,6,4,1},{0x7,6,5,1},{0x4,7,7,1},{0x5,7,8,1},{0x6,7,6,1},{0x7,7,2,2},
    {0x78,7,9,1},{0x79,7,1,3},{0x7A,7,10,1},{0x7B,7,0,8},{0x7C,7,0,9},
    {0x20,8,1,5},{0x21,8,11,1},{0x22,8,0,11},{0x23,8,0,10},{0x24,8,13,1},{0x25,8,12,1},{0x26,8,3,2},{0x27,8,1,4},
    {0xFA,8,0,12},{0xFB,8,0,13},{0xFC,8,2,3},{0xFD,8,4,2},{0xFE,8,0,14},{0xFF,8,0,15},
    {0x4,9,5,2},{0x5,9,14,1},{0x7,9,15,1},{0xC,10,2,4},{0xD,10,16,1}};
constexpr unsigned shared_code_length=12; // Table zero codes from this length on also belong to table one.
struct Pattern {std::uint16_t code;std::uint8_t length,value;};
// Table B-9, coded_block_pattern (bit 5: first luma block ... bit 0: Cr).
constexpr Pattern patterns[]={
    {7,3,60},{13,4,4},{12,4,8},{11,4,16},{10,4,32},
    {19,5,12},{18,5,48},{17,5,20},{16,5,40},{15,5,28},{14,5,44},{13,5,52},{12,5,56},{11,5,1},{10,5,61},{9,5,2},{8,5,62},
    {15,6,24},{14,6,36},{13,6,3},{12,6,63},
    {23,7,5},{22,7,9},{21,7,17},{20,7,33},{19,7,6},{18,7,10},{17,7,18},{16,7,34},
    {31,8,7},{30,8,11},{29,8,19},{28,8,35},{27,8,13},{26,8,49},{25,8,21},{24,8,41},{23,8,14},{22,8,50},{21,8,22},{20,8,42},
    {19,8,15},{18,8,51},{17,8,23},{16,8,43},{15,8,25},{14,8,37},{13,8,26},{12,8,38},{11,8,29},{10,8,45},{9,8,53},{8,8,57},
    {7,8,30},{6,8,46},{5,8,54},{4,8,58},{7,9,31},{6,9,47},{5,9,55},{4,9,59},{3,9,27},{2,9,39}};
// Tables B-12 and B-13, dct_dc_size: {code, length} indexed by size 0..11.
struct DcSize {std::uint16_t code;std::uint8_t length;};
constexpr DcSize luma_sizes[12]={{4,3},{0,2},{1,2},{5,3},{6,3},{14,4},{30,5},{62,6},{126,7},{254,8},{510,9},{511,9}};
constexpr DcSize chroma_sizes[12]={{0,2},{1,2},{2,2},{6,3},{14,4},{30,5},{62,6},{126,7},{254,8},{510,9},{1022,10},{1023,10}};

// The code tables are complete prefix codes; these sums catch a mistyped code or length.
constexpr unsigned coefficientCodeSpace() { // In units of 2^-17, with sign bits, end of block and escape.
    unsigned sum=(1u<<15)+(1u<<11);
    for(const auto& entry:coefficients)sum+=2u<<(16-entry.length);
    return sum;
}
static_assert(coefficientCodeSpace()==(1u<<17)-(1u<<5),"Table B-14 code space");
static_assert(sizeof coefficients/sizeof coefficients[0]==111,"Table B-14 entry count");
constexpr unsigned coefficientOneCodeSpace() {
    unsigned sum=(1u<<13)+(1u<<11);
    for(const auto& entry:coefficients_one)sum+=2u<<(16-entry.length);
    for(const auto& entry:coefficients)if(entry.length>=shared_code_length)sum+=2u<<(16-entry.length);
    return sum;
}
static_assert(coefficientOneCodeSpace()==(1u<<17)-(1u<<5),"Table B-15 code space");
constexpr bool patternsComplete() {
    unsigned sum=0;std::uint64_t seen=0;
    for(const auto& entry:patterns){sum+=1u<<(9-entry.length);seen|=1ull<<entry.value;}
    return sum==510 && seen==0xFFFFFFFFFFFFFFFEull;
}
static_assert(patternsComplete(),"Table B-9");
constexpr bool dcSizesComplete(const DcSize (&table)[12]) {
    unsigned sum=0;
    for(const auto& entry:table)sum+=1u<<(10-entry.length);
    return sum==1024;
}
static_assert(dcSizesComplete(luma_sizes) && dcSizesComplete(chroma_sizes),"Tables B-12/B-13");

constexpr std::uint8_t zigzag_scan[64]={
    0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
constexpr std::uint8_t alternate_scan[64]={
    0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
    51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63};

// Bit access that reports a dry input instead of returning short.
struct Bits {
    IpuInput& input;
    std::uint32_t peek(unsigned count) {if(!input.available(count))throw IpuInputDry();return input.peek(count);}
    std::uint32_t take(unsigned count) {const auto value=peek(count);input.skip(count);return value;}
};

int dcDifference(Bits& bits,const DcSize (&table)[12]) {
    const auto next=bits.peek(10);
    for(unsigned size=0;size<12;++size) {
        if((next>>(10-table[size].length))!=table[size].code)continue;
        bits.input.skip(table[size].length);
        if(!size)return 0;
        const int value=int(bits.take(size));
        return (value>>(size-1))?value:value-((1<<size)-1); // A leading 0 marks a negative difference.
    }
    throw std::runtime_error("IPU BDEC: invalid DC size code");
}

int clampCoefficient(int value) {return value<-2048?-2048:value>2047?2047:value;}

// AC coefficients (or all coefficients of a non-intra block) into `block` (row order), dequantised.
void coefficientsInto(Bits& bits,const IpuBlockParameters& p,bool intra,std::array<int,64>& block) {
    const auto* scan=p.alternate_scan?alternate_scan:zigzag_scan;
    const auto* matrix=intra?p.intra_matrix:p.nonintra_matrix;
    for(unsigned position=intra?1:0;;++position) {
        const auto next=bits.peek(17);
        unsigned run=0;int level=0;bool negative=false;
        const bool one=intra && p.intra_table_one;
        if(!intra && position==0 && (next>>16)) {bits.input.skip(1);level=1;negative=bits.take(1);}
        else if(one?(next>>13)==6:(next>>15)==2) {bits.input.skip(one?4:2);return;} // End of block.
        else if((next>>11)==1) { // Escape: 6-bit run, then the level.
            bits.input.skip(6);run=bits.take(6);
            if(p.mpeg1) {
                level=std::int8_t(bits.take(8));
                if(level==0)level=int(bits.take(8)); else if(level==-128)level=int(bits.take(8))-256;
            } else level=int(bits.take(12)<<20)>>20;
            if(!level)throw std::runtime_error("IPU BDEC: escape with level 0");
            negative=level<0;if(negative)level=-level;
        } else {
            const Coefficient* found=nullptr;
            if(one)for(const auto& entry:coefficients_one)if((next>>(17-entry.length))==entry.code){found=&entry;break;}
            if(!found)for(const auto& entry:coefficients)
                if((!one || entry.length>=shared_code_length) && (next>>(17-entry.length))==entry.code){found=&entry;break;}
            if(!found)throw std::runtime_error("IPU BDEC: invalid DCT coefficient code");
            bits.input.skip(found->length);run=found->run;level=found->level;negative=bits.take(1);
        }
        position+=run;
        if(position>63)throw std::runtime_error("IPU BDEC: coefficient run past the block");
        // Inverse quantisation on the magnitude (truncating toward zero), MPEG-1 results made odd.
        const int weight=int(p.quantiser_scale)*matrix[position];
        int value=intra?(level*weight)>>4:((2*level+1)*weight)>>5;
        if(p.mpeg1 && value && !(value&1))--value;
        block[scan[position]]=clampCoefficient(negative?-value:value);
    }
}

// Inverse DCT basis: basis[x][u] = C(u)/2 * cos((2x+1) u pi / 16).
const std::array<std::array<double,8>,8>& dctBasis() {
    static const auto basis=[] {
        std::array<std::array<double,8>,8> table{};
        const double pi=std::acos(-1.0);
        for(unsigned x=0;x<8;++x)for(unsigned u=0;u<8;++u)
            table[x][u]=(u?1.0:std::sqrt(0.5))*0.5*std::cos((2*x+1)*u*pi/16.0);
        return table;
    }();
    return basis;
}
}

unsigned ipuQuantiserScale(unsigned code,bool nonlinear) {
    static constexpr std::uint8_t table[32]={0,1,2,3,4,5,6,7,8,10,12,14,16,18,20,22,24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112};
    return nonlinear?table[code&31]:(code&31)*2;
}

void ipuInverseDct(std::array<int,64>& block) {
    const auto& basis=dctBasis();
    double rows[64];
    for(unsigned v=0;v<8;++v)for(unsigned x=0;x<8;++x) {
        double sum=0;
        for(unsigned u=0;u<8;++u)sum+=basis[x][u]*block[v*8+u];
        rows[v*8+x]=sum;
    }
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
        double sum=0;
        for(unsigned v=0;v<8;++v)sum+=basis[y][v]*rows[v*8+x];
        block[y*8+x]=int(std::floor(sum+0.5));
    }
}

void ipuDecodeMacroblock(IpuInput& input,const IpuBlockParameters& p,std::array<int,3>& dc_predictors,IpuMacroblock& out) {
    if(!p.intra_matrix || !p.nonintra_matrix || !p.quantiser_scale)throw std::runtime_error("IPU BDEC without quantiser tables or scale");
    Bits bits{input};
    out.fill(0);
    unsigned pattern=0x3F;
    if(!p.intra) {
        const auto next=bits.peek(9);
        const Pattern* found=nullptr;
        for(const auto& entry:patterns)if((next>>(9-entry.length))==entry.code){found=&entry;break;}
        if(!found)throw std::runtime_error("IPU BDEC: invalid coded block pattern");
        input.skip(found->length);pattern=found->value;
    }
    for(unsigned index=0;index<6;++index) {
        if(!(pattern&(0x20u>>index)))continue;
        std::array<int,64> block{};
        if(p.intra) {
            const unsigned component=index<4?0:index-3;
            dc_predictors[component]+=dcDifference(bits,component?chroma_sizes:luma_sizes);
            block[0]=dc_predictors[component]<<(3-p.dc_precision);
        }
        coefficientsInto(bits,p,p.intra,block);
        ipuInverseDct(block);
        // Destination: luma blocks in the 16x16 plane (alternate lines for field DCT), then Cb, Cr.
        std::int16_t* base=out.data();unsigned stride=16;
        if(index<4) {
            base+=(index&1)*8+(index&2?(p.field_dct?16:128):0);
            if(p.field_dct)stride=32;
        } else {base+=256+(index-4)*64;stride=8;}
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x) {
            const int value=block[y*8+x];
            base[y*stride+x]=std::int16_t(p.intra?(value<0?0:value>255?255:value):(value<-256?-256:value>255?255:value));
        }
    }
}
