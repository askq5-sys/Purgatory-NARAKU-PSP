/* GU setup, texture binding and drawing primitives.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Graphics                                                                  */
/* ------------------------------------------------------------------------- */

static void init_gu(void)
{
    sceGuInit();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuDrawBuffer(GU_PSM_8888, (void *)0, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)0x88000, BUF_W);
    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuClearColor(0xFF000000);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
}

#ifdef NARAKU_SWIZZLE_TEXTURES
#define TEXTURE_SWIZZLE_FLAG(texture) (texture_is_swizzled(texture) ? GU_TRUE : GU_FALSE)
#else
#define TEXTURE_SWIZZLE_FLAG(texture) GU_FALSE
#endif

static void bind_texture_8888(void *texture, int width, int height)
{
    sceGuTexMode(GU_PSM_8888, 0, 0, TEXTURE_SWIZZLE_FLAG(texture));
    sceGuTexImage(0, width, height, width, texture);
}

/* Pixel-art/world textures must never inherit filtering/wrap state from a
 * previously drawn layer.  PSP GU state persists across draw calls AND across
 * display lists, so the LINEAR filter used by Fog A1/detail pictures could
 * leak into the first map/player draw of the next frame.  On atlas sprites
 * that produces a one-frame dark/light halo around every opaque edge -- most
 * visible exactly when a message box is dismissed before the next one opens. */
static void set_pixel_art_texture_state(void)
{
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuColor(0xFFFFFFFF);
}

static void draw_bound_rect(
    float src_x, float src_y, float src_w, float src_h,
    float dst_x, float dst_y, float dst_w, float dst_h)
{
    TextureVertex *v = (TextureVertex *)sceGuGetMemory(2 * sizeof(TextureVertex));
    v[0].u = src_x;
    v[0].v = src_y;
    v[0].x = dst_x;
    v[0].y = dst_y;
    v[0].z = 0.0f;

    v[1].u = src_x + src_w;
    v[1].v = src_y + src_h;
    v[1].x = dst_x + dst_w;
    v[1].y = dst_y + dst_h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v
    );
}

static void draw_bound_rect_color(
    float src_x, float src_y, float src_w, float src_h,
    float dst_x, float dst_y, float dst_w, float dst_h,
    uint32_t color)
{
    TextureColorVertex *v =
        (TextureColorVertex *)sceGuGetMemory(2 * sizeof(TextureColorVertex));
    v[0].u = src_x;
    v[0].v = src_y;
    v[0].color = color;
    v[0].x = dst_x;
    v[0].y = dst_y;
    v[0].z = 0.0f;

    v[1].u = src_x + src_w;
    v[1].v = src_y + src_h;
    v[1].color = color;
    v[1].x = dst_x + dst_w;
    v[1].y = dst_y + dst_h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v
    );
}

static void draw_texture_alpha(
    void *texture, int tex_w, int tex_h,
    float src_w, float src_h,
    float x, float y, float w, float h, int alpha)
{
    if (alpha < 0) alpha = 0;
    if (alpha > 255) alpha = 255;

    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    bind_texture_8888(texture, tex_w, tex_h);

    TextureColorVertex *v =
        (TextureColorVertex *)sceGuGetMemory(2 * sizeof(TextureColorVertex));
    uint32_t color = ((uint32_t)alpha << 24) | 0x00FFFFFFu;

    v[0].u = 0.0f;
    v[0].v = 0.0f;
    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].u = src_w;
    v[1].v = src_h;
    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v
    );

    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
}

static void draw_solid_rect(float x, float y, float w, float h, int r, int g, int b, int a)
{
    sceGuDisable(GU_TEXTURE_2D);
    ColorVertex *v = (ColorVertex *)sceGuGetMemory(2 * sizeof(ColorVertex));
    uint32_t color = GU_RGBA(r, g, b, a);

    v[0].color = color;
    v[0].x = x;
    v[0].y = y;
    v[0].z = 0.0f;

    v[1].color = color;
    v[1].x = x + w;
    v[1].y = y + h;
    v[1].z = 0.0f;

    sceGuDrawArray(
        GU_SPRITES,
        GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v
    );
    sceGuEnable(GU_TEXTURE_2D);
}
