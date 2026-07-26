#!/usr/bin/env python3
"""Generates the Crystal Cavern (scene 2) assets:
  assets/scene2_tiles.png  352x16 tileset strip, 22 tiles
  assets/scene2_pal0.png   16x1 palette
  assets/scene2.map        32x32 tilemap (MAP format)

The tileset mirrors bg0's animation-band layout so the engine's scripted
sequences apply unchanged:
  0-5   vein band (conduit analog)   6  solid rock (static)
  7     rock with vein (static)      8-14 crystal growth (porthole analog)
  15    empty/transparent (static)   16,20,21 geode sparkle (glimmer analog)
  17-19 amethyst pulse (plasma analog)
All RGB values are multiples of 8 so they survive the 5-bit quantization.
"""
from PIL import Image
import struct

PAL = [
    (0, 0, 16),      # 0 backdrop (transparent on BG0)
    (16, 16, 24),    # 1 darkest rock outline
    (40, 32, 56),    # 2 rock dark
    (64, 56, 80),    # 3 rock mid
    (104, 88, 120),  # 4 rock light
    (152, 136, 160), # 5 rock highlight
    (24, 64, 80),    # 6 teal deep
    (32, 104, 112),  # 7 teal mid
    (48, 160, 152),  # 8 teal bright
    (96, 216, 200),  # 9 teal glow (semitransparent)
    (88, 48, 136),   # 10 purple deep
    (136, 72, 192),  # 11 purple mid
    (192, 120, 240), # 12 purple bright
    (248, 184, 248), # 13 pink glow (semitransparent)
    (248, 248, 216), # 14 warm white sparkle
    (32, 16, 40),    # 15 shadow violet
]

N_TILES = 22
tiles = [[[0] * 16 for _ in range(16)] for _ in range(N_TILES)]


def rock(x, y):
    v = (x * 3 + y * 5) % 7
    if v == 0: return 3
    if v == 5: return 1
    return 2


def base_rock(t):
    for y in range(16):
        for x in range(16):
            t[y][x] = rock(x, y)
    for i in range(16):  # edge shading
        t[0][i] = 4 if i % 3 else 3
        t[15][i] = 1
        t[i][0] = 4 if i % 4 else 3
        t[i][15] = 1


# --- tile 6: solid rock, static
base_rock(tiles[6])
for y in range(16):
    for x in range(16):
        if (x * 7 + y * 11) % 23 == 0:
            tiles[6][y][x] = 4

# --- tile 7: rock with a purple vein, static
base_rock(tiles[7])
for y in range(16):
    for x in range(16):
        d = (x + y) - 15
        if d == 0: tiles[7][y][x] = 11
        elif abs(d) == 1: tiles[7][y][x] = 10

# --- tiles 0-5: vein band, energy flowing brighter with each frame
for f in range(6):
    t = tiles[f]
    base_rock(t)
    for y in range(5, 11):
        for x in range(1, 15):
            sparkle = (x * 5 + y * 3) % 4
            level = f - sparkle
            if level < 0: c = 6
            else: c = min(9, 6 + level)
            if y in (5, 10):  # vein rim stays deep
                c = min(c, 7)
            t[y][x] = c
    if f == 5:  # white-hot sparkles at full power
        for x in (4, 9, 13):
            t[7][x] = 14

# --- tiles 8-14: crystal growth on shadow, growing with each frame
for f in range(7):
    t = tiles[8 + f]
    for y in range(16):
        for x in range(16):
            t[y][x] = 15 if (x * 3 + y * 7) % 11 else 1
    r = f + 1
    for y in range(16):
        for x in range(16):
            md = abs(x - 8) + abs(y - 8)
            if md <= r:
                if md >= r: c = 12
                elif md >= r - 1: c = 11
                else: c = 10
                if md <= 1: c = 13
                t[y][x] = c
    if f >= 5:  # sparkles at the tips
        t[8 - r][8] = 14
        t[8 + r if 8 + r < 16 else 15][8] = 14

# --- tile 15: empty (all transparent, the cavern void)
# already zeroed

# --- tiles 16,20,21: geode, sparkles rotate colors
SPARKS = [(6, 6), (9, 5), (7, 9), (10, 8), (5, 8), (8, 7), (9, 10), (6, 10)]
for n, f in ((16, 0), (20, 1), (21, 2)):
    t = tiles[n]
    for y in range(16):
        for x in range(16):
            d2 = (x - 8) * (x - 8) + (y - 8) * (y - 8)
            if d2 >= 49: t[y][x] = rock(x, y)
            elif d2 >= 30: t[y][x] = 4 if (x + y) % 3 else 5
            elif d2 >= 22: t[y][x] = 1
            else: t[y][x] = 15
    for k, (sx, sy) in enumerate(SPARKS):
        t[sy][sx] = (8, 12, 14)[(k + f) % 3]

# --- tiles 17-19: amethyst cluster pulse
for f in range(3):
    t = tiles[17 + f]
    for y in range(16):
        for x in range(16):
            t[y][x] = 15 if (x + y * 2) % 9 else 1
    for cx, cy, r in ((5, 10, 3), (10, 9, 4), (8, 4, 2)):
        for y in range(16):
            for x in range(16):
                md = abs(x - cx) + abs(y - cy)
                if md <= r:
                    c = 10 + f if md == r else 11 + f
                    t[y][x] = min(13, c)
    if f == 2:
        t[4][8] = 14
        t[9][10] = 14

# ---- write tileset strip and palette
strip = Image.new('RGB', (N_TILES * 16, 16))
for n in range(N_TILES):
    for y in range(16):
        for x in range(16):
            strip.putpixel((n * 16 + x, y), PAL[tiles[n][y][x]])
strip.save('assets/scene2_tiles.png')

pal_img = Image.new('RGB', (16, 1))
pal_img.putdata(PAL)
pal_img.save('assets/scene2_pal0.png')

# ---- tilemap: cavern with ceiling, floor and crystal clusters
M = [[15] * 32 for _ in range(32)]
for x in range(32):
    M[0][x] = 7 if x % 5 == 2 else 6
    M[1][x] = 6 if x % 3 else 7
    M[30][x] = 6 if (x + 1) % 3 else 7
    M[31][x] = 7 if x % 4 == 1 else 6
    if x % 3 == 0:
        M[2][x] = 8 + (x % 7)          # hanging crystals (growth band)
    if (x + 1) % 3 == 0:
        M[29][x] = 8 + ((x * 2) % 7)   # floor crystals

for k in range(14):  # floating clusters of three, like scene 1
    cx = (k * 37) % 26 + 3
    cy = (k * 53) % 22 + 4
    M[cy][cx] = (k * 11) % 6                       # vein band
    M[cy][cx + 1] = 8 + (k * 5) % 7                # growth band
    M[cy][cx + 2] = (16, 20, 21, 17)[k % 4]        # geode or amethyst

for k in range(6):  # lone amethyst pulses
    M[(k * 41) % 20 + 6][(k * 29) % 28 + 2] = 17 + k % 3

with open('assets/scene2.map', 'wb') as f:
    f.write(b'MAP\n')
    f.write(bytes([32, 32]))
    for row in M:
        for v in row:
            f.write(struct.pack('>H', v))

print('scene2 assets written: 22 tiles, 16 colors, 32x32 map')
