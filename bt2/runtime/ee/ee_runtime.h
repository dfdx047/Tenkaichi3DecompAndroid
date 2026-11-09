#pragma once

// The full EE runtime (hand-written code only). Generated code sees just its base, PS2Runtime
// (ps2_runtime.h), so edits here do not recompile generated units. Broader PS2 APIs fail explicitly.
#include <string>
#include <atomic>
#include <algorithm>
#include <array>
#include <map>
#include <bit>
#include <cstdint>
#include <stdexcept>
#include <cstring>
#include <span>
#include <set>
#include <unordered_map>
#include <vector>
#include <optional>
#include <functional>
#include <iostream>
#include <immintrin.h>
#include "ps2_runtime.h"
#include "ee_interpreter_handlers.h"
#include "intc_checkpoint.h"
#include "retained_ee_stack.h"
#include "gs/gif_dma_staging.h"
#include "logical_timer2.h"
#include "logical_timer1.h"
#include "post_reset_sif.h"
#include "gs/vif_startup.h"
#include "gs/texture_gif_startup.h"
#include "ipu_device.h"
#include "services/sif_rpc.h"
#include "services/iop_heap.h"
#include "services/cri_audio.h"
#include "services/audio_mixer.h"
#include "services/card_folder.h"
#include "services/devices/pad_input.h"
#include "services/sound_sampler.h"
#include "stopped_dma_channels.h"
#include "vu/vif1.h"
#include <memory>
struct EeExecutionOwner;

struct UnsupportedSyscall : std::runtime_error {
    UnsupportedSyscall(std::uint32_t selector, std::uint32_t pc, std::uint32_t handler = 0)
        : std::runtime_error("Unsupported bootstrap syscall"), selector(selector), resume_pc(pc), handler(handler) {}
    std::uint32_t selector, resume_pc, handler;
};

struct UnsupportedGuestBranch : std::runtime_error {
    UnsupportedGuestBranch(std::uint32_t target, std::uint32_t source, std::uint32_t resume)
        : std::runtime_error("Unsupported bootstrap guest dispatch"), target(target), source_pc(source), resume_pc(resume) {}
    std::uint32_t target, source_pc, resume_pc;
};

struct UnsupportedGuestMemory : std::runtime_error {
    UnsupportedGuestMemory(std::uint32_t address, std::size_t size)
        : std::runtime_error("Unsupported or out-of-bounds guest memory access"), address(address), size(size) {}
    std::uint32_t address;
    std::size_t size;
};

struct UnsupportedVuStatus : std::runtime_error {
    UnsupportedVuStatus():std::runtime_error("Unsupported VU0 VPU_STAT read") {}
};


class EeRuntime : public PS2Runtime {
public:
    // CPU exceptions raised by generated code (trapping ADD/ADDI) stop at a checked boundary.
    [[noreturn]] void SignalException(R5900Context*, PS2Exception exception) {
        throw std::runtime_error("EE exception "+std::to_string(int(exception))+" raised by generated code");
    }
    // VU activity as generated code sees it (vu0_macro.cpp): the BC2T/BC2F condition, CFC2/CTC2 of the
    // status flag register, and VPU_STAT. No VU microprogram is ever started in the frame model.
    bool cop2Condition(R5900Context* context);
    std::uint32_t readVuStatusFlag(R5900Context* context);
    void writeVuStatusFlag(R5900Context* context,std::uint32_t value);
    // Flag effects of a VU0 macro arithmetic instruction (from its word), run before the generated code
    // for it wherever a function clears and then reads the status flags (prepare_runtime.py).
    void vu0UpdateFlags(R5900Context* context,std::uint32_t instruction);
    std::uint16_t vu0_status_flags_=0; // Z (bit 0), S (bit 1) of the last operation; sticky ZS (6), SS (7).
    std::uint32_t readVuStatus(R5900Context* context);
    // Diagnostic handoff: retain the parent while running the ready sound child.
    // The priority request remains pending; the first wait is a checked boundary.
    void probeReadySoundThread(std::uint8_t*, R5900Context*);
    bool ee_sound_dispatch_probe = false, ee_sound_probe_attempted = false;
    // Diagnostic phase stop for capture comparisons: the first entry to this address, or the first
    // return to it, stops as a branch boundary. Zero disables it; it never changes guest state.
    std::uint32_t phase_stop = 0;
    std::vector<std::uint32_t> stop_returns; // Guest return addresses live when a stop first unwound.
    R5900Context ee_sound_probe_frontier{}, ee_sound_probe_parent{};
    unsigned ee_sound_probe_waits = 0;
    RetainedEeStack* active_ee_stack = nullptr;
    bool ee_queued_sound_rpc=false, sound_thread_waiting=false;
    struct QueuedSoundsBind { bool pending=false;unsigned epoch=0;std::array<unsigned,16> reply{}; } queued_sounds_bind;
    unsigned queued_sound_submissions=0,queued_sound_completions=0,sound_dispatch_bind_id=0;
    std::uint32_t sound_dispatch_wait_ra=0,sound_dispatch_wait_sp=0;
    unsigned startup_worker_dispatches=0,startup_worker_status_queries=0,startup_worker_waits=0;
    R5900Context startup_dispatch_parent{};
    // Scheduler scope opened by main's admitted priority request. Dispatched workers use
    // kernel thread semantics; any transition requiring preemption fails closed.
    bool startup_scheduler_active=false;
    unsigned scheduled_priority_changes=0,scheduled_resumes=0,scheduled_suspends=0,main_priority_admissions=0;
    unsigned scheduled_wakes=0,main_yields=0,scheduled_starts=0;
    // Main's own sleep: it is not runnable while sleeping; wakeups that arrive first are banked.
    unsigned frame_csr_reads=0;
    bool main_sleeping=false;unsigned main_wakeups=0,main_sleeps=0,main_wakes=0,main_sleep_fields=0;
    bool sleepMainThread(std::uint8_t*,R5900Context*);
    void wakeMainThread();
    bool mainYieldsToReadyWorkers(std::uint8_t*,R5900Context*);
    std::uint32_t main_priority_previous=0;
    // Main executing without an idle point still spends time: after this many guest calls or loop
    // iterations (backward branches) since the last VBLANK a field has elapsed (logical CPU time; busy-wait
    // loops and long computations such as decompression rely on it). Delivered only with interrupts enabled.
    static constexpr unsigned main_calls_per_field=100000,main_checkpoints_per_field=500000;
    static constexpr unsigned frame_checkpoint_limit=400000000; // Loop iterations within one field once frames are presented.
    unsigned checkpoint_limit=0; // Diagnostic override of the runaway guard (BT2_EE_LOOP_LIMIT); 0 keeps the default.
    unsigned main_calls_since_field_=0,main_checkpoints_since_field_=0,busy_main_vblanks=0;
    R5900Context* main_context_=nullptr; // Main's context as last seen at a guest branch.
    // VU0 data memory (4 KB) and the macro-mode instructions that address it (vu0_macro.cpp).
    std::array<std::uint8_t,4096> vu0_memory_{};unsigned vu0_memory_instructions=0;
    void vu0MemoryInstruction(R5900Context* context,std::uint32_t instruction);
    // VU0's random number register R: 23 bits, read as a float in [1, 2) (vu0_macro.cpp).
    std::uint32_t vu0_random_=0x3F800000;unsigned vu0_random_instructions=0;
    void vu0RandomInstruction(R5900Context* context,std::uint32_t instruction);
    void busyMainField();
    unsigned cri_stream_transfers=0,load_poll_idles=0,asset_poll_idles=0,dbc_port_calls=0,dbc_main_calls=0,scmd_calls=0;
    bool handleThreadSyscall(std::uint8_t*,std::uint32_t,R5900Context*);
    void createThread(std::uint8_t*,R5900Context*);
    void writeThreadStatus(std::uint8_t*,std::uint32_t id,std::uint32_t out,std::uint32_t pc);
    void exitThread(std::uint32_t selector,R5900Context*);
    unsigned thread_exits=0,thread_deletions=0,thread_terminations=0;
    void requireNoPreemption(std::uint32_t,const R5900Context*) const;
    bool handleAdmittedIntcSyscall(std::uint8_t*,std::uint32_t,R5900Context*);
    unsigned vblank_handler_registrations=0,vblank_enable_calls=0,vblank_disable_calls=0;
    bool in_interrupt_=false;
    unsigned vblank_deliveries=0,worker_wakes=0,worker_resumes=0,worker_sleep_returns=0;
    bool handleInterruptSyscall(std::uint8_t*,std::uint32_t,R5900Context*);
    void deliverVblank(std::uint8_t*,R5900Context*);
    void advanceLogicalField();
    void runIntcHandlers(std::uint8_t*,R5900Context*,std::uint32_t,const char*);
    void runReadyThreads(std::uint8_t*,R5900Context*);
    void queueSoundsBind(R5900Context*,const std::array<unsigned,16>&);
    void finishQueuedSoundsBind(std::uint8_t*,R5900Context*);


