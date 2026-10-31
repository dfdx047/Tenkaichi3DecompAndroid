import copy,unittest
from verify_startup_workers import native_scope

def report():
    """Synthetic normal-startup scheduler fields; no captured or game data."""
    return {
        'queued_sound_submissions':1,'queued_sound_completions':1,'sounds_rpc_binds':1,'sound_thread_waiting':1,'sound_dispatch_bind_id':29,
        'sound_dispatch_wait_ra':0x332874,'sound_dispatch_wait_sp':0x1FFECB0,'ee_sound_probe_attempted':0,'ee_sound_probe_waits':0,
        'ee_sound_probe_parent_pc':0,'ee_sound_bind_returns':1,'pending_ee_bind':[0]*5,'native_iop_error':'',
        'ee_sound_context':[3,0,0x32EB88,0x38BDF0,0x3A6C80,0x333098,0,0x10001],
        'ee_sound_probe_frontier':[0x32EB88,0x38BDF0,0x3A6C80,0x333098,0x3A6D00,0x10001],
        'sound_thread_words':[0]*7+[1,0],'sound_iop_context':[0xA1304,0xA0F24,0xA94E0,0x1E7588,0],
        'main_priority_admissions':1,'main_priority_previous':2,'startup_scheduler_active':0,'startup_dispatch_parent_pc':0x32EAF8,
        'startup_worker_dispatches':2,'startup_worker_waits':4,'worker_resumes':2,'scheduled_resumes':11,'scheduled_suspends':11,
        'scheduled_priority_changes':22,'cri_stream_transfers':2,
        'startup_worker_state':[[4,0x2EDE58,0,1,8,0x38BDF0,0x34C470,0x100250,0],
            [5,0x2EDF48,0,0,16,0x38BDF0,0x34E430,0x2EE00C,1],[6,0x2EE050,0,0,18,0x38BDF0,0x34F430,0x2EE100,1],
            [7,0x2EE140,0,1,25,0x38BDF0,0x351470,0x100250,0]],
    }

class StartupWorkerScopeTest(unittest.TestCase):
    def test_accepts_admitted_scope(self):
        native_scope(report())

    def rejects(self,change):
        r=copy.deepcopy(report());change(r)
        with self.assertRaises(Exception):native_scope(r)

    def test_rejects_pending_or_wrong_admission(self):
        self.rejects(lambda r:r.update(main_priority_admissions=0))
        self.rejects(lambda r:r.update(main_priority_previous=24))
        self.rejects(lambda r:r.update(startup_scheduler_active=1))

    def test_rejects_unbalanced_critical_sections(self):
        self.rejects(lambda r:r.update(scheduled_suspends=10))
        self.rejects(lambda r:r.update(scheduled_priority_changes=21))

    def test_accepts_worker7_first_dispatch(self):
        # Frame phase: main resumes worker 7 once; it serves the callbacks and parks in its first sleep.
        r=report();r.update(startup_worker_dispatches=3,startup_worker_waits=5)
        r['startup_worker_state'][3]=[7,0x2EE140,0,0,25,0x38BDF0,0x351420,0x2EE1D8,1]
        native_scope(r)

    def test_rejects_helper_wait_or_state_mismatch(self):
        self.rejects(lambda r:r['startup_worker_state'][1].__setitem__(6,0x34E370))
        self.rejects(lambda r:r['startup_worker_state'][2].__setitem__(4,1))
        self.rejects(lambda r:r['startup_worker_state'][0].__setitem__(3,0))
        self.rejects(lambda r:r.update(startup_worker_waits=1))

if __name__=='__main__':unittest.main()
