/* Camera, map tiles, event sprites and pictures.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* World rendering                                                           */
/* ------------------------------------------------------------------------- */

#include "event_tile_passage.h"

static void vm_begin_map_scroll(int direction,int distance,int speed)
{
    if(speed<1)speed=1;
    if(speed>6)speed=6;
    g_map_scroll_start_x=g_camera_scroll_x;g_map_scroll_start_y=g_camera_scroll_y;
    g_map_scroll_target_x=g_camera_scroll_x;g_map_scroll_target_y=g_camera_scroll_y;
    if(direction==2)g_map_scroll_target_y+=distance*TILE_PX;
    else if(direction==4)g_map_scroll_target_x-=distance*TILE_PX;
    else if(direction==6)g_map_scroll_target_x+=distance*TILE_PX;
    else if(direction==8)g_map_scroll_target_y-=distance*TILE_PX;
    g_map_scroll_total=distance*256/(1<<speed);
    if(g_map_scroll_total<1)g_map_scroll_total=1;
    g_map_scroll_remaining=g_map_scroll_total;
}
static void tick_map_scroll(void)
{
    float t;
    if(g_map_scroll_remaining<=0)return;
    --g_map_scroll_remaining;
    t=(float)(g_map_scroll_total-g_map_scroll_remaining)/g_map_scroll_total;
    g_camera_scroll_x=g_map_scroll_start_x+(g_map_scroll_target_x-g_map_scroll_start_x)*t;
    g_camera_scroll_y=g_map_scroll_start_y+(g_map_scroll_target_y-g_map_scroll_start_y)*t;
}

static void compute_camera(
    const MapState *m, float player_x, float player_y,
    float *camera_x, float *camera_y, float *origin_x, float *origin_y)
{
    float map_w = (float)(m->w * TILE_PX);
    float map_h = (float)(m->h * TILE_PX);
    float max_x = map_w > SCREEN_W ? map_w - SCREEN_W : 0.0f;
    float max_y = map_h > SCREEN_H ? map_h - SCREEN_H : 0.0f;

    *origin_x = map_w < SCREEN_W ? (SCREEN_W - map_w) * 0.5f : 0.0f;
    *origin_y = map_h < SCREEN_H ? (SCREEN_H - map_h) * 0.5f : 0.0f;

    /* These rooms have asymmetric empty map margins. Centre their visible
     * geometry without changing event coordinates, collision or vertical scroll. */
    if (m->id == 11 || m->id == 21 || m->id == 23 || m->id == 27) *origin_x += TILE_PX;
    if (m->id == 13) {
        *origin_x = TILE_PX * 0.5f;
        max_x = 0.0f;
    }

    /* These cinematic maps stage their actors in the right-hand part of
     * the source map. Frame the action rather than Will's offscreen position. */
    if (m->id == 98 || m->id == 99) {
        *origin_x = SCREEN_W * 0.5f - (m->id == 98 ? 13.5f : 15.5f) * TILE_PX;
        if (m->id == 98) *origin_x += TILE_PX; /* Keep the rooftop shot right of centre. */
    }

    if (g_camera_locked) {
        *camera_x = clamp_float(g_camera_lock_x + g_camera_scroll_x, 0.0f, max_x);
        *camera_y = clamp_float(g_camera_lock_y + g_camera_scroll_y, 0.0f, max_y);
        return;
    }

    *camera_x = max_x > 0.0f
        ? clamp_float(player_x - SCREEN_W * 0.5f + g_camera_scroll_x, 0.0f, max_x)
        : 0.0f;
    *camera_y = max_y > 0.0f
        ? clamp_float(player_y - SCREEN_H * 0.5f + g_camera_scroll_y, 0.0f, max_y)
        : 0.0f;

    /* Map 098 starts with hidden Will at the bottom map boundary. Scroll
     * from the bounded initial viewport, not his offscreen centre: otherwise
     * the 10-tile pan leaves the rooftop actor clipped at the top edge. */
    if (m->id == 98 && !g_camera_locked)
        /* The visible roof starts at source row 6. Stop the upward pan
         * there instead of exposing the map's empty upper margin. */
        *camera_y = clamp_float(clamp_float(player_y - SCREEN_H * 0.5f,
            0.0f, max_y) + g_camera_scroll_y, 6.0f * TILE_PX, max_y);

}

static void draw_character_frame(
    int pattern, int direction_row, float foot_x, float foot_y)
{
    int sx, sy;
    if (pattern < 0 || pattern > 2) pattern = 1;
    if (direction_row < 0 || direction_row > 3) direction_row = 0;
    sx = pattern * CHAR_SLOT_W + 1;
    sy = direction_row * CHAR_SLOT_H + 1;
    /* MZ Game_CharacterBase.screenX/screenY use Math.floor(). */
    draw_bound_rect(
        (float)sx, (float)sy, (float)CHAR_SRC_W, (float)CHAR_SRC_H,
        foot_x - CHAR_DST_W * 0.5f,
        foot_y - CHAR_DST_H,
        (float)CHAR_DST_W, (float)CHAR_DST_H
    );
}

/* First and last tile index that can touch the screen along one axis.  The
 * range is one tile wider on each side; callers keep their exact checks. */
