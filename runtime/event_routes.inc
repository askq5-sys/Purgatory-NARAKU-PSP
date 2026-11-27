/* Event and player move routes, scripted actors.
 * Part of main.c: included there in order, not a standalone unit. */

/* Keep the laser trail reset independent of memset's return value.  Some
 * PPSSPP JIT/IR paths on ARM devices have been observed to lose that return
 * pointer when this reset is inlined, turning the following sentinel store
 * into a write at 0x404.  The void wrapper forces the trail header to form
 * addresses from the actual object instead. */
static __attribute__((noinline)) void laser_trail_memset_discard_result(
    void *dst, int value, size_t size)
{
    (void)memset(dst, value, size);
}
#define memset(dst, value, size) \
    laser_trail_memset_discard_result((dst), (value), (size))
#include "laser_chase_trail.h"
#undef memset

static void clear_event_routes(void)
{
    laser_chase_reset();
    g_s003_paused = 0;
    memset(g_factory_page,0,sizeof(g_factory_page));
    g_factory_machine_frame=g_factory_corpse_phase=0;
    g_pressing_room_frame = 0;
    g_laser_room_frame = g_laser_stop_count = g_laser_route_index = 0;
    int i;
    free(g_player_route.records);memset(&g_player_route,0,sizeof(g_player_route));
    g_ambush_reveal_started = 0;
    for(i=0;i<MAX_EVENT_ID;++i) {
        free(g_routes[i].records);memset(&g_routes[i],0,sizeof(EventRoute));
        g_event_jump_height[i]=0;g_event_opacity[i]=255;g_event_through[i]=0;g_event_direction[i]=-1;g_event_direction_fix[i]=0;g_event_erased[i]=0;
        player_animation_reset(&g_event_animation[i]);
    }
}

static int event_route_can_step(const MapState *m, int id, int x, int y, int code)
{
    int dx=code==2?-1:(code==3?1:0), dy=code==1?1:(code==4?-1:0);
    int bit=code==1?1:(code==2?2:(code==3?4:8));
    int reverse=code==1?8:(code==2?4:(code==3?2:1));
    return code>=1 && code<=4 && (pass_mask_at(m,x,y)&bit) &&
           (pass_mask_at(m,x+dx,y+dy)&reverse) &&
           !vm_event_blocks_except(m,x+dx,y+dy,id);
}

