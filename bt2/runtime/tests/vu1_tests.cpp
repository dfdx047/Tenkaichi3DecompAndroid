// VIF1 codes and VU1 microprogram instructions (synthetic lists and programs; no game data).
#include "vu/vif1.h"
#include "vu/vu1_program.h"
#include <bit>
#include <cfenv>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool value,const char* what) {if(!value){std::cerr<<"VU1 check failed: "<<what<<'\n';std::exit(1);}}
template<class F> bool throws(F f) {try{f();}catch(const std::runtime_error&){return true;}return false;}
std::uint32_t fb(float v) {return std::bit_cast<std::uint32_t>(v);}

// Instruction encoders (fields as in the public instruction set description).
struct Pair {std::uint32_t lower,upper;};
constexpr std::uint32_t X=8,Y=4,Z=2,W=1,XYZW=15,E=0x40000000,IBIT=0x80000000;
constexpr std::uint32_t upper(std::uint32_t dest,unsigned ft,unsigned fs,unsigned fd,unsigned op) {return dest<<21|ft<<16|fs<<11|fd<<6|op;}
constexpr std::uint32_t upperSpecial(std::uint32_t dest,unsigned ft,unsigned fs,unsigned code) {return dest<<21|ft<<16|fs<<11|(code>>2)<<6|0x3C|(code&3);}
constexpr std::uint32_t lower(unsigned op,std::uint32_t dest,unsigned it,unsigned is,int imm) {return op<<25|dest<<21|it<<16|is<<11|(std::uint32_t(imm)&0x7FF);}
constexpr std::uint32_t lowerSpecial(std::uint32_t dest,unsigned it,unsigned is,unsigned code) {return 0x80000000u|dest<<21|it<<16|is<<11|(code>>2)<<6|0x3C|(code&3);}
constexpr std::uint32_t integerOp(unsigned id,unsigned is,unsigned it,unsigned function) {return 0x80000000u|it<<16|is<<11|id<<6|function;}
constexpr std::uint32_t UNOP=upperSpecial(0,0,0,0x2F),LNOP=lowerSpecial(0,0,0,0x30);
constexpr std::uint32_t iaddiu(unsigned it,unsigned is,unsigned imm) {return 8u<<25|((imm>>11)&15)<<21|it<<16|is<<11|(imm&0x7FF);}
constexpr std::uint32_t isubiu(unsigned it,unsigned is,unsigned imm) {return 9u<<25|((imm>>11)&15)<<21|it<<16|is<<11|(imm&0x7FF);}
constexpr std::uint32_t branch(unsigned op,unsigned it,unsigned is,int from,int to) {return lower(op,0,it,is,to-(from+1));}
constexpr std::uint32_t div(unsigned fs,unsigned fsf,unsigned ft,unsigned ftf) {return lowerSpecial(ftf<<2|fsf,ft,fs,0x38);}
constexpr Pair lo(std::uint32_t word) {return {word,UNOP};}
constexpr Pair up(std::uint32_t word) {return {LNOP,word};}
constexpr Pair nop{LNOP,UNOP};

void loadProgram(Vu1& vu,const std::vector<Pair>& code,unsigned at=0) {
    for(std::size_t n=0;n<code.size();++n)vu.writeProgram(std::uint32_t(at+n),&code[n]);
}
// Runs `code` from instruction 0, followed by an E-bit instruction and its delay slot.
void exec(Vu1& vu,std::vector<Pair> code,const Vu1::Kick& kick={}) {
    code.push_back({LNOP,UNOP|E});code.push_back(nop);
    loadProgram(vu,code);vu.run(0,kick);
}
void setFloat(Vu1& vu,unsigned reg,float x,float y,float z,float w) {vu.vf[reg]={fb(x),fb(y),fb(z),fb(w)};}
bool isFloat(const Vu1& vu,unsigned reg,float x,float y,float z,float w) {return vu.vf[reg]==std::array<std::uint32_t,4>{fb(x),fb(y),fb(z),fb(w)};}
std::uint32_t dataWord(const Vu1& vu,unsigned qword,unsigned lane) {std::uint32_t v;std::memcpy(&v,vu.data.data()+qword*16+lane*4,4);return v;}
void setData(Vu1& vu,unsigned qword,std::array<std::uint32_t,4> v) {std::memcpy(vu.data.data()+qword*16,v.data(),16);}

