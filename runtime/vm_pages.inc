/* Event page selection for the script VM.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* v0.4.0 event VM page selection                                            */
/* ------------------------------------------------------------------------- */

static int vm_read_event(const MapState *m, int index, VmEventRecord *out)
{
    const uint8_t *p;
    if (!m->vm_bin || index < 0 || index >= m->vm_event_count) return 0;
    p = m->vm_events + (size_t)index * VM_EVENT_BYTES;
    out->event_id = (int)read_u16_le(p + 0);
    out->x = (int)read_u16_le(p + 2);
    out->y = (int)read_u16_le(p + 4);
    out->first_page = (int)read_u16_le(p + 6);
    out->page_count = (int)p[8];
    return 1;
}

static int vm_read_page(const MapState *m, int index, VmPageRecord *out)
{
    const uint8_t *p;
    if (!m->vm_bin || index < 0 || index >= m->vm_page_count) return 0;
    p = m->vm_pages + (size_t)index * VM_PAGE_BYTES;
    out->cond_flags = (int)p[0];
    out->trigger = (int)p[1];
    out->priority = (int)p[2];
    out->flags = (int)p[3];
    out->switch1 = (int)read_u16_le(p + 4);
    out->switch2 = (int)read_u16_le(p + 6);
    out->self_index = (int)p[8];
    out->supported = (int)p[9];
    out->sprite_ref = (int)read_u16_le(p + 10);
    out->cmd_offset = read_u32_le(p + 12);
    out->cmd_size = read_u32_le(p + 16);
    return 1;
}

static int vm_page_conditions_true(const MapState *m, int event_id, const VmPageRecord *p)
{
    if ((p->cond_flags & 0x01) &&
        (p->switch1 <= 0 || p->switch1 >= MAX_SWITCHES || !g_switches[p->switch1]))
        return 0;
    if ((p->cond_flags & 0x02) &&
        (p->switch2 <= 0 || p->switch2 >= MAX_SWITCHES || !g_switches[p->switch2]))
        return 0;
    if (p->cond_flags & 0x04) {
        uint8_t bits;
        if (m->id < 1 || m->id > MAX_MAP_ID || event_id < 0 || event_id >= MAX_EVENT_ID)
            return 0;
        bits = g_self_switches[m->id][event_id];
        if ((bits & (1u << p->self_index)) == 0) return 0;
    }
    return 1;
}

static int vm_active_page_index(
    const MapState *m, const VmEventRecord *ev, VmPageRecord *out, int *index_out)
{
    int i;
    int found = 0;
    int found_index = -1;
    if(ev->event_id>=0 && ev->event_id<MAX_EVENT_ID && g_event_erased[ev->event_id])return 0;
    VmPageRecord tmp;
    for (i = 0; i < ev->page_count; ++i) {
        int idx = ev->first_page + i;
        if (!vm_read_page(m, idx, &tmp)) continue;
        if (vm_page_conditions_true(m, ev->event_id, &tmp)) {
            *out = tmp;
            found = 1;
            found_index = idx;
        }
    }
    if (index_out) *index_out = found_index;
    return found;
}

static int vm_active_page(const MapState *m, const VmEventRecord *ev, VmPageRecord *out)
{
    return vm_active_page_index(m, ev, out, NULL);
}

static int is_chase_enemy(int map_id,int id)
{
    return ((map_id==24 || map_id==25) && id==3) ||
           (map_id==32 && id==23) || (map_id==33 && id==46) ||
           (map_id==34 && id==51) || (map_id==49 && id==28) ||
           (map_id==50 && id==10) || (map_id==51 && id==4) ||
           (map_id==52 && id==5) || (map_id==53 && id==15) || (map_id==95 && (id==3 || id==4 || id==6 || id==7 || id==9));
}
static int is_spider_enemy(int map_id,int id)
{
    return (map_id==67 && id>=1 && id<=4) || (map_id==68 && id==2);
}
static int is_chase_map(int map_id)
{
    return map_id==24 || map_id==25 || (map_id>=32 && map_id<=34) || (map_id>=49 && map_id<=53) || map_id==67 || map_id==68 || map_id==69 || map_id==72 || map_id==95;
}

