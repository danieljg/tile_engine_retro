# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A retro tile-based graphics engine plus a shoot-em-up game, written in C (gnu99) as a **libretro core**. Target platforms are desktop (via RetroArch) and homebrew consoles (3DS, Wii, PSP planned). Minimal dependencies: libxmp (module music playback) and the libretro API. Design goals and the GFX file format are documented in `Readme`.

## Build and run

```sh
make            # builds the libretro core (cd core && make) and generates core/*.gfx assets
make run        # builds, then launches: cd core && retroarch -L ./tile_engine_retro_libretro.so
make bmptogfx   # builds the BMP→GFX asset converter (gcc bmp_to_gfx.c qdbmp.c -lm)
make clean      # removes core/*.o, core/*.so, core/*.gfx
```

- The core's own build lives in `core/Makefile` (platform-detecting, standard libretro-style). `DEBUG=1 make` in `core/` builds with `-O0 -g`.
- Music via libxmp is optional: auto-detected with `pkg-config libxmp`, overridable with `make HAVE_XMP=0/1`. Without it the core builds silent (all xmp code is behind `#ifdef HAVE_XMP`).
- The core is a `.dylib` on macOS and `.so` elsewhere; the root Makefile picks the right name via `uname`.
- 3DS: `core/build_3ds.sh` (requires devkitARM, produces a static `.a` with `-D_3DS`).
- `make test` runs the headless ASan harness (see `test/harness.c` below). There is no linter.

## Asset pipeline

Manifest-driven via `gfxtool` (`tools/gfxtool.c`, built by `make gfxtool`). Each pack has a manifest in `assets/*.mfst` declaring `type` (background/fullsprite/halfsprite), `tileset` images (PNG or BMP) and `palette` images (16 colors from the first 16 raster pixels, `alpha=i,j` marks semitransparent colors). 8-bit indexed BMP tilesets contribute their pixel indices directly (qdbmp); other images are matched RGB-exact against the palette and fail loudly on unknown colors. Other subcommands: `gfxtool dump in.gfx out.png` (contact sheet), `gfxtool import-scene image palette out.gfx out.map` (slice + dedup a 512×512 scene into tileset + tilemap).

Tilemaps ship as `.map` files (`"MAP\n"`, width/height bytes, big-endian uint16 entries) loaded by `read_map_data()`. Scene 2's tileset/palette/map are *generated* by `tools/gen_scene2.py` (`make scene2-assets` to regenerate) — its tileset mirrors bg0's animation-band layout (0–5, 8–14, 16/20/21, 17–19, statics 6/7/15) so the scripted sequences in `libretro-core.c` apply to both scenes; keep that layout when adding scene tilesets.

The root Makefile generates all `core/*.gfx` + `core/*.map`. At runtime `retro_init` loads these (and the music module `core/test.xm`) by relative path, so RetroArch must be launched with `core/` as the working directory (which `make run` does). Missing assets are logged, not fatal. The game boots into a scene-select menu (`enter_menu`/`start_scene`); scene 1's map is procedural, scene 2 loads `scene2.map`. Save states record `current_scene` and reload the scene tileset on restore.

## Architecture

The core builds from **two objects**: `gfx_engine.c` — the engine proper, a plain library whose entire state lives in an explicit `gfx_context` passed to every function (no globals of its own) — and `core/libretro-core.c`, which `#include`s `../game2.h`. The game layer is still header-style app code: `game2.h` defines the one `gfx_context GFX` instance plus all game globals, and is included exactly once per binary (the core, or a test). Engine calls all take `&GFX`. The build is `-Wall -Wextra -pedantic` clean; keep it that way.

