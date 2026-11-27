/* Script VM: commands, move routes, transfers.
 * Part of main.c: included there in order, not a standalone unit. */

static void fade_game_world(
    const MapState *m, void *char_atlas,
    float player_x, float player_y,
    int player_pattern, int player_dir,
    int from, int to, int frames);

static int vm_move_player_one(
    MapState *m, void *char_atlas,
    int *tile_x, int *tile_y, int *direction_row,
    int dx, int dy, int speed_code, int through, int keep_direction)
{
    int nx = *tile_x + dx;
    int ny = *tile_y + dy;
    int bit = 0;
    int reverse_bit = 0;
    int frames;
    int i;
    float step_px;
    float start_x;
    float start_y;

    if (dx == 0 && dy > 0) { bit = 0x01; reverse_bit = 0x08; if (!keep_direction && !g_player_direction_fix) *direction_row = 0; }
    else if (dx < 0 && dy == 0) { bit = 0x02; reverse_bit = 0x04; if (!keep_direction && !g_player_direction_fix) *direction_row = 1; }
    else if (dx > 0 && dy == 0) { bit = 0x04; reverse_bit = 0x02; if (!keep_direction && !g_player_direction_fix) *direction_row = 2; }
    else if (dx == 0 && dy < 0) { bit = 0x08; reverse_bit = 0x01; if (!keep_direction && !g_player_direction_fix) *direction_row = 3; }
    else return 0;

    if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h) return 0;
    if (!through) {
        uint8_t a = pass_mask_at(m, *tile_x, *tile_y);
        uint8_t b = pass_mask_at(m, nx, ny);
        if (!(a & bit) || !(b & reverse_bit) || vm_event_blocks_at(m, nx, ny)) return 0;
    }

    speed_code = clamp_move_speed_code(speed_code);
    step_px = move_speed_px_per_frame(speed_code);
    frames = (int)((float)TILE_PX / step_px + 0.999f);
    if (frames < 1) frames = 1;

    start_x = ((float)*tile_x + 0.5f) * TILE_PX;
    start_y = ((float)*tile_y + 1.0f) * TILE_PX;
    for (i = 1; i <= frames; ++i) {
        float t = (float)i / (float)frames;
        float wx = start_x + (float)dx * TILE_PX * t;
        float wy = start_y + (float)dy * TILE_PX * t;
        g_player_animation_moving = i < frames;
        g_player_animation_jumping = i == frames; /* arrival: stop count is still zero */
        g_player_animation_speed = speed_code;
        render_world(
            m, char_atlas,
            g_player_visible, wx, wy, 1, *direction_row, 0, 0);
    }
    *tile_x = nx;
    *tile_y = ny;
    return 1;
}

