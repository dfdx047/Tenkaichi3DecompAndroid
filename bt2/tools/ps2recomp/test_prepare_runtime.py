"""Synthetic memory-layout and generated dispatch-hook checks."""
import unittest
from prepare_runtime import sign_extend_float_moves, route_vu_random, build_memory, route_tail_calls, route_direct_jumps, route_data_cache, route_vu_status,route_vu_control, route_indexed_cache, route_fast_halfword_reads, route_fast_byte_reads, widen_sign_branches, route_recursive_calls, route_vu_memory, detach_generated_headers, read_patchable_immediates, patchable_code
import json, re
from run_pipeline import ROOT


class RuntimePreparationTests(unittest.TestCase):
    def test_vu_reset_pulse_is_not_silently_masked(self):
        source='ctx->vu0_fbrst = GPR_U32(ctx, 4) & 0x00000C0Cu;'
        text,count=route_vu_control(source)
        self.assertEqual(count,1);self.assertIn('writeVuFbrst(ctx, GPR_U32(ctx, 4))',text)
        self.assertEqual(route_vu_control(text),(text,0))
        with self.assertRaises(ValueError):route_vu_control('ctx->vu0_fbrst = 0;')
    def test_vu_status_routes_to_checked_runtime(self):
        source='SET_GPR_U32(ctx, 6, ctx->vu0_vpu_stat);'
        text,count=route_vu_status(source)
        self.assertEqual(count,1);self.assertIn('runtime->readVuStatus(ctx)',text)
        self.assertEqual(route_vu_status(text),(text,0))
        with self.assertRaises(ValueError):route_vu_status('unexpected(ctx->vu0_vpu_stat)')

    def test_checked_cache_hook_scope(self):
        source = '// 0x1234: 0xbd180040  cache       0x18, 0x40($t0)\n    ctx->pc = 0x1234u;\n    // CACHE instruction (ignored)'
        text, count = route_data_cache(source)
        self.assertEqual(count, 1)
        self.assertIn('dataCacheLine(rdram, ADD32(GPR_U32(ctx, 8), 0x40u))', text)
        self.assertEqual(route_data_cache(text), (text, 0))
        self.assertEqual(route_data_cache(source.replace('0x18,', '0x19,')), (source.replace('0x18,', '0x19,'), 0))

    def test_indexed_cache_preserves_explicit_tag_reads(self):
        source='// 0x1234: 0xbcd00001  cache       0x10, 0x1($a2)\nctx->pc = 0x1234u;\n// CACHE instruction (ignored)\nSET_GPR_S32(ctx, 2, (int32_t)ctx->cop0_taglo);'
        text,count,reads=route_indexed_cache(source)
        self.assertEqual((count,reads),(1,1))
        self.assertIn('indexedDataCache(rdram, ctx, 0x10u',text)
        self.assertIn('readCacheTagLo(ctx)',text)
        self.assertEqual(route_indexed_cache(text),(text,0,0))
        with self.assertRaises(ValueError):route_indexed_cache(source.replace('0x10,','0x11,'))
        with self.assertRaises(ValueError):route_indexed_cache('ctx->cop0_taglo = 0;')

    def test_fast_halfword_load_uses_checked_owned_ram(self):
        text,count=route_fast_halfword_reads('SET_GPR_ZE32(ctx, 2, (uint16_t)FAST_READ16(0x1234u));')
        self.assertEqual(count,1);self.assertIn('READ16(0x1234u)',text)
        self.assertEqual(route_fast_halfword_reads(text),(text,0))
        with self.assertRaises(ValueError):route_fast_halfword_reads('FAST_READ16(variable)')

    def test_fast_byte_load_preserves_signed_and_unsigned_casts(self):
        source='(int8_t)FAST_READ8(0x1234u); (uint8_t)FAST_READ8(0x5678u);'
        text,count=route_fast_byte_reads(source)
        self.assertEqual(count,2);self.assertEqual(text,'(int8_t)READ8(0x1234u); (uint8_t)READ8(0x5678u);')
        self.assertEqual(route_fast_byte_reads(text),(text,0))
        with self.assertRaises(ValueError):route_fast_byte_reads('FAST_READ8(variable)')

    def load(self, address=4, memory_size=8, file_size=4, offset=0):
        return {"virtual_address": address, "memory_size": memory_size,
                "file_size": file_size, "file_offset": offset}

    def test_file_payload_and_zero_fill(self):
        image = build_memory(b"test", {"load_segments": [self.load()]}, size=16)
        self.assertEqual(image, bytes(4) + b"test" + bytes(8))

    def test_unsafe_loads_rejected(self):
        for loads in ([self.load(address=12)], [self.load(file_size=9)],
                      [self.load(offset=1)], [self.load(), self.load(address=8)]):
            with self.subTest(loads=loads), self.assertRaises(ValueError):
                build_memory(b"test", {"load_segments": loads}, size=16)

    def test_tail_hook_does_not_replace_declarations_or_other_calls(self):
        source = "void FUN_00001234_0x1234(uint8_t*);\nFUN_00001234_0x1234(rdram, ctx, runtime); return;\nother(rdram, ctx, runtime);"
        text, count = route_tail_calls(source)
        self.assertEqual(count, 1)
        self.assertIn("void FUN_00001234_0x1234(uint8_t*);", text)
        self.assertIn("0x1234u", text)
        self.assertIn("GuestBranchKind::TailJump", text)
        self.assertIn("other(rdram, ctx, runtime);", text)
        self.assertEqual(route_tail_calls(text), (text, 0))

    def test_sign_branches_compare_64_bits(self):
        source=('const bool branch_taken_0x31a378 = (GPR_S32(ctx, 17) >= 0); '
                'const bool branch_taken_0x10 = (GPR_S32(ctx, 2) < 0); '
                'const bool branch_taken_0x14 = (GPR_U64(ctx, 3) != GPR_U64(ctx, 0)); '
                'SET_GPR_S32(ctx, 2, (int32_t)GPR_S32(ctx, 4));')
        text,count=widen_sign_branches(source)
        self.assertEqual(count,2)
        self.assertIn('= (GPR_S64(ctx, 17) >= 0);',text)
        self.assertIn('= (GPR_S64(ctx, 2) < 0);',text)
        self.assertIn('(int32_t)GPR_S32(ctx, 4)',text)

    def test_prepared_sources_do_not_include_the_generated_declarations(self):
        source='#include "ps2_runtime.h"\n#include <ps2_recompiled_functions.h>\n#include <ps2_recompiled_stubs.h>\nvoid FUN_00100008_0x100008() {}\n'
        text=detach_generated_headers(source,'FUN_00100008_0x100008')
        self.assertNotIn('ps2_recompiled',text);self.assertIn('#include "ps2_runtime.h"',text)
        with self.assertRaises(ValueError):detach_generated_headers(source+'FUN_00100010_0x100010();\n','FUN_00100008_0x100008')

    def test_vu_status_flag_access_goes_to_the_runtime(self):
        from prepare_runtime import route_cop2_condition
        source='SET_GPR_U32(ctx, 2, ctx->vu0_status);\nctx->vu0_status = static_cast<uint16_t>(GPR_U32(ctx, 3) & 0xFC0);\n'
        text,count=route_cop2_condition(source)
        self.assertEqual(count,2)
        self.assertIn('SET_GPR_U32(ctx, 2, runtime->readVuStatusFlag(ctx));',text)
        self.assertIn('runtime->writeVuStatusFlag(ctx, GPR_U32(ctx, 3) & 0xFC0);',text)
        with self.assertRaises(ValueError):route_cop2_condition('x = ctx->vu0_status + 1;')
        # Flag-setting instructions between the clear and the read report to the runtime; others do not.
        body=('ctx->vu0_status = static_cast<uint16_t>(GPR_U32(ctx, 0) & 0xFFFFu);\n'
              '    // 0x129484: 0x4bab282c  vsub.xyw    $vf0, $vf5, $vf11\n    ctx->pc = 0x129484u;\n'
              '    // 0x12948c: 0x4b86297d  vftoi4.xy   $vf6, $vf5\n'
              'SET_GPR_U32(ctx, 2, ctx->vu0_status);\n    // 0x1294b0: 0x4bab282c  vsub.xyw    $vf0, $vf5, $vf11\n')
        text,count=route_cop2_condition(body)
        self.assertEqual(count,3)
        self.assertEqual(text.count('runtime->vu0UpdateFlags(ctx, 0x4bab282cu);'),1)
        self.assertIn('vsub.xyw    $vf0, $vf5, $vf11\n    runtime->vu0UpdateFlags(ctx, 0x4bab282cu);\n    ctx->pc = 0x129484u;',text)

    def test_sqrt_takes_its_operand_from_ft(self):
        from prepare_runtime import fix_sqrt_operand
        source=('    // 0x144314: 0x460c0084  c1          0xC0084\n    ctx->pc = 0x144314u;\n    ctx->f[2] = FPU_SQRT_S(ctx->f[0]);\n'
                '        // 0x13b4c8: 0x46140044  c1          0x140044 (Delay Slot)\n        ctx->f[1] = FPU_SQRT_S(ctx->f[0]);\n')
        text,count=fix_sqrt_operand(source)
        self.assertEqual(count,2)
        self.assertIn('    ctx->f[2] = FPU_SQRT_S(ctx->f[12]);\n',text)
        self.assertIn('        ctx->f[1] = FPU_SQRT_S(ctx->f[20]);\n',text)
        self.assertEqual(fix_sqrt_operand('    ctx->f[1] = FPU_ADD_S(ctx->f[1], ctx->f[0]);\n')[1],0)
        with self.assertRaises(ValueError):fix_sqrt_operand('    ctx->f[2] = FPU_SQRT_S(ctx->f[0]);\n')
        with self.assertRaises(ValueError):fix_sqrt_operand('    // 0x100000: 0x46000840  add.s\n    ctx->f[2] = FPU_SQRT_S(ctx->f[0]);\n')

    def test_clip_compares_with_the_magnitude_of_w_and_sets_above_in_the_low_bit(self):
        from prepare_runtime import fix_clip
        source=('    { __m128 fs = ctx->vu0_vf[3]; __m128 ft = _mm_shuffle_ps(ctx->vu0_vf[7], ctx->vu0_vf[7], _MM_SHUFFLE(3,3,3,3)); '
                '__m128 neg_ft = _mm_xor_ps(ft, _mm_castsi128_ps(_mm_set1_epi32(0x80000000))); __m128 gt = _mm_cmpgt_ps(fs, ft); __m128 lt = _mm_cmplt_ps(fs, neg_ft); '
                'uint32_t gt_mask = (uint32_t)_mm_movemask_ps(gt); uint32_t lt_mask = (uint32_t)_mm_movemask_ps(lt); '
                'uint32_t flags = ((lt_mask & 0x1) << 0) | ((gt_mask & 0x1) << 1) | ((lt_mask & 0x2) << 1) | ((gt_mask & 0x2) << 2) | ((lt_mask & 0x4) << 2) | ((gt_mask & 0x4) << 3); '
                'ctx->vu0_clip_flags = ((ctx->vu0_clip_flags << 6) | (flags & 0x3F)) & 0xFFFFFF; }\n')
        text,count=fix_clip(source)
        self.assertEqual(count,1)
        self.assertIn('__m128 fs = ctx->vu0_vf[3]; __m128 limit = _mm_and_ps(_mm_shuffle_ps(ctx->vu0_vf[7], ctx->vu0_vf[7], _MM_SHUFFLE(3,3,3,3)), _mm_castsi128_ps(_mm_set1_epi32(0x7FFFFFFF)));',text)
        self.assertIn('uint32_t flags = (above & 0x1) | ((below & 0x1) << 1) | ((above & 0x2) << 1) | ((below & 0x2) << 2) | ((above & 0x4) << 2) | ((below & 0x4) << 3);',text)
        self.assertTrue(text.endswith('ctx->vu0_clip_flags = ((ctx->vu0_clip_flags << 6) | (flags & 0x3F)) & 0xFFFFFF; }\n'))
        self.assertEqual(fix_clip('    ctx->f[1] = FPU_ADD_S(ctx->f[1], ctx->f[0]);\n')[1],0)
        with self.assertRaises(ValueError):fix_clip('    { __m128 neg_ft = ft; }\n')

    def test_unresolved_jump_table_reenters_the_function(self):
        from prepare_runtime import route_local_indirect_jumps
        jump=('    {\n        const uint32_t jumpTarget = GPR_U32(ctx, 3);\n        ctx->pc = jumpTarget;\n'
              '        if (!runtime->dispatchGuestBranch(rdram, ctx, jumpTarget, 0x1004u, 0x0u, PS2Runtime::GuestBranchKind::IndirectJump, "JR")) {\n            return;\n        }\n    }\n')
        source=('void f(uint8_t* rdram, R5900Context* ctx, PS2Runtime *runtime) {\n\n    switch (ctx->pc) {\n        case 0x1000u: goto label_1000;\n'
                '        case 0x100cu: goto label_100c;\n        default: break;\n    }\n'+jump+'label_100c:\n}\n')
        text,count=route_local_indirect_jumps(source)
        self.assertEqual(count,1)
        self.assertIn('\nbt2_reenter:\n    switch (ctx->pc) {\n',text)
        self.assertIn('        switch (jumpTarget) { case 0x1000u: case 0x100cu: goto bt2_reenter; default: break; }\n        if (!runtime->dispatchGuestBranch(rdram, ctx, jumpTarget,',text)
        # Without entry labels (a plain function-pointer jump) nothing changes.
        plain='void f() {\n'+jump+'}\n'
        self.assertEqual(route_local_indirect_jumps(plain),(plain,0))
        with self.assertRaises(ValueError):route_local_indirect_jumps(source.replace('case 0x100cu: goto label_100c;','case 0x100cu: return;'))

    def test_vf0_stays_constant(self):
        from prepare_runtime import keep_vf0_constant
        source=('    { __m128 res = PS2_VSUB(ctx->vu0_vf[5], ctx->vu0_vf[11]); __m128i mask = _mm_set_epi32(-1, 0, -1, -1); '
                'ctx->vu0_vf[0] = PS2_VBLEND(ctx->vu0_vf[0], res, _mm_castsi128_ps(mask)); }\n'
                '    ctx->vu0_vf[10] = PS2_VADD(ctx->vu0_vf[0], ctx->vu0_vf[1]);\n')
        text,count=keep_vf0_constant(source)
        self.assertEqual(count,1)
        self.assertIn('(void)PS2_VBLEND(ctx->vu0_vf[0], res, _mm_castsi128_ps(mask)); }\n',text)
        self.assertIn('ctx->vu0_vf[10] = PS2_VADD(ctx->vu0_vf[0], ctx->vu0_vf[1]);',text)
        with self.assertRaises(ValueError):keep_vf0_constant('    std::memcpy(&ctx->vu0_vf[0], &bits, 16);\n')
        with self.assertRaises(ValueError):keep_vf0_constant('    ctx->vu0_vf[0]  = x;\n')

    def test_float_bits_moved_to_a_register_are_sign_extended(self):
        source=('    // 0x30eb08: 0x44120800  mfc1        $s2, $f1\n'
                '    { uint32_t bits; std::memcpy(&bits, &ctx->f[1], sizeof(bits)); SET_GPR_U32(ctx, 18, bits); }\n'
                '    { uint32_t bits; std::memcpy(&bits, &ctx->vu0_q, sizeof(bits)); SET_GPR_U32(ctx, 2, bits); }\n'
                '    SET_GPR_U32(ctx, 31, 0x1001F0u);\n')
        text,count=sign_extend_float_moves(source)
        self.assertEqual(count,2)
        self.assertIn('std::memcpy(&bits, &ctx->f[1], sizeof(bits)); SET_GPR_S32(ctx, 18, (int32_t)bits); }',text)
        self.assertIn('std::memcpy(&bits, &ctx->vu0_q, sizeof(bits)); SET_GPR_S32(ctx, 2, (int32_t)bits); }',text)
        self.assertIn('SET_GPR_U32(ctx, 31, 0x1001F0u);',text) # A return address is not a float.
        with self.assertRaises(ValueError):sign_extend_float_moves('    { uint32_t bits = x; SET_GPR_U32(ctx, 2, bits); }\n')

    def test_vu0_random_instructions_run_natively(self):
        source=('    // 0x128580: 0x4b05043c  vrnext.x    $vf5, $R\n    ctx->pc = 0x128580u;\n'
                '    {\n    uint32_t r_vals[4];\n    _mm_storeu_si128((__m128i*)r_vals, _mm_castps_si128(ctx->vu0_r));\n\n    r_vals[3] = 1;\n}\n'
                '        // 0x12857c: 0x4a002c3e  vrinit      $R, $vf5x (Delay Slot)\n'
                '        {\n            uint32_t r0 = 1;\n            ctx->vu0_r = _mm_castsi128_ps(_mm_set_epi32(r0, r0, r0, r0));\n        }\n'
                '    ctx->in_delay_slot = false;\n')
        text,count=route_vu_random(source)
        self.assertEqual(count,2)
        self.assertIn('    ctx->pc = 0x128580u;\n    runtime->vu0RandomInstruction(ctx, 0x4b05043cu);\n',text)
        self.assertIn('(Delay Slot)\n        runtime->vu0RandomInstruction(ctx, 0x4a002c3eu);\n    ctx->in_delay_slot = false;',text)
        with self.assertRaises(ValueError):route_vu_random('    ctx->vu0_r = x;\n')

    def test_vu0_memory_instructions_run_natively(self):
        source=('    // 0x128ef0: 0x4bef837d  vsqi.xyzw   $vf16, ($vi15++)\n    ctx->pc = 0x128ef0u;\n'
                '    { uint32_t addr = ((uint32_t)(ctx->vi[16] & 0x3FF)) << 4; WRITE128(addr, x); }\n'
                '        // 0x128f00: 0x4bf37b7e  vlqd.xyzw   $vf19, (--$vi15) (Delay Slot)\n'
                '        { ctx->vi[15] = (ctx->vi[15] - 1) & 0x3FF; uint32_t addr = ((uint32_t)(ctx->vi[15] & 0x3FF)) << 4; }\n'
                '    ctx->vi[15] = ctx->vi[0] + 0;\n')
        text,count=route_vu_memory(source)
        self.assertEqual(count,2)
        self.assertIn('    runtime->vu0MemoryInstruction(ctx, 0x4bef837du);\n',text)
        self.assertIn('        runtime->vu0MemoryInstruction(ctx, 0x4bf37b7eu);\n',text)
        self.assertIn('ctx->vi[15] = ctx->vi[0] + 0;',text)
        with self.assertRaises(ValueError):route_vu_memory('{ uint32_t addr = ((uint32_t)(ctx->vi[3] & 0x3FF)) << 4; }\n')

    def test_recursive_self_call_goes_through_the_dispatcher(self):
        source=('    SET_GPR_U32(ctx, 31, 0x28DB18u);\n    ctx->pc = 0x28DB14u;\n    ctx->branch_pc = 0x28DB10u;\n'
                '    SET_GPR_U64(ctx, 5, 1);\n    ctx->pc = 0x28DAA8u;\n    goto label_28daa8;\n    ctx->pc = 0x28DB18u;\n'
                '    ctx->pc = 0x28DB40u;\n    goto label_28db60;\n')
        text,count=route_recursive_calls(source,0x28DAA8)
        self.assertEqual(count,1)
        self.assertIn('dispatchGuestBranch(rdram, ctx, 0x28DAA8u, ctx->branch_pc, 0x28DB18u, PS2Runtime::GuestBranchKind::DirectCall, "JAL")',text)
        self.assertNotIn('goto label_28daa8',text);self.assertIn('goto label_28db60',text)
        # A linked jump to another label of the function is left alone.
        self.assertEqual(route_recursive_calls(source,0x28DB00),(source,0))

    def test_direct_jump_ends_the_path(self):
        source = ('    ctx->pc = 0x28BB60u;\n'
                  '    if (!runtime->dispatchGuestBranch(rdram, ctx, 0x28BB60u, 0x3BC5A0u, 0x0u, PS2Runtime::GuestBranchKind::DirectJump, "J")) {\n'
                  '        return;\n    }\n    ctx->pc = 0x3BC5A8u;\n')
        text, count = route_direct_jumps(source)
        self.assertEqual(count, 1)
        self.assertIn('dispatchGuestBranch(rdram, ctx, 0x28BB60u, 0x3BC5A0u, 0u, PS2Runtime::GuestBranchKind::TailJump, "generated tail jump"); return;', text)
        self.assertNotIn("DirectJump", text)
        self.assertEqual(route_direct_jumps(text), (text, 0))
        other = 'if (!runtime->dispatchGuestBranch(rdram, ctx, ctx->r[31], 0x10u, 0x0u, PS2Runtime::GuestBranchKind::Return, "JR")) {\n return;\n}'
        self.assertEqual(route_direct_jumps(other), (other, 0))

    def test_patchable_lui_reads_guest_memory(self):
        source=('    // 0x12ba08: 0x3c013f40  lui         $at, 0x3F40\n    ctx->pc = 0x12ba08u;\n'
                '    SET_GPR_S32(ctx, 1, (int32_t)((uint32_t)16192 << 16));\n'
                '    // 0x12ba0c: 0x3c023f40  lui         $v0, 0x3F40\n    ctx->pc = 0x12ba0cu;\n'
                '    SET_GPR_S32(ctx, 2, (int32_t)((uint32_t)16192 << 16));\n')
        words={0x12ba08:(0x3c013f40,0x3c013f10)}
        text,count=read_patchable_immediates(source,words)
        self.assertEqual(count,1)
        self.assertIn('SET_GPR_S32(ctx, 1, (int32_t)(READ32(0x12ba08u) << 16));',text)
        self.assertIn('SET_GPR_S32(ctx, 2, (int32_t)((uint32_t)16192 << 16));',text) # Not listed: unchanged.
        self.assertEqual(read_patchable_immediates(text,words),(text,0))
        # Another word at a listed address is another build of the game: refused.
        with self.assertRaises(ValueError):read_patchable_immediates(source,{0x12ba08:(0x3c013f80,0x3c013f40)})

    def test_patch_list_matches_the_runtime_table(self):
        config=json.loads((ROOT/'config/game_patches.json').read_text(encoding='utf-8'))['patches']
        table=(ROOT/'runtime/ee/game_patches.cpp').read_text(encoding='utf-8')
        for name,patch in config.items():
            body=re.search(r'GamePatchWord '+name+r'\[\]\{(.*?)\};',table,re.S)
            self.assertIsNotNone(body,name)
            runtime=sorted(tuple(int(x,16) for x in row) for row in re.findall(r'\{(0x[0-9A-F]+)u,(0x[0-9A-F]+)u,(0x[0-9A-F]+)u\}',body[1]))
            listed=sorted(tuple(int(x,16) for x in row) for row in patch['code']+patch['data'])
            self.assertEqual(runtime,listed,name)
        self.assertEqual(len(patchable_code()),2)

if __name__ == "__main__":
    unittest.main()
