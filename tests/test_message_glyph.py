"""Message text glyphs: the table-driven rasterizer matches the original
per-sample float code exactly, and a new message only clears the rows the
previous one used."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
src = (ROOT / 'runtime' / 'font_text.inc').read_text()
a = src.index('static int g_runtime_message_dirty_y0')
b = src.index('\n}\n', src.index('static void rasterize_glyph_to_message_surface(')) + 3
current = src[a:b]

# The rasterizer as it was before the sample tables (1.7.13).
REFERENCE = r'''
static void glyph_reference(
    uint32_t cp, int dst_x, int dst_y, int color_index, float scale)
{
    const uint8_t *rec = font_find_glyph(cp);
    const uint8_t *page_data;
    int page, slot, sx, sy, adv;
    int x, y;
    int r, g, b;

    /* Cache at twice screen resolution; downsample only when drawing. */
    dst_x = (dst_x - MESSAGE_SURFACE_X) * MESSAGE_SURFACE_SCALE;
    dst_y *= MESSAGE_SURFACE_SCALE;
    scale *= MESSAGE_SURFACE_SCALE;
    if (!g_runtime_message_surface) return;
    if (!rec) rec = font_find_glyph((uint32_t)'?');
    if (!rec) return;

    page = (int)rec[4];
    slot = (int)read_u16_le(rec + 5);
    adv = (int)rec[7];
    if (page < 0 || page >= g_font.page_count || !g_font.pages[page]) return;
    if (color_index < 0) color_index = 0;
    if (color_index > 31) color_index = 31;

    sx = (slot % FONT_COLS) * FONT_CELL_W;
    sy = (slot / FONT_COLS) * FONT_CELL_H;
    page_data = (const uint8_t *)g_font.pages[page];
    r = g_text_palette[color_index][0];
    g = g_text_palette[color_index][1];
    b = g_text_palette[color_index][2];

    /* Sample the source alpha at destination pixel centres, just like the
     * linearly filtered menu font. Keep the cached RGBA surface so inline
     * colours remain deterministic and no per-frame glyph work is added. */
    for (y = 0; y < (int)(FONT_CELL_H * scale); ++y) {
        int py = dst_y + y;
        float fy = (y + 0.5f) / scale - 0.5f;
        int iy = (int)fy;
        float wy = fy - iy;
        if (py < 0 || py >= MESSAGE_SURFACE_H) continue;
        for (x = 0; x < (int)(adv * scale); ++x) {
            int px = dst_x + x;
            float fx = (x + 0.5f) / scale - 0.5f;
            int ix = (int)fx;
            float wx = fx - ix;
            float alpha = 0.0f;
            int dx, dy, qx, qy;
            uint8_t a;
            if (px < 0 || px >= MESSAGE_SURFACE_W) continue;
            /* Four subpixel samples preserve thin stems when reducing. */
            for (qy = 0; qy < 2; ++qy) {
                for (qx = 0; qx < 2; ++qx) {
                    fx = (x + 0.25f + qx * 0.5f) / scale - 0.5f;
                    fy = (y + 0.25f + qy * 0.5f) / scale - 0.5f;
                    ix = (int)fx; iy = (int)fy;
                    if (fx < ix) --ix;
                    if (fy < iy) --iy;
                    wx = fx - ix; wy = fy - iy;
                    for (dy = 0; dy < 2; ++dy) {
                        for (dx = 0; dx < 2; ++dx) {
                            int sample_x = ix + dx, sample_y = iy + dy;
                            if (sample_x >= 0 && sample_x < adv &&
                                sample_y >= 0 && sample_y < FONT_CELL_H)
                                alpha += 0.25f * page_data[(sy + sample_y) * FONT_TEX_W + sx + sample_x] *
                                    (dx ? wx : 1.0f - wx) * (dy ? wy : 1.0f - wy);
                        }
                    }
                }
            }
            a = (uint8_t)(alpha + 0.5f);
            if (a == 0) continue;
            g_runtime_message_surface[py * MESSAGE_SURFACE_W + px] =
                ((uint32_t)a << 24) | ((uint32_t)b << 16) |
                ((uint32_t)g << 8) | (uint32_t)r;
        }
    }
}
'''

HARNESS = r'''
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define MESSAGE_SURFACE_W 512
#define MESSAGE_SURFACE_H 256
#define MESSAGE_SURFACE_X 110
#define MESSAGE_SURFACE_SCALE 2
#define FONT_TEX_W 512
#define FONT_CELL_W 24
#define FONT_CELL_H 28
#define FONT_COLS 21
static uint32_t *g_runtime_message_surface;
static struct { int page_count; void *pages[2]; } g_font;
static uint8_t g_text_palette[32][3];
static uint8_t g_rec[8];
static uint16_t read_u16_le(const uint8_t *p){return p[0]|p[1]<<8;}
static const uint8_t *font_find_glyph(uint32_t cp){(void)cp;return g_rec;}
@REFERENCE@
@CURRENT@
int main(void)
{
    static uint32_t a[512*256], b[512*256];
    uint8_t *page = malloc(512*512);
    unsigned seed = 1;
    float scales[] = {0.55f, 0.75f};
    for (int i = 0; i < 512*512; ++i) { seed = seed*1103515245u + 12345u; page[i] = (seed >> 16) & 255; if ((seed >> 8) & 3) page[i] = 0; }
    g_font.pages[0] = page; g_font.page_count = 1;
    for (int c = 0; c < 32; ++c) { g_text_palette[c][0] = c*7; g_text_palette[c][1] = 255-c; g_text_palette[c][2] = c*3; }
    /* The second pass draws every glyph from the coverage cache. */
    for (int pass = 0; pass < 2; ++pass)
    for (int slot = 0; slot < FONT_COLS*18; ++slot)
        for (int k = 0; k < 2; ++k) {
            int dx = 100 + (slot % 37) * 11 - 3, dy = -6 + (slot % 13) * 19;
            g_rec[4] = 0; g_rec[5] = slot & 255; g_rec[6] = slot >> 8; g_rec[7] = 1 + (slot * 7) % 24;
            memset(a, 0, sizeof a); g_runtime_message_surface = a;
            glyph_reference(1, dx, dy, slot % 32, scales[k]);
            memset(b, 0, sizeof b); g_runtime_message_surface = b;
            g_runtime_message_dirty_y0 = MESSAGE_SURFACE_H; g_runtime_message_dirty_y1 = -1;
            rasterize_glyph_to_message_surface(1, dx, dy, slot % 32, scales[k]);
            if (memcmp(a, b, sizeof a)) { printf("slot %d scale %d differs\n", slot, k); return 1; }
            /* Every row holding text lies inside the dirty range. */
            for (int y = 0; y < 256; ++y)
                for (int x = 0; x < 512; ++x)
                    if (b[y*512+x] && (y < g_runtime_message_dirty_y0 || y > g_runtime_message_dirty_y1)) return 2;
        }
    return 0;
}
'''

with tempfile.TemporaryDirectory() as td:
    c, exe = Path(td) / 'g.c', Path(td) / 'g'
    c.write_text(HARNESS.replace('@REFERENCE@', REFERENCE).replace('@CURRENT@', current))
    for opt in ('-O0', '-O2'):
        subprocess.run(['cc', '-std=c99', opt, str(c), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
assert 'memset(g_runtime_message_surface, 0, MESSAGE_SURFACE_BYTES)' not in src
assert 'g_runtime_message_dirty_y0 = 0;' in src, 'a new surface is cleared whole once'
print('PASS: message glyphs (computed and from the coverage cache) are identical to the original sampling at both scales; '
      'only rows holding text are cleared for the next message')
