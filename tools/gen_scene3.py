#!/usr/bin/env python3
"""Generates the Koi Pond (scene 3) assets for the five-plane architecture:

  bg4 floor    scene3_depths_tiles.png (shared set) + scene3_floor.map
               An opaque sandy pond bottom: grain, dune ripples, raised
               banks with edge-transition tiles, pebbles, debris, weeds,
               caustic-lit cells (colors 6-9 rotate at runtime).
  bg3 caustics scene3_caustics_tiles.png + scene3_caustics.map
               Bright translucent light: cellular caustic webs (4 frames,
               Voronoi-edge style) and godray bands. Palette sweeps fast.
  bg2/bg1      shared depths tileset + scene3_depth2.map / scene3_depth1.map
  depth veils  Feathered translucent water clouds (colors 3-5 with alpha)
               and tall weeds; fish below a veil get tinted by it.
  bg0 surface  scene3_surface_tiles.png + scene3_surface.map
               Translucent wave arcs and bank reeds. Pads are sprites.

  Sprites: koi (fsp 18-19 + palettes 5-6), lily pads (fsp 20-24 via
  index remap onto fsp palette 0), pond half-sprites (hsp 128-133:
  ripples, pellet, sparkle twinkles).

All RGB values are multiples of 8 to survive 5-bit quantization.
"""
from PIL import Image
import struct
import math

OFF = 0x8000
T = 16  # tile size


def h8(x, y, seed=0):
    """Deterministic pixel hash, 0..255 — organic grain without grids."""
    v = (x * 374761393 + y * 668265263 + seed * 974711) & 0xFFFFFFFF
    v = (v ^ (v >> 13)) * 1274126177 & 0xFFFFFFFF
    return (v >> 16) & 0xFF


def new_tile():
    return [[0] * T for _ in range(T)]


def save_strip(tiles, pal, path, size=T):
    img = Image.new('RGB', (len(tiles) * size, size))
    for n, t in enumerate(tiles):
        for y in range(size):
            for x in range(size):
                img.putpixel((n * size + x, y), pal[t[y][x]])
    img.save(path)


def save_pal(pal, path):
    im = Image.new('RGB', (16, 1))
    im.putdata(pal)
    im.save(path)


def save_map(m, path):
    with open(path, 'wb') as f:
        f.write(b'MAP\n')
        f.write(bytes([32, 32]))
        for row in m:
            for v in row:
                f.write(struct.pack('>H', v))


# ================= depths palette (floor + veils, shared) =================
DPAL = [
    (104, 88, 64),  # 0 sand base (backdrop, olive-leaning)
    (84, 68, 48),   # 1 sand grain dark
    (120, 104, 80), # 2 sand grain light
    (64, 104, 88),  # 3 water column  } translucent; (sand+3)/2 = wet olive
    (72, 120, 112), # 4 veil cloud mid} the depth layers
    (88, 144, 128), # 5 veil cloud bright
    (128, 112, 72), # 6 lit sand dim   } warm light, rotated 6..9
    (152, 136, 88), # 7 lit sand mid   }
    (184, 164, 108),# 8 lit sand bright}
    (216, 196, 132),# 9 lit sand glint }
    (48, 72, 40),   # 10 weed dark
    (72, 104, 56),  # 11 weed light
    (152, 124, 88), # 12 bank light
    (184, 152, 108),# 13 bank bright
    (64, 48, 32),   # 14 debris / dark accents
    (96, 88, 80),   # 15 pebble gray
]

# ---- depths tileset: 21 tiles, laid out in bands ----
#  0 sand fine        1 sand coarse      2 dune ripple A   3 dune ripple B
#  4 lit sand A       5 lit sand B       6 pebbles small   7 pebble cluster
#  8 bank interior    9 bank edge N     10 bank edge S    11 bank edge W
# 12 bank edge E     13 debris          14 weed tuft      15 weed tall lower
# 16 weed tall upper 17 veil sparse     18 veil medium    19 veil dense
# 20 veil feather    21 water veil A (upper) 22 water veil B (lower)
N_D = 23
dt = [new_tile() for _ in range(N_D)]


def sand(t, seed, density=1):
    for y in range(T):
        for x in range(T):
            h = h8(x, y, seed)
            if h < 9 * density: t[y][x] = 1
            elif h > 255 - 7 * density: t[y][x] = 2
            else: t[y][x] = 0

sand(dt[0], 1)
sand(dt[1], 7, density=2)

for n, seed in ((2, 3), (3, 11)):  # dune ripples: wavy horizontal ridges
    sand(dt[n], seed)
    for y in range(T):
        for x in range(T):
            ridge = (y + int(2.5 * math.sin(x * 0.42 + seed))) % 8
            if ridge == 0:
                dt[n][y][x] = 1
            elif ridge == 1 and h8(x, y, seed) > 70:
                dt[n][y][x] = 2

for n, seed in ((4, 5), (5, 13)):  # caustic-lit sand: teal glints on grain
    sand(dt[n], seed)
    for y in range(T):
        for x in range(T):
            h = h8(x, y, seed + 40)
            if h < 14:
                dt[n][y][x] = 6 + (h % 3)
            elif h < 20:
                dt[n][y][x] = 9

sand(dt[6], 17)  # small pebbles
for cx, cy, r in ((4, 5, 1.8), (11, 3, 1.4), (8, 11, 2.1), (13, 13, 1.5)):
    for y in range(T):
        for x in range(T):
            d = math.hypot(x - cx + .5, y - cy + .5)
            if d <= r:
                dt[6][y][x] = 15 if d < r - 0.9 else 14
sand(dt[7], 23)  # pebble cluster
for cx, cy, r in ((5, 4, 2.6), (11, 7, 2.2), (4, 11, 1.9), (12, 12, 2.4),
                  (8, 8, 1.6)):
    for y in range(T):
        for x in range(T):
            d = math.hypot(x - cx + .5, y - cy + .5)
            if d <= r:
                c = 15 if d < r - 1 else 14
                if x - cx < -r * 0.3 and y - cy < -r * 0.3 and d < r - 1:
                    c = 13  # light from upper-left
                dt[7][y][x] = c


