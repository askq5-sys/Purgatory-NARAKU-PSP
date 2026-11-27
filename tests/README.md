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

A few factory and stage checks also write JSON summaries to `tests/reports/`
(not tracked by git).

The script creates the two converter manifests the tests read
(`tools/prepare_test_data.py`) when they are missing.

## How the tests read the code

The runtime is one C translation unit: `main.c` includes `runtime/*.inc` in
order. Tests read the combined text through `tests/source_util.py`
(`main_source`) and the PC data through `original_dir`. Tests that recompile
map scripts compare against `vm_util.styled(...)`, which applies the window
style step (`tools/update_window_styles_v174.py`) that installed scripts
pass through.

## Keeping installed scripts current

`tools/refresh_vm_text.py ORIGINAL assets` recompiles every installed map and
common event page and replaces the ones whose only differences are message or
choice text, or a sound that now uses its exact volume/pitch variant;
`tools/build_deploy.sh` runs it on the game folder. Jump targets are compared
by command, so longer text is fine. `test_vm_text_current` checks that nothing
is left to refresh.

Two pages stay as installed because their commands differ: the Map024 crusher
loop (event 12, unsupported in the installed build) and Map055 event 1, which
lacks the fourth lever page added in 1.4.2. The lever page has no visible
effect: the lever's own scene leaves switch 622 on, which already shows the
broken lever. Tests that walk source pages skip it through
`vm_util.data_pages`.

Exact sound IDs are numbered over every map (`vm_util.use_exact_audio_ids`);
`assets/audio_exact_v079.json` lists only the converted PCM files. The Russian
dictionary is keyed by the normalized English text, so refreshed pages keep
their translations; `test_vm_text_current` and the refresh were checked
against `russian177.bin` with no missing keys.
