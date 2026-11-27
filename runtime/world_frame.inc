/* Picture animation, special rooms and render_world.
 * Part of main.c: included there in order, not a standalone unit. */

static float picture_ease(float t, int kind)
{
    if (kind == 1) return t*t;
    if (kind == 2) return 1.0f-(1.0f-t)*(1.0f-t);
    if (kind == 3) return t < 0.5f ? 2.0f*t*t : 1.0f-2.0f*(1.0f-t)*(1.0f-t);
    return t;
}

static void tick_pictures(void)
{
    int i;
    for (i=0; i<MAX_ACTIVE_PICTURES; ++i) {
        PictureSlot *p=&g_pictures[i];
        float t, v[5]; int j;
        if (!p->number || p->duration <= 0) continue;
        t=picture_ease((float)(p->total-p->duration+1)/p->total,p->easing);
        for(j=0;j<5;++j) v[j]=p->from[j]+(p->target[j]-p->from[j])*t;
        p->x=v[0];p->y=v[1];p->scale_x=v[2];p->scale_y=v[3];p->opacity=v[4];
        --p->duration;
    }
}

static void move_picture_start(PictureSlot *p,int origin,int x,int y,int zx,int zy,
                               int alpha,int blend,int duration,int easing)
{
    if (!p || !p->number) return;
    p->origin=origin;p->blend=blend;p->easing=easing;
    p->from[0]=p->x;p->from[1]=p->y;p->from[2]=p->scale_x;
    p->from[3]=p->scale_y;p->from[4]=p->opacity;
    p->target[0]=x;p->target[1]=y;p->target[2]=zx;p->target[3]=zy;p->target[4]=alpha;
    p->duration=p->total=duration;
    /* MZ duration=0 records a target without running updateMove. */
}

static void draw_vm_pictures(int below_a1, int gallery)
{
    int i,j,order[MAX_ACTIVE_PICTURES];
    for(i=0;i<MAX_ACTIVE_PICTURES;++i)order[i]=i;
    for(i=1;i<MAX_ACTIVE_PICTURES;++i)for(j=i;j>0 &&
        g_pictures[order[j]].number<g_pictures[order[j-1]].number;--j) {
        int tmp=order[j];order[j]=order[j-1];order[j-1]=tmp;
    }
    for(i=0;i<MAX_ACTIVE_PICTURES;++i) {
        PictureSlot *p = &g_pictures[order[i]];
        float x,y,w,h;
        if(!p->number || !p->texture || p->opacity<1)continue;
        if ((p->number < 6) != below_a1) continue;
        w=(float)p->w * p->scale_x / 100.0f;
        h=(float)p->h * p->scale_y / 100.0f;
        {
            int fit_screen = gallery || p->fit_screen ||
                (p->resource_id >= 90 && p->resource_id <= 97) ||
                (!p->portrait && p->number == 11 && p->w == 408 && p->h == 312);
            if (p->resource_id >= 90 && p->resource_id <= 97) {
                w = SCREEN_W; h = SCREEN_H; x = 0; y = 0;
            } else if (fit_screen) {
                float sx = (float)SCREEN_W / w;
                float sy = (float)SCREEN_H / h;
                float scale = sx < sy ? sx : sy;
                w *= scale;
                h *= scale;
                x = ((float)SCREEN_W - w) * 0.5f;
                y = ((float)SCREEN_H - h) * 0.5f;
            } else {
            x=render_round_pixel(36.0f+(float)p->x*0.5f);
            y=render_round_pixel(-20.0f+(float)p->y*0.5f);
            if(p->origin==1){x-=w*0.5f;y-=h*0.5f;}
            /* Sprite_Picture.updatePosition() rounds the final picture
             * coordinate after origin handling. */
            x=render_round_pixel(x);
            y=render_round_pixel(y);
            }
        }
        /* Cover fractional rasterisation gaps under Finish A1 as well as
         * its side fields. The texture also carries a replicated edge texel. */
        if (p->resource_id==104 && p->fit_screen) {
            sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
            draw_solid_rect(0,0,SCREEN_W,SCREEN_H,255,255,255,(int)p->opacity);
        }
        if (gallery) draw_solid_rect(0,0,SCREEN_W,SCREEN_H,0,0,0,(int)p->opacity);
        if (!p->portrait && ((p->number == 11 && p->w == 408 && p->h == 312) ||
                              (p->resource_id >= 99 && p->resource_id <= 103)))
            draw_solid_rect(0,0,SCREEN_W,SCREEN_H,0,0,0,(int)p->opacity);
        sceGuTexMode(GU_PSM_4444,0,0,GU_FALSE);
        sceGuTexImage(0,512,512,512,p->texture);
        sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
        {
            int fit_filter = gallery || p->portrait || p->fit_screen ||
                (p->resource_id >= 90 && p->resource_id <= 97) ||
                (!p->portrait && p->number == 11 && p->w == 408 && p->h == 312);
            sceGuTexFilter(fit_filter ? GU_LINEAR : GU_NEAREST,
                           fit_filter ? GU_LINEAR : GU_NEAREST);
        }
        if (p->blend == 1) sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_FIX,0,0xFFFFFFFF);
        else if (p->blend == 2) sceGuBlendFunc(GU_ADD,GU_DST_COLOR,GU_ONE_MINUS_SRC_ALPHA,0,0);
        else if (p->blend == 3) sceGuBlendFunc(GU_ADD,GU_FIX,GU_ONE_MINUS_SRC_COLOR,0xFFFFFFFF,0);
        else sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
        sceGuColor(((unsigned int)p->opacity<<24)|0x00FFFFFFu);
        if(p->portrait) {
            float zx=gallery?w/p->w:p->scale_x/100.0f,zy=gallery?h/p->h:p->scale_y/100.0f;
            draw_bound_rect(1,1,p->source_w,p->source_h,
                x+p->picture_offset_x*zx,y+p->picture_offset_y*zy,
                p->picture_region_w*zx,p->picture_region_h*zy);
        } else draw_bound_rect(0,0,(float)p->w,(float)p->h,x,y,w,h);
        sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
        sceGuColor(0xFFFFFFFF);
        sceGuTexFunc(GU_TFX_REPLACE,GU_TCC_RGBA);
    }
}


