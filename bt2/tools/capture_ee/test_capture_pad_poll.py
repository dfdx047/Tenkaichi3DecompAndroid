import struct,unittest
from capture_pad_poll import hook
class PadHookTests(unittest.TestCase):
    def test_preserves_stack_and_registers(self):
        words=struct.unpack('<'+str(len(hook())//4)+'I',hook())
        self.assertEqual(words[:4],(0x27BDFFF0,0xAFA30000,0xAFA80004,0xAFA90008))
        self.assertEqual(words[-6:],(0x8FA30000,0x8FA80004,0x8FA90008,0x27BD0010,0x03E00008,0))
    def test_reads_fifo_once_and_stops_at_count(self):
        for count in (5,10):
            words=struct.unpack('<'+str(len(hook(count))//4)+'I',hook(count))
            self.assertEqual(words.count(0x90428264),1)
            self.assertIn(0x2D090000|count,words)
            branch=words.index(0x15200003)
            self.assertEqual(words[branch+4],0x8FA30000)
            self.assertEqual(words[branch+2],0x1000FFFF)

    def test_dma_register_addresses(self):
        words=struct.unpack('<'+str(len(hook())//4)+'I',hook())
        for offset in (0x10F0,0x1570):
            i=words.index(0x8D290000|offset)
            self.assertEqual(words[i-1],0x3C09BF80)
        for offset in (0x8268,0x8280):
            i=words.index(0x8D290000|offset)
            self.assertEqual(words[i-1],0x3C09BF81)