static void visible_tile_range(float origin, float camera, int screen, int count,
                               int *first, int *last)
{
    float start = (camera - origin) / (float)TILE_PX;
    int a = (int)floorf(start) - 1;
    int b = (int)floorf(start + (float)screen / (float)TILE_PX) + 1;
    if (a < 0) a = 0;
    if (b > count - 1) b = count - 1;
    *first = a;
    *last = b;
}

static void draw_map_pass(
    const MapState *m, int upper,
    float camera_x, float camera_y, float origin_x, float origin_y)
{
    enum { MAP_BATCH_TILES = 64 };
    TextureVertex batch[MAP_BATCH_TILES * 2];
    int batch_tiles = 0;
    int y;
    int x0, x1, y0, y1;
    int bound_atlas = -1;
    int bound_filter = -1;

    /* Map rooms are made from hundreds of tile quads.  Sending each tile as
     * its own GU draw call costs more CPU time than filling a small vertex
     * batch. Keep traversal order and flush whenever atlas/filter state
     * changes, so transparency and pixel-art sampling stay identical. */
#define FLUSH_MAP_BATCH() do { \
        if (batch_tiles > 0) { \
            TextureVertex *vertices = (TextureVertex *)sceGuGetMemory( \
                (size_t)batch_tiles * 2u * sizeof(TextureVertex)); \
            if (vertices) { \
                memcpy(vertices, batch, (size_t)batch_tiles * 2u * sizeof(TextureVertex)); \
                sceGuDrawArray(GU_SPRITES, \
                    GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_TRANSFORM_2D, \
                    batch_tiles * 2, NULL, vertices); \
            } \
            batch_tiles = 0; \
        } \
    } while (0)

    /* Never let Fog A1 / monitor / fitted-picture GU_LINEAR state bleed into
     * map-atlas sampling.  The atlases have transparent gutters, so LINEAR for
     * even one frame creates a visible contour around bricks and sprites. */
    set_pixel_art_texture_state();

    /* Walk only the tiles around the screen instead of the whole map. */
    visible_tile_range(origin_x, camera_x, SCREEN_W, m->w, &x0, &x1);
    visible_tile_range(origin_y, camera_y, SCREEN_H, m->h, &y0, &y1);
    for (y = y0; y <= y1; ++y) {
        float dy = origin_y + (float)(y * TILE_PX) - camera_y;
        int x;
        if (dy <= -TILE_PX || dy >= SCREEN_H) continue;

        for (x = x0; x <= x1; ++x) {
            float dx = origin_x + (float)(x * TILE_PX) - camera_x;
            int z;
            if (dx <= -TILE_PX || dx >= SCREEN_W) continue;

            for (z = 0; z < 4; ++z) {
                uint16_t word = map_word(m, z, x, y);
                uint16_t global_slot;
                int is_upper;
                int atlas_index;
                uint16_t local_slot;
                int filter;

                if (word == 0) continue;
                is_upper = (word & 0x8000) != 0;
                if (is_upper != upper) continue;

                global_slot = word & 0x3FFF;
                atlas_index = ((int)global_slot - 1) / 100;
                local_slot = (uint16_t)(((global_slot - 1) % 100) + 1);
                if (atlas_index < 0 || atlas_index >= m->tile_atlas_count) continue;
                filter = (word & 0x4000) ? GU_LINEAR : GU_NEAREST;

                if (bound_atlas != atlas_index || bound_filter != filter) {
                    FLUSH_MAP_BATCH();
                }
                if (bound_atlas != atlas_index) {
                    bind_texture_8888(
                        m->tile_atlases[atlas_index],
                        TILE_ATLAS_W, TILE_ATLAS_H
                    );
                    bound_atlas = atlas_index;
                }
                if (bound_filter != filter) {
                    sceGuTexFilter(filter, filter);
                    bound_filter = filter;
                }

                {
                    int index = (int)local_slot - 1;
                    int sx = (index % TILE_ATLAS_COLS) * TILE_SLOT + 1;
                    int sy = (index / TILE_ATLAS_COLS) * TILE_SLOT + 1;
                    TextureVertex *v = &batch[batch_tiles * 2];
                    v[0].u = (float)sx; v[0].v = (float)sy;
                    v[0].x = dx; v[0].y = dy; v[0].z = 0.0f;
                    v[1].u = (float)(sx + TILE_SOURCE);
                    v[1].v = (float)(sy + TILE_SOURCE);
                    v[1].x = dx + TILE_PX; v[1].y = dy + TILE_PX; v[1].z = 0.0f;
                }
                if (++batch_tiles >= MAP_BATCH_TILES) FLUSH_MAP_BATCH();
            }
        }
    }
    FLUSH_MAP_BATCH();
#undef FLUSH_MAP_BATCH
}

/* Crowded source sheets use one detail page, freed on map transfer. */
static void *g_event_texture_base,*g_event_texture_detail,*g_event_texture_bound;
static unsigned int g_ambush_reveal_start;
static int g_ambush_reveal_started;

/* Short visual reveal only; the original route and collision stay active. */
static int ambush_reveal_opacity(int map_id, int event_id, unsigned int frame)
{
    unsigned int age;
    if (map_id != 34 || event_id != 51) return 255;
    if (!g_ambush_reveal_started) {
        g_ambush_reveal_started = 1;
        g_ambush_reveal_start = frame;
    }
    age = frame - g_ambush_reveal_start;
    return age >= 6 ? 255 : (int)((age + 1) * 255 / 7);
}

