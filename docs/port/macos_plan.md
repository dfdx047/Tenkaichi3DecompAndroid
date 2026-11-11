# Apple Silicon runtime architecture

BT4decomp includes a native Apple Silicon build, selected with `BT3_CC=mac64`.
The arm64 package targets macOS 15 or later. Installation instructions and
download handling are in [native platform setup](native_platforms.md).

## Memory model

The game stores four-byte pointers. On x86-64, the host runtime preserves those
layouts with `__ptr32 __uptr` and low-address mappings. Apple Silicon requires
address translation because the low 4 GB of the process address space are unmapped.

`port/tools/ptr32.py` transforms pointer operations for the target architecture.
`port/src/port_addr.h` defines the address translation interface.
Game structure layouts and deterministic arithmetic must remain consistent
across platform variants.

## Toolchain

The macOS variant uses Homebrew LLVM and the macOS SDK. `BT3_LLVM` selects an
alternate LLVM binary directory; `port/tools/toolchain.py` defines compiler and
linker options. The runtime uses SDL3 with Metal and CoreAudio.

The native workflow builds the runtime, tests transport, audio and updates,
and checks the relocated package. Gameplay on individual Mac hardware still
requires validation; a successful build alone does not establish compatibility
with every graphics device or remote opponent.
