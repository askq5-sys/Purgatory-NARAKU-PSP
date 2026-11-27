#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspge.h>
#include <pspdebug.h>
#include <pspaudio.h>
#include <psppower.h>

#include <errno.h>
#include <limits.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "runtime/compressed_assets.h"

PSP_MODULE_INFO("NARAKU PSP", PSP_MODULE_USER, 1, 78);
#include "runtime/player_animation.h"
#include "runtime/render_boundaries.h"
#include "runtime/tone_gray.h"
#ifdef NARAKU_SWIZZLE_TEXTURES
#include "runtime/texture_swizzle.h"
#endif
#include "runtime/pressing_room.h"
PSP_MAIN_THREAD_ATTR(PSP_THREAD_ATTR_USER);

#define SCREEN_W 480
#define SCREEN_H 272
#define BUF_W 512
#define TILE_PX 24

#define TILE_ATLAS_W 512
#define TILE_ATLAS_H 512
#define TILE_ATLAS_BYTES (TILE_ATLAS_W * TILE_ATLAS_H * 4)
#define TILE_SLOT 50
#define TILE_SOURCE 48
#define TILE_ATLAS_COLS 10

#define CHAR_ATLAS_W 256
#define CHAR_ATLAS_H 512
#define CHAR_ATLAS_BYTES (CHAR_ATLAS_W * CHAR_ATLAS_H * 4)
#define CHAR_SLOT_W 50
#define CHAR_SLOT_H 80
#define CHAR_SRC_W 48
#define CHAR_SRC_H 78
#define CHAR_DST_W 24
#define CHAR_DST_H 39

#define EVENT_ATLAS_W 512
#define EVENT_ATLAS_H 512
#define EVENT_ATLAS_BYTES (EVENT_ATLAS_W * EVENT_ATLAS_H * 4)

#define PIC_TEX_W 512
#define PIC_TEX_H 512
#define PIC_TEX_BYTES (PIC_TEX_W * PIC_TEX_H * 4)

#define MAP_HEADER_BYTES 20
#define EVENT_HEADER_BYTES 14
#define TRANSFER_RECORD_BYTES 20
#define SPRITE_RECORD_BYTES 16
#define MAX_MAP_ID 139
#define MAX_TILE_ATLASES 3
#define MAX_SWITCHES 1601
#define MAX_VARIABLES 64
#define MAX_ITEMS 128
#define MAX_EVENT_ID 160

#define VM_HEADER_BYTES 20
#define VM_EVENT_BYTES 10
#define VM_PAGE_BYTES 20

#define FONT_TEX_W 512
#define FONT_TEX_H 512
#define FONT_PAGE_BYTES (FONT_TEX_W * FONT_TEX_H)
#define FONT_MAX_PAGES 8
#define FONT_CELL_W 24
#define FONT_CELL_H 28
#define FONT_COLS 21
#define FONT_MAP_HEADER_BYTES 12
#define FONT_GLYPH_BYTES 8

#define VM_OP_END 0
#define VM_OP_TEXT 1
#define VM_OP_SWITCH 2
#define VM_OP_SELF_SWITCH 3
#define VM_OP_WAIT 4
#define VM_OP_TRANSFER 5
#define VM_OP_SE 6
#define VM_OP_ITEM 7
#define VM_OP_TRANSPARENCY 8
#define VM_OP_TINT 9
#define VM_OP_FLASH 10
#define VM_OP_BGM 11
#define VM_OP_FADE_BGM 12
#define VM_OP_EXIT 13
#define VM_OP_PICTURE6 14
#define VM_OP_VARIABLE 15
#define VM_OP_COND_SWITCH 16
#define VM_OP_COND_VAR 17
#define VM_OP_COND_ITEM 18
#define VM_OP_COND_LANG 19
#define VM_OP_JUMP 20
#define VM_OP_MOVE_ROUTE 21
#define VM_OP_CHOICES 22
#define VM_OP_COND_CHOICE 23
#define VM_OP_COMMON_EVENT 24
#define VM_OP_SHOW_PICTURE 25
#define VM_OP_MOVE_PICTURE 26
#define VM_OP_ERASE_PICTURE 27
#define VM_OP_SCENE 28
#define VM_OP_SCROLL_MAP 29
#define VM_OP_SHAKE 30
#define VM_OP_STOP_SE 31
#define VM_OP_SHOW_PICTURE_EX 32
#define VM_OP_MOVE_PICTURE_EX 33
#define VM_OP_MOVE_ROUTE_EX 34
#define VM_OP_ERASE_EVENT 35
#define VM_OP_SAVE_ACCESS 36
#define VM_OP_FADE_SCREEN 37
#define VM_OP_NUMBER_INPUT 38
#define VM_OP_PARTY 41
#define VM_OP_BALLOON 42
#define VM_OP_SCROLL_TEXT 43
#define VM_OP_PARALLAX 39
#define VM_OP_TEXT_TRANSPARENT 40
#define VM_OP_TEXT_STYLE 44
#define VM_OP_CHOICES_STYLE 45

#define SCENE_ITEM 1
#define SCENE_SAVE 3
#define SCENE_LOAD 4
#define SCENE_OPTIONS 5
#define SCENE_TITLE 6

