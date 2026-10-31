import copy,unittest
from verify_timer2 import native_scope

def report():
    """Synthetic logical-time report fields; no captured or game data."""
    return {'timer2_fields':111,'vblank_deliveries':109,'polled_vblanks':2,'timer2_deliveries':16,'timer2_wraps':16,'display_reset_phase':13}

class Timer2ScopeTest(unittest.TestCase):
    def test_accepts_logical_accounting(self):
        native_scope(report())

    def rejects(self,**change):
        r=copy.deepcopy(report());r.update(change)
        with self.assertRaises(Exception):native_scope(r)

    def test_rejects_field_accounting_mismatch(self):
        self.rejects(timer2_fields=110)
        self.rejects(polled_vblanks=3)

    def test_rejects_unacknowledged_or_impossible_wraps(self):
        self.rejects(timer2_deliveries=15)
        self.rejects(timer2_wraps=0,timer2_deliveries=0)
        self.rejects(timer2_wraps=40,timer2_deliveries=40)

    def test_rejects_before_display_setup(self):
        self.rejects(display_reset_phase=12)

if __name__=='__main__':unittest.main()
