#!/usr/bin/env python3
"""Draw the lower half of dark doorway gradients below characters.

Tileset 8 (B sheet 死体集1) tiles 155/156 are the bottom row of a black,
semi-transparent doorway gradient flagged ☆ (above characters).  A 48x78
character standing on the floor cell below the doorway reaches into that row
with its head, so the head is dimmed even though the character has not entered
the doorway.  This clears the PSP "upper" bit (0x8000) only on those cells.
The top gradient row (147/148) stays above characters, so walking into the
doorway still darkens the character.

Usage: fix_doorway_gradient_priority.py ORIGINAL_DATA_DIR PSP_ASSETS_DIR
Idempotent: cells already patched are reported and left unchanged.
"""
import json, struct, sys
from pathlib import Path

TILESET_ID = 8
TILE_IDS = (155, 156)
HEADER = 20


def main(data_dir, assets_dir):
    data_dir, assets_dir = Path(data_dir), Path(assets_dir)
    tilesets = json.loads((data_dir / "Tilesets.json").read_text(encoding="utf-8-sig"))
    flags = tilesets[TILESET_ID]["flags"]
    total = 0
    for src in sorted(data_dir.glob("Map[0-9][0-9][0-9].json")):
        m = json.loads(src.read_text(encoding="utf-8-sig"))
        if m["tilesetId"] != TILESET_ID:
            continue
        w, h, d = m["width"], m["height"], m["data"]
        cells = [(z, x, y) for z in range(4) for y in range(h) for x in range(w)
                 if d[(z * h + y) * w + x] in TILE_IDS and flags[d[(z * h + y) * w + x]] & 0x10]
        if not cells:
            continue
        map_id = int(src.stem[3:])
        dst = assets_dir / f"map{map_id:03d}.bin"
        b = bytearray(dst.read_bytes())
        if b[:4] != b"NM40" or struct.unpack_from("<HH", b, 4) != (w, h):
            sys.exit(f"{dst.name}: header/size mismatch with {src.name}")
        changed = 0
        for z, x, y in cells:
            off = HEADER + ((z * h + y) * w + x) * 2
            word = struct.unpack_from("<H", b, off)[0]
            if word == 0:
                sys.exit(f"{dst.name}: empty word at z{z} ({x},{y}), layout differs")
            if word & 0x8000:
                struct.pack_into("<H", b, off, word & 0x7FFF)
                changed += 1
        if changed:
            dst.write_bytes(b)
        print(f"{dst.name}: {len(cells)} cell(s), patched {changed}")
        total += changed
    print(f"done, {total} word(s) changed")


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
