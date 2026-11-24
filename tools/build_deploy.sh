#!/usr/bin/env bash
# Build the EBOOT and install it, together with the changed assets listed in
# tools/assets_patch.txt, into an existing game folder (PPSSPP by default).
# Usage: tools/build_deploy.sh [GAME_DIR]
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DEST="${1:-/mnt/c/Users/Rgood/Documents/PPSSPP/PSP/GAME/NARAKU}"
test -d "$DEST/assets" || { echo "Game folder has no assets/: $DEST" >&2; exit 1; }
mkdir -p "$ROOT_DIR/assets"

# Keep installed sprite references and commands from earlier stages.
for src in "$DEST/assets"/map*_vm.bin "$DEST/assets/common_vm.bin"; do
    test -f "$src" || continue
    name="${src##*/}"
    test -f "$ROOT_DIR/assets/$name" || cp "$src" "$ROOT_DIR/assets/$name"
done
python3 "$ROOT_DIR/tools/update_window_styles_v174.py" "$ROOT_DIR/assets"

mkdir -p "$ROOT_DIR/build"
cd "$ROOT_DIR/build"
psp-cmake ..
make -B -j"$(nproc)"
test -s EBOOT.PBP
python3 "$ROOT_DIR/tools/check_eboot_imports.py" EBOOT.PBP

while IFS= read -r name; do
    test -n "$name" || continue
    test -s "$ROOT_DIR/assets/$name"
    cp "$ROOT_DIR/assets/$name" "$DEST/assets/$name"
done < "$ROOT_DIR/tools/assets_patch.txt"
for src in "$ROOT_DIR/assets"/map*_vm.bin "$ROOT_DIR/assets/common_vm.bin"; do
    test -f "$src" || continue
    cp "$src" "$DEST/assets/${src##*/}"
done
cp EBOOT.PBP "$DEST/EBOOT.PBP"
echo "NARAKU deployed to $DEST. Existing saves and config are retained."