- **`gfx_engine.h`** — the engine. Emulates 16-bit-console-style hardware in software:
  - Color is ARGB 1555 (`color_16bit`, masks at top of file). Palette index 0 is transparent everywhere except BG0, where it is the backdrop color.
  - Four layers, rendered back to front: BG0, BG1 (16x16-tile backgrounds), SP0 = "full sprites" (`fsp`, 16x16, 32 slots, for characters/ships), SP1 = "half sprites" (`hsp`, 8x8, 128 slots, for bullets/score/HUD text via `draw_text`).
  - Each layer is a global struct (`bg[2]`, `fsp`, `hsp`) holding palettes, a tileset (1024 tiles), and either a 32x32 tilemap with per-scanline scroll offsets (backgrounds) or OAM arrays (sprites). Sprite OAM slots are allocated from a free-list stack (`add_fsp`/`delete_fsp` are O(1)); a returned id equal to `fsp_count`/`hsp_count` means "no free slot".
  - Sprite/tile attributes are bit-packed into `uint16_t` OAM words manipulated through `Mask_*` #defines (in_use, enable, palette, tile index, flips, x/y position oversampled by 3 bits). This bitmask-packing idiom pervades the codebase — follow it when adding state.
  - Sprite transforms (`set_fsp_effects`/`set_hsp_effects`): h-flip and rotation select pre-computed tileset variants generated at load time (`tile_h`, `tile_r`, `tile_rh` — memory for speed); v-flip and double-size are computed at render time. `rotation+h_flip+v_flip` = rotate −90°.
  - `read_gfx_data(file, gfxtype)` parses `.gfx` files into these structs (gfxtype 0/1 = BG0/BG1, 2 = fsp, 3 = hsp) and regenerates the transform variants.
- **`game2.h`** — the game layer, structure-of-arrays style: each entity kind is one struct of parallel arrays whose elements are bit-packed words. Includes AABB collisions, firing, the scripted **timeline system** (`tl_event` lists — "at frame N do X", with `TL_LOOP` for repeating waves; this is how levels/waves are authored), four synthesized **SFX voices** (`sfx_play`, rendered in the core's 44.1 kHz `audio_batch_cb` mix), the scene menu, and the HUD (owns hsp slots 0–15 by allocation order — fragile, don't reorder `add_hud`). Scenes come in two kinds (`scene_defs` in libretro-core.c): SHMUP and POND — the koi pond drives sprite rotation as fish headings, per-scanline sine offsets as water refraction, and palette rotation as caustics. The old AoS `game.h` was deleted in 2026.
- **`core/libretro-core.c`** — libretro entry points. `retro_init` initializes engine structs, loads `.gfx` assets and music; `retro_run` polls input, steps game state, calls `render_frame()` and pushes audio frames from libxmp. The renderer composes both backgrounds into `bg_cache` via span-based `render_bg_scanline()` (one tilemap lookup per tile-aligned span; shared by both layers via a base/overlay flag) — **anything that changes background appearance (scroll, tilemap writes, bg palettes, viewport) must set `bg_cache_dirty`** or the change won't show. Sprites draw on top each frame through `render_sprite_layer()` (clipped once per sprite). Live palettes are derived: base palettes → shimmer/pulse effects → `set_fade(0..32)`; never write palettes directly, go through the base arrays. Save states walk the `save_blocks[]` manifest — anything new that mutates after load (including function-`static` state, which must be promoted to file scope) must be added to that table or save states will silently miss it.
- **`test/units.c` + `test/harness.c`** — run with `make test`: 67 unit checks over the pure parts, then 1800 frames of gameplay with frame CRCs plus a save-state serialize/restore/replay round-trip, all under AddressSanitizer, no RetroArch or libxmp needed. Run after any engine or game-logic change; renderer changes should keep the frame CRCs identical unless visuals intentionally changed. `HARNESS_DUMP=<prefix>` makes the harness write frames as PNGs (how the README screenshots are made).
- **`bmp_to_gfx.c` + `qdbmp.c/h`** — standalone host-side asset converter (qdbmp is a vendored BMP reader).

Comments are a mix of English and Spanish; both are fine.
