#include <cstdio>
#include "ee_runtime.h"
#include "ps2_runtime_macros.h"

// EE interpreter bridge (WHOLE_GAME_COVERAGE_PLAN.md, step 3). Code that no generated function holds runs
// here instead of stopping the run. Branches and jumps are decoded here, with the rules the generated code
// follows; every other instruction runs through the handler the recompiler made from that instruction word
// (ee_interpreter_handlers.h), so it means the same as in compiled code. Calls go through dispatch, which
// runs compiled code where it exists. What runs here is logged (interpreted_entries) so that the next build
// maps it and runs it compiled.

void EeRuntime::interpretEntry(std::uint8_t* rdram, R5900Context* context, PS2Runtime* runtime) {
    static_cast<EeRuntime*>(runtime)->interpret(rdram, context);
}

namespace {
[[noreturn]] void stop(const char* what, std::uint32_t word, std::uint32_t pc) {
    char text[200];
    std::snprintf(text, sizeof text, "EE interpreter: %s 0x%08X at 0x%08X", what, word, pc);
    throw std::runtime_error(text);
}

bool isControl(std::uint32_t word) {
    const std::uint32_t op = word >> 26, rs = (word >> 21) & 31, rt = (word >> 16) & 31, fn = word & 63;
    return (op == 0 && (fn == 8 || fn == 9)) || (op >= 2 && op <= 7) || (op >= 20 && op <= 23)
        || (op == 1 && (rt <= 3 || (rt >= 16 && rt <= 19))) || ((op == 16 || op == 17 || op == 18) && rs == 8);
}
}

void EeRuntime::setInterpreterHandlers(std::span<const EeInterpreterHandlerPart> parts) {
    interpreter_handlers_.clear();
    for (const auto& part : parts)
        for (unsigned n = 0; n < part.count; ++n)
            if (!part.handlers[n].run || !interpreter_handlers_.emplace(part.handlers[n].word, &part.handlers[n]).second)
                throw std::runtime_error("Invalid or duplicate interpreter handler");
}

bool EeRuntime::functionExtent(std::uint32_t address, std::uint32_t& start, std::uint32_t& end) const {
    auto found = function_extents_.upper_bound(address);
    if (found == function_extents_.begin()) return false;
    --found;
    if (address >= found->second) return false;
    start = found->first; end = found->second;
    return true;
}

void EeRuntime::runInterpretedInstruction(std::uint8_t* rdram, R5900Context* context, std::uint32_t pc, std::uint32_t word, bool& vu_flags_reported) {
    const auto found = interpreter_handlers_.find(word);
    if (found == interpreter_handlers_.end())
        stop("no handler (the word is not in the mapped code; map it and rerun generate_interpreter.py) for the instruction", word, pc);
    const auto flags = found->second->flags;
    // Between a write of the VU0 status flag and its next read, flag-setting VU0 instructions report
    // their effects first, as prepare_runtime.route_cop2_condition makes the compiled code do.
    if (vu_flags_reported && (flags & EE_HANDLER_UNKNOWN_VU)) stop("VU0 instruction of unknown flag effect between a status flag write and read:", word, pc);
    if (vu_flags_reported && (flags & EE_HANDLER_SETS_VU_FLAGS)) vu0UpdateFlags(context, word);
    found->second->run(rdram, context, this, pc);
    if (flags & EE_HANDLER_WRITES_VU_STATUS) vu_flags_reported = true;
    else if (flags & EE_HANDLER_READS_VU_STATUS) vu_flags_reported = false;
}

void EeRuntime::interpret(std::uint8_t* rdram, R5900Context* context) {
    const std::uint32_t entry = context->pc;
    // A mapped function's code runs within its extent (interior entries and BT2_EE_INTERPRET=all);
    // unmapped code runs until it reaches compiled code or returns.
    std::uint32_t start = 0, end = 0;
    const bool mapped = functionExtent(entry, start, end);
    const bool bridged = !registered(entry);
    struct Count {
        EeRuntime& runtime; std::uint32_t entry; bool bridged; std::uint64_t instructions = 0;
        ~Count() {
            if (bridged) {runtime.interpreted_instructions += instructions; runtime.interpreted_entries[entry].instructions += instructions;}
            else runtime.interpret_all_instructions += instructions;
        }
    } count{*this, entry, bridged};
    if (bridged) {
        auto& record = interpreted_entries[entry];
        if (!record.dispatches++) {
            record.first_word = read32(rdram, entry);
            std::fprintf(stderr, "EE interpreter: unmapped code at 0x%08X runs interpreted (thread %u)\n", entry, current_thread_id);
        }
    }
    bool vu_flags_reported = false;
    for (;;) {
        const std::uint32_t pc = context->pc;
        if (mapped && pc == end) return; // Ran off the function's end, as the compiled function does.
        if (!mapped && pc != entry && registered(pc)) {
            // Unmapped code ran into compiled code: that runs it.
            dispatchGuestBranch(rdram, context, pc, pc, 0, GuestBranchKind::TailJump, "interpreter fall-through");
            return;
        }
        const std::uint32_t word = read32(rdram, pc);
        ++count.instructions;
        if (isControl(word)) {
            ++count.instructions; // The delay slot.
            if (interpretControl(rdram, context, pc, word, start, end, vu_flags_reported)) return;
            continue;
        }
        runInterpretedInstruction(rdram, context, pc, word, vu_flags_reported);
        // ERET (no delay slot) returns to EPC or ErrorEPC: its handler set the pc, and the generated
        // code leaves the function there.
        if (word == 0x42000018) return;
        context->pc = pc + 4;
    }
}

