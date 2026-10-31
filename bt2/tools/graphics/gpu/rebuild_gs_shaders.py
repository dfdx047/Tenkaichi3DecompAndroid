"""Compile again the paraLLEl-GS shaders the project's patches change, and point the checkout's
gs/shaders/slangmosh.hpp at them.

paraLLEl-GS keeps its shaders compiled, in one SPIR-V bank (slangmosh.hpp, made by Granite's slangmosh tool,
which this project does not build). parallel_gs_no_8bit_storage.patch changes the sources of three of them
(VRAM bytes through 32-bit words, for drivers without 8-bit storage), so their compiled forms are made here with
glslc (the Android NDK's, or one on PATH) and added to the bank; the bank's other shaders and the reflection data
stay as they were (the changed shaders keep their bindings). The header is always made from the pinned
revision's own, so running this again gives the same file.

Run by fetch_parallel_gs.py after the patches; by hand: python tools/graphics/gpu/rebuild_gs_shaders.py.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

# (program in slangmosh.hpp, shader file, its defines)
PROGRAMS = [
    ("upload[0]", "upload.comp", {"SUPER_SAMPLED": 0}),
    ("upload[1]", "upload.comp", {"SUPER_SAMPLED": 1}),
    ("vram_copy", "vram_copy.comp", {}),
] + [(f"ubershader[{c}][{d}]", "ubershader.comp", {"FEEDBACK_COLOR": c, "FEEDBACK_DEPTH": d}) for d in (0, 1) for c in (0, 1)]


def glslc():
    found = shutil.which("glslc")
    if found:
        return found
    ndk = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")
    candidates = []
    if ndk:
        candidates.append(Path(ndk))
    candidates += sorted((Path.home() / "Android/Sdk/ndk").glob("*"), reverse=True)
    for base in candidates:
        for tool in base.glob("shader-tools/*/glslc*"):
            return str(tool)
    sys.exit("glslc not found: install the Android NDK or the Vulkan SDK")


def main():
    lock = json.loads((ROOT / "config" / "parallel_gs.lock.json").read_text(encoding="utf-8"))
    checkout = ROOT / "work" / "dependencies" / f"parallel-gs-{lock['parallel-gs']['revision'][:12]}"
    shaders = checkout / "gs" / "shaders"
    header = subprocess.check_output(["git", "-C", str(checkout), "show", "HEAD:gs/shaders/slangmosh.hpp"], text=True)
    start = header.index("static const uint32_t spirv_bank[] =")
    body_start = header.index("{", start) + 1
    body_end = header.index("};", body_start)
    bank = [int(word, 16) for word in re.findall(r"0x([0-9a-fA-F]{8})u", header[body_start:body_end])]
    compiler = glslc()
    with tempfile.TemporaryDirectory() as temporary:
        for program, source, defines in PROGRAMS:
            out = Path(temporary) / "shader.spv"
            command = [compiler, "--target-env=vulkan1.1", "-O", "-I", str(checkout / "Granite/assets/shaders/inc"),
                       *[f"-D{name}={value}" for name, value in defines.items()], str(shaders / source), "-o", str(out)]
            subprocess.run(command, check=True)
            code = out.read_bytes()
            offset = len(bank)
            bank += [int.from_bytes(code[n:n + 4], "little") for n in range(0, len(code), 4)]
            pattern = re.compile(r"(this->" + re.escape(program) + r" = device\.request_program\(spirv_bank \+ )\d+, \d+(, &layout\);)")
            header, count = pattern.subn(lambda m: f"{m.group(1)}{offset}, {len(code)}{m.group(2)}", header)
            if count != 1:
                sys.exit(f"{program} not found once in slangmosh.hpp")
    lines = ["\t" + ", ".join(f"0x{word:08x}u" for word in bank[n:n + 8]) + "," for n in range(0, len(bank), 8)]
    header = header[:body_start] + "\n" + "\n".join(lines) + "\n" + header[body_end:]
    (shaders / "slangmosh.hpp").write_text(header, encoding="utf-8")
    print(f"slangmosh.hpp: {len(PROGRAMS)} shaders compiled again ({len(bank)} words in the bank)")


if __name__ == "__main__":
    main()
