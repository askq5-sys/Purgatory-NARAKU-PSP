/* Script VM: flashes, tints, messages, choices, numbers.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* v0.4.0 synchronous event VM                                               */
/* ------------------------------------------------------------------------- */

static void vm_begin_screen_flash(int r, int g, int b, int a, int frames)
{
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    if (frames < 0) frames = 0;
    g_screen_flash_r = r;
    g_screen_flash_g = g;
    g_screen_flash_b = b;
    g_screen_flash_a = a;
    g_screen_flash_total = frames;
    g_screen_flash_remaining = frames;
}

static void vm_begin_world_tint(int r, int g, int b, int gray, int frames)
{
    if (r < -255) r = -255;
    if (r > 255) r = 255;
    if (g < -255) g = -255;
    if (g > 255) g = 255;
    if (b < -255) b = -255;
    if (b > 255) b = 255;
    if (gray < 0) gray = 0;
    if (gray > 255) gray = 255;
    if (frames <= 0) {
        g_world_tone_r = r;
        g_world_tone_g = g;
        g_world_tone_b = b;
        g_world_tone_gray = gray;
        g_world_tone_from_r = r;
        g_world_tone_from_g = g;
        g_world_tone_from_b = b;
        g_world_tone_from_gray = gray;
        g_world_tone_target_r = r;
        g_world_tone_target_g = g;
        g_world_tone_target_b = b;
        g_world_tone_target_gray = gray;
        g_world_tone_total = 0;
        g_world_tone_remaining = 0;
        return;
    }
    g_world_tone_from_r = g_world_tone_r;
    g_world_tone_from_g = g_world_tone_g;
    g_world_tone_from_b = g_world_tone_b;
    g_world_tone_from_gray = g_world_tone_gray;
    g_world_tone_target_r = r;
    g_world_tone_target_g = g;
    g_world_tone_target_b = b;
    g_world_tone_target_gray = gray;
    g_world_tone_total = frames;
    g_world_tone_remaining = frames;
}

static void vm_tick_world_tint(void)
{
    int done;
    if (g_world_tone_remaining <= 0 || g_world_tone_total <= 0) return;
    done = g_world_tone_total - g_world_tone_remaining + 1;
    g_world_tone_r = g_world_tone_from_r +
        (g_world_tone_target_r - g_world_tone_from_r) * done / g_world_tone_total;
    g_world_tone_g = g_world_tone_from_g +
        (g_world_tone_target_g - g_world_tone_from_g) * done / g_world_tone_total;
    g_world_tone_b = g_world_tone_from_b +
        (g_world_tone_target_b - g_world_tone_from_b) * done / g_world_tone_total;
    g_world_tone_gray = g_world_tone_from_gray +
        (g_world_tone_target_gray - g_world_tone_from_gray) * done / g_world_tone_total;
    g_world_tone_remaining--;
    if (g_world_tone_remaining <= 0) {
        g_world_tone_r = g_world_tone_target_r;
        g_world_tone_g = g_world_tone_target_g;
        g_world_tone_b = g_world_tone_target_b;
        g_world_tone_gray = g_world_tone_target_gray;
        g_world_tone_total = 0;
    }
}

static int vm_text_has_visible_content(const char *text, size_t len)
{
    size_t i = 0;
    if (!text || len == 0) return 0;
    while (i < len) {
        unsigned char c = (unsigned char)text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            ++i;
            continue;
        }
        /* U+3000 IDEOGRAPHIC SPACE, used by Japanese blank input gates. */
        if (i + 2 < len &&
            (unsigned char)text[i] == 0xE3 &&
            (unsigned char)text[i + 1] == 0x80 &&
            (unsigned char)text[i + 2] == 0x80) {
            i += 3;
            continue;
        }
        return 1;
    }
    return 0;
}

static const MapState *g_vm_position_owner;
static int *g_vm_tile_x, *g_vm_tile_y, *g_vm_direction;
static void vm_pump_stage_message(const MapState *m, void *char_atlas, int x, int y, int direction);

static void vm_render_frame(
    const MapState *m, void *char_atlas,
    int tile_x, int tile_y, int direction_row,
    int black_alpha, int red_alpha)
{
    if (g_vm_position_owner==m && g_vm_tile_x && g_vm_tile_y && g_vm_direction) {
        tile_x=*g_vm_tile_x;tile_y=*g_vm_tile_y;direction_row=*g_vm_direction;
    }
    float wx = ((float)tile_x + 0.5f) * TILE_PX;
    float wy = ((float)tile_y + 1.0f) * TILE_PX;
    g_player_animation_speed = clamp_move_speed_code(g_player_move_speed_code);
    render_world(
        m, char_atlas,
        g_player_visible, wx, wy, 1, direction_row, red_alpha, black_alpha);
    vm_pump_stage_message(m,char_atlas,tile_x,tile_y,direction_row);
}