static void vm_execute_move_route(
    MapState *m, void *char_atlas,
    int *tile_x, int *tile_y, int *direction_row,
    const uint8_t *records, int count, int target_id)
{
    int speed_code = 3;
    int i;
    if (target_id >= 0 && target_id < MAX_EVENT_ID) {
        VmEventRecord target_ev;
        VmPageRecord target_pg;
        int active_page_index = -1;
        int ei;

        /* RPG Maker resets an event's move speed when its active page changes,
         * then Change Speed inside a move route persists until another page
         * refresh.  This matters twice immediately: Map001's falling Enri
         * changes page speeds 6 -> 5 -> 4 -> 2, while Map004's hatch grate
         * starts at 6 and its first route deliberately changes it to 2 before
         * the second route moves it back. */
        for (ei = 0; m && ei < m->vm_event_count; ++ei) {
            if (!vm_read_event(m, ei, &target_ev)) continue;
            if (target_ev.event_id != target_id) continue;
            if (vm_active_page_index(m, &target_ev, &target_pg, &active_page_index)) {
                int page_speed = (target_pg.flags >> 2) & 0x07;
                if (page_speed < 1 || page_speed > 6) page_speed = 3;
                if (g_event_active_page[target_id] != active_page_index) {
                    g_event_active_page[target_id] = (int16_t)active_page_index;
                    g_event_move_speed[target_id] = (int8_t)page_speed;
                }
            }
            break;
        }
        if (g_event_move_speed[target_id] >= 1 && g_event_move_speed[target_id] <= 6)
            speed_code = g_event_move_speed[target_id];

        /* NPC/event routes.  Positions stay in tile units, with interpolation
           during synchronous Wait move-route commands. */
        for (i = 0; i < count; ++i) {
            const uint8_t *r = records + (size_t)i * 5u;
            int code = r[0];
            int n = (int16_t)read_u16_le(r + 1);
            int dx = 0, dy = 0, f, frames;
            float old_x, old_y;
            if (code == 29) {
                speed_code = clamp_move_speed_code(n);
                g_event_move_speed[target_id] = (int8_t)speed_code;
                continue;
            }
            if (code == 15) {
                vm_wait_frames(n,m,char_atlas,*tile_x,*tile_y,*direction_row);
                continue;
            }
            if (code == 44) {
                int packed = (int16_t)read_u16_le(r + 3);
                int volume = (packed >> 8) & 0xFF;
                int pitch = packed & 0xFF;
                if (volume <= 0) volume = 90;
                if (pitch <= 0) pitch = 100;
                play_vm_se_params(n, volume, pitch, 0);
                continue;
            }
            if (code == 1) dy = 1;
            else if (code == 2) dx = -1;
            else if (code == 3) dx = 1;
            else if (code == 4) dy = -1;
            else continue;
            if (speed_code < 1) speed_code = 1;
            if (speed_code > 6) speed_code = 6;
            old_x = g_event_shift_x[target_id];
            old_y = g_event_shift_y[target_id];
            frames = 256/(1 << speed_code);
            if (frames < 1) frames = 1;
            for (f = 1; f <= frames; ++f) {
                float t = (float)f/(float)frames;
                g_event_shift_x[target_id] = old_x + dx*t;
                g_event_shift_y[target_id] = old_y + dy*t;
                vm_render_frame(m,char_atlas,*tile_x,*tile_y,*direction_row,0,0);
            }
        }
        return;
    }
    /* Player move routes begin at the character's current persistent speed,
     * just like RPG Maker; Change Speed (route code 29) survives the route.
     * Preserve animation phase across successive routes. */
    speed_code = clamp_move_speed_code(g_player_move_speed_code);
    g_player_animation_speed = speed_code;
    for (i = 0; i < count; ++i) {
        const uint8_t *r = records + (size_t)i * 5u;
        int code = (int)r[0];
        int p1 = (int16_t)read_u16_le(r + 1);
        int p2 = (int16_t)read_u16_le(r + 3);
        int dx = 0, dy = 0;
        if (code == 1) { dy = 1; vm_move_player_one(m,char_atlas,tile_x,tile_y,direction_row,dx,dy,speed_code,g_player_through,0); }
        else if (code == 2) { dx = -1; vm_move_player_one(m,char_atlas,tile_x,tile_y,direction_row,dx,dy,speed_code,g_player_through,0); }
        else if (code == 3) { dx = 1; vm_move_player_one(m,char_atlas,tile_x,tile_y,direction_row,dx,dy,speed_code,g_player_through,0); }
        else if (code == 4) { dy = -1; vm_move_player_one(m,char_atlas,tile_x,tile_y,direction_row,dx,dy,speed_code,g_player_through,0); }
        else if (code == 12 || code == 13) {
            player_route_delta(code, *direction_row, &dx, &dy);
            vm_move_player_one(m,char_atlas,tile_x,tile_y,direction_row,dx,dy,speed_code,g_player_through,1);
        } else if (code == 14) {
            /* Match Game_CharacterBase::jump/updateJump from RPG Maker MZ.
             * The logical destination is reached regardless of tile collision;
             * the sprite follows the same parabolic jumpHeight curve, scaled
             * 1:2 together with the rest of NARAKU's 48px -> PSP 24px world. */
            int tx = *tile_x + p1;
            int ty = *tile_y + p2;
            int f;
            int sq = p1 * p1 + p2 * p2;
            int distance = 0;
            int jump_peak;
            int jump_count;
            float sx = ((float)*tile_x + 0.5f) * TILE_PX;
            float sy = ((float)*tile_y + 1.0f) * TILE_PX;
            if (!g_player_direction_fix && abs(p1) > abs(p2)) {
                if (p1 < 0) *direction_row = 1;
                else if (p1 > 0) *direction_row = 2;
            } else if (!g_player_direction_fix && p2 != 0) {
                if (p2 < 0) *direction_row = 3;
                else *direction_row = 0;
            }
            while ((distance + 1) * (distance + 1) <= sq) distance++;
            if (distance * distance != sq) {
                int lo = sq - distance * distance;
                int hi = (distance + 1) * (distance + 1) - sq;
                if (hi <= lo) distance++;
            }
            if (g_player_walk_anime || g_player_step_anime)
                player_animation_reset(&g_player_animation);
            jump_peak = 10 + distance - speed_code;
            if (jump_peak < 1) jump_peak = 1;
            jump_count = jump_peak * 2;
            if (tx >= 0 && ty >= 0 && tx < m->w && ty < m->h) {
                int old_camera_locked = g_camera_locked;
                float old_camera_x = g_camera_lock_x;
                float old_camera_y = g_camera_lock_y;
                for (f = 1; f <= jump_count; ++f) {
                    int remain = jump_count - f;
                    int d = abs(remain - jump_peak);
                    float jump_h = ((float)(jump_peak * jump_peak - d * d) * 0.5f) * 0.5f;
                    float t = (float)f / (float)jump_count;
                    float base_x = sx + p1 * TILE_PX * t;
                    float base_y = sy + p2 * TILE_PX * t;
                    float map_w = (float)(m->w * TILE_PX);
                    float map_h = (float)(m->h * TILE_PX);
                    float max_x = map_w > SCREEN_W ? map_w - SCREEN_W : 0.0f;
                    float max_y = map_h > SCREEN_H ? map_h - SCREEN_H : 0.0f;
                    /* Camera follows _realX/_realY, not the visual jumpHeight. */
                    g_camera_locked = 1;
                    g_camera_lock_x = max_x > 0.0f
                        ? clamp_float(base_x - SCREEN_W * 0.5f, 0.0f, max_x) : 0.0f;
                    g_camera_lock_y = max_y > 0.0f
                        ? clamp_float(base_y - SCREEN_H * 0.5f, 0.0f, max_y) : 0.0f;
                    g_player_animation_jumping = 1;
                    g_player_animation_moving = remain > 0 && (p1 || p2);
                    render_world(m,char_atlas,g_player_visible,
                        base_x, base_y - jump_h,
                        1,*direction_row,0,0);
                }
                g_camera_locked = old_camera_locked;
                g_camera_lock_x = old_camera_x;
                g_camera_lock_y = old_camera_y;
                *tile_x = tx; *tile_y = ty;
            }
        } else if (code == 15) {
            vm_wait_frames(p1, m, char_atlas, *tile_x, *tile_y, *direction_row);
        } else if (code == 16 && !g_player_direction_fix) *direction_row = 0;
        else if (code == 17 && !g_player_direction_fix) *direction_row = 1;
        else if (code == 18 && !g_player_direction_fix) *direction_row = 2;
        else if (code == 19 && !g_player_direction_fix) *direction_row = 3;
        else if (code == 31) g_player_walk_anime=1;
        else if (code == 32) g_player_walk_anime=0;
        else if (code == 33) g_player_step_anime=1;
        else if (code == 34) g_player_step_anime=0;
        else if (code == 35) g_player_direction_fix=1;
        else if (code == 36) g_player_direction_fix=0;
        else if (code == 29) {
            speed_code = clamp_move_speed_code(p1);
            g_player_move_speed_code = speed_code;
            g_player_animation_speed = speed_code;
        }
        else if (code == 44) {
            int volume = (p2 >> 8) & 0xFF;
            int pitch = p2 & 0xFF;
            if (volume <= 0) volume = 90;
            if (pitch <= 0) pitch = 100;
            play_vm_se_params(p1, volume, pitch, 0);
        }
        else if (code == 37) g_player_through = 1;
        else if (code == 38) g_player_through = 0;
        else if (code == 39) g_player_visible = 0;
        else if (code == 40) g_player_visible = 1;
        /* Animation and direction-fix state persist across routes. */
    }
}