void memoryAndIntegers() {
    Vu1 vu;setData(vu,7,{0x11,0x20002,0x33,0x44});
    exec(vu,{lo(iaddiu(1,0,5)),lo(lower(0x00,XYZW,1,1,2)),      // vi1=5; LQ vf1,2(vi1)
        lo(lower(0x01,X|Y,1,1,3)),lo(lower(0x04,Y,2,1,2)),      // SQ.xy vf1,3(vi1); ILW.y vi2,2(vi1)
        lo(lower(0x05,Z|W,2,1,4)),                              // ISW.zw vi2,4(vi1)
        lo(iaddiu(3,0,0x7FFF)),lo(integerOp(4,3,1,0x30)),       // vi3=0x7FFF; IADD vi4=vi3+vi1
        lo(integerOp(5,3,1,0x31)),lo(integerOp(2,1,6,0x32)|0),  // ISUB vi5=vi3-vi1; IADDI vi6=vi1+2
        lo(integerOp(0x1F,1,7,0x32)),                           // IADDI vi7=vi1-1
        lo(integerOp(8,3,1,0x34)),lo(integerOp(9,3,1,0x35)),    // IAND, IOR
        lo(isubiu(10,1,7)),lo(iaddiu(0,0,9))});                 // ISUBIU vi10=vi1-7; VI0 stays 0
    check(vu.vf[1]==std::array<std::uint32_t,4>{0x11,0x20002,0x33,0x44},"LQ");
    check(dataWord(vu,8,0)==0x11 && dataWord(vu,8,1)==0x20002 && dataWord(vu,8,2)==0 && dataWord(vu,8,3)==0,"SQ writes the dest fields");
    check(vu.vi[2]==2,"ILW takes 16 bits of the dest field");
    check(dataWord(vu,9,0)==0 && dataWord(vu,9,2)==2 && dataWord(vu,9,3)==2,"ISW");
    check(vu.vi[4]==0x8004 && vu.vi[5]==0x7FFA && vu.vi[6]==7 && vu.vi[7]==4 && vu.vi[8]==5 && vu.vi[9]==0x7FFF,"integer arithmetic");
    check(vu.vi[10]==0xFFFE && vu.vi[0]==0,"ISUBIU wraps; VI0 is constant");
    check(vu.pc==16 && vu.runs==1 && vu.instructions==16,"the run ends after the E bit's delay slot");

    // Post-increment and pre-decrement forms, the register-addressed integer forms, MOVE and MR32.
    Vu1 m;setData(m,4,{1,2,3,4});setData(m,5,{5,6,7,8});m.vi[1]=4;m.vi[2]=0x20;m.vi[3]=0x30;m.vi[4]=6;m.vi[5]=0xABCD;
    exec(m,{lo(lowerSpecial(XYZW,1,1,0x34)),lo(lowerSpecial(X|W,2,1,0x34)),   // LQI vf1,(vi1++); LQI.xw vf2,(vi1++)
        lo(lowerSpecial(XYZW,2,1,0x35)),lo(lowerSpecial(XYZW,3,1,0x37)),      // SQI vf1,(vi2++); SQD vf1,(--vi3)
        lo(lowerSpecial(XYZW,3,4,0x36)),                                      // LQD vf3,(--vi4)
        lo(lowerSpecial(Z,6,3,0x3E)),lo(lowerSpecial(X|Y,5,2,0x3F)),          // ILWR.z vi6,(vi3); ISWR.xy vi5,(vi2)
        lo(lowerSpecial(Y|Z,4,1,0x30)),lo(lowerSpecial(XYZW,5,1,0x31)),       // MOVE.yz vf4,vf1; MR32 vf5,vf1
        lo(lowerSpecial(XYZW,0,1,0x30))});                                    // MOVE vf0,vf1: VF0 stays constant
    check(m.vf[1]==std::array<std::uint32_t,4>{1,2,3,4} && m.vf[2]==std::array<std::uint32_t,4>{5,0,0,8} && m.vi[1]==6,"LQI");
    check(dataWord(m,0x20,3)==4 && m.vi[2]==0x21 && dataWord(m,0x2F,0)==1 && m.vi[3]==0x2F,"SQI and SQD");
    check(m.vf[3]==std::array<std::uint32_t,4>{5,6,7,8} && m.vi[4]==5,"LQD");
    check(m.vi[6]==3 && dataWord(m,0x21,0)==0xABCD && dataWord(m,0x21,1)==0xABCD && dataWord(m,0x21,2)==0,"ILWR and ISWR");
    check(m.vf[4]==std::array<std::uint32_t,4>{0,2,3,0} && m.vf[5]==std::array<std::uint32_t,4>{2,3,4,1},"MOVE and MR32");
    check(m.vf[0]==std::array<std::uint32_t,4>{0,0,0,0x3F800000},"VF0 is constant");
    // Data addresses wrap at 1024 quadwords.
    Vu1 wrap;setData(wrap,1,{9,9,9,9});wrap.vi[1]=0x3FF;
    exec(wrap,{lo(lower(0x00,XYZW,1,1,2))});
    check(wrap.vf[1][0]==9,"data address wraps");
}

