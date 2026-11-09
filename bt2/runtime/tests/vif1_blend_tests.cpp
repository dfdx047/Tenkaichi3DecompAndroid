// In-between VIF1 lists (runtime/vu/vif1_blend.h) on synthetic lists (no game data).
#include "vu/vif1_blend.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"VIF1 blend check failed: "<<what<<'\n';std::exit(1);}}
std::uint32_t bits(float f) {std::uint32_t w;std::memcpy(&w,&f,4);return w;}
float value(std::uint32_t w) {float f;std::memcpy(&f,&w,4);return f;}
// STCYCL 1,1; UNPACK V4-32 of two quadwords; MSCAL; DIRECT of one quadword; UNPACK V2-16 of one element.
std::vector<std::uint32_t> list(float a,float b,std::uint32_t tag,std::uint32_t direct) {
    return {0x01000101u,0x6C020000u,bits(a),tag,bits(1.0f),bits(0.0f),bits(b),bits(2.0f),bits(3.0f),bits(4.0f),
            0x14000000u,0x50000001u,direct,0,0,0,0x65010010u,0x00020001u};
}
}

int main() {
    const auto older=list(1.0f,10.0f,0x00000005u,0x1111u),newer=list(3.0f,20.0f,0x00000007u,0x2222u);
    const auto unpacks=vif1Unpacks(newer);
    check(unpacks && unpacks->size()==2 && (*unpacks)[0].start==2 && (*unpacks)[0].words==8 && (*unpacks)[1].words==1,"unpacks found and sized");
    std::size_t blended=0;
    const auto mid=blendVif1(older,newer,0.5f,&blended);
    check(mid.has_value() && blended==2,"two floats blended");
    check(value((*mid)[2])==2.0f && value((*mid)[6])==15.0f,"halfway values");
    check((*mid)[3]==0x00000007u,"a word that is not a normal float is the newer one");
    check((*mid)[12]==0x2222u,"DIRECT data is the newer frame's");
    check((*mid)[4]==bits(1.0f) && (*mid)[17]==0x00020001u,"equal words and other formats unchanged");
    // Lists that differ in places: unpacks are aligned by their codes; the rest pairs and is blended.
    auto shorter=newer;shorter.resize(16);
    std::size_t unmatched=9;
    const auto without_last=blendVif1(older,shorter,0.5f,&blended,&unmatched);
    check(without_last && blended==2 && unmatched==0 && value((*without_last)[2])==2.0f,"a missing unpack leaves the others paired");
    auto resized=newer;resized[1]=0x6C010000u;resized.erase(resized.begin()+6,resized.begin()+10);
    const auto other_size=blendVif1(older,resized,0.5f,&blended,&unmatched);
    check(other_size && blended==0 && unmatched==1 && value((*other_size)[2])==3.0f,"an unpack of another code is the newer one");
    // An unpack inserted in front: the ones after it still pair.
    std::vector<std::uint32_t> inserted{0x6C010040u,bits(9.0f),bits(9.0f),bits(9.0f),bits(9.0f)};
    inserted.insert(inserted.end(),newer.begin(),newer.end());
    const auto with_insert=blendVif1(older,inserted,0.5f,&blended,&unmatched);
    check(with_insert && blended==2 && unmatched==1 && value((*with_insert)[1])==9.0f && value((*with_insert)[7])==2.0f,"an inserted unpack keeps the rest paired");
    // Alignment itself: a shortest edit script's equal elements.
    const std::vector<std::uint64_t> sa{1,2,3,4,5,6},sb{1,3,4,7,5,6,8};
    const auto aligned=alignSequences(sa,sb,16);
    check(aligned && aligned->size()==5 && (*aligned)[1]==std::pair<std::size_t,std::size_t>(2,1) && (*aligned)[3]==std::pair<std::size_t,std::size_t>(4,4),"sequences aligned");
    check(!alignSequences(sa,std::vector<std::uint64_t>{9,9,9,9,9,9},3),"too many edits refused");
    // STCYCL with WL > CL: the data covers CL of every WL elements.
    const std::vector<std::uint32_t> skipping{0x01000201u,0x6C040000u,1,2,3,4,5,6,7,8};
    const auto u=vif1Unpacks(skipping);
    check(u && u->size()==1 && (*u)[0].words==8,"WL > CL sizes the data by CL");
    check(!vif1Unpacks(std::vector<std::uint32_t>{0x6C020000u,1,2}),"data past the end is refused");
    // A bone block (34 quadwords): an axis turning by 90 degrees keeps its length halfway (not 0.707).
    auto block=[&](unsigned quadwords,unsigned at,float x,float y) {
        std::vector<std::uint32_t> l{0x01000101u,0x6C000000u|(quadwords<<16)};
        for(unsigned q=0;q<quadwords;++q)for(unsigned k=0;k<4;++k)l.push_back(bits(q==at?(k==0?x:k==1?y:0.0f):1.0f));
        return l;
    };
    const auto turned=blendVif1(block(34,22,1,0),block(34,22,0,1),0.5f);
    check(turned && std::abs(std::hypot(value((*turned)[2+22*4]),value((*turned)[2+22*4+1]))-1.0f)<1e-5f,"a bone axis keeps its length");
    // A camera block (12 quadwords) whose view axis turns by 90 degrees is a cut; a small turn is not.
    check(cameraTurn(block(12,4,1,0),block(12,4,0,1))<0.01f && cameraTurn(block(12,4,1,0),block(12,4,1,0.1f))>0.99f,"camera cuts found");
    std::cout<<"VIF1 blend tests passed\n";
    return 0;
}
