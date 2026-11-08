"""Lists the source lines of a sample profile (BT2_SAMPLE_PROFILE), using the program's PDB.

Usage: profile_lines.py PROFILE EXECUTABLE [--top N] [--file PART]

Needs the executable linked with /DEBUG and the sampled code compiled with line information (the
Release probe's hot runtime files are). Samples without line information are summed as "[no line]".
`--file` keeps only lines whose source path contains PART. Windows only (dbghelp.dll).
"""
import argparse, collections, ctypes, os
from ctypes import wintypes
from pathlib import Path


class Line64(ctypes.Structure):
    _fields_ = [('SizeOfStruct', wintypes.DWORD), ('Key', ctypes.c_void_p), ('LineNumber', wintypes.DWORD),
                ('FileName', ctypes.c_char_p), ('Address', ctypes.c_uint64)]


def lines(profile, executable):
    """Returns (total samples, Counter by 'file:line')."""
    dbghelp = ctypes.WinDLL('dbghelp.dll'); process = ctypes.c_void_p(0x5A5A)
    dbghelp.SymSetOptions(0x10)  # Load lines.
    dbghelp.SymInitialize.argtypes = [ctypes.c_void_p, ctypes.c_char_p, wintypes.BOOL]
    dbghelp.SymLoadModuleEx.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint64, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD]
    dbghelp.SymLoadModuleEx.restype = ctypes.c_uint64
    dbghelp.SymGetLineFromAddr64.argtypes = [ctypes.c_void_p, ctypes.c_uint64, ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(Line64)]
    executable = Path(executable).resolve()
    if not dbghelp.SymInitialize(process, str(executable.parent).encode(), False): raise OSError('SymInitialize failed')
    base = dbghelp.SymLoadModuleEx(process, None, str(executable).encode(), None, 0x140000000, 0, None, 0)
    if not base: raise OSError('Cannot load symbols for ' + str(executable))
    rows = Path(profile).read_text().splitlines(); total = int(rows[0].split()[1]); counts = collections.Counter()
    for row in rows[1:]:
        name, offset, samples, _caller = row.rsplit(' ', 3)
        if name.lower() != executable.name.lower(): continue
        line = Line64(); line.SizeOfStruct = ctypes.sizeof(Line64); displacement = wintypes.DWORD()
        if dbghelp.SymGetLineFromAddr64(process, base + int(offset, 16), ctypes.byref(displacement), ctypes.byref(line)):
            counts[f'{os.path.basename(line.FileName.decode(errors="replace"))}:{line.LineNumber}'] += int(samples)
        else:
            counts['[no line]'] += int(samples)
    dbghelp.SymCleanup(process)
    return total, counts


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('profile'); p.add_argument('executable'); p.add_argument('--top', type=int, default=40); p.add_argument('--file')
    a = p.parse_args()
    total, counts = lines(a.profile, a.executable)
    print(f'{total} samples')
    for place, samples in counts.most_common():
        if a.file and a.file not in place: continue
        print(f'{samples / total * 100:6.2f}%  {samples:7d}  {place}')
        a.top -= 1
        if not a.top: break


if __name__ == '__main__':
    main()
