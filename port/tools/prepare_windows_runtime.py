#!/usr/bin/env python3
"""Assemble pinned Windows voice and GitHub authorization dependencies."""
import argparse
import hashlib
import json
import pathlib
import urllib.request
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
GH_VERSION = "2.103.3"
GH_URL = f"https://github.com/cli/cli/releases/download/v{GH_VERSION}/gh_{GH_VERSION}_windows_amd64.zip"
GH_SHA256 = "98d0cfa794721665d7973dd728c78aff7bd836d58f10f6d7a03568eccb0a22e0"
VOICE_DLLS = ("libdatachannel.dll", "libopus-0.dll", "libwinpthread-1.dll")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def fetch_github_cli(archive):
    archive.parent.mkdir(parents=True, exist_ok=True)
    if not archive.exists() or sha256(archive.read_bytes()) != GH_SHA256:
        temporary = archive.with_suffix(archive.suffix + ".part")
        request = urllib.request.Request(GH_URL, headers={"User-Agent": "BT4decomp-build"})
        with urllib.request.urlopen(request, timeout=120) as response, temporary.open("wb") as output:
            while block := response.read(1024 * 1024):
                output.write(block)
        if sha256(temporary.read_bytes()) != GH_SHA256:
            temporary.unlink(missing_ok=True)
            raise SystemExit("GitHub CLI archive checksum mismatch")
        temporary.replace(archive)
    return archive


def prepare(archive=None):
    archive = pathlib.Path(archive).resolve() if archive else ROOT / f"port/build/gh_{GH_VERSION}_windows_amd64.zip"
    archive = fetch_github_cli(archive)
    target = ROOT / "port/build/windows-runtime"
    manifest = {}
    runtime_assets = json.loads((ROOT / "port/runtime-assets.json").read_text(encoding="utf-8"))
    expected_hashes = {item["source"]: item["sha256"] for item in runtime_assets}

    def put(relative, data):
        output = target.joinpath(*pathlib.PurePosixPath(relative).parts)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
        manifest[relative] = sha256(data)

    voice = ROOT / "port/voice-runtime"
    for name in VOICE_DLLS:
        source = voice / name
        if not source.is_file():
            raise SystemExit("Missing pinned Windows voice dependency: " + name)
        relative = "port/voice-runtime/" + name
        data = source.read_bytes()
        if expected_hashes.get(relative) != sha256(data):
            raise SystemExit("Windows voice dependency checksum mismatch: " + name)
        put(name, data)
    license_dir = voice / "licenses/voice"
    for source in sorted(license_dir.rglob("*")):
        if source.is_file() and source.name != "SOURCES.json":
            put("licenses/voice/" + source.relative_to(license_dir).as_posix(), source.read_bytes())

    source_archive = ROOT / "port/build/libdatachannel-build-source.zip"
    source_lock = ROOT / "port/release/rtc-sources.lock.json"
    build_receipt = license_dir / "WINDOWS-BUILD.json"
    if not source_archive.is_file() or not source_lock.is_file() or not build_receipt.is_file():
        raise SystemExit("Pinned Windows RTC source kit is incomplete")
    receipt = json.loads(build_receipt.read_text(encoding="utf-8"))
    if receipt["library_sha256"] != manifest["libdatachannel.dll"] or receipt["source_lock_sha256"] != sha256(source_lock.read_bytes()):
        raise SystemExit("Windows RTC build receipt does not match the shipped library and source lock")
    put("licenses/source/VOICE-SDK-source.zip", source_archive.read_bytes())
    put("licenses/voice/SOURCES.json", source_lock.read_bytes())
    put("licenses/voice/BUILD.json", build_receipt.read_bytes())
    windows_sources = license_dir / "SOURCES.json"
    if windows_sources.is_file():
        put("licenses/voice/WINDOWS-RUNTIME-SOURCES.json", windows_sources.read_bytes())

    with zipfile.ZipFile(archive) as package:
        names = set(package.namelist())
        executable = "bin/gh.exe"
        license_name = "LICENSE"
        if executable not in names or license_name not in names:
            raise SystemExit("Pinned GitHub CLI archive is incomplete")
        put("tbo/setup/gh.exe", package.read(executable))
        put("licenses/GitHub-CLI-MIT.txt", package.read(license_name))

    (target / "manifest.json").write_text(
        json.dumps({
            "github_cli_version": GH_VERSION,
            "github_cli_url": GH_URL,
            "github_cli_archive_sha256": GH_SHA256,
            "files": manifest,
        }, indent=2) + "\n",
        encoding="utf-8",
    )
    print(f"Prepared {len(manifest)} verified Windows runtime files")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=pathlib.Path)
    prepare(parser.parse_args().archive)
