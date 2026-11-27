"""Swizzled texture layout matches the GE block order; registry bookkeeping."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from source_util import main_source
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime/texture_swizzle.h"
/* PSPSDK sample layout, byte by byte: 16-byte x 8-row blocks in row order. */
static void reference(unsigned char *out, const unsigned char *in, int width_bytes, int height)
{
    int rowblocks = width_bytes / 16;
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width_bytes; ++x) {
            int block = (x / 16) + (y / 8) * rowblocks;
            out[block * 128 + (y % 8) * 16 + (x % 16)] = in[y * width_bytes + x];
        }
}
int main(void)
{
    static const int sizes[][2] = {{512,512},{256,512},{512,128},{128,128},{64,64},{4,8}};
    for (unsigned k = 0; k < sizeof sizes / sizeof *sizes; ++k) {
        int w = sizes[k][0], h = sizes[k][1];
        uint32_t *in = malloc((size_t)w * h * 4), *a = malloc((size_t)w * h * 4), *b = malloc((size_t)w * h * 4);
        for (int i = 0; i < w * h; ++i) in[i] = (uint32_t)i * 2654435761u ^ (uint32_t)k;
        swizzle_8888(a, in, w, h);
        reference((unsigned char *)b, (const unsigned char *)in, w * 4, h);
        for (int i = 0; i < w * h; ++i) assert(a[i] == b[i]);
        free(in); free(a); free(b);
    }
    int t[SWIZZLED_TEXTURE_SLOTS + 1];
    for (int i = 0; i < SWIZZLED_TEXTURE_SLOTS; ++i) assert(texture_mark_swizzled(&t[i]));
    assert(!texture_mark_swizzled(&t[SWIZZLED_TEXTURE_SLOTS]));
    assert(texture_is_swizzled(&t[3]) && !texture_is_swizzled(&t[SWIZZLED_TEXTURE_SLOTS]) && !texture_is_swizzled(0));
    texture_forget(&t[3]);
    assert(!texture_is_swizzled(&t[3]) && texture_mark_swizzled(&t[SWIZZLED_TEXTURE_SLOTS]));
    puts("ok");
    return 0;
}
'''


def test():
    with tempfile.TemporaryDirectory() as tmp:
        c = Path(tmp) / 't.c'
        exe = Path(tmp) / 't'
        c.write_text(HARNESS)
        subprocess.run(['cc', '-std=c99', '-O1', '-I', str(ROOT), str(c), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True, stdout=subprocess.DEVNULL)
    s = main_source(ROOT)
    # Every 8888 bind reports the layout, and every swizzled texture is freed
    # through texture_free so a reused address is never taken as swizzled.
    assert s.count('sceGuTexMode(GU_PSM_8888, 0, 0, TEXTURE_SWIZZLE_FLAG(') == 2
    assert s.count('sceGuTexMode(GU_PSM_8888') == 2
    for name in ('m->tile_atlases[i]', 'm->event_atlas', 'm->event_atlas_detail', 'm->parallax_texture', 'alternate'):
        assert f'texture_free({name})' in s, name
        assert f'free({name})' not in s.replace(f'texture_free({name})', ''), name
    print('PASS: swizzle matches the GE 16-byte x 8-row block layout for all used sizes; '
          'registry marks, rejects when full and forgets freed textures; every 8888 bind uses the layout flag')


if __name__ == '__main__':
    test()
