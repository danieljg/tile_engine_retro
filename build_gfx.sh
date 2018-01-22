#!/bin/bash
gcc -o bmptogfx bmp_to_gfx.c qdbmp.c -lm
./bmptogfx font_8x8.bmp output.gfx 8
mv output.gfx core/font_8x8.gfx
./bmptogfx bg_stars.bmp output.gfx 16
mv output.gfx core/bg_stars.gfx
./bmptogfx fsp.bmp output.gfx 16
mv output.gfx core/fsp.gfx
./bmptogfx panels_16x16_tilesheet.bmp output.gfx 16
mv output.gfx core/panels_16x16_tilesheet.gfx
./bmptogfx space_16x16_tilesheet.bmp output.gfx 16
mv output.gfx core/space_16x16_tilesheet.gfx
