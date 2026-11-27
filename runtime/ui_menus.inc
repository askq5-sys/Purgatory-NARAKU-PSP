/* Title, item, options and save/load menus.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Native PSP UI: original NARAKU menu/save/options bridge                  */
/* ------------------------------------------------------------------------- */

static void draw_character_frame(int pattern, int direction_row, float foot_x, float foot_y);

enum {
    UI_L_ITEMS = 0,
    UI_L_KEY_ITEMS = 1,
    UI_L_SAVE_PROMPT = 2,
    UI_L_LOAD_PROMPT = 3,
    UI_L_FILE = 4,
    UI_L_SETTINGS = 5,
    UI_L_BACK = 6,
    UI_L_SAVE = 7,
    UI_L_LOAD = 8,
    UI_L_RETURN_TITLE = 9,
    UI_L_CANCEL = 10,
    UI_L_NEW_GAME = 11,
    UI_L_CONTINUE = 12,
    UI_L_ALWAYS_DASH = 13,
    UI_L_BGM_VOLUME = 14,
    UI_L_SE_VOLUME = 15,
    UI_L_WINDOW_OPACITY = 16,
    UI_L_LANGUAGE = 17,
    UI_L_ON = 18,
    UI_L_OFF = 19,
    UI_L_EMPTY = 20,
    UI_L_INVENTORY_TITLE = 21,
    UI_L_OPTIONS_TITLE = 22,
    UI_L_RETURN_TITLE_CONFIRM = 23,
    UI_L_SHUTDOWN = 24
};

static int init_ui_data(void)
{
    uint32_t item_off, label_off, blob_off;
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.data = (uint8_t *)load_whole_file(UI_DATA_PATH, &g_ui.size);
    if (!g_ui.data || g_ui.size < UI_HEADER_BYTES) return 0;
    if (memcmp(g_ui.data, "NU60", 4) != 0) return 0;
    g_ui.item_count = (int)read_u16_le(g_ui.data + 4);
    g_ui.label_count = (int)read_u16_le(g_ui.data + 6);
    item_off = read_u32_le(g_ui.data + 8);
    label_off = read_u32_le(g_ui.data + 12);
    blob_off = read_u32_le(g_ui.data + 16);
    if (g_ui.item_count < 1 || g_ui.item_count > MAX_ITEMS ||
        g_ui.label_count < 1 || g_ui.label_count > 128) return 0;
    if (item_off > g_ui.size || label_off > g_ui.size || blob_off > g_ui.size) return 0;
    if ((size_t)item_off + (size_t)g_ui.item_count * UI_ITEM_REC_BYTES > g_ui.size) return 0;
    if ((size_t)label_off + (size_t)g_ui.label_count * 4u > g_ui.size) return 0;
    g_ui.item_table = g_ui.data + item_off;
    g_ui.label_table = g_ui.data + label_off;
    g_ui.blob = g_ui.data + blob_off;
    g_ui.blob_size = g_ui.size - blob_off;
    g_ui.iconset = load_exact_file(UI_ICONSET_PATH, UI_ICONSET_BYTES);
    if (!g_ui.iconset) return 0;
    return 1;
}

static int ui_label_text(int id, int lang, const char **text, size_t *len)
{
    uint32_t off;
    const uint8_t *p;
    uint16_t lens[4];
    int i;
    if (!g_ui.data || id < 0 || id >= g_ui.label_count || lang < 0 || lang > 4) return 0;
    off = read_u32_le(g_ui.label_table + id * 4);
    if (off + 8u > g_ui.blob_size) return 0;
    p = g_ui.blob + off;
    for (i = 0; i < 4; ++i) lens[i] = read_u16_le(p + i * 2);
    p += 8;
    for (i = 0; i < 4; ++i) {
        if ((size_t)(p - g_ui.blob) + lens[i] > g_ui.blob_size) return 0;
        if (i == vm_text_language(lang)) {
            if (text) *text = (const char *)p;
            if (len) *len = lens[i];
            if(lang==4 && text && len)russian_lookup(text,len);
            return 1;
        }
        p += lens[i];
    }
    return 0;
}

