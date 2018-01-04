#!/bin/bash
gcc -o bmptogfx bmp_to_gfx.c qdbmp.c -lm
./bmptogfx font_numbers_8x8.bmp output.gfx