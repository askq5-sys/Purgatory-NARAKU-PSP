from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MarketGameRootTests(unittest.TestCase):
    def test_market_build_uses_the_distribution_folder_name(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        source = (ROOT / "main.c").read_text()
        script = (ROOT / "tools/build_market_v178.sh").read_text()
        self.assertIn('set(NARAKU_GAME_DIR "NARAKU" CACHE STRING', cmake)
        self.assertIn('NARAKU_GAME_DIR=\\"${NARAKU_GAME_DIR}\\"', cmake)
        self.assertIn('#define ROOT "ms0:/PSP/GAME/" NARAKU_GAME_DIR "/"', source)
        self.assertIn('-DNARAKU_GAME_DIR=NARAKU', script)
        self.assertNotIn('GAME_DIR_NAME=', script)


if __name__ == "__main__":
    unittest.main()