static int g_s003_player_x, g_s003_player_y;
static void tick_event_routes(const MapState *m)
{
    int id;
    if(m->id==32 && !g_s003_paused)
        laser_chase_record(g_s003_player_x,g_s003_player_y);
    for(id=1;id<MAX_EVENT_ID;++id) {
        EventRoute *r=&g_routes[id];
        int budget=128;
        if(!r->records)continue;
        if(r->remaining>0) {
            float t=(float)(r->total-r->remaining+1)/r->total;
            g_event_shift_x[id]=r->start_x+r->dx*t;
            g_event_shift_y[id]=r->start_y+r->dy*t;
            --r->remaining;
            if(r->jump_peak>0) {
                int d=abs(r->remaining-r->jump_peak);
                g_event_jump_height[id]=(r->jump_peak*r->jump_peak-d*d)*.25f;
            }
            continue;
        }
        while(r->records && budget-->0) {
            const uint8_t *p;int code,n;
            if(r->index>=r->count) {
                if((r->flags&1) && r->count) {r->index=0;break;}
                free(r->records);memset(r,0,sizeof(*r));break;
            }
            p=r->records+r->index*5;code=p[0];n=(int16_t)read_u16_le(p+1);
            ++r->index;
            r->jump_peak=0;g_event_jump_height[id]=0;
            if(code==14) {
                int dx=n,dy=(int16_t)read_u16_le(p+3),sq=dx*dx+dy*dy,distance=0;
                int speed=g_event_move_speed[id]>0?g_event_move_speed[id]:3;
                while((distance+1)*(distance+1)<=sq)++distance;
                if(sq-distance*distance >= (distance+1)*(distance+1)-sq)++distance;
                if(!g_event_direction_fix[id]) {
                    if(abs(dx)>abs(dy) && dx)g_event_direction[id]=dx<0?1:2;
                    else if(dy)g_event_direction[id]=dy<0?3:0;
                }
                player_animation_reset(&g_event_animation[id]);
                r->jump_peak=10+distance-speed;
                if(r->jump_peak<1)r->jump_peak=1;
                r->dx=dx;r->dy=dy;r->start_x=g_event_shift_x[id];r->start_y=g_event_shift_y[id];
                r->total=r->jump_peak*2;r->remaining=r->total-1;
                g_event_shift_x[id]=r->start_x+(float)dx/r->total;
                g_event_shift_y[id]=r->start_y+(float)dy/r->total;
                g_event_jump_height[id]=(2*r->jump_peak-1)*.25f;
                break;
            }
            if(code==9)code=1+rand()%4;
            if(code==10) {
                int x=r->base_x+(int)(g_event_shift_x[id]+(g_event_shift_x[id]<0?-.5f:.5f));
                int y=r->base_y+(int)(g_event_shift_y[id]+(g_event_shift_y[id]<0?-.5f:.5f));
                int dx=g_s003_player_x-x, dy=g_s003_player_y-y;
                int secondary;
                int trail_code=(m->id==32 && id==23) ? laser_chase_step(x,y) : -1;
                if (abs(dx)>abs(dy)) {
                    code=dx>0?3:2; secondary=dy>0?1:(dy<0?4:0);
                } else {
                    code=dy>0?1:(dy<0?4:0); secondary=dx>0?3:(dx<0?2:0);
                }
                if (code && !g_event_through[id] && !event_route_can_step(m,id,x,y,code) &&
                    secondary && event_route_can_step(m,id,x,y,secondary)) code=secondary;
                if(trail_code>=0)code=trail_code;
            }
            if(code==13)code=4-g_event_direction[id];
            if(code==29) {
                g_event_move_speed[id]=n<1?1:(n>6?6:n);
                /* MZ processMoveCommand uses one update for a speed change.
                 * The giant-spider route alternates 5/6; consuming the next
                 * move in this same update shortened every chase cycle. */
                if(m->id==72)break;
            }
            else if(code>=16 && code<=19) {
                if(!g_event_direction_fix[id])g_event_direction[id]=code-16;
                /* MZ updates these rotating sensors once per frame. Consuming
                 * all four turns at once would display only the last beam. */
                if(m->id==43 && (id==5 || id==8))break;
            }
            else if(code==35)g_event_direction_fix[id]=1;
            else if(code==36)g_event_direction_fix[id]=0;
            else if(code==37)g_event_through[id]=1;
            else if(code==38)g_event_through[id]=0;
            else if(code==39)g_event_opacity[id]=0;
            else if(code==40)g_event_opacity[id]=255;
            else if(code==42)g_event_opacity[id]=n<0?0:(n>255?255:n);
            else if(code==44)play_vm_se_params(n,90,100,0);
            else if(code==15 || (code>=1 && code<=4)) {
                int speed=g_event_move_speed[id];
                r->dx=code==2?-1:(code==3?1:0);r->dy=code==1?1:(code==4?-1:0);
                if(code!=15 && !g_event_through[id]) {
                    int x=r->base_x+(int)(g_event_shift_x[id] + (g_event_shift_x[id] < 0 ? -0.5f : 0.5f));
                    int y=r->base_y+(int)(g_event_shift_y[id] + (g_event_shift_y[id] < 0 ? -0.5f : 0.5f));
                    int nx=x+r->dx,ny=y+r->dy;
                    int bit=code==1?1:(code==2?2:(code==3?4:8));
                    int reverse=code==1?8:(code==2?4:(code==3?2:1));
                    if(!(pass_mask_at(m,x,y)&bit) || !(pass_mask_at(m,nx,ny)&reverse) || vm_event_blocks_except(m,nx,ny,id)) {
                        r->dx=r->dy=0;
                        if (!(r->flags & 2)) {
#ifdef NARAKU_ROUTE_TRACE
                            /* Debug only: independent actors must not invalidate
                             * each other's blocked-command deduplication. */
                            static int last_map = -1;
                            static int last_index[MAX_EVENT_ID];
                            if (last_map != m->id) {
                                memset(last_index,0,sizeof(last_index));last_map=m->id;
                            }
                            if (last_index[id] != r->index) {
                                FILE *log=fopen(ASSET_ROOT "route_trace.log","a");
                                if(log) {
                                    fprintf(log,"map=%d event=%d blocked step=%d\n",m->id,id,r->index);
                                    fclose(log);
                                }
                                last_index[id]=r->index;
                            }
#endif
                            --r->index;
                        }
                        break;
                    }
                }
                if(code>=1 && code<=4 && p[0]!=13 && !g_event_direction_fix[id])
                    g_event_direction[id]=code-1;
                r->start_x=g_event_shift_x[id];r->start_y=g_event_shift_y[id];
                r->total=code==15?n:(256/(1<<(speed>0?speed:3)));
                /* Only the final approach to the Ending 6 meeting: gradually
                 * give Will a small lead before pursuers leave the next scene. */
                if (m->id == 95 && g_switches[939] && g_factory_page[id] &&
                    is_chase_enemy(95,id) && code!=15 &&
                    g_s003_player_y>=12 && g_s003_player_y<=16 &&
                    g_s003_player_x>=1 && g_s003_player_x<9)
                    r->total += r->total * (9-g_s003_player_x) / 16;
                r->remaining=r->total;
                if(r->remaining>0) {
                    g_event_shift_x[id]+= (float)r->dx/r->total;
                    g_event_shift_y[id]+= (float)r->dy/r->total;
                    --r->remaining;break;
                }
            }
        }
    }
    for (id = 1; id < MAX_EVENT_ID; ++id) {
        EventRoute *r = &g_routes[id];
        int moving = r->records && r->remaining > 0 && (r->dx || r->dy);
        player_animation_tick(&g_event_animation[id], moving, r->jump_peak>0 && r->remaining>0,
                              g_event_move_speed[id] > 0 ? g_event_move_speed[id] : 3,
                              1, (m->id == 30 && id == 3) || (m->id == 39 && id >= 2 && id <= 4));
    }
    (void)m;
}

