# Purgatory -NARAKU- PSP

An unofficial PSP homebrew port of Purgatory -NARAKU-.

## Current version: 1.7.14

The port implements Enri's and Will's stories, the ending routes,
secret-route events, and bonus areas. The developer has completed
the available routes during testing; broader hardware and regression
testing is still needed.

This is a test release. Implemented content should not be interpreted
as a guarantee that every branch is free of bugs.

## Features

- Enri and Will, with character-aware save-slot previews.
- Dialogue, choices, scripted movement, chases and puzzles.
- Inventory, key items, notes, save/load and settings menus.
- Normal, dimmed and transparent message windows.
- Original music and sound effects.
- English, Japanese, Simplified Chinese and Traditional Chinese.
- An additional unofficial Russian translation.
- Save-file deletion with confirmation in all five languages.
- Settings values aligned to a shared right edge.
- Compressed market build for smaller texture and audio assets.

## Changes in 1.7.14

- Dialogue no longer stutters when a new line or portrait appears.
- Audio files are about four times smaller and sound the same.
- Gray-tinted scenes are faster again; the save menu no longer rereads the slots.

## Changes in 1.7.13

- Fixed outdated English text in a few scenes (green key, Cleaver/Axe
  choice, broken lines); the Map055 levers use their original sounds.
- Gray-tinted scenes take less time per frame.
- The `assets` folder is about 200 MB smaller; unused code was removed.

## Changes in 1.7.12

- Removed a debug log that wrote to the memory stick during Map012 scenes.
- The game runs the PSP CPU at 333 MHz.
- Maps draw only the tiles around the screen.

## Changes in 1.7.11

- Fixed the black screen on real PSP hardware: the EBOOT no longer links
  PSP kernel libraries, which only emulators accept.
- Fixed Enri's head looking transparent when walking past dark doorways.
- Fixed an out-of-bounds read in transfer handling.
- Restored the XMB icon, background and music.

Real PSP testing is still pending; reports are welcome.

## Changes in 1.7.9–1.7.10

- Fixed a startup crash under PPSSPP JIT/IR by making the laser-trail
  reset independent of `memset`'s return value. The updated build was
  tested under both OpenGL and Vulkan.
- Shortened and faded the repeated wet-floor and blood-puddle step sound
  to reduce overlapping audio work on slower devices.
- Batched Map013 tile rendering, reducing tile draw submissions from
  about 518 to about 36 for a full map pass while preserving order and
  filtering.
- Reduced the wait after the axe pickup from 60 frames to 12.

The Russian translation is ready for tester review.
Its wording and layout have not undergone a complete Russian playthrough.

## Controls

| Button | Action |
| --- | --- |
| D-pad | Move |
| X | Confirm / interact / examine |
| O | Open menu / cancel |
| R | Dash |

## Installation and builds

