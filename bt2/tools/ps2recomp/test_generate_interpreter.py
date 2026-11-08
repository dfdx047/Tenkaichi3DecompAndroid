import unittest
from generate_interpreter import is_control, handler_body, handler_flags, render_unit, render_table, WRITES_VU_STATUS, READS_VU_STATUS, SETS_VU_FLAGS, UNKNOWN_VU


def one_instruction(address, statements, comment='0x24420001  addiu       $v0, $v0, 0x1'):
    return (f'void FUN_{address:08X}_0x{address:x}(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {{\n'
            '#ifdef PS2_FUNCTION_LOG_TRACKER\n    PS_LOG_ENTRY("x");\n#endif\n\n'
            f'    ctx->pc = 0x{address:x}u;\n\n    // 0x{address:x}: {comment}\n    ctx->pc = 0x{address:x}u;\n'
            f'{statements}    ctx->pc = 0x{address + 4:X}u;\n}}\n')


class InterpreterHandlerTests(unittest.TestCase):
    def test_control_words(self):
        for word in (0x03E00008, 0x0320F809, 0x08000000, 0x0C000000, 0x10400003, 0x50400003, 0x04410002, 0x41000000, 0x45000016, 0x49010002):
            with self.subTest(word=hex(word)):
                self.assertTrue(is_control(word))
        # ADDIU, SYSCALL, ERET (no delay slot: a handler), MTSAB (REGIMM 24), a COP1 arithmetic word.
        for word in (0x24420001, 0x0000000C, 0x42000018, 0x07180000, 0x46000002):
            with self.subTest(word=hex(word)):
                self.assertFalse(is_control(word))

    def test_body_takes_the_program_counter_as_a_parameter(self):
        source = one_instruction(0x1000010, '    ctx->pc = 0x1000014u;\n    runtime->handleSyscall(rdram, ctx, 0x0u);\n')
        body = handler_body(source, 0x1000010)
        self.assertIn('ctx->pc = pc;', body)
        self.assertIn('ctx->pc = (pc + 4u);', body)
        self.assertNotIn('PS_LOG_ENTRY', body)
        self.assertNotIn('//', body)
        self.assertNotIn('0x1000010', body.lower())
        with self.assertRaises(ValueError):
            handler_body(one_instruction(0x1000010, '    goto label_1000010;\n'), 0x1000010)
        with self.assertRaises(ValueError): # The address in another form than the pc assignments.
            handler_body(one_instruction(0x1000010, '    SET_GPR_U32(ctx, 2, 0x1000010);\n'), 0x1000010)

    def test_flags(self):
        self.assertEqual(handler_flags(one_instruction(0, ''), 'x'), 0)
        self.assertEqual(handler_flags(one_instruction(0, '', '0x4a0  vadd.xyz    $vf1, $vf2, $vf3'), 'x'), SETS_VU_FLAGS)
        self.assertEqual(handler_flags(one_instruction(0, '', '0x4a0  vnop'), 'x'), 0)
        self.assertEqual(handler_flags(one_instruction(0, '', '0x4a0  viadd       $vi1, $vi2, $vi3'), 'x'), UNKNOWN_VU)
        self.assertEqual(handler_flags(one_instruction(0, '', '0x48  ctc2'), 'runtime->writeVuStatusFlag(ctx, 1);'), WRITES_VU_STATUS)
        self.assertEqual(handler_flags(one_instruction(0, '', '0x48  cfc2'), 'runtime->readVuStatusFlag(ctx)'), READS_VU_STATUS)

    def test_units_and_table(self):
        unit = render_unit(3, [(0x24420001, 'ctx->pc = pc;', 0), (0x24420002, 'ctx->pc = pc;', 4)])
        self.assertIn('void h_24420001(uint8_t* rdram, R5900Context* ctx, PS2Runtime* runtime, uint32_t pc)', unit)
        self.assertIn('extern const EeInterpreterHandler ee_interpreter_handlers_003[] = {', unit)
        self.assertLess(unit.index('{0x24420001u, h_24420001, 0u},'), unit.index('{0x24420002u, h_24420002, 4u},'))
        table = render_table([2048, 5])
        self.assertIn('{ee_interpreter_handlers_001, 5u},', table)
        self.assertIn('ee_interpreter_handler_part_count = 2u;', table)


if __name__ == '__main__':
    unittest.main()