void arithmetic() {
    Vu1 vu;setFloat(vu,1,1,2,3,4);setFloat(vu,2,10,20,30,40);
    exec(vu,{up(upper(XYZW,2,1,3,0x28)),up(upper(X|Y,2,1,4,0x2C)),up(upper(XYZW,2,1,5,0x2A)),   // ADD, SUB.xy, MUL
        up(upper(XYZW,2,1,6,0x03)),up(upper(XYZW,2,1,7,0x04)),up(upper(XYZW,2,1,8,0x19)),      // ADDw, SUBx, MULy
        up(upperSpecial(XYZW,2,1,0x18)),up(upperSpecial(XYZW,2,1,0x09)),up(upper(XYZW,2,1,9,0x0A)),  // MULAx, MADDAy, MADDz
        up(upperSpecial(XYZW,2,1,0x28)),up(upper(XYZW,2,1,10,0x0C)),                           // ADDA, MSUBx
        up(upperSpecial(XYZW,2,1,0x2A)),up(upper(XYZW,2,1,11,0x29)),                           // MULA, MADD
        up(upperSpecial(XYZW,2,1,0x2C)),up(upper(XYZW,2,1,12,0x2D)),                           // SUBA, MSUB
        up(upperSpecial(XYZW,2,1,0x00)),up(upperSpecial(XYZW,2,1,0x04)),up(upperSpecial(XYZW,2,1,0x0C)), // ADDAx, SUBAx (discarded), MSUBAx
        up(upper(XYZW,0,0,13,0x28)),up(upper(XYZW,2,1,0,0x28))});                              // ADD vf13=vf0+vf0; writes to VF0 are dropped
    check(isFloat(vu,3,11,22,33,44),"ADD");
    check(isFloat(vu,4,-9,-18,0,0),"SUB with a field mask");
    check(isFloat(vu,5,10,40,90,160),"MUL");
    check(isFloat(vu,6,41,42,43,44) && isFloat(vu,7,-9,-8,-7,-6) && isFloat(vu,8,20,40,60,80),"broadcast forms");
    check(isFloat(vu,9,60,120,180,240),"MULA, MADDA, MADD accumulate");
    check(isFloat(vu,10,1,2,3,4),"ADDA then MSUBx");      // (vf1+vf2) - vf1*10
    check(isFloat(vu,11,20,80,180,320),"MULA then MADD"); // 2 * vf1*vf2
    check(isFloat(vu,12,-19,-58,-117,-196),"SUBA then MSUB"); // (vf1-vf2) - vf1*vf2
    check(vu.acc==std::array<std::uint32_t,4>{fb(-19),fb(-28),fb(-37),fb(-46)},"MSUBAx"); // (vf1-10) - vf1*10
    check(isFloat(vu,13,0,0,0,2) && isFloat(vu,0,0,0,0,1),"VF0 operands and writes");

    // Q and I operands; the I bit loads I for the upper instruction of the same pair.
    Vu1 qi;setFloat(qi,1,1,2,3,4);qi.q=fb(2);
    exec(qi,{up(upper(XYZW,0,1,2,0x1C)),up(upper(XYZW,0,1,3,0x20)),up(upper(XYZW,0,1,4,0x24)),   // MULq, ADDq, SUBq
        {fb(0.5f),IBIT|upper(XYZW,0,1,5,0x1E)},up(upper(XYZW,0,1,6,0x22)),up(upper(XYZW,0,1,7,0x26)), // MULi (I=0.5), ADDi, SUBi
        up(upperSpecial(XYZW,0,1,0x1C)),up(upper(XYZW,0,1,8,0x21)),                              // MULAq, MADDq
        up(upperSpecial(XYZW,0,1,0x1E)),up(upper(XYZW,0,1,9,0x23)),up(upper(XYZW,0,1,10,0x27))}); // MULAi, MADDi, MSUBi
    check(isFloat(qi,2,2,4,6,8) && isFloat(qi,3,3,4,5,6) && isFloat(qi,4,-1,0,1,2),"Q operand");
    check(isFloat(qi,5,0.5f,1,1.5f,2) && isFloat(qi,6,1.5f,2.5f,3.5f,4.5f) && isFloat(qi,7,0.5f,1.5f,2.5f,3.5f),"I operand");
    check(isFloat(qi,8,4,8,12,16) && isFloat(qi,9,1,2,3,4) && isFloat(qi,10,0,0,0,0),"accumulator forms with Q and I");

    // Conversions and ABS.
    Vu1 c;c.vf[1]={7,std::uint32_t(-24),24,0x40000000};setFloat(c,2,1.5f,-2.7f,1e20f,-1e20f);
    exec(c,{up(upperSpecial(XYZW,3,1,0x10)),up(upperSpecial(XYZW,4,1,0x11)),     // ITOF0, ITOF4
        up(upperSpecial(XYZW,5,2,0x14)),up(upperSpecial(XYZW,6,2,0x15)),          // FTOI0, FTOI4
        up(upperSpecial(X|Y,7,2,0x1D))});                                         // ABS.xy
    check(isFloat(c,3,7,-24,24,1073741824.0f) && isFloat(c,4,0.4375f,-1.5f,1.5f,67108864.0f),"ITOF");
    check(c.vf[5]==std::array<std::uint32_t,4>{1,std::uint32_t(-2),0x7FFFFFFF,0x80000000},"FTOI0 truncates and saturates");
    check(c.vf[6]==std::array<std::uint32_t,4>{24,std::uint32_t(-43),0x7FFFFFFF,0x80000000},"FTOI4");
    check(isFloat(c,7,1.5f,2.7f,0,0),"ABS");

    // Float model: results are rounded toward zero, denormals are zero, overflow clamps; the host's
    // rounding mode is restored.
    Vu1 f;setFloat(f,1,1.0f,1e30f,std::bit_cast<float>(0x00000001u),-1e30f);setFloat(f,2,0x1.8p-24f,1e30f,1.0f,1e30f);
    exec(f,{up(upper(X,2,1,3,0x28)),up(upper(Y|W,2,1,3,0x2A)),up(upper(Z,2,1,3,0x2A))});
    check(f.vf[3]==std::array<std::uint32_t,4>{fb(1.0f),0x7F7FFFFF,0,0xFF7FFFFF},"chop rounding, denormals, clamping");
    check(std::fegetround()==FE_TONEAREST,"host rounding restored");
}

