"""Negative completion evidence and capture instrumentation bounds."""
import unittest
from verify_sound_rpc import completed_card_query
from capture_iop_delay import hook as delay_hook
from capture_card_alarm import hook as alarm_hook
from capture_sio2_flow import count_hook,fifo_hook

class TimedEvidenceTests(unittest.TestCase):
    def test_observers_stay_in_owned_code_windows(self):
        self.assertLessEqual(len(count_hook(0x1000)),0x80)
        self.assertLessEqual(len(fifo_hook()),0x80)
        self.assertLessEqual(len(delay_hook(0x1000)),0x300)
        self.assertLessEqual(len(alarm_hook(0x1000)),0x300)

if __name__=='__main__':unittest.main()
