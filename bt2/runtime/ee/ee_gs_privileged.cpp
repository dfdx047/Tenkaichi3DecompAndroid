#include "ee/ee_runtime.h"

// The GS library's reset path, one behavior wherever it is called from (RUNTIME_GENERALIZATION_PLAN.md,
// group 2): the CSR reset, the CSR read that follows it, GsGetIMR/GsPutIMR and SetGsCrt. Each one does
// what the GS and the kernel do; afterwards the startup's sequence counters are advanced from their own
// state (the first reset, the graph startup's, main's two display re-initializations, the frame-phase
// resets). They are diagnostics and the startup's later steps read them; none decides whether a
// request is accepted.

void EeRuntime::gsCsrReset() {
    // RESET: the drawing registers return to their defaults (VRAM stays as drawn). The status bits and
    // FIELD read as clear until the next VBlank, as every capture of the read after a reset shows.
    gif_dma.gs.resetDrawingState();gs_csr_cleared=true;++gs_resets;
    if(!gs_csr_reset_writes)gs_csr_reset_writes=1;
    else if(!graph_startup_phase)graph_startup_phase=1;
    else if(graph_startup_phase==4 && (display_reset_phase==0 || display_reset_phase==8))++display_reset_phase;
    else if(display_reset_phase==13 && !frame_gs_reset_step) {frame_gs_reset_step=1;++frame_gs_resets;}
}

std::uint64_t EeRuntime::gsCsrRead() {
    if(graph_startup_phase==1)graph_startup_phase=2;
    else if(display_reset_phase==1 || display_reset_phase==9)++display_reset_phase;
    else if(frame_gs_reset_step==1)frame_gs_reset_step=2;
    else if(gs_csr_reset_writes && !gs_csr_reads)gs_csr_reads=1;
    // Revision 0x1B (the game consumes this byte), ID 0x55, the latched status bits, FINISH while a
    // FINISH event is not cleared, and the field the logical VBlank model last published.
    if(gs_csr_cleared)return 0x551B4000u;
    return 0x551B4000u|(display_intc_stat&0x1Du)|(gsFinishPending()?2u:0u)|(display_field?0x2000u:0u);
}

std::uint32_t EeRuntime::gsPutImr(std::uint64_t mask,std::uint32_t pc) {
    // Only the interrupt mask bits (8..15) exist; anything else is a request this model cannot answer.
    if(mask&~0xFF00ull)throw UnsupportedSyscall(0x71,pc);
    const auto previous=gs_imr;gs_imr=std::uint32_t(mask);
    if(graph_startup_phase==2)graph_startup_phase=3;
    else if(display_reset_phase==2 || display_reset_phase==10)++display_reset_phase;
    else if(frame_gs_reset_step==2)frame_gs_reset_step=3;
    else if(gs_csr_reads && !gs_imr_writes)gs_imr_writes=1;
    return previous;
}

void EeRuntime::gsSetCrt(std::uint64_t interlace,std::uint64_t mode,std::uint64_t field_mode,std::uint32_t pc) {
    // NTSC (mode 2) is what the presentation scans out; interlace and FFMD are single bits.
    if(interlace>1 || mode!=2 || field_mode>1)throw UnsupportedSyscall(2,pc);
    gs_crt_request={std::uint32_t(interlace),2,std::uint32_t(field_mode)};
    if(frame_gs_reset_step==3 || frame_gs_reset_step==4) {frame_gs_crt_request=gs_crt_request;frame_gs_reset_step=0;}
    else if(display_reset_phase==4 || display_reset_phase==11)++display_reset_phase;
    else if(graph_startup_phase==3) {graph_startup_phase=4;graph_halfword_checkpoint=halfword_writes;}
    else if(gs_imr_writes && !gs_crt_calls)gs_crt_calls=1;
}
