#!/usr/bin/env python3
"""Extracts the executables from the disc image, splits them with splat and writes build.ninja.

Usage: .venv/bin/python configure.py && ninja
"""

import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ISO = ROOT / "Dragon Ball Z - Budokai Tenkaichi 3 (USA) (En,Ja).iso"
DISC = ROOT / "disc"
BINUTILS = "tools/binutils/mips-ps2-decompals-"

# End of .sdata rounded up to the 128-byte section alignment the original link used.
ROM_PAD_TO = 0x2FF180

AS_FLAGS = "-EL -march=r5900 -mabi=eabi -G0 -no-pad-sections -Iinclude"
# Compiler output is assembled as o64 so that address arithmetic on 32-bit pointers expands to
# addiu/addu as Sony's assembler did; with eabi the modern gas emits daddiu/daddu.
CC_AS_FLAGS = "-EL -march=r5900 -mabi=o64 -no-pad-sections -mno-pdr -Iinclude"

# The game was built with Sony's ee-gcc 2.96 at -O2. Spike's code uses the default
# small-data threshold (-G8); the CRI middleware was built with -G0.
# -fno-strict-aliasing is the original's flag: about 1,060 matched functions break without it
# (measured 2026-10-05 over all files; see docs/decomp_guide.md, "Toolchain check").
CC = "tools/ee-gcc2.96/bin/ee-gcc"
CC_FLAGS = "-O2 -fno-strict-aliasing -Iinclude"
# Keys are path prefixes: a directory (with its slash) or the stem of a group of files. The wish screen
# (src/sys/late_a*.c, linked behind the libraries) was built with -G0 like the middleware around it.
G_FLAGS = {"src/cri/": "-G0", "src/menu/": "-G0", "src/sys/late_a": "-G0"}
G_DEFAULT = "-G8"
# Assembler-only exceptions (the compiler keeps the flag above). Under -G0 Sony's assembler still deferred the
# small-data decision for a `symbol(reg)` load of a symbol not defined yet, so the load's last instruction did
# not move into the delay slot of a following `jal` (which got a nop); the modern gas does that only when it is
# given a non-zero -G. `SimEv28` (the card game) has the overlay's only such site. Assembling with -G8 is safe
# only for a file without float constants (`li.s` would become a gp-relative .lit4 load).
AS_G_FLAGS = {"src/menu/menu_t_b": "-G8"}

# Each target is one binary that is split, rebuilt and compared on its own.
# `subdir` is the target's folder under asm/ and `src_subdir` the one under src/; the main executable owns
# everything else. `extern_syms` is a generated linker script that defines every listed symbol lying OUTSIDE
# the target's image (splat only writes undefined_*_auto entries for references made from assembly, so C code
# of the overlay calling the main executable, or the reverse, needs them); `image` is the target's address range.
TARGETS = [
    {
        "name": "SLUS_216.78",
        "yaml": "config/SLUS_216.78.yaml",
        "subdir": None,
        "original": "disc/SLUS_216.78.rom",
        "built": "build/SLUS_216.78.rom",
        "src_subdir": None,
        "image": (0x100000, 0x334C00),
        "extern_syms": "build/SLUS_216.78_extern_syms.ld",
        "ld_scripts": ["build/SLUS_216.78.ld", "build/undefined_funcs_auto.txt",
                       "build/undefined_syms_auto.txt", "build/SLUS_216.78_extern_syms.ld",
                       "config/linker_script_extra.ld"],
    },
    {
        # Menu/UI code loaded at 0x334C00, right after the main executable's .bss.
        "name": "DBZP",
        "yaml": "config/DBZP.yaml",
        "subdir": "dbzp",
        "src_subdir": "menu",
        "original": "disc/BIN/DBZP.BIN",
        "built": "build/DBZP.BIN",
        "image": (0x334C00, 0x3BE71C),
        "extern_syms": "build/DBZP_extern_syms.ld",
        "ld_scripts": ["build/DBZP.ld", "build/dbzp_undefined_funcs_auto.txt",
                       "build/dbzp_undefined_syms_auto.txt", "build/DBZP_extern_syms.ld",
                       "config/linker_script_extra_dbzp.ld"],
    },
]
SUBDIRS = {t[k] for t in TARGETS for k in ("subdir", "src_subdir") if t[k]}


# The VU1 microprogram block (.vutext): listings in address order, the wrapper that includes the assembled block,
# and the splat-generated file whose object it replaces in the link.
VU1_LISTINGS = [f"src/vu1/prog{n}.vsm" for n in ("0", "1", "2a", "2b", "4", "5", "6", "7", "8")]
VU1_BIN = "build/vu1/vutext.bin"
VU1_ASM = "src/vu1/vutext.s"
VU1_SPLAT_ASM = "asm/data/cod/1BF6B0.s"


def run(cmd, **kwargs):
    print("$", " ".join(shlex.quote(str(c)) for c in cmd))
    subprocess.run(cmd, check=True, cwd=ROOT, **kwargs)


def extract():
    if (DISC / "SLUS_216.78").exists() and (DISC / "BIN" / "DBZP.BIN").exists():
        return
    if not ISO.exists():
        sys.exit(f"missing disc image: {ISO.name}")
    run(["7z", "x", "-y", f"-o{DISC}", ISO, "SYSTEM.CNF", "SLUS_216.78", "BIN", "IRX"],
        stdout=subprocess.DEVNULL)


def make_rom():
    run([BINUTILS + "objcopy", "-O", "binary", "--gap-fill=0x00",
         f"--pad-to={ROM_PAD_TO:#x}", DISC / "SLUS_216.78", DISC / "SLUS_216.78.rom"])


