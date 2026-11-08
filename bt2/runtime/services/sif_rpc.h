#pragma once
#include <cstdint>

// Native SIF RPC services (docs/plans/iop/NATIVE_SERVICES_PLAN.md): the game's SIF RPC requests are
// answered by host code; no IOP code runs. This header is included by ps2_runtime.h,
// so it holds declarations and plain data only.

// One decoded sceSifSetDma request carrying a SIF RPC command packet.
struct SifRpcRequest {
    std::uint32_t packet = 0;      // EE address of the 64-byte command packet.
    std::uint32_t cid = 0;         // 0x80000009 bind, 0x8000000A call.
    std::uint32_t rpc_id = 0, client = 0, rpc_number = 0, send_size = 0, receive = 0, recv_size = 0, rmode = 0;
    std::uint32_t send_source = 0; // EE address of the send data (0 without data).
};

// An open cdrom0: file of the native FILEIO service.
struct NativeFile {std::uint32_t lba = 0, size = 0, position = 0;};

struct NativePadState; // runtime/services/pad_service.cpp

// A CRI stream channel: the EE buffer the game uploads and the IOP buffer it is returned from.
struct NativeCriChannel {std::uint32_t ee = 0, iop = 0; bool pending = false;};
