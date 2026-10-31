import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import battle_run,run_gate
class GateTests(unittest.TestCase):
    def test_every_phase_verifier_exists_and_is_not_run_on_the_full_report(self):
        full=run_gate.full_run_verifiers()
        for name in run_gate.phase_only():
            self.assertTrue((run_gate.ROOT/'tools/capture_ee'/f'verify_{name}.py').is_file(),name)
            self.assertNotIn(name,full)
        self.assertTrue(full)
        for name in run_gate.NOT_IN_GATE:
            self.assertTrue((run_gate.ROOT/'tools/capture_ee'/f'verify_{name}.py').is_file(),name)
            self.assertNotIn(name,full)
    def test_verifier_arguments(self):
        disc=Path('disc.iso');report=Path('report.json')
        self.assertEqual(run_gate.verifier_command('timer2',report,disc)[2:],['--native','report.json'])
        self.assertEqual(run_gate.verifier_command('disc_read',report,disc)[2:],['--native','report.json','--disc','disc.iso','--workspace','0x1D4B00'])
        self.assertEqual(run_gate.verifier_command('ee_sound_dispatch',report,disc)[-1],'--queued')
    def test_battle_run_is_judged_against_its_baseline(self):
        good=dict(battle_run.BASELINE,frame_budget_reached=1,gs_display_flips=battle_run.FRAMES,vu1_native_runs=7,vu1_verified_runs=7,distinct_ee_entered=battle_run.ENTERED_AT_LEAST)
        self.assertEqual(battle_run.judge(good),[])
        self.assertEqual(battle_run.judge(dict(good,vu1_native_runs=0,vu1_verified_runs=0)),[]) # A build without native programs.
        for change in ({'frame_budget_reached':0,'gs_display_flips':812},{'vu1_kick_hash':'0'},{'vu1_instructions':1},{'vu1_runs':1},
                       {'vu1_verified_runs':6},{'distinct_ee_entered':battle_run.ENTERED_AT_LEAST-1},
                       {'ee_interpreted_instructions':5,'ee_interpreted_entries':[{'entry':0x1000}]}):
            self.assertEqual(len(battle_run.judge(dict(good,**change))),1,change)
        self.assertTrue((Path(battle_run.__file__).with_name('pad-fight.txt')).is_file())
if __name__=='__main__':unittest.main()
