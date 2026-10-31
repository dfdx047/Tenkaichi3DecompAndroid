"""Generate only the observed copied-kernel entry; all derived output stays ignored."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import uuid
from generate_game import ROOT, LOCAL_CANDIDATE_SHA256
from build_tools import recompiler
from run_pipeline import verify_generated

SOURCE, TARGET, SIZE = 0x0036B328, 0x80076000, 0x740


def extract_payload(data, source=SOURCE, size=SIZE):
    if len(data) < 52 or data[:7] != b'\x7fELF\x01\x01\x01' or size <= 0:
        raise ValueError('Invalid ELF/payload size')
    table = struct.unpack_from('<I', data, 28)[0]
    stride, count = struct.unpack_from('<HH', data, 42)
    if stride != 32 or table + count * stride > len(data):
        raise ValueError('Invalid load table')
    matches = []
    for index in range(count):
        kind, offset, address, _, length, _, _, _ = struct.unpack_from('<8I', data, table + index * stride)
        if kind == 1 and address <= source and source + size <= address + length:
            begin = offset + source - address
            if begin + size > len(data):
                raise ValueError('Payload exceeds file')
            matches.append((begin, data[begin:begin + size]))
    if len(matches) != 1:
        raise ValueError('Payload must have exactly one file-backed load')
    return matches[0]


def package_elf(payload, target, code_size):
    if not payload or len(payload) % 4 or target % 4 or code_size % 4 or not 0 < code_size <= len(payload) or target + len(payload) > 2**32:
        raise ValueError('Invalid payload mapping')
    names = b'\0.text\0.shstrtab\0.data\0'
    text_offset = 256
    names_offset = text_offset + len(payload)
    result = bytearray(names_offset + len(names))
    result[text_offset:names_offset] = payload
    result[names_offset:] = names
    struct.pack_into('<16sHHIIIIIHHHHHH', result, 0,
                     b'\x7fELF\x01\x01\x01' + bytes(9), 2, 8, 1, target, 52, 96, 0, 52, 32, 1, 40, 4, 2)
    struct.pack_into('<8I', result, 52, 1, text_offset, target, target, len(payload), len(payload), 5, 16)
    struct.pack_into('<10I', result, 136, 1, 1, 6, target, text_offset, code_size, 0, 0, 4, 0)
    struct.pack_into('<10I', result, 176, 7, 3, 0, 0, names_offset, len(names), 0, 0, 1, 0)
    struct.pack_into('<10I', result, 216, 17, 1, 2, target + code_size, text_offset + code_size, len(payload) - code_size, 0, 0, 4, 0)
    return bytes(result)


def leaf_extent(payload, target):
    """Conservative direct-branch closure for an observed leaf entry, not symbol recovery."""
    pending, reached, returns = [target], set(), 0
    def word(address):
        offset = address - target
        if offset < 0 or offset % 4 or offset + 4 > len(payload):
            raise ValueError('Leaf flow escapes the copied payload')
        return struct.unpack_from('<I', payload, offset)[0]
    def control(instruction):
        op = instruction >> 26
        return op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (op == 0 and instruction & 63 in (8, 9)) or (op == 16 and (instruction >> 21) & 31 >= 8)
    while pending:
        address = pending.pop()
        if address in reached:
            continue
        instruction = word(address)
        reached.add(address)
        op = instruction >> 26
        is_return = instruction == 0x03E00008
        branch = op in (4, 5, 6, 7, 20, 21, 22, 23)
        if is_return or branch:
            delay = address + 4
            if control(word(delay)):
                raise ValueError('Control flow in leaf delay slot is unsupported')
            reached.add(delay)
            if is_return:
                returns += 1
            else:
                immediate = instruction & 0xFFFF
                if immediate & 0x8000:
                    immediate -= 0x10000
                pending.extend([address + 8, address + 4 + immediate * 4])
        elif control(instruction):
            raise ValueError('Leaf discovery requires direct conditional branches and JR RA only')
        else:
            pending.append(address + 4)
    if not returns or reached != set(range(target, max(reached) + 4, 4)):
        raise ValueError('Leaf closure is not contiguous or has no return')
    return max(reached) + 4


def generate(folder, output, executable, tool, function_map=None, target=TARGET):
    output.mkdir(parents=True)
    config = folder / 'config.toml'
    config.write_text('[general]\n' + '\n'.join(f'{key} = {json.dumps(path.resolve().as_posix())}'
        for key, path in [('input', executable), ('output', output)] + ([('ghidra_output', function_map)] if function_map else [])) +
        f'\nentry_points = ["0x{target:08X}"]\nsingle_file_output = false\nlow_memory_mode = true\noutput_worker_threads = 1\n'
        'patch_syscalls = false\npatch_cop0 = false\npatch_cache = false\nstubs = []\nskip = []\n', encoding='utf-8')
    with (folder / 'generation.log').open('w', encoding='utf-8') as log:
        subprocess.run([str(tool), str(config)], cwd=folder, stdout=log, stderr=subprocess.STDOUT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', choices=['timer3', 'startup'], default='timer3')
    args = parser.parse_args()
    source, target, size = (SOURCE, TARGET, SIZE) if args.payload == 'timer3' else (0x0036BAF8, 0x80075000, 0x330)
    suffix = '' if args.payload == 'timer3' else '-startup'
    header_prefix = 'bt2_kernel' if args.payload == 'timer3' else 'bt2_startup_kernel'
    verify_generated()
    data = (ROOT / 'rom/SLUS_214.41').read_bytes()
    if hashlib.sha256(data).hexdigest() != LOCAL_CANDIDATE_SHA256:
        raise ValueError('Retail source provenance changed')
    offset, payload = extract_payload(data, source, size)
    tool = recompiler()
    folder = ROOT / ('work/kernel-generation' + suffix)
    folder.mkdir(exist_ok=True)
    discovery = folder / ('discovery-' + uuid.uuid4().hex)
    discovery.mkdir()
    executable = discovery / 'payload.elf'
    executable.write_bytes(package_elf(payload, target, size))
    generate(discovery, discovery / 'generated', executable, tool, target=target)
    entry = discovery / f'generated/sub_{target:08X}_0x{target:x}.cpp'
    match = re.search(rf'// Address: 0x{target:x} - 0x([0-9a-fA-F]+)', entry.read_text(encoding='utf-8'))
    if not match:
        raise ValueError('Observed kernel entry was not discovered')
    end = int(match[1], 16)
    if not target < end <= target + size:
        raise ValueError('Provisional entry exceeds copied payload')
    discovered_end = end
    end = leaf_extent(payload, target)
    if end > discovered_end:
        raise ValueError('Leaf closure exceeds discovered function range')
    executable = folder / 'payload.elf'
    executable.write_bytes(package_elf(payload, target, end - target))
    function_map = folder / 'functions.csv'
    function_map.write_text(f'Name,Start,End,Size\nFUN_{target:08X},0x{target:08X},0x{end:08X},{end-target}\n', encoding='utf-8')
    output = ROOT / ('game/generated/bt2-kernel' + suffix)
    if output.exists():
        if output.is_symlink() or output.resolve().parent != (ROOT / 'game/generated').resolve():
            raise ValueError('Unexpected kernel output path')
        output.rename(folder / ('previous-output-' + uuid.uuid4().hex))
    generate(folder, output, executable, tool, function_map, target=target)
    log = (folder / 'generation.log').read_text(encoding='utf-8')
    if not re.search(r'Warnings: \d+, errors: 0', log) or 'decode failures: 0' not in log:
        raise ValueError('Scoped kernel generation has errors; inspect log')
    # Isolate generated headers from the main ELF headers, preserving raw originals.
    prepared = ROOT / ('game/generated/bt2-kernel-runtime' + suffix)
    prepared.mkdir(exist_ok=True)
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in output.iterdir() if p.suffix in ('.cpp', '.h')}
    prepared_files = {}
    for name in (f'FUN_{target:08X}_0x{target:x}.cpp', 'ps2_recompiled_functions.h', 'ps2_recompiled_stubs.h'):
        text = (output / name).read_text(encoding='utf-8').replace('ps2_recompiled_functions.h', header_prefix + '_functions.h').replace('ps2_recompiled_stubs.h', header_prefix + '_stubs.h').replace('PS2_RECOMPILED_FUNCTIONS_H', header_prefix.upper() + '_FUNCTIONS_H').replace('PS2_RECOMPILED_STUBS_H', header_prefix.upper() + '_STUBS_H')
        destination = prepared / name.replace('ps2_recompiled_functions.h', header_prefix + '_functions.h').replace('ps2_recompiled_stubs.h', header_prefix + '_stubs.h')
        destination.write_text(text, encoding='utf-8')
        prepared_files[destination.name] = hashlib.sha256(destination.read_bytes()).hexdigest()
    manifest = {'source_elf_sha256': LOCAL_CANDIDATE_SHA256, 'source_address': source, 'source_file_offset': offset,
        'runtime_address': target, 'copy_size': size, 'payload_sha256': hashlib.sha256(payload).hexdigest(),
        'entry_end': end, 'automatic_discovery_end': discovered_end, 'scope': 'contiguous direct-branch leaf closure', 'map_sha256': hashlib.sha256(function_map.read_bytes()).hexdigest(),
        'packaged_elf_sha256': hashlib.sha256(executable.read_bytes()).hexdigest(),
        'lock_sha256': hashlib.sha256((ROOT / 'config/ps2recomp.lock.json').read_bytes()).hexdigest(),
        'recompiler_sha256': hashlib.sha256(tool.read_bytes()).hexdigest(), 'files': files, 'prepared_files': prepared_files,
        'discovery_log': str(discovery / 'generation.log'),
        'limitations': 'Provisional automatic entry boundary only. Bytes preserved at observed runtime address; no instruction rewriting. Remaining payload is non-executable data in scoped generation; discovery of other entries is not validated.'}
    (folder / 'generation.manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Generated scoped kernel entry [0x{target:X},0x{end:X}); payload/code/maps remain ignored.')


if __name__ == '__main__':
    main()
