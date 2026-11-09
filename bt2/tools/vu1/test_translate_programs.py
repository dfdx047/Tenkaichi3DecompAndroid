import struct, subprocess, sys, tempfile, unittest
from pathlib import Path
from translate_programs import Untranslatable, decode, followed_pairs, read_program, translate

NOP_UPPER = 0x000002FF           # Upper NOP.
NOP_LOWER = 0x8000033C           # MOVE vf0, vf0.
E_BIT = 0x40000000
ADD = (0xF << 21) | (2 << 16) | (1 << 11) | (3 << 6) | 0x28   # ADD.xyzw vf3, vf1, vf2.


def lower_b(offset): return (0x20 << 25) | (offset & 0x7FF)
def lower_ibne(it, is_, offset): return (0x29 << 25) | (it << 16) | (is_ << 11) | (offset & 0x7FF)
def lower_lq(ft, is_, imm, dest=0xF): return (dest << 21) | (ft << 16) | (is_ << 11) | (imm & 0x7FF)


class Translate(unittest.TestCase):
    def test_decode_matches_the_interpreter_cases(self):
        d = decode(lower_lq(5, 2, -1 & 0x7FF), ADD)
        self.assertEqual((d['upper'], d['reg'], d['fs'], d['ft'], d['mask'], d['lower'], d['lower_vf'], d['lower_vi'], d['lower_imm']),
                         ('register', 3, 1, 2, 15, 'lq', 5, 2, -1))
        self.assertFalse(d['skip_lower'])
        self.assertTrue(decode(lower_lq(3, 2, 0), ADD)['skip_lower'])  # The load writes the register the upper instruction writes.
        self.assertEqual(decode(NOP_LOWER, NOP_UPPER)['upper'], 'nop')
        self.assertEqual(decode(NOP_LOWER, NOP_UPPER)['lower'], 'nop')
        self.assertTrue(decode(lower_b(4), NOP_UPPER)['branch'])
        self.assertTrue(decode(0x12345678, 0x80000000 | NOP_UPPER)['i_bit'])

    def test_followed_pairs_stop_after_an_end_and_after_an_unconditional_branch(self):
        # 0: loop body, 1: IBNE back to 0, 2: its delay pair, 3: E bit, 4: the pair after it, 5: never run.
        pairs = {0: (NOP_LOWER, ADD), 1: (lower_ibne(1, 2, -2), NOP_UPPER), 2: (NOP_LOWER, NOP_UPPER),
                 3: (NOP_LOWER, NOP_UPPER | E_BIT), 4: (NOP_LOWER, NOP_UPPER), 5: (NOP_LOWER, NOP_UPPER)}
        self.assertEqual(followed_pairs(0, pairs), [0, 1, 3])  # 2 and 4 run only as the pairs after 1 and 3.

    def test_a_program_becomes_a_function_with_its_control_flow(self):
        pairs = {0: (NOP_LOWER, ADD), 1: (lower_ibne(1, 2, -2), NOP_UPPER), 2: (NOP_LOWER, NOP_UPPER),
                 3: (NOP_LOWER, NOP_UPPER | E_BIT), 4: (NOP_LOWER, NOP_UPPER)}
        text = '\n'.join(translate(0, pairs, 'abc'))
        self.assertIn('static bool program_abc(Vu1& vu,const Vu1::Kick& kick)', text)
        self.assertIn('storeMasked(vu.vf[3].data(),up,15);', text)        # The arithmetic with its operands as constants.
        self.assertIn('vu.lower(0x%08Xu,1,kick);' % lower_ibne(1, 2, -2), text)  # The branch through the interpreter's own code.
        self.assertIn('goto L0;', text)                                    # Taken: back to the loop.
        self.assertIn('vu.pc=5;return true;', text)                        # The end, after the pair that follows the E bit.
        self.assertEqual(text.count('++vu.instructions'), 5)               # Pairs 0, 1, 2 (as delay), 3 and 4 (after the end).

    def test_what_cannot_be_followed_exactly_is_refused(self):
        with self.assertRaises(Untranslatable):  # A branch in a delay slot.
            translate(0, {0: (lower_b(2), NOP_UPPER), 1: (lower_b(2), NOP_UPPER), 3: (NOP_LOWER, NOP_UPPER | E_BIT), 4: (NOP_LOWER, NOP_UPPER)}, 'k')
        with self.assertRaises(Untranslatable):  # A debug bit.
            translate(0, {0: (NOP_LOWER, NOP_UPPER | 0x08000000), 1: (NOP_LOWER, NOP_UPPER | E_BIT), 2: (NOP_LOWER, NOP_UPPER)}, 'k')

    def test_the_tool_reads_a_census_and_writes_the_registry(self):
        pairs = [(0, NOP_LOWER, NOP_UPPER | E_BIT), (1, NOP_LOWER, NOP_UPPER)]
        with tempfile.TemporaryDirectory() as folder:
            census = Path(folder) / 'census'; census.mkdir()
            (census / '00000000000000aa.vu1').write_bytes(struct.pack('<II', 0, len(pairs)) + b''.join(struct.pack('<III', *p) for p in pairs))
            self.assertEqual(read_program(census / '00000000000000aa.vu1'), (0, {0: (NOP_LOWER, NOP_UPPER | E_BIT), 1: (NOP_LOWER, NOP_UPPER)}))
            output = Path(folder) / 'out' / 'vu1_native.cpp'
            subprocess.run([sys.executable, str(Path(__file__).with_name('translate_programs.py')), str(census), str(output)], check=True, capture_output=True)
            text = output.read_text()
            self.assertIn('{0x00000000000000aaull,&Vu1Native::program_00000000000000aa},', text)
            self.assertIn('std::span<const Vu1::NativeEntry> Vu1::nativePrograms()', text)


if __name__ == '__main__':
    unittest.main()