static int lucas_fall_frame_from_switches(void)
{
    /* RPG Maker selects the last valid event page.  Event 45 uses switches
     * 45..49 for the five visible frames; switch 50 makes it blank again. */
    if (g_switches[50]) return -1;
    if (g_switches[49]) return 4;
    if (g_switches[48]) return 3;
    if (g_switches[47]) return 2;
    if (g_switches[46]) return 1;
    if (g_switches[45]) return 0;
    return -1;
}

static void draw_lucas_fall_event(
    const MapState *m,
    float camera_x, float camera_y, float origin_x, float origin_y)
{
    int slot;
    float foot_x, foot_y;
    TextureVertex *v;
    float src_x;

    if (!m || m->id != 1 || !g_lucas_fall_atlas) return;
    slot = lucas_fall_frame_from_switches();
    if (slot < 0) return;

    foot_x = render_floor_pixel(
        origin_x +
        ((float)LUCAS_FALL_EVENT_X + g_event_shift_x[45] + 0.5f) * TILE_PX - camera_x
    );
    foot_y = render_floor_pixel(
        origin_y +
        ((float)LUCAS_FALL_EVENT_Y + g_event_shift_y[45] + 1.0f) * TILE_PX - camera_y
    );

    bind_texture_8888(g_lucas_fall_atlas, LUCAS_FALL_ATLAS_W, LUCAS_FALL_ATLAS_H);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    src_x = (float)(slot * LUCAS_FALL_SLOT_W + 1);
    v = (TextureVertex *)sceGuGetMemory(2 * sizeof(TextureVertex));
    v[0].u = src_x;
    v[0].v = 1.0f;
    v[0].x = foot_x - (float)LUCAS_FALL_SRC_W * 0.5f;
    v[0].y = foot_y - (float)LUCAS_FALL_SRC_H;
    v[0].z = 0.0f;
    v[1].u = src_x + (float)LUCAS_FALL_SRC_W;
    v[1].v = 1.0f + (float)LUCAS_FALL_SRC_H;
    v[1].x = foot_x + (float)LUCAS_FALL_SRC_W * 0.5f;
    v[1].y = foot_y;
    v[1].z = 0.0f;
    sceGuDrawArray(
        GU_SPRITES,
        GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
        2, NULL, v
    );
}

