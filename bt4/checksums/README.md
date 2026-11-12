# Release file hashes

Each platform folder contains the SHA-256 of every file inside the corresponding official ZIP, including the game, installer and bundled libraries. Keep the version and platform identical when comparing files. Installed game data and user modifications are not part of these release manifests.

- [R5 Windows](v0.1.39.5-R5/windows-x64/SHA256SUMS.txt)
- [R5 Linux](v0.1.39.5-R5/linux-x64/SHA256SUMS.txt)
- [R5 macOS](v0.1.39.5-R5/macos-arm64/SHA256SUMS.txt)
- [R5 Hotfix Windows](v0.1.39.5-R5.1/windows-x64/SHA256SUMS.txt)
- [R5 Hotfix Linux](v0.1.39.5-R5.1/linux-x64/SHA256SUMS.txt)
- [R5 Hotfix macOS](v0.1.39.5-R5.1/macos-arm64/SHA256SUMS.txt)
- [R5 1.1 Windows](v0.1.39.5-R5.2/windows-x64/SHA256SUMS.txt)
- [R5 1.1 Linux](v0.1.39.5-R5.2/linux-x64/SHA256SUMS.txt)
- [R5 1.1 macOS](v0.1.39.5-R5.2/macos-arm64/SHA256SUMS.txt)
- [R5 1.2 Windows](v0.1.39.5-R5.3/windows-x64/SHA256SUMS.txt)
- [R5 1.2 Linux](v0.1.39.5-R5.3/linux-x64/SHA256SUMS.txt)
- [R5 1.2 macOS](v0.1.39.5-R5.3/macos-arm64/SHA256SUMS.txt)
- [R5 1.3 Windows](v0.1.39.5-R5.4/windows-x64/SHA256SUMS.txt)
- [R5 1.3 Linux](v0.1.39.5-R5.4/linux-x64/SHA256SUMS.txt)
- [R5 1.3 macOS](v0.1.39.5-R5.4/macos-arm64/SHA256SUMS.txt)

Verify a complete ZIP without extracting or executing its contents:

```text
python port/tools/verify_release_manifest.py checksums/v0.1.39.5-R5/windows-x64/SHA256SUMS.txt BT4decomp-windows-x64.zip
```

For a single file on Windows, compare the result of `Get-FileHash -Algorithm SHA256 .\BT4decomp.exe` with its manifest entry. On Linux, `sha256sum` computes that digest; on macOS use `shasum -a 256`.

Matching hashes establish that bytes match this inventory. They do **not** establish that a file is harmless, resolve antivirus findings, or replace an Authenticode signature. ZIP updater signatures are separate `.sig` assets checked by the launcher. See the [current Windows antivirus investigation](../docs/security/windows-antivirus-review.md).