void pipelines() {
    // DIV: Q changes 7 cycles later; a read before that sees the old value and is counted.
    Vu1 vu;setFloat(vu,1,1,2,3,4);setFloat(vu,2,2,0,-8,0);vu.q=fb(3);
    exec(vu,{lo(div(0,3,2,0)),up(upper(XYZW,0,1,3,0x1C)),nop,nop,nop,nop,       // DIV Q=vf0w/vf2x; MULq at +1
        up(upper(XYZW,0,1,4,0x1C)),up(upper(XYZW,0,1,5,0x1C))});                 // MULq at +6 and +7
    check(isFloat(vu,3,3,6,9,12) && isFloat(vu,4,3,6,9,12) && isFloat(vu,5,0.5f,1,1.5f,2),"DIV latency");
    check(vu.early_q_reads==2,"early Q reads are counted");
    // WAITQ completes the division; a division by zero gives the largest magnitude with the sign of both.
    Vu1 w;setFloat(w,1,1,2,3,4);setFloat(w,2,2,0,-8,0);
    exec(w,{lo(div(1,2,2,2)),lo(lowerSpecial(0,0,0,0x3B)),up(upper(XYZW,0,1,3,0x1C)),   // Q=vf1z/vf2z=-0.375
        lo(div(2,2,2,1)),lo(lowerSpecial(0,0,0,0x3B)),up(upper(X,0,0,4,0x20)),           // Q=-8/0; ADDq.x vf4=vf0+Q
        lo(div(0,3,2,1)),lo(div(1,0,2,0))});                                             // 1/0, then 1/2 while the first is pending
    check(isFloat(w,3,-0.375f,-0.75f,-1.125f,-1.5f) && w.vf[4][0]==0xFF7FFFFF && w.early_q_reads==0,"WAITQ and division by zero");
    check(w.q==fb(0.5f),"pending divisions complete in order and at the end of the run");

    // CLIP: flags appear 4 cycles later, six bits per CLIP; FCAND, FCOR and FCGET read them.
    Vu1 c;setFloat(c,1,2,-3,0.5f,1);setFloat(c,2,0,0,-5,-2);
    exec(c,{up(upperSpecial(X|Y|Z,1,1,0x1F)),lo(lower(0x12,0,0,0,0)|0xFFFFFF),nop,nop,     // CLIP vf1,vf1w; FCAND at +1
        lo(lower(0x1C,0,2,0,0)),lo(lower(0x12,0,0,0,0)|1),                                   // FCGET vi2 at +4; FCAND 1
        lo(integerOp(3,1,0,0x30)),lo(lower(0x13,0,0,0,0)|0xFFFFF6),lo(integerOp(4,1,0,0x30)), // vi3=vi1; FCOR; vi4=vi1
        lo(lower(0x13,0,0,0,0)|0xFFFFF0),lo(integerOp(5,1,0,0x30)),                          // FCOR (not all set); vi5=vi1
        up(upperSpecial(X|Y|Z,2,2,0x1F)),nop,nop,nop,lo(lower(0x1C,0,6,0,0))});   // CLIP vf2,vf2w; FCGET vi6
    check(c.vi[2]==9 && c.vi[3]==1 && c.vi[4]==1 && c.vi[5]==0 && c.early_clip_reads==1,"CLIP flags and their latency");
    check(c.vi[6]==0x260 && c.clip==0x260,"CLIP shifts the earlier flags"); // 9<<6 | -z.
}

