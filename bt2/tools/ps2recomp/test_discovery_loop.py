import unittest
import discovery_loop as d

DISPATCH=['    ctx->pc = 0x100; runtime->dispatchGuestBranch(rdram, ctx, 0x200, 0x100);','    return;']
DIRECT=['    FUN_00000200_0x200(rdram, ctx, runtime); return;']

class DiscoveryLoopTests(unittest.TestCase):
    def test_tail_jump_becoming_a_direct_call_is_expected(self):
        self.assertIsNone(d.unexpected_change(['a;']+DISPATCH+['}'],['a;']+DIRECT+['}']))
        self.assertIsNone(d.unexpected_change(['a;'],['a;']))
    def test_any_other_change_is_reported(self):
        self.assertEqual(d.unexpected_change(['a;','b;'],['a;','c;']),'-b;')
        self.assertEqual(d.unexpected_change(['a;'],['a;','SET_GPR_U32(ctx, 2, 1);']),'+SET_GPR_U32(ctx, 2, 1);')
    def test_receipt_takes_the_new_hash_of_a_checked_file_and_the_map_hash(self):
        receipt={'map_sha256':'old','files':{'f.cpp':'1','g.cpp':'2'}}
        manifest={'map_sha256':'new','files':{'f.cpp':'9','g.cpp':'2','h.cpp':'3'}}
        changed=d.refresh_receipt(receipt,manifest,lambda n:DISPATCH,lambda n:DIRECT)
        self.assertEqual(changed,['f.cpp'])
        self.assertEqual(receipt,{'map_sha256':'new','files':{'f.cpp':'9','g.cpp':'2'}})
    def test_receipt_is_refused_when_a_selected_source_changed_otherwise(self):
        receipt={'map_sha256':'old','files':{'f.cpp':'1'}}
        with self.assertRaisesRegex(ValueError,'unexpected change in f.cpp'):
            d.refresh_receipt(receipt,{'map_sha256':'new','files':{'f.cpp':'9'}},lambda n:['x;'],lambda n:['y;'])
        self.assertEqual(receipt['files']['f.cpp'],'1')
    def test_map_gap_needs_the_branch_target_line_and_a_target(self):
        self.assertTrue(d.is_map_gap(['Branch target/source/resume: 3b9d58/3ba354/3ba35c'],{'target':0x3b9d58}))
        self.assertFalse(d.is_map_gap(['Memory address/pc/acks/polls: 1/2/3/4'],{'target':0}))
        self.assertFalse(d.is_map_gap(['Branch target/source/resume: 0/1/2'],{'target':0}))
    def test_interpreted_code_named_in_the_log(self):
        # The line ee_interpreter.cpp prints for each place of unmapped code it runs.
        line='EE interpreter: unmapped code at 0x003A1B40 runs interpreted (thread 1)'
        self.assertEqual(d.INTERPRETED.findall(line),['003A1B40'])
        self.assertEqual(d.INTERPRETED.findall('EE interpreter: no handler for the instruction 0x24420005 at 0x00001000'),[])
    def test_labels_of_a_generated_source(self):
        import tempfile,pathlib
        with tempfile.TemporaryDirectory() as folder:
            path=pathlib.Path(folder)/'f.cpp'
            path.write_text('    switch (ctx->pc) {\n        case 0x1000u: goto label_1000;\n        case 0x1010u: goto label_1010;\n        default: break;\n    }\n',encoding='utf-8')
            self.assertEqual(d.labels_of(path),{'1000','1010'})
            path.write_text('    ctx->pc = 0x1000u;\n',encoding='utf-8')
            self.assertEqual(d.labels_of(path),set())

if __name__=='__main__':unittest.main()
