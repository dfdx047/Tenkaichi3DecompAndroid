"""Synthetic checks for branch arithmetic and entry coverage rejection."""

import hashlib
import unittest
from review_entry import audit, control


class EntryAuditTests(unittest.TestCase):
    def test_signed_branch_and_jump_region(self):
        self.assertEqual(control(0x1000FFFE, 0x100010)["target"], 0x10000C)
        self.assertEqual(control(0x08000001, 0x8FFFFFFC)["target"], 0x90000004)

    def test_indirect_and_syscall(self):
        self.assertEqual(control(0x0200F809, 0)["register"], 16)
        self.assertEqual(control(12, 0)["code"], 0)
        self.assertEqual(control(0x04000000, 0)["kind"], "bltz")
        self.assertTrue(control(0x04030000, 0)["branch_likely"])
        self.assertTrue(control(0x5400FFFE, 0x100010)["branch_likely"])
        with self.assertRaises(ValueError):
            control(0x45000000, 0)  # Coprocessor branch is unsupported
        self.assertIsNone(control(0x04180000, 0))  # R5900 MTSA family, not a branch

    def test_terminal_delay_slot_and_mismatch(self):
        data = bytes.fromhex("8000040800000000")  # j 0x00100200; nop (authored fixture)
        metadata = {"entry_point": 0x100000, "sections": [
            {"flags": 4, "occupies_file_bytes": True, "address": 0x100000,
             "size": 8, "file_offset": 0}]}
        report = {"language": "r5900:LE:32:default", "executable_sha256": hashlib.sha256(data).hexdigest(),
                  "functions": [{"name": "FUN_00100000", "start": 0x100000,
                                 "end_exclusive": 0x100008, "body_size": 8, "contiguous_aligned": True}],
                  "entry_instructions": [
                      {"address": "00100000", "flows": ["00100200"], "flow_type": "UNCONDITIONAL_JUMP"},
                      {"address": "00100004", "flows": [], "flow_type": "FALL_THROUGH"}]}
        self.assertEqual(audit(data, metadata, report)["instruction_count"], 2)
        return_data = bytes.fromhex("0800e00300000000")  # jr ra; nop
        report["executable_sha256"] = hashlib.sha256(return_data).hexdigest()
        report["entry_instructions"][0]["flows"] = []
        self.assertEqual(audit(return_data, metadata, report, allow_return=True)["instruction_count"], 2)
        with self.assertRaises(ValueError):
            audit(return_data, metadata, report)
        report["executable_sha256"] = hashlib.sha256(data).hexdigest()
        report["entry_instructions"][0]["flows"] = ["00100300"]
        with self.assertRaises(ValueError):
            audit(data, metadata, report)
        report["entry_instructions"].pop()
        with self.assertRaises(ValueError):
            audit(data, metadata, report)


if __name__ == "__main__":
    unittest.main()