static int ui_item_text(
    int item_id, int lang,
    const char **name, size_t *name_len,
    const char **desc, size_t *desc_len,
    int *icon, int *itype, int *occasion)
{
    const uint8_t *rec;
    uint32_t off, size;
    const uint8_t *p;
    uint16_t lens[8];
    int i;
    if (!g_ui.data || item_id < 0 || item_id >= g_ui.item_count || lang < 0 || lang > 4) return 0;
    rec = g_ui.item_table + item_id * UI_ITEM_REC_BYTES;
    off = read_u32_le(rec + 4);
    size = read_u32_le(rec + 8);
    if (!size || off + size > g_ui.blob_size || size < 16) return 0;
    if (icon) *icon = (int)read_u16_le(rec + 0);
    if (itype) *itype = (int)rec[2];
    if (occasion) *occasion = (int)rec[3];
    p = g_ui.blob + off;
    for (i = 0; i < 8; ++i) lens[i] = read_u16_le(p + i * 2);
    p += 16;
    for (i = 0; i < 4; ++i) {
        size_t nl = lens[i * 2 + 0];
        size_t dl = lens[i * 2 + 1];
        if ((size_t)(p - (g_ui.blob + off)) + nl + dl > size) return 0;
        if (i == vm_text_language(lang)) {
            if (name) *name = (const char *)p;
            if (name_len) *name_len = nl;
            if (desc) *desc = (const char *)(p + nl);
            if (desc_len) *desc_len = dl;
            if(lang==4) {
                if(name && name_len)russian_lookup(name,name_len);
                if(desc && desc_len)russian_lookup(desc,desc_len);
            }
            return 1;
        }
        p += nl + dl;
    }
    return 0;
}

static void ui_draw_label(int id, float x, float y, float max_x, int lines)
{
    const char *text = NULL;
    size_t len = 0;
    if (ui_label_text(id, g_language, &text, &len))
        draw_utf8_wrapped(text, len, x, y, max_x, lines);
}

