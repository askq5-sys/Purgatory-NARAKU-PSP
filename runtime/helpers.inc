/* Small numeric helpers and the HOME exit callback.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Small helpers                                                             */
/* ------------------------------------------------------------------------- */

static uint16_t read_u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void write_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static float clamp_float(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

/* RPG Maker MZ rasterises its map/character/picture positions onto whole
 * screen pixels before Pixi finally draws them:
 *   Tilemap.updateTransform(): ox/oy = Math.ceil(origin.x/y)
 *   Game_CharacterBase.screenX/Y(): Math.floor(...)
 *   Sprite_Picture.updatePosition(): Math.round(picture.x/y)
 *
 * The PSP port used raw fractional GU coordinates instead.  With nearest
 * filtered tiles and the semi-transparent moving A1/Fog layers this causes a
 * one-pixel shimmer that is easiest to notice while an interaction freezes
 * the player and the world is otherwise still.  Keep these tiny helpers
 * local instead of depending on libm just for floor/ceil/round. */
static float render_floor_pixel(float value)
{
    int i = (int)value;
    if (value < 0.0f && (float)i != value) --i;
    return (float)i;
}

static float render_ceil_pixel(float value)
{
    int i = (int)value;
    if (value > 0.0f && (float)i != value) ++i;
    return (float)i;
}

static float render_round_pixel(float value)
{
    return value >= 0.0f
        ? (float)((int)(value + 0.5f))
        : (float)((int)(value - 0.5f));
}

static int clamp_move_speed_code(int speed_code)
{
    if (speed_code < 1) return 1;
    if (speed_code > 6) return 6;
    return speed_code;
}

static int player_real_move_speed(int dashing)
{
    int speed = clamp_move_speed_code(g_player_move_speed_code);
    if (dashing) speed += 1;
    /* MZ can therefore reach real speed 7 when base speed is 6. */
    if (speed > 7) speed = 7;
    return speed;
}

static float move_speed_px_per_frame(int real_speed)
{
    if (real_speed < 1) real_speed = 1;
    if (real_speed > 7) real_speed = 7;
    /* MZ: distancePerFrame = 2^realMoveSpeed / 256 tiles/frame. */
    return (float)TILE_PX * (float)(1 << real_speed) / 256.0f;
}

static int direction_to_row(int direction)
{
    if (direction == 2) return 0;
    if (direction == 4) return 1;
    if (direction == 6) return 2;
    if (direction == 8) return 3;
    return -1;
}

/* ------------------------------------------------------------------------- */
/* Exit callback                                                             */
/* ------------------------------------------------------------------------- */

static int exit_callback(int arg1, int arg2, void *common)
{
    (void)arg1;
    (void)arg2;
    (void)common;
    g_audio_stop = 1;
    sceKernelExitGame();
    return 0;
}

static int callback_thread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

static void setup_callbacks(void)
{
    int thid = sceKernelCreateThread(
        "Callback Thread", callback_thread, 0x11, 0xFA0, 0, NULL
    );
    if (thid >= 0) sceKernelStartThread(thid, 0, NULL);
}