static void draw_event_sprite_record(
    const EventSpriteRecord *sp, int map_id,
    float camera_x, float camera_y, float origin_x, float origin_y)
{
    float shift_x = sp->event_id >= 0 && sp->event_id < MAX_EVENT_ID
        ? g_event_shift_x[sp->event_id] : 0.0f;
    float shift_y = sp->event_id >= 0 && sp->event_id < MAX_EVENT_ID
        ? g_event_shift_y[sp->event_id] : 0.0f;
    /* At native PSP scale the slow fall moves only 0.375 px/frame.
     * Preserve its fractional position with isolated high-resolution frames;
     * all other pixel-art sprites keep their established integer alignment. */
    int smooth_fall = (map_id == 109 && sp->event_id == 3) || (map_id == 1 && sp->event_id == 5 && g_routes[5].remaining > 0) ||
                      (map_id == 16 && sp->event_id == 36) ||
                      (map_id == 12 && (sp->flags & 0x04)) || (map_id == 30 && (sp->flags & 0x04)) || (map_id == 39 && (sp->flags & 0x04)) ||
                      is_chase_enemy(map_id,sp->event_id) || is_spider_enemy(map_id,sp->event_id) || (map_id==32 && sp->event_id==19) || ((map_id==69 || map_id==72 || map_id==78) && g_routes[sp->event_id].remaining>0);
    if ((map_id == 1 && sp->event_id == 5) || (map_id == 16 && sp->event_id == 36)) {
        camera_x = g_actor_camera_x;
        camera_y = g_actor_camera_y;
    }
    float foot_x = origin_x + ((float)sp->x + shift_x + 0.5f) * TILE_PX - camera_x;
    float foot_y = origin_y + ((float)sp->y + shift_y + 1.0f) * TILE_PX - camera_y;
    if(sp->event_id>=0 && sp->event_id<MAX_EVENT_ID)foot_y-=g_event_jump_height[sp->event_id];
    if (!smooth_fall) {
        foot_x = render_floor_pixel(foot_x);
        foot_y = render_floor_pixel(foot_y);
    }
    void *texture=(sp->sx>>12) ? g_event_texture_detail : g_event_texture_base;
    if(texture && texture!=g_event_texture_bound) {
        bind_texture_8888(texture,EVENT_ATLAS_W,EVENT_ATLAS_H);
        g_event_texture_bound=texture;
    }
    float src_x = (float)(sp->sx & 0x0FFF);
    float src_y = (float)sp->sy;
    if((sp->flags&0x40) && sp->event_id>=0 && sp->event_id<MAX_EVENT_ID && g_event_direction[sp->event_id]>=0) {
        int row=g_event_direction[sp->event_id];
        if ((sp->flags & 0xC4) == 0xC4 && map_id!=94) {
            src_x+=(row&1)*3*sp->sw;
            src_y+=(row>>1)*sp->sh;
        } else if((map_id==43 || (((map_id>=48 && map_id<=104) || (map_id>=111 && map_id<=139)) || map_id==115)) && (sp->flags&4) && !(sp->flags&0x80)) {
            src_x+=(row&1)*sp->sw;src_y+=(row>>1)*sp->sh;
        } else src_y+=row*sp->sh;
    }
    float dst_w = (float)sp->sw;
    float dst_h = (float)sp->sh;
    float shift = (sp->flags & 0x01) ? 0.0f : 3.0f;

    /* Sprite flag bit 1 means that the atlas stores the three RPG Maker
     * patterns horizontally.  Step animation uses the engine's standing
     * cadence: animationWait=(9-moveSpeed)*3 and visual sequence
     * 1 -> 2 -> 1 -> 0.  This restores animated SAVE arrows, key monitors,
     * keys, etc. instead of baking a single middle frame forever. */
    if (sp->flags & 0x80) {
        int id = sp->event_id;
        int pattern = id > 0 && id < MAX_EVENT_ID
            ? g_event_animation[id].pattern : 1;
        if (pattern >= 3) pattern = 1;
        /* Map035's escape scene forces seven up steps and thirteen right
         * steps at speed5 (one tile per eight frames). Keep the visible gait
         * tied to those actual steps even during blocking scene playback.
         * Pauses and look-around commands retain the regular idle state. */
        if(map_id==35 && id==7 && g_routes[id].records &&
           (g_routes[id].dx || g_routes[id].dy)) {
            static const int gait[4]={1,2,1,0};
            float x=g_event_shift_x[id],y=g_event_shift_y[id];
            int steps=(int)((x<0?-x:x)+(y<0?-y:y));
            pattern=gait[steps&3];
        }
        if(map_id==72 && id==2 && !(sp->flags&0x40)) {
            /* Giant spider: three native poses in a 2x2 atlas block. */
            src_x+=(pattern&1)*sp->sw;src_y+=(pattern>>1)*sp->sh;
        } else src_x += pattern * sp->sw;
    }
    if (sp->flags & 0x02) {
        static const int sequence[4] = {1, 2, 1, 0};
        int speed = (sp->flags >> 3) & 0x07;
        int wait_frames;
        int phase;
        if (speed < 1) speed = 3;
        if (speed > 6) speed = 6;
        wait_frames = (9 - speed) * 3;
        if (wait_frames < 1) wait_frames = 1;
        phase = (int)((g_world_render_frames / (unsigned int)wait_frames) & 3u);
        src_x += (float)(sequence[phase] * sp->sw);
    }

    /* Flag bit 2 keeps a wide/detail-heavy source sprite at original PC
     * resolution in the atlas, but renders it at PSP half size.  Hardware
     * linear filtering preserves the tiny monitor lettering much better than
     * throwing away every other pixel during conversion. */
    if (sp->flags & 0x04) {
        float scale = (((map_id==21 || map_id==23 || map_id==27) && sp->sw==36 && sp->sh==144) || ((map_id==27 || (((map_id>=32 && map_id<=104) || (map_id>=111 && map_id<=139)) || map_id==115)) && sp->sw==54 && sp->sh==108) || (map_id==40 && sp->sw==108 && sp->sh==324)) ? (2.0f/3.0f) : 0.5f;
        dst_w *= scale;
        dst_h *= scale;
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    } else {
        sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    }


    if (map_id == 16 || ((map_id == 24 || map_id == 25) && sp->event_id == 3))
        sceGuTexFilter(GU_LINEAR, GU_LINEAR);

    {
        int opacity = ambush_reveal_opacity(map_id, sp->event_id, g_world_render_frames);
        /* The rooftop withdrawal finishes on the last right-hand tile.
         * Hide its actor at that endpoint while the original fade continues. */
        if (map_id == 98 && sp->event_id == 1 &&
            sp->x + g_event_shift_x[1] >= 17.0f)
            opacity = 0;

        /* Dealers leave the cinematic frame at the original viewport edge.
         * Keep their forced routes intact; this affects only presentation. */
        if (map_id == 94 && (sp->event_id == 11 || sp->event_id == 12)) {
            float cell_x = sp->x + g_event_shift_x[sp->event_id];
            if (cell_x > 17.0f) opacity = (int)(opacity * clamp_float(18.0f-cell_x,0.0f,1.0f));
        }
        if (sp->event_id >= 0 && sp->event_id < MAX_EVENT_ID)
            opacity = opacity * g_event_opacity[sp->event_id] / 255;
        if (opacity != 255) {
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            sceGuColor(((unsigned int)opacity << 24) | 0xFFFFFFu);
        }
    }
    draw_bound_rect(
        src_x, src_y, (float)sp->sw, (float)sp->sh,
        foot_x - dst_w * 0.5f,
        foot_y - shift - dst_h,
        dst_w, dst_h
    );

    set_pixel_art_texture_state();
}

