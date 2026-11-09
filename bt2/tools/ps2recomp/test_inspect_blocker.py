import unittest
from inspect_blocker import classify, stop_address


class BlockerTests(unittest.TestCase):
    def test_entry_interior_and_exclusive_end(self):
        rows = [{"start": 0x100, "end": 0x120}]
        self.assertEqual(classify(0x100, rows, [], [])["map_status"], "mapped entry")
        self.assertEqual(classify(0x104, rows, [], [])["map_status"], "inside mapped function")
        self.assertEqual(classify(0x104, rows, [], [])["neutral_name"], "FUN_00000100")
        self.assertEqual(classify(0x120, rows, [], [])["map_status"], "map gap")

    def test_quarantine_evidence_is_retained(self):
        candidates = [{"start": 0x100, "end_exclusive": 0x120}]
        excluded = [{"start": 0x100, "end": 0x120, "reason": "noncontiguous_or_invalid"}]
        result = classify(0x104, [], candidates, excluded)
        self.assertEqual(result["map_status"], "map gap")
        self.assertEqual(result["analysis_candidates"], candidates)
        self.assertEqual(result["exclusions"], excluded)

    def test_copied_kernel_target_is_separate_from_elf_map(self):
        result = classify(0x80076000, [{"start": 0x36B328, "end": 0x36BA68}], [], [])
        self.assertEqual(result["map_status"], "map gap")
        self.assertTrue(result["requires_kernel_payload_mapping"])
        self.assertEqual(result["mapped_ranges"], [])
        self.assertFalse(classify(0x36B328, [], [], [])["requires_kernel_payload_mapping"])


    def test_registration_diagnostic_targets_handler_not_wrapper(self):
        stop = {'status': 'blocked at unsupported guest memory', 'access': 'handler registration',
                'handler': 0x80075038, 'pc': 0x33A818}
        self.assertEqual(stop_address(stop), 0x80075038)
        stop['access'] = 'write'
        self.assertEqual(stop_address(stop), 0x33A818)
        stop.update(status='blocked at unsupported syscall', access='syscall')
        self.assertEqual(stop_address(stop), 0x80075038)

    def test_disconnected_body_gap_is_not_analysis_ownership(self):
        candidate={'start':0x100,'end_exclusive':0x208,'body_ranges':['00000100-00000107','00000200-00000207']}
        excluded={'start':0x100,'end':0x208,'reason':'noncontiguous_or_invalid'}
        result=classify(0x150,[],[candidate],[excluded])
        self.assertEqual(result['analysis_candidates'],[])
        self.assertEqual(result['bounding_gap_candidates'],[candidate])
        self.assertEqual(result['exclusions'],[excluded])
        self.assertEqual(result['map_status'],'map gap')
        self.assertEqual(result['neutral_name'],'FUN_00000150')
        for address in [0x100,0x107,0x200,0x207]:
            result=classify(address,[],[candidate],[excluded])
            self.assertEqual(result['analysis_candidates'],[candidate])
            self.assertEqual(result['bounding_gap_candidates'],[])
        self.assertEqual(classify(0x208,[],[candidate],[excluded])['analysis_candidates'],[])

    def test_real_separate_entry_inside_other_candidate_extent_is_preserved(self):
        disconnected={'start':0x100,'end_exclusive':0x208,'body_ranges':['00000100-00000107','00000200-00000207']}
        actual={'start':0x150,'end_exclusive':0x160,'body_ranges':['00000150-0000015f']}
        result=classify(0x150,[{'start':0x150,'end':0x160}],[disconnected,actual],[])
        self.assertEqual(result['analysis_candidates'],[actual])
        self.assertEqual(result['bounding_gap_candidates'],[disconnected])
        self.assertEqual(result['map_status'],'mapped entry')

    def test_empty_explicit_body_does_not_imply_extent_membership(self):
        result=classify(0x104,[],[{'start':0x100,'end_exclusive':0x120,'body_ranges':[]}],[])
        self.assertEqual(result['analysis_candidates'],[])
        self.assertEqual(len(result['bounding_gap_candidates']),1)