def bank(t, seed):  # raised sand bank interior
    for y in range(T):
        for x in range(T):
            h = h8(x, y, seed)
            if h < 22: t[y][x] = 12
            elif h > 240: t[y][x] = 13
            else: t[y][x] = 12 if h & 1 else 13

bank(dt[8], 29)
# bank edges: dithered transition from bank to plain sand, one per side
for n, (dx, dy) in ((9, (0, -1)), (10, (0, 1)), (11, (-1, 0)), (12, (1, 0))):
    bank(dt[n], 29)
    for y in range(T):
        for x in range(T):
            #distance toward the outside edge of the tile
            along = (T - 1 - y) if dy < 0 else y if dy > 0 else \
                    (T - 1 - x) if dx < 0 else x
            fall = along / (T - 1.0)  # 0 deep in bank .. 1 at sand
            if h8(x, y, 31) < fall * 255:
                h = h8(x, y, 1)
                dt[n][y][x] = 1 if h < 9 else (2 if h > 248 else 0)
            #dark waterline contour where the bank steps down
            if 0.52 < fall < 0.68 and h8(x, y, 47) < 190:
                dt[n][y][x] = 14

sand(dt[13], 37)  # debris: sunken leaf + twig
for k in range(9):
    x = 4 + k
    y = 6 + int(1.8 * math.sin(k * 0.7))
    if 0 <= x < T:
        dt[13][y][x] = 14
        if k in (3, 4, 5) and y + 1 < T:
            dt[13][y + 1][x] = 10
for k in range(5):
    dt[13][12][7 + k] = 14

sand(dt[14], 41)  # weed tuft
for s, hgt in ((4, 6), (8, 8), (12, 5)):
    for k in range(hgt):
        y = T - 1 - k
        x = s + int(1.2 * math.sin(k * 0.8 + s))
        if 0 <= x < T:
            dt[14][y][x] = 10 if k % 3 else 11
            if x + 1 < T:
                dt[14][y][x + 1] = 11 if k % 3 else 10

sand(dt[15], 43)  # tall weed, lower half
# upper half floats over open water/veils, so it stays transparent-based
for s, lean in ((4, 0.9), (9, -0.7), (13, 0.6)):
    for k in range(T):
        y = T - 1 - k
        x = s + int(lean * 2.2 * math.sin(k * 0.35 + s))
        if 0 <= x < T:
            dt[15][y][x] = 10 if k % 3 else 11
            if x + 1 < T:
                dt[15][y][x + 1] = 11 if k % 3 else 10
    for k in range(T, T + 12):
        y = 2 * T - 1 - k
        x = s + int(lean * 2.2 * math.sin(k * 0.35 + s))
        if 0 <= x < T and 0 <= y < T:
            dt[16][y][x] = 11 if k % 3 else 10
            if x + 1 < T:
                dt[16][y][x + 1] = 10 if k % 3 else 11

# veils: translucent clouds; density falls off toward the tile edge so
# assembled blobs feather naturally
for n, (lo, hi, core) in ((17, (10, 26, 3)), (18, (26, 52, 4)),
                          (19, (52, 86, 5)), (20, (4, 11, 3))):
    for y in range(T):
        for x in range(T):
            edge = min(x, y, T - 1 - x, T - 1 - y) / (T / 2.0)
            dens = lo + (hi - lo) * min(1.0, edge * 1.6)
            h = h8(x, y, n * 7)
            if h < dens:
                dt[n][y][x] = core if h < dens * 0.4 else \
                              3 if core == 3 else core - 1


# full-coverage water veils: organic dither, no checker; A lighter, B
# fuller — stacked, they grade everything below them progressively
for n, (dens, mix) in ((21, (100, 255)), (22, (130, 90))):
    for y in range(T):
        for x in range(T):
            h = h8(x, y, n * 13)
            if h < dens:
                dt[n][y][x] = 4 if h < dens - mix else 3

# ---- floor map: banks with classified edges, dune flows, dressing ----
FM = [[0] * 32 for _ in range(32)]
for y in range(32):
    for x in range(32):
        h = h8(x, y, 100)
        band = (y + int(2.2 * math.sin(x * 0.35))) % 11
        if band == 0:
            FM[y][x] = 2 + (h & 1)       # dune streaks
        elif h < 30:
            FM[y][x] = 1                  # coarse patch
        elif h < 42:
            FM[y][x] = 4 + (h & 1)        # sunlit sand
        else:
            FM[y][x] = 0

BANKS = ((7, 6, 4.2), (24, 22, 5.0), (27, 4, 3.2), (4, 27, 3.4))
def in_bank(x, y):
    return any(math.hypot(x - cx, y - cy) <= r for cx, cy, r in BANKS)
for y in range(32):
    for x in range(32):
        if not in_bank(x, y):
            continue
        n_out = not in_bank(x, y - 1)
        s_out = not in_bank(x, y + 1)
        w_out = not in_bank(x - 1, y)
        e_out = not in_bank(x + 1, y)
        if not (n_out or s_out or w_out or e_out):
            FM[y][x] = 8
        elif n_out: FM[y][x] = 9
        elif s_out: FM[y][x] = 10
        elif w_out: FM[y][x] = 11
        else: FM[y][x] = 12
#pebbles hug the banks; debris and weeds fill the open sand
for x, y, t_ in ((11, 8, 7), (21, 24, 6), (26, 8, 6), (3, 23, 7), (12, 5, 6),
                 (28, 25, 7)):
    if FM[y][x] < 8:
        FM[y][x] = t_
#weed bases: tufts at the frond sprite anchors (FROND_SITES in game2.h)
for x, y in ((2, 3), (3, 11), (10, 2), (17, 8), (12, 13), (18, 13),
             (1, 7), (8, 9), (22, 5), (24, 12)):
    FM[y][x] = 14
for x, y in ((4, 15), (13, 21), (27, 28), (23, 2)):
    if FM[y][x] < 8:
        FM[y][x] = 14

