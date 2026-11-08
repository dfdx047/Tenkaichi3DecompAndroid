"""The BT3 renderer's shaders (runtime/gs/hw/bt3/shaders) as SPIR-V and as source text, in the header its copy
includes (shaders.h), made as BT3's own build makes it (Tenkaichi3DecompAndroid port/tools/android.py, shaders()).

    python tools/graphics/hw/make_bt3_shaders.py <output folder>
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SHADERS = ROOT / "runtime" / "gs" / "hw" / "bt3" / "shaders"


def glslc():
    found = shutil.which("glslc")
    if found:
        return found
    ndk = os.environ.get("ANDROID_NDK_HOME") or os.environ.get("ANDROID_NDK_ROOT")
    bases = ([Path(ndk)] if ndk else []) + sorted((Path.home() / "Android/Sdk/ndk").glob("*"), reverse=True)
    for base in bases:
        for tool in base.glob("shader-tools/*/glslc*"):
            return str(tool)
    sys.exit("glslc not found: install the Android NDK or the Vulkan SDK")


def array(name, data):
    return f"static const unsigned char {name}[] = {{" + ",".join(str(b) for b in data) + "};\n"


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    compiler = glslc()
    # gsnz.frag: gs.frag without its depth output (NO_FRAG_DEPTH), for the draws that do not need it
    nz = out / "gsnz.frag"
    nz.write_text((SHADERS / "gs.frag").read_text().replace("\n", "\n#define NO_FRAG_DEPTH\n", 1))
    sources = sorted(p for p in SHADERS.glob("*.*") if p.is_file()) + [nz]
    arrays = []
    spv = out / "shader.spv"
    for src in sources:
        stage = src.suffix[1:]
        r = subprocess.run([compiler, f"-fshader-stage={stage}", str(src), "-o", str(spv)], capture_output=True, text=True)
        if r.returncode:
            print("shader not compiled:", src.name, r.stderr[:300], file=sys.stderr)
            continue
        arrays.append(array("k" + src.stem.capitalize() + stage.capitalize() + "Spv", spv.read_bytes()))
    # gs.frag and gsnz.frag a second time without their discards (the draws that never discard)
    for src, name in ((SHADERS / "gs.frag", "kGsNdFragSpv"), (nz, "kGsnzNdFragSpv")):
        r = subprocess.run([compiler, "-fshader-stage=frag", "-DGS_NO_DISCARD=1", str(src), "-o", str(spv)], capture_output=True, text=True)
        if r.returncode:
            sys.exit(f"shader not compiled: {name}: {r.stderr[:300]}")
        arrays.append(array(name, spv.read_bytes()))
    for src in sources:
        if src.suffix in (".vert", ".frag"):
            arrays.append(array("k" + src.stem.capitalize() + src.suffix[1:].capitalize() + "Glsl", src.read_bytes() + b"\0"))
    spv.unlink(missing_ok=True)
    (out / "shaders.h").write_text("/* generated from runtime/gs/hw/bt3/shaders by tools/graphics/hw/make_bt3_shaders.py */\n" + "".join(arrays))


if __name__ == "__main__":
    main()
