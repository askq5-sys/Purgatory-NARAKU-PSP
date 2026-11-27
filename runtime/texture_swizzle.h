/* Swizzled 32-bit textures (NARAKU_SWIZZLE_TEXTURES builds).
 *
 * The GE reads textures in 16-byte x 8-row blocks. Storing a texture in that
 * block order ("swizzled") lets the GE fetch it far more efficiently than the
 * plain row order; the sampled pixels are identical. Swizzled textures are
 * remembered here so bind_texture_8888 can tell the GE which layout to use.
 * Width in bytes must be a multiple of 16 and height a multiple of 8. */
#ifndef NARAKU_TEXTURE_SWIZZLE_H
#define NARAKU_TEXTURE_SWIZZLE_H
#include <stdint.h>
#include <string.h>

static void swizzle_8888(uint32_t *out, const uint32_t *in, int width, int height)
{
    /* One block: 4 pixels (16 bytes) wide, 8 rows high, 32 words in a row. */
    const int blocks_per_row = width / 4;
    int by, bx, y;
    for (by = 0; by < height / 8; ++by)
        for (bx = 0; bx < blocks_per_row; ++bx) {
            uint32_t *dst = out + (by * blocks_per_row + bx) * 32;
            for (y = 0; y < 8; ++y)
                memcpy(dst + y * 4, in + (by * 8 + y) * width + bx * 4, 16);
        }
}

#define SWIZZLED_TEXTURE_SLOTS 16
static const void *g_swizzled_textures[SWIZZLED_TEXTURE_SLOTS];

static int texture_is_swizzled(const void *texture)
{
    int i;
    if (!texture) return 0;
    for (i = 0; i < SWIZZLED_TEXTURE_SLOTS; ++i)
        if (g_swizzled_textures[i] == texture) return 1;
    return 0;
}

static int texture_mark_swizzled(const void *texture)
{
    int i;
    for (i = 0; i < SWIZZLED_TEXTURE_SLOTS; ++i)
        if (!g_swizzled_textures[i]) { g_swizzled_textures[i] = texture; return 1; }
    return 0;
}

static void texture_forget(const void *texture)
{
    int i;
    for (i = 0; i < SWIZZLED_TEXTURE_SLOTS; ++i)
        if (g_swizzled_textures[i] == texture) g_swizzled_textures[i] = 0;
}
#endif