# ---- depth veil maps: clouds assembled from center/ring/feather ----
def veil_blob(m, cx, cy, r, dense):
    for y in range(32):
        for x in range(32):
            d = math.hypot(x - cx, y - cy)
            if d > r:
                continue
            if d < r * 0.45:
                m[y][x] = 19 if dense else 18
            elif d < r * 0.8:
                m[y][x] = 18 if dense else 17
            else:
                m[y][x] = 20

#depth 1: sparse upper veil clouds (organic dither, no checker)
D1 = [[OFF] * 32 for _ in range(32)]  #sparse silt, not a blanket
for cx, cy, r in ((6, 8, 3.2), (20, 5, 2.8), (27, 16, 3.4), (10, 22, 3.0),
                  (22, 27, 3.2)):
    veil_blob(D1, cx, cy, r, dense=False)

D2 = [[OFF] * 32 for _ in range(32)]  #sparse silt, not a blanket
for cx, cy, r in ((13, 7, 4.2), (27, 9, 3.2), (5, 14, 3.6), (18, 18, 4.6),
                  (28, 23, 3.4), (9, 28, 4.0), (23, 30, 2.8), (1, 4, 2.7)):
    veil_blob(D2, cx, cy, r, dense=True)


# ================= caustics: four styles to choose between ===============
CPAL = [
    (0, 0, 0),      # 0 transparent
    (192, 184, 144),# 1 web dim      } warm light, swept 1..3
    (224, 216, 168),# 2 web bright   }
    (248, 244, 200),# 3 web glint    }
    (176, 164, 120),# 4 pool dim     } swept 4..6
    (208, 196, 144),# 5 pool mid     }
    (232, 224, 168),# 6 pool bright  }
    (248, 248, 240),# 7 sparkle
] + [(0, 0, 0)] * 8

NFRAME = 12  # frames of a looping light dance
NPOS = 64    # 8x8 tiles per period (128px)
PER = 128
N_C = NFRAME * NPOS + 3

def torus_d(ax, ay, bx, by):
    dx = abs(ax - bx); dy = abs(ay - by)
    if dx > PER / 2: dx = PER - dx
    if dy > PER / 2: dy = PER - dy
    return math.hypot(dx, dy)

# style 0 POOLS  : overlapping soft light pools
# style 1 WEB    : a fine caustic net, thin and dim
# style 2 MIX    : pools with a faint net riding over them
# style 3 SPECKLE: sparse drifting glints, barely there
POOLS = [((41 * k * k + 29 * k) % PER, (67 * k * k + 53 * k + 19) % PER,
          9.0 + (k % 5) * 3.0) for k in range(8)]
NETPTS = [((37 * k * k + 23 * k + 7) % PER, (53 * k * k + 41 * k + 17) % PER)
          for k in range(13)]

def caustic_tiles(style):
    tiles = [new_tile() for _ in range(N_C)]
    for f in range(NFRAME):
        ph = 2 * math.pi * f / NFRAME
        live = [((bx + 8.0 * math.cos(ph + k * 0.9)) % PER,
                 (by + 8.0 * math.sin(ph * (1 if k % 2 else -1) + k * 1.7)) % PER,
                 r * (1.0 + 0.10 * math.sin(ph * 2 + k)))
                for k, (bx, by, r) in enumerate(POOLS)]
        net = [((bx + 6.0 * math.cos(ph * 1.3 + k)) % PER,
                (by + 6.0 * math.sin(ph + k * 2.1)) % PER)
               for k, (bx, by) in enumerate(NETPTS)]
        for v in range(NPOS):
            cx, cy = v % 8, v // 8
            t = tiles[f * NPOS + v]
            for y in range(T):
                for x in range(T):
                    wx, wy = cx * 16 + x, cy * 16 + y
                    n = h8(wx, wy, f) / 255.0
                    inten = 0.0
                    if style in (0, 2, 3):
                        for px, py, r in live:
                            d = torus_d(wx, wy, px, py)
                            if d < r:
                                q = 1.0 - d / r
                                inten += q * q
                    if style == 0 or style == 2:
                        if inten > 0.80:   t[y][x] = 3
                        elif inten > 0.58: t[y][x] = 3 if n < 0.18 else 2
                        elif inten > 0.40: t[y][x] = 2 if n < 0.30 else 1
                        elif inten > 0.26 and n < (inten - 0.26) * 5.0:
                            t[y][x] = 1
                    if style == 3:  # speckle: only the very hottest points
                        if inten > 0.86 and n < 0.30:
                            t[y][x] = 3 if n < 0.12 else 2
                        elif inten > 0.55 and n < 0.06:
                            t[y][x] = 1
                    if style == 1 or style == 2:
                        ds = sorted(torus_d(wx, wy, px, py) for px, py in net)
                        gap = ds[1] - ds[0]
                        if style == 1:
                            if gap < 0.30:   t[y][x] = 2
                            elif gap < 0.60 and n < 0.55: t[y][x] = 1
                        else:  # a whisper of net over the pools
                            if gap < 0.18 and t[y][x] == 0: t[y][x] = 1
                            elif gap < 0.16: t[y][x] = 2
    # the tail tiles: two light pools and a sparkle, shared by all styles
    for n_, (rr, dens) in ((NFRAME * NPOS, (7.2, 150)),
                           (NFRAME * NPOS + 1, (6.0, 90))):
        for y in range(T):
            for x in range(T):
                d = math.hypot(x - 8 + .5, y - 8 + .5)
                if d > rr:
                    continue
                fall = 1.0 - d / rr
                if h8(x, y, n_ * 5) < dens * fall:
                    tiles[n_][y][x] = 6 if fall > 0.6 else 5 if fall > 0.3 else 4
    sp = NFRAME * NPOS + 2
    tiles[sp][6][9] = 7
    tiles[sp][10][4] = 7
    tiles[sp][2][13] = 7
    return tiles

POOL_A = NFRAME * NPOS
POOL_B = POOL_A + 1
SPARK_C = POOL_A + 2

CM = [[0] * 32 for _ in range(32)]
for y in range(32):
    for x in range(32):
        CM[y][x] = (x & 7) + (y & 7) * 8   #position variant, frame 0
