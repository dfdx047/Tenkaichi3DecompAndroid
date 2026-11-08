#include "ee/ee_interpreter_handlers.h"

// Builds without the generated interpreter handlers (tools/ps2recomp/generate_interpreter.py): the
// interpreter has none, so dispatch to code no generated function holds stops as before.
const EeInterpreterHandlerPart ee_interpreter_handler_parts[1] = {{nullptr, 0}};
const unsigned ee_interpreter_handler_part_count = 0;
