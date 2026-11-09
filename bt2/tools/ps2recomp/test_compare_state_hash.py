import unittest
from compare_state_hash import first_difference, differing_pages


class StateHashComparisonTests(unittest.TestCase):
    def test_first_differing_common_field(self):
        a = {1: (0, 'aa'), 2: (1, 'bb'), 3: (1, 'cc')}
        self.assertEqual(first_difference(a, dict(a)), (None, 3))
        b = {**a, 2: (1, 'bx'), 3: (2, 'cx')}
        self.assertEqual(first_difference(a, b), ((2, (1, 'bb'), (1, 'bx')), 3))
        # Only common fields count: a run that stopped earlier agrees on what both ran.
        self.assertEqual(first_difference(a, {1: (0, 'aa')}), (None, 1))

    def test_differing_pages(self):
        a = {('ee', 0): '1', ('ee', 1): '2', ('vu1', 0): '3'}
        b = {('ee', 0): '1', ('ee', 1): '9', ('sp', 0): '4'}
        self.assertEqual(differing_pages(a, b), [('ee', 1), ('sp', 0), ('vu1', 0)])


if __name__ == '__main__':
    unittest.main()
