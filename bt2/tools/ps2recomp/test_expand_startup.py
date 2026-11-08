import unittest
from expand_startup import select_target, render, neighbours, remaining_overlay, entry_labels


class StartupExpansionTests(unittest.TestCase):
    def test_only_exact_unique_main_entries_are_selected(self):
        rows = [{'Start': '0x1000', 'End': '0x1080'}]
        self.assertEqual(select_target(0x1000, rows), 0x1000)
        for target in (None, '0x1000', 0, 0x1001, 0x1004, 0x80001000):
            with self.subTest(target=target), self.assertRaises(ValueError):
                select_target(target, rows)
        with self.assertRaises(ValueError):
            select_target(0x1000, rows * 2)

    def test_neighbours_need_a_span_a_unique_row_and_a_generated_source(self):
        rows = [{'Start': '0x1000', 'End': '0x1080'}, {'Start': '0x1080', 'End': '0x1100'}, {'Start': '0x1080', 'End': '0x1100'}]
        self.assertEqual(neighbours(0x1000, rows, set(), 0), [])
        # 0x1080 is duplicated and no generated source exists for these synthetic addresses.
        self.assertEqual(neighbours(0x1000, rows, set(), 0x1000), [])

    def test_remaining_overlay_entries_are_unique_generated_and_not_yet_selected(self):
        rows = [{'Start': '0x3000', 'End': '0x3040'}, {'Start': '0x2000', 'End': '0x2040'}, {'Start': '0x4000', 'End': '0x4040'},
                {'Start': '0x5000', 'End': '0x5040'}, {'Start': '0x5000', 'End': '0x5080'}, {'Start': '0x6000', 'End': '0x6040'}]
        generated = {f'FUN_{a:08X}_0x{a:x}.cpp' for a in (0x2000, 0x3000, 0x4000, 0x5000)}
        # 0x4000 is selected, 0x5000 is duplicated and 0x6000 has no generated source.
        self.assertEqual(remaining_overlay(rows, {0x4000}, generated),
                         [{'start': 0x2000, 'end': 0x2040}, {'start': 0x3000, 'end': 0x3040}])

    def test_neutral_bindings_and_source_selection(self):
        cmake, header = render([0x1000, 0x2000])
        self.assertIn('${BT2_GENERATED_DIR}/../bt2-runtime/FUN_00001000_0x1000.cpp', cmake)
        # Self-contained header: declarations of the selected functions, then a table registered in order.
        self.assertIn('void FUN_00002000_0x2000(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime);', header)
        self.assertLess(header.index('{0x00001000, FUN_00001000_0x1000},'), header.index('{0x00002000, FUN_00002000_0x2000},'))
        self.assertIn('runtime.registerFunction(entry.address, entry.function);', header)
        self.assertNotIn('game_data', cmake)

    def test_interior_entries_are_the_entry_switch_labels(self):
        source = ('void FUN_00001000_0x1000(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {\n'
                  '    switch (ctx->pc) {\n        case 0x1000u: goto label_1000;\n        case 0x1010u: goto label_1010;\n'
                  '        case 0x1008u: goto label_1008;\n        default: break;\n    }\n'
                  '    switch (jumpTarget) {\n            case 0x1040u: goto label_1040;\n            default: break;\n    }\n')
        self.assertEqual(entry_labels(source, 0x1000), [0x1008, 0x1010])
        self.assertEqual(entry_labels('void f() {\n    ctx->pc = 0x1000u;\n}\n', 0x1000), [])
        cmake, header = render([0x1000], [(0x1008, 0x1000)])
        # Interior entries are registered after every function, with their owner.
        self.assertIn('{0x00001008, FUN_00001000_0x1000, 0x00001000},', header)
        self.assertLess(header.index('registerFunction'), header.index('registerInteriorEntry'))
        self.assertTrue(header.endswith('}\n'))
        self.assertNotIn('registerInteriorEntry', render([0x1000])[1])
