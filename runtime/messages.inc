/* Runtime messages and screen fades.
 * Part of main.c: included there in order, not a standalone unit. */

/* ------------------------------------------------------------------------- */
/* Runtime messages / fades                                                  */
/* ------------------------------------------------------------------------- */

static void fade_game_world(
    const MapState *m, void *char_atlas,
    float player_x, float player_y,
    int player_pattern, int player_dir,
    int from, int to, int frames)
{
    int i;
    for (i = 1; i <= frames; ++i) {
        int a = from + (to - from) * i / frames;
        render_world(
            m, char_atlas,
            g_player_visible, player_x, player_y, player_pattern, player_dir, 0, a);
    }
}
