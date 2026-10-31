"""Disc discovery stays inside the selected installation."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import native_play as play


class LauncherTests(unittest.TestCase):
    def imported(self, root, name, game='bt3'):
        folder = root / name
        resource = folder / ('pzs4us1/00003.bin' if game == 'bt4' else 'pzs3us1/00000.bin')
        resource.parent.mkdir(parents=True)
        resource.touch()
        (folder / 'native-disc.json').write_text(json.dumps(dict(game=game, region='eu', revision='b14-rev2')))
        return folder

    def test_one_disc_launches_without_picker(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            self.imported(root, 'gamedata')
            exe = root / ('port/build/bt3.exe' if sys.platform == 'win32' else 'port/build/bt3_64')
            exe.parent.mkdir(parents=True)
            exe.touch()
            with patch.object(play, 'ROOT', root), patch.object(play.subprocess, 'call', return_value=0) as run, patch('tkinter.Tk', side_effect=AssertionError('Unexpected picker')):
                with self.assertRaises(SystemExit) as result:
                    play.main()
                self.assertEqual(result.exception.code, 0)
                self.assertEqual(run.call_args.kwargs['env']['BT3_DATA'], str(root / 'gamedata'))

    def test_only_compatible_local_imports_count(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / 'selected'
            self.imported(root, 'gamedata')
            self.imported(Path(d) / 'other-install', 'gamedata', 'bt4')
            bad = root / 'gamedata-incomplete'
            bad.mkdir()
            (bad / 'native-disc.json').write_text('{"game":"bt4","revision":"b14-rev2"}')
            invalid = root / 'gamedata-invalid'
            invalid.mkdir()
            (invalid / 'native-disc.json').write_text('unfinished')
            self.assertEqual(len(play.compatible_games(root)), 1)
            self.imported(root, 'gamedata-bt4', 'bt4')
            self.assertEqual(len(play.compatible_games(root)), 2)


if __name__ == '__main__':
    unittest.main()
