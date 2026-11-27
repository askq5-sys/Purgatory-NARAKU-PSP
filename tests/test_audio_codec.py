"""NAUD audio: the converter's files play back through the runtime's reader, losslessly."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from pathlib import Path
import subprocess
import tempfile

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
_sys.path.insert(0, str(ROOT / 'tools'))
import pack_audio as P  # noqa: E402

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "runtime/compressed_assets.h"
/* Reads NAME.pcm through PcmReader in uneven pieces, twice (rewind), and
 * writes what it got to OUT. */
int main(int argc, char **argv)
{
    PcmReader r;
    static uint8_t buf[16384];
    FILE *out = fopen(argv[2], "wb");
    size_t sizes[] = {16384, 1000, 4, 7};
    long first = 0, second = 0;
    (void)argc;
    assert(pcm_reader_open(&r, argv[1]));
    for (int pass = 0; pass < 2; ++pass) {
        size_t got, k = 0;
        while ((got = pcm_reader_read(&r, buf, sizes[k++ % 4])) > 0) {
            if (pass == 0) { fwrite(buf, 1, got, out); first += (long)got; }
            else second += (long)got;
        }
        assert(pcm_reader_finished(&r));
        assert(pcm_reader_rewind(&r));
        assert(!pcm_reader_finished(&r) || first == 0);
    }
    assert(first == second);
    pcm_reader_close(&r);
    return fclose(out) != 0;
}
'''


def write(path, frames):
    path.write_bytes(np.asarray(frames, dtype='<i2').tobytes())


with tempfile.TemporaryDirectory() as td:
    d = Path(td)
    t = np.arange(44100 * 3 + 517)
    music = np.stack([8000 * np.sin(t * 0.031), 6000 * np.sin(t * 0.017 + 1)], axis=1).astype(np.int16)
    write(d / 'bgm_exact_1000.pcm', music)
    quiet = np.clip((music.astype(np.int64) * 2731 + 2048) >> 12, -32768, 32767)
    write(d / 'bgm_exact_1001.pcm', quiet)                       # 2/3 volume copy -> alias
    mono = (9000 * np.sin(np.arange(20000) * 0.05)).astype(np.int16)
    write(d / 'se_exact_1000.pcm', np.stack([mono, mono], axis=1))  # mono in stereo
    rng = np.random.default_rng(7)
    noise = rng.integers(-20000, 20000, size=(3001, 2))
    write(d / 'se_noise.pcm', noise)                             # incompressible -> verbatim blocks
    write(d / 'se_plain.pcm', noise[:100])                       # no .nau: read as is
    lines = []
    P.convert(d, report=lines.append)
    assert P.convert(d, report=lines.append) == 0, 'converting again does nothing'
    (d / 'se_plain.nau').unlink()

    head = lambda name: (d / name).read_bytes()[:16]
    assert head('bgm_exact_1000.nau')[:4] == b'NAUD' and head('bgm_exact_1000.nau')[5] == 2
    assert head('bgm_exact_1001.nau')[14] & 1, 'quiet copy is an alias'
    assert (d / 'bgm_exact_1001.nau').stat().st_size < 64
    assert head('se_exact_1000.nau')[5] == 1, 'identical channels stored mono'
    assert (d / 'bgm_exact_1000.nau').stat().st_size * 2 < (d / 'bgm_exact_1000.pcm').stat().st_size
    assert (d / 'se_noise.nau').stat().st_size <= (d / 'se_noise.pcm').stat().st_size + 64, 'noise falls back to verbatim'

    c, exe = d / 'h.c', d / 'h'
    c.write_text(HARNESS)
    subprocess.run(['cc', '-std=c99', '-O0', '-Wall', '-Werror', '-I', str(ROOT), str(c), '-o', str(exe)], check=True)

    def play(name):
        out = d / (name + '.out')
        subprocess.run([str(exe), str(d / (name + '.pcm')), str(out)], check=True)
        return P.load(out)

    assert np.array_equal(play('bgm_exact_1000'), music), 'lossless'
    got_quiet = play('bgm_exact_1001')
    assert got_quiet.shape == quiet.shape and np.abs(got_quiet - quiet).max() <= 1
    assert np.array_equal(play('se_exact_1000'), np.stack([mono, mono], axis=1))
    assert np.array_equal(play('se_noise'), noise)
    assert np.array_equal(play('se_plain'), noise[:100]), '.pcm without .nau still plays'
    # Extreme values and mid/side: full-scale opposite channels stay exact.
    edge = np.array([[32767, -32768], [-32768, 32767], [0, -1], [-1, 0]] * 700, dtype=np.int16)
    write(d / 'se_edge.pcm', edge)
    P.convert(d, report=lines.append)
    assert np.array_equal(play('se_edge'), edge)
print('PASS: NAUD audio is lossless (stereo, mid/side, mono, verbatim), aliases are within one step, loops and falls back to .pcm')
