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
D1 = [[21] * 32 for _ in range(32)]  #the upper water column
for cx, cy, r in ((6, 8, 3.2), (20, 5, 2.8), (27, 16, 3.4), (10, 22, 3.0),
                  (22, 27, 3.2)):
    veil_blob(D1, cx, cy, r, dense=False)

D2 = [[22] * 32 for _ in range(32)]  #the lower water column
for cx, cy, r in ((13, 7, 4.2), (27, 9, 3.2), (5, 14, 3.6), (18, 18, 4.6),
                  (28, 23, 3.4), (9, 28, 4.0), (23, 30, 2.8), (1, 4, 2.7)):
    veil_blob(D2, cx, cy, r, dense=True)


# ================= caustics layer: cellular webs + godrays =================
CPAL = [
    (0, 0, 0),      # 0 transparent
    (96, 92, 72),   # 1 web dim      } warm light, swept 1..3 —
    (136, 128, 96), # 2 web bright   } deliberately mid-value: the
    (176, 168, 120),# 3 web glint    } additive blend doubles them
    (176, 164, 120),# 4 pool dim     } swept 4..6
    (208, 196, 144),# 5 pool mid     }
    (232, 224, 168),# 6 pool bright  }
    (248, 248, 240),# 7 sparkle
] + [(0, 0, 0)] * 8

#tiles 0..511: 8 frames x 64 position variants covering a 128x128 px
#torus. Ridges sit where the two nearest feature points are equidistant
#(the cell walls of a real caustic); each point rides a small closed
#orbit, so consecutive frames differ gently and frame 7 loops to 0.
NFRAME = 24  # x3: the light breathes instead of stepping
NPOS = 16    # 4x4 tiles per period (64px, the tile budget's price)
PER = 64
N_C = NFRAME * NPOS + 3
ct = [new_tile() for _ in range(N_C)]

def torus_d(ax, ay, bx, by):
    dx = abs(ax - bx); dy = abs(ay - by)
    if dx > PER/2: dx = PER - dx
    if dy > PER/2: dy = PER - dy
    return math.hypot(dx, dy)

BASE_PTS = [((37 * k * k + 23 * k) % PER, (53 * k * k + 41 * k + 17) % PER)
            for k in range(7)]
for f in range(NFRAME):
    ph = 2 * math.pi * f / NFRAME
    pts = [((bx + 5.0 * math.cos(ph + k)) % PER,
            (by + 5.0 * math.sin(ph * (1 if k % 2 else -1) + k * 2)) % PER)
           for k, (bx, by) in enumerate(BASE_PTS)]
    for v in range(NPOS):
        cx, cy = v % 4, v // 4
        t = ct[f * NPOS + v]
        for y in range(T):
            for x in range(T):
                wx, wy = cx * 16 + x, cy * 16 + y
                ds = sorted(torus_d(wx, wy, px, py) for px, py in pts)
                gap = ds[1] - ds[0]
                if gap < 0.55:
                    t[y][x] = 3 if gap < 0.20 else 2
                elif gap < 0.90 and h8(wx, wy, f) < 46:
                    t[y][x] = 1

# light pools (after the animation bank) and a sparkle tile
POOL_A = NFRAME * NPOS
POOL_B = POOL_A + 1
SPARK_C = POOL_A + 2
for n, (rr, dens) in ((POOL_A, (7.2, 150)), (POOL_B, (6.0, 90))):
    for y in range(T):
        for x in range(T):
            d = math.hypot(x - 8 + .5, y - 8 + .5)
            if d > rr:
                continue
            fall = 1.0 - d / rr
            if h8(x, y, n * 5) < dens * fall:
                ct[n][y][x] = 6 if fall > 0.6 else 5 if fall > 0.3 else 4

ct[SPARK_C][6][9] = 7   # sparkle specks
ct[SPARK_C][10][4] = 7
ct[SPARK_C][2][13] = 7

