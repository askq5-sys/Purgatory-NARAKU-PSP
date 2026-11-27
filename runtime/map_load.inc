/* Map, event and atlas loading.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Map loading                                                               */
/* ------------------------------------------------------------------------- */

static void free_map(MapState *m)
{
    int i;
    if (m->map_bin) free(m->map_bin);
    for (i = 0; i < MAX_TILE_ATLASES; ++i) {
        if (m->tile_atlases[i]) free(m->tile_atlases[i]);
    }
    if (m->events_bin) free(m->events_bin);
    if (m->event_atlas) free(m->event_atlas);
    if (m->event_atlas_detail) free(m->event_atlas_detail);
    if (m->vm_bin) free(m->vm_bin);
    free(m->parallax_texture);
    memset(m, 0, sizeof(*m));
}

static int load_map(MapState *m, int map_id)
{
    char path[192];
    size_t cells;
    size_t expected_map;
    size_t min_events;
    size_t offset;
    int i;

    free_map(m);
    clear_event_routes();
    g_balloon_frames = 0;
    clear_stage_parallel();
    g_stage_parallax_y=0;g_stage_parallax_scrolling=(map_id==54 || map_id==59 || (map_id==75 && !g_switches[763]));
    memset(g_event_shift_x, 0, sizeof(g_event_shift_x));
    memset(g_event_shift_y, 0, sizeof(g_event_shift_y));
    memset(g_event_move_speed, 0, sizeof(g_event_move_speed));
    for (i = 0; i < MAX_EVENT_ID; ++i) g_event_active_page[i] = -1;
    g_camera_scroll_x = 0.0f;
    g_camera_scroll_y = 0.0f;
    g_map_scroll_remaining = 0;
    g_screen_shake_x = 0.0f;

    if (map_id < 1 || map_id > MAX_MAP_ID) return 0;

    snprintf(path, sizeof(path), ASSET_ROOT "map%03d.bin", map_id);
    m->map_bin = (uint8_t *)load_whole_file(path, &m->map_size);
    if (!m->map_bin || m->map_size < MAP_HEADER_BYTES) goto fail;
    if (memcmp(m->map_bin, "NM40", 4) != 0) goto fail;

    m->w = (int)read_u16_le(m->map_bin + 4);
    m->h = (int)read_u16_le(m->map_bin + 6);
    m->tile_px = (int)read_u16_le(m->map_bin + 8);
    m->tile_count = (int)read_u16_le(m->map_bin + 14);
    m->tile_atlas_count = (m->tile_count + 99) / 100;
    if (m->tile_atlas_count < 1) m->tile_atlas_count = 1;
    if (m->w <= 0 || m->h <= 0 || m->tile_px != TILE_PX) goto fail;
    if (m->tile_atlas_count > MAX_TILE_ATLASES) goto fail;

    cells = (size_t)m->w * (size_t)m->h;
    expected_map = MAP_HEADER_BYTES + cells * 4u * 2u + cells * 2u;
    if (read_u16_le(m->map_bin + 16) & 1) expected_map += cells;
    if (m->map_size != expected_map) goto fail;

    {
        int atlas_index;
        for (atlas_index = 0; atlas_index < m->tile_atlas_count; ++atlas_index) {
            snprintf(
                path, sizeof(path),
                ASSET_ROOT "map%03d_atlas%d.rgba8888",
                map_id, atlas_index
            );
            m->tile_atlases[atlas_index] = load_exact_file(path, TILE_ATLAS_BYTES);
            if (!m->tile_atlases[atlas_index]) goto fail;
        }
    }

    snprintf(path, sizeof(path), ASSET_ROOT "map%03d_events.bin", map_id);
    m->events_bin = (uint8_t *)load_whole_file(path, &m->events_size);
    if (!m->events_bin || m->events_size < EVENT_HEADER_BYTES) goto fail;
    if (memcmp(m->events_bin, "NE10", 4) != 0) goto fail;
    if ((int)read_u16_le(m->events_bin + 4) != m->w ||
        (int)read_u16_le(m->events_bin + 6) != m->h) goto fail;

    m->transfer_count = (int)read_u16_le(m->events_bin + 8);
    m->sprite_count = (int)read_u16_le(m->events_bin + 10);

    min_events = EVENT_HEADER_BYTES + cells * 2u + cells * 3u +
                 (size_t)m->transfer_count * TRANSFER_RECORD_BYTES +
                 (size_t)m->sprite_count * SPRITE_RECORD_BYTES;
    if (m->events_size != min_events) goto fail;

    offset = EVENT_HEADER_BYTES;
    m->action_map = m->events_bin + offset;
    offset += cells * 2u;
    m->step_map = m->events_bin + offset;
    offset += cells;
    m->transfer_map = m->events_bin + offset;
    offset += cells;
    m->block_map = m->events_bin + offset;
    offset += cells;
    m->transfer_records = m->events_bin + offset;
    offset += (size_t)m->transfer_count * TRANSFER_RECORD_BYTES;
    m->sprite_records = m->events_bin + offset;

    if (m->sprite_count > 0) {
        snprintf(path, sizeof(path), ASSET_ROOT "map%03d_event_atlas.rgba8888", map_id);
        m->event_atlas = load_exact_file(path, EVENT_ATLAS_BYTES);
        if (!m->event_atlas) goto fail;
        int need_detail=map_id==32 || map_id==39 || map_id==40 || map_id==47;
        {
            for (i=0;i<m->sprite_count;++i)
                if (read_u16_le(m->sprite_records+(size_t)i*SPRITE_RECORD_BYTES+4)>>12) need_detail=1;
        }
        if (need_detail) {
            snprintf(path,sizeof(path),ASSET_ROOT "map%03d_event_atlas1.rgba8888",map_id);
            m->event_atlas_detail=load_exact_file(path,EVENT_ATLAS_BYTES);
            if(!m->event_atlas_detail)goto fail;
        }
    }

    if (map_id==54 || map_id==59 || map_id==61 || map_id==75) {
        m->parallax_texture=load_exact_file(ASSET_ROOT "parallax_chain.rgba8888",EVENT_ATLAS_BYTES);
        if (!m->parallax_texture) goto fail;
    }

    snprintf(path, sizeof(path), ASSET_ROOT "map%03d_vm.bin", map_id);
    m->vm_bin = (uint8_t *)load_whole_file(path, &m->vm_size);
    if (m->vm_bin) {
        uint32_t cmd_off;
        uint32_t cmd_size;
        size_t need;
        if (m->vm_size < VM_HEADER_BYTES || memcmp(m->vm_bin, "NV40", 4) != 0) goto fail;
        m->vm_event_count = (int)read_u16_le(m->vm_bin + 4);
        m->vm_page_count = (int)read_u16_le(m->vm_bin + 6);
        cmd_off = read_u32_le(m->vm_bin + 8);
        cmd_size = read_u32_le(m->vm_bin + 12);
        m->vm_flags = read_u32_le(m->vm_bin + 16);
        need = VM_HEADER_BYTES + (size_t)m->vm_event_count * VM_EVENT_BYTES +
               (size_t)m->vm_page_count * VM_PAGE_BYTES;
        if (cmd_off != need || (size_t)cmd_off + (size_t)cmd_size > m->vm_size) goto fail;
        m->vm_events = m->vm_bin + VM_HEADER_BYTES;
        m->vm_pages = m->vm_events + (size_t)m->vm_event_count * VM_EVENT_BYTES;
        m->vm_commands = m->vm_bin + cmd_off;
        m->vm_command_size = (size_t)cmd_size;
    }

    m->id = map_id;
    return 1;

fail:
    free_map(m);
    return 0;
}