    static constexpr std::size_t ram_size = 32 * 1024 * 1024;


    void registerFunction(std::uint32_t address, RecompiledFunction function) {
        if (address >= 0x80000000 || !function || !functions_.emplace(address, function).second)
            throw std::runtime_error("Invalid or duplicate guest registration");
    }

    // Tool runs only (the game starter, runtime/probe/game_starter.cpp): a host function in place of
    // a registered one; returns the function it replaces.
    RecompiledFunction replaceFunction(std::uint32_t address, RecompiledFunction function) {
        const auto found = functions_.find(address);
        if (found == functions_.end() || !function) throw std::runtime_error("Invalid guest replacement");
        const auto replaced = found->second;
        found->second = function;
        return replaced;
    }

    // A function generated from a runtime-loaded overlay: dispatchable only while main RAM holds the
    // exact bytes it was generated from (checked on every dispatch; EE_OVERLAY_PLAN.md).
    void registerOverlayFunction(std::uint32_t address, RecompiledFunction function, std::span<const std::uint8_t> expected) {
        if (address >= ram_size || !function || expected.empty() || expected.size() % 4 || expected.size() > ram_size - address
                || !functions_.emplace(address, function).second)
            throw std::runtime_error("Invalid or duplicate overlay registration");
        overlay_code_bindings_.emplace(address, std::vector<std::uint8_t>(expected.begin(), expected.end()));
        registerFunctionExtent(address, address + static_cast<std::uint32_t>(expected.size()));
    }

    // The extent [start, end) of a generated function, which the interpreter needs to tell a jump within
    // the function from one to another (ee_interpreter.cpp). Overlay functions get theirs on registration.
    void registerFunctionExtent(std::uint32_t start, std::uint32_t end) {
        if (end <= start || start % 4 || end % 4 || !function_extents_.emplace(start, end).second)
            throw std::runtime_error("Invalid or duplicate function extent");
    }

    // EE interpreter bridge (ee_interpreter.cpp; WHOLE_GAME_COVERAGE_PLAN.md, step 3): code no generated
    // function holds runs interpreted, with one handler per instruction word made by the recompiler.
    // Without handlers (tests, builds without the generated table) such a dispatch stops as before.
    void setInterpreterHandlers(std::span<const EeInterpreterHandlerPart> parts);
    void interpret(std::uint8_t* rdram, R5900Context* context);
    static void interpretEntry(std::uint8_t* rdram, R5900Context* context, PS2Runtime* runtime); // A function that interprets from context->pc.
    struct InterpretedEntry {std::uint64_t dispatches=0, instructions=0; std::uint32_t first_word=0;};
    std::map<std::uint32_t, InterpretedEntry> interpreted_entries; // Unmapped code that ran, by entry address.
    std::uint64_t interpreted_instructions=0; // Instructions of unmapped code run by the interpreter.
    bool interpret_all=false; // BT2_EE_INTERPRET=all: every generated function runs interpreted (diagnostic comparison).
    std::uint64_t interpret_all_instructions=0;

    // An entry inside a registered function: a label of its generated entry switch, so the function
    // resumes there when called with that pc (WHOLE_GAME_COVERAGE_PLAN.md, step 1). Overlay entries
    // are checked against the bytes of the function that holds them.
    void registerInteriorEntry(std::uint32_t address, RecompiledFunction function, std::uint32_t owner) {
        const auto found = functions_.find(owner);
        if (address <= owner || address % 4 || found == functions_.end() || found->second != function
                || functions_.contains(address) || !interior_entries_.emplace(address, InteriorEntry{function, owner}).second)
            throw std::runtime_error("Invalid or duplicate interior entry registration");
    }

    void registerKernelFunction(std::uint32_t address, RecompiledFunction function,
                                std::span<const std::uint8_t> expected_payload);

    void callFunction(std::uint32_t address, R5900Context& context) {
        const auto found = functions_.find(address);
        if (found == functions_.end()) throw std::runtime_error("Unregistered guest target");
        verifyKernelBinding(address);
        context.pc = address;
        checkpoints_ = 0;
        entered_functions.insert(address);
        found->second(memory_.empty() ? nullptr : memory_.data(), &context, this);
        completed_functions.insert(address);
    }

    // Isolated logical RPC_END callback validation. The caller supplies checked
    // capture state; this does not emulate interrupt entry or schedule delivery.
    void validateRpcEndCallback(R5900Context& context) {
        if (rpc_end_callback_active_ || context.r[4].low != 0x3A94C0
                || context.r[5].low != 0x3AAFC0 || context.r[28].low != 0x38BDF0)
            throw std::runtime_error("Invalid isolated RPC_END callback context");
        rpc_end_callback_active_ = true;
        try { callFunction(0x3322E8, context); }
        catch (...) { rpc_end_callback_active_ = false; throw; }
        rpc_end_callback_active_ = false;
    }

    void expectStartupZeroRegion(std::uint32_t start, std::uint32_t end) {
        if (startup_zero_end_ || thread_initialized || end <= start || end > memory_.size())
            throw std::runtime_error("Invalid startup zero checkpoint");
        startup_zero_start_ = start;
        startup_zero_end_ = end;
    }

    void handleSyscall(std::uint8_t* rdram, R5900Context* context, std::uint32_t code);

    bool dispatchGuestBranch(std::uint8_t* rdram, R5900Context* context, std::uint32_t target,
                             std::uint32_t source, std::uint32_t resume, GuestBranchKind kind, const char*);

    void loadMemory(std::span<const std::uint8_t> bytes) {
        if (bytes.size() != ram_size) throw std::runtime_error("Expected bounded 32 MiB guest image");
        memory_.assign(bytes.begin(), bytes.end());
    }

    std::span<std::uint8_t> memory() { return memory_; }
    // Scratchpad RAM (0x70000000-0x70003FFF), a separate 16 KiB buffer zero-filled as on reset. Every
    // accessor checks this window before main RAM; accesses must be naturally aligned and inside it.
    static constexpr std::size_t scratchpad_size = 16 * 1024;
    std::span<std::uint8_t> scratchpad() { return scratchpad_; }
    std::uint8_t* scratchpadAt(std::uint32_t address, unsigned size) {
        if (address < 0x70000000 || address >= 0x70000000 + scratchpad_size) return nullptr;
        if (address % size || address - 0x70000000 + size > scratchpad_size) throw UnsupportedGuestMemory(address, size);
        ++scratchpad_accesses;
        return scratchpad_.data() + (address - 0x70000000);
    }
    unsigned scratchpad_accesses = 0;

    // PS2SDK UNCACHED_SEG: reached low 32 MiB main-RAM alias only.
    // Enabled after the explicit RPCINIT query; no TLB/MMIO or cache state.
    std::uint32_t uncachedWordAddress(std::uint8_t* rdram, std::uint32_t address) {
        if (address < 0x20000000 || address >= 0x22000000) return address;
        if (sif_rpc_reads != 1 || address % 4) throw UnsupportedGuestMemory(address, 4);
        const auto physical = address - 0x20000000;
        checkMemory(rdram, physical, 4);
        ++uncached_word_accesses;
        return physical;
    }
    unsigned uncached_word_accesses = 0;
    std::uint32_t uncachedByteAddress(std::uint8_t* rdram, std::uint32_t address) {
        if (address < 0x20000000 || address >= 0x22000000) return address;
        if (sif_rpc_reads != 1) throw UnsupportedGuestMemory(address, 1);
        const auto physical = address - 0x20000000;
        checkMemory(rdram, physical, 1);
        ++uncached_byte_accesses;
        return physical;
    }
    unsigned uncached_byte_accesses = 0;
    // The same alias for halfword, doubleword and quadword accesses (owned RAM is coherent).
    std::uint32_t uncachedWideAddress(std::uint32_t address, unsigned size) {
        if (address < 0x20000000 || address >= 0x22000000) return address;
        if (sif_rpc_reads != 1) throw UnsupportedGuestMemory(address, size);
        ++uncached_wide_accesses;
        return address - 0x20000000;
    }
    unsigned uncached_wide_accesses = 0;
    void write8(std::uint8_t* rdram, std::uint32_t address, std::uint8_t value) {
        if (auto* spr = scratchpadAt(address, 1)) {*spr = value; return;}
        address = uncachedByteAddress(rdram, address);
        checkMemory(rdram, address, 1);
        memory_[address] = value;
        ++byte_writes;
    }