static int start_event_route(const MapState *m,int id,const uint8_t *records,int count,int flags)
{
    EventRoute *r, pending; int i, carry_step;
    if(id<1 || id>=MAX_EVENT_ID || count<=0)return 0;
    r=&g_routes[id];pending=*r;
    carry_step=m->id==94 && pending.records && pending.remaining>0;
    free(r->records);memset(r,0,sizeof(*r));
    if(carry_step) {
        /* forceMoveRoute replaces commands, not a step already in flight. */
        r->start_x=pending.start_x;r->start_y=pending.start_y;
        r->dx=pending.dx;r->dy=pending.dy;
        r->total=pending.total;r->remaining=pending.remaining;
        r->jump_peak=pending.jump_peak;
    } else g_event_jump_height[id]=0;
    r->records=malloc(count*5);if(!r->records)return 0;
    memcpy(r->records,records,count*5);r->count=count;r->flags=flags;
    for(i=0;i<m->vm_event_count;++i) {
        VmEventRecord ev;VmPageRecord pg;int page_index;
        if(vm_read_event(m,i,&ev) && ev.event_id==id && vm_active_page_index(m,&ev,&pg,&page_index)) {
            if(g_event_active_page[id]!=page_index || !g_event_move_speed[id]) {
                g_event_active_page[id]=page_index;
                g_event_move_speed[id]=(pg.flags>>2)&7;
                g_event_direction[id]=(pg.flags>>5)&3;g_event_direction_fix[id]=!!(pg.flags&0x80);
                g_event_through[id]=!!(pg.flags&1);
                g_event_opacity[id]=255;
            }
            r->base_x=ev.x;r->base_y=ev.y;break;
        }
    }
    return 1;
}

#include "s003_routes.h"
static void tick_s003_autonomous(const MapState *m, float player_x, float player_y)
{
    int i;
    g_s003_player_x=(int)(player_x/TILE_PX);
    g_s003_player_y=(int)(player_y/TILE_PX)-1;
    if (g_s003_paused || (m->id != 24 && m->id != 25)) return;
    for(i=0;i<m->vm_event_count;++i) {
        VmEventRecord ev; VmPageRecord pg; int page;
        if (!vm_read_event(m,i,&ev) || ev.event_id!=3 ||
            !vm_active_page_index(m,&ev,&pg,&page)) continue;
        if ((m->id==24 && page==ev.first_page+5) || (m->id==25 && page==ev.first_page+1)) {
            if (!g_routes[3].records) {
                const uint8_t *route=m->id==24?s003_route_24:s003_route_25;
                int count=(m->id==24?sizeof(s003_route_24):sizeof(s003_route_25))/5;
                start_event_route(m,3,route,count,3);
            }
        }
        return;
    }
}

#include "factory_routes.h"
static void tick_factory_autonomous(const MapState *m)
{
    unsigned int i;
    if((m->id<32 || m->id>34) && m->id!=43 && (m->id<48 || m->id>55) && m->id!=67 && m->id!=68 && m->id!=72 && m->id!=95 && m->id!=120)return;
    if(g_s003_paused)return;
    for(i=0;i<sizeof(factory_routes)/sizeof(factory_routes[0]);++i) {
        const FactoryRoute *fr=&factory_routes[i];
        int k,active;VmEventRecord ev;VmPageRecord pg;
        if(fr->map!=m->id)continue;
        for(k=0;k<m->vm_event_count;++k) {
            if(!vm_read_event(m,k,&ev) || ev.event_id!=fr->id)continue;
            if(!vm_active_page_index(m,&ev,&pg,&active))break;
            active-=ev.first_page;
            if(active!=fr->page) {
                /* Page changes cancel an autonomous route, as in MZ. */
                if(g_factory_page[fr->id]==fr->page+1) {
                    free(g_routes[fr->id].records);memset(&g_routes[fr->id],0,sizeof(EventRoute));
                    g_factory_page[fr->id]=0;
                }
                break;
            }
            if(!g_routes[fr->id].records) {
                start_event_route(m,fr->id,fr->records,fr->count,fr->flags);
                g_factory_page[fr->id]=fr->page+1;
            }
            break;
        }
    }
}