static uint16_t map_word(const MapState *m, int z, int x, int y)
{
    int cell = (z * m->h + y) * m->w + x;
    return read_u16_le(m->map_bin + MAP_HEADER_BYTES + cell * 2);
}

static uint8_t event_tile_pass_mask(const MapState *m,int x,int y,uint8_t fallback);

static uint8_t pass_mask_at(const MapState *m, int x, int y)
{
    size_t cells;
    size_t pass_offset;
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    cells = (size_t)m->w * (size_t)m->h;
    pass_offset = MAP_HEADER_BYTES + cells * 4u * 2u;
    uint8_t mask=m->map_bin[pass_offset + (size_t)y * m->w + x];
    return (m->vm_flags&2) ? event_tile_pass_mask(m,x,y,mask) : mask;
}

/* Original tileset counter flag permits one action check beyond a counter. */
static int counter_at(const MapState *m, int x, int y)
{
    size_t cells;
    if (!m->map_bin || x < 0 || y < 0 || x >= m->w || y >= m->h ||
        !(read_u16_le(m->map_bin + 16) & 1)) return 0;
    cells = (size_t)m->w * m->h;
    return m->map_bin[MAP_HEADER_BYTES + cells * 10u + (size_t)y * m->w + x] != 0;
}

static uint16_t action_event_at(const MapState *m, int x, int y)
{
    size_t cell;
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    cell = (size_t)y * m->w + x;
    return read_u16_le(m->action_map + cell * 2u);
}

static uint8_t transfer_index_at(const MapState *m, int x, int y)
{
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 0;
    return m->transfer_map[(size_t)y * m->w + x];
}

static uint8_t event_block_at(const MapState *m, int x, int y)
{
    if (x < 0 || y < 0 || x >= m->w || y >= m->h) return 1;
    return m->block_map[(size_t)y * m->w + x];
}

static int read_transfer_record(const MapState *m, int index, TransferRecord *out)
{
    const uint8_t *p;
    if (index <= 0 || index > m->transfer_count) return 0;
    p = m->transfer_records + (size_t)(index - 1) * TRANSFER_RECORD_BYTES;

    out->dest_map = (int)read_u16_le(p + 0);
    out->dest_x = (int)read_u16_le(p + 2);
    out->dest_y = (int)read_u16_le(p + 4);
    out->direction = (int)p[6];
    out->flags = (int)p[7];

    out->switch_id[0] = (int)read_u16_le(p + 8);
    out->switch_value[0] = (int)p[10];
    out->switch_id[1] = (int)read_u16_le(p + 12);
    out->switch_value[1] = (int)p[14];
    out->switch_id[2] = (int)read_u16_le(p + 16);
    out->switch_value[2] = (int)p[18];
    return 1;
}

static int read_sprite_record(const MapState *m, int index, EventSpriteRecord *out)
{
    const uint8_t *p;
    if (index < 0 || index >= m->sprite_count) return 0;
    p = m->sprite_records + (size_t)index * SPRITE_RECORD_BYTES;

    out->x = (int)read_u16_le(p + 0);
    out->y = (int)read_u16_le(p + 2);
    out->sx = (int)read_u16_le(p + 4);
    out->sy = (int)read_u16_le(p + 6);
    out->sw = (int)read_u16_le(p + 8);
    out->sh = (int)read_u16_le(p + 10);
    out->priority = (int)p[12];
    out->flags = (int)p[13];
    out->event_id = (int)read_u16_le(p + 14);
    return 1;
}