static int is_dedicated_story_key_event(int map_id, int event_id)
{
    return (map_id == 5 && event_id == 7) ||
           (map_id == 15 && event_id == 12) ||
           (map_id == 19 && event_id == 2);
}

/* MZ sorts equal-priority sprites by foot Y. This keeps moving characters
 * behind foreground tentacles and corpse barricades at the correct depth. */
static void draw_tentacle_room_sprites(
    const MapState *m, int priority, int side, float player_y,
    float camera_x, float camera_y, float origin_x, float origin_y)
{
    EventSpriteRecord sprites[MAX_EVENT_ID];
    float ys[MAX_EVENT_ID];
    int count=0,i;
    const float player_sort_y=player_y-3.0f;
    for(i=0;i<m->vm_event_count && count<MAX_EVENT_ID;++i) {
        VmEventRecord ev;VmPageRecord pg;EventSpriteRecord sp;
        int active,j;float y;
        if(!vm_read_event(m,i,&ev) || !vm_active_page_index(m,&ev,&pg,&active)) continue;
        if(pg.priority!=priority || !pg.sprite_ref || !read_sprite_record(m,pg.sprite_ref-1,&sp)) continue;
        if(ev.event_id>0 && ev.event_id<MAX_EVENT_ID) {
            int id=ev.event_id;
            if(g_event_active_page[id]!=active) {
                g_event_active_page[id]=active;
                g_event_direction[id]=(pg.flags>>5)&3;
                g_event_move_speed[id]=(pg.flags>>2)&7;
                g_event_direction_fix[id]=!!(pg.flags&0x80);
                g_event_through[id]=!!(pg.flags&1);
                g_event_opacity[id]=255;
                player_animation_reset(&g_event_animation[id]);
            }
        }
        y=(ev.y+1.0f+(ev.event_id<MAX_EVENT_ID?g_event_shift_y[ev.event_id]:0))*TILE_PX;
        if(ev.event_id>=0 && ev.event_id<MAX_EVENT_ID)y-=g_event_jump_height[ev.event_id];
        y-=(sp.flags&1)?0.0f:3.0f;
        if(priority==1 && ((side<0 && y>=player_sort_y)||(side>0 && y<player_sort_y))) continue;
        j=count;
        while(j>0 && (ys[j-1]>y || (ys[j-1]==y && sprites[j-1].event_id>sp.event_id))) {
            sprites[j]=sprites[j-1];ys[j]=ys[j-1];--j;
        }
        sprites[j]=sp;ys[j]=y;++count;
    }
    for(i=0;i<count;++i)
        draw_event_sprite_record(&sprites[i],m->id,camera_x,camera_y,origin_x,origin_y);
}

