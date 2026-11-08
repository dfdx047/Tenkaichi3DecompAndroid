#include "probe_state.h"
#ifdef BT2_EXTRA_STARTUP_BINDINGS
#include <bt2_extra_startup.h>
#endif

// Entries chosen by the selection loop (tools/ps2recomp/expand_startup.py). The generated header
// declares only these functions, so changing the selection recompiles just this file.
void registerSelectedStartup([[maybe_unused]] EeRuntime& runtime) {
#ifdef BT2_EXTRA_STARTUP_BINDINGS
    registerExtraStartup(runtime);
#endif
}
