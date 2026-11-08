import tempfile,unittest
from pathlib import Path
import battle_sweep


class BattleSweepTests(unittest.TestCase):
    def test_numbers(self):
        self.assertEqual(battle_sweep.numbers('0-3,7,9-10'),[0,1,2,3,7,9,10])
        self.assertEqual(battle_sweep.numbers('22'),[22])

    def test_cases_take_the_next_character_and_the_stages_in_turn(self):
        self.assertEqual(battle_sweep.cases([0,1,2],[0],None,[9,22]),['0:0,1,9','1:0,2,22','2:0,3,9'])
        self.assertEqual(battle_sweep.cases([4],[0,1],70,[22]),['4:0,70,22','4:1,70,22'])

    def test_name(self):
        self.assertEqual(battle_sweep.name('0:2,70,22'),'c0v2-c70-s22')
        self.assertEqual(battle_sweep.name('4:1,70:3,9,2'),'c4v1-c70v3-s9')

    def test_stop_line(self):
        lines=['Snapshot: flip 1 at field 2','Bootstrap execution budget exceeded; no display flip for 600 fields','Distinct EE entries: 1']
        self.assertEqual(battle_sweep.stop_line(lines),lines[1])
        self.assertEqual(battle_sweep.stop_line(['[INFO]: error in driver','all fine']),'')

    def test_summary_round_trip(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'summary.tsv'
            rows={'0:0,1,22':{'case':'0:0,1,22','result':'ok','flips':4500,'entered':3600,'interpreted':0,'seconds':40,'stop':''}}
            battle_sweep.write_summary(path,rows)
            self.assertEqual(battle_sweep.read_summary(path)['0:0,1,22']['result'],'ok')
            self.assertEqual(battle_sweep.read_summary(Path(folder)/'none.tsv'),{})


if __name__=='__main__':unittest.main()
