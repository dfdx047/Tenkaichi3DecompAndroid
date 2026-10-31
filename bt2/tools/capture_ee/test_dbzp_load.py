import copy,unittest
from verify_dbzp_load import native_scope

def report():
    """Synthetic FILEIO lifecycle fields; no captured or game data."""
    return {'fileio_binds':1,'fileio_calls':6,'fileio_completions':5}

class DbzpLoadScopeTest(unittest.TestCase):
    def test_accepts_completed_load(self):
        native_scope(report())

    def rejects(self,**change):
        r=copy.deepcopy(report());r.update(change)
        with self.assertRaises(Exception):native_scope(r)

    def test_rejects_incomplete_lifecycle(self):
        self.rejects(fileio_calls=5,fileio_completions=4)
        self.rejects(fileio_completions=4)
        self.rejects(fileio_binds=0)

if __name__=='__main__':unittest.main()