static void draw_lucas_corpse_event(
    const MapState *m,
    float camera_x, float camera_y, float origin_x, float origin_y)
{
    float foot_x, foot_y;
    TextureVertex *v;
    if (!m || m->id != 1 || !g_lucas_corpse) return;
    /* Event 39 becomes the fallen Lucas body when switch 51 is ON and stays
     * as the same corpse on page 3 while switch 52 is ON. */
    if (!g_switches[51] && !g_switches[52]) return;

    foot_x = render_floor_pixel(
        origin_x + ((float)LUCAS_CORPSE_EVENT_X + 0.5f) * TILE_PX - camera_x
    );
    foot_y = render_floor_pixel(
        origin_y + ((float)LUCAS_CORPSE_EVENT_Y + 1.0f) * TILE_PX - camera_y
    );

    bind_texture_8888(g_lucas_corpse, LUCAS_CORPSE_TEX_W, LUCAS_CORPSE_TEX_H);
    v = (TextureVertex *)sceGuGetMemory(2 * sizeof(TextureVertex));
    v[0].u = 1.0f; v[0].v = 1.0f;
    v[0].x = foot_x - (float)LUCAS_CORPSE_SRC_W * 0.5f;
    v[0].y = foot_y - (float)LUCAS_CORPSE_SRC_H;
    v[0].z = 0.0f;
    v[1].u = 1.0f + (float)LUCAS_CORPSE_SRC_W;
    v[1].v = 1.0f + (float)LUCAS_CORPSE_SRC_H;
    v[1].x = foot_x + (float)LUCAS_CORPSE_SRC_W * 0.5f;
    v[1].y = foot_y;
    v[1].z = 0.0f;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
                   2, NULL, v);
}


static void repair_map005_key_state(const MapState *m)
{
    if (!m || m->id != 5) return;

    /* 0.7.4: one-time legacy-save repair.  Builds where the Map005 key was
     * invisible could still let Event 7 run, leaving switch 144 + item 52 set
     * even though the player never saw the pickup.  For users upgrading from
     * those builds, restore the original pre-pickup state once while the
     * Processing Plant shutter has not yet opened.  The key then becomes both
     * visible and normally actionable again. */
    if (!g_map5_key_migration_done) {
        if (!g_switches[16]) {
            g_switches[144] = 0;
            g_items[52] = 0;
        }
        g_map5_key_migration_done = 1;
        save_config();
    }

    /* Also repair the narrower inconsistent state for any future save: a
     * pickup switch without the actual Red Key item cannot be valid before
     * the Processing Plant door has opened. */
    if (!g_switches[16] && g_switches[144] && g_items[52] <= 0)
        g_switches[144] = 0;
}

static void vm_begin_screen_flash(int r, int g, int b, int a, int frames);

/* Map026 parallel event3 and sensor event4's autonomous turn route.
 * MZ frequency5 waits until stopCount > 0; route-end consumes one extra
 * update without resetting stopCount before the next downward turn. */
static void tick_laser_room(const MapState *m)
{
    if (!m || m->id != 26 || g_switches[137]) {
        g_laser_room_frame = g_laser_stop_count = g_laser_route_index = 0;
        return;
    }
    if (++g_laser_room_frame >= 500) {
        play_vm_se_params(1031,50,80,0);
        g_switches[134] = g_switches[135] = g_switches[136] = g_switches[137] = 1;
        g_event_direction[4] = 0;
        return;
    }
    if (!g_routes[4].records && ++g_laser_stop_count > 0) {
        if (g_laser_route_index < 4) {
            g_event_direction[4] = g_laser_route_index++;
            g_laser_stop_count = 0;
        } else g_laser_route_index = 0;
    }
}

static void tick_pressing_room(const MapState *m)
{
    int phase = pressing_room_tick(&g_pressing_room_frame,
                                   m && m->id == 24 && !g_switches[191]);
    if (phase == 1) { g_switches[184] = 0; g_switches[183] = 1; }
    else if (phase == 2) {
        vm_begin_screen_flash(255,0,0,255,5);
        g_switches[183] = 0; g_switches[184] = 1;
        play_vm_se_params(1133,90,100,0);
    } else if (phase == 3) { g_switches[184] = 0; g_switches[183] = 0; }
}

/* Map032 parallel pages 19/20. Their waits and routes run while the
 * player keeps moving; running them through the blocking scene VM loses input. */
