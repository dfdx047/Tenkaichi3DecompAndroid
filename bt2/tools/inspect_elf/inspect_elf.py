"""Read ELF32 little-endian MIPS metadata without extracting code or assets."""

import argparse
import hashlib
import json
from pathlib import Path
import struct


def inspect(data: bytes) -> dict:
    def bounds(offset, size, label):
        if offset > len(data) or size > len(data) - offset:
            raise ValueError(f"{label} lies outside the file.")

    bounds(0, 52, "ELF header")
    (ident, kind, machine, version, entry, phoff, shoff, flags,
     ehsize, phsize, phnum, shsize, shnum, shstrndx) = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    if ident[:7] != b"\x7fELF\x01\x01\x01" or (kind, machine, version, ehsize) != (2, 8, 1, 52):
        raise ValueError("Expected an ELF32 little-endian MIPS executable with version 1 and header size 52.")
    if phnum == 0xFFFF or shstrndx == 0xFFFF or (shoff and shnum == 0):
        raise ValueError("Extended ELF numbering is not supported.")
    if phnum and (not phoff or phsize != 32):
        raise ValueError("Invalid program header offset or entry size.")
    if shnum and (not shoff or shsize != 40):
        raise ValueError("Invalid section header offset or entry size.")
    if (not phnum and phoff) or (not shnum and (shoff or shstrndx)):
        raise ValueError("Inconsistent absent header table.")
    bounds(phoff, phsize * phnum, "Program header table")
    bounds(shoff, shsize * shnum, "Section header table")
    programs, loads, warnings = [], [], []
    for index in range(phnum):
        ptype, offset, vaddr, paddr, filesz, memsz, pflags, align = struct.unpack_from("<8I", data, phoff + index * phsize)
        if ptype != 0:
            bounds(offset, filesz, f"Program {index} payload")
        if ptype == 1:
            if filesz > memsz or vaddr + memsz > 0x100000000:
                raise ValueError(f"Load segment {index} has invalid sizes or address range.")
            if align > 1 and (align & (align - 1) or vaddr % align != offset % align):
                warnings.append(f"Load segment {index} does not satisfy generic ELF alignment rules; file_size={filesz}, memory_size={memsz}. Its purpose is unknown.")
        record = dict(index=index, type=ptype, file_offset=offset, virtual_address=vaddr,
                      physical_address=paddr, file_size=filesz, memory_size=memsz,
                      flags=pflags, alignment=align)
        programs.append(record)
        if ptype == 1:
            loads.append({**record, "memory_end_exclusive": vaddr + memsz, "zero_fill_bytes": memsz - filesz})
    raw_sections = []
    for index in range(shnum):
        section = struct.unpack_from("<10I", data, shoff + index * shsize)
        name, stype, sflags, addr, offset, size, link, info, align, entsize = section
        if stype not in (0, 8):
            bounds(offset, size, f"Section {index} payload")
        if align > 1 and align & (align - 1):
            raise ValueError(f"Section {index} has invalid alignment.")
        if sflags & 2 and addr + size > 0x100000000:
            raise ValueError(f"Section {index} has an overflowing allocated address range.")
        raw_sections.append(section)
    names = b""
    if shstrndx:
        if shstrndx >= shnum or raw_sections[shstrndx][1] != 3:
            raise ValueError("Invalid section-name string table index/type.")
        name_section = raw_sections[shstrndx]
        names = data[name_section[4]:name_section[4] + name_section[5]]
        if not names or names[0] != 0 or names[-1] != 0:
            raise ValueError("Section-name string table is not NUL-delimited.")
    sections = []
    for index, section in enumerate(raw_sections):
        name, stype, sflags, addr, offset, size, link, info, align, entsize = section
        section_name = None
        if names:
            if name >= len(names):
                raise ValueError(f"Section {index} name offset is out of bounds.")
            section_name = names[name:names.index(0, name)].decode("utf-8", errors="replace")
        sections.append(dict(index=index, name=section_name, type=stype, flags=sflags, address=addr,
                             file_offset=offset, size=size, link=link, info=info,
                             alignment=align, entry_size=entsize, occupies_file_bytes=stype not in (0, 8)))
    entry_mapped = any(load["virtual_address"] <= entry < load["memory_end_exclusive"] and load["flags"] & 1 for load in loads)
    if not entry_mapped:
        warnings.append("Entry is not inside an executable load segment.")
    return dict(status="metadata inspection complete", size_bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                entry_point=entry, elf_flags=flags, program_header_offset=phoff, section_header_offset=shoff,
                program_headers=programs, load_segments=loads, sections=sections,
                entry_in_executable_load_segment=entry_mapped,
                warnings=warnings,
                scope="ELF metadata only; addresses are file declarations, not a confirmed PS2 hardware memory map. No instructions or functions analyzed.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    try:
        if args.executable.stat().st_size > 128 * 1024 * 1024:
            raise ValueError("Input exceeds the 128 MiB limit; supply an executable, not a disc image.")
        report = inspect(args.executable.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        print(json.dumps({"status": "inspection failed", "error": str(error)}, indent=2))
        return 1
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
