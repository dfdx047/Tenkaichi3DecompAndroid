// Hand-written VU1 programs: skinned vertices blended from two bones (vu1_handwritten_helpers.h).
#include "vu1_handwritten_helpers.h"

using namespace vu1_simd;

// Skinned vertices with two bones, to two output lists.
//
// Input at TOP: three header quadwords, a quadword whose first word holds the vertex count (15
// bits) and that also starts the second list, one more header, then three quadwords per vertex:
// position (w: the weight of the first bone), a second vector (transformed like a direction by
// two further matrices and blended the same way) and a third that is scaled by 1/w of the
// projected position. Each vertex is taken relative to the two bones' origins (left in two
// registers by the caller), transformed by both bone matrices and blended; the result goes
// through two 4x4 matrices: one gives the output position (divided by its w, converted to
// fixed point with four fraction bits), the other is only tested against the view volume. A
// vertex outside it gets the caller's marker written into the w of its position in both lists.
// The first list (252 quadwords after the input) is sent at the end.
bool Vu1Hand::skinnedTwoLists(Vu1& vu,const Vu1::Kick& kick,unsigned lead) {
    const __m128i xyz=laneMask(0xE);
    const __m128 one=_mm_set1_ps(1.0f),zero_one=_mm_setr_ps(0.0f,0.0f,0.0f,1.0f);
    const auto at=[&](std::uint32_t address) {return vu.cell(address);};
    const auto get=[&](std::uint32_t address) {return load4(at(address));};
    std::uint64_t pairs=lead+26; // The jump to the program's body where it starts with one, and its setup.

    // Setup: the header, the count, the two lists' first quadwords.
    std::uint16_t in=std::uint16_t(vu.top);
    const __m128i head0=get(in++),head1=get(in++),head2=get(in++);
    std::uint32_t count_word;std::memcpy(&count_word,at(in),4);
    const __m128i head3=get(in++),head4=get(in++);
    __m128i r19=get(in);__m128 c19=clean(r19);
    const std::uint16_t all_bits=32767;
    std::uint16_t count=std::uint16_t(count_word)&all_bits;
    const std::uint16_t first=std::uint16_t(in+252);
    const __m128 origin_a=clean(reg(vu,30)),origin_b=clean(reg(vu,31));
    __m128 v28=_mm_sub_ps(c19,origin_a),v29=_mm_sub_ps(c19,origin_b);
    store4(at(first),head0);store4(at(first+1u),head1);store4(at(first+2u),head3);
    std::uint16_t out_a=std::uint16_t(first+3),out_b=std::uint16_t(out_a+3u*count);
    store4(at(out_b),head0);store4(at(out_b+1u),head2);store4(at(out_b+2u),head4);
    out_b=std::uint16_t(out_b+3);
    // The first vertex through the first bone, with the matrix the caller left in registers.
    __m128i r1=reg(vu,1),r2=reg(vu,2),r3=reg(vu,3),r4=reg(vu,4);
    __m128 acc=_mm_mul_ps(clean(r1),x(v28));
    __m128i r5=get(4);
    acc=_mm_add_ps(acc,_mm_mul_ps(clean(r2),y(v28)));
    __m128i r6=get(5);
    acc=_mm_add_ps(acc,_mm_mul_ps(clean(r3),z(v28)));
    __m128i r7=get(6);
    v28=_mm_add_ps(acc,_mm_mul_ps(clean(r4),one));
    __m128i r8=get(7);

    __m128i r9,r10,r11,r12,r13,r14,r15,r16,r17=head0,r20=head2,r21,v17,v22=reg(vu,22),v23,v29i;
    const __m128i marker_source=_mm_set1_epi32(int(std::uint32_t(vu.vi[13]))),marker_lane=laneMask(1),r18=reg(vu,18);
    __m128 v24,v25,v26,v27;
    std::uint32_t q=vu.q,clip=vu.clip;std::uint16_t outside=0;
    const std::uint32_t half=pairLow(vu,87); // The constant the program loads into I.
    const std::uint32_t outside_mask=pairLow(vu,100)&0xFFFFFFu; // The flags of the last three tests.
    const __m128 half_lanes=clean(_mm_set1_epi32(int(half)));
    do {
        pairs+=58;
        // Second bone; the vertex's second vector by the two direction matrices (26..33).
        acc=_mm_mul_ps(clean(r5),x(v29));
        r21=get(in+1u);const __m128 c21=clean(r21);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r6),y(v29)));
        r9=get(26);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r7),z(v29)));
        r10=get(27);
        v29=_mm_add_ps(acc,_mm_mul_ps(clean(r8),one));
        r11=get(28);r12=get(29);
        acc=_mm_mul_ps(clean(r9),x(c21));
        r13=get(30);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r10),y(c21)));
        r14=get(31);
        __m128 v20=_mm_sub_ps(v28,v29);
        r15=get(32);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r11),z(c21)));
        r16=get(33);
        v26=_mm_add_ps(acc,_mm_mul_ps(clean(r12),one));
        r1=get(14);
        acc=_mm_mul_ps(clean(r13),x(c21));
        r2=get(15);
        v20=_mm_mul_ps(v20,w(c19)); // Blend by the weight: b + (a - b) * weight.
        r3=get(16);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r14),y(c21)));
        r4=get(17);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r15),z(c21)));
        r5=get(18);
        v27=_mm_add_ps(acc,_mm_mul_ps(clean(r16),one));
        r6=get(19);
        v20=_mm_add_ps(v29,v20);
        r7=get(20);r8=get(21);
        v26=_mm_sub_ps(v26,v27);
        // The blended position through the output matrix (14..17) and the test matrix (18..21).
        acc=_mm_mul_ps(clean(r1),x(v20));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r2),y(v20)));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r3),z(v20)));
        v24=_mm_add_ps(acc,_mm_mul_ps(clean(r4),one));
        v26=_mm_mul_ps(v26,w(c19));
        acc=_mm_mul_ps(clean(r5),x(v20));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r6),y(v20)));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r7),z(v20)));
        q=reciprocalW(v24);
        const __m128 q_lanes=clean(_mm_set1_epi32(int(q)));
        v25=_mm_add_ps(acc,_mm_mul_ps(clean(r8),one));
        r17=get(in+2u);
        v26=_mm_add_ps(v27,v26);
        clip=clipTest(clip,v25);
        v26=f(select(n(v26),n(_mm_mul_ps(v26,half_lanes)),xyz));
        v24=_mm_mul_ps(v24,q_lanes);
        r1=get(0);
        v17=select(r17,n(_mm_mul_ps(clean(r17),q_lanes)),xyz);
        r20=get(22);
        r2=get(1);
        v26=f(select(n(v26),n(_mm_add_ps(v26,half_lanes)),xyz));
        r3=get(2);
        v29i=fixed(v24,16.0f);
        r4=get(3);
        v23=fixed(clean(r20),1.0f);
        outside=(clip&outside_mask)?1:0;
        store4(at(out_a),v17);
        // The second list's first quadword per vertex: (x of the blended vector, 0, 1) times Q.
        v22=select(v22,n(_mm_add_ps(zero_one,v26)),laneMask(8));
        in=std::uint16_t(in+3);
        v22=select(v22,n(_mm_add_ps(zero_one,zero_one)),laneMask(4));
        r19=get(in);c19=clean(r19);
        v22=select(v22,n(_mm_add_ps(zero_one,one)),laneMask(2));
        r5=get(4);r6=get(5);r7=get(6);
        v28=_mm_sub_ps(c19,origin_a);
        store4(at(out_a+2u),v29i);
        v22=select(v22,n(_mm_mul_ps(f(v22),q_lanes)),xyz);
        store4(at(out_b+2u),v29i);
        v29=_mm_sub_ps(c19,origin_b);
        r8=get(7);
        store4(at(out_b+1u),r18);
        // The next vertex through the first bone already.
        acc=_mm_mul_ps(clean(r1),x(v28));
        count=std::uint16_t(count-1);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r2),y(v28)));
        out_a=std::uint16_t(out_a+3);out_b=std::uint16_t(out_b+3);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r3),z(v28)));
        store4(at(out_a-2u),v23);
        store4(at(out_b-3u),v22);
        v28=_mm_add_ps(acc,_mm_mul_ps(clean(r4),one));
        if(outside) {
            pairs+=2;
            store4(at(out_a-1u),select(get(out_a-1u),marker_source,marker_lane));
            store4(at(out_b-1u),select(get(out_b-1u),marker_source,marker_lane));
        }
    } while(count);

    // What the program leaves in its registers, then the first list goes out.
    setReg(vu,1,r1);setReg(vu,2,r2);setReg(vu,3,r3);setReg(vu,4,r4);setReg(vu,5,r5);setReg(vu,6,r6);setReg(vu,7,r7);setReg(vu,8,r8);
    setReg(vu,9,r9);setReg(vu,10,r10);setReg(vu,11,r11);setReg(vu,12,r12);setReg(vu,13,r13);setReg(vu,14,r14);setReg(vu,15,r15);setReg(vu,16,r16);
    setReg(vu,17,v17);setReg(vu,19,r19);setReg(vu,20,r20);setReg(vu,21,r21);setReg(vu,22,v22);setReg(vu,23,v23);
    setReg(vu,24,n(v24));setReg(vu,25,n(v25));setReg(vu,26,n(v26));setReg(vu,27,n(v27));setReg(vu,28,n(v28));setReg(vu,29,n(v29));
    store4(vu.acc.data(),n(acc));
    vu.vi[1]=outside;vu.vi[2]=first;vu.vi[3]=in;vu.vi[4]=out_b;vu.vi[7]=out_a;vu.vi[10]=count;vu.vi[11]=all_bits;
    vu.q=q;vu.i=half;vu.clip=clip;
    vu.lower(pairLow(vu,122),122,kick); // The packet at the first list's address.
    pairs+=3;vu.pc=125; // Where the program ends.
    vu.instructions+=pairs;vu.cycle_+=pairs;
    return true;
}

