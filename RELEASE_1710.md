# Purgatory -NARAKU- PSP 1.7.10 — Test Update

This update contains the JIT startup workaround and the follow-up audio and
Map013 performance changes. The updated executable has been tested in PPSSPP
with JIT using both OpenGL and Vulkan. It remains an unofficial test build;
real PSP hardware verification is still pending.

## Changes

- Fixed a startup crash during the laser-chase trail reset. The reset no
  longer depends on the value returned by `memset`.
- Shortened the repeated wet-floor and blood-puddle step sound to 0.35 seconds
  with a short fade, limiting the buildup of overlapping effects.
- Batched Map013's tile drawing. A full map pass now submits about 36 batches
  instead of about 518 individual tile calls, with order and filtering kept.
- Reduced the pause after collecting the axe from 60 frames to 12.
- Added a compressed market build that packages supported textures and audio
  losslessly and preserves other assets.

## Build

Apply the incremental patch to an existing project and use a working PSP SDK.
For the compressed market build, run `tools/build_market_v178.sh` after the
normal debug build. It writes a `NARAKU_MARKET` staging folder; rename that
folder to `NARAKU` for installation because the tested EBOOT resolves game
assets under `PSP/GAME/NARAKU`.

After updating, restart the game and load an in-game save. Old emulator
savestates may not match the updated executable.

## Validation status

Host checks cover compressed asset packing and PCM reading. PPSSPP startup
was confirmed with JIT on OpenGL and Vulkan. Real PSP compatibility and the
Map013 frame-rate gain on console hardware still need testing.
