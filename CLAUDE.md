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
- There are no tests and no linter.

## Asset pipeline

Source art lives in `bmp/` as BMP files plus `.pal` palette files (and `.pal.clr` variants). `bmptogfx` converts them into the custom `.gfx` format loaded at runtime:

```sh
./bmptogfx <input bmps and pals...> <output.gfx> <gfx_type>
# gfx_type: 0 = background tileset (16x16), 1 = full sprites (16x16), 2 = half sprites (8x8)
```

The root Makefile generates `core/bg0.gfx`, `core/bg1.gfx`, `core/fsp.gfx`, `core/hsp.gfx`. At runtime `retro_init` loads these (and the music module `core/test.xm`) by relative path, so RetroArch must be launched with `core/` as the working directory (which `make run` does). Missing assets are logged, not fatal.

## Architecture

The whole core is a **single translation unit**: `core/libretro-core.c` directly `#include`s `../gfx_engine.h` and `../game.h`, which contain implementations and global state, not just declarations. There is no separate compilation; touching any header rebuilds the one object file.

- **`gfx_engine.h`** — the engine. Emulates 16-bit-console-style hardware in software:
  - Color is ARGB 1555 (`color_16bit`, masks at top of file). Palette index 0 is transparent everywhere except BG0, where it is the backdrop color.
  - Four layers, rendered back to front: BG0, BG1 (16x16-tile backgrounds), SP0 = "full sprites" (`fsp`, 16x16, 32 slots, for characters/ships), SP1 = "half sprites" (`hsp`, 8x8, 128 slots, for bullets/score/HUD text via `draw_text`).
  - Each layer is a global struct (`bg[2]`, `fsp`, `hsp`) holding palettes, a tileset (1024 tiles), and either a 32x32 tilemap with per-scanline scroll offsets (backgrounds) or OAM arrays (sprites).
  - Sprite/tile attributes are bit-packed into `uint16_t` OAM words manipulated through `Mask_*` #defines (in_use, enable, palette, tile index, flips, x/y position oversampled by 3 bits). This bitmask-packing idiom pervades the codebase — follow it when adding state.
  - `read_gfx_data(file, gfxtype)` parses `.gfx` files into these structs (gfxtype 0/1 = BG0/BG1, 2 = fsp, 3 = hsp).
- **`game.h`** — the active game logic (players, enemies, projectiles, power-ups, scores) using bit-packed `physics_body` structs. `game2.h` is a newer variant that is **not yet included anywhere** — check which one is wired into `libretro-core.c` before editing game logic.
- **`core/libretro-core.c`** — libretro entry points. `retro_init` initializes engine structs, loads `.gfx` assets and music; `retro_run` polls input, steps game state, calls `render_frame()` (software-composites all layers into a `uint16_t` framebuffer) and pushes audio frames from libxmp.
- **`bmp_to_gfx.c` + `qdbmp.c/h`** — standalone host-side asset converter (qdbmp is a vendored BMP reader).

Comments are a mix of English and Spanish; both are fine.