void controlFlow() {
    // A loop: the delay slot runs on every pass.
    Vu1 vu;
    exec(vu,{lo(iaddiu(1,0,3)),lo(iaddiu(2,0,0)),lo(iaddiu(2,2,1)),lo(isubiu(1,1,1)),nop,
        lo(branch(0x29,0,1,5,2)),lo(iaddiu(3,3,1))});                    // IBNE vi1,vi0 -> 2
    check(vu.vi[2]==3 && vu.vi[3]==3 && vu.vi[1]==0,"IBNE loop with delay slot");
    // BAL links to the instruction after its delay slot; JR returns there. The run ends at the E bit.
    Vu1 call;
    loadProgram(call,{lo(branch(0x21,15,0,0,4)),lo(iaddiu(4,0,1)),{iaddiu(5,0,7),UNOP|E},nop,
        lo(iaddiu(6,0,9)),lo(lower(0x24,0,0,15,0)),nop});
    call.run(0,{});
    check(call.vi[15]==2 && call.vi[4]==1 && call.vi[6]==9 && call.vi[5]==7 && call.pc==4,"BAL and JR");
    // MSCNT continues after the previous end.
    loadProgram(call,{lo(iaddiu(7,0,5)),{LNOP,UNOP|E},nop},4);
    call.resume({});
    check(call.vi[7]==5 && call.pc==7 && call.runs==2,"resume");
    // Conditional forms: each taken branch skips an increment of vi9.
    Vu1 b;b.vi[1]=0xFFFF;b.vi[2]=1;
    std::vector<Pair> code;
    auto test=[&](unsigned op,unsigned it,unsigned is) {
        const int at=int(code.size());
        code.push_back(lo(branch(op,it,is,at,at+3)));code.push_back(nop);code.push_back(lo(iaddiu(9,9,1)));
    };
    test(0x28,0,0);test(0x28,1,2);  // IBEQ: taken, not taken
    test(0x2C,0,1);test(0x2C,0,2);  // IBLTZ
    test(0x2D,0,2);test(0x2D,0,0);  // IBGTZ
    test(0x2E,0,0);test(0x2E,0,2);  // IBLEZ
    test(0x2F,0,0);test(0x2F,0,1);  // IBGEZ
    test(0x20,0,0);                 // B
    exec(b,code);
    check(b.vi[9]==5,"conditional branches");
    // A flag instruction's result is read by the branch that follows it; the delayed value after any
    // other writer is refused.
    Vu1 flag;flag.clip=1;
    exec(flag,{lo(lower(0x12,0,0,0,0)|1),lo(branch(0x29,0,1,1,4)),nop,lo(iaddiu(9,9,1))});
    check(flag.vi[9]==0,"FCAND then IBNE");
    Vu1 hazard;
    check(throws([&]{exec(hazard,{lo(iaddiu(1,0,1)),lo(branch(0x29,0,1,1,4)),nop,nop});}),"branch on a just-written VI is refused");
    Vu1 endless;loadProgram(endless,{lo(branch(0x20,0,0,0,0)),nop});
    check(throws([&]{endless.run(0,{});}),"a program without an end stops");

    // One pair: both instructions read the registers as they were; the upper write wins.
    Vu1 pair;setFloat(pair,1,1,2,3,4);setFloat(pair,2,10,20,30,40);setData(pair,0,{fb(7),fb(7),fb(7),fb(7)});
    exec(pair,{{lowerSpecial(XYZW,3,1,0x30),upper(XYZW,2,1,1,0x28)},     // MOVE vf3,vf1 | ADD vf1=vf1+vf2
        {lower(0x00,XYZW,4,0,0),upper(XYZW,2,2,4,0x28)},                 // LQ vf4 | ADD vf4=vf2+vf2
        {lowerSpecial(XYZW,2,1,0x30),upper(XYZW,2,2,5,0x28)}});          // MOVE vf2,vf1 | ADD vf5=vf2+vf2
    check(isFloat(pair,3,1,2,3,4) && isFloat(pair,1,11,22,33,44),"the lower instruction reads the old register");
    check(isFloat(pair,4,20,40,60,80),"a lower write to the upper target is discarded");
    check(isFloat(pair,5,20,40,60,80) && isFloat(pair,2,11,22,33,44),"the upper instruction reads the old register");

    // XTOP, XITOP and instructions that are not modelled.
    Vu1 top;top.top=0x8D;top.itop=0x12;
    exec(top,{lo(lowerSpecial(0,1,0,0x68)),lo(lowerSpecial(0,2,0,0x69))});
    check(top.vi[1]==0x8D && top.vi[2]==0x12,"XTOP and XITOP");
    Vu1 bad;
    check(throws([&]{exec(bad,{up(upper(XYZW,2,1,3,0x2B))});}),"MAX is refused");
    check(throws([&]{exec(bad,{lo(lower(0x10,0,0,0,0))});}),"FCEQ is refused");
    check(throws([&]{exec(bad,{lo(lowerSpecial(0,0,1,0x7B))});}),"WAITP is refused");
    check(throws([&]{exec(bad,{{LNOP,UNOP|0x10000000}});}),"the D bit is refused");
}

