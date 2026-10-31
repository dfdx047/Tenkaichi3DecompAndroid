import copy,unittest
from verify_display_reset import native_scope

def report():
    """Synthetic display-setup report fields; no captured or game data."""
    return {'display_reset_phase':13,'polled_vblanks':2,'vblank_handler_registrations':2,'vblank_enable_calls':2,
        'vblank_disable_calls':1,'heap_calls':7,'heap_frees':2,'native_module_loads':15,'cri_rpc_calls':42,'native_iop_error':''}

class DisplayResetScopeTest(unittest.TestCase):
    def test_accepts_completed_display_setup(self):
        native_scope(report())

    def rejects(self,**change):
        r=copy.deepcopy(report());r.update(change)
        with self.assertRaises(Exception):native_scope(r)

    def test_rejects_incomplete_or_extra_phases(self):
        self.rejects(display_reset_phase=12)
        self.rejects(polled_vblanks=1)
        self.rejects(polled_vblanks=3)

    def test_rejects_handler_replacement_mismatch(self):
        self.rejects(vblank_handler_registrations=1)
        self.rejects(vblank_enable_calls=3)
        self.rejects(vblank_disable_calls=0)

    def test_rejects_reload_lifecycle_mismatch(self):
        self.rejects(heap_calls=6)
        self.rejects(heap_frees=1)
        self.rejects(native_module_loads=14)
        self.rejects(native_iop_error='failed')

if __name__=='__main__':unittest.main()
