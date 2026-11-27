# Tools

Python tools need Pillow and numpy (`pip install -r requirements.txt`).
Scripts that read the PC game take its folder (the one with `data/`, or
`www/data/` for some releases).

## Build and install

| Script | Purpose |
| --- | --- |
| `build_deploy.sh` | Build the EBOOT and install it into a game folder (PPSSPP by default) |
| `build_market.sh` | Build the compressed release folder `NARAKU_MARKET` |
| `pack_market_assets.py` | Compress assets for the release; leaves out files the EBOOT never loads |
| `check_eboot_imports.py` | Refuse EBOOTs that import PSP kernel libraries (black screen on hardware) |
| `assets_patch.txt` | Assets `build_deploy.sh` copies from the local `assets/` |

## Maintaining an installed game folder

| Script | Purpose |
| --- | --- |
| `refresh_vm_text.py` | Update outdated message text and sounds in installed map scripts |
| `update_window_styles_v174.py` | Give messages and choices their original background and position |
| `fix_doorway_gradient_priority.py` | Doorway shading fix for converted maps |
| `runtime_assets.py` | List or move asset files the EBOOT never loads |
| `prepare_test_data.py` | Write the converter manifests the full test run reads |

`build_deploy.sh` runs the first three when it finds the PC game data.

## Converting the PC game

The assets were built up release by release; the version in a script's name
is the port version that introduced it, and running them in that order
reproduces `assets/`. Later scripts reuse `prepare_world_v060.py`.

| Script | Produces |
| --- | --- |
| `prepare_intro_v006.py`, `prepare_intro_v007.py` | Title-era music, A1 veil and a few sounds (their opening images are no longer used) |
| `prepare_audio_v051.py`, `prepare_parity_v056.py` | Water, splash and bone sounds, A1 and fog overlays |
| `prepare_world_v060.py` | Maps, tile and event atlases, script bytecode, pictures, player atlas, font |
| `prepare_ui_v060.py` | Menus, icons, title music and menu sounds |
| `prepare_vm_v060.py` | Script bytecode and picture table only (fast rebuild) |
| `prepare_font_v083.py` | Font pages with the original Latin font |
| `prepare_exact_audio_v*.py` | Sounds at the exact volume/pitch of each event, for the maps of that version |
| `prepare_portraits_v132.py` | Dialogue portraits |
| `prepare_stage_v*.py` | Maps added in that version (each imports its exact-audio script) |
| `prepare_arena_door_v147.py` | Map069 door passage data |
| `prepare_russian_v177.py`, `pack_russian_v177.py` | Russian dictionary and Cyrillic glyphs (`ru_*.json`, `ru_translation.tsv`) |
| `decrypt_mz.py`, `png_to_psp.py` | Decrypt an RPG Maker MZ image; convert a PNG to a raw PSP texture |

`verify_parity_v056.py`, `verify_parity_v060.py` and `verify_v079.py` print
the original game's values that the port reproduces, as a manual check.

Superseded converters (`prepare_world_v010`–`v051`, the 0.0.x Map001 and
event scripts, older parity checks) were removed; they remain in the git
history.