void kicks() {
    // XGKICK copies whole GIF packets up to EOP, wrapping at the end of data memory.
    Vu1 vu;
    auto tag=[&](unsigned qword,std::uint64_t nloop,bool eop,unsigned flg,unsigned nreg) {
        const std::uint64_t t=nloop|(eop?0x8000u:0u)|(std::uint64_t(flg)<<58)|(std::uint64_t(nreg)<<60);
        std::memcpy(vu.data.data()+qword*16,&t,8);
    };
    tag(0x3FE,2,false,1,3);  // REGLIST, 2 x 3 registers: 3 quadwords (0x3FF, 0, 1).
    tag(2,1,true,0,2);       // PACKED, 1 x 2 registers: 2 quadwords, EOP.
    setData(vu,4,{0xAA,0,0,0});
    std::vector<std::vector<std::uint8_t>> packets;
    vu.vi[2]=0x3FE;
    exec(vu,{lo(lowerSpecial(0,0,2,0x6C))},[&](std::span<const std::uint8_t> p){
        check(std::fegetround()==FE_TONEAREST,"the GS runs with the host's rounding");
        packets.emplace_back(p.begin(),p.end());});
    check(packets.size()==1 && packets[0].size()==7*16 && vu.kicks==1,"XGKICK packet length");
    check(!std::memcmp(packets[0].data(),vu.data.data()+0x3FE*16,32) && !std::memcmp(packets[0].data()+32,vu.data.data(),5*16),"XGKICK wraps");
    tag(0x10,3,true,2,0);    // IMAGE, 3 quadwords.
    check(vu.gifPacket(0x10).size()==4*16,"IMAGE packet");
    tag(0x20,0x7FFF,true,0,0);
    check(throws([&]{vu.gifPacket(0x20);}),"a packet larger than VU1 memory is refused");
}

void vif() {
    Vif1 v;
    std::vector<std::vector<std::uint8_t>> direct;
    const Vu1::Kick path2=[&](std::span<const std::uint8_t> d){direct.emplace_back(d.begin(),d.end());};
    auto run=[&](const std::vector<std::uint32_t>& words) {v.check(words);v.run(words,path2,{});};
    // State codes; OFFSET clears the double buffer flag and points TOPS at BASE.
    run({0x01000404,0x0300008D,0x020001B4,0x04000012,0x05000000,0x07001234,0x00000000,0x10000000,0x11000000,0x13000000,
        0x20000000,0xDEADBEEF,0x30000000,1,2,3,4,0x31000000,5,6,7,8});
    check(v.r.cycle==0x404 && v.r.base==0x8D && v.r.ofst==0x1B4 && v.r.tops==0x8D && !v.r.dbf && v.r.itops==0x12 && v.r.mark==0x1234,"state codes");
    check(v.r.mask==0xDEADBEEF && v.r.row==std::array<std::uint32_t,4>{1,2,3,4} && v.r.col==std::array<std::uint32_t,4>{5,6,7,8} && v.codes==13,"STMASK, STROW, STCOL");
    // UNPACK V4-32: absolute, then relative to TOPS (FLG).
    run({0x6C020005,1,2,3,4,5,6,7,8, 0x6C018003,9,10,11,12});
    check(dataWord(v.vu,5,0)==1 && dataWord(v.vu,6,3)==8 && dataWord(v.vu,0x90,0)==9 && dataWord(v.vu,0x90,3)==12 && v.unpacks==2 && v.unpacked_qwords==3,"UNPACK V4-32");
    // V4-16 signed and unsigned, V4-8, V4-5 and the S forms (one value in all four fields).
    run({0x6D010010,0x7FFF8000,0x0001FFFF, 0x6D014011,0x7FFF8000,0x0001FFFF, 0x6E010012,0x7F80FF01, 0x6E014013,0x7F80FF01,
        0x6F020014,0x8000FFFF, 0x60010030,0x12345678, 0x61020016,0x7FFF8000, 0x62034018,0x0080FF01});
    check(dataWord(v.vu,0x10,0)==0xFFFF8000 && dataWord(v.vu,0x10,1)==0x7FFF && dataWord(v.vu,0x10,2)==0xFFFFFFFF && dataWord(v.vu,0x10,3)==1,"V4-16 signed");
    check(dataWord(v.vu,0x11,0)==0x8000 && dataWord(v.vu,0x11,2)==0xFFFF,"V4-16 unsigned");
    check(dataWord(v.vu,0x12,0)==1 && dataWord(v.vu,0x12,1)==0xFFFFFFFF && dataWord(v.vu,0x12,2)==0xFFFFFF80 && dataWord(v.vu,0x12,3)==0x7F,"V4-8 signed");
    check(dataWord(v.vu,0x13,1)==0xFF && dataWord(v.vu,0x13,2)==0x80,"V4-8 unsigned");
    check(dataWord(v.vu,0x14,0)==0xF8 && dataWord(v.vu,0x14,1)==0xF8 && dataWord(v.vu,0x14,2)==0xF8 && dataWord(v.vu,0x14,3)==0x80
        && dataWord(v.vu,0x15,0)==0 && dataWord(v.vu,0x15,2)==0 && dataWord(v.vu,0x15,3)==0x80,"V4-5");
    check(dataWord(v.vu,0x16,0)==0xFFFF8000 && dataWord(v.vu,0x16,3)==0xFFFF8000 && dataWord(v.vu,0x17,2)==0x7FFF,"S-16");
    check(dataWord(v.vu,0x18,0)==1 && dataWord(v.vu,0x19,3)==0xFF && dataWord(v.vu,0x1A,1)==0x80,"S-8 unsigned");
    check(dataWord(v.vu,0x30,0)==0x12345678 && dataWord(v.vu,0x30,3)==0x12345678,"S-32");

    // MPG, then MSCAL/MSCALF/MSCNT: TOP takes TOPS and the buffers swap at each start.
    std::vector<std::uint32_t> list={0x4A060002};
    for(const Pair p:{lo(lowerSpecial(0,1,0,0x68)),Pair{lowerSpecial(0,2,0,0x69),UNOP|E},nop,lo(lowerSpecial(0,3,0,0x68)),Pair{LNOP,UNOP|E},nop}) {list.push_back(p.lower);list.push_back(p.upper);}
    list.insert(list.end(),{0x14000002,0x15000005});
    run(list);
    check(v.programs==1 && v.calls==2 && v.vu.vi[1]==0x8D && v.vu.vi[2]==0x12 && v.vu.vi[3]==0x241 && v.r.tops==0x8D && !v.r.dbf,"MSCAL and MSCALF");
    // A second program block after the first: MSCNT continues at the end of the last run (instruction 8).
    std::vector<std::uint32_t> more={0x4A020008};
    for(const Pair p:{Pair{iaddiu(4,0,77),UNOP|E},nop}) {more.push_back(p.lower);more.push_back(p.upper);}
    more.push_back(0x17000000);
    run(more);
    check(v.vu.vi[4]==77 && v.calls==3 && v.r.dbf && v.r.tops==0x241 && v.r.top==0x8D,"MSCNT");
    // DIRECT and DIRECTHL pass their quadwords on.
    run({0x50000001,1,2,3,4,0x51000002,5,6,7,8,9,10,11,12});
    check(direct.size()==2 && direct[0].size()==16 && direct[1].size()==32 && direct[1][0]==5 && v.directs==2 && v.direct_qwords==3,"DIRECT");

    // Forms that are not modelled are refused by the check, before any effect.
    const auto before=v.r;const auto codes=v.codes;
    auto refused=[&](std::vector<std::uint32_t> words,std::uint32_t code,std::size_t index) {
        try {v.check(words);} catch(const Vif1Unsupported& u) {return u.code==code && u.index==index && v.codes==codes && v.r.base==before.base && v.r.cycle==before.cycle;}
        return false;
    };
    check(refused({0x03000001,0x08000000},0x08000000,1),"undefined command");
    check(refused({0x64010000,1,2},0x64010000,0) && refused({0x68010000,1,2,3},0x68010000,0),"UNPACK V2 and V3");
    check(refused({0x7C010000,1,2,3,4},0x7C010000,0),"UNPACK with masking");
    check(refused({0x05000001,0x6C010000,1,2,3,4},0x6C010000,1),"UNPACK with an addition mode");
    check(refused({0x01000104,0x6C010000,1,2,3,4},0x6C010000,1),"UNPACK with a skipping write cycle");
    check(refused({0x6C020000,1,2,3,4},0x6C020000,0) && refused({0x50000001,1},0x50000001,0) && refused({0x4A010000,1},0x4A010000,0),"data continuing in the next transfer");
    check(refused({0x80000000},0x80000000,0) && refused({0x00000000,0x1C000000},0x1C000000,1),"interrupt bit and unknown command");
    check(v.r.mode==0 && v.r.cycle==0x404,"a refused check changes nothing");
    // MSKPATH3 holds or releases PATH3; the check leaves the register alone.
    v.check(std::vector<std::uint32_t>{0x06008000});check(!v.r.mskpath3,"MSKPATH3 check has no effect");
    v.run(std::vector<std::uint32_t>{0x06008000},Vu1::Kick{},Vu1::Kick{});check(v.r.mskpath3,"MSKPATH3 mask");
    v.run(std::vector<std::uint32_t>{0x06000000},Vu1::Kick{},Vu1::Kick{});check(!v.r.mskpath3,"MSKPATH3 release");
}
}

