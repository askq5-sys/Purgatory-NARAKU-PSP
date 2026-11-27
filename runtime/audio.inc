/* Music streaming and sound effects.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Audio                                                                     */
/* ------------------------------------------------------------------------- */

static void set_bgm_track(const char *path)
{
    if (!path) path = "";
    strncpy(g_bgm_path, path, sizeof(g_bgm_path) - 1);
    g_bgm_path[sizeof(g_bgm_path) - 1] = '\0';
    g_bgm_fade_us = 0;
    g_bgm_generation++;
}

#ifdef NARAKU_PERF_HUD
/* Music read timing for the perf HUD: one buffer plays for 92.9 ms, so a
 * read (memory stick + decoding) slower than that leaves a gap. */
#define PERF_BGM_BUFFER_US 92880u
static volatile unsigned int g_perf_bgm_read_max, g_perf_bgm_late;
#define PERF_BGM_READ_BEGIN() unsigned long long perf_bgm_t0 = sceKernelGetSystemTimeWide()
#define PERF_BGM_READ_END() do { \
        unsigned int us_ = (unsigned int)(sceKernelGetSystemTimeWide() - perf_bgm_t0); \
        if (us_ > g_perf_bgm_read_max) g_perf_bgm_read_max = us_; \
        if (us_ > PERF_BGM_BUFFER_US) ++g_perf_bgm_late; \
    } while (0)
#else
#define PERF_BGM_READ_BEGIN() do { } while (0)
#define PERF_BGM_READ_END() do { } while (0)
#endif