#caustics blanket the whole floor — frames hash-staggered so the net
#shimmers without a visible grid; a few pools and sparkles on top
CM = [[0] * 32 for _ in range(32)]
for y in range(32):
    for x in range(32):
        CM[y][x] = (x & 3) + (y & 3) * 4   #position variant, frame 0
#the light layer is light and nothing else: no pools, no specks

# ================= surface: Wind Waker water =============================
#Full-coverage animated surface: a translucent base tint (organic dither,
#~55%% coverage), wandering cel-style contour lines, and flash regions
#that light up as the palette rotates 2->3->4 at runtime. Frames 0-3
#cycle through the tilemap; reeds live on tiles 4-5.
WPAL = [
    (0, 0, 0),      # 0 transparent
    (112, 168, 152),# 1 base water tint (semitransparent)
    (176, 216, 200),# 2 contour, one soft color (semitransparent)
    (232, 240, 216),# 3 crest glint — pulsed at runtime (semitransparent)
    (0, 0, 0),      # 4 unused
    (120, 144, 56), # 5 reed olive
    (84, 104, 40),  # 6 reed dark
] + [(0, 0, 0)] * 9

#tiles 0..511: 8 frames x 64 positions over a 128px field. The tint is
#smooth blobs (dither only at their edges — no checkerboard wash), the
#contours drift on looping phase shifts.
SFRAME = 8   # the surface keeps its own geometry: 8 frames x 64
SPOS = 64    # positions over a 128px period (512-tile bank)
SPER = 128
N_W = SFRAME * SPOS + 2
REED_A = SFRAME * SPOS
REED_B = REED_A + 1
wt = [new_tile() for _ in range(N_W)]
W128 = 2 * math.pi / SPER
for f in range(SFRAME):
    fp = 2 * math.pi * f / SFRAME
    for v in range(SPOS):
        vx, vy = v % 8, v // 8
        t = wt[f * SPOS + v]
        for y in range(T):
            for x in range(T):
                wx, wy = vx * 16 + x, vy * 16 + y
                c = (math.sin(wx * W128 * 2 + fp)
                     + math.sin(wy * W128 * 3 - fp)
                     + 0.6 * math.sin((wx + wy) * W128 * 5 + 2 * fp)
                     + 0.45 * math.sin((wx - 2 * wy) * W128 * 7 - fp))
                #smooth tint blobs, static across frames; dithered rim
                g = (math.sin(wx * W128 + 1.1) + math.sin(wy * W128 * 2 + 0.4)
                     + math.sin((wx + wy) * W128 * 3 - 0.8))
                if abs(c) < 0.09:
                    #one soft color; the rare crest pixels go bright
                    t[y][x] = 3 if (abs(c) < 0.03
                                    and h8(wx, wy, 9) < 90) else 2
                else:
                    t[y][x] = 1  #the film covers every pixel
for n, seed in ((REED_A, 0), (REED_B, 5)):  # reed clusters
    for s, hgt, lean in ((4, 14, 0.9), (9, 16, -0.7), (13, 11, 0.5)):
        for k in range(hgt):
            y = T - 1 - k
            x = s + int(lean * math.sin(k * 0.35 + seed))
            if 0 <= x < T:
                wt[n][y][x] = 5 if (k + seed) % 3 else 6
                if x + 1 < T:
                    wt[n][y][x + 1] = 6 if (k + seed) % 3 else 5

WM = [[0] * 32 for _ in range(32)]
for y in range(32):
    for x in range(32):
        WM[y][x] = (x & 7) + (y & 7) * 8   #position variant, frame 0
#the water layer is water and nothing else: reeds retired from the map

