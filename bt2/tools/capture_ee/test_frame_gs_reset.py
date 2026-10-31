import unittest
from verify_frame_gs_reset import GS_STATE, state


class FrameGsResetStateTests(unittest.TestCase):
    def test_state_decodes_mode_revision_and_handler(self):
        ram = bytearray(GS_STATE + 16)
        ram[GS_STATE:GS_STATE + 16] = bytes.fromhex('0100020000001b0001000000' + '05000000')
        self.assertEqual(state(bytes(ram)), ([1, 2, 0, 0x1B], (1, 5)))


if __name__ == '__main__':
    unittest.main()
