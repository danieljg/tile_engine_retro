#!/usr/bin/env python3
"""Generates the Koi Pond (scene 3) assets — a top-down pond, unrelated to
the space scenes:
  assets/scene3_depths_tiles.png / _pal0.png / assets/scene3_depths.map
      bg1: the water itself — calm base, sparse caustic webs (palette
      colors 6-9 rotate at runtime), light shafts and floor detail.
      This is the layer the per-scanline sine warp bends.
  assets/scene3_surface_tiles.png / _pal0.png / assets/scene3_surface.map
      bg0: lily pads (small, medium, wide 2x1 and big 2x2), lotus
      flowers, reeds, semitransparent sparkles and pad shadows. This
      layer does NOT warp — the pads sit still while light moves below.
  assets/koi_tiles.png (fsp tiles 18-19) + koi palettes (fsp 5-6)
  assets/pond_hsp_tiles.png (hsp tiles 128-131): ripple rings + pellet
All RGB values are multiples of 8 to survive 5-bit quantization.
"""
from PIL import Image
import struct
import math

# ====================== depths (bg1) ======================
DPAL = [
    (8, 16, 40),    # 0 deep water (backdrop)
    (12, 24, 52),   # 1 water dither dark
    (16, 32, 64),   # 2 water dither light
    (28, 52, 88),   # 3 light shaft dim
    (40, 72, 108),  # 4 light shaft mid
    (56, 96, 128),  # 5 light shaft bright
    (32, 88, 104),  # 6 caustic dim   } rotated 6..9
    (48, 120, 128), # 7 caustic mid   } at runtime
    (72, 160, 152), # 8 caustic bright}
    (104, 200, 184),# 9 caustic glint }
    (24, 48, 40),   # 10 weed dark
    (36, 76, 52),   # 11 weed light
    (56, 44, 36),   # 12 mud
    (80, 64, 48),   # 13 mud light
    (20, 28, 48),   # 14 water shadow
    (16, 24, 32),   # 15 pebble dark
]

N_D = 10
dt = [[[0]*16 for _ in range(16)] for _ in range(N_D)]

def calm(t, seed):
    """Soft water: mostly backdrop with a quiet diagonal dither."""
    for y in range(16):
        for x in range(16):
            v = (x*7 + y*13 + seed) % 23
            t[y][x] = 1 if v == 0 else (2 if v == 11 else 0)

calm(dt[0], 0)  # t0 open water

for n, seed in ((1, 4), (2, 9)):  # t1,t2 caustic webs: thin bright arcs
    calm(dt[n], seed)
    for y in range(16):
        for x in range(16):
            a = (x + y*2 + seed*3) % 13
            b = (x*2 - y + seed*5) % 11
            if a == 0 and (x + seed) % 3:
                dt[n][y][x] = 6 + ((x + y) % 2)
            elif b == 0 and (y + seed) % 4 == 1:
                dt[n][y][x] = 7 + ((x + y) % 3)

def shaft(t, cols, seed):
    """A band of sunlight: vertical brightening with ragged edges."""
    calm(t, seed)
    for y in range(16):
        for x in range(16):
            for c0, w in cols:
                d = x - c0
                if 0 <= d < w:
                    edge = (d == 0 or d == w-1)
                    ragged = (y*3 + x*5 + seed) % 4
                    if edge and ragged == 0:
                        continue
                    lvl = 4 if not edge else 3
                    if (x*11 + y*7 + seed) % 19 == 0:
                        lvl = 5
                    t[y][x] = lvl

shaft(dt[3], [(4, 7)], 2)   # t3 wide shaft
shaft(dt[4], [(1, 4), (11, 3)], 6)  # t4 two narrow shafts

calm(dt[5], 5)  # t5 weeds
for s, h in ((3, 12), (8, 15), (12, 10)):
    for k in range(h):
        y = 15 - k
        x = s + int(1.5 + 1.2*math.sin(k*0.7 + s))
        if 0 <= x < 16:
            dt[5][y][x] = 10 if k % 3 else 11