#ifndef NARAKU_GAME_DIR
#define NARAKU_GAME_DIR "NARAKU"
#endif
#define ROOT "ms0:/PSP/GAME/" NARAKU_GAME_DIR "/"
#define ASSET_ROOT ROOT "assets/"
#define CONFIG_PATH ROOT "config.bin"
#define PROGRESS_PATH ROOT "progress.bin"
#define COMMON_VM_PATH ASSET_ROOT "common_vm.bin"
#define UI_DATA_PATH ASSET_ROOT "ui060.bin"
#define UI_ICONSET_PATH ASSET_ROOT "ui_iconset.rgba4444"
#define TITLE_EN_PATH ASSET_ROOT "title_en.rgba8888"
#define TITLE_JA_PATH ASSET_ROOT "title_ja.rgba8888"
#define TITLE_ZHCN_PATH ASSET_ROOT "title_zhcn.rgba8888"
#define TITLE_ZHTW_PATH ASSET_ROOT "title_zhtw.rgba8888"
#define TITLE_BGM_PATH ASSET_ROOT "bgm_title.pcm"
#define SE_MENU_OPEN_PATH ASSET_ROOT "se_menu_open.pcm"
#define SE_DECISION1_PATH ASSET_ROOT "se_decision1.pcm"
#define SE_DECISION2_PATH ASSET_ROOT "se_decision2.pcm"
#define SE_CANCEL2_PATH ASSET_ROOT "se_cancel2.pcm"
#define SE_CURSOR2_PATH ASSET_ROOT "se_cursor2.pcm"
#define SE_UI_CURSOR_PATH ASSET_ROOT "se_ui_cursor.pcm"
#define SE_UI_OK_PATH ASSET_ROOT "se_ui_ok.pcm"
#define SE_UI_CANCEL_PATH ASSET_ROOT "se_ui_cancel.pcm"
#define SE_UI_BUZZER_PATH ASSET_ROOT "se_ui_buzzer.pcm"
#define SE_UI_SAVE_PATH ASSET_ROOT "se_ui_save.pcm"
#define SE_UI_LOAD_PATH ASSET_ROOT "se_ui_load.pcm"
#define SAVE_SLOT_COUNT 20
#define UI_ICONSET_BYTES (512 * 512 * 2)
#define UI_ITEM_REC_BYTES 12
#define UI_HEADER_BYTES 20

#define CHAR_ATLAS_PATH ASSET_ROOT "enri_atlas.rgba8888"
#define BGM_PATH ASSET_ROOT "bgm_low.pcm"
#define A1_PATH ASSET_ROOT "a1_overlay.rgba8888"
#define FOG_A1_PATH ASSET_ROOT "fog_a1.rgba8888"
#define FOG_A1_BYTES (512 * 512 * 4)
#define KEY_ANIM_PATH ASSET_ROOT "key_anim.rgba8888"
#define KEY_ANIM_W 128
#define KEY_ANIM_H 128
#define KEY_ANIM_BYTES (KEY_ANIM_W * KEY_ANIM_H * 4)
#define MESSAGE_SURFACE_W 512
#define MESSAGE_SURFACE_H 256
#define MESSAGE_SURFACE_X 110
#define MESSAGE_SURFACE_SCALE 2
#define MESSAGE_SURFACE_Y (g_runtime_message_position==0?-38:g_runtime_message_position==1?39:144)
#define MESSAGE_SURFACE_BYTES (MESSAGE_SURFACE_W * MESSAGE_SURFACE_H * 4)
#define SE_BONE_PATH ASSET_ROOT "se_bonecrush4.pcm"
#define SE_WATER_PATH ASSET_ROOT "se_water.pcm"
#define SE_WATER_70_80_PATH ASSET_ROOT "se_water_70_80.pcm"
#define SE_WATER_50_80_PATH ASSET_ROOT "se_water_50_80.pcm"
#define SE_SPLASH_PATH ASSET_ROOT "se_splash.pcm"
#define SE_SPLASH_STEP70_PATH ASSET_ROOT "se_splash_step70.pcm"
#define SE_SPLASH_STEP120_PATH ASSET_ROOT "se_splash_step120.pcm"
#define SE_SPLASH_20_70_PATH ASSET_ROOT "se_splash_20_70.pcm"
#define SE_BONE_100_PATH ASSET_ROOT "se_bonecrush4_100.pcm"
#define SE_EVASION1_PATH ASSET_ROOT "se_evasion1.pcm"
#define SE_SLIME_FALL_PATH ASSET_ROOT "se_slime_fall.pcm"
#define SE_SE7_PATH ASSET_ROOT "se_se7.pcm"
#define SE_SWITCH1_PATH ASSET_ROOT "se_switch1.pcm"
#define SE_SWITCH2_PATH ASSET_ROOT "se_switch2.pcm"
#define SE_MONITOR_PATH ASSET_ROOT "se_monitor.pcm"
#define SE_OPEN9_PATH ASSET_ROOT "se_open9.pcm"
#define SE_GATE1_PATH ASSET_ROOT "se_gate1.pcm"
#define SE_GATE2_PATH ASSET_ROOT "se_gate2.pcm"
#define SE_SWORD5_PATH ASSET_ROOT "se_sword5.pcm"
#define SE_HIT_AXE3_PATH ASSET_ROOT "se_hit_axe3.pcm"
#define SE_BONE5_PATH ASSET_ROOT "se_bonecrush5.pcm"
#define SE_EQUIP2_PATH ASSET_ROOT "se_equip2.pcm"
#define SE_SLASH7_PATH ASSET_ROOT "se_slash7.pcm"
#define SE_BLOW2_PATH ASSET_ROOT "se_blow2.pcm"
#define SE_MONSTER5_PATH ASSET_ROOT "se_monster5.pcm"
#define SE_HATCH_SLIDE_PATH ASSET_ROOT "se_hatch_slide.pcm"

