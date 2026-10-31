import os,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent))
import msvc
class MsvcTests(unittest.TestCase):
    def test_environment_lines(self):
        text=os.linesep.join(['INCLUDE=C:/a;C:/b','noise','Path=C:/x=y','=hidden=1',''])
        self.assertEqual(msvc.parse_environment(text),{'INCLUDE':'C:/a;C:/b','Path':'C:/x=y'})
    def test_installation_with_ninja_is_preferred(self):
        with tempfile.TemporaryDirectory() as folder:
            roots=[Path(folder)/name for name in ('none','tools','ninja')]
            for root in roots[1:]:
                (root/'VC/Auxiliary/Build').mkdir(parents=True);(root/'VC/Auxiliary/Build/vcvars64.bat').write_text('')
            self.assertEqual(msvc.choose_installation([str(r) for r in roots]),str(roots[1]))
            (roots[2]/msvc.NINJA).mkdir(parents=True);(roots[2]/msvc.NINJA/'ninja.exe').write_text('')
            self.assertEqual(msvc.choose_installation([str(r) for r in roots]),str(roots[2]))
            with self.assertRaises(SystemExit):msvc.choose_installation([str(roots[0])])
if __name__=='__main__':unittest.main()