    std::uint16_t read16(std::uint8_t* rdram,std::uint32_t address) {
        if(auto* spr=scratchpadAt(address,2)){std::uint16_t value;std::memcpy(&value,spr,2);return value;}
        address=uncachedWideAddress(address,2);
        checkMemory(rdram,address,2);
        if(address%2)throw std::runtime_error("Unsupported unaligned halfword load");
        std::uint16_t value;std::memcpy(&value,memory_.data()+address,2);return value;
    }
    void write16(std::uint8_t* rdram,std::uint32_t address,std::uint16_t value) {
        if(auto* spr=scratchpadAt(address,2)){std::memcpy(spr,&value,2);return;}
        address=uncachedWideAddress(address,2);
        checkMemory(rdram,address,2);
        if(address%2)throw std::runtime_error("Unsupported unaligned halfword store");
        std::memcpy(memory_.data()+address,&value,2);++halfword_writes;
    }
    ProbeGpr read128(std::uint8_t* rdram,std::uint32_t address) {
        if(auto* spr=scratchpadAt(address,16)){ProbeGpr value;std::memcpy(&value,spr,16);return value;}
        address=uncachedWideAddress(address,16);
        if(address==0x10005000)[[unlikely]]return vif1FifoRead(); // VIF1 FIFO: local->host GS data.
        checkMemory(rdram,address,sizeof(ProbeGpr));
        if(address%16)throw std::runtime_error("Unsupported unaligned quadword load");
        ProbeGpr value;std::memcpy(&value,memory_.data()+address,sizeof(value));return value;
    }
    // Vector register stores (VU0 macro mode: SQC2) pass the lanes as an integer vector.
    void write128(std::uint8_t* rdram, std::uint32_t address, __m128i value) { write128(rdram, address, std::bit_cast<ProbeGpr>(value)); }
    void write128(std::uint8_t* rdram, std::uint32_t address, const ProbeGpr& value) {
        if (auto* spr = scratchpadAt(address, 16)) {std::memcpy(spr, &value, 16); return;}
        address = uncachedWideAddress(address, 16);
        checkMemory(rdram, address, sizeof(value));
        if (address % 16) throw std::runtime_error("Unsupported unaligned quadword store");
        if (address == quadword_watch_address && on_watched_quadword) on_watched_quadword(value);
        std::memcpy(memory_.data() + address, &value, sizeof(value));
        ++quadword_writes;
    }

    void write32(std::uint8_t* rdram, std::uint32_t address, std::uint32_t value);

