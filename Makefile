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
bg0_input =	bmp/bg0.bmp	bmp/bg0.pal
bg1_input =	bmp/bg1.bmp	bmp/bg1.pal
fsp_input = \
	bmp/fsp1.bmp bmp/fsp2.bmp \
	bmp/fsp1.pal bmp/fsp2.pal bmp/fsp3.pal bmp/fsp4.pal bmp/fsp5.pal
hsp_input = \
	bmp/hsp1.bmp\
	bmp/hsp1.pal bmp/hsp2.pal bmp/hsp3.pal

all: $(CORE) GFX

run: $(CORE) GFX
	cd core && "$(RETROARCH)" -L ./$(notdir $(CORE))

$(CORE): $(core_source)
	cd core && make

bmptogfx: bmp_to_gfx.c qdbmp.c
	gcc -o bmptogfx bmp_to_gfx.c qdbmp.c -lm

# Generate GFX
GFX: core/bg0.gfx core/bg1.gfx core/fsp.gfx core/hsp.gfx

core/bg0.gfx: bmptogfx $(bg0_input)
	./bmptogfx $(bg0_input) core/bg0.gfx 0

core/bg1.gfx: bmptogfx $(bg1_input)
	./bmptogfx $(bg1_input) core/bg1.gfx 0

core/fsp.gfx: bmptogfx $(fsp_input)
	./bmptogfx $(fsp_input) core/fsp.gfx 1

core/hsp.gfx: bmptogfx $(hsp_input)
	./bmptogfx $(hsp_input) core/hsp.gfx 2

clean:
	rm -f core/*.o
	rm -f core/*.so core/*.dylib
	rm -f core/*.gfx
	rm -f bmptogfx
