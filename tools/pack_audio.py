#!/usr/bin/env python3
"""Compress a game folder's audio losslessly (NAUD, runtime/naraku_audio.h).

For every assets/NAME.pcm (44.1 kHz 16-bit stereo) this writes NAME.nau next
to it; the EBOOT plays NAME.nau when it exists and NAME.pcm otherwise.
Nothing is lost: every file is decoded again and compared with the original.

* Sounds whose two channels are identical are stored as mono.
* A sound that is another one at a lower volume (same samples times a gain,
  within one step of rounding) is stored as a few-byte alias of it.
* Files already converted (NAME.nau newer than NAME.pcm) are skipped.

The .pcm files stay; `tools/runtime_assets.py` lists them as unused once
their .nau exists, and can move them out of the folder.

Usage: python3 tools/pack_audio.py GAME/assets [--force] [--check]
"""
from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
BLOCK_FRAMES = 1024
GAIN_ONE = 4096
CLIP = 32767
SUFFIX = '.nau'

DRIVER = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "runtime/naraku_audio.h"

static void put16(uint8_t *p, unsigned v) { p[0] = v & 255; p[1] = (v >> 8) & 255; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }

static int encode(const char *src, const char *dst, int channels)
{
    FILE *f = fopen(src, "rb"), *o;
    long bytes; int16_t *in, *pick; uint32_t frames, done;
    uint8_t header[NAUD_HEADER_BYTES] = {'N','A','U','D',1};
    size_t cap = naud_max_payload(2, BLOCK) * 4;
    uint8_t *payload = malloc(cap), *trial = malloc(cap);
    int32_t *scratch = malloc(sizeof(int32_t) * 4 * BLOCK);
    if (!f) return 1;
    fseek(f, 0, SEEK_END); bytes = ftell(f); fseek(f, 0, SEEK_SET);
    frames = (uint32_t)(bytes / 4);
    in = malloc((size_t)frames * 4 + 4); pick = malloc((size_t)frames * 4 + 4);
    if (fread(in, 4, frames, f) != frames) return 2;
    fclose(f);
    for (uint32_t i = 0; i < frames; ++i)
        for (int c = 0; c < channels; ++c) pick[(size_t)i * channels + c] = in[2 * (size_t)i + c];
    header[5] = (uint8_t)channels; put16(header + 6, BLOCK); put32(header + 8, frames);
    put16(header + 12, NAUD_GAIN_ONE);
    o = fopen(dst, "wb");
    if (!o) return 3;
    fwrite(header, 1, sizeof(header), o);
    for (done = 0; done < frames; done += BLOCK) {
        int n = frames - done < BLOCK ? (int)(frames - done) : BLOCK;
        uint8_t size[2];
        size_t len = naud_encode_block(pick + (size_t)done * channels, channels, n, scratch, payload, trial);
        if (len > naud_max_payload(channels, BLOCK)) return 4;
        put16(size, (unsigned)len);
        fwrite(size, 1, 2, o);
        fwrite(payload, 1, len, o);
    }
    return fclose(o) != 0;
}

static int decode(const char *src, const char *dst)
{
    FILE *f = fopen(src, "rb"), *o = fopen(dst, "wb");
    uint8_t header[NAUD_HEADER_BYTES], *payload; int16_t *out; int32_t *scratch;
    NaudHeader h; uint32_t done;
    if (!f || !o || fread(header, 1, sizeof(header), f) != sizeof(header) ||
        !naud_parse_header(header, &h) || h.alias) return 1;
    payload = malloc(naud_max_payload(h.channels, h.block_frames));
    out = malloc((size_t)h.block_frames * 4);
    scratch = malloc(sizeof(int32_t) * 2 * (size_t)h.block_frames);
    for (done = 0; done < h.total_frames; done += (uint32_t)h.block_frames) {
        int n = h.total_frames - done < (uint32_t)h.block_frames ? (int)(h.total_frames - done) : h.block_frames;
        uint8_t size[2]; size_t len;
        if (fread(size, 1, 2, f) != 2) return 2;
        len = size[0] | (size[1] << 8);
        if (len > naud_max_payload(h.channels, h.block_frames) || fread(payload, 1, len, f) != len) return 2;
        if (naud_decode_block(payload, len, &h, n, h.gain, scratch, out) != n) return 5;
        fwrite(out, 4, (size_t)n, o);
    }
    fclose(f);
    return fclose(o) != 0;
}