calm(dt[6], 7)  # t6 sandy floor patch
for y in range(16):
    for x in range(16):
        v = (x*5 + y*11) % 13
        if v < 5: dt[6][y][x] = 12
        elif v == 6: dt[6][y][x] = 13

calm(dt[7], 3)  # t7 pebbles
for cx, cy, r in ((4, 5, 2), (11, 4, 2), (7, 11, 3), (13, 13, 2)):
    for y in range(16):
        for x in range(16):
            d2 = (x-cx)**2 + (y-cy)**2
            if d2 <= r*r:
                dt[7][y][x] = 15 if d2 > (r-1)*(r-1) else 13
calm(dt[8], 11)  # t8 caustic sparse
for y in range(16):
    for x in range(16):
        if (x*3 + y*5) % 17 == 0:
            dt[8][y][x] = 6
calm(dt[9], 13)  # t9 gentle shadow (under pad clusters)
for y in range(16):
    for x in range(16):
        if (x + y) % 2 and ((x*5 + y*3) % 7) < 3:
            dt[9][y][x] = 14

DM = [[0]*32 for _ in range(32)]
for y in range(32):
    for x in range(32):
        v = (x*11 + y*7) % 23
        if v == 0:   DM[y][x] = 1
        elif v == 5: DM[y][x] = 2
        elif v == 9: DM[y][x] = 8
        else:        DM[y][x] = 0
#sand banks and pebbles
for cx, cy, r in ((6, 26, 6), (24, 5, 5), (28, 27, 6), (3, 4, 4)):
    for y in range(32):
        for x in range(32):
            if (x-cx)**2 + (y-cy)**2 <= r*r:
                DM[y][x] = 6 if (x*3 + y*5) % 7 else 7
#weed stands
for x, y in ((2, 12), (10, 3), (17, 28), (29, 14), (14, 14), (21, 20)):
    DM[y][x] = 5
#caustic-lit patches on the floor
for x, y in ((8, 8), (19, 6), (5, 19), (26, 20), (13, 24), (30, 8),
             (16, 11), (23, 12), (9, 30), (1, 26)):
    DM[y][x] = 1 + ((x + y) % 2)

#light-web map (bg1 middle overlay): godrays + caustic webs over the floor,
#disabled (0x8000) where there is no light feature
OFF = 0x8000
LM = [[OFF]*32 for _ in range(32)]
for x0, w in ((4, 3), (14, 4), (24, 3)):   #godray bands
    for y in range(32):
        for k in range(w):
            LM[y][x0+k] = 3 if (x0 + k + y) % 3 else 4
for x, y in ((8, 4), (10, 9), (19, 14), (7, 17), (28, 6), (12, 21),
             (21, 25), (2, 8), (30, 22), (17, 2), (5, 27), (27, 16)):
    LM[y][x] = 1 + ((x + y) % 2)           #caustic webs
for y in range(32):                        #drifting plankton speckle
    for x in range(32):
        if LM[y][x] == OFF and (x*13 + y*17) % 29 == 0:
            LM[y][x] = 0

# ====================== surface (bg0) ======================
SPAL = [
    (0, 0, 0),      # 0 transparent
    (16, 40, 20),   # 1 pad outline
    (28, 64, 32),   # 2 pad dark rim
    (44, 96, 44),   # 3 pad base
    (68, 132, 60),  # 4 pad mid
    (100, 168, 84), # 5 pad light
    (140, 204, 116),# 6 pad highlight
    (24, 56, 28),   # 7 pad vein
    (216, 232, 248),# 8 sparkle (semitransparent)
    (40, 64, 88),   # 9 pad shadow (semitransparent)
    (232, 120, 144),# 10 lotus pink
    (248, 192, 208),# 11 lotus light
    (176, 48, 72),  # 12 lotus deep
    (248, 232, 160),# 13 lotus center
    (120, 144, 56), # 14 reed olive
    (84, 104, 40),  # 15 reed dark
]