    // Generated doubleword accesses: addresses in the EE register window take the same checked
    // MMIO path as constant-address accesses; everything else is main RAM.
    static bool mmioWindow(std::uint32_t address) {return address>=0x10000000 && address<0x14000000;}
    void guestWrite64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value) {
        if(mmioWindow(address))Store64(rdram,context,address,value);else write64(rdram,address,value);
    }
    std::uint64_t guestRead64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address) {
        return mmioWindow(address)?Load64(rdram,context,address):read64(rdram,address);
    }
    // Generated quadword stores: the same rule (FIFO writes reach their device whether or not the
    // generator knew the address).
    void guestWrite128(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, const ProbeGpr& value) {
        if(mmioWindow(address))Store128(rdram,context,address,value);else write128(rdram,address,value);
    }
    void write64(std::uint8_t* rdram, std::uint32_t address, std::uint64_t value) {
        if (auto* spr = scratchpadAt(address, 8)) {std::memcpy(spr, &value, 8); return;}
        address = uncachedWideAddress(address, 8);
        checkMemory(rdram, address, 8);
        if (address % 8) throw std::runtime_error("Unsupported unaligned doubleword store");
        std::memcpy(memory_.data() + address, &value, 8);
        ++doubleword_writes;
    }
    std::uint8_t read8(std::uint8_t* rdram, std::uint32_t address) {
        if (auto* spr = scratchpadAt(address, 1)) return *spr;
        address = uncachedByteAddress(rdram, address);
        checkMemory(rdram, address, 1);
        return memory_[address];
    }
    std::uint64_t read64(std::uint8_t* rdram, std::uint32_t address) {
        if (auto* spr = scratchpadAt(address, 8)) {std::uint64_t value; std::memcpy(&value, spr, 8); return value;}
        address = uncachedWideAddress(address, 8); // E.g. bounded searches copying owned results.
        checkMemory(rdram, address, 8);
        if (address % 8) throw std::runtime_error("Unsupported unaligned doubleword read");
        std::uint64_t value;
        std::memcpy(&value, memory_.data() + address, 8);
        return value;
    }

    static constexpr std::size_t kernel_snapshot_size = 0x80004;
    void loadKernelSnapshot(std::span<const std::uint8_t> bytes) {
        if (bytes.size() != kernel_snapshot_size) throw std::runtime_error("Kernel snapshot must cover physical [0, 0x80004)");
        if (kernel_patch_end_) throw std::runtime_error("Cannot replace a kernel baseline with enabled patches");
        kernel_snapshot_.assign(bytes.begin(), bytes.end());
    }

    void enableStartupKernelPayload(std::span<const std::uint8_t> expected) {
        if (!kernel_patch_end_ || kernel_snapshot_.empty() || !startup_kernel_payload_.empty() || expected.size() != 0x330)
            throw std::runtime_error("Invalid startup kernel payload capability");
        if (kernel_patch_start_ < 0x80075330 && 0x80075000 < kernel_patch_end_)
            throw std::runtime_error("Startup kernel payload overlaps existing patch");
        startup_kernel_payload_.assign(expected.begin(), expected.end());
    }
    unsigned startup_kernel_word_writes = 0;

    void enableKernelPatch(std::uint32_t start, std::uint32_t size) {
        if (kernel_snapshot_.empty() || kernel_patch_end_ || !size || start % 4 || size % 4
                || start < 0x80000000 || start >= 0x80080000 || size > 0x80080000 - start)
            throw std::runtime_error("Invalid bounded kernel patch range");
        kernel_patch_start_ = start;
        kernel_patch_end_ = start + size;
    }
    std::span<const std::uint8_t> kernelBaseline() const { return kernel_snapshot_; }
    bool log_kernel_reads = false; // BT2_SELF_STARTUP: name each kernel baseline word the game reads (read32).

    // Single captured startup read, not a timer scheduler or mutable register model.
    void supplyOsdConfig(std::uint32_t word) {
        if (osd_config_supplied) throw std::runtime_error("Duplicate OSD checkpoint");
        osd_config_word = word;
        osd_config_supplied = true;
    }
    void supplyOsdReadback(std::uint32_t expected) {
        if (!osd_config_supplied || osd_readback_supplied || osd_config_reads || expected != osd_config_word
                || ((expected & 0xFFFF1FFF) | 0x2000) != expected)
            throw std::runtime_error("Unsupported OSD same-value readback checkpoint");
        osd_readback_supplied = true;
    }
    unsigned osd_config_reads = 0, osd_config_sets = 0;

    void supplyTimer3Mode(std::uint32_t value) {
        if (value > 0xFFFF || timer3_mode_supplied_) throw std::runtime_error("Invalid/repeated Timer 3 capture");
        timer3_mode_ = value;
        timer3_mode_supplied_ = true;
    }

    void supplyTimer2Readback(std::uint32_t expected) {
        if (expected > 0xFFFF || timer2_readback_supplied_) throw std::runtime_error("Invalid/repeated Timer 2 readback validation");
        timer2_readback_expected_ = expected;
        timer2_readback_supplied_ = true;
    }

    void supplyIntcMask(std::uint32_t value) {
        if (value > 0xFFFF || intc_mask_supplied_) throw std::runtime_error("Invalid/repeated INTC mask checkpoint");
        intc_mask = value;
        intc_mask_supplied_ = true;
    }

    void supplyTimer2Mode(std::uint32_t value) {
        if (value > 0xFFFF || timer2_mode_supplied_) throw std::runtime_error("Invalid/repeated Timer 2 capture");
        timer2_mode_ = value;
        timer2_mode_supplied_ = true;
    }

    // Reached CACHE 0x18 path only. This adapter has coherent main RAM and no cache state.
    // Validate a complete aligned line; this does not perform DMA or synchronize an IOP.
    void dataCacheLine(std::uint8_t* rdram, std::uint32_t address) {
        if (address >= 0x20000000 && address < 0x22000000) {
            if (sif_rpc_reads != 1) throw UnsupportedGuestMemory(address, 64);
            address -= 0x20000000;
        }
        if (address & 63) throw UnsupportedGuestMemory(address, 64);
        checkMemory(rdram, address, 64);
        ++data_cache_lines;
    }
    // Owned EE RAM is coherent: no delayed cache writes or valid logical lines.
    // This is the reached indexed two-way scan, not physical R5900 cache emulation.
    void indexedDataCache(std::uint8_t* rdram, R5900Context* ctx,
                          std::uint32_t operation, std::uint32_t address);
    std::uint32_t readCacheTagLo(R5900Context* ctx) {
        const auto way=indexed_cache_tag_reads&1u;
        if (!indexed_cache_tag_ready || (ctx->pc!=(way?0x32F508u:0x32F4CCu) && ctx->pc!=(way?0x32F648u:0x32F60Cu)))
            throw std::runtime_error("TagLo read without matching indexed load");
        indexed_cache_tag_ready=false;
        ++indexed_cache_tag_reads;
        return 0; // Explicit invalid tag; verified against both captured indexed ways.
    }
    unsigned indexed_cache_operations=0, indexed_cache_tag_reads=0;
    bool indexed_cache_tag_ready=false;
    unsigned data_cache_lines = 0;
    static constexpr std::size_t sif_iop_size = 2 * 1024 * 1024;
    void supplySifRpcState(std::uint32_t word) {
        if (sif_iop_.empty() || sif_rpc_supplied_ || word != 1)
            throw std::runtime_error("SIF RPC state checkpoint differs or was already supplied");
        sif_rpc_supplied_ = true;
    }
    unsigned sif_rpc_reads = 0;
    void supplySifDma(std::span<const std::uint8_t> before, std::uint32_t result);
    void supplySifBind(std::span<const std::uint8_t> bundle);
    void supplyCdInit(std::span<const std::uint8_t> bundle);
    void supplyScmdBind(std::span<const std::uint8_t> bundle);
    bool iop_reset_pending=false, prior_iop_services_valid=true;
    GifDmaStaging gif_dma;
    PostResetSif post_sif;
    PostResetCd post_cd;
    PostResetLoadfile post_loadfile;
    unsigned native_module_loads=0;
    std::string native_module_request,native_iop_error;
    unsigned native_iop_steps=0,native_iop_targets=0,native_iop_waiters=0;
    std::uint32_t native_iop_pc=0;
    unsigned heap_binds=0,heap_calls=0,heap_frees=0;
    bool sound_thread_start_supplied=false,sound_thread_checkpoint=false;
    unsigned sound_thread_queries=0,sound_thread_starts=0;
    void supplySoundThreadStart(std::span<const std::uint8_t> evidence) {
        const std::array<std::uint32_t,14> expected={0x70030C11,3,0x10,0x315380,0x3A5CC0,0x1000,0x38BDF0,5,5,0,0,0,0,0};
        if(sound_thread_start_supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("EE sound thread-start evidence differs/repeated");
        sound_thread_start_supplied=true;
    }
    unsigned mc_frame_queries=0; // Frame-phase GetInfo queries; kept out of the startup service accounting.
    // The memory card in slot 1 as a host folder (docs/plans/memory-card/MEMORY_CARD_PLAN.md); empty: no card.
    std::filesystem::path memory_card_folder;bool memory_card_reported_=false;
    unsigned sounds_output_mode=0,sounds_output_mode_requests=0; // SOUNDS request 0x60: the last mode asked for.
    unsigned sounds_bank_replacements=0; // SOUNDS request 0x80.
    std::optional<CardFolder> memory_card_;std::vector<CardFolder::Entry> memory_card_listing_;std::size_t memory_card_listed_=0;unsigned memory_card_file_requests=0;
    std::uint32_t memoryCardFreeClusters() const;
    unsigned mc_rpc_binds=0,mc_rpc_calls=0,mc_lock_waits=0,mc_lock_signals=0,mc_lock_polls=0;
    unsigned sound_rpc_binds=0,sound_rpc_calls=0,sounds_rpc_binds=0,sounds_rpc_calls=0,dbc_rpc_binds=0,dbc_rpc_calls=0,dbc_lock_waits=0,dbc_lock_signals=0;
    std::vector<std::string> ee_console_messages;
    unsigned dmac_startup_writes=0;
    std::uint32_t dmac_control=0,dmac_priority=0;
    VifStartup vif_startup;
    TextureGifStartup texture_gif;
    unsigned ee_heap_lock_waits=0,ee_heap_lock_signals=0,heap_end_queries=0;
    unsigned timer1_setup_writes=0;
    std::uint32_t timer1_mode=0;
    // GS privileged display registers (0x12000000 + 16*n) as last written; PMODE n=0 .. BGCOLOR n=14.
    std::array<std::uint64_t,15> gs_privileged{};unsigned gs_display_writes=0,gs_display_flips=0;
    // Frame-phase GIF DMA (channel 2), normal mode from main RAM; transfers complete synchronously
    // into the general GS path (FRAME_GIF_PLAN.md).
    struct FrameGif {std::uint32_t chcr=0,madr=0,qwc=0,tadr=0;unsigned starts=0,chain_starts=0;std::uint64_t bytes=0;} frame_gif;unsigned frame_syncs=0; // Frame GIF: normal and source-chain mode.
    // Frame-phase VIF1 DMA (channel 1), chain mode with tag transfer; tags are walked synchronously
    // and their VIF codes run by the VIF1 interpreter, which owns VU1 (VU1_PIPELINE_PLAN.md).
    Vif1 vif1;
    struct FrameVif1 {std::uint32_t chcr=0,madr=0,tadr=0,qwc=0;bool cis=false;unsigned sends=0,starts=0,tags=0,codes=0,directs=0;std::uint64_t direct_qwords=0;
        std::uint32_t blocked_code=0,blocked_tag=0,blocked_tadr=0;unsigned unsupported=0;} frame_vif1;
    struct FrameVif1Unsupported {std::uint32_t code,tag;};
    // GS readback (ee_gs_readback.cpp): FIFO and bus direction, the FINISH events already cleared in
    // CSR, and counters. gs_download_source supplies local->host pixels when the GPU renderer holds VRAM.
    struct GsReadback {bool fdr=false,busdir=false;unsigned finishes_cleared=0,downloads=0;std::uint64_t dma_qwords=0,fifo_qwords=0;} gs_readback;
    std::function<void(std::span<std::uint8_t>)> gs_download_source;
    bool gsFinishPending() const {return gif_dma.gs.state.finishes!=gs_readback.finishes_cleared;}
    void frameVif1Codes(std::span<const std::uint32_t> words);
    void frameVif1Send(std::uint32_t value);
    void frameVif1Receive(std::uint32_t value);
    void gsDownload(std::span<std::uint8_t> out);
    bool gsReadbackStore32(std::uint32_t address,std::uint32_t value);
    bool gsReadbackStore64(std::uint32_t address,std::uint64_t value);
    std::uint32_t vif1Status() const;
    ProbeGpr vif1FifoRead();
    void vif1FifoWrite(const ProbeGpr& value);
    void frameVif1Start(std::uint32_t value);void frameGifChainStart(std::uint32_t value);
    bool frameGifContext() const {return gs_display_flips>=1 && current_thread_id==1 && !in_interrupt_;}
    // Main running the game's own code past the replayed startup checkpoints, outside an interrupt.
    bool mainGameContext() const {return current_thread_id==1 && main_priority_admissions && !startup_scheduler_active && !in_interrupt_;}
    LogicalTimer1 timer1;unsigned timer1_frame_writes=0,timer1_starts=0,timer1_polls=0,timer1_wait_polls=0,timer1_stops=0,timer1_vblanks=0,sound_tick_boundaries=0;
    unsigned frame_limit=0;bool frame_budget_reached=false; // Optional probe frame budget (display flips).
    // Diagnostic float watch (probe: BT2_EE_WATCH_FLOAT=<hex address>:<limit>): the run stops at the first
    // guest call that finds the float at the address non-finite or above the limit in magnitude.
    std::uint32_t float_watch_address=0;float float_watch_limit=0;
    // Diagnostic call trace (probe: BT2_EE_CALL_TRACE): called before each guest call or jump the
    // dispatcher sees, with the source and target addresses. Never changes guest state.
    std::function<void(const R5900Context&,std::uint32_t,std::uint32_t)> on_guest_call;
    // Diagnostic store watch (probe: BT2_EE_WATCH_QUADWORD): called before each 128-bit store to the address.
    std::uint32_t quadword_watch_address=0;std::function<void(const ProbeGpr&)> on_watched_quadword;
    const R5900Context* mainContext() const {return main_context_;}
    std::uint32_t lastCallSource() const {return last_branch_source_;}
    std::uint32_t lastCallTarget() const {return last_branch_target_;}
    unsigned flipless_field_limit=3600; // Fields without a display flip before the run stops (probe: BT2_STALL_FIELDS).
    std::function<void()> on_display_flip; // Host presentation hook; never changes guest state.
    // Each VIF1 transfer's words before it runs (diagnostics): a copy, never guest memory; a change to it
    // changes only what VU1 and the GS receive.
    std::function<void(std::span<std::uint32_t>)> on_vif1_transfer;
    bool vif1_running=false; // While a VIF1 transfer runs: what reaches the GS sink is VU1's and VIF1's output.
    std::function<void()> on_field_end; // At each VBLANK, before anything of the next field runs (state hash); never changes guest state.
    std::function<PadInput()> on_pad_input; // Live controller input, asked once per field; unset for scripted runs.
    // Online play: with a second pad on (pad_second), asked once per field with the local controller; fills both players' controllers (it may
    // wait inside for the other side's). False: no exchange now (the local controller is player 1's, player 2's is neutral).
    std::function<bool(const PadInput& local,PadInput& player1,PadInput& player2)> on_pad_exchange;
    std::atomic<bool> pad_second{false};
    std::function<void(unsigned field,std::uint16_t pressed)> on_pad_change; // Live input whose buttons changed, with the pad script's field number.
    unsigned graph_startup_phase=0;
    // Main's display re-initialization after the first archive load. The startup clear routine
    // (0x100624 -> 0x100258) runs again: 0 CSR reset, 1 CSR read, 2 IMR, 3 VSync handler removed,
    // 4 CRT(1,2,1), 5 VSync wait, 6 two GIF requests. 7 field sample (0x271178). Display setup
    // (0x100650 -> 0x271078): 8 CSR reset, 9 CSR read, 10 IMR, 11 CRT(1,2,0), 12 handler re-added.
    bool sounds_hardware_timers_supported=true; // IOP_HARDWARE_TIMER_PLAN.md
    bool sound_voice_transfers_supported=true; // Owned SPU2 voice transfers (IOP_SPU2_TRANSFER_PLAN.md).
    unsigned fileio_binds=0,fileio_calls=0,fileio_completions=0;
    bool fileio_workers_supported=true;
    // The SOUNDS hardware timer's worker reads SPU2 voice state (ENDX) on its ticks; until that is
    // modelled (SPU2_VOICE_PLAN.md), a VBlank that would deliver timer ticks is a checked boundary.
    bool sounds_timer_ticks_supported=false;
    unsigned display_reset_phase=0,polled_vblanks=0,timer2_deliveries=0,file_load_idles=0,iop_uploads=0,iop_upload_polls=0;
    unsigned delivering_cause_=0;
    LogicalTimer2 timer2;
    bool timer2Context(const R5900Context* context) const {
        // Main after admission, or the cause-11 handler during a logical delivery.
        return timer2.started() && ((in_interrupt_ && delivering_cause_==11)
            || (!in_interrupt_ && main_priority_admissions && current_thread_id==1 && !startup_scheduler_active)) && context;
    }
    std::uint32_t vsync_flag=0,vsync_csr=0;unsigned frame_vsync_deliveries=0;
    std::uint32_t display_intc_stat=0;
    bool display_field=true; // Toggles at every VBlank; anchored to the original 0x271178 sample (field 1).
    std::optional<GifDmaStaging> clear_repeat_gif;
    // Frame-phase GS reset (the GS library reset routine 0x30FDC0, mode 0, called again after display
    // setup for a new scene): CSR reset, CSR read, PutIMR, VSync handler removal, SetGsCrt. Separate
    // from the startup sequence above; each step is checked by its program counter.
    unsigned frame_gs_reset_step=0,frame_gs_resets=0;bool frame_vsync_handler_removed=false;
    std::array<std::uint32_t,3> frame_gs_crt_request{};std::uint32_t frame_vsync_handler=0;
    bool frameGsResetStep(const R5900Context* context,unsigned step,std::uint32_t pc) const {
        return frame_gs_reset_step==step && display_reset_phase==13 && gs_display_flips>=1 && current_thread_id==1
            && !in_interrupt_ && context->pc==pc;
    }
    bool displayResetStep(const R5900Context* context,unsigned phase,std::uint32_t pc,std::uint32_t frame) const {
        return display_reset_phase==phase && main_priority_admissions && current_thread_id==1 && !startup_scheduler_active
            && !in_interrupt_ && graph_startup_phase==4 && gs_csr_reset_writes==1 && cri_rpc_calls==42
            && context->pc==pc && context->r[29].low==initial_sp-frame;
    }
    // Main's second embedded-buffer load (0x100634 -> 0x320580), between the field sample and display setup.
    bool embeddedReload() const {
        return display_reset_phase==8 && main_priority_admissions && current_thread_id==1 && !startup_scheduler_active
            && !in_interrupt_ && cri_rpc_calls==42 && iop_epoch==1;
    }
    bool displayVsyncWait(const R5900Context* context,std::uint32_t pc,std::uint32_t offset) const {
        // SyncV without a callback polls INTC_STAT directly while cause 2 is disabled.
        return !(intc_mask&4) && (displayResetStep(context,5,pc,656-offset) || displayResetStep(context,7,pc,96-offset));
    }
    std::uint64_t graph_halfword_checkpoint=0;
    bool heap_allocation_supplied=false,heap_copy_supplied=false;
    unsigned heap_transfers=0,heap_dma_polls=0;
    void supplyHeapAllocation(std::span<const std::uint8_t> evidence) {
        constexpr std::array<std::uint32_t,17> expected={0x400,0x3AC880,0x80000008,0,0,0,0,0x3AC840,0x8000000A,0,0,0,0,0,0,0,0xCE500};
        if(heap_allocation_supplied || (evidence.size()!=sizeof(expected) && evidence.size()!=sizeof(expected)+20) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("IOP heap allocation evidence differs or repeated");
        if(evidence.size()==sizeof(expected)+20) {
            constexpr std::array<std::uint32_t,5> copy={0x369F90,0xCE500,0x360,0,0xFFFFFFFF};
            if(std::memcmp(evidence.data()+sizeof(expected),copy.data(),sizeof(copy)))throw std::runtime_error("Heap copy evidence differs");
            heap_copy_supplied=true;
        }
        heap_allocation_supplied=true;
    }
    bool heap_bind_supplied=false;
    void supplyHeapBind(std::span<const std::uint8_t> evidence) {
        constexpr std::array<std::uint32_t,16> expected={0,0,0x80000008,0,0,0x203A97C0,0,0x3AC840,0x80000009,0x48438,0x48480,0,0,0,0,0};
        if(heap_bind_supplied || evidence.size()!=sizeof(expected) || std::memcmp(evidence.data(),expected.data(),sizeof(expected)))
            throw std::runtime_error("IOP heap bind evidence differs or repeated");
        heap_bind_supplied=true;
    }
    unsigned serviceOperations() const {return cri_rpc_calls+cri_rpc_binds+ncmd_ready_calls+ncmd_rpc_binds+disc_rpc_calls+disc_rpc_binds+mc_rpc_binds+mc_rpc_calls+ post_cd.operations()+post_loadfile.operations()+native_module_loads+heap_binds+heap_calls+heap_frees+sound_rpc_binds+sound_rpc_calls+sounds_rpc_binds+sounds_rpc_calls+dbc_rpc_binds+dbc_rpc_calls;}
    IntcCheckpoint gs_interrupt;
    unsigned gs_field_reads=0;
    // The GS library's reset path (ee_gs_privileged.cpp). The four counters mark the first reset's steps
    // (the startup checks read them); gs_resets counts every CSR reset.
    unsigned gs_csr_reset_writes=0,gs_csr_reads=0,gs_imr_writes=0,gs_crt_calls=0,gs_resets=0;
    std::uint32_t gs_imr=0xFF00; // Every GS interrupt masked, as the first PutIMR's captured return shows.
    bool gs_csr_cleared=false; // After a CSR reset until the next VBlank: the status bits and FIELD read as clear.
    void gsCsrReset();
    std::uint64_t gsCsrRead();
    std::uint32_t gsPutImr(std::uint64_t mask,std::uint32_t pc);
    void gsSetCrt(std::uint64_t interlace,std::uint64_t mode,std::uint64_t field_mode,std::uint32_t pc);
    std::array<std::uint32_t,3> gs_crt_request{};
    unsigned iop_epoch=0, reset_clear_writes=0, reset_ready_checkpoint_reads=0;
    std::uint32_t reset_rpcinit=1, reset_subaddr=0x19600;
    unsigned sif_stop_calls = 0, sif_reset_subaddr_reads = 0, reset_smflag_writes = 0;
    std::uint32_t sif_smflag = 0;
    std::uint32_t sif2_chcr = 0, sif2_qwc = 0;
    unsigned scmd_bind_calls = 0, scmd_mmode_calls = 0;
    unsigned semaphore_polls = 0;
    unsigned cd_init_calls = 0, cd_init_context_preservations = 0, cd_semaphores_created = 0;
    unsigned sif_bind_callbacks = 0;
    unsigned sif_bind_context_preservations = 0;
    unsigned semaphore_deletions = 0;
    unsigned rpc_call_semaphores = 0;
    std::uint32_t rpc_call_sema_attributes = 0;
    std::span<const std::uint8_t> sifIopMemory() const { return sif_iop_; }
    unsigned sif_dma_submissions = 0;
    std::uint32_t sif_ee_receive_address = 0;
    void supplySifCommand(std::uint32_t word) {
        if (!dma5_control_supplied_ || sif_command_supplied_ || word != 0x19600)
            throw std::runtime_error("SIF checkpoint differs or was already supplied");
        sif_command_word_ = word;
        sif_command_supplied_ = true;
    }
    unsigned sif_command_reads = 0;
    void supplyDmaStatus(std::uint32_t word) {
        if (dma_status_supplied_ || word != 0x00800040)
            throw std::runtime_error("Unsupported/repeated DMA status checkpoint");
        dma_status_word_ = word;
        dmac_mask = word & 0xFFFF0000;
        dma_status_supplied_ = true;
    }
    void supplyDma5Control(std::uint32_t word) {
        if (!dma_status_supplied_ || dma5_control_supplied_ || word != 0x184)
            throw std::runtime_error("Unsupported/repeated DMA 5 control checkpoint");
        dma5_control_word_ = word;
        dma5_control_supplied_ = true;
    }
    unsigned dma_status_reads = 0, dma5_control_reads = 0;
    struct DmacHandler { std::uint32_t channel, address, arg, gp; };
    std::vector<DmacHandler> dmac_handlers;
    std::vector<std::uint32_t> dmac_handler_order;
    std::uint32_t dmac_mask = 0, dmac_enable_calls = 0, dmac_disable_calls = 0, dmac_removals = 0;

    // Frame phase: IPU setup registers (ipu_device.h) and the DMAC master enable, idle (reset value
    // 0x1201; D_ENABLEW mirrors into D_ENABLER, and only the suspend bit CPND may change).
    IpuDevice ipu;std::uint32_t dmac_enabler=0x1201;unsigned dmac_suspends=0;
    StoppedDmaChannels scratchpad_dma{0x1000D000,0x1000D400}; // fromSPR (8), toSPR (9).
    unsigned scratchpad_transfers=0;
    void scratchpadTransfer(std::uint32_t address,std::uint32_t value);
    bool ipuContext() const {return gs_display_flips>=1 && !in_interrupt_;}
    bool ipuRead(std::uint32_t address,std::uint32_t& value);
    bool ipuWrite(std::uint32_t address,std::uint32_t value);
    // Emitted slow-path memory ABI; unsupported MMIO still fails explicitly.
    void Store128(std::uint8_t* rdram,R5900Context* context,std::uint32_t address,const ProbeGpr& value) {
        if(address==0x10005000 && context->r[29].low==initial_sp-32
            && ((vif_startup.phase==3 && context->pc==0x30FFB4) || (vif_startup.phase==4 && context->pc==0x30FFBC))) {
            const auto expected=read128(rdram,0x3680A0+(vif_startup.phase-3)*16);
            if(value.low!=expected.low || value.high!=expected.high)throw UnsupportedGuestMemory(address,16);
            std::array<std::uint32_t,4> words;std::memcpy(words.data(),&value,16);
            vif_startup.consume(words);++vif_startup.phase;return;
        }
        if(address==0x10005000 && frameGifContext()) {checkMemory(rdram,0,0);vif1FifoWrite(value);return;}
        if(address==0x10007010 && ipuContext()) {
            std::array<std::uint32_t,4> words;std::memcpy(words.data(),&value,16);
            try {ipu.writeFifo(address,words);} catch(const std::runtime_error& e) {std::cerr<<"IPU refused: "<<e.what()<<std::endl;throw UnsupportedGuestMemory(address,16);}
            return;
        }
        write128(rdram,address,value); // Other FIFO/MMIO paths still reject through checked RAM bounds.
    }
    void writeVuFbrst(R5900Context* context,std::uint32_t value) {
        if(context->pc!=0x30FF90 || context->r[29].low!=initial_sp-32 || vif_startup.phase!=2 || value!=0x200)
            throw std::runtime_error("Unsupported VU control write");
        ++vif_startup.vu_resets;vif_startup.phase=3;context->vu0_fbrst=value&0xC0C;
    }
    void handleBreak(std::uint8_t*,R5900Context*) {
        throw std::runtime_error("Unsupported guest BREAK instruction");
    }
    void Store64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint64_t value);
    std::uint64_t Load64(std::uint8_t* rdram, R5900Context* context, std::uint32_t address);
    void Store32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address, std::uint32_t value);

    std::uint32_t Load32(std::uint8_t* rdram, R5900Context* context, std::uint32_t address);

    std::uint32_t read32(std::uint8_t* rdram, std::uint32_t address);

    bool eeCheckpointDue() {
        // Runaway guard. One observed null-bind retry executes a 2^20+1-iteration guest loop. The guard
        // applies per frame and per VBLANK field (each restarts the count): run length is bounded by the
        // frame limit and the flipless-field limit (deliverVblank), and a loop that never reaches the
        // next field still stops. A worker thread above main runs to its next wait within one field
        // (logical time does not advance for it), so the frame-phase bound has to hold a whole burst of
        // loading work: battle loading copies and unpacks for tens of millions of iterations in one go.
        if (gs_display_flips != checkpoint_flips_ || vblank_deliveries != checkpoint_fields_)
            {checkpoint_flips_ = gs_display_flips; checkpoint_fields_ = vblank_deliveries; checkpoints_ = 0;}
        if (++checkpoints_ > (checkpoint_limit ? checkpoint_limit : gs_display_flips ? frame_checkpoint_limit : post_cd.supplied ? 2000000u : 1000000u))
            throw std::runtime_error("Bootstrap execution budget exceeded; thread "+std::to_string(current_thread_id)
                +", frame "+std::to_string(gs_display_flips)+", last branch "+std::to_string(last_branch_source_)+" -> "+std::to_string(last_branch_target_));
        if (current_thread_id == 1 && ++main_checkpoints_since_field_ > main_checkpoints_per_field) busyMainField();
        return false; // Run to a checked boundary, rather than pretending to schedule the EE.
    }

    std::uint32_t expected_return = 0;
    std::uint32_t syscall_resume_pc = 0;
    bool thread_initialized = false, heap_initialized = false;
    std::uint32_t heap_start = 0, heap_end = 0;
    std::uint32_t stack_base = 0, initial_sp = 0;
    std::array<std::uint32_t, 5> thread_arguments{};
    struct DormantThread {
        std::uint32_t id, entry, stack_base, stack_size, gp, initial_priority, heap_base, context_address, initial_sp;
        bool ready=false;R5900Context context{};std::uint32_t current_priority=0;bool suspended=false;
        bool waiting=false;unsigned wakeups=0; // Waiting means SleepThread; other wait kinds are unmodeled.
        std::uint32_t attr=0,option=0;
        bool exited=false,deleted=false; // Exited: its stack is released by the scheduler; deleted: the id is free.
    };
    std::vector<DormantThread> threads;
    void releaseThread(DormantThread&); // ee_thread_scheduler.cpp
    std::uint32_t current_thread_id = 0, main_thread_priority = 0;
    unsigned priority_changes = 0,main_thread_queries=0;
    unsigned startup_worker_queries=0,startup_worker_starts=0,startup_worker_priority_changes=0;
    unsigned startup_ready_queries=0,startup_worker_suspends=0;
    unsigned disc_bind_semaphores=0,disc_rpc_binds=0,disc_rpc_calls=0;
    std::string local_disc_path;
    unsigned cri_rpc_binds=0,cri_rpc_calls=0;
    unsigned ncmd_bind_semaphores=0,ncmd_rpc_binds=0,ncmd_ready_calls=0,ncmd_read_calls=0;
    bool ncmd_bind_supplied=false;
    void supplyNcmdBind(std::span<const std::uint8_t> b) {
        const std::array<unsigned,16> expected={0,0,0x80000008,0,0,0x203A97C0,0,0x369590,0x80000009,0x43418,0x43DB8,0,0,0,0,0};
        if(ncmd_bind_supplied || b.size()!=sizeof(expected) || std::memcmp(b.data(),expected.data(),sizeof(expected)))throw std::runtime_error("Invalid command bind evidence");
        ncmd_bind_supplied=true;
    }
    bool disc_bind_supplied=false;
    void supplyDiscBind(std::span<const std::uint8_t> bundle) {
        const std::array<std::uint32_t,16> reply={0,0,0x80000008,0,0,0x203A97C0,0,0x3A8780,0x80000009,0x433D0,0x441C8,0,0,0,0,0};
        if(disc_bind_supplied || bundle.size()!=sizeof(reply) || std::memcmp(bundle.data(),reply.data(),sizeof(reply)))
            throw std::runtime_error("Invalid disc-search bind evidence");
        disc_bind_supplied=true;
    }
    struct Semaphore { std::uint32_t count, maximum, option; bool deleted = false; };
    // Bounded bootstrap objects; no wait queues or scheduling implementation.
    std::vector<Semaphore> semaphores;
    std::uint32_t semaphore_waits = 0, semaphore_signals = 0;
    std::array<std::uint32_t, 256> syscall_handlers{};
    unsigned syscall_registrations = 0, custom_syscall_calls = 0;
    struct IntcHandler { std::uint32_t cause, address, arg, gp; };
    // Shared 128-entry pool, IDs start at one, matching pinned Play!.
    std::vector<IntcHandler> intc_handlers;
    std::vector<std::uint32_t> intc_handler_order;
    std::uint32_t timer3_interrupt_handler = 0;
    bool timer3_interrupt_enable_requested = false;
    unsigned extended_registrations = 0;
    std::uint32_t intc_mask = 0;
    unsigned intc_enable_calls = 0;
    bool startup_zero_verified = false;
    std::uint64_t startup_quadword_writes = 0, startup_byte_writes = 0;
    std::uint64_t kernel_word_writes = 0;
    unsigned timer3_mode_reads = 0, timer2_mode_reads = 0, timer2_mode_writes = 0;
    std::uint32_t timer2_mode_after_write = 0;
    unsigned cache_requests = 0, instruction_cache_invalidations = 0;
    unsigned kernel_dispatch_validations = 0;
    unsigned returns = 0, guest_calls = 0;
    // Distinct registered entry addresses; completion means a normal invocation return.
    std::set<std::uint32_t> entered_functions,completed_functions;
    // The targets last added to the two sets, by address slot: a call to a function already recorded skips the set.
    // (16384 slots: with 4096 the main program's and the overlay's functions shared slots, and each such call searched
    // the set again: about 1% of the game thread in a fight on Android, measured)
    std::array<std::uint32_t,16384> recently_entered_{},recently_completed_{};
    void noteEntered(std::uint32_t target) {auto& slot=recently_entered_[(target>>2)&16383];if(slot!=target){slot=target;entered_functions.insert(target);}}
    void noteCompleted(std::uint32_t target) {auto& slot=recently_completed_[(target>>2)&16383];if(slot!=target){slot=target;completed_functions.insert(target);}}
    std::uint64_t halfword_writes=0;
    std::uint64_t doubleword_writes = 0, word_writes = 0;
    std::uint64_t byte_writes = 0, quadword_writes = 0;

    // The C library per-call stdio semaphore names (options) reached by main.
    static bool stdioCallSema(std::uint32_t option) {
        return option==0x3839A0 || option==0x3839B8 || option==0x3839D0 || option==0x3839E8;
    }
    std::uint64_t overlay_dispatch_validations=0;
    std::uint64_t interior_entry_dispatches=0; // Dispatches that entered a function at an interior entry.
    bool interior_entries_disabled=false; // BT2_NO_INTERIOR_ENTRIES: dispatch knows function starts only (diagnostic comparison).
