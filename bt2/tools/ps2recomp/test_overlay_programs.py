import importlib.util
import unittest
from unittest import mock

import overlay_programs
from overlay_programs import ROOT, PROGRAMS, program_for, wrap


def inspector():
    spec = importlib.util.spec_from_file_location('bt2_elf', ROOT / 'tools/inspect_elf/inspect_elf.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class OverlayWrapTests(unittest.TestCase):
    """Synthetic images only; no disc or game data."""

    def test_wrapped_elf_has_one_executable_text_at_link_address(self):
        image = bytes(range(256)) * 4
        metadata = inspector().inspect(wrap(image, 0x3B3100, 0x3B3180))
        self.assertEqual(metadata['entry_point'], 0x3B3180)
        text = [s for s in metadata['sections'] if s['name'] == '.text']
        self.assertEqual(len(text), 1)
        self.assertEqual((text[0]['address'], text[0]['size'], text[0]['flags'] & 6), (0x3B3100, len(image), 6))
        loads = metadata['load_segments']
        self.assertEqual([(l['virtual_address'], l['file_size'], l['memory_size']) for l in loads], [(0x3B3100, len(image), len(image))])
        data = wrap(image, 0x3B3100, 0x3B3180)
        offset = text[0]['file_offset']
        self.assertEqual(data[offset:offset + len(image)], image)

    def test_wrap_is_deterministic(self):
        image = b'\x08\x00\xe0\x03' * 8
        self.assertEqual(wrap(image, 0x100000, 0x100000), wrap(image, 0x100000, 0x100000))
        self.assertNotEqual(wrap(image, 0x100000, 0x100000), wrap(image, 0x100000, 0x100004))

    def test_program_ranges(self):
        p = PROGRAMS['dbzp']
        self.assertEqual(program_for(p['base']), 'dbzp')
        self.assertEqual(program_for(p['base'] + p['size'] - 1), 'dbzp')
        self.assertIsNone(program_for(p['base'] - 4))
        self.assertIsNone(program_for(p['base'] + p['size']))
        self.assertIsNone(program_for(None))


class OverlayBindingTests(unittest.TestCase):
    def test_render_registers_exact_body_bytes(self):
        import expand_startup
        image = bytes(range(64))
        with mock.patch.dict(PROGRAMS, {'test': {'base': 0x400000, 'size': 64}}), \
                mock.patch.object(overlay_programs, 'load', return_value=(image, b'')):
            cmake, header = expand_startup.render_overlays({'test': [{'start': 0x400008, 'end': 0x400010}]}, {'test': [(0x40000C, 0x400008)]})
            self.assertIn('test-runtime/FUN_00400008_0x400008.cpp', cmake)
            self.assertIn('bt2_overlay_00400008[] = {8,9,10,11,12,13,14,15};', header)
            self.assertIn('runtime.registerOverlayFunction(0x00400008, FUN_00400008_0x400008, bt2_overlay_00400008);', header)
            self.assertIn('runtime.registerInteriorEntry(0x0040000C, FUN_00400008_0x400008, 0x00400008);', header)
            self.assertLess(header.index('registerOverlayFunction'), header.index('registerInteriorEntry'))
            for start, end in ((0x3FFFFC, 0x400008), (0x400038, 0x400044), (0x400010, 0x400010)):
                with self.assertRaises(ValueError):
                    expand_startup.render_overlays({'test': [{'start': start, 'end': end}]}, {})


if __name__ == '__main__':
    unittest.main()