static int vm_transfer_player(
    MapState *m, void *char_atlas,
    int *tile_x, int *tile_y, int *direction_row,
    int dest_map, int dest_x, int dest_y, int direction, int fade)
{
    float old_x = ((float)*tile_x + 0.5f) * TILE_PX;
    float old_y = ((float)*tile_y + 1.0f) * TILE_PX;
    int new_row;
    int frames = fade == 2 ? 0 : 24;
    int old_white=g_transfer_white;
    g_transfer_white=(fade==1);

    if (dest_map < 1 || dest_map > MAX_MAP_ID) {g_transfer_white=old_white;return 0;}

    if (frames > 0) {
        fade_game_world(
            m, char_atlas,
            old_x, old_y, g_player_visible, *direction_row,
            0, 255, frames
        );
    }

    if (!load_map(m, dest_map)) {g_transfer_white=old_white;return 0;}
    *tile_x = dest_x;
    *tile_y = dest_y;
    new_row = direction_to_row(direction);
    if (new_row >= 0) *direction_row = new_row;

    if (frames > 0) {
        float nx = ((float)*tile_x + 0.5f) * TILE_PX;
        float ny = ((float)*tile_y + 1.0f) * TILE_PX;
        fade_game_world(
            m, char_atlas,
            nx, ny, g_player_visible, *direction_row,
            255, 0, frames
        );
    }
    g_transfer_white=old_white;
    return 1;
}

static void load_common_vm(void)
{
    g_common_vm = (uint8_t *)load_whole_file(COMMON_VM_PATH, &g_common_vm_size);
    if (!g_common_vm || g_common_vm_size < 8 || memcmp(g_common_vm, "NC50", 4) != 0)
        fatal_error("Missing or invalid common_vm.bin");
    g_common_vm_count = (int)read_u16_le(g_common_vm + 4);
    if (g_common_vm_count > 256 || g_common_vm_size < 8u + (size_t)g_common_vm_count * 8u)
        fatal_error("Bad Common Event table");
}

static int vm_execute_page(
    MapState *m, const VmEventRecord *ev, const VmPageRecord *pg,
    void *char_atlas, int lang,
    int *tile_x, int *tile_y, int *direction_row);

static void vm_call_common(
    MapState *m, const VmEventRecord *ev, int common_id,
    void *char_atlas, int lang,
    int *tile_x, int *tile_y, int *direction_row)
{
    uint32_t offset, size;
    const uint8_t *original_cmds;
    size_t original_size;
    VmPageRecord page;
    if (!g_common_vm || common_id < 1 || common_id >= g_common_vm_count || g_common_depth >= 8)
        return;
    offset = read_u32_le(g_common_vm + 8 + common_id * 8);
    size = read_u32_le(g_common_vm + 8 + common_id * 8 + 4);
    if (!size || offset >= g_common_vm_size || size > g_common_vm_size - offset)
        return;
    memset(&page, 0, sizeof(page));
    page.supported = 1;
    page.cmd_offset = 0;
    page.cmd_size = size;
    original_cmds = m->vm_commands;
    original_size = m->vm_command_size;
    m->vm_commands = g_common_vm + offset;
    m->vm_command_size = size;
    ++g_common_depth;
    vm_execute_page(m, ev, &page, char_atlas, lang, tile_x, tile_y, direction_row);
    --g_common_depth;
    /* A map transfer during a Common Event can change the MapState. */
    if (m->vm_commands == g_common_vm + offset) {
        m->vm_commands = original_cmds;
        m->vm_command_size = original_size;
    }
}

