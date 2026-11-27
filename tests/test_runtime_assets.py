"""tools/runtime_assets.py knows the files the EBOOT loads from its source."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from runtime_assets import asset_pattern, is_runtime_asset  # noqa: E402

pattern = asset_pattern()
used = ['map001_vm.bin', 'map114_atlas1.rgba8888', 'map113_event_atlas.rgba8888',
        'se_exact_1076.pcm.z', 'bgm_exact_1002.pcm', 'font_page7.t8', 'pic_012.p44',
        'actor09_atlas.rgba8888', 'common_vm.bin', 'russian177.bin', 'ui_iconset.rgba4444',
        'title_zhtw.rgba8888', 'se_ui_save.pcm']
unused = ['m001_e001_en.rgba8888', 'evt069_en.rgba8888', 'opening1_en.rgba8888',
          'dialog1_en.rgba8888', 'enri_portrait.rgba8888', 'fall_atlas.rgba8888',
          'lucas_corpse.rgba8888', 'lucas_fall_atlas.rgba8888', 'map001_atlas.rgba8888',
          'fog_a1.rgba4444', 'audio_exact_v079.json', 'scene_trace.log']
for name in used:
    assert is_runtime_asset(name, pattern), name
for name in unused:
    assert not is_runtime_asset(name, pattern), name
import tempfile
with tempfile.TemporaryDirectory() as td:
    folder = Path(td)
    for name in ('bgm_exact_1000.pcm', 'bgm_exact_1000.nau', 'se_ui_ok.pcm'):
        (folder / name).write_bytes(b'x')
    assert is_runtime_asset('bgm_exact_1000.nau', pattern, folder)
    assert not is_runtime_asset('bgm_exact_1000.pcm', pattern, folder), 'replaced by its .nau'
    assert is_runtime_asset('se_ui_ok.pcm', pattern, folder), 'no .nau: still played'
    assert not is_runtime_asset('opening1_en.nau', pattern, folder)
print('PASS: runtime asset names come from the source; legacy leftovers are unused')
