"""Bring the text of installed Event VM pages up to date with the converter.

Some installed map*_vm.bin pages were compiled before later English text fixes
in prepare_world_v060.localized_string.  A full --vm-only rebuild would also
replace hand-tuned page layouts (and needs the window-style pass afterwards),
so this tool works page by page:

  * every page is recompiled from the original RPG Maker data with the same
    adaptations the world builder applies, then styled like assets/;
  * a page is replaced only when the fresh code has the same commands in the
    same order and differs only in message/choice strings (jump targets are
    compared by command index, so a longer string is fine);
  * any other difference is reported and the installed page is kept.

Usage: python3 tools/refresh_vm_text.py ORIGINAL_GAME_DIR ASSETS_DIR [--check]
With --check nothing is written; the exit code is 1 if a page would change.
"""
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import json  # noqa: E402

import prepare_world_v060 as world  # noqa: E402
import update_window_styles_v174 as styles  # noqa: E402

META = json.loads((HERE / 'window_styles_v174.json').read_text())
TEXT_KINDS = (1, 22)


def shape(code):
    """Commands with jump targets as command indices and text kept apart."""
    records = styles.decode(code)
    index = {a: i for i, (a, _, _, _) in enumerate(records)}
    index[len(code)] = len(records)
    ops, texts = [], []
    for a, z, op, kind in records:
        if kind in TEXT_KINDS:
            head = 3 if op in (44, 45) else 1
            ops.append(('text', op, code[a:a + head]))
            texts.append(code[a + head:z])
            continue
        body = bytearray(code[a:z])
        if op in styles.JUMPS:
            at = 1 + styles.JUMPS[op]
            target = struct.unpack_from('<I', body, at)[0]
            struct.pack_into('<I', body, at, index[target])
        ops.append(('op', op, bytes(body)))
    return ops, texts


AUDIO_OPS = {6: 'se', 11: 'bgm'}
SAFE = ('text', 'audio')


def installed(assets, name):
    """True if the game folder has the file, plain or compressed."""
    return (assets / name).is_file() or (assets / (name + '.z')).is_file()


def compare(old, new, assets=None):
    """'same', 'text', 'audio' (both safe to replace) or a reason string.

    'audio' means a sound command now names an exact volume/pitch variant
    whose PCM file is installed, instead of the legacy ID an older compile
    used; nothing else about the command may change.
    """
    if old == new:
        return 'same'
    if old == b'\0' or new == b'\0':
        return 'supported state differs'
    try:
        a_ops, a_txt = shape(old)
        b_ops, b_txt = shape(new)
    except (ValueError, KeyError) as exc:
        return f'decode failed: {exc}'
    if len(a_ops) != len(b_ops):
        return f'command count {len(a_ops)} -> {len(b_ops)}'
    audio = False
    for i, (x, y) in enumerate(zip(a_ops, b_ops)):
        if x == y:
            continue
        kind = AUDIO_OPS.get(x[1])
        if (kind and x[1] == y[1] and x[2][3:] == y[2][3:] and assets is not None
                and installed(assets, f'{kind}_exact_{struct.unpack_from("<H", y[2], 1)[0]:04d}.pcm')):
            audio = True
            continue
        return f'command {i} differs (op {x[1]} -> {y[1]})'
    if audio:
        return 'audio'
    return 'text' if a_txt != b_txt else 'same'


def audio_ids(game):
    """Exact sound IDs as prepare_exact_audio_v170 numbers them.

    IDs are assigned over every sound variant in every map and common event,
    while assets/audio_exact_v079.json only lists the files it converted, so
    the manifest alone would compile later maps with sound 0.
    """
    keys = {'se': set(), 'bgm': set()}

    def visit(commands):
        for cmd in commands:
            code, p = cmd.get('code', 0), cmd.get('parameters', [])
            if code in (241, 250) and p and isinstance(p[0], dict):
                k = world.audio_key(p[0])
                if k[0]:
                    keys['bgm' if code == 241 else 'se'].add(k)
            if code == 205 and len(p) > 1:
                visit(p[1].get('list', []))
            if code == 44 and p and isinstance(p[0], dict):
                keys['se'].add(world.audio_key(p[0]))

    for path in sorted((game / 'data').glob('Map[0-9][0-9][0-9].json')):
        for ev in world.read_json(path).get('events', []):
            for pg in (ev or {}).get('pages', []):
                visit(pg.get('list', []))
    for ev in world.read_json(game / 'data' / 'CommonEvents.json'):
        if ev:
            visit(ev.get('list', []))
    return tuple({k: 1000 + i for i, k in enumerate(sorted(keys[kind]))}
                 for kind in ('se', 'bgm'))


def load_texts(game, assets):
    texts = world.read_json(game / 'data' / 'I18NTexts.json')
    # The game folder has no converter manifests; the IDs derive from the data.
    manifest = assets / 'picture_v050_manifest.json'
    world.PICTURE_IDS = ({str(k): int(v) for k, v in world.read_json(manifest).items()}
                         if manifest.is_file() else world._collect_picture_ids_without_conversion(game))
    world.EXACT_SE_IDS, world.EXACT_BGM_IDS = audio_ids(game)
    return texts


