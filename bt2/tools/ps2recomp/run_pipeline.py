"""Automated provisional map -> generation -> probe build/run -> integration diagnostics.

Probes execute a real BT2 routine and entry up to a checked unsupported boundary.
"""

import argparse
import hashlib
import os
import json
from pathlib import Path
import subprocess
import sys
import struct
from build_tools import recompiler
from generate_game import ROOT, LOCAL_CANDIDATE_SHA256


def run(command, log=None, check=True):
    if log is None:
        return subprocess.run(command, cwd=ROOT, check=check)
    with log.open("w", encoding="utf-8") as stream:
        return subprocess.run(command, cwd=ROOT, check=check, stdout=stream, stderr=subprocess.STDOUT)


def cached_digests(folder, names, cache_path):
    """SHA-256 of each named file, re-hashing only files whose size or modification time changed since
    the cached digest (the same change test incremental builds use). Returns {name: digest}."""
    try:
        cache = json.loads(cache_path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        cache = {}
    stats = {entry.name: entry.stat() for entry in os.scandir(folder) if entry.name in names}
    digests, changed = {}, False
    for name in names:
        stat = stats.get(name)
        if stat is None:
            raise ValueError(f"Generated source missing: {name}")
        key = [stat.st_size, stat.st_mtime_ns]
        entry = cache.get(name)
        if not entry or entry[:2] != key:
            entry = key + [hashlib.sha256((folder / name).read_bytes()).hexdigest()]
            cache[name] = entry;changed = True
        digests[name] = entry[2]
    if changed:
        cache_path.write_text(json.dumps(cache), encoding="utf-8")
    return digests


def verify_generated():
    folder = ROOT / "work/bt2-generation"
    manifest = json.loads((folder / "generation.manifest.json").read_text(encoding="utf-8"))
    if (manifest["executable_sha256"] != hashlib.sha256((ROOT / "rom/SLUS_214.41").read_bytes()).hexdigest()
            or manifest["executable_sha256"] != LOCAL_CANDIDATE_SHA256
            or manifest["map_sha256"] != hashlib.sha256((folder / "candidates.csv").read_bytes()).hexdigest()
            or manifest["lock_sha256"] != hashlib.sha256((ROOT / "config/ps2recomp.lock.json").read_bytes()).hexdigest()
            or manifest["recompiler_sha256"] != hashlib.sha256(recompiler(ROOT).read_bytes()).hexdigest()):
        raise ValueError("Generation provenance differs; rerun generation.")
    generated = ROOT / "game/generated/bt2"
    actual = {entry.name for entry in os.scandir(generated) if entry.is_file() and Path(entry.name).suffix in (".cpp", ".h")}
    if actual != set(manifest["files"]):
        raise ValueError("Generated source file set changed.")
    if any(Path(name).name != name for name in manifest["files"]):
        raise ValueError("Generated source manifest names a path.")
    digests = cached_digests(generated, list(manifest["files"]), folder / "generated-digests.cache.json")
    for name, expected in manifest["files"].items():
        if digests[name] != expected:
            raise ValueError(f"Generated source changed: {name}")


def verify_snapshot(path):
    data = path.read_bytes()
    manifest = json.loads(path.with_suffix('.json').read_text(encoding='utf-8'))
    if len(data) != 0x80004 or hashlib.sha256(data).hexdigest() != manifest['snapshot_sha256'] or not manifest['capture_note']:
        raise ValueError('Kernel snapshot size/hash/capture provenance differs.')
    return manifest


def verify_timer_capture(path, timer=3):
    if timer not in (2, 3):
        raise ValueError("Unsupported timer capture.")
    address, marker_value, register = (0x00338A1C, 0xB703, 0x1810) if timer == 3 else (0x00338C58, 0xB704, 0x1010)
    return verify_word_capture(path, f'timer{timer}-mode', f'observed_timer{timer}_mode', address, marker_value, register)


def verify_intc_capture(path):
    return verify_word_capture(path, 'intc-mask', 'observed_intc_mask', 0x0032E9A4, 0xB705, 0xF010)


def verify_timer2_readback_capture(path):
    return verify_word_capture(path, 'timer2-readback', 'observed_timer2_readback', 0x00338DD8, 0xB706, 0x1010)


def verify_word_capture(path, point, field, address, marker_value, register, max_word=0xFFFF):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith(point + ':')
            or manifest['patch']['address'] != address or manifest['patch']['size'] != 40):
        raise ValueError(f'{point} capture point/source differs.')
    data = path.with_name('ee-ram.bin').read_bytes()
    if len(data) != 32 * 1024 * 1024 or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']:
        raise ValueError('Timer capture RAM hash/size differs.')
    mode, = struct.unpack_from('<I', data, 0x00387E84)
    marker, = struct.unpack_from('<I', data, 0x00387E80)
    trap = struct.pack('<10I', *manifest['patch']['authored_words'])
    expected_trap = struct.pack('<10I', 0x3C181000, 0x37180000 | register, 0x8F190000, 0x3C180038, 0x37187E80,
                                0xAF190004, 0x34190000 | marker_value, 0xAF190000, 0x08000000 | ((address + 32) >> 2), 0)
    if (trap != expected_trap or mode != manifest[field] or mode > max_word or marker != marker_value
            or data[address:address + 40] != trap):
        raise ValueError('Timer capture payload/instrumentation differs.')
    return mode, manifest


