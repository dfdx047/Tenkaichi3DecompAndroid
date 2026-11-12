#!/usr/bin/env python3
"""Unpacks the user's own disc image into loose folders for the PC build.

    port/tools/extract_disc.py <disc.iso> [out_dir]        (out_dir default: gamedata/)

Result: out_dir/pzs3us0/00000.bin ..., out_dir/pzs3us1/..., out_dir/pzs3us2/...: one file per entry of each AFS
archive, named by its index (the id the game asks for), plus the loose disc files under out_dir/disc/.
A file placed in out_dir/mods/<same path> is used instead of the original (see port/src/plat_file.c).
Needs `7z` to read the disc image. Nothing extracted may be committed or redistributed."""
import pathlib, shutil, struct, subprocess, sys

def split_afs(afs, out):
    out.mkdir(parents=True, exist_ok=True)
    with open(afs, "rb") as f:
        magic, count = struct.unpack("<4sI", f.read(8))
        if magic != b"AFS\0":
            raise SystemExit(f"{afs}: not an AFS archive")
        table = struct.unpack(f"<{count * 2}I", f.read(count * 8))
        for i in range(count):
            f.seek(table[i * 2])
            (out / f"{i:05d}.bin").write_bytes(f.read(table[i * 2 + 1]))
    return count

def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    iso = sys.argv[1]
    out = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else "gamedata")
    disc = out / "disc"
    disc.mkdir(parents=True, exist_ok=True)
    if sys.platform == "darwin" and shutil.which("7z") is None and shutil.which("bsdtar") is not None:  # (macOS: its own bsdtar reads disc images)
        subprocess.run(["bsdtar", "-xf", iso, "-C", str(disc), "DATA", "BIN", "SLUS_216.78"], check=True, stdout=subprocess.DEVNULL)
    else:
        subprocess.run(["7z", "x", "-y", f"-o{disc}", iso, "DATA", "BIN", "SLUS_216.78"], check=True,
                       stdout=subprocess.DEVNULL)
    for afs in sorted((disc / "DATA").glob("*.AFS")):
        n = split_afs(afs, out / afs.stem.lower())
        print(f"{afs.name}: {n} files")
        afs.unlink()  # the loose files replace it
    print(f"done: {out}")

main()
