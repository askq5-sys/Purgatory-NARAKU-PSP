/* Asset file loading (plain and .z compressed).
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Files                                                                     */
/* ------------------------------------------------------------------------- */

static void fatal_error(const char *message)
{
    boot_diag_error(message);
    pspDebugScreenInit();
    pspDebugScreenClear();
    pspDebugScreenPrintf("\nNARAKU PSP 1.7.8\n\nERROR:\n%s\n", message);
    pspDebugScreenPrintf("\nHOME: exit\n");
    while (1) sceDisplayWaitVblankStart();
}

/* Large files are read in 64 KB pieces. The music thread reads its next
 * buffer from the same memory stick; between pieces it gets the device
 * within a few milliseconds instead of waiting for a whole 1 MB atlas,
 * which made music stutter while a map loaded. */
#define FILE_READ_CHUNK (64u * 1024u)

static size_t read_chunked(void *dst, size_t size, FILE *f)
{
    size_t done = 0;
    while (done < size) {
        size_t want = size - done;
        size_t got;
        if (want > FILE_READ_CHUNK) want = FILE_READ_CHUNK;
        got = fread((uint8_t *)dst + done, 1, want, f);
        done += got;
        if (got != want) break;
    }
    return done;
}

static void *load_exact_file(const char *path, size_t expected_size)
{
#ifdef NARAKU_COMPRESSED_ASSETS
    char compressed_path[256];
    int path_len = snprintf(compressed_path, sizeof(compressed_path), "%s.z", path);
    if (path_len > 0 && (size_t)path_len < sizeof(compressed_path)) {
        FILE *zf = fopen(compressed_path, "rb");
        if (zf) {
            long compressed_len;
            uint8_t *compressed = NULL;
            void *data = NULL;
            uLongf output_len = (uLongf)expected_size;
            int ok = 0;
            if (fseek(zf, 0, SEEK_END) == 0 &&
                (compressed_len = ftell(zf)) > 0 &&
                fseek(zf, 0, SEEK_SET) == 0) {
                compressed = (uint8_t *)malloc((size_t)compressed_len);
                data = memalign(16, expected_size);
                if (compressed && data &&
                    read_chunked(compressed, (size_t)compressed_len, zf) == (size_t)compressed_len &&
                    uncompress((Bytef *)data, &output_len, (const Bytef *)compressed,
                               (uLong)compressed_len) == Z_OK &&
                    output_len == (uLongf)expected_size)
                    ok = 1;
            }
            fclose(zf);
            free(compressed);
            if (ok) {
                sceKernelDcacheWritebackInvalidateAll();
                return data;
            }
            free(data);
            return NULL;
        }
    }
#endif
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    void *data = memalign(16, expected_size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t got = read_chunked(data, expected_size, f);
    int extra = fgetc(f);
    fclose(f);

    if (got != expected_size || extra != EOF) {
        free(data);
        return NULL;
    }

    sceKernelDcacheWritebackInvalidateAll();
    return data;
}

#ifdef NARAKU_SWIZZLE_TEXTURES
/* Replace a freshly loaded plain texture with a swizzled copy. On any
 * failure the plain texture is kept and drawn unswizzled. */
static void *texture_swizzle_loaded(void *texture, int width, int height)
{
    uint32_t *out;
    if (!texture || (width & 3) || (height & 7)) return texture;
    out = (uint32_t *)memalign(16, (size_t)width * (size_t)height * 4u);
    if (!out) return texture;
    if (!texture_mark_swizzled(out)) { free(out); return texture; }
    swizzle_8888(out, (const uint32_t *)texture, width, height);
    free(texture);
    sceKernelDcacheWritebackRange(out, (unsigned int)(width * height * 4));
    return out;
}

static void texture_free(void *texture)
{
    texture_forget(texture);
    free(texture);
}
#else
static inline void *texture_swizzle_loaded(void *texture, int width, int height)
{
    (void)width; (void)height;
    return texture;
}
#define texture_free free
#endif

/* Retain only the currently selected alternate leader (512 KiB).
 * Original Enri's atlas stays owned by main; no per-frame file reads. */
static void *player_form_texture(void *normal)
{
    static void *alternate;
    static int loaded_actor;
    int actor=player_form_actor();
    char path[128];
    if(actor==1) {
        if(alternate) {texture_free(alternate);alternate=NULL;loaded_actor=0;}
        return normal;
    }
    if(actor!=loaded_actor) {
        texture_free(alternate);alternate=NULL;
        snprintf(path,sizeof(path),ASSET_ROOT "actor%02d_atlas.rgba8888",actor);
        alternate=texture_swizzle_loaded(load_exact_file(path,CHAR_ATLAS_BYTES),CHAR_ATLAS_W,CHAR_ATLAS_H);
        loaded_actor=actor;
    }
    return alternate ? alternate : normal;
}

static void *load_whole_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return NULL;
    }

    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }

    void *data = memalign(16, (size_t)len);
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t got = read_chunked(data, (size_t)len, f);
    fclose(f);

    if (got != (size_t)len) {
        free(data);
        return NULL;
    }

    if (out_size) *out_size = (size_t)len;
    sceKernelDcacheWritebackInvalidateAll();
    return data;
}
