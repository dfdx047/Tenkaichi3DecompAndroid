import unittest
from probe_kernel import extract_kernel


class SnapshotTests(unittest.TestCase):
    def test_raw_capture_prefix(self):
        data = bytes([0x5A]) * (32 * 1024 * 1024)
        self.assertEqual(extract_kernel(data), data[:0x80004])

    def test_wrong_capture_format(self):
        for data in (b"", bytes(512 * 1024), bytes(4 * 1024 * 1024)):
            with self.assertRaises(ValueError):
                extract_kernel(data)