#SPAL remains the sprite color table for pads/rings/fronds (not a bg pal)
SPAL = [
    (0, 0, 0),      # 0 transparent
    (16, 40, 20),   # 1 pad outline
    (28, 64, 32),   # 2 pad dark rim
    (44, 96, 44),   # 3 pad base
    (68, 132, 60),  # 4 pad mid
    (100, 168, 84), # 5 pad light
    (140, 204, 116),# 6 pad highlight
    (24, 56, 28),   # 7 pad vein
    (216, 232, 248),# 8 wave glint (semitransparent)
    (16, 16, 24),   # 9 pad shadow (semitransparent, near-black)
    (232, 120, 144),# 10 lotus pink
    (248, 192, 208),# 11 lotus light
    (176, 48, 72),  # 12 lotus deep
    (248, 232, 160),# 13 lotus center
    (120, 144, 56), # 14 reed olive
    (84, 104, 40),  # 15 reed dark
]

# ================= lily pads as sprites (unchanged interface) =============
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
                        c = 7
                if light > 0.55 and tt < 0.45 and (x + y) % 3 == 0:
                    c = 6
            canvas[y][x] = c

pad_small = new_tile()
draw_pad(pad_small, 8, 8, 6.4, notch_dir=0.5, veins=False)
pad_med = new_tile()
draw_pad(pad_med, 8, 8, 7.6, notch_dir=2.2, wobble=0.4)
pad_lotus = new_tile()
draw_pad(pad_lotus, 8, 9, 6.8, notch_dir=-2.4, veins=False)
for dx, dy, c in ((0, -1, 12), (-1, 0, 10), (1, 0, 10), (0, 1, 10),
                  (-1, -1, 11), (1, -1, 11), (0, -2, 11), (0, 0, 13)):
    pad_lotus[6 + dy][8 + dx] = c
pad_tiny = new_tile()
draw_pad(pad_tiny, 8, 8, 4.6, notch_dir=1.6, veins=False)
shadow_tile = new_tile()
for y in range(T):
    for x in range(T):
        if math.hypot(x - 8 + .5, y - 8 + .5) < 7.2 and h8(x, y, 3) < 118:
            shadow_tile[y][x] = 9
fish_shadow = new_tile()
for y in range(T):
    for x in range(T):
        if math.hypot((x - 8 + .5) / 5.0, (y - 8 + .5) / 2.6) < 1.0 \
           and h8(x, y, 5) < 118:
            fish_shadow[y][x] = 9
def make_ring(r, seed):
    t = new_tile()
    for y in range(T):
        for x in range(T):
            d = math.hypot(x - 8 + .5, y - 8 + .5)
            if abs(d - r) < 0.55 and h8(x, y, seed) < 205:
                t[y][x] = 8
    return t

ring_tile = make_ring(7.1, 33)   # frame A
ring_tile_b = make_ring(7.35, 77)# frame B: breathes outward
ring_sm = make_ring(5.1, 33)
ring_sm_b = make_ring(5.35, 77)
frond_a = new_tile()    # upper weed fronds, two sway frames
frond_b = new_tile()
for tgt, sway in ((frond_a, 1.0), (frond_b, -1.0)):
    for s, lean in ((5, 0.8), (10, -0.6)):
        for k in range(15):
            y = T - 1 - k
            x = s + int(sway * lean * 2.4 * math.sin(k * 0.4 + s))
            if 0 <= x < T:
                tgt[y][x] = 3 if k % 3 else 5
                if x + 1 < T and k > 3:
                    tgt[y][x + 1] = 5 if k % 3 else 3

# ================= koi (fsp tiles 18-19, palettes 5-6) ====================
fsp_pal0 = list(Image.open('assets/fsp_pal0.png').convert('RGB').getdata())[:16]
used = [1, 2, 3, 4, 5]
assert len(set(fsp_pal0[i] for i in used)) == len(used), \
    'fsp palette 0 has duplicate colors at koi indices'


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

koi_frames = [draw_koi(True), draw_koi(False)]
kimg = Image.new('RGB', (32, 16), fsp_pal0[0])
for n, t in enumerate(koi_frames):
    for y in range(T):
        for x in range(T):
            kimg.putpixel((n * 16 + x, y), fsp_pal0[t[y][x]])
kimg.save('assets/koi_tiles.png')

