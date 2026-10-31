import copy,unittest
from verify_overlay_dispenv import native_scope

def report():
    """Synthetic overlay frame report fields; no captured or game data."""
    return {'gs_display_flips':1,'gs_display_writes':5,'overlay_dispatch_validations':2,'timer1_starts':1,'fileio_calls':6,'native_iop_error':''}

class OverlayDispenvScopeTest(unittest.TestCase):
    def test_accepts_first_flip(self):
        native_scope(report())

    def rejects(self,**change):
        r=copy.deepcopy(report());r.update(change)
        with self.assertRaises(Exception):native_scope(r)

    def test_rejects_incomplete_or_repeated_flip(self):
        self.rejects(gs_display_flips=0)
        self.rejects(gs_display_writes=4)

    def test_accepts_later_display_writes(self):
        r=report();r.update(gs_display_writes=11);native_scope(r)

    def test_rejects_missing_overlay_or_timer_or_failure(self):
        self.rejects(overlay_dispatch_validations=1)
        self.rejects(timer1_starts=0)
        self.rejects(fileio_calls=5)

if __name__=='__main__':unittest.main()