private:
    void receiveFirstRpcBind(std::uint8_t* rdram, R5900Context* context);
    // An IOP->EE SIF command handler call: packet header at the EE command buffer (0x3A94C0),
    // run on a CPU copy below the interrupted stack, as RPC_END is.
    void deliverEeCommand(std::uint8_t* rdram,R5900Context* context,std::uint32_t handler,std::uint32_t argument);
    void deliverRpcEnd(std::uint8_t* rdram, R5900Context* context,
                       std::uint32_t client = 0x3A87A8, unsigned semaphore = 3,
                       std::uint32_t server = 0x3F570, std::uint32_t buffer = 0x3FB20);
    // Native SIF RPC services (runtime/services/). A server's handles are the opaque IOP addresses
    // the EE stores at bind time; calls route by the handle in the client (client+36).
    struct NativeServer {
        std::uint32_t id,server,buffer,bind_token;bool native_bind;
    };
    static constexpr std::uint32_t sif_receive_buffer=0x3A94C0; // EE SIF command receive buffer.
    std::array<NativeServer,14> native_servers_{{
        {0x80000597,0x433D0,0x441C8,0x740001,true},  // CDVD file search
        {0x80000595,0x43418,0x43DB8,0x760001,true},  // CDVD N-commands
        {0x80000593,0x434F0,0x439A8,0,false},        // CDVD S-commands (bound with CD init)
        {0x80000001,0x47788,0x477D0,0x70001,true}, // FILEIO file server
        {0x80000400,0x536F8,0x532B0,0x70001,true},   // MCSERV memory card
        {0x80001300,0x62DE0,0x62F08,0,true},          // DBCMAN main
        {0x8000131C,0x62E28,0x62F98,0,true},          // DBCMAN (bound, unused)
        {0x8000131E,0x62E70,0x65028,0,true},          // DBCMAN port 1
        {0x8000131F,0x62EB4,0x650B8,0,true},          // DBCMAN port 0
        {0x80000003,0x48438,0x48480,0x40001,true},    // IOP heap
        {0x80000006,0x22568,0x225B0,0,false},         // LOADFILE (bound with the captured replay)
        {0x80000701,0xA15E0,0xA1C30,0x60001,true},    // SDRDRV sound driver
        {0x02000004,0x1E5D90,0x1D16E0,0x60003,true},  // SOUNDS
        {0x90000200,0x1B5D30,0x1B57C0,0x780001,true}, // CRI ADX
    }};
    const NativeServer* nativeServerById(std::uint32_t id) const;
    const NativeServer* nativeServerByHandle(std::uint32_t handle) const;
    bool decodeSifRpc(std::uint8_t* rdram,const R5900Context* context,SifRpcRequest& out);
    void completeNativeRpc(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server,
                           const std::array<std::uint32_t,16>& reply);
    void completeNativeCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    unsigned& nativeBindCounter(const NativeServer& server);
    bool callNativeService(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool nativeDiscCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool nativeFileioCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    void finishNativeFileio(R5900Context* context);
    bool nativeCardCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool nativePadCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    NativePadState& nativePadState();
    std::shared_ptr<NativePadState> native_pad_;
    bool nativeHeapCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool handleNativeRawDma(std::uint8_t* rdram,R5900Context* context);
    bool nativeModuleCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool nativeSoundDriverCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool nativeSoundsCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool nativeCriCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server);
    bool nativeCriStreamUpload(std::uint8_t* rdram,R5900Context* context,std::uint32_t source,std::uint32_t destination,std::uint32_t size);
    std::vector<std::uint8_t> native_iop_ram_=std::vector<std::uint8_t>(2u*1024*1024); // Uploaded IOP data (no IOP code runs).
    IopHeap native_heap_;
    std::map<std::uint32_t,unsigned EeRuntime::*> native_dma_tokens_;                 // Transfer token -> poll counter.
    std::vector<NativeCriChannel> native_cri_channels_;
    unsigned native_cri_422s_=0,native_cri_400s_=0;
    bool native_sounds_started_=false;
    unsigned native_sounds_banks_=0; // Sound banks loaded by SOUNDS 0x20 and not unloaded by 0x30 (status word 5).
    std::uint32_t native_cri_players_=0; // Live 0x408 player slots (bit n); a new player takes the lowest free one.
    std::map<std::uint32_t,NativeFile> native_files_;std::uint32_t native_next_fd_=1;
    std::array<std::uint32_t,2> native_fileio_buffers_{};std::uint32_t native_fileio_next_buffer_=0;
