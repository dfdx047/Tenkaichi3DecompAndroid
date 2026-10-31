import struct
import unittest
from generate_kernel import extract_payload, package_elf, leaf_extent


class KernelPackagingTests(unittest.TestCase):
    def test_runtime_mapping_preserves_bytes_and_separates_code(self):
        payload = bytes(range(64))
        elf = package_elf(payload, 0x80076000, 8)
        self.assertEqual(extract_payload(elf, 0x80076000, 64), (256, payload))
        self.assertEqual(struct.unpack_from('<I', elf, 24)[0], 0x80076000)
        text = struct.unpack_from('<10I', elf, 136)
        data = struct.unpack_from('<10I', elf, 216)
        self.assertEqual((text[2], text[3], text[5]), (6, 0x80076000, 8))
        self.assertEqual((data[2], data[3], data[5]), (2, 0x80076008, 56))

    def test_invalid_mapping_is_rejected(self):
        for payload, target, size in [(b'', 0, 4), (b'123', 0, 4), (b'1234', 1, 4),
                                      (b'1234', 0, 0), (b'1234', 0, 8), (b'12345678', 0xFFFFFFFC, 4)]:
            with self.subTest(target=target, size=size), self.assertRaises(ValueError):
                package_elf(payload, target, size)

    def test_non_file_backed_truncated_and_ambiguous_sources_rejected(self):
        elf = bytearray(package_elf(bytes(64), 0x1000, 8))
        for data, source, size in [(bytes(elf), 0x1040, 4), (bytes(elf[:-65]), 0x1000, 64),
                                   (b'notelf', 0x1000, 4), (bytes(elf), 0x1000, 0)]:
            with self.assertRaises(ValueError):
                extract_payload(data, source, size)
        struct.pack_into('<H', elf, 44, 2)
        elf[84:116] = elf[52:84]
        with self.assertRaises(ValueError):
            extract_payload(elf, 0x1000, 8)

    def test_leaf_closure_excludes_adjacent_functions(self):
        payload = struct.pack('<6I', 0x10800002, 0, 0, 0x03E00008, 0, 0x03E00008) + bytes(4)
        self.assertEqual(leaf_extent(payload, 0x80075000), 0x80075014)

    def test_leaf_escaping_or_indirect_flow_rejected(self):
        for words in [(0x10007FFF, 0), (0x08000000, 0), (0x03E00008,), (0x03E00008, 0x08000000)]:
            with self.assertRaises(ValueError):
                leaf_extent(struct.pack('<' + str(len(words)) + 'I', *words), 0x80075000)
