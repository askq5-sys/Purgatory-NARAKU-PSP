/* Switches, variables, items and save files.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Progress / switches                                                       */
/* ------------------------------------------------------------------------- */

static void reset_runtime_game_state(void)
{
    g_player_party_mask=1;
    int i;
    g_save_enabled = 1;
    memset(g_switches, 0, sizeof(g_switches));
    memset(g_variables, 0, sizeof(g_variables));
    memset(g_items, 0, sizeof(g_items));
    memset(g_self_switches, 0, sizeof(g_self_switches));
    clear_event_routes();
    g_balloon_frames = 0;
    clear_stage_parallel();
    g_stage_parallax_y=0;g_stage_parallax_scrolling=0;
    memset(g_event_shift_x, 0, sizeof(g_event_shift_x));
    memset(g_event_shift_y, 0, sizeof(g_event_shift_y));
    memset(g_event_move_speed, 0, sizeof(g_event_move_speed));
    for (i = 0; i < MAX_EVENT_ID; ++i) g_event_active_page[i] = -1;
    for (i = 0; i < MAX_ACTIVE_PICTURES; ++i) {
        if (g_pictures[i].texture) free(g_pictures[i].texture);
        memset(&g_pictures[i], 0, sizeof(g_pictures[i]));
    }
    g_picture6_visible = 0;
    g_picture6_alpha = 0;
    g_player_move_speed_code = 3;
    g_player_direction_fix = 0;
    g_player_walk_anime=1;g_player_step_anime=0;
    g_player_through = 0;
    g_player_visible = 1;
    g_camera_scroll_x = 0.0f;
    g_camera_scroll_y = 0.0f;
    g_map_scroll_remaining = 0;
    g_screen_shake_x = 0.0f;
    g_fog_scroll = 0.0f;
    g_fog_active_prev = 0;
    g_map8_drop_timer = 0;
    g_map9_water_timer = 0;
    g_world_render_frames = 0;
    g_script_fade_alpha=g_number_active=0;
    player_animation_reset(&g_player_animation);
    g_player_animation_moving = g_player_animation_jumping = 0;
    g_player_animation_speed = 3;
    g_playtime_frames = 0;
    g_world_tone_r = 0;
    g_world_tone_g = 0;
    g_world_tone_b = 0;
    g_world_tone_gray = 0;
    g_world_tone_from_r = 0;
    g_world_tone_from_g = 0;
    g_world_tone_from_b = 0;
    g_world_tone_from_gray = 0;
    g_world_tone_target_r = 0;
    g_world_tone_target_g = 0;
    g_world_tone_target_b = 0;
    g_world_tone_target_gray = 0;
    g_world_tone_total = 0;
    g_world_tone_remaining = 0;
    g_screen_flash_r = 0;
    g_screen_flash_g = 0;
    g_screen_flash_b = 0;
    g_screen_flash_a = 0;
    g_screen_flash_total = 0;
    g_screen_flash_remaining = 0;
    g_last_choice = -1;
    g_request_title = 0;
    g_loaded_from_scene = 0;
}

static void clear_progress(void)
{
    remove(PROGRESS_PATH);
    reset_runtime_game_state();
}

