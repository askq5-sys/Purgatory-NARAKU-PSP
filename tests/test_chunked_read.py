"""Large asset reads go in 64 KB pieces and still return every byte."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
src = (ROOT / 'runtime' / 'files.inc').read_text()
a = src.index('#define FILE_READ_CHUNK')
b = src.index('\n}\n', src.index('static size_t read_chunked(')) + 3
assert 'read_chunked(' in (ROOT / 'runtime' / 'world_draw.inc').read_text()
assert src.count('read_chunked(') == 4, 'every large read in files.inc uses it'

HARNESS = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned calls, largest;
static size_t counted_fread(void *p, size_t s, size_t n, FILE *f)
{
    ++calls;
    if (n > largest) largest = (unsigned)n;
    return fread(p, s, n, f);
}
#define fread counted_fread
''' + src[a:b] + r'''
#undef fread
int main(int argc, char **argv)
{
    static const size_t sizes[] = {0, 1, 65536, 65537, 1048576 + 3};
    (void)argc;
    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        size_t n = sizes[i];
        uint8_t *want = malloc(n + 1), *got = malloc(n + 1);
        FILE *f = fopen(argv[1], "wb+");
        for (size_t k = 0; k < n; ++k) want[k] = (uint8_t)(k * 31 + i);
        fwrite(want, 1, n, f);
        rewind(f);
        calls = largest = 0;
        assert(read_chunked(got, n, f) == n);
        assert(memcmp(want, got, n) == 0);
        assert(largest <= FILE_READ_CHUNK);
        assert(calls == (n + FILE_READ_CHUNK - 1) / FILE_READ_CHUNK);
        /* A short file stops at its end. */
        rewind(f);
        assert(read_chunked(got, n + 1, f) == n);
        fclose(f);
        free(want);
        free(got);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    c = Path(td) / 't.c'
    exe = Path(td) / 't'
    c.write_text(HARNESS)
    subprocess.run(['cc', '-std=c99', '-O0', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe), str(Path(td) / 'data.bin')], check=True)
print('PASS: assets are read in 64 KB pieces without losing bytes')
