import struct
import unittest
from capture_ee import export_rva, instrument, TRAP, HANDLER, MARKER, capture_trap
from capture_ee import vif_startup_stub,FRESH_BIND_STUB,EXPECTED_SHA
from capture_ee import graph_csr_stub,graph_startup_stub,texture_stub,TEXTURE_POINTS
from unittest.mock import patch


class CaptureTests(unittest.TestCase):
    def test_frame_vsync_observer_preserves_other_callers(self):
        blob=texture_stub('rpc-vsync-flags-after');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        self.assertEqual(w[:3],(0x8FB80010,0x3C190031,0x37390518))
        early=4+(w[3]&0xFFFF)
        self.assertEqual(w[early:],(0x3C1F0032,0x37FFF1A0,0x08000000|(0x338268>>2),0))

    def test_frame_card_callback_observer_skips_startup_query(self):
        blob=texture_stub('rpc-mc-frame-callback');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        self.assertEqual(w[:2],(0x3C18003A,0x8F19700C)) # Type-output flag of the GetInfo input.
        self.assertEqual(w[3]>>16,0x1320) # BEQ t9,zero: startup GetInfo requested no outputs.
        early=4+(w[3]&0xFFFF)
        self.assertEqual(w[early:],(0x3C02003A,0x3C032000,0x08000000|(0x31DD68>>2),0))
        halt=early-2;self.assertEqual(w[halt],0x08000000|((FRESH_BIND_STUB+halt*4)>>2))
        self.assertIn(0x3419B7B8,w)

    def test_reverse_bind_observer_runs_original_api_and_can_resume(self):
        from capture_ee_reverse_bind import hook
        for terminal in (True,False):
            blob=hook(0x1DA24,terminal);w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
            self.assertIn(0x0C000000|(0x1DA24>>2),w)
            self.assertEqual(w[-4:],(0x24190001,0xAF190000,0x1000FFFF,0) if terminal else (0x8F1F001C,0,0x03E00008,0))

    def test_last_helper_observer_preserves_earlier_shared_invocation(self):
        for point,pair in [('rpc-helper7-ready-status',(0x8FA30000,0x24020008)),('rpc-helper7-suspend-after',(0x0040882D,0x0220102D))]:
            w=struct.unpack('<'+str(len(texture_stub(point))//4)+'I',texture_stub(point))
            self.assertEqual(w[0],0x24180007)
            early=2+(w[1]&0xFFFF)
            self.assertEqual(w[early:early+2],pair)
            self.assertEqual(w[-2],0x08000000|((TEXTURE_POINTS[point][0]+8)>>2))

    def test_rpc_child_terminal_capture_freezes_after_register_publication(self):
        for point in ('rpc-ee-server-registered','rpc-ee-server-wait','rpc-ee-callback-before','rpc-helper5-first-wait','rpc-helper6-first-wait'):
            blob=texture_stub(point);w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
            expected=(0x40196000,0xAF190028,0xAF10002C,0xAF1C0030,0x42000039,0x0000000F)
            self.assertEqual(w[8:14],expected)
            self.assertEqual(w[-2],0x08000000|((FRESH_BIND_STUB+(len(w)-2)*4)>>2))

    def test_helper6_flag_observer_publishes_actual_load(self):
        point='rpc-helper6-flag-before'
        self.assertEqual(TEXTURE_POINTS[point],(0x2EE0C0,0xB7A3))
        blob=texture_stub(point);w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        self.assertIn(0xAF030034,w)
        self.assertLess(w.index(0x0000000F),w.index(0xAF030034))
        self.assertEqual(w[-2],0x08000000|((FRESH_BIND_STUB+(len(w)-2)*4)>>2))

    def test_critical_capture_selects_thread_and_resumes_original_calls(self):
        for thread in (5,6):
            for phase in ('critical-before','critical-after','restore-before'):
                point=f'rpc-helper{thread}-{phase}'
                blob=texture_stub(point);w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
                self.assertEqual(w[0],0x24180000|thread)
                early=2+(w[1]&0xFFFF)
                if phase=='critical-after':
                    self.assertEqual(w[early:early+2],(0x3C030035,0xAC623490))
                else:
                    self.assertEqual(w[early+2],0x08000000|(0x32EAF0>>2))
                    self.assertEqual(w[early+3],0x2610BBF8 if phase=='critical-before' else 0x0040202D)
                self.assertIn(0x42000039,w)

    def test_earlier_helper_lifecycle_capture_points(self):
        for point,address,marker in [('rpc-helper-created',0x2EE51C,0xB796),('rpc-helper-started',0x2EE538,0xB797),
                                    ('rpc-helper2-created',0x2EE684,0xB798),('rpc-helper2-started',0x2EE698,0xB799)]:
            observed,trap,value=capture_trap(point)
            self.assertEqual((observed,value),(address,marker))
            self.assertEqual(len(trap),8)
            self.assertGreater(len(texture_stub(point)),8)

    def test_control_return_observer_preserves_both_branch_paths(self):
        with patch('capture_ee.CRI_CALL_INDEX',40):
            w=struct.unpack('<22I',texture_stub('rpc-cri-nth-control-after'))
        self.assertEqual(w[5],0x24180028)
        self.assertEqual(7+(w[6]&0xFFFF),16)
        self.assertEqual(w[16:18],(0x1A200003,0x0000302D))
        self.assertEqual(w[18],0x08000000|(0x2FD200>>2))
        self.assertEqual(w[20],0x08000000|(0x2FD228>>2))

    def test_indexed_cri_capture_counts_only_target_client(self):
        for index in (1,10,36,64):
            with patch('capture_ee.CRI_CALL_INDEX',index):
                blob=texture_stub('rpc-cri-nth-before');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
                self.assertIn(0x24180000|index,w)
                self.assertEqual(w[0:2],(0x3C180035,0x37185C70))
                self.assertEqual(w[-4:-2],(0x27BDFF40,0xFFB10030))

    def test_cache_capture_keeps_original_tag_read_and_and(self):
        blob=texture_stub('rpc-cache-after')
        self.assertEqual(len(blob),0xA00)
        self.assertEqual(struct.unpack_from('<128I',blob,0x800),(0xDEADBEEF,)*128)
        for offset,way,resume in ((0x100,0,0x32F4D4),(0x180,1,0x32F510)):
            words=struct.unpack_from('<9I',blob,offset)
            self.assertEqual(words[0],0x4002E000)
            self.assertEqual(words[5],0xAF420000|(way*0x100))
            self.assertEqual(words[6],0x00471024)
            self.assertEqual((words[7]&0x3FFFFFF)<<2,resume)

    def test_invalidate_capture_resumes_original_addu(self):
        blob=texture_stub('rpc-cache-invalidate-after')
        for offset,resume in ((0x100,0x32F614),(0x180,0x32F650)):
            words=struct.unpack_from('<9I',blob,offset)
            self.assertEqual(words[0],0x4002E000)
            self.assertEqual(words[6],0x00471024)
            self.assertEqual((words[7]&0x3FFFFFF)<<2,resume)

    def test_disc_semaphore_hook_preserves_earlier_clients(self):
        blob=texture_stub('rpc-disc-bind-sema');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        self.assertEqual(w[:2],(0x3C18003A,0x37188780))
        self.assertEqual(w[2]&0xFFFF0000,0x16380000)
        early=3+(w[2]&65535)
        self.assertEqual(w[early:],(0x04410003,0xAE220008,0x08000000|(0x33281C>>2),0,0x08000000|(0x33282C>>2),0))
        self.assertEqual((w[14]&0x3FFFFFF)<<2,FRESH_BIND_STUB+56)

    def test_second_slot_capture_preserves_first_call(self):
        blob=texture_stub('rpc-dbc-port1-before');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        early=4+(w[3]&65535)
        self.assertEqual(w[3]&0xFFFF0000,0x13200000)
        self.assertEqual(w[early:],(0x3C1F0031,0x37FF3650,0x08000000|(0x332970>>2),0x0000582D))
        self.assertEqual((w[15]&0x3FFFFFF)<<2,FRESH_BIND_STUB+60)

    def test_card_slice_hook_calls_original_before_reading_programmed_registers(self):
        from capture_sio2_dma import hook
        w=struct.unpack('<'+str(len(hook(0x12340))//4)+'I',hook(0x12340))
        self.assertEqual(w[5:8],(0x8FB90010,0,0xAF190014))
        self.assertEqual(w[11:15],(0x0C000000|(0x12340>>2),0,0x3C18001D,0xAF020018))
        self.assertEqual((w[16],w[20],w[24]),(0x8F391540,0x8F391544,0x8F391548))
        self.assertEqual(w[-2:],(0x1000FFFF,0))

    def test_reverse_dma_hooks_preserve_actual_calls_and_negative_poll(self):
        from capture_dbc_dma import submit_hook,poll_hook,SUBMIT,POLL
        target=0x12340
        w=struct.unpack('<'+str(len(submit_hook(target))//4)+'I',submit_hook(target))
        self.assertEqual(w[-8],0x0C000000|(target>>2))
        self.assertEqual(w[-11:-8],(0x3C19001D,0x37390040,0xAC990000))
        p=struct.unpack('<'+str(len(poll_hook(target))//4)+'I',poll_hook(target))
        self.assertEqual(p[3],0x0C000000|(target>>2))
        self.assertEqual(12+(p[11]&65535),17)
        self.assertEqual(p[15],0x03E00008)

    def test_dbc_hooks_publish_real_returns_without_unrelated_mmio(self):
        for point in ('rpc-dbc-command-before','rpc-dbc-command-after','rpc-dbc-binds-after','rpc-dbc-sema-after','rpc-dbc-init-after'):
            address,trap,marker=capture_trap(point)
            self.assertEqual(address,TEXTURE_POINTS[point][0])
            self.assertEqual(marker,TEXTURE_POINTS[point][1])
            stub=texture_stub(point);w=struct.unpack('<'+str(len(stub)//4)+'I',stub)
            self.assertEqual(w[2:8],(0xFF020008,0xAF1D0010,0xAF040018,0xAF05001C,0xAF060020,0xAF070024))
            self.assertEqual((w[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+40)

    def test_sound_console_stubs_publish_actual_arguments(self):
        for point in ('sound-bind-after','sound-init-before','sound-init-after','deci2-after'):
            blob=texture_stub(point);words=struct.unpack('<'+str(len(blob)//4)+'I',blob)
            self.assertEqual(words[2:8],(0xFF020008,0xAF1D0010,0xAF040018,0xAF05001C,0xAF060020,0xAF070024))
            self.assertEqual((words[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+40)
        for point in ('sound-ee-thread-before','sound-ee-thread-after'):
            blob=texture_stub(point);words=struct.unpack('<'+str(len(blob)//4)+'I',blob)
            self.assertEqual(words[8:10],(0x40196000,0xAF190028))
            self.assertEqual((words[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+48)


    def test_sound_status_hook_preserves_earlier_thread_start(self):
        blob=texture_stub('sound-ee-status-after');w=struct.unpack('<'+str(len(blob)//4)+'I',blob)
        self.assertEqual(w[0],0x24180003)
        early=2+(w[1]&65535)
        self.assertEqual(w[1]&0xFFFF0000,0x16380000)
        self.assertEqual(w[early:early+3],(0x0040802D,0x06000003,0x8FA30000))
        self.assertEqual((w[early+3]&0x3FFFFFF)<<2,0x32FDFC)
        self.assertEqual((w[early+5]&0x3FFFFFF)<<2,0x32FDEC)

    def test_texture_completion_skips_busy_reads(self):
        words=struct.unpack('<'+str(len(texture_stub('texture-complete'))//4)+'I',texture_stub('texture-complete'))
        self.assertEqual(words[0],0x30580100)
        early=2+(words[1]&65535)
        self.assertEqual(words[early],0x30420100)
        self.assertEqual((words[early+1]&0x3FFFFFF)<<2,0x28B9A0)
        for point,(address,marker) in TEXTURE_POINTS.items():
            self.assertEqual(capture_trap(point)[0],address)
            self.assertEqual(capture_trap(point)[2],marker)

    def test_second_csr_hook_preserves_the_first_call(self):
        words=struct.unpack('<'+str(len(graph_csr_stub())//4)+'I',graph_csr_stub())
        self.assertEqual(words[:2],(0x3C1801FF,0x3718ECF0))
        early=3+(words[2]&65535)
        self.assertEqual(words[early],0xA6120002)
        self.assertEqual((words[early+1]&0x3FFFFFF)<<2,0x30FE58)
        self.assertEqual(capture_trap('graph-csr-after')[0],0x30FE54)
    def test_vif_hook_has_a_file_backed_stub_segment(self):
        data=bytearray(116+4096);struct.pack_into('<I',data,28,52);struct.pack_into('<HH',data,42,32,2)
        struct.pack_into('<8I',data,52,1,116,0x100000,0x100000,4096,4096,5,4096)
        struct.pack_into('<8I',data,84,1,0,0,0,0,0,5,4096)
        metadata={'sha256':EXPECTED_SHA,'load_segments':[{'virtual_address':0x100000,'memory_size':4096,'file_size':4096,'flags':5,'file_offset':116}]}
        with patch('capture_ee.inspect',return_value=metadata):prepared,offset=instrument(bytes(data),'vif-startup-after')
        program=struct.unpack_from('<8I',prepared,84);stub=vif_startup_stub()
        self.assertEqual(program[2:6],(FRESH_BIND_STUB,FRESH_BIND_STUB,len(stub),len(stub)))
        self.assertEqual(prepared[program[1]:program[1]+program[4]],stub)
        self.assertEqual(prepared[offset:offset+8],capture_trap('vif-startup-after')[1])
        words=struct.unpack('<'+str(len(stub)//4)+'I',stub)
        self.assertIn(0x4842E000,words);self.assertEqual((words[-2]&0x3FFFFFF)<<2,FRESH_BIND_STUB+(len(words)-2)*4)
    def test_loadfile_traps_do_not_overlap_the_caller(self):
        for point,address,marker in [('loadfile-bind-after',0x337264,0xB737),('loadfile-version-after',0x3372A8,0xB738),('loadfile-module-before',0x337BDC,0xB739),('loadfile-module-after',0x337BE4,0xB73A)]:
            actual,trap,value=capture_trap(point);words=struct.unpack('<8I',trap)
            self.assertEqual((actual,value),(address,marker))
            self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
            self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)
            self.assertLessEqual(address+len(trap),0x337C28)

    def test_authored_trap_encodings(self):
        words = struct.unpack('<6I', TRAP)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), MARKER)
        self.assertEqual(words[3], (0x2b << 26) | (24 << 21) | (25 << 16))
        self.assertEqual((words[4] & 0x03ffffff) << 2, HANDLER + 16)
        self.assertEqual(words[5], 0)

    def test_bind_capture_preserves_request_and_publishes_caller_boundary(self):
        address, trap, marker = capture_trap('sif-bind-after')
        words = struct.unpack('<8I', trap)
        self.assertEqual(address, 0x33284C)
        self.assertEqual(marker, 0xB70F)
        self.assertEqual(words[2:4], (0xAF020004, 0xAF1D0008))
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)
        self.assertEqual(words[7], 0)

    def test_rpc_call_semaphore_capture_boundary(self):
        address, trap, marker = capture_trap('rpc-call-sema')
        words = struct.unpack('<8I', trap)
        self.assertEqual((address, marker), (0x332AD0, 0xB710))
        self.assertEqual(words[2:4], (0xAF020004, 0xAF1D0008))
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)

    def test_wrong_elf_rejected(self):
        with self.assertRaises(ValueError):
            instrument(bytes(52))

    def test_pe_export_lookup_and_missing_symbol(self):
        data = bytearray(1024)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 0x3c, 0x80)
        data[0x80:0x84] = b'PE\0\0'
        struct.pack_into('<HH', data, 0x84, 0x8664, 1)
        struct.pack_into('<H', data, 0x94, 240)
        struct.pack_into('<H', data, 0x98, 0x20b)
        struct.pack_into('<II', data, 0x98+112, 0x1000, 40)
        struct.pack_into('<IIII', data, 0x98+240+8, 512, 0x1000, 512, 512)
        struct.pack_into('<6I', data, 512+16, 1, 1, 1, 0x1040, 0x1044, 0x1048)
        struct.pack_into('<I', data, 576, 0x2000)
        struct.pack_into('<I', data, 580, 0x1050)
        struct.pack_into('<H', data, 584, 0)
        data[592:598] = b'EEmem\0'
        self.assertEqual(export_rva(data, 'EEmem'), 0x2000)
        with self.assertRaises(ValueError):
            export_rva(data, 'missing')
        struct.pack_into('<I', data, 576, 0x1000)
        with self.assertRaises(ValueError):
            export_rva(data, 'EEmem')

    def test_truncated_pe_rejected(self):
        for data in (b'', b'MZ', b'MZ' + bytes(100)):
            with self.assertRaises(ValueError):
                export_rva(data, 'EEmem')

    def test_timer_trap_reads_exact_mmio_and_publishes_payload(self):
        address, trap, marker = capture_trap('timer3-mode')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338a1c)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x10001810)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(words[5], 0xaf190004)
        self.assertEqual(marker, 0xb703)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)

    def test_timer2_trap_reads_reached_register(self):
        address, trap, marker = capture_trap('timer2-mode')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338c58)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x10001010)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(words[5], 0xaf190004)
        self.assertEqual(marker, 0xb704)
        self.assertEqual(words[6], 0x3419b704)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)

    def test_intc_trap_reads_mask_before_enable_syscall(self):
        address, trap, marker = capture_trap('intc-mask')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x32e9a4)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x1000f010)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(marker, 0xb705)
        self.assertEqual(words[6], 0x3419b705)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)

    def test_timer2_readback_trap_at_second_read(self):
        address, trap, marker = capture_trap('timer2-readback')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338dd8)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x10001010)
        self.assertEqual(words[2], 0x8f190000)
        self.assertEqual(words[5], 0xaf190004)
        self.assertEqual(marker, 0xb706)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)

    def test_dma_status_trap_reads_reached_register(self):
        address, trap, marker = capture_trap('dma-status')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x3319CC)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x1000E010)
        self.assertEqual(words[2], 0x8F190000)
        self.assertEqual(words[5], 0xAF190004)
        self.assertEqual(marker, 0xB709)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)

    def test_dma5_control_trap_reads_reached_register(self):
        address, trap, marker = capture_trap('dma5-control')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x3319EC)
        self.assertEqual(((words[0] & 0xffff) << 16) | (words[1] & 0xffff), 0x1000C000)
        self.assertEqual(words[2], 0x8F190000)
        self.assertEqual(marker, 0xB70A)
        self.assertEqual((words[8] & 0x03ffffff) << 2, address + 32)

    def test_sif_dma_traps_publish_result_and_stack(self):
        for point, expected_address, expected_marker in (
                ('sif-dma-before', 0x331D44, 0xB70C), ('sif-dma-after', 0x331D4C, 0xB70D)):
            address, trap, marker = capture_trap(point)
            words = struct.unpack('<8I', trap)
            self.assertEqual((address, marker), (expected_address, expected_marker))
            self.assertEqual(words[2:4], (0xAF020004, 0xAF1D0008))
            self.assertEqual((words[6] & 0x03FFFFFF) << 2, address + 24)
            self.assertEqual(words[7], 0)

    def test_sif_rpc_query_trap(self):
        address, trap, marker = capture_trap('sif-rpc-state')
        words = struct.unpack('<8I', trap)
        self.assertEqual((address, marker), (0x332110, 0xB70E))
        self.assertEqual(words[2:4], (0xAF020004, 0xAF040008))
        self.assertEqual((words[6] & 0x03FFFFFF) << 2, address + 24)

    def test_sif_trap_publishes_return_and_selector(self):
        address, trap, marker = capture_trap('sif-command')
        words = struct.unpack('<8I', trap)
        self.assertEqual(address, 0x331A34)
        self.assertEqual(words[2:4], (0xAF020004, 0xAF040008))
        self.assertEqual(marker, 0xB70B)
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)
        self.assertEqual(words[7], 0)


class OsdCaptureTests(unittest.TestCase):
    def test_osd_trap_reads_returned_stack_word_before_setter(self):
        address, trap, marker = capture_trap('osd-config')
        words = struct.unpack('<8I', trap)
        self.assertEqual(address, 0x3385F4)
        self.assertEqual(words[0], (0x23 << 26) | (29 << 21) | (25 << 16))
        self.assertEqual(((words[1] & 0xffff) << 16) | (words[2] & 0xffff), MARKER)
        self.assertEqual(words[3], 0xAF190004)
        self.assertEqual(marker, 0xB707)
        self.assertEqual(words[5], 0xAF190000)
        self.assertEqual((words[6] & 0x03ffffff) << 2, address + 24)
        self.assertEqual(words[7], 0)


class OsdReadbackCaptureTests(unittest.TestCase):
    def test_trap_after_getter_before_restore(self):
        address, trap, marker = capture_trap('osd-readback')
        words = struct.unpack('<10I', trap)
        self.assertEqual(address, 0x338620)
        self.assertEqual(marker, 0xB708)
        self.assertEqual(words[2], 0x8FB90004)
        self.assertEqual(words[4], 0x8FB90000)
        self.assertEqual(words[3], 0xAF190004)
        self.assertEqual(words[5], 0xAF190008)
        self.assertEqual((words[8] & 0x3ffffff) << 2, address + 32)
        self.assertEqual(words[9], 0)


class McmanReturnCaptureTests(unittest.TestCase):
    def test_unique_caller_return_preserves_module_result_publication(self):
        address,trap,marker=capture_trap('mcman-module-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x100434,0xB73B))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)


class HeapBindReturnCaptureTests(unittest.TestCase):
    def test_unique_return_preserves_result_and_stack(self):
        address,trap,marker=capture_trap('fileio-bind-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x1005A4,0xB747))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)


class HeapAllocationReturnCaptureTests(unittest.TestCase):
    def test_original_allocation_return_is_published(self):
        address,trap,marker=capture_trap('heap-alloc-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x3205D4,0xB748))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)


class HeapCopyReturnCaptureTests(unittest.TestCase):
    def test_trap_follows_the_completed_poll_loop(self):
        address,trap,marker=capture_trap('heap-copy-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x320620,0xB749))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)

class ModuleBufferReturnCaptureTests(unittest.TestCase):
    def test_unique_buffer_load_return(self):
        address,trap,marker=capture_trap('module-buffer-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x320630,0xB74A))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)

class HeapFreeReturnCaptureTests(unittest.TestCase):
    def test_unique_owned_free_return(self):
        address,trap,marker=capture_trap('heap-free-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x320650,0xB74B))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)

class DmacStartupCaptureTests(unittest.TestCase):
    def test_unique_setup_return(self):
        address,trap,marker=capture_trap('dmac-startup-after')
        words=struct.unpack('<8I',trap)
        self.assertEqual((address,marker),(0x1005B4,0xB74C))
        self.assertEqual(words[2:4],(0xFF020008,0xAF1D0010))
        self.assertEqual((words[6]&0x3FFFFFF)<<2,address+24)


class CriRpcCaptureTests(unittest.TestCase):
    def test_other_clients_keep_original_call_prologue(self):
        words=struct.unpack('<'+str(len(texture_stub('rpc-cri-command-before'))//4)+'I',texture_stub('rpc-cri-command-before'))
        self.assertEqual(words[-4:],(0x27BDFF40,0xFFB10030,0x08000000|(0x332978>>2),0))
        target=3+(words[2]&0xFFFF)
        self.assertEqual(target,len(words)-4)
        self.assertIn(0xAF1F0038,words)
        self.assertIn(0xAF080028,words)
    def test_only_cri_client_halts_semaphore_observer(self):
        words=struct.unpack('<'+str(len(texture_stub('rpc-cri-bind-sema'))//4)+'I',texture_stub('rpc-cri-bind-sema'))
        self.assertEqual(words[:2],(0x3C180035,0x37185C70))
        self.assertEqual(words[-6:-4],(0x04410003,0xAE220008))
    def test_unique_before_and_after_sites(self):
        self.assertEqual(capture_trap('rpc-cri-bind-before')[0],0x2FD02C)
        self.assertEqual(capture_trap('rpc-cri-bind-after')[0],0x2FD034)
        self.assertEqual(capture_trap('rpc-cri-command-after')[0],0x2FCB20)


class CriSecondObserverTests(unittest.TestCase):
    def test_counts_only_matching_client_and_resumes_first(self):
        w=struct.unpack('<'+str(len(texture_stub('rpc-cri-second-before'))//4)+'I',texture_stub('rpc-cri-second-before'))
        self.assertEqual(w[8],0xAF19003C)
        self.assertEqual(3+(w[2]&65535),len(w)-4)
        self.assertEqual(11+(w[10]&65535),len(w)-4)
        self.assertEqual(w[-6],0x08000000|((FRESH_BIND_STUB+(len(w)-6)*4)>>2))
