// Hand-written VU1 program part: clipping one triangle of a strip (vu1_handwritten_helpers.h, vu1_triangle_strip_program.cpp).
//
// The clipping of one triangle of a strip against the six planes of the view volume, as the
// strip program does it. It works on the machine's registers directly (the strip loop stores
// its values before and takes them back after), because nearly every register takes part.
//
// The triangle's three vertices (test coordinates, and from the input and the output list their
// other two quadwords) are pushed out from their centre by a factor the program sets, and put
// as a closed polygon into a work list. Six passes, one per plane, copy the polygon from one
// work list to the other: an edge that crosses the plane contributes the crossing point, a
#include "vu1_handwritten_helpers.h"

using namespace vu1_simd;

void Vu1Hand::Clipper::triangle() {
    auto& vi=vu.vi;
    // The centre of the three vertices; what the strip loop needs again goes on the stack.
    vu.i=pairLow(vu,89);
    __m128 acc=_mm_add_ps(value(22),value(23));
    vu.i=pairLow(vu,90);
    const __m128 weight=clean(_mm_set1_epi32(int(vu.i)));
    __m128 centre=_mm_add_ps(acc,_mm_mul_ps(value(24),weight));
    push(20);push(23);push(24);
    centre=_mm_mul_ps(centre,weight);
    push(25);
    vi[14]=std::uint16_t(vi[14]-1);
    putWord(vi[14],0,vi[2]);putWord(vi[14],1,vi[3]);
    const __m128 from_a=_mm_sub_ps(value(22),centre);
    putWord(vi[14],2,vi[5]);
    const __m128 from_b=_mm_sub_ps(value(23),centre);
    vi[14]=std::uint16_t(vi[14]-1);
    const __m128 from_c=_mm_sub_ps(value(24),centre);
    putWord(vi[14],0,vi[6]);putWord(vi[14],1,vi[7]);putWord(vi[14],2,vi[10]);
    vi[15]=105;
    pairs+=16;
    // The three vertices, pushed out from the centre, as a closed polygon in the first work
    // list: test coordinates, then per vertex the input's vector and the output's integers
    // as floats.
    vu.i=pairLow(vu,120);
    const __m128 scale=clean(_mm_set1_epi32(int(vu.i))),unit=w(value(0));
    acc=_mm_mul_ps(centre,unit);
    const __m128 a=_mm_add_ps(acc,_mm_mul_ps(from_a,scale)),b=_mm_add_ps(acc,_mm_mul_ps(from_b,scale)),c=_mm_add_ps(acc,_mm_mul_ps(from_c,scale));
    vi[8]=work_a;vi[9]=work_b;
    set(25,from_a);set(26,from_b);set(27,from_c);set(28,centre);store4(vu.acc.data(),n(acc));
    store4(at(vi[8]),n(a));store4(at(vi[8]+3u),n(b));store4(at(vi[8]+6u),n(c));store4(at(vi[8]+9u),n(a));
    __m128i p=get(vi[3]-7u),q=get(vi[3]-4u),r=get(vi[3]-1u);
    store4(at(vi[8]+2u),p);store4(at(vi[8]+5u),q);store4(at(vi[8]+8u),r);store4(at(vi[8]+11u),p);
    p=n(_mm_cvtepi32_ps(get(vi[7]-9u)));q=n(_mm_cvtepi32_ps(get(vi[7]-6u)));r=n(_mm_cvtepi32_ps(get(vi[7]-3u)));
    store4(at(vi[8]+1u),p);store4(at(vi[8]+4u),q);store4(at(vi[8]+7u),r);store4(at(vi[8]+10u),p);
    setReg(vu,22,p);setReg(vu,23,q);setReg(vu,24,r);
    pairs+=28;
    // The planes: z below, z above, x below, x above, y below, y above. A vertex's flags
    // hold "above" and "below" per axis; the earlier vertex's six are above the later one's.
    unsigned pass=0;
    for(;pass<6;++pass) {
        const unsigned axis=pass<2?2:pass<4?0:1;const bool below=!(pass&1);
        const std::uint16_t later=std::uint16_t(1u<<(axis*2+below));
        vi[5]=std::uint16_t(later<<6);vi[6]=later;vi[7]=std::uint16_t(axis);
        vi[8]=pass&1?work_b:work_a;vi[9]=pass&1?work_a:work_b;
        vi[10]=pass?vi[11]:3;
        // The plane's side: w is compared with -coordinate or +coordinate.
        const __m128 zero_one=value(0);
        setReg(vu,30,select(reg(vu,30),n(below?_mm_sub_ps(zero_one,w(zero_one)):_mm_add_ps(zero_one,w(zero_one))),laneMask(8)));
        vi[11]=0;
        next(21);next(22);next(23);
        pairs+=10;
        do {
            vi[10]=std::uint16_t(vi[10]-1);
            vi[2]=std::uint16_t(161+pass*20);
            edge();
            pairs+=5;
        } while(vi[10]);
        // The polygon is closed again: its first vertex once more at the end.
        vi[8]=pass&1?work_a:work_b;
        vi[2]=std::uint16_t(166+pass*20);
        setReg(vu,21,get(vi[8]));setReg(vu,22,get(vi[8]+1u));setReg(vu,23,get(vi[8]+2u));
        append(21);append(22);append(23);
        pairs+=13;
        if(!vi[11])break; // Nothing is left of the triangle.
    }
    if(pass==6) {
        vi[8]=work_a;vi[9]=work_b;vi[10]=vi[11];vi[11]=0;vi[1]=274;
        pairs+=6;
        fan();
    }
    pairs+=2;
    // Back to the strip: its registers from the stack.
    vi[6]=word(vi[14],0);vi[7]=word(vi[14],1);vi[10]=word(vi[14],2);
    vi[14]=std::uint16_t(vi[14]+1);
    vi[2]=word(vi[14],0);vi[3]=word(vi[14],1);vi[5]=word(vi[14],2);
    vi[14]=std::uint16_t(vi[14]+1);
    const __m128 cleared=_mm_mul_ps(value(31),x(value(0)));
    pop(25);
    set(31,cleared);
    pop(24);pop(23);pop(20);
    pairs+=12;
}