def verify_dma_capture(path, channel=False):
    point, address, marker, register, field = (
        ('dma5-control', 0x003319EC, 0xB70A, 0xC000, 'observed_dma5_control') if channel else
        ('dma-status', 0x003319CC, 0xB709, 0xE010, 'observed_dma_status'))
    return verify_word_capture(path, point, field, address, marker, register, max_word=0xFFFFFFFF)


def verify_sif_capture(path, rpc=False):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    data = path.with_name('ee-ram.bin').read_bytes()
    address, marker, point = (0x332110, 0xB70E, 'sif-rpc-state') if rpc else (0x331A34, 0xB70B, 'sif-command')
    words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF040008,
             0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
    trap = struct.pack('<8I', *words)
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith(point + ':')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != words or patch['marker_address'] != 0x387E80
            or patch['marker_value'] != marker or len(data) != 32 * 1024 * 1024
            or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']
            or data[address:address + len(trap)] != trap):
        raise ValueError('SIF capture provenance/instrumentation differs.')
    actual_marker, word, selector = struct.unpack_from('<III', data, 0x387E80)
    if (actual_marker != marker or word != manifest['observed_sif_rpc_state' if rpc else 'observed_sif_command']
            or selector != manifest['observed_sif_rpc_selector' if rpc else 'observed_sif_selector'] or selector != (0x80000002 if rpc else 0x80000000)):
        raise ValueError('SIF capture payload/selector differs.')
    return word, manifest


