import struct, unittest
from disassemble import lines, lower, upper

NOP_UPPER = 0x000002FF
NOP_LOWER = 0x8000033C


def upper_word(dest, ft, fs, fd, op): return (dest << 21) | (ft << 16) | (fs << 11) | (fd << 6) | op
def special(dest, ft, fs, code): return (dest << 21) | (ft << 16) | (fs << 11) | ((code >> 2) << 6) | 0x3C | (code & 3)
def lower_special(dest, it, is_, code): return (0x40 << 25) | special(dest, it, is_, code)


class Disassemble(unittest.TestCase):
    def test_upper_instructions(self):
        self.assertEqual(upper(NOP_UPPER), 'NOP')
        self.assertEqual(upper(upper_word(15, 2, 1, 3, 0x28)), 'ADD.xyzw vf3, vf1, vf2')
        self.assertEqual(upper(upper_word(14, 19, 20, 20, 0x1B)), 'MULw.xyz vf20, vf20, vf19w')   # A broadcast field.
        self.assertEqual(upper(upper_word(15, 0, 24, 24, 0x1C)), 'MULq.xyzw vf24, vf24, Q')
        self.assertEqual(upper(upper_word(8, 0, 26, 26, 0x22)), 'ADDi.x vf26, vf26, I')
        self.assertEqual(upper(special(15, 28, 1, 0x18)), 'MULAx.xyzw ACC, vf1, vf28x')             # To the accumulator.
        self.assertEqual(upper(special(15, 28, 2, 0x09)), 'MADDAy.xyzw ACC, vf2, vf28y')
        self.assertEqual(upper(special(15, 29, 24, 0x15)), 'FTOI4.xyzw vf29, vf24')
        self.assertEqual(upper(special(15, 5, 4, 0x12)), 'ITOF12.xyzw vf5, vf4')
        self.assertEqual(upper(special(14, 25, 25, 0x1F)), 'CLIP vf25, vf25w')
        self.assertEqual(upper(upper_word(15, 1, 2, 3, 0x30)), '.word 0x01E110F0')                  # Not modelled.

    def test_lower_instructions(self):
        self.assertEqual(lower(NOP_LOWER, 0), 'NOP')
        self.assertEqual(lower((15 << 21) | (5 << 16) | (2 << 11) | 0x7FF, 0), 'LQ.xyzw vf5, -1(vi2)')
        self.assertEqual(lower((0x01 << 25) | (15 << 21) | (7 << 16) | (29 << 11) | 2, 0), 'SQ.xyzw vf29, 2(vi7)')
        self.assertEqual(lower((0x08 << 25) | (2 << 16) | (3 << 11) | 252, 0), 'IADDIU vi2, vi3, 252')
        self.assertEqual(lower((0x29 << 25) | (10 << 16) | (0 << 11) | (-59 & 0x7FF), 120), 'IBNE vi10, vi0, 62')  # A target, not an offset.
        self.assertEqual(lower((0x20 << 25) | (-90 & 0x7FF), 125), 'B 36')
        self.assertEqual(lower((0x12 << 25) | 0x03FFFF, 0), 'FCAND vi1, 0x03FFFF')
        self.assertEqual(lower((0x40 << 25) | (10 << 16) | (10 << 11) | (31 << 6) | 0x32, 0), 'IADDI vi10, vi10, -1')
        self.assertEqual(lower((0x40 << 25) | (10 << 16) | (7 << 11) | (4 << 6) | 0x30, 0), 'IADD vi4, vi7, vi10')
        self.assertEqual(lower(lower_special(15, 17, 3, 0x34), 0), 'LQI.xyzw vf17, (vi3++)')
        self.assertEqual(lower(lower_special(0, 3, 0, 0x68), 0), 'XTOP vi3')
        self.assertEqual(lower(lower_special(0, 0, 2, 0x6C), 0), 'XGKICK vi2')
        self.assertEqual(lower((0x40 << 25) | (3 << 23) | (3 << 21) | (24 << 16) | (0 << 11) | (14 << 6) | 0x3C, 0), 'DIV Q, vf0w, vf24w')
        self.assertEqual(lower(0x7E000000, 0), '.word 0x7E000000')

    def test_a_program_is_listed_in_address_order_with_its_marks(self):
        half, = struct.unpack('<I', struct.pack('<f', 0.5))
        text = lines(1, {1: (NOP_LOWER, NOP_UPPER), 0: (half, 0x80000000 | NOP_UPPER), 2: (NOP_LOWER, 0x40000000 | NOP_UPPER)})
        self.assertEqual(text[0].split(), ['0', 'NOP', 'I', '=', '0.5'])   # The I bit: the lower word is a float.
        self.assertEqual(text[1].split(), ['1', '>', 'NOP', 'NOP'])        # The start address.
        self.assertEqual(text[2].split(), ['2', 'E', 'NOP', 'NOP'])        # The end bit.


if __name__ == '__main__':
    unittest.main()
