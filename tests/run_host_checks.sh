#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$TEST_DIR"' EXIT
cc -std=c99 -Wall -Wextra -Werror -fsanitize=undefined "$ROOT_DIR/tests/player_animation_test.c" -o "$TEST_DIR/player_animation_test"
"$TEST_DIR/player_animation_test"
python3 "$ROOT_DIR/tests/test_routes.py"
python3 "$ROOT_DIR/tests/test_inventory_snapshot.py"
python3 "$ROOT_DIR/tests/test_market_game_root.py"
python3 "$ROOT_DIR/tests/test_market_asset_pack.py"
python3 "$ROOT_DIR/tests/test_pcm_reader_host.py"
python3 "$ROOT_DIR/tests/test_user_mode_link.py"
bash -n "$ROOT_DIR/tools/build_deploy.sh"
bash -n "$ROOT_DIR/tools/build_market.sh"
