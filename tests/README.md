# Tests

## Quick checks

```bash
./tests/run_host_checks.sh
```

Needs only a C compiler and Python 3. Runs the tests that use no game data:
runtime logic extracted from the source (routes, chase contact, saving,
message windows, pictures, camera clipping…), the build scripts and the
user-mode link check. Run it before every push.

## All checks

```bash
./tests/run_all_checks.sh
```

Also runs the tests that read the converted game and the PC version. It
needs:

- `assets/` in the repository with the unpacked game files, for example a
  copy of the installed PPSSPP `NARAKU/assets` folder (plain files, not the
  `.z` market build);
- the PC version (its `data/` and `img/` folders) in `$NARAKU_ORIGINAL`,
  `original/` inside the repository or `original/` next to it;
- Python packages from `requirements.txt` (Pillow, numpy).

The script creates the two converter manifests the tests read
(`tools/prepare_test_data.py`) when they are missing.

## How the tests read the code

The runtime is one C translation unit: `main.c` includes `runtime/*.inc` in
order. Tests read the combined text through `tests/source_util.py`
(`main_source`) and the PC data through `original_dir`. Tests that recompile
map scripts compare against `vm_util.styled(...)`, which applies the window
style step (`tools/update_window_styles_v174.py`) that installed scripts
pass through.

## Known stale scripts

`run_all_checks.sh` reports these tests as known stale instead of failing:

| Test | Why it fails |
| --- | --- |
| `test_native_actors_139` | 99 of ~3500 map script pages in `assets/` were compiled before later converter fixes |
| `test_stage_135` | same, plus Map055 event 1 lacks the fourth "broken lever" page added in 1.4.2 |
| `test_will_stage_170` | same |
| `test_factory_extension` | same |

The converter fixes missing from the installed scripts are English text
changes ("Obtained an Iron Key" → "Obtained a Green Key", the red
\c[18]Cleaver highlight, collapsed spaces and line breaks) and the Map055
lever page. The lever page has no visible effect: the lever's own scene
leaves switch 622 on, which already shows the broken lever. These tests pass
once the map scripts are rebuilt with the current converter; the Russian
dictionary is keyed by the English text and has to be rebuilt with them.
