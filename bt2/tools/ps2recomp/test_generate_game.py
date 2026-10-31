import io
import unittest
from unittest.mock import Mock

from generate_game import validate_map


class FunctionMapTests(unittest.TestCase):
    metadata = {"sections": [{"flags": 6, "address": 0x100000, "size": 16}]}

    def read_map(self, text):
        path = Mock()
        path.open.return_value = io.StringIO(text)
        return validate_map(path, self.metadata)

    def test_actual_export_header_and_neutral_name(self):
        rows = self.read_map("Name,Start,End,Size\nFUN_00100000,0x100000,0x10000c,12\n")
        self.assertEqual(rows[0]["name"], "FUN_00100000")

    def test_unverified_names_and_bad_ranges_rejected(self):
        for row in (
            "BattleStart,0x100000,0x10000c,12",
            "FUN_00100004,0x100000,0x10000c,12",
            "FUN_00100000,0x100000,0x100020,32",
            "FUN_00100000,0x100000,0x10000c,8",
            "FUN_00100001,0x100001,0x10000d,12",
        ):
            with self.subTest(row=row), self.assertRaises(ValueError):
                self.read_map("Name,Start,End,Size\n" + row + "\n")

    def test_empty_and_duplicate_maps_rejected(self):
        for text in (
            "Name,Start,End,Size\n",
            "Name,Start,End,Size\n" + "FUN_00100000,0x100000,0x10000c,12\n" * 2,
        ):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.read_map(text)

    def test_missing_and_extra_csv_fields_rejected(self):
        for row in ("FUN_00100000,0x100000,0x10000c", "FUN_00100000,0x100000,0x10000c,12,extra"):
            with self.subTest(row=row), self.assertRaises(ValueError):
                self.read_map("Name,Start,End,Size\n" + row + "\n")

    def test_overlapping_ranges_rejected(self):
        with self.assertRaises(ValueError):
            self.read_map("Name,Start,End,Size\nFUN_00100000,0x100000,0x10000c,12\n"
                          "FUN_00100008,0x100008,0x100010,8\n")


if __name__ == "__main__":
    unittest.main()