static void draw_event_sprites(
    const MapState *m, int priority, int side_of_player,
    float player_world_y,
    float camera_x, float camera_y, float origin_x, float origin_y)
{
    int i;
    if (!m->event_atlas || m->sprite_count <= 0) return;

    g_event_texture_base=m->event_atlas;g_event_texture_detail=m->event_atlas_detail;
    bind_texture_8888(m->event_atlas, EVENT_ATLAS_W, EVENT_ATLAS_H);
    g_event_texture_bound=m->event_atlas;
    if((m->id==16 || m->id==30 || (((m->id>=32 && m->id<=104) || (m->id>=111 && m->id<=139)) || m->id==115)) && (m->vm_flags&1) && m->vm_bin) {
        draw_tentacle_room_sprites(m,priority,side_of_player,player_world_y,
                                  camera_x,camera_y,origin_x,origin_y);
        return;
    }

    /* 0.6.4 atlases contain a sprite record for every visual event page and
     * each VM page points at its matching record.  This is essential for the
     * original switch-driven cutscenes: falling Enri/Lucas, corpses, the meat
     * wall, key shutters, etc.  Older assets keep the static 0.5.x path. */
    if ((m->vm_flags & 0x01u) && m->vm_bin) {
        for (i = 0; i < m->vm_event_count; ++i) {
            VmEventRecord ev;
            VmPageRecord pg;
            EventSpriteRecord sp;
            float event_y;
            int sprite_index;
            int active_index;

            if (!vm_read_event(m, i, &ev)) continue;
            if (g_key_anim && is_dedicated_story_key_event(m->id, ev.event_id)) continue;
            if (!vm_active_page_index(m, &ev, &pg, &active_index)) continue;
            if ((m->id == 12 || m->id == 24 || m->id == 25 || m->id == 27 || m->id == 30 || (m->id >= 32 && m->id <= 55)) && ev.event_id > 0 && ev.event_id < MAX_EVENT_ID &&
                g_event_active_page[ev.event_id] != active_index) {
                g_event_active_page[ev.event_id] = active_index;
                g_event_direction[ev.event_id] = (pg.flags >> 5) & 3;
                g_event_move_speed[ev.event_id] = (pg.flags >> 2) & 7;
                g_event_direction_fix[ev.event_id] = !!(pg.flags & 0x80);
                g_event_through[ev.event_id] = !!(pg.flags & 1);
                g_event_opacity[ev.event_id] = 255;
                player_animation_reset(&g_event_animation[ev.event_id]);
            }
            if (pg.sprite_ref <= 0) continue;
            if (pg.priority != priority) continue;

            sprite_index = pg.sprite_ref - 1;
            if (!read_sprite_record(m, sprite_index, &sp)) continue;
            if(ev.event_id>=0 && ev.event_id<MAX_EVENT_ID && g_event_direction[ev.event_id]<0)
                g_event_direction[ev.event_id]=(pg.flags>>5)&3;

            if (priority == 1 && side_of_player != 0) {
                event_y = ((float)ev.y + 1.0f +
                    (ev.event_id < MAX_EVENT_ID ? g_event_shift_y[ev.event_id] : 0.0f)) * TILE_PX;
                if (side_of_player < 0 && event_y > player_world_y) continue;
                if (side_of_player > 0 && event_y <= player_world_y) continue;
            }

            draw_event_sprite_record(&sp, m->id, camera_x, camera_y, origin_x, origin_y);
        }
        return;
    }

    for (i = 0; i < m->sprite_count; ++i) {
        EventSpriteRecord sp;
        float event_y;
        if (!read_sprite_record(m, i, &sp)) continue;
        if (g_key_anim && is_dedicated_story_key_event(m->id, sp.event_id)) continue;
        if (sp.priority != priority) continue;

        if (priority == 1 && side_of_player != 0) {
            event_y = ((float)sp.y + 1.0f +
                (sp.event_id < MAX_EVENT_ID ? g_event_shift_y[sp.event_id] : 0)) * TILE_PX;
            if (side_of_player < 0 && event_y > player_world_y) continue;
            if (side_of_player > 0 && event_y <= player_world_y) continue;
        }

        draw_event_sprite_record(&sp, m->id, camera_x, camera_y, origin_x, origin_y);
    }
}

/* Dedicated early key rendering.
 *
 * The original !鍵 sheet uses direction rows as key colours.  Several PSP
 * builds had correct VM state but the tiny page sprite still vanished behind
 * the early map/event-atlas path.  0.7.4 keeps the original three animation
 * frames in a tiny dedicated texture and draws these story keys after the
 * upper tile pass.  Switches remain authoritative, so collected keys vanish. */
static void draw_story_key_foreground(
    const MapState *m,
    float camera_x, float camera_y, float origin_x, float origin_y,
    int side_of_player, float player_world_y)
{
    static const int sequence[4] = {1, 2, 1, 0};
    int row = -1;
    int tx = 0, ty = 0;
    int speed = 3;
    int wait_frames, phase;
    float foot_x, foot_y;

    if (!m || !g_key_anim) return;

    if (m->id == 5 && !g_switches[144]) {
        row = 0; tx = 2; ty = 3;      /* Red Key */
    } else if (m->id == 15 && !g_switches[142]) {
        row = 1; tx = 2; ty = 4;      /* Iron Key */
    } else if (m->id == 19 && !g_switches[143]) {
        row = 2; tx = 6; ty = 2;      /* Green Key */
    } else {
        return;
    }

    if (side_of_player < 0 && (ty + 1.0f) * TILE_PX > player_world_y) return;
    if (side_of_player > 0 && (ty + 1.0f) * TILE_PX <= player_world_y) return;
    wait_frames = (9 - speed) * 3;
    phase = (int)((g_world_render_frames / (unsigned int)wait_frames) & 3u);
    foot_x = origin_x + ((float)tx + 0.5f) * TILE_PX - camera_x;
    foot_y = origin_y + ((float)ty + 1.0f) * TILE_PX - camera_y;

    bind_texture_8888(g_key_anim, KEY_ANIM_W, KEY_ANIM_H);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_NEAREST, GU_NEAREST);
    sceGuColor(0xFFFFFFFF);
    draw_bound_rect(
        (float)(sequence[phase] * 32), (float)(row * 32), 24.0f, 24.0f,
        /* !-prefixed RPG Maker object characters use no 6px foot offset. */
        foot_x - 12.0f, foot_y - 24.0f, 24.0f, 24.0f
    );
}

