# Native FLAC music cache

The converter stores decoded ADX music in FLAC without adding another lossy
encoding step. Conversion preserves the decoded source; it cannot recover
detail absent from the original recording.

`music_convert.c` uses the same ADX predictor coefficient truncation and sample
clamping as `gs/snd_adx.c`. It does not upsample, normalize or denoise. libFLAC
1.5.0 encodes signed 16-bit samples at compression level 8 with verification.
libogg 1.3.6 is linked statically as the libFLAC package dependency.

Build with UCRT64 GCC and `-DFLAC__NO_DLL`; link `libFLAC.a`, `libogg.a`, `-lm`,
`-lssp`, static winpthread and static libgcc. The release binary needs Windows
UCRT, not FFmpeg or a separate FLAC installation. Corresponding Xiph notices
are in the release's `licenses/flac` and `licenses/libogg` folders.

Build dependencies are the MSYS2 UCRT64 libFLAC and libogg development packages. Upstream sources: xiph/flac tag 1.5.0 and xiph/ogg v1.3.6.

Runtime retains original ADX paths and durations as game state. FLAC is a
presentation cache in `tbo/music/v1`, keyed by the whole original file's FNV64.
The decoder uses the existing SDL stream for gain, pan, mono, pause and time
stretch. It never creates another output device or applies miniaudio's
external-song volume lift. A missing or incompatible cache falls back to ADX.

The native default stereo balance preserves left/right independently. Mono
averages channels. Previously, a common centered pan summed both inputs into
both outputs and clipped before the user's volume adjustment. Independent
per-channel routing is retained when the channel pan values differ.

The converter writes and verifies temporary files before renaming them. It
locks the cache while writing, can resume after interruption, and marks each
completed language. On first real game launch, missing selected-language music
is prepared by a hidden process at idle/background priority; the game keeps
using its originals while preparation runs. A completed profile skips the
library pass. Room creation does not wait for this work.