public:
    unsigned native_disc_requests=0,native_fileio_requests=0,native_card_requests=0,native_pad_requests=0,native_pad_transfers=0,native_other_binds=0;
    unsigned native_cri_requests=0,native_cri_returns=0,native_cri_commands=0;
    CriAudioFlow native_cri_audio; // What the IOP player reports back while a stream plays.
    // Sound output (docs/plans/sound/SOUND_OUTPUT_PLAN.md): with a receiver set, the stream PCM the
    // player consumes is mixed and handed over as 48 kHz stereo, one field per VBLANK.
    // Evidence for the sound effects path (BT2_SOUND_LOG): every SOUNDS and sound driver request with
    // its command bytes, and the two IOP blocks of each loaded bank. `number` is the RPC function.
    std::function<void(std::string_view kind,std::uint32_t number,std::span<const std::uint8_t> data)> on_sound_evidence;
    AudioMixer audio_mixer;
    SoundSampler sound_sampler; // Effects: banks, sound memory and voices behind SOUNDS and the sound driver.
    std::function<void(std::span<const std::int16_t>)> on_audio_field;
    std::uint64_t audio_fields=0,audio_stream_bytes=0,audio_single_stream_bytes=0; // `single`: of one-channel streams.
    void nativePadVblank(std::uint8_t* rdram);
    void nativeCriVblank(const R5900Context* interrupted);
    bool handleNativeDmaStat(R5900Context* context);
    const std::vector<std::uint8_t>& nativeIopMemory() const {return native_iop_ram_;}
    bool handleNativeSifDma(std::uint8_t* rdram,R5900Context* context);