static PictureSlot *picture_slot(int number)
{
    int i;
    PictureSlot *empty = NULL;
    if (number < 1 || number > 100) return NULL;
    for (i = 0; i < MAX_ACTIVE_PICTURES; ++i) {
        if (g_pictures[i].number == number) return &g_pictures[i];
        if (!g_pictures[i].number) empty = &g_pictures[i];
    }
    return empty;
}

/* Pictures shown again (dialogue portraits, menu pages) come from memory
 * instead of a 512 KB memory-stick read that held the frame for ~70 ms.
 * The cache is small and emptied on every map load, so it adds at most
 * PICTURE_CACHE_SLOTS x 512 KB to a map's memory use. */
#define PICTURE_CACHE_SLOTS 4
typedef struct {
    int key;                      /* resource id, negative for the Russian controls page */
    void *texture;
    unsigned char header[8], portrait[12];
    unsigned age;
} PictureCacheEntry;
static PictureCacheEntry g_picture_cache[PICTURE_CACHE_SLOTS];
static unsigned g_picture_cache_clock;

/* Prefetch: while a message waits for the player, a low-priority thread
 * reads the next picture the script will show (vm_scan_next_picture), so
 * the next Show Picture finds it in the cache. Only the main thread
 * allocates, frees and touches the cache; the loader only reads into the
 * buffer it was given and then publishes the state. */
enum { PREFETCH_IDLE, PREFETCH_LOADING, PREFETCH_READY, PREFETCH_FAILED };
static volatile int g_prefetch_state = PREFETCH_IDLE;
static int g_prefetch_key;
static void *g_prefetch_texture;
static unsigned char g_prefetch_header[8], g_prefetch_portrait[12];
static char g_prefetch_path[180];
static int g_prefetch_queue[2];

static int picture_header_ok(const unsigned char *header)
{
    return (memcmp(header,"NP50",4) == 0 || memcmp(header,"NGO1",4) == 0 || memcmp(header,"NPR1",4) == 0) &&
           read_u16_le(header+4) >= 1 && read_u16_le(header+6) >= 1 &&
           read_u16_le(header+4) <= 512 && read_u16_le(header+6) <= 512;
}

static int picture_prefetch_thread(SceSize args, void *argp)
{
    FILE *fp = fopen(g_prefetch_path, "rb");
    int ok = 0;
    (void)args; (void)argp;
    memset(g_prefetch_portrait, 0, sizeof(g_prefetch_portrait));
    if (fp) {
        ok = fread(g_prefetch_header,1,8,fp) == 8 && picture_header_ok(g_prefetch_header) &&
             (memcmp(g_prefetch_header,"NPR1",4) != 0 || fread(g_prefetch_portrait,1,12,fp) == 12) &&
             read_chunked(g_prefetch_texture,512*512*2,fp) == 512*512*2;
        fclose(fp);
    }
    g_prefetch_state = ok ? PREFETCH_READY : PREFETCH_FAILED;
    sceKernelExitDeleteThread(0);
    return 0;
}

static void picture_cache_store(int key, void *texture, const unsigned char *header, const unsigned char *portrait);

/* Main thread: take a finished prefetch into the cache. With wait, first
 * let a running one finish. */
static void picture_prefetch_collect(int wait)
{
    while (wait && g_prefetch_state == PREFETCH_LOADING) sceKernelDelayThread(1000);
    if (g_prefetch_state == PREFETCH_READY) {
        sceKernelDcacheWritebackInvalidateAll();
        picture_cache_store(g_prefetch_key, g_prefetch_texture, g_prefetch_header, g_prefetch_portrait);
        g_prefetch_texture = NULL;
        g_prefetch_state = PREFETCH_IDLE;
    } else if (g_prefetch_state == PREFETCH_FAILED) {
        free(g_prefetch_texture);
        g_prefetch_texture = NULL;
        g_prefetch_state = PREFETCH_IDLE;
    }
}

static void picture_cache_clear(void)
{
    int i;
    picture_prefetch_collect(1);
    g_prefetch_queue[0] = g_prefetch_queue[1] = 0;
    for (i = 0; i < PICTURE_CACHE_SLOTS; ++i) {
        free(g_picture_cache[i].texture);
        memset(&g_picture_cache[i], 0, sizeof(g_picture_cache[i]));
    }
}

