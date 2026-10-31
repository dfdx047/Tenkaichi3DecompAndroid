import struct,unittest
from capture_cri_thread import start_hook,wait_hook,dma_hook,adma_hook,program_hook,transfer_hook,irq_hook,volume_hook
class CriThreadHookTests(unittest.TestCase):
    def test_original_start_is_called_once_and_parent_can_resume(self):
        for terminal in (True,False):
            w=struct.unpack('<16I',start_hook(0x12340,terminal))
            self.assertEqual(sum(x>>26==3 for x in w),1)
            self.assertEqual(w[6],0x0C000000|(0x12340>>2))
            self.assertEqual(w[-4:],(0x24190001,0xAF190000,0x1000FFFF,0) if terminal else (0x8F1F000C,0,0x03E00008,0))
    def test_wait_keeps_original_kernel_target_and_delay(self):
        w=struct.unpack('<7I',wait_hook(0x12340));self.assertEqual(w[-2:],(0x08000000|(0x12340>>2),0x24000008))
    def test_dma_observer_reads_original_control_once(self):
        w=struct.unpack('<14I',dma_hook());self.assertEqual(w[1:4],(0x8E620000,0,0xAF020034))
        self.assertEqual(w.count(0x8E620000),1)
    def test_adma_terminal_only_records_requested_value(self):
        w=struct.unpack('<12I',adma_hook());self.assertEqual(w[1:3],(0xAF020034,0xAF100038))
        self.assertNotIn(0xA6020000,w)

    def test_program_preserves_displaced_delay_and_store(self):
        w=struct.unpack('<22I',program_hook());self.assertEqual(w[:2],(0x02801021,0xAFA30010))
        self.assertEqual(w[3],0xAF030034)
    def test_transfer_calls_original_export_once(self):
        w=struct.unpack('<18I',transfer_hook(0x84860))
        self.assertEqual(sum(x>>26==3 for x in w),1);self.assertEqual(w[8],0x0C000000|(0x84860>>2))
        self.assertEqual(w[11],0xAF020050)
    def test_irq_observer_does_not_issue_hardware_writes(self):
        w=struct.unpack('<22I',irq_hook())
        self.assertEqual(w.count(0x8F2A10C8),1);self.assertNotIn(0x0C000000|(0x85420>>2),w)
    def test_volume_executes_store_once_and_returns_or_halts_after_two(self):
        w=struct.unpack('<32I',volume_hook());self.assertEqual(w.count(0xA4450000),1)
        for pc,target in ((3,8),(6,30),(23,30)):
            self.assertEqual(pc+1+(w[pc]&0xFFFF),target)
        self.assertEqual(w[11],0x00194100);self.assertEqual(w[-2:],(0x03E00008,0))
