all: core/tile_engine_retro_libretro.so GFX

run: core/tile_engine_retro_libretro.so GFX
	cd core && retroarch -L ./tile_engine_retro_libretro.so

core/tile_engine_retro_libretro.so: gfx_engine.h core/libretro.h core/libretro-core.c core/link.T
	cd core && make

GFX: core/font_8x8.gfx core/bg_stars.gfx core/fsp.gfx core/space_16x16_tilesheet.gfx

bmptogfx: bmp_to_gfx.c qdbmp.c
	gcc -o bmptogfx bmp_to_gfx.c qdbmp.c -lm

core/font_8x8.gfx: bmptogfx font_8x8.bmp
	./bmptogfx font_8x8.bmp core/font_8x8.gfx 2

core/bg_stars.gfx: bmptogfx bg_stars.bmp
	./bmptogfx bg_stars.bmp core/bg_stars.gfx 0

core/fsp.gfx: bmptogfx fsp.bmp
	./bmptogfx fsp.bmp core/fsp.gfx 1

core/space_16x16_tilesheet.gfx: bmptogfx space_16x16_tilesheet.bmp
	./bmptogfx space_16x16_tilesheet.bmp core/space_16x16_tilesheet.gfx 0
