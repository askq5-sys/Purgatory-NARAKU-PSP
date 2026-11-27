/* Opening and the Map001 intro.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Opening / Map001 intro                                                    */
/* ------------------------------------------------------------------------- */

static const char *opening1_path(int lang)
{
    switch (lang) {
        case 1: return ASSET_ROOT "opening1_ja.rgba8888";
        case 2: return ASSET_ROOT "opening1_zhcn.rgba8888";
        case 3: return ASSET_ROOT "opening1_zhtw.rgba8888";
        default: return ASSET_ROOT "opening1_en.rgba8888";
    }
}

static const char *opening2_path(int lang)
{
    return lang == 0
        ? ASSET_ROOT "opening2_en.rgba8888"
        : ASSET_ROOT "opening2_default.rgba8888";
}

static const char *dialog_path(int lang, int which)
{
    static const char *paths[4][2] = {
        {ASSET_ROOT "dialog1_en.rgba8888", ASSET_ROOT "dialog2_en.rgba8888"},
        {ASSET_ROOT "dialog1_ja.rgba8888", ASSET_ROOT "dialog2_ja.rgba8888"},
        {ASSET_ROOT "dialog1_zhcn.rgba8888", ASSET_ROOT "dialog2_zhcn.rgba8888"},
        {ASSET_ROOT "dialog1_zhtw.rgba8888", ASSET_ROOT "dialog2_zhtw.rgba8888"}
    };
    if (lang < 0 || lang > 3) lang = 0;
    if (which < 0 || which > 1) which = 0;
    return paths[lang][which];
}

static void run_opening(int lang)
{
    void *pic1 = load_exact_file(opening1_path(lang), PIC_TEX_BYTES);
    void *pic2;

    if (!pic1) fatal_error("Could not load Opening 1 texture");
    fade_picture(pic1, 0, 255, 120);
    wait_cross();
    fade_picture(pic1, 255, 0, 60);
    free(pic1);

    pic2 = load_exact_file(opening2_path(lang), PIC_TEX_BYTES);
    if (!pic2) fatal_error("Could not load Opening 2 texture");
    fade_picture(pic2, 0, 255, 120);
    wait_cross();
    fade_picture(pic2, 255, 0, 60);
    free(pic2);
}

static void wait_scene_frames(
    int frames, const MapState *m, void *char_atlas,
    void *a1, void *fall_atlas, int player_visible,
    float player_x, float player_y, int player_pattern, int player_dir,
    int fall_slot, float fall_y, int black_alpha)
{
    int i;
    for (i = 0; i < frames; ++i) {
        render_world(
            m, char_atlas, a1, fall_atlas,
            player_visible, player_x, player_y,
            player_pattern, player_dir,
            fall_slot, fall_y,
            NULL, NULL, 0, black_alpha, 0
        );
    }
}

static void fade_black_scene(
    int from, int to, int frames,
    const MapState *m, void *char_atlas,
    void *a1, void *fall_atlas,
    int player_visible, float player_x, float player_y,
    int fall_slot, float fall_y)
{
    int i;
    for (i = 1; i <= frames; ++i) {
        int a = from + (to - from) * i / frames;
        render_world(
            m, char_atlas, a1, fall_atlas,
            player_visible, player_x, player_y, 1, 0,
            fall_slot, fall_y,
            NULL, NULL, 0, a, 0
        );
    }
}

static void flash_red_scene(
    int frames, const MapState *m, void *char_atlas,
    void *a1, void *fall_atlas, int fall_slot, float fall_y)
{
    int i;
    for (i = 0; i < frames; ++i) {
        int a = 255 - (255 * i / frames);
        render_world(
            m, char_atlas, a1, fall_atlas,
            0, 0.0f, 0.0f, 1, 0,
            fall_slot, fall_y,
            NULL, NULL, a, 0, 0
        );
    }
}

static float animate_fall_move(
    int slot, float start_y, int tiles, int frames_per_tile,
    const MapState *m, void *char_atlas, void *a1, void *fall_atlas)
{
    int total = tiles * frames_per_tile;
    float end_y = start_y + (float)tiles;
    int i;

    for (i = 1; i <= total; ++i) {
        float t = (float)i / (float)total;
        float y = start_y + (end_y - start_y) * t;
        render_world(
            m, char_atlas, a1, fall_atlas,
            0, 0.0f, 0.0f, 1, 0,
            slot, y,
            NULL, NULL, 0, 0, 0
        );
    }
    return end_y;
}

static void wait_blank_message(
    const MapState *m, void *char_atlas,
    void *a1, void *fall_atlas, int fall_slot, float fall_y)
{
    SceCtrlData pad;
    uint32_t prev;

    sceCtrlPeekBufferPositive(&pad, 1);
    prev = pad.Buttons;

    while (1) {
        render_world(
            m, char_atlas, a1, fall_atlas,
            0, 0.0f, 0.0f, 1, 0,
            fall_slot, fall_y,
            NULL, NULL, 0, 0, 1
        );

        sceCtrlPeekBufferPositive(&pad, 1);
        if ((pad.Buttons & ~prev) & PSP_CTRL_CROSS) return;
        prev = pad.Buttons;
    }
}

static void wait_dialog(
    void *dialog, const MapState *m, void *char_atlas,
    void *a1, void *portrait,
    float player_x, float player_y)
{
    SceCtrlData pad;
    uint32_t prev;

    sceCtrlPeekBufferPositive(&pad, 1);
    prev = pad.Buttons;

    while (1) {
        render_world(
            m, char_atlas, a1, NULL,
            1, player_x, player_y, 1, 0,
            -1, 0.0f,
            portrait, dialog, 0, 0, 0
        );

        sceCtrlPeekBufferPositive(&pad, 1);
        if ((pad.Buttons & ~prev) & PSP_CTRL_CROSS) return;
        prev = pad.Buttons;
    }
}

