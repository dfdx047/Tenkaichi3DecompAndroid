import struct
import unittest
from verify_cd_init import validate_state


class CdInitEvidenceTests(unittest.TestCase):
    def setUp(self):
        # Authored protocol fixture; no executable or capture bytes.
        self.ee = bytearray(32 * 1024 * 1024)
        self.iop = bytearray(2 * 1024 * 1024)
        def put(data, address, *words):
            struct.pack_into('<' + str(len(words)) + 'I', data, address, *words)
        request = [0x400, 0x3FB20, 0x8000000A, 0, 5, 0x203A97C0, 3, 0x3A87A8,
                   0, 4, 0x3695C0, 16, 1, 0x3F570, 0, 0]
        put(self.ee, 0x387E80, 0xB711, 0x902, 0x1FFEBD0)
        put(self.iop, 0x19600, *request)
        request[0], request[4], request[6] = 0x440, 4, 0
        put(self.ee, 0x3A97C0, *request)
        put(self.ee, 0x3A94C0, 0x1000, 0x3695C0, 0x80000008, 0, 0, 0, 0,
            0x3A87A8, 0x8000000A)
        put(self.ee, 0x3695C0, 1, 0, 0, 0)
        put(self.iop, 0x196C0, 0x19600, 0x19680, 0x3A94C0)
        put(self.iop, 0x3F570, 0x80000592, 0x39534, 0x3FB20, 4, 0, 0, 0,
            0x3A87A8, 0x203A97C0, 0, 0x3695C0, 16, 1, 0, 0, 0, 0x3F558)
        put(self.iop, 0x3F560, 0x3F570)
        put(self.ee, 0x3A87A8, 0, 3, 3)
        put(self.ee, 0x3A87BC, 0x3FB20)
        put(self.ee, 0x3A87CC, 0x3F570)

    def test_consistent_mode_zero_completion(self):
        validate_state(self.ee, self.iop)

    def test_wrong_mode_reply_client_or_server_rejected(self):
        for data, address in ((self.ee, 0x3A8800), (self.ee, 0x3695C0),
                              (self.ee, 0x3A94E0), (self.ee, 0x3A87C4),
                              (self.iop, 0x19628), (self.iop, 0x3F598),
                              (self.iop, 0x3F560)):
            with self.subTest(address=hex(address)):
                original = data[address]
                data[address] ^= 1
                with self.assertRaises(ValueError):
                    validate_state(self.ee, self.iop)
                data[address] = original

    def test_invalid_sizes_rejected(self):
        with self.assertRaises(ValueError):
            validate_state(self.ee, self.iop[:-1])