def adapted_map(game, map_id):
    """Map data exactly as prepare_world_v060 compiles its VM pages."""
    data = world.read_json(game / 'data' / f'Map{map_id:03d}.json')
    data = world.adapt_stage_152(world.adapt_stage_151(world.adapt_stage_150(
        world.adapt_stage_142(data, map_id), map_id), map_id), map_id)
    if map_id == 9:
        data = world.adapt_plant1_worm(data)
    return world.adapt_stage_151(world.adapt_stage_142(data, map_id), map_id)


def fresh_page(page, texts, map_id, event_id, page_index):
    if not world.page_vm_supported(page):
        return b'\0'
    code = world.compile_vm_commands(page, texts, preserve_message_background=True)
    meta = META.get(str(map_id))
    if meta is None:
        return code
    return styles.convert(code, meta.get(f'{event_id}:{page_index}', []))


def refresh_map(path, game, texts, report):
    map_id = int(path.stem[3:6])
    data = adapted_map(game, map_id)
    events = {int(ev.get('id', 0)): ev for ev in data.get('events', []) if ev}
    b = path.read_bytes()
    magic, ne, np_, blob, _, _ = struct.unpack_from('<4sHHIII', b)
    if magic != b'NV40':
        raise ValueError(f'{path}: not an NV40 file')
    base = 20 + ne * 10
    header = bytearray(b[:blob])
    out = bytearray()
    changed = 0
    for i in range(ne):
        eid, _, _, first, n, _ = struct.unpack_from('<HHHHBB', b, 20 + i * 10)
        ev = events.get(eid)
        src_pages = ev.get('pages', []) if ev else []
        for j in range(n):
            at = base + (first + j) * 20
            off, size = struct.unpack_from('<II', b, at + 12)
            old = b[blob + off:blob + off + size]
            code = old
            if len(src_pages) != n:
                verdict = f'page count {n} installed, {len(src_pages)} in data'
            else:
                verdict = compare(old, fresh_page(src_pages[j], texts, map_id, eid, j), path.parent)
            if verdict in SAFE:
                code = fresh_page(src_pages[j], texts, map_id, eid, j)
                changed += 1
            report.append((f'map{map_id:03d} ev{eid} p{j}', verdict))
            struct.pack_into('<II', header, at + 12, len(out), len(code))
            out += code
    struct.pack_into('<I', header, 12, len(out))
    return bytes(header + out), changed


def refresh_common(path, game, texts, report):
    common = world.read_json(game / 'data' / 'CommonEvents.json')
    b = path.read_bytes()
    magic, count, _ = struct.unpack_from('<4sHH', b)
    if magic != b'NC50' or count != len(common):
        raise ValueError(f'{path}: unexpected common event table')
    out = bytearray(b[:8 + count * 8])
    blob = bytearray()
    changed = 0
    meta = META['_common']
    for i, ce in enumerate(common):
        off, size = struct.unpack_from('<II', b, 8 + i * 8)
        if not size:
            continue
        old = b[off:off + size]
        cmds = {'list': ce.get('list', [])}
        new = styles.convert(world.compile_vm_commands(cmds, texts, preserve_message_background=True),
                             meta.get(str(i), []))
        verdict = compare(old, new, path.parent)
        code = new if verdict in SAFE else old
        changed += verdict in SAFE
        report.append((f'common {i}', verdict))
        struct.pack_into('<II', out, 8 + i * 8, len(out) + len(blob), len(code))
        blob += code
    # Offsets are absolute, so fix them up once the record table is final.
    return bytes(out + blob), changed


def main(argv):
    check = '--check' in argv
    args = [a for a in argv if a != '--check']
    if len(args) != 2:
        print(__doc__)
        return 2
    game, assets = Path(args[0]), Path(args[1])
    if not (game / 'data').is_dir() and (game / 'www' / 'data').is_dir():
        game = game / 'www'
    texts = load_texts(game, assets)
    report, pending, total = [], [], 0
    for path in sorted(assets.glob('map*_vm.bin')):
        data, changed = refresh_map(path, game, texts, report)
        if changed:
            pending.append((path, data))
            total += changed
    common = assets / 'common_vm.bin'
    if common.exists():
        data, changed = refresh_common(common, game, texts, report)
        if changed:
            pending.append((common, data))
            total += changed
    kept = [(k, v) for k, v in report if v not in ('same',) + SAFE]
    for key, verdict in report:
        if verdict in SAFE:
            print(f'  {verdict:5} {key}')
    for key, verdict in kept:
        print(f'  kept  {key}: {verdict}')
    print(f'{total} outdated pages in {len(pending)} files; '
          f'{len(kept)} pages kept because their commands differ')
    if check:
        return 1 if total else 0
    for path, data in pending:
        tmp = path.with_suffix('.refresh.tmp')
        tmp.write_bytes(data)
        tmp.replace(path)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