KOI_ORANGE = list(fsp_pal0)
KOI_ORANGE[1] = (16, 12, 16)
KOI_ORANGE[2] = (240, 88, 24)
KOI_ORANGE[3] = (248, 176, 80)
KOI_ORANGE[4] = (248, 136, 48)
KOI_ORANGE[5] = (248, 248, 240)
KOI_CALICO = list(fsp_pal0)
KOI_CALICO[1] = (16, 12, 16)
KOI_CALICO[2] = (248, 248, 248)
KOI_CALICO[3] = (240, 216, 184)
KOI_CALICO[4] = (248, 192, 160)
KOI_CALICO[5] = (224, 56, 40)
for name, pal in (('koi_pal0', KOI_ORANGE), ('koi_pal1', KOI_CALICO)):
    im = Image.new('RGB', (16, 1))
    im.putdata([tuple(c) for c in pal])
    im.save(f'assets/{name}.png')

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

# ============== lily pads onto the fsp palette (index remap) ==============
PAD_TILES = [pad_small, pad_med, pad_lotus, pad_tiny, shadow_tile,
             fish_shadow, ring_tile, ring_tile_b, frond_a, frond_b,
             ring_sm, ring_sm_b]
need = sorted({v for t in PAD_TILES for row in t for v in row})
first = []
seen = set()
for i, c in enumerate(fsp_pal0):
    if c not in seen:
        seen.add(c)
        if i != 0:
            first.append(i)
merges = [(7, 1), (12, 10), (11, 10), (6, 5), (13, 11), (2, 1), (4, 3)]
work = [t for t in PAD_TILES]
needed = [v for v in need if v != 0]
while len(needed) > len(first):
    a, b = merges.pop(0)
    work = [[[b if v == a else v for v in row] for row in t] for t in work]
    needed = sorted({v for t in work for row in t for v in row if v != 0})
mapping = {0: 0}
for v, slot in zip(needed, first):
    mapping[v] = slot
leaf_pal = [(0, 0, 0)] * 16
for v, slot in mapping.items():
    leaf_pal[slot] = SPAL[v]
shadow_slot = mapping[9]
ring_slot = mapping.get(8, shadow_slot)
pimg2 = Image.new('RGB', (len(work) * 16, 16), fsp_pal0[0])
for n, t in enumerate(work):
    for y in range(T):
        for x in range(T):
            pimg2.putpixel((n * 16 + x, y), fsp_pal0[mapping[t[y][x]]])
pimg2.save('assets/pad_fsp_tiles.png')
lp = Image.new('RGB', (16, 1))
lp.putdata(leaf_pal)
lp.save('assets/leaf_pal.png')

mf = [l for l in open('assets/fsp.mfst').read().splitlines()
      if 'pad_fsp_tiles' not in l and 'leaf_pal' not in l]
mf.append('tileset assets/pad_fsp_tiles.png')
mf.append('palette assets/leaf_pal.png '
          f'alpha={shadow_slot},{ring_slot}')
open('assets/fsp.mfst', 'w').write('\n'.join(mf) + '\n')
print(f'pad sprites: shadow color slot {shadow_slot}')

# ================= write the background sets ==============================
save_strip(dt, DPAL, 'assets/scene3_depths_tiles.png')
save_pal(DPAL, 'assets/scene3_depths_pal0.png')
save_map(FM, 'assets/scene3_floor.map')
save_map(D1, 'assets/scene3_depth1.map')
save_map(D2, 'assets/scene3_depth2.map')
save_strip(ct, CPAL, 'assets/scene3_caustics_tiles.png')
save_pal(CPAL, 'assets/scene3_caustics_pal0.png')
save_map(CM, 'assets/scene3_caustics.map')
save_strip(wt, WPAL, 'assets/scene3_surface_tiles.png')
save_pal(WPAL, 'assets/scene3_surface_pal0.png')
save_map(WM, 'assets/scene3_surface.map')
print(f'scene3: floor/veils {N_D} tiles, caustics {N_C}, surface {N_W},'
      ' 2 koi frames, 6 pond half-sprites')
