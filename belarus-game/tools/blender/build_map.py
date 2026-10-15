# -*- coding: utf-8 -*-
"""
3D diorama map of Belarus for "Судьба Республики Беларусь в объективе технологий".

    blender -b -P tools/blender/build_map.py -- [options]

The script reads the geography from src/game.h and src/data_core.c (BORDER, RIVERS,
CITIES), builds the whole scene procedurally, saves blender/belarus_map.blend and renders
assets/map3d.png (1600x1280 RGBA, transparent background) plus assets/map3d_grid.json,
the lon/lat -> pixel lookup the game uses to place things on the picture.

EEVEE needs an OpenGL context; on a headless Linux box run it under xvfb:
    xvfb-run -a -s "-screen 0 1280x1024x24" blender -b -P tools/blender/build_map.py

Options (after "--"):
    --preview         half resolution, 16 samples, writes /tmp/map3d_preview.png only
    --samples N       EEVEE render samples (default 64)
    --percent P       render resolution percentage (default 100)
    --out PATH        render path (default assets/map3d.png)
    --no-render       build + save the .blend and the grid, skip rendering
    --no-save         do not write the .blend

Pure Python (no numpy needed). World units are kilometres:
    X = (lon - 28.0) * 65.95,  Y = (lat - 53.7) * 111.32,  Z up, terrain height x40.
"""
import bpy
import math
import random
import os
import sys
import re
import json
import time
import tempfile
from mathutils import Vector, Euler
from mathutils.geometry import delaunay_2d_cdt
from mathutils.bvhtree import BVHTree
from bpy_extras.object_utils import world_to_camera_view

T_START = time.time()


def log(*a):
    print("[map %6.1fs]" % (time.time() - T_START), *a, flush=True)


# ----------------------------------------------------------------------------- options
ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def opt(name, default=None):
    if name in ARGV:
        i = ARGV.index(name)
        if i + 1 < len(ARGV):
            return ARGV[i + 1]
    return default


PREVIEW = "--preview" in ARGV
DO_RENDER = "--no-render" not in ARGV
DO_SAVE = "--no-save" not in ARGV and not PREVIEW
SAMPLES = int(opt("--samples", "16" if PREVIEW else "64"))
PERCENT = int(opt("--percent", "50" if PREVIEW else "100"))

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT_PNG = opt("--out", os.path.join(tempfile.gettempdir(), "map3d_preview.png") if PREVIEW
              else os.path.join(ROOT, "assets", "map3d.png"))
OUT_BLEND = os.path.join(ROOT, "blender", "belarus_map.blend")
OUT_GRID = os.path.join(ROOT, "assets", "map3d_grid.json")
RES_X, RES_Y = 1600, 1280
VIEW = opt("--view", "Standard")
BORDER_CROP = [float(v) for v in opt("--crop", "").split(",")] if opt("--crop") else None  # x0,y0,x1,y1 (0..1, y up)

# ----------------------------------------------------------------------------- projection
LON0, LAT0, KX, KY = 28.0, 53.7, 65.95, 111.32
VEXAG = 0.04          # metres -> km with 40x vertical exaggeration
Z_BOTTOM = -22.0      # bottom of the diorama slab (top is 4..14 km)


def to_xy(lon, lat):
    return ((lon - LON0) * KX, (lat - LAT0) * KY)


def to_ll(x, y):
    return (LON0 + x / KX, LAT0 + y / KY)


def sstep(a, b, x):
    t = (x - a) / (b - a)
    if t <= 0.0:
        return 0.0
    if t >= 1.0:
        return 1.0
    return t * t * (3.0 - 2.0 * t)


