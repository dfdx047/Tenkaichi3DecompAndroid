# PC port

BT4decomp runs the game natively on Windows x64, Linux x86-64 and Apple Silicon. The runtime adapts
the original game code for host memory, deterministic arithmetic, rendering,
audio, input and online play.

## Architecture

| Component | Location |
| --- | --- |
| Game logic | `src/`, `include/` |
| Platform compatibility | `port/include/port_compat.h`, `port/src/` |
| Rendering and interface | `port/src/gs/` |
| Native installer | `port/setup/` |
| Compiler and source conversion tools | `port/tools/` |
| Runtime libraries and notices | `port/voice-runtime/`, `port/third_party/` |
| Tag Team service | `cloudflare/` |
| Matchmaking | `port/matchmaking/` |

The game retains a four-byte pointer domain and deterministic floating-point
behavior. Platform changes must preserve game structure layouts, address
translation and network simulation behavior.

## Build requirements

The Windows build uses a MinGW-w64 toolchain, Clang/libclang, Python, SDL3 and
the voice dependencies. Compiler selection and dependency paths are defined in
`port/tools/toolchain.py`. The `BT3_CC=win64` variant selects Windows;
`BT3_SDL3` selects the SDL3 development directory.

The BT4 build helpers are documented in [tools/review/README.md](../../tools/review/README.md).
They require the external toolchain and generated build inputs described there.
Game data and the external toolchain are not included in the source archive.

`install.py` validates compatible BT4 disc profiles and builds the native runtime
before extraction. Use a compatible **BT4 B14 Rev2 ISO**. Prebuilt packages and
platform-specific dependencies are described in [native platform setup](native_platforms.md).
See the [BT4 profile](../../BT4_PROFILE.md) for runtime and protocol identity.

## Development checks

Run the portable layout tests from the repository root:

```sh
python -m unittest discover -s tools/review/tests -v
```

Simulation changes require deterministic state comparisons and network checks.
Rendering, audio and input changes require validation on the affected platform.
The [decompilation guide](../decomp_guide.md) describes original-binary comparisons.

## Customization and platform reference

- [Stages](adding_stages.md)
- [Music](adding_songs.md)
- [Texture packs](texture_packs.md)
- [Menus](menus.md)
- [Fonts](menu_font.md)
- [Translations](translations.md)
- [Native platform installation](native_platforms.md)
- [Apple Silicon architecture](macos_plan.md)

Release packages target Windows x64, Linux x86-64 and macOS arm64. See the
native platform guide for minimum OS versions and the scope of validation.

## Menu layouts

F1 > UI > Menus provides Stretched, 4:3 and Widescreen (beta). The optional
widescreen layout positions menu columns and guides at their respective screen
edges while preserving the proportions of text and artwork. It is disabled by
default. Tenkaichi Budokai Online continues to use its existing full-width layout.
The menu layout setting is independent of the combat aspect ratio and HUD layout.
