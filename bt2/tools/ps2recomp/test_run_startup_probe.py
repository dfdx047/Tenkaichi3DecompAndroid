import tempfile,unittest
from pathlib import Path
from run_startup_probe import INPUTS,command,summary
class StartupProbeTests(unittest.TestCase):
    def test_preflight_and_cpp_argument_order(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);disc=root/'owned.iso'
            with self.assertRaises(ValueError):command(root,disc)
            paths=[root/'build/probe/Debug/bt2-entry-probe.exe',disc,*[root/'work/bt2-generation'/x for x in INPUTS]]
            for path in paths:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'synthetic')
            args=command(root,disc)
            self.assertEqual(len(args),25)
            self.assertEqual(Path(args[2]).name,'entry-stop.json')
            self.assertEqual(args[-2],str(disc));self.assertEqual(Path(args[-1]).name,'ncmd-bind.bin')
    def test_summary_distinguishes_entries_from_events(self):
        report=dict(distinct_ee_entered=8,distinct_ee_completed=6,distinct_kernel_entered=2,distinct_kernel_completed=2,calls=90,returns=85,cri_rpc_calls=42,heap_calls=6,selector=0x77,pc=0x32F028,target=0)
        self.assertIn('8 entered / 6 completed',summary(report))
        self.assertIn('90 / 85',summary(report))
        del report['distinct_ee_entered']
        with self.assertRaises(ValueError):summary(report)
