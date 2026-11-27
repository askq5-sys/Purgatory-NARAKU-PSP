#!/usr/bin/env python3
"""Write the converter manifests that host tests read from assets/.

The game itself does not ship these files. They are derived from the PC
game data without converting or changing any game asset:
  picture_v050_manifest.json  picture name -> pic_NNN.p44 id
  portraits_v132.json         geometry of the cropped dialogue portraits

Usage: tools/prepare_test_data.py PC_GAME_DIR [ASSETS_DIR]
PC_GAME_DIR is the folder that holds data/ and img/ of the PC version.
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import prepare_world_v060 as w  # noqa: E402
from prepare_portraits_v132 import is_portrait  # noqa: E402
from PIL import Image  # noqa: E402


def portrait_report(game, manifest):
    key = bytes.fromhex(w.read_json(game / 'data/System.json')['encryptionKey'])
    report = []
    for name, rid in manifest.items():
        if not is_portrait(name):
            continue
        src = w.resolve_named_file(game / 'img/pictures', name, ('.png_', '.png'))
        image = (w.load_encrypted_png(src, key) if src.name.endswith('.png_')
                 else Image.open(src).convert('RGBA'))
        bbox = image.getchannel('A').getbbox()
        if bbox is None:
            continue
        bw, bh = bbox[2] - bbox[0], bbox[3] - bbox[1]
        scale = min(1, 510 / bw, 510 / bh)
        tw, th = max(1, round(bw * scale)), max(1, round(bh * scale))
        logical = (max(1, round(image.width * .5)), max(1, round(image.height * .5)))
        report.append({'id': rid, 'name': name, 'canvas': image.size, 'logical': logical,
                       'bounds': bbox, 'texture_region': [tw, th],
                       'display_region': [bw * .5, bh * .5]})
    return report


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    game = Path(sys.argv[1]).resolve()
    out = Path(sys.argv[2] if len(sys.argv) == 3 else 'assets').resolve()
    manifest = w._collect_picture_ids_without_conversion(game)
    (out / 'picture_v050_manifest.json').write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    report = portrait_report(game, manifest)
    (out / 'portraits_v132.json').write_text(json.dumps(report, ensure_ascii=False, indent=2))
    print(f'pictures: {len(manifest)}, portraits: {len(report)}')


if __name__ == '__main__':
    main()
