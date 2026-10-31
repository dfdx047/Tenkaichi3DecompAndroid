import struct,unittest
from types import SimpleNamespace
from capture_ee import texture_stub,FRESH_BIND_STUB,validate_disc_observer_modes
from capture_disc_api import hook
from capture_disc_type import hook as type_hook
from verify_disc_command import first_read_request,ready_completed
class DiscCommandTests(unittest.TestCase):
    def test_semaphore_passthrough_restores_each_original_branch(self):
        for point,fall,taken in [('rpc-ncmd-bind-sema',0x33281C,0x33282C),('rpc-ncmd-call-sema',0x332AD8,0x332AE8)]:
            w=struct.unpack('<22I',texture_stub(point));self.assertEqual(w[:2],(0x3C180036,0x37189590))
            self.assertEqual(w[16:],(0x04410003,0xAE220008,0x08000000|(fall>>2),0,0x08000000|(taken>>2),0))
            self.assertEqual((w[14]&0x3FFFFFF)<<2,FRESH_BIND_STUB+56)
    def test_api_filter_preserves_nonmatching_calls_and_observes_actual_return(self):
        for kind in ('sc-media','sc-read2','sc-key','sc-dual','sc-lsn','sc-timeout'):
            b=hook(0x12340,kind);w=struct.unpack('<'+str(len(b)//4)+'I',b)
            self.assertEqual(w[1]&0xFFFF0000,0x14980000)
            self.assertEqual(w[2+(w[1]&65535):],(0x08000000|(0x12340>>2),0))
            self.assertIn(0x0C000000|(0x12340>>2),w);self.assertIn(0xAF020008,w)
    def test_ready_observer_executes_both_displaced_loads_before_publication(self):
        w=struct.unpack('<12I',type_hook(True));self.assertEqual(w[3:6],(0x90630000,0x90420000,0))
        self.assertEqual(w[6:10],(0xAF030008,0xAF020010,0x24190001,0xAF190000))
    def test_observers_reject_mixed_modes_wrong_profile_and_checkpoint(self):
        a=dict(capture_point='rpc-ncmd-call-sema',capture_disc_api='sc-read2',capture_disc_type=False,disc=True,absent_cards=True,capture_dbc_dma=False)
        validate_disc_observer_modes(SimpleNamespace(**a))
        for key,value in [('capture_disc_type',True),('capture_dbc_dma',True),('disc',False),('absent_cards',False),('capture_point','rpc-disc-init-after'),('capture_disc_api','read')]:
            with self.subTest(key=key):
                with self.assertRaises(ValueError):validate_disc_observer_modes(SimpleNamespace(**dict(a,**{key:value})))
    def test_read_request_rejects_changed_extent_destination_mode_and_callback(self):
        b=bytearray(32*1024*1024);v=[0xE2A,2,0x39DE80,0x100,0x3694C0,0x369580];struct.pack_into('<6I',b,0x3684C0,*v);self.assertTrue(first_read_request(b))
        for n in range(6):
            x=v.copy();x[n]+=1;struct.pack_into('<6I',b,0x3684C0,*x);self.assertFalse(first_read_request(b))
    def test_readiness_completion_requires_prior_search_and_no_runtime_failure(self):
        r=dict(disc_rpc_binds=1,disc_rpc_calls=1,disc_dma_submissions=1,disc_dma_polls=0,disc_result=1,native_iop_error='',ncmd_rpc_binds=1,ncmd_bind_semaphores=1,ncmd_ready_calls=1)
        self.assertTrue(ready_completed(r))
        for key,value in [('disc_result',0),('native_iop_error','failed'),('ncmd_rpc_binds',0),('ncmd_ready_calls',0),('ncmd_bind_semaphores',0)]:self.assertFalse(ready_completed(dict(r,**{key:value})))

    def test_directory_diagnostics_reject_changed_files_count_and_hidden_failure(self):
        from verify_sound_rpc import completed_directory_diagnostics
        names=['ZS2US_1.AFS','ZS2US_2.AFS','ZS2USOP.SFD','ZS2USED.SFD','LOGO_O.SFD']
        r=dict(console_messages=6,console_messages_text=['DVCI: "'+name+'" found.\n' for name in names]+['DVCI: Total 5 files.\n'],disc_rpc_calls=6,ncmd_read_calls=1,ncmd_ready_calls=1,scmd_calls=0,native_disc_requests=8)
        self.assertTrue(completed_directory_diagnostics(r))
        self.assertTrue(completed_directory_diagnostics(dict(r,console_messages=7,console_messages_text=r['console_messages_text']+['later startup message\n'])))
        for key,value in [('console_messages',5),('console_messages_text',r['console_messages_text'][::-1]),('disc_rpc_calls',5),('ncmd_read_calls',0),('native_disc_requests',7),('native_iop_error','failed')]:self.assertFalse(completed_directory_diagnostics(dict(r,**{key:value})))

    def test_callback_entry_observer_never_calls_or_publishes_completion(self):
        w=struct.unpack('<11I',hook(0x3E830,'read-callback'))
        self.assertFalse(any(x>>26==3 for x in w));self.assertNotIn(0xAF020008,w)
    def test_callback_return_observer_calls_trampoline_before_result(self):
        from capture_disc_api import CODE
        w=struct.unpack('<15I',hook(0x3E830,'read-callback-after'))
        self.assertEqual(w[7],0x0C000000|((CODE+0x100)>>2));self.assertEqual(w[10],0xAF020008);self.assertEqual(w[12],0xAF190000)
    def test_rv_observer_uses_distinct_reached_import(self):
        from capture_disc_api import POINTS
        self.assertEqual(POINTS['read-rv'],(0x4B80,63));self.assertNotEqual(POINTS['read-rv'],POINTS['read'])