def srgb(r, g, b, a=1.0):
    """sRGB 0..255 -> linear RGBA tuple."""
    def c(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return (c(r), c(g), c(b), a)


# ----------------------------------------------------------------------------- data parsing
def parse_data():
    hdr = open(os.path.join(ROOT, "src", "game.h"), encoding="utf-8").read()
    src = open(os.path.join(ROOT, "src", "data_core.c"), encoding="utf-8").read()
    order = re.findall(r"C_\w+", re.search(r"C_MINSK.*?C_COUNT_", hdr, re.S).group(0))[:-1]
    cdict = {}
    for m in re.finditer(r'\[(C_\w+)\]\s*=\s*\{\s*L"([^"]*)"\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)\s*,\s*(\d+)', src):
        cdict[m.group(1)] = (m.group(2), float(m.group(3)), float(m.group(4)), int(m.group(5)))
    cities = [(key,) + cdict[key] for key in order]
    pair = r"\{\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\}"
    body = re.search(r"BORDER\[\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    border = [(float(a), float(b)) for a, b in re.findall(pair, body)]
    arrays = {}
    for m in re.finditer(r"static const GeoPt (\w+)\[\]\s*=\s*\{(.*?)\};", src, re.S):
        arrays[m.group(1)] = [(float(a), float(b)) for a, b in re.findall(pair, m.group(2))]
    rbody = re.search(r"const River RIVERS\[\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    rivers = [(n, arrays[a]) for n, a in re.findall(r'\{\s*L"([^"]*)"\s*,\s*(\w+)\s*,', rbody)]
    return cities, border, rivers


# ----------------------------------------------------------------------------- noise / curves
class VNoise:
    """Smooth value noise on a 256x256 lattice, result in 0..1."""

    def __init__(self, seed):
        r = random.Random(seed)
        self.t = [r.random() for _ in range(65536)]

    def __call__(self, x, y):
        xi = math.floor(x)
        yi = math.floor(y)
        fx = x - xi
        fy = y - yi
        sx = fx * fx * (3.0 - 2.0 * fx)
        sy = fy * fy * (3.0 - 2.0 * fy)
        i0 = xi & 255
        i1 = (xi + 1) & 255
        j0 = (yi & 255) << 8
        j1 = ((yi + 1) & 255) << 8
        t = self.t
        a = t[j0 + i0]
        b = t[j0 + i1]
        c = t[j1 + i0]
        d = t[j1 + i1]
        return a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy

    def fbm(self, x, y, scale, octaves=3):
        s, amp, tot, norm = 1.0 / scale, 1.0, 0.0, 0.0
        for o in range(octaves):
            tot += amp * self(x * s + 17.3 * o, y * s - 9.1 * o)
            norm += amp
            amp *= 0.5
            s *= 2.03
        return tot / norm


def catmull_rom(P, closed=False, per_km=1.0):
    """Centripetal Catmull-Rom through the points P (list of (x, y))."""
    n = len(P)
    if closed:
        pts = [P[-1]] + list(P) + [P[0], P[1]]
        nseg = n
    else:
        pts = ([(2 * P[0][0] - P[1][0], 2 * P[0][1] - P[1][1])] + list(P) +
               [(2 * P[-1][0] - P[-2][0], 2 * P[-1][1] - P[-2][1])])
        nseg = n - 1
    out = []

    def tj(ti, a, b):
        return ti + max(1e-4, math.hypot(b[0] - a[0], b[1] - a[1])) ** 0.5

    for s in range(nseg):
        p0, p1, p2, p3 = pts[s], pts[s + 1], pts[s + 2], pts[s + 3]
        t0 = 0.0
        t1 = tj(t0, p0, p1)
        t2 = tj(t1, p1, p2)
        t3 = tj(t2, p2, p3)
        k = max(2, int(math.hypot(p2[0] - p1[0], p2[1] - p1[1]) * per_km))
        for q in range(k):
            t = t1 + (t2 - t1) * q / k
            res = []
            for c in (0, 1):
                a1 = (t1 - t) / (t1 - t0) * p0[c] + (t - t0) / (t1 - t0) * p1[c]
                a2 = (t2 - t) / (t2 - t1) * p1[c] + (t - t1) / (t2 - t1) * p2[c]
                a3 = (t3 - t) / (t3 - t2) * p2[c] + (t - t2) / (t3 - t2) * p3[c]
                b1 = (t2 - t) / (t2 - t0) * a1 + (t - t0) / (t2 - t0) * a2
                b2 = (t3 - t) / (t3 - t1) * a2 + (t - t1) / (t3 - t1) * a3
                res.append((t2 - t) / (t2 - t1) * b1 + (t - t1) / (t2 - t1) * b2)
            out.append((res[0], res[1]))
    if not closed:
        out.append(P[-1])
    return out


def resample(P, step, closed=False):
    pts = list(P) + ([P[0]] if closed else [])
    cum = [0.0]
    for i in range(1, len(pts)):
        cum.append(cum[-1] + math.hypot(pts[i][0] - pts[i - 1][0], pts[i][1] - pts[i - 1][1]))
    total = cum[-1]
    n = max(2, int(round(total / step)))
    out, j = [], 0
    for k in range(n if closed else n + 1):
        d = total * k / n
        while j < len(cum) - 2 and cum[j + 1] < d:
            j += 1
        seg = max(1e-9, cum[j + 1] - cum[j])
        t = min(1.0, max(0.0, (d - cum[j]) / seg))
        out.append((pts[j][0] + (pts[j + 1][0] - pts[j][0]) * t, pts[j][1] + (pts[j + 1][1] - pts[j][1]) * t))
    return out


class PointHash:
    """Bucketed point set for 'nearest within radius' queries."""

    def __init__(self, cell):
        self.cell = cell
        self.b = {}

    def add(self, x, y, data=None):
        k = (int(math.floor(x / self.cell)), int(math.floor(y / self.cell)))
        self.b.setdefault(k, []).append((x, y, data))

    def near(self, x, y, r):
        c = self.cell
        kx0, kx1 = int(math.floor((x - r) / c)), int(math.floor((x + r) / c))
        ky0, ky1 = int(math.floor((y - r) / c)), int(math.floor((y + r) / c))
        for kx in range(kx0, kx1 + 1):
            for ky in range(ky0, ky1 + 1):
                for p in self.b.get((kx, ky), ()):
                    yield p

    def nearest(self, x, y, r):
        best = r
        for px, py, _ in self.near(x, y, r):
            d = math.hypot(px - x, py - y)
            if d < best:
                best = d
        return best


# ----------------------------------------------------------------------------- geography
CITIES, BORDER_LL, RIVERS_LL = parse_data()
log("parsed: %d cities, %d border pts, %d rivers" % (len(CITIES), len(BORDER_LL), len(RIVERS_LL)))

bxy = [to_xy(*p) for p in BORDER_LL]
area = sum(bxy[i][0] * bxy[(i + 1) % len(bxy)][1] - bxy[(i + 1) % len(bxy)][0] * bxy[i][1]
           for i in range(len(bxy))) / 2.0
if area < 0:
    bxy.reverse()                                    # counter-clockwise from above
BORDER = resample(catmull_rom(bxy, True, 2.0), 1.0, True)
BMINX = min(p[0] for p in BORDER)
BMAXX = max(p[0] for p in BORDER)
BMINY = min(p[1] for p in BORDER)
BMAXY = max(p[1] for p in BORDER)
BHASH = PointHash(5.0)
for p in BORDER:
    BHASH.add(p[0], p[1])


def border_dist(x, y, r=8.0):
    return BHASH.nearest(x, y, r)


# Mask raster (1 km) over the slab; also used as the inside/outside lookup.
MRES = 1.0
MX0, MY0 = BMINX - 4.0, BMINY - 4.0
MNX = int(math.ceil((BMAXX + 4.0 - MX0) / MRES))
MNY = int(math.ceil((BMAXY + 4.0 - MY0) / MRES))
MW, MH = MNX * MRES, MNY * MRES
INSIDE = bytearray(MNX * MNY)
for j in range(MNY):
    y = MY0 + (j + 0.5) * MRES
    xs = []
    n = len(BORDER)
    for i in range(n):
        ax, ay = BORDER[i]
        bx, by = BORDER[(i + 1) % n]
        if (ay <= y < by) or (by <= y < ay):
            xs.append(ax + (y - ay) / (by - ay) * (bx - ax))
    xs.sort()
    for k in range(0, len(xs) - 1, 2):
        i0 = max(0, int(math.ceil((xs[k] - MX0) / MRES - 0.5)))
        i1 = min(MNX - 1, int(math.floor((xs[k + 1] - MX0) / MRES - 0.5)))
        for i in range(i0, i1 + 1):
            INSIDE[j * MNX + i] = 1


def inside(x, y):
    i = int((x - MX0) / MRES)
    j = int((y - MY0) / MRES)
    if 0 <= i < MNX and 0 <= j < MNY:
        return INSIDE[j * MNX + i] == 1
    return False


# --- rivers: data rivers + a few smaller ones
RIVER_STYLE = {  # name: (full width km, width factor at the first point, valley depth m)
    "Западная Двина": (2.3, 0.9, 11), "Днепр": (2.5, 0.8, 12), "Неман": (2.1, 0.45, 11),
    "Припять": (2.3, 0.55, 8), "Березина": (1.7, 0.45, 9), "Сож": (1.7, 0.75, 9),
    "Вилия": (1.4, 0.5, 8),
}
EXTRA_RIVERS = [
    ("Свислочь", [(27.15, 54.17), (27.40, 54.06), (27.48, 53.98), (27.56, 53.90), (27.70, 53.78),
                  (27.95, 53.60), (28.30, 53.40), (28.70, 53.20), (29.05, 53.09), (29.27, 53.08)], (1.0, 0.6, 6)),
    ("Птичь", [(27.25, 53.75), (27.55, 53.50), (27.90, 53.30), (28.30, 53.10), (28.65, 52.90),
               (28.70, 52.60), (28.62, 52.30), (28.53, 52.13)], (1.0, 0.55, 5)),
    ("Ясельда", [(24.55, 52.70), (24.90, 52.55), (25.25, 52.42), (25.55, 52.25), (25.85, 52.13),
                 (26.00, 52.09)], (1.0, 0.55, 4)),
    ("Щара", [(26.10, 52.85), (25.75, 52.75), (25.45, 52.85), (25.30, 53.08), (25.00, 53.25),
              (24.62, 53.42)], (1.0, 0.55, 5)),
    ("Друть", [(29.70, 54.45), (29.78, 54.15), (29.90, 53.80), (30.00, 53.45), (30.06, 53.12)], (0.9, 0.55, 5)),
    ("Случь", [(27.45, 53.20), (27.55, 53.03), (27.50, 52.75), (27.35, 52.45), (27.15, 52.20),
               (26.98, 52.055)], (0.9, 0.55, 4)),
]


def build_rivers():
    out = []
    for name, pts in RIVERS_LL:
        w, w0, dep = RIVER_STYLE.get(name, (1.4, 0.6, 7))
        out.append((name, [to_xy(*p) for p in pts], w, w0, dep))
    for name, pts, (w, w0, dep) in EXTRA_RIVERS:
        out.append((name, [to_xy(*p) for p in pts], w, w0, dep))
    # Western Bug: follows the south-west border (lon < 23.75, lat < 52.4), shifted inside
    seg = [p for p in BORDER_LL if p[0] < 23.75 and p[1] < 52.4]
    if len(seg) >= 3:
        seg.sort(key=lambda p: p[1])
        pts = catmull_rom([to_xy(*p) for p in seg], False, 1.0)
        pts = resample(pts, 1.0)
        shifted = []
        for i, (x, y) in enumerate(pts):
            a = pts[max(0, i - 1)]
            b = pts[min(len(pts) - 1, i + 1)]
            dx, dy = b[0] - a[0], b[1] - a[1]
            L = math.hypot(dx, dy) or 1.0
            nx, ny = -dy / L, dx / L
            if not inside(x + nx * 3.0, y + ny * 3.0):
                nx, ny = -nx, -ny
            shifted.append((x + nx * 2.1, y + ny * 2.1))
        out.append(("Западный Буг", shifted, 1.3, 1.0, 4))
    rivers = []
    for name, pts, w, w0, dep in out:
        sm = resample(catmull_rom(pts, False, 1.5), 0.5)
        rivers.append({"name": name, "pts": sm, "w": w, "w0": w0, "depth": dep})
    return rivers


RIVERS = build_rivers()
RHASH = PointHash(4.0)
for r in RIVERS:
    n = len(r["pts"])
    for i, (x, y) in enumerate(r["pts"]):
        hw = 0.5 * r["w"] * (r["w0"] + (1.0 - r["w0"]) * i / max(1, n - 1))
        RHASH.add(x, y, hw)


def river_clearance(x, y, r=6.0):
    """distance to the nearest river bank (negative inside the water)."""
    best = r
    for px, py, hw in RHASH.near(x, y, r):
        d = math.hypot(px - x, py - y) - hw
        if d < best:
            best = d
    return best


# --- lakes: (name, lon, lat, semi-axis a km, b km, rotation deg)
LAKES_DEF = [
    ("Нарочь", 26.75, 54.86, 7.2, 4.6, 15), ("Мястро", 26.92, 54.88, 2.3, 1.5, 60),
    ("Баторино", 26.96, 54.83, 1.8, 1.2, 0), ("Свирь", 26.42, 54.82, 5.0, 1.4, 5),
    ("Освейское", 28.10, 56.02, 4.6, 3.6, 0), ("Дрисвяты", 26.80, 55.60, 3.8, 3.0, 30),
    ("Лукомльское", 29.07, 54.68, 4.6, 2.6, 70), ("Червоное", 27.90, 52.40, 4.2, 3.2, 20),
    ("Дривяты", 27.00, 55.62, 3.6, 1.7, 100), ("Снуды", 27.12, 55.59, 2.6, 1.5, 30),
    ("Струсто", 27.13, 55.66, 2.3, 1.5, 120), ("Неспиш", 27.04, 55.68, 1.6, 1.0, 0),
    ("Минское море", 27.43, 54.03, 4.8, 1.6, 75), ("Вилейское вдхр.", 27.12, 54.555, 8.5, 1.9, 28),
    ("Выгоновское", 25.85, 52.58, 3.0, 2.3, 0), ("Споровское", 25.32, 52.43, 2.6, 1.5, 10),
    ("Селява", 28.97, 54.33, 2.2, 1.4, 40),
]


class Lake:
    def __init__(self, idx, name, lon, lat, a, b, rot):
        self.name = name
        self.cx, self.cy = to_xy(lon, lat)
        self.a, self.b, self.rot = a, b, math.radians(rot)
        r = random.Random(100 + idx)
        self.h = [(r.uniform(0.03, 0.08) / k ** 0.3, r.uniform(0, 6.283), k) for k in (2, 3, 4, 5, 7)]
        self.rmax = max(a, b) * 1.25
        # keep the whole lake 1.5 km inside the border
        for _ in range(40):
            bad = None
            for t in range(24):
                th = t * math.tau / 24
                rr = self.R(th)
                x, y = self.cx + rr * math.cos(th), self.cy + rr * math.sin(th)
                if not inside(x, y) or border_dist(x, y) < 1.5:
                    bad = th
                    break
            if bad is None:
                break
            self.cx -= 0.7 * math.cos(bad)
            self.cy -= 0.7 * math.sin(bad)
        self.level = None

    def R(self, th):
        c, s = math.cos(th - self.rot), math.sin(th - self.rot)
        base = 1.0 / math.sqrt((c / self.a) ** 2 + (s / self.b) ** 2)
        return base * (1.0 + sum(a * math.sin(k * th + p) for a, p, k in self.h))

    def rho(self, x, y):
        dx, dy = x - self.cx, y - self.cy
        r = math.hypot(dx, dy)
        if r < 1e-6:
            return 0.0
        return r / self.R(math.atan2(dy, dx))


LAKES = [Lake(i, *d) for i, d in enumerate(LAKES_DEF)]


def lake_rho(x, y):
    best = 9.0
    for lk in LAKES:
        if abs(x - lk.cx) < lk.rmax + 4 and abs(y - lk.cy) < lk.rmax + 4:
            best = min(best, lk.rho(x, y))
    return best


# --- cities
TIER = {"C_MINSK": 4, "C_VITEBSK": 3, "C_MOGILEV": 3, "C_GOMEL": 3, "C_GRODNO": 3, "C_BREST": 3,
        "C_POLOTSK": 2, "C_PINSK": 2, "C_NOVOGRUDOK": 2, "C_TUROV": 2, "C_BOBRUISK": 2, "C_BORISOV": 2,
        "C_MOZYR": 2, "C_ORSHA": 2, "C_NOVOPOLOTSK": 2, "C_SOLIGORSK": 2, "C_LIDA": 2, "C_SLUTSK": 2}
CITY_R = {4: 11.0, 3: 6.6, 2: 4.8, 1: 3.6}
TOWNS = []
for key, name, lon, lat, major in CITIES:
    tier = TIER.get(key, 2 if major else 1)
    x, y = to_xy(lon, lat)
    TOWNS.append({"key": key, "name": name, "lon": lon, "lat": lat, "x": x, "y": y, "tier": tier,
                  "r": CITY_R[tier]})


def town_clearance(x, y):
    best = 99.0
    for t in TOWNS:
        d = math.hypot(x - t["x"], y - t["y"]) - t["r"]
        if d < best:
            best = d
    return best


# ----------------------------------------------------------------------------- heights
BUMPS = [  # lon, lat, amplitude m, sigma along km, sigma across km, axis rotation deg
    (24.45, 53.15, 95, 32, 18, 35),    # Volkovysk upland
    (25.80, 53.60, 140, 22, 15, 35),   # Novogrudok upland, 323 m
    (25.35, 53.25, 40, 40, 25, 20),    # Baranovichi plain
    (27.07, 53.87, 100, 16, 12, 40),   # Dzyarzhynskaya hill, 345 m
    (27.45, 54.00, 80, 45, 22, 40),    # Minsk upland
    (28.05, 54.25, 65, 38, 16, 20),    # Lahoysk-Barysaw heights
    (26.00, 54.40, 135, 26, 18, 45),   # Ashmyany upland
    (26.70, 55.00, 45, 26, 16, 35),    # Svyantsyany ridges
    (27.00, 55.60, 70, 24, 18, 10),    # Braslav upland
    (30.00, 55.62, 115, 26, 20, 20),   # Vitebsk upland
    (30.00, 54.60, 90, 50, 22, 0),     # Orsha upland
    (30.70, 54.05, 40, 50, 40, 0),     # Mogilev-Horki plateau
    (27.10, 53.15, 45, 30, 12, 25),    # Kopyl ridge
    (29.20, 52.03, 65, 26, 6, 10),     # Mozyr ridge
    (24.00, 53.75, -38, 45, 35, 0),    # Neman lowland at Grodno
    (28.50, 55.25, -14, 55, 35, 0),    # Polotsk lowland
    (30.60, 52.60, -14, 60, 50, 0),    # Gomel Polesie
]
RIDGE = [to_xy(*p) for p in ((24.4, 53.1), (25.8, 53.6), (27.07, 53.87), (27.9, 54.2), (29.0, 54.45),
                             (30.0, 54.6))]
BUMPS_XY = []
for lon, lat, amp, sa, sb, rot in BUMPS:
    cx, cy = to_xy(lon, lat)
    BUMPS_XY.append((cx, cy, amp, sa, sb, math.cos(math.radians(rot)), math.sin(math.radians(rot))))
NZ_H = VNoise(7)


def seg_dist(px, py, ax, ay, bx, by):
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / L2)) if L2 > 0 else 0.0
    return math.hypot(px - ax - t * dx, py - ay - t * dy)


def height_m(x, y):
    lat = LAT0 + y / KY
    h = 152.0 - 24.0 * sstep(52.9, 52.1, lat)
    up = 0.0
    for cx, cy, amp, sa, sb, c, s in BUMPS_XY:
        dx, dy = x - cx, y - cy
        if abs(dx) > 4 * sa or abs(dy) > 4 * sa:
            continue
        u = dx * c + dy * s
        v = -dx * s + dy * c
        e = (u / sa) ** 2 + (v / sb) ** 2
        if e < 16:
            g = amp * math.exp(-e)
            h += g
            if g > 0:
                up += g
    d = min(seg_dist(x, y, *RIDGE[i], *RIDGE[i + 1]) for i in range(len(RIDGE) - 1))
    g = 36.0 * math.exp(-(d / 38.0) ** 2)
    h += g
    up += g
    upl = min(1.0, up / 110.0)
    n = NZ_H.fbm(x, y, 45.0, 3) - 0.5
    h += n * (14.0 + 26.0 * upl)
    h += (NZ_H.fbm(x + 500.0, y - 300.0, 11.0, 3) - 0.5) * (14.0 + 62.0 * upl)   # rolling hills
    if lat > 54.8:  # hummocky moraine north
        h += (NZ_H(x / 6.0 + 50, y / 6.0) - 0.5) * 14.0 * sstep(54.8, 55.3, lat)
    return h


HRES = 1.5
HX0, HY0 = -375.0, -370.0
HNX = int((410.0 - HX0) / HRES) + 1
HNY = int((380.0 - HY0) / HRES) + 1
HM = [0.0] * (HNX * HNY)
for j in range(HNY):
    y = HY0 + j * HRES
    row = j * HNX
    for i in range(HNX):
        HM[row + i] = height_m(HX0 + i * HRES, y)
log("height raster %dx%d, raw range %.0f..%.0f m" % (HNX, HNY, min(HM), max(HM)))

# river valleys
CARVE = [0.0] * (HNX * HNY)
for r in RIVERS:
    pts = r["pts"]
    n = len(pts)
    for k in range(0, n, 2):
        x, y = pts[k]
        hw = 0.5 * r["w"] * (r["w0"] + (1.0 - r["w0"]) * k / max(1, n - 1))
        flat, sig, dep = hw + 0.7, 3.5, r["depth"]
        rad = flat + 2.6 * sig
        i0, i1 = max(0, int((x - rad - HX0) / HRES)), min(HNX - 1, int((x + rad - HX0) / HRES) + 1)
        j0, j1 = max(0, int((y - rad - HY0) / HRES)), min(HNY - 1, int((y + rad - HY0) / HRES) + 1)
        for j in range(j0, j1 + 1):
            gy = HY0 + j * HRES
            for i in range(i0, i1 + 1):
                d = math.hypot(HX0 + i * HRES - x, gy - y)
                f = 1.0 if d <= flat else math.exp(-((d - flat) / sig) ** 2)
                v = dep * f
                if v > CARVE[j * HNX + i]:
                    CARVE[j * HNX + i] = v
for k in range(len(HM)):
    HM[k] -= CARVE[k]


def hm_sample(x, y):
    fx = (x - HX0) / HRES
    fy = (y - HY0) / HRES
    i = max(0, min(HNX - 2, int(math.floor(fx))))
    j = max(0, min(HNY - 2, int(math.floor(fy))))
    tx = min(1.0, max(0.0, fx - i))
    ty = min(1.0, max(0.0, fy - j))
    k = j * HNX + i
    a, b, c, d = HM[k], HM[k + 1], HM[k + HNX], HM[k + HNX + 1]
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty


# lakes: flatten the floor to a level a little below the lowest shore point
for lk in LAKES:
    rim = [hm_sample(lk.cx + lk.R(t * math.tau / 16) * math.cos(t * math.tau / 16),
                     lk.cy + lk.R(t * math.tau / 16) * math.sin(t * math.tau / 16)) for t in range(16)]
    lk.level = min(rim + [hm_sample(lk.cx, lk.cy)]) - 2.0
    rad = lk.rmax + 4.0
    for j in range(max(0, int((lk.cy - rad - HY0) / HRES)), min(HNY, int((lk.cy + rad - HY0) / HRES) + 2)):
        for i in range(max(0, int((lk.cx - rad - HX0) / HRES)), min(HNX, int((lk.cx + rad - HX0) / HRES) + 2)):
            rho = lk.rho(HX0 + i * HRES, HY0 + j * HRES)
            w = 1.0 - sstep(1.0, 1.0 + 3.0 / max(lk.a, lk.b), rho)
            if w > 0:
                k = j * HNX + i
                HM[k] = HM[k] * (1 - w) + min(HM[k], lk.level) * w

HK = [v * VEXAG for v in HM]


def H(x, y):
    """terrain top height in km (the same function is used outside the border)."""
    fx = (x - HX0) / HRES
    fy = (y - HY0) / HRES
    i = max(0, min(HNX - 2, int(math.floor(fx))))
    j = max(0, min(HNY - 2, int(math.floor(fy))))
    tx = min(1.0, max(0.0, fx - i))
    ty = min(1.0, max(0.0, fy - j))
    k = j * HNX + i
    a, b, c, d = HK[k], HK[k + 1], HK[k + HNX], HK[k + HNX + 1]
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty


for nm, lon, lat in (("Dzyarzhynskaya", 27.07, 53.87), ("Novogrudok", 25.8, 53.6), ("Ashmyany", 26.0, 54.4),
                     ("Vitebsk upl", 30.0, 55.6), ("Orsha upl", 30.0, 54.6), ("Braslav", 27.0, 55.6),
                     ("Polesie", 27.5, 52.0), ("Grodno", 23.83, 53.68), ("Gomel", 30.98, 52.43),
                     ("Minsk", 27.56, 53.90), ("Mogilev", 30.33, 53.90)):
    log("  height %-14s %4.0f m" % (nm, H(*to_xy(lon, lat)) / VEXAG))

# ----------------------------------------------------------------------------- masks (R forest, G swamp, B meadow, A town)
NZ_F = VNoise(21)
NZ_S = VNoise(33)
NZ_M = VNoise(45)
NPIX = MNX * MNY
MR = [0.0] * NPIX
MG = [0.0] * NPIX
MB = [0.0] * NPIX
MA = [0.0] * NPIX
ACTIVE = bytearray(NPIX)          # inside, dilated by 2 px
for j in range(MNY):
    for i in range(MNX):
        if INSIDE[j * MNX + i]:
            for jj in range(max(0, j - 2), min(MNY, j + 3)):
                base = jj * MNX
                for ii in range(max(0, i - 2), min(MNX, i + 3)):
                    ACTIVE[base + ii] = 1


def stamp(arr, lon, lat, rx, ry, rot, noise, warp=0.45, edge=(0.72, 1.0), dens=1.0):
    cx, cy = to_xy(lon, lat)
    c, s = math.cos(math.radians(rot)), math.sin(math.radians(rot))
    rad = max(rx, ry) * 1.5
    for j in range(max(0, int((cy - rad - MY0) / MRES)), min(MNY, int((cy + rad - MY0) / MRES) + 1)):
        y = MY0 + (j + 0.5) * MRES
        for i in range(max(0, int((cx - rad - MX0) / MRES)), min(MNX, int((cx + rad - MX0) / MRES) + 1)):
            k = j * MNX + i
            if not ACTIVE[k]:
                continue
            x = MX0 + (i + 0.5) * MRES
            dx, dy = x - cx, y - cy
            u = (dx * c + dy * s) / rx
            v = (-dx * s + dy * c) / ry
            rho = math.sqrt(u * u + v * v) + (noise.fbm(x, y, 7.0, 2) - 0.5) * warp
            val = (1.0 - sstep(edge[0], edge[1], rho)) * dens
            if val > arr[k]:
                arr[k] = val


FOREST_MASSIFS = [  # lon, lat, rx, ry, rot, density
    (24.02, 52.66, 30, 22, 25, 1.0),   # Belovezhskaya Pushcha
    (26.40, 53.85, 26, 18, 25, 1.0),   # Naliboki
    (28.30, 54.70, 24, 24, 0, 1.0),    # Berezinsky
    (28.00, 52.00, 22, 16, 0, 1.0),    # Pripyatsky
    (28.00, 52.10, 66, 34, 0, 0.8),    # Polesie forests
    (26.80, 54.90, 24, 18, 0, 0.95),   # Narochansky
    (27.00, 55.60, 18, 15, 0, 0.9),    # Braslav lakes
    (29.10, 53.35, 22, 16, 30, 0.85),  # Bobruisk forests
    (25.30, 52.65, 22, 14, 10, 0.8),   # Ivatsevichy forests
    (30.40, 52.75, 20, 14, 60, 0.75),  # Gomel forests
]
for lon, lat, rx, ry, rot, dens in FOREST_MASSIFS:
    stamp(MR, lon, lat, rx, ry, rot, NZ_F, 0.5, (0.65, 1.0), dens)

BOGS = [  # lon, lat, rx, ry, rot
    (27.30, 51.90, 17, 11, 10),   # Almany bogs
    (27.80, 55.55, 9, 7, 30),     # Yelnya
    (28.45, 54.82, 10, 8, 0),     # Berezinsky bogs
    (24.75, 51.98, 13, 6, 0),     # Zvanets
    (25.35, 52.42, 12, 5, 10),    # Sporovo
    (24.40, 52.75, 7, 5, 0),      # Dikoe
    (28.20, 55.92, 8, 5, 0),      # Osveya mires
    (29.60, 51.75, 11, 7, 0),     # Polesie reserve area
]
for lon, lat, rx, ry, rot in BOGS:
    stamp(MG, lon, lat, rx, ry, rot, NZ_S, 0.55, (0.62, 0.95), 1.0)

for j in range(MNY):
    y = MY0 + (j + 0.5) * MRES
    lat = LAT0 + y / KY
    for i in range(MNX):
        k = j * MNX + i
        if not ACTIVE[k]:
            continue
        x = MX0 + (i + 0.5) * MRES
        lon = LON0 + x / KX
        # scattered forest, about a third of the land
        n = NZ_F.fbm(x + 300, y - 200, 22.0, 3)
        sc = sstep(0.555, 0.61, n) * 0.85
        if sc > MR[k]:
            MR[k] = sc
        # Polesie wetlands band along the Pripyat
        band = sstep(52.55, 52.2, lat) * sstep(24.0, 24.6, lon) * sstep(30.5, 29.9, lon)
        if band > 0:
            n2 = NZ_S.fbm(x, y, 12.0, 3)
            sw = band * sstep(0.53, 0.60, n2 + 0.06 * band)
            if sw > MG[k]:
                MG[k] = sw

# floodplain meadows along rivers
for r in RIVERS:
    pts = r["pts"]
    n = len(pts)
    for q in range(0, n, 2):
        x, y = pts[q]
        hw = 0.5 * r["w"] * (r["w0"] + (1.0 - r["w0"]) * q / max(1, n - 1))
        fp = (4.0 if r["name"] == "Припять" else 2.6 if r["w"] >= 2.0 else 1.6) * (0.6 + 0.8 * NZ_M(x / 9.0, y / 9.0))
        rad = hw + fp + 1.5
        for j in range(max(0, int((y - rad - MY0) / MRES)), min(MNY, int((y + rad - MY0) / MRES) + 1)):
            for i in range(max(0, int((x - rad - MX0) / MRES)), min(MNX, int((x + rad - MX0) / MRES) + 1)):
                k = j * MNX + i
                d = math.hypot(MX0 + (i + 0.5) * MRES - x, MY0 + (j + 0.5) * MRES - y)
                v = 1.0 - sstep(hw + 0.4, hw + 0.4 + fp, d)
                if v > MB[k]:
                    MB[k] = v
# lake shores
for lk in LAKES:
    rad = lk.rmax + 4.0
    for j in range(max(0, int((lk.cy - rad - MY0) / MRES)), min(MNY, int((lk.cy + rad - MY0) / MRES) + 1)):
        for i in range(max(0, int((lk.cx - rad - MX0) / MRES)), min(MNX, int((lk.cx + rad - MX0) / MRES) + 1)):
            k = j * MNX + i
            rho = lk.rho(MX0 + (i + 0.5) * MRES, MY0 + (j + 0.5) * MRES)
            v = 1.0 - sstep(1.0, 1.0 + 2.2 / max(lk.a, lk.b), rho)
            if v > MB[k]:
                MB[k] = v
            if rho < 1.0 + 1.5 / max(lk.a, lk.b):
                MR[k] = 0.0
                MG[k] = 0.0
# towns
for t in TOWNS:
    rad = t["r"] + 4.0
    for j in range(max(0, int((t["y"] - rad - MY0) / MRES)), min(MNY, int((t["y"] + rad - MY0) / MRES) + 1)):
        for i in range(max(0, int((t["x"] - rad - MX0) / MRES)), min(MNX, int((t["x"] + rad - MX0) / MRES) + 1)):
            k = j * MNX + i
            x, y = MX0 + (i + 0.5) * MRES, MY0 + (j + 0.5) * MRES
            d = math.hypot(x - t["x"], y - t["y"]) + (NZ_M(x / 2.5, y / 2.5) - 0.5) * 1.6
            v = 1.0 - sstep(t["r"] * 0.8, t["r"] + 1.2, d)
            if v > MA[k]:
                MA[k] = v
for k in range(NPIX):
    MR[k] *= (1.0 - 0.85 * MG[k]) * (1.0 - MA[k]) * (1.0 - 0.75 * MB[k])
    MG[k] *= (1.0 - MA[k])
    MB[k] *= (1.0 - MA[k])
log("masks %dx%d done; forest %.0f%%, swamp %.0f%% of land" % (MNX, MNY,
    100.0 * sum(1 for k in range(NPIX) if INSIDE[k] and MR[k] > 0.5) / max(1, sum(INSIDE)),
    100.0 * sum(1 for k in range(NPIX) if INSIDE[k] and MG[k] > 0.5) / max(1, sum(INSIDE))))


def mask_sample(arr, x, y):
    fx = (x - MX0) / MRES - 0.5
    fy = (y - MY0) / MRES - 0.5
    i = max(0, min(MNX - 2, int(math.floor(fx))))
    j = max(0, min(MNY - 2, int(math.floor(fy))))
    tx = min(1.0, max(0.0, fx - i))
    ty = min(1.0, max(0.0, fy - j))
    k = j * MNX + i
    a, b, c, d = arr[k], arr[k + 1], arr[k + MNX], arr[k + MNX + 1]
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty


# ----------------------------------------------------------------------------- scene reset
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'NONE'
TMPDIR = tempfile.mkdtemp(prefix="bymap_")


def new_object(name, mesh, coll=None):
    ob = bpy.data.objects.new(name, mesh)
    (coll or scene.collection).objects.link(ob)
    return ob


def set_corner_colors(mesh, face_cols, name="col"):
    attr = mesh.color_attributes.new(name, 'FLOAT_COLOR', 'CORNER')
    flat = []
    for poly in mesh.polygons:
        c = face_cols[poly.index]
        flat.extend(c * poly.loop_total)
    attr.data.foreach_set("color", flat)


def set_uv(mesh, vert_uv, name="UVMap"):
    uvl = mesh.uv_layers.new(name=name)
    li = [0] * len(mesh.loops)
    mesh.loops.foreach_get("vertex_index", li)
    flat = []
    for vi in li:
        flat.extend(vert_uv[vi])
    uvl.data.foreach_set("uv", flat)


# ----------------------------------------------------------------------------- terrain mesh
pts2 = [Vector(p) for p in BORDER]
nb = len(pts2)
GSP = 1.6
gy = BMINY + GSP * 0.5
while gy < BMAXY:
    row = int(round((gy - BMINY) / GSP))
    gx = BMINX + GSP * (0.5 if row % 2 else 0.0)
    while gx < BMAXX:
        if inside(gx, gy) and border_dist(gx, gy, 3.0) > 0.75:
            pts2.append(Vector((gx, gy)))
        gx += GSP
    gy += GSP * 0.866
cdt = delaunay_2d_cdt(pts2, [], [list(range(nb))], 1, 1e-4)
tv2, tfaces = cdt[0], cdt[2]
tverts = [(v.x, v.y, H(v.x, v.y)) for v in tv2]
me = bpy.data.meshes.new("Terrain")
me.from_pydata(tverts, [], [list(f) for f in tfaces])
me.update()
if sum(p.normal.z for p in me.polygons[:200]) < 0:
    me.flip_normals()
me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
set_uv(me, [((x - MX0) / MW, (y - MY0) / MH) for x, y, _ in tverts])
terrain = new_object("Terrain", me)
log("terrain: %d verts, %d tris" % (len(me.vertices), len(me.polygons)))
BVH = BVHTree.FromPolygons([Vector(v) for v in tverts], [list(f) for f in tfaces], all_triangles=True)


def ground(x, y):
    hit = BVH.ray_cast(Vector((x, y, 100.0)), Vector((0.0, 0.0, -1.0)), 400.0)
    return hit[0].z if hit[0] is not None else None


# slab sides + bottom
sv, sf, suv = [], [], []
cum = 0.0
for i, (x, y) in enumerate(BORDER):
    top = H(x, y)
    sv.append((x, y, top))
    sv.append((x, y, Z_BOTTOM))
    if i:
        cum += math.hypot(x - BORDER[i - 1][0], y - BORDER[i - 1][1])
    suv.append((cum, 0.0))
    suv.append((cum, top - Z_BOTTOM))
for i in range(nb):
    a, b = 2 * i, 2 * ((i + 1) % nb)
    sf.append([a, a + 1, b + 1, b])
sf.append([2 * i + 1 for i in reversed(range(nb))])
me = bpy.data.meshes.new("SlabSides")
me.from_pydata(sv, [], sf)
me.update()
set_uv(me, suv)
slab = new_object("SlabSides", me)

# ----------------------------------------------------------------------------- shader helpers


class NB:
    def __init__(self, mat):
        mat.use_nodes = True
        self.nt = mat.node_tree
        self.nt.nodes.clear()

    def node(self, typ, **props):
        n = self.nt.nodes.new(typ)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def put(self, sock, val):
        if isinstance(val, bpy.types.NodeSocket):
            self.nt.links.new(val, sock)
        else:
            sock.default_value = val

    def math(self, op, a, b=0.0, c=None):
        n = self.node('ShaderNodeMath', operation=op)
        self.put(n.inputs[0], a)
        self.put(n.inputs[1], b)
        if c is not None:
            self.put(n.inputs[2], c)
        return n.outputs[0]

    def mix(self, fac, c1, c2, blend='MIX'):
        n = self.node('ShaderNodeMixRGB', blend_type=blend)
        self.put(n.inputs['Fac'], fac)
        self.put(n.inputs['Color1'], c1)
        self.put(n.inputs['Color2'], c2)
        return n.outputs['Color']

    def comb(self, x, y, z):
        n = self.node('ShaderNodeCombineXYZ')
        self.put(n.inputs[0], x)
        self.put(n.inputs[1], y)
        self.put(n.inputs[2], z)
        return n.outputs[0]

    def sep(self, v):
        n = self.node('ShaderNodeSeparateXYZ')
        self.put(n.inputs[0], v)
        return n.outputs

    def ramp(self, fac, stops, interp='LINEAR'):
        n = self.node('ShaderNodeValToRGB')
        cr = n.color_ramp
        cr.interpolation = interp
        while len(cr.elements) < len(stops):
            cr.elements.new(0.5)
        for el, (pos, col) in zip(cr.elements, stops):
            el.position = pos
            el.color = col
        self.put(n.inputs['Fac'], fac)
        return n.outputs['Color']

    def smooth(self, a, b, x, lo=0.0, hi=1.0):
        n = self.node('ShaderNodeMapRange', interpolation_type='SMOOTHSTEP')
        self.put(n.inputs['Value'], x)
        n.inputs['From Min'].default_value = a
        n.inputs['From Max'].default_value = b
        n.inputs['To Min'].default_value = lo
        n.inputs['To Max'].default_value = hi
        return n.outputs['Result']

    def noise(self, vec, size_km, detail=2.0, rough=0.5):
        n = self.node('ShaderNodeTexNoise')
        self.put(n.inputs['Vector'], vec)
        n.inputs['Scale'].default_value = 1.0 / size_km
        n.inputs['Detail'].default_value = detail
        n.inputs['Roughness'].default_value = rough
        return n.outputs['Fac']

    def white(self, vec):
        n = self.node('ShaderNodeTexWhiteNoise', noise_dimensions='3D')
        self.put(n.inputs['Vector'], vec)
        return n

    def bsdf(self, color, rough=0.9, spec=0.25):
        b = self.node('ShaderNodeBsdfPrincipled')
        self.put(b.inputs['Base Color'], color)
        self.put(b.inputs['Roughness'], rough)
        for nm in ('Specular IOR Level', 'Specular'):
            if nm in b.inputs:
                b.inputs[nm].default_value = spec
                break
        out = self.node('ShaderNodeOutputMaterial')
        self.nt.links.new(b.outputs[0], out.inputs['Surface'])
        return b


# --- mask image
img = bpy.data.images.new("MapMasks", MNX, MNY, alpha=True, float_buffer=False)
img.colorspace_settings.name = 'Non-Color'
img.alpha_mode = 'CHANNEL_PACKED'
flat = [0.0] * (NPIX * 4)
flat[0::4] = MR
flat[1::4] = MG
flat[2::4] = MB
flat[3::4] = MA
img.pixels.foreach_set(flat)
img.filepath_raw = os.path.join(TMPDIR, "map_masks.png")
img.file_format = 'PNG'
img.save()
img.pack()
img.colorspace_settings.name = 'Non-Color'
img.alpha_mode = 'CHANNEL_PACKED'
del flat

# --- terrain material
mat_t = bpy.data.materials.new("Terrain")
nb_ = NB(mat_t)
geo = nb_.node('ShaderNodeNewGeometry')
X, Y, _Z = nb_.sep(geo.outputs['Position'])
P2 = nb_.comb(X, Y, 0.0)
vor = nb_.node('ShaderNodeTexVoronoi', voronoi_dimensions='2D', feature='F1')
nb_.put(vor.inputs['Vector'], P2)
vor.inputs['Scale'].default_value = 1.0 / 46.0
vor.inputs['Randomness'].default_value = 0.9
vcol = nb_.node('ShaderNodeSeparateColor')
nb_.put(vcol.inputs[0], vor.outputs['Color'])
rr, rg, rbb = vcol.outputs[0], vcol.outputs[1], vcol.outputs[2]
vor2 = nb_.node('ShaderNodeTexVoronoi', voronoi_dimensions='2D', feature='DISTANCE_TO_EDGE')
nb_.put(vor2.inputs['Vector'], P2)
vor2.inputs['Scale'].default_value = 1.0 / 46.0
vor2.inputs['Randomness'].default_value = 0.9
ang = nb_.math('MULTIPLY', rr, math.pi)
ca, sa = nb_.math('COSINE', ang), nb_.math('SINE', ang)
U = nb_.math('ADD', nb_.math('MULTIPLY', X, ca), nb_.math('MULTIPLY', Y, sa))
V = nb_.math('SUBTRACT', nb_.math('MULTIPLY', Y, ca), nb_.math('MULTIPLY', X, sa))
rowh = nb_.math('MULTIPLY_ADD', rg, 2.0, 3.2)
vv = nb_.math('DIVIDE', V, rowh)
row = nb_.math('FLOOR', vv)
fv = nb_.math('FRACT', vv)
seedr = nb_.math('MULTIPLY', rbb, 37.0)
w1 = nb_.white(nb_.comb(row, seedr, 0.0))
w2 = nb_.white(nb_.comb(row, seedr, 5.0))
cellw = nb_.math('MULTIPLY_ADD', w1.outputs['Value'], 5.5, 3.6)
uu = nb_.math('ADD', nb_.math('DIVIDE', U, cellw), nb_.math('MULTIPLY', w2.outputs['Value'], 7.0))
col_i = nb_.math('FLOOR', uu)
fu = nb_.math('FRACT', uu)
wc = nb_.white(nb_.comb(row, col_i, nb_.math('MULTIPLY_ADD', rbb, 53.0, rr)))
ccol = nb_.node('ShaderNodeSeparateColor')
nb_.put(ccol.inputs[0], wc.outputs['Color'])
FIELD_PAL = [srgb(160, 174, 120), srgb(146, 152, 90), srgb(200, 186, 126), srgb(172, 182, 128),
             srgb(140, 164, 96), srgb(188, 176, 112), srgb(156, 164, 104), srgb(206, 194, 138),
             srgb(132, 156, 94), srgb(176, 158, 112), srgb(166, 178, 116)]
field = nb_.ramp(ccol.outputs[0], [(i / len(FIELD_PAL), c) for i, c in enumerate(FIELD_PAL)], 'CONSTANT')
bright = nb_.math('MULTIPLY_ADD', ccol.outputs[1], 0.1, 0.95)
field = nb_.mix(1.0, field, bright, 'MULTIPLY')
dv = nb_.math('MULTIPLY', nb_.math('MINIMUM', fv, nb_.math('SUBTRACT', 1.0, fv)), rowh)
du = nb_.math('MULTIPLY', nb_.math('MINIMUM', fu, nb_.math('SUBTRACT', 1.0, fu)), cellw)
de = nb_.math('MULTIPLY', vor2.outputs['Distance'], 46.0)
edge = nb_.math('MINIMUM', nb_.math('MINIMUM', du, dv), de)
hedge = nb_.smooth(0.03, 0.16, edge, 0.55, 0.0)
field = nb_.mix(hedge, field, srgb(98, 116, 70))
grain = nb_.noise(P2, 0.6, 2.0)
field = nb_.mix(1.0, field, nb_.math('MULTIPLY_ADD', grain, 0.12, 0.94), 'MULTIPLY')
# masks
uvn = nb_.node('ShaderNodeUVMap')
uvn.uv_map = "UVMap"
mt = nb_.node('ShaderNodeTexImage', interpolation='Linear', extension='EXTEND')
mt.image = img
nb_.put(mt.inputs['Vector'], uvn.outputs['UV'])
msep = nb_.node('ShaderNodeSeparateColor')
nb_.put(msep.inputs[0], mt.outputs['Color'])
mR, mG, mB = msep.outputs[0], msep.outputs[1], msep.outputs[2]
mA = mt.outputs['Alpha']
jit = nb_.math('MULTIPLY', nb_.math('SUBTRACT', nb_.noise(P2, 2.2, 2.0), 0.5), 0.16)
c = nb_.mix(nb_.smooth(0.3, 0.75, mB, 0.0, 0.8), field, srgb(116, 158, 76))
fcol = nb_.mix(nb_.noise(P2, 9.0, 2.0), srgb(44, 80, 46), srgb(78, 112, 58))
c = nb_.mix(nb_.smooth(0.4, 0.6, nb_.math('ADD', mR, jit)), c, fcol)
sbase = nb_.mix(nb_.noise(P2, 3.0, 3.0), srgb(136, 124, 72), srgb(110, 106, 62))
sbase = nb_.mix(nb_.smooth(0.56, 0.66, nb_.noise(P2, 1.4, 2.0)), sbase, srgb(160, 152, 92))
npool = nb_.noise(nb_.comb(X, Y, 3.7), 2.2, 1.5)
pf = nb_.smooth(0.565, 0.59, npool)
prim = nb_.smooth(0.535, 0.565, npool)
scol = nb_.mix(prim, sbase, srgb(150, 162, 110))
scol = nb_.mix(pf, scol, srgb(56, 136, 142))
sf = nb_.smooth(0.42, 0.58, nb_.math('ADD', mG, jit))
c = nb_.mix(sf, c, scol)
c = nb_.mix(nb_.smooth(0.4, 0.85, mA, 0.0, 0.85), c, srgb(198, 188, 160))
rough = nb_.math('SUBTRACT', 0.92, nb_.math('MULTIPLY', nb_.math('MULTIPLY', pf, sf), 0.7))
nb_.bsdf(c, rough, 0.3)
terrain.data.materials.append(mat_t)

# --- slab side material: layered earth
mat_s = bpy.data.materials.new("SlabEarth")
nb_ = NB(mat_s)
geo = nb_.node('ShaderNodeNewGeometry')
_X, _Y, Zs = nb_.sep(geo.outputs['Position'])
uvn = nb_.node('ShaderNodeUVMap')
uvn.uv_map = "UVMap"
_u, depth, _w = nb_.sep(uvn.outputs['UV'])
wob = nb_.math('MULTIPLY', nb_.math('SUBTRACT', nb_.noise(geo.outputs['Position'], 30.0, 2.0), 0.5), 7.0)
wob2 = nb_.math('MULTIPLY', nb_.math('SUBTRACT', nb_.noise(geo.outputs['Position'], 2.5, 2.0), 0.5), 1.0)
zt = nb_.math('ADD', nb_.math('ADD', Zs, wob), wob2)
zt = nb_.math('DIVIDE', nb_.math('SUBTRACT', zt, -26.0), 44.0)
STRATA = [(-26, (64, 44, 30)), (-17.5, (66, 46, 31)), (-17, (168, 138, 96)), (-15.5, (162, 132, 90)),
          (-15, (112, 80, 54)), (-10.5, (106, 76, 50)), (-10, (186, 162, 122)), (-9.0, (178, 154, 114)),
          (-8.5, (92, 64, 42)), (-4.5, (96, 66, 44)), (-4.0, (158, 126, 86)), (-3.0, (150, 120, 82)),
          (-2.5, (126, 84, 56)), (2.5, (120, 80, 54)), (3.0, (102, 74, 50)), (18, (100, 72, 48))]
strata = nb_.ramp(zt, [((z + 26.0) / 44.0, srgb(*cc)) for z, cc in STRATA], 'LINEAR')
fine = nb_.noise(nb_.comb(_u, nb_.math('MULTIPLY', depth, 6.0), 0.0), 0.8, 3.0)
strata = nb_.mix(1.0, strata, nb_.math('MULTIPLY_ADD', fine, 0.25, 0.88), 'MULTIPLY')
col = nb_.mix(nb_.smooth(1.4, 2.6, depth, 1.0, 0.0), strata, srgb(58, 42, 30))
col = nb_.mix(nb_.smooth(0.35, 0.75, depth, 1.0, 0.0), col, srgb(80, 102, 54))
col = nb_.mix(1.0, col, nb_.smooth(-22.0, -6.0, Zs, 0.62, 1.0), 'MULTIPLY')
nb_.bsdf(col, 0.95, 0.15)
slab.data.materials.append(mat_s)

# --- water
mat_w = bpy.data.materials.new("Water")
nb_ = NB(mat_w)
uvn = nb_.node('ShaderNodeUVMap')
uvn.uv_map = "UVMap"
shore, _v, _w = nb_.sep(uvn.outputs['UV'])
geo = nb_.node('ShaderNodeNewGeometry')
ripple = nb_.math('MULTIPLY', nb_.math('SUBTRACT', nb_.noise(geo.outputs['Position'], 1.5, 2.0), 0.5), 0.2)
wcol = nb_.ramp(nb_.math('ADD', shore, ripple), [(0.0, srgb(26, 70, 128)), (0.6, srgb(32, 84, 146)),
                                                 (0.9, srgb(58, 124, 170)), (1.0, srgb(104, 162, 186))])
nb_.bsdf(wcol, 0.12, 0.6)

# --- painted materials (colour from the 'col' corner attribute)


def painted(name, vary):
    m = bpy.data.materials.new(name)
    nb_ = NB(m)
    at = nb_.node('ShaderNodeAttribute', attribute_name="col")
    colr = at.outputs['Color']
    if vary:
        oi = nb_.node('ShaderNodeObjectInfo')
        hs = nb_.node('ShaderNodeHueSaturation')
        nb_.put(hs.inputs['Hue'], nb_.math('MULTIPLY_ADD', oi.outputs['Random'], 0.05, 0.475))
        nb_.put(hs.inputs['Value'], nb_.math('MULTIPLY_ADD', oi.outputs['Random'], 0.32, 0.84))
        nb_.put(hs.inputs['Color'], colr)
        colr = hs.outputs['Color']
    nb_.bsdf(colr, 0.85, 0.2)
    return m


mat_inst = painted("PaintedVaried", True)
mat_town = painted("Painted", False)


# ----------------------------------------------------------------------------- mesh builder
class MeshBuf:
    def __init__(self):
        self.v, self.f, self.c = [], [], []

    def vert(self, p):
        self.v.append(tuple(p))
        return len(self.v) - 1

    def face(self, idx, col, center=None):
        if center is not None:
            a, b, c3 = (Vector(self.v[i]) for i in idx[:3])
            nrm = (b - a).cross(c3 - b)
            cen = sum((Vector(self.v[i]) for i in idx), Vector()) / len(idx)
            if nrm.dot(cen - Vector(center)) < 0:
                idx = list(reversed(idx))
        self.f.append(list(idx))
        self.c.append(col)

    def to_mesh(self, name):
        me = bpy.data.meshes.new(name)
        me.from_pydata(self.v, [], self.f)
        me.update()
        set_corner_colors(me, self.c)
        return me


def prism(mb, cx, cy, z0, z1, r, n, col, top_col=None, rot=0.0, cone=False, r_top=None):
    ring0 = [mb.vert((cx + r * math.cos(rot + k * math.tau / n), cy + r * math.sin(rot + k * math.tau / n), z0))
             for k in range(n)]
    if cone:
        apex = mb.vert((cx, cy, z1))
        for k in range(n):
            mb.face([ring0[k], ring0[(k + 1) % n], apex], col, (cx, cy, (z0 + z1) / 2))
        return
    rt = r if r_top is None else r_top
    ring1 = [mb.vert((cx + rt * math.cos(rot + k * math.tau / n), cy + rt * math.sin(rot + k * math.tau / n), z1))
             for k in range(n)]
    for k in range(n):
        mb.face([ring0[k], ring0[(k + 1) % n], ring1[(k + 1) % n], ring1[k]], col, (cx, cy, (z0 + z1) / 2))
    if top_col is not None:
        mb.face(ring1, top_col, (cx, cy, z0 - 1.0))


def ico(mb, cx, cy, cz, rx, rz, col, rot=0.0):
    t = (1.0 + 5 ** 0.5) / 2.0
    base = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
            (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    faces = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
             (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
             (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    L = math.sqrt(1 + t * t)
    cr, sr = math.cos(rot), math.sin(rot)
    idx = []
    for x, y, z in base:
        x, y, z = x / L, y / L, z / L
        x, y = x * cr - y * sr, x * sr + y * cr
        idx.append(mb.vert((cx + x * rx, cy + y * rx, cz + z * rz)))
    for f in faces:
        mb.face([idx[i] for i in f], col, (cx, cy, cz))


def gable_house(mb, cx, cy, z0, ang, L, W, hw, hr, wall, roof, flat=False):
    ca_, sa_ = math.cos(ang), math.sin(ang)
    cen = (cx, cy, z0 + hw * 0.5)

    def P(lx, ly, z):
        return (cx + lx * ca_ - ly * sa_, cy + lx * sa_ + ly * ca_, z)
    l, w = L / 2, W / 2
    zt = z0 + hw
    b = [mb.vert(P(-l, -w, z0)), mb.vert(P(l, -w, z0)), mb.vert(P(l, w, z0)), mb.vert(P(-l, w, z0))]
    t = [mb.vert(P(-l, -w, zt)), mb.vert(P(l, -w, zt)), mb.vert(P(l, w, zt)), mb.vert(P(-l, w, zt))]
    for k in range(4):
        mb.face([b[k], b[(k + 1) % 4], t[(k + 1) % 4], t[k]], wall, cen)
    if flat:
        mb.face(t, roof, (cx, cy, z0))
        return zt
    oh = 0.12 * W
    g0, g1 = mb.vert(P(-l, 0, zt + hr)), mb.vert(P(l, 0, zt + hr))
    mb.face([t[3], t[0], g0], wall, cen)
    mb.face([t[1], t[2], g1], wall, cen)
    r0, r1 = mb.vert(P(-l - oh, 0, zt + hr)), mb.vert(P(l + oh, 0, zt + hr))
    e = [mb.vert(P(-l - oh, -w - oh, zt - 0.06)), mb.vert(P(l + oh, -w - oh, zt - 0.06)),
         mb.vert(P(l + oh, w + oh, zt - 0.06)), mb.vert(P(-l - oh, w + oh, zt - 0.06))]
    mb.face([e[0], e[1], r1, r0], roof, (cx, cy, z0))
    mb.face([e[2], e[3], r0, r1], roof, (cx, cy, z0))
    return zt + hr


# ----------------------------------------------------------------------------- towns
WALLS = [srgb(242, 232, 208), srgb(236, 226, 200), srgb(246, 242, 228), srgb(232, 216, 186),
         srgb(240, 224, 196)]
ROOFS = [srgb(198, 84, 50), srgb(178, 70, 42), srgb(208, 106, 60), srgb(170, 84, 60), srgb(190, 96, 54),
         srgb(120, 94, 84)]
HS = 1.3   # houses are oversized like tabletop miniatures
rng = random.Random(2024)
mb = MeshBuf()
HOUSE_PTS = PointHash(3.0)


def place(t, r_max, sep, tries=60):
    for _ in range(tries):
        rr = r_max * math.sqrt(rng.random())
        th = rng.random() * math.tau
        x, y = t["x"] + rr * math.cos(th), t["y"] + rr * math.sin(th)
        if HOUSE_PTS.nearest(x, y, sep) < sep:
            continue
        if river_clearance(x, y) < 0.9 or lake_rho(x, y) < 1.25:
            continue
        if not inside(x, y) or border_dist(x, y) < 1.5:
            continue
        HOUSE_PTS.add(x, y)
        return x, y
    return None


def base_z(x, y, rad):
    zs = [ground(x + dx, y + dy) for dx, dy in ((0, 0), (rad, 0), (-rad, 0), (0, rad), (0, -rad))]
    zs = [z for z in zs if z is not None]
    return (min(zs) - 0.12) if zs else H(x, y)


for t in TOWNS:
    tier = t["tier"]
    grid_ang = rng.random() * math.pi
    R = t["r"]
    if tier == 4:
        for _ in range(9):          # high-rise towers in the centre
            p = place(t, R * 0.42, 2.2)
            if p:
                z0 = base_z(p[0], p[1], 0.8)
                gable_house(mb, p[0], p[1], z0, grid_ang + rng.choice((0, math.pi / 2)), HS * rng.uniform(1.0, 1.4),
                            HS * rng.uniform(0.9, 1.2), HS * rng.uniform(2.8, 4.6), 0, srgb(226, 228, 230),
                            srgb(128, 132, 138), flat=True)
        for _ in range(12):         # panel blocks
            p = place(t, R * 0.9, 2.5)
            if p:
                z0 = base_z(p[0], p[1], 1.3)
                gable_house(mb, p[0], p[1], z0, grid_ang + rng.choice((0, math.pi / 2)), HS * rng.uniform(2.2, 2.8),
                            HS * 0.85, HS * rng.uniform(1.2, 1.7), 0, rng.choice((srgb(216, 210, 198), srgb(222, 218, 210))),
                            srgb(142, 140, 138), flat=True)
    if tier >= 2:                   # church with a dome
        p = place(t, R * 0.35, 2.3)
        if p:
            x, y = p
            z0 = base_z(x, y, 1.0)
            ang = grid_ang + rng.uniform(-0.2, 0.2)
            zt = gable_house(mb, x, y, z0, ang, HS * 1.7, HS * 1.1, HS * 1.0, HS * 0.45, srgb(248, 246, 238),
                             srgb(74, 118, 96))
            dx, dy = math.cos(ang) * 0.35 * HS, math.sin(ang) * 0.35 * HS
            dome_col = srgb(222, 176, 64) if rng.random() < 0.6 else srgb(70, 134, 112)
            prism(mb, x + dx, y + dy, zt - 0.3 * HS, zt + 0.45 * HS, 0.36 * HS, 8, srgb(248, 246, 238))
            prism(mb, x + dx, y + dy, zt + 0.45 * HS, zt + 0.72 * HS, 0.42 * HS, 8, dome_col, r_top=0.3 * HS)
            prism(mb, x + dx, y + dy, zt + 0.72 * HS, zt + 1.1 * HS, 0.3 * HS, 8, dome_col, cone=True)
    if tier == 3:
        for _ in range(2):
            p = place(t, R * 0.6, 2.4)
            if p:
                z0 = base_z(p[0], p[1], 1.0)
                gable_house(mb, p[0], p[1], z0, grid_ang, HS * rng.uniform(1.6, 2.2), HS * 0.9,
                            HS * rng.uniform(1.3, 1.9), 0,
                            srgb(220, 214, 202), srgb(140, 138, 136), flat=True)
    count = {4: 58, 3: 22, 2: 12, 1: 7}[tier]
    for _ in range(count):
        p = place(t, R, 2.0)
        if not p:
            continue
        x, y = p
        L = HS * rng.uniform(1.1, 1.6)
        W = HS * rng.uniform(0.75, 0.95)
        z0 = base_z(x, y, 0.8)
        ang = grid_ang + rng.choice((0, math.pi / 2)) + rng.uniform(-0.15, 0.15)
        gable_house(mb, x, y, z0, ang, L, W, HS * rng.uniform(0.5, 0.75), HS * rng.uniform(0.38, 0.52),
                    rng.choice(WALLS), rng.choice(ROOFS))
town_ob = new_object("Towns", mb.to_mesh("Towns"))
town_ob.data.materials.append(mat_town)
log("towns: %d faces" % len(mb.f))

# ----------------------------------------------------------------------------- water meshes


def river_mesh():
    verts, faces, uvs = [], [], []
    for r in RIVERS:
        pts = r["pts"]
        n = len(pts)
        prev = None
        for i in range(n):
            x, y = pts[i]
            a = pts[max(0, i - 1)]
            b = pts[min(n - 1, i + 1)]
            dx, dy = b[0] - a[0], b[1] - a[1]
            L = math.hypot(dx, dy) or 1.0
            nx, ny = -dy / L, dx / L
            hw = 0.5 * r["w"] * (r["w0"] + (1.0 - r["w0"]) * i / max(1, n - 1))
            sec = [(x + nx * hw, y + ny * hw, 1.0), (x, y, 0.0), (x - nx * hw, y - ny * hw, 1.0)]
            zs = [ground(px, py) for px, py, _ in sec]
            if any(z is None for z in zs) or border_dist(x, y, 3.0) < hw + 0.3:
                prev = None
                continue
            idx = []
            for (px, py, s), z in zip(sec, zs):
                verts.append((px, py, z + 0.1))
                uvs.append((s, 0.0))
                idx.append(len(verts) - 1)
            if prev is not None:
                faces.append([prev[0], prev[1], idx[1], idx[0]])
                faces.append([prev[1], prev[2], idx[2], idx[1]])
            prev = idx
    me = bpy.data.meshes.new("Rivers")
    me.from_pydata(verts, [], faces)
    me.update()
    for p in me.polygons:
        if p.normal.z < 0:
            p.flip()
    me.update()
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    set_uv(me, uvs)
    return me


rivers_ob = new_object("Rivers", river_mesh())
rivers_ob.data.materials.append(mat_w)


def lakes_mesh():
    verts, faces, uvs = [], [], []
    for lk in LAKES:
        nrim = max(40, int(2 * math.pi * max(lk.a, lk.b) / 0.35))
        pts = []
        for k in range(nrim):
            th = k * math.tau / nrim
            rr = lk.R(th)
            pts.append(Vector((lk.cx + rr * math.cos(th), lk.cy + rr * math.sin(th))))
        rim_n = len(pts)
        for frac in (0.82, 0.6, 0.35):
            m = max(8, int(nrim * frac * 0.6))
            for k in range(m):
                th = (k + 0.5 * (frac > 0.5)) * math.tau / m
                rr = lk.R(th) * frac
                pts.append(Vector((lk.cx + rr * math.cos(th), lk.cy + rr * math.sin(th))))
        pts.append(Vector((lk.cx, lk.cy)))
        res = delaunay_2d_cdt(pts, [], [list(range(rim_n))], 1, 1e-5)
        base = len(verts)
        for v in res[0]:
            z = ground(v.x, v.y)
            z = (H(v.x, v.y) if z is None else z) + 0.1
            verts.append((v.x, v.y, z))
            rho = lk.rho(v.x, v.y)
            uvs.append((min(1.0, max(0.0, (rho - 0.45) / 0.55)) ** 1.5, 0.0))
        for f in res[2]:
            faces.append([base + i for i in f])
    me = bpy.data.meshes.new("Lakes")
    me.from_pydata(verts, [], faces)
    me.update()
    for p in me.polygons:
        if p.normal.z < 0:
            p.flip()
    me.update()
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    set_uv(me, uvs)
    return me


lakes_ob = new_object("Lakes", lakes_mesh())
lakes_ob.data.materials.append(mat_w)
log("water: rivers %d faces, lakes %d faces" % (len(rivers_ob.data.polygons), len(lakes_ob.data.polygons)))

# ----------------------------------------------------------------------------- trees (instances)
kinds_coll = bpy.data.collections.new("TreeKinds")
scene.collection.children.link(kinds_coll)
TRUNK, BIRCH_TRUNK = srgb(92, 66, 44), srgb(214, 208, 196)


def kind_obj(name, build):
    mbk = MeshBuf()
    build(mbk)
    ob = new_object(name, mbk.to_mesh(name), kinds_coll)
    ob.data.materials.append(mat_inst)
    ob.location = (0, 0, -500)
    return ob


kind_obj("K0_spruce", lambda m: (prism(m, 0, 0, -0.2, 0.45, 0.12, 4, TRUNK),
                                 prism(m, 0, 0, 0.35, 2.15, 0.88, 7, srgb(34, 76, 50), cone=True),
                                 prism(m, 0, 0, 1.25, 3.1, 0.62, 7, srgb(38, 82, 54), cone=True, rot=0.4)))
kind_obj("K1_pine", lambda m: (prism(m, 0, 0, -0.2, 1.2, 0.11, 4, TRUNK),
                               prism(m, 0, 0, 1.0, 1.75, 0.72, 6, srgb(56, 100, 54), r_top=0.62),
                               prism(m, 0, 0, 1.75, 2.75, 0.62, 6, srgb(60, 106, 56), cone=True)))
kind_obj("K2_birch", lambda m: (prism(m, 0, 0, -0.2, 1.0, 0.09, 4, BIRCH_TRUNK),
                                ico(m, 0, 0, 1.65, 0.72, 0.95, srgb(132, 170, 72))))
kind_obj("K3_oak", lambda m: (prism(m, 0, 0, -0.2, 0.75, 0.13, 4, TRUNK),
                              ico(m, 0, 0, 1.45, 0.98, 0.82, srgb(84, 126, 54), 0.5)))


def reed(m):
    for k in range(5):
        th = k * math.tau / 5
        bx, by = 0.12 * math.cos(th), 0.12 * math.sin(th)
        tx, ty = 0.45 * math.cos(th + 0.3), 0.45 * math.sin(th + 0.3)
        px, py = -math.sin(th) * 0.06, math.cos(th) * 0.06
        a = m.vert((bx - px, by - py, -0.1))
        b = m.vert((bx + px, by + py, -0.1))
        c3 = m.vert((tx, ty, 0.95 + 0.15 * (k % 2)))
        m.face([a, b, c3], srgb(206, 190, 112) if k % 2 else srgb(176, 176, 98))


kind_obj("K4_reed", reed)
lc = bpy.context.view_layer.layer_collection.children["TreeKinds"]
lc.exclude = True

ng = bpy.data.node_groups.new("ScatterKinds", 'GeometryNodeTree')
ng.interface.new_socket(name="Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
ng.interface.new_socket(name="Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')
gi = ng.nodes.new('NodeGroupInput')
go = ng.nodes.new('NodeGroupOutput')
ci = ng.nodes.new('GeometryNodeCollectionInfo')
ci.transform_space = 'ORIGINAL'
ci.inputs['Collection'].default_value = kinds_coll
ci.inputs['Separate Children'].default_value = True
ci.inputs['Reset Children'].default_value = True


def named(name, dtype):
    n = ng.nodes.new('GeometryNodeInputNamedAttribute')
    n.data_type = dtype
    n.inputs['Name'].default_value = name
    return [o for o in n.outputs if o.enabled][0]


comb = ng.nodes.new('ShaderNodeCombineXYZ')
ng.links.new(named("rot", 'FLOAT'), comb.inputs['Z'])
iop = ng.nodes.new('GeometryNodeInstanceOnPoints')
iop.inputs['Pick Instance'].default_value = True
ng.links.new(gi.outputs[0], iop.inputs['Points'])
ng.links.new(ci.outputs[0], iop.inputs['Instance'])
ng.links.new(named("kind", 'INT'), iop.inputs['Instance Index'])
ng.links.new(comb.outputs[0], iop.inputs['Rotation'])
ng.links.new(named("scale", 'FLOAT'), iop.inputs['Scale'])
ng.links.new(iop.outputs['Instances'], go.inputs[0])


def scatter_object(name, items):
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(items))
    me.vertices.foreach_set("co", [c for it in items for c in it[0]])
    for an, at, pos in (("scale", 'FLOAT', 2), ("rot", 'FLOAT', 3), ("kind", 'INT', 1)):
        a = me.attributes.new(an, at, 'POINT')
        a.data.foreach_set("value", [it[pos] for it in items])
    me.update()
    ob = new_object(name, me)
    mod = ob.modifiers.new("Scatter", 'NODES')
    mod.node_group = ng
    return ob


rng = random.Random(77)
TS = 1.45
trees, reeds = [], []
y = BMINY
while y < BMAXY:
    x = BMINX
    while x < BMAXX:
        px, py = x + rng.uniform(0, TS), y + rng.uniform(0, TS)
        x += TS
        if not inside(px, py):
            continue
        f = mask_sample(MR, px, py)
        g = mask_sample(MG, px, py)
        pick = rng.random()
        is_tree = pick < f * 1.05 or (g > 0.55 and pick < 0.08)
        is_reed = g > 0.5 and not is_tree and rng.random() < 0.2
        if not (is_tree or is_reed):
            continue
        if border_dist(px, py, 3.0) < 1.4 or river_clearance(px, py) < 0.7 or lake_rho(px, py) < 1.12:
            continue
        if town_clearance(px, py) < 0.6:
            continue
        z = ground(px, py)
        if z is None:
            continue
        lat = LAT0 + py / KY
        if is_tree:
            north = sstep(53.4, 55.2, lat)
            south = sstep(52.7, 52.1, lat)
            w = [0.12 + 0.42 * north, 0.18 + 0.4 * south, 0.24, 0.22 - 0.1 * north]
            if g > 0.55:
                w = [0.05, 0.25, 0.6, 0.1]
            k = rng.choices(range(4), weights=w)[0]
            trees.append(((px, py, z), k, rng.uniform(0.78, 1.25), rng.uniform(0, math.tau)))
        else:
            reeds.append(((px, py, z), 4, rng.uniform(0.8, 1.3), rng.uniform(0, math.tau)))
    y += TS
MAX_TREES = 19800
if len(trees) > MAX_TREES:
    log("thinning %d tree candidates" % len(trees))
    trees = rng.sample(trees, MAX_TREES)
if len(reeds) > 4000:
    reeds = rng.sample(reeds, 4000)
forest_ob = scatter_object("Forest", trees)
reeds_ob = scatter_object("Reeds", reeds)
log("instances: %d trees, %d reed tufts" % (len(trees), len(reeds)))

# ----------------------------------------------------------------------------- camera, light, world
cam_data = bpy.data.cameras.new("MapCamera")
cam_data.type = 'ORTHO'
cam = new_object("MapCamera", cam_data)
cam.rotation_euler = Euler((math.radians(38.0), 0.0, 0.0))
Rm = cam.rotation_euler.to_matrix()
right, up, fwd = Rm @ Vector((1, 0, 0)), Rm @ Vector((0, 1, 0)), Rm @ Vector((0, 0, -1))
hull = [Vector((x, y, H(x, y) + 3.5)) for x, y in BORDER[::3]] + [Vector((x, y, Z_BOTTOM)) for x, y in BORDER[::3]]
A = [p.dot(right) for p in hull]
B = [p.dot(up) for p in hull]
MARGIN = 0.035
aspect = RES_X / RES_Y
cam_data.ortho_scale = max((max(A) - min(A)) / (1 - 2 * MARGIN), (max(B) - min(B)) * aspect / (1 - 2 * MARGIN))
centre = right * (max(A) + min(A)) / 2 + up * (max(B) + min(B)) / 2
DIST = 900.0
cam.location = centre - fwd * DIST
depths = [(p - cam.location).dot(fwd) for p in hull]
cam_data.clip_start = max(1.0, min(depths) - 80.0)
cam_data.clip_end = max(depths) + 80.0
scene.camera = cam
log("camera: ortho_scale %.1f km, %.3f px/km" % (cam_data.ortho_scale, RES_X / cam_data.ortho_scale))

sun_data = bpy.data.lights.new("Sun", 'SUN')
sun_data.energy = float(opt('--sun', '3.4'))
sun_data.color = (1.0, 0.94, 0.84)
sun_data.angle = math.radians(2.5)
sun_data.shadow_cascade_max_distance = cam_data.clip_end
sun_data.shadow_cascade_count = 4
sun_data.shadow_cascade_exponent = 0.4
sun_data.shadow_cascade_fade = 0.1
sun_data.use_contact_shadow = True
sun_data.contact_shadow_distance = 1.5
sun = new_object("Sun", sun_data)
az, el = math.radians(225.0), math.radians(35.0)
to_sun = Vector((math.sin(az) * math.cos(el), math.cos(az) * math.cos(el), math.sin(el)))
sun.rotation_euler = (-to_sun).to_track_quat('-Z', 'Y').to_euler()

world = bpy.data.worlds.new("Sky")
scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes.get("Background")
bg.inputs["Color"].default_value = srgb(168, 192, 226)
bg.inputs["Strength"].default_value = float(opt('--sky', '0.5'))

# ----------------------------------------------------------------------------- render settings
scene.render.engine = 'BLENDER_EEVEE'
scene.render.resolution_x = RES_X
scene.render.resolution_y = RES_Y
scene.render.resolution_percentage = PERCENT
scene.render.film_transparent = True
scene.render.filter_size = 1.2
scene.render.image_settings.file_format = 'PNG'
scene.render.image_settings.color_mode = 'RGBA'
scene.render.image_settings.color_depth = '8'
scene.render.image_settings.compression = 90
ee = scene.eevee
ee.taa_render_samples = SAMPLES
ee.use_gtao = True
ee.gtao_distance = 2.5
ee.gtao_factor = 1.2
ee.use_soft_shadows = True
ee.shadow_cascade_size = '4096'
ee.use_shadow_high_bitdepth = True
scene.view_settings.view_transform = VIEW
scene.view_settings.look = 'None'
scene.view_settings.exposure = 0.0
if BORDER_CROP:
    scene.render.use_border = True
    scene.render.use_crop_to_border = True
    (scene.render.border_min_x, scene.render.border_min_y,
     scene.render.border_max_x, scene.render.border_max_y) = BORDER_CROP

if "--clay" in ARGV:   # debug: plain terrain to judge the relief and shadows
    for ob in (forest_ob, reeds_ob):
        ob.hide_render = True
    mat_t.node_tree.nodes.clear()
    NB(mat_t).bsdf((0.5, 0.5, 0.5, 1.0), 0.9, 0.2)

# ----------------------------------------------------------------------------- grid export
bpy.context.view_layer.update()      # make camera.matrix_world current before projecting
grid = {"width": RES_X, "height": RES_Y, "lon0": 22.5, "lat0": 50.5, "step": 0.25, "nlon": 47, "nlat": 27}
pts = []
for jj in range(grid["nlat"]):
    for ii in range(grid["nlon"]):
        x, y = to_xy(grid["lon0"] + ii * grid["step"], grid["lat0"] + jj * grid["step"])
        co = world_to_camera_view(scene, cam, Vector((x, y, H(x, y))))
        pts.append([round(co.x * RES_X, 2), round((1.0 - co.y) * RES_Y, 2)])
grid["pts"] = pts
cl = []
for t in TOWNS:
    co = world_to_camera_view(scene, cam, Vector((t["x"], t["y"], H(t["x"], t["y"]))))
    cl.append({"name": t["name"], "px": round(co.x * RES_X, 1), "py": round((1.0 - co.y) * RES_Y, 1)})
grid["cities"] = cl
grid["px_per_km"] = round(RES_X / cam_data.ortho_scale, 4)
if not PREVIEW:
    os.makedirs(os.path.dirname(OUT_GRID), exist_ok=True)
    with open(OUT_GRID, "w", encoding="utf-8") as fh:
        json.dump(grid, fh, ensure_ascii=False, separators=(",", ":"))
    log("grid written:", OUT_GRID)
else:
    with open(os.path.join(tempfile.gettempdir(), "map3d_grid_preview.json"), "w", encoding="utf-8") as fh:
        json.dump(grid, fh, ensure_ascii=False, separators=(",", ":"))

# ----------------------------------------------------------------------------- save + render
if DO_SAVE:
    os.makedirs(os.path.dirname(OUT_BLEND), exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=OUT_BLEND, compress=True)
    log("saved", OUT_BLEND, "%.1f MB" % (os.path.getsize(OUT_BLEND) / 1e6))
if DO_RENDER:
    scene.render.filepath = OUT_PNG
    bpy.ops.render.render(write_still=True)
    log("rendered", OUT_PNG)
log("done")
