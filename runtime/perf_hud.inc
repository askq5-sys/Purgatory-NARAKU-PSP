/* Frame-time counter for hardware measurements (NARAKU_PERF_HUD builds).
 * Part of main.c: included there in order, not a standalone unit.
 *
 * "work" is the time from the previous vertical blank to the moment the GE
 * finished the next world frame: game logic, scripts and drawing together.
 * Above 16.7 ms the frame misses a vblank and the game drops below 60 FPS.
 * The second line shows the slowest music read in the last second and how
 * many reads since startup took longer than a buffer plays (audible gaps),
 * pictures shown since startup as cache hit / waited for prefetch / read on
 * the spot, the slowest Show Picture and the slowest message text build in
 * the last second (runtime/perf_probe.h).
 * The map number helps to report where something happened; menus (title,
 * inventory, save/load) count as their own frames and show "Menu".
 * "heap" is the memory malloc hands out now and the most it has taken from
 * the system so far (fragmentation included). The SDK reserves its heap from
 * all free memory at startup, so the kernel's free-memory count says nothing
 * about how close a map comes to the PSP-1000 limit; this does.
 * Without the build option every function is empty and the EBOOT is unchanged. */
#ifdef NARAKU_PERF_HUD
#include <malloc.h>
static unsigned long long g_perf_after_vblank, g_perf_window_start;
static unsigned int g_perf_frames, g_perf_work_sum, g_perf_work_max;
static char g_perf_text[96], g_perf_text2[96];
static int g_perf_text_len, g_perf_text2_len;
static unsigned int g_perf_heap_peak;

static void perf_after_vblank(void)
{
    g_perf_after_vblank = sceKernelGetSystemTimeWide();
    if (!g_perf_window_start) g_perf_window_start = g_perf_after_vblank;
}

static void perf_frame_done(int map_id)
{
    unsigned long long now = sceKernelGetSystemTimeWide();
    unsigned int work, elapsed;
    if (!g_perf_after_vblank) return;
    work = (unsigned int)(now - g_perf_after_vblank);
    g_perf_work_sum += work;
    if (work > g_perf_work_max) g_perf_work_max = work;
    ++g_perf_frames;
    elapsed = (unsigned int)(now - g_perf_window_start);
    if (elapsed >= 1000000u) {
        unsigned int fps10 = (unsigned int)((unsigned long long)g_perf_frames * 10000000ull / elapsed);
        unsigned int avg = g_perf_work_sum / g_perf_frames;
        char place[8];
        struct mallinfo mi = mallinfo();
        if (map_id > 0) snprintf(place, sizeof(place), "Map%03d", map_id % 1000);
        else memcpy(place, "Menu", 5);
        unsigned int heap = (unsigned int)mi.uordblks;
        if ((unsigned int)mi.arena > g_perf_heap_peak) g_perf_heap_peak = (unsigned int)mi.arena;
        g_perf_text_len = snprintf(g_perf_text, sizeof(g_perf_text),
            "%s FPS %u.%u work %u.%u/%u.%u ms heap %u/%u KB", place,
            fps10 / 10, fps10 % 10, avg / 1000, avg % 1000 / 100,
            g_perf_work_max / 1000, g_perf_work_max % 1000 / 100,
            heap / 1024, g_perf_heap_peak / 1024);
        g_perf_text2_len = snprintf(g_perf_text2, sizeof(g_perf_text2),
            "music %u.%ums gaps %u pic %u/%u/%u %ums text %ums",
            g_perf_bgm_read_max / 1000, g_perf_bgm_read_max % 1000 / 100, g_perf_bgm_late,
            g_perf_pic_hit, g_perf_pic_wait, g_perf_pic_miss,
            g_perf_pic_us_max / 1000, g_perf_text_us_max / 1000);
        g_perf_bgm_read_max = g_perf_pic_us_max = g_perf_text_us_max = 0;
        if (g_perf_text2_len < 0) g_perf_text2_len = 0;
        if (g_perf_text2_len >= (int)sizeof(g_perf_text2)) g_perf_text2_len = (int)sizeof(g_perf_text2) - 1;
        if (g_perf_text_len < 0) g_perf_text_len = 0;
        if (g_perf_text_len >= (int)sizeof(g_perf_text)) g_perf_text_len = (int)sizeof(g_perf_text) - 1;
        g_perf_frames = g_perf_work_sum = g_perf_work_max = 0;
        g_perf_window_start = now;
    }
}

static void perf_hud_draw(void)
{
    if (g_perf_text_len <= 0) return;
    draw_solid_rect(0.0f, 0.0f, (float)SCREEN_W, 30.0f, 0, 0, 0, 170);
    draw_utf8_wrapped(g_perf_text, (size_t)g_perf_text_len, 4.0f, 2.0f, (float)SCREEN_W - 4.0f, 1);
    if (g_perf_text2_len > 0)
        draw_utf8_wrapped(g_perf_text2, (size_t)g_perf_text2_len, 4.0f, 16.0f, (float)SCREEN_W - 4.0f, 1);
}
#else
static inline void perf_after_vblank(void) {}
static inline void perf_frame_done(int map_id) { (void)map_id; }
static inline void perf_hud_draw(void) {}
#endif
