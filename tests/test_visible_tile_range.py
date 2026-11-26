"""visible_tile_range must keep every tile the full-map loop would draw."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def extract(source, start):
    i = source.index(start)
    depth = 0
    for j in range(source.index("{", i), len(source)):
        depth += {"{": 1, "}": -1}.get(source[j], 0)
        if depth == 0:
            return source[i:j + 1]
    raise ValueError(start)


HARNESS = r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define TILE_PX 24
#define SCREEN_W 480
#define SCREEN_H 272
%s
static int visible(float origin, float camera, int i, int screen)
{
    float d = origin + (float)(i * TILE_PX) - camera;
    return !(d <= -TILE_PX || d >= screen);
}
int main(void)
{
    srand(7);
    for (int n = 0; n < 200000; ++n) {
        int count = 1 + rand() %% 200;
        int screen = (n & 1) ? SCREEN_W : SCREEN_H;
        float origin = (float)(rand() %% 2000 - 1000) / 7.0f;
        float camera = (float)(rand() %% 12000 - 2000) / 3.0f;
        int first, last;
        visible_tile_range(origin, camera, screen, count, &first, &last);
        for (int i = 0; i < count; ++i) {
            if (visible(origin, camera, i, screen) && (i < first || i > last)) {
                printf("miss count=%%d origin=%%f camera=%%f i=%%d range=%%d..%%d\n",
                       count, origin, camera, i, first, last);
                return 1;
            }
        }
    }
    puts("ok");
    return 0;
}
'''


class VisibleTileRangeTests(unittest.TestCase):
    def test_range_covers_every_visible_tile(self):
        source = (ROOT / "main.c").read_text()
        function = extract(source, "static void visible_tile_range(")
        with tempfile.TemporaryDirectory() as tmp:
            c = Path(tmp) / "t.c"
            exe = Path(tmp) / "t"
            c.write_text(HARNESS % function)
            subprocess.run(["cc", "-O2", "-std=c99", str(c), "-o", str(exe), "-lm"], check=True)
            out = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(out.returncode, 0, out.stdout)

    def test_release_has_no_scene_trace(self):
        source = (ROOT / "main.c").read_text()
        self.assertFalse("scene_trace" in source, "debug scene_trace logging is back in main.c")
        # Remaining debug logging must stay behind its compile-time switch.
        for match in re.finditer(r'fopen\([^)]*,\s*"a"\)', source):
            before = source[:match.start()]
            in_route_trace = before.rfind("#ifdef NARAKU_ROUTE_TRACE") > before.rfind("#endif")
            in_boot_diag = before.rfind("#ifdef NARAKU_BOOT_DIAGNOSTICS") > before.rfind("#else")
            self.assertTrue(in_route_trace or in_boot_diag, source[match.start():match.start() + 60])


if __name__ == "__main__":
    unittest.main()