#define AUDIO_FRAMES 4096
#define AUDIO_BYTES (AUDIO_FRAMES * 2 * (int)sizeof(int16_t))
/* RPG Maker's blood-floor step effect has a long reverb tail (about 1.5 s).
 * Replaying the whole clip on every tile stacks many voices and repeated
 * DEFLATE work on low-end phones. Keep the footfall and a short faded tail. */
#define STEP_SE_MAX_BYTES (44100 * 2 * 2 * 35 / 100) /* 0.35 s, stereo S16 */
#define STEP_SE_FADE_BYTES (44100 * 2 * 2 * 9 / 100) /* last 0.09 s */

static unsigned int __attribute__((aligned(16))) gu_list[65536];
static uint8_t *g_common_vm = NULL;
static size_t g_common_vm_size = 0;
static int g_common_vm_count = 0;
static int g_common_depth = 0;
static int g_language = 0;
static int g_always_dash = 0;
static int g_bgm_volume = 75;
static int g_se_volume = 75;
static int g_window_opacity = 195;
static int g_config_valid = 0;
/* 0.7.4 one-time compatibility migration.  Several 0.6/0.7 test builds
 * could award Map005's Red Key while its sprite was invisible.  Persist
 * whether that legacy save repair has already been offered. */
static int g_map5_key_migration_done = 1;
static int g_request_title = 0;
static int g_loaded_from_scene = 0;
static volatile int g_bgm_generation = 1;
static volatile int g_se_generation = 1;
static volatile unsigned long long g_bgm_fade_start = 0;
static volatile unsigned long long g_bgm_fade_us = 0;
static volatile int g_bgm_fade_generation = 0;
static void *g_draw_buffer = (void *)0;
static void *g_last_world_buffer = (void *)0;
static int g_last_world_buffer_valid = 0;
static char g_bgm_path[192] = BGM_PATH;

typedef struct {
    uint8_t *data;
    size_t size;
    int item_count;
    int label_count;
    const uint8_t *item_table;
    const uint8_t *label_table;
    const uint8_t *blob;
    size_t blob_size;
    void *iconset;
} UiState;
static UiState g_ui;

static int g_last_choice = -1;
static int g_choice_active = 0;
static int g_number_active,g_number_digits,g_number_cursor,g_number_value;
static int g_script_fade_alpha;
static int g_choice_count = 0;
static int g_choice_cursor = 0;
static const char *g_choice_labels[6];
static size_t g_choice_lengths[6];
#define MAX_ACTIVE_PICTURES 5
typedef struct {
    int number, resource_id, w, h, origin;
    float x, y, scale_x, scale_y, opacity;
    int blend, duration, total, easing;
    float from[5], target[5];
    int fit_screen;
    int source_w,source_h,portrait;
    float picture_offset_x,picture_offset_y,picture_region_w,picture_region_h;
    void *texture;
} PictureSlot;
static PictureSlot g_pictures[MAX_ACTIVE_PICTURES];
static volatile int g_audio_stop = 0;
#ifdef NARAKU_BOOT_DIAGNOSTICS
static char g_boot_diag_path[512] = ROOT "BOOTLOG.TXT";
static int g_boot_diag_row;

static void boot_diag_init(const char *argv0)
{
    const char *slash = argv0 ? strrchr(argv0, '/') : NULL;
    if (slash && (size_t)(slash - argv0) < sizeof(g_boot_diag_path) - 16) {
        size_t n = (size_t)(slash - argv0) + 1;
        memcpy(g_boot_diag_path, argv0, n);
        strcpy(g_boot_diag_path + n, "BOOTLOG.TXT");
    }
    pspDebugScreenInit();
    pspDebugScreenClear();
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("NARAKU PSP 1.7.8 - PSP-1000 BOOT TEST\n");
    pspDebugScreenPrintf("Keep this screen or send a photo.\n\n");
    g_boot_diag_row = 3;
}

static void boot_diag(const char *stage)
{
    FILE *f = fopen(g_boot_diag_path, "a");
    int logged = f != NULL;
    int free_bytes = sceKernelTotalFreeMemSize();
    if (f) {
        fprintf(f, "%s free=%d\n", stage, free_bytes);
        fclose(f);
    }
    if (g_boot_diag_row < 33) {
        pspDebugScreenSetXY(0, g_boot_diag_row++);
        pspDebugScreenPrintf("%-25s %5d KB  log:%s\n", stage,
                             free_bytes / 1024, logged ? "OK" : "FAIL");
    }
}

static void boot_diag_error(const char *message)
{
    FILE *f = fopen(g_boot_diag_path, "a");
    if (f) {
        fprintf(f, "FATAL: %s\n", message ? message : "unknown");
        fclose(f);
    }
}
#else
static void boot_diag_init(const char *argv0) { (void)argv0; }
static void boot_diag(const char *stage) { (void)stage; }
static void boot_diag_error(const char *message) { (void)message; }
#endif