def verify_sif_dma_pair(before_path, after_path):
    captures = []
    for path, point, address, marker in (
            (before_path, 'sif-dma-before', 0x331D44, 0xB70C),
            (after_path, 'sif-dma-after', 0x331D4C, 0xB70D)):
        m = json.loads(path.read_text(encoding='utf-8'))
        ee = path.with_name('ee-ram.bin').read_bytes()
        iop = path.with_name('iop-ram.bin').read_bytes()
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF1D0008,
                 0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
        patch = m['patch']
        if (m['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
                or not m['capture_point'].startswith(point + ':')
                or patch['address'] != address or patch['size'] != 32
                or patch['authored_words'] != words or patch['marker_address'] != 0x387E80
                or patch['marker_value'] != marker or len(ee) != 32 * 1024 * 1024
                or len(iop) != 2 * 1024 * 1024
                or hashlib.sha256(ee).hexdigest() != m['capture_sha256']
                or hashlib.sha256(iop).hexdigest() != m['iop_capture_sha256']
                or ee[address:address + 32] != struct.pack('<8I', *words)):
            raise ValueError('SIF DMA capture provenance/instrumentation differs.')
        actual_marker, result, sp = struct.unpack_from('<III', ee, 0x387E80)
        if (actual_marker != marker or result != m['observed_sif_dma_result']
                or sp != m['observed_sif_dma_sp'] or sp != 0x1FFEC70
                or struct.unpack_from('<4I', ee, sp) != (0x3A9580, 0x19600, 20, 0x44)
                or struct.unpack_from('<5I', ee, 0x3A9580) != (20, 0, 0x80000000, 0, 0x3A94C0)
                or struct.unpack_from('<2I', iop, 0x196C0) != (0x19600, 0x19680)):
            raise ValueError('SIF DMA descriptor/packet/table differs.')
        captures.append((m, ee, iop, result))
    before, after = captures
    if (before[0]['pcsx2_sha256'] != after[0]['pcsx2_sha256']
            or before[0]['bios_sha256'] != after[0]['bios_sha256']
            or after[3] != 0x701
            or struct.unpack_from('<I', before[2], 0x196C8)[0] != 0x93640
            or struct.unpack_from('<I', after[2], 0x196C8)[0] != 0x3A94C0
            or after[2][0x19600:0x19614] != bytes(4) + after[1][0x3A9584:0x3A9594]):
        raise ValueError('SIF DMA receive effects/return differ.')
    return before[2] + struct.pack('<I', after[3]), {'before': before[0], 'after': after[0]}


def verify_osd_capture(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    data = path.with_name('ee-ram.bin').read_bytes()
    address, marker = 0x003385F4, 0xB707
    words = [0x8FB90000, 0x3C180038, 0x37187E80, 0xAF190004,
             0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
    trap = struct.pack('<8I', *words)
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith('osd-config:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != words
            or patch['marker_address'] != 0x387E80 or patch['marker_value'] != marker
            or len(data) != 32 * 1024 * 1024
            or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']
            or data[address:address + len(trap)] != trap):
        raise ValueError('OSD capture provenance/instrumentation differs.')
    actual_marker, word = struct.unpack_from('<II', data, 0x387E80)
    if actual_marker != marker or word != manifest['observed_osd_config']:
        raise ValueError('OSD capture payload differs.')
    return word, manifest


def verify_osd_readback_capture(path):
    manifest = json.loads(path.read_text(encoding='utf-8'))
    data = path.with_name('ee-ram.bin').read_bytes()
    address, marker = 0x00338620, 0xB708
    words = [0x3C180038, 0x37187E80, 0x8FB90004, 0xAF190004,
             0x8FB90000, 0xAF190008, 0x34190000 | marker, 0xAF190000,
             0x08000000 | ((address + 32) >> 2), 0]
    trap = struct.pack('<10I', *words)
    patch = manifest['patch']
    if (manifest['source_elf_sha256'] != LOCAL_CANDIDATE_SHA256
            or not manifest['capture_point'].startswith('osd-readback:')
            or patch['address'] != address or patch['size'] != len(trap)
            or patch['authored_words'] != words
            or patch['marker_address'] != 0x387E80 or patch['marker_value'] != marker
            or len(data) != 32 * 1024 * 1024
            or hashlib.sha256(data).hexdigest() != manifest['capture_sha256']
            or data[address:address + len(trap)] != trap):
        raise ValueError('OSD readback provenance/instrumentation differs.')
    actual_marker, readback, original = struct.unpack_from('<III', data, 0x387E80)
    if (actual_marker != marker or readback != manifest['observed_osd_readback']
            or original != manifest['original_osd_config']
            or readback != original or ((original & 0xFFFF1FFF) | 0x2000) != original):
        raise ValueError('OSD readback does not validate the observed same-value write.')
    return readback, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reuse-generated", action="store_true", help="Verify provenance and reuse existing generated output.")
    parser.add_argument('--kernel-snapshot', type=Path, help='Explicit local snapshot with hash/capture-note sidecar; enables the next startup boundary.')
    parser.add_argument('--timer3-capture', type=Path, help='Explicit capture.json from the timer3-mode capture; replays one observed MMIO read.')
    parser.add_argument('--timer2-capture', type=Path, help='Explicit timer2-mode capture; requires kernel snapshot and Timer 3 capture.')
    parser.add_argument('--intc-capture', type=Path, help='Explicit intc-mask checkpoint; requires Timer 2 capture.')
    parser.add_argument('--timer2-readback-capture', type=Path, help='Explicit validation of the post-write mode read; requires INTC checkpoint.')
    parser.add_argument('--osd-capture', type=Path, help='First OSD config output checkpoint; requires Timer 2 readback.')
    parser.add_argument('--osd-readback-capture', type=Path, help='Validate first same-value OSD setter/readback; requires OSD capture.')
    parser.add_argument('--dma-status-capture', type=Path, help='Explicit first D_STAT checkpoint; requires OSD readback.')
    parser.add_argument('--dma5-control-capture', type=Path, help='Explicit first channel 5 CHCR checkpoint; requires D_STAT.')
    parser.add_argument('--sif-command-capture', type=Path, help='Explicit first SifGetReg return checkpoint; requires channel 5 capture.')
    parser.add_argument('--sif-dma-before-capture', type=Path, help='First SIF submission EE/IOP capture; requires matching after capture.')
    parser.add_argument('--sif-dma-after-capture', type=Path, help='First SIF return/receive capture; requires first-query capture.')
    parser.add_argument('--sif-rpc-state-capture', type=Path, help='Explicit reached RPCINIT query result; requires first DMA receive captures.')
    parser.add_argument('--sif-bind-capture', type=Path, help='Checked first CD initialization bind reply; requires RPC state capture.')
    parser.add_argument('--rpc-call-sema-capture', type=Path, help='Checked first RPC call semaphore result; requires bind response capture.')
    parser.add_argument('--cd-init-capture', type=Path, help='Checked first mode-0 CD initialization completion; requires bind/call-semaphore inputs.')
    parser.add_argument('--cd-semaphores-capture', type=Path, help='Checked four CD semaphore results; requires CD-init completion.')
    parser.add_argument('--scmd-bind-capture', type=Path, help='Checked second CD service bind; requires CD semaphore checkpoint.')
    parser.add_argument('--scmd-mmode-capture', type=Path, help='Checked mode-2 media-mode call completion; requires SCMD bind.')
    parser.add_argument('--sif-stop-capture', type=Path, help='Checked reached SIF2 stop completion; requires media-mode evidence.')
    parser.add_argument('--sif-reset-subaddr-capture', type=Path, help='Checked post-stop SUBADDR; requires SIF stop.')
    parser.add_argument('--reset-smflag-before-capture', type=Path)
    parser.add_argument('--reset-smflag-after-capture', type=Path)
    parser.add_argument('--reset-submission-captures', type=Path, help='Mounted-disc reboot submission/clear capture folder.')
    parser.add_argument('--reset-ready-capture',type=Path,help='Disc-backed ready-branch bit checkpoint; requires submission chain.')
    parser.add_argument('--gs-csr-capture',type=Path,help='Explicit GS reset/readback checkpoint; requires ready checkpoint.')
    parser.add_argument('--gs-imr-capture',type=Path)
    parser.add_argument('--gs-crt-capture',type=Path)
    parser.add_argument('--gs-interrupt-captures',type=Path)
    parser.add_argument('--gs-csr-field-capture',type=Path)
    parser.add_argument('--gif-captures',type=Path)
    parser.add_argument('--gif-sync-captures',type=Path)
    parser.add_argument('--post-sif-captures',type=Path)
    parser.add_argument('--post-cd-captures',type=Path)
    parser.add_argument('--loadfile-captures',type=Path)
    parser.add_argument('--heap-bind-capture',type=Path,help='Guarded IOP heap bind capture directory; requires LOADFILE evidence and explicit local disc.')
    parser.add_argument('--texture-startup-captures',action='store_true',help='Verify local texture GIF startup captures and append checked evidence; requires heap copy.')
    parser.add_argument('--heap-copy-capture',type=Path,help='Guarded first heap transfer/completion capture; requires allocation evidence.')
    parser.add_argument('--heap-allocation-capture',type=Path,help='Guarded first heap allocation capture; requires heap bind evidence.')
    parser.add_argument('--heap-disc',type=Path,help='User-local disc for heap-service/IOPRP identity validation.')
    args = parser.parse_args()
    if args.sif_rpc_state_capture and not args.sif_dma_after_capture:
        parser.error('--sif-rpc-state-capture requires --sif-dma-after-capture')
    sif_rpc = verify_sif_capture(args.sif_rpc_state_capture, rpc=True) if args.sif_rpc_state_capture else None
    rpc_call_sema = None
    if args.rpc_call_sema_capture and not args.sif_bind_capture:
        parser.error('--rpc-call-sema-capture requires --sif-bind-capture')
    sif_bind = None
    if args.sif_bind_capture:
        if not sif_rpc:
            parser.error('--sif-bind-capture requires --sif-rpc-state-capture')
        sys.path.insert(0, str(ROOT / 'tools/capture_ee'))
        from verify_rpc_bind import verify as verify_bind, verify_call_semaphore
        bind_ee, bind_manifest = verify_bind(args.sif_bind_capture)
        if any(bind_manifest[key] != sif_rpc[1][key] for key in ('pcsx2_sha256', 'bios_sha256')):
            raise ValueError('RPC bind and query reference environment differ.')
        bind_iop = args.sif_bind_capture.with_name('iop-ram.bin').read_bytes()
        bundle = bind_iop + bind_ee[0x3A94C0:0x3A9500] + struct.pack('<I', 0x801)
        if args.rpc_call_sema_capture:
            rpc_call_sema = verify_call_semaphore(args.rpc_call_sema_capture)
            if any(bind_manifest[key] != rpc_call_sema[1][key] for key in ('pcsx2_sha256', 'bios_sha256')):
                raise ValueError('RPC bind and call semaphore environment differ.')
            bundle += rpc_call_sema[0]
        sif_bind = bundle, bind_manifest
    cd_init = None
    cd_semaphores = None
    if args.cd_semaphores_capture and not args.cd_init_capture:
        parser.error('--cd-semaphores-capture requires --cd-init-capture')
    if args.cd_init_capture:
        if not sif_bind or not rpc_call_sema:
            parser.error('--cd-init-capture requires bind and RPC call-semaphore captures')
        from verify_cd_init import verify as verify_cd_init, verify_semaphores
        cd_init = verify_cd_init(args.cd_init_capture)
        if any(cd_init[1][key] != sif_bind[1][key] for key in ('pcsx2_sha256', 'bios_sha256')):
            raise ValueError('CD-init and bind reference environments differ.')
        if args.cd_semaphores_capture:
            cd_semaphores = verify_semaphores(args.cd_semaphores_capture)
            if any(cd_semaphores[1][key] != cd_init[1][key] for key in ('pcsx2_sha256', 'bios_sha256')):
                raise ValueError('CD-init and semaphore reference environments differ.')
            cd_init = cd_init[0] + cd_semaphores[0], cd_init[1]

    scmd_bind = None
    if args.scmd_bind_capture:
        if not cd_semaphores:
            parser.error('--scmd-bind-capture requires --cd-semaphores-capture')
        from verify_scmd_bind import verify as verify_scmd_bind
        scmd_bind = verify_scmd_bind(args.scmd_bind_capture)
        if any(scmd_bind[1][key] != cd_init[1][key] for key in ('pcsx2_sha256','bios_sha256')):
            raise ValueError('SCMD bind reference environment differs.')
    scmd_mmode = None
    if args.scmd_mmode_capture:
        if not scmd_bind: parser.error('--scmd-mmode-capture requires --scmd-bind-capture')
        from verify_scmd_bind import verify_mmode
        scmd_mmode = verify_mmode(args.scmd_mmode_capture)
        if any(scmd_mmode[1][key] != scmd_bind[1][key] for key in ('pcsx2_sha256','bios_sha256')):
            raise ValueError('Media-mode reference environment differs.')
        scmd_bind = scmd_bind[0]+scmd_mmode[0], scmd_bind[1]
    sif_stop = None
    sif_reset_subaddr = None
    for path,point in ((args.sif_stop_capture,'sif-stop-after'),(args.sif_reset_subaddr_capture,'sif-reset-subaddr')):
        if not path: continue
        if not scmd_mmode or (point == 'sif-reset-subaddr' and not sif_stop):
            parser.error('SIF stop/query requires media-mode and ordered stop evidence')
        from verify_sif_reset import verify as verify_sif_reset
        value = verify_sif_reset(path,point)
        if any(value[1][key] != scmd_bind[1][key] for key in ('pcsx2_sha256','bios_sha256')):
            raise ValueError('SIF stop/query environment differs.')
        scmd_bind = scmd_bind[0]+value[0],scmd_bind[1]
        if point == 'sif-stop-after': sif_stop = value
        else: sif_reset_subaddr = value
    reset_smflag = None
    if bool(args.reset_smflag_before_capture) != bool(args.reset_smflag_after_capture):
        parser.error('Reset SMFLAG requires before and after captures')
    if args.reset_smflag_before_capture:
        if not sif_reset_subaddr: parser.error('Reset SMFLAG requires post-stop SUBADDR capture')
        from verify_sif_reset import verify_smflag_pair
        reset_smflag=verify_smflag_pair(args.reset_smflag_before_capture,args.reset_smflag_after_capture)
        if any(reset_smflag[1]['before'][key] != scmd_bind[1][key] for key in ('pcsx2_sha256','bios_sha256')):
            raise ValueError('Reset SMFLAG environment differs.')
        scmd_bind=scmd_bind[0]+reset_smflag[0],scmd_bind[1]
    image_report=None
    reset_submission=None
    if args.reset_submission_captures:
        if not reset_smflag:parser.error('Reboot submission requires reset SMFLAG evidence')
        sys.path.insert(0,str(ROOT/'tools/iop'))
        from inspect_image import inspect as inspect_iop, disc_image
        local_image=ROOT/'rom/IRX/IOPRP300.IMG'
        iso=ROOT/'rom/Dragon Ball Z - Budokai Tenkaichi 2 (USA) (En,Ja).iso'
        image=local_image.read_bytes()
        if disc_image(iso)!=image:raise ValueError('Local IOP image differs from disc.')
        image_report=inspect_iop(image)
        with iso.open('rb') as stream:image_report['disc_sha256']=hashlib.file_digest(stream,'sha256').hexdigest()
        from verify_sif_reset import verify_submission_chain
        reset_submission=verify_submission_chain(args.reset_submission_captures,image_report)
        first=reset_submission[1]['reset-submit-after']
        if any(first[key]!=scmd_bind[1][key] for key in ('pcsx2_sha256','bios_sha256')):raise ValueError('Reboot environment differs.')
        scmd_bind=scmd_bind[0]+reset_submission[0],scmd_bind[1]
    reset_ready=None
    if args.reset_ready_capture:
        if not reset_submission:parser.error('Ready checkpoint requires reboot submission chain')
        from verify_sif_reset import verify_ready
        reset_ready=verify_ready(args.reset_ready_capture,reset_submission[1])
        scmd_bind=scmd_bind[0]+reset_ready[0],scmd_bind[1]
    gs_csr=None
    if args.gs_csr_capture:
        if not reset_ready:parser.error('GS checkpoint requires ready evidence')
        from verify_gs import verify as verify_gs
        gs_csr=verify_gs(args.gs_csr_capture,reset_ready[1])
        scmd_bind=scmd_bind[0]+gs_csr[0],scmd_bind[1]
    gs_imr=gs_crt=None
    if args.gs_imr_capture:
        if not gs_csr:parser.error('GS IMR requires CSR checkpoint')
        gs_imr=verify_gs(args.gs_imr_capture,gs_csr[1],'gs-imr-after')
        scmd_bind=scmd_bind[0]+gs_imr[0],scmd_bind[1]
    if args.gs_crt_capture:
        if not gs_imr:parser.error('GS CRT requires IMR checkpoint')
        gs_crt=verify_gs(args.gs_crt_capture,gs_imr[1],'gs-crt-after')
        scmd_bind=scmd_bind[0]+gs_crt[0],scmd_bind[1]
    gs_interrupt=gs_field=None
    if args.gs_interrupt_captures:
        if not gs_crt:parser.error('INTC checkpoints require CRT evidence')
        from verify_gs import verify_interrupts
        gs_interrupt=verify_interrupts(args.gs_interrupt_captures,gs_crt[1])
        scmd_bind=scmd_bind[0]+gs_interrupt[0],scmd_bind[1]
    if args.gs_csr_field_capture:
        if not gs_interrupt:parser.error('CSR field requires INTC checkpoints')
        gs_field=verify_gs(args.gs_csr_field_capture,gs_crt[1],'gs-csr-field')
        scmd_bind=scmd_bind[0]+gs_field[0],scmd_bind[1]
    post_cd=None
    loadfile=None
    post_sif=None
    gif_sync=None
    gif=None
    if args.gif_captures:
        if not gs_field:parser.error('GIF checkpoints require the CSR field checkpoint')
        from verify_gif import verify as verify_gif
        gif=verify_gif(args.gif_captures,gs_field[1])
        scmd_bind=scmd_bind[0]+gif[0],scmd_bind[1]
    if args.gif_sync_captures:
        if not gif:parser.error('GIF synchronization requires first packet evidence')
        from verify_gif import verify_sync
        gif_sync=verify_sync(args.gif_sync_captures,gs_field[1])
        scmd_bind=scmd_bind[0]+gif_sync[0],scmd_bind[1]
    if args.post_sif_captures:
        if not gif_sync:parser.error('Post-reset SIF requires the GS synchronization chain')
        from verify_post_sif import verify as verify_post_sif
        post_sif=verify_post_sif(args.post_sif_captures,gs_field[1])
        scmd_bind=scmd_bind[0]+post_sif[0],scmd_bind[1]
    if args.post_cd_captures:
        if not post_sif:parser.error('Fresh CD services require post-reset SIF evidence')
        from verify_post_cd import verify as verify_post_cd,verify_scmd
        post_cd=verify_post_cd(args.post_cd_captures,gs_field[1])
        scmd=verify_scmd(args.post_cd_captures,gs_field[1])
        post_cd=(post_cd[0]+scmd[0],{**post_cd[1],"scmd":scmd[1]})
        scmd_bind=scmd_bind[0]+post_cd[0],scmd_bind[1]
    if args.loadfile_captures:
        if not args.post_cd_captures:raise ValueError('LOADFILE requires fresh CD/SCMD evidence')
        from verify_loadfile import verify as verify_loadfile
        loadfile=verify_loadfile(args.loadfile_captures,gs_field[1])
        scmd_bind=scmd_bind[0]+loadfile[0],scmd_bind[1]
    if bool(args.sif_dma_before_capture) != bool(args.sif_dma_after_capture) or (args.sif_dma_after_capture and not args.sif_command_capture):
        parser.error('SIF DMA captures require both before/after and --sif-command-capture')
    sif_dma = verify_sif_dma_pair(args.sif_dma_before_capture, args.sif_dma_after_capture) if args.sif_dma_after_capture else None
    if sif_dma and sif_rpc and any(sif_dma[1]['after'][key] != sif_rpc[1][key] for key in ('pcsx2_sha256', 'bios_sha256')):
        raise ValueError('SIF RPC query and DMA reference environment differ.')
    if args.sif_command_capture and not args.dma5_control_capture:
        parser.error('--sif-command-capture requires --dma5-control-capture')
    sif_command = verify_sif_capture(args.sif_command_capture) if args.sif_command_capture else None
    if args.dma_status_capture and not args.osd_readback_capture:
        parser.error('--dma-status-capture requires --osd-readback-capture')
    if args.dma5_control_capture and not args.dma_status_capture:
        parser.error('--dma5-control-capture requires --dma-status-capture')
    dma_status = verify_dma_capture(args.dma_status_capture) if args.dma_status_capture else None
    dma5_control = verify_dma_capture(args.dma5_control_capture, channel=True) if args.dma5_control_capture else None
    if args.osd_readback_capture and not args.osd_capture:
        parser.error('--osd-readback-capture requires --osd-capture')
    osd_readback = verify_osd_readback_capture(args.osd_readback_capture) if args.osd_readback_capture else None
    if args.osd_capture and not args.timer2_readback_capture:
        parser.error('--osd-capture requires --timer2-readback-capture')
    osd_capture = verify_osd_capture(args.osd_capture) if args.osd_capture else None
    if osd_readback and osd_capture and osd_readback[0] != osd_capture[0]:
        parser.error('OSD captures differ in their original configuration')
    if args.timer2_readback_capture and not args.intc_capture:
        parser.error('--timer2-readback-capture requires --intc-capture')
    readback_capture = verify_timer2_readback_capture(args.timer2_readback_capture) if args.timer2_readback_capture else None
    if args.intc_capture and not args.timer2_capture:
        parser.error('--intc-capture requires --timer2-capture')
    intc_capture = verify_intc_capture(args.intc_capture) if args.intc_capture else None
    if args.timer2_capture and not args.timer3_capture:
        parser.error('--timer2-capture requires --timer3-capture')
    timer2_capture = verify_timer_capture(args.timer2_capture, 2) if args.timer2_capture else None
    if args.timer3_capture and not args.kernel_snapshot:
        parser.error('--timer3-capture requires --kernel-snapshot')
    timer_capture = verify_timer_capture(args.timer3_capture) if args.timer3_capture else None
    snapshot_provenance = verify_snapshot(args.kernel_snapshot) if args.kernel_snapshot else None
    folder = ROOT / "work/bt2-generation"
    heap_allocation_bundle=None
    heap_bundle=None
    if args.heap_bind_capture:
        if not loadfile or not args.heap_disc:raise ValueError('Heap bind requires LOADFILE captures and --heap-disc')
        from verify_heap_bind import verify as verify_heap_bind
        heap_bundle=folder/'heap-bind.bin'
        verify_heap_bind(ROOT,args.heap_bind_capture,args.heap_disc,heap_bundle)
        heap_environment=json.loads((args.heap_bind_capture/'capture.json').read_text(encoding='utf-8'))
        if any(heap_environment[k]!=gs_field[1][k] for k in ('pcsx2_sha256','bios_sha256','disc_sha256')):
            raise ValueError('Heap bind environment differs from startup')
    folder.mkdir(parents=True, exist_ok=True)
    if not args.reuse_generated:
        run([sys.executable, str(ROOT / "tools/ps2recomp/prepare_map.py")])
        run([sys.executable, str(ROOT / "tools/ps2recomp/generate_game.py"), "--candidate-map", str(folder / "candidates.csv")])
    verify_generated()
    from expand_startup import verify_extra
    verify_extra()
    run([sys.executable, str(ROOT / "tools/ps2recomp/prepare_runtime.py")])
    if args.heap_allocation_capture:
        if not heap_bundle:raise ValueError('Heap allocation requires heap bind evidence')
        from verify_heap_allocation import verify as verify_heap_allocation
        heap_allocation_bundle=folder/'heap-allocation.bin'
        verify_heap_allocation(ROOT,args.heap_allocation_capture,args.heap_disc,heap_allocation_bundle)
    if args.heap_copy_capture:
        if not heap_allocation_bundle:raise ValueError('Heap copy requires allocation evidence')
        from verify_heap_copy import verify as verify_heap_copy
        verify_heap_copy(ROOT,args.heap_copy_capture,args.heap_disc,heap_allocation_bundle)
    texture_bundle=None
    if args.texture_startup_captures:
        if not args.heap_copy_capture:raise ValueError('Texture startup requires heap copy evidence')
        from verify_texture_startup import verify as verify_texture
        texture_bundle=folder/'texture-startup.bin'
        verify_texture(ROOT,texture_bundle)
    kernel_generation = None
    if timer_capture:
        run([sys.executable, str(ROOT / "tools/ps2recomp/generate_kernel.py")])
        kernel_generation = json.loads((ROOT / "work/kernel-generation/generation.manifest.json").read_text(encoding="utf-8"))
    startup_kernel_generation = None
    if osd_readback:
        run([sys.executable, str(ROOT / "tools/ps2recomp/generate_kernel.py"), "--payload", "startup"])
        startup_kernel_generation = json.loads((ROOT / "work/kernel-generation-startup/generation.manifest.json").read_text(encoding="utf-8"))
    build = ROOT / "build/probe"
    # The probe builds with Ninja inside the MSVC environment (tools/build/msvc.py).
    msvc = [sys.executable, str(ROOT / "tools/build/msvc.py")]
    run(msvc + ["cmake", "-S", str(ROOT), "-B", str(build), "-G", "Ninja Multi-Config", "-DBT2_BUILD_GENERATED_PROBE=ON", "-DBT2_BUILD_KERNEL_PAYLOAD=" + ("ON" if timer_capture else "OFF"), "-DBT2_BUILD_STARTUP_KERNEL_PAYLOAD=" + ("ON" if osd_readback else "OFF")], folder / "configure.log")
    run(msvc + ["cmake", "--build", str(build), "--config", "Debug"], folder / "probe-build.log")
    run(["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure", "-R", "generated-bt2-probe|bootstrap-runtime|generated-kernel-handler|generated-buffer-fill|generated-timer-write|generated-copy-handler"], folder / "probe-test.log")
    run([str(build / "Debug/bt2-generated-probe.exe")])
    result = run(msvc + ["cmake", "--build", str(build), "--config", "Debug", "--target", "bt2-entry-probe"],
                 folder / "integration-build.log", check=False)
    entry_stop = None
    if result.returncode == 0:
        run(["ctest", "--test-dir", str(build), "-C", "Debug", "--output-on-failure", "-R", "^generated-entry-probe$"], folder / "entry-test.log")
        entry_command = [str(build / "Debug/bt2-entry-probe.exe"), str(folder / "guest-memory.bin"), str(folder / "entry-stop.json")]
        if args.kernel_snapshot:
            entry_command.append(str(args.kernel_snapshot.resolve()))
        if timer_capture:
            timer_word = folder / 'timer3-mode.bin'
            timer_word.write_bytes(struct.pack('<I', timer_capture[0]))
            entry_command.append(str(timer_word))
        if timer2_capture:
            timer2_word = folder / 'timer2-mode.bin'
            timer2_word.write_bytes(struct.pack('<I', timer2_capture[0]))
            entry_command.append(str(timer2_word))
        if intc_capture:
            intc_word = folder / 'intc-mask.bin'
            intc_word.write_bytes(struct.pack('<I', intc_capture[0]))
            entry_command.append(str(intc_word))
        if readback_capture:
            readback_word = folder / 'timer2-readback.bin'
            readback_word.write_bytes(struct.pack('<I', readback_capture[0]))
            entry_command.append(str(readback_word))
        if osd_capture:
            osd_word = folder / 'osd-config.bin'
            osd_word.write_bytes(struct.pack('<I', osd_capture[0]))
            entry_command.append(str(osd_word))
        if osd_readback:
            osd_readback_word = folder / 'osd-readback.bin'
            osd_readback_word.write_bytes(struct.pack('<I', osd_readback[0]))
            entry_command.append(str(osd_readback_word))
        for checkpoint, name in ((dma_status, 'dma-status'), (dma5_control, 'dma5-control'), (sif_command, 'sif-command')):
            if checkpoint:
                word_path = folder / (name + '.bin')
                word_path.write_bytes(struct.pack('<I', checkpoint[0]))
                entry_command.append(str(word_path))
        if sif_dma:
            bundle = folder / 'sif-dma.bin'
            bundle.write_bytes(sif_dma[0])
            entry_command.append(str(bundle))
        if sif_rpc:
            word_path = folder / 'sif-rpc-state.bin'
            word_path.write_bytes(struct.pack('<I', sif_rpc[0]))
            entry_command.append(str(word_path))
        if sif_bind:
            bundle = folder / 'sif-bind.bin'
            bundle.write_bytes(sif_bind[0])
            entry_command.append(str(bundle))
        if cd_init:
            bundle = folder / 'cd-init.bin'
            bundle.write_bytes(cd_init[0])
            entry_command.append(str(bundle))
        if scmd_bind:
            bundle = folder / 'scmd-bind.bin'
            bundle.write_bytes(scmd_bind[0])
            entry_command.append(str(bundle))
        if heap_bundle:entry_command.append(str(heap_bundle))
        if heap_allocation_bundle:entry_command.append(str(heap_allocation_bundle))
        if texture_bundle:entry_command.append(str(texture_bundle))
        run(entry_command)
        entry_stop = json.loads((folder / "entry-stop.json").read_text(encoding="utf-8"))
    (folder / "pipeline-result.json").write_text(json.dumps({"probe_build_and_execution": "passed",
        "heap_bind_provenance": json.loads(heap_bundle.with_suffix('.json').read_text(encoding='utf-8')) if heap_bundle else None,
        "heap_allocation_provenance": json.loads(heap_allocation_bundle.with_suffix('.json').read_text(encoding='utf-8')) if heap_allocation_bundle else None,
        "gif_provenance": gif[1] if gif else None,
        "gif_sync_provenance": gif_sync[1] if gif_sync else None,
        "post_cd_provenance": post_cd[1] if post_cd else None,
        "loadfile_provenance": loadfile[1] if loadfile else None,
        "post_sif_provenance": post_sif[1] if post_sif else None,
        "gs_interrupt_provenance": gs_interrupt[1] if gs_interrupt else None,
        "gs_field_provenance": gs_field[1] if gs_field else None,
        "gs_imr_provenance": gs_imr[1] if gs_imr else None,
        "gs_crt_provenance": gs_crt[1] if gs_crt else None,
        "gs_csr_provenance": gs_csr[1] if gs_csr else None,
        "reset_ready_provenance": reset_ready[1] if reset_ready else None,
        "iop_image_provenance": image_report,
        "reset_submission_provenance": reset_submission[1] if reset_submission else None,
        "reset_smflag_provenance": reset_smflag[1] if reset_smflag else None,
        "sif_stop_provenance": sif_stop[1] if sif_stop else None,
        "sif_reset_subaddr_provenance": sif_reset_subaddr[1] if sif_reset_subaddr else None,
        "scmd_mmode_provenance": scmd_mmode[1] if scmd_mmode else None,
        "scmd_bind_provenance": scmd_bind[1] if scmd_bind else None,
        "cd_semaphores_provenance": cd_semaphores[1] if cd_semaphores else None,
        "cd_init_provenance": cd_init[1] if cd_init else None,
        "rpc_call_semaphore_provenance": rpc_call_sema[1] if rpc_call_sema else None,
        "sif_bind_provenance": sif_bind[1] if sif_bind else None,
        "sif_rpc_provenance": sif_rpc[1] if sif_rpc else None,
        "sif_dma_provenance": sif_dma[1] if sif_dma else None,
        "sif_command_provenance": sif_command[1] if sif_command else None,
        "dma_status_provenance": dma_status[1] if dma_status else None,
        "dma5_control_provenance": dma5_control[1] if dma5_control else None,
        "osd_readback_provenance": osd_readback[1] if osd_readback else None,
        "osd_capture_provenance": osd_capture[1] if osd_capture else None,
        "startup_kernel_generation_provenance": startup_kernel_generation,
        "kernel_generation_provenance": kernel_generation,
        "timer2_readback_capture_provenance": readback_capture[1] if readback_capture else None,
        "intc_capture_provenance": intc_capture[1] if intc_capture else None,
        "timer2_capture_provenance": timer2_capture[1] if timer2_capture else None,
        "timer3_capture_provenance": timer_capture[1] if timer_capture else None, "kernel_snapshot_provenance": snapshot_provenance, "entry_build_exit_code": result.returncode, "entry_stop": entry_stop, "game_booted": False,
        "integration_log": str(folder / "integration-build.log")}, indent=2) + "\n", encoding="utf-8")
    print(f"Probes verified. Entry build exit code: {result.returncode}; inspect {folder / 'integration-build.log'}.")
    if result.returncode:
        print("Boot integration remains blocked; this diagnostic failure is recorded separately from the passing probe.")
    elif entry_stop:
        run([sys.executable, str(ROOT / "tools/ps2recomp/inspect_blocker.py")])
        print(f"Entry stop: {entry_stop['status']}; game boot remains blocked. See {folder / 'entry-stop.json'}.")


if __name__ == "__main__":
    main()