static void ui_frame_begin(void)
{
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

static void ui_frame_end(void)
{
    perf_hud_draw();
    sceGuFinish();
    sceGuSync(0, 0);
    perf_frame_done(0);
    sceDisplayWaitVblankStart();
    perf_after_vblank();
    g_draw_buffer = sceGuSwapBuffers();
}

static void ui_panel(float x, float y, float w, float h)
{
    int alpha = g_window_opacity;
    if (alpha < 64) alpha = 64;
    if (alpha > 255) alpha = 255;
    draw_solid_rect(x, y, w, h, 8, 8, 12, alpha);
    draw_solid_rect(x + 2, y + 2, w - 4, 1, 210, 210, 220, 210);
}

#include "inventory.inc"

/* Settings values share one right edge in every language. */
static void ui_draw_settings_value(const char *text, size_t len, float y)
{
    const char *p=text,*end=text+len;
    float width=0;
    int color=0;
    while(p<end) {
        const uint8_t *rec;
        if(parse_text_color_escape(&p,end,&color))continue;
        rec=font_find_glyph(utf8_next_cp(&p,end));
        if(!rec)rec=font_find_glyph('?');
        width+=(rec?rec[7]:10)*0.75f;
    }
    draw_utf8_wrapped(text,len,444.0f-width,y,444.0f,1);
}

static void ui_draw_value_number(int value, float y)
{
    char buf[32];
    snprintf(buf,sizeof(buf),"%d",value);
    ui_draw_settings_value(buf,strlen(buf),y);
}

/* Save-deletion UI follows the same language order as the game. */
static const char *save_delete_label(int id)
{
    static const char *text[5][7]={
        {"Delete save files","Delete all save files?","Language and settings will be kept.","Cancel","Delete","Save files deleted.","Some save files could not be deleted."},
        {"セーブデータを消去","すべてのセーブデータを消去しますか？","言語と設定は保持されます。","キャンセル","消去","セーブデータを消去しました。","一部のセーブデータを消去できませんでした。"},
        {"清空存档","清空所有存档？","语言和设置将保留。","取消","清空","存档已清空。","部分存档无法清空。"},
        {"清空存檔","清空所有存檔？","語言和設定將保留。","取消","清空","存檔已清空。","部分存檔無法清空。"},
        {"Удалить сохранения","Удалить все сохранения?","Язык и настройки сохранятся.","Отмена","Удалить","Сохранения удалены.","Не удалось удалить часть сохранений."}
    };
    int lang=g_language>=0 && g_language<5?g_language:0;
    return text[lang][id>=0 && id<7?id:0];
}

static int ui_delete_save_files(void)
{
    char path[192];int slot,failed=0;
    if(remove(PROGRESS_PATH)!=0 && errno!=ENOENT)failed=1;
    for(slot=1;slot<=SAVE_SLOT_COUNT;++slot) {
        save_slot_path(slot,path,sizeof(path));
        if(remove(path)!=0 && errno!=ENOENT)failed=1;
    }
    return !failed;
}

static void ui_confirm_delete_saves(void)
{
    SceCtrlData pad;uint32_t prev;int yes=0,done=0,result=0;
    sceCtrlPeekBufferPositive(&pad,1);prev=pad.Buttons;
    while(!done) {
        ui_frame_begin();ui_panel(30,70,420,130);
        draw_utf8_wrapped(save_delete_label(1),strlen(save_delete_label(1)),48,88,430,1);
        draw_utf8_wrapped(save_delete_label(2),strlen(save_delete_label(2)),48,118,430,1);
        draw_solid_rect(yes?260:48,151,170,25,160,0,0,150);
        draw_utf8_wrapped(save_delete_label(3),strlen(save_delete_label(3)),58,155,210,1);
        draw_utf8_wrapped(save_delete_label(4),strlen(save_delete_label(4)),270,155,430,1);
        ui_frame_end();sceCtrlPeekBufferPositive(&pad,1);
        uint32_t pressed=pad.Buttons&~prev;
        if(pressed&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT))yes=!yes;
        if(pressed&ui_cancel_mask())return;
        if(pressed&ui_ok_mask()) {
            if(!yes)return;
            result=ui_delete_save_files();done=1;
        }
        prev=pad.Buttons;
    }
    do {
        ui_frame_begin();ui_panel(30,90,420,80);
        const char *text=save_delete_label(result?5:6);
        draw_utf8_wrapped(text,strlen(text),48,112,430,2);
        ui_frame_end();sceCtrlPeekBufferPositive(&pad,1);
        uint32_t pressed=pad.Buttons&~prev;prev=pad.Buttons;
        if(pressed&(ui_ok_mask()|ui_cancel_mask()))break;
    }while(1);
}

