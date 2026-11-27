import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from source_util import main_source
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MarketGameRootTests(unittest.TestCase):
    def test_market_build_uses_the_distribution_folder_name(self):
        cmake = (ROOT / "CMakeLists.txt").read_text()
        source = main_source(ROOT)
        script = (ROOT / "tools/build_market.sh").read_text()
        self.assertIn('set(NARAKU_GAME_DIR "NARAKU" CACHE STRING', cmake)
        self.assertIn('NARAKU_GAME_DIR=\\"${NARAKU_GAME_DIR}\\"', cmake)
        self.assertIn('#define ROOT "ms0:/PSP/GAME/" NARAKU_GAME_DIR "/"', source)
        self.assertIn('-DNARAKU_GAME_DIR=NARAKU', script)
        self.assertNotIn('GAME_DIR_NAME=', script)

    def test_build_scripts_never_inherit_cached_options(self):
        market = (ROOT / "tools/build_market.sh").read_text()
        deploy = (ROOT / "tools/build_deploy.sh").read_text()
        self.assertIn('-DNARAKU_PERF_HUD=OFF', market)
        self.assertIn('-DNARAKU_BOOT_DIAGNOSTICS=OFF', market)
        for option in ("PERF_HUD", "SWIZZLE_TEXTURES", "BOOT_DIAGNOSTICS"):
            self.assertIn(f'-DNARAKU_{option}="${{NARAKU_{option}:-OFF}}"', deploy)


if __name__ == "__main__":
    unittest.main()
