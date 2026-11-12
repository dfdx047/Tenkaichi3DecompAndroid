# Windows antivirus reports: investigation in progress

Users reported antivirus alerts for the Windows R5 ZIP. The supplied VirusTotal screenshot identifies the official R5 `BT4decomp.exe` and shows Gridinsoft `Trojan.Heur!.02214123` and Trapmine `Malicious.moderate.ml.score`, with 2 of 71 engines flagging the file at that time. A separate Skyhigh alert for `libdatachannel.dll` was also reported. We have not independently retrieved the current Skyhigh report or established whether these alerts are false positives.

The EXE SHA-256 in the screenshot matches the EXE extracted from the official R5 release ZIP. Per-file SHA-256 manifests for **all files** in the Windows, Linux and macOS R5 packages are published under [checksums/v0.1.39.5-R5](../../checksums/v0.1.39.5-R5). These hashes identify the distributed bytes; they are not a malware verdict or a code-signing certificate.

## Windows executable inventory

| File | SHA-256 |
| --- | --- |
| `BT4decomp-setup.exe` | `7a97dc2f477d60009aa14382d19b455c0c118bcc5775fea8068b6e324308bac2` |
| `BT4decomp.exe` | `cc78f3f47eb08350113bb6e5a12093ea6f60aaa4bba104a48962ddccc378d34e` |
| `Install.exe` | `7a97dc2f477d60009aa14382d19b455c0c118bcc5775fea8068b6e324308bac2` |
| `SDL3.dll` | `49db50085edc74c2b5962f010e2416557188dcc67d6fb8a61381de2a2cd3a287` |
| `libdatachannel.dll` | `4a90d22d9d232da2906857f3b58044165215a1bfb481f9f4cf4be4f33a5c9909` |
| `libopus-0.dll` | `e747edad6bf02cf5cdcce417b95be3b7ba108f93df1b024778be5d73fa6dae39` |
| `libwinpthread-1.dll` | `a5df05bf091bc2ba0a89f83a60d2049e9d434bb9631fec82faf20ff64a53456a` |
| `tbo/setup/gh.exe` | `d303d8ac3f5e9438bf7c977b64dd6425513eae46218f8881d30694f28c1d0dfd` |

## Verification performed

- Checked the published platform ZIPs against their GitHub asset digests and update signatures before generating the manifests.
- Verified that the shipped `libdatachannel.dll` matches the SHA-256 pinned in `port/runtime-assets.json` and its Windows build receipt, `port/voice-runtime/licenses/voice/WINDOWS-BUILD.json`.
- The source lock for libdatachannel and its dependencies is public in `port/release/rtc-sources.lock.json`. The package includes the corresponding source/license material. This provenance check does not replace malware analysis.
- A custom Microsoft Defender scan of every extracted Windows PE executable and DLL completed with “found no threats.” This is one engine's result, not confirmation that every vendor considers the package clean. The scan used `-DisableRemediation`, which scans without changing detections, exclusions or protection settings.
- The R5 Hotfix Windows package contains a new game executable with the online fixes and the same pinned libdatachannel DLL as R5. Microsoft Defender reported no threats when scanning the extracted Windows executables and DLLs in the signed Hotfix ZIP. The vendor review remains open. See the [Hotfix SHA-256 manifests](../../checksums/v0.1.39.5-R5.1).

## What remains open

The current Gridinsoft, Trapmine and Skyhigh verdicts require independent vendor review. No zero-detection guarantee is being made. We will update this document with dated evidence and any resulting source or dependency corrections. Do not turn off antivirus protection or add exclusions to work around an alert. If a security product blocks a file, leave it blocked while the review is pending.

Public report links for the identified R5 bytes: [BT4decomp.exe](https://www.virustotal.com/gui/file/cc78f3f47eb08350113bb6e5a12093ea6f60aaa4bba104a48962ddccc378d34e/detection), [libdatachannel.dll](https://www.virustotal.com/gui/file/4a90d22d9d232da2906857f3b58044165215a1bfb481f9f4cf4be4f33a5c9909/detection). Report contents and vendor results may change over time.
