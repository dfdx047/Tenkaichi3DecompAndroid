"""Functions the map takes from merged candidates and from entry addresses, and the scan that finds such addresses (synthetic code)."""
import struct, unittest
from prepare_map import add_entries, select_candidates, split_merged
from map_gaps import find_gaps, find_code, routine_end, preceded_by, without_label

NOP, JR_RA = 0, 0x03E00008
def jump(target): return 0x08000000 | (target >> 2)
def call(target): return 0x0C000000 | (target >> 2)
def branch(at, target): return 0x10000000 | (((target - at - 4) >> 2) & 0xFFFF) # BEQ zero,zero
def bne(at, target): return 0x14400000 | (((target - at - 4) >> 2) & 0xFFFF)    # BNE v0,zero
LI = 0x24020001 # ADDIU v0,zero,1

def text(words, size=None):
    return ({'sections': [{'name': '.text', 'address': 0x1000, 'size': (size or len(words)) * 4, 'file_offset': 0, 'flags': 4, 'occupies_file_bytes': True}]},
            struct.pack('<%dI' % len(words), *words))


class SplitMergedTests(unittest.TestCase):
    def test_tail_piece_becomes_its_own_function(self):
        # 0x1000: a function that ends in a jump to a shared tail at 0x1020; 0x100C: another function.
        words = [LI, jump(0x1020), NOP, LI, JR_RA, NOP, NOP, NOP, LI, JR_RA, NOP]
        words = words[:8] + [LI, JR_RA, NOP] # 0x1020: LI / JR RA / NOP
        metadata, data = text(words)
        merged = {'start': 0x1000, 'end_exclusive': 0x102C, 'body_size': 24, 'contiguous_aligned': False, 'body_ranges': ['00001000-0000100b', '00001020-0000102b']}
        other = {'start': 0x100C, 'end_exclusive': 0x1018, 'body_size': 12, 'contiguous_aligned': True, 'body_ranges': ['0000100c-00001017']}
        pieces = split_merged(merged, metadata, data, sorted([(0x1000, 0x102C), (0x100C, 0x1018)]))
        self.assertEqual([(p['start'], p['end_exclusive'], p['contiguous_aligned']) for p in pieces], [(0x1000, 0x100C, True), (0x1020, 0x102C, True)])
        selected, excluded = select_candidates({'functions': [merged, other]}, metadata, data=data)
        self.assertEqual(selected, [{'start': 0x1000, 'end': 0x100C}, {'start': 0x100C, 'end': 0x1018}, {'start': 0x1020, 'end': 0x102C}])
        self.assertEqual(excluded, [])

    def test_tail_inside_another_function_is_not_added(self):
        words = [LI, jump(0x1014), NOP, LI, LI, LI, JR_RA, NOP]
        metadata, data = text(words)
        merged = {'start': 0x1000, 'end_exclusive': 0x1020, 'body_size': 24, 'contiguous_aligned': False, 'body_ranges': ['00001000-0000100b', '00001014-0000101f']}
        holder = {'start': 0x100C, 'end_exclusive': 0x1020, 'body_size': 20, 'contiguous_aligned': True, 'body_ranges': ['0000100c-0000101f']}
        selected, _ = select_candidates({'functions': [merged, holder]}, metadata, data=data)
        self.assertEqual(selected, [{'start': 0x1000, 'end': 0x100C}, {'start': 0x100C, 'end': 0x1020}])

    def test_padding_and_unfinished_code_do_not_split(self):
        # A NOP between the ranges is padding; a range that does not end in a transfer runs on.
        words = [LI, JR_RA, NOP, NOP, LI, JR_RA, NOP]
        metadata, data = text(words)
        padded = {'start': 0x1000, 'end_exclusive': 0x101C, 'body_size': 24, 'contiguous_aligned': False, 'body_ranges': ['00001000-0000100b', '00001010-0000101b']}
        self.assertIsNone(split_merged(padded, metadata, data, [(0x1000, 0x101C)]))
        running = [LI, LI, LI, 0x12345678, LI, JR_RA, NOP]
        metadata, data = text(running)
        open_end = {'start': 0x1000, 'end_exclusive': 0x101C, 'body_size': 24, 'contiguous_aligned': False, 'body_ranges': ['00001000-0000100b', '00001010-0000101b']}
        self.assertIsNone(split_merged(open_end, metadata, data, [(0x1000, 0x101C)]))