static int bgm_thread(SceSize args, void *argp)
{
    PcmReader reader;
    int reader_open = 0;
    int channel;
    int16_t *buffer;
    int seen_generation = -1;
    int fade_stopped = 0;

    (void)args;
    (void)argp;
    memset(&reader, 0, sizeof(reader));

    channel = sceAudioChReserve(
        PSP_AUDIO_NEXT_CHANNEL, AUDIO_FRAMES, PSP_AUDIO_FORMAT_STEREO
    );
    if (channel < 0) return 0;

    buffer = (int16_t *)memalign(64, AUDIO_BYTES);
    if (!buffer) {
        sceAudioChRelease(channel);
        return 0;
    }

    while (!g_audio_stop) {
        if (seen_generation != g_bgm_generation) {
            char path[192];
            if (reader_open) { pcm_reader_close(&reader); reader_open = 0; }
            strncpy(path, g_bgm_path, sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0';
            seen_generation = g_bgm_generation;
            fade_stopped = 0;
            if (path[0]) reader_open = pcm_reader_open(&reader, path);
        }

        if (!reader_open || fade_stopped) {
            sceKernelDelayThread(10000);
            continue;
        }

        {
            PERF_BGM_READ_BEGIN();
            size_t got = pcm_reader_read(&reader, buffer, AUDIO_BYTES);
            if (got < AUDIO_BYTES) {
                if (pcm_reader_finished(&reader)) {
                    size_t more = 0;
                    if (pcm_reader_rewind(&reader))
                        more = pcm_reader_read(&reader, (uint8_t *)buffer + got, AUDIO_BYTES - got);
                    got += more;
                }
                if (got < AUDIO_BYTES)
                    memset((uint8_t *)buffer + got, 0, AUDIO_BYTES - got);
            }
            PERF_BGM_READ_END();
            {
                int vol = PSP_AUDIO_VOLUME_MAX * g_bgm_volume / 100;
                if (vol < 0) vol = 0;
                if (vol > PSP_AUDIO_VOLUME_MAX) vol = PSP_AUDIO_VOLUME_MAX;
                if (g_bgm_fade_us && g_bgm_fade_generation == seen_generation) {
                    unsigned long long elapsed = sceKernelGetSystemTimeWide() - g_bgm_fade_start;
                    unsigned long long duration = g_bgm_fade_us;
                    if (elapsed >= duration) { vol = 0; fade_stopped = 1; }
                    else vol = (int)((unsigned long long)vol * (duration - elapsed) / duration);
                }
                sceAudioOutputBlocking(channel, vol, buffer);
            }
        }
    }

    if (reader_open) pcm_reader_close(&reader);
    free(buffer);
    sceAudioChRelease(channel);
    return 0;
}

static void start_bgm(void)
{
    int thid = sceKernelCreateThread(
        "NARAKU BGM", bgm_thread, 0x12, 0x2000, 0, NULL
    );
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}

static int se_thread(SceSize args, void *argp)
{
    int generation;
    SeJob *job;
    PcmReader reader;
    int channel;
    int16_t *buffer;
    size_t bytes_played = 0;

    if (args < sizeof(SeJob) || !argp) sceKernelExitDeleteThread(0);
    job = (SeJob *)argp;
    generation = job->generation;
    memset(&reader, 0, sizeof(reader));
    if (!pcm_reader_open(&reader, job->path)) sceKernelExitDeleteThread(0);

    channel = sceAudioChReserve(
        PSP_AUDIO_NEXT_CHANNEL, AUDIO_FRAMES, PSP_AUDIO_FORMAT_STEREO
    );
    if (channel < 0) {
        pcm_reader_close(&reader);
        sceKernelExitDeleteThread(0);
    }

    buffer = (int16_t *)memalign(64, AUDIO_BYTES);
    if (!buffer) {
        sceAudioChRelease(channel);
        pcm_reader_close(&reader);
        sceKernelExitDeleteThread(0);
    }

    while (!g_audio_stop && generation == g_se_generation) {
        size_t request_bytes = AUDIO_BYTES;
        size_t got;
        if (job->max_bytes) {
            if (bytes_played >= job->max_bytes) break;
            if (request_bytes > job->max_bytes - bytes_played)
                request_bytes = job->max_bytes - bytes_played;
        }
        got = pcm_reader_read(&reader, buffer, request_bytes);
        if (got == 0) break;
        if (got < AUDIO_BYTES)
            memset((uint8_t *)buffer + got, 0, AUDIO_BYTES - got);
        if (job->fade_bytes && job->max_bytes) {
            size_t fade_start = job->max_bytes > job->fade_bytes
                ? job->max_bytes - job->fade_bytes : 0;
            size_t samples = got / sizeof(int16_t);
            size_t i;
            for (i = 0; i < samples; ++i) {
                size_t byte_pos = bytes_played + i * sizeof(int16_t);
                if (byte_pos >= fade_start) {
                    size_t remain = byte_pos < job->max_bytes
                        ? job->max_bytes - byte_pos : 0;
                    buffer[i] = (int16_t)((int32_t)buffer[i] *
                        (int32_t)remain / (int32_t)job->fade_bytes);
                }
            }
        }
        {
            int vol = PSP_AUDIO_VOLUME_MAX * g_se_volume / 100;
            if (vol < 0) vol = 0;
            if (vol > PSP_AUDIO_VOLUME_MAX) vol = PSP_AUDIO_VOLUME_MAX;
            sceAudioOutputBlocking(channel, vol, buffer);
        }
        bytes_played += got;
        if (got < request_bytes || got < AUDIO_BYTES) break;
    }

    free(buffer);
    sceAudioChRelease(channel);
    pcm_reader_close(&reader);
    sceKernelExitDeleteThread(0);
    return 0;
}

static void play_se_async_limited(const char *path, size_t max_bytes, size_t fade_bytes)
{
    SeJob job;
    int thid;

    memset(&job, 0, sizeof(job));
    job.generation = g_se_generation;
    strncpy(job.path, path, sizeof(job.path) - 1);
    job.max_bytes = max_bytes;
    job.fade_bytes = fade_bytes;

    thid = sceKernelCreateThread(
        "NARAKU SE", se_thread, 0x13, 0x2000, 0, NULL
    );
    if (thid >= 0) sceKernelStartThread(thid, sizeof(job), &job);
}

static void play_se_async(const char *path)
{
    play_se_async_limited(path, 0, 0);
}

static void play_vm_se_params(int se_id, int volume, int pitch, int pan)
{
    if (se_id >= 1000) {
        char path[180];
        snprintf(path, sizeof(path), ASSET_ROOT "se_exact_%04d.pcm", se_id);
        play_se_async(path);
        return;
    }
    (void)pan; /* New exact PCM assets include volume, pitch and pan. */

    /* The blood/water floor sound is reused with materially different event
     * parameters.  0.6.8 discarded them and always played the opening splash
     * (20%, pitch 50), making footsteps far too loud and low.  Pick the exact
     * pre-rendered variant from the command's original MZ volume/pitch. */
    if (se_id == 3) {
        if (volume == 5 && pitch == 70)
            play_se_async_limited(SE_SPLASH_STEP70_PATH, STEP_SE_MAX_BYTES, STEP_SE_FADE_BYTES);
        else if (volume == 5 && pitch == 120)
            play_se_async_limited(SE_SPLASH_STEP120_PATH, STEP_SE_MAX_BYTES, STEP_SE_FADE_BYTES);
        else if (volume == 20 && pitch == 70)
            play_se_async_limited(SE_SPLASH_20_70_PATH, STEP_SE_MAX_BYTES, STEP_SE_FADE_BYTES);
        else
            play_se_async_limited(SE_SPLASH_PATH, STEP_SE_MAX_BYTES, STEP_SE_FADE_BYTES);
        return;
    }

    /* Other early sounds already have their story-used event parameters baked
     * into their PCM assets.  Runtime master SE volume (default 75) is applied
     * once in se_thread, matching the original Options setting. */
    if (se_id == 1) play_se_async(SE_BONE_PATH);              /* 40%, pitch 70 */
    else if (se_id == 2) {
        if (volume == 70 && pitch == 80) play_se_async(SE_WATER_70_80_PATH);
        else if (volume == 50 && pitch == 80) play_se_async(SE_WATER_50_80_PATH);
        else play_se_async(SE_WATER_PATH);                    /* canonical 90%, pitch 100 */
    }
    else if (se_id == 4) play_se_async_limited(SE_SPLASH_STEP70_PATH, STEP_SE_MAX_BYTES, STEP_SE_FADE_BYTES);/* legacy alias */
    else if (se_id == 5) play_se_async_limited(SE_SPLASH_STEP120_PATH, STEP_SE_MAX_BYTES, STEP_SE_FADE_BYTES);/* legacy alias */
    else if (se_id == 6) play_se_async(SE_BONE_100_PATH);     /* 90%, pitch 100 */
    else if (se_id == 7) play_se_async(SE_MENU_OPEN_PATH);
    else if (se_id == 8) play_se_async(SE_DECISION1_PATH);
    else if (se_id == 9) play_se_async(SE_DECISION2_PATH);
    else if (se_id == 10) play_se_async(SE_CANCEL2_PATH);
    else if (se_id == 11) play_se_async(SE_CURSOR2_PATH);
    else if (se_id == 12) play_se_async(SE_EVASION1_PATH);
    else if (se_id == 13) play_se_async(SE_SLIME_FALL_PATH);
    else if (se_id == 14) play_se_async(SE_SE7_PATH);
    else if (se_id == 15) play_se_async(SE_SWITCH1_PATH);
    else if (se_id == 16) play_se_async(SE_SWITCH2_PATH);
    else if (se_id == 17) play_se_async(SE_MONITOR_PATH);
    else if (se_id == 18) play_se_async(SE_OPEN9_PATH);
    else if (se_id == 19) play_se_async(SE_GATE1_PATH);
    else if (se_id == 20) play_se_async(SE_GATE2_PATH);
    else if (se_id == 21) play_se_async(SE_SWORD5_PATH);
    else if (se_id == 22) play_se_async(SE_HIT_AXE3_PATH);
    else if (se_id == 23) play_se_async(SE_BONE5_PATH);
    else if (se_id == 24) play_se_async(SE_EQUIP2_PATH);
    else if (se_id == 25) play_se_async(SE_SLASH7_PATH);
    else if (se_id == 26) play_se_async(SE_BLOW2_PATH);
    else if (se_id == 27) play_se_async(SE_MONSTER5_PATH);
    else if (se_id == 28) play_se_async(SE_HATCH_SLIDE_PATH);
}

