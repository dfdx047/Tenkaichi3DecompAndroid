import copy,unittest
from verify_sound_bank import native_scope

def report():
    """Synthetic sound-bank report fields; no captured or game data."""
    return {'display_reset_phase':13,'iop_uploads':3,'heap_calls':9,'sound_rpc_calls':3,'native_iop_error':''}

class SoundBankScopeTest(unittest.TestCase):
    def test_accepts_completed_bank_blocks(self):
        native_scope(report())

    def rejects(self,**change):
        r=copy.deepcopy(report());r.update(change)
        with self.assertRaises(Exception):native_scope(r)

    def test_rejects_incomplete_loader(self):
        self.rejects(iop_uploads=2)
        self.rejects(heap_calls=8)
        self.rejects(sound_rpc_calls=2)

    def test_rejects_native_failure_or_early_phase(self):
        self.rejects(native_iop_error='failed')
        self.rejects(display_reset_phase=12)

if __name__=='__main__':unittest.main()
