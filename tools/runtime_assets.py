#!/usr/bin/env python3
"""Which files in a game assets/ folder the EBOOT can load.

The names come from the runtime source itself: every string literal in
main.c and runtime/ that names a file, with printf fields (%03d, %d, %s)
turned into wildcards. Compressed copies (NAME.z) count as NAME.

Older builds left pre-rendered dialog pages, the hand-made opening and other
leftovers in assets/; nothing loads them any more. Once the audio has been
converted (tools/pack_audio.py), the original .pcm files are unused too.

Usage:
  python3 tools/runtime_assets.py GAME/assets            list unused files
  python3 tools/runtime_assets.py GAME/assets --move-to DIR
                                                         move them into DIR
"""
from __future__ import annotations

import argparse
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXTENSIONS = ('rgba8888', 'rgba4444', 'pcm', 'bin', 't8', 'p44')
_LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')
_NAME = re.compile(r'^[A-Za-z0-9_%]+\.(?:' + '|'.join(EXTENSIONS) + ')$')


def _sources(root: Path):
    yield root / 'main.c'
    yield from sorted((root / 'runtime').glob('*.inc'))
    yield from sorted((root / 'runtime').glob('*.h'))


def _wildcard(name: str) -> str:
    out = re.escape(name)
    out = re.sub(r'%0(\d)d', lambda m: '[0-9]{%s}' % m[1], out)
    return out.replace('%d', '[0-9]+').replace('%s', '[a-z]+')


def asset_pattern(root: Path = ROOT) -> re.Pattern:
    names = set()
    for path in _sources(root):
        for literal in _LITERAL.findall(path.read_text(encoding='utf-8')):
            if _NAME.match(literal):
                names.add(literal)
    if not names:
        raise RuntimeError(f'no asset names found under {root}')
    return re.compile('^(?:%s)$' % '|'.join(sorted(map(_wildcard, names))))


def is_runtime_asset(name: str, pattern: re.Pattern, folder: Path | None = None) -> bool:
    """True if the EBOOT can load `name`. With `folder`, a NAME.pcm (or
    NAME.pcm.z) whose NAME.nau is there counts as unused: the EBOOT plays the
    .nau (tools/pack_audio.py) instead."""
    if name.endswith('.z'):
        name = name[:-2]
    if name.endswith('.nau'):
        return bool(pattern.match(name[:-4] + '.pcm'))
    if folder is not None and name.endswith('.pcm') and (folder / (name[:-4] + '.nau')).is_file():
        return False
    return bool(pattern.match(name))


def unused_assets(assets: Path, pattern: re.Pattern | None = None) -> list[Path]:
    pattern = pattern or asset_pattern()
    return [p for p in sorted(assets.iterdir())
            if p.is_file() and not is_runtime_asset(p.name, pattern, assets)]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('assets', type=Path)
    parser.add_argument('--move-to', type=Path,
                        help='move unused files here instead of only listing them')
    args = parser.parse_args()
    unused = unused_assets(args.assets)
    size = sum(p.stat().st_size for p in unused)
    groups: dict[str, list[int]] = {}
    for p in unused:
        key = re.sub(r'[0-9]+', 'N', p.name)
        groups.setdefault(key, [0, 0])
        groups[key][0] += 1
        groups[key][1] += p.stat().st_size
    for key, (count, total) in sorted(groups.items()):
        print(f'{count:5}  {total / 1048576:8.1f} MB  {key}')
    print(f'{len(unused)} unused files, {size / 1048576:.1f} MB')
    if args.move_to and unused:
        args.move_to.mkdir(parents=True, exist_ok=True)
        for p in unused:
            shutil.move(str(p), str(args.move_to / p.name))
        print(f'moved to {args.move_to}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
