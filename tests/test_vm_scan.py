"""runtime/vm_scan.h walks every installed script page like the converter's
decoder, and finds the next Show Picture the same way."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
_sys.path.insert(0, str(ROOT / 'tools'))
import update_window_styles_v174 as S  # noqa: E402

HARNESS = r'''
#include <stdio.h>
#include <stdlib.h>
#include "runtime/vm_scan.h"
/* stdin: u32 count, then per page u32 size + bytes. stdout per page: the
 * command starts, then the next-picture id seen from every start. */
int main(void)
{
    unsigned count, size, i;
    if (fread(&count, 4, 1, stdin) != 1) return 1;
    for (i = 0; i < count; ++i) {
        uint8_t *b;
        size_t pos = 0, next;
        if (fread(&size, 4, 1, stdin) != 1) return 1;
        b = malloc(size + 1);
        if (fread(b, 1, size, stdin) != size) return 1;
        while (pos < size) {
            if (vm_scan_op(b, size, pos, &next) < 0) { printf("bad %zu\n", pos); break; }
            int two[2];
            vm_scan_next_pictures(b, size, pos, 64, two, 2);
            printf("%zu %d %d %d\n", pos, vm_scan_next_picture(b, size, pos, 64), two[0], two[1]);
            pos = next;
        }
        printf("end\n");
        free(b);
    }
    return 0;
}
'''


def pages():
    for path in sorted((ROOT / 'assets').glob('map*_vm.bin')):
        b = path.read_bytes()
        _, ne, _, blob, _, _ = struct.unpack_from('<4sHHIII', b)
        base = 20 + ne * 10
        for i in range(ne):
            _, _, _, first, n, _ = struct.unpack_from('<HHHHBB', b, 20 + i * 10)
            for j in range(n):
                off, size = struct.unpack_from('<II', b, base + (first + j) * 20 + 12)
                code = b[blob + off:blob + off + size]
                if code != b'\0':
                    yield code
    common = (ROOT / 'assets' / 'common_vm.bin').read_bytes()
    count = struct.unpack_from('<H', common, 4)[0]
    for i in range(count):
        off, size = struct.unpack_from('<II', common, 8 + i * 8)
        if size:
            yield common[off:off + size]


def expected(code):
    records = S.decode(code)
    out = []
    for k, (a, _, _, _) in enumerate(records):
        ids = []
        for a2, _, op, _ in records[k:k + 64]:
            if op == 0 or len(ids) == 2:
                break
            if op in (25, 32):
                r = struct.unpack_from('<H', code, a2 + 3)[0]
                if r > 0:
                    ids.append(r)
        ids += [0, 0]
        out.append(f'{a} {ids[0]} {ids[0]} {ids[1]}')
    return out + ['end']


codes = list(pages())
with tempfile.TemporaryDirectory() as td:
    c, exe = Path(td) / 's.c', Path(td) / 's'
    c.write_text(HARNESS)
    subprocess.run(['cc', '-std=c99', '-O2', '-I', str(ROOT), str(c), '-o', str(exe)], check=True)
    data = struct.pack('<I', len(codes)) + b''.join(struct.pack('<I', len(x)) + x for x in codes)
    got = subprocess.run([str(exe)], input=data, capture_output=True, check=True).stdout.decode().split('\n')
want = [line for code in codes for line in expected(code)]
assert got[:len(want)] == want, next((g, w) for g, w in zip(got, want) if g != w)
print(f'PASS: vm_scan walks all {len(codes)} installed script pages like the converter '
      f'and finds the same next pictures')
