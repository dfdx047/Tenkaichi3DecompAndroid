#pragma once
#include "ps2_runtime.h"

// The EE interpreter's handlers (WHOLE_GAME_COVERAGE_PLAN.md, step 3): one per distinct instruction word of
// the mapped code that is not a branch or jump, made by the recompiler from that word with the same source
// preparation as the game's functions (tools/ps2recomp/generate_interpreter.py; local and ignored). A
// handler runs its instruction at `pc`. The table is in parts, each sorted, the parts in word order.
using EeInterpreterHandlerFunction = void (*)(std::uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime, std::uint32_t pc);
// Flags: the instruction writes the VU0 status flag (1), reads it (2), sets VU0 flags whose effects
// are reported between such a write and read (4), or is a VU0 instruction of unknown flag effect (8).
struct EeInterpreterHandler {std::uint32_t word; EeInterpreterHandlerFunction run; std::uint32_t flags;};
enum : std::uint32_t {EE_HANDLER_WRITES_VU_STATUS=1, EE_HANDLER_READS_VU_STATUS=2, EE_HANDLER_SETS_VU_FLAGS=4, EE_HANDLER_UNKNOWN_VU=8};
struct EeInterpreterHandlerPart {const EeInterpreterHandler* handlers; unsigned count;};

// Defined by the generated table, or empty in builds without it (interpreter_handlers_none.cpp).
extern const EeInterpreterHandlerPart ee_interpreter_handler_parts[];
extern const unsigned ee_interpreter_handler_part_count;
