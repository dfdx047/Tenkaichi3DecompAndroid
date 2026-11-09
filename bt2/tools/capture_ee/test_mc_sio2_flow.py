"""Observers must retain real DMA execution and bounded scratch ownership."""
import struct,unittest
from capture_sio2_flow import hook,send_hook,control_hook

def words(b):return struct.unpack('<'+str(len(b)//4)+'I',b)
class CardFlowTests(unittest.TestCase):
    def test_start_observer_tail_calls_actual_api(self):
        target=0x75B4;w=words(send_hook(target))
        self.assertEqual(w[-2:],(0x08000000|(target>>2),0))
        for branch in (1,7):
            self.assertEqual(branch+1+(w[branch]&0xFFFF),len(w)-2)
        self.assertIn(0x2F230040,w) # Bound trace writes to 64 owned records.
    def test_control_observer_stores_device_register_and_bounds_trace(self):
        w=words(control_hook());self.assertEqual(w[:3],(0x3C02BF80,0x34428268,0xAC440000))
        self.assertEqual(7+1+(w[7]&0xFFFF),len(w)-2)
        self.assertIn(0x2F230020,w) # No overlap with the send observer region.
        self.assertEqual(words(hook(0))[-2:],(0x1000FFFF,0))
if __name__=='__main__':unittest.main()