private:
    void receiveFreshLoadfile(std::uint8_t* rdram,R5900Context* context);
    void receiveFreshScmd(std::uint8_t* rdram,R5900Context* context);
    void receiveFreshCd(std::uint8_t* rdram,R5900Context* context);
    void receiveFirstCdInit(std::uint8_t* rdram, R5900Context* context);
    void receiveScmdBind(std::uint8_t* rdram, R5900Context* context);
    void receiveScmdMmode(std::uint8_t* rdram,R5900Context* context);
    bool gif_dma_supplied_=false;
    bool gs_interrupt_supplied_=false,gs_field_supplied_=false;
    bool reset_submission_supplied_=false, reset_ready_supplied_=false;
    std::array<std::uint8_t,104> reset_request_{};
    bool sif_stop_supplied_ = false, sif_reset_subaddr_supplied_ = false, reset_smflag_supplied_ = false;
    bool scmd_mmode_supplied_ = false;
    std::array<std::uint8_t,64> scmd_mmode_reply_{};
    bool scmd_bind_supplied_ = false;
    std::array<std::uint8_t,64> scmd_reply_{};
    bool cd_init_supplied_ = false, cd_semaphores_supplied_ = false;
    std::array<std::uint8_t, 88> cd_init_bundle_{};
    bool sif_call_sema_supplied_ = false;
    std::vector<std::uint8_t> sif_bind_iop_;
    std::array<std::uint8_t, 64> sif_bind_reply_{};

    std::vector<std::uint8_t> sif_iop_;
    std::uint32_t sif_dma_result_ = 0;
    bool sif_rpc_supplied_ = false;
    bool rpc_end_callback_active_ = false;
    bool sif_command_supplied_ = false;
    std::uint32_t sif_command_word_ = 0;
    bool dma_status_supplied_ = false, dma5_control_supplied_ = false;
    std::uint32_t dma_status_word_ = 0, dma5_control_word_ = 0;
    bool osd_config_supplied = false, osd_readback_supplied = false;
    std::uint32_t osd_config_word = 0;
    std::uint64_t addIntcHandler(std::uint32_t cause, std::uint32_t address, std::uint32_t arg, std::uint32_t gp) {
        if (intc_handlers.size() == 128) return UINT64_MAX;
        intc_handlers.push_back({cause, address, arg, gp});
        const auto id = static_cast<std::uint32_t>(intc_handlers.size());
        intc_handler_order.insert(intc_handler_order.begin(), id);
        return id;
    }
    std::uint32_t startup_zero_start_ = 0, startup_zero_end_ = 0;
    void verifyKernelBinding(std::uint32_t address);
    std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> kernel_code_bindings_;
    std::unordered_map<std::uint32_t, std::vector<std::uint8_t>> overlay_code_bindings_;
    // An aligned word of main RAM, or null for every other address (devices, scratchpad, kernel
    // views, unaligned or out-of-range accesses), which take the checked paths.
    std::uint8_t* ramWord(std::uint8_t* rdram,std::uint32_t address) {
        return address<=ram_size-4 && !(address&3) && rdram==memory_.data() && memory_.size()==ram_size?rdram+address:nullptr;
    }
    void checkMemory(std::uint8_t* rdram, std::uint32_t address, std::size_t count) const {
        if (memory_.empty() || rdram != memory_.data() || address > memory_.size() || count > memory_.size() - address)
            throw UnsupportedGuestMemory(address, count);
    }
    std::unordered_map<std::uint32_t, RecompiledFunction> functions_;
    struct InteriorEntry {RecompiledFunction function; std::uint32_t owner;};
    std::unordered_map<std::uint32_t, InteriorEntry> interior_entries_;
    std::map<std::uint32_t, std::uint32_t> function_extents_;
    std::unordered_map<std::uint32_t, const EeInterpreterHandler*> interpreter_handlers_;
    bool registered(std::uint32_t address) const {return functions_.contains(address) || interior_entries_.contains(address);}
    bool functionExtent(std::uint32_t address, std::uint32_t& start, std::uint32_t& end) const;
    void runInterpretedInstruction(std::uint8_t* rdram, R5900Context* context, std::uint32_t pc, std::uint32_t word, bool& vu_flags_reported);
    bool interpretControl(std::uint8_t* rdram, R5900Context* context, std::uint32_t pc, std::uint32_t word,
                          std::uint32_t start, std::uint32_t end, bool& vu_flags_reported);
    std::vector<std::uint8_t> memory_, kernel_snapshot_, startup_kernel_payload_;
    std::set<std::uint32_t> kernel_reads_logged_;
    std::vector<std::uint8_t> scratchpad_ = std::vector<std::uint8_t>(scratchpad_size);
    std::unordered_map<std::uint32_t, std::uint32_t> kernel_patch_words_;
    std::uint32_t kernel_patch_start_ = 0, kernel_patch_end_ = 0;
    std::vector<std::uint32_t> call_returns_;
    std::vector<std::uint32_t> vif1_list_words_;std::vector<std::pair<std::size_t,std::uint32_t>> vif1_list_tags_; // frameVif1Start's working buffers.
    unsigned checkpoints_ = 0, checkpoint_flips_ = 0, checkpoint_fields_ = 0, dispatch_depth_ = 0;
    unsigned flipless_fields_ = 0, flipless_flips_ = 0; // Fields delivered since the last display flip.
    unsigned field_flips_ = 0, field_writes_ = 0;       // Flip and display-write counts at the last VBLANK.
    void countDisplayFlip();
    std::uint32_t last_branch_source_ = 0, last_branch_target_ = 0;
    bool timer3_mode_supplied_ = false;
    std::uint32_t timer3_mode_ = 0, timer2_mode_ = 0;
    bool timer2_mode_supplied_ = false, intc_mask_supplied_ = false, timer2_readback_supplied_ = false;
    std::uint32_t timer2_readback_expected_ = 0;
    friend struct EeExecutionOwner;
    // Destroy before the invocation/memory members: cancellation needs both alive.
    std::shared_ptr<EeExecutionOwner> sound_execution_;
    std::vector<std::shared_ptr<EeExecutionOwner>> worker_executions_;

};