static void ui_options_scene(void)
{
    SceCtrlData pad;
    uint32_t prev;
    int cursor = 0;
    int running = 1;
    const int rows = 7;

    /* Settings is normally entered with X.  Do not reinterpret that still-
     * held X as an immediate press on the Language row (the 0.6.1 bug that
     * changed English to Japanese every time Settings was opened). */
    sceCtrlPeekBufferPositive(&pad, 1);
    prev = pad.Buttons;

    while (running) {
        int i;
        ui_frame_begin();
        ui_panel(20, 12, SCREEN_W - 40, 248);
        ui_draw_label(UI_L_OPTIONS_TITLE, 34, 20, 450, 1);
        for (i = 0; i < rows; ++i) {
            float y = 56.0f + i * 27.0f;
            if (i == cursor)
                draw_solid_rect(29, y - 3, 422, 29, 58, 68, 94, 210);
            if (i == 0) {
                static const char *langs[5] = {"English", "日本語", "简体中文", "繁體中文", "Русский"};
                ui_draw_label(UI_L_LANGUAGE, 38, y, 280, 1);
                ui_draw_settings_value(langs[g_language],strlen(langs[g_language]),y);
            } else if (i == 1) {
                ui_draw_label(UI_L_ALWAYS_DASH, 38, y, 300, 1);
                const char *value; size_t len;
                if(ui_label_text(g_always_dash?UI_L_ON:UI_L_OFF,g_language,&value,&len))
                    ui_draw_settings_value(value,len,y);
            } else if (i == 2) {
                ui_draw_label(UI_L_BGM_VOLUME, 38, y, 300, 1);
                ui_draw_value_number(g_bgm_volume, y);
            } else if (i == 3) {
                ui_draw_label(UI_L_SE_VOLUME, 38, y, 300, 1);
                ui_draw_value_number(g_se_volume, y);
            } else if (i == 4) {
                ui_draw_label(UI_L_WINDOW_OPACITY, 38, y, 300, 1);
                ui_draw_value_number(g_window_opacity, y);
            } else if(i==5) {
                draw_utf8_wrapped(save_delete_label(0),strlen(save_delete_label(0)),38,y,444,1);
            } else {
                ui_draw_label(UI_L_BACK, 38, y, 444, 1);
            }
        }
        ui_frame_end();

        sceCtrlPeekBufferPositive(&pad, 1);
        {
            uint32_t pressed = pad.Buttons & ~prev;
            int delta = 0;
            if (pressed & PSP_CTRL_UP) { cursor = (cursor + rows - 1) % rows; play_se_async(SE_UI_CURSOR_PATH); }
            if (pressed & PSP_CTRL_DOWN) { cursor = (cursor + 1) % rows; play_se_async(SE_UI_CURSOR_PATH); }
            if (pressed & PSP_CTRL_LEFT) delta = -1;
            if (pressed & PSP_CTRL_RIGHT) delta = 1;
            if (pressed & ui_ok_mask()) {
                if (cursor == 0) delta = 1;
                else if (cursor == 1) { g_always_dash = !g_always_dash; play_se_async(SE_UI_OK_PATH); }
                else if (cursor == 5) { ui_confirm_delete_saves();sceCtrlPeekBufferPositive(&pad,1); }
                else if (cursor == 6) { running = 0; play_se_async(SE_UI_CANCEL_PATH); }
            }
            if (delta) {
                if (cursor == 0) {
                    g_language = (g_language + (delta > 0 ? 1 : 4)) % 5;
                    save_config();
                    play_se_async(SE_UI_CURSOR_PATH);
                } else if (cursor == 1) {
                    g_always_dash = !g_always_dash;
                    play_se_async(SE_UI_CURSOR_PATH);
                } else if (cursor == 2) {
                    g_bgm_volume += delta * 5;
                    if (g_bgm_volume < 0) g_bgm_volume = 0;
                    if (g_bgm_volume > 100) g_bgm_volume = 100;
                    play_se_async(SE_UI_CURSOR_PATH);
                } else if (cursor == 3) {
                    g_se_volume += delta * 5;
                    if (g_se_volume < 0) g_se_volume = 0;
                    if (g_se_volume > 100) g_se_volume = 100;
                    play_se_async(SE_UI_CURSOR_PATH);
                } else if (cursor == 4) {
                    g_window_opacity += delta * 16;
                    if (g_window_opacity < 64) g_window_opacity = 64;
                    if (g_window_opacity > 255) g_window_opacity = 255;
                }
            }
            if (pressed & ui_cancel_mask()) {
                running = 0;
                play_se_async(SE_UI_CANCEL_PATH);
            }
        }
        prev = pad.Buttons;
    }
    save_config();
}

static void ui_draw_save_background(void)
{
    int i;
    /* The original save/load scenes sit on a black -> deep blood-red vertical
     * gradient, not on the gameplay frame.  Sixteen bands are visually smooth
     * enough at the PSP's 272px height and cost almost nothing. */
    for (i = 0; i < 16; ++i) {
        int r = 34 - i * 2;
        if (r < 2) r = 2;
        draw_solid_rect(0.0f, (float)(i * 17), (float)SCREEN_W, 17.0f,
                        r, 0, 3, 255);
    }
}