def split(target):
    # splat shells out to mips-linux-gnu-* binutils; tools/bin aliases them.
    env = dict(os.environ, PATH=f"{ROOT / 'tools' / 'bin'}{os.pathsep}{os.environ['PATH']}")
    run([sys.executable, "-m", "splat", "split", ROOT / target["yaml"]], env=env,
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def linked_c_files(yaml_path):
    """C files named by `c` subsegments in a splat yaml: the ones the linker script uses."""
    text = (ROOT / yaml_path).read_text()
    return {Path("src") / f"{name}.c" for name in re.findall(r"\[0x[0-9A-Fa-f]+, c, ([\w/]+)\]", text)}


def write_extern_syms(target):
    """Linker script defining the listed symbols that lie outside the target's image."""
    text = (ROOT / target["yaml"]).read_text()
    lo, hi = target["image"]
    seen = {}
    for name in re.findall(r"^\s+- (config/\S+\.txt)$", text, re.M):
        if "reloc" in name:
            continue
        for line in (ROOT / name).read_text().splitlines():
            m = re.match(r"(\w+) = 0x([0-9A-Fa-f]+);", line)
            if m and not lo <= int(m.group(2), 16) < hi:
                seen.setdefault(m.group(1), int(m.group(2), 16))
    out = ["/* Generated by configure.py: listed symbols outside this image. */"]
    out += [f"{name} = 0x{addr:08X};" for name, addr in sorted(seen.items(), key=lambda x: (x[1], x[0]))]
    (ROOT / target["extern_syms"]).write_text("\n".join(out) + "\n")


def sources(top, suffix, subdir):
    """Files under top/ belonging to a target: its own subdir, or everything outside all subdirs."""
    found = []
    for path in sorted((ROOT / top).rglob(f"*{suffix}")):
        rel = path.relative_to(ROOT)
        # asm/**/nonmatchings holds per-function files pulled in by INCLUDE_ASM, not standalone units.
        if "nonmatchings" in rel.parts:
            continue
        owner = rel.parts[1] if rel.parts[1] in SUBDIRS else None
        if owner == subdir:
            found.append(rel)
    return found


def write_ninja():
    out = [
        f"as = {BINUTILS}as",
        f"ld = {BINUTILS}ld",
        f"objcopy = {BINUTILS}objcopy",
        f"as_flags = {AS_FLAGS}",
        "",
        "rule as",
        "  command = $as $as_flags $in -o $out",
        "  description = AS $in",
        "",
        # ee-gcc's driver crashes when it runs its own assembler on a modern host, so compile
        # to assembly and assemble with the modern gas. gcc_prelude.inc makes `move` encode
        # as daddu, the way Sony's assembler did.
        "rule cc",
        f"  command = {CC} {CC_FLAGS} $gflag -S $in -o $out.s && "
        f"$as {CC_AS_FLAGS} $asgflag include/gcc_prelude.inc $out.s -o $out && "
        f"{sys.executable} scripts/set_eabi64.py $out",
        "  description = CC $in",
        "",
        "rule ld",
        "  command = $ld -EL $scripts -Map $out.map -o $out",
        "  description = LD $out",
        "",
        "rule rom",
        "  command = $objcopy -O binary --gap-fill=0x00 $in $out",
        "  description = OBJCOPY $out",
        "",
        "rule check",
        "  command = cmp $in && touch $out",
        "  description = CHECK $in",
        "",
    ]
    # The VU1 microprograms are assembled from listings; the object replaces the one splat cuts from the image.
    out += [
        "rule vuasm",
        f"  command = {sys.executable} scripts/vuasm.py -o $out $in",
        "  description = VUASM $out",
        "",
        f"build {VU1_BIN}: vuasm {' '.join(VU1_LISTINGS)} | scripts/vuasm.py",
        "",
    ]
    headers = " ".join(str(p.relative_to(ROOT)) for p in sorted((ROOT / "include").rglob("*.h")))
    for target in TARGETS:
        asm = sources("asm", ".s", target["subdir"])
        # Only compile C files that have a segment: work-in-progress files under src/ that are
        # not linked yet must not be able to break the build.
        linked = linked_c_files(target["yaml"])
        srcs = [p for p in sources("src", ".c", target["src_subdir"]) if p in linked]
        objs = [Path("build") / p.with_suffix(".o") for p in asm + srcs]
        elf = f"build/{target['name']}.elf"
        ok = f"build/{target['name']}.ok"

        for src, obj in zip(asm, objs):
            if str(src) == VU1_SPLAT_ASM:
                out.append(f"build {obj}: as {VU1_ASM} | {VU1_BIN} include/macro.inc include/labels.inc")
                continue
            out.append(f"build {obj}: as {src} | include/macro.inc include/labels.inc")
        for src, obj in zip(srcs, objs[len(asm):]):
            gflag = next((g for d, g in G_FLAGS.items() if str(src).startswith(d)), G_DEFAULT)
            out.append(f"build {obj}: cc {src} | {headers} include/gcc_prelude.inc")
            out.append(f"  gflag = {gflag}")
            asg = next((g for d, g in AS_G_FLAGS.items() if str(src).startswith(d)), gflag)
            out.append(f"  asgflag = {asg}")
        out += [
            f"build {elf}: ld | {' '.join(map(str, objs))} {' '.join(target['ld_scripts'])}",
            f"  scripts = {' '.join('-T ' + s for s in target['ld_scripts'])}",
            f"build {target['built']}: rom {elf}",
            f"build {ok}: check {target['built']} {target['original']}",
            f"default {ok}",
            "",
        ]
    (ROOT / "build.ninja").write_text("\n".join(out))


def main():
    extract()
    make_rom()
    for target in TARGETS:
        split(target)
        write_extern_syms(target)
    write_ninja()


if __name__ == "__main__":
    main()
