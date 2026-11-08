"""Synthetic provenance tests; no game bytes or generated retail source required."""
import hashlib
import struct
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import run_pipeline
from build_tools import RECOMPILER


class GenerationProvenanceTests(unittest.TestCase):
    def setUp(self):
        fixture_parent = run_pipeline.ROOT / "work/test-provenance"
        fixture_parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=fixture_parent)
        self.root = Path(self.temp.name)
        if not self.root.resolve().is_relative_to(fixture_parent.resolve()):
            raise ValueError("Unexpected test cleanup location")
        self.addCleanup(self.temp.cleanup)
        files = {"rom/SLUS_214.41": b"authored fixture", "config/ps2recomp.lock.json": b"lock",
                 RECOMPILER: b"tool fixture",
                 "work/bt2-generation/candidates.csv": b"map fixture",
                 "game/generated/bt2/fixture.cpp": b"// authored source\n"}
        for name, contents in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(contents)
        digest = lambda name: hashlib.sha256(files[name]).hexdigest()
        manifest = {"executable_sha256": digest("rom/SLUS_214.41"),
                    "map_sha256": digest("work/bt2-generation/candidates.csv"),
                    "lock_sha256": digest("config/ps2recomp.lock.json"),
                    "recompiler_sha256": digest(RECOMPILER),
                    "files": {"fixture.cpp": digest("game/generated/bt2/fixture.cpp")}}
        (self.root / "work/bt2-generation/generation.manifest.json").write_text(json.dumps(manifest))
        self.root_patch = patch.object(run_pipeline, "ROOT", self.root)
        self.hash_patch = patch.object(run_pipeline, "LOCAL_CANDIDATE_SHA256", manifest["executable_sha256"])
        self.root_patch.start()
        self.hash_patch.start()
        self.addCleanup(self.root_patch.stop)
        self.addCleanup(self.hash_patch.stop)

    def test_matching_generation_accepted(self):
        run_pipeline.verify_generated()

    def test_modified_generated_source_rejected(self):
        (self.root / "game/generated/bt2/fixture.cpp").write_text("changed source")
        with self.assertRaises(ValueError):
            run_pipeline.verify_generated()

    def test_change_after_cached_verification_rejected(self):
        # The digest cache must not hide an edit made after a successful (cached) verification,
        # even one that keeps the file size.
        run_pipeline.verify_generated()
        path = self.root / "game/generated/bt2/fixture.cpp"
        original = path.read_bytes()
        path.write_bytes(bytes([original[0] ^ 1]) + original[1:])
        with self.assertRaises(ValueError):
            run_pipeline.verify_generated()

    def test_stale_extra_source_rejected(self):
        (self.root / "game/generated/bt2/stale.cpp").write_text("stale source")
        with self.assertRaises(ValueError):
            run_pipeline.verify_generated()

    def test_changed_lock_rejected(self):
        (self.root / "config/ps2recomp.lock.json").write_text("different lock")
        with self.assertRaises(ValueError):
            run_pipeline.verify_generated()


class SnapshotProvenanceTests(unittest.TestCase):
    def test_valid_and_modified_snapshot(self):
        folder = run_pipeline.ROOT / 'work/test-provenance'
        folder.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=folder) as temp:
            path = Path(temp) / 'kernel.bin'
            data = bytes(0x80004)
            path.write_bytes(data)
            manifest = {'snapshot_sha256': hashlib.sha256(data).hexdigest(), 'capture_note': 'authored fixture'}
            path.with_suffix('.json').write_text(json.dumps(manifest), encoding='utf-8')
            self.assertEqual(run_pipeline.verify_snapshot(path), manifest)
            path.write_bytes(bytes([1]) + data[1:])
            with self.assertRaises(ValueError):
                run_pipeline.verify_snapshot(path)

    def test_wrong_size_snapshot(self):
        folder = run_pipeline.ROOT / 'work/test-provenance'
        folder.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=folder) as temp:
            path = Path(temp) / 'kernel.bin'
            path.write_bytes(b'fixture')
            path.with_suffix('.json').write_text(json.dumps({'snapshot_sha256': hashlib.sha256(b'fixture').hexdigest(),
                                                          'capture_note': 'authored fixture'}), encoding='utf-8')
            with self.assertRaises(ValueError):
                run_pipeline.verify_snapshot(path)


