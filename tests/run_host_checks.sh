#!/usr/bin/env bash
# Quick checks that need no game data: run on a clean clone before pushing.
# tests/run_all_checks.sh also runs the tests that read assets/ and the PC game.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_DIR"' EXIT
cc -std=c99 -Wall -Wextra -Werror -fsanitize=undefined "$ROOT_DIR/tests/player_animation_test.c" -o "$TEST_DIR/player_animation_test"
"$TEST_DIR/player_animation_test"
for c in test_audit_158 test_laser_chase_154; do
    cc -std=gnu11 -O1 -I"$ROOT_DIR" "$ROOT_DIR/tests/$c.c" -o "$TEST_DIR/$c" -lm
    "$TEST_DIR/$c"
done
for t in routes inventory_snapshot market_game_root market_asset_pack pcm_reader_host \
         user_mode_link visible_tile_range book_save_136 cutscene_position_137 escape_gait \
         factory_depth flashback gallery_door_172 giant_contact_149 killer_contact \
         note_mask_136 notification_pause rope_138 save_preview_159 save_preview_170 \
         shop_loop_159 spider_exit_171 story_save_access transparent_143 tone_gray texture_swizzle refresh_vm_text; do
    python3 "$ROOT_DIR/tests/test_$t.py"
done
bash -n "$ROOT_DIR/tools/build_deploy.sh"
bash -n "$ROOT_DIR/tools/build_market.sh"
