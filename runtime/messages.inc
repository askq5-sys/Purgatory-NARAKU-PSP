/* Runtime messages and screen fades.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Runtime messages / fades                                                  */
/* ------------------------------------------------------------------------- */

static void event_texture_path(
    char *out, size_t out_size, int map_id, int event_id, int lang)
{
    snprintf(
        out, out_size,
        ASSET_ROOT "m%03d_e%03d_%s.rgba8888",
        map_id, event_id, lang_tag(lang)
    );
}

static void wait_event_message(
    int event_id, int lang,
    const MapState *m, void *char_atlas, void *a1,
    float player_x, float player_y,
    int player_pattern, int player_dir)
{
    char path[192];
    void *dialog;
    SceCtrlData pad;
    uint32_t prev;

    event_texture_path(path, sizeof(path), m->id, event_id, lang);
    dialog = load_exact_file(path, DIALOG_TEX_BYTES);
    if (!dialog) return;

    do {
        render_world(
            m, char_atlas, a1, NULL,
            g_player_visible, player_x, player_y, player_pattern, player_dir,
            -1, 0.0f, NULL, dialog, 0, 0, 0
        );
        sceCtrlPeekBufferPositive(&pad, 1);
    } while (pad.Buttons & PSP_CTRL_CROSS);

    prev = pad.Buttons;
    while (1) {
        render_world(
            m, char_atlas, a1, NULL,
            g_player_visible, player_x, player_y, player_pattern, player_dir,
            -1, 0.0f, NULL, dialog, 0, 0, 0
        );
        sceCtrlPeekBufferPositive(&pad, 1);
        if ((pad.Buttons & ~prev) & PSP_CTRL_CROSS) break;
        prev = pad.Buttons;
    }

    do {
        sceCtrlPeekBufferPositive(&pad, 1);
        sceDisplayWaitVblankStart();
    } while (pad.Buttons & PSP_CTRL_CROSS);

    free(dialog);
}

static void fade_game_world(
    const MapState *m, void *char_atlas, void *a1,
    float player_x, float player_y,
    int player_pattern, int player_dir,
    int from, int to, int frames)
{
    int i;
    for (i = 1; i <= frames; ++i) {
        int a = from + (to - from) * i / frames;
        render_world(
            m, char_atlas, a1, NULL,
            g_player_visible, player_x, player_y, player_pattern, player_dir,
            -1, 0.0f, NULL, NULL, 0, a, 0
        );
    }
}