def draw_pad(canvas, cx, cy, r, notch_dir=0.55, veins=True, wobble=0.0):
    """A shaded lily pad with a notch and radial veins, light upper-left."""
    W = len(canvas[0]); H = len(canvas)
    for y in range(H):
        for x in range(W):
            dx, dy = x - cx + 0.5, y - cy + 0.5
            rr = r + wobble*math.sin(math.atan2(dy, dx)*5.0)
            d = math.hypot(dx, dy)
            if d > rr:
                continue
            ang = math.atan2(dy, dx)
            #the notch: a wedge cut toward notch_dir
            da = (ang - notch_dir + math.pi) % (2*math.pi) - math.pi
            if abs(da) < 0.30 and d > rr*0.22:
                continue
            if abs(da) < 0.42 and d > rr*0.22:
                canvas[y][x] = 1  #notch edge
                continue
            if d > rr - 1.1:
                c = 1
            elif d > rr - 2.4:
                c = 2
            else:
                #base shading: light from the upper left
                light = (-dx - dy) / (rr*1.4)
                t = d / rr
                if light > 0.35 and t < 0.75: c = 5
                elif light > 0.05: c = 4
                elif light < -0.45: c = 2
                else: c = 3
                if t < 0.18: c = 5
                #radial veins
                if veins and d > rr*0.3:
                    v = (ang*4.0/math.pi) % 1.0
                    if v < 0.10 and abs(da) > 0.6:
                        c = 7
                #dew highlight
                if light > 0.55 and t < 0.45 and (x + y) % 3 == 0:
                    c = 6
            canvas[y][x] = c

def new_canvas(w, h):
    return [[0]*w for _ in range(h)]

N_S = 16
st = [[[0]*16 for _ in range(16)] for _ in range(N_S)]

# t0 empty
draw_pad(st[1], 8, 8, 6.4, notch_dir=0.5, veins=False)   # t1 small pad
draw_pad(st[2], 8, 8, 7.6, notch_dir=2.2, wobble=0.4)    # t2 medium pad
wide = new_canvas(32, 16)                                # t3-4 wide pad 2x1
draw_pad(wide, 16, 8, 0, veins=False)  # placeholder; ellipse below
for y in range(16):
    for x in range(32):
        dx, dy = (x-16+0.5)/15.0, (y-8+0.5)/7.2
        d = math.hypot(dx, dy)
        if d > 1.0: continue
        ang = math.atan2(dy, dx)
        da = (ang - 0.9 + math.pi) % (2*math.pi) - math.pi
        if abs(da) < 0.22 and d > 0.25: wide[y][x] = 0; continue
        if d > 0.90: c = 1
        elif d > 0.78: c = 2
        else:
            light = (-dx - dy)/1.6
            if light > 0.28: c = 5
            elif light > 0.0: c = 4
            elif light < -0.35: c = 2
            else: c = 3
            if d < 0.2: c = 5
            if light > 0.45 and d < 0.5 and (x+y) % 3 == 0: c = 6
        wide[y][x] = c
for y in range(16):
    for x in range(16):
        st[3][y][x] = wide[y][x]
        st[4][y][x] = wide[y][x+16]
big = new_canvas(32, 32)                                 # t5-8 big pad 2x2
draw_pad(big, 16, 16, 14.2, notch_dir=0.75, wobble=0.7)
for y in range(16):
    for x in range(16):
        st[5][y][x] = big[y][x]
        st[6][y][x] = big[y][x+16]
        st[7][y][x] = big[y+16][x]
        st[8][y][x] = big[y+16][x+16]
draw_pad(st[9], 8, 9, 6.8, notch_dir=-2.4, veins=False)  # t9 lotus pad
for dx, dy, c in ((0, -1, 12), (-1, 0, 10), (1, 0, 10), (0, 1, 10),
                  (-1, -1, 11), (1, -1, 11), (0, -2, 11), (0, 0, 13)):
    st[9][6+dy][8+dx] = c