static void run_map001_intro(
    int lang, const MapState *m, void *char_atlas,
    void *a1, void *fall_atlas, int start_x, int start_y)
{
    float player_x = ((float)start_x + 0.5f) * TILE_PX;
    float player_y = ((float)start_y + 1.0f) * TILE_PX;
    float fall_y = 0.0f;

    /* The falling event is a separate sprite.  v0.2.0 accidentally let
     * helper calls with player=(0,0) recalculate the camera, which caused
     * the visible up/down snap during the abyss fall.  Keep the Map001
     * camera fixed for the whole intro, exactly like a scripted MZ scene. */
    {
        float map_w = (float)(m->w * TILE_PX);
        float map_h = (float)(m->h * TILE_PX);
        float max_x = map_w > SCREEN_W ? map_w - SCREEN_W : 0.0f;
        float max_y = map_h > SCREEN_H ? map_h - SCREEN_H : 0.0f;
        g_camera_lock_x = max_x > 0.0f ? clamp_float(player_x - SCREEN_W * 0.5f, 0.0f, max_x) : 0.0f;
        g_camera_lock_y = max_y > 0.0f ? clamp_float(player_y - SCREEN_H * 0.5f, 0.0f, max_y) : 0.0f;
        g_camera_locked = 1;
    }
    void *portrait;
    void *dialog1;
    void *dialog2;

    wait_scene_frames(
        120, m, char_atlas, NULL, fall_atlas,
        0, player_x, player_y, 1, 0, -1, 0.0f, 255
    );

    g_picture6_visible = 1;
    g_picture6_alpha = 255;
    fade_black_scene(
        255, 0, 60, m, char_atlas,
        a1, fall_atlas, 0, player_x, player_y, -1, 0.0f
    );

    wait_scene_frames(
        40, m, char_atlas, a1, fall_atlas,
        0, player_x, player_y, 1, 0, -1, 0.0f, 0
    );

    fall_y = animate_fall_move(0, fall_y, 6, 4, m, char_atlas, a1, fall_atlas);
    play_se_async(SE_BONE_PATH);
    flash_red_scene(20, m, char_atlas, a1, fall_atlas, 0, fall_y);

    fall_y = animate_fall_move(1, fall_y, 1, 8, m, char_atlas, a1, fall_atlas);
    fall_y = animate_fall_move(2, fall_y, 1, 16, m, char_atlas, a1, fall_atlas);

    play_se_async(SE_WATER_PATH);
    fall_y = animate_fall_move(3, fall_y, 1, 64, m, char_atlas, a1, fall_atlas);

    wait_scene_frames(
        20, m, char_atlas, a1, fall_atlas,
        0, player_x, player_y, 1, 0, 3, fall_y, 0
    );

    /* Original NARAKU uses transparent, blank Show Text entries here.
     * On the PC build they do not present the player with a visible/useful
     * confirmation step.  The old PSP intro turned them into two dark X-gates,
     * producing the fake "jerk on X" pauses reported during the fall. */

    play_se_async(SE_SPLASH_PATH);
    wait_scene_frames(10, m, char_atlas, a1, fall_atlas, 0, player_x, player_y, 1, 0, 4, fall_y, 0);
    wait_scene_frames(10, m, char_atlas, a1, fall_atlas, 0, player_x, player_y, 1, 0, 5, fall_y, 0);
    wait_scene_frames(10, m, char_atlas, a1, fall_atlas, 0, player_x, player_y, 1, 0, 3, fall_y, 0);

    /* Original NARAKU uses transparent, blank Show Text entries here.
     * On the PC build they do not present the player with a visible/useful
     * confirmation step.  The old PSP intro turned them into two dark X-gates,
     * producing the fake "jerk on X" pauses reported during the fall. */

    play_se_async(SE_SPLASH_PATH);
    wait_scene_frames(10, m, char_atlas, a1, fall_atlas, 0, player_x, player_y, 1, 0, 4, fall_y, 0);
    wait_scene_frames(10, m, char_atlas, a1, fall_atlas, 0, player_x, player_y, 1, 0, 5, fall_y, 0);
    wait_scene_frames(10, m, char_atlas, a1, fall_atlas, 0, player_x, player_y, 1, 0, 3, fall_y, 0);

    fade_black_scene(
        0, 255, 50, m, char_atlas,
        a1, fall_atlas, 0, player_x, player_y, 3, fall_y
    );
    play_se_async(SE_WATER_PATH);

    wait_scene_frames(
        60, m, char_atlas, a1, fall_atlas,
        0, player_x, player_y, 1, 0, -1, 0.0f, 255
    );

    fade_black_scene(
        255, 0, 50, m, char_atlas,
        a1, NULL, 1, player_x, player_y, -1, 0.0f
    );

    portrait = load_exact_file(PORTRAIT_PATH, PIC_TEX_BYTES);
    if (!portrait) fatal_error("Could not load Enri portrait");

    dialog1 = load_exact_file(dialog_path(lang, 0), DIALOG_TEX_BYTES);
    if (!dialog1) fatal_error("Could not load first dialogue texture");
    wait_dialog(dialog1, m, char_atlas, a1, portrait, player_x, player_y);
    free(dialog1);

    dialog2 = load_exact_file(dialog_path(lang, 1), DIALOG_TEX_BYTES);
    if (!dialog2) fatal_error("Could not load second dialogue texture");
    wait_dialog(dialog2, m, char_atlas, a1, portrait, player_x, player_y);
    free(dialog2);
    free(portrait);

    g_camera_locked = 0;
    g_switches[3] = 1;
}