static void picture_cache_store(int key, void *texture, const unsigned char *header, const unsigned char *portrait)
{
    PictureCacheEntry *victim = &g_picture_cache[0];
    int i;
    for (i = 0; i < PICTURE_CACHE_SLOTS; ++i) {
        PictureCacheEntry *e = &g_picture_cache[i];
        if (!e->texture) { victim = e; break; }
        if (e->age < victim->age) victim = e;
    }
    free(victim->texture);
    victim->key = key;
    victim->texture = texture;
    memcpy(victim->header, header, sizeof(victim->header));
    memcpy(victim->portrait, portrait, sizeof(victim->portrait));
    victim->age = ++g_picture_cache_clock;
}

static void picture_cache_put(const PictureSlot *slot)
{
    picture_cache_store(slot->cache_key, slot->texture, slot->cache_header, slot->cache_portrait);
}

static int picture_cache_has(int key)
{
    int i;
    for (i = 0; i < PICTURE_CACHE_SLOTS; ++i)
        if (g_picture_cache[i].texture && g_picture_cache[i].key == key) return 1;
    for (i = 0; i < MAX_ACTIVE_PICTURES; ++i)
        if (g_pictures[i].number && g_pictures[i].cache_key == key) return 1;
    return 0;
}

static int picture_cache_key(int resource_id)
{
    return (g_language==4 && resource_id>=9 && resource_id<=12) ? -resource_id : resource_id;
}

/* Starts reading picture resource_id in the background unless it is shown,
 * cached or already being read. */
static void picture_prefetch(int resource_id)
{
    int key, thid;
    picture_prefetch_collect(0);
    if (resource_id < 1 || g_prefetch_state != PREFETCH_IDLE) return;
    key = picture_cache_key(resource_id);
    if (picture_cache_has(key)) return;
    if (key < 0) snprintf(g_prefetch_path, sizeof(g_prefetch_path), ASSET_ROOT "controls_ru177.p44");
    else snprintf(g_prefetch_path, sizeof(g_prefetch_path), ASSET_ROOT "pic_%03d.p44", resource_id);
    g_prefetch_texture = memalign(16, 512 * 512 * 2);
    if (!g_prefetch_texture) return;
    g_prefetch_key = key;
    g_prefetch_state = PREFETCH_LOADING;
    thid = sceKernelCreateThread("NARAKU PIC", picture_prefetch_thread, 0x30, 0x2000, 0, NULL);
    if (thid < 0 || sceKernelStartThread(thid, 0, NULL) < 0) {
        free(g_prefetch_texture);
        g_prefetch_texture = NULL;
        g_prefetch_state = PREFETCH_IDLE;
    }
}

/* A message line often changes two portraits at once (speaker and
 * listener), so up to two upcoming pictures are queued and read one after
 * the other; picture_prefetch_tick() runs every frame while text waits. */
static void picture_prefetch_tick(void)
{
    int i;
    picture_prefetch_collect(0);
    if (g_prefetch_state != PREFETCH_IDLE) return;
    for (i = 0; i < 2; ++i) {
        int rid = g_prefetch_queue[i];
        g_prefetch_queue[i] = 0;
        if (rid > 0 && !picture_cache_has(picture_cache_key(rid))) {
            picture_prefetch(rid);
            return;
        }
    }
}

static void picture_prefetch_queue(int first, int second)
{
    g_prefetch_queue[0] = first;
    g_prefetch_queue[1] = second != first ? second : 0;
    picture_prefetch_tick();
}

static void *picture_cache_take(int key, unsigned char *header, unsigned char *portrait)
{
    int i;
    for (i = 0; i < PICTURE_CACHE_SLOTS; ++i) {
        PictureCacheEntry *e = &g_picture_cache[i];
        if (e->texture && e->key == key) {
            void *texture = e->texture;
            memcpy(header, e->header, sizeof(e->header));
            memcpy(portrait, e->portrait, sizeof(e->portrait));
            memset(e, 0, sizeof(*e));
            return texture;
        }
    }
    return NULL;
}

static void erase_picture_slot(int number)
{
    int i;
    for (i = 0; i < MAX_ACTIVE_PICTURES; ++i) {
        if (g_pictures[i].number != number) continue;
        if (g_pictures[i].texture) {
            if (g_pictures[i].cache_key) picture_cache_put(&g_pictures[i]);
            else free(g_pictures[i].texture);
        }
        memset(&g_pictures[i], 0, sizeof(g_pictures[i]));
        return;
    }
}

