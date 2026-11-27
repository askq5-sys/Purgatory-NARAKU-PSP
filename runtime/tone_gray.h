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

/* The RGB tone is applied after gray as clamped add/subtract passes. A tone
 * of +255 or -255 on every channel turns each pixel fully white or black, so
 * the gray result would be overwritten. */
static inline int tone_hides_gray(int r, int g, int b)
{
    return (r <= -255 && g <= -255 && b <= -255) || (r >= 255 && g >= 255 && b >= 255);
}
#endif
