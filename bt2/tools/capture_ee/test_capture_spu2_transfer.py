import struct,unittest
from capture_spu2_transfer import hook
from capture_spu2_initialized import hook as initialized_hook
from capture_cri_low_read import hook as low_hook,loading_hook
class TransferHookTests(unittest.TestCase):
    def test_original_poll_precedes_diagnostic_dma_read(self):
        words=struct.unpack('<'+str(len(hook())//4)+'I',hook())
        self.assertEqual(words[:5],(0x3C18001D,0xAF040004,0x94820000,0,0xAF020008))
        self.assertEqual(words.count(0x94820000),1)
        self.assertIn(0xAF2F10C8,words)
        self.assertIn(0x35EF0200,words) # Read direction, not a write to sound RAM.
        self.assertEqual(words[-4:],(0x24190001,0xAF190000,0x1000FFFF,0))
        self.assertLess(len(hook()),0x200)
    def test_initialized_snapshot_stores_use_owned_scratch_bases(self):
        words=struct.unpack('<19I',initialized_hook())
        stores=[w for w in words if w>>26==43]
        self.assertEqual([(w>>21)&31 for w in stores],[24,24,24,15,24])
        self.assertIn(0xADED0000,words)
        self.assertNotIn(0xAFED0000,words) # Reject a store through the retail return address.
        self.assertEqual(words[13],0x15C0FFF9)
    def test_staged_low_read_preserves_return_context_and_executes_lw_once(self):
        words=struct.unpack('<14I',loading_hook())
        self.assertEqual(words[:3],(0x27BDFFF0,0xAFB80000,0xAFB90004))
        self.assertEqual(words[-5:],(0x8FB80000,0x8FB90004,0x27BD0010,0x03E00008,0x27BD0020))
        terminal=struct.unpack('<18I',low_hook())
        self.assertEqual(terminal.count(0x8C440000),1)
        self.assertEqual(terminal[5:8],(0x8C440000,0,0xAF040014))