static void picture_show(int number, int resource_id, int origin,
                         int x, int y, int zoomx, int zoomy, int opacity)
{
    char path[180];
    unsigned char header[8],portrait_header[12];
    int portrait=0, russian_controls, cache_key;
    FILE *fp;
    PictureSlot *slot;
    void *data;
    if (number == 6 && resource_id == 0) {
        g_picture6_alpha = opacity;
        g_picture6_visible = opacity > 0;
        return;
    }
    if (resource_id < 1) return;
    erase_picture_slot(number);
    if (number == 6) { g_picture6_visible = 0; g_picture6_alpha = 0; }
    slot = picture_slot(number);
    if (!slot) return; /* PSP-1000 memory cap: max five pictures */
    cache_key = picture_cache_key(resource_id);
    russian_controls = cache_key < 0;
    /* A prefetch of this very picture is worth waiting for; any other
     * finished one goes to the cache first. */
    picture_prefetch_collect(g_prefetch_state == PREFETCH_LOADING && g_prefetch_key == cache_key);
    memset(portrait_header, 0, sizeof(portrait_header));
    data = picture_cache_take(cache_key, header, portrait_header);
    portrait = data && memcmp(header,"NPR1",4)==0;
    if (!data) {
        snprintf(path, sizeof(path), ASSET_ROOT "pic_%03d.p44", resource_id);
        if (russian_controls)
            snprintf(path,sizeof(path),ASSET_ROOT "controls_ru177.p44");
        fp = fopen(path,"rb");
        if (!fp) return;
        if (fread(header,1,8,fp) != 8 ||
            (memcmp(header,"NP50",4) != 0 && memcmp(header,"NGO1",4) != 0 && memcmp(header,"NPR1",4) != 0)) {
            fclose(fp);return;
        }
        portrait=memcmp(header,"NPR1",4)==0;
        if(portrait && fread(portrait_header,1,12,fp)!=12) {fclose(fp);return;}
        if(read_u16_le(header+4)<1 || read_u16_le(header+6)<1 ||
           read_u16_le(header+4)>512 || read_u16_le(header+6)>512) {fclose(fp);return;}
        data = memalign(16, 512 * 512 * 2);
        if (!data) {
            /* Memory is short: give the cached pictures back first. */
            picture_cache_clear();
            data = memalign(16, 512 * 512 * 2);
        }
        if (!data) { fclose(fp); return; }
        if (read_chunked(data,512*512*2,fp) != 512*512*2) {
            free(data); fclose(fp);return;
        }
        fclose(fp);
        sceKernelDcacheWritebackInvalidateAll();
    }
    slot->cache_key = cache_key;
    memcpy(slot->cache_header, header, sizeof(slot->cache_header));
    memcpy(slot->cache_portrait, portrait_header, sizeof(slot->cache_portrait));
    slot->number = number; slot->resource_id = resource_id;
    slot->w = (int)read_u16_le(header+4);slot->h=(int)read_u16_le(header+6);
    slot->source_w=slot->w;slot->source_h=slot->h;slot->portrait=portrait;
    if(portrait) {
        slot->w=read_u16_le(portrait_header);slot->h=read_u16_le(portrait_header+2);
        slot->picture_offset_x=read_u16_le(portrait_header+4)*0.5f;
        slot->picture_offset_y=read_u16_le(portrait_header+6)*0.5f;
        slot->picture_region_w=read_u16_le(portrait_header+8)*0.5f;
        slot->picture_region_h=read_u16_le(portrait_header+10)*0.5f;
    }
    slot->origin=origin;slot->x=x;slot->y=y;
    slot->scale_x=zoomx;slot->scale_y=zoomy;slot->opacity=opacity;
    /* Keep the NGO1 asset tag, but also recognise the canonical NARAKU
     * Game Over resource IDs / dimensions at runtime.  This makes old picture
     * manifests harmless: the 816x624 originals (408x312 PSP assets) are
     * always fitted inside 480x272 instead of being cropped at y=-20. */
    slot->fit_screen =
        (memcmp(header,"NGO1",4) == 0) ||
        (resource_id == 9 && slot->w == SCREEN_W && slot->h == SCREEN_H) ||
        (resource_id >= 90 && resource_id <= 97) ||
        (!portrait && number == 11 && slot->w == 408 && slot->h == 312);
    slot->texture=data;
}

static void draw_fog_a1_overlay(int effect_active)
{
    float u;

    /* Original Common Event 15 ("フォグ起動") moves picture #7 by exactly
     * 816 source pixels over 816 frames.  On the 816px PC viewport that is a
     * clean one-pixel step every frame.  Earlier PSP builds resized one period
     * to 480px and then MOVED the destination quad by 480/816 px per frame.
     * Even with linear filtering this makes the translucent mask interact with
     * PPSSPP's final screen scaling and can look like the bricks/body piles are
     * trembling while the world is otherwise still (messages/scripts).
     *
     * 0.7.7 keeps the screen quad completely fixed.  The 816px period is
     * prepared as a 512-texel seamless strip, GU_REPEAT wraps U, and only the
     * texture coordinate advances by 512/816 texels per vblank.  This retains
     * the original 816-frame loop while removing geometry/raster stepping. */
    if (!g_fog_overlay || !effect_active) {
        g_fog_scroll = 0.0f;
        g_fog_active_prev = 0;
        return;
    }

    if (!g_fog_active_prev) {
        g_fog_scroll = 0.0f;
        g_fog_active_prev = 1;
    }

    u = g_fog_scroll;
    sceGuTexMode(GU_PSM_8888, 0, 0, TEXTURE_SWIZZLE_FLAG(g_fog_overlay));
    sceGuTexImage(0, 512, 512, 512, g_fog_overlay);
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_REPEAT, GU_CLAMP);
    sceGuColor(0xFFFFFFFF);

    /* One complete 512-texel fog period is mapped across the whole PSP screen.
     * Sampling beyond u=512 wraps seamlessly to the same strip. */
    draw_bound_rect(
        u, 0.0f, 512.0f, (float)SCREEN_H,
        0.0f, 0.0f, (float)SCREEN_W, (float)SCREEN_H
    );

    /* Restore the canonical pixel-art state immediately.  In 0.7.7 the fog
     * left GU_LINEAR active, and because GU state persists between frames the
     * next map pass could be linearly sampled for exactly one frame. */
    set_pixel_art_texture_state();

    g_fog_scroll += 512.0f / 816.0f;
    if (g_fog_scroll >= 512.0f)
        g_fog_scroll -= 512.0f;
}