// A branch or jump at `pc`; true when the interpreted frame ends (a return, or a transfer to code that
// runs elsewhere). Conditions and targets are read before the delay slot, as in the generated code.
bool EeRuntime::interpretControl(std::uint8_t* rdram, R5900Context* context, std::uint32_t pc, std::uint32_t word,
                                 std::uint32_t start, std::uint32_t end, bool& vu_flags_reported) {
    const std::uint32_t op = word >> 26, rs = (word >> 21) & 31, rt = (word >> 16) & 31, rd = (word >> 11) & 31, fn = word & 63;
    const std::uint32_t branch_target = pc + 4 + (static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(word & 0xFFFF))) << 2);
    const auto delay = [&] {
        const std::uint32_t at = pc + 4, slot = read32(rdram, at);
        if (isControl(slot)) stop("branch in the delay slot of the branch", word, pc);
        context->pc = at; context->in_delay_slot = true; context->branch_pc = pc;
        runInterpretedInstruction(rdram, context, at, slot, vu_flags_reported);
        context->in_delay_slot = false;
    };
    // Within the function (or, for unmapped code, anywhere that is not compiled) the jump stays here; a
    // backward one is a loop iteration, which the runtime counts as the generated code does.
    const auto local = [&](std::uint32_t target) {return end ? target >= start && target < end : !registered(target);};
    const auto transfer = [&](std::uint32_t target, bool counted) {
        context->pc = target;
        if (local(target)) return counted && target <= pc && eeCheckpointDue();
        dispatchGuestBranch(rdram, context, target, pc, 0, GuestBranchKind::TailJump, "interpreted jump");
        return true;
    };
    const auto call = [&](std::uint32_t target, GuestBranchKind kind, const char* name) {
        context->pc = target;
        dispatchGuestBranch(rdram, context, target, pc, pc + 8, kind, name);
        context->pc = pc + 8;
        return false;
    };
    const auto branch = [&](bool taken, bool likely) {
        if (likely && !taken) {context->pc = pc + 8; return false;}
        delay();
        if (taken) return transfer(branch_target, true);
        context->pc = pc + 8;
        return false;
    };
    if (op == 2) {const std::uint32_t target = ((pc + 4) & 0xF0000000u) | ((word & 0x03FFFFFFu) << 2); delay(); return transfer(target, true);}
    if (op == 3) {
        const std::uint32_t target = ((pc + 4) & 0xF0000000u) | ((word & 0x03FFFFFFu) << 2);
        SET_GPR_U32(context, 31, pc + 8); delay();
        return call(target, GuestBranchKind::DirectCall, "JAL");
    }
    if (op == 0 && fn == 8) {
        const std::uint32_t target = GPR_U32(context, rs);
        delay();
        if (rs == 31) {context->pc = target; return true;} // JR RA returns, as in the generated code.
        return transfer(target, false); // A table jump within the function is not a loop iteration.
    }
    if (op == 0 && fn == 9) {
        const std::uint32_t target = GPR_U32(context, rs);
        SET_GPR_U32(context, rd, pc + 8); delay();
        return call(target, GuestBranchKind::IndirectCall, "JALR");
    }
    switch (op) {
    case 4: case 20: return branch(GPR_U64(context, rs) == GPR_U64(context, rt), op == 20);
    case 5: case 21: return branch(GPR_U64(context, rs) != GPR_U64(context, rt), op == 21);
    case 6: case 22: return branch(GPR_S64(context, rs) <= 0, op == 22);
    case 7: case 23: return branch(GPR_S64(context, rs) > 0, op == 23);
    case 1:
        if (rt <= 3) return branch(rt & 1 ? GPR_S64(context, rs) >= 0 : GPR_S64(context, rs) < 0, rt >= 2);
        stop("unsupported branch and link", word, pc);
    case 16: // BC0F: the generated code takes it as never taken.
        if (rt == 0) return branch(false, false);
        stop("unsupported COP0 branch", word, pc);
    case 17: {const bool set = (context->fcr31 & 0x800000) != 0; if (rt <= 3) return branch(rt & 1 ? set : !set, rt >= 2); break;}
    case 18: {const bool set = cop2Condition(context); if (rt <= 3) return branch(rt & 1 ? set : !set, rt >= 2); break;}
    default: break;
    }
    stop("unsupported branch", word, pc);
}
