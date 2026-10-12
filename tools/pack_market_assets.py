#!/usr/bin/env python3
"""Copy game assets into a lossless DEFLATE release layout."""
from __future__ import annotations

import argparse
import shutil
import zlib
from pathlib import Path

COMPRESS_SUFFIXES = {".rgba8888", ".pcm"}


def pack_assets(source: Path, destination: Path) -> tuple[int, int, int]:
    source = source.resolve()
    destination = destination.resolve()
    if not source.is_dir():
        raise FileNotFoundError(f"asset source does not exist: {source}")
    if source == destination or source in destination.parents:
        raise ValueError("destination must be separate from the source asset tree")
    if destination.exists():
        raise FileExistsError(f"destination already exists: {destination}")
    destination.mkdir(parents=True)

    packed_count = raw_total = packed_total = 0
    for src in sorted(source.rglob("*")):
        if not src.is_file():
            continue
        dst = destination / src.relative_to(source)
        dst.parent.mkdir(parents=True, exist_ok=True)
        if src.suffix in COMPRESS_SUFFIXES:
            raw = src.read_bytes()
            packed = zlib.compress(raw, level=9)
            if len(packed) < len(raw):
                dst = dst.with_name(dst.name + ".z")
                dst.write_bytes(packed)
                packed_count += 1
                raw_total += len(raw)
                packed_total += len(packed)
                continue
        shutil.copy2(src, dst)
        raw_total += src.stat().st_size
        packed_total += dst.stat().st_size
    return packed_count, raw_total, packed_total


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="existing game assets directory")
    parser.add_argument("destination", type=Path, help="new compressed assets directory")
    args = parser.parse_args()
    count, raw, packed = pack_assets(args.source, args.destination)
    percent = (100.0 * packed / raw) if raw else 100.0
    print(f"Compressed files: {count}")
    print(f"Asset bytes: {raw:,} -> {packed:,} ({percent:.1f}%)")


if __name__ == "__main__":
    main()
