import tempfile, unittest
from pathlib import Path
from profile_report import read_map, report

MAP = """ probe

 Timestamp is 0 (synthetic)

 Preferred load address is 0000000140000000

 Start         Length     Name                   Class
 0001:00000000 00001000H .text                   CODE

  Address         Publics by Value              Rva+Base               Lib:Object

 0000:00000000       __guard_flags              0000000000000100     <absolute>
 0001:00000000       first                      0000000140001000 f   a.obj
 0001:00000040       second                     0000000140001040 f   a.obj
 0001:00000100       third                      0000000140001100 f   b.obj
"""


class ProfileReport(unittest.TestCase):
    def test_names_places_and_sums_other_modules(self):
        with tempfile.TemporaryDirectory() as folder:
            map_path = Path(folder) / 'probe.map'; map_path.write_text(MAP)
            profile = Path(folder) / 'run.profile'
            profile.write_text('samples 10\nprobe.exe 1000 2 0\nprobe.exe 103f 1 0\nprobe.exe 1040 3 0\nprobe.exe 2000 1 0\nother lib.dll 0 1 0\nother lib.dll 0 1 1041\nprobe.exe 10 1 0\n')
            symbols = read_map(map_path)
            self.assertEqual(symbols, [(0x1000, 'first'), (0x1040, 'second'), (0x1100, 'third')])  # Absolute symbols are left out.
            total, counts = report(profile, symbols, 'PROBE.EXE')
            self.assertEqual(total, 10)
            self.assertEqual(dict(counts), {'first': 3, 'second': 3, 'third': 1, '[other lib.dll]': 1, '[other lib.dll] <- second': 1, '[before the first symbol]': 1})

    def test_refuses_a_file_that_is_not_a_map(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'x.map'; path.write_text('nothing here\n')
            with self.assertRaises(ValueError): read_map(path)


if __name__ == '__main__':
    unittest.main()
