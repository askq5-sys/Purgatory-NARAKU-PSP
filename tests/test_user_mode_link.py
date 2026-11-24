from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class UserModeLinkTests(unittest.TestCase):
    def test_eboot_does_not_link_kernel_stubs(self):
        # pspkernel puts *ForKernel imports (SysclibForKernel memcpy, ...) into a
        # user-mode EBOOT. PPSSPP accepts them; real PSP firmware does not.
        cmake = (ROOT / "CMakeLists.txt").read_text()
        libs = re.search(r"target_link_libraries\(\$\{PROJECT_NAME\}\s+([^)]*)\)", cmake).group(1)
        self.assertNotIn("pspkernel", libs.split())
        self.assertNotIn("USE_KERNEL_LIBS", cmake)

    def test_build_scripts_check_imports(self):
        for name in ("build_deploy.sh", "build_market.sh"):
            script = (ROOT / "tools" / name).read_text()
            self.assertIn("check_eboot_imports.py", script, name)


if __name__ == "__main__":
    unittest.main()
