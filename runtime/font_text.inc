/* UTF-8 font, text layout and the message surface.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Runtime UTF-8 font                                                        */
/* ------------------------------------------------------------------------- */

#include "russian.inc"

static int init_runtime_font(void)
{
    char path[192];
    int i;

    memset(&g_font, 0, sizeof(g_font));
    snprintf(path, sizeof(path), ASSET_ROOT "font_map.bin");
    g_font.map_bin = (uint8_t *)load_whole_file(path, &g_font.map_size);
    if (!g_font.map_bin || g_font.map_size < FONT_MAP_HEADER_BYTES) return 0;
    if (memcmp(g_font.map_bin, "NF30", 4) != 0) return 0;

    g_font.glyph_count = (int)read_u16_le(g_font.map_bin + 4);
    g_font.page_count = (int)g_font.map_bin[6];
    if (g_font.page_count < 1 || g_font.page_count > FONT_MAX_PAGES) return 0;
    if ((int)g_font.map_bin[7] != FONT_CELL_W ||
        (int)g_font.map_bin[8] != FONT_CELL_H ||
        (int)g_font.map_bin[9] != FONT_COLS) return 0;
    if (g_font.map_size != FONT_MAP_HEADER_BYTES +
        (size_t)g_font.glyph_count * FONT_GLYPH_BYTES) return 0;

    for (i = 0; i < 256; ++i)
        g_font.clut[i] = ((uint32_t)i << 24) | 0x00FFFFFFu;

    for (i = 0; i < g_font.page_count; ++i) {
        snprintf(path, sizeof(path), ASSET_ROOT "font_page%d.t8", i);
        g_font.pages[i] = load_exact_file(path, FONT_PAGE_BYTES);
        if (!g_font.pages[i]) return 0;
    }
    return 1;
}

