import re, unittest
from pathlib import Path
import run_startup_probe

ROOT = Path(__file__).resolve().parents[2]


class GameInputs(unittest.TestCase):
    def test_the_game_program_passes_the_runner_s_startup_inputs(self):
        # The game program starts the runtime with the same input files, in the same order, as the runner.
        source = (ROOT / 'runtime/game/game_main.cpp').read_text(encoding='utf-8')
        listed = re.search(r'startup_inputs\[\]=\{(.*?)\};', source, re.S)
        self.assertIsNotNone(listed)
        self.assertEqual(tuple(re.findall(r'"([^"]+)"', listed[1])), tuple(run_startup_probe.INPUTS))


if __name__ == '__main__':
    unittest.main()
