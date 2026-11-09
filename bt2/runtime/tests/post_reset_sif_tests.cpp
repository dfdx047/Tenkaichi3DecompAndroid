#include "ee/post_reset_sif.h"
#include <iostream>
#include <vector>
void check(bool v) {if(!v)throw std::runtime_error("Fresh SIF assertion failed");}
template<class F> void reject(F f) {bool caught=false;try{f();}catch(const std::runtime_error&){caught=true;}check(caught);}
int main() {
    PostResetSif sif;reject([&]{sif.status();});
    std::array<std::uint32_t,34> pub={0,0,0x1FFED40,0x800044,0x84,0x3A9500,0x70000,0,0x1E640,
        1,0,0x1FFED40,0xA00044,0x184,0x3A94E0,0x70000,0,0x1E640,
        0x1E640,0x1E6C0,0,0x1E730,0x1E640,0x1E6C0,0x3A94C0,0x1E730,0,0,0x80000001,0,0,1,0,1};
    auto evidence=std::span(reinterpret_cast<const std::uint8_t*>(pub.data()),sizeof(pub));
    sif.supply(evidence);reject([&]{sif.supply(evidence);});reject([&]{sif.channel();});check(sif.phase==0);
    check(sif.status()==0x800044 && sif.channel()==0x84);sif.chain();sif.handler();sif.enable();
    reject([&]{sif.get(2);});check(sif.get(0x80000000)==0 && sif.get(4)==0x70000 && sif.get(2)==0x1E640);
    reject([&]{sif.set(0x80000000,0x19600);});sif.set(0x80000000,0x1E640);sif.set(0x80000001,0x3A9598);
    std::array<std::uint32_t,5> packet={20,0,0x80000002,0,0x3A94C0};
    auto bytes=std::span(reinterpret_cast<const std::uint8_t*>(packet.data()),sizeof(packet));
    packet[4]=0;reject([&]{sif.submit(bytes);});check(sif.phase==10 && !sif.submissions && !sif.ee_receive_address);
    packet[4]=0x3A94C0;check(sif.submit(bytes)!=0 && sif.ee_receive_address==0x3A94C0 && !sif.sreg_responses);
    check(sif.get(0x80000002)==0);packet[0]=16;packet[3]=1;check(sif.submit(bytes.first(16))!=0);
    check(sif.sreg_responses==1 && sif.submissions==2);sif.set(0x80000002,1);
    check(sif.phase==14 && sif.rpc_initialized);reject([&]{sif.submit(bytes.first(16));});reject([&]{sif.get(0x80000002);});
    PostResetCd cd;
    std::array<std::uint32_t,52> replies{};
    for(unsigned n : {0u,16u}) {replies[n+2]=0x80000008;replies[n+5]=0x203A97C0;replies[n+7]=0x3A87A8;replies[n+8]=0x80000009;}
    replies[25]=0x43388;replies[26]=0x441B8;
    replies[32]=0x1000;replies[33]=0x3695C0;replies[34]=0x80000008;replies[39]=0x3A87A8;replies[40]=0x8000000A;
    replies[48]=1;replies[49]=replies[50]=0x225;
    auto service=std::span(reinterpret_cast<const std::uint8_t*>(replies.data()),sizeof(replies));
    reject([&]{cd.supply(service.first(204));});check(!cd.supplied);
    replies[25]=0x3F570;reject([&]{cd.supply(service);});check(!cd.supplied);
    replies[25]=0x43388;
    std::array<std::uint8_t,340> missing_service{};std::copy(service.begin(),service.end(),missing_service.begin());
    reject([&]{cd.supply(missing_service);});check(!cd.supplied);
    cd.supply(service);check(cd.supplied && !cd.scmd_supplied && !cd.operations());
    reject([&]{cd.supply(service);});
    PostResetLoadfile loader;
    std::array<std::uint32_t,33> lf{};
    lf[2]=0x80000008;lf[5]=0x203A97C0;lf[7]=0x3ACC00;lf[8]=0x80000009;lf[9]=0x22568;lf[10]=0x225B0;
    lf[16]=0x400;lf[17]=0x3ACA00;lf[18]=0x80000008;lf[23]=0x3ACC00;lf[24]=0x8000000A;lf[32]=0x30303033;
    auto lb=std::span(reinterpret_cast<const std::uint8_t*>(lf.data()),sizeof(lf));
    reject([&]{loader.supply(lb.first(128));});check(!loader.supplied);
    lf[9]=0x43388;reject([&]{loader.supply(lb);});check(!loader.supplied);
    lf[9]=0x22568;lf[32]=0;reject([&]{loader.supply(lb);});check(!loader.supplied);
    lf[32]=0x30303033;loader.supply(lb);check(loader.supplied && !loader.operations());
    reject([&]{loader.supply(lb);});
    std::cout<<"Fresh SIF transport tests passed\n";
}
