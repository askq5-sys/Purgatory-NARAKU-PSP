"""Installed map/common scripts carry the converter's current text and sounds.

tools/refresh_vm_text.py (run by tools/build_deploy.sh) updates pages whose
text or exact sound IDs are out of date. Only the pages listed here differ in
their commands and stay as installed on purpose.
"""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from pathlib import Path
from source_util import original_dir

r = Path(__file__).resolve().parents[1]
_sys.path.insert(0, str(r / 'tools'))
import refresh_vm_text as R  # noqa: E402

KEPT = {
    # Parallel crusher loop the installed build leaves unsupported.
    'map024 ev12 p0',
    # Fourth lever page added by adapt_stage_142 (see tests/vm_util.py).
    'map055 ev1 p0', 'map055 ev1 p1', 'map055 ev1 p2',
}
game = original_dir(r)
assets = r / 'assets'
texts = R.load_texts(game, assets)
report = []
for path in sorted(assets.glob('map*_vm.bin')):
    R.refresh_map(path, game, texts, report)
R.refresh_common(assets / 'common_vm.bin', game, texts, report)
outdated = [k for k, v in report if v in R.SAFE]
assert not outdated, f'run tools/refresh_vm_text.py: {outdated[:5]}'
kept = {k for k, v in report if v != 'same'}
assert kept == KEPT, sorted(kept ^ KEPT)

# Russian is looked up by the normalized English text of each message.
import struct
import prepare_russian_v177 as ru  # noqa: E402
dic = (assets / 'russian177.bin').read_bytes()
assert dic[:4] == b'NR77'
keys = set()
for i in range(struct.unpack_from('<I', dic, 4)[0]):
    at = struct.unpack_from('<I', dic, 8 + i * 12)[0]
    keys.add(dic[at:dic.index(b'\0', at)].decode())
ru.ROOT = r
missing = {s for s in ru.installed_strings() if ru.norm(s) and ru.norm(s) not in keys}
assert not missing, sorted(missing)[:5]
print(f'PASS: {len(report)} installed pages match the converter text and sounds; '
      f'every English line has a Russian entry')
