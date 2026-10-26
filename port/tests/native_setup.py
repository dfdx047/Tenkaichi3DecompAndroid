"""Installer failures must leave existing installations and saves untouched."""
from pathlib import Path
import hashlib,json,os,struct,subprocess,sys,tempfile,unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'port/tools'))
import native_setup as setup
import native_save as saves
from mc_extract import Card


class SetupTests(unittest.TestCase):
    def test_existing_install_is_preserved(self):
        with tempfile.TemporaryDirectory() as d:
            target=Path(d)/'game';target.mkdir();keep=target/'keep';keep.write_bytes(b'existing')
            with self.assertRaises(ValueError):setup.install('missing.iso',target)
            self.assertEqual(keep.read_bytes(),b'existing')

    def test_invalid_save_is_rejected_before_writing(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/saves.CANONICAL;p.write_bytes(b'invalid')
            with self.assertRaises(ValueError):saves.collect(p)
            self.assertEqual(p.read_bytes(),b'invalid')

    def test_native_save_is_copied_and_custom_equipment_kept(self):
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'source';source.mkdir()
            file=source/saves.CANONICAL;file.write_bytes(bytes(range(256))*64)
            custom=b'BT4CUST1'+bytes(153*56);(source/'native-bt4-custom.bin').write_bytes(custom)
            target=Path(d)/'new';saves.write(saves.collect(source),target)
            self.assertEqual((target/'card1'/saves.CANONICAL/saves.CANONICAL).read_bytes(),file.read_bytes())
            self.assertEqual((target/'native-bt4-custom.bin').read_bytes(),custom)

    def test_invalid_card_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            file=Path(d)/'card.ps2';file.write_bytes(bytes(8192))
            with self.assertRaises(ValueError):Card(file)

    def test_language_and_platform_launcher(self):
        for platform, language in ((p,l) for p in ('windows','linux') for l in ('en','es')):
            with self.subTest(platform=platform,language=language), tempfile.TemporaryDirectory() as d:
                root=Path(d);target=root/'game';iso=root/'game.iso';iso.write_bytes(b'fake')
                payload=root/'payload';(payload/'engine').mkdir(parents=True);(payload/'engine/TagTeam.dat').write_bytes(b'fake')
                executable='engine/TagTeam.exe' if platform=='windows' else 'engine/TagTeam'
                (payload/executable).write_bytes(b'fake engine')
                files={executable:'fixture'}
                if platform=='windows':
                    (payload/'Play.exe').write_bytes(b'fake launcher');files['Play.exe']='fixture'
                with patch.object(setup.native_tables,'identify',return_value=({'id':'test'},[],[])), \
                     patch.object(setup.native_tables,'reconstruct',return_value=[]), \
                     patch.object(setup.native_tables,'verify'),patch.object(setup,'payload_files',return_value=files), \
                     patch.object(setup,'platform_name',return_value=platform), \
                     patch.object(setup,'extract',side_effect=lambda _, out:out.mkdir()):
                    setup.install(iso,target,language,payload=payload,progress=lambda _:None)
                # Port_SettingText reads quoted strings, unlike numeric settings.
                self.assertIn(f'menu_lang="{language}"\n',(target/'settings.txt').read_text())
                launcher=target/('Play.exe' if platform=='windows' else 'Play.sh')
                self.assertTrue(launcher.is_file())
                self.assertFalse((target/'Play.cmd').exists())
                self.assertEqual(json.loads((target/'installation.json').read_text())['platform'],platform)
                if platform=='linux' and os.name!='nt':
                    self.assertTrue(os.access(launcher,os.X_OK))
                    self.assertTrue(os.access(target/executable,os.X_OK))

    def test_missing_linux_library_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            payload=Path(d);(payload/'engine').mkdir()
            names=('engine/TagTeam','engine/TagTeam.mem','engine/TagTeam.dat')
            for name in names:(payload/name).write_bytes(b'fixture')
            manifest={name:hashlib.sha256(b'fixture').hexdigest() for name in names}
            (payload/'manifest.json').write_text(json.dumps(manifest))
            with patch.object(setup,'platform_name',return_value='linux'):
                with self.assertRaisesRegex(ValueError,'Incomplete'):setup.payload_files(payload)

    def test_packaging_rejects_missing_table_ranges(self):
        with tempfile.TemporaryDirectory() as d:
            recipe=Path(d)/'recipes.json'
            recipe.write_text(json.dumps({'profiles':[{'id':'test','ranges':[[0,10,4,'',[]],[0,14,4,'',[]]]}]}))
            with patch.object(setup.native_tables,'RECIPES',recipe):
                covered=b'BT3D'+struct.pack('<IQ4I',1,0,100,0,10,8)
                setup.native_tables.check_recipe_coverage(covered)
                missing=b'BT3D'+struct.pack('<IQ4I',1,0,100,0,10,9)
                with self.assertRaisesRegex(ValueError,'Rebuild'):setup.native_tables.check_recipe_coverage(missing)

    @unittest.skipIf(os.name=='nt','POSIX launcher')
    def test_linux_launcher_handles_spaces_and_clears_test_flags(self):
        with tempfile.TemporaryDirectory(prefix='tag team ') as d:
            root=Path(d);(root/'engine').mkdir()
            exe=root/'engine/TagTeam'
            exe.write_text('#!/bin/sh\nprintf "%s\\n" "$PWD" "$BT3_DATA" "${BT3_DEMO-unset}" "$BT3_TAG_TEAM" "$1"\n')
            exe.chmod(0o755)
            launcher=root/'Play.sh';launcher.write_text(setup.PLAY_LINUX);launcher.chmod(0o755)
            result=subprocess.run([str(launcher),'space argument'],cwd='/',env={**os.environ,'BT3_DEMO':'1'},capture_output=True,text=True,check=True)
            self.assertEqual(result.stdout.splitlines(),[str(root),str(root/'gamedata'),'unset','1','space argument'])

    def test_failed_import_cleans_only_its_temporary_folder(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);target=root/'game';iso=root/'game.iso';iso.write_bytes(b'fake')
            payload=root/'payload';(payload/'engine').mkdir(parents=True);(payload/'engine/TagTeam.dat').write_bytes(b'fake')
            sibling=root/'keep';sibling.write_bytes(b'existing')
            with patch.object(setup.native_tables,'identify',return_value=({'id':'test'},[],[])), \
                 patch.object(setup.native_tables,'reconstruct',return_value=[]), \
                 patch.object(setup.native_tables,'verify'),patch.object(setup,'payload_files',return_value={}), \
                 patch.object(setup,'extract',side_effect=ValueError('bad archive')):
                with self.assertRaises(ValueError):setup.install(iso,target,payload=payload,progress=lambda _:None)
            self.assertFalse(target.exists());self.assertEqual(sibling.read_bytes(),b'existing')
            self.assertFalse(list(root.glob('.tag-team-install-*')))


if __name__=='__main__':unittest.main()