static uint8_t g_switches[MAX_SWITCHES];
static int g_save_enabled = 1;
static int32_t g_variables[MAX_VARIABLES];
static int16_t g_items[MAX_ITEMS];
#include "runtime/factory_puzzle.h"
#include "runtime/consumed_items.h"
#include "runtime/player_forms.h"
static uint8_t g_self_switches[MAX_MAP_ID + 1][MAX_EVENT_ID];
static float g_event_shift_x[MAX_EVENT_ID];
static float g_event_shift_y[MAX_EVENT_ID];
static float g_event_jump_height[MAX_EVENT_ID];
static int8_t g_event_move_speed[MAX_EVENT_ID];
static int16_t g_event_active_page[MAX_EVENT_ID];
static int g_event_opacity[MAX_EVENT_ID];
static int g_event_direction[MAX_EVENT_ID];
static uint8_t g_event_direction_fix[MAX_EVENT_ID];
static uint8_t g_event_erased[MAX_EVENT_ID];
static uint8_t g_event_through[MAX_EVENT_ID];
typedef struct {
    uint8_t *records;
    int count,index,flags,remaining,total,dx,dy,base_x,base_y,jump_peak;
    float start_x,start_y;
} EventRoute;
static EventRoute g_routes[MAX_EVENT_ID];
static PlayerAnimation g_event_animation[MAX_EVENT_ID];
static int g_s003_paused;
static int g_factory_page[MAX_EVENT_ID];
static int g_factory_machine_frame,g_factory_corpse_phase;
static EventRoute g_player_route;
static int *g_route_player_x,*g_route_player_y,*g_route_player_direction;
static float g_route_real_x,g_route_real_y;
static int g_player_direction_fix=0;
static int g_player_walk_anime=1,g_player_step_anime=0;
static void clear_event_routes(void);
static void vm_tick_world_tint(void);

static int g_camera_locked = 0;
static float g_camera_lock_x = 0.0f;
static float g_camera_lock_y = 0.0f;
static void *g_a1_overlay = NULL;
static void *g_ui_char_atlas = NULL;
static void *g_balloon_atlas = NULL;
static int g_balloon_frames = 0, g_balloon_target = -1, g_balloon_id = 1;
static void *g_fog_overlay = NULL;
static void *g_key_anim = NULL;
static uint32_t *g_runtime_message_surface = NULL;
static int g_runtime_message_surface_ready = 0;
static float g_fog_scroll = 0.0f;
static int g_fog_active_prev = 0;

/* Early story parallel events that matter to the Processing Plant route.
 * RPG Maker runs these while the player can still move; older PSP builds only
 * executed autoruns (trigger 3), so the two-Green-Key shutter never armed the
 * later Worm sequence. */
static int g_map8_drop_timer = 0;
static int g_map9_water_timer = 0;
/* One tick per rendered world frame.  Event pages with RPG Maker
 * stepAnime use this to cycle 1 -> 2 -> 1 -> 0 exactly while standing. */
static unsigned int g_world_render_frames = 0;
/* One persistent animation state for manual movement and event routes. */
static PlayerAnimation g_player_animation = {0, 1};
static int g_player_animation_moving = 0;
static int g_player_animation_jumping = 0;
static int g_player_animation_speed = 3;
static uint32_t g_playtime_frames = 0;
static float g_actor_camera_x, g_actor_camera_y;
static int g_picture6_visible = 0;
static int g_picture6_alpha = 0;
/* RPG Maker MZ stores the player base move speed persistently.  NARAKU
 * deliberately changes it with Player Touch events (not by terrain type):
 * blood/sludge areas commonly use speed 3, while ordinary floor uses speed 4.
 * Dashing adds +1 to this base value, exactly like Game_Player::realMoveSpeed. */
static int g_player_move_speed_code = 3;
/* RPG Maker's Through flag belongs to the character, not to one individual
 * Set Movement Route command.  Some NARAKU scenes deliberately enable it in
 * one route, perform a jump/move in later routes, then disable it afterwards
 * (Map007 Event 25 is the early corpse-pile slide). */
static int g_player_through = 0;
static int g_player_visible = 1;
static float g_camera_scroll_x = 0.0f;
static float g_camera_scroll_y = 0.0f;
static int g_map_scroll_remaining, g_map_scroll_total;
static float g_map_scroll_start_x, g_map_scroll_start_y, g_map_scroll_target_x, g_map_scroll_target_y;
static float g_screen_shake_x = 0.0f;
static float g_stage_parallax_y;
static int g_stage_parallax_scrolling;
static void clear_stage_parallel(void);

/* RPG Maker screen tone persists between interpreter commands.  Keep the
 * actual RGB tone values instead of collapsing every non-black tone to zero.
 * NARAKU's normal early-game look uses [10,-10,-10,0], which is the subtle
 * red veil seen in the PC version after the opening. */
static int g_world_tone_r = 0;
static int g_world_tone_g = 0;
static int g_world_tone_b = 0;
static int g_world_tone_gray = 0;
static int g_world_tone_from_r = 0;
static int g_world_tone_from_g = 0;
static int g_world_tone_from_b = 0;
static int g_world_tone_from_gray = 0;
static int g_world_tone_target_r = 0;
static int g_world_tone_target_g = 0;
static int g_world_tone_target_b = 0;
static int g_world_tone_target_gray = 0;
static int g_world_tone_total = 0;
static int g_world_tone_remaining = 0;