for cx, cy in ((5, 7), (16, 3), (26, 12), (9, 19), (21, 23), (29, 27)):
    CM[cy][cx] = POOL_A
    if cx + 1 < 32: CM[cy][cx + 1] = POOL_B
for x, y in ((11, 8), (24, 15), (5, 22), (18, 28), (28, 8)):
    CM[y][x] = SPARK_C

# ================= surface: three styles =================================
# style 0 CONTOUR : soft cel contours drifting over a translucent film
# style 1 WEB     : the web-like reflections, broken so they read as
#                   glare rather than a grid
# style 2 CALM    : film and rare glints only, nearly invisible
WPAL = [
    (0, 0, 0),      # 0 transparent
    (112, 168, 152),# 1 base water tint (semitransparent)
    (176, 216, 200),# 2 contour (semitransparent)
    (232, 240, 216),# 3 crest glint — pulsed at runtime (semitransparent)
    (0, 0, 0),      # 4 unused
    (120, 144, 56), # 5 reed olive
    (84, 104, 40),  # 6 reed dark
] + [(0, 0, 0)] * 9

SFRAME = 8
SPOS = 64
SPER = 128
N_W = SFRAME * SPOS + 2
REED_A = SFRAME * SPOS
REED_B = REED_A + 1
W128 = 2 * math.pi / SPER

SNET = [((29 * k * k + 47 * k + 11) % SPER, (61 * k * k + 31 * k + 5) % SPER)
        for k in range(15)]

def surface_tiles(style):
    tiles = [new_tile() for _ in range(N_W)]
    for f in range(SFRAME):
        fp = 2 * math.pi * f / SFRAME
        net = [((bx + 5.0 * math.cos(fp + k * 1.1)) % SPER,
                (by + 5.0 * math.sin(fp * 1.2 + k * 0.7)) % SPER)
               for k, (bx, by) in enumerate(SNET)]
        for v in range(SPOS):
            vx, vy = v % 8, v // 8
            t = tiles[f * SPOS + v]
            for y in range(T):
                for x in range(T):
                    wx, wy = vx * 16 + x, vy * 16 + y
                    c = (math.sin(wx * W128 * 2 + fp)
                         + math.sin(wy * W128 * 3 - fp)
                         + 0.6 * math.sin((wx + wy) * W128 * 5 + 2 * fp)
                         + 0.45 * math.sin((wx - 2 * wy) * W128 * 7 - fp))
                    n = h8(wx, wy, 30)
                    if style == 0:
                        if abs(c) < 0.09:
                            t[y][x] = 3 if (abs(c) < 0.03
                                            and h8(wx, wy, 9) < 90) else 2
                        else:
                            t[y][x] = 1
                    elif style == 1:
                        # glare webs: cell walls, broken hard by noise so
                        # the repeat never reads as a grid
                        ds = sorted(((abs(wx - px) if abs(wx - px) < SPER/2
                                      else SPER - abs(wx - px)),
                                     (abs(wy - py) if abs(wy - py) < SPER/2
                                      else SPER - abs(wy - py)))
                                    for px, py in net)
                        d2 = sorted(math.hypot(a, b) for a, b in ds)
                        gap = d2[1] - d2[0]
                        if gap < 0.34 and h8(wx, wy, f + 3) < 150:
                            t[y][x] = 3 if h8(wx, wy, 9) < 60 else 2
                        elif gap < 0.75 and h8(wx, wy, f + 5) < 60:
                            t[y][x] = 2
                        else:
                            t[y][x] = 1
                    else:
                        if abs(c) < 0.035 and h8(wx, wy, 9) < 70:
                            t[y][x] = 3
                        else:
                            t[y][x] = 1
                    if t[y][x] == 0:
                        t[y][x] = 1   # the film never fully clears
    for n_, seed in ((REED_A, 0), (REED_B, 5)):
        for s, hgt, lean in ((4, 14, 0.9), (9, 16, -0.7), (13, 11, 0.5)):
            for k in range(hgt):
                y = T - 1 - k
                x = s + int(lean * math.sin(k * 0.35 + seed))
                if 0 <= x < T:
                    tiles[n_][y][x] = 5 if (k + seed) % 3 else 6
                    if x + 1 < T:
                        tiles[n_][y][x + 1] = 6 if (k + seed) % 3 else 5
    return tiles

WM = [[0] * 32 for _ in range(32)]
for y in range(32):
    for x in range(32):
        WM[y][x] = (x & 7) + (y & 7) * 8   #position variant, frame 0
for x, y, t_ in ((0, 29, REED_A), (1, 30, REED_B), (2, 29, REED_A),
                 (29, 29, REED_B), (30, 30, REED_A), (31, 29, REED_B),
                 (0, 1, REED_B), (1, 0, REED_A), (30, 0, REED_B),
                 (31, 1, REED_A)):
    WM[y][x] = t_

# ================= sprites: pads, props, plants, koi ======================
# Sprite tilesets are painted in *index space* against a 16-grey reference
# palette (assets/index16.png, declared as indexref in the manifest), so
# every palette slot is addressable — the base fsp palette's duplicate
# colors used to make half the slots unreachable.
IDXREF = [(8 * i, 8 * i, 8 * i) for i in range(16)]
save_pal(IDXREF, 'assets/index16.png')

PROP_PAL = [
    (0, 0, 0),       # 0 transparent
    (16, 32, 20),    # 1 outline
    (28, 64, 32),    # 2 pad dark rim
    (44, 96, 44),    # 3 pad base
    (68, 132, 60),   # 4 pad mid
    (100, 168, 84),  # 5 pad light
    (140, 204, 116), # 6 pad highlight
    (16, 16, 24),    # 7 shadow (semitransparent)
    (216, 232, 248), # 8 ring glint (semitransparent)
    (232, 120, 144), # 9 lotus pink
    (248, 192, 208), # 10 lotus light
    (56, 40, 28),    # 11 wood dark
    (96, 68, 44),    # 12 wood mid
    (140, 104, 68),  # 13 wood light
    (104, 104, 112), # 14 rock grey
    (72, 112, 56),   # 15 moss / weed green
]