static void vm_wait_frames(
    int frames, const MapState *m, void *char_atlas,
    int tile_x, int tile_y, int direction_row)
{
    int i;
    for (i = 0; i < frames; ++i)
        vm_render_frame(m, char_atlas, tile_x, tile_y, direction_row, 0, 0);
}

/* Dialogue-only English styling; preserve surrounding color escapes. */
static char *style_green_key_dialogue(const char *text, size_t len, size_t *out_len)
{
    const char *p = text, *end = text + len;
    char *out = malloc(len * 3 + 1), *dst = out;
    int color = 0;
    if (!out) return NULL;
    while (p < end) {
        const char *before = p;
        if (parse_text_color_escape(&p, end, &color)) {
            memcpy(dst, before, p - before); dst += p - before;
            continue;
        }
        {
            static const char *names[] = {"Green key", "Iron key", "Red key", "Gold key", "Cleaver"};
            static const int colors[] = {3, -1, 18, 14, -1};
            int k, matched = 0;
            for (k = 0; k < 5; ++k) {
                size_t n = strlen(names[k]), j;
                if ((size_t)(end-p) < n ||
                    (p != text && latin_word_cp((unsigned char)p[-1])) ||
                    ((size_t)(end-p) > n && latin_word_cp((unsigned char)p[n]))) continue;
                for (j = 0; j < n; ++j) {
                    unsigned char c = p[j], expected = names[k][j];
                    if (c >= 'A' && c <= 'Z') c += 'a'-'A';
                    if (expected >= 'A' && expected <= 'Z') expected += 'a'-'A';
                    if (c != expected) break;
                }
                if (j != n) continue;
                if (colors[k] >= 0) dst += sprintf(dst, "\\c[%d]", colors[k]);
                memcpy(dst, p, n);
                dst += n;
                if (colors[k] >= 0) dst += sprintf(dst, "\\c[%d]", color);
                p += n; matched = 1; break;
            }
            if (matched) continue;
        }
        *dst++ = *p;
        ++p;
    }
    *dst = 0; *out_len = (size_t)(dst - out);
    return out;
}