static int vm_execute_page(
    MapState *m, const VmEventRecord *ev, const VmPageRecord *pg,
    void *char_atlas, int lang,
    int *tile_x, int *tile_y, int *direction_row)
{
    const uint8_t *p;
    const uint8_t *end;
    const uint8_t *base;
    uint8_t *cmd_copy;
    int source_map_id;
    int source_event_id;
    int did_transfer = 0;
    const MapState *previous_owner=g_vm_position_owner;
    int *previous_x=g_vm_tile_x,*previous_y=g_vm_tile_y,*previous_direction=g_vm_direction;

    if (!pg->supported || !m->vm_commands) return 0;
    if ((size_t)pg->cmd_offset + (size_t)pg->cmd_size > m->vm_command_size) return 0;

    /* RPG Maker's Game_Interpreter owns its command list independently of
     * Game_Map.  A Transfer Player can load another map, but the current
     * event keeps executing after the transfer.  Our map loader frees the
     * old map VM blob, so executing directly from m->vm_commands made that
     * impossible and could leave cutscenes half-finished (invisible/frozen
     * player, switches not restored, etc.).  Keep a private copy for the
     * lifetime of this interpreter invocation. */
    cmd_copy = (uint8_t *)malloc(pg->cmd_size ? (size_t)pg->cmd_size : 1u);
    if (!cmd_copy) return 0;
    if (pg->cmd_size)
        memcpy(cmd_copy, m->vm_commands + pg->cmd_offset, (size_t)pg->cmd_size);
    source_map_id = m->id;
    source_event_id = ev ? ev->event_id : 0;
    g_vm_position_owner=m;g_vm_tile_x=tile_x;g_vm_tile_y=tile_y;g_vm_direction=direction_row;
    reset_factory_checkpoint_tail(source_map_id,source_event_id,pg->trigger);
    p = cmd_copy;
    base = p;
    end = p + pg->cmd_size;

    while (p < end) {
        int op = *p++;
        if (op == VM_OP_END) break;

        if (op == VM_OP_TEXT || op == VM_OP_TEXT_TRANSPARENT || op==VM_OP_TEXT_STYLE) {
            g_runtime_message_background=op==VM_OP_TEXT_TRANSPARENT?2:0;
            g_runtime_message_position=2;
            if(op==VM_OP_TEXT_STYLE) {
                if(end-p<2)break;
                g_runtime_message_background=*p++;g_runtime_message_position=*p++;
            }
            g_runtime_message_transparent=g_runtime_message_background==2;
            uint16_t sl[4], ml[4];
            const char *speaker = NULL;
            const char *message = NULL;
            size_t speaker_len = 0;
            size_t message_len = 0;
            int i;
            const uint8_t *q;
            if (end - p < 16) break;
            for (i = 0; i < 4; ++i) {
                sl[i] = read_u16_le(p + i * 4 + 0);
                ml[i] = read_u16_le(p + i * 4 + 2);
            }
            q = p + 16;
            for (i = 0; i < 4; ++i) {
                if (q + sl[i] + ml[i] > end) goto vm_done;
                if (i == vm_text_language(lang)) {
                    speaker = (const char *)q;
                    speaker_len = sl[i];
                    message = (const char *)(q + sl[i]);
                    message_len = ml[i];
                }
                q += sl[i] + ml[i];
            }
            p = q;
            /* Read the next picture while the player reads this message. */
            picture_prefetch(vm_scan_next_picture(base, pg->cmd_size, (size_t)(p - base), 64));
            vm_wait_text(
                m, char_atlas, *tile_x, *tile_y, *direction_row,
                speaker ? speaker : "", speaker_len,
                message ? message : "", message_len,
                p < end && (*p == VM_OP_TEXT || *p == VM_OP_TEXT_TRANSPARENT || *p==VM_OP_TEXT_STYLE)
            );
            g_runtime_message_transparent=0;g_runtime_message_background=0;
        } else if (op == VM_OP_NUMBER_INPUT) {
            int id,digits;
            if(end-p<3)break;
            id=read_u16_le(p);digits=p[2];p+=3;
            if(id>0 && id<MAX_VARIABLES && digits>=1 && digits<=8)
                g_variables[id]=vm_wait_number_input(m,char_atlas,*tile_x,*tile_y,*direction_row,g_variables[id],digits);
        } else if (op == VM_OP_FADE_SCREEN) {
            int from,to,i;
            if(end-p<1)break;
            from=g_script_fade_alpha;to=*p++?255:0;
            for(i=1;i<=24;++i) {
                g_script_fade_alpha=from+(to-from)*i/24;
                vm_render_frame(m,char_atlas,*tile_x,*tile_y,*direction_row,0,0);
            }
        } else if (op == VM_OP_SWITCH) {
            int first, last, value, i;
            if (end - p < 5) break;
            first = (int)read_u16_le(p + 0);
            last = (int)read_u16_le(p + 2);
            value = (int)p[4];
            p += 5;
            if (first < 0) first = 0;
            if (last >= MAX_SWITCHES) last = MAX_SWITCHES - 1;
            for (i = first; i <= last; ++i) g_switches[i] = (uint8_t)value;
            reconcile_consumed_items();
        } else if (op == VM_OP_SELF_SWITCH) {
            int idx, value;
            if (end - p < 2) break;
            idx = (int)p[0]; value = (int)p[1]; p += 2;
            if (source_map_id >= 1 && source_map_id <= MAX_MAP_ID &&
                source_event_id >= 0 && source_event_id < MAX_EVENT_ID && idx >= 0 && idx < 4) {
                if (value) g_self_switches[source_map_id][source_event_id] |= (uint8_t)(1u << idx);
                else g_self_switches[source_map_id][source_event_id] &= (uint8_t)~(1u << idx);
            }
        } else if (op == VM_OP_SCROLL_TEXT) {
            int speed, no_fast, i, rows=1; uint16_t lens[4]; const uint8_t *q;
            const char *text=NULL; size_t len=0;
            if (end-p<10) break;
            speed=*p++; no_fast=*p++;
            for(i=0;i<4;++i) lens[i]=read_u16_le(p+i*2);
            q=p+8;
            for(i=0;i<4;++i) {
                if(q+lens[i]>end) goto vm_done;
                if(i==vm_text_language(lang)){text=(const char*)q;len=lens[i];}
                q+=lens[i];
            }
            p=q;
            if(g_language==4)russian_lookup(&text,&len);
            if(text) {
                /* Count the same word/glyph wraps used by draw_utf8_wrapped. */
                const char *c=text,*limit=text+len; float x=36; int in_word=0,color=0;
                while(c<limit) {
                    const char *before=c; uint32_t cp; const uint8_t *rec; float adv;
                    if(parse_text_color_escape(&c,limit,&color))continue;
                    cp=utf8_next_cp(&c,limit);
                    if(latin_word_cp(cp)&&!in_word){float w=latin_word_width(before,limit)*0.75f;if(w<=408&&x+w>444&&x>36){x=36;++rows;}}
                    in_word=latin_word_cp(cp);
                    if(cp=='\r')continue;
                    if(cp=='\n'){x=36;++rows;continue;}
                    rec=font_find_glyph(cp);adv=(rec?rec[7]:10)*0.75f;
                    if(x+adv>444&&x>36){x=36;++rows;}x+=adv;
                }
                g_scroll_text=text;g_scroll_text_len=len;
                for(g_scroll_text_y=SCREEN_H;g_scroll_text_y>-(rows*25.0f+25);) {
                    SceCtrlData pad;
                    vm_render_frame(m,char_atlas,*tile_x,*tile_y,*direction_row,0,0);
                    sceCtrlPeekBufferPositive(&pad,1);
                    g_scroll_text_y-=clamp_float(speed,1,8)*0.25f*(!no_fast&&(pad.Buttons&(PSP_CTRL_CROSS|PSP_CTRL_CIRCLE|PSP_CTRL_DOWN))?3:1);
                }
                g_scroll_text=NULL;g_scroll_text_len=0;
            }
        } else if (op == VM_OP_BALLOON) {
            int wait;
            if (end - p < 4) break;
            g_balloon_target = (int16_t)read_u16_le(p);
            if (!g_balloon_target) g_balloon_target = source_event_id;
            g_balloon_id = p[2]; wait = p[3]; p += 4;
            if (!g_balloon_atlas)
                g_balloon_atlas = load_exact_file(ASSET_ROOT "balloon_atlas.rgba8888", 512u*512u*4u);
            g_balloon_frames = g_balloon_atlas && g_balloon_id>=1 && g_balloon_id<=10 ? 76 : 0;
            if (wait) while (g_balloon_frames > 0)
                vm_render_frame(m,char_atlas,*tile_x,*tile_y,*direction_row,0,0);
        } else if (op == VM_OP_WAIT) {
            int frames;
            if (end - p < 2) break;
            frames = (int)read_u16_le(p); p += 2;
            /* Button-room arrival: two frames earlier, only its first wait. */
            if (source_map_id == 36 && source_event_id == 17 &&
                p - base == 6 && frames == 120) frames = 118;
            /* Map013 Event069 grants the axe, then idles for 60 frames before
             * its obtained-item message.  That full second was especially
             * noticeable in this tile-heavy room because every wait frame
             * redraws the world. Preserve a short pickup beat and the sound
             * cue while avoiding the unnecessary one-second pause. */
            if (source_map_id == 13 && source_event_id == 69 && frames == 60)
                frames = 12;
            vm_wait_frames(frames, m, char_atlas, *tile_x, *tile_y, *direction_row);
        } else if (op == VM_OP_TRANSFER) {
            int dest_map, dest_x, dest_y, direction, fade;
            if (end - p < 8) break;
            dest_map = (int)read_u16_le(p + 0);
            dest_x = (int)read_u16_le(p + 2);
            dest_y = (int)read_u16_le(p + 4);
            direction = (int)p[6];
            fade = (int)p[7];
            p += 8;
            if (vm_transfer_player(
                    m, char_atlas, tile_x, tile_y, direction_row,
                    dest_map, dest_x, dest_y, direction, fade)) {
                did_transfer = 1;
                /* Do not return here.  MZ waits for the transfer and then
                 * resumes the same Game_Interpreter command list. */
            }
        } else if (op == VM_OP_SE) {
            int se_id, volume, pitch, pan;
            if (end - p < 5) break;
            se_id = (int)read_u16_le(p + 0);
            volume = (int)p[2];
            pitch = (int)p[3];
            pan = (int)p[4] - 100;
            p += 5;
            play_vm_se_params(se_id, volume, pitch, pan);
        } else if (op == VM_OP_ITEM) {
            int item_id;
            int16_t delta;
            if (end - p < 4) break;
            item_id = (int)read_u16_le(p + 0);
            delta = (int16_t)read_u16_le(p + 2);
            p += 4;
            if (item_id >= 0 && item_id < MAX_ITEMS) {
                int v = (int)g_items[item_id] + (int)delta;
                if (v < 0) v = 0;
                if (v > 999) v = 999;
                g_items[item_id] = (int16_t)v;
            }
        } else if (op == VM_OP_TRANSPARENCY) {
            if (end - p < 1) break;
            /* RPG Maker MZ command211: setTransparent(params[0] === 0).
             * Parameter 0 means Transparency ON (player hidden), parameter 1
             * means Transparency OFF (player visible).  Earlier PSP builds
             * had this inverted, so Map004's autorun hid Enri exactly when
             * the original game restores her after the hatch transition. */
            g_player_visible = p[0] ? 1 : 0;
            p += 1;
        } else if (op == VM_OP_TINT) {
            int r, g, b, gray, duration, wait;
            if (end - p < 11) break;
            r = (int16_t)read_u16_le(p + 0);
            g = (int16_t)read_u16_le(p + 2);
            b = (int16_t)read_u16_le(p + 4);
            gray = (int16_t)read_u16_le(p + 6);
            duration = (int)read_u16_le(p + 8);
            wait = (int)p[10];
            p += 11;

            /* MZ keeps the complete screen tone after command 223.  0.6.4
             * reduced everything except full black to zero, which removed
             * NARAKU's persistent red veil [10,-10,-10,0]. */
            vm_begin_world_tint(r, g, b, gray, duration);
            if (wait && duration > 0)
                vm_wait_frames(duration, m, char_atlas,
                               *tile_x, *tile_y, *direction_row);
        } else if (op == VM_OP_FLASH) {
            int r, g, b, a, duration, wait;
            if (end - p < 11) break;
            r = (int16_t)read_u16_le(p + 0);
            g = (int16_t)read_u16_le(p + 2);
            b = (int16_t)read_u16_le(p + 4);
            a = (int16_t)read_u16_le(p + 6);
            duration = (int)read_u16_le(p + 8);
            wait = (int)p[10];
            p += 11;
            vm_begin_screen_flash(r, g, b, a, duration);
            if (wait && duration > 0)
                vm_wait_frames(duration, m, char_atlas,
                               *tile_x, *tile_y, *direction_row);
        } else if (op == VM_OP_SCROLL_MAP) {
            int direction,distance,speed,wait;
            if(end-p<4)break;
            direction=p[0];distance=p[1];speed=p[2];wait=p[3];p+=4;
            vm_begin_map_scroll(direction,distance,speed);
            if(wait)vm_wait_frames(g_map_scroll_total,m,char_atlas,*tile_x,*tile_y,*direction_row);
        } else if (op == VM_OP_SHAKE) {
            int power, speed, duration, wait, i;
            if (end - p < 5) break;
            power = (int)p[0];
            speed = (int)p[1];
            duration = (int)read_u16_le(p + 2);
            wait = (int)p[4];
            p += 5;
            if (duration < 0) duration = 0;
            if (wait) {
                for (i = 0; i < duration; ++i) {
                    int period = 18 - speed;
                    float phase;
                    if (period < 2) period = 2;
                    phase = (float)(i % period) / (float)period;
                    /* Triangular shake is inexpensive and close to MZ's
                     * oscillating screen shake at PSP scale. */
                    g_screen_shake_x = ((phase < 0.5f ? phase * 4.0f - 1.0f
                                                      : 3.0f - phase * 4.0f)
                                        * (float)power * 0.5f);
                    vm_render_frame(m, char_atlas, *tile_x, *tile_y, *direction_row, 0, 0);
                }
            }
            if (!wait && m->id>=48 && m->id<=79 && duration>0) {
                g_stage_shake_power=power;g_stage_shake_speed=speed;
                g_stage_shake_total=g_stage_shake_frames=duration;
            } else g_screen_shake_x = 0.0f;
        } else if (op == VM_OP_BGM) {
            int bgm_id;
            if (end - p < 5) break;
            bgm_id = (int)read_u16_le(p + 0);
            p += 5;
            if (bgm_id >= 1000) {
                char path[180];
                snprintf(path,sizeof(path),ASSET_ROOT "bgm_exact_%04d.pcm",bgm_id);
                set_bgm_track(path);
            }
            else if (bgm_id == 1) set_bgm_track(BGM_PATH);
            else if (bgm_id == 2) set_bgm_track("");
        } else if (op == VM_OP_FADE_BGM) {
            int seconds;
            if (end - p < 2) break;
            seconds=read_u16_le(p);p+=2;
            g_bgm_fade_start=sceKernelGetSystemTimeWide();
            g_bgm_fade_generation=g_bgm_generation;
            g_bgm_fade_us=seconds ? (unsigned long long)seconds*1000000u : 1;
        } else if (op == VM_OP_STOP_SE) {
            ++g_se_generation;
        } else if (op == VM_OP_ERASE_EVENT) {
            if(m->id==source_map_id && source_event_id>0 && source_event_id<MAX_EVENT_ID)
                g_event_erased[source_event_id]=1;
        } else if (op == VM_OP_PICTURE6) {
            if (end - p < 1) break;
            g_picture6_alpha = (int)p[0];
            g_picture6_visible = g_picture6_alpha > 0;
            p += 1;
        } else if (op == VM_OP_VARIABLE) {
            int first, last, oper, i;
            int32_t value;
            if (end - p < 9) break;
            first = (int)read_u16_le(p + 0);
            last = (int)read_u16_le(p + 2);
            oper = (int)p[4];
            value = (int32_t)read_u32_le(p + 5);
            p += 9;
            if (first < 0) first = 0;
            if (last >= MAX_VARIABLES) last = MAX_VARIABLES - 1;
            for (i = first; i <= last; ++i) {
                if (oper == 0) g_variables[i] = value;
                else if (oper == 1) g_variables[i] += value;
                else if (oper == 2) g_variables[i] -= value;
                else if (oper == 3) g_variables[i] *= value;
                else if (oper == 4 && value != 0) g_variables[i] /= value;
                else if (oper == 5 && value != 0) g_variables[i] %= value;
            }
        } else if (op == VM_OP_COND_SWITCH) {
            int id, expected;
            uint32_t target;
            if (end - p < 7) break;
            id = (int)read_u16_le(p + 0);
            expected = (int)p[2];
            target = read_u32_le(p + 3);
            p += 7;
            if (id <= 0 || id >= MAX_SWITCHES || (!!g_switches[id]) != expected) {
                if (target >= pg->cmd_size) break;
                p = base + target;
            }
        } else if (op == VM_OP_COND_VAR) {
            int id, oper;
            int32_t value, actual;
            int ok = 0;
            uint32_t target;
            if (end - p < 11) break;
            id = (int)read_u16_le(p + 0);
            oper = (int)p[2];
            value = (int32_t)read_u32_le(p + 3);
            target = read_u32_le(p + 7);
            p += 11;
            actual = (id >= 0 && id < MAX_VARIABLES) ? g_variables[id] : 0;
            if (oper == 0) ok = (actual == value);
            else if (oper == 1) ok = (actual >= value);
            else if (oper == 2) ok = (actual <= value);
            else if (oper == 3) ok = (actual > value);
            else if (oper == 4) ok = (actual < value);
            else if (oper == 5) ok = (actual != value);
            if (!ok) {
                if (target >= pg->cmd_size) break;
                p = base + target;
            }
        } else if (op == VM_OP_COND_ITEM) {
            int id;
            uint32_t target;
            if (end - p < 6) break;
            id = (int)read_u16_le(p + 0);
            target = read_u32_le(p + 2);
            p += 6;
            if (id < 0 || id >= MAX_ITEMS || g_items[id] <= 0) {
                if (target >= pg->cmd_size) break;
                p = base + target;
            }
        } else if (op == VM_OP_COND_LANG) {
            int expected;
            uint32_t target;
            if (end - p < 5) break;
            expected = (int)p[0];
            target = read_u32_le(p + 1);
            p += 5;
            if (vm_text_language(lang) != expected) {
                if (target >= pg->cmd_size) break;
                p = base + target;
            }
        } else if (op == VM_OP_JUMP) {
            uint32_t target;
            if (end - p < 4) break;
            target = read_u32_le(p);
            if (target >= pg->cmd_size) break;
            p = base + target;
        } else if (op == VM_OP_SHOW_PICTURE_EX) {
            int id,rid,origin,x,y,zx,zy,alpha,blend;
            PictureSlot *slot;
            if(end-p<21) break;
            id=read_u16_le(p);rid=read_u16_le(p+2);origin=p[4];
            x=(int32_t)read_u32_le(p+5);y=(int32_t)read_u32_le(p+9);
            zx=read_u16_le(p+13);zy=read_u16_le(p+15);alpha=p[17];blend=p[18];
            if(p[19]) { x=(x>=0 && x<MAX_VARIABLES)?g_variables[x]:0;
                         y=(y>=0 && y<MAX_VARIABLES)?g_variables[y]:0; }
            p+=21;
            picture_show(id,rid,origin,x,y,zx,zy,alpha);
            slot=picture_slot(id);if(slot && slot->number) slot->blend=blend;
        } else if (op == VM_OP_MOVE_PICTURE_EX) {
            int id,origin,x,y,zx,zy,alpha,blend,duration,wait,easing;
            if(end-p<22) break;
            id=read_u16_le(p);origin=p[2];x=(int32_t)read_u32_le(p+3);
            y=(int32_t)read_u32_le(p+7);zx=read_u16_le(p+11);zy=read_u16_le(p+13);
            alpha=p[15];blend=p[16];duration=read_u16_le(p+17);wait=p[19];easing=p[20];
            if(p[21]) { x=(x>=0 && x<MAX_VARIABLES)?g_variables[x]:0;
                         y=(y>=0 && y<MAX_VARIABLES)?g_variables[y]:0; }
            p+=22;
            move_picture_start(picture_slot(id),origin,x,y,zx,zy,alpha,blend,duration,easing);
            if(wait) vm_wait_frames(duration,m,char_atlas,*tile_x,*tile_y,*direction_row);
        } else if (op == VM_OP_SHOW_PICTURE) {
            int id,rid,origin,x,y,zx,zy,alpha;
            if (end - p < 16) break;
            id=read_u16_le(p); rid=read_u16_le(p+2);origin=p[4];
            x=(int32_t)read_u32_le(p+5);y=(int32_t)read_u32_le(p+9);
            zx=p[13];zy=p[14];alpha=p[15];p+=16;
            picture_show(id,rid,origin,x,y,zx,zy,alpha);
        } else if (op == VM_OP_MOVE_PICTURE) {
            int id,x,y,zx,zy,alpha,duration,wait,i;
            PictureSlot *slot;
            int ox,oy,oa;
            if (end - p < 17) break;
            id=read_u16_le(p);x=(int32_t)read_u32_le(p+2);
            y=(int32_t)read_u32_le(p+6);
            zx=p[10];zy=p[11];alpha=p[12];
            duration=read_u16_le(p+13);wait=p[15];
            p+=17;
            if (id == 6) {
                /* A1 is a dedicated full-screen texture rather than a generic
                 * PictureSlot.  Preserve MZ Move Picture opacity fades too. */
                int oa6 = g_picture6_visible ? g_picture6_alpha : 0;
                if (wait && duration > 0 && duration <= 600) {
                    for (i = 1; i <= duration; ++i) {
                        g_picture6_alpha = oa6 + (alpha - oa6) * i / duration;
                        g_picture6_visible = g_picture6_alpha > 0;
                        vm_render_frame(m,char_atlas,*tile_x,*tile_y,*direction_row,0,0);
                    }
                }
                g_picture6_alpha = alpha;
                g_picture6_visible = alpha > 0;
                continue;
            }
            slot=picture_slot(id);
            if (!slot || !slot->number) continue;
            ox=slot->x; oy=slot->y; oa=slot->opacity;
            if (wait && duration > 0 && duration <= 600) {
                for(i=1;i<=duration;++i) {
                    slot->x=ox+(x-ox)*i/duration;
                    slot->y=oy+(y-oy)*i/duration;
                    slot->opacity=oa+(alpha-oa)*i/duration;
                    vm_render_frame(m,char_atlas,*tile_x,*tile_y,*direction_row,0,0);
                }
            }
            slot->x=x;slot->y=y;slot->scale_x=zx;
            slot->scale_y=zy;slot->opacity=alpha;
        } else if (op == VM_OP_ERASE_PICTURE) {
            int id;
            if (end - p < 2) break;
            id=read_u16_le(p);p+=2;
            erase_picture_slot(id);
            if(id==6){g_picture6_visible=0;g_picture6_alpha=0;}
        } else if (op == VM_OP_CHOICES || op==VM_OP_CHOICES_STYLE) {
            g_choice_background=0;g_choice_position=2;
            if(op==VM_OP_CHOICES_STYLE) {
                if(end-p<2)break;
                g_choice_background=*p++;g_choice_position=*p++;
            }
            int count, cancel_index, default_index, ci, li;
            const uint8_t *q;
            if (end - p < 3) break;
            count = (int)p[0];
            cancel_index = (int)(int8_t)p[1];
            default_index = (int)(int8_t)p[2];
            p += 3;
            if (count < 1 || count > 6) break;
            q = p;
            for (ci = 0; ci < count; ++ci) {
                uint16_t lens[4];
                if (end - q < 8) goto vm_done;
                for (li = 0; li < 4; ++li) lens[li] = read_u16_le(q + li * 2);
                q += 8;
                for (li = 0; li < 4; ++li) {
                    if ((size_t)(end - q) < lens[li]) goto vm_done;
                    if (li == vm_text_language(lang)) {
                        g_choice_labels[ci] = (const char *)q;
                        g_choice_lengths[ci] = lens[li];
                    }
                    q += lens[li];
                }
            }
            p = q;
            g_last_choice = vm_wait_choices(m, char_atlas, *tile_x, *tile_y,
                                            *direction_row, count, default_index, cancel_index);
        } else if (op == VM_OP_COND_CHOICE) {
            int choice;
            uint32_t target;
            if (end - p < 5) break;
            choice = (int)p[0]; target = read_u32_le(p + 1); p += 5;
            if (g_last_choice != choice) {
                if (target >= pg->cmd_size) break;
                p = base + target;
            }
        } else if (op == VM_OP_COMMON_EVENT) {
            int common_id;
            if (end - p < 2) break;
            common_id = (int)read_u16_le(p); p += 2;
            vm_call_common(m, ev, common_id, char_atlas, lang, tile_x, tile_y, direction_row);
            lang = g_language;
            if (g_request_title || g_loaded_from_scene) goto vm_done;
        } else if (op == VM_OP_PARTY) {
            if (end-p<2) break;
            player_change_party(p[0],p[1]);p+=2;
        } else if (op == VM_OP_PARALLAX) {
            if (end-p<1) break;
            g_stage_parallax_scrolling=!!*p++;g_stage_parallax_y=0;
        } else if (op == VM_OP_SAVE_ACCESS) {
            if (end - p < 1) break;
            g_save_enabled = *p++ ? 1 : 0;
        } else if (op == VM_OP_SCENE) {
            int scene_id;
            if (end - p < 1) break;
            scene_id = (int)*p++;
            if (scene_id == SCENE_ITEM) {
                ui_inventory_scene();
                lang = g_language;
            } else if (scene_id == SCENE_SAVE) {
                ui_save_load_scene(1, m->id, *tile_x, *tile_y, *direction_row, NULL);
            } else if (scene_id == SCENE_LOAD) {
                ProgressState loaded_state;
                if (ui_save_load_scene(0, 0, 0, 0, 0, &loaded_state)) {
                    if (!load_map(m, loaded_state.map_id))
                        fatal_error("Could not load selected save map");
                    *tile_x = loaded_state.tile_x;
                    *tile_y = loaded_state.tile_y;
                    *direction_row = loaded_state.direction_row;
                    if (*tile_x < 0 || *tile_x >= m->w || *tile_y < 0 || *tile_y >= m->h) {
                        *tile_x = 0;
                        *tile_y = 0;
                    }
                    did_transfer = 1;
                    g_loaded_from_scene = 1;
                    goto vm_done;
                }
            } else if (scene_id == SCENE_OPTIONS) {
                ui_options_scene();
                lang = g_language;
            } else if (scene_id == SCENE_TITLE) {
                g_request_title = 1;
                goto vm_done;
            }
        } else if (op == VM_OP_MOVE_ROUTE_EX) {
            int target,count,flags;
            if(end-p<5)break;
            target=(int16_t)read_u16_le(p);count=read_u16_le(p+2);flags=p[4];p+=5;
            if(end-p<count*5)break;
            if(target>=0 && m->id!=source_map_id){p+=count*5;continue;}
            if(target==0)target=source_event_id;
            if(target>0) {
                if(start_event_route(m,target,p,count,flags) && (flags&4))
                    while(g_routes[target].records)
                        vm_render_frame(m,char_atlas,*tile_x,*tile_y,*direction_row,0,0);
            } else if((flags&4) || !start_player_route(p,count,flags,tile_x,tile_y,direction_row)) {
                free(g_player_route.records);memset(&g_player_route,0,sizeof(g_player_route));
                vm_execute_move_route(m,char_atlas,tile_x,tile_y,direction_row,p,count,target);
            }
            p+=count*5;
        } else if (op == VM_OP_MOVE_ROUTE) {
            int target, count;
            if (end - p < 4) break;
            target = (int16_t)read_u16_le(p);
            count = (int)read_u16_le(p + 2);
            p += 4;
            if (count < 0 || end - p < count * 5) break;
            if (target >= 0 && m->id != source_map_id) {
                /* Game_Interpreter.character() returns null for map events
                 * after transferring away from the interpreter's source map.
                 * The player target (-1) remains valid. */
                p += count * 5;
                continue;
            }
            if (target == 0) target = source_event_id;
            vm_execute_move_route(m, char_atlas, tile_x, tile_y, direction_row,
                                  p, count, target);
            p += count * 5;
        } else if (op == VM_OP_EXIT) {
            break;
        } else {
            break;
        }
    }

vm_done:
    g_vm_position_owner=previous_owner;g_vm_tile_x=previous_x;
    g_vm_tile_y=previous_y;g_vm_direction=previous_direction;
    free(cmd_copy);
    return did_transfer;
}

