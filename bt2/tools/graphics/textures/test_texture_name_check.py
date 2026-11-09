import tempfile
import unittest
from pathlib import Path

import texture_name_check


class TextureNameCheckTest(unittest.TestCase):
    def test_names_found_by_folder_and_texture_hash(self):
        with tempfile.TemporaryDirectory() as d:
            d = Path(d)
            names = d / 'names.txt'
            names.write_text('# uploaded_pixels 10\n1a-2b-00001a53 3\n1a-ff-00001a53 1\n9c-00000000 2\n', encoding='utf-8')
            pack = d / 'pack' / 'replacements'
            (pack / 'Stages').mkdir(parents=True)
            (pack / 'Stages' / '1a-2b-00001a53.dds').write_bytes(b'')
            (pack / '9c-00000000.png').write_bytes(b'')
            (pack / 'notes.txt').write_bytes(b'')
            drawn, notes = texture_name_check.read_names(names)
            self.assertEqual(drawn, {'1a-2b-00001a53': 3, '1a-ff-00001a53': 1, '9c-00000000': 2})
            self.assertEqual(notes, ['uploaded_pixels 10'])
            index = texture_name_check.pack_index(d / 'pack')
            self.assertEqual(index, {'1a-2b-00001a53': 'Stages', '9c-00000000': '.'})
            found, by_texture = texture_name_check.check(drawn, index)
            self.assertEqual(found, {'1a-2b-00001a53': 'Stages', '9c-00000000': '.'})
            self.assertEqual(by_texture, 3)  # The second name differs in its palette only.

    def test_old_tcc_bit_and_names_without_palette(self):
        with tempfile.TemporaryDirectory() as d:
            pack = Path(d) / 'replacements'
            pack.mkdir()
            (pack / '1a-2b-00005a53.dds').write_bytes(b'')  # Bit 14 set, as older PCSX2 versions wrote it.
            (pack / '3c-00001a54.dds').write_bytes(b'')     # No palette part: every palette.
            index = texture_name_check.pack_index(d)
            self.assertEqual(set(index), {'1a-2b-00001a53', '3c-00001a54'})
            found, _ = texture_name_check.check({'1a-2b-00001a53': 1, '3c-77-00001a54': 1, '3c-77-00001a53': 1}, index)
            self.assertEqual(set(found), {'1a-2b-00001a53', '3c-77-00001a54'})


if __name__ == '__main__':
    unittest.main()
