/* MZ ColorFilter saturation for Tint Screen gray: each channel moves toward
 * the HSL lightness of its pixel. Kept separate so host tests can compare it
 * with the reference formula. */
#ifndef NARAKU_TONE_GRAY_H
#define NARAKU_TONE_GRAY_H
#include <stdint.h>

static inline uint32_t tone_gray_pixel(uint32_t c, int gray)
{
    int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
    int hi = r > g ? r : g, lo = r < g ? r : g;
    int l;
    if (b > hi) hi = b;
    if (b < lo) lo = b;
    l = (hi + lo + 1) / 2;
    r = (r * (255 - gray) + l * gray + 127) / 255;
    g = (g * (255 - gray) + l * gray + 127) / 255;
    b = (b * (255 - gray) + l * gray + 127) / 255;
    return (c & 0xFF000000u) | (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

/* Per-frame table for one gray level: each channel becomes
 * c + floor(((l - c) * gray + 127) / 255), which equals tone_gray_pixel
 * exactly but needs no division (the PSP CPU divides slowly). */
typedef struct {
    int16_t delta[511];          /* index: (l - c) + 255 */
} ToneGrayTable;

static inline void tone_gray_table(ToneGrayTable *t, int gray)
{
    int d;
    for (d = -255; d <= 255; ++d) {
        int n = d * gray + 127;
        t->delta[d + 255] = (int16_t)(n >= 0 ? n / 255 : -((-n + 254) / 255));
    }
}

static inline uint32_t tone_gray_pixel_table(uint32_t c, const ToneGrayTable *t)
{
    int r = c & 255, g = (c >> 8) & 255, b = (c >> 16) & 255;
    int hi = r > g ? r : g, lo = r < g ? r : g;
    int l;
    if (b > hi) hi = b;
    if (b < lo) lo = b;
    l = (hi + lo + 1) >> 1;
    r += t->delta[l - r + 255];
    g += t->delta[l - g + 255];
    b += t->delta[l - b + 255];
    return (c & 0xFF000000u) | (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

/* Converts one row in place. Runs of equal pixels (dark, faded scenes are
 * mostly that) reuse the previous result. */
static inline void tone_gray_row(uint32_t *row, int count, const ToneGrayTable *t)
{
    uint32_t last_in = row[0] ^ 1u, last_out = 0;
    int x;
    for (x = 0; x < count; ++x) {
        uint32_t c = row[x];
        if (c != last_in) {
            last_in = c;
            last_out = tone_gray_pixel_table(c, t);
        }
        row[x] = last_out;
    }
}

/* The RGB tone is applied after gray as clamped add/subtract passes. A tone
 * of +255 or -255 on every channel turns each pixel fully white or black, so
 * the gray result would be overwritten. */
static inline int tone_hides_gray(int r, int g, int b)
{
    return (r <= -255 && g <= -255 && b <= -255) || (r >= 255 && g >= 255 && b >= 255);
}
#endif
