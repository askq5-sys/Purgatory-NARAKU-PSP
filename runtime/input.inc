/* Controller input and language selection.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Input / language                                                          */
/* ------------------------------------------------------------------------- */

/* PPSSPP can report no buttons during the very first frame after launch.
 * Sample a short startup window so SELECT/TRIANGLE are reliable. */
static uint32_t read_boot_hotkeys(void)
{
    SceCtrlData pad;
    uint32_t seen = 0;
    int i;

    for (i = 0; i < 36; ++i) {
        sceCtrlPeekBufferPositive(&pad, 1);
        seen |= pad.Buttons;
        sceDisplayWaitVblankStart();
    }
    return seen;
}

static void config_defaults(void)
{
    g_language = 0;
    g_always_dash = 0;
    g_bgm_volume = 75;
    g_se_volume = 75;
    g_window_opacity = 195;
    g_map5_key_migration_done = 1;
    g_config_valid = 0;
}

static void save_config(void);

static int load_config(void)
{
    FILE *f;
    unsigned char b[16];
    size_t n;
    config_defaults();
    f = fopen(CONFIG_PATH, "rb");
    if (!f) return 0;
    n = fread(b, 1, sizeof(b), f);
    fclose(f);

    if (n >= 10 && memcmp(b, "NPS9", 4) == 0) {
        if (b[4] <= 4) g_language = (int)b[4];
        g_always_dash = b[5] ? 1 : 0;
        g_bgm_volume = b[6] <= 100 ? (int)b[6] : 75;
        g_se_volume = b[7] <= 100 ? (int)b[7] : 75;
        g_window_opacity = b[8] >= 64 ? (int)b[8] : 195;
        g_map5_key_migration_done = b[9] ? 1 : 0;
        g_config_valid = 1;
        return 1;
    }
    if (n >= 10 && memcmp(b, "NPS8", 4) == 0) {
        if (b[4] <= 4) g_language = (int)b[4];
        g_always_dash = b[5] ? 1 : 0;
        g_bgm_volume = b[6] <= 100 ? (int)b[6] : 75;
        g_se_volume = b[7] <= 100 ? (int)b[7] : 75;
        g_window_opacity = b[8] >= 64 ? (int)b[8] : 195;
        /* Old test saves may have picked the invisible Red Key.  Keep the
         * migration pending across restarts until Map005 is actually loaded. */
        g_map5_key_migration_done = 0;
        g_config_valid = 1;
        save_config();
        return 1;
    }
    if (n >= 10 && memcmp(b, "NPS7", 4) == 0) {
        /* 0.6.0/0.6.1 could accidentally persist Japanese/Chinese while the
         * title was still English.  Migrate once to an English baseline,
         * preserving the user's other option values.  From NPS8 onward the
         * language is a normal persistent user preference. */
        g_language = 0;
        g_always_dash = b[5] ? 1 : 0;
        g_bgm_volume = b[6] <= 100 ? (int)b[6] : 75;
        g_se_volume = b[7] <= 100 ? (int)b[7] : 75;
        g_window_opacity = b[8] >= 64 ? (int)b[8] : 195;
        g_map5_key_migration_done = 0;
        g_config_valid = 1;
        save_config();
        return 1;
    }
    if (n >= 5 && memcmp(b, "NPS6", 4) == 0) {
        g_language = 0;
        g_map5_key_migration_done = 0;
        g_config_valid = 1;
        save_config();
        return 1;
    }
    return 0;
}

static void save_config(void)
{
    FILE *f;
    unsigned char b[16];
    memset(b, 0, sizeof(b));
    memcpy(b, "NPS9", 4);
    b[4] = (unsigned char)g_language;
    b[5] = (unsigned char)(g_always_dash ? 1 : 0);
    b[6] = (unsigned char)g_bgm_volume;
    b[7] = (unsigned char)g_se_volume;
    b[8] = (unsigned char)g_window_opacity;
    b[9] = (unsigned char)(g_map5_key_migration_done ? 1 : 0);
    f = fopen(CONFIG_PATH, "wb");
    if (!f) return;
    fwrite(b, 1, sizeof(b), f);
    fclose(f);
    g_config_valid = 1;
}

static int load_saved_language(void)
{
    load_config();
    return g_config_valid ? g_language : -1;
}

static void save_language(int lang)
{
    if (lang < 0) lang = 0;
    if (lang > 4) lang = 4;
    g_language = lang;
    save_config();
}

static uint32_t ui_ok_mask(void)
{
    return PSP_CTRL_CROSS;
}

static uint32_t ui_cancel_mask(void)
{
    return PSP_CTRL_CIRCLE;
}

static int choose_language(int selected)
{
    SceCtrlData pad;
    uint32_t prev = 0;
    const char *names[5] = {
        "English",
        "Japanese",
        "Simplified Chinese",
        "Traditional Chinese",
        "Russian"
    };

    if (selected < 0 || selected > 4) selected = 0;
    pspDebugScreenInit();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    while (1) {
        int i;
        pspDebugScreenClear();
        pspDebugScreenSetXY(0, 1);
        pspDebugScreenPrintf("NARAKU PSP 1.7.8\n\n");
        pspDebugScreenPrintf("Choose language:\n\n");

        for (i = 0; i < 5; ++i)
            pspDebugScreenPrintf("%c %s\n", i == selected ? '>' : ' ', names[i]);

        pspDebugScreenPrintf("\nD-PAD: select   X: confirm\n");

        sceCtrlPeekBufferPositive(&pad, 1);
        {
            uint32_t pressed = pad.Buttons & ~prev;
            if (pressed & PSP_CTRL_UP) selected = (selected + 4) % 5;
            if (pressed & PSP_CTRL_DOWN) selected = (selected + 1) % 5;
            if (pressed & PSP_CTRL_CROSS) {
                save_language(selected);
                do {
                    sceCtrlPeekBufferPositive(&pad, 1);
                    sceDisplayWaitVblankStart();
                } while (pad.Buttons & PSP_CTRL_CROSS);
                return selected;
            }
        }
        prev = pad.Buttons;
        sceDisplayWaitVblankStart();
    }
}

static const char *lang_tag(int lang)
{
    switch (lang) {
        case 1: return "ja";
        case 2: return "zhcn";
        case 3: return "zhtw";
        default: return "en";
    }
}