// One edge of the polygon against the pass's plane: from the vertex in hand (moved by the
// offset register) to the next one of the list.
void Vu1Hand::Clipper::edge() {
    auto& vi=vu.vi;
    const __m128 offset=value(31);
    const __m128 p0=_mm_add_ps(offset,value(21));
    next(21);
    const __m128 p1=_mm_add_ps(offset,value(22));
    next(22);
    const __m128 p2=_mm_add_ps(offset,value(23));
    next(23);
    set(17,p0);set(18,p1);set(19,p2);
    vu.clip=clipTest(clipTest(vu.clip,p0),value(21));
    vi[1]=std::uint16_t(vu.clip&0xFFFu);
    const bool earlier_outside=(vi[1]&vi[5])!=0;
    vi[4]=vi[1]&vi[6];
    const bool later_outside=vi[4]!=0;
    if(earlier_outside) {
        if(later_outside) {pairs+=22;return;} // The whole edge is outside.
        vi[1]=306;crossing();
        append(25);append(26);append(27);
        vi[11]=std::uint16_t(vi[11]+1);
        pairs+=28;
    } else if(later_outside) {
        vi[1]=318;crossing();
        append(17);append(18);append(19);append(25);append(26);append(27);
        vi[11]=std::uint16_t(vi[11]+2);
        pairs+=31;
    } else {
        append(17);append(18);append(19);
        vi[11]=std::uint16_t(vi[11]+1);
        pairs+=24;
    }
}