static const uint8_t *font_find_glyph(uint32_t cp)
{
    int lo = 0;
    int hi = g_font.glyph_count - 1;
    const uint8_t *base = g_font.map_bin + FONT_MAP_HEADER_BYTES;

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const uint8_t *rec = base + (size_t)mid * FONT_GLYPH_BYTES;
        uint32_t rcp = read_u32_le(rec);
        if (rcp == cp) return rec;
        if (rcp < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

static uint32_t utf8_next_cp(const char **pp, const char *end)
{
    const unsigned char *p = (const unsigned char *)*pp;
    uint32_t cp;
    if ((const char *)p >= end) return 0;
    if (p[0] < 0x80) {
        *pp = (const char *)(p + 1);
        return p[0];
    }
    if ((p[0] & 0xE0) == 0xC0 && (const char *)(p + 1) < end) {
        cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
        *pp = (const char *)(p + 2);
        return cp;
    }
    if ((p[0] & 0xF0) == 0xE0 && (const char *)(p + 2) < end) {
        cp = ((uint32_t)(p[0] & 0x0F) << 12) |
             ((uint32_t)(p[1] & 0x3F) << 6) |
             (uint32_t)(p[2] & 0x3F);
        *pp = (const char *)(p + 3);
        return cp;
    }
    if ((p[0] & 0xF8) == 0xF0 && (const char *)(p + 3) < end) {
        cp = ((uint32_t)(p[0] & 0x07) << 18) |
             ((uint32_t)(p[1] & 0x3F) << 12) |
             ((uint32_t)(p[2] & 0x3F) << 6) |
             (uint32_t)(p[3] & 0x3F);
        *pp = (const char *)(p + 4);
        return cp;
    }
    *pp = (const char *)(p + 1);
    return (uint32_t)'?';
}

/* Exact 0..31 text palette sampled from NARAKU's original Window.png. */
static const uint8_t g_text_palette[32][3] = {
    {255,255,255},{32,160,214},{255,120,76},{102,204,64},
    {153,204,255},{204,192,255},{255,255,160},{128,128,128},
    {192,192,192},{32,128,204},{255,56,16},{0,160,16},
    {62,154,222},{160,152,255},{255,204,32},{0,0,0},
    {132,170,255},{255,255,64},{255,32,32},{32,32,64},
    {224,128,64},{240,192,64},{64,128,192},{64,192,240},
    {128,255,128},{192,128,128},{128,128,255},{255,128,255},
    {0,160,64},{0,224,96},{160,96,224},{192,128,255}
};

static void bind_font_page_color(int page, int r, int g, int b)
{
    int i;
    if (page < 0 || page >= g_font.page_count || !g_font.pages[page]) return;
    for (i = 0; i < 256; ++i)
        g_font.clut[i] = ((uint32_t)i << 24) |
                         ((uint32_t)(b & 255) << 16) |
                         ((uint32_t)(g & 255) << 8) |
                         (uint32_t)(r & 255);
    /* Recolouring the CLUT happens in cached CPU memory.  Flush it before
     * handing the palette to the GU; otherwise the PSP can keep using the
     * previous white palette and \\c[n] escapes look uncoloured. */
    sceKernelDcacheWritebackRange(g_font.clut, sizeof(g_font.clut));
    sceGuClutMode(GU_PSM_8888, 0, 0xFF, 0);
    sceGuClutLoad(32, g_font.clut);
    sceGuTexMode(GU_PSM_T8, 0, 0, GU_FALSE);
    sceGuTexImage(0, FONT_TEX_W, FONT_TEX_H, FONT_TEX_W, g_font.pages[page]);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
}

static float draw_glyph_cp_color(uint32_t cp, float x, float y, int r, int g, int b)
{
    const uint8_t *rec = font_find_glyph(cp);
    int page;
    int slot;
    int sx;
    int sy;
    int adv;

    if (!rec) rec = font_find_glyph((uint32_t)'?');
    if (!rec) return 7.5f;

    page = (int)rec[4];
    slot = (int)read_u16_le(rec + 5);
    adv = (int)rec[7];
    sx = (slot % FONT_COLS) * FONT_CELL_W;
    sy = (slot / FONT_COLS) * FONT_CELL_H;

    /* Keep the indexed font palette white/alpha-only and tint the glyph with
     * a vertex colour.  Re-loading a recoloured CLUT for every character was
     * unreliable on real PSP/GU and PPSSPP: \c[n] escapes parsed correctly,
     * but the cached white palette could still win.  Vertex modulation is the
     * same colour path used by the rest of the renderer and is deterministic. */
    bind_font_page_color(page, 255, 255, 255);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    draw_bound_rect_color(
        (float)sx, (float)sy, (float)adv, (float)FONT_CELL_H,
        x, y + 3.5f, adv * 0.75f, FONT_CELL_H * 0.75f,
        0xFF000000u | ((uint32_t)(b & 255) << 16) |
        ((uint32_t)(g & 255) << 8) | (uint32_t)(r & 255)
    );
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    return adv * 0.75f;
}

static int parse_text_color_escape(const char **pp, const char *end, int *out_index)
{
    const char *p = *pp;
    int value = 0;
    int digits = 0;
    if (p + 3 >= end || p[0] != '\\' || (p[1] != 'c' && p[1] != 'C') || p[2] != '[')
        return 0;
    p += 3;
    while (p < end && *p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        ++p;
        ++digits;
    }
    if (!digits || p >= end || *p != ']') return 0;
    ++p;
    if (value < 0) value = 0;
    if (value > 31) value = 31;
    *out_index = value;
    *pp = p;
    return 1;
}


/* Rows of the message surface that hold text, so the next message only
 * clears those instead of the whole 512 KB surface. */
static int g_runtime_message_dirty_y0 = 0, g_runtime_message_dirty_y1 = MESSAGE_SURFACE_H - 1;

/* Coverage (alpha) of one glyph at one message scale, kept in a small
 * direct-mapped cache: message text repeats the same letters, so most
 * glyphs are only copied. 256 entries of at most 64x64 bytes. */
typedef struct {
    int page, slot, adv, w, h;
    float scale;
    uint8_t *alpha;
} GlyphCoverage;
#define GLYPH_COVERAGE_SLOTS 256
static GlyphCoverage g_glyph_coverage[GLYPH_COVERAGE_SLOTS];

static const GlyphCoverage *glyph_coverage(int page, int slot, int sx, int sy, int adv,
                                           const uint8_t *page_data, float scale)
{
    enum { MAXW = 64 };
    unsigned h_index = ((unsigned)page * 977u + (unsigned)slot * 31u + (unsigned)adv +
                        (scale > 1.25f ? 128u : 0u)) % GLYPH_COVERAGE_SLOTS;
    GlyphCoverage *c = &g_glyph_coverage[h_index];
    int w = (int)(adv * scale), h = (int)(FONT_CELL_H * scale);
    int six[MAXW][2], siy[MAXW][2];
    float wx1[MAXW][2], wx0[MAXW][2], wy1[MAXW][2], wy0[MAXW][2];
    int x, y, q;
    if (c->alpha && c->page == page && c->slot == slot && c->adv == adv && c->scale == scale)
        return c;
    if (w > MAXW) w = MAXW;
    if (h > MAXW) h = MAXW;
    if (w < 0) w = 0;
    free(c->alpha);
    c->alpha = (uint8_t *)malloc((size_t)(w * h > 0 ? w * h : 1));
    if (!c->alpha) return NULL;
    c->page = page; c->slot = slot; c->adv = adv; c->scale = scale; c->w = w; c->h = h;
    /* Sample positions and weights depend only on the column or row and the
     * scale; the float expressions are the original per-pixel ones. */
    for (x = 0; x < w; ++x)
        for (q = 0; q < 2; ++q) {
            float f = (x + 0.25f + q * 0.5f) / scale - 0.5f;
            int i = (int)f;
            if (f < i) --i;
            six[x][q] = i; wx1[x][q] = f - i; wx0[x][q] = 1.0f - (f - i);
        }
    for (y = 0; y < h; ++y)
        for (q = 0; q < 2; ++q) {
            float f = (y + 0.25f + q * 0.5f) / scale - 0.5f;
            int i = (int)f;
            if (f < i) --i;
            siy[y][q] = i; wy1[y][q] = f - i; wy0[y][q] = 1.0f - (f - i);
        }
    for (y = 0; y < h; ++y)
        for (x = 0; x < w; ++x) {
            float alpha = 0.0f;
            int qx, qy, dx, dy;
            for (qy = 0; qy < 2; ++qy)
                for (qx = 0; qx < 2; ++qx) {
                    int ix = six[x][qx], iy = siy[y][qy];
                    for (dy = 0; dy < 2; ++dy) {
                        int sample_y = iy + dy;
                        const uint8_t *src;
                        float fy = dy ? wy1[y][qy] : wy0[y][qy];
                        if (sample_y < 0 || sample_y >= FONT_CELL_H) continue;
                        src = page_data + (sy + sample_y) * FONT_TEX_W + sx;
                        for (dx = 0; dx < 2; ++dx) {
                            int sample_x = ix + dx;
                            if (sample_x >= 0 && sample_x < adv)
                                alpha += 0.25f * src[sample_x] *
                                    (dx ? wx1[x][qx] : wx0[x][qx]) * fy;
                        }
                    }
                }
            c->alpha[y * w + x] = (uint8_t)(alpha + 0.5f);
        }
    return c;
}

static void rasterize_glyph_to_message_surface(
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
     * colours remain deterministic and no per-frame glyph work is added.
     * Four subpixel samples per pixel, each a bilinear tap, preserve thin
     * stems when reducing. A glyph's coverage depends only on the glyph and
     * the scale, so it is computed once (glyph_coverage) and reused. */
    {
        const GlyphCoverage *cov = glyph_coverage(page, slot, sx, sy, adv, page_data, scale);
        int w, h;
        if (!cov) return;
        w = cov->w; h = cov->h;
        for (y = 0; y < h; ++y) {
            int py = dst_y + y;
            uint32_t *row;
            const uint8_t *alpha_row = cov->alpha + y * w;
            if (py < 0 || py >= MESSAGE_SURFACE_H) continue;
            row = g_runtime_message_surface + py * MESSAGE_SURFACE_W;
            for (x = 0; x < w; ++x) {
                int px = dst_x + x;
                uint8_t a = alpha_row[x];
                if (a == 0 || px < 0 || px >= MESSAGE_SURFACE_W) continue;
                row[px] = ((uint32_t)a << 24) | ((uint32_t)b << 16) |
                          ((uint32_t)g << 8) | (uint32_t)r;
                if (py < g_runtime_message_dirty_y0) g_runtime_message_dirty_y0 = py;
                if (py > g_runtime_message_dirty_y1) g_runtime_message_dirty_y1 = py;
            }
        }
    }
}

static int latin_word_cp(uint32_t cp)
{
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z') ||
           (cp >= '0' && cp <= '9') || cp == '\'' || cp == '-' ||
           (g_language==4 && cp>=0x0400 && cp<=0x052f);
}

static int latin_word_width(const char *p, const char *end)
{
    int width = 0, color = 0;
    while (p < end) {
        uint32_t cp;
        const uint8_t *rec;
        if (parse_text_color_escape(&p, end, &color)) continue;
        cp = utf8_next_cp(&p, end);
        if (!latin_word_cp(cp)) break;
        rec = font_find_glyph(cp);
        if (!rec) rec = font_find_glyph('?');
        width += rec ? rec[7] : 10;
    }
    return width;
}

static int message_word_width(const char *p, const char *end, float scale)
{
    int width = 0, color = 0;
    while (p < end) {
        const uint8_t *rec;
        uint32_t cp;
        if (parse_text_color_escape(&p, end, &color)) continue;
        cp = utf8_next_cp(&p,end);
        if (!latin_word_cp(cp)) break;
        rec = font_find_glyph(cp);
        if (!rec) rec = font_find_glyph('?');
        width += rec ? (int)(rec[7] * scale) : 5;
    }
    return width;
}

static void rasterize_utf8_wrapped_to_message_surface(
    const char *text, size_t len,
    int start_x, int start_y, int max_x, int max_lines, float scale)
{
    const char *p = text;
    const char *end = text + len;
    int x = start_x;
    int y = start_y;
    int line = 0;
    int color_index = 0;
    int in_word = 0;

    while (p < end && line < max_lines) {
        const char *before = p;
        uint32_t cp;
        const uint8_t *rec;
        int adv;
        int parsed_color = color_index;

        if (parse_text_color_escape(&p, end, &parsed_color)) {
            color_index = parsed_color;
            continue;
        }

        cp = utf8_next_cp(&p, end);
        if (latin_word_cp(cp) && !in_word) {
            int width = message_word_width(before, end, scale);
            if (width <= max_x - start_x && x + width > max_x && x > start_x) {
                x = start_x; y += 14; ++line;
                if (line >= max_lines) break;
            }
        }
        in_word = latin_word_cp(cp);
        if (cp == '\r') continue;
        if (cp == '\n') {
            x = start_x;
            y += 14;
            ++line;
            continue;
        }

        rec = font_find_glyph(cp);
        if (!rec) rec = font_find_glyph((uint32_t)'?');
        adv = rec ? (int)(rec[7] * scale) : 5;

        if (x + adv > max_x && x > start_x) {
            x = start_x;
            y += 14;
            ++line;
            if (line >= max_lines) break;
        }

        if (cp == ' ')
            x += adv;
        else {
            rasterize_glyph_to_message_surface(cp, x, y, color_index, scale);
            x += adv;
        }

        if (p == before) break;
    }
}

/* Original ITB_ResizeMessageWindow: 54% width, three rows, bottom anchored.
 * Scale its 816x624 layout onto the PSP screen; text is top/left aligned. */
#define MESSAGE_BOX_X 110
#define MESSAGE_BOX_Y (g_runtime_message_position==0?0:g_runtime_message_position==1?105:210)
#define MESSAGE_BOX_W 260
#define MESSAGE_BOX_H 62
#define MESSAGE_NAME_H 26
static int runtime_message_text_y(void) { return MESSAGE_BOX_Y + 8; }
static int runtime_name_y(void) { return MESSAGE_BOX_Y?MESSAGE_BOX_Y-MESSAGE_NAME_H:MESSAGE_BOX_H; }

static int runtime_speaker_width(void)
{
    const char *p = g_runtime_speaker, *end = p ? p + g_runtime_speaker_len : p;
    int width = 16, color = 0;
    while (p && p < end) {
        const uint8_t *rec;
        if (parse_text_color_escape(&p, end, &color)) continue;
        rec = font_find_glyph(utf8_next_cp(&p, end));
        width += rec ? (int)(rec[7] * 0.75f) : 5;
    }
    return width > MESSAGE_BOX_W ? MESSAGE_BOX_W : width;
}

static void rebuild_runtime_message_surface(void)
{
    if (!g_runtime_message_surface) {
        g_runtime_message_surface = (uint32_t *)memalign(16, MESSAGE_SURFACE_BYTES);
        if (!g_runtime_message_surface) {
            g_runtime_message_surface_ready = 0;
            return;
        }
        g_runtime_message_dirty_y0 = 0;
        g_runtime_message_dirty_y1 = MESSAGE_SURFACE_H - 1;
    }

    if (g_runtime_message_dirty_y0 <= g_runtime_message_dirty_y1)
        memset(g_runtime_message_surface + g_runtime_message_dirty_y0 * MESSAGE_SURFACE_W, 0,
               (size_t)(g_runtime_message_dirty_y1 - g_runtime_message_dirty_y0 + 1) *
               MESSAGE_SURFACE_W * sizeof(uint32_t));
    g_runtime_message_dirty_y0 = MESSAGE_SURFACE_H;
    g_runtime_message_dirty_y1 = -1;

    if (g_runtime_speaker && g_runtime_speaker_len > 0) {
        rasterize_utf8_wrapped_to_message_surface(
            g_runtime_speaker, g_runtime_speaker_len,
            MESSAGE_BOX_X + 8, (runtime_name_y() + 5) - MESSAGE_SURFACE_Y,
            MESSAGE_BOX_X + runtime_speaker_width() - 6, 1, 0.75f
        );
    }
    if (g_runtime_message && g_runtime_message_len > 0) {
        rasterize_utf8_wrapped_to_message_surface(
            g_runtime_message, g_runtime_message_len,
            MESSAGE_BOX_X + 9, runtime_message_text_y() - MESSAGE_SURFACE_Y,
            MESSAGE_BOX_X + MESSAGE_BOX_W - 9, 3, 0.55f
        );
    }

    sceKernelDcacheWritebackRange(g_runtime_message_surface, MESSAGE_SURFACE_BYTES);
    g_runtime_message_surface_ready = 1;
}

static void draw_utf8_wrapped(
    const char *text, size_t len,
    float start_x, float start_y, float max_x, int max_lines)
{
    const char *p = text;
    const char *end = text + len;
    float x = start_x;
    float y = start_y;
    int line = 0;
    int color_index = 0;
    int in_word = 0;

    while (p < end && line < max_lines) {
        const char *before = p;
        uint32_t cp;
        const uint8_t *rec;
        float adv;
        int parsed_color;

        parsed_color = color_index;
        if (parse_text_color_escape(&p, end, &parsed_color)) {
            color_index = parsed_color;
            continue;
        }

        cp = utf8_next_cp(&p, end);
        if (latin_word_cp(cp) && !in_word) {
            float width = latin_word_width(before, end) * 0.75f;
            if (width <= max_x - start_x && x + width > max_x && x > start_x) {
                x = start_x; y += 25; ++line;
                if (line >= max_lines) break;
            }
        }
        in_word = latin_word_cp(cp);
        if (cp == '\r') continue;
        if (cp == '\n') {
            x = start_x;
            y += 25.0f;
            line++;
            continue;
        }

        rec = font_find_glyph(cp);
        if (!rec) rec = font_find_glyph((uint32_t)'?');
        adv = (rec ? (int)rec[7] : 10) * 0.75f;

        if (x + adv > max_x && x > start_x) {
            x = start_x;
            y += 25.0f;
            line++;
            if (line >= max_lines) break;
        }

        if (cp == ' ') {
            x += adv;
        } else {
            const uint8_t *rgb = g_text_palette[color_index];
            x += (float)draw_glyph_cp_color(cp, x, y, rgb[0], rgb[1], rgb[2]);
        }

        if (p == before) break;
    }
}

static void draw_naraku_window(float x, float y, float w, float h, int alpha)
{
    /* NARAKU's PC windows are almost-black with a thin blood-red frame.
     * Keep that identity at 480x272 instead of the generic blue/grey PSP box. */
    draw_solid_rect(x, y, w, h, 10, 2, 4, alpha);
    draw_solid_rect(x, y, w, 2.0f, 210, 0, 0, 255);
    draw_solid_rect(x, y + h - 2.0f, w, 2.0f, 210, 0, 0, 255);
    draw_solid_rect(x, y, 2.0f, h, 210, 0, 0, 255);
    draw_solid_rect(x + w - 2.0f, y, 2.0f, h, 210, 0, 0, 255);
    draw_solid_rect(x + 3.0f, y + 3.0f, w - 6.0f, 1.0f, 70, 0, 0, 230);
}

/* Scale the frame itself, including horizontal border thickness, just like
 * MZ Window._container.scale.y. Text lives outside that scaled container. */
static void draw_message_window_reveal(float x, float y, float w, float h, int alpha)
{
    float scale = g_runtime_message_openness / 255.0f;
    float height = h * scale;
    y += (h - height) * 0.5f;
    draw_solid_rect(x, y, w, height, 10, 2, 4, alpha);
    draw_solid_rect(x, y, w, 2.0f * scale, 210, 0, 0, 255);
    draw_solid_rect(x, y + height - 2.0f * scale, w, 2.0f * scale, 210, 0, 0, 255);
    draw_solid_rect(x, y, 2.0f, height, 210, 0, 0, 255);
    draw_solid_rect(x + w - 2.0f, y, 2.0f, height, 210, 0, 0, 255);
    draw_solid_rect(x + 3.0f, y + 3.0f * scale, w - 6.0f, scale, 70, 0, 0, 230);
}

static int g_choice_window_width = 96;

static void rebuild_choice_text_surface(void)
{
    int i, width = 0;
    for (i = 0; i < g_choice_count; ++i) {
        const char *p = g_choice_labels[i], *end = p + g_choice_lengths[i];
        int line_width = 0, color = 0;
        while (p < end) {
            const uint8_t *rec;
            if (parse_text_color_escape(&p, end, &color)) continue;
            rec = font_find_glyph(utf8_next_cp(&p, end));
            if (!rec) rec = font_find_glyph('?');
            line_width += rec ? (int)(rec[7] * 0.55f) : 5;
        }
        if (line_width > width) width = line_width;
    }
    g_choice_window_width = width + 24;
    if (g_choice_window_width < 96) g_choice_window_width = 96;
    if (g_choice_window_width > SCREEN_W) g_choice_window_width = SCREEN_W;
}

static void draw_choice_text(int index, float x, float y)
{
    const char *p = g_choice_labels[index], *end = p + g_choice_lengths[index];
    int color = 0;
    while (p < end) {
        const uint8_t *rec;
        int slot, adv;
        uint32_t tint;
        if (parse_text_color_escape(&p, end, &color)) continue;
        rec = font_find_glyph(utf8_next_cp(&p, end));
        if (!rec) rec = font_find_glyph('?');
        if (!rec) continue;
        slot = read_u16_le(rec + 5); adv = rec[7];
        bind_font_page_color(rec[4], 255, 255, 255);
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        tint = 0xff000000u | g_text_palette[color][0] |
            (g_text_palette[color][1] << 8) | (g_text_palette[color][2] << 16);
        draw_bound_rect_color((slot % FONT_COLS) * FONT_CELL_W,
            (slot / FONT_COLS) * FONT_CELL_H, adv, FONT_CELL_H,
            x, y, adv * 0.55f, FONT_CELL_H * 0.55f, tint);
        x += (int)(adv * 0.55f);
    }
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
}

static void draw_runtime_choice_overlay(void)
{
    int i;
    float h, x, y;
    if (!g_choice_active) return;
    h = g_choice_count * 16 + 12;
    x = g_choice_position==0?0:g_choice_position==1?
        (SCREEN_W-g_choice_window_width)*0.5f:SCREEN_W-g_choice_window_width;
    /* Original Window_ChoiceList.windowY anchors above the bottom message box,
     * even when that box is currently closed. Default choice position is right. */
    y = MESSAGE_BOX_Y>=SCREEN_H/2?MESSAGE_BOX_Y-h:MESSAGE_BOX_Y+MESSAGE_BOX_H;
    if (y < 0) y = 0;
    if(g_choice_background==0)draw_naraku_window(x,y,g_choice_window_width,h,g_window_opacity);
    else if(g_choice_background==1)draw_solid_rect(x,y,g_choice_window_width,h,0,0,0,190);
    for (i = 0; i < g_choice_count; ++i) {
        if (i == g_choice_cursor)
            draw_solid_rect(x + 5, y + 6 + i * 16,
                g_choice_window_width - 10, 16, 160, 0, 0, 150);
    }
    for (i = 0; i < g_choice_count; ++i)
        draw_choice_text(i, x + 10, y + 6 + i * 16);
}

/* Original number-input command, adapted to the PSP direction buttons. */
static void draw_runtime_number_overlay(void)
{
    int i, divisor=1;
    float w=g_number_digits*20+24,x=(SCREEN_W-w)*0.5f,y=MESSAGE_BOX_Y-38;
    if(!g_number_active)return;
    draw_naraku_window(x,y,w,36,g_window_opacity);
    draw_solid_rect(x+12+g_number_cursor*20,y+8,20,20,160,0,0,150);
    for(i=1;i<g_number_digits;++i)divisor*=10;
    for(i=0;i<g_number_digits;++i) {
        const uint8_t *rec=font_find_glyph('0'+(g_number_value/divisor)%10);
        if(rec) {
            int slot=read_u16_le(rec+5),adv=rec[7];
            bind_font_page_color(rec[4],255,255,255);
            sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
            sceGuTexFilter(GU_LINEAR,GU_LINEAR);
            draw_bound_rect_color((slot%FONT_COLS)*FONT_CELL_W,(slot/FONT_COLS)*FONT_CELL_H,
                adv,FONT_CELL_H,x+12+i*20+(20-adv*0.75f)*0.5f,y+7,adv*0.75f,FONT_CELL_H*0.75f,0xffffffffu);
        }
        divisor/=10;
    }
    set_pixel_art_texture_state();
}

static void draw_runtime_message_overlay(void)
{
    float top;
    int has_speaker;
    if (!g_runtime_message_active || g_runtime_message_openness <= 0) return;

    has_speaker = g_runtime_speaker && g_runtime_speaker_len > 0;
    top = MESSAGE_BOX_Y;

    if (g_runtime_message_background==1)
        draw_solid_rect(MESSAGE_BOX_X,top,MESSAGE_BOX_W,MESSAGE_BOX_H,0,0,0,190);
    if (!g_runtime_message_transparent && g_runtime_message_background==0)
        draw_message_window_reveal(MESSAGE_BOX_X, top, MESSAGE_BOX_W,
                               MESSAGE_BOX_H, g_window_opacity);

    if (has_speaker && !g_runtime_message_transparent && g_runtime_message_background==0) {
        /* Separate name plate, matching the small bordered speaker box in the
         * original rather than consuming the first line of the message box. */
        draw_message_window_reveal(MESSAGE_BOX_X, runtime_name_y(),
                                   runtime_speaker_width(), MESSAGE_NAME_H,
                                   g_window_opacity);
    }

    if (g_runtime_message_openness < 255) return;

    /* High-resolution RGBA cache preserves inline colours and lets the GU
     * filter the final reduction, matching the smoothly scaled menu font. */
    if (g_runtime_message_surface_ready && g_runtime_message_surface) {
        bind_texture_8888(g_runtime_message_surface, MESSAGE_SURFACE_W, MESSAGE_SURFACE_H);
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        sceGuColor(0xFFFFFFFF);
        draw_bound_rect(
            0.0f, 0.0f, MESSAGE_SURFACE_W, MESSAGE_SURFACE_H,
            MESSAGE_SURFACE_X, MESSAGE_SURFACE_Y,
            MESSAGE_SURFACE_W / MESSAGE_SURFACE_SCALE,
            MESSAGE_SURFACE_H / MESSAGE_SURFACE_SCALE
        );
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    } else {
        if (has_speaker)
            draw_utf8_wrapped(g_runtime_speaker, g_runtime_speaker_len,
                              MESSAGE_BOX_X + 8, top - MESSAGE_NAME_H + 5,
                              MESSAGE_BOX_X + runtime_speaker_width() - 6, 1);
        if (g_runtime_message && g_runtime_message_len > 0)
            draw_utf8_wrapped(g_runtime_message, g_runtime_message_len,
                              MESSAGE_BOX_X + 9, runtime_message_text_y(),
                              MESSAGE_BOX_X + MESSAGE_BOX_W - 9, 3);
    }

}

