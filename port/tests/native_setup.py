"""Installer failures must leave existing installations and saves untouched."""
from pathlib import Path
import sys,tempfile,unittest
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

    def test_installed_language_uses_native_text_setting_format(self):
        for language in ('en', 'es'):
            with self.subTest(language=language), tempfile.TemporaryDirectory() as d:
                root=Path(d);target=root/'game';iso=root/'game.iso';iso.write_bytes(b'fake')
                payload=root/'payload';(payload/'engine').mkdir(parents=True);(payload/'engine/TagTeam.dat').write_bytes(b'fake')
                with patch.object(setup.native_tables,'identify',return_value=({'id':'test'},[],[])), \
                     patch.object(setup.native_tables,'reconstruct',return_value=[]), \
                     patch.object(setup.native_tables,'verify'),patch.object(setup,'payload_files',return_value={}), \
                     patch.object(setup,'extract',side_effect=lambda _, out:out.mkdir()):
                    setup.install(iso,target,language,payload=payload,progress=lambda _:None)
                # Port_SettingText reads quoted strings, unlike numeric settings.
                self.assertIn(f'menu_lang="{language}"\n',(target/'settings.txt').read_text())

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