// The program a start runs, as told from program memory (vu1_program.h), and the entry hook.
void programs() {
    Vu1 vu;
    // 0: a pair; 1: IBNE back to 0; 2: its delay pair; 3: E bit; 4: the pair after it; 5: never run.
    loadProgram(vu,{nop,lo(branch(0x29,1,2,1,0)),nop,{LNOP,UNOP|E},nop,nop});
    const auto first=Vu1Program::reachable(vu.program,0);
    check(first.start==0 && first.pairs.size()==5 && first.pairs.front().address==0 && first.pairs.back().address==4 && !first.indirect,"reachable pairs end after the E bit's delay pair");
    const Pair other{LNOP,upperSpecial(0,1,2,0x2F)};
    vu.writeProgram(5,&other);check(Vu1Program::reachable(vu.program,0).key==first.key,"a pair outside the program does not change its key");
    vu.writeProgram(4,&other);check(Vu1Program::reachable(vu.program,0).key!=first.key,"a pair inside the program changes its key");
    check(Vu1Program::reachable(vu.program,3).pairs.size()==2 && Vu1Program::reachable(vu.program,3).key!=first.key,"another start is another program");
    // An unconditional branch: its delay pair runs, the pair after that does not belong.
    Vu1 jump;loadProgram(jump,{lo(branch(0x20,0,0,0,3)),nop,nop,{LNOP,UNOP|E},nop});
    const auto jumped=Vu1Program::reachable(jump.program,0);
    check(jumped.pairs.size()==4 && jumped.pairs[2].address==3,"the pair after an unconditional branch's delay pair is not reached");
    // A call and a register jump: the return address is taken as the jump's target.
    Vu1 call;loadProgram(call,{lo(branch(0x21,15,0,0,4)),nop,{LNOP,UNOP|E},nop,lo(lower(0x24,0,0,15,0)),nop});
    const auto called=Vu1Program::reachable(call.program,0);
    check(called.indirect && called.pairs.size()==6,"a register jump reaches the return address of the program's call");
    // The entry hook fires once per start address until program memory changes.
    unsigned entries=0;vu.on_entry=[&](std::uint32_t start){check(start==0,"entry address");++entries;};
    vu.vi[1]=vu.vi[2]=0;vu.run(0,{});vu.run(0,{});check(entries==1,"one entry while the program is unchanged");
    vu.writeProgram(5,&other);vu.run(0,{});check(entries==2,"a write to program memory makes the next start a new entry");
    check(vu.native_runs==0 && Vu1::nativePrograms().empty(),"no native translations in this build");
}