def draw_pad(canvas, cx, cy, r, notch_dir=0.55, veins=True, wobble=0.0):
    W = len(canvas[0]); H = len(canvas)
    for y in range(H):
        for x in range(W):
            dx, dy = x - cx + 0.5, y - cy + 0.5
            rr = r + wobble * math.sin(math.atan2(dy, dx) * 5.0)
            d = math.hypot(dx, dy)
            if d > rr:
                continue
            ang = math.atan2(dy, dx)
            da = (ang - notch_dir + math.pi) % (2 * math.pi) - math.pi
            if abs(da) < 0.30 and d > rr * 0.22:
                continue
            if abs(da) < 0.42 and d > rr * 0.22:
                canvas[y][x] = 1
                continue
            if d > rr - 1.1:
                c = 1
            elif d > rr - 2.4:
                c = 2
            else:
                light = (-dx - dy) / (rr * 1.4)
                tt = d / rr
                if light > 0.35 and tt < 0.75: c = 5
                elif light > 0.05: c = 4
                elif light < -0.45: c = 2
                else: c = 3
                if tt < 0.18: c = 5
                if veins and d > rr * 0.3:
                    v = (ang * 4.0 / math.pi) % 1.0
                    if v < 0.10 and abs(da) > 0.6:
                        c = 2
                if light > 0.55 and tt < 0.45 and (x + y) % 3 == 0:
                    c = 6
            canvas[y][x] = c

pad_small = new_tile()
draw_pad(pad_small, 8, 8, 6.4, notch_dir=0.5, veins=False)
pad_med = new_tile()
draw_pad(pad_med, 8, 8, 7.6, notch_dir=2.2, wobble=0.4)
pad_lotus = new_tile()
draw_pad(pad_lotus, 8, 9, 6.8, notch_dir=-2.4, veins=False)
for dx, dy, c in ((0, -1, 9), (-1, 0, 9), (1, 0, 9), (0, 1, 9),
                  (-1, -1, 10), (1, -1, 10), (0, -2, 10), (0, 0, 10)):
    pad_lotus[6 + dy][8 + dx] = c
pad_tiny = new_tile()
draw_pad(pad_tiny, 8, 8, 4.6, notch_dir=1.6, veins=False)

shadow_tile = new_tile()
for y in range(T):
    for x in range(T):
        if math.hypot(x - 8 + .5, y - 8 + .5) < 7.2 and h8(x, y, 3) < 118:
            shadow_tile[y][x] = 7
fish_shadow = new_tile()
for y in range(T):
    for x in range(T):
        if math.hypot((x - 8 + .5) / 5.0, (y - 8 + .5) / 2.6) < 1.0 \
           and h8(x, y, 5) < 118:
            fish_shadow[y][x] = 7
fish_shadow_sm = new_tile()
for y in range(T):
    for x in range(T):
        if math.hypot((x - 8 + .5) / 3.4, (y - 8 + .5) / 1.9) < 1.0 \
           and h8(x, y, 15) < 104:
            fish_shadow_sm[y][x] = 7

def make_ring(r, seed):
    """A meniscus, not a decal: a broken dithered arc, brightest toward
    the light."""
    t = new_tile()
    for y in range(T):
        for x in range(T):
            dx, dy = x - 8 + .5, y - 8 + .5
            d = math.hypot(dx, dy)
            if abs(d - r) > 0.85:
                continue
            lit = (-dx - dy) / (r * 1.4)
            if h8(x, y, seed) < 90 + int(120 * (lit + 0.5)):
                t[y][x] = 8
    return t

ring_tile = make_ring(7.1, 33)
ring_tile_b = make_ring(7.35, 77)
ring_sm = make_ring(5.1, 33)
ring_sm_b = make_ring(5.35, 77)

frond_a = new_tile()
frond_b = new_tile()
for tgt, sway in ((frond_a, 1.0), (frond_b, -1.0)):
    for s, lean in ((5, 0.8), (10, -0.6)):
        for k in range(15):
            y = T - 1 - k
            x = s + int(sway * lean * 2.4 * math.sin(k * 0.4 + s))
            if 0 <= x < T:
                tgt[y][x] = 15 if k % 3 else 4
                if x + 1 < T and k > 3:
                    tgt[y][x + 1] = 4 if k % 3 else 15

# ---- taller plants: two-piece stalks that sway in tandem ----
def stalk(t, stems, sway, top):
    """stems: (x, height, lean). top=True draws the fronded upper half."""
    for s, hgt, lean in stems:
        for k in range(hgt):
            y = T - 1 - k
            bend = sway * lean * (k / 6.0)
            x = int(s + bend)
            if not (0 <= x < T):
                continue
            t[y][x] = 15 if k % 4 else 4
            if x + 1 < T:
                t[y][x + 1] = 1 if k % 4 else 15
            if top and k > hgt - 7 and k % 2 == 0:
                for d in (-2, 2):     # leaflets near the tip
                    if 0 <= x + d < T:
                        t[y][x + d] = 4
        if top:
            ty = T - hgt
            if 0 <= ty < T:
                tx = int(s + sway * lean * (hgt / 6.0))
                for d in (-1, 0, 1):
                    if 0 <= tx + d < T:
                        t[max(ty - 1, 0)][tx + d] = 5

plant_base_a = new_tile(); plant_base_b = new_tile()
plant_top_a = new_tile();  plant_top_b = new_tile()
for tb, tt, sway in ((plant_base_a, plant_top_a, 1.0),
                     (plant_base_b, plant_top_b, -1.0)):
    stalk(tb, ((4, 16, 0.5), (9, 16, -0.4), (13, 14, 0.6)), sway, False)
    stalk(tt, ((4, 15, 0.9), (9, 16, -0.8), (13, 12, 1.0)), sway, True)

# broad-leaved bottom plant, two sway frames
broad_a = new_tile(); broad_b = new_tile()
for t, sway in ((broad_a, 1.0), (broad_b, -1.0)):
    for leaf in range(5):
        ang = -math.pi / 2 + (leaf - 2) * 0.42 + sway * 0.10
        ln = 7 + (leaf % 2) * 3
        for k in range(ln):
            x = int(8 + math.cos(ang) * k + sway * 0.16 * k)
            y = int(15 + math.sin(ang) * k)
            if 0 <= x < T and 0 <= y < T:
                t[y][x] = 4 if k < ln - 2 else 5
                if 0 <= x + 1 < T and k > 1:
                    t[y][x + 1] = 15
                if 0 <= x - 1 < T and k > 2:
                    t[y][x - 1] = 1
    for x in range(6, 11):
        t[15][x] = 1

