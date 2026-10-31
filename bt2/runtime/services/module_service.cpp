#include "ee/ee_runtime.h"
#include <array>
#include <string>
#include <string_view>

// Native LOADFILE calls (0x80000006): IOP module loads by path (fn 0) and from an IOP buffer
// (fn 6). No IOP module runs; their services are native. The reply is {module ID, residency
// result}: the result each module's entry returned (0 resident, 1 no resident, 2 removable).

namespace {
constexpr std::uint32_t load_by_path=0,load_from_buffer=6;
struct ModuleResult {std::string_view path;std::uint32_t result;};
constexpr std::array<ModuleResult,13> module_results={{
    {"cdrom0:\\IRX\\SIO2MAN.IRX;1",0},{"cdrom0:\\IRX\\MCMAN.IRX;1",2},{"cdrom0:\\IRX\\MCSERV.IRX;1",2},
    {"cdrom0:\\IRX\\DBCMAN.IRX;1",0},{"cdrom0:\\IRX\\SIO2D.IRX;1",0},{"cdrom0:\\IRX\\DS2U_D.IRX;1",0},
    {"cdrom0:\\IRX\\LIBSD.IRX;1",0},{"cdrom0:\\IRX\\SDRDRV.IRX;1",2},{"cdrom0:\\IRX\\MODHSYN.IRX;1",0},
    {"cdrom0:\\IRX\\MODSESQ2.IRX;1",0},{"cdrom0:\\IRX\\CDVDSTM.IRX;1",2},{"cdrom0:\\IRX\\SOUNDS.IRX;1",0},
    {"cdrom0:\\IRX\\CRI_ADXI.IRX;1",2}}};
constexpr std::uint32_t embedded_module_result=1; // The EE-embedded module ends without residency.
}

bool EeRuntime::nativeModuleCall(std::uint8_t* rdram,R5900Context* context,const SifRpcRequest& r,const NativeServer& server) {
    // Version (0xFF) and the bind stay on the captured LOADFILE replay for now.
    if(r.rpc_number!=load_by_path && r.rpc_number!=load_from_buffer)return false;
    if(r.send_size!=512 || r.recv_size!=8)throw UnsupportedSyscall(0x77,context->pc);
    std::uint32_t result;
    if(r.rpc_number==load_by_path) {
        std::string path;for(unsigned n=8;n<260;++n){const auto ch=read8(rdram,r.send_source+n);if(!ch)break;path+=static_cast<char>(ch);}
        const auto found=std::find_if(module_results.begin(),module_results.end(),[&](const auto& m){return m.path==path;});
        if(found==module_results.end())throw UnsupportedSyscall(0x77,context->pc);
        result=found->result;
    } else {
        // Load from the IOP buffer the EE uploaded the embedded module into.
        const auto buffer=read32(rdram,r.send_source);
        if(!native_heap_.isBlock(buffer) || read32(rdram,r.send_source+4))throw UnsupportedSyscall(0x77,context->pc);
        result=embedded_module_result;
    }
    write32(rdram,r.receive,native_module_loads+1);write32(rdram,r.receive+4,result);
    completeNativeCall(rdram,context,r,server);
    ++native_module_loads;++sif_dma_submissions;context->r[2].low=0x30000+serviceOperations();return true;
}
