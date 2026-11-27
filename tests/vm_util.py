"""Apply the installed post-processing steps to freshly compiled VM code.

Map scripts in assets/ pass through tools/update_window_styles_v174.py after
compilation: message and choice windows carry the original background and
position. Tests that recompile a page compare against styled(...) so they
check the files the game actually reads.
"""
import json
import sys
from pathlib import Path

_TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(_TOOLS))
import update_window_styles_v174 as _styles  # noqa: E402

_META = json.loads((_TOOLS / 'window_styles_v174.json').read_text())


def styled(map_id, event_id, page_index, code):
    """Return code as update_window_styles_v174 leaves it in assets/."""
    meta = _META.get(str(int(map_id)))
    if meta is None or code == b'\0':
        return code
    return _styles.convert(code, meta.get(f'{event_id}:{page_index}', []))
