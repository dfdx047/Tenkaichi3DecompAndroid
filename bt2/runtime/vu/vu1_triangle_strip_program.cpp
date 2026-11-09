// Hand-written VU1 program: a triangle strip with the view volume test per triangle (vu1_handwritten_helpers.h).
#include "vu1_handwritten_helpers.h"

using namespace vu1_simd;

// A triangle strip of plain vertices, with the view volume test per triangle.
//
// Input at TOP: two header quadwords, a quadword with the vertex count, then three quadwords
// per vertex (position, a vector converted to integers, a vector scaled by Q). The matrices are
// in registers, left there by the caller: one gives the output position, the other the
// coordinates the test uses. The header goes out as a packet of its own first; the vertices
// follow it in the output list 218 quadwords after the input and are sent at the end.
// A triangle with a vertex outside the view volume takes one of two ways: with all three
// vertices beyond one plane its last vertex is marked as not drawn; otherwise the program clips
// the triangle and sends what is left as a packet of its own (`Clipper`).
bool Vu1Hand::clippedStrip(Vu1& vu,const Vu1::Kick& kick,unsigned lead) {
    const __m128i xyz=laneMask(0xE);
    const auto at=[&](std::uint32_t address) {return vu.cell(address);};
    const auto get=[&](std::uint32_t address) {return load4(at(address));};
    std::uint64_t pairs=lead+25;
    std::uint16_t vi1=std::uint16_t(vu.top);
    const __m128i r17=get(vi1),r18=get(vi1+1u);
    vi1=std::uint16_t(vi1+2);
    std::uint16_t vi3=std::uint16_t(vi1+1);
    __m128i r20=get(vi3);__m128 c20=clean(r20);
    const std::uint16_t marker=0x8000;
    std::uint32_t count_word;std::memcpy(&count_word,at(vi1),4);
    std::uint16_t vi10=std::uint16_t(count_word)&32767;
    const __m128 c1=clean(reg(vu,1)),c2=clean(reg(vu,2)),c3=clean(reg(vu,3)),c4=clean(reg(vu,4));
    const __m128 c5=clean(reg(vu,5)),c6=clean(reg(vu,6)),c7=clean(reg(vu,7)),c8=clean(reg(vu,8));
    __m128 acc=_mm_mul_ps(c1,x(c20));
    __m128i r19=get(vi1);
    acc=_mm_add_ps(acc,_mm_mul_ps(c2,y(c20)));
    std::uint16_t vi2=std::uint16_t(vi3+218);
    acc=_mm_add_ps(acc,_mm_mul_ps(c3,z(c20)));
    std::uint16_t vi4=0xFFFE,vi5=0xFFFE,vi6=0xFFFE; // Three vertices make the first triangle.
    __m128 v25=_mm_add_ps(acc,_mm_mul_ps(c4,w(c20)));
    std::uint32_t q=reciprocalW(v25),clip=vu.clip;
    store4(at(vi2),r17);store4(at(vi2+1u),r18);store4(at(vi2+2u),r19);
    std::uint16_t vi7=std::uint16_t(vi2+4);
    __m128 v22=f(reg(vu,0)),v23=v22,v24=v22;
    setReg(vu,17,r17);setReg(vu,18,r18);vu.vi[11]=32767;vu.vi[13]=marker;
    vu.vi[2]=vi2;vu.lower(pairLow(vu,44),44,kick); // The header's packet.
    vi2=std::uint16_t(vi2+2);
    std::uint32_t one_plane[6];
    for(unsigned k=0;k<6;++k)one_plane[k]=pairLow(vu,71+k*3)&0xFFFFFFu;
    const std::uint32_t last_vertex=pairLow(vu,58)&0xFFFFFFu;
    __m128i r21=reg(vu,21),v26=reg(vu,26),v27=reg(vu,27),v28=reg(vu,28);
    const __m128i marker_lanes=_mm_set1_epi32(int(std::uint32_t(marker))),marker_lane=laneMask(1);
    do {
        pairs+=18;
        acc=_mm_mul_ps(c5,x(c20));
        v22=v23;
        acc=_mm_add_ps(acc,_mm_mul_ps(c6,y(c20)));
        v23=v24;
        acc=_mm_add_ps(acc,_mm_mul_ps(c7,z(c20)));
        r19=get(vi3+2u);
        v24=_mm_add_ps(acc,_mm_mul_ps(c8,w(c20)));
        r21=get(vi3+1u);
        clip=clipTest(clip,v22);
        r20=get(vi3+3u);c20=clean(r20);
        clip=clipTest(clip,v23);
        vi4=vi5;
        const __m128 q_lanes=clean(_mm_set1_epi32(int(q)));
        v25=_mm_mul_ps(v25,q_lanes);
        vi5=vi6;
        v28=select(v28,n(_mm_mul_ps(clean(r19),q_lanes)),xyz);
        vi3=std::uint16_t(vi3+3);
        clip=clipTest(clip,v24);
        vi7=std::uint16_t(vi7+3);
        acc=_mm_mul_ps(c1,x(c20));
        vi10=std::uint16_t(vi10-1);
        v27=fixed(v25,16.0f);
        vi4=std::uint16_t(vi4+vi5);
        acc=_mm_add_ps(acc,_mm_mul_ps(c2,y(c20)));
        store4(at(vi7-4u),v28);
        v26=fixed(clean(r21),1.0f);
        vi1=(clip&last_vertex)?1:0;
        acc=_mm_add_ps(acc,_mm_mul_ps(c3,z(c20)));
        vi6=vi1;
        v25=_mm_add_ps(acc,_mm_mul_ps(c4,w(c20)));
        vi4=std::uint16_t(vi4+vi6);
        store4(at(vi7-2u),v27);
        store4(at(vi7-3u),v26);
        if(std::int16_t(vi4)>0) {
            // A vertex of this triangle is outside. All three beyond one plane: nothing to clip.
            for(unsigned k=0;k<6;++k) {
                pairs+=3;
                vi1=((clip|one_plane[k])&0xFFFFFFu)==0xFFFFFFu?1:0;
                if(vi1)break;
            }
            if(!vi1) {
                // The triangle is clipped; that works on the machine's registers.
                setReg(vu,19,r19);setReg(vu,20,r20);setReg(vu,21,r21);setReg(vu,22,n(v22));setReg(vu,23,n(v23));setReg(vu,24,n(v24));
                setReg(vu,25,n(v25));setReg(vu,26,v26);setReg(vu,27,v27);setReg(vu,28,v28);store4(vu.acc.data(),n(acc));
                vu.vi[1]=vi1;vu.vi[2]=vi2;vu.vi[3]=vi3;vu.vi[4]=vi4;vu.vi[5]=vi5;vu.vi[6]=vi6;vu.vi[7]=vi7;vu.vi[10]=vi10;
                vu.q=q;vu.clip=clip;
                Clipper clipper{vu,kick};clipper.triangle();pairs+=clipper.pairs;
                r19=reg(vu,19);r20=reg(vu,20);c20=clean(r20);r21=reg(vu,21);v22=clean(reg(vu,22));v23=clean(reg(vu,23));v24=clean(reg(vu,24));
                v25=clean(reg(vu,25));v26=reg(vu,26);v27=reg(vu,27);v28=reg(vu,28);acc=clean(load4(vu.acc.data()));
                vi1=vu.vi[1];vi2=vu.vi[2];vi3=vu.vi[3];vi4=vu.vi[4];vi5=vu.vi[5];vi6=vu.vi[6];vi7=vu.vi[7];vi10=vu.vi[10];
                q=vu.q;clip=vu.clip;
            }
            pairs+=3;
            store4(at(vi7-2u),select(get(vi7-2u),marker_lanes,marker_lane)); // Not drawn.
        }
        pairs+=2;
        q=reciprocalW(v25);
    } while(vi10);
    setReg(vu,19,r19);setReg(vu,20,r20);setReg(vu,21,r21);setReg(vu,22,n(v22));setReg(vu,23,n(v23));setReg(vu,24,n(v24));
    setReg(vu,25,n(v25));setReg(vu,26,v26);setReg(vu,27,v27);setReg(vu,28,v28);store4(vu.acc.data(),n(acc));
    vu.vi[1]=vi1;vu.vi[2]=vi2;vu.vi[3]=vi3;vu.vi[4]=vi4;vu.vi[5]=vi5;vu.vi[6]=vi6;vu.vi[7]=vi7;vu.vi[10]=vi10;
    vu.q=q;vu.clip=clip;
    vu.lower(pairLow(vu,66),66,kick); // The strip's packet.
    pairs+=3;vu.pc=69; // Where the program ends.
    vu.instructions+=pairs;vu.cycle_+=pairs;
    return true;
}
