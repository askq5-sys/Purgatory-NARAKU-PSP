"""tools/refresh_vm_text.py replaces a page only when just its text changed."""
import struct
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import refresh_vm_text as R  # noqa: E402


def text(en):
    data = en.encode()
    return bytes([1]) + struct.pack('<8H', 0, len(data), 0, 0, 0, 0, 0, 0) + data


def page(en, extra=b''):
    """A message, a jump over a switch command, the switch, then the end."""
    head = text(en)
    switch = bytes([2]) + struct.pack('<HHB', 5, 5, 1)
    jump_at = len(head)
    target = jump_at + 5 + len(switch)
    return head + bytes([20]) + struct.pack('<I', target) + switch + extra + b'\0'


old, new = page('Obtained an Iron Key.'), page('Obtained a Green Key, longer.')
assert R.compare(old, old) == 'same'
# The jump moved with the longer text but still lands on the same command.
assert R.compare(old, new) == 'text'
assert R.compare(old, page('x', bytes([4]) + struct.pack('<H', 1))).startswith('command count')
bad = bytearray(new)
struct.pack_into('<I', bad, len(text('Obtained a Green Key, longer.')) + 1, 0)
assert R.compare(old, bytes(bad)).startswith('command'), 'retargeted jump must be kept'
assert R.compare(b'\0', new) == 'supported state differs'

# Sound IDs: accepted only when the exact variant is installed.
se = lambda i: bytes([6]) + struct.pack('<HBBB', i, 90, 100, 100) + b'\0'
with tempfile.TemporaryDirectory() as td:
    assets = Path(td)
    assert R.compare(se(19), se(1076), assets).startswith('command')
    (assets / 'se_exact_1076.pcm.z').write_bytes(b'x')
    assert R.compare(se(19), se(1076), assets) == 'audio'
    louder = bytes([6]) + struct.pack('<HBBB', 1076, 100, 100, 100) + b'\0'
    assert R.compare(se(19), louder, assets).startswith('command')
print('PASS: text-only and exact-sound page refresh, jumps compared by command')