static void vm_wait_text_page(
    const MapState *m, void *char_atlas,
    int tile_x, int tile_y, int direction_row,
    const char *speaker, size_t speaker_len,
    const char *message, size_t message_len, int continue_message)
{
    SceCtrlData pad;
    uint32_t prev;
    char *styled = NULL;
    int auto_advance = 0, auto_frames = 0;
    /* A trailing RPG Maker control suppresses the input pause. Never print it. */
    if (message) {
        size_t visible_len = message_len;
        while (visible_len && (message[visible_len-1]=='\n' ||
               message[visible_len-1]=='\r' || message[visible_len-1]==' ')) --visible_len;
        if (visible_len >= 2 && message[visible_len-2]=='\\' && message[visible_len-1]=='^') {
            auto_advance = 1;
            message_len = visible_len-2;
        }
    }

    /* This shutter notification fits two native PSP rows. Ignore the PC
     * source wraps (including a trailing newline) before choosing its layout. */
    if (g_language == 0 && message && message_len < 128) {
        char flat[128]; size_t i, n=0; int space=0;
        for (i=0;i<message_len;++i) {
            char c=message[i];
            if (c==' ' || c=='\n' || c=='\r' || c=='\t') {space=n>0;continue;}
            if (space) flat[n++]=' ';
            space=0;flat[n++]=c;
        }
        flat[n]=0;
        if (!strcmp(flat,"It seems two keys are required to open the shutter.")) {
            message="It seems two keys are required\nto open the shutter.";
            message_len=strlen(message);
        }
    }
    if (g_language == 0 && message &&
        ((message_len == 16 && !memcmp(message,"Obtained an Axe.",16)) ||
         (message_len == 17 && !memcmp(message,"Obtained an Axe.\n",17)))) {
        message = "Obtained an \\c[8]Axe\\c[0].";
        message_len = strlen(message);
    }

    /* Map020 gold door: remove the PC-specific wrap and repair the missing
     * sentence space. Both sentences fit at the existing PSP font size. */
    if (g_language == 0 && message &&
        (message_len == sizeof("An old door made from gold plate. It won't \nopen.It's locked.") - 1 ||
         (message_len == sizeof("An old door made from gold plate. It won't \nopen.It's locked.") && message[message_len - 1] == '\n')) &&
        !memcmp(message, "An old door made from gold plate. It won't \nopen.It's locked.",
                sizeof("An old door made from gold plate. It won't \nopen.It's locked.") - 1)) {
        message = "An old door made from gold plate.\nIt won't open. It's locked.";
        message_len = strlen(message);
    }
    if (g_language == 0 && message && message_len) {
        styled = style_green_key_dialogue(message, message_len, &message_len);
        if (styled) message = styled;
    }

    g_runtime_speaker = speaker;
    g_runtime_speaker_len = speaker_len;
    g_runtime_message = message;
    g_runtime_message_len = message_len;

    /* NARAKU uses empty, transparent Show Text commands as an input gate
     * while a full-screen picture is visible (e.g. equations / controls).
     * Drawing our normal dialogue box for those commands was incorrect and
     * also made the controls board look like a modal menu. */
    g_runtime_message_active =
        (vm_text_has_visible_content(speaker, speaker_len) ||
         vm_text_has_visible_content(message, message_len)) ? 1 : 0;
    if (g_runtime_message_active)
        rebuild_runtime_message_surface();
    else
        g_runtime_message_surface_ready = 0;

    if (g_runtime_message_active) {
        while (g_runtime_message_openness < 255) {
            g_runtime_message_openness += 32;
            if (g_runtime_message_openness > 255) g_runtime_message_openness = 255;
            vm_render_frame(m, char_atlas, tile_x, tile_y, direction_row, 0, 0);
        }
    } else g_runtime_message_openness = 0;

    sceCtrlPeekBufferPositive(&pad, 1);
    prev = pad.Buttons;
    while (1) {
        /* Empty Show Text is only an invisible input gate in NARAKU.
         * Force the overlay off every frame so a stale previous text box
         * can never leak under full-screen pictures such as A2. */
        if (!vm_text_has_visible_content(speaker, speaker_len) &&
            !vm_text_has_visible_content(message, message_len))
            g_runtime_message_active = 0;
        vm_render_frame(m, char_atlas, tile_x, tile_y, direction_row, 0, 0);
        sceCtrlPeekBufferPositive(&pad, 1);
        if (auto_advance && ++auto_frames >= 30) break;
        if ((pad.Buttons & ~prev) & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE)) break;
        prev = pad.Buttons;
    }

    if (g_runtime_message_active && !continue_message) {
        while (g_runtime_message_openness > 0) {
            g_runtime_message_openness -= 32;
            if (g_runtime_message_openness < 0) g_runtime_message_openness = 0;
            vm_render_frame(m, char_atlas, tile_x, tile_y, direction_row, 0, 0);
        }
    }
    g_runtime_message_active = 0;
    g_runtime_speaker = NULL;
    g_runtime_message = NULL;
    g_runtime_speaker_len = 0;
    g_runtime_message_len = 0;
    g_runtime_message_surface_ready = 0;
    free(styled);
}

/* PC font-size directives are metadata. Strip before measuring words so old
 * installed resources and restored emulator states cannot print them either. */
static char *strip_message_font_controls(const char *text, size_t len, size_t *out_len)
{
    char *out = malloc(len + 1), *dst = out;
    const char *p = text, *end = text + len;
    if (!out) return NULL;
    while (p < end) {
        if (end-p >= 6 && p[0]=='\\' && (p[1]=='f'||p[1]=='F') &&
            (p[2]=='s'||p[2]=='S') && p[3]=='[') {
            const char *q=p+4;
            while (q<end && *q>='0' && *q<='9') ++q;
            if (q>p+4 && q<end && *q==']') {p=q+1;continue;}
        }
        *dst++=*p++;
    }
    *dst=0;*out_len=(size_t)(dst-out);
    return out;
}

/* Reflow the PC-authored flashback prose at native PSP size. Explicit source
 * line breaks are layout hints here; paginate rather than drop a fourth row. */
