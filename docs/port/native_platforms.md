# Native platform setup

BT4decomp packages target Windows x64, Linux x86-64 (glibc 2.35 or later),
and macOS 15 or later on Apple Silicon arm64. Intel macOS and ARM Linux are not supported by
these packages. macOS binaries are ad-hoc signed and are not notarized.

Extract the complete package. On Linux or macOS, run `./BT4decomp-setup`, select
your own compatible BT4 B14 Rev2 LATINO or English ISO, and start `./BT4decomp`.
If an extractor loses executable permissions, run `chmod +x BT4decomp BT4decomp-setup`.
macOS may require allowing the downloaded binary in Privacy & Security.
The package includes no ISO or extracted game data.

## From source

`python3 install.py --check` checks dependencies without installing.
`python3 install.py --iso /path/to/BT4.iso` validates the ISO's BT4 program
profile and builds a runtime in `BT4decomp-runtime/`. `./play.sh` starts it.
Use `--language es`, `en`, or `both`; the default uses the ISO's language.
An optional second language uses the GitHub CLI (`gh`); authorize access if
the package's repository requires it. A single-language install needs no GitHub account.

Linux source builds need GCC/G++, matching LLVM and libclang, SDL3 development
headers, zlib, Opus, a WebSocket-enabled libcurl, and glslangValidator or glslc.
The release container pins the required compiler and TLS library.
Apple Silicon source builds use Xcode command-line tools and Homebrew
`llvm sdl3 shaderc spirv-cross curl pkgconf opus openssl@3 cmake ninja`; `BT3_CURL` can override the
Homebrew curl prefix. `install.py` prepares matching Python libclang bindings.

## Online and updates

Gameplay uses the same BT4 compatibility family and wire formats across platforms.
Current source builds negotiate a WebRTC Tag Team game path, with native TLS
WebSockets for room control and backup delivery. Lobbies and 1 vs 1 retain UDP
and TURN. The release builds include a pinned RTC SDK for media and SCTP;
voice uses SDL audio streams with TLS Opus as a fallback. See the
[SDK build reference](../../port/release/RTC_SDK.md). Published package contents
are identified by their release; the main branch may contain newer changes.
Game synchronization does not wait for voice packets.

Signed updates select a platform-specific ZIP. Saves, settings, installed game
data and custom content remain outside the update replacement list. Build and
package checks do not replace gameplay validation on individual hardware.
