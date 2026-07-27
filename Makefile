# The core's filename differs per OS (see core/Makefile)
UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
	CORE = core/tile_engine_retro_libretro.dylib
else
	CORE = core/tile_engine_retro_libretro.so
endif

# RetroArch may live in PATH or in an app bundle (MacPorts/official dmg)
RETROARCH ?= $(shell command -v retroarch \
	|| ls /Applications/MacPorts/RetroArch.app/Contents/MacOS/RetroArch \
	      /Applications/RetroArch.app/Contents/MacOS/RetroArch 2>/dev/null \
	   | head -1)

core_source = \
	gfx_engine.h game2.h core/libretro.h core/libretro-core.c core/link.T

all: $(CORE) GFX

run: $(CORE) GFX
	@test -n "$(RETROARCH)" || { \
	  echo "RetroArch not found. On Linux Mint/Ubuntu run ./setup_linux.sh"; \
	  exit 1; }
	cd core && "$(RETROARCH)" -L ./$(notdir $(CORE))

$(CORE): $(core_source)
	cd core && make

# Asset pipeline: manifest-driven (see assets/*.mfst and tools/gfxtool.c)
gfxtool: tools/gfxtool.c tools/qdbmp.c tools/stb_image.h tools/stb_image_write.h
	cc -O2 -std=gnu99 -o gfxtool tools/gfxtool.c tools/qdbmp.c -lm

GFX: core/bg0.gfx core/bg1.gfx core/fsp.gfx core/hsp.gfx \
     core/scene2.gfx core/scene2.map \
     $(CAUSTIC_STYLES) $(SURFACE_STYLES) core/scene3_surface.map \
     core/scene3_depths.gfx \
     core/scene3_floor.gfx core/scene3_floor.map \
     core/scene3_depth1.map core/scene3_depth2.map \
     core/scene3_caustics.map

core/bg0.gfx: gfxtool assets/bg0.mfst bmp/bg0.bmp assets/bg0_pal0.png
	./gfxtool build assets/bg0.mfst core/bg0.gfx

core/bg1.gfx: gfxtool assets/bg1.mfst bmp/bg1.bmp assets/bg1_pal0.png
	./gfxtool build assets/bg1.mfst core/bg1.gfx

core/fsp.gfx: gfxtool assets/fsp.mfst bmp/fsp1.bmp bmp/fsp2.bmp \
              $(wildcard assets/fsp_pal*.png)
	./gfxtool build assets/fsp.mfst core/fsp.gfx

core/hsp.gfx: gfxtool assets/hsp.mfst bmp/hsp1.bmp \
              $(wildcard assets/hsp_pal*.png)
	./gfxtool build assets/hsp.mfst core/hsp.gfx

core/scene2.gfx: gfxtool assets/scene2.mfst assets/scene2_tiles.png \
                 assets/scene2_pal0.png
	./gfxtool build assets/scene2.mfst core/scene2.gfx

core/scene2.map: assets/scene2.map
	cp assets/scene2.map core/scene2.map

core/scene3_depths.gfx: gfxtool assets/scene3_depths.mfst \
                        assets/scene3_depths_tiles.png \
                        assets/scene3_depths_pal0.png
	./gfxtool build assets/scene3_depths.mfst core/scene3_depths.gfx

core/scene3_surface.map: assets/scene3_surface.map
	cp assets/scene3_surface.map core/scene3_surface.map

CAUSTIC_STYLES = core/scene3_caustics0.gfx core/scene3_caustics1.gfx \
                 core/scene3_caustics2.gfx core/scene3_caustics3.gfx
SURFACE_STYLES = core/scene3_surface0.gfx core/scene3_surface1.gfx \
                 core/scene3_surface2.gfx

core/scene3_caustics%.gfx: gfxtool assets/scene3_caustics%.mfst \
                           assets/scene3_caustics%_tiles.png \
                           assets/scene3_caustics_pal0.png
	./gfxtool build assets/scene3_caustics$*.mfst core/scene3_caustics$*.gfx

core/scene3_surface%.gfx: gfxtool assets/scene3_surface%.mfst \
                          assets/scene3_surface%_tiles.png \
                          assets/scene3_surface_pal0.png
	./gfxtool build assets/scene3_surface$*.mfst core/scene3_surface$*.gfx

# the pond bed is a painted 512x512 scene, sliced and deduplicated
core/scene3_floor.gfx core/scene3_floor.map: gfxtool \
                       assets/scene3_floor.png assets/scene3_floor_pal.png
	./gfxtool import-scene assets/scene3_floor.png \
		assets/scene3_floor_pal.png core/scene3_floor.gfx core/scene3_floor.map

core/scene3_depth1.map: assets/scene3_depth1.map
	cp assets/scene3_depth1.map core/scene3_depth1.map

core/scene3_depth2.map: assets/scene3_depth2.map
	cp assets/scene3_depth2.map core/scene3_depth2.map

core/scene3_caustics.map: assets/scene3_caustics.map
	cp assets/scene3_caustics.map core/scene3_caustics.map

# Regenerate the generated scene art (requires python3 + Pillow)
scene2-assets:
	python3 tools/gen_scene2.py

scene3-assets:
	python3 tools/gen_scene3.py

# Headless tests: unit tests for the pure parts, then a 30s gameplay run
# plus a save-state round trip. All under AddressSanitizer; no RetroArch,
# no libxmp. Set HARNESS_DUMP=<prefix> to also dump frames as PNGs.
test: GFX
	cc -g -O1 -fsanitize=address -std=gnu99 -Wall -Wextra \
		-o core/test_units test/units.c gfx_engine.c
	cd core && ./test_units
	cc -g -O1 -fsanitize=address -std=gnu99 -Icore \
		-o core/test_harness test/harness.c core/libretro-core.c gfx_engine.c
	cd core && ./test_harness

clean:
	rm -f core/*.o
	rm -f core/*.so core/*.dylib
	rm -f core/*.gfx core/*.map
	rm -f core/test_harness
	rm -rf core/test_harness.dSYM
	rm -f gfxtool