class TimerCaptureTests(unittest.TestCase):
    def setUp(self):
        folder = run_pipeline.ROOT / 'work/test-provenance'
        folder.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=folder)
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'capture.json'
        self.data = bytearray(32 * 1024 * 1024)
        self.words = [0x3C181000, 0x37181810, 0x8F190000, 0x3C180038, 0x37187E80,
                      0xAF190004, 0x3419B703, 0xAF190000, 0x08000000 | ((0x338A1C + 32) >> 2), 0]
        self.data[0x338A1C:0x338A1C + 40] = struct.pack('<10I', *self.words)
        struct.pack_into('<II', self.data, 0x387E80, 0xB703, 0x83)
        self.manifest = {'source_elf_sha256': run_pipeline.LOCAL_CANDIDATE_SHA256,
                         'capture_point': 'timer3-mode: authored fixture',
                         'patch': {'address': 0x338A1C, 'size': 40, 'authored_words': self.words},
                         'observed_timer3_mode': 0x83, 'capture_sha256': hashlib.sha256(self.data).hexdigest()}
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()

    def save(self):
        self.path.write_text(json.dumps(self.manifest), encoding='utf-8')

    def test_dma_full_word_checkpoints_and_changed_marker(self):
        for channel, name, address, register, marker, field, value in [
            (False, 'dma-status', 0x3319CC, 0xE010, 0xB709, 'observed_dma_status', 0x800040),
            (True, 'dma5-control', 0x3319EC, 0xC000, 0xB70A, 'observed_dma5_control', 0x184)]:
            words = self.words.copy()
            words[1], words[6], words[8] = 0x37180000 | register, 0x34190000 | marker, 0x08000000 | ((address + 32) >> 2)
            self.data[address:address + 40] = struct.pack('<10I', *words)
            struct.pack_into('<II', self.data, 0x387E80, marker, value)
            self.manifest.update(capture_point=name + ': authored fixture',
                                 patch={'address': address, 'size': 40, 'authored_words': words},
                                 capture_sha256=hashlib.sha256(self.data).hexdigest())
            self.manifest[field] = value
            self.path.with_name('ee-ram.bin').write_bytes(self.data)
            self.save()
            self.assertEqual(run_pipeline.verify_dma_capture(self.path, channel)[0], value)
            struct.pack_into('<I', self.data, 0x387E80, marker + 1)
            self.manifest['capture_sha256'] = hashlib.sha256(self.data).hexdigest()
            self.path.with_name('ee-ram.bin').write_bytes(self.data)
            self.save()
            with self.assertRaises(ValueError):
                run_pipeline.verify_dma_capture(self.path, channel)

    def test_sif_rpc_query_capture(self):
        address, marker = 0x332110, 0xB70E
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF040008,
                 0x3419B70E, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
        self.data[address:address + 32] = struct.pack('<8I', *words)
        struct.pack_into('<III', self.data, 0x387E80, marker, 1, 0x80000002)
        self.manifest.update(capture_point='sif-rpc-state: fixture',
            patch=dict(address=address, size=32, authored_words=words, marker_address=0x387E80, marker_value=marker),
            capture_sha256=hashlib.sha256(self.data).hexdigest(), observed_sif_rpc_state=1, observed_sif_rpc_selector=0x80000002)
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        self.assertEqual(run_pipeline.verify_sif_capture(self.path, rpc=True)[0], 1)
        self.manifest['observed_sif_rpc_selector'] = 0x80000000
        self.save()
        with self.assertRaises(ValueError): run_pipeline.verify_sif_capture(self.path, rpc=True)

    def test_sif_return_capture_and_rejected_selector(self):
        address, marker = 0x331A34, 0xB70B
        words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF040008,
                 0x3419B70B, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
        self.data[address:address + 32] = struct.pack('<8I', *words)
        struct.pack_into('<III', self.data, 0x387E80, marker, 0x19600, 0x80000000)
        self.manifest.update(capture_point='sif-command: authored fixture',
            patch={'address': address, 'size': 32, 'authored_words': words,
                   'marker_address': 0x387E80, 'marker_value': marker},
            capture_sha256=hashlib.sha256(self.data).hexdigest(),
            observed_sif_command=0x19600, observed_sif_selector=0x80000000)
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        self.assertEqual(run_pipeline.verify_sif_capture(self.path)[0], 0x19600)
        self.manifest['observed_sif_selector'] = 0x80000001
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_sif_capture(self.path)
        self.manifest['observed_sif_selector'] = 0x80000000
        self.manifest['patch']['authored_words'][2] ^= 1
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_sif_capture(self.path)

    def test_valid_capture_and_changed_payload_rejected(self):
        self.assertEqual(run_pipeline.verify_timer_capture(self.path)[0], 0x83)
        self.manifest['observed_timer3_mode'] = 0x100
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer_capture(self.path)

    def test_wrong_source_and_ram_hash_rejected(self):
        self.manifest['source_elf_sha256'] = 'wrong'
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer_capture(self.path)
        self.manifest['source_elf_sha256'] = run_pipeline.LOCAL_CANDIDATE_SHA256
        self.manifest['capture_sha256'] = 'wrong'
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer_capture(self.path)

    def test_timer2_capture_and_forged_trap_rejected(self):
        address = 0x338C58
        words = self.words.copy()
        words[1], words[6], words[8] = 0x37181010, 0x3419B704, 0x08000000 | ((address + 32) >> 2)
        self.data[address:address + 40] = struct.pack('<10I', *words)
        struct.pack_into('<II', self.data, 0x387E80, 0xB704, 0x80)
        self.manifest.update(capture_point='timer2-mode: authored fixture', observed_timer2_mode=0x80,
                             patch={'address': address, 'size': 40, 'authored_words': words},
                             capture_sha256=hashlib.sha256(self.data).hexdigest())
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        self.assertEqual(run_pipeline.verify_timer_capture(self.path, 2)[0], 0x80)
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer_capture(self.path)
        words[1] = 0x37181810
        self.data[address:address + 40] = struct.pack('<10I', *words)
        self.manifest['capture_sha256'] = hashlib.sha256(self.data).hexdigest()
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer_capture(self.path, 2)

    def test_intc_checkpoint_validation(self):
        address = 0x32E9A4
        words = self.words.copy()
        words[1], words[6], words[8] = 0x3718F010, 0x3419B705, 0x08000000 | ((address + 32) >> 2)
        self.data[address:address + 40] = struct.pack('<10I', *words)
        struct.pack_into('<II', self.data, 0x387E80, 0xB705, 0x1002)
        self.manifest.update(capture_point='intc-mask: authored fixture', observed_intc_mask=0x1002,
                             patch={'address': address, 'size': 40, 'authored_words': words},
                             capture_sha256=hashlib.sha256(self.data).hexdigest())
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        self.assertEqual(run_pipeline.verify_intc_capture(self.path)[0], 0x1002)
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer_capture(self.path, 2)
        self.manifest['observed_intc_mask'] = 0x1802
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_intc_capture(self.path)

    def test_readback_capture_is_separate_from_initial_mode(self):
        address = 0x338DD8
        words = self.words.copy()
        words[1], words[6], words[8] = 0x37181010, 0x3419B706, 0x08000000 | ((address + 32) >> 2)
        self.data[address:address + 40] = struct.pack('<10I', *words)
        struct.pack_into('<II', self.data, 0x387E80, 0xB706, 0x382)
        self.manifest.update(capture_point='timer2-readback: authored fixture', observed_timer2_readback=0x382,
                             patch={'address': address, 'size': 40, 'authored_words': words},
                             capture_sha256=hashlib.sha256(self.data).hexdigest())
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        self.assertEqual(run_pipeline.verify_timer2_readback_capture(self.path)[0], 0x382)
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer_capture(self.path, 2)
        self.manifest['patch']['address'] = 0x338C58
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_timer2_readback_capture(self.path)

    def test_osd_full_word_checkpoint_and_tamper_rejection(self):
        address, marker, word = 0x3385F4, 0xB707, 0x21C12010
        words = [0x8FB90000, 0x3C180038, 0x37187E80, 0xAF190004,
                 0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
        self.data[address:address + 32] = struct.pack('<8I', *words)
        struct.pack_into('<II', self.data, 0x387E80, marker, word)
        self.manifest.update(capture_point='osd-config: authored fixture', observed_osd_config=word,
                             patch={'address': address, 'size': 32, 'authored_words': words,
                                    'marker_address': 0x387E80, 'marker_value': marker},
                             capture_sha256=hashlib.sha256(self.data).hexdigest())
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        self.assertEqual(run_pipeline.verify_osd_capture(self.path)[0], word)
        for key, value in [('source_elf_sha256', 'wrong'), ('capture_sha256', 'wrong'),
                           ('observed_osd_config', word ^ 1), ('capture_point', 'timer2-mode: fixture')]:
            previous = self.manifest[key]
            self.manifest[key] = value
            self.save()
            with self.assertRaises(ValueError):
                run_pipeline.verify_osd_capture(self.path)
            self.manifest[key] = previous
        self.manifest['patch']['authored_words'][0] ^= 1
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_osd_capture(self.path)

    def test_osd_readback_requires_same_value_and_exact_trap(self):
        address, marker, word = 0x338620, 0xB708, 0x21C12010
        words = [0x3C180038, 0x37187E80, 0x8FB90004, 0xAF190004,
                 0x8FB90000, 0xAF190008, 0x34190000 | marker, 0xAF190000,
                 0x08000000 | ((address + 32) >> 2), 0]
        self.data[address:address + 40] = struct.pack('<10I', *words)
        struct.pack_into('<III', self.data, 0x387E80, marker, word, word)
        self.manifest.update(capture_point='osd-readback: authored fixture', observed_osd_readback=word,
                             original_osd_config=word,
                             patch={'address': address, 'size': 40, 'authored_words': words,
                                    'marker_address': 0x387E80, 'marker_value': marker},
                             capture_sha256=hashlib.sha256(self.data).hexdigest())
        self.path.with_name('ee-ram.bin').write_bytes(self.data)
        self.save()
        self.assertEqual(run_pipeline.verify_osd_readback_capture(self.path)[0], word)
        self.manifest['observed_osd_readback'] ^= 1
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_osd_readback_capture(self.path)
        self.manifest['observed_osd_readback'] = word
        self.manifest['patch']['authored_words'][2] ^= 4
        self.save()
        with self.assertRaises(ValueError):
            run_pipeline.verify_osd_readback_capture(self.path)




class SifDmaCaptureTests(unittest.TestCase):
    def test_receive_pair_rejects_changed_return_effect_and_hash(self):
        folder = run_pipeline.ROOT / 'work/test-provenance'
        folder.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=folder) as temp:
            paths = []
            for after, address, marker in ((False, 0x331D44, 0xB70C), (True, 0x331D4C, 0xB70D)):
                path = Path(temp) / ('after' if after else 'before') / 'capture.json'
                path.parent.mkdir()
                ee = bytearray(32 * 1024 * 1024)
                iop = bytearray(2 * 1024 * 1024)
                words = [0x3C180038, 0x37187E80, 0xAF020004, 0xAF1D0008,
                         0x34190000 | marker, 0xAF190000, 0x08000000 | ((address + 24) >> 2), 0]
                ee[address:address + 32] = struct.pack('<8I', *words)
                result = 0x701 if after else 0
                struct.pack_into('<III', ee, 0x387E80, marker, result, 0x1FFEC70)
                struct.pack_into('<4I', ee, 0x1FFEC70, 0x3A9580, 0x19600, 20, 0x44)
                struct.pack_into('<5I', ee, 0x3A9580, 20, 0, 0x80000000, 0, 0x3A94C0)
                struct.pack_into('<3I', iop, 0x196C0, 0x19600, 0x19680, 0x3A94C0 if after else 0x93640)
                if after: iop[0x19600:0x19614] = bytes(4) + ee[0x3A9584:0x3A9594]
                m = dict(source_elf_sha256=run_pipeline.LOCAL_CANDIDATE_SHA256,
                    capture_point=('sif-dma-after' if after else 'sif-dma-before') + ': fixture',
                    patch=dict(address=address, size=32, authored_words=words, marker_address=0x387E80, marker_value=marker),
                    capture_sha256=hashlib.sha256(ee).hexdigest(), iop_capture_sha256=hashlib.sha256(iop).hexdigest(),
                    observed_sif_dma_result=result, observed_sif_dma_sp=0x1FFEC70, pcsx2_sha256='fixture-pcsx2', bios_sha256='fixture-bios')
                path.write_text(json.dumps(m), encoding='utf-8')
                path.with_name('ee-ram.bin').write_bytes(ee)
                path.with_name('iop-ram.bin').write_bytes(iop)
                paths.append(path)
            blob, _ = run_pipeline.verify_sif_dma_pair(*paths)
            self.assertEqual(len(blob), 2 * 1024 * 1024 + 4)
            for field, value in (('observed_sif_dma_result', 0x702), ('iop_capture_sha256', 'wrong')):
                changed = dict(m); changed[field] = value
                paths[1].write_text(json.dumps(changed), encoding='utf-8')
                with self.assertRaises(ValueError): run_pipeline.verify_sif_dma_pair(*paths)
            iop[0x196C8] ^= 1
            m['iop_capture_sha256'] = hashlib.sha256(iop).hexdigest()
            paths[1].write_text(json.dumps(m), encoding='utf-8')
            paths[1].with_name('iop-ram.bin').write_bytes(iop)
            with self.assertRaises(ValueError): run_pipeline.verify_sif_dma_pair(*paths)

if __name__ == "__main__":
    unittest.main()