# ---- bottom dressing: rocks and a shell ----
rock_a = new_tile()
for cx, cy, r in ((6, 11, 4.2), (11, 12, 3.0)):
    for y in range(T):
        for x in range(T):
            dx, dy = x - cx + .5, y - cy + .5
            d = math.hypot(dx, dy)
            if d > r:
                continue
            lit = (-dx - dy) / (r * 1.5)
            rock_a[y][x] = 1 if d > r - 1 else (14 if lit > 0.1 else 11)
            if lit > 0.55 and h8(x, y, 9) < 120:
                rock_a[y][x] = 6
for x in range(3, 14):
    if h8(x, 15, 4) < 150:
        rock_a[15][x] = 11

rock_b = new_tile()
for cx, cy, r in ((5, 12, 3.0), (9, 10, 4.6), (13, 13, 2.4)):
    for y in range(T):
        for x in range(T):
            dx, dy = x - cx + .5, y - cy + .5
            d = math.hypot(dx, dy)
            if d > r:
                continue
            lit = (-dx - dy) / (r * 1.5)
            rock_b[y][x] = 1 if d > r - 1 else (14 if lit > 0.0 else 11)
            if lit > 0.6 and h8(x, y, 12) < 100:
                rock_b[y][x] = 6
            if d < r * 0.5 and h8(x, y, 17) < 40:
                rock_b[y][x] = 15   # moss patches

shell = new_tile()
for y in range(T):
    for x in range(T):
        dx, dy = (x - 8 + .5) / 4.4, (y - 10 + .5) / 3.2
        d = dx * dx + dy * dy
        if d <= 1.0 and dy < 0.35:
            rib = int((math.atan2(dy, dx) + math.pi) * 3.5) % 2
            shell[y][x] = 13 if rib else 12
            if d > 0.82:
                shell[y][x] = 1
for x in range(5, 12):
    shell[11][x] = 1

# ---- the jetty post: a rotting piling, leaning, with a fallen crate ----
crate = new_tile()   # collapsed crate half-buried beside the post
for y in range(6, 15):
    for x in range(2, 13):
        edge = (y == 6 or y == 14 or x == 2 or x == 12)
        crate[y][x] = 1 if edge else (12 if (x + y) % 2 else 11)
for x in range(3, 12):
    crate[10][x] = 1          # broken slat line
for y in range(7, 14):
    crate[y][7] = 1
for x in range(4, 11):        # silt piled against the lower edge
    if h8(x, 15, 21) < 190:
        crate[15][x] = 14

def post_shaft(t, x0, wide, lit_hi, lit_lo, mossy):
    for y in range(T):
        w = wide
        for k in range(-w, w + 1):
            x = x0 + k + int(y * 0.12)
            if not (0 <= x < T):
                continue
            if abs(k) == w:
                t[y][x] = 1
            elif k < -w // 2:
                t[y][x] = lit_hi        # light from the upper left
            elif k > w // 2:
                t[y][x] = lit_lo
            else:
                t[y][x] = 12
            if h8(x, y, 31) < 26:
                t[y][x] = 11            # rot pitting
            if mossy and h8(x, y, 37) < 60 and abs(k) < w:
                t[y][x] = 15            # algae in the water

post_low = new_tile()
post_shaft(post_low, 9, 3, 12, 11, True)
post_up = new_tile()
post_shaft(post_up, 7, 3, 13, 12, True)
post_top = new_tile()
post_shaft(post_top, 5, 3, 13, 12, False)
for y in range(0, 5):          # the weathered cut top face, above water
    for x in range(2, 9):
        d = abs(x - 5) + abs(y - 2)
        if d <= 3:
            post_top[y][x] = 13 if d < 2 else 12
        if d == 3:
            post_top[y][x] = 1
for x in range(3, 8):          # splintered rim
    if h8(x, 0, 41) < 140:
        post_top[0][x] = 1

post_shadow = new_tile()
for y in range(T):
    for x in range(T):
        if math.hypot((x - 8 + .5) / 5.4, (y - 9 + .5) / 3.0) < 1.0 \
           and h8(x, y, 25) < 132:
            post_shadow[y][x] = 7

# ================= pond half-sprites (hsp 128-133) ========================
hsp_pal0 = list(Image.open('assets/hsp_pal0.png').convert('RGB').getdata())[:16]
pt = [[[0] * 8 for _ in range(8)] for _ in range(6)]
for n, (lo, hi) in enumerate(((2, 6), (6, 11), (10, 14))):
    for y in range(8):
        for x in range(8):
            d2 = (2 * x - 7) ** 2 + (2 * y - 7) ** 2
            if lo * 4 <= d2 <= hi * 4 and (x + y + n) % 4:
                pt[n][y][x] = 1
for x, y in ((3, 3), (4, 3), (3, 4), (4, 4)):
    pt[3][y][x] = 2
for x, y in ((3, 1), (3, 5), (1, 3), (5, 3), (3, 3)):
    pt[4][y][x] = 1
for x, y in ((2, 2), (4, 4), (2, 4), (4, 2), (3, 3)):
    pt[5][y][x] = 1
pimg = Image.new('RGB', (48, 8), hsp_pal0[0])
for n in range(6):
    for y in range(8):
        for x in range(8):
            pimg.putpixel((n * 8 + x, y), hsp_pal0[pt[n][y][x]])
pimg.save('assets/pond_hsp_tiles.png')

