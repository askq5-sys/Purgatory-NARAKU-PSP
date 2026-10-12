import os
import tempfile
import unittest
import zlib
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.pack_market_assets import pack_assets


class MarketAssetPackTests(unittest.TestCase):
    def test_compresses_supported_assets_losslessly_and_copies_others(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            source, dest = root / "source", root / "packed"
            source.mkdir()
            texture = bytes(range(256)) * 64
            audio = b"\0\0\x11\0" * 10000
            (source / "map001.rgba8888").write_bytes(texture)
            (source / "bgm.pcm").write_bytes(audio)
            (source / "map001.bin").write_bytes(b"vm data")

            count, _, _ = pack_assets(source, dest)
            self.assertEqual(count, 2)
            self.assertEqual(zlib.decompress((dest / "map001.rgba8888.z").read_bytes()), texture)
            self.assertEqual(zlib.decompress((dest / "bgm.pcm.z").read_bytes()), audio)
            self.assertEqual((dest / "map001.bin").read_bytes(), b"vm data")
            self.assertFalse((dest / "map001.rgba8888").exists())
            self.assertTrue((source / "map001.rgba8888").is_file())

    def test_refuses_to_overwrite_existing_destination(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            source, dest = root / "source", root / "packed"
            source.mkdir()
            dest.mkdir()
            sentinel = dest / "keep.txt"
            sentinel.write_text("keep", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                pack_assets(source, dest)
            self.assertEqual(sentinel.read_text(encoding="utf-8"), "keep")

    def test_keeps_incompressible_asset_raw(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            source, dest = root / "source", root / "packed"
            source.mkdir()
            raw = os.urandom(4096)
            (source / "noise.pcm").write_bytes(raw)
            count, _, _ = pack_assets(source, dest)
            self.assertEqual(count, 0)
            self.assertEqual((dest / "noise.pcm").read_bytes(), raw)


if __name__ == "__main__":
    unittest.main()