for x, y in ((3, 4), (11, 2), (7, 9), (13, 12), (2, 13)):  # t10 sparkles
    st[10][y][x] = 8
for y in range(16):                                       # t11 shadow blob
    for x in range(16):
        d = math.hypot(x-8+0.5, y-8+0.5)
        if d < 6.5 and (x + y) % 2 == 0:
            st[11][y][x] = 9
for n, seed in ((12, 0), (13, 5)):                        # t12-13 reeds
    for s, h, lean in ((4, 14, 0.9), (9, 16, -0.7), (13, 11, 0.5)):
        for k in range(h):
            y = 15 - k
            x = s + int(lean*math.sin(k*0.35 + seed))
            if 0 <= x < 16:
                st[n][y][x] = 14 if (k + seed) % 3 else 15
                if k == h-1 and 0 <= x < 16:
                    st[n][y][x] = 14
# t14 small pad, other notch direction; t15 tiny pad
draw_pad(st[14], 8, 8, 6.2, notch_dir=3.6, veins=False)
draw_pad(st[15], 8, 8, 4.6, notch_dir=1.6, veins=False)

SM = [[0]*32 for _ in range(32)]
def place(x, y, t):
    if SM[y][x] == 0: SM[y][x] = t
#reeds along the banks
for x, y, t in ((0, 29, 12), (1, 30, 13), (2, 29, 12), (29, 29, 13),
                (30, 30, 12), (31, 29, 13), (0, 1, 13), (1, 0, 12),
                (30, 0, 13), (31, 1, 12)):
    place(x, y, t)
#sparkles on open water
for x, y in ((4, 2), (16, 2), (28, 7), (2, 13), (11, 19), (25, 19),
             (6, 28), (19, 29), (30, 18), (13, 6), (22, 23)):
    place(x, y, 10)

# ====================== koi (fsp tiles 18-19, palettes 5-6) ==============
fsp_pal0 = list(Image.open('assets/fsp_pal0.png').convert('RGB').getdata())[:16]
used = [1, 2, 3, 4, 5]
assert len(set(fsp_pal0[i] for i in used)) == len(used), \
    'fsp palette 0 has duplicate colors at koi indices'

