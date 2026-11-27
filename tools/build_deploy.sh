#!/usr/bin/env bash
# Build the EBOOT and install it into an existing game folder (PPSSPP by default).
#
# Assets listed in tools/assets_patch.txt are copied from the repository's
# assets/ folder when it has them; otherwise the copy already installed in the
# game folder (plain or compressed .z) is kept. Without an installed copy the
# script stops.
#
# When the PC game's data folder is available, the doorway shading fix is
# applied to the installed maps and outdated script text is refreshed
# (tools/refresh_vm_text.py). It is looked up in $NARAKU_ORIGINAL_DATA,
# original/data and original/www/data.
#
# Audio in the game folder is compressed losslessly (tools/pack_audio.py,
# about 4.6x smaller); already converted sounds are skipped.
#
# Usage: tools/build_deploy.sh [GAME_DIR]
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-/mnt/c/Users/Rgood/Documents/PPSSPP/PSP/GAME/NARAKU}"
test -d "$DEST/assets" || { echo "Game folder has no assets/: $DEST" >&2; exit 1; }

# Check the asset list before building or changing anything.
missing=0
while IFS= read -r name; do
    test -n "$name" || continue
    if [[ ! -s "$ROOT_DIR/assets/$name" && ! -s "$DEST/assets/$name" && ! -s "$DEST/assets/$name.z" &&
          ! ( "$name" == *.pcm && -s "$DEST/assets/${name%.pcm}.nau" ) ]]; then
        echo "Missing asset (not in assets/ nor in the game folder): $name" >&2
        missing=1
    fi
done < "$ROOT_DIR/tools/assets_patch.txt"
test "$missing" = 0

mkdir -p "$ROOT_DIR/build"
cd "$ROOT_DIR/build"
# Compressed-asset support also reads plain files, so one EBOOT fits both layouts.
# Options are passed every time so a value cached by an earlier build cannot
# leak in: NARAKU_PERF_HUD=ON tools/build_deploy.sh builds the measuring EBOOT.
psp-cmake -DNARAKU_COMPRESSED_ASSETS=ON -DNARAKU_GAME_DIR=NARAKU \
    -DNARAKU_PERF_HUD="${NARAKU_PERF_HUD:-OFF}" \
    -DNARAKU_SWIZZLE_TEXTURES="${NARAKU_SWIZZLE_TEXTURES:-OFF}" \
    -DNARAKU_BOOT_DIAGNOSTICS="${NARAKU_BOOT_DIAGNOSTICS:-OFF}" ..
make -B -j"$(nproc)"
test -s EBOOT.PBP
python3 "$ROOT_DIR/tools/check_eboot_imports.py" EBOOT.PBP

copied=0
if [[ -d "$ROOT_DIR/assets" ]]; then
    while IFS= read -r name; do
        test -n "$name" || continue
        test -s "$ROOT_DIR/assets/$name" || continue
        # Unchanged files keep their date, so converted audio is not redone.
        cmp -s "$ROOT_DIR/assets/$name" "$DEST/assets/$name" && continue
        cp "$ROOT_DIR/assets/$name" "$DEST/assets/$name"
        rm -f "$DEST/assets/$name.z"
        copied=$((copied + 1))
    done < "$ROOT_DIR/tools/assets_patch.txt"
    for src in "$ROOT_DIR/assets"/map*_vm.bin "$ROOT_DIR/assets/common_vm.bin"; do
        test -f "$src" || continue
        cp "$src" "$DEST/assets/${src##*/}"
        copied=$((copied + 1))
    done
fi
echo "Assets copied from the repository: $copied"

# Window styles are idempotent; apply them to the installed scripts.
python3 "$ROOT_DIR/tools/update_window_styles_v174.py" "$DEST/assets"

original_data=""
for dir in "${NARAKU_ORIGINAL_DATA:-}" "$ROOT_DIR/original/data" "$ROOT_DIR/original/www/data"; do
    if [[ -n "$dir" && -f "$dir/Tilesets.json" ]]; then original_data="$dir"; break; fi
done
if [[ -n "$original_data" ]]; then
    python3 "$ROOT_DIR/tools/fix_doorway_gradient_priority.py" "$original_data" "$DEST/assets"
    python3 "$ROOT_DIR/tools/refresh_vm_text.py" "$(dirname "$original_data")" "$DEST/assets"
else
    echo "PC game data not found; doorway fix and script text refresh skipped (set NARAKU_ORIGINAL_DATA to its data folder)."
fi

if python3 -c "import numpy" 2>/dev/null; then
    python3 "$ROOT_DIR/tools/pack_audio.py" "$DEST/assets"
else
    echo "numpy not installed; audio conversion skipped (pip install -r requirements.txt)."
fi

cp EBOOT.PBP "$DEST/EBOOT.PBP"
echo "NARAKU deployed to $DEST. Existing saves and config are retained."
