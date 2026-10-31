// VU1: 16 KB of data memory, 16 KB of microprogram memory and the microprogram interpreter
// (docs/plans/graphics/VU1_PIPELINE_PLAN.md). VIF1 fills both memories (UNPACK, MPG) and starts
// programs (MSCAL, MSCALF, MSCNT); a program runs synchronously to its E bit and sends GS packets
// with XGKICK (PATH1). Only the instructions the game's microprograms use are modelled; any other
// stops by name.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <vector>

class Vu1 {
public:
    // Receives each XGKICK packet (whole GIF packets up to EOP, copied out of data memory).
    using Kick=std::function<void(std::span<const std::uint8_t>)>;
    static constexpr std::uint32_t data_mask=0x3FF,program_mask=0x7FF; // Quadwords; instructions.

    std::array<std::uint8_t,16384> data{},program{};
    std::array<std::array<std::uint32_t,4>,32> vf{}; // Bit patterns (x, y, z, w); VF0 is (0, 0, 0, 1).
    std::array<std::uint16_t,16> vi{};               // VI0 is 0.
    std::array<std::uint32_t,4> acc{};
    std::uint32_t q=0,i=0,clip=0; // Q and I as bit patterns; the 24 clipping flags.
    std::uint32_t pc=0;           // Instruction index where MSCNT continues.
    std::uint32_t top=0,itop=0;   // Set by VIF1 at each start (XTOP, XITOP).
    std::uint64_t instructions=0;
    unsigned runs=0,kicks=0,native_runs=0; // `native_runs`: starts that ran a native program (hand-written or translated).
    unsigned hand_written_runs=0;          // Those of them that ran a hand-written program.
    std::uint64_t kick_hash=0xCBF29CE484222325ull; // Over every XGKICK packet's bytes, in order: equal hashes mean equal output.
    // Reads of Q or the clipping flags while a newer result was still in the pipeline. The model
    // counts one cycle per instruction (no stalls), so a nonzero count marks timing-sensitive code.
    unsigned early_q_reads=0,early_clip_reads=0;

    // Called with the start address the first time a start runs from it since program memory last
    // changed (the program census, native program lookup).
    std::function<void(std::uint32_t)> on_entry;
    // Called by VIF1 after each start or continuation when set: where it began, the instruction
    // pairs it ran and the time it took in nanoseconds (the program census's cost per program).
    std::function<void(std::uint32_t,std::uint64_t,std::uint64_t)> on_ran;
    std::uint32_t program_generation=1; // Advances with every write to program memory through `writeProgram`.
    // One instruction pair (8 bytes) to program memory at instruction index `index`.
    void writeProgram(std::uint32_t index,const void* pair) {std::memcpy(program.data()+(index&program_mask)*8,pair,8);++program_generation;}

    // Native translations of microprograms (generated locally from the game's own programs by
    // tools/vu1/translate_programs.py; none in a build without them). A translation is chosen by the
    // key of the program reachable from the start address (vu1_program.h) and behaves exactly as the
    // interpreter does on it. It returns true when the program has ended, false with `pc` set when
    // it reached a pair outside itself.
    using NativeProgram=bool(*)(Vu1&,const Kick&);
    struct NativeEntry {std::uint64_t key;NativeProgram run;};
    static std::span<const NativeEntry> nativePrograms();
    bool use_native=true; // Off: every program is interpreted.
    // Hand-written native programs (vu1_handwritten_registry.cpp, the project's own code): a microprogram
    // written out as what it does, with the same packets and the same state left behind as the
    // interpreter. Chosen by the same key and before a translation. `extra_programs` are looked up
    // first of all (tests register programs of their own there).
    static std::span<const NativeEntry> handWrittenPrograms();
    std::vector<NativeEntry> extra_programs;
    bool use_hand_written=true; // Off: translations and the interpreter only.
    // Verification of native programs: every start that has one runs in the interpreter first and
    // then natively from the same state; the packets sent and the state left behind (memory, the
    // registers, ACC, Q, I, the clipping flags, where it stopped, the pair count) must be equal, or
    // the run stops naming the program's key. Several times slower; for checking, not for play.
    bool verify_native=false;unsigned verified_runs=0;
    static constexpr std::uint64_t run_limit=16'000'000; // Instruction pairs per start; a program that never ends stops here.

