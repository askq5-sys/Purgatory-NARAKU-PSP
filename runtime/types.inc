/* Basic data types shared by the runtime.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Basic types                                                               */
/* ------------------------------------------------------------------------- */

typedef struct {
    float u;
    float v;
    float x;
    float y;
    float z;
} TextureVertex;

typedef struct {
    float u;
    float v;
    uint32_t color;
    float x;
    float y;
    float z;
} TextureColorVertex;

typedef struct {
    uint32_t color;
    float x;
    float y;
    float z;
} ColorVertex;

typedef struct {
    char path[192];
    int generation;
    size_t max_bytes;
    size_t fade_bytes;
} SeJob;

typedef struct {
    int id;
    int w;
    int h;
    int tile_px;

    uint8_t *map_bin;
    size_t map_size;
    int tile_count;
    int tile_atlas_count;
    void *tile_atlases[MAX_TILE_ATLASES];

    uint8_t *events_bin;
    size_t events_size;
    int transfer_count;
    int sprite_count;
    void *event_atlas;
    void *event_atlas_detail;
    void *parallax_texture;

    const uint8_t *action_map;
    const uint8_t *step_map;
    const uint8_t *transfer_map;
    const uint8_t *block_map;
    const uint8_t *transfer_records;
    const uint8_t *sprite_records;

    uint8_t *vm_bin;
    size_t vm_size;
    int vm_event_count;
    int vm_page_count;
    const uint8_t *vm_events;
    const uint8_t *vm_pages;
    const uint8_t *vm_commands;
    size_t vm_command_size;
    uint32_t vm_flags;
} MapState;

typedef struct {
    int dest_map;
    int dest_x;
    int dest_y;
    int direction;
    int flags;
    int switch_id[3];
    int switch_value[3];
} TransferRecord;

typedef struct {
    int x;
    int y;
    int sx;
    int sy;
    int sw;
    int sh;
    int priority;
    int flags;
    int event_id;
} EventSpriteRecord;

typedef struct {
    int map_id;
    int tile_x;
    int tile_y;
    int direction_row;
    int intro_done;
} ProgressState;

typedef struct {
    uint8_t *map_bin;
    size_t map_size;
    int glyph_count;
    int page_count;
    void *pages[FONT_MAX_PAGES];
    uint32_t clut[256] __attribute__((aligned(16)));
} FontState;

typedef struct {
    int event_id;
    int x;
    int y;
    int first_page;
    int page_count;
} VmEventRecord;

typedef struct {
    int cond_flags;
    int trigger;
    int priority;
    int flags;
    int switch1;
    int switch2;
    int self_index;
    int supported;
    int sprite_ref;
    uint32_t cmd_offset;
    uint32_t cmd_size;
} VmPageRecord;

static FontState g_font;

static const char *g_runtime_speaker = NULL;
static size_t g_runtime_speaker_len = 0;
static const char *g_runtime_message = NULL;
static size_t g_runtime_message_len = 0;
static int g_pressing_room_frame = 0;
static int g_laser_room_frame = 0, g_laser_stop_count = 0, g_laser_route_index = 0;
static int g_runtime_message_active = 0;
static int g_runtime_message_transparent = 0;
static int g_runtime_message_background = 0, g_runtime_message_position = 2;
static int g_choice_background = 0, g_choice_position = 2;
static int g_runtime_message_openness = 0;