static void tick_player_route(const MapState *m)
{
    EventRoute *r=&g_player_route;int budget=128;
    if(!r->records)return;
    if(r->remaining>0) {
        float t=(float)(r->total-r->remaining+1)/r->total;
        g_route_real_x=(r->start_x+r->dx*t+0.5f)*TILE_PX;
        g_route_real_y=(r->start_y+r->dy*t+1.0f)*TILE_PX;
        --r->remaining;return;
    }
    while(r->records && budget-->0) {
        const uint8_t *p;int code,n,dx=0,dy=0;
        if(r->index>=r->count) {
            if((r->flags&1) && r->count){r->index=0;break;}
            free(r->records);memset(r,0,sizeof(*r));break;
        }
        p=r->records+r->index*5;code=p[0];n=(int16_t)read_u16_le(p+1);++r->index;
        if(code==29)g_player_move_speed_code=n<1?1:(n>6?6:n);
        else if(code>=16 && code<=19 && !g_player_direction_fix)*g_route_player_direction=code-16;
        else if(code==31)g_player_walk_anime=1;
        else if(code==32)g_player_walk_anime=0;
        else if(code==33)g_player_step_anime=1;
        else if(code==34)g_player_step_anime=0;
        else if(code==35)g_player_direction_fix=1;
        else if(code==36)g_player_direction_fix=0;
        else if(code==37)g_player_through=1;
        else if(code==38)g_player_through=0;
        else if(code==39)g_player_visible=0;
        else if(code==40)g_player_visible=1;
        else if(code==44)play_vm_se_params(n,90,100,0);
        else if(code==15 || (code>=1 && code<=4) || code==12 || code==13) {
            int nx,ny,bit,reverse;
            if (code != 15) player_route_delta(code, *g_route_player_direction, &dx, &dy);
            nx=*g_route_player_x+dx;ny=*g_route_player_y+dy;
            bit=dy>0?1:(dx<0?2:(dx>0?4:8));
            reverse=dy>0?8:(dx<0?4:(dx>0?2:1));
            if(code>=1 && code<=4 && !g_player_direction_fix)*g_route_player_direction=code-1;
            if(code!=15 && (nx<0 || ny<0 || nx>=m->w || ny>=m->h ||
               (!g_player_through &&
                (!(pass_mask_at(m,*g_route_player_x,*g_route_player_y)&bit) ||
                 !(pass_mask_at(m,nx,ny)&reverse) || vm_event_blocks_at(m,nx,ny))))) {
                if(!(r->flags&2))--r->index;
                break;
            }
            r->start_x=*g_route_player_x;r->start_y=*g_route_player_y;
            *g_route_player_x=nx;*g_route_player_y=ny;r->dx=dx;r->dy=dy;
            r->total=code==15?n:256/(1<<clamp_move_speed_code(g_player_move_speed_code));
            r->remaining=r->total;
            if(r->remaining>0) {
                float t=1.0f/r->total;
                g_route_real_x=(r->start_x+dx*t+0.5f)*TILE_PX;
                g_route_real_y=(r->start_y+dy*t+1.0f)*TILE_PX;
                --r->remaining;break;
            }
        }
    }
}

static int start_player_route(const uint8_t *records,int count,int flags,int *x,int *y,int *direction)
{
    EventRoute *r=&g_player_route;int i;
    for(i=0;i<count;++i)if(records[i*5]==14)return 0; /* jumping still uses the synchronous path */
    free(r->records);memset(r,0,sizeof(*r));
    if(count<=0)return 1;
    r->records=malloc(count*5);if(!r->records)return 0;
    memcpy(r->records,records,count*5);r->count=count;r->flags=flags;
    g_route_player_x=x;g_route_player_y=y;g_route_player_direction=direction;
    g_route_real_x=(*x+0.5f)*TILE_PX;g_route_real_y=(*y+1.0f)*TILE_PX;
    /* Speed/facing/flags-only routes take effect without locking input for
     * an extra frame on every floor tile. Timed/repeating routes stay queued. */
    if (!(flags & 1)) {
        int instant = 1;
        for (i = 0; i < count; ++i) {
            int code = records[i * 5];
            if (!((code >= 16 && code <= 19) || code == 29 ||
                  (code >= 31 && code <= 40) || code == 44)) {
                instant = 0; break;
            }
        }
        if (instant) tick_player_route(NULL);
    }
    return 1;
}