static int vm_run_event_at(
    MapState *m, int x, int y, int trigger,
    void *char_atlas, int lang,
    int *tile_x, int *tile_y, int *direction_row)
{
    VmEventRecord ev;
    VmPageRecord pg;
    if (!vm_find_event_at(m, x, y, trigger, &ev, &pg)) return 0;
    if (is_chase_enemy(m->id,ev.event_id) && trigger == 2) {
        g_s003_paused = 1;
        free(g_routes[ev.event_id].records); memset(&g_routes[ev.event_id],0,sizeof(g_routes[ev.event_id]));
    }
    return 1 + vm_execute_page(
        m, &ev, &pg, char_atlas, lang, tile_x, tile_y, direction_row
    );
}

static void vm_run_autoruns(
    MapState *m, void *char_atlas, int lang,
    int *tile_x, int *tile_y, int *direction_row)
{
    int pass;
    for (pass = 0; pass < 12; ++pass) {
        int i;
        int ran = 0;
        for (i = 0; i < m->vm_event_count; ++i) {
            VmEventRecord ev;
            VmPageRecord pg;
            if (!vm_read_event(m, i, &ev)) continue;
            if (!vm_active_page(m, &ev, &pg)) continue;
            if (pg.trigger != 3 || !pg.supported) continue;
            vm_execute_page(m, &ev, &pg, char_atlas, lang, tile_x, tile_y, direction_row);
            lang = g_language;
            if (g_request_title || g_loaded_from_scene) return;
            ran = 1;
            break;
        }
        if (!ran) break;
    }
}