// Where the edge crosses the plane: the two ends' distances to it (w -+ the coordinate of
// the pass's axis) give the fraction; all three quadwords are interpolated with it.
void Vu1Hand::Clipper::crossing() {
    auto& vi=vu.vi;
    const __m128 side=x(value(30)),a=value(17),b=value(21);
    __m128 da=_mm_sub_ps(a,w(_mm_mul_ps(a,side))),db=_mm_sub_ps(b,w(_mm_mul_ps(b,side)));
    for(unsigned turn=vi[7];turn;--turn) {
        da=_mm_shuffle_ps(da,da,_MM_SHUFFLE(0,3,2,1));db=_mm_shuffle_ps(db,db,_MM_SHUFFLE(0,3,2,1));
        pairs+=7;
    }
    vi[12]=0;
    const __m128i difference=select(reg(vu,27),n(_mm_sub_ps(db,da)),laneMask(0xE));
    const std::uint32_t numerator=normal(lane(n(da),0)),denominator=normal(lane(difference,0));
    vu.q=(denominator&0x7FFFFFFFu)
        ?normal(std::bit_cast<std::uint32_t>(_mm_cvtss_f32(_mm_div_ss(f(_mm_set1_epi32(int(numerator))),f(_mm_set1_epi32(int(denominator)))))))
        :((numerator^denominator)&0x80000000u)|0x7F7FFFFFu;
    const __m128i fraction=_mm_and_si128(n(_mm_add_ps(value(0),clean(_mm_set1_epi32(int(vu.q))))),_mm_set1_epi32(0x7FFFFFFF));
    setReg(vu,29,select(reg(vu,29),fraction,laneMask(8)));
    const __m128 t=x(f(fraction));
    set(25,_mm_add_ps(_mm_mul_ps(_mm_sub_ps(b,a),t),a));
    const __m128 a1=value(18),a2=value(19);
    set(26,_mm_add_ps(_mm_mul_ps(_mm_sub_ps(value(22),a1),t),a1));
    set(27,_mm_add_ps(_mm_mul_ps(_mm_sub_ps(value(23),a2),t),a2));
    pairs+=27;
}

// The polygon as a fan in a packet of its own: each vertex through the output matrix the
// caller left in registers, as the strip loop converts its vertices.
void Vu1Hand::Clipper::fan() {
    auto& vi=vu.vi;
    const __m128i xyz=laneMask(0xE);
    __m128i r20=get(vi[8]);__m128 c20=clean(r20);
    vi[3]=vi[8];vi[2]=fan_packet;
    const __m128 c9=value(9),c10=value(10),c11=value(11),c12=value(12);
    __m128 acc=_mm_mul_ps(c9,x(c20));
    acc=_mm_add_ps(acc,_mm_mul_ps(c10,y(c20)));
    acc=_mm_add_ps(acc,_mm_mul_ps(c11,z(c20)));
    putWord(vi[2],0,vi[13]);
    __m128 v25=_mm_add_ps(acc,_mm_mul_ps(c12,w(c20)));
    vi[7]=std::uint16_t(vi[2]+2);
    vi[11]=std::uint16_t(vi[10]+vi[13]);
    vu.lower(pairLow(vu,375),375,kick);
    vu.q=reciprocalW(v25);
    putWord(vi[2],0,vi[11]);
    pairs+=13;
    __m128i r21,r22,v26,v28=reg(vu,28),v29;
    do {
        r22=get(vi[3]+1u);r20=get(vi[3]+3u);r21=get(vi[3]+2u);c20=clean(r20);
        vi[7]=std::uint16_t(vi[7]+3);
        v29=fixed(clean(r22),1.0f);
        vi[3]=std::uint16_t(vi[3]+3);
        const __m128 q_lanes=clean(_mm_set1_epi32(int(vu.q)));
        acc=_mm_mul_ps(c9,x(c20));
        const __m128 projected=_mm_mul_ps(v25,q_lanes);
        acc=_mm_add_ps(acc,_mm_mul_ps(c10,y(c20)));
        acc=_mm_add_ps(acc,_mm_mul_ps(c11,z(c20)));
        store4(at(vi[7]-3u),v29);
        v28=select(v28,n(_mm_mul_ps(clean(r21),q_lanes)),xyz);
        vi[10]=std::uint16_t(vi[10]-1);
        v26=fixed(projected,16.0f);
        v25=_mm_add_ps(acc,_mm_mul_ps(c12,w(c20)));
        store4(at(vi[7]-4u),v28);store4(at(vi[7]-2u),v26);
        vu.q=reciprocalW(v25);
        pairs+=17;
    } while(vi[10]);
    setReg(vu,20,r20);setReg(vu,21,r21);setReg(vu,22,r22);set(25,v25);setReg(vu,26,v26);setReg(vu,28,v28);setReg(vu,29,v29);
    store4(vu.acc.data(),n(acc));
    vu.lower(pairLow(vu,395),395,kick);
    pairs+=4;
}