/* Screen Flash (event command 224) is asynchronous in RPG Maker.  NARAKU's
 * axe/bone-hit flashes use wait=false, so keeping the flash only inside the
 * command handler made every red impact frame disappear in older ports. */
static int g_transfer_white = 0;
static int g_screen_flash_r = 0;
static int g_screen_flash_g = 0;
static int g_screen_flash_b = 0;
static int g_screen_flash_a = 0;
static int g_screen_flash_total = 0;
static int g_screen_flash_remaining = 0;

#include "runtime/types.inc"
#include "runtime/helpers.inc"
#include "runtime/files.inc"
#include "runtime/graphics.inc"
#include "runtime/font_text.inc"
#include "runtime/input.inc"
#include "runtime/audio.inc"
#include "runtime/progress.inc"
#include "runtime/ui_menus.inc"
#include "runtime/map_load.inc"
#include "runtime/vm_pages.inc"
#include "runtime/world_draw.inc"
#include "runtime/event_routes.inc"
#include "runtime/perf_hud.inc"
#include "runtime/world_frame.inc"
#include "runtime/vm_text.inc"
#include "runtime/vm_interpreter.inc"
#include "runtime/messages.inc"

/* ------------------------------------------------------------------------- */
/* Main                                                                      */
/* ------------------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    int lang;
    int title_choice;
    ProgressState progress;
    MapState map;
    void *char_atlas;
    uint32_t boot_buttons;

    int tile_x = 0;
    int tile_y = 0;
    int target_x = 0;
    int target_y = 0;
    int direction_row = 0;
    int moving = 0;
    int move_dx = 0;
    int move_dy = 0;
    float move_progress = 0.0f;
    float current_move_speed = move_speed_px_per_frame(3);
    int current_real_move_speed = 3;
    int walk_finish_hold = 0;

    SceCtrlData pad;
    uint32_t prev_buttons = 0;

    (void)argc;
    boot_diag_init(argv && argc > 0 ? argv[0] : NULL);
    boot_diag("01 main entered");
    (void)argv;

    /* Homebrew starts at 222 MHz; the full 333 MHz leaves headroom for zlib
     * streaming and drawing on real hardware. Emulators ignore the setting. */
    scePowerSetClockFrequency(333, 333, 166);

    memset(&map, 0, sizeof(map));
    reset_runtime_game_state();
    boot_diag("02 state reset");

    setup_callbacks();
    boot_diag("03 callbacks ready");
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    boot_buttons = read_boot_hotkeys();
    boot_diag("04 controls sampled");

    /* SELECT at boot remains only as an emergency language fallback.  The
     * normal language control now lives in NARAKU's Options scene.  The old
     * TRIANGLE "delete progress" shortcut is intentionally gone: the PC game
     * has no equivalent and 0.6.0 uses explicit save slots. */
    lang = load_saved_language();
    boot_diag("05 config loaded");
    if (lang < 0) {
        /* Fresh installs start in English.  Language selection is an Option,
         * not a mandatory first-boot wizard in the PSP port. */
        g_language = 0;
        save_config();
        lang = 0;
    }
    if (boot_buttons & PSP_CTRL_SELECT)
        lang = choose_language(lang);
    g_language = lang;

    boot_diag("06 loading Enri atlas");
    char_atlas = texture_swizzle_loaded(load_exact_file(CHAR_ATLAS_PATH, CHAR_ATLAS_BYTES), CHAR_ATLAS_W, CHAR_ATLAS_H);
    if (!char_atlas) fatal_error("Could not load enri_atlas.rgba8888");
    boot_diag("06a Enri atlas loaded");
    g_ui_char_atlas = char_atlas;

    boot_diag("07 loading A1 overlay");
    g_a1_overlay = texture_swizzle_loaded(load_exact_file(A1_PATH, PIC_TEX_BYTES), PIC_TEX_W, PIC_TEX_H);
    if (!g_a1_overlay) fatal_error("Could not load a1_overlay.rgba8888");
    boot_diag("07a A1 overlay loaded");
    boot_diag("08 loading fog overlay");
    g_fog_overlay = texture_swizzle_loaded(load_exact_file(FOG_A1_PATH, FOG_A1_BYTES), 512, 512);
    if (!g_fog_overlay) fatal_error("Could not load fog_a1.rgba8888");
    boot_diag("08a fog overlay loaded");
    boot_diag("09 loading key animation");
    g_key_anim = load_exact_file(KEY_ANIM_PATH, KEY_ANIM_BYTES);
    if (!g_key_anim) fatal_error("Could not load key_anim.rgba8888");
    boot_diag("09a key animation loaded");

    boot_diag("11a base textures loaded");
    init_gu();
    boot_diag("12 GU initialized");
    if (!init_runtime_font())
        fatal_error("Could not load v0.7.9 runtime font assets");
    boot_diag("13 font loaded");
    if (!init_russian())
        fatal_error("Could not load russian177.bin");
    boot_diag("14 Russian data loaded");
    if (!init_ui_data())
        fatal_error("Could not load v0.7.9 UI assets");
    boot_diag("15 UI loaded");
    load_common_vm();
    boot_diag("16 common VM loaded");
    set_bgm_track(TITLE_BGM_PATH);
    start_bgm();
    boot_diag("17 audio thread started");