def draw_koi(tail_up):
    t = [[0]*16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            dx, dy = (x - 6.5) / 5.5, (y - 8) / 3.5
            if dx*dx + dy*dy <= 1.0:
                t[y][x] = 2 if dx*dx + dy*dy > 0.55 else 3
    for x, y in ((5, 7), (6, 7), (5, 8), (8, 9), (9, 8)):
        t[y][x] = 5
    t[7][10] = 1
    ty = 6 if tail_up else 10
    for k in range(4):
        x = 3 - k
        for y in range(8 - k, 9 + k):
            yy = y + (ty - 8) * k // 3
            if 0 <= yy < 16:
                t[yy][x] = 4
    t[4][7] = 4
    t[12][7] = 4
    t[8][12] = 1
    return t

koi_frames = [draw_koi(True), draw_koi(False)]
kimg = Image.new('RGB', (32, 16), fsp_pal0[0])
for n, t in enumerate(koi_frames):
    for y in range(16):
        for x in range(16):
            kimg.putpixel((n*16 + x, y), fsp_pal0[t[y][x]])
kimg.save('assets/koi_tiles.png')

KOI_ORANGE = list(fsp_pal0)
KOI_ORANGE[1] = (32, 24, 24)
KOI_ORANGE[2] = (232, 96, 32)
KOI_ORANGE[3] = (248, 200, 120)
KOI_ORANGE[4] = (248, 144, 64)
KOI_ORANGE[5] = (248, 240, 224)
KOI_CALICO = list(fsp_pal0)
KOI_CALICO[1] = (32, 24, 24)
KOI_CALICO[2] = (240, 240, 232)
KOI_CALICO[3] = (248, 224, 200)
KOI_CALICO[4] = (248, 200, 180)
KOI_CALICO[5] = (216, 72, 48)
for name, pal in (('koi_pal0', KOI_ORANGE), ('koi_pal1', KOI_CALICO)):
    im = Image.new('RGB', (16, 1))
    im.putdata([tuple(c) for c in pal])
    im.save(f'assets/{name}.png')

# ====================== pond half-sprites (hsp 128-131) ==================
hsp_pal0 = list(Image.open('assets/hsp_pal0.png').convert('RGB').getdata())[:16]
pt = [[[0]*8 for _ in range(8)] for _ in range(4)]
for n, (lo, hi) in enumerate(((2, 6), (6, 11), (10, 14))):
    for y in range(8):
        for x in range(8):
            d2 = (2*x-7)*(2*x-7) + (2*y-7)*(2*y-7)
            if lo*4 <= d2 <= hi*4 and (x + y + n) % 4:
                pt[n][y][x] = 1
for x, y in ((3, 3), (4, 3), (3, 4), (4, 4)):
    pt[3][y][x] = 2
pimg = Image.new('RGB', (32, 8), hsp_pal0[0])
for n in range(4):
    for y in range(8):
        for x in range(8):
            pimg.putpixel((n*8 + x, y), hsp_pal0[pt[n][y][x]])
pimg.save('assets/pond_hsp_tiles.png')


# ============== lily pads as full sprites (fsp tiles 20-24) ==============
#shadow blob tile in the surface palette's shadow color (index 9)
shadow_tile = [[0]*16 for _ in range(16)]
for y in range(16):
    for x in range(16):
        if math.hypot(x-8+0.5, y-8+0.5) < 7.2 and (x + y) % 2 == 0:
            shadow_tile[y][x] = 9

PAD_TILES = [st[1], st[2], st[9], st[15], shadow_tile]
need = sorted({v for t in PAD_TILES for row in t for v in row})
#map surface-palette indices onto slots whose fsp-pal0 colors are unique
first = []
seen = set()
for i, c in enumerate(fsp_pal0):
    if c not in seen:
        seen.add(c)
        if i != 0:
            first.append(i)
merges = [(7, 1), (12, 10), (11, 10), (6, 5), (13, 11)]
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
pimg2 = Image.new('RGB', (len(work)*16, 16), fsp_pal0[0])
for n, t in enumerate(work):
    for y in range(16):
        for x in range(16):
            pimg2.putpixel((n*16 + x, y), fsp_pal0[mapping[t[y][x]]])
pimg2.save('assets/pad_fsp_tiles.png')
lp = Image.new('RGB', (16, 1))
lp.putdata(leaf_pal)
lp.save('assets/leaf_pal.png')

#keep the fsp manifest in sync (idempotent)
mf = [l for l in open('assets/fsp.mfst').read().splitlines()
      if 'pad_fsp_tiles' not in l and 'leaf_pal' not in l]
mf.append('tileset assets/pad_fsp_tiles.png')
mf.append(f'palette assets/leaf_pal.png alpha={shadow_slot}')
open('assets/fsp.mfst', 'w').write('\n'.join(mf) + '\n')
print(f'pad sprites: shadow color slot {shadow_slot}')

# ====================== write everything ======================
def save_strip(tiles, pal, path, size=16):
    img = Image.new('RGB', (len(tiles)*size, size))
    for n, t in enumerate(tiles):
        for y in range(size):
            for x in range(size):
                img.putpixel((n*size + x, y), pal[t[y][x]])
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

save_strip(dt, DPAL, 'assets/scene3_depths_tiles.png')
save_pal(DPAL, 'assets/scene3_depths_pal0.png')
save_map(DM, 'assets/scene3_depths.map')
save_map(LM, 'assets/scene3_lights.map')
save_strip(st, SPAL, 'assets/scene3_surface_tiles.png')
save_pal(SPAL, 'assets/scene3_surface_pal0.png')
save_map(SM, 'assets/scene3_surface.map')
print(f'scene3 assets: depths {N_D} tiles, surface {N_S} tiles,'
      ' 2 koi frames, 2 koi palettes, 4 pond half-sprites')