static void tick_factory_parallel(const MapState *m)
{
    if(m->id!=32)return;
    if(g_switches[237]) {
        int f=++g_factory_machine_frame;
        if(f==300)play_vm_se_params(1059,90,50,0);
        if(f>=300 && f<=420) {
            int phase=((f-300)/10)&1;
            g_switches[233]=g_switches[235]=!phase;
            g_switches[234]=g_switches[236]=phase;
        }
        if(f>=430) {
            g_switches[233]=g_switches[234]=g_switches[235]=g_switches[236]=0;
            g_switches[237]=0;play_vm_se_params(1077,90,100,0);
            /* Original Set Event Location: event 19 returns to (14,16). */
            free(g_routes[19].records);memset(&g_routes[19],0,sizeof(EventRoute));
            g_event_shift_x[19]=g_event_shift_y[19]=0;
            g_switches[257]=0;g_factory_machine_frame=0;g_factory_corpse_phase=0;
        }
    } else g_factory_machine_frame=0;
    if(g_switches[256] && !g_factory_corpse_phase) {
        static const uint8_t down[]={37,0,0,0,0,1,0,0,0,0,1,0,0,0,0,38,0,0,0,0};
        start_event_route(m,19,down,4,0);g_factory_corpse_phase=1;
    }
    if(g_factory_corpse_phase==1 && !g_routes[19].records) {
        play_vm_se_params(1134,90,70,0);vm_begin_screen_flash(255,0,0,255,10);
        g_switches[256]=0;g_switches[258]=1;g_factory_corpse_phase=2;
    }
    if(g_switches[258] && g_factory_corpse_phase!=3) {
        static const uint8_t left[]={29,3,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0};
        start_event_route(m,19,left,11,0);g_factory_corpse_phase=3;
    }
    if(g_factory_corpse_phase==3 && !g_routes[19].records) {
        g_switches[258]=0;g_factory_corpse_phase=0;
    }
}

#include "stage_parallel.h"

static const char *g_scroll_text;
static size_t g_scroll_text_len;
static float g_scroll_text_y;

