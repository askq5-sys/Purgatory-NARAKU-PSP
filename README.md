# Purgatory -NARAKU- PSP

An unofficial PSP homebrew port of Purgatory -NARAKU-.

## Current version: 1.7.10

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

The source/resource patch is incremental, not a standalone game or a
precompiled EBOOT. Apply it over an existing port project. Building requires
a working PSP SDK environment.

Build the debug version:

```bash
./tools/build_deploy_v178.sh
```

Build the compressed market folder after the debug build:

```bash
./tools/build_market_v178.sh
```

The market build is produced in `NARAKU_MARKET` on the PC, while its EBOOT
uses the game root `PSP/GAME/NARAKU`. Rename the folder to `NARAKU` when
installing it on a PSP or PPSSPP device. Keep the EBOOT and compressed
`assets` folder together. Restart the game after updating and load an
in-game save. Avoid emulator savestates created with an older executable.

Select Russian through Settings → Language → Русский.

## Bug reports

[Submit a bug report with screenshots or video](https://docs.google.com/forms/d/e/1FAIpQLSeCmy7igU36vjf8h_UQxBvWXnwr3AmQDEiRwXMiN3tbJ9G-bg/viewform)

Include the port version, language, location, steps to reproduce, expected
and actual behaviour, and whether you loaded an in-game save or savestate.
For emulator reports, include PPSSPP version, device and graphics backend.

## Validation

Host checks cover translation lookup, Cyrillic coverage, preservation of
existing dialogue/script resources, selected runtime regressions, and
compressed-asset handling. OpenGL and Vulkan startup were confirmed in
PPSSPP. Real PSP compatibility, visual layout and complete route coverage
still require tester verification.

## Credits

Original game: Nama.
This is an unofficial fan port and is not affiliated with the original
creator. Original game assets remain the property of their respective owners.
