#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SOURCE_GAME="${1:-/mnt/c/Users/Rgood/Documents/PPSSPP/PSP/GAME/NARAKU}"
DEST_GAME="${2:-/mnt/c/Users/Rgood/Documents/PPSSPP/PSP/GAME/NARAKU_MARKET}"

test -d "$SOURCE_GAME/assets"
test -f "$SOURCE_GAME/EBOOT.PBP"
source_real="$(realpath "$SOURCE_GAME")"
dest_real="$(realpath -m "$DEST_GAME")"
if [[ "$source_real" == "$dest_real" ]]; then
    echo "Source and market destination must be different directories." >&2
    exit 2
fi
case "$dest_real/" in
    "$source_real/"*)
        echo "Market destination cannot be inside the source game folder." >&2
        exit 2
        ;;
esac
case "$source_real/" in
    "$dest_real/"*)
        echo "Market destination cannot contain the source game folder." >&2
        exit 2
        ;;
esac

BUILD_DIR="$ROOT_DIR/build_market178"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
psp-cmake -DNARAKU_COMPRESSED_ASSETS=ON -DNARAKU_GAME_DIR=NARAKU ..
if ! grep -q 'NARAKU_GAME_DIR' CMakeFiles/naraku_psp.dir/flags.make; then
    echo "CMake did not embed the game folder name in the EBOOT. Update CMakeLists.txt before building." >&2
    exit 1
fi
make -B -j"$(nproc)"
test -s EBOOT.PBP

mkdir -p "$DEST_GAME"
STAGE="$(mktemp -d "$DEST_GAME/.market-stage.XXXXXX")"
trap 'rm -rf "$STAGE"' EXIT
python3 "$ROOT_DIR/tools/pack_market_assets.py" "$SOURCE_GAME/assets" "$STAGE/assets"
cp EBOOT.PBP "$STAGE/EBOOT.PBP"

# Replace generated assets and executable only; saves/config are untouched.
OLD_ASSETS="$DEST_GAME/.market-assets-old.$$"
if [[ -e "$DEST_GAME/assets" ]]; then
    mv "$DEST_GAME/assets" "$OLD_ASSETS"
fi
if ! mv "$STAGE/assets" "$DEST_GAME/assets"; then
    if [[ -e "$OLD_ASSETS" ]]; then mv "$OLD_ASSETS" "$DEST_GAME/assets"; fi
    echo "Could not install compressed assets." >&2
    exit 1
fi
rm -rf "$OLD_ASSETS"
mv -f "$STAGE/EBOOT.PBP" "$DEST_GAME/EBOOT.PBP"
echo "Compressed market build deployed to: $DEST_GAME"
