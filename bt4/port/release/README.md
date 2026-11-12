# Automatic updates

The launcher checks the latest published release of `NauTaro/DBZ-BT4-Tag-Team-Decomp` after startup. It downloads the
package for its platform and the corresponding signature in the background, then verifies the archive hash and its Ed25519
signature. When the verified update is ready, the game shows a restart prompt at the offline main menu. The player can
apply it immediately or choose Later; the game does not close or restart before that confirmation. It does not restart
during a battle, online session, lobby, spectator session, or while the settings window is open. Player data and game data
are preserved by the installer.

## Publishing a release

BT4 changes increment the revision, not the upstream engine version. Update `port/include/bt4_version.h`,
the build identities, `port/release/BT4_VERSION.txt`, package text and `CHANGELOG.md` together.
Push canonical tags such as `v0.1.39.3-R1`, with the matching display title `0.1.39.3 R1`. Patch revisions such as
`v0.1.39.3-R1.1` are also ordered by the updater. Each upstream adaptation starts its own revision series at R1.
The historical numeric tag `v0.1.39.3` belongs to the older R2 migration bridge; current releases use the
canonical upstream-and-revision format. The workflow checks the tag against the declared revision before packaging.

The release workflow builds Windows x64 and calls `native-platforms.yml` for Linux x86-64 and macOS Apple Silicon.
After all three builds pass, it signs and verifies each ZIP against `port/src/update_key.h`, uploads all six files to a
draft release, and checks that the asset set is complete. The release remains a draft until it is explicitly published
after gameplay validation. Missing artifacts, a missing signing key, or a key mismatch stop the workflow. An interrupted
upload leaves a draft rather than a public release with missing platform files.

The updater expects these filenames, each with an adjacent `.sig` file:

- `BT4decomp-windows-x64.zip`
- `BT4decomp-linux-x64.zip`
- `BT4decomp-macos-arm64.zip`

Dispatching the release workflow on a branch builds a Windows artifact without publishing. The native workflow
can also be dispatched independently for test builds; set its version input to the intended tag. These test
artifacts do not become an automatic update until a complete signed release is published.

The signing key is a one-time repository setup:

1. Generate a key with `python3 port/tools/update_key.py generate --out <safe-file>`.
2. Put the private key line in the repository Actions secret `UPDATE_SIGNING_KEY`. Keep the file offline and never
   commit it.
3. Commit the printed public key in `port/src/update_key.h`.

The current public key was generated for this BT4 updater. The matching private key is configured in the repository Actions secret; it is never committed. Players whose installed launcher predates the updater
or was built without this public key need one bootstrap install; later signed releases download automatically and install
only after the player confirms the restart prompt.

To check an archive manually, run `python3 port/tools/update_key.py verify <archive.zip>` with its `.sig` file beside
it. The command must report `signature good`.

## Windows runtime dependencies

The BT4 voice backend and bilingual setup require runtime DLLs and the GitHub CLI helper that are loaded
dynamically and therefore are not discovered by ordinary executable import scanning. Before packaging,
`port/tools/prepare_windows_runtime.py` restores only these dependencies from the pinned bootstrap archive,
verifies the archive SHA-256, and includes their licenses and voice SDK source offer. No legacy game executable
or game data is extracted. The package step checks every restored file against the dependency manifest.

Linux and macOS package builds use native TLS, SDL audio and a pinned RTC SDK with media and SCTP,
with bundled libcurl, SDL, Opus and SDK dependencies. Their build, transport and package checks are in `port/release/inside_native.sh`. Actual gameplay
coverage and platform requirements are documented in [native platform setup](../../docs/port/native_platforms.md).

## Windows shader reproducibility

Windows builds use the verified SPIR-V in `port/src/gs/generated/spirv`. Its normalized source hashes and bytecode hashes must match the manifest; the existing DXIL and SM3 checks remain enabled. This avoids compiler-version differences between generated SPIR-V and the checked-in DirectX binaries.

After editing GLSL, set `BT3_REGENERATE_SPIRV=1` during shader generation, regenerate DXIL and SM3 with their build tools, and refresh the SPIR-V cache and manifest together. Do not bypass the source or bytecode checks.
