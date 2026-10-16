# -*- coding: utf-8 -*-
"""
Данные для интерактивной 3D-карты Беларуси (её можно вращать, наклонять и приближать прямо в игре).

Берёт готовую сцену blender/belarus_map.blend (её строит build_map.py) и выгружает:
    assets/terrain.png       — вид строго сверху (ортокамера): рельеф, поля, пущи, болота, реки, озёра,
                               посёлки с тем же солнцем и небом, что и у большой карты; фон прозрачный
    assets/terrain_h.png     — карта высот: R,G — высота в метрах модели (16 бит, ×40 по вертикали),
                               B — 255 внутри границы, A — 255
    assets/terrain.json      — границы прямоугольника в км, размеры сетки высот, низ плиты и контур
                               борта плиты (x, y, высота верха) — по нему игра строит земляные стенки

Игра рисует всё сама, собственным программным растеризатором (src/terrain3d.c), без OpenGL и библиотек.

Запуск (из корня belarus-game; EEVEE нужен виртуальный дисплей):
    xvfb-run -a -s "-screen 0 1280x1024x24" blender -b blender/belarus_map.blend -P tools/blender/build_terrain3d.py -- \
        [--tex 2048] [--grid 2.5] [--samples 48]

Мировые единицы сцены — километры: X = (lon - 28) * 65.95, Y = (lat - 53.7) * 111.32, Z вверх.
"""
import bpy
import json
import math
import os
import sys
import time
from mathutils import Vector
from mathutils.bvhtree import BVHTree

T0 = time.time()
ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def opt(name, default):
    if name in ARGV:
        i = ARGV.index(name)
        if i + 1 < len(ARGV):
            return ARGV[i + 1]
    return default


def log(*a):
    print("[terrain3d %6.1fs]" % (time.time() - T0), *a, flush=True)


TEX_W = int(opt("--tex", "2048"))
GRID = float(opt("--grid", "2.5"))          # шаг сетки высот, км
SAMPLES = int(opt("--samples", "48"))
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
ASSETS = os.path.join(ROOT, "assets")

scene = bpy.context.scene
terrain = bpy.data.objects["Terrain"]
slab = bpy.data.objects["SlabSides"]

# ---------------------------------------------------------------- контур плиты (верхняя кромка борта)
sv = slab.data.vertices
outline = [(sv[i].co.x, sv[i].co.y, sv[i].co.z) for i in range(0, len(sv) - 1, 2)]
z_bottom = min(v.co.z for v in sv)
# прореживаем до ~1 км * 3: стенки всё равно гладкие
step = max(1, len(outline) // 900)
outline = outline[::step]
xs = [p[0] for p in outline]
ys = [p[1] for p in outline]
X0, X1 = min(xs) - 2.0, max(xs) + 2.0
Y0, Y1 = min(ys) - 2.0, max(ys) + 2.0
W, H = X1 - X0, Y1 - Y0
TEX_H = int(round(TEX_W * H / W))
log("rect %.1f..%.1f x %.1f..%.1f km, texture %dx%d, outline %d pts, bottom %.1f" % (X0, X1, Y0, Y1, TEX_W, TEX_H, len(outline), z_bottom))

# ---------------------------------------------------------------- высоты: луч сверху по мешу рельефа
mw = terrain.matrix_world
me = terrain.data
verts = [mw @ v.co for v in me.vertices]
polys = [list(p.vertices) for p in me.polygons]
bvh = BVHTree.FromPolygons(verts, polys, all_triangles=False)
NX = int(round(W / GRID)) + 1
NY = int(round(H / GRID)) + 1
hgt = [None] * (NX * NY)
for j in range(NY):
    y = Y0 + H * j / (NY - 1)
    for i in range(NX):
        x = X0 + W * i / (NX - 1)
        hit = bvh.ray_cast(Vector((x, y, 200.0)), Vector((0.0, 0.0, -1.0)), 500.0)
        if hit[0] is not None:
            hgt[j * NX + i] = hit[0].z
inside = [h is not None for h in hgt]
log("height grid %dx%d, inside %d" % (NX, NY, sum(inside)))
# снаружи — высота ближайшей точки внутри (разрастание по соседям), чтобы край не проваливался
filled = list(hgt)
while True:
    new = {}
    for k in range(NX * NY):
        if filled[k] is not None:
            continue
        i, j = k % NX, k // NX
        acc, n = 0.0, 0
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            ii, jj = i + di, j + dj
            if 0 <= ii < NX and 0 <= jj < NY and filled[jj * NX + ii] is not None:
                acc += filled[jj * NX + ii]
                n += 1
        if n:
            new[k] = acc / n
    if not new:
        break
    for k, v in new.items():
        filled[k] = v
zmin, zmax = min(filled), max(filled)
log("heights %.2f..%.2f km" % (zmin, zmax))

import struct, zlib


def write_png(path, w, h, rows):
    """rows: список bytes по w*4 (RGBA). Простой PNG без PIL (Blender-python)."""
    raw = b"".join(b"\x00" + r for r in rows)

    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    data += chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(data)


rows = []
for j in range(NY - 1, -1, -1):          # верхняя строка картинки — север (большие Y)
    r = bytearray()
    for i in range(NX):
        k = j * NX + i
        m = int(round(max(0.0, filled[k]) * 1000.0))
        m = max(0, min(65535, m))
        r += bytes((m >> 8, m & 255, 255 if inside[k] else 0, 255))
    rows.append(bytes(r))
write_png(os.path.join(ASSETS, "terrain_h.png"), NX, NY, rows)

# ---------------------------------------------------------------- вид сверху
for ob in bpy.data.objects:
    if ob.name in ("SlabSides",):
        ob.hide_render = True
cam_data = bpy.data.cameras.new("TopCamera")
cam_data.type = 'ORTHO'
cam_data.ortho_scale = max(W, H)
cam_data.clip_start = 1.0
cam_data.clip_end = 400.0
cam = bpy.data.objects.new("TopCamera", cam_data)
scene.collection.objects.link(cam)
cam.location = Vector(((X0 + X1) / 2, (Y0 + Y1) / 2, 150.0))
cam.rotation_euler = (0.0, 0.0, 0.0)
scene.camera = cam
r = scene.render
r.resolution_x, r.resolution_y, r.resolution_percentage = TEX_W, TEX_H, 100
r.use_border = False
r.film_transparent = True
r.image_settings.file_format = 'PNG'
r.image_settings.color_mode = 'RGBA'
r.image_settings.color_depth = '8'
scene.eevee.taa_render_samples = SAMPLES
sun = bpy.data.objects.get("Sun")
if sun:
    sun.data.shadow_cascade_max_distance = 400.0
r.filepath = os.path.join(ASSETS, "terrain.png")
log("rendering top view ...")
bpy.ops.render.render(write_still=True)
log("written", r.filepath)

meta = {
    "x0": X0, "y0": Y0, "x1": X1, "y1": Y1,
    "nx": NX, "ny": NY, "zscale": 0.001, "zbottom": z_bottom,
    "tex_w": TEX_W, "tex_h": TEX_H,
    "lon0": 28.0, "lat0": 53.7, "kx": 65.95, "ky": 111.32,
    "outline": [[round(x, 2), round(y, 2), round(z, 3)] for x, y, z in outline],
}
with open(os.path.join(ASSETS, "terrain.json"), "w") as f:
    json.dump(meta, f, separators=(",", ":"))
log("done")
