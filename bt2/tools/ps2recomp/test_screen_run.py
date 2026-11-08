import unittest
import screen_run


class PadScriptTest(unittest.TestCase):
    def test_crosses_only(self):
        self.assertEqual(screen_run.pad_script(1100,120),'800:cross,808:-,920:cross,928:-,1040:cross,1048:-')

    def test_down_replaces_overlapping_crosses(self):
        steps=dict(s.split(':') for s in screen_run.pad_script(1500,120,400).split(','))
        self.assertEqual(steps['1000'],'down')
        self.assertEqual(steps['1060'],'-')
        self.assertNotIn('1040',steps) # The cross due inside the down press is dropped.
        self.assertEqual(steps['1160'],'cross')
        self.assertEqual(steps['1400'],'down')

    def test_fields_increase(self):
        fields=[int(s.split(':')[0]) for s in screen_run.pad_script(20000,120,400).split(',')]
        self.assertEqual(fields,sorted(set(fields)))


if __name__=='__main__':
    unittest.main()
