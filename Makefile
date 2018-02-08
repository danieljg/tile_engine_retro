core_source = \
	gfx_engine.h game.h core/libretro.h core/libretro-core.c core/link.T
bg0_input =	bmp/bg0.bmp	bmp/bg0.pal
bg1_input =	bmp/bg1.bmp	bmp/bg1.pal
fsp_input = \
	bmp/fsp1.bmp bmp/fsp2.bmp \
	bmp/fsp1.pal bmp/fsp2.pal bmp/fsp3.pal bmp/fsp4.pal bmp/fsp5.pal
hsp_input = \
	bmp/hsp1.bmp\
	bmp/hsp1.pal bmp/hsp2.pal bmp/hsp3.pal

all: core/tile_engine_retro_libretro.so GFX

run: core/tile_engine_retro_libretro.so GFX
	cd core && retroarch -L ./tile_engine_retro_libretro.so

core/tile_engine_retro_libretro.so: $(core_source)
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
	rm core/*.o
	rm core/*.so
	rm core/*.gfx