static int read_progress_file(const char *path, ProgressState *out)
{
    FILE *f;
    uint8_t header[16];
    size_t n;

    memset(out, 0, sizeof(*out));
    reset_runtime_game_state();

    f = fopen(path, "rb");
    if (!f) return 0;

    n = fread(header, 1, 4, f);
    if (n != 4) {
        fclose(f);
        return 0;
    }

    if (memcmp(header, "PG60", 4) == 0 ||
        memcmp(header, "PG40", 4) == 0 || memcmp(header, "PG30", 4) == 0) {
        if (fread(header + 4, 1, 12, f) != 12) {
            fclose(f);
            return 0;
        }

        out->map_id = (int)read_u16_le(header + 4);
        out->tile_x = (int)read_u16_le(header + 6);
        out->tile_y = (int)read_u16_le(header + 8);
        out->direction_row = (int)header[10];
        out->intro_done = (int)header[11];

        if (memcmp(header, "PG60", 4) == 0 || memcmp(header, "PG40", 4) == 0) {
            g_picture6_visible = header[12] ? 1 : 0;
            g_picture6_alpha = g_picture6_visible ? (header[13] ? (int)header[13] : 255) : 0;
            if ((header[14]&7) >= 1 && (header[14]&7) <= 6)
                g_player_move_speed_code = (int)(header[14]&7);
            if (memcmp(header, "PG60", 4) == 0)
                g_player_visible = (header[15] & 1) ? 1 : 0;
            if (memcmp(header, "PG60", 4) == 0)
                { player_form_restore_bits(header[15]);
                  if(header[14]&192) g_player_party_mask=(uint8_t)(((header[15]>>2)&63)|(header[14]&192)); }
            if (memcmp(header, "PG60", 4) == 0)
                g_save_enabled = (header[15] & 2) ? 0 : 1;
        } else {
            g_picture6_visible = out->intro_done ? 1 : 0;
            g_picture6_alpha = g_picture6_visible ? 255 : 0;
        }

        if (fread(g_switches, 1, sizeof(g_switches), f) != sizeof(g_switches) ||
            fread(g_variables, 1, sizeof(g_variables), f) != sizeof(g_variables) ||
            fread(g_items, 1, sizeof(g_items), f) != sizeof(g_items) ||
            fread(g_self_switches, 1, sizeof(g_self_switches), f) != sizeof(g_self_switches)) {
            fclose(f);
            reset_runtime_game_state();
            return 0;
        }
        {
            uint8_t pt[4];
            if (fread(pt, 1, 4, f) == 4)
                g_playtime_frames = read_u32_le(pt) * 60u;
        }
        fclose(f);
    } else if (memcmp(header, "PG10", 4) == 0) {
        uint8_t old[136];
        if (fread(old, 1, sizeof(old), f) != sizeof(old)) {
            fclose(f);
            return 0;
        }
        fclose(f);
        out->map_id = (int)read_u16_le(old + 0);
        out->tile_x = (int)read_u16_le(old + 2);
        out->tile_y = (int)read_u16_le(old + 4);
        out->direction_row = (int)old[6];
        out->intro_done = (int)old[7];
        g_picture6_visible = out->intro_done ? 1 : 0;
        g_picture6_alpha = g_picture6_visible ? 255 : 0;
        memcpy(g_switches, old + 8, 128);
    } else {
        fclose(f);
        return 0;
    }

    if (out->map_id < 1 || out->map_id > MAX_MAP_ID) {
        reset_runtime_game_state();
        return 0;
    }
    reconcile_consumed_items();
    if (out->direction_row < 0 || out->direction_row > 3) out->direction_row = 0;
    return 1;
}

static int write_progress_file(
    const char *path,
    int map_id, int tile_x, int tile_y, int direction_row, int intro_done)
{
    FILE *f;
    uint8_t header[16];

    memset(header, 0, sizeof(header));
    memcpy(header, "PG60", 4);
    write_u16_le(header + 4, (uint16_t)map_id);
    write_u16_le(header + 6, (uint16_t)tile_x);
    write_u16_le(header + 8, (uint16_t)tile_y);
    header[10] = (uint8_t)direction_row;
    header[11] = (uint8_t)(intro_done ? 1 : 0);
    header[12] = (uint8_t)(g_picture6_visible ? 1 : 0);
    header[13] = (uint8_t)(g_picture6_visible ? g_picture6_alpha : 0);
    header[14] = (uint8_t)(clamp_move_speed_code(g_player_move_speed_code) | (g_player_party_mask&192));
    header[15] = (uint8_t)((g_player_visible ? 1 : 0) | (g_save_enabled ? 0 : 2) | player_form_save_bits());

    f = fopen(path, "wb");
    if (!f) return 0;
    if (fwrite(header, 1, sizeof(header), f) != sizeof(header) ||
        fwrite(g_switches, 1, sizeof(g_switches), f) != sizeof(g_switches) ||
        fwrite(g_variables, 1, sizeof(g_variables), f) != sizeof(g_variables) ||
        fwrite(g_items, 1, sizeof(g_items), f) != sizeof(g_items) ||
        fwrite(g_self_switches, 1, sizeof(g_self_switches), f) != sizeof(g_self_switches)) {
        fclose(f);
        return 0;
    }
    {
        uint32_t seconds = g_playtime_frames / 60u;
        uint8_t pt[4];
        pt[0] = (uint8_t)(seconds & 0xFFu);
        pt[1] = (uint8_t)((seconds >> 8) & 0xFFu);
        pt[2] = (uint8_t)((seconds >> 16) & 0xFFu);
        pt[3] = (uint8_t)((seconds >> 24) & 0xFFu);
        if (fwrite(pt, 1, 4, f) != 4) { fclose(f); return 0; }
    }
    fclose(f);
    return 1;
}

