import struct, tempfile, unittest
from pathlib import Path
from runtime_census import syscall_number, scan_program, physical, block, runtime_mentions, installed_syscalls

JR_RA, NOP, SYSCALL = 0x03E00008, 0, 0x0000000C


def program(words, base=0x1000):
    data = struct.pack('<%dI' % len(words), *words)
    return lambda a: struct.unpack_from('<I', data, a - base)[0] if base <= a <= base + len(data) - 4 else None


class RuntimeCensusTests(unittest.TestCase):
    def test_syscall_numbers(self):
        # A stub: ADDIU v1, zero, 0x3C; SYSCALL. A negative number; v1 written otherwise; no number at all.
        word = program([0x2403003C, SYSCALL, JR_RA, NOP, 0x2403FFAB, SYSCALL, 0x00601825, SYSCALL, SYSCALL])
        self.assertEqual(syscall_number(word, 0x1000, 0x1004), 0x3C)
        self.assertEqual(syscall_number(word, 0x1010, 0x1014), -0x55)
        self.assertIsNone(syscall_number(word, 0x1010, 0x101C))  # OR v1, v1, zero writes v1 from a register.
        self.assertIsNone(syscall_number(word, 0x1020, 0x1020))

    def test_register_accesses(self):
        # LUI a0, 0x1000; SW at 0x810(a0) (Timer 1 mode); LUI a1, 0xB000; ORI a1, a1, 0x1000; LW 0(a1)
        # (Timer 2 through the uncached alias); after a call the base is not known any more.
        word = program([0x3C041000, 0xAC800810, 0x3C05B000, 0x34A51000, 0x8CA60000, 0x0C000000, NOP, 0xAC800810, JR_RA, NOP])
        syscalls, accesses = scan_program([(0x1000, 0x1028)], word)
        self.assertEqual(dict(syscalls), {})
        self.assertEqual(sorted(accesses), [0x10000810, 0x10001000])
        self.assertEqual(len(accesses[0x10000810]['store sw']), 1)
        self.assertIn('pointer built', accesses[0x10001000])
        self.assertEqual(physical(0xB0001000), 0x10001000)
        self.assertEqual(block(0x10000810), 'Timer 1')

    def test_runtime_mentions_and_gates(self):
        with tempfile.TemporaryDirectory() as folder:
            Path(folder, 'a.cpp').write_text(
                'if(code==0 && selector==0x44) {\n'
                'if(code==0 && selector==0x73 && context->pc==0x32EFD8) {\n'
                'if(address==0x10000810 && context->pc==0x28C770\n    && sp==1) {\n'
                'if(address>=0x10002000 && address<0x10002040) {\n', encoding='utf-8')
            numbers, addresses, ranges = runtime_mentions(Path(folder))
        self.assertEqual(numbers, {0x44: False, 0x73: True})
        self.assertEqual(addresses, {0x10000810: True, 0x10002000: False, 0x10002040: False})
        self.assertEqual(ranges, [(0x10002000, 0x10002040, False)])

    def test_installed_numbers(self):
        # JAL to the SetSyscall stub at 0x1000 with the number in the delay slot.
        word = program([0x24030074, SYSCALL, JR_RA, NOP, 0x0C000400, 0x2404005A, JR_RA, NOP])
        self.assertEqual(installed_syscalls([([(0x1000, 0x1010), (0x1010, 0x1020)], word, None, None)], {0x1000}), {0x5A})


if __name__ == '__main__':
    unittest.main()