/* Run the two early one-shot parallel events that gate the Worm route without
 * freezing player input.  This mirrors the relevant RPG Maker trigger-4
 * pages frame-for-frame instead of executing their Wait commands synchronously.
 *
 * Map006 Event 13 (second Green Key) sets switch 16 ON.  Once the player later
 * reaches Map008, Event 70 waits 180 frames, plays the water cue, then sets
 * switch 22 ON.  That activates Map009 Event 4 page 5; touching the opened meat
 * wall starts the original Worm cutscene, whose animation is driven by
 * switches 41-44 and Map009 Event 7.
 */
static void vm_tick_story_parallel(const MapState *m)
{
    if (!m) return;

    /* If an older save somehow contains the "timer finished" flag without the
     * immediately-following Worm-arm flag, repair that impossible original
     * state on Map009.  In RPG Maker those two switch writes are consecutive. */
    if (m->id == 9 && g_switches[16] && g_switches[17] && !g_switches[22])
        g_switches[22] = 1;

    /* Map008 Event 70 is the delayed off-screen water cue that arms the
     * Map009 Worm route.  Older builds reset this 180-frame timer as soon as
     * the player transferred out of Map008, so a quick walk into Map009 could
     * leave switch 22 permanently OFF.  Once started, carry the timer through
     * the immediate Map008 -> Map009 transfer.  If an old save is already on
     * Map009 with switch 16 set but the tail never fired, repair it there. */
    if (g_switches[16] && !g_switches[17]) {
        if (m->id == 8) {
            if (g_map8_drop_timer < 1)
                g_map8_drop_timer = 1;
            else
                ++g_map8_drop_timer;
        } else if (m->id == 9) {
            if (g_map8_drop_timer < 1)
                g_map8_drop_timer = 180;
            else
                ++g_map8_drop_timer;
        }

        if (g_map8_drop_timer >= 180) {
            play_vm_se_params(2, 70, 80, 80); /* 【魔王魂】 水01 */
            g_switches[17] = 1;
            g_switches[13] = 0;
            g_switches[22] = 1;
            g_map8_drop_timer = 0;
        }
    } else if (!g_switches[16] || g_switches[17]) {
        g_map8_drop_timer = 0;
    }

    /* Map009 Event 115: delayed off-screen water sound after taking the Green
     * Key from the corpse. */
    if (m->id == 9 && g_switches[26]) {
        if (++g_map9_water_timer >= 200) {
            play_vm_se_params(2, 50, 80, 80);
            g_switches[26] = 0;
            g_map9_water_timer = 0;
        }
    } else {
        g_map9_water_timer = 0;
    }
}