static void vm_event_contact_cell(const MapState *m, const VmEventRecord *ev, int *x, int *y)
{
    int id=ev->event_id;
    *x=ev->x; *y=ev->y;
    if(id<0 || id>=MAX_EVENT_ID) return;
    /* MZ commits a moving event's logical cell at step start. In the
     * dealer scene, trailing actors must enter the cell already vacated by
     * the leader, even while its sprite is still interpolating out of it. */
    if(is_chase_enemy(m->id,id) || is_spider_enemy(m->id,id) || m->id==69 || m->id==72 ||
       (m->id==94 && g_routes[id].records && g_routes[id].remaining>0)) {
        const EventRoute *r=&g_routes[id];
        float sx=g_event_shift_x[id], sy=g_event_shift_y[id];
        if(r->records && r->remaining>0) {sx=r->start_x+r->dx;sy=r->start_y+r->dy;}
        *x+=(int)(sx+(sx<0?-.5f:.5f));
        *y+=(int)(sy+(sy<0?-.5f:.5f));
    } else {
        *x+=(int)g_event_shift_x[id];
        *y+=(int)g_event_shift_y[id];
    }
}

static int vm_find_event_at(
    const MapState *m, int x, int y, int trigger,
    VmEventRecord *ev_out, VmPageRecord *page_out)
{
    int i;
    VmEventRecord ev;
    VmPageRecord pg;
    if (!m->vm_bin) return 0;
    for (i = 0; i < m->vm_event_count; ++i) {
        if (!vm_read_event(m, i, &ev)) continue;
        {
            int cell_x,cell_y;
            vm_event_contact_cell(m,&ev,&cell_x,&cell_y);
            if((cell_x!=x || cell_y!=y) &&
               !(trigger==1 && spider_entrance_contact(m->id,ev.event_id,x,y,m->w))) continue;
        }
        if (!vm_active_page(m, &ev, &pg)) continue;
        if (!pg.supported || pg.cmd_size <= 1) continue;
        /* Negative trigger requests MZ checkEventTriggerTouchFront: only
         * same-priority Player Touch events on the blocked destination cell. */
        if (trigger == -2 || trigger == -3) {
            /* MZ action: current cell below/above actors, front cell normal
             * priority. Empty pages cannot set Game_Event._starting. */
            /* MZ checkEventTriggerHere([0]), checkEventTriggerThere([0,1,2]).
             * Invisible normal-priority Touch doors also accept action input. */
            if(trigger==-2) {
                if(pg.trigger!=0 || pg.priority==1)continue;
            } else if(pg.trigger>2 || pg.priority!=1)continue;
        } else if (trigger < 0) {
            if ((pg.trigger != 1 && !(((m->id>=80 && m->id<=104) || (m->id>=111 && m->id<=139)) && pg.trigger==2)) || pg.priority != 1) continue;
        } else if (pg.trigger != trigger) continue;
        if (ev_out) *ev_out = ev;
        if (page_out) *page_out = pg;
        return 1;
    }
    return 0;
}

static int vm_event_blocks_except(const MapState *m, int x, int y, int except_id)
{
    int i;
    VmEventRecord ev;
    VmPageRecord pg;
    if (!m->vm_bin) return event_block_at(m, x, y);
    for (i = 0; i < m->vm_event_count; ++i) {
        if (!vm_read_event(m, i, &ev)) continue;
        if (ev.event_id == except_id) continue;
        {
            int cell_x,cell_y;
            vm_event_contact_cell(m,&ev,&cell_x,&cell_y);
            if(cell_x!=x || cell_y!=y) continue;
        }
        if (!vm_active_page(m, &ev, &pg)) continue;
        if (pg.priority == 1 && !(pg.flags & 0x01) &&
            !(ev.event_id>=0 && ev.event_id<MAX_EVENT_ID && g_event_through[ev.event_id])) return 1;
    }
    return 0;
}

static int vm_event_blocks_at(const MapState *m, int x, int y)
{
    return vm_event_blocks_except(m, x, y, -1);
}


/* A supported active VM page owns this cell, including an empty page
 * selected after a one-shot cutscene. Never revive its legacy transfer. */
static int vm_cell_owns_transfer(const MapState *m, int x, int y)
{
    int i;
    VmEventRecord ev;
    VmPageRecord pg;
    if (!m->vm_bin) return 0;
    for (i = 0; i < m->vm_event_count; ++i) {
        if (!vm_read_event(m, i, &ev)) continue;
        if (ev.x != x || ev.y != y) continue;
        if (vm_active_page(m, &ev, &pg) && pg.supported) return 1;
    }
    return 0;
}