static void render_world(
    const MapState *m, void *char_atlas,
    void *a1_overlay, void *fall_atlas,
    int player_visible,
    float player_world_x, float player_world_y,
    int player_pattern, int player_direction,
    int fall_slot, float fall_y_tiles,
    void *portrait, void *dialog,
    int flash_red_alpha, int black_alpha, int show_blank_box)
{
    float camera_x, camera_y, origin_x, origin_y;
    float player_screen_x, player_screen_y;

    /* Snapshot movement before route ticking so its final interpolation frame
     * keeps the walking phase even when remaining becomes zero. */
    int subpixel_jump = g_player_animation_jumping;
    int had_player_route = g_player_route.records != NULL;
    int route_moving = had_player_route && g_player_route.remaining > 0 &&
        (g_player_route.dx || g_player_route.dy);
    tick_player_route(m);
    if(had_player_route) {
        player_world_x=g_route_real_x;player_world_y=g_route_real_y;
        player_direction=*g_route_player_direction;
        player_visible=g_player_visible;
        route_moving |= g_player_route.remaining > 0 &&
            (g_player_route.dx || g_player_route.dy);
        g_player_animation_speed = clamp_move_speed_code(g_player_move_speed_code);
    }
    {
        int route_arrival = route_moving && g_player_route.remaining == 0;
        player_pattern = player_animation_tick(&g_player_animation,
        g_player_animation_moving || (route_moving && !route_arrival),
        g_player_animation_jumping || route_arrival,
        g_player_animation_speed, g_player_walk_anime, g_player_step_anime);
    }
    g_player_animation_moving = g_player_animation_jumping = 0;
    repair_map005_key_state(m);
    reconcile_consumed_items();

    tick_map_scroll();
    compute_camera(
        m, player_world_x, player_world_y,
        &camera_x, &camera_y, &origin_x, &origin_y
    );
    /* MZ Tilemap.updateTransform() ceils the pixel origin before placing the
     * lower/upper tile layers.  Reproduce that rasterisation on the native
     * 480x272 PSP framebuffer instead of feeding fractional coordinates to
     * nearest-filtered map tiles. */
    /* Keep the actor anchored to the continuous camera; only tiles use the
     * integer camera origin. Otherwise following produces a one-pixel sawtooth. */
    float actor_camera_x = camera_x;
    float actor_camera_y = camera_y;
    g_actor_camera_x = actor_camera_x;
    g_actor_camera_y = actor_camera_y;
    camera_x = render_ceil_pixel(camera_x);
    camera_y = render_ceil_pixel(camera_y);
    origin_x = render_round_pixel(origin_x);
    origin_y = render_round_pixel(origin_y);
    /* Spriteset_Base.updatePosition(): Math.round(screen.shake()). */
    origin_x += render_round_pixel(g_screen_shake_x);

    /* Map 096: Will's original run ends on the last right-hand tile while
     * the interpreter continues the pursuers' scene. Treat that endpoint as
     * an exit from the cinematic frame, without changing party visibility. */
    if (m->id == 96 && player_world_x >= ((float)m->w - 0.5f) * TILE_PX)
        player_visible = 0;

    player_screen_x = origin_x + player_world_x - actor_camera_x;
    player_screen_y = origin_y + player_world_y - actor_camera_y;
    if (m->id == 1 && !subpixel_jump) {
        player_screen_x = render_floor_pixel(player_screen_x);
        player_screen_y = render_floor_pixel(player_screen_y);
    }

    tick_pictures();
    tick_pressing_room(m);
    tick_laser_room(m);
    tick_s003_autonomous(m,player_world_x,player_world_y);
    tick_factory_autonomous(m);
    tick_event_routes(m);
    tick_factory_parallel(m);
    tick_stage_parallel(m);
    vm_tick_world_tint();
    sceGuStart(GU_DIRECT, gu_list);
    sceGuClearColor(0xFF000000);
    sceGuClear(GU_COLOR_BUFFER_BIT);

    /* GU state survives the previous frame.  Start every world frame from a
     * canonical nearest/clamped state so a dialogue transition cannot expose
     * one LINEAR-filtered world frame. */
    set_pixel_art_texture_state();
    if (m->parallax_texture) {
        bind_texture_8888(m->parallax_texture,512,512);
        sceGuTexFilter(GU_LINEAR,GU_LINEAR);
        draw_bound_rect(0,0,408,312,36,g_stage_parallax_y,408,312);
        if (g_stage_parallax_scrolling) {
            draw_bound_rect(0,0,408,312,36,g_stage_parallax_y-312,408,312);
            /* Original Sy=-10 advances the displayed panorama by 1.25 PSP pixels. */
            g_stage_parallax_y+=1.25f;
            if (g_stage_parallax_y>=312) g_stage_parallax_y-=312;
        }
        set_pixel_art_texture_state();
    }
    draw_map_pass(m, 0, camera_x, camera_y, origin_x, origin_y);

    draw_event_sprites(
        m, 0, 0, player_world_y,
        camera_x, camera_y, origin_x, origin_y
    );

    if (fall_slot >= 0 && fall_atlas && m->id == 1) {
        float fall_foot_x = origin_x + ((float)FALL_EVENT_X + 0.5f) * TILE_PX - camera_x;
        float fall_foot_y = origin_y + ((fall_y_tiles + 1.0f) * TILE_PX) - camera_y;
        bind_texture_8888(fall_atlas, FALL_ATLAS_W, FALL_ATLAS_H);
        set_pixel_art_texture_state();
        draw_fall_frame(fall_slot, fall_foot_x, fall_foot_y);
    }

    draw_event_sprites(
        m, 1, -1, player_world_y,
        camera_x, camera_y, origin_x, origin_y
    );
    draw_story_key_foreground(m, actor_camera_x, actor_camera_y, origin_x, origin_y,
                              -1, player_world_y);

    if (player_visible) {
        bind_texture_8888(player_form_texture(char_atlas), CHAR_ATLAS_W, CHAR_ATLAS_H);
        set_pixel_art_texture_state();
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
        int secret_ladder_top=secret_ladder_clip_top(m->id,player_world_x,player_world_y,
                                                   TILE_PX,origin_y,actor_camera_y,SCREEN_H);
        if(secret_ladder_top>=0)sceGuScissor(0,secret_ladder_top,SCREEN_W,SCREEN_H);
        int player_opacity=cinematic_player_opacity(m->id,player_world_x,TILE_PX);
        if(player_opacity<255) {
            sceGuTexFunc(GU_TFX_MODULATE,GU_TCC_RGBA);
            sceGuColor(((unsigned int)player_opacity<<24)|0xFFFFFFu);
        }
        draw_character_frame(
            player_pattern, player_direction,
            player_screen_x, player_screen_y
        );
        if(secret_ladder_top>=0)sceGuScissor(0,0,SCREEN_W,SCREEN_H);
        if(player_opacity<255)sceGuColor(0xFFFFFFFF);
        set_pixel_art_texture_state();
    }

    draw_event_sprites(
        m, 1, 1, player_world_y,
        camera_x, camera_y, origin_x, origin_y
    );
    draw_story_key_foreground(m, actor_camera_x, actor_camera_y, origin_x, origin_y,
                              1, player_world_y);

    draw_map_pass(m, 1, camera_x, camera_y, origin_x, origin_y);

    draw_event_sprites(
        m, 2, 0, player_world_y,
        camera_x, camera_y, origin_x, origin_y
    );

    if (g_balloon_frames > 0 && g_balloon_atlas) {
        float bx=player_screen_x, by=player_screen_y-39.0f;
        int visible=player_visible, i;
        if (g_balloon_target >= 0) {
            visible=0;
            for(i=0;i<m->vm_event_count;++i) {
                VmEventRecord ev;
                if(vm_read_event(m,i,&ev) && ev.event_id==g_balloon_target && ev.event_id<MAX_EVENT_ID) {
                    bx=origin_x+(ev.x+g_event_shift_x[ev.event_id]+0.5f)*TILE_PX-actor_camera_x;
                    by=origin_y+(ev.y+g_event_shift_y[ev.event_id]+1.0f)*TILE_PX-actor_camera_y-39.0f;
                    visible=1;break;
                }
            }
        }
        if(visible) {
            int frame=(76-g_balloon_frames)/8;if(frame>7)frame=7;
            bind_texture_8888(g_balloon_atlas,512,512);
            sceGuTexFilter(GU_LINEAR,GU_LINEAR);
            draw_bound_rect(frame*50+1,(g_balloon_id-1)*50+1,48,48,bx-12,by-24,24,24);
            set_pixel_art_texture_state();
        }
        --g_balloon_frames;
    }

    /* 0.5.x atlases omitted switch-driven Lucas pages; keep the old fallback
     * only for legacy assets.  0.6.4 renders them through active VM pages. */
    if (!(m->vm_flags & 0x01u)) {
        draw_lucas_fall_event(m, camera_x, camera_y, origin_x, origin_y);
        draw_lucas_corpse_event(m, camera_x, camera_y, origin_x, origin_y);
    }

    /* Gray is MZ/Pixi ColorFilter saturation (HSL lightness), before RGB tone.
     * Only gray needs readback. Split and sync the GU list before accessing VRAM.
     * The frame is read through the cached VRAM alias: uncached reads cost a
     * full bus transaction per pixel on hardware. Drop stale cache lines before
     * reading the GE output and write the result back before the GE resumes.
     * Skipped when the tone turns every pixel black or white anyway. */
    if (g_world_tone_gray > 0 &&
        !tone_hides_gray(g_world_tone_r, g_world_tone_g, g_world_tone_b)) {
        int x,y,gray=g_world_tone_gray > 255 ? 255 : g_world_tone_gray;
        uint32_t *fb=(uint32_t *)(0x04000000u | (uintptr_t)g_draw_buffer);
        const unsigned int bytes=(unsigned int)(BUF_W*SCREEN_H*4);
        sceGuFinish();sceGuSync(0,0);
        sceKernelDcacheWritebackInvalidateRange(fb,bytes);
        for(y=0;y<SCREEN_H;++y) {
            uint32_t *row=fb+y*BUF_W;
            for(x=0;x<SCREEN_W;++x) row[x]=tone_gray_pixel(row[x],gray);
        }
        sceKernelDcacheWritebackInvalidateRange(fb,bytes);
        sceGuStart(GU_DIRECT,gu_list);
        sceGuTexFlush();
    }
    {
        int r=g_world_tone_r,g=g_world_tone_g,b=g_world_tone_b;
        /* Fixed-factor additive/subtractive passes preserve per-channel tone. */
        sceGuBlendFunc(GU_ADD,GU_FIX,GU_FIX,0xFFFFFFFF,0xFFFFFFFF);
        draw_solid_rect(0,0,SCREEN_W,SCREEN_H,r>0?r:0,g>0?g:0,b>0?b:0,255);
        sceGuBlendFunc(GU_REVERSE_SUBTRACT,GU_FIX,GU_FIX,0xFFFFFFFF,0xFFFFFFFF);
        draw_solid_rect(0,0,SCREEN_W,SCREEN_H,r<0?-r:0,g<0?-g:0,b<0?-b:0,255);
        sceGuBlendFunc(GU_ADD,GU_SRC_ALPHA,GU_ONE_MINUS_SRC_ALPHA,0,0);
    }

    draw_vm_pictures(1, m->id==114);
    {
        int veil_active = (a1_overlay != NULL) || g_picture6_visible || g_switches[20];

        /* Save files produced by several 0.6.x builds could lose switch 20 and
         * Picture #6 independently during transfers.  In the original game the
         * first Processing Plant arc keeps A1 + Fog continuously from Map001
         * through Map009; Map010 is the first place that explicitly switches
         * 20 OFF and erases pictures 6/7.  Repair that early legacy state on
         * sight instead of letting the veil randomly disappear by map. */
        if (!veil_active && m && m->id >= 2 && m->id <= 9) {
            veil_active = 1;
            g_switches[20] = 1;
            g_picture6_visible = 1;
            g_picture6_alpha = 255;
        }

        if (!a1_overlay && veil_active)
            a1_overlay = g_a1_overlay;

        if (a1_overlay) {
            int a1_alpha = 255;
            if (a1_overlay == g_a1_overlay && g_picture6_visible)
                a1_alpha = g_picture6_alpha;
            if (a1_alpha > 0) {
                draw_texture_alpha(
                    a1_overlay, PIC_TEX_W, PIC_TEX_H,
                    (float)SCREEN_W, (float)SCREEN_H,
                    0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, a1_alpha
                );
            }
        }

        /* Picture #7 in the original: animated black fog/vignette above A1. */
        draw_fog_a1_overlay(veil_active);
    }

    draw_vm_pictures(0, m->id==114);
    if (g_scroll_text) draw_utf8_wrapped(g_scroll_text,g_scroll_text_len,36,g_scroll_text_y,444,512);

    if (portrait) {
        draw_texture_alpha(
            portrait, PIC_TEX_W, PIC_TEX_H,
            (float)SCREEN_W, (float)SCREEN_H,
            0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H, 255
        );
    }

    if (show_blank_box) {
        draw_solid_rect(
            6.0f, (float)(SCREEN_H - DIALOG_VISIBLE_H + 4),
            (float)(SCREEN_W - 12), (float)(DIALOG_VISIBLE_H - 8),
            10, 10, 14, 220
        );
    }

    if (dialog) {
        draw_texture_alpha(
            dialog, DIALOG_TEX_W, DIALOG_TEX_H,
            (float)SCREEN_W, (float)DIALOG_VISIBLE_H,
            0.0f, (float)(SCREEN_H - DIALOG_VISIBLE_H),
            (float)SCREEN_W, (float)DIALOG_VISIBLE_H, 255
        );
    }

    draw_runtime_message_overlay();
    draw_runtime_choice_overlay();
    draw_runtime_number_overlay();

    if (flash_red_alpha > 0) {
        draw_solid_rect(
            0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H,
            255, 0, 0, flash_red_alpha
        );
    }

    if (g_screen_flash_remaining > 0 && g_screen_flash_total > 0 &&
        g_screen_flash_a > 0) {
        int fa = g_screen_flash_a * g_screen_flash_remaining / g_screen_flash_total;
        if (fa > 0)
            draw_solid_rect(
                0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H,
                g_screen_flash_r, g_screen_flash_g, g_screen_flash_b, fa
            );
    }

    if (g_script_fade_alpha>black_alpha) black_alpha=g_script_fade_alpha;
    if (black_alpha > 0) {
        draw_solid_rect(
            0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H,
            g_transfer_white?255:0,g_transfer_white?255:0,g_transfer_white?255:0,black_alpha
        );
    }

    perf_hud_draw();
    sceGuFinish();
    sceGuSync(0, 0);
    perf_frame_done();
    sceDisplayWaitVblankStart();
    perf_after_vblank();
    g_last_world_buffer = g_draw_buffer;
    g_last_world_buffer_valid = 1;
    g_draw_buffer = sceGuSwapBuffers();
    ++g_world_render_frames;
    ++g_playtime_frames;
    if (g_screen_flash_remaining > 0) {
        --g_screen_flash_remaining;
        if (g_screen_flash_remaining <= 0) {
            g_screen_flash_remaining = 0;
            g_screen_flash_total = 0;
        }
    }
}
