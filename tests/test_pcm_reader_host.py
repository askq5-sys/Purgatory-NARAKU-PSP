import os
import subprocess
import tempfile
import unittest
import zlib
from pathlib import Path


class PcmReaderHostTests(unittest.TestCase):
    def test_stream_decode_and_rewind_match_original_pcm(self):
        root = Path(__file__).resolve().parents[1]
        with tempfile.TemporaryDirectory() as td:
            tmp = Path(td)
            raw_path = tmp / "audio.pcm"
            raw = (bytes(range(256)) * 14001) + b"\x13\x00\x44"
            raw_path.write_bytes(raw)
            raw_path.with_name(raw_path.name + ".z").write_bytes(zlib.compress(raw, 9))
            for profile in ("debug", "compressed"):
                exe = tmp / f"pcm_reader_test_{profile}"
                command = ["cc", "-std=c99", "-Wall", "-Wextra"]
                if profile == "compressed":
                    command.append("-DNARAKU_COMPRESSED_ASSETS")
                command.extend([str(root / "tests/test_pcm_reader_host.c"), "-o", str(exe)])
                if profile == "compressed":
                    command.append("-lz")
                subprocess.run(command, check=True)
                subprocess.run([str(exe), str(raw_path)], check=True)


if __name__ == "__main__":
    unittest.main()
