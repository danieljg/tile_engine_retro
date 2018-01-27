all: core/tile_engine_retro_libretro.so GFX

run: core/tile_engine_retro_libretro.so GFX
	cd core && retroarch -L ./tile_engine_retro_libretro.so

core/tile_engine_retro_libretro.so: gfx_engine.h core/libretro.h core/libretro-core.c core/link.T
	cd core && make

bmptogfx: bmp_to_gfx.c qdbmp.c
	gcc -o bmptogfx bmp_to_gfx.c qdbmp.c -lm

# Generate GFX
GFX: core/bg0.gfx core/fsp.gfx core/hsp.gfx

core/bg0.gfx: bmptogfx bmp/bg0.bmp
	./bmptogfx bmp/bg0.bmp core/bg0.gfx 0

core/fsp.gfx: bmptogfx bmp/fsp1.bmp bmp/fsp1.pal bmp/fsp2.bmp bmp/fsp2.pal
	./bmptogfx bmp/fsp1.bmp bmp/fsp1.pal bmp/fsp2.bmp bmp/fsp2.pal core/fsp.gfx 1

core/hsp.gfx: bmptogfx bmp/hsp1.bmp
	./bmptogfx bmp/hsp1.bmp core/hsp.gfx 2