static void vm_wait_text(
    const MapState *m, void *char_atlas,
    int tile_x, int tile_y, int direction_row,
    const char *speaker, size_t speaker_len,
    const char *message, size_t message_len, int continue_message)
{
    if(g_language==4) {
        russian_lookup(&speaker,&speaker_len);
        russian_lookup(&message,&message_len);
    }
    char *cleaned = strip_message_font_controls(message, message_len, &message_len);
    const char *p, *end;
    if (cleaned) message = cleaned;
    p = message; end = message + message_len;
    char *page, *dst;
    int width = 0, line = 0;
    const uint8_t *space_rec;
    int space_width;
    /* Ending captions are separate authored lines, not prose wrap hints. */
    if ((g_language != 0 && g_language != 4) || !message_len ||
        (g_language==4 && message_len>=sizeof("КОНЕЦ ")-1 && !memcmp(message,"КОНЕЦ ",sizeof("КОНЕЦ ")-1)) ||
        (message_len>=4 && !memcmp(message,"END ",4))) {
        vm_wait_text_page(m,char_atlas,tile_x,tile_y,direction_row,
                          speaker,speaker_len,message,message_len,continue_message);
        free(cleaned);
        return;
    }
    page = malloc(message_len * 2 + 4);
    if (!page) {
        vm_wait_text_page(m,char_atlas,tile_x,tile_y,direction_row,
                          speaker,speaker_len,message,message_len,continue_message);
        free(cleaned);
        return;
    }
    space_rec = font_find_glyph(' ');
    space_width = space_rec ? (int)(space_rec[7] * 0.55f) : 5;
    dst = page;
    while (p < end) {
        const char *word, *q;
        int word_width = 0;
        while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p;
        if (p == end) break;
        word = p;
        while (p < end && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') ++p;
        q = word;
        while (q < p) {
            const uint8_t *rec;
            /* Colour and auto-close controls have no visible advance. */
            if (p-q >= 2 && q[0] == '\\' && q[1] == '^') { q += 2; continue; }
            if (p-q >= 5 && q[0] == '\\' && (q[1]=='c'||q[1]=='C') && q[2]=='[') {
                const char *control=q+3;
                while(control<p && *control>='0' && *control<='9') ++control;
                if(control<p && *control==']') {q=control+1;continue;}
            }
            rec = font_find_glyph(utf8_next_cp(&q,p));
            if (!rec) rec = font_find_glyph('?');
            word_width += rec ? (int)(rec[7] * 0.55f) : 5;
        }
        if (width && width + space_width + word_width > MESSAGE_BOX_W - 18) {
            width = 0;
            if (++line == 3) {
                vm_wait_text_page(m,char_atlas,tile_x,tile_y,direction_row,
                                  speaker,speaker_len,page,(size_t)(dst-page),1);
                dst = page; line = 0;
            } else *dst++ = '\n';
        }
        if (width) { *dst++ = ' '; width += space_width; }
        memcpy(dst,word,(size_t)(p-word)); dst += p-word;
        width += word_width;
    }
    vm_wait_text_page(m,char_atlas,tile_x,tile_y,direction_row,
                      speaker,speaker_len,page,(size_t)(dst-page),continue_message);
    free(page);
    free(cleaned);
}

static void vm_pump_stage_message(const MapState *m, void *char_atlas, int x, int y, int direction)
{
    int id=g_stage_message_event,i,active;VmEventRecord ev;VmPageRecord pg;
    StageParallel *r;const uint8_t *p,*q,*end;
    const char *speaker="",*message="";size_t sl=0,ml=0;
    if(id<1 || id>=MAX_EVENT_ID)return;
    g_stage_message_event=0;r=&g_stage_parallel[id];
    for(i=0;i<m->vm_event_count;++i) {
        if(!vm_read_event(m,i,&ev) || ev.event_id!=id)continue;
        if(!vm_active_page_index(m,&ev,&pg,&active) || r->page!=active+1 ||
           r->text_offset>=pg.cmd_size || (size_t)pg.cmd_offset+pg.cmd_size>m->vm_command_size)break;
        p=m->vm_commands+pg.cmd_offset+r->text_offset;
        end=m->vm_commands+pg.cmd_offset+pg.cmd_size;
        if(end-p<17 || (*p!=VM_OP_TEXT && *p!=VM_OP_TEXT_TRANSPARENT && *p!=VM_OP_TEXT_STYLE))break;
        {int op=*p++;
        g_runtime_message_background=op==VM_OP_TEXT_TRANSPARENT?2:0;g_runtime_message_position=2;
        if(op==VM_OP_TEXT_STYLE){if(end-p<18)break;g_runtime_message_background=*p++;g_runtime_message_position=*p++;}
        g_runtime_message_transparent=g_runtime_message_background==2;}
        q=p+16;
        for(i=0;i<4;++i) {
            size_t a=read_u16_le(p+i*4),b=read_u16_le(p+i*4+2);
            if((size_t)(end-q)<a+b)break;
            if(i==vm_text_language(g_language)){speaker=(const char*)q;message=(const char*)q+a;sl=a;ml=b;}
            q+=a+b;
        }
        if(i==4)vm_wait_text(m,char_atlas,x,y,direction,speaker,sl,message,ml,0);
        break;
    }
    g_runtime_message_transparent=0;g_runtime_message_background=0;
    r->text_blocked=0;
}

static int vm_wait_choices(
    const MapState *m, void *char_atlas,
    int tile_x, int tile_y, int direction_row,
    int count, int default_index, int cancel_index)
{
    SceCtrlData pad;
    uint32_t prev;
    if (count < 1 || count > 6) return -1;
    if(g_language==4) {
        int i;
        for(i=0;i<count;++i)russian_lookup(&g_choice_labels[i],&g_choice_lengths[i]);
    }
    g_choice_active = 1;
    g_choice_count = count;
    rebuild_choice_text_surface();
    g_choice_cursor = (default_index >= 0 && default_index < count) ? default_index : 0;
    sceCtrlPeekBufferPositive(&pad, 1);
    prev = pad.Buttons;
    while (1) {
        uint32_t pressed;
        vm_render_frame(m, char_atlas, tile_x, tile_y, direction_row, 0, 0);
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev;
        prev = pad.Buttons;
        if (pressed & PSP_CTRL_UP) {
            g_choice_cursor = (g_choice_cursor + count - 1) % count;
            play_se_async(SE_UI_CURSOR_PATH);
        }
        if (pressed & PSP_CTRL_DOWN) {
            g_choice_cursor = (g_choice_cursor + 1) % count;
            play_se_async(SE_UI_CURSOR_PATH);
        }
        if (pressed & ui_ok_mask()) {
            play_se_async(SE_UI_OK_PATH);
            break;
        }
        if (pressed & ui_cancel_mask()) {
            if (cancel_index >= 0 && cancel_index < count) {
                g_choice_cursor = cancel_index;
                play_se_async(SE_UI_CANCEL_PATH);
                break;
            } else {
                play_se_async(SE_UI_BUZZER_PATH);
            }
        }
    }
    g_choice_active = 0;
    return g_choice_cursor;
}

static int number_input_change_digit(int value,int digits,int cursor,int change)
{
    int place=1,i,digit;
    for(i=cursor+1;i<digits;++i)place*=10;
    digit=(value/place)%10;
    return value+(((digit+change+10)%10)-digit)*place;
}

static int vm_wait_number_input(const MapState *m,void *char_atlas,
    int tile_x,int tile_y,int direction_row,int value,int digits)
{
    SceCtrlData pad;uint32_t prev,held=0;int held_frames=0,max=1,i;
    if(digits<1 || digits>8)return value;
    for(i=0;i<digits;++i)max*=10;
    g_number_value=value<0?0:(value>=max?max-1:value);
    g_number_digits=digits;g_number_cursor=0;g_number_active=1;
    sceCtrlPeekBufferPositive(&pad,1);prev=pad.Buttons;
    while(1) {
        uint32_t pressed,nav;
        vm_render_frame(m,char_atlas,tile_x,tile_y,direction_row,0,0);
        sceCtrlPeekBufferPositive(&pad,1);pressed=pad.Buttons&~prev;prev=pad.Buttons;
        nav=pad.Buttons&(PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT);
        if(nav!=held) {held=nav;held_frames=0;}
        else if(nav && ++held_frames>=28 && (held_frames-28)%5==0)pressed|=nav;
        if(pressed&PSP_CTRL_LEFT)g_number_cursor=(g_number_cursor+digits-1)%digits;
        if(pressed&PSP_CTRL_RIGHT)g_number_cursor=(g_number_cursor+1)%digits;
        if(pressed&PSP_CTRL_UP)g_number_value=number_input_change_digit(g_number_value,digits,g_number_cursor,1);
        if(pressed&PSP_CTRL_DOWN)g_number_value=number_input_change_digit(g_number_value,digits,g_number_cursor,-1);
        if(pressed&(PSP_CTRL_UP|PSP_CTRL_DOWN|PSP_CTRL_LEFT|PSP_CTRL_RIGHT))play_se_async(SE_UI_CURSOR_PATH);
        if(pressed&ui_ok_mask()) {play_se_async(SE_UI_OK_PATH);break;}
        if(pressed&ui_cancel_mask())play_se_async(SE_UI_BUZZER_PATH);
    }
    g_number_active=0;
    return g_number_value;
}
