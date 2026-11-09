"""Reject partial or mixed queued/diagnostic sound completion evidence."""
import unittest
from verify_sound_rpc import queued_sound_bind_completed,completed_card_query

class QueuedSoundEvidenceTests(unittest.TestCase):
    def profile(self):
        return dict(queued_sound_submissions=1,queued_sound_completions=1,sounds_rpc_binds=1,sound_thread_waiting=1,
            sound_dispatch_bind_id=29,sound_dispatch_wait_ra=0x332874,sound_dispatch_wait_sp=0x1FFECB0,
            ee_sound_probe_attempted=0,ee_sound_probe_waits=0,ee_sound_probe_parent_pc=0,
            ee_sound_context=[3,0,0x32EB88,0x38BDF0,0x3A6C80,0x333098,0,0x10001],
            ee_sound_probe_frontier=[0x32EB88,0x38BDF0,0x3A6C80,0x333098,0x3A6D00,0x10001])
    def test_complete_owned_profile(self):
        self.assertTrue(queued_sound_bind_completed(self.profile()))
    def test_each_required_field_is_required(self):
        for key in self.profile():
            with self.subTest(key=key):
                r=self.profile();del r[key];self.assertFalse(queued_sound_bind_completed(r))
    def test_partial_duplicate_and_diagnostic_states_rejected(self):
        for key,value in [('queued_sound_submissions',2),('queued_sound_completions',0),('sounds_rpc_binds',0),
                ('sound_dispatch_bind_id',30),('sound_dispatch_wait_ra',0x32EAF8),
                ('sound_dispatch_wait_sp',0x1FFEFB0),('ee_sound_probe_attempted',1),('ee_sound_probe_waits',1)]:
            with self.subTest(key=key):
                r=self.profile();r[key]=value;self.assertFalse(queued_sound_bind_completed(r))
    def test_child_cpu_and_frontier_each_word(self):
        for key in ['ee_sound_context','ee_sound_probe_frontier']:
            for index in range(len(self.profile()[key])):
                r=self.profile();r[key][index]^=1
                with self.subTest(key=key,index=index):self.assertFalse(queued_sound_bind_completed(r))
    def test_card_query_needs_native_requests_result_and_queued_bind(self):
        # MCSERV init and the startup query, then one native request per frame-phase query.
        r=dict(mc_rpc_binds=1,mc_rpc_calls=2,mc_frame_queries=3,native_card_requests=5,mc_result_words=[0xFFFFFFF5,0,0])
        self.assertFalse(completed_card_query(r));r.update(self.profile());self.assertTrue(completed_card_query(r))
        for key,value in [('native_card_requests',4),('mc_rpc_calls',1),('mc_result_words',[0,0,0]),('queued_sound_completions',0)]:
            with self.subTest(key=key):self.assertFalse(completed_card_query(dict(r,**{key:value})))
