import struct
import unittest
import hashlib
import json
from pathlib import Path
import tempfile
from capture_ee import capture_trap, EXPECTED_SHA, MARKER, ROOT
from verify_rpc_bind import validate_state, verify_call_semaphore


class BindEvidenceTests(unittest.TestCase):
    def setUp(self):
        # Authored protocol fixture only; no retail bytes or captures.
        self.ee = bytearray(32 * 1024 * 1024)
        self.iop = bytearray(2 * 1024 * 1024)
        def put(data, address, *words):
            struct.pack_into('<' + str(len(words)) + 'I', data, address, *words)
        put(self.ee, 0x387E80, 0xB70F, 0x801, 0x1FFEC20)
        put(self.iop, 0x19600, 0, 0, 0x80000009, 0, 5, 0x203A97C0, 2, 0x3A87A8, 0x80000592)
        put(self.iop, 0x196C0, 0x19600, 0x19680, 0x3A94C0)
        put(self.ee, 0x3A94C0, 0, 0, 0x80000008, 0, 0, 0x203A97C0, 0,
            0x3A87A8, 0x80000009, 0x3F570, 0x3FB20)
        put(self.iop, 0x3F570, 0x80000592, 0x39534, 0x3FB20)
        put(self.iop, 0x3F570 + 64, 0x3F558)
        put(self.iop, 0x3F558 + 8, 0x3F570)
        put(self.ee, 0x3A87A8, 0, 2, 3, 0, 0, 0x3FB20, 0, 0, 0, 0x3F570)
        put(self.ee, 0x3A97C0, 64, 0, 0x80000009, 0, 4, 0x203A97C0, 0,
            0x3A87A8, 0x80000592)
        for command, callback in ((8, 0x3322E8), (9, 0x3326C0), (10, 0x3328E0), (12, 0x3323F8)):
            put(self.ee, 0x3A95C0 + command * 12, callback, 0x3AAFC0, 0x38BDF0)

    def test_consistent_protocol_state(self):
        validate_state(self.ee, self.iop)

    def test_wrong_reply_client_queue_and_packet_rejected(self):
        for data, address in ((self.ee, 0x3A94E4), (self.ee, 0x3A87A8),
                              (self.iop, 0x3F560), (self.iop, 0x19620),
                              (self.ee, 0x3A97D0), (self.ee, 0x3A9620)):
            with self.subTest(address=hex(address)):
                original = data[address]
                data[address] ^= 1
                with self.assertRaises(ValueError):
                    validate_state(self.ee, self.iop)
                data[address] = original

    def test_wrong_sizes_rejected(self):
        with self.assertRaises(ValueError):
            validate_state(self.ee[:-1], self.iop)

    def test_call_semaphore_provenance_and_descriptor(self):
        address, trap, marker = capture_trap('rpc-call-sema')
        self.ee[address:address + len(trap)] = trap
        struct.pack_into('<III', self.ee, MARKER, marker, 3, 0x1FFEBD0)
        struct.pack_into('<6I', self.ee, 0x1FFEBD0, 0, 1, 0, 0, 0, 0x3838E8)
        manifest = {'source_elf_sha256': EXPECTED_SHA,
                    'capture_point': 'rpc-call-sema: authored fixture',
                    'capture_sha256': hashlib.sha256(self.ee).hexdigest(),
                    'patch': {'address': address, 'size': len(trap),
                              'authored_words': list(struct.unpack('<8I', trap)),
                              'marker_address': MARKER, 'marker_value': marker}}
        (ROOT / 'work').mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(dir=ROOT / 'work') as folder:
            path = Path(folder) / 'capture.json'
            dump = path.with_name('ee-ram.bin')
            dump.write_bytes(self.ee)
            path.write_text(json.dumps(manifest), encoding='utf-8')
            self.assertEqual(struct.unpack('<7I', verify_call_semaphore(path)[0]),
                             (0, 1, 0, 0, 0, 0x3838E8, 3))
            manifest['capture_sha256'] = 'wrong'
            path.write_text(json.dumps(manifest), encoding='utf-8')
            with self.assertRaises(ValueError):
                verify_call_semaphore(path)
            self.ee[0x1FFEBD0 + 20] ^= 1
            manifest['capture_sha256'] = hashlib.sha256(self.ee).hexdigest()
            dump.write_bytes(self.ee)
            path.write_text(json.dumps(manifest), encoding='utf-8')
            with self.assertRaises(ValueError):
                verify_call_semaphore(path)


if __name__ == '__main__':
    unittest.main()