// Native programs registered by key, and the verification mode that runs them against the
// interpreter. The programs here are the test's own; the registry of hand-written programs holds
// only keys of the game's programs, which no test contains.
bool nativeRight(Vu1& vu,const Vu1::Kick&) {vu.vi[1]=5;vu.vi[2]=7;vu.instructions+=4;vu.pc=4;return true;}
bool nativeWrongRegister(Vu1& vu,const Vu1::Kick&) {vu.vi[1]=5;vu.vi[2]=8;vu.instructions+=4;vu.pc=4;return true;}
bool nativeWrongCount(Vu1& vu,const Vu1::Kick&) {vu.vi[1]=5;vu.vi[2]=7;vu.instructions+=3;vu.pc=4;return true;}
bool nativeLeaves(Vu1& vu,const Vu1::Kick&) {vu.vi[1]=5;++vu.instructions;vu.pc=1;return false;} // The first pair only.
void nativePrograms() {
    const std::vector<Pair> code{lo(iaddiu(1,0,5)),lo(iaddiu(2,1,2)),{LNOP,UNOP|E},nop};
    const auto started=[&](Vu1& vu,Vu1::NativeProgram native) {
        loadProgram(vu,code);
        vu.extra_programs={{Vu1Program::reachable(vu.program,0).key,native}};
    };
    Vu1 plain;loadProgram(plain,code);plain.run(0,{});
    check(plain.vi[1]==5 && plain.vi[2]==7 && plain.instructions==4 && plain.pc==4 && plain.native_runs==0,"the interpreter's result");
    Vu1 native;started(native,nativeRight);native.run(0,{});native.run(0,{});
    check(native.native_runs==2 && native.hand_written_runs==0 && native.vi==plain.vi && native.instructions==8,"a registered program runs in the interpreter's place");
    Vu1 other;started(other,nativeRight);const Pair changed=lo(iaddiu(2,1,3));other.writeProgram(1,&changed);other.run(0,{});
    check(other.native_runs==0 && other.vi[2]==8,"another program under the same start has another key");
    Vu1 off;started(off,nativeWrongRegister);off.use_native=false;off.run(0,{});
    check(off.native_runs==0 && off.vi[2]==7,"native programs can be switched off");
    Vu1 verified;started(verified,nativeRight);verified.verify_native=true;verified.run(0,{});verified.run(0,{});
    check(verified.verified_runs==2 && verified.vi==plain.vi && verified.instructions==8,"verification passes an equal program and counts each start once");
    Vu1 wrong;started(wrong,nativeWrongRegister);wrong.verify_native=true;
    check(throws([&]{wrong.run(0,{});}),"verification stops at a different register");
    Vu1 miscounted;started(miscounted,nativeWrongCount);miscounted.verify_native=true;
    check(throws([&]{miscounted.run(0,{});}),"and at a different pair count");
    Vu1 partial;started(partial,nativeLeaves);partial.verify_native=true;partial.run(0,{});
    check(partial.verified_runs==1 && partial.vi==plain.vi && partial.instructions==4,"a program that leaves early is finished by the interpreter");
    // The hand-written programs: one entry per key.
    const auto by_hand=Vu1::handWrittenPrograms();
    check(!by_hand.empty(),"the build has hand-written programs");
    for(std::size_t a=0;a<by_hand.size();++a) {
        check(by_hand[a].run!=nullptr,"each has its function");
        for(std::size_t b=a+1;b<by_hand.size();++b)check(by_hand[a].key!=by_hand[b].key,"each key once");
    }
}

int main() {
    memoryAndIntegers();
    arithmetic();
    pipelines();
    controlFlow();
    kicks();
    vif();
    programs();
    nativePrograms();
    std::cout<<"VU1 tests passed\n";
    return 0;
}
