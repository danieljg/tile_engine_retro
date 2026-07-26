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
	cd core && "$(RETROARCH)" -L ./$(notdir $(CORE))

$(CORE): $(core_source)
	cd core && make

# Asset pipeline: manifest-driven (see assets/*.mfst and tools/gfxtool.c)
gfxtool: tools/gfxtool.c tools/qdbmp.c tools/stb_image.h tools/stb_image_write.h
	cc -O2 -std=gnu99 -o gfxtool tools/gfxtool.c tools/qdbmp.c -lm

GFX: core/bg0.gfx core/bg1.gfx core/fsp.gfx core/hsp.gfx \
     core/scene2.gfx core/scene2.map

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

# Regenerate the generated scene 2 art (requires python3 + Pillow)
scene2-assets:
	python3 tools/gen_scene2.py

# Headless test: drives the core through 30s of gameplay plus a
# save-state round trip under AddressSanitizer. No RetroArch, no libxmp.
test: GFX
	cc -g -O1 -fsanitize=address -std=gnu99 -Icore \
		-o core/test_harness test/harness.c core/libretro-core.c
	cd core && ./test_harness

clean:
	rm -f core/*.o
	rm -f core/*.so core/*.dylib
	rm -f core/*.gfx core/*.map
	rm -f core/test_harness
	rm -rf core/test_harness.dSYM
	rm -f gfxtool