    Vu1() {vf[0][3]=0x3F800000;}
    void run(std::uint32_t start,const Kick& kick); // MSCAL/MSCALF: from instruction `start`.
    void resume(const Kick& kick) {run(pc,kick);}   // MSCNT.
    // The GIF packets at data address `address` (quadwords, wrapping), up to and including EOP.
    std::vector<std::uint8_t> gifPacket(std::uint32_t address) const;

private:
    friend struct Vu1Native; // The generated translations.
    friend struct Vu1Hand;   // The hand-written programs.
    struct Result {enum Target {None,Register,Accumulator} target=None;unsigned reg=0,mask=0;std::array<std::uint32_t,4> value{};};
    Result upper(std::uint32_t word,std::uint32_t at);
    void lower(std::uint32_t word,std::uint32_t at,const Kick& kick); // (inline: vu1_lower.h)
    void lowerDiv(std::uint32_t word);                                // DIV and XGKICK, out of line (vu1.cpp)
    void lowerKick(std::uint32_t word,std::uint32_t at,const Kick& kick);
    [[noreturn]] static void refuseBranch(unsigned reg,std::uint32_t at);
    void setVi(unsigned reg,std::uint16_t value,bool flag_instruction=false);
    std::uint16_t branchVi(unsigned reg,std::uint32_t at) const;
    std::uint8_t* cell(std::uint32_t address) {return data.data()+(address&data_mask)*16;}
    std::uint32_t readQ();
    std::uint32_t readClip();
    void settle(bool all); // Publishes pipeline results that are due (all of them at the end of a run).
    void interpret(const Kick& kick); // From `pc` to the program's end.
    void runVerified(NativeProgram native,const Kick& kick);

    std::uint64_t cycle_=0;
    bool q_pending_=false;std::uint64_t q_ready_=0;std::uint32_t q_next_=0; // DIV: 7 cycles.
    struct ClipEntry {std::uint64_t ready;std::uint32_t value;};
    std::array<ClipEntry,4> clip_queue_{};unsigned clip_count_=0;std::uint32_t clip_latest_=0; // CLIP: 4 cycles.
    unsigned vi_written_=0;std::uint64_t vi_written_cycle_=0; // Last VI write other than by a flag instruction.
    bool branch_=false;std::uint32_t branch_target_=0;
    std::array<std::uint32_t,program_mask+1> entry_generation_{}; // The generation each start address was last looked up in.
    std::array<NativeProgram,program_mask+1> native_{};          // Its native program, if any.
    std::array<bool,program_mask+1> native_by_hand_{};           // Whether that one is hand-written.
    unsigned host_csr_=0; // The float mode outside the run, restored around XGKICK.
    // An instruction pair decoded once: what the run loop needs without taking the words apart again.
    // An entry is current while its words equal program memory, so no writer has to invalidate it.
    // `Slow` kinds go through `upper` and `lower`, which also refuse what is not modelled.
    struct Decoded {
        enum Upper : std::uint8_t {UpperSlow,UpperNop,UpperRegister,UpperAccumulator};
        enum Lower : std::uint8_t {LowerSlow,LowerNop,LowerLq,LowerSq,LowerLqi,LowerSqi};
        std::uint32_t low=0,high=0;
        Upper upper=UpperSlow;Lower lower=LowerSlow;
        std::uint8_t operation=0,source=0,fs=0,ft=0,reg=0,field=0,mask=0; // Upper arithmetic.
        std::uint8_t lower_vf=0,lower_vi=0,lower_mask=0;std::int16_t lower_imm=0; // Lower loads and stores.
        bool valid=false,i_bit=false,ending=false,debug_bits=false,skip_lower=false;
    };
    std::array<Decoded,program_mask+1> decoded_{};
    void decode(Decoded& d,std::uint32_t low,std::uint32_t high) const;
    std::vector<std::uint8_t> packet_; // The XGKICK packet being sent; kept to reuse its storage.
    void gifPacketInto(std::vector<std::uint8_t>& packet,std::uint32_t address) const;
    std::uint32_t gifPacketQwords(std::uint32_t address) const; // The packets' length up to and including EOP.
};

#include "vu1_lower.h"