static void ui_format_playtime(uint32_t seconds, char *buf, size_t size)
{
    uint32_t h = seconds / 3600u;
    uint32_t m = (seconds / 60u) % 60u;
    uint32_t s = seconds % 60u;
    snprintf(buf, size, "%02u:%02u:%02u",
             (unsigned int)h, (unsigned int)m, (unsigned int)s);
}

#include "save_previews.h"

static int ui_save_load_scene(
    int saving,
    int current_map, int current_x, int current_y, int current_dir,
    ProgressState *loaded)
{
    SceCtrlData pad;
    uint32_t prev = 0;
    int cursor = 0;
    int scroll = 0;
    const int visible = 5;

    /* Direct Scene_Save calls from books remain available when the menu
     * command is disabled, matching MZ SceneManager.push(Scene_Save). */

    /* The X press that opened Save/Load must never also activate File 1.
     * The slot screen is armed only after that physical press is released. */
    sceCtrlPeekBufferPositive(&pad, 1);
    prev = pad.Buttons;

    clear_save_previews();
    while (1) {
        int i;
        if (cursor < scroll) scroll = cursor;
        if (cursor >= scroll + visible) scroll = cursor - visible + 1;
        if (scroll < 0) scroll = 0;
        if (scroll > SAVE_SLOT_COUNT - visible) scroll = SAVE_SLOT_COUNT - visible;

        ui_frame_begin();
        ui_draw_save_background();
        draw_naraku_window(4.0f, 18.0f, 472.0f, 38.0f, 230);
        {
            const char *prompt;
            size_t prompt_len;
            if (ui_label_text(saving ? UI_L_SAVE_PROMPT : UI_L_LOAD_PROMPT,
                              g_language, &prompt, &prompt_len))
                ui_inventory_name(prompt, prompt_len, 14, 21, 448);
        }

        draw_naraku_window(4.0f, 60.0f, 472.0f, 207.0f, 225);
        for (i = 0; i < visible; ++i) {
            int slot = scroll + i + 1;
            float y = 65.0f + i * 39.0f;
            int map_id = 0, sx = 0, sy = 0, actor = 1;
            uint32_t playtime = 0;
            int exists = save_slot_metadata(slot, &map_id, &sx, &sy, &playtime, &actor);
            char slot_text[32];

            if (slot - 1 == cursor)
                draw_solid_rect(10.0f, y, 460.0f, 36.0f, 72, 8, 20, 220);
            else if ((i & 1) == 0)
                draw_solid_rect(10.0f, y, 460.0f, 36.0f, 12, 8, 9, 135);

            if (g_language == 0) {
                snprintf(slot_text, sizeof(slot_text), "file %d", slot);
                ui_inventory_name(slot_text, strlen(slot_text), 16, y + 2, 98);
            } else {
                const char *label;
                size_t label_len;
                if (ui_label_text(UI_L_FILE, g_language, &label, &label_len))
                    ui_inventory_name(label, label_len, 16, y + 2, 60);
                snprintf(slot_text, sizeof(slot_text), "%d", slot);
                ui_inventory_name(slot_text, strlen(slot_text), 82, y + 2, 32);
            }

            if (exists) {
                char time_text[32];
                void *preview=save_preview_texture(actor);
                if(preview) {
                    bind_texture_8888(preview,64,128);
                    set_pixel_art_texture_state();
                    sceGuTexFilter(GU_LINEAR,GU_LINEAR);
                    draw_bound_rect(1,1,CHAR_SRC_W,CHAR_SRC_H,
                        142,y+2,32.0f*CHAR_SRC_W/CHAR_SRC_H,32);
                    set_pixel_art_texture_state();
                }
                ui_format_playtime(playtime, time_text, sizeof(time_text));
                ui_inventory_name(time_text, strlen(time_text), 364, y + 2, 102);
            }
        }
        ui_frame_end();

        sceCtrlPeekBufferPositive(&pad, 1);
        {
            uint32_t pressed = pad.Buttons & ~prev;
            if (pressed & PSP_CTRL_UP) {
                cursor = (cursor + SAVE_SLOT_COUNT - 1) % SAVE_SLOT_COUNT;
                play_se_async(SE_UI_CURSOR_PATH);
            }
            if (pressed & PSP_CTRL_DOWN) {
                cursor = (cursor + 1) % SAVE_SLOT_COUNT;
                play_se_async(SE_UI_CURSOR_PATH);
            }
            if (pressed & PSP_CTRL_LTRIGGER) {
                cursor -= visible; if (cursor < 0) cursor = 0;
                play_se_async(SE_UI_CURSOR_PATH);
            }
            if (pressed & PSP_CTRL_RTRIGGER) {
                cursor += visible; if (cursor >= SAVE_SLOT_COUNT) cursor = SAVE_SLOT_COUNT - 1;
                play_se_async(SE_UI_CURSOR_PATH);
            }
            if (pressed & ui_ok_mask()) {
                int slot = cursor + 1;
                if (saving) {
                    if (save_to_slot(slot, current_map, current_x, current_y, current_dir)) {
                        play_se_async(SE_UI_SAVE_PATH);
                        clear_save_previews(); return 1;
                    }
                } else if (save_slot_exists(slot) && loaded && load_from_slot(slot, loaded)) {
                    play_se_async(SE_UI_LOAD_PATH);
                    clear_save_previews(); return 1;
                } else {
                    play_se_async(SE_UI_BUZZER_PATH);
                }
            }
            if (pressed & ui_cancel_mask()) {
                play_se_async(SE_UI_CANCEL_PATH);
                clear_save_previews(); return 0;
            }
        }
        prev = pad.Buttons;
    }
}