The repository holds the runtime source and build tools, not the game data.
`assets/` (converted game data), `original/` (files from the PC game) and
`xmb/` (`ICON0.PNG`, `PIC1.PNG`, `SND0.AT3` for the PSP menu) are kept
locally and are not committed. Building requires the
[pspdev](https://github.com/pspdev/pspdev) SDK.

Build and install into an existing game folder (PPSSPP by default):

```bash
./tools/build_deploy.sh [GAME_DIR]
```

Assets from `tools/assets_patch.txt` are copied from the local `assets/`
folder when present; otherwise the copies already installed in the game
folder are kept. To apply the doorway shading fix to the installed maps and
refresh outdated script text (`tools/refresh_vm_text.py`), point the script
at the PC game's `data` folder:

```bash
NARAKU_ORIGINAL_DATA=/path/to/pc-game/data ./tools/build_deploy.sh
```

Build the compressed market folder after that:

```bash
./tools/build_market.sh [SOURCE_GAME_DIR] [DEST_GAME_DIR]
```

For measurements on a PSP, build with `NARAKU_PERF_HUD=ON tools/build_deploy.sh`
(or `-DNARAKU_PERF_HUD=ON`): the top line shows FPS, the work time per frame
(average/worst over one second; above 16.7 ms the game drops below 60 FPS) and
the heap in use / the most the heap has grown to. Without the option the
EBOOT is unchanged. The build scripts pass every option explicitly, so a value
left in the CMake cache never reaches a normal or release build.

`NARAKU_SWIZZLE_TEXTURES=ON` (environment or `-D`) stores the map tile atlases, event sprite
atlases, character atlases, A1 veil, fog and parallax in the GE block
("swizzled") layout, which the PSP samples faster. The pictures on screen are
identical; a wrong layout would show as scrambled textures, also in PPSSPP.
Off by default until it is measured on hardware.

Audio is stored as NAUD files (`runtime/naraku_audio.h`), compressed without
loss: each block is predicted from the previous samples and the prediction
errors are Rice coded (as in FLAC), mono where both channels are equal, and
quieter copies as a gain on the louder one. `build_deploy.sh` converts a game
folder's `.pcm` audio (330 MB to 72 MB); the EBOOT still plays `.pcm` files
that have no `.nau`.

The market folder only gets the files the EBOOT loads. Game folders updated
from older releases still hold about 200 MB of such leftovers (pre-rendered
dialog pages, the old opening); list them, or move them out, with

```bash
python3 tools/runtime_assets.py GAME_DIR/assets [--move-to DIR]
```

Both scripts stop if the EBOOT imports PSP kernel libraries
(`tools/check_eboot_imports.py`): such an EBOOT runs in PPSSPP but shows a
black screen on a real PSP.

The market build is produced in `NARAKU_MARKET` on the PC, while its EBOOT
uses the game root `PSP/GAME/NARAKU`. Rename the folder to `NARAKU` when
installing it on a PSP or PPSSPP device. Keep the EBOOT and compressed
`assets` folder together. Restart the game after updating and load an
in-game save. Avoid emulator savestates created with an older executable.

Select Russian through Settings → Language → Русский.

## Repository layout

| Path | Contents |
| --- | --- |
| `main.c` | Settings, shared state and the main loop |
| `runtime/*.inc` | Runtime modules included by `main.c` (see below) |
| `runtime/*.h` | Self-contained helpers (routes, animation, puzzles) |
| `tools/` | Build, install and conversion scripts, see `tools/README.md` |
| `tests/` | Quick checks (`run_host_checks.sh`) and full checks with game data (`run_all_checks.sh`), see `tests/README.md` |
| `docs/releases/` | Release notes |

### Runtime modules

The game is compiled as one C file: `main.c` includes the modules below in
this order, so they share its `static` state and must stay in that order.
Tests read the combined source through `tests/source_util.py`.

| Module | Contents |
| --- | --- |
| `types.inc` | Basic data types |
| `helpers.inc` | Small helpers, HOME exit callback |
| `files.inc` | Asset loading (plain and `.z`) |
| `graphics.inc` | GU setup and drawing primitives |
| `font_text.inc` | UTF-8 font, text layout, message surface |
| `input.inc` | Controller input, language |
| `audio.inc` | Music streaming, sound effects |
| `progress.inc` | Switches, variables, items, save files |
| `ui_menus.inc` | Title, item, options, save/load menus |
| `map_load.inc` | Map, event and atlas loading |
| `vm_pages.inc` | Event page selection |
| `world_draw.inc` | Camera, tiles, event sprites, pictures |
| `event_routes.inc` | Move routes, scripted actors |
| `perf_hud.inc` | Frame-time counter (`NARAKU_PERF_HUD` builds) |
| `world_frame.inc` | Picture animation, special rooms, frame rendering |
| `vm_text.inc` | Script VM: flashes, tints, messages, choices |
| `vm_interpreter.inc` | Script VM: commands, move routes, transfers |
| `messages.inc` | Runtime messages, screen fades |

Per-version deploy scripts and asset lists up to 1.7.8 were removed from the
tree; they remain in the git history (commit `b564dfe`).

## Bug reports

[Submit a bug report with screenshots or video](https://docs.google.com/forms/d/e/1FAIpQLSeCmy7igU36vjf8h_UQxBvWXnwr3AmQDEiRwXMiN3tbJ9G-bg/viewform)

Include the port version, language, location, steps to reproduce, expected
and actual behaviour, and whether you loaded an in-game save or savestate.
For emulator reports, include PPSSPP version, device and graphics backend.

## Validation

Host checks cover translation lookup, Cyrillic coverage, preservation of
existing dialogue/script resources, selected runtime regressions, and
compressed-asset handling. OpenGL and Vulkan startup were confirmed in
PPSSPP. Real PSP startup (fixed in 1.7.11), visual layout and complete route coverage
still require tester verification.

## Credits

Original game: Nama.
This is an unofficial fan port and is not affiliated with the original
creator. Original game assets remain the property of their respective owners.
