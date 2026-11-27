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


def use_exact_audio_ids(world, game):
    """Number exact sounds as the converter does (all maps, not the manifest).

    assets/audio_exact_v079.json only lists the PCM files that were converted;
    the IDs themselves are assigned across every map and common event.
    """
    import refresh_vm_text
    world.EXACT_SE_IDS, world.EXACT_BGM_IDS = refresh_vm_text.audio_ids(game)


# Pages the converter adds that the installed scripts do not have. Map055
# event 1 gained a fourth lever page (switch 632) in 1.4.2; the installed map
# keeps three, which looks the same because switch 622 stays ON from then on.
# Adding a page changes the event and sprite tables, so it needs a full map
# rebuild rather than tools/refresh_vm_text.py.
INSTALLED_PAGE_LIMIT = {(55, 1): 3}


def data_pages(map_id, event_id, pages):
    """The source pages that have an installed counterpart."""
    return pages[:INSTALLED_PAGE_LIMIT.get((int(map_id), int(event_id)), len(pages))]