# ================= koi (fsp tiles 18-21, palettes 5-6) ====================
def draw_koi(tail_up):
    t = new_tile()
    for y in range(T):
        for x in range(T):
            dx, dy = (x - 6.5) / 5.5, (y - 8) / 3.5
            if dx * dx + dy * dy <= 1.0:
                t[y][x] = 2 if dx * dx + dy * dy > 0.55 else 3
    for x, y in ((5, 7), (6, 7), (5, 8), (8, 9), (9, 8)):
        t[y][x] = 5
    t[7][10] = 1
    ty = 6 if tail_up else 10
    for k in range(4):
        x = 3 - k
        for y in range(8 - k, 9 + k):
            yy = y + (ty - 8) * k // 3
            if 0 <= yy < T:
                t[yy][x] = 4
    t[4][7] = 4
    t[12][7] = 4
    t[8][12] = 1
    return t

def draw_koi_small(tail_up):
    """Small fry: the same fish at roughly two thirds scale."""
    t = new_tile()
    for y in range(T):
        for x in range(T):
            dx, dy = (x - 7.5) / 3.7, (y - 8) / 2.3
            if dx * dx + dy * dy <= 1.0:
                t[y][x] = 2 if dx * dx + dy * dy > 0.5 else 3
    for x, y in ((6, 7), (7, 8), (9, 8)):
        t[y][x] = 5
    t[7][10] = 1
    ty = 7 if tail_up else 9
    for k in range(3):
        x = 5 - k
        for yy in range(8 - (k + 1) // 2, 9 + (k + 1) // 2):
            y2 = yy + (ty - 8) * k // 2
            if 0 <= y2 < T:
                t[y2][x] = 4
    t[8][11] = 1
    return t

koi_frames = [draw_koi(True), draw_koi(False),
              draw_koi_small(True), draw_koi_small(False)]
kimg = Image.new('RGB', (16 * len(koi_frames), 16), IDXREF[0])
for n, t in enumerate(koi_frames):
    for y in range(T):
        for x in range(T):
            kimg.putpixel((n * 16 + x, y), IDXREF[t[y][x]])
kimg.save('assets/koi_tiles.png')

KOI_ORANGE = [(0, 0, 0)] * 16
KOI_ORANGE[1] = (16, 12, 16)
KOI_ORANGE[2] = (240, 88, 24)
KOI_ORANGE[3] = (248, 176, 80)
KOI_ORANGE[4] = (248, 136, 48)
KOI_ORANGE[5] = (248, 248, 240)
KOI_CALICO = [(0, 0, 0)] * 16
KOI_CALICO[1] = (16, 12, 16)
KOI_CALICO[2] = (248, 248, 248)
KOI_CALICO[3] = (240, 216, 184)
KOI_CALICO[4] = (248, 192, 160)
KOI_CALICO[5] = (224, 56, 40)
for name, pal in (('koi_pal0', KOI_ORANGE), ('koi_pal1', KOI_CALICO)):
    save_pal(pal, f'assets/{name}.png')

# ============== the sprite sheet and its palette ==========================
PAD_TILES = [pad_small, pad_med, pad_lotus, pad_tiny, shadow_tile,
             fish_shadow, fish_shadow_sm, ring_tile, ring_tile_b,
             frond_a, frond_b, ring_sm, ring_sm_b,
             plant_base_a, plant_base_b, plant_top_a, plant_top_b,
             broad_a, broad_b, rock_a, rock_b, shell,
             crate, post_low, post_up, post_top, post_shadow]
pimg2 = Image.new('RGB', (len(PAD_TILES) * 16, 16), IDXREF[0])
for n, t in enumerate(PAD_TILES):
    for y in range(T):
        for x in range(T):
            pimg2.putpixel((n * 16 + x, y), IDXREF[t[y][x]])
pimg2.save('assets/pad_fsp_tiles.png')
save_pal(PROP_PAL, 'assets/leaf_pal.png')

#keep the fsp manifest in sync (idempotent)
mf = [l for l in open('assets/fsp.mfst').read().splitlines()
      if not any(k in l for k in ('pad_fsp_tiles', 'leaf_pal', 'indexref',
                                  'koi_tiles', 'koi_pal'))]
mf.insert(1, 'indexref assets/index16.png')
mf.append('tileset assets/koi_tiles.png')
mf.append('palette assets/koi_pal0.png')
mf.append('palette assets/koi_pal1.png')
mf.append('tileset assets/pad_fsp_tiles.png')
mf.append('palette assets/leaf_pal.png alpha=7,8')
open('assets/fsp.mfst', 'w').write('\n'.join(mf) + '\n')
print(f'sprite sheet: {len(PAD_TILES)} prop tiles, full 16-slot palette')

# ================= the pond floor: one painted 512x512 scene ============
# Tile-based floors repeat; a painted scene does not. This paints the
# whole bed — depth gradient, contour terraces, dune ripples, lit
# patches, pebbles, weed roots — and gfxtool import-scene slices and
# deduplicates it into a tileset + tilemap.
FS = 512
FPAL = [
    (16, 28, 36),   # 0 deepest shadow (backdrop)
    (28, 44, 52),   # 1 deep floor shadow / contour lines
    (40, 60, 68),   # 2 deep floor
    (56, 76, 76),   # 3 deep-mid
    (76, 92, 84),   # 4 mid floor
    (96, 108, 92),  # 5 mid floor light
    (120, 124, 96), # 6 sunlit A } rotated 6..9 at runtime: the floor
    (148, 148, 112),# 7 sunlit B } shimmers where the light pools
    (180, 176, 132),# 8 sunlit C }
    (216, 208, 160),# 9 sunlit D }
    (112, 108, 84), # 10 shallow sand dark
    (140, 132, 100),# 11 shallow sand
    (172, 160, 124),# 12 shallow sand light
    (208, 192, 148),# 13 sunlit shallows
    (36, 56, 40),   # 14 weed root / debris dark
    (72, 96, 56),   # 15 weed light
]
RAMP = [1, 2, 3, 4, 5, 10, 11, 12, 13]  # deep -> shallow

A2P = 2 * math.pi / FS

def fdepth(x, y):
    """1 = deep, 0 = shallow. Periodic in both axes so the river wraps."""
    d = 0.52
    d += 0.30 * math.sin(A2P * x + 0.4)
    d += 0.14 * math.sin(A2P * 3 * y + 1.1)
    d += 0.10 * math.sin(A2P * 2 * (x + y) + 2.0)
    d += 0.07 * math.sin(A2P * 5 * (x - 2 * y) - 0.7)
    d += 0.05 * math.sin(A2P * 7 * (2 * x + y) + 1.7)
    return 0.0 if d < 0.0 else (1.0 if d > 1.0 else d)

fimg = [[0] * FS for _ in range(FS)]
for y in range(FS):
    for x in range(FS):
        d = fdepth(x, y)
        t = (1.0 - d) * (len(RAMP) - 1)
        i0 = int(t)
        if i0 > len(RAMP) - 2: i0 = len(RAMP) - 2
        frac = t - i0
        # dithered transition between ramp steps: no banding
        idx = i0 + (1 if frac * 255 > h8(x, y, 3) else 0)
        g = h8(x, y, 11)
        if g < 16 and idx > 0: idx -= 1
        elif g > 242 and idx + 1 < len(RAMP): idx += 1
        c = RAMP[idx]
        # dune ripples riding the depth contours
        r = math.sin(d * 46.0 + A2P * 2 * y)
        if r > 0.86 and h8(x, y, 5) < 200:
            c = RAMP[min(idx + 1, len(RAMP) - 1)]
        elif r < -0.90 and h8(x, y, 7) < 170:
            c = RAMP[max(idx - 1, 0)]
        # depth terraces: a dark contour where the bed steps down
        for lvl in (0.34, 0.58, 0.78):
            if abs(d - lvl) < 0.004 and h8(x, y, 13) < 210:
                c = 1
        # patches the sunlight actually reaches (runtime-rotated colors)
        lit = (math.sin(A2P * 4 * x + 1.0) * math.sin(A2P * 3 * y - 2.0)
               + 0.6 * math.sin(A2P * 6 * (x + y)))
        if lit > 0.78 and d < 0.62 and h8(x, y, 21) < 150:
            fimg[y][x] = 6 + (h8(x, y, 23) & 3)
            continue
        fimg[y][x] = c

def fblob(cx, cy, r, painter):
    for y in range(int(cy - r) - 1, int(cy + r) + 2):
        for x in range(int(cx - r) - 1, int(cx + r) + 2):
            dx, dy = x - cx, y - cy
            d = math.hypot(dx, dy)
            if d <= r:
                painter(x % FS, y % FS, d / r, dx, dy)

# pebble fields, sunk into the deeper water
for k in range(26):
    px = (k * 97 + 31) % FS
    py = (k * 173 + 61) % FS
    if fdepth(px, py) < 0.45:
        continue
    for j in range(5 + (k % 4)):
        cx = px + ((h8(k, j, 1) % 40) - 20)
        cy = py + ((h8(k, j, 2) % 40) - 20)
        rr = 1.6 + (h8(k, j, 3) % 20) / 10.0
        def peb(x, y, t, dx, dy, rr=rr):
            fimg[y][x] = 1 if t > 0.72 else (2 if (dx + dy) > -rr * 0.4 else 3)
        fblob(cx, cy, rr, peb)

# weed roots and debris in the shallows
for k in range(30):
    px = (k * 211 + 17) % FS
    py = (k * 139 + 83) % FS
    if fdepth(px, py) > 0.5:
        continue
    for s in range(3 + (k % 3)):
        sx = px + (h8(k, s, 4) % 14) - 7
        for step in range(6 + (h8(k, s, 5) % 9)):
            yy = (py - step) % FS
            xx = (sx + int(1.8 * math.sin(step * 0.5 + s))) % FS
            fimg[yy][xx] = 14 if step % 3 else 15
            if step > 2 and (xx + 1) % FS != 0:
                fimg[yy][(xx + 1) % FS] = 15 if step % 3 else 14

# sunken twigs, a few, lying flat
for k in range(9):
    px = (k * 317 + 41) % FS
    py = (k * 251 + 127) % FS
    ln = 14 + (k % 12)
    ang = (h8(k, 0, 9) % 256) / 256.0 * math.pi
    for step in range(ln):
        xx = int(px + step * math.cos(ang)) % FS
        yy = int(py + step * math.sin(ang)) % FS
        fimg[yy][xx] = 14
        if step % 4 == 0:
            fimg[(yy + 1) % FS][xx] = 1

fim = Image.new('RGB', (FS, FS))
for y in range(FS):
    for x in range(FS):
        fim.putpixel((x, y), FPAL[fimg[y][x]])
fim.save('assets/scene3_floor.png')
save_pal(FPAL, 'assets/scene3_floor_pal.png')
print('floor scene painted: 512x512')

# ================= write the background sets ==============================
save_strip(dt, DPAL, 'assets/scene3_depths_tiles.png')
save_pal(DPAL, 'assets/scene3_depths_pal0.png')
save_map(D1, 'assets/scene3_depth1.map')
save_map(D2, 'assets/scene3_depth2.map')
for st_ in range(4):
    save_strip(caustic_tiles(st_), CPAL,
               f'assets/scene3_caustics{st_}_tiles.png')
    open(f'assets/scene3_caustics{st_}.mfst', 'w').write(
        f'# caustic style {st_} (tools/gen_scene3.py)\ntype background\n'
        f'tileset assets/scene3_caustics{st_}_tiles.png\n'
        f'palette assets/scene3_caustics_pal0.png alpha=1,2,3,4,5,6,7\n')
save_pal(CPAL, 'assets/scene3_caustics_pal0.png')
save_map(CM, 'assets/scene3_caustics.map')
for st_ in range(3):
    save_strip(surface_tiles(st_), WPAL,
               f'assets/scene3_surface{st_}_tiles.png')
    open(f'assets/scene3_surface{st_}.mfst', 'w').write(
        f'# surface style {st_} (tools/gen_scene3.py)\ntype background\n'
        f'tileset assets/scene3_surface{st_}_tiles.png\n'
        f'palette assets/scene3_surface_pal0.png alpha=1,2,3\n')
save_pal(WPAL, 'assets/scene3_surface_pal0.png')
save_map(WM, 'assets/scene3_surface.map')
print(f'styles: 4 caustic banks, 3 surface banks')
