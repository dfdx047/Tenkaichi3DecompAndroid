// Hand-written native VU1 programs: what they share (vu1_handwritten_registry.cpp has the overview and the
// registry; the programs are in vu1_skinned_vertex_programs.cpp, vu1_triangle_strip_program.cpp and vu1_triangle_clipper.cpp).
// `Vu1Hand` is a friend of `Vu1`: the programs work on the machine's state directly.
//
// Why plain SSE operations are enough: a run computes with the float mode of vu1.cpp (round toward
// zero, denormals read as zero, tiny results flushed). In that mode sums and products of finite
// operands are finite, so only values loaded from data memory or left in registers by an earlier
// program need the model's treatment of infinities and NaNs (`clamp`), once, where they are read.
//
// No float pragmas here: everything is SSE intrinsics on run-time values, which the compiler neither
// folds nor reorders, and the helpers of vu1_simd.h are inlined only into code with their settings.
#pragma once
#include "vu1.h"
#include "vu1_simd.h"
#include <bit>
#include <cstring>

struct Vu1Hand {
    // Lane helpers on SSE values; `clean` gives a loaded value the model's treatment of infinities and NaNs.
    static __m128 f(__m128i v) {return _mm_castsi128_ps(v);}
    static __m128i n(__m128 v) {return _mm_castps_si128(v);}
    static __m128 x(__m128 v) {return _mm_shuffle_ps(v,v,0x00);}
    static __m128 y(__m128 v) {return _mm_shuffle_ps(v,v,0x55);}
    static __m128 z(__m128 v) {return _mm_shuffle_ps(v,v,0xAA);}
    static __m128 w(__m128 v) {return _mm_shuffle_ps(v,v,0xFF);}
    static __m128 clean(__m128i v) {return vu1_simd::clamp(f(v));}
    static __m128i reg(const Vu1& vu,unsigned r) {return vu1_simd::load4(vu.vf[r].data());}
    static void setReg(Vu1& vu,unsigned r,__m128i v) {vu1_simd::store4(vu.vf[r].data(),v);}
    static std::uint32_t lane(__m128i v,unsigned index) {alignas(16) std::uint32_t l[4];_mm_store_si128(reinterpret_cast<__m128i*>(l),v);return l[index];}
    // Float to fixed point, truncated, saturated at the largest positive value (the processor's own
    // conversion gives the lowest value for everything out of range).
    static __m128i fixed(__m128 v,float scale) {
        const __m128 scaled=_mm_mul_ps(v,_mm_set1_ps(scale));
        return _mm_xor_si128(_mm_cvttps_epi32(scaled),n(_mm_cmpge_ps(scaled,_mm_set1_ps(2147483648.0f))));
    }
    static std::uint32_t pairLow(const Vu1& vu,std::uint32_t at) {std::uint32_t word;std::memcpy(&word,vu.program.data()+at*8,4);return word;}
    // Q = 1 / w of `v`, as the model divides: a division by zero gives the largest magnitude.
    static std::uint32_t reciprocalW(__m128 v) {
        const std::uint32_t d=lane(n(v),3);
        return (d&0x7FFFFFFFu)?vu1_simd::normal(std::bit_cast<std::uint32_t>(_mm_cvtss_f32(_mm_div_ss(_mm_set1_ps(1.0f),w(v))))):(d&0x80000000u)|0x7F7FFFFFu;
    }
    // The view volume test: x, y, z of `v` against +-|w|; six new flags below the earlier ones.
    static std::uint32_t clipTest(std::uint32_t flags,__m128 v) {
        const __m128 limit=_mm_andnot_ps(_mm_set1_ps(-0.0f),w(v));
        const unsigned above=unsigned(_mm_movemask_ps(_mm_cmpgt_ps(v,limit))),below=unsigned(_mm_movemask_ps(_mm_cmplt_ps(v,_mm_sub_ps(_mm_setzero_ps(),limit))));
        flags<<=6;
        for(unsigned l=0;l<3;++l)flags|=((above>>l)&1u)<<(l*2)|((below>>l)&1u)<<(l*2+1);
        return flags&0xFFFFFFu;
    }

    // The programs (lead: the pairs of the jump some starts go through first).
    static bool skinnedTwoLists(Vu1& vu,const Vu1::Kick& kick,unsigned lead); // vu1_skinned_vertex_programs.cpp
    static bool skinnedOneList(Vu1& vu,const Vu1::Kick& kick,unsigned lead);
    static bool clippedStrip(Vu1& vu,const Vu1::Kick& kick,unsigned lead);   // vu1_triangle_strip_program.cpp

    // The clipping of one triangle of a strip (vu1_triangle_clipper.cpp), on the machine's registers.
    static constexpr std::uint16_t work_a=61,work_b=101,fan_packet=30;
    struct Clipper {
        Vu1& vu;const Vu1::Kick& kick;std::uint64_t pairs=0;
        std::uint8_t* at(std::uint32_t address) const {return vu.cell(address);}
        __m128i get(std::uint32_t address) const {return vu1_simd::load4(at(address));}
        __m128 value(unsigned r) const {return clean(reg(vu,r));}
        void set(unsigned r,__m128 v) const {setReg(vu,r,n(v));}
        void putWord(std::uint32_t address,unsigned lane,std::uint16_t v) const {const std::uint32_t wide=v;std::memcpy(at(address)+lane*4,&wide,4);}
        std::uint16_t word(std::uint32_t address,unsigned lane) const {std::uint32_t wide;std::memcpy(&wide,at(address)+lane*4,4);return std::uint16_t(wide);}
        void push(unsigned r) const {vu.vi[14]=std::uint16_t(vu.vi[14]-1);vu1_simd::store4(at(vu.vi[14]),reg(vu,r));}
        void pop(unsigned r) const {setReg(vu,r,get(vu.vi[14]));vu.vi[14]=std::uint16_t(vu.vi[14]+1);}
        void append(unsigned r) const {vu1_simd::store4(at(vu.vi[9]),reg(vu,r));vu.vi[9]=std::uint16_t(vu.vi[9]+1);}
        void next(unsigned r) const {setReg(vu,r,get(vu.vi[8]));vu.vi[8]=std::uint16_t(vu.vi[8]+1);}
        void triangle();
    private:
        void edge();
        void crossing();
        void fan();
    };

    // The same programs are started at their first pair or through a jump two pairs long.
    static bool skinnedTwoListsJump(Vu1& vu,const Vu1::Kick& kick) {return skinnedTwoLists(vu,kick,2);}
    static bool skinnedTwoListsDirect(Vu1& vu,const Vu1::Kick& kick) {return skinnedTwoLists(vu,kick,0);}
    static bool skinnedOneListJump(Vu1& vu,const Vu1::Kick& kick) {return skinnedOneList(vu,kick,2);}
    static bool skinnedOneListDirect(Vu1& vu,const Vu1::Kick& kick) {return skinnedOneList(vu,kick,0);}
    static bool clippedStripJump(Vu1& vu,const Vu1::Kick& kick) {return clippedStrip(vu,kick,2);}
    static bool clippedStripDirect(Vu1& vu,const Vu1::Kick& kick) {return clippedStrip(vu,kick,0);}
};