// Skinned vertices with two bones, to one output list: as above without the second vector and
// the second list. Header: two quadwords, two skipped, the count quadword. Per vertex the
// position (w: the weight) and a vector scaled by Q; the second output quadword of each vertex
// is a register the caller set. The two 4x4 matrices are at 11..14 and 15..18.
bool Vu1Hand::skinnedOneList(Vu1& vu,const Vu1::Kick& kick,unsigned lead) {
    const __m128i xyz=laneMask(0xE);
    const __m128 one=_mm_set1_ps(1.0f);
    const auto at=[&](std::uint32_t address) {return vu.cell(address);};
    const auto get=[&](std::uint32_t address) {return load4(at(address));};
    std::uint64_t pairs=lead+24;
    std::uint16_t in=std::uint16_t(vu.top);
    const __m128i head0=get(in++),head1=get(in++);
    in=std::uint16_t(in+2);
    std::uint32_t count_word;std::memcpy(&count_word,at(in),4);
    const __m128i head2=get(in++);
    __m128i r19=get(in);__m128 c19=clean(r19);
    const std::uint16_t all_bits=32767;
    std::uint16_t count=std::uint16_t(count_word)&all_bits;
    const std::uint16_t first=std::uint16_t(in+252);
    const __m128 origin_a=clean(reg(vu,30)),origin_b=clean(reg(vu,31));
    __m128 v28=_mm_sub_ps(c19,origin_a),v29=_mm_sub_ps(c19,origin_b);
    store4(at(first),head0);store4(at(first+1u),head1);store4(at(first+2u),head2);
    std::uint16_t out=std::uint16_t(first+3);
    __m128i r1=reg(vu,1),r2=reg(vu,2),r3=reg(vu,3),r4=reg(vu,4);
    __m128 acc=_mm_mul_ps(clean(r1),x(v28));
    __m128i r5=get(4);
    acc=_mm_add_ps(acc,_mm_mul_ps(clean(r2),y(v28)));
    __m128i r6=get(5);
    acc=_mm_add_ps(acc,_mm_mul_ps(clean(r3),z(v28)));
    __m128i r7=get(6);
    v28=_mm_add_ps(acc,_mm_mul_ps(clean(r4),one));
    __m128i r8=get(7);
    const __m128i second=reg(vu,9);
    const std::uint32_t outside_mask=pairLow(vu,77)&0xFFFFFFu;
    __m128i r21,v22,v29i;__m128 v20,v24,v25;
    std::uint32_t q=vu.q,clip=vu.clip;std::uint16_t outside=0;
    do {
        pairs+=56;
        acc=_mm_mul_ps(clean(r5),x(v29));
        r21=get(in+1u);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r6),y(v29)));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r7),z(v29)));
        v29=_mm_add_ps(acc,_mm_mul_ps(clean(r8),one));
        v20=_mm_sub_ps(v28,v29);
        r1=get(11);r2=get(12);
        v20=_mm_mul_ps(v20,w(c19));
        r3=get(13);r4=get(14);r5=get(15);r6=get(16);
        v20=_mm_add_ps(v29,v20);
        r7=get(17);r8=get(18);
        acc=_mm_mul_ps(clean(r1),x(v20));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r2),y(v20)));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r3),z(v20)));
        v24=_mm_add_ps(acc,_mm_mul_ps(clean(r4),one));
        acc=_mm_mul_ps(clean(r5),x(v20));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r6),y(v20)));
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r7),z(v20)));
        q=reciprocalW(v24);
        const __m128 q_lanes=clean(_mm_set1_epi32(int(q)));
        v25=_mm_add_ps(acc,_mm_mul_ps(clean(r8),one));
        const __m128i r22=get(in+2u);
        clip=clipTest(clip,v25);
        v24=_mm_mul_ps(v24,q_lanes);
        r1=get(0);
        v22=select(r22,n(_mm_mul_ps(clean(r22),q_lanes)),xyz);
        r2=get(1);r3=get(2);r4=get(3);
        outside=(clip&outside_mask)?1:0;
        store4(at(out),v22);
        in=std::uint16_t(in+3);
        v29i=fixed(v24,16.0f);
        r19=get(in);c19=clean(r19);
        r5=get(4);r6=get(5);r7=get(6);
        v28=_mm_sub_ps(c19,origin_a);
        store4(at(out+2u),v29i);
        v29=_mm_sub_ps(c19,origin_b);
        r8=get(7);
        acc=_mm_mul_ps(clean(r1),x(v28));
        count=std::uint16_t(count-1);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r2),y(v28)));
        out=std::uint16_t(out+3);
        acc=_mm_add_ps(acc,_mm_mul_ps(clean(r3),z(v28)));
        store4(at(out-2u),second);
        v28=_mm_add_ps(acc,_mm_mul_ps(clean(r4),one));
    } while(count);
    setReg(vu,1,r1);setReg(vu,2,r2);setReg(vu,3,r3);setReg(vu,4,r4);setReg(vu,5,r5);setReg(vu,6,r6);setReg(vu,7,r7);setReg(vu,8,r8);
    setReg(vu,19,r19);setReg(vu,20,n(v20));setReg(vu,21,r21);setReg(vu,22,v22);setReg(vu,24,n(v24));setReg(vu,25,n(v25));
    setReg(vu,26,head0);setReg(vu,27,head1);setReg(vu,28,n(v28));setReg(vu,29,n(v29));
    store4(vu.acc.data(),n(acc));
    vu.vi[1]=outside;vu.vi[2]=first;vu.vi[3]=in;vu.vi[7]=out;vu.vi[10]=count;vu.vi[11]=all_bits;
    vu.q=q;vu.clip=clip;
    vu.lower(pairLow(vu,96),96,kick);
    pairs+=3;vu.pc=99; // Where the program ends.
    vu.instructions+=pairs;vu.cycle_+=pairs;
    return true;
}
