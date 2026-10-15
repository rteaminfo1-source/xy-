#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Стилизованные low-poly 3D-модели строек для карты (спрайт-лист).

Запуск (из корня belarus-game):
    blender -b -P tools/blender/build_sprites.py -- [--samples 160] [--only 0,5,7]
                                                       [--work /tmp/belarus_sprites] [--no-save] [--no-outline]
Blender (Cycles, без шумодава) строит 29 моделей из примитивов, сохраняет
blender/belarus_landmarks.blend и рендерит каждую в клетку 160x160 с прозрачным
фоном и мягкой контактной тенью (shadow catcher). Затем этот же файл запускается
обычным python3 (нужен Pillow) и собирает assets/sprites.png + assets/sprites.json:
    python3 tools/blender/build_sprites.py --post --work /tmp/belarus_sprites
Проекция общая с картой: X = (lon-28)*65.95 км, Y = (lat-53.7)*111.32 км, Z вверх;
камера ортографическая, наклон 38 градусов от вертикали, смотрит на север.
Солнце с юго-запада (азимут 225, высота 35). Якорь модели (центр основания) — пиксель (80,132).
"""
import os, sys, json, math, random, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(os.path.dirname(HERE))
NAMES = ["fort", "church", "stone", "pier", "workshop", "mill", "tower", "castle", "press", "townhall",
         "college", "manufactory", "canal", "railway", "powerplant", "tram", "lab", "radio", "factory",
         "dumptruck", "refinery", "mine", "camp", "metro", "techpark", "library", "dish", "npp", "solar"]
CELL, COLS, ANCHOR = 160, 8, (80, 132)
ORTHO = 16.0          # ширина кадра клетки в единицах модели (10 px на единицу)
TILT = 38.0           # наклон камеры от вертикали
SUN_AZ, SUN_EL = 225.0, 35.0


def parse_args(argv):
    a = {"samples": 160, "only": None, "work": os.path.join(tempfile.gettempdir(), "belarus_sprites"),
         "save": True, "post": False, "outline": True}
    i = 0
    while i < len(argv):
        k = argv[i]
        if k == "--samples": a["samples"] = int(argv[i + 1]); i += 1
        elif k == "--only": a["only"] = [int(x) for x in argv[i + 1].split(",")]; i += 1
        elif k == "--work": a["work"] = argv[i + 1]; i += 1
        elif k == "--no-save": a["save"] = False
        elif k == "--no-outline": a["outline"] = False
        elif k == "--post": a["post"] = True
        i += 1
    return a


try:
    import bpy
    IN_BLENDER = True
except ImportError:
    IN_BLENDER = False

# ---------------------------------------------------------------------------------------------
#  Постобработка (обычный python3 + Pillow): затухание тени, контур, сборка листа, превью
# ---------------------------------------------------------------------------------------------
def post(args):
    from PIL import Image, ImageFilter, ImageChops
    work = args["work"]
    n = len(NAMES)
    rows = (n + COLS - 1) // COLS
    sheet = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
    # эллиптическое затухание длинных теней (тени уходят на северо-восток: вверх-вправо)
    fall = Image.new("L", (CELL, CELL))
    fp = fall.load()
    for y in range(CELL):
        for x in range(CELL):
            dx, dy = (x - 86) / 71.0, (y - 121) / 52.0
            r = math.sqrt(dx * dx + dy * dy)
            t = min(1.0, max(0.0, (1.0 - r) / 0.5))
            edge = min(1.0, x / 4.0, (CELL - 1 - x) / 4.0, y / 4.0, (CELL - 1 - y) / 4.0)
            fp[x, y] = int(255 * 0.85 * t * t * (3 - 2 * t) * edge)
    for i, name in enumerate(NAMES):
        src = os.path.join(work, "cell_%02d.png" % i)
        msk = os.path.join(work, "mask_%02d_0001.png" % i)
        if not os.path.exists(src):
            print("missing", src); continue
        im = Image.open(src).convert("RGBA")
        m = Image.open(msk).convert("L") if os.path.exists(msk) else im.getchannel("A")
        a = im.getchannel("A")
        m = ImageChops.darker(m, a)
        shadow = ImageChops.subtract(a, m)                       # только тень
        shadow = ImageChops.multiply(shadow, fall)
        rgb = im.convert("RGB")
        out = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        # слой тени (тёплый тёмный)
        out = Image.composite(Image.new("RGBA", (CELL, CELL), (28, 24, 30, 255)), out, shadow)
        out.putalpha(shadow)
        if args.get("outline", True):
            # тонкий мягкий тёмный контур для читаемости на карте
            dil = m.filter(ImageFilter.MaxFilter(3))
            ring = ImageChops.subtract(dil, m).point(lambda v: int(v * 0.55))
            ol = Image.new("RGBA", (CELL, CELL), (34, 26, 22, 0))
            ol.putalpha(ring)
            out = Image.alpha_composite(out, ol)
        obj = rgb.convert("RGBA")
        obj.putalpha(m)
        out = Image.alpha_composite(out, obj)
        sheet.paste(out, ((i % COLS) * CELL, (i // COLS) * CELL))
    os.makedirs(os.path.join(GAME, "assets"), exist_ok=True)
    sheet.save(os.path.join(GAME, "assets", "sprites.png"), optimize=True)
    with open(os.path.join(GAME, "assets", "sprites.json"), "w", encoding="utf-8") as f:
        json.dump({"cell": CELL, "cols": COLS, "anchor": list(ANCHOR), "names": NAMES}, f, ensure_ascii=False)
        f.write("\n")
    # превью для проверки: на «карте» в 160 и в 48 px
    for size in (160, 48):
        bg = Image.new("RGBA", (COLS * size, rows * size), (150, 170, 120, 255))
        small = sheet.resize((COLS * size, rows * size), Image.LANCZOS) if size != CELL else sheet
        bg = Image.alpha_composite(bg, small)
        bg.convert("RGB").save(os.path.join(work, "preview_%d.png" % size))
    print("sheet written:", os.path.join(GAME, "assets", "sprites.png"))


if not IN_BLENDER:
    if __name__ == "__main__":
        post(parse_args(sys.argv[1:]))
    sys.exit(0)

# ---------------------------------------------------------------------------------------------
#  Blender: материалы, примитивы
# ---------------------------------------------------------------------------------------------
import bmesh
from mathutils import Vector, noise
from math import radians, sin, cos, pi, sqrt


def lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hexc(h):
    h = h.lstrip("#")
    return tuple(lin(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (1.0,)


# p: (hex, roughness, metallic) | e: emission | s: stripes (c1, c2, axis, period, frac) | g: grid
PAL = {
    "white": ("p", "#f4f0e6", .6), "plaster": ("p", "#f7f3ea", .6), "cream": ("p", "#efdfb4", .7),
    "yellow_wall": ("p", "#efd37c", .7), "stone": ("p", "#a49d91", .85), "stone_light": ("p", "#d6cfc0", .8),
    "granite": ("p", "#a0968c", .9), "carve": ("p", "#43271c", .9), "brick": ("p", "#b6432a", .8),
    "brick_dark": ("p", "#8e3322", .8), "brick_light": ("p", "#c9683f", .8), "terracotta": ("p", "#d0582c", .6),
    "roof_red": ("p", "#a4402a", .7), "roof_dark": ("p", "#3b3f4a", .55), "roof_green": ("p", "#3d8a68", .45),
    "copper": ("p", "#5fb091", .45), "slate": ("p", "#56637a", .5), "shingle": ("p", "#6e4530", .8),
    "thatch": ("p", "#d4a64e", .9), "wood": ("p", "#8d5a32", .8), "wood_light": ("p", "#c08a52", .8),
    "wood_dark": ("p", "#4e3020", .8), "earth": ("p", "#9e7d4e", .95), "sand": ("p", "#dcc48c", .95),
    "grass": ("p", "#7fa64e", .9), "grass_dark": ("p", "#5c8a3c", .9), "forest_floor": ("p", "#76703e", .95),
    "spruce": ("p", "#2a5c3c", .85), "spruce_light": ("p", "#3a7448", .85), "leaf": ("p", "#62a044", .85),
    "water": ("p", "#2a6db6", .12), "water_light": ("p", "#62b0d6", .2), "foam": ("p", "#f4f8fa", .5),
    "smoke": ("p", "#e6e4df", .9), "steam": ("p", "#fbfbf8", .9), "gold": ("p", "#f0b92a", .3, .85),
    "brass": ("p", "#d8a640", .35, .7), "iron": ("p", "#38383c", .5, .4), "steel": ("p", "#a4aeb8", .35, .6),
    "steel_dark": ("p", "#4b5058", .45, .4), "chrome": ("p", "#dfe5ea", .18, 1.0), "dark": ("p", "#2a2a2e", .6),
    "window": ("p", "#24385a", .25), "glass": ("p", "#5aa0d8", .1, .15), "red": ("p", "#d4302a", .5),
    "tire": ("p", "#2a2a2a", .85), "belaz_yellow": ("p", "#f6c21c", .4), "belaz_green": ("p", "#2f8a3e", .4),
    "loco_black": ("p", "#25262b", .35), "car_green": ("p", "#3d8450", .5), "tram_red": ("p", "#d0302a", .4),
    "concrete": ("p", "#cfcac0", .85), "concrete_light": ("p", "#e2dfd8", .85), "paving": ("p", "#cdc8bf", .9),
    "gravel": ("p", "#8f857a", .95), "salt": ("p", "#f4e6e2", .75), "salt_pink": ("p", "#e8b9b4", .75),
    "frame_blue": ("p", "#2e63b4", .5), "npp_blue": ("p", "#2a6ac4", .45), "clockface": ("p", "#fbf6e6", .5),
    "metro_red": ("p", "#e0221c", .35), "glass_a": ("p", "#1f6688", .12, .25), "glass_b": ("p", "#58a8c8", .12, .25),
    "tglass1": ("p", "#56a6e0", .1, .2), "tglass2": ("p", "#2f62b0", .1, .2), "tglass3": ("p", "#38b0a2", .1, .2),
    "orange": ("p", "#f08a24", .5), "teal": ("p", "#2fa0a0", .5), "black": ("p", "#151517", .6),
    "fire": ("e", "#ff7a18", 9.0), "fire_core": ("e", "#ffd94a", 12.0), "spark": ("e", "#5fd0ff", 4.0),
    "lamp": ("e", "#fff0a8", 6.0), "redlight": ("e", "#ff3020", 8.0), "glow_win": ("e", "#ffd27a", 2.5),
    "logs": ("s", "#9a6236", "#5a3820", "Z", .36, .2, .85), "logs_v": ("s", "#946036", "#6a4226", "X", .5, .3, .85),
    "planks": ("s", "#a06c3c", "#5e3c22", "X", .3, .15, .85), "sail": ("s", "#d8302a", "#f4ecd8", "X", .62, .5, .8),
    "awning": ("s", "#d43a2c", "#f6efe0", "X", .38, .5, .8), "coil": ("s", "#d0843c", "#5a3418", "Z", .14, .3, .4),
    "solar": ("g", "#1c3d86", "#9ab8e2", .36, .1, .25), "redwhite": ("s", "#d8302a", "#f4f0e8", "Z", 1.2, .5, .6),
}
MATS = {}


def _inp(node, names):
    for n in names:
        if n in node.inputs:
            return node.inputs[n]
    return None


def mat(key):
    if key in MATS:
        return MATS[key]
    spec = PAL[key]
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    kind = spec[0]
    _inp(b, ["Specular IOR Level", "Specular"]).default_value = 0.35
    if kind == "p":
        b.inputs["Base Color"].default_value = hexc(spec[1])
        b.inputs["Roughness"].default_value = spec[2]
        b.inputs["Metallic"].default_value = spec[3] if len(spec) > 3 else 0.0
    elif kind == "e":
        b.inputs["Base Color"].default_value = hexc(spec[1])
        _inp(b, ["Emission Color", "Emission"]).default_value = hexc(spec[1])
        b.inputs["Emission Strength"].default_value = spec[2]
    else:
        tc = nt.nodes.new("ShaderNodeTexCoord")
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(tc.outputs["Object"], sep.inputs[0])

        def stripe(axis, period, frac):
            mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"; mul.inputs[1].default_value = 1.0 / period
            nt.links.new(sep.outputs[axis], mul.inputs[0])
            fr = nt.nodes.new("ShaderNodeMath"); fr.operation = "FRACT"; nt.links.new(mul.outputs[0], fr.inputs[0])
            lt = nt.nodes.new("ShaderNodeMath"); lt.operation = "LESS_THAN"; lt.inputs[1].default_value = frac
            nt.links.new(fr.outputs[0], lt.inputs[0])
            return lt.outputs[0]
        if kind == "s":
            _, c1, c2, axis, period, frac, rough = spec
            fac = stripe(axis, period, frac)
        else:
            _, c1, c2, period, frac, rough = spec
            sx, sy = stripe("X", period, frac), stripe("Y", period * 1.6, frac * 1.3)
            mx = nt.nodes.new("ShaderNodeMath"); mx.operation = "MAXIMUM"
            nt.links.new(sx, mx.inputs[0]); nt.links.new(sy, mx.inputs[1])
            fac = mx.outputs[0]
        mix = nt.nodes.new("ShaderNodeMixRGB")
        mix.inputs["Color1"].default_value = hexc(c1)
        mix.inputs["Color2"].default_value = hexc(c2)
        nt.links.new(fac, mix.inputs["Fac"])
        nt.links.new(mix.outputs["Color"], b.inputs["Base Color"])
        b.inputs["Roughness"].default_value = rough
    MATS[key] = m
    return m


PARENT = [None]
COLL = [None]


def mk(name, verts, faces, mkey, loc=(0, 0, 0), rot=(0, 0, 0), scl=1.0, bevel=0.0, smooth=False,
       mats=None, fmi=None):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces])
    me.update(calc_edges=True)
    o = bpy.data.objects.new(name, me)
    COLL[0].objects.link(o)
    o.parent = PARENT[-1]
    o.location = loc
    o.rotation_euler = [radians(a) for a in rot]
    o.scale = scl if isinstance(scl, (tuple, list)) else (scl, scl, scl)
    o.pass_index = 1
    me.materials.append(mat(mkey))
    if mats:
        for k in mats:
            me.materials.append(mat(k))
        if fmi:
            me.polygons.foreach_set("material_index", fmi)
    if smooth:
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    me.update()
    if bevel > 0:
        md = o.modifiers.new("bevel", "BEVEL")
        md.width = bevel; md.segments = 1; md.limit_method = "ANGLE"; md.angle_limit = radians(35)
    return o


class grp:
    """Подсборка с собственным положением/поворотом (поворот в градусах)."""
    def __init__(self, loc=(0, 0, 0), rot=(0, 0, 0), scl=1.0):
        e = bpy.data.objects.new("grp", None)
        COLL[0].objects.link(e)
        e.parent = PARENT[-1]
        e.location = loc
        e.rotation_euler = [radians(a) for a in rot]
        e.scale = (scl, scl, scl)
        self.e = e

    def __enter__(self):
        PARENT.append(self.e)
        return self.e

    def __exit__(self, *a):
        PARENT.pop()


# ---------- генераторы геометрии ----------
BOXF = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]


def g_box(sx, sy, sz):
    hx, hy = sx / 2.0, sy / 2.0
    return [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0),
            (-hx, -hy, sz), (hx, -hy, sz), (hx, hy, sz), (-hx, hy, sz)], BOXF


def g_lathe(profile, segs=16, phase=0.0, cap0=True, cap1=True):
    verts, rings = [], []
    ph = radians(phase)
    for (r, z) in profile:
        if r < 1e-6:
            rings.append([len(verts)]); verts.append((0, 0, z))
        else:
            idx = []
            for i in range(segs):
                a = ph + 2 * pi * i / segs
                idx.append(len(verts)); verts.append((r * cos(a), r * sin(a), z))
            rings.append(idx)
    faces = []
    for k in range(len(rings) - 1):
        A, B = rings[k], rings[k + 1]
        if len(A) == 1 and len(B) == 1:
            continue
        for i in range(segs):
            j = (i + 1) % segs
            if len(A) == 1: faces.append((A[0], B[i], B[j]))
            elif len(B) == 1: faces.append((A[i], A[j], B[0]))
            else: faces.append((A[i], A[j], B[j], B[i]))
    if cap0 and len(rings[0]) > 1: faces.append(tuple(reversed(rings[0])))
    if cap1 and len(rings[-1]) > 1: faces.append(tuple(rings[-1]))
    return verts, faces


def g_prism(sx, sy, h, ends=True):
    """Двускатная крыша, конёк вдоль X (ends=False — без фронтонов, со свесом-толщиной)."""
    hx, hy = sx / 2.0, sy / 2.0
    v = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0), (-hx, 0, h), (hx, 0, h)]
    if ends:
        return v, [(0, 3, 2, 1), (0, 1, 5, 4), (2, 3, 4, 5), (1, 2, 5), (3, 0, 4)]
    t = 0.14
    v += [(x, y, z - t) for (x, y, z) in v]
    return v, [(0, 1, 5, 4), (2, 3, 4, 5), (6, 10, 11, 7), (8, 11, 10, 9), (0, 6, 7, 1), (2, 8, 9, 3),
               (0, 4, 10, 6), (3, 9, 10, 4), (1, 7, 11, 5), (2, 5, 11, 8)]


def g_hip(sx, sy, h):
    """Вальмовая крыша (конёк вдоль X)."""
    hx, hy = sx / 2.0, sy / 2.0
    rx = max(0.02, hx - hy)
    v = [(-hx, -hy, 0), (hx, -hy, 0), (hx, hy, 0), (-hx, hy, 0), (-rx, 0, h), (rx, 0, h)]
    return v, [(0, 3, 2, 1), (0, 1, 5, 4), (2, 3, 4, 5), (1, 2, 5), (3, 0, 4)]


def g_barrel(L, r, segs=10):
    v = []
    for s in (-L / 2.0, L / 2.0):
        for i in range(segs + 1):
            a = pi * i / segs
            v.append((s, r * cos(a), r * sin(a)))
    n = segs + 1
    f = [(i, i + 1, n + i + 1, n + i) for i in range(segs)]
    f.append(tuple(range(n))[::-1]); f.append(tuple(range(n, 2 * n)))
    f.append((0, n, 2 * n - 1, n - 1))
    return v, f


def g_beam(p1, p2, t, t2=None):
    p1, p2 = Vector(p1), Vector(p2)
    t2 = t if t2 is None else t2
    d = (p2 - p1).normalized()
    up = Vector((0, 0, 1)) if abs(d.z) < 0.95 else Vector((1, 0, 0))
    a = d.cross(up).normalized() * (t / 2.0)
    b = d.cross(a).normalized() * (t2 / 2.0)
    v = [p1 - a - b, p1 + a - b, p1 + a + b, p1 - a + b, p2 - a - b, p2 + a - b, p2 + a + b, p2 - a + b]
    return v, BOXF


def g_ico(r, sub=2):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=r)
    bm.verts.index_update()
    v = [tuple(x.co) for x in bm.verts]
    f = [tuple(x.index for x in fc.verts) for fc in bm.faces]
    bm.free()
    return v, f


def g_torus(R, r, segs=20, segs2=8):
    v, f = [], []
    for i in range(segs):
        a = 2 * pi * i / segs
        for j in range(segs2):
            b = 2 * pi * j / segs2
            rr = R + r * cos(b)
            v.append((rr * cos(a), rr * sin(a), r * sin(b)))
    for i in range(segs):
        for j in range(segs2):
            i2, j2 = (i + 1) % segs, (j + 1) % segs2
            f.append((i * segs2 + j, i2 * segs2 + j, i2 * segs2 + j2, i * segs2 + j2))
    return v, f


def g_disc(rx, ry, h, n=36, amp=0.0, seed=0):
    """Неровный плоский «блин» (вода, площадки)."""
    v = []
    for i in range(n):
        a = 2 * pi * i / n
        k = 1.0 + amp * noise.noise(Vector((cos(a) * 1.7 + seed, sin(a) * 1.7, seed * 0.37)))
        v.append((rx * k * cos(a), ry * k * sin(a), 0))
    v += [(x, y, h) for (x, y, _) in v]
    f = [tuple(range(n))[::-1], tuple(range(n, 2 * n))]
    f += [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    return v, f


def g_loft(sections, closed=False):
    v, f = [], []
    m = len(sections[0])
    for s in sections:
        v.extend(s)
    for k in range(len(sections) - 1):
        a, b = k * m, (k + 1) * m
        for i in range(m if closed else m - 1):
            j = (i + 1) % m
            f.append((a + i, a + j, b + j, b + i))
    return v, f


# ---------- удобные обёртки ----------
def box(sx, sy, sz, loc, m, rot=(0, 0, 0), bevel=0.04):
    return mk("box", *g_box(sx, sy, sz), m, loc, rot, bevel=bevel)


def cyl(r, h, loc, m, segs=12, rot=(0, 0, 0), phase=0.0, smooth=False, scl=1.0):
    return mk("cyl", *g_lathe([(r, 0), (r, h)], segs, phase), m, loc, rot, scl=scl, smooth=smooth)


def hcyl(r, L, loc, m, axis="x", segs=12, smooth=False):
    """Горизонтальный цилиндр с центром в loc."""
    v, f = g_lathe([(r, -L / 2.0), (r, L / 2.0)], segs)
    rot = (0, 90, 0) if axis == "x" else (90, 0, 0)
    return mk("hcyl", v, f, m, loc, rot, smooth=smooth)


def cone(r, h, loc, m, segs=12, r2=0.0, rot=(0, 0, 0), phase=0.0, smooth=False):
    return mk("cone", *g_lathe([(r, 0), (r2, h)], segs, phase), m, loc, rot, smooth=smooth)


def lathe(profile, loc, m, segs=16, rot=(0, 0, 0), smooth=False, cap0=True, cap1=True, phase=0.0, scl=1.0):
    return mk("lathe", *g_lathe(profile, segs, phase, cap0, cap1), m, loc, rot, scl=scl, smooth=smooth)


def pyramid(side, h, loc, m, rot=(0, 0, 0)):
    return mk("pyr", *g_lathe([(side / sqrt(2), 0), (0, h)], 4, 45), m, loc, rot)


def prism(sx, sy, h, loc, m, rot=(0, 0, 0)):
    return mk("roof", *g_prism(sx, sy, h), m, loc, rot)


def hip(sx, sy, h, loc, m, rot=(0, 0, 0)):
    return mk("hip", *g_hip(sx, sy, h), m, loc, rot)


def beam(p1, p2, t, m, t2=None):
    return mk("beam", *g_beam(p1, p2, t, t2), m)


def ico(r, loc, m, sub=2, scl=1.0, smooth=False):
    return mk("ico", *g_ico(r, sub), m, loc, scl=scl, smooth=smooth)


def puff(x, y, z, r, m="smoke"):
    return ico(r, (x, y, z), m, sub=2)


def win(x, yf, z, w=0.4, h=0.6, m="window", frame="white"):
    box(w, 0.08, h, (x, yf, z), m, bevel=0)
    if frame:
        box(w + 0.14, 0.05, h + 0.14, (x, yf + 0.012, z - 0.07), frame, bevel=0)


def swin(xf, y, z, w=0.4, h=0.6, m="window", frame="white"):
    """Окно на восточной (+X) стене."""
    box(0.08, w, h, (xf, y, z), m, bevel=0)
    if frame:
        box(0.05, w + 0.14, h + 0.14, (xf - 0.012, y, z - 0.07), frame, bevel=0)


def spruce(x, y, h, m="spruce"):
    cyl(0.1 + h * 0.02, h * 0.3, (x, y, 0), "wood_dark", segs=6)
    for k in range(3):
        cone(h * (0.3 - 0.075 * k), h * (0.42 - 0.04 * k), (x, y, h * (0.16 + 0.24 * k)), m, segs=7,
             phase=random.uniform(0, 50))


def tree(x, y, h, m="leaf"):
    cyl(0.09, h * 0.5, (x, y, 0), "wood_dark", segs=6)
    ico(h * 0.32, (x, y, h * 0.62), m, sub=1, scl=(1, 1, 1.12))


def chimney_smoke(x, y, z, k=1.0, m="smoke"):
    puff(x, y, z + 0.35 * k, 0.36 * k, m)
    puff(x + 0.35 * k, y + 0.3 * k, z + 1.05 * k, 0.5 * k, m)
    puff(x + 0.95 * k, y + 0.65 * k, z + 1.75 * k, 0.62 * k, m)


# ---------------------------------------------------------------------------------------------
#  Модели (единица = 10 px при масштабе 1; фасад смотрит на юг, -Y; восток +X)
# ---------------------------------------------------------------------------------------------
def m_fort():
    cyl(1.0, 0.12, (0, 0, 0), "earth", segs=32, scl=(4.75, 3.1, 1))
    Rx, Ry, n = 4.5, 2.88, 46
    for k in range(n):
        a = 2 * pi * k / n
        if abs(((a - 1.5 * pi + pi) % (2 * pi)) - pi) < 0.2:
            continue
        x, y = Rx * cos(a), Ry * sin(a)
        h = 1.6 + random.uniform(-0.12, 0.12)
        cyl(0.26, h, (x, y, 0), "logs_v", segs=6)
        cone(0.26, 0.42, (x, y, h), "wood_light", segs=6)
    # надвратная башня
    box(1.8, 1.2, 2.6, (0, -2.5, 0), "logs", bevel=0.05)
    box(1.0, 0.1, 1.4, (0, -3.12, 0), "dark", bevel=0)
    box(2.1, 1.5, 0.18, (0, -2.5, 2.6), "wood_dark")
    pyramid(2.3, 1.2, (0, -2.5, 2.78), "roof_red")
    # главная башня
    tx, ty = 0.6, 0.7
    box(2.2, 2.2, 3.5, (tx, ty, 0), "logs", bevel=0.05)
    box(2.9, 2.9, 1.0, (tx, ty, 3.5), "logs", bevel=0.05)
    for dx in (-0.8, 0, 0.8):
        box(0.28, 0.08, 0.45, (tx + dx, ty - 1.47, 3.8), "dark", bevel=0)
    box(0.7, 0.08, 1.2, (tx, ty - 1.12, 0), "dark", bevel=0)
    pyramid(3.5, 2.2, (tx, ty, 4.5), "roof_red")
    cyl(0.05, 1.5, (tx, ty, 6.3), "wood_dark", segs=6)
    mk("pennant", [(0, 0, 0), (0, 0, -0.5), (1.1, 0, -0.25)], [(0, 1, 2)], "red", (tx, ty, 7.75))
    # избы
    box(2.0, 1.5, 1.3, (-2.5, 0.8, 0), "logs", bevel=0.05)
    prism(2.4, 1.9, 1.0, (-2.5, 0.8, 1.3), "thatch")
    box(0.45, 0.08, 0.8, (-2.2, 0.03, 0), "dark", bevel=0)
    box(1.6, 1.3, 1.1, (2.7, -0.9, 0), "logs", bevel=0.05)
    prism(1.95, 1.7, 0.9, (2.7, -0.9, 1.1), "thatch")
    box(1.4, 1.1, 0.9, (-2.6, -1.4, 0), "logs", bevel=0.05)
    prism(1.7, 1.45, 0.8, (-2.6, -1.4, 0.9), "thatch")


def m_church():
    W = "plaster"
    box(3.2, 3.2, 4.2, (0, 0, 0), W, bevel=0.05)
    box(1.5, 2.6, 3.1, (-2.3, 0, 0), W, bevel=0.05)
    cyl(1.15, 3.2, (1.6, 0, 0), W, segs=14)
    cone(1.2, 0.7, (1.6, 0, 3.2), "roof_green", segs=14)
    mk("vault", *g_barrel(3.4, 1.6, 10), "roof_green", (0, 0, 4.2))
    mk("vault", *g_barrel(3.4, 1.6, 10), "roof_green", (0, 0, 4.2), (0, 0, 90))
    prism(2.8, 1.75, 0.85, (-2.3, 0, 3.1), "roof_green", rot=(0, 0, 90))
    # лопатки и окна на южном фасаде
    for x in (-1.6, -0.55, 0.55, 1.6):
        box(0.26, 0.12, 4.2, (x, -1.6, 0), W, bevel=0.02)
    for x in (-1.08, 0, 1.08):
        win(x, -1.62, 2.3, 0.3, 1.2, "window", None)
    box(0.8, 0.1, 1.6, (-2.3, -1.3, 0), "dark", bevel=0)
    box(1.05, 0.08, 1.75, (-2.3, -1.27, 0), "gold", bevel=0)
    # барабан, луковица, крест
    cyl(0.85, 2.2, (0, 0, 5.2), W, segs=12, phase=15)
    for a in range(0, 360, 60):
        x, y = 0.86 * cos(radians(a - 90)), 0.86 * sin(radians(a - 90))
        box(0.18, 0.06, 0.9, (x, y, 5.8), "window", rot=(0, 0, a), bevel=0)
    cyl(0.98, 0.18, (0, 0, 7.35), W, segs=12)
    onion = [(0.88, 0), (1.18, 0.38), (1.26, 0.7), (1.08, 1.1), (0.68, 1.5), (0.3, 1.82), (0.11, 2.1), (0.07, 2.3), (0, 2.36)]
    lathe(onion, (0, 0, 7.5), "gold", segs=16, smooth=True)
    box(0.12, 0.12, 1.45, (0, 0, 9.75), "gold", bevel=0)
    box(0.8, 0.12, 0.12, (0, 0, 10.65), "gold", bevel=0)
    box(0.5, 0.11, 0.1, (0, 0, 10.2), "gold", rot=(0, -22, 0), bevel=0)


def m_stone():
    v, f = g_ico(1.0, 3)
    nv = []
    for (x, y, z) in v:
        p = Vector((x, y, z))
        d = 1 + 0.16 * noise.noise(p * 1.6 + Vector((3.1, 1.2, 0.5))) + 0.07 * noise.noise(p * 4.2 + Vector((7, 2, 1)))
        p = p * d
        p = Vector((p.x * 3.0, p.y * 1.95, p.z * 2.3 - 0.3))
        top = 1.45 + 0.25 * p.y          # верх наклонён к зрителю
        if p.z > top:
            p.z = top + (p.z - top) * 0.08
        nv.append(p)
    mk("boulder", nv, f, "granite")
    with grp((0, 0, 1.43), (14, 0, 0)):
        box(0.56, 2.25, 0.16, (0, -0.15, 0), "carve", bevel=0.02)
        box(1.95, 0.56, 0.16, (0, 0.38, 0), "carve", bevel=0.02)
    for k in range(18):
        a = random.uniform(0, 2 * pi)
        x, y = 3.05 * cos(a), 2.0 * sin(a)
        if y < 1.0:
            for j in range(3):
                cone(0.16, random.uniform(0.35, 0.6), (x + random.uniform(-.25, .25), y + random.uniform(-.15, .15), 0),
                     random.choice(["grass", "grass_dark"]), segs=5)
    for (x, y, r) in ((-3.2, -0.6, 0.35), (2.9, -1.1, 0.28), (3.4, 0.3, 0.22)):
        ico(r, (x, y, r * 0.4), "granite", sub=1, scl=(1.3, 1, 0.8))


def m_pier():
    mk("shallow", *g_disc(6.4, 2.95, 0.05, 40, 0.07, 1), "water_light")
    mk("deep", *g_disc(5.7, 2.45, 0.08, 40, 0.07, 2), "water", (0.3, 0, 0))
    mk("bank", *g_disc(1.5, 1.6, 0.3, 20, 0.1, 3), "sand", (-5.7, 0.7, 0))
    mk("bankg", *g_disc(1.1, 1.3, 0.36, 20, 0.12, 4), "grass", (-6.0, 1.0, 0))
    # причал
    box(5.6, 1.3, 0.14, (-2.9, 0.75, 0.5), "planks", bevel=0.02)
    for x in (-4.9, -3.7, -2.5, -1.3, -0.25):
        for y in (0.18, 1.32):
            cyl(0.11, 0.72, (x, y, -0.05), "wood_dark", segs=6)
    cyl(0.27, 0.6, (-4.4, 1.05, 0.64), "wood", segs=10)
    cyl(0.27, 0.6, (-3.85, 1.1, 0.64), "wood", segs=10)
    box(0.55, 0.55, 0.5, (-2.9, 1.0, 0.64), "wood_light")
    # ладья
    with grp((0.9, -0.85, -0.12), (0, 0, 3)):
        L, Wd = 7.0, 1.9
        secs = []
        for i in range(15):
            t = -1 + 2 * i / 14.0
            x = t * L / 2
            w = Wd / 2 * max(0.0, 1 - abs(t) ** 2.4) ** 0.55
            zt = 0.62 + 0.6 * abs(t) ** 3
            kz = 0.05 + 0.4 * abs(t) ** 3
            secs.append([(x, -w, zt), (x, -0.82 * w, 0.4 * zt + 0.6 * kz), (x, 0, kz),
                         (x, 0.82 * w, 0.4 * zt + 0.6 * kz), (x, w, zt)])
        mk("hull", *g_loft(secs), "wood", smooth=False)
        beam((L / 2 - 0.15, 0, 1.1), (L / 2 + 0.3, 0, 1.85), 0.16, "wood_dark")
        ico(0.2, (L / 2 + 0.35, 0, 1.92), "red", sub=1)
        beam((-L / 2 + 0.15, 0, 1.1), (-L / 2 - 0.25, 0, 1.7), 0.16, "wood_dark")
        for k, x in enumerate((-2.1, -1.0, 0.1, 1.2, 2.3)):
            t = 2 * x / L
            w = Wd / 2 * max(0.0, 1 - abs(t) ** 2.4) ** 0.55
            hcyl(0.26, 0.07, (x, -w - 0.02, 0.62), "red" if k % 2 == 0 else "cream", axis="y", segs=10)
        cyl(0.08, 3.9, (0.0, 0, 0.1), "wood_dark", segs=6)
        beam((-1.55, -0.05, 3.75), (1.55, -0.05, 3.75), 0.11, "wood_dark")
        box(2.9, 0.05, 2.35, (0, -0.06, 1.38), "sail", bevel=0)


def gable(w, h, loc, m, rot=(0, 0, 0)):
    """Треугольный фронтон в плоскости XZ (смотрит на юг)."""
    return mk("gable", [(-w / 2, 0, 0), (w / 2, 0, 0), (0, 0, h), (-w / 2, 0.12, 0), (w / 2, 0.12, 0), (0, 0.12, h)],
              [(0, 1, 2), (3, 5, 4), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)], m, loc, rot)


def m_workshop():
    hx, hy = -0.7, 0.5
    box(3.0, 2.8, 2.0, (hx, hy, 0), "logs", bevel=0.05)
    mk("roof", *g_prism(3.5, 3.7, 1.9, False), "roof_red", (hx, hy, 2.0), (0, 0, 90))
    gable(3.0, 1.75, (hx, hy - 1.42, 2.0), "logs")
    win(hx, hy - 1.44, 2.45, 0.4, 0.45, "glow_win", "wood_dark")
    box(0.7, 0.08, 1.3, (hx - 0.75, hy - 1.42, 0), "dark", bevel=0)
    win(hx + 0.5, hy - 1.42, 0.8, 0.5, 0.5, "glow_win", "wood_dark")
    box(0.65, 0.65, 2.3, (hx + 0.9, hy + 0.9, 1.9), "stone", bevel=0.05)
    box(0.8, 0.8, 0.15, (hx + 0.9, hy + 0.9, 4.2), "dark", bevel=0.02)
    chimney_smoke(hx + 0.9, hy + 0.9, 4.3, 1.15)
    # навес у восточной стены
    for y in (-1.6, 0.9):
        cyl(0.07, 1.6, (2.6, y, 0), "wood_dark", segs=6)
    box(1.95, 2.9, 0.07, (1.75, -0.3, 1.78), "awning", rot=(0, -14, 0), bevel=0)
    cyl(0.2, 0.45, (1.6, -0.9, 0), "wood", segs=8)
    box(0.55, 0.24, 0.24, (1.6, -0.9, 0.45), "iron", bevel=0.03)
    box(0.6, 1.2, 0.6, (2.0, 0.3, 0), "wood_light")
    cyl(0.28, 0.62, (-2.5, -1.3, 0), "wood", segs=10)
    cyl(0.28, 0.62, (-2.65, -0.6, 0), "wood", segs=10)
    for k in range(3):
        hcyl(0.17, 1.4, (-2.75 + 0.0, 1.0, 0.17 + 0.3 * k), "wood_light", axis="y", segs=8)


def m_mill():
    box(11.0, 1.8, 0.1, (0, -1.75, 0), "water", bevel=0)
    for y in (-0.82, -2.68):
        box(11.0, 0.14, 0.26, (0, y, 0), "wood_dark", bevel=0.02)
    hx, hy = -0.9, 0.7
    box(3.1, 3.0, 2.4, (hx, hy, 0), "logs", bevel=0.05)
    mk("roof", *g_prism(3.6, 3.7, 2.4, False), "thatch", (hx, hy, 2.4), (0, 0, 90))
    gable(3.1, 2.2, (hx, hy - 1.52, 2.4), "logs")
    win(hx, hy - 1.54, 2.9, 0.4, 0.45, "window", "wood_dark")
    box(0.7, 0.08, 1.3, (hx - 0.7, hy - 1.52, 0), "dark", bevel=0)
    with grp((1.15, -1.6, 1.35), (90, 0, 0)):
        R = 1.95
        for z in (-0.4, 0.4):
            mk("rim", *g_torus(R, 0.1, 24, 6), "wood_dark", (0, 0, z))
            for k in range(8):
                a = 2 * pi * k / 8
                beam((0, 0, z), (R * cos(a), R * sin(a), z), 0.11, "wood")
        for k in range(16):
            a = 2 * pi * k / 16
            box(0.65, 0.07, 0.95, ((R + 0.1) * cos(a), (R + 0.1) * sin(a), -0.48), "wood_light",
                rot=(0, 0, math.degrees(a)), bevel=0)
        cyl(0.3, 1.0, (0, 0, -0.5), "wood_dark", segs=8)
        cyl(0.1, 1.5, (0, 0, -1.2), "wood_dark", segs=6)
    for dx in (-0.9, 0.0, 0.9):
        puff(1.15 + dx, -1.55, 0.12, 0.32, "foam")
    hcyl(0.6, 0.22, (-3.0, -0.55, 0.6), "stone", axis="y", segs=14)
    for (x, y) in ((-3.3, 0.6), (-3.0, 1.3)):
        ico(0.3, (x, y, 0.25), "cream", sub=1, scl=(1, 1, 1.2))


def m_tower():
    mk("mound", *g_disc(2.75, 2.5, 0.22, 28, 0.06, 5), "grass")
    cyl(2.2, 0.55, (0, 0, 0), "stone", segs=16, phase=11.25)
    cyl(2.0, 9.0, (0, 0, 0.5), "brick", segs=16, phase=11.25)
    for z in (2.6, 5.4):
        for a in (-150, -120, -90, -60, -30, 0):
            ra = radians(a)
            box(0.5, 0.12, 1.7, (1.99 * cos(ra), 1.99 * sin(ra), z), "plaster", rot=(0, 0, a + 90), bevel=0)
            box(0.16, 0.14, 0.9, (2.0 * cos(ra), 2.0 * sin(ra), z + 0.4), "dark", rot=(0, 0, a + 90), bevel=0)
    cyl(2.04, 0.14, (0, 0, 8.0), "plaster", segs=16, phase=11.25)
    cyl(2.3, 0.6, (0, 0, 9.4), "brick", segs=16, phase=11.25)
    cyl(2.33, 0.16, (0, 0, 9.3), "plaster", segs=16, phase=11.25)
    for k in range(16):
        a = 22.5 * k
        ra = radians(a)
        box(0.6, 0.42, 0.62, (2.08 * cos(ra), 2.08 * sin(ra), 10.0), "brick", rot=(0, 0, a + 90), bevel=0.03)
        box(0.3, 0.42, 0.32, (2.08 * cos(ra), 2.08 * sin(ra), 10.62), "brick", rot=(0, 0, a + 90), bevel=0.02)
    cone(1.95, 1.3, (0, 0, 9.9), "roof_red", segs=16, phase=11.25)
    cyl(0.06, 0.9, (0, 0, 11.2), "iron", segs=6)
    box(0.9, 0.12, 1.3, (0, -2.12, 0.5), "dark", bevel=0)


def m_castle():
    X, Y, H = 4.0, 2.2, 2.6
    box(7.6, 4.2, 0.05, (0, 0, 0), "sand", bevel=0)
    for (sx, sy, x, y) in ((2 * X, 0.55, 0, -Y), (2 * X, 0.55, 0, Y), (0.55, 2 * Y, -X, 0), (0.55, 2 * Y, X, 0)):
        box(sx, sy, H, (x, y, 0), "brick", bevel=0.03)
        box(sx + 0.06, sy + 0.06, 0.16, (x, y, H - 0.5), "plaster", bevel=0)
    for k in range(13):
        x = -X + 0.6 + k * (2 * X - 1.2) / 12
        box(0.32, 0.55, 0.38, (x, -Y, H), "brick", bevel=0.02)
        box(0.32, 0.55, 0.38, (x, Y, H), "brick", bevel=0.02)
    for k in range(6):
        y = -Y + 0.7 + k * (2 * Y - 1.4) / 5
        box(0.55, 0.32, 0.38, (-X, y, H), "brick", bevel=0.02)
        box(0.55, 0.32, 0.38, (X, y, H), "brick", bevel=0.02)
    # дворец
    box(7.0, 1.9, 3.5, (0, 1.4, 0), "plaster", bevel=0.04)
    prism(7.4, 2.3, 1.5, (0, 1.4, 3.5), "terracotta")
    for x in (-2.6, -1.3, 1.3, 2.6):
        win(x, 0.44, 2.3, 0.36, 0.6, "window", None)
    # угловые башни
    for (x, y) in ((-X, -Y), (X, -Y), (-X, Y), (X, Y)):
        box(1.8, 1.8, 4.6, (x, y, 0), "brick", bevel=0.04)
        for dz in (1.4, 3.0):
            box(0.4, 0.06, 0.9, (x - 0.45, y - 0.92, dz), "plaster", bevel=0)
            box(0.4, 0.06, 0.9, (x + 0.45, y - 0.92, dz), "plaster", bevel=0)
            box(0.06, 0.4, 0.9, (x + 0.92 * (1 if x > 0 else -1), y, dz), "plaster", bevel=0)
        cyl(1.02, 1.6, (x, y, 4.6), "plaster", segs=8, phase=22.5)
        for a in range(-90, 270, 45):
            box(0.22, 0.06, 0.7, (0.98 * cos(radians(a)) + x, 0.98 * sin(radians(a)) + y, 5.0), "brick_dark",
                rot=(0, 0, a + 90), bevel=0)
        cone(1.2, 2.2, (x, y, 6.2), "roof_dark", segs=8, phase=22.5)
        ico(0.14, (x, y, 8.45), "gold", sub=1)
    # надвратная башня
    box(2.2, 2.0, 5.6, (0, -Y, 0), "brick", bevel=0.04)
    box(1.0, 0.1, 1.7, (0, -Y - 1.02, 0), "dark", bevel=0)
    for dz in (2.2, 3.8):
        for dx in (-0.6, 0, 0.6):
            box(0.36, 0.06, 0.9, (dx, -Y - 1.02, dz), "plaster", bevel=0)
    box(1.7, 1.6, 1.4, (0, -Y, 5.6), "plaster", bevel=0.03)
    for dx in (-0.4, 0.4):
        box(0.24, 0.06, 0.7, (dx, -Y - 0.82, 5.9), "brick_dark", bevel=0)
    pyramid(2.3, 2.4, (0, -Y, 7.0), "roof_dark")
    ico(0.15, (0, -Y, 9.45), "gold", sub=1)


def m_press():
    bx, by = -0.6, 0.5
    box(3.0, 3.2, 3.3, (bx, by, 0), "cream", bevel=0.05)
    mk("roof", *g_prism(3.4, 3.3, 2.3, False), "roof_dark", (bx, by, 3.3), (0, 0, 90))
    fy = by - 1.62
    for k, (w, h) in enumerate(((3.1, 0.6), (2.35, 0.55), (1.6, 0.55), (0.85, 0.6))):
        z = 3.3 + sum(hh for (_, hh) in ((3.1, 0.6), (2.35, 0.55), (1.6, 0.55), (0.85, 0.6))[:k])
        box(w, 0.34, h, (bx, fy + 0.1, z), "cream", bevel=0.03)
        box(w + 0.08, 0.4, 0.08, (bx, fy + 0.1, z + h - 0.04), "stone_light", bevel=0)
    hcyl(0.26, 0.08, (bx, fy - 0.08, 4.05), "window", axis="y", segs=12)
    box(0.9, 0.1, 1.6, (bx - 0.6, fy, 0), "wood_dark", bevel=0)
    box(1.15, 0.35, 0.15, (bx - 0.6, fy - 0.15, 0), "stone_light", bevel=0.02)
    win(bx + 0.75, fy, 0.75, 0.45, 0.8)
    for x in (-0.9, 0.0, 0.9):
        win(bx + x, fy, 2.15, 0.45, 0.8)
    # кронштейн и большая книга-вывеска
    ex, ey = bx + 1.5, fy + 0.15
    beam((ex - 0.1, ey, 2.95), (ex + 2.0, ey, 2.95), 0.12, "iron")
    beam((ex, ey, 2.25), (ex + 1.0, ey, 2.95), 0.08, "iron")
    for dx in (0.55, 1.75):
        beam((ex + dx, ey, 2.95), (ex + dx, ey, 2.55), 0.05, "iron")
    with grp((ex + 1.15, ey, 1.35)):
        box(2.2, 0.12, 1.35, (0, 0, 0), "red", bevel=0.03)
        box(1.0, 0.09, 1.18, (-0.53, -0.08, 0.09), "plaster", rot=(0, 0, 8), bevel=0)
        box(1.0, 0.09, 1.18, (0.53, -0.08, 0.09), "plaster", rot=(0, 0, -8), bevel=0)
        box(0.08, 0.12, 1.2, (0, -0.12, 0.08), "red", bevel=0)
        for dz in (0.35, 0.55, 0.75, 0.95):
            box(0.66, 0.04, 0.06, (-0.55, -0.15, dz), "dark", bevel=0)
            box(0.66, 0.04, 0.06, (0.55, -0.15, dz), "dark", bevel=0)
    box(0.5, 0.4, 0.3, (bx - 2.0, fy + 0.2, 0), "plaster")
    box(0.45, 0.35, 0.25, (bx - 2.0, fy + 0.2, 0.3), "red")
    box(0.45, 0.35, 0.2, (bx - 2.0, fy + 0.75, 0), "frame_blue")


def m_townhall():
    box(5.6, 2.4, 3.2, (0, 0.3, 0), "plaster", bevel=0.05)
    for x in (-2.2, -1.32, 1.32, 2.2):
        box(0.6, 0.08, 1.35, (x, -0.92, 0.15), "window", bevel=0)
    box(0.8, 0.08, 1.5, (0, -0.92, 0), "wood_dark", bevel=0)
    for x in (-2.2, -1.32, -0.44, 0.44, 1.32, 2.2):
        win(x, -0.92, 2.0, 0.38, 0.75)
    box(5.8, 2.6, 0.2, (0, 0.3, 3.2), "cream", bevel=0.03)
    hip(5.9, 2.8, 1.4, (0, 0.3, 3.4), "terracotta")
    box(1.7, 1.7, 3.6, (0, 0.3, 3.2), "plaster", bevel=0.04)
    box(1.85, 1.85, 0.2, (0, 0.3, 6.8), "cream", bevel=0.03)
    hcyl(0.68, 0.06, (0, -0.58, 5.6), "gold", axis="y", segs=20)
    hcyl(0.58, 0.08, (0, -0.6, 5.6), "clockface", axis="y", segs=20)
    beam((0, -0.66, 5.6), (0, -0.66, 6.05), 0.07, "black")
    beam((0, -0.66, 5.6), (0.32, -0.66, 5.48), 0.07, "black")
    cyl(0.75, 1.3, (0, 0.3, 7.0), "plaster", segs=8, phase=22.5)
    for a in (-135, -90, -45, 0, 180):
        box(0.24, 0.06, 0.7, (0.72 * cos(radians(a)), 0.3 + 0.72 * sin(radians(a)), 7.3), "dark", rot=(0, 0, a + 90), bevel=0)
    cyl(0.86, 0.14, (0, 0.3, 8.3), "cream", segs=8, phase=22.5)
    dome = [(0.82, 0), (0.86, 0.3), (0.7, 0.75), (0.35, 1.05), (0.14, 1.2), (0, 1.25)]
    lathe(dome, (0, 0.3, 8.44), "roof_green", segs=16, smooth=True)
    cyl(0.16, 0.5, (0, 0.3, 9.6), "roof_green", segs=8)
    cone(0.1, 1.0, (0, 0.3, 10.1), "gold", segs=6)
    ico(0.13, (0, 0.3, 10.15), "gold", sub=1)


def m_college():
    box(8.4, 2.6, 3.6, (0, 0.2, 0), "yellow_wall", bevel=0.05)
    for x in (-3.65, 3.65):
        box(1.5, 2.9, 3.8, (x, 0.2, 0), "yellow_wall", bevel=0.05)
        hip(1.7, 3.1, 1.2, (x, 0.2, 3.8), "copper", rot=(0, 0, 90))
    box(8.6, 2.8, 0.2, (0, 0.2, 3.6), "plaster", bevel=0.03)
    hip(8.6, 2.9, 1.6, (0, 0.2, 3.8), "copper")
    for x in (-2.6, -2.0, 2.0, 2.6, -3.65, 3.65):
        for z in (0.6, 2.2):
            win(x, -1.12 if abs(x) < 3 else -1.27, z, 0.38, 0.8)
    # портик
    box(4.0, 1.2, 0.25, (0, -1.6, 0), "stone_light", bevel=0.03)
    for k in range(6):
        cyl(0.2, 3.1, (-1.5 + 0.6 * k, -1.9, 0.25), "plaster", segs=10)
    box(4.0, 1.25, 0.4, (0, -1.6, 3.3), "plaster", bevel=0.03)
    prism(1.5, 4.2, 1.1, (0, -1.55, 3.7), "plaster", rot=(0, 0, 90))
    box(0.8, 0.08, 1.7, (0, -1.12, 0.25), "wood_dark", bevel=0)
    # бельведер
    cyl(0.6, 0.9, (0, 0.2, 5.0), "plaster", segs=8, phase=22.5)
    lathe([(0.66, 0), (0.62, 0.35), (0.42, 0.68), (0.15, 0.85), (0, 0.88)], (0, 0.2, 5.9), "copper", segs=16, smooth=True)
    cone(0.07, 0.8, (0, 0.2, 6.7), "gold", segs=6)


def m_manufactory():
    box(8.0, 2.6, 2.2, (0, 0.2, 0), "brick_light", bevel=0.05)
    prism(8.4, 3.2, 1.25, (0, 0.2, 2.2), "slate")
    for k in range(8):
        x = -3.4 + k * 0.97
        if k != 4:
            win(x, -1.12, 0.8, 0.42, 0.85)
    box(1.0, 0.08, 1.5, (0.48, -1.12, 0), "wood_dark", bevel=0)
    box(1.9, 1.5, 1.6, (-3.0, -1.6, 0), "plaster", bevel=0.04)
    prism(2.2, 1.8, 0.8, (-3.0, -1.6, 1.6), "terracotta")
    box(0.5, 0.08, 1.0, (-3.0, -2.36, 0), "wood_dark", bevel=0)
    for (x, h) in ((-1.4, 6.6), (2.3, 5.7)):
        lathe([(0.62, 0), (0.44, h)], (x, 1.0, 0), "brick", segs=4, phase=45)
        lathe([(0.55, 0), (0.55, 0.32)], (x, 1.0, h - 0.15), "dark", segs=4, phase=45)
        chimney_smoke(x, 1.0, h + 0.2, 1.25)
    for k in range(3):
        box(0.5, 0.5, 0.45, (2.6 + 0.6 * k, -1.7, 0), "wood_light")
    box(0.5, 0.5, 0.45, (2.9, -1.7, 0.45), "wood_light")


def m_canal():
    for s in (-1, 1):
        box(12.4, 1.75, 0.85, (0, s * 2.05, 0), "earth", bevel=0.03)
        box(12.4, 1.75, 0.14, (0, s * 2.05, 0.85), "grass", bevel=0.03)
        box(12.4, 0.36, 1.05, (0, s * 1.2, 0), "stone", bevel=0.03)
    box(4.6, 2.1, 0.78, (-3.9, 0, 0), "water", bevel=0)
    box(4.0, 2.1, 0.52, (0.4, 0, 0), "water", bevel=0)
    box(3.8, 2.1, 0.28, (4.3, 0, 0), "water", bevel=0)
    for (x, wl) in ((-1.6, 0.78), (2.4, 0.52)):
        box(0.32, 2.1, 1.25, (x, 0, 0), "wood_dark", bevel=0.03)
        box(0.36, 2.1, 0.08, (x, 0, 1.25), "plaster", bevel=0)
        for s in (-1, 1):
            beam((x, s * 1.05, 1.3), (x - 2.1, s * 1.75, 1.42), 0.17, "plaster")
            box(0.2, 0.2, 0.2, (x - 2.1, s * 1.75, 1.32), "black", bevel=0)
    for dx in (0.25, 0.55):
        for dy in (-0.6, 0, 0.6):
            puff(2.4 + dx, dy, 0.32, 0.2, "foam")
    box(1.7, 1.25, 1.25, (-4.2, 2.3, 0.99), "plaster", bevel=0.04)
    prism(2.0, 1.55, 0.85, (-4.2, 2.3, 2.24), "terracotta")
    box(0.4, 0.08, 0.75, (-4.2, 1.64, 0.99), "wood_dark", bevel=0)
    for x in (-5.4, 0.6, 5.0):
        cyl(0.12, 0.3, (x, -1.55, 0.99), "black", segs=8)
    tree(3.4, 2.4, 2.6)
    tree(5.0, 2.2, 2.2)
    tree(-1.0, -2.4, 1.8)


def m_railway():
    box(13.2, 1.9, 0.22, (0, 0, 0), "gravel", bevel=0.05)
    for k in range(25):
        box(0.24, 1.55, 0.08, (-6.3 + k * 0.525, 0, 0.22), "wood_dark", bevel=0)
    for y in (-0.45, 0.45):
        box(13.2, 0.1, 0.13, (0, y, 0.3), "steel", bevel=0)
    # паровоз
    box(5.6, 1.25, 0.32, (-3.3, 0, 0.75), "black", bevel=0.02)
    for x in (-4.6, -3.45, -2.3):
        for y in (-0.7, 0.7):
            hcyl(0.56, 0.14, (x, y, 0.62), "red", axis="y", segs=14)
            hcyl(0.18, 0.18, (x, y, 0.62), "black", axis="y", segs=8)
    for y in (-0.7, 0.7):
        hcyl(0.32, 0.12, (-5.6, y, 0.38), "red", axis="y", segs=10)
    beam((-4.6, -0.82, 0.55), (-2.3, -0.82, 0.55), 0.1, "steel")
    hcyl(0.68, 3.5, (-4.0, 0, 1.72), "loco_black", axis="x", segs=14)
    for x in (-5.1, -4.1, -3.1):
        hcyl(0.7, 0.1, (x, 0, 1.72), "brass", axis="x", segs=14)
    hcyl(0.7, 0.45, (-5.85, 0, 1.72), "black", axis="x", segs=14)
    lathe([(0.22, 0), (0.2, 0.55), (0.42, 1.15), (0.46, 1.35), (0.36, 1.35)], (-5.45, 0, 2.2), "loco_black", segs=12)
    lathe([(0.32, 0), (0.3, 0.2), (0.2, 0.38), (0, 0.44)], (-3.85, 0, 2.3), "brass", segs=12, smooth=True)
    box(1.8, 1.45, 1.9, (-1.4, 0, 1.07), "car_green", bevel=0.04)
    box(2.1, 1.65, 0.14, (-1.4, 0, 2.97), "red", bevel=0.02)
    win(-1.4, -0.74, 2.0, 0.55, 0.55, "glow_win", "black")
    box(0.16, 1.55, 0.38, (-6.15, 0, 0.6), "red", bevel=0)
    mk("plough", *g_prism(1.4, 0.6, 0.55), "red", (-6.5, 0, 0.3), (90, 0, 90))
    hcyl(0.16, 0.12, (-6.12, 0, 2.05), "lamp", axis="x", segs=10)
    chimney_smoke(-5.45, 0, 3.4, 1.35)
    puff(-2.6, 0.9, 5.8, 0.75)
    # вагон
    box(5.2, 1.5, 1.55, (3.4, 0, 0.82), "car_green", bevel=0.04)
    box(5.22, 1.52, 0.55, (3.4, 0, 1.55), "cream", bevel=0.02)
    for k in range(6):
        win(1.4 + 0.8 * k, -0.77, 1.62, 0.42, 0.42, "window", None)
    mk("roof", *g_barrel(5.5, 0.82, 10), "slate", (3.4, 0, 2.35), scl=(1, 1, 0.45))
    for x in (1.6, 2.4, 4.4, 5.2):
        for y in (-0.7, 0.7):
            hcyl(0.36, 0.12, (x, y, 0.56), "black", axis="y", segs=12)
    box(0.6, 0.2, 0.15, (0.55, 0, 0.95), "black", bevel=0)


def m_powerplant():
    for (x, h) in ((-2.4, 10.0), (-0.3, 10.4), (1.8, 9.6)):
        lathe([(0.62, 0), (0.46, h - 3.2)], (x, 3.0, 0), "concrete", segs=12)
        r0 = 0.46
        for k in range(4):
            z0 = h - 3.2 + k * 0.8
            r1 = r0 - 0.025
            lathe([(r0, 0), (r1, 0.8)], (x, 3.0, z0), "red" if k % 2 == 0 else "plaster", segs=12)
            r0 = r1
        lathe([(r0 + 0.06, 0), (r0 + 0.06, 0.2)], (x, 3.0, h), "dark", segs=12)
        puff(x, 3.0, h + 0.55, 0.55)
        puff(x + 0.5, 3.4, h + 1.35, 0.7)
    box(5.4, 2.4, 5.4, (-0.3, 1.45, 0), "brick", bevel=0.05)
    for x in (-2.1, -1.2, -0.3, 0.6, 1.5):
        box(0.4, 0.08, 1.4, (x, 0.24, 3.6), "window", bevel=0)
    box(6.6, 2.6, 3.6, (-0.5, -0.9, 0), "concrete_light", bevel=0.05)
    box(6.8, 2.8, 0.25, (-0.5, -0.9, 3.6), "slate", bevel=0.03)
    for k in range(6):
        box(0.55, 0.08, 2.6, (-3.0 + k * 1.0, -2.22, 0.6), "window", bevel=0)
    box(1.1, 0.1, 0.5, (-0.5, -2.24, 3.0), "red", bevel=0)
    for (x, y) in ((3.7, -1.2), (3.7, 0.4)):
        box(0.9, 0.9, 0.9, (x, y, 0), "steel_dark", bevel=0.04)
        for dx in (-0.25, 0.25):
            cyl(0.07, 0.55, (x + dx, y, 0.9), "plaster", segs=6)
    beam((3.4, -2.0, 0), (3.4, -2.0, 2.2), 0.12, "steel_dark")
    beam((4.2, -2.0, 0), (4.2, -2.0, 2.2), 0.12, "steel_dark")
    beam((3.2, -2.0, 2.1), (4.4, -2.0, 2.1), 0.12, "steel_dark")


def m_tram():
    box(10.4, 1.9, 0.12, (0, 0, 0), "paving", bevel=0.03)
    for y in (-0.48, 0.48):
        box(10.4, 0.12, 0.1, (0, y, 0.12), "steel", bevel=0)
    box(6.0, 1.4, 0.35, (0, 0, 0.25), "black", bevel=0.02)
    for x in (-1.9, 1.9):
        for y in (-0.62, 0.62):
            hcyl(0.34, 0.12, (x, y, 0.4), "black", axis="y", segs=12)
    box(6.6, 1.65, 0.95, (0, 0, 0.55), "tram_red", bevel=0.06)
    box(6.6, 1.65, 0.85, (0, 0, 1.5), "cream", bevel=0.03)
    for k in range(7):
        win(-2.7 + 0.9 * k, -0.84, 1.58, 0.6, 0.6, "window", None)
    swin(3.31, 0, 1.58, 0.9, 0.6, "window", None)
    box(6.7, 1.72, 0.22, (0, 0, 2.35), "tram_red", bevel=0.03)
    mk("roof", *g_barrel(6.8, 0.88, 10), "tram_red", (0, 0, 2.55), scl=(1, 1, 0.42))
    box(3.6, 0.8, 0.28, (0, 0, 2.75), "cream", bevel=0.03)
    for x in (-3.32, 3.32):
        hcyl(0.14, 0.1, (x, 0, 1.05), "lamp", axis="x", segs=10)
    for x in (-3.1, -1.4, 0.3, 2.0):
        box(0.45, 0.06, 0.12, (x, -0.84, 1.1), "cream", bevel=0)
    beam((1.3, 0, 3.0), (-0.9, 0, 4.25), 0.08, "dark")
    beam((-4.6, 0, 4.3), (4.6, 0, 4.3), 0.06, "dark")
    for x in (-4.2, 4.2):
        cyl(0.11, 4.8, (x, 1.65, 0), "spruce", segs=8)
        beam((x, 1.65, 4.55), (x, -0.15, 4.4), 0.08, "spruce")
        ico(0.13, (x, 1.65, 4.85), "gold", sub=1)


def m_lab():
    hx = -1.3
    box(3.6, 2.5, 2.5, (hx, 0.4, 0), "plaster", bevel=0.05)
    hip(3.9, 2.8, 1.3, (hx, 0.4, 2.5), "slate")
    box(0.7, 0.08, 1.4, (hx - 0.7, -0.87, 0), "wood_dark", bevel=0)
    for x in (0.4, 1.2):
        win(hx + x - 0.5, -0.87, 0.9, 0.42, 0.8, "glow_win", "white")
    box(0.4, 0.4, 0.9, (hx + 1.0, 0.9, 3.0), "brick", bevel=0.03)
    ax, ay = hx - 1.0, 0.7
    cyl(0.05, 3.0, (ax, ay, 3.2), "iron", segs=6)
    for z, w in ((5.6, 1.0), (5.0, 1.4), (4.4, 1.8)):
        beam((ax - w / 2, ay, z), (ax + w / 2, ay, z), 0.06, "iron")
    ico(0.12, (ax, ay, 6.25), "redlight", sub=1)
    # катушка Теслы
    cx, cy = 2.3, -0.3
    box(1.3, 1.3, 0.5, (cx, cy, 0), "wood_dark", bevel=0.04)
    cyl(0.62, 0.32, (cx, cy, 0.5), "coil", segs=16)
    cyl(0.3, 3.4, (cx, cy, 0.5), "coil", segs=14)
    mk("torus", *g_torus(0.68, 0.3, 20, 8), "chrome", (cx, cy, 4.15), smooth=True)
    random.seed(77)
    for ang, ln, dz in ((150, 1.8, 0.4), (25, 2.0, 0.2), (-40, 1.9, -0.9), (95, 1.4, 0.8), (-150, 1.5, -0.4)):
        a = radians(ang)
        p = Vector((cx + 0.95 * cos(a), cy + 0.95 * sin(a), 4.15))
        d = Vector((cos(a), sin(a), dz / ln)).normalized()
        for s in range(5):
            q = p + d * (ln / 5) + Vector((random.uniform(-.2, .2), random.uniform(-.2, .2), random.uniform(-.25, .25)))
            beam(p, q, 0.07, "spark")
            p = q
    ico(0.18, (cx, cy, 4.55), "spark", sub=1)


def m_radio():
    mx, my = 1.4, 0.7
    H, n = 15.0, 10
    for k in range(n):
        z0, z1 = H * k / n, H * (k + 1) / n
        w0, w1 = 1.05 - 0.8 * k / n, 1.05 - 0.8 * (k + 1) / n
        m = "red" if k % 2 == 0 else "plaster"
        c0 = [(mx + sx * w0, my + sy * w0, z0) for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        c1 = [(mx + sx * w1, my + sy * w1, z1) for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        for i in range(4):
            j = (i + 1) % 4
            beam(c0[i], c1[i], 0.17, m)
            beam(c1[i], c1[j], 0.1, m)
            beam(c0[i], c1[j], 0.07, m)
            beam(c0[j], c1[i], 0.07, m)
    for z in (H * 0.5, H):
        w = 1.05 - 0.8 * z / H
        box(2 * w + 0.5, 2 * w + 0.5, 0.1, (mx, my, z), "steel_dark", bevel=0)
    cyl(0.07, 1.6, (mx, my, H), "steel_dark", segs=6)
    ico(0.18, (mx, my, H + 1.65), "redlight", sub=1)
    ico(0.15, (mx + 0.62, my - 0.62, H * 0.5 + 0.2), "redlight", sub=1)
    bx = -2.5
    box(3.2, 2.0, 1.8, (bx, -0.6, 0), "plaster", bevel=0.05)
    box(3.35, 2.15, 0.18, (bx, -0.6, 1.8), "slate", bevel=0.03)
    box(0.6, 0.08, 1.2, (bx - 0.9, -1.62, 0), "wood_dark", bevel=0)
    for x in (0.0, 0.9):
        win(bx + x, -1.62, 0.7, 0.5, 0.6)
    lathe([(0.05, -0.1), (0.5, 0.18), (0.62, 0.3), (0, 0.06)], (bx + 0.6, -0.3, 2.25), "plaster", segs=14,
          rot=(-40, 0, 0), smooth=True)
    cyl(0.07, 0.35, (bx + 0.6, -0.3, 1.98), "steel_dark", segs=6)
    box(1.0, 0.8, 0.8, (mx - 0.1, my - 1.6, 0), "steel_dark", bevel=0.03)


def m_factory():
    box(8.0, 4.6, 2.0, (0.3, 0.3, 0), "concrete_light", bevel=0.05)
    for k in range(4):
        y0 = -2.0 + k * 1.15 + 0.0
        y1 = y0 + 1.15
        v = [(-3.7, y0, 0), (4.3, y0, 0), (4.3, y0, 1.0), (-3.7, y0, 1.0), (-3.7, y1, 0), (4.3, y1, 0)]
        f = [(0, 1, 2, 3), (3, 2, 5, 4), (1, 5, 2), (0, 3, 4), (0, 4, 5, 1)]
        mk("tooth", v, f, "slate", (0, 0, 2.0))
        box(7.8, 0.06, 0.8, (0.3, y0 - 0.02, 2.08), "glass", bevel=0)
        for x in (-2.4, -0.4, 1.6, 3.4):
            box(0.08, 0.08, 0.85, (x, y0 - 0.05, 2.06), "plaster", bevel=0)
    for k in range(7):
        box(0.55, 0.08, 1.1, (-2.2 + k * 0.95, -1.95, 0.55), "window", bevel=0)
    box(1.6, 0.1, 1.6, (2.9, -1.96, 0), "frame_blue", bevel=0)
    lathe([(0.66, 0), (0.42, 7.2)], (-3.1, 2.1, 0), "brick", segs=4, phase=45)
    lathe([(0.5, 0), (0.5, 0.35)], (-3.1, 2.1, 7.0), "dark", segs=4, phase=45)
    chimney_smoke(-3.1, 2.1, 7.4, 1.25)
    box(2.4, 1.2, 2.8, (-2.6, -2.3, 0), "plaster", bevel=0.04)
    box(2.5, 1.3, 0.15, (-2.6, -2.3, 2.8), "slate", bevel=0.02)
    box(1.9, 0.08, 0.55, (-2.6, -2.94, 1.95), "red", bevel=0)
    box(0.6, 0.08, 1.2, (-2.6, -2.94, 0), "window", bevel=0)
    for x in (-3.3, -1.9):
        win(x, -2.94, 1.0, 0.4, 0.6)


def m_dumptruck():
    with grp((0.1, 0.3, 0), (0, 0, 21), 0.86):
        for (x, ys) in ((-2.9, (-1.85, 1.85)), (2.5, (-2.15, -1.25, 1.25, 2.15))):
            for y in ys:
                hcyl(1.4, 0.82, (x, y, 1.4), "tire", axis="y", segs=16, smooth=False)
                hcyl(0.72, 0.86, (x, y, 1.4), "belaz_yellow", axis="y", segs=10)
                hcyl(0.3, 0.9, (x, y, 1.4), "dark", axis="y", segs=8)
        box(8.0, 2.4, 0.9, (0, 0, 1.2), "steel_dark", bevel=0.05)
        bed = [(-1.0, -2.0, 2.4), (4.7, -2.0, 2.4), (4.7, 2.0, 2.4), (-1.0, 2.0, 2.4),
               (-1.7, -2.55, 4.3), (5.1, -2.55, 3.95), (5.1, 2.55, 3.95), (-1.7, 2.55, 4.3)]
        mk("bed", bed, BOXF, "belaz_yellow", bevel=0.08)
        box(1.5, 5.1, 0.25, (-2.4, 0, 4.15), "belaz_yellow", bevel=0.05)
        for x in (0.3, 2.0, 3.6):
            box(0.2, 0.1, 1.5, (x, -2.3, 2.6), "belaz_yellow", rot=(-17, 0, 0), bevel=0)
        mk("ore", *g_ico(1.0, 2), "gravel", (1.7, 0, 4.0), scl=(2.6, 1.9, 0.75))
        box(1.9, 4.9, 0.25, (-3.8, 0, 2.3), "belaz_green", bevel=0.05)
        box(1.7, 1.7, 1.6, (-3.7, -1.45, 2.55), "belaz_green", bevel=0.06)
        box(1.9, 1.9, 0.14, (-3.7, -1.45, 4.15), "plaster", bevel=0.03)
        box(0.08, 1.3, 0.75, (-4.57, -1.45, 3.15), "window", bevel=0)
        box(1.3, 0.08, 0.75, (-3.65, -2.32, 3.15), "window", bevel=0)
        box(0.5, 2.6, 1.7, (-4.55, 0.4, 0.75), "belaz_green", bevel=0.05)
        for z in (1.0, 1.35, 1.7, 2.05):
            box(0.06, 2.0, 0.12, (-4.82, 0.4, z), "dark", bevel=0)
        for y in (-0.5, 1.3):
            hcyl(0.15, 0.1, (-4.84, y, 1.95), "lamp", axis="x", segs=8)
        beam((-4.7, -2.0, 0.6), (-4.0, -2.4, 2.3), 0.12, "plaster")
        beam((-4.7, -2.45, 2.5), (-4.7, 2.4, 2.5), 0.07, "plaster")
        for y in (-2.4, -0.8, 0.8, 2.4):
            beam((-4.7, y, 2.5), (-4.7, y, 3.0), 0.07, "plaster")
        beam((-4.7, -2.4, 3.0), (-4.7, 2.4, 3.0), 0.07, "plaster")


def m_refinery():
    for (x, y, r, h) in ((-3.5, -0.9, 1.4, 1.7), (-3.4, 1.7, 1.2, 1.5), (-0.7, -1.35, 1.15, 1.4)):
        cyl(r, h, (x, y, 0), "plaster", segs=18)
        cyl(r + 0.02, 0.22, (x, y, h - 0.45), "npp_blue", segs=18)
        cone(r + 0.03, 0.35, (x, y, h), "concrete_light", segs=18, r2=0.25)
    for (x, y, r, h) in ((1.9, 1.0, 0.55, 7.4), (3.1, 0.1, 0.46, 6.0), (0.9, 1.9, 0.42, 5.2)):
        cyl(r, h, (x, y, 0), "steel", segs=12)
        lathe([(r, 0), (r * 0.8, r * 0.45), (0, r * 0.62)], (x, y, h), "steel", segs=12, smooth=True)
        for z in [1.4 + 1.4 * k for k in range(int((h - 1.0) / 1.4))]:
            cyl(r + 0.22, 0.08, (x, y, z), "steel_dark", segs=12)
        beam((x + r + 0.05, y - 0.1, 0), (x + r + 0.05, y - 0.1, h), 0.06, "steel_dark")
    for (y, z, m) in ((-0.25, 1.5, "belaz_yellow"), (-0.05, 1.75, "red"), (0.15, 1.5, "steel")):
        hcyl(0.11, 5.6, (-0.1, y, z), m, axis="x", segs=8)
    for x in (-2.0, 0.0, 2.2):
        beam((x, -0.4, 0), (x, -0.4, 1.9), 0.1, "steel_dark")
        beam((x, 0.3, 0), (x, 0.3, 1.9), 0.1, "steel_dark")
    for k in range(3):
        lathe([(0.16, 0), (0.14, 2.2)], (4.6, 1.7, 2.2 * k), "red" if k % 2 == 0 else "plaster", segs=8)
    lathe([(0.26, 0), (0.42, 0.35), (0.3, 0.8), (0.08, 1.25), (0, 1.35)], (4.6, 1.7, 6.6), "fire", segs=10, smooth=True)
    lathe([(0.14, 0), (0.22, 0.25), (0.12, 0.6), (0, 0.75)], (4.6, 1.6, 6.62), "fire_core", segs=8, smooth=True)
    puff(5.0, 2.0, 8.3, 0.4)
    box(1.1, 0.8, 0.9, (3.6, -1.6, 0), "plaster", bevel=0.03)


def m_mine():
    v, f = g_lathe([(4.1, 0), (3.6, 1.0), (2.6, 2.4), (1.4, 3.7), (0.45, 4.6), (0, 4.75)], 28, cap0=True, cap1=False)
    nv = []
    for (x, y, z) in v:
        p = Vector((x, y, z))
        if z > 0.01:
            p += Vector((noise.noise(p * 0.8 + Vector((1, 2, 3))), noise.noise(p * 0.8 + Vector((4, 5, 6))),
                         0.6 * noise.noise(p * 0.9 + Vector((7, 8, 9))))) * 0.45
        nv.append(p)
    mk("heap", nv, f, "salt", (1.9, 1.1, 0))
    mk("heap2", *g_lathe([(2.3, 0), (1.7, 1.0), (0.8, 2.0), (0, 2.4)], 18, cap1=False), "salt_pink", (4.2, -0.6, 0))
    for (x, y, r) in ((0.0, -1.7, 0.4), (3.0, -2.2, 0.35)):
        ico(r, (x, y, r * 0.3), "salt_pink", sub=1, scl=(1.3, 1, 0.7))
    # копёр
    hx, hy = -3.0, -0.4
    w = 0.75
    for (sx, sy) in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
        beam((hx + sx * w, hy + sy * w, 0), (hx + sx * 0.5, hy + sy * 0.5, 7.2), 0.2, "frame_blue")
    for z in (1.8, 3.6, 5.4, 7.0):
        ww = w - (w - 0.5) * z / 7.2
        for (a, b) in (((-1, -1), (1, -1)), ((1, -1), (1, 1)), ((1, 1), (-1, 1)), ((-1, 1), (-1, -1))):
            beam((hx + a[0] * ww, hy + a[1] * ww, z), (hx + b[0] * ww, hy + b[1] * ww, z), 0.12, "frame_blue")
    for k in range(3):
        z0, z1 = 1.8 * k, 1.8 * (k + 1)
        beam((hx - w, hy - w, z0), (hx + w, hy - w, z1), 0.08, "frame_blue")
    for y in (-0.35, 0.35):
        beam((hx - 0.5, hy + y, 7.0), (hx - 2.3, hy + y, 0), 0.2, "frame_blue")
    box(1.5, 1.4, 0.7, (hx, hy, 7.2), "red", bevel=0.03)
    with grp((hx, hy - 0.45, 8.15), (90, 0, 0)):
        for dx in (-0.45, 0.45):
            mk("sheave", *g_torus(0.62, 0.1, 18, 6), "red", (dx, 0, 0))
            for k in range(4):
                a = pi * k / 4
                beam((dx - 0.6 * cos(a), -0.6 * sin(a), 0), (dx + 0.6 * cos(a), 0.6 * sin(a), 0), 0.07, "red")
    box(2.3, 2.1, 2.4, (-5.0, 0.2, 0), "brick", bevel=0.04)
    prism(2.6, 2.4, 0.9, (-5.0, 0.2, 2.4), "slate", rot=(0, 0, 90))
    box(2.8, 2.2, 2.0, (hx + 0.3, hy - 1.0, 0), "plaster", bevel=0.04)
    box(2.9, 2.3, 0.15, (hx + 0.3, hy - 1.0, 2.0), "slate", bevel=0.02)
    for x in (-0.7, 0.2, 1.1):
        win(hx + 0.3 + x, hy - 2.12, 0.8, 0.45, 0.65)
    beam((hx + 1.4, hy - 0.4, 2.0), (1.0, 0.6, 4.6), 0.42, "steel")


def m_camp():
    mk("ground", *g_disc(4.7, 2.8, 0.08, 36, 0.09, 6), "forest_floor")
    for (x, y, s) in ((-2.0, 0.6, 1.0), (1.5, 1.1, 0.9)):
        mk("mound", *g_ico(1.0, 2), "grass_dark", (x, y + 0.2, -0.15), scl=(1.5 * s, 1.2 * s, 0.85 * s))
        box(1.25 * s, 0.25, 0.95 * s, (x, y - 0.95 * s, 0), "logs", bevel=0.03)
        box(0.5 * s, 0.08, 0.75 * s, (x, y - 1.08 * s, 0), "dark", bevel=0)
        for k in range(3):
            hcyl(0.11, 1.4 * s, (x, y - 0.95 * s + 0.0, 0.95 * s + 0.11 + 0.0 * k), "wood", axis="x", segs=6)
        cyl(0.07, 0.65, (x + 0.6 * s, y + 0.3, 0.6 * s), "iron", segs=6)
        puff(x + 0.65 * s, y + 0.4, 1.55 * s, 0.24)
        puff(x + 0.9 * s, y + 0.65, 1.95 * s, 0.32)
    fx, fy = 0.0, -1.35
    for k in range(9):
        a = 2 * pi * k / 9
        ico(0.15, (fx + 0.55 * cos(a), fy + 0.5 * sin(a), 0.08), "stone", sub=1)
    for a in (20, 80, 140):
        r = radians(a)
        beam((fx - 0.5 * cos(r), fy - 0.5 * sin(r), 0.1), (fx + 0.5 * cos(r), fy + 0.5 * sin(r), 0.35), 0.14, "wood_dark")
    lathe([(0.38, 0), (0.42, 0.25), (0.28, 0.7), (0.1, 1.05), (0, 1.15)], (fx, fy, 0.15), "fire", segs=9, smooth=True)
    lathe([(0.2, 0), (0.22, 0.2), (0.12, 0.55), (0, 0.72)], (fx, fy - 0.12, 0.15), "fire_core", segs=8, smooth=True)
    L = bpy.data.lights.new("fire", "POINT"); L.energy = 60; L.color = (1.0, 0.55, 0.2); L.shadow_soft_size = 0.3
    lo = bpy.data.objects.new("fire", L); COLL[0].objects.link(lo); lo.parent = PARENT[-1]; lo.location = (fx, fy - 0.2, 0.9)
    hcyl(0.18, 1.4, (fx - 1.3, fy - 0.2, 0.18), "wood", axis="y", segs=8)
    hcyl(0.18, 1.4, (fx + 1.25, fy - 0.35, 0.18), "wood", axis="y", segs=8)
    for (x, y, h, m) in ((-3.7, -0.4, 3.6, "spruce"), (3.5, -0.6, 3.1, "spruce_light"), (3.4, 1.6, 4.0, "spruce"),
                         (-0.5, 2.3, 4.3, "spruce"), (-3.6, 1.9, 3.3, "spruce_light"), (0.4, 0.8, 2.4, "spruce_light")):
        spruce(x, y, h, m)


def m_metro():
    box(7.2, 4.6, 0.14, (0, 0, 0), "paving", bevel=0.03)
    gx = 0.9
    box(4.2, 2.4, 2.2, (gx, 0.5, 0.14), "glass", bevel=0.02)
    for x in (-1.2, -0.4, 0.4, 1.2):
        box(0.07, 0.08, 2.2, (gx + x, -0.71, 0.14), "plaster", bevel=0)
    box(1.2, 0.08, 1.5, (gx, -0.72, 0.14), "window", bevel=0)
    box(5.4, 3.4, 0.32, (gx, 0.4, 2.34), "plaster", bevel=0.05)
    for (x, y) in ((gx - 2.5, -1.1), (gx + 2.5, -1.1)):
        cyl(0.09, 2.2, (x, y, 0.14), "plaster", segs=8)
    box(5.4, 0.08, 0.14, (gx, -1.3, 2.4), "metro_red", bevel=0)
    px, py = -2.5, -1.1
    cyl(0.13, 3.9, (px, py, 0.14), "steel_dark", segs=8)
    with grp((px, py, 5.2), (-8, 0, 0), 1.3):
        hcyl(1.28, 0.2, (0, 0, 0), "plaster", axis="y", segs=24)
        hcyl(1.34, 0.12, (0, 0.04, 0), "steel_dark", axis="y", segs=24)
        t = 0.34
        y = -0.2
        beam((-0.62, y, -0.66), (-0.62, y, 0.7), t, "metro_red", 0.2)
        beam((0.62, y, -0.66), (0.62, y, 0.7), t, "metro_red", 0.2)
        beam((-0.62, y, 0.62), (0.0, y, -0.2), t, "metro_red", 0.2)
        beam((0.0, y, -0.2), (0.62, y, 0.62), t, "metro_red", 0.2)
    tree(-2.5, 1.5, 2.2)
    tree(3.1, -1.7, 1.7)


def m_techpark():
    box(8.2, 5.2, 0.15, (0, 0, 0), "paving", bevel=0.03)

    def tower(x, y, sx, sy, h, gm, slab=0.95):
        box(sx, sy, h, (x, y, 0.15), gm, bevel=0.02)
        z = 0.15 + slab
        while z < h:
            box(sx + 0.08, sy + 0.08, 0.1, (x, y, z), "plaster", bevel=0)
            z += slab
        box(sx + 0.16, sy + 0.16, 0.22, (x, y, h + 0.15), "plaster", bevel=0.02)
    tower(-1.7, 0.9, 2.6, 2.4, 12.6, "tglass1")
    v, f = g_prism(2.6, 2.4, 1.2)
    mk("cap", v, f, "tglass1", (-1.7, 0.9, 12.97), rot=(0, 0, 90))
    cyl(0.05, 1.5, (-1.7, 0.9, 14.1), "steel_dark", segs=6)
    tower(1.8, 1.3, 2.4, 2.2, 9.2, "tglass2")
    box(1.0, 0.8, 0.5, (1.6, 1.4, 9.57), "steel", bevel=0.02)
    tower(0.9, -1.25, 3.4, 1.9, 5.0, "tglass3", 1.0)
    tower(-3.0, -1.4, 1.5, 1.4, 3.0, "tglass1", 1.0)
    for (x, y, h) in ((-3.5, 1.5, 1.6), (3.4, -1.8, 1.5), (-1.3, -2.1, 1.3), (3.5, 2.0, 1.4)):
        tree(x, y, h)


def m_library():
    box(9.0, 5.0, 1.0, (0, 0.3, 0), "stone_light", bevel=0.05)
    box(7.0, 4.0, 1.1, (0, 0.3, 1.0), "plaster", bevel=0.05)
    for k in range(10):
        box(0.4, 0.08, 0.55, (-3.0 + k * 0.666, -1.72, 1.25), "window", bevel=0)
    box(1.6, 1.0, 0.45, (0, -2.2, 0), "stone_light", bevel=0.03)
    for k in range(3):
        box(1.6, 0.35, 0.33 * (k + 1), (0, -2.55 + k * 0.3 + 0.35, 0), "stone_light", bevel=0.02)
    a = 1 + sqrt(2)
    k = 1.3
    pts = set()
    for sx in (-1, 1):
        for sy in (-1, 1):
            for sz in (-1, 1):
                pts.add((sx * k, sy * k, sz * a * k)); pts.add((sx * k, sy * a * k, sz * k)); pts.add((sx * a * k, sy * k, sz * k))
    bm = bmesh.new()
    for p in pts:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=list(bm.verts))
    bmesh.ops.dissolve_limit(bm, angle_limit=radians(1), verts=list(bm.verts), edges=list(bm.edges))
    bm.verts.index_update()
    v = [tuple(x.co) for x in bm.verts]
    f = [tuple(x.index for x in fc.verts) for fc in bm.faces]
    bm.free()
    fmi = []
    for fc in f:
        cz = sum(v[i][2] for i in fc) / len(fc)
        cy = sum(v[i][1] for i in fc) / len(fc)
        fmi.append(1 if len(fc) == 3 else (2 if abs(cz) < 0.5 and cy < -1 else 0))
    cz = 2.0 + a * k - 0.35
    mk("diamond", v, f, "glass_a", (0, 0.5, cz), mats=["glass_a", "glass_b", "tglass1"], fmi=fmi)
    o = mk("frame", v, f, "plaster", (0, 0.5, cz))
    md = o.modifiers.new("wire", "WIREFRAME"); md.thickness = 0.13; md.use_replace = True
    cyl(1.5, 0.5, (0, 0.5, 2.0), "plaster", segs=8, phase=22.5)
    cyl(0.05, 1.0, (0, 0.5, cz + a * k), "steel_dark", segs=6)
    for (x, y) in ((-3.9, -2.1), (3.9, -2.1)):
        tree(x, y, 1.6)


def m_dish():
    box(4.4, 4.0, 0.35, (0.9, 0.5, 0), "concrete", bevel=0.04)
    cyl(0.85, 2.4, (0.9, 0.7, 0.35), "plaster", segs=12)
    box(1.4, 0.9, 0.6, (0.9, 0.7, 2.6), "steel", bevel=0.04)
    R, fo = 3.3, 2.0
    depth = R * R / (4 * fo)
    rs = [R * i / 8 for i in range(9)]
    prof = [(0, -0.16)] + [(r, r * r / (4 * fo) - 0.16) for r in rs[1:]] + [(R + 0.06, depth)] + \
           [(r, r * r / (4 * fo)) for r in reversed(rs[1:])] + [(0, 0)]
    with grp((0.9, 0.55, 3.55), (52, 0, 0)):
        lathe(prof, (0, 0, 0), "plaster", segs=28, smooth=True, cap0=False, cap1=False)
        cyl(0.7, 0.55, (0, 0, -0.65), "steel", segs=12)
        for k in range(4):
            a = pi / 4 + pi / 2 * k
            beam((R * 0.92 * cos(a), R * 0.92 * sin(a), depth * 0.85), (0, 0, fo - 0.25), 0.08, "steel")
        cyl(0.24, 0.55, (0, 0, fo - 0.4), "plaster", segs=10)
        cyl(0.27, 0.08, (0, 0, fo + 0.15), "red", segs=10)
    bx = -3.4
    box(2.6, 2.1, 1.7, (bx, -0.7, 0), "plaster", bevel=0.05)
    box(2.62, 2.12, 0.3, (bx, -0.7, 1.15), "npp_blue", bevel=0)
    box(2.75, 2.25, 0.16, (bx, -0.7, 1.7), "slate", bevel=0.02)
    box(0.55, 0.08, 1.0, (bx - 0.6, -1.76, 0), "window", bevel=0)
    win(bx + 0.5, -1.76, 0.45, 0.6, 0.5)
    with grp((bx + 0.5, -0.4, 2.3), (40, 0, 0)):
        lathe([(0, -0.08), (0.5, 0.06), (0.62, 0.18), (0, 0.02)], (0, 0, 0), "plaster", segs=14, smooth=True)
    cyl(0.07, 0.45, (bx + 0.5, -0.4, 1.86), "steel_dark", segs=6)


def m_npp():
    outer = [(2.35, 0), (2.02, 1.6), (1.74, 3.3), (1.58, 4.9), (1.58, 5.7), (1.66, 6.5), (1.75, 7.1)]
    inner = [(r - 0.14, z) for (r, z) in reversed(outer)]
    prof = outer + inner
    for (x, y) in ((-2.5, 1.3), (2.6, 1.5)):
        lathe(prof, (x, y, 0), "concrete_light", segs=26, smooth=True, cap0=False, cap1=False)
        cyl(2.1, 0.06, (x, y, 0), "concrete", segs=20)
        cyl(1.77, 0.3, (x, y, 6.6), "red", segs=26, smooth=True)
        puff(x + 0.1, y + 0.1, 7.6, 1.15, "steam")
        puff(x + 0.7, y + 0.5, 8.55, 1.3, "steam")
        puff(x + 1.4, y + 1.0, 9.35, 1.1, "steam")
    for (x, y) in ((-1.7, -1.25), (1.7, -1.05)):
        cyl(1.28, 2.7, (x, y, 0), "plaster", segs=20)
        cyl(1.3, 0.35, (x, y, 1.9), "npp_blue", segs=20)
        lathe([(1.28, 0), (1.2, 0.45), (0.95, 0.85), (0.55, 1.15), (0, 1.28)], (x, y, 2.7), "concrete", segs=20, smooth=True)
    box(5.2, 1.6, 2.0, (0.1, 0.2, 0), "plaster", bevel=0.05)
    box(5.22, 1.62, 0.35, (0.1, 0.2, 1.45), "npp_blue", bevel=0)
    lathe([(0.32, 0), (0.24, 6.2)], (0.0, 0.9, 0), "plaster", segs=10)
    lathe([(0.25, 0), (0.24, 0.6)], (0.0, 0.9, 5.6), "red", segs=10)


def m_solar():
    box(11.2, 6.0, 0.08, (0, 0.05, 0), "grass", bevel=0.05)
    for y in (-2.25, -0.85, 0.55, 1.95):
        for k in range(5):
            x = -4.4 + k * 2.2
            box(2.05, 1.2, 0.07, (x, y, 0.62), "solar", rot=(30, 0, 0), bevel=0.01)
            for dx in (-0.8, 0.8):
                beam((x + dx, y - 0.35, 0.08), (x + dx, y - 0.35, 0.4), 0.07, "steel")
                beam((x + dx, y + 0.35, 0.08), (x + dx, y + 0.35, 0.85), 0.07, "steel")
    box(0.9, 0.6, 1.0, (5.0, 2.55, 0), "plaster", bevel=0.03)
    box(0.95, 0.65, 0.08, (5.0, 2.55, 1.0), "slate", bevel=0)


MODELS = [  # (функция, масштаб)
    (m_fort, 1.0), (m_church, 1.15), (m_stone, 1.4), (m_pier, 1.0), (m_workshop, 1.2), (m_mill, 1.15),
    (m_tower, 1.2), (m_castle, 1.0), (m_press, 1.3), (m_townhall, 1.25), (m_college, 1.15), (m_manufactory, 1.05),
    (m_canal, 1.0), (m_railway, 1.0), (m_powerplant, 1.0), (m_tram, 1.1), (m_lab, 1.3), (m_radio, 1.0),
    (m_factory, 1.0), (m_dumptruck, 1.0), (m_refinery, 1.15), (m_mine, 1.05), (m_camp, 1.1), (m_metro, 1.2),
    (m_techpark, 1.0), (m_library, 1.15), (m_dish, 1.15), (m_npp, 1.0), (m_solar, 1.05),
]
assert len(MODELS) == len(NAMES)


# ---------------------------------------------------------------------------------------------
#  Сцена, свет, камера, рендер
# ---------------------------------------------------------------------------------------------
def setup_scene(args):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    cy = sc.cycles
    cy.device = "CPU"
    cy.samples = args["samples"]
    cy.use_adaptive_sampling = True
    cy.adaptive_threshold = 0.005
    cy.use_denoising = False
    try:
        cy.use_preview_denoising = False
    except Exception:
        pass
    cy.max_bounces, cy.diffuse_bounces, cy.glossy_bounces = 4, 2, 2
    cy.transmission_bounces, cy.transparent_max_bounces = 2, 4
    cy.caustics_reflective = cy.caustics_refractive = False
    cy.filter_width = 1.2
    sc.render.resolution_x = sc.render.resolution_y = CELL
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    im = sc.render.image_settings
    im.file_format, im.color_mode, im.color_depth = "PNG", "RGBA", "8"
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    sc.view_settings.exposure = 0.0
    sc.frame_current = 1
    # небо (холодный рассеянный свет)
    w = bpy.data.worlds.new("Sky")
    sc.world = w
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs[0].default_value = hexc("#b4cbe6")
    bg.inputs[1].default_value = 0.62
    # солнце с юго-запада
    sd = bpy.data.lights.new("Sun", "SUN")
    sd.energy = 4.0
    sd.angle = radians(9)
    sd.color = hexc("#fff3e0")[:3]
    sun = bpy.data.objects.new("Sun", sd)
    sc.collection.objects.link(sun)
    az, el = radians(SUN_AZ), radians(SUN_EL)
    s = Vector((sin(az) * cos(el), cos(az) * cos(el), sin(el)))
    sun.rotation_euler = (-s).to_track_quat("-Z", "Y").to_euler()
    # камера
    cd = bpy.data.cameras.new("Cam")
    cd.type = "ORTHO"; cd.ortho_scale = ORTHO; cd.clip_start = 0.1; cd.clip_end = 400
    cam = bpy.data.objects.new("Cam", cd)
    sc.collection.objects.link(cam)
    sc.camera = cam
    t = radians(TILT)
    cam.rotation_euler = (t, 0, 0)
    fwd, up = Vector((0, sin(t), -cos(t))), Vector((0, cos(t), sin(t)))
    target = up * ((ANCHOR[1] - CELL / 2.0) * ORTHO / CELL)
    cam.location = target - fwd * 100
    # ловец теней
    COLL[0] = sc.collection
    PARENT[:] = [None]
    pl = mk("ShadowCatcher", [(-60, -60, 0), (60, -60, 0), (60, 60, 0), (-60, 60, 0)], [(0, 1, 2, 3)], "concrete")
    pl.is_shadow_catcher = True
    pl.pass_index = 0
    # композитинг: маска объектов (без тени) в отдельный файл
    sc.view_layers[0].use_pass_object_index = True
    sc.use_nodes = True
    nt = sc.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    rl = nt.nodes.new("CompositorNodeRLayers")
    comp = nt.nodes.new("CompositorNodeComposite")
    nt.links.new(rl.outputs["Image"], comp.inputs["Image"])
    idm = nt.nodes.new("CompositorNodeIDMask")
    idm.index = 1
    idm.use_antialiasing = True
    nt.links.new(rl.outputs["IndexOB"], idm.inputs[0])
    fo = nt.nodes.new("CompositorNodeOutputFile")
    fo.base_path = args["work"]
    fo.format.file_format = "PNG"
    fo.format.color_mode = "BW"
    try:
        fo.format.color_management = "OVERRIDE"
        fo.format.view_settings.view_transform = "Raw"
    except Exception:
        pass
    nt.links.new(idm.outputs[0], fo.inputs[0])
    return sc, fo


def build(sc):
    roots, colls = [], []
    for i, (fn, s) in enumerate(MODELS):
        nm = "LM_%02d_%s" % (i, NAMES[i])
        c = bpy.data.collections.new(nm)
        sc.collection.children.link(c)
        r = bpy.data.objects.new(nm, None)
        r.empty_display_size = 3
        c.objects.link(r)
        COLL[0] = c
        PARENT[:] = [r]
        random.seed(1000 + i)
        fn()
        r.scale = (s, s, s)
        roots.append(r); colls.append(c)
    bpy.context.view_layer.update()
    return roots, colls


def check_bounds(i, coll):
    deps = bpy.context.evaluated_depsgraph_get()
    k = CELL / ORTHO
    t = radians(TILT)
    xs, ys = [], []
    for o in coll.all_objects:
        if o.type != "MESH":
            continue
        oe = o.evaluated_get(deps)
        M = oe.matrix_world
        for c in oe.bound_box:
            w = M @ Vector(c)
            xs.append(ANCHOR[0] + w.x * k)
            ys.append(ANCHOR[1] - (w.y * cos(t) + w.z * sin(t)) * k)
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    flag = "  <-- CHECK" if (x1 - x0 > 142 or y1 - y0 > 127 or y1 > 158 or y0 < 2 or x0 < 4 or x1 > 156) else ""
    print("[%2d] %-12s x %5.1f..%5.1f (w %5.1f)  y %5.1f..%5.1f (h %5.1f)%s" % (
        i, NAMES[i], x0, x1, x1 - x0, y0, y1, y1 - y0, flag))


def main():
    args = parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    os.makedirs(args["work"], exist_ok=True)
    sc, fo = setup_scene(args)
    roots, colls = build(sc)
    todo = args["only"] if args["only"] is not None else list(range(len(MODELS)))
    for i, c in enumerate(colls):
        check_bounds(i, c)
    for i in todo:
        for j, c in enumerate(colls):
            c.hide_render = (j != i)
        fo.file_slots[0].path = "mask_%02d_" % i
        sc.render.filepath = os.path.join(args["work"], "cell_%02d.png" % i)
        bpy.ops.render.render(write_still=True)
        print("rendered", i, NAMES[i], flush=True)
    for c in colls:
        c.hide_render = False
    if args["save"]:
        # в .blend модели расставлены сеткой 8 x 4 для просмотра; для рендера клетки — в начале координат
        for i, r in enumerate(roots):
            r.location = ((i % COLS) * 16.0, -(i // COLS) * 14.0, 0)
        out = os.path.join(GAME, "blender", "belarus_landmarks.blend")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=out, compress=True)
        print("saved", out)
    cmd = ["python3", os.path.abspath(__file__), "--post", "--work", args["work"]]
    if not args["outline"]:
        cmd.append("--no-outline")
    subprocess.run(cmd, check=False)


main()
