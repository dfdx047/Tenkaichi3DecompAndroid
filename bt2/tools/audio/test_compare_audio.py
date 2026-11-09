import unittest
import numpy as np
import compare_audio as c


class CompareAudioTests(unittest.TestCase):
    def setUp(self):
        rng = np.random.default_rng(1)
        self.rate = 8000
        left = rng.normal(0, 3000, 4 * self.rate)
        right = rng.normal(0, 1000, 4 * self.rate)
        self.ref = np.stack([left, right], axis=1)

    def test_lag_and_envelope(self):
        a = self.ref[2000:6000, 0]
        lag, score = c.best_lag(a, self.ref[:, 0])
        self.assertEqual(lag, 2000)
        self.assertAlmostEqual(score, 1.0, places=4)  # Each side has its own mean removed.
        self.assertTrue(np.allclose(c.envelope(np.array([3.0, 4.0, 0.0, 0.0]), 2), [np.sqrt(12.5), 0.0]))

    def test_segment_reports_offset_level_and_channel_order(self):
        # The port's audio is the reference from 0.5 s on, 2.5 times louder.
        port = self.ref[self.rate // 2:] * 2.5
        s = c.segment_report(port, self.ref, self.rate, self.rate, self.rate, self.rate // 2, self.rate // 4)
        self.assertAlmostEqual(s['offset_s'], 0.5)
        for channel in (0, 1):
            self.assertAlmostEqual(s['rms_ratio'][channel], 2.5, places=6)
            self.assertAlmostEqual(s['fit_gain'][channel], 2.5, places=6)
            self.assertAlmostEqual(s['same'][channel], 1.0, places=6)
            self.assertLess(abs(s['swapped'][channel]), 0.1)
        # Swapped channels show up as a match on the other side.
        s = c.segment_report(port[:, ::-1], self.ref, self.rate, self.rate, self.rate, self.rate // 2, self.rate // 4)
        self.assertGreater(s['swapped'][0], 0.99)
        self.assertLess(abs(s['same'][0]), 0.1)

    def test_silent_or_short_segments_are_skipped(self):
        silent = np.zeros((self.rate, 2))
        self.assertIsNone(c.segment_report(silent, self.ref, self.rate, 0, self.rate, 0, 100))
        self.assertIsNone(c.segment_report(self.ref, self.ref, self.rate, 3 * self.rate + 1, self.rate, 0, 100))


if __name__ == '__main__':
    unittest.main()
