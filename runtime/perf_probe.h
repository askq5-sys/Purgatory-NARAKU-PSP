/* Timers and counters for the perf HUD (NARAKU_PERF_HUD builds only).
 * Included early so every module can probe; without the option the macros
 * expand to nothing and the EBOOT is unchanged. runtime/perf_hud.inc shows
 * the values once per second. */
#ifndef NARAKU_PERF_PROBE_H
#define NARAKU_PERF_PROBE_H

#ifdef NARAKU_PERF_HUD
/* Pictures shown: from cache or prefetch (hit), after waiting for a running
 * prefetch (wait), read on the spot (miss); slowest Show Picture and
 * slowest message text build in the last second, in microseconds. */
static unsigned int g_perf_pic_hit, g_perf_pic_wait, g_perf_pic_miss;
static unsigned int g_perf_pic_us_max, g_perf_text_us_max;
#define PERF_COUNT(counter) (++(counter))
#define PERF_TIME_BEGIN(name) unsigned long long perf_t0_##name = sceKernelGetSystemTimeWide()
#define PERF_TIME_END(name, max) do { \
        unsigned int us_ = (unsigned int)(sceKernelGetSystemTimeWide() - perf_t0_##name); \
        if (us_ > (max)) (max) = us_; \
    } while (0)
#else
#define PERF_COUNT(counter) ((void)0)
#define PERF_TIME_BEGIN(name) do { } while (0)
#define PERF_TIME_END(name, max) do { } while (0)
#endif

#endif
