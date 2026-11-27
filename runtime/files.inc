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
                    fread(compressed, 1, (size_t)compressed_len, zf) == (size_t)compressed_len &&
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

    size_t got = fread(data, 1, expected_size, f);
    int extra = fgetc(f);
    fclose(f);

    if (got != expected_size || extra != EOF) {
        free(data);
        return NULL;
    }

    sceKernelDcacheWritebackInvalidateAll();
    return data;
}

/* Retain only the currently selected alternate leader (512 KiB).
 * Original Enri's atlas stays owned by main; no per-frame file reads. */
static void *player_form_texture(void *normal)
{
    static void *alternate;
    static int loaded_actor;
    int actor=player_form_actor();
    char path[128];
    if(actor==1) {
        if(alternate) {free(alternate);alternate=NULL;loaded_actor=0;}
        return normal;
    }
    if(actor!=loaded_actor) {
        free(alternate);alternate=NULL;
        snprintf(path,sizeof(path),ASSET_ROOT "actor%02d_atlas.rgba8888",actor);
        alternate=load_exact_file(path,CHAR_ATLAS_BYTES);
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

    size_t got = fread(data, 1, (size_t)len, f);
    fclose(f);

    if (got != (size_t)len) {
        free(data);
        return NULL;
    }

    if (out_size) *out_size = (size_t)len;
    sceKernelDcacheWritebackInvalidateAll();
    return data;
}