class AddEntriesTests(unittest.TestCase):
    def test_entry_in_unmapped_code(self):
        # 0x1000: a mapped function. 0x100C: unmapped, with a forward branch over its first return.
        words = [LI, JR_RA, NOP, bne(0x100C, 0x101C), NOP, JR_RA, NOP, LI, JR_RA, NOP, LI, JR_RA, NOP]
        metadata, data = text(words)
        rows, notes = add_entries([{'start': 0x1000, 'end': 0x100C}, {'start': 0x1028, 'end': 0x1034}], [0x100C], metadata, data)
        self.assertEqual(rows, [{'start': 0x1000, 'end': 0x100C}, {'start': 0x100C, 'end': 0x1028}, {'start': 0x1028, 'end': 0x1034}])
        self.assertTrue(notes[0]['result'].startswith('new function of 28'))

    def test_stub_ending_in_a_system_call(self):
        # 0x100C: LI, SYSCALL, then the next function at 0x1014: taken up to that function.
        SYSCALL = 0x0000000C
        words = [LI, JR_RA, NOP, LI, SYSCALL, LI, JR_RA, NOP]
        metadata, data = text(words)
        mapped = [{'start': 0x1000, 'end': 0x100C}, {'start': 0x1014, 'end': 0x1020}]
        rows, notes = add_entries(mapped, [0x100C], metadata, data)
        self.assertIn({'start': 0x100C, 'end': 0x1014}, rows)
        # Code that only runs into the next function is still refused.
        words[4] = LI
        metadata, data = text(words)
        rows, notes = add_entries(mapped, [0x100C], metadata, data)
        self.assertEqual(rows, mapped)

    def test_entries_that_are_not_taken(self):
        words = [LI, JR_RA, NOP, LI, LI, LI, LI, JR_RA, NOP]
        metadata, data = text(words)
        mapped = [{'start': 0x1000, 'end': 0x100C}, {'start': 0x1018, 'end': 0x1024}]
        # Code that runs into the next function, an address that is already a function, one outside the code.
        rows, notes = add_entries(mapped, [0x100C, 0x1018, 0x2000, 0x1002], metadata, data)
        self.assertEqual(rows, mapped)
        self.assertEqual([n['result'] for n in notes], ['not aligned code of this program', 'not taken: runs into the next function', 'already a function', 'not aligned code of this program'])
        # A branch back before the entry: the code belongs to something earlier.
        back = [LI, JR_RA, NOP, LI, bne(0x1010, 0x1000), NOP, JR_RA, NOP]
        metadata, data = text(back)
        rows, notes = add_entries([{'start': 0x1000, 'end': 0x100C}], [0x100C], metadata, data)
        self.assertEqual(len(rows), 1)
        self.assertEqual(notes[0]['result'], 'not taken: branches outside itself')

    def test_entry_inside_a_function_splits_it(self):
        # One mapped range holding two functions: ... JR RA / NOP | LI / JR RA / NOP.
        words = [LI, JR_RA, NOP, LI, JR_RA, NOP]
        metadata, data = text(words)
        rows, notes = add_entries([{'start': 0x1000, 'end': 0x1018}], [0x100C], metadata, data)
        self.assertEqual(rows, [{'start': 0x1000, 'end': 0x100C}, {'start': 0x100C, 'end': 0x1018}])
        self.assertTrue(notes[0]['result'].startswith('split'))
        # Not where the code before runs into it, and not where a branch crosses it.
        running = [LI, LI, LI, LI, JR_RA, NOP]
        metadata, data = text(running)
        rows, notes = add_entries([{'start': 0x1000, 'end': 0x1018}], [0x100C], metadata, data)
        self.assertEqual(len(rows), 1)
        self.assertEqual(notes[0]['result'], 'inside a function whose code runs into it')
        crossing = [bne(0x1000, 0x1010), JR_RA, NOP, LI, JR_RA, NOP]
        metadata, data = text(crossing)
        rows, notes = add_entries([{'start': 0x1000, 'end': 0x1018}], [0x100C], metadata, data)
        self.assertEqual(len(rows), 1)
        self.assertEqual(notes[0]['result'], 'inside a function that branches across it')


