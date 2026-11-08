"""Synthetic metadata fixtures; no game files are required."""

import struct
import unittest

from inspect_elf import inspect


def fixture():
    data = bytearray(512)
    ident = b"\x7fELF\x01\x01\x01" + bytes(9)
    struct.pack_into("<16sHHIIIIIHHHHHH", data, 0,
                     ident, 2, 8, 1, 0x100008, 52, 128, 0, 52, 32, 1, 40, 3, 1)
    struct.pack_into("<8I", data, 52, 1, 256, 0x100000, 0x100000, 16, 32, 5, 16)
    names = b"\0.shstrtab\0.bss\0"
    data[300:300 + len(names)] = names
    struct.pack_into("<10I", data, 168, 1, 3, 0, 0, 300, len(names), 0, 0, 1, 0)
    # NOBITS has memory size but requires no corresponding file bytes.
    struct.pack_into("<10I", data, 208, 11, 8, 3, 0x100010, 512, 16, 0, 0, 16, 0)
    return data


class InspectionTests(unittest.TestCase):
    def test_load_and_nobits(self):
        report = inspect(fixture())
        self.assertEqual(report["entry_point"], 0x100008)
        self.assertTrue(report["entry_in_executable_load_segment"])
        self.assertEqual(report["load_segments"][0]["zero_fill_bytes"], 16)
        self.assertEqual(report["sections"][2]["name"], ".bss")
        self.assertFalse(report["sections"][2]["occupies_file_bytes"])
        self.assertEqual(report["warnings"], [])

    def test_truncated_header_and_tables(self):
        for length in (0, 51, 70, 200):
            with self.subTest(length=length), self.assertRaises(ValueError):
                inspect(fixture()[:length])

    def test_payload_out_of_bounds(self):
        data = fixture()
        struct.pack_into("<I", data, 56, 510)
        with self.assertRaises(ValueError):
            inspect(data)

    def test_file_size_exceeds_memory(self):
        data = fixture()
        struct.pack_into("<I", data, 72, 8)
        with self.assertRaises(ValueError):
            inspect(data)

    def test_bad_string_index(self):
        data = fixture()
        struct.pack_into("<I", data, 208, 99)
        with self.assertRaises(ValueError):
            inspect(data)

    def test_empty_segment_alignment_is_reported(self):
        data = fixture()
        struct.pack_into("<8I", data, 52, 1, 257, 0, 0x200000, 0, 0, 7, 4096)
        report = inspect(data)
        self.assertTrue(any("alignment" in warning for warning in report["warnings"]))
        self.assertFalse(report["entry_in_executable_load_segment"])

    def test_extended_numbering_is_explicitly_unsupported(self):
        data = fixture()
        struct.pack_into("<H", data, 44, 0xFFFF)
        with self.assertRaisesRegex(ValueError, "Extended"):
            inspect(data)


if __name__ == "__main__":
    unittest.main()
