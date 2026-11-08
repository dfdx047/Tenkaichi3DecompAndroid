import struct,unittest
from capture_spu2_status import hook
class Spu2HookTests(unittest.TestCase):
    def test_reads_original_status_once_and_publishes_address(self):
        words=struct.unpack('<'+str(len(hook())//4)+'I',hook())
        self.assertEqual(words[:5],(0x3C18001D,0xAF020004,0x94420000,0,0xAF020008))
        self.assertEqual(words.count(0x94420000),1)
        self.assertEqual(words[-4:],(0x24190001,0xAF190000,0x1000FFFF,0))
        self.assertLessEqual(len(hook()),0x100)

    def test_interrupt_hook_calls_kernel_once_and_restores_return_context(self):
        from capture_spu2_interrupts import hook as interrupt_hook
        words=struct.unpack('<41I',interrupt_hook(0x12340))
        self.assertEqual(words[:6],(0x27BDFFD0,0xAFBF002C,0xAFA40000,0xAFA50004,0x0C0048D0,0))
        self.assertEqual(sum(w>>26==3 for w in words),1)
        self.assertEqual(words[29:34],(0x24030003,0x17230003,0,0x1000FFFF,0))
        self.assertEqual(words[-7:],(0x8FB80008,0x8FB9000C,0x8FA30010,0x8FBF002C,0x27BD0030,0x03E00008,0))
