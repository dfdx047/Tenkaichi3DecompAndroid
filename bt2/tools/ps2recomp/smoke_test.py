"""Generate C++ from a self-authored three-instruction ELF, without game data."""

from pathlib import Path
import struct
import subprocess
from build_tools import recompiler

ROOT = Path(__file__).resolve().parents[2]


def main():
    folder = ROOT / "work" / "ps2recomp-smoke"
    folder.mkdir(parents=True, exist_ok=True)
    data = bytearray(512)
    ident = b"\x7fELF\x01\x01\x01" + bytes(9)
    struct.pack_into("<16sHHIIIIIHHHHHH", data, 0,
                     ident, 2, 8, 1, 0x100000, 52, 128, 0, 52, 32, 1, 40, 3, 2)
    struct.pack_into("<8I", data, 52, 1, 256, 0x100000, 0x100000, 12, 12, 5, 16)
    # Our own code: addiu v0, zero, 42; jr ra; nop.
    struct.pack_into("<3I", data, 256, 0x2402002A, 0x03E00008, 0)
    names = b"\0.text\0.shstrtab\0"
    data[300:300 + len(names)] = names
    struct.pack_into("<10I", data, 168, 1, 1, 6, 0x100000, 256, 12, 0, 0, 4, 0)
    struct.pack_into("<10I", data, 208, 7, 3, 0, 0, 300, len(names), 0, 0, 1, 0)
    executable = folder / "synthetic.elf"
    executable.write_bytes(data)
    function_map = folder / "functions.csv"
    function_map.write_text("name,start,end,size\nsynthetic_return42,0x00100000,0x0010000c,12\n", encoding="utf-8")
    output = folder / "generated"
    config = folder / "config.toml"
    config.write_text(
        f'[general]\ninput = "{executable.as_posix()}"\nghidra_output = "{function_map.as_posix()}"\n'
        f'output = "{output.as_posix()}"\nsingle_file_output = false\n'
        'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\nstubs = []\nskip = []\n',
        encoding="utf-8",
    )
    tool = recompiler(ROOT)
    subprocess.run([str(tool), str(config)], cwd=folder, check=True)
    generated = list(output.glob("*.cpp"))
    text = "\n".join(path.read_text(encoding="utf-8") for path in generated)
    if not generated or "synthetic_return42" not in text or "ADD32(GPR_U32(ctx, 0), 42)" not in text or "jumpTarget = GPR_U32(ctx, 31)" not in text:
        raise RuntimeError("Expected synthetic function translation was not generated.")
    print("Synthetic translation passed. Generated C++ has not been compiled or executed.")


if __name__ == "__main__":
    main()