static int load_progress(ProgressState *out)
{
    return read_progress_file(PROGRESS_PATH, out);
}

static void save_progress(
    int map_id, int tile_x, int tile_y, int direction_row, int intro_done)
{
    write_progress_file(PROGRESS_PATH, map_id, tile_x, tile_y, direction_row, intro_done);
}

static void save_slot_path(int slot, char *path, size_t path_size)
{
    if (slot < 1) slot = 1;
    if (slot > SAVE_SLOT_COUNT) slot = SAVE_SLOT_COUNT;
    snprintf(path, path_size, ROOT "save%02d.bin", slot);
}

static int save_slot_exists(int slot)
{
    char path[192];
    FILE *f;
    save_slot_path(slot, path, sizeof(path));
    f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

static int any_save_slots(void)
{
    int i;
    for (i = 1; i <= SAVE_SLOT_COUNT; ++i)
        if (save_slot_exists(i)) return 1;
    return 0;
}

static int save_to_slot(
    int slot, int map_id, int tile_x, int tile_y, int direction_row)
{
    char path[192];
    save_slot_path(slot, path, sizeof(path));
    return write_progress_file(path, map_id, tile_x, tile_y, direction_row, 1);
}

static int load_from_slot(int slot, ProgressState *out)
{
    char path[192];
    save_slot_path(slot, path, sizeof(path));
    return read_progress_file(path, out);
}

static int save_slot_metadata(
    int slot, int *map_id, int *tile_x, int *tile_y, uint32_t *playtime_seconds, int *actor)
{
    char path[192];
    FILE *f;
    uint8_t h[16];
    uint8_t pt[4];
    long playtime_offset = 16L + (long)sizeof(g_switches) +
                           (long)sizeof(g_variables) + (long)sizeof(g_items) +
                           (long)sizeof(g_self_switches);
    save_slot_path(slot, path, sizeof(path));
    f = fopen(path, "rb");
    if (!f) return 0;
    if (fread(h, 1, sizeof(h), f) != sizeof(h)) {
        fclose(f); return 0;
    }
    if (memcmp(h, "PG60", 4) != 0 && memcmp(h, "PG40", 4) != 0 &&
        memcmp(h, "PG30", 4) != 0) { fclose(f); return 0; }
    if(actor)*actor=memcmp(h,"PG60",4)==0 ? player_actor_from_mask(((h[15]>>2)&63) | (h[14]&192)) : 1;
    if (map_id) *map_id = (int)read_u16_le(h + 4);
    if (tile_x) *tile_x = (int)read_u16_le(h + 6);
    if (tile_y) *tile_y = (int)read_u16_le(h + 8);
    if (playtime_seconds) {
        *playtime_seconds = 0;
        if (fseek(f, playtime_offset, SEEK_SET) == 0 && fread(pt, 1, 4, f) == 4)
            *playtime_seconds = read_u32_le(pt);
    }
    fclose(f);
    return 1;
}