class FindGapsTests(unittest.TestCase):
    def test_targets_and_pointers(self):
        # 0x1000 (mapped): calls 0x1018 (unmapped) and 0x1010 (the middle of the mapped 0x100C).
        # Data at 0x1034: a pointer to unmapped code after a completed function (0x1028), one into
        # the middle of unmapped code (0x101C: not after a transfer), one to a mapped start.
        words = [call(0x1018), call(0x1010), JR_RA, LI, JR_RA, NOP, LI, LI, JR_RA, NOP, LI, JR_RA, NOP, 0x1028, 0x101C, 0x100C]
        data = struct.pack('<%dI' % len(words), *words)
        word = lambda a: struct.unpack_from('<I', data, a - 0x1000)[0] if 0x1000 <= a <= 0x1000 + len(data) - 4 else None
        direct, inside, pointers = find_gaps([(0x1000, 0x100C), (0x100C, 0x1018)], word, (0x1000, 0x1034), [(0x1034, 0x1040)])
        self.assertEqual((direct, inside, pointers), ([0x1018], [0x1010], [0x1028]))

    def test_built_unreferenced_and_kernel_code(self):
        LUI_A0, ADDIU_A0 = 0x3C040000, 0x24841020 # lui a0,0 ; addiu a0,a0,0x1020
        ERET = 0x42000018
        # 0x1000 (mapped) builds 0x1020. Gaps: 0x1010 a routine after completed code with its
        # instructions (only its start is an entry), 0x1020 the built routine, 0x1030 padding then a
        # routine, 0x1040 code that runs into ERET, and what follows the ERET.
        words = [LUI_A0, ADDIU_A0, JR_RA, NOP,
                 LI, LI, JR_RA, LI,
                 LI, JR_RA, NOP, NOP,
                 NOP, LI, JR_RA, NOP,
                 LI, ERET, LI, JR_RA, NOP]
        data = struct.pack('<%dI' % len(words), *words)
        word = lambda a: struct.unpack_from('<I', data, a - 0x1000)[0] if 0x1000 <= a <= 0x1000 + len(data) - 4 else None
        built, unreferenced, kernel = find_code([(0x1000, 0x1010)], word, (0x1000, 0x1054))
        self.assertEqual((built, unreferenced, kernel), ([0x1020], [0x1010, 0x1034], [0x1040]))

    def test_routine_end_and_preceding_code(self):
        BEQ_FORWARD = 0x10400003 # beq v0,zero,+3: reaches past the first JR RA
        words = [BEQ_FORWARD, NOP, JR_RA, NOP, LI, JR_RA, NOP]
        data = struct.pack('<%dI' % len(words), *words)
        word = lambda a: struct.unpack_from('<I', data, a - 0x1000)[0] if 0x1000 <= a <= 0x1000 + len(data) - 4 else None
        self.assertEqual(routine_end(word, 0x1000, 0x101C), 0x101C)
        self.assertIsNone(routine_end(word, 0x1000, 0x1018))
        self.assertEqual(preceded_by(word, 0x1010, 0x1000), 'transfer') # After JR RA and its delay slot.
        self.assertIsNone(preceded_by(word, 0x100C, 0x1000)) # The delay slot itself.
        self.assertIsNone(preceded_by(word, 0x1014, 0x1000))

    def test_addresses_inside_functions_without_a_label(self):
        LUI_A0, ADDIU_A0 = 0x3C040000, 0x24841008
        words = [LUI_A0, ADDIU_A0, LI, LI, JR_RA, NOP, 0x100C, 0x1000, 0x1010]
        data = struct.pack('<%dI' % len(words), *words)
        word = lambda a: struct.unpack_from('<I', data, a - 0x1000)[0] if 0x1000 <= a <= 0x1000 + len(data) - 4 else None
        rows, code, data_range = [(0x1000, 0x1018)], (0x1000, 0x1018), [(0x1018, 0x1024)]
        # Built 0x1008 and stored 0x100C and 0x1010 lie inside; 0x1000 is the start.
        self.assertEqual(without_label(rows, word, code, data_range, set()), [0x1008, 0x100C, 0x1010])
        self.assertEqual(without_label(rows, word, code, data_range, {0x100C, 0x1010}), [0x1008])


if __name__ == '__main__':
    unittest.main()
