#include "probe_state.h"
#ifdef BT2_OVERLAY_BINDINGS
#include <bt2_overlays.h>
#endif

// Selected overlay entries, each registered with the bytes it was generated from. The generated
// header is self-contained, so changing the overlay selection recompiles just this file.
void registerSelectedOverlays([[maybe_unused]] EeRuntime& runtime) {
#ifdef BT2_OVERLAY_BINDINGS
    registerOverlays(runtime);
#endif
}