int main(int argc, char **argv)
{
    if (argc == 5 && !strcmp(argv[1], "enc")) return encode(argv[2], argv[3], atoi(argv[4]));
    if (argc == 4 && !strcmp(argv[1], "dec")) return decode(argv[2], argv[3]);
    return 64;
}
'''


def build_driver(workdir: Path) -> Path:
    src = workdir / 'naud_driver.c'
    exe = workdir / 'naud_driver'
    src.write_text(DRIVER)
    subprocess.run(['cc', '-std=c99', '-O2', f'-DBLOCK={BLOCK_FRAMES}', '-I', str(ROOT),
                    str(src), '-o', str(exe)], check=True)
    return exe


def alias_bytes(target: str, gain: int) -> bytes:
    name = target.encode()
    if len(name) >= 48:
        raise ValueError(f'alias target name too long: {target}')
    return (b'NAUD' + bytes([1, 2]) + struct.pack('<HIHBB', BLOCK_FRAMES, 0, gain, 1, 0)
            + name + b'\0')


def load(path: Path) -> np.ndarray:
    data = np.frombuffer(path.read_bytes(), dtype='<i2')
    return data[:len(data) // 2 * 2].reshape(-1, 2)


def find_alias(quiet: np.ndarray, loud: np.ndarray):
    """Gain (Q12) that turns `loud` into `quiet` within one step, else None."""
    if quiet.shape != loud.shape or not len(quiet):
        return None
    a = loud.astype(np.int64)
    if np.abs(a).max() >= CLIP:           # clipped: the quiet copy is not a scaled copy
        return None
    denom = int((a * a).sum())
    if not denom:
        return None
    g = float((a * quiet.astype(np.int64)).sum()) / denom
    if not 0.0 < g < 1.0:
        return None
    q = int(round(g * GAIN_ONE))
    rebuilt = np.clip((a * q + 2048) >> 12, -32768, 32767)
    if np.abs(rebuilt - quiet).max() > 1:
        return None
    return q


def up_to_date(pcm: Path) -> bool:
    """NAME.nau is newer than NAME.pcm and, if an alias, points at samples
    (aliases of aliases written by the first version are redone)."""
    nau = pcm.with_suffix(SUFFIX)
    if not nau.is_file() or nau.stat().st_mtime < pcm.stat().st_mtime:
        return False
    data = nau.read_bytes()[:16 + 48]
    if len(data) < 16 or data[:4] != b'NAUD':
        return False
    if data[14] & 1:
        target = data[16:].split(b'\0')[0].decode(errors='replace')
        return is_sample_file(nau.parent / target)
    return True


def plan(assets: Path, force: bool):
    """[(pcm, kind, extra)] where kind is 'mono', 'stereo' or 'alias'.

    Within each group of equally long sounds, the loudest come first and a
    sound may only become an alias of one that holds samples itself (the
    EBOOT follows one alias, never a chain)."""
    pcms = sorted(p for p in assets.glob('*.pcm') if p.is_file())
    todo = {p for p in pcms if force or not up_to_date(p)}
    by_size: dict[int, list[Path]] = {}
    for p in pcms:
        by_size.setdefault(p.stat().st_size, []).append(p)
    decided: dict[Path, tuple] = {}
    for group in by_size.values():
        if not any(p in todo for p in group):
            continue
        samples = {p: load(p) for p in group}
        peak = {p: int(np.abs(s.astype(np.int32)).max(initial=0)) for p, s in samples.items()}
        roots: list[Path] = []
        for p in sorted(group, key=lambda x: (-peak[x], x.name)):
            target = None
            for root in roots:
                if peak[root] <= peak[p]:
                    continue
                gain = find_alias(samples[p], samples[root])
                if gain is not None:
                    target = (root, gain)
                    break
            if target:
                decided[p] = ('alias', target)
                continue
            roots.append(p)
            s = samples[p]
            decided[p] = ('mono' if len(s) and np.array_equal(s[:, 0], s[:, 1]) else 'stereo', None)
    return [(p, decided[p][0], decided[p][1]) for p in pcms if p in todo]


def is_sample_file(path: Path) -> bool:
    """True for a .nau that holds samples (not an alias)."""
    try:
        with path.open('rb') as f:
            head = f.read(16)
    except OSError:
        return False
    return len(head) == 16 and head[:4] == b'NAUD' and not head[14] & 1


def convert(assets: Path, force: bool = False, check: bool = False, report=print) -> int:
    jobs = plan(assets, force)
    if check:
        for p, kind, _ in jobs:
            report(f'  {kind:6} {p.name}')
        report(f'{len(jobs)} files to convert')
        return len(jobs)
    raw = packed = 0
    counts = {'stereo': 0, 'mono': 0, 'alias': 0}
    # Samples first, aliases after: an alias needs its target's file.
    jobs.sort(key=lambda job: job[1] == 'alias')
    with tempfile.TemporaryDirectory() as td:
        exe = build_driver(Path(td))
        for p, kind, extra in jobs:
            out = p.with_suffix(SUFFIX)
            tmp = out.with_suffix(SUFFIX + '.tmp')
            if kind == 'alias' and is_sample_file(extra[0].with_suffix(SUFFIX)):
                other, gain = extra
                tmp.write_bytes(alias_bytes(other.with_suffix(SUFFIX).name, gain))
            else:
                if kind == 'alias':
                    samples = load(p)
                    kind = 'mono' if len(samples) and np.array_equal(samples[:, 0], samples[:, 1]) else 'stereo'
                subprocess.run([str(exe), 'enc', str(p), str(tmp), '1' if kind == 'mono' else '2'],
                               check=True)
                dec = Path(td) / 'check.pcm'
                subprocess.run([str(exe), 'dec', str(tmp), str(dec)], check=True)
                if not np.array_equal(load(dec), load(p)):
                    tmp.unlink()
                    raise RuntimeError(f'{p.name}: decoded audio differs from the original')
            tmp.replace(out)
            counts[kind] += 1
            raw += p.stat().st_size
            packed += out.stat().st_size
    if jobs:
        report(f'{len(jobs)} sounds compressed losslessly: {counts["stereo"]} stereo, '
               f'{counts["mono"]} mono, {counts["alias"]} volume aliases; '
               f'{raw / 1048576:.1f} MB -> {packed / 1048576:.1f} MB')
    else:
        report('audio: nothing to convert')
    return len(jobs)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('assets', type=Path)
    ap.add_argument('--force', action='store_true', help='convert again even if up to date')
    ap.add_argument('--check', action='store_true', help='only list what would be converted')
    args = ap.parse_args()
    convert(args.assets, args.force, args.check)
    return 0


if __name__ == '__main__':
    sys.exit(main())