static const char *title_path_for_language(int lang)
{
    if (lang == 1) return TITLE_JA_PATH;
    if (lang == 2) return TITLE_ZHCN_PATH;
    if (lang == 3) return TITLE_ZHTW_PATH;
    return TITLE_EN_PATH;
}

typedef struct {
    uint32_t held;
    int fresh;
    unsigned long long next;
} MenuNav;

static int menu_nav_step(MenuNav *nav,uint32_t buttons,unsigned long long now)
{
    uint32_t direction=buttons & (PSP_CTRL_UP|PSP_CTRL_DOWN);
    int step;
    nav->fresh = 0;
    if(!direction || direction==(PSP_CTRL_UP|PSP_CTRL_DOWN)) {
        nav->held=0;nav->next=0;return 0;
    }
    step=direction==PSP_CTRL_UP?-1:1;
    if(direction!=nav->held) {
        nav->held=direction;nav->fresh=1;nav->next=now+400000;return step;
    }
    if(nav->next && now>=nav->next) {
        nav->next=now+100000;return step;
    }
    return 0;
}

/* Held repeat stops at an edge; only a fresh press can wrap. */
static int menu_nav_cursor(int cursor, int rows, int step, int fresh)
{
    int next = cursor + step;
    if (next < 0) return fresh ? rows - 1 : 0;
    if (next >= rows) return fresh ? 0 : rows - 1;
    return next;
}

static void ui_title_label(int id, float y)
{
    const char *text, *p, *end;
    size_t len;
    int width = 0;
    if (!ui_label_text(id, g_language, &text, &len)) return;
    p = text; end = text + len;
    while (p < end) {
        uint32_t cp = utf8_next_cp(&p, end);
        const uint8_t *rec = font_find_glyph(cp);
        if (!rec) rec = font_find_glyph((uint32_t)'?');
        width += rec ? rec[7] : 10;
    }
    draw_utf8_wrapped(text, len, 130.0f - width * 0.75f / 2.0f, y, 234, 1);
}

