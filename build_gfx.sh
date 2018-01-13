#!/bin/bash
gcc -o bmptogfx bmp_to_gfx.c qdbmp.c -lm
./bmptogfx font_numbers_8x8.bmp output.gfx 8
mv output.gfx core/font_numbers_8x8.gfx
./bmptogfx bg_stars.bmp output.gfx 16
mv output.gfx core/bg_stars.gfx
./bmptogfx sr388_invader.bmp output.gfx 16
mv output.gfx core/sr388_invader.gfx