TITLE_ENTRY:
    g_request_title = 0;
    g_loaded_from_scene = 0;
    moving = 0;
    move_progress = 0.0f;
    player_animation_reset(&g_player_animation);

    boot_diag("18 entering title menu");
    title_choice = ui_title_scene(&progress);
    lang = g_language;

    if (title_choice == 0) {
        /* Shutdown is a real terminal title command.  Stop audio first, give
         * the confirmation SE a moment to reach the mixer, then ask PSP/PPSSPP
         * to terminate the game.  Most importantly, never continue into the
         * save-load path with an uninitialised ProgressState. */
        g_audio_stop = 1;
        sceKernelDelayThread(120000);
        sceKernelExitGame();
        return 0;
    }

    if (title_choice == 1) {
        const int start_x = 12;
        const int start_y = 9;

        reset_runtime_game_state();
        /* System.json has optTransparent=true.  The original starts Map001
         * with the real Player hidden and uses Event 5 as the falling Enri
         * sprite; command211[1] restores the Player near the end of the intro. */
        g_player_visible = 0;
        if (!load_map(&map, 1))
            fatal_error("Could not load Map001 v0.7.9 assets");

        /* 0.6.9 no longer re-enacts the opening in hand-written C.  The real
         * Map001 autorun is now executed by the VM together with its switch
         * driven event sprites, so the fall, pauses and post-fall pose follow
         * the original event data instead of a second approximation. */
        g_player_move_speed_code = 3;
        progress.map_id = 1;
        progress.tile_x = start_x;
        progress.tile_y = start_y;
        progress.direction_row = 0;
        progress.intro_done = 0;
    } else {
        /* ui_title_scene() has already restored switches/variables/items and
         * player state from the selected manual slot. */
        if (!load_map(&map, progress.map_id))
            fatal_error("Could not load selected save map assets");
    }

    tile_x = progress.tile_x;
    tile_y = progress.tile_y;
    direction_row = progress.direction_row;
    if (tile_x < 0 || tile_x >= map.w || tile_y < 0 || tile_y >= map.h) {
        tile_x = 0;
        tile_y = 0;
    }

    vm_run_autoruns(&map, char_atlas, lang, &tile_x, &tile_y, &direction_row);
    lang = g_language;
    if (g_request_title) goto TITLE_ENTRY;
    g_loaded_from_scene = 0;

    target_x = tile_x;
    target_y = tile_y;
    moving = 0;
    move_dx = 0;
    move_dy = 0;
    move_progress = 0.0f;

    sceCtrlPeekBufferPositive(&pad, 1);
    prev_buttons = pad.Buttons;

    while (1) {
        uint32_t pressed;
        walk_finish_hold = 0;
        sceCtrlPeekBufferPositive(&pad, 1);
        pressed = pad.Buttons & ~prev_buttons;

        if (g_stage_message_event) {
            vm_pump_stage_message(&map,char_atlas,tile_x,tile_y,direction_row);
            sceCtrlPeekBufferPositive(&pad,1);prev_buttons=pad.Buttons;pressed=0;
        }
        vm_tick_story_parallel(&map);

        if (!moving && !g_player_route.records) {
            uint8_t mask = pass_mask_at(&map, tile_x, tile_y);
            int bit = 0;
            int dx = 0;
            int dy = 0;
            int row = direction_row;

            if (pad.Buttons & PSP_CTRL_DOWN) {
                bit = 0x01; dy = 1; row = 0;
            } else if (pad.Buttons & PSP_CTRL_LEFT) {
                bit = 0x02; dx = -1; row = 1;
            } else if (pad.Buttons & PSP_CTRL_RIGHT) {
                bit = 0x04; dx = 1; row = 2;
            } else if (pad.Buttons & PSP_CTRL_UP) {
                bit = 0x08; dy = -1; row = 3;
            }

            if (bit != 0) {
                int nx = tile_x + dx;
                int ny = tile_y + dy;
                if (!g_player_direction_fix) direction_row = row;

                {
                    int inside = (nx >= 0 && ny >= 0 && nx < map.w && ny < map.h);
                    int reverse_bit = bit == 0x01 ? 0x08 :
                                      bit == 0x08 ? 0x01 :
                                      bit == 0x02 ? 0x04 : 0x02;
                    uint8_t target_mask = inside ? pass_mask_at(&map, nx, ny) : 0;
                    int can_enter = inside && (
                        g_player_through ||
                        ((mask & bit) && (target_mask & reverse_bit) &&
                         !vm_event_blocks_at(&map, nx, ny))
                    );

                    if (inside && is_chase_map(map.id) && !g_s003_paused &&
                        vm_run_event_at(&map,nx,ny,2,char_atlas,lang,&tile_x,&tile_y,&direction_row)) {
                        moving=0; move_progress=0; target_x=tile_x; target_y=tile_y;
                        vm_run_autoruns(&map,char_atlas,lang,&tile_x,&tile_y,&direction_row);
                        if(g_request_title) goto TITLE_ENTRY;
                        continue;
                    }
                    if (inside && !can_enter &&
                        vm_run_event_at(&map,nx,ny,-1,char_atlas,lang,&tile_x,&tile_y,&direction_row)) {
                        moving=0; move_progress=0; target_x=tile_x; target_y=tile_y;
                        vm_run_autoruns(&map,char_atlas,lang,&tile_x,&tile_y,&direction_row);
                        if(g_request_title) goto TITLE_ENTRY;
                        continue;
                    }
                    if (can_enter) {
                        int dash_button = (pad.Buttons & PSP_CTRL_RTRIGGER) != 0;
                        int dashing = g_always_dash ? !dash_button : dash_button;
                        target_x = nx;
                        target_y = ny;
                        move_dx = dx;
                        move_dy = dy;
                        move_progress = 0.0f;
                        current_real_move_speed = player_real_move_speed(dashing);
                        current_move_speed = move_speed_px_per_frame(current_real_move_speed);
                        moving = 1;
                    }
                }
            }

            /* MenuCallCommon is active in the original project.  The RPG Maker
             * cancel/menu input invokes Common Event 1, whose real command list
             * plays 【魔王魂】-SE1 and pushes Scene_Item. */
            if (!moving && !g_player_route.records && (pressed & ui_cancel_mask())) {
                VmEventRecord menu_event;
                memset(&menu_event, 0, sizeof(menu_event));
                /* Capture before Common Event 1 can show its menu pictures.
                 * Every menu opening takes a fresh snapshot of this location. */
                free(g_menu_world_backdrop);
                g_menu_world_backdrop = ui_inventory_backdrop();
                vm_call_common(
                    &map, &menu_event, 1, char_atlas, lang,
                    &tile_x, &tile_y, &direction_row
                );
                free(g_menu_world_backdrop);
                g_menu_world_backdrop = NULL;
                lang = g_language;
                if (g_request_title) goto TITLE_ENTRY;
                if (g_loaded_from_scene) {
                    g_loaded_from_scene = 0;
                    vm_run_autoruns(&map, char_atlas, lang, &tile_x, &tile_y, &direction_row);
                    lang = g_language;
                    if (g_request_title) goto TITLE_ENTRY;
                }
                target_x = tile_x;
                target_y = tile_y;
                sceCtrlPeekBufferPositive(&pad, 1);
                prev_buttons = pad.Buttons;
                continue;
            } else if (!moving && !g_player_route.records && (pressed & ui_ok_mask())) {
                int fx = tile_x;
                int fy = tile_y;
                int vm_result;

                /* RPG Maker checks the current cell for below-character action
                 * events, then the cell in front of the player. */
                vm_result = vm_run_event_at(
                    &map, fx, fy, -2, char_atlas, lang,
                    &tile_x, &tile_y, &direction_row
                );

                if (!vm_result) {
                    if (direction_row == 0) fy++;
                    else if (direction_row == 1) fx--;
                    else if (direction_row == 2) fx++;
                    else if (direction_row == 3) fy--;
                    vm_result = vm_run_event_at(
                        &map, fx, fy, -3, char_atlas, lang,
                        &tile_x, &tile_y, &direction_row
                    );
                }

                if (!vm_result && counter_at(&map, fx, fy)) {
                    if (direction_row == 0) fy++;
                    else if (direction_row == 1) fx--;
                    else if (direction_row == 2) fx++;
                    else if (direction_row == 3) fy--;
                    vm_result = vm_run_event_at(
                        &map, fx, fy, -3, char_atlas, lang,
                        &tile_x, &tile_y, &direction_row
                    );
                }

                lang = g_language;
                if (g_request_title) goto TITLE_ENTRY;

                if (vm_result) {
                    target_x = tile_x;
                    target_y = tile_y;
                    g_loaded_from_scene = 0;
                    vm_run_autoruns(&map, char_atlas, lang, &tile_x, &tile_y, &direction_row);
                    lang = g_language;
                    if (g_request_title) goto TITLE_ENTRY;
                    target_x = tile_x;
                    target_y = tile_y;
                    sceCtrlPeekBufferPositive(&pad, 1);
                    prev_buttons = pad.Buttons;
                    continue;
                }
            }
        }

        if (moving) {
            move_progress += current_move_speed;

            if (move_progress >= TILE_PX) {
                uint8_t transfer_index;
                tile_x = target_x;
                tile_y = target_y;
                move_progress = 0.0f;
                moving = 0;
                /* Keep the current walk cel for the tile-arrival frame.
                 * Older builds snapped to the standing cel for one frame at
                 * every tile boundary, which made held movement look jerky. */
                walk_finish_hold = 1;

                {
                    unsigned int frames_before_touch = g_world_render_frames;
                    int map_before_touch = map.id;
                    int vm_touch = vm_run_event_at(
                        &map, tile_x, tile_y, 1, char_atlas, lang,
                        &tile_x, &tile_y, &direction_row
                    );
                    if (!vm_touch) {
                        vm_touch = vm_run_event_at(
                            &map, tile_x, tile_y, 2, char_atlas, lang,
                            &tile_x, &tile_y, &direction_row
                        );
                    }
                    lang = g_language;
                    if (g_request_title) goto TITLE_ENTRY;
                    if (vm_touch) {
                        target_x = tile_x;
                        target_y = tile_y;
                        g_loaded_from_scene = 0;
                        vm_run_autoruns(&map, char_atlas, lang, &tile_x, &tile_y, &direction_row);
                        lang = g_language;
                        if (g_request_title) goto TITLE_ENTRY;
                        target_x = tile_x;
                        target_y = tile_y;
                        if (map.id != map_before_touch)
                            player_animation_reset(&g_player_animation);
                        /* Instant floor/speed/SE events must still present the
                         * tile-arrival frame. A modal event already rendered it. */
                        if (g_world_render_frames == frames_before_touch) {
                            g_player_animation_speed = current_real_move_speed;
                            g_player_animation_jumping = 1; /* stop count still zero */
                            render_world(&map, char_atlas,
                                g_player_visible, (tile_x + 0.5f) * TILE_PX,
                                (tile_y + 1.0f) * TILE_PX, 1, direction_row, 0, 0);
                        }
                        sceCtrlPeekBufferPositive(&pad, 1);
                        prev_buttons = pad.Buttons;
                        continue;
                    }
                }

                transfer_index = vm_cell_owns_transfer(&map, tile_x, tile_y)
                    ? 0 : transfer_index_at(&map, tile_x, tile_y);
                if (transfer_index) {
                    TransferRecord tr;
                    if (read_transfer_record(&map, transfer_index, &tr)) {
                        float old_x = ((float)tile_x + 0.5f) * TILE_PX;
                        float old_y = ((float)tile_y + 1.0f) * TILE_PX;
                        int k;

                        if (tr.flags & 0x01)
                            play_se_async(SE_SPLASH_PATH);

                        if (tr.dest_map < 1 || tr.dest_map > MAX_MAP_ID) {
                            char msg[128];
                            snprintf(
                                msg, sizeof(msg),
                                "Map %03d could not be loaded by v0.7.9.",
                                tr.dest_map
                            );
                            fatal_error(msg);
                        }

                        fade_game_world(
                            &map, char_atlas,
                            old_x, old_y, 1, direction_row,
                            0, 255, 10
                        );

                        for (k = 0; k < 3; ++k) {
                            if (tr.switch_id[k] > 0 && tr.switch_id[k] < MAX_SWITCHES)
                                g_switches[tr.switch_id[k]] = (uint8_t)tr.switch_value[k];
                        }

                        if (!load_map(&map, tr.dest_map))
                            fatal_error("Transfer destination assets could not be loaded");

                        tile_x = tr.dest_x;
                        tile_y = tr.dest_y;
                        target_x = tile_x;
                        target_y = tile_y;

                        {
                            int new_row = direction_to_row(tr.direction);
                            if (new_row >= 0) direction_row = new_row;
                        }

                        {
                            float new_x = ((float)tile_x + 0.5f) * TILE_PX;
                            float new_y = ((float)tile_y + 1.0f) * TILE_PX;
                            fade_game_world(
                                &map, char_atlas,
                                new_x, new_y, 1, direction_row,
                                255, 0, 10
                            );
                        }

                        vm_run_autoruns(&map, char_atlas, lang, &tile_x, &tile_y, &direction_row);
                        lang = g_language;
                        if (g_request_title) goto TITLE_ENTRY;
                        g_loaded_from_scene = 0;
                        target_x = tile_x;
                        target_y = tile_y;
                        player_animation_reset(&g_player_animation);

                        sceCtrlPeekBufferPositive(&pad, 1);
                        prev_buttons = pad.Buttons;
                        continue;
                    }
                }
            }
        }

        /* The spider hold timer can expire while Enri is idle. Dispatch the
         * original death autorun as soon as its switch is raised. */
        if (((map.id==67 || map.id==68) && g_switches[831]) || (map.id==69 && g_switches[736])) {
            vm_run_autoruns(&map,char_atlas,lang,&tile_x,&tile_y,&direction_row);
            if (g_request_title) goto TITLE_ENTRY;
            continue;
        }
        if (is_chase_map(map.id) && !g_s003_paused) {
            /* MZ commits the player's logical destination when a step starts.
             * In the giant-spider corridor, checking the departing cell too
             * catches Enri after she has already escaped that contact tile. */
            int logical_contact = map.id==72 && moving;
            int contact = vm_run_event_at(&map,
                                          logical_contact ? target_x : tile_x,
                                          logical_contact ? target_y : tile_y,
                                          2,char_atlas,lang,&tile_x,&tile_y,&direction_row);
            if (!contact && moving && map.id!=72)
                contact=vm_run_event_at(&map,target_x,target_y,2,char_atlas,lang,
                                       &tile_x,&tile_y,&direction_row);
            if (contact) {
                moving=0; move_progress=0;
                target_x=tile_x; target_y=tile_y;
                vm_run_autoruns(&map,char_atlas,lang,&tile_x,&tile_y,&direction_row);
                if (g_request_title) goto TITLE_ENTRY;
                continue;
            }
        }

        {
            float world_x = ((float)tile_x + 0.5f) * TILE_PX;
            float world_y = ((float)tile_y + 1.0f) * TILE_PX;
            int pattern;

            if (moving) {
                world_x += move_dx * move_progress;
                world_y += move_dy * move_progress;
            }

            pattern = 1; /* render_world owns the persistent animation phase. */
            g_player_animation_moving = moving;
            g_player_animation_jumping = walk_finish_hold;
            g_player_animation_speed = (moving || walk_finish_hold)
                ? current_real_move_speed : clamp_move_speed_code(g_player_move_speed_code);

            render_world(
                &map, char_atlas,
                g_player_visible, world_x, world_y, pattern, direction_row, 0, 0);
        }

        prev_buttons = pad.Buttons;
    }

    return 0;
}