/* Returns 1 = new game, 2 = loaded save. */
static int ui_title_scene(ProgressState *loaded)
{
    SceCtrlData pad;
    uint32_t prev = 0;
    int cursor = 0;
    int continue_ok = any_save_slots();
    MenuNav nav;
    void *title = NULL;
    int loaded_lang = -1;
    const int rows = 4;
    const uint32_t nav_mask = PSP_CTRL_UP | PSP_CTRL_DOWN;
    set_bgm_track(TITLE_BGM_PATH);

    /* Ignore directions already held when entering the title, then allow
     * fresh presses, direction changes and the original 24/6-frame repeat. */
    sceCtrlPeekBufferPositive(&pad, 1);
    prev = pad.Buttons;
    nav.held = pad.Buttons & nav_mask;
    nav.next = 0;

    while (1) {
        if (loaded_lang != g_language || !title) {
            if (title) free(title);
            title = load_exact_file(title_path_for_language(g_language), PIC_TEX_BYTES);
            loaded_lang = g_language;
        }

        sceCtrlPeekBufferPositive(&pad, 1);
        {
            uint32_t pressed = pad.Buttons & ~prev;

            int step = menu_nav_step(&nav,pad.Buttons,sceKernelGetSystemTimeWide());
            if(step) {
                int next = menu_nav_cursor(cursor, rows, step, nav.fresh);
                if (next != cursor) {
                    cursor = next;
                    play_se_async(SE_UI_CURSOR_PATH);
                }
            }

            if (pressed & ui_ok_mask()) {
                if (cursor == 0) {
                    play_se_async(SE_UI_OK_PATH);
                    if (title) free(title);
                    set_bgm_track(BGM_PATH);
                    return 1;
                }
                if (cursor == 1) {
                    if (!continue_ok) {
                        /* Original-style disabled command feedback: the row
                         * stays grey and X produces only the buzzer SE. */
                        play_se_async(SE_UI_BUZZER_PATH);
                        goto title_draw;
                    }
                    if (ui_save_load_scene(0, 0, 0, 0, 0, loaded)) {
                        if (title) free(title);
                        set_bgm_track(BGM_PATH);
                        return 2;
                    }
                    continue_ok = any_save_slots();
                    sceCtrlPeekBufferPositive(&pad, 1);
                    prev = pad.Buttons;
                    nav.held = pad.Buttons & nav_mask;
                    nav.next = 0;
                    continue;
                }
                if (cursor == 2) {
                    play_se_async(SE_UI_OK_PATH);
                    ui_options_scene();
                    sceCtrlPeekBufferPositive(&pad, 1);
                    prev = pad.Buttons;
                    nav.held = pad.Buttons & nav_mask;
                    nav.next = 0;
                    continue;
                }
                if (cursor == 3) {
                    play_se_async(SE_UI_OK_PATH);
                    if (title) free(title);
                    /* Let main own the actual process shutdown.  0.6.4 called
                     * sceKernelExitGame() here, then fell through as if this
                     * were a loaded save when the emulator returned from the
                     * syscall. */
                    return 0;
                }
            }
        }
title_draw:
        prev = pad.Buttons;
        ui_frame_begin();
        if (title)
            draw_texture_alpha(title, 512, 512, 480, 272, 0, 0, 480, 272, 255);

        /* Closer to the original PC title: compact four-row menu at lower
         * left, while keeping the PSP font readable at native 480x272. */
        /* Black outer/inner edges and a thin red frame, as in Window.png.
         * Keep the current readable PSP dimensions. */
        draw_solid_rect(18, 145, 224, 123, 0, 0, 0, 255);
        draw_solid_rect(19, 146, 222, 121, 190, 0, 0, 255);
        draw_solid_rect(21, 148, 218, 117, 0, 0, 0, 255);
        draw_solid_rect(22, 149, 216, 115, 28, 28, 28, 235);
        {
            int row;
            for (row = 0; row < rows; ++row)
                draw_solid_rect(26, 151 + row * 27, 208, 25,
                                row == cursor ? 65 : 0, 0, 0,
                                row == cursor ? 225 : 100);
        }
        ui_title_label(UI_L_NEW_GAME, 151);
        ui_title_label(UI_L_CONTINUE, 178);
        ui_title_label(UI_L_SETTINGS, 205);
        ui_title_label(UI_L_SHUTDOWN, 232);
        if (!continue_ok)
            draw_solid_rect(28, 180, 204, 23, 0, 0, 0, 120);
        ui_frame_end();

    }
}
