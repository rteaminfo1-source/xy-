#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""3D-фигурки войск: настольные миниатюры на круглых подставках из тёмного дерева.

16 типов войск в порядке UNIT_TYPES (src/data_war.c): от княжеской дружины XII века
до ПВО. Все фигурки — героини: дружинница, арбалетчица, всадница-гусария, снайпер
в пилотке, лётчица, партизанка в платке, танкистка в люке, зенитчица и другие. Женский
облик читается и в 48 px: длинная коса с красным бантом (через плечо на грудь или по
спине), лицо с ресницами, губами и румянцем, тонкая талия, у многих — юбка. Техника
(пушки, грузовик, танк, тягач, РЛС) осталась, но рядом с ней всегда стоит героиня.
Каждая фигурка собирается в Blender из примитивов (лофты, тела вращения,
трубки-кривые, выдавленные контуры), стоит на лакированной подставке с латунным
пояском и рендерится в ракурсе 3/4 чуть сверху тем же тёплым светом, что и постройки
карты (солнце с юго-запада, азимут 225), плюс заполняющий и контровой свет.

Запуск (из корня belarus-game; EEVEE нужен виртуальный дисплей):
    xvfb-run -a -s "-screen 0 1280x1024x24" blender -b -P tools/blender/build_units.py -- \\
        [--work /tmp/belarus_units] [--only druzhina,tank] [--samples 64] [--no-save] [--zoom 4]
Blender строит все фигурки (каждую в своей коллекции), рендерит выбранные в 320x320
(вдвое крупнее клетки) с прозрачным фоном и сохраняет blender/belarus_units.blend
(фигурки расставлены сеткой 4x4, сцена, свет и этот скрипт внутри). В конце сам
вызывает шаг --post. Его можно запустить и отдельно обычным python3 (нужен Pillow):
    python3 tools/blender/build_units.py --post --work /tmp/belarus_units
--post добавляет мягкую тень под подставкой, уменьшает кадры до 160x160 и собирает
лист в 4 колонки: assets/units.png и assets/units.json
    {"cell_w":160,"cell_h":160,"cols":4,"ids":[...]}  — порядок как в UNIT_TYPES.
Камера ортографическая, 50 px клетки на единицу (радиус подставки = 1), подъём 30°;
центр верха подставки во всех клетках в одной точке — пиксель (80, ANCHOR_Y),
нижний край подставки в 4 px от низа клетки. Превью на фоне #5a4a3a (160 и 64 px)
кладутся в рабочую папку: preview_160.png, preview_64.png. Для проверки лиц есть крупный
план: --zoom 4.5 [--zh 2.02] — рендер в ту же папку, без листа и без сохранения .blend.
"""
import os, sys, json, math, random, subprocess, tempfile, time

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(os.path.dirname(HERE))
IDS = ["druzhina", "crossbow", "khorugv", "gunners", "musketeers", "hussars", "rockets", "line",
       "artillery", "rifle", "air", "partisans", "motor", "tank", "missile", "airdef"]
CELL, COLS, SCALE = 160, 4, 2          # клетка листа, колонок, во сколько раз крупнее рендер
PPU = 50.0                             # пикселей клетки на единицу модели
ELEV = 30.0                            # подъём камеры над горизонтом, градусы
BASE_R, BASE_H = 0.86, 0.13            # подставка: радиус и толщина (верх — z = 0)
MARGIN = 4                             # от нижнего края подставки до низа клетки, px
_se, _ce = math.sin(math.radians(ELEV)), math.cos(math.radians(ELEV))
ANCHOR = (CELL / 2.0, CELL - MARGIN - (BASE_R * _se + BASE_H * _ce) * PPU)
YAW = -32.0                            # общий разворот фигурок (лицом к ключевому свету)


def parse_args(argv):
    a = {"work": os.path.join(tempfile.gettempdir(), "belarus_units"), "only": None, "samples": 64,
         "save": True, "post": False, "zoom": 0.0, "zh": 2.02}
    i = 0
    while i < len(argv):
        k = argv[i]
        if k == "--work": a["work"] = argv[i + 1]; i += 1
        elif k == "--only":
            a["only"] = [IDS[int(x)] if x.isdigit() else x for x in argv[i + 1].split(",") if x]; i += 1
        elif k == "--samples": a["samples"] = int(argv[i + 1]); i += 1
        elif k == "--no-save": a["save"] = False
        elif k == "--post": a["post"] = True
        elif k == "--zoom": a["zoom"] = float(argv[i + 1]); i += 1
        elif k == "--zh": a["zh"] = float(argv[i + 1]); i += 1
        i += 1
    return a


# ----------------------------------------------------------------------------------------------
#  Сборка листа (обычный python3 + Pillow)
# ----------------------------------------------------------------------------------------------
def post(a):
    from PIL import Image, ImageDraw, ImageFilter
    work = a["work"]
    W = CELL * SCALE
    rows = (len(IDS) + COLS - 1) // COLS
    sheet = Image.new("RGBA", (COLS * CELL, rows * CELL), (0, 0, 0, 0))
    # мягкая тень под подставкой (эллипс чуть больше основания, сдвинут вниз-вправо)
    sh = Image.new("L", (W, W), 0)
    cx, cy = ANCHOR[0] * SCALE, (ANCHOR[1] + BASE_H * _ce * PPU) * SCALE
    rx, ry = BASE_R * PPU * SCALE * 1.04, BASE_R * _se * PPU * SCALE * 1.10
    ImageDraw.Draw(sh).ellipse((cx - rx + 4, cy - ry + 5, cx + rx + 4, cy + ry + 5), fill=150)
    sh = sh.filter(ImageFilter.GaussianBlur(6))
    shadow = Image.new("RGBA", (W, W), (22, 14, 8, 255))
    shadow.putalpha(sh)
    done = 0
    for n, uid in enumerate(IDS):
        p = os.path.join(work, uid + ".png")
        if not os.path.exists(p):
            print("missing", uid); continue
        im = Image.open(p).convert("RGBA")
        if im.size != (W, W):
            im = im.resize((W, W), Image.LANCZOS)
        im = Image.alpha_composite(shadow, im)
        im = im.convert("RGBa").resize((CELL, CELL), Image.LANCZOS).convert("RGBA")
        sheet.paste(im, ((n % COLS) * CELL, (n // COLS) * CELL))
        done += 1
    os.makedirs(os.path.join(GAME, "assets"), exist_ok=True)
    sheet.save(os.path.join(GAME, "assets", "units.png"), optimize=True)
    with open(os.path.join(GAME, "assets", "units.json"), "w", encoding="utf-8") as f:
        json.dump({"cell_w": CELL, "cell_h": CELL, "cols": COLS, "ids": IDS}, f, ensure_ascii=False)
        f.write("\n")
    # превью для проверки: на тёмно-коричневом фоне интерфейса, в 160 и в 64 px
    for size in (160, 64):
        bg = Image.new("RGBA", (COLS * size, rows * size), (0x5a, 0x4a, 0x3a, 255))
        small = sheet if size == CELL else sheet.convert("RGBa").resize(
            (COLS * size, rows * size), Image.LANCZOS).convert("RGBA")
        Image.alpha_composite(bg, small).convert("RGB").save(os.path.join(work, "preview_%d.png" % size))
    print("sheet", sheet.size, "cells", done, "->", os.path.join(GAME, "assets", "units.png"))


try:
    import bpy, bmesh
    from mathutils import Vector, Matrix
    IN_BLENDER = True
except ImportError:
    IN_BLENDER = False

if not IN_BLENDER:                     # обычный python3: только сборка листа
    if __name__ == "__main__":
        post(parse_args(sys.argv[1:]))
    sys.exit(0)

from math import radians, sin, cos, pi, sqrt


# ----------------------------------------------------------------------------------------------
#  Материалы
# ----------------------------------------------------------------------------------------------
def lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hexc(h, a=1.0):
    h = h.lstrip("#")
    return tuple(lin(int(h[i:i + 2], 16) / 255.0) for i in (0, 2, 4)) + (a,)


def M(col, rough=0.6, metal=0.0, **kw):
    d = dict(col=col, rough=rough, metal=metal)
    d.update(kw)
    return d


# kind: None — однотонный (var — шумовая вариация тона, bump — рельеф шумом);
#       wood — волокна, mail — кольчуга, stripe — полосы, leopard, camo, birch, twotone (верх/низ)
PAL = {
    # подставка
    "base": M("#3a2213", 0.38, kind="wood", col2="#1e110a", coat=0.55),
    "base_side": M("#2c190e", 0.4, kind="wood", col2="#170c06", coat=0.55),
    "brass": M("#d2a24c", 0.3, 0.9),
    "gold": M("#e8b437", 0.28, 0.9),
    # люди
    "skin": M("#e6b592", 0.5, sss=0.08),
    "eye": M("#20140e", 0.3),
    "glint": M("#ffffff", 0.2, emit=1.5),
    "lips": M("#c23848", 0.4, sheen=0.2),
    "blush": M("#f09a8a", 0.55, sss=0.05),
    # волосы: тёмно-русые, русые, светлые, рыжие, тёмные
    "hair": M("#5a361e", 0.6, sheen=0.4, var=(60, 0.75, 1.2)),
    "hair_fair": M("#b5803e", 0.55, sheen=0.45, var=(60, 0.78, 1.18)),
    "hair_blond": M("#e0b860", 0.5, sheen=0.45, var=(60, 0.8, 1.15)),
    "hair_red": M("#a8481e", 0.55, sheen=0.45, var=(60, 0.78, 1.18)),
    "hair_dark": M("#2c1a10", 0.55, sheen=0.4, var=(60, 0.75, 1.25)),
    "ribbon": M("#e02838", 0.45, sheen=0.5),
    "ribbon_grey": M("#8a9098", 0.5, sheen=0.3),
    # металл
    "steel": M("#c4ccd4", 0.28, 0.9),
    "steel_dark": M("#6e757e", 0.38, 0.85),
    "iron": M("#3c3d42", 0.45, 0.75),
    "mail": M("#9ea6ae", 0.42, 0.9, kind="mail"),
    "bronze": M("#c08440", 0.3, 0.9, var=(18, 0.85, 1.12)),
    "gunmetal": M("#2c2e33", 0.42, 0.6),
    # ткани и кожа
    "red": M("#b8242a", 0.7, sheen=0.3, var=(40, 0.85, 1.12)),
    "red_bright": M("#d8262a", 0.55, sheen=0.2),
    "crimson": M("#9c1830", 0.65, sheen=0.3, var=(40, 0.85, 1.12)),
    "white_cloth": M("#ece6d6", 0.7, sheen=0.3),
    "linen": M("#d9cfb4", 0.75, sheen=0.3),
    "trousers_dark": M("#3a3430", 0.75, sheen=0.2),
    "trousers_blue": M("#2c3648", 0.75, sheen=0.2),
    "leather": M("#7a4a26", 0.6, var=(30, 0.85, 1.15)),
    "leather_dark": M("#3e2414", 0.55),
    "boots": M("#2a1d16", 0.45, coat=0.2),
    "boots_black": M("#18171a", 0.4, coat=0.3),
    "gambeson": M("#c4a46a", 0.8, kind="stripe", col2="#9a7c48", axis="X", period=0.07, frac=0.18, sheen=0.3),
    "blue_coat": M("#2f5096", 0.7, sheen=0.3, var=(40, 0.88, 1.1)),
    "green_coat": M("#244a32", 0.7, sheen=0.3, var=(40, 0.88, 1.1)),
    "khaki": M("#8a8452", 0.75, sheen=0.25, var=(40, 0.88, 1.1)),
    "khaki_dark": M("#6c6840", 0.75, sheen=0.2),
    "skirt_khaki": M("#5e5a36", 0.75, sheen=0.25, var=(40, 0.88, 1.1)),
    "skirt_dark": M("#2e2c30", 0.8, sheen=0.3, var=(40, 0.85, 1.12)),
    "stockings": M("#a8926c", 0.6, sheen=0.3),
    "stockings_dark": M("#4a4038", 0.7, sheen=0.3),
    "olive_uniform": M("#5c6038", 0.7, sheen=0.25, var=(40, 0.9, 1.08)),
    "teal_coat": M("#21606a", 0.7, sheen=0.3, var=(40, 0.88, 1.1)),
    "kerchief": M("#b8343a", 0.75, kind="dots", col2="#f2ead8", scale=26.0, sheen=0.4),
    "flight_leather": M("#6e4224", 0.45, sheen=0.2, var=(30, 0.82, 1.15), coat=0.15),
    "tank_helmet": M("#26221e", 0.55, sheen=0.2),
    "tank_overall": M("#3c3a34", 0.8, sheen=0.25, var=(40, 0.88, 1.1)),
    "camo_green": M("#6a7250", 0.7, kind="camo", col2="#4c5636", col3="#857f5e", cs=2.4, sheen=0.2),
    "telogreika": M("#3e4436", 0.8, kind="stripe", col2="#2a2e24", axis="Z", period=0.06, frac=0.16, sheen=0.3),
    "fur": M("#6e5038", 0.85, sheen=0.5, var=(90, 0.7, 1.3), bump=(160, 0.5)),
    "fur_dark": M("#3e2c20", 0.85, sheen=0.5, var=(90, 0.7, 1.3), bump=(160, 0.5)),
    "leopard": M("#e0a040", 0.75, kind="leopard", col2="#2a1a10", sheen=0.4),
    "black_hat": M("#1d1b1e", 0.55, sheen=0.3),
    "feather": M("#f4f1ea", 0.6, sheen=0.4),
    "feather_red": M("#c82a2a", 0.6, sheen=0.3),
    "red_star": M("#d42024", 0.4, emit=0.4),
    "boots_yellow": M("#c8902c", 0.5, coat=0.2),
    "felt": M("#5c5650", 0.9, sheen=0.4),
    "gold_cloth": M("#e0b23c", 0.55, sheen=0.3),
    "lance": M("#d8c8a0", 0.5),
    "horse": M("#d6d0c4", 0.55, sheen=0.25, var=(12, 0.85, 1.08)),
    "horse_mane": M("#4a4440", 0.7, var=(60, 0.75, 1.25)),
    # дерево
    "wood": M("#8a5530", 0.65, var=(25, 0.8, 1.15)),
    "wood_light": M("#b8834c", 0.65, var=(25, 0.8, 1.15)),
    "wood_dark": M("#4e2f1a", 0.6, var=(25, 0.8, 1.15)),
    "wood_red": M("#8e3a22", 0.55, var=(25, 0.85, 1.1)),
    "rope": M("#b89a6a", 0.8),
    # раскраска щитов и знамён
    "shield_red": M("#c0262a", 0.45, coat=0.3),
    "pavise_a": M("#2f6a46", 0.5, coat=0.2),
    "pavise_b": M("#e0b040", 0.5, coat=0.2),
    "banner": M("#c01f26", 0.65, sheen=0.4),
    # огонь, дым
    "fire": M("#ff8a1c", 0.5, emit=8.0),
    "fire_core": M("#ffe060", 0.5, emit=12.0),
    "ember": M("#ff5a10", 0.5, emit=6.0),
    "smoke": M("#e8e6e0", 0.95),
    "smoke_dark": M("#b0aca4", 0.95),
    # техника
    "olive": M("#434a24", 0.55, var=(6, 0.9, 1.08)),
    "olive_dark": M("#343c1e", 0.6),
    "truck": M("#54532c", 0.55, var=(6, 0.9, 1.08)),
    "canvas": M("#9a8a5a", 0.85, sheen=0.3, var=(30, 0.85, 1.12)),
    "tank": M("#3c4420", 0.5, var=(5, 0.88, 1.1)),
    "missile_green": M("#40482a", 0.5, var=(6, 0.9, 1.08)),
    "missile_white": M("#e4e4dc", 0.35),
    "camo": M("#a49460", 0.55, kind="camo", col2="#4e5a2c", col3="#5a4028"),
    "gun_green": M("#3f6a34", 0.5, var=(10, 0.85, 1.1)),
    "tire": M("#222124", 0.8),
    "track": M("#2e2c2a", 0.75, kind="stripe", col2="#1a1918", axis="Y", period=0.06, frac=0.35),
    "glass": M("#1c3048", 0.08, 0.3, coat=0.8),
    "canopy": M("#7ab0d0", 0.06, 0.2, coat=0.8),
    "lamp": M("#fff2b0", 0.3, emit=3.0),
    "plane": M("#4a5a28", 0.45, kind="twotone", col2="#9ab8cc"),
    "prop": M("#2a2a2c", 0.4, 0.4),
    "yellow": M("#e8b828", 0.45),
    "black": M("#18181a", 0.5),
    "rocket_red": M("#a8281e", 0.6, kind="stripe", col2="#1e1a18", axis="Z", period=0.12, frac=0.5),
    "birch": M("#ece8e0", 0.7, kind="birch", col2="#24201e"),
    "leaf": M("#3e7a2a", 0.75, var=(30, 0.7, 1.25)),
    "grass": M("#5c7e34", 0.8, var=(30, 0.7, 1.2)),
    "earth": M("#4a3420", 0.9, var=(30, 0.8, 1.2)),
}
MATS = {}


def _in(node, *names):
    for n in names:
        if n in node.inputs:
            return node.inputs[n]
    return None


def mat(key):
    if key in MATS:
        return MATS[key]
    s = PAL[key]
    m = bpy.data.materials.new(key)
    m.use_nodes = True
    nt = m.node_tree
    N, L = nt.nodes, nt.links
    b = N["Principled BSDF"]
    c = hexc(s["col"])
    b.inputs["Base Color"].default_value = c
    b.inputs["Roughness"].default_value = s["rough"]
    b.inputs["Metallic"].default_value = s["metal"]
    _in(b, "Specular IOR Level", "Specular").default_value = 0.4
    if s.get("coat"):
        _in(b, "Coat Weight").default_value = s["coat"]; _in(b, "Coat Roughness").default_value = 0.12
    if s.get("sheen"):
        _in(b, "Sheen Weight").default_value = s["sheen"]
    if s.get("sss"):
        _in(b, "Subsurface Weight").default_value = s["sss"]
        _in(b, "Subsurface Radius").default_value = (1.0, 0.4, 0.25)
        _in(b, "Subsurface Scale").default_value = 0.01
    if s.get("emit"):
        _in(b, "Emission Color").default_value = c
        _in(b, "Emission Strength").default_value = s["emit"]
    tc = N.new("ShaderNodeTexCoord")

    def mapped(scale):
        mp = N.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = scale if isinstance(scale, (tuple, list)) else (scale,) * 3
        L.new(tc.outputs["Object"], mp.inputs["Vector"])
        return mp.outputs["Vector"]

    def noise(scale, detail=3.0):
        nz = N.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.0; nz.inputs["Detail"].default_value = detail
        L.new(mapped(scale), nz.inputs["Vector"])
        return nz.outputs["Fac"]

    def ramp(fac, c1, c2, p1=0.3, p2=0.7, const=False):
        rp = N.new("ShaderNodeValToRGB")
        if const:
            rp.color_ramp.interpolation = "CONSTANT"
        rp.color_ramp.elements[0].position = p1; rp.color_ramp.elements[1].position = p2
        rp.color_ramp.elements[0].color = c1; rp.color_ramp.elements[1].color = c2
        L.new(fac, rp.inputs["Fac"])
        return rp

    def scaled(k):
        return tuple(min(1.0, v * k) for v in c[:3]) + (1.0,)

    kind = s.get("kind")
    col_out = None
    if s.get("var"):
        sc, lo, hi = s["var"]
        col_out = ramp(noise(sc), scaled(lo), scaled(hi)).outputs["Color"]
    if kind == "wood":
        wv = N.new("ShaderNodeTexWave")
        wv.wave_type = "BANDS"; wv.bands_direction = "X"
        wv.inputs["Scale"].default_value = 1.0; wv.inputs["Distortion"].default_value = 6.0
        wv.inputs["Detail"].default_value = 3.0; wv.inputs["Detail Scale"].default_value = 1.5
        L.new(mapped((9.0, 2.0, 2.0)), wv.inputs["Vector"])
        col_out = ramp(wv.outputs["Fac"], c, hexc(s["col2"]), 0.15, 0.9).outputs["Color"]
    elif kind == "mail":
        vo = N.new("ShaderNodeTexVoronoi")
        vo.inputs["Scale"].default_value = 1.0
        L.new(mapped(140.0), vo.inputs["Vector"])
        bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.6
        bp.inputs["Distance"].default_value = 0.004
        L.new(vo.outputs["Distance"], bp.inputs["Height"]); L.new(bp.outputs["Normal"], b.inputs["Normal"])
        col_out = ramp(vo.outputs["Distance"], scaled(0.55), c, 0.0, 0.6).outputs["Color"]
    elif kind == "stripe":
        sep = N.new("ShaderNodeSeparateXYZ"); L.new(tc.outputs["Object"], sep.inputs[0])
        mul = N.new("ShaderNodeMath"); mul.operation = "MULTIPLY"; mul.inputs[1].default_value = 1.0 / s["period"]
        L.new(sep.outputs[s["axis"]], mul.inputs[0])
        fr = N.new("ShaderNodeMath"); fr.operation = "FRACT"; L.new(mul.outputs[0], fr.inputs[0])
        col_out = ramp(fr.outputs[0], hexc(s["col2"]), c, s["frac"], s["frac"] + 0.001, True).outputs["Color"]
        if key != "track" and key != "rocket_red":
            bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.35
            bp.inputs["Distance"].default_value = 0.01
            L.new(fr.outputs[0], bp.inputs["Height"]); L.new(bp.outputs["Normal"], b.inputs["Normal"])
    elif kind == "leopard":
        vo = N.new("ShaderNodeTexVoronoi"); vo.inputs["Scale"].default_value = 1.0
        L.new(mapped(13.0), vo.inputs["Vector"])
        col_out = ramp(vo.outputs["Distance"], hexc(s["col2"]), c, 0.22, 0.27).outputs["Color"]
    elif kind == "camo":
        cs = s.get("cs", 1.0)
        f1 = noise(2.6 * cs, 2.0)
        r1 = ramp(f1, hexc(s["col2"]), c, 0.47, 0.48)
        f2 = noise((3.4 * cs,) * 3, 2.0)
        mx = N.new("ShaderNodeMixRGB")
        lt = N.new("ShaderNodeMath"); lt.operation = "GREATER_THAN"; lt.inputs[1].default_value = 0.6
        L.new(f2, lt.inputs[0]); L.new(lt.outputs[0], mx.inputs["Fac"])
        L.new(r1.outputs["Color"], mx.inputs["Color1"]); mx.inputs["Color2"].default_value = hexc(s["col3"])
        col_out = mx.outputs["Color"]
    elif kind == "dots":
        vo = N.new("ShaderNodeTexVoronoi"); vo.inputs["Scale"].default_value = 1.0
        L.new(mapped(s.get("scale", 24.0)), vo.inputs["Vector"])
        col_out = ramp(vo.outputs["Distance"], hexc(s["col2"]), c, 0.16, 0.19).outputs["Color"]
    elif kind == "birch":
        f = noise((5.0, 5.0, 38.0), 1.0)
        col_out = ramp(f, c, hexc(s["col2"]), 0.63, 0.64).outputs["Color"]
    elif kind == "twotone":
        sep = N.new("ShaderNodeSeparateXYZ"); L.new(tc.outputs["Normal"], sep.inputs[0])
        col_out = ramp(sep.outputs["Z"], hexc(s["col2"]), c, -0.05, 0.05).outputs["Color"]
    if s.get("bump"):
        sc, st = s["bump"]
        bp = N.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = st
        bp.inputs["Distance"].default_value = 0.01
        L.new(noise(sc, 4.0), bp.inputs["Height"]); L.new(bp.outputs["Normal"], b.inputs["Normal"])
    if col_out is not None:
        L.new(col_out, b.inputs["Base Color"])
    MATS[key] = m
    return m


# ----------------------------------------------------------------------------------------------
#  Примитивы. Всё строится в локальных координатах текущей подсборки (стек PARENT).
#  Фигурка смотрит на -Y, X — вправо от зрителя, Z — вверх; верх подставки — z = 0.
# ----------------------------------------------------------------------------------------------
PARENT = [None]
COLL = [None]


def _link(ob):
    COLL[0].objects.link(ob)
    ob.parent = PARENT[-1]
    return ob


def mk(name, verts, faces, m, loc=(0, 0, 0), rot=(0, 0, 0), scl=1.0, smooth=False, sub=0, bevel=0.0,
       solid=0.0, autosmooth=None):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces])
    me.update(calc_edges=True)
    bm = bmesh.new(); bm.from_mesh(me)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me); bm.free()
    if smooth or autosmooth:
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    if autosmooth:
        try:
            me.use_auto_smooth = True; me.auto_smooth_angle = radians(autosmooth)
        except AttributeError:
            pass
    me.update()
    me.materials.append(mat(m))
    ob = _link(bpy.data.objects.new(name, me))
    ob.location = loc
    ob.rotation_euler = [radians(a) for a in rot]
    ob.scale = scl if isinstance(scl, (tuple, list)) else (scl, scl, scl)
    if bevel:
        md = ob.modifiers.new("bevel", "BEVEL"); md.width = bevel; md.segments = 2
        md.limit_method = "ANGLE"; md.angle_limit = radians(35)
    if solid:
        md = ob.modifiers.new("solid", "SOLIDIFY"); md.thickness = solid; md.offset = 0.0
    if sub:
        md = ob.modifiers.new("sub", "SUBSURF"); md.levels = sub; md.render_levels = sub
    return ob


class grp:
    """Подсборка: положение, поворот (градусы, XYZ) и масштаб, либо готовая матрица."""
    def __init__(self, loc=(0, 0, 0), rot=(0, 0, 0), scl=1.0, matrix=None):
        e = _link(bpy.data.objects.new("grp", None))
        if matrix is not None:
            e.matrix_basis = matrix
        else:
            e.location = loc
            e.rotation_euler = [radians(a) for a in rot]
            e.scale = scl if isinstance(scl, (tuple, list)) else (scl, scl, scl)
        self.e = e

    def __enter__(self):
        PARENT.append(self.e)
        return self.e

    def __exit__(self, *a):
        PARENT.pop()


def look(p, d, up=(0, 0, 1)):
    """Матрица: локальная Z смотрит вдоль d, локальная Y — по возможности вдоль up, начало в p."""
    z = Vector(d).normalized()
    u = Vector(up)
    y = u - z * u.dot(z)
    if y.length < 1e-6:
        y = Vector((0, 1, 0)) - z * z.y
    y.normalize()
    x = y.cross(z)
    return Matrix(((x.x, y.x, z.x, p[0]), (x.y, y.y, z.y, p[1]), (x.z, y.z, z.z, p[2]), (0, 0, 0, 1)))


BOXF = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]


def box(size, c, m, rot=(0, 0, 0), bevel=0.0):
    hx, hy, hz = size[0] / 2.0, size[1] / 2.0, size[2] / 2.0
    v = [(-hx, -hy, -hz), (hx, -hy, -hz), (hx, hy, -hz), (-hx, hy, -hz),
         (-hx, -hy, hz), (hx, -hy, hz), (hx, hy, hz), (-hx, hy, hz)]
    return mk("box", v, BOXF, m, c, rot, bevel=bevel)


def hexa(pts, m, bevel=0.0, smooth=False):
    """Произвольный «кирпич» из 8 вершин (низ 0-3, верх 4-7, порядок как у box)."""
    return mk("hexa", pts, BOXF, m, bevel=bevel, smooth=smooth)


def g_lathe(profile, segs=16, phase=0.0, cap0=True, cap1=True, a0=None, a1=None):
    verts, rings = [], []
    ph = radians(phase)
    part = a0 is not None
    cnt = segs + 1 if part else segs
    for (r, z) in profile:
        if r < 1e-6:
            rings.append([len(verts)]); verts.append((0, 0, z))
        else:
            idx = []
            for i in range(cnt):
                a = (a0 + (a1 - a0) * i / segs) if part else ph + 2 * pi * i / segs
                # угол 0 — спереди (-Y), растёт к +X
                idx.append(len(verts)); verts.append((r * sin(a), -r * cos(a), z))
            rings.append(idx)
    faces = []
    n = cnt
    for k in range(len(rings) - 1):
        A, B = rings[k], rings[k + 1]
        if len(A) == 1 and len(B) == 1:
            continue
        for i in range(segs if part else n):
            j = i + 1 if part else (i + 1) % n
            if len(A) == 1: faces.append((A[0], B[i], B[j]))
            elif len(B) == 1: faces.append((A[i], A[j], B[0]))
            else: faces.append((A[i], A[j], B[j], B[i]))
    if not part:
        if cap0 and len(rings[0]) > 1: faces.append(tuple(reversed(rings[0])))
        if cap1 and len(rings[-1]) > 1: faces.append(tuple(rings[-1]))
    return verts, faces


def lathe(profile, c, m, segs=24, rot=(0, 0, 0), smooth=True, sub=0, scl=1.0, cap0=True, cap1=True,
          a0=None, a1=None, solid=0.0):
    return mk("lathe", *g_lathe(profile, segs, 0.0, cap0, cap1, a0, a1), m, c, rot, scl=scl, smooth=smooth,
              sub=sub, solid=solid)


def g_sphere(segs=20, rings=12):
    v = [(0.0, 0.0, 1.0)]
    for r in range(1, rings):
        th = pi * r / rings
        for s in range(segs):
            ph = 2 * pi * s / segs
            v.append((sin(th) * cos(ph), sin(th) * sin(ph), cos(th)))
    v.append((0.0, 0.0, -1.0))
    last = len(v) - 1
    f = [(0, 1 + s, 1 + (s + 1) % segs) for s in range(segs)]
    for r in range(rings - 2):
        for s in range(segs):
            a = 1 + r * segs + s; b = 1 + r * segs + (s + 1) % segs
            f.append((a, a + segs, b + segs, b))
    base = 1 + (rings - 2) * segs
    f += [(base + s, last, base + (s + 1) % segs) for s in range(segs)]
    return v, f


def ell(c, r, m, rot=(0, 0, 0), segs=20, rings=12, sub=0):
    if not isinstance(r, (tuple, list)):
        r = (r, r, r)
    return mk("ell", *g_sphere(segs, rings), m, c, rot, scl=tuple(r), smooth=True, sub=sub)


def cyl2(p0, p1, r0, r1, m, segs=12, cap=True, smooth=True):
    """Цилиндр/конус между двумя точками."""
    p0, p1 = Vector(p0), Vector(p1)
    ax = p1 - p0
    q = Vector((0, 0, 1)).rotation_difference(ax.normalized())
    v, f = [], []
    for (p, r) in ((p0, r0), (p1, r1)):
        for i in range(segs):
            a = 2 * pi * i / segs
            v.append(p + q @ Vector((cos(a) * r, sin(a) * r, 0)))
    f = [(i, (i + 1) % segs, segs + (i + 1) % segs, segs + i) for i in range(segs)]
    if cap:
        f.append(tuple(range(segs - 1, -1, -1))); f.append(tuple(range(segs, 2 * segs)))
    return mk("cyl", v, f, m, smooth=smooth, autosmooth=40 if smooth else None)


def wheel_disc(c, r, w, m, axis="x", segs=20):
    """Колесо/диск: цилиндр с осью по X (или Y), центр c."""
    c = Vector(c)
    d = Vector((w / 2.0, 0, 0)) if axis == "x" else Vector((0, w / 2.0, 0))
    return cyl2(c - d, c + d, r, r, m, segs)


def tube(pts, radii, m, res=8, caps=True, cyclic=False, bres=4):
    cu = bpy.data.curves.new("tube", "CURVE")
    cu.dimensions = "3D"; cu.bevel_depth = 1.0; cu.bevel_resolution = bres; cu.resolution_u = res
    cu.use_fill_caps = caps
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(pts) - 1)
    if not isinstance(radii, (list, tuple)):
        radii = [radii] * len(pts)
    for bp, p, r in zip(sp.bezier_points, pts, radii):
        bp.co = tuple(p); bp.handle_left_type = "AUTO"; bp.handle_right_type = "AUTO"; bp.radius = r
    sp.use_cyclic_u = cyclic
    sp.use_smooth = True
    cu.materials.append(mat(m))
    return _link(bpy.data.objects.new("tube", cu))


def ring(c, rx, ry, r, m, tilt=(0, 0, 0), n=24):
    """Кольцо-ремень: эллипс rx x ry в плоскости XY, повёрнутый на tilt (градусы)."""
    with grp(c, tilt):
        pts = [(rx * cos(2 * pi * i / n), ry * sin(2 * pi * i / n), 0) for i in range(n)]
        tube(pts, r, m, cyclic=True, res=4, bres=2)


def g_slab(outline, t, plane="xz", bend=None):
    """Выдавленный плоский контур толщиной t; bend(u, v) — изгиб поперёк плоскости."""
    n = len(outline)
    v = []
    for s in (-t / 2.0, t / 2.0):
        for (a, b) in outline:
            d = s + (bend(a, b) if bend else 0.0)
            v.append((a, d, b) if plane == "xz" else ((a, b, d) if plane == "xy" else (d, a, b)))
    f = [tuple(range(n))[::-1], tuple(range(n, 2 * n))]
    f += [(i, (i + 1) % n, n + (i + 1) % n, n + i) for i in range(n)]
    return v, f


def slab(outline, t, m, plane="xz", bend=None, loc=(0, 0, 0), rot=(0, 0, 0), bevel=0.0, scl=1.0):
    return mk("slab", *g_slab(outline, t, plane, bend), m, loc, rot, scl=scl, bevel=bevel)


def ell_ring(z, rx, ry, cx=0.0, cy=0.0, n=32, a0=0.0, a1=2 * pi, closed=True):
    """Точки эллипса на высоте z; угол 0 — спереди (-Y), растёт к +X."""
    cnt = n if closed else n + 1
    return [(cx + rx * sin(a0 + (a1 - a0) * i / n), cy - ry * cos(a0 + (a1 - a0) * i / n), z) for i in range(cnt)]


def loft(rings, m, closed=True, cap0=False, cap1=False, smooth=True, sub=1, solid=0.0):
    n = len(rings[0])
    v = [p for r in rings for p in r]
    f = []
    mm = n if closed else n - 1
    for k in range(len(rings) - 1):
        a, b = k * n, (k + 1) * n
        for i in range(mm):
            j = (i + 1) % n
            f.append((a + i, a + j, b + j, b + i))
    if cap0:
        cc = [sum(p[i] for p in rings[0]) / n for i in range(3)]
        v.append(cc); ci = len(v) - 1
        f += [(j if True else 0, i, ci) for i, j in ((i, (i + 1) % n) for i in range(mm))]
    if cap1:
        cc = [sum(p[i] for p in rings[-1]) / n for i in range(3)]
        v.append(cc); ci = len(v) - 1
        base = (len(rings) - 1) * n
        f += [(base + i, base + (i + 1) % n, ci) for i in range(mm)]
    return mk("loft", v, f, m, smooth=smooth, sub=sub, solid=solid)


def star(c, r, m, t=0.012, rot=(0, 0, 0), n=5):
    pts = [((r if i % 2 == 0 else r * 0.42) * cos(pi / 2 + pi * i / n),
            (r if i % 2 == 0 else r * 0.42) * sin(pi / 2 + pi * i / n)) for i in range(2 * n)]
    return slab(pts, t, m, "xz", loc=c, rot=rot)


def puff(c, r, m="smoke"):
    v, f = g_sphere(12, 8)
    random.seed(int(abs(c[0] * 1000 + c[1] * 77 + c[2] * 13)))
    v = [(x * (1 + random.uniform(-0.12, 0.12)), y * (1 + random.uniform(-0.12, 0.12)),
          z * (1 + random.uniform(-0.12, 0.12))) for (x, y, z) in v]
    return mk("puff", v, f, m, c, scl=r, smooth=True, sub=1)


# ----------------------------------------------------------------------------------------------
#  Подставка
# ----------------------------------------------------------------------------------------------
def base_disc():
    R, H = BASE_R, BASE_H
    lathe([(0, 0), (R - 0.06, 0), (R - 0.022, -0.01), (R - 0.004, -0.035), (R, -0.052)], (0, 0, 0), "base",
          segs=64, cap0=False, cap1=False)
    lathe([(R, -0.052), (R, -H + 0.03), (R - 0.018, -H + 0.004), (R - 0.035, -H), (0, -H)], (0, 0, 0),
          "base_side", segs=64, cap0=False, cap1=False)
    ring((0, 0, -0.064), R + 0.004, R + 0.004, 0.01, "brass", n=64)


# ----------------------------------------------------------------------------------------------
#  Героиня. Рост ~1.9 (голова 0.33): крупноголовая «миниатюра», читается в 60 px.
#  Все фигурки — женщины: плечи уже бёдер, тонкая талия, губы, ресницы и румянец, а главная
#  примета, заметная и в 48 px, — длинная коса с бантом: через плечо на грудь или по спине.
# ----------------------------------------------------------------------------------------------
HC = Vector((0.0, -0.005, 1.685))       # центр головы
HR = (0.142, 0.15, 0.165)               # радиусы головы
HS = 1.12                               # голова и убор чуть крупнее (масштаб вокруг HC)
SH_X, SH_Z = 0.19, 1.36                 # плечевой сустав
HAT_DZ = 0.045                          # уборы сидят чуть выше середины головы — глаза не прячутся

# сечения туловища: высота, полуоси (вбок, вперёд) и сдвиг вперёд (грудь)
TORSO = [(0.80, 0.160, 0.116, 0.012), (0.90, 0.168, 0.122, 0.012), (1.02, 0.134, 0.100, 0.0),
         (1.13, 0.148, 0.114, -0.012), (1.22, 0.164, 0.128, -0.02), (1.31, 0.172, 0.114, -0.004),
         (1.39, 0.146, 0.096, 0.0), (1.47, 0.062, 0.056, 0.0)]


def tr(z, k=1.06, kd=1.04):
    """Сечение туловища на высоте z: (полуось вбок, полуось вперёд, сдвиг вперёд)."""
    T = TORSO
    if z <= T[0][0] or z >= T[-1][0]:
        a = b = T[0] if z <= T[0][0] else T[-1]
        t = 0.0
    else:
        i = next(i for i in range(1, len(T)) if T[i][0] >= z)
        a, b = T[i - 1], T[i]
        t = (z - a[0]) / (b[0] - a[0])
    return ((a[1] + (b[1] - a[1]) * t) * k, (a[2] + (b[2] - a[2]) * t) * kd, a[3] + (b[3] - a[3]) * t)


def front_y(z, x, k=1.06):
    """Y передней поверхности туловища в точке (x, z)."""
    rx, ry, cy = tr(z, k)
    return cy - ry * sqrt(max(0.0, 1.0 - (x / rx) ** 2))


def back_y(z, x, k=1.06):
    rx, ry, cy = tr(z, k)
    return cy + ry * sqrt(max(0.0, 1.0 - (x / rx) ** 2))


def hp(dx, dy, dz):
    """Точка у головы в координатах фигурки (с учётом увеличения головы HS)."""
    return HC + Vector((dx, dy, dz)) * HS


def ik_elbow(S, H, a, b, pole):
    S, H, pole = Vector(S), Vector(H), Vector(pole)
    d = H - S
    L = d.length
    if L > (a + b) * 0.97:
        k = L / ((a + b) * 0.97); a *= k; b *= k
    dv = d / max(L, 1e-6)
    x = (a * a - b * b + L * L) / (2 * L)
    h = sqrt(max(a * a - x * x, 0.0))
    p = pole - dv * pole.dot(dv)
    if p.length < 1e-6:
        p = Vector((0, 0, -1)) - dv * (-dv.z)
    p.normalize()
    return S + dv * x + p * h


def arm(side, H, coat, hand="skin", pole=None, r=(0.068, 0.058, 0.047), cuff=None):
    S = Vector((side * SH_X, 0.0, SH_Z))
    H = Vector(H)
    pole = Vector(pole) if pole else Vector((side * 0.8, 0.5, -1.0))
    E = ik_elbow(S, H, 0.29, 0.28, pole)
    W = H + (E - H).normalized() * 0.04
    tube([S, E, W], [r[0], r[1], r[2]], coat)
    if cuff:
        cyl2(W + (E - W).normalized() * 0.02, W - (E - W).normalized() * 0.01, r[2] + 0.011, r[2] + 0.011, cuff)
    ell(H, (0.047, 0.048, 0.053), hand, segs=12, rings=8)
    return E


def leg(side, pants, boots, hip=None, knee=None, ankle=None, boot_top=0.3, foot_dir=(0, -1, 0)):
    hip = Vector(hip or (side * 0.098, 0.0, 0.86))
    ankle = Vector(ankle or (side * 0.12, 0.0, 0.09))
    knee = Vector(knee) if knee else (hip + ankle) / 2 + Vector((0, -0.035, 0))
    tube([hip, knee, ankle], [0.094, 0.07, 0.056], pants)
    if boots:
        t = max(0.0, min(1.0, (boot_top - ankle.z) / max(knee.z - ankle.z, 1e-3)))
        top = ankle + (knee - ankle) * t if boot_top < knee.z else knee + (hip - knee) * 0.1
        cyl2(top, ankle + Vector((0, 0, -0.02)), 0.07, 0.062, boots, segs=14)
        fd = Vector(foot_dir).normalized()
        ell(ankle + fd * 0.055 + Vector((0, 0, -0.03)), (0.062, 0.118, 0.056), boots,
            rot=(0, 0, math.degrees(math.atan2(fd.x, -fd.y))), segs=14, rings=8)


def torso(coat, k=1.06, kd=1.04):
    loft([ell_ring(z, rx * k, ry * kd, cy=cy, n=28) for (z, rx, ry, cy) in TORSO], coat, cap0=True, cap1=True)
    for s in (-1, 1):
        ell((s * (SH_X - 0.005), 0.0, SH_Z - 0.005), (0.08, 0.082, 0.072), coat, segs=14, rings=10)


def skirt(m, zb, rx, ry, z0=1.02, cy=0.0, a0=None, a1=None):
    """Юбка или полы одежды от талии z0 до подола zb (полуоси rx, ry); a0..a1 — только полы сзади."""
    w0, w1, c0 = tr(z0)
    sec = [(z0, w0 + 0.006, w1 + 0.006, c0)]
    if zb < 0.83:
        h0, h1, hc = tr(0.88)
        f = (z0 - 0.88) / (z0 - zb)
        sec.append((0.88, max(h0 + 0.016, w0 + (rx - w0) * f), max(h1 + 0.014, w1 + (ry - w1) * f),
                    hc + (cy - hc) * f))
    sec.append((zb, rx, ry, cy))
    if a0 is None:
        loft([ell_ring(z, a, b, cy=c) for (z, a, b, c) in sec], m, solid=0.014)
    else:
        loft([ell_ring(z, a, b, cy=c, a0=a0, a1=a1, closed=False) for (z, a, b, c) in sec], m, closed=False,
             solid=0.014)


def head(skin="skin", hair="hair", cap=True, locks=True):
    """Голова: лицо с ресницами, бровями, губами и румянцем; волосы — шапочка и пряди у висков."""
    cyl2((0, 0, 1.43), (0, 0, 1.6), 0.05, 0.048, skin, segs=12)
    ell(HC, HR, skin, segs=24, rings=16)
    ell(H(0, -0.146, -0.035), (0.017, 0.022, 0.026), skin, segs=10, rings=8)
    for s in (-1, 1):
        ell(H(s * 0.05, -0.133, 0.002), (0.021, 0.012, 0.029), "eye", segs=10, rings=6)
        ell(H(s * 0.042, -0.143, 0.013), (0.0075, 0.004, 0.008), "glint", segs=6, rings=4)
        tube([H(s * 0.026, -0.142, 0.027), H(s * 0.052, -0.146, 0.037), H(s * 0.078, -0.131, 0.033),
              H(s * 0.092, -0.12, 0.048)], [0.004, 0.006, 0.005, 0.003], "eye")
        tube([H(s * 0.024, -0.143, 0.074), H(s * 0.055, -0.14, 0.085), H(s * 0.086, -0.121, 0.076)],
             [0.005, 0.0065, 0.004], hair)
        ell(H(s * 0.078, -0.113, -0.042), (0.03, 0.016, 0.024), "blush", segs=10, rings=6)
    ell(H(0, -0.13, -0.078), (0.024, 0.008, 0.009), "lips", segs=12, rings=6)
    if cap:
        ell(H(0, 0.025, 0.035), (0.152, 0.158, 0.158), hair, segs=20, rings=12)
    if locks:
        for s in (-1, 1):
            tube([H(s * 0.012, -0.115, 0.13), H(s * 0.08, -0.13, 0.09), H(s * 0.133, -0.092, 0.02),
                  H(s * 0.15, -0.045, -0.07)], [0.02, 0.025, 0.026, 0.022], hair)


def plait(pts, hair, r0=0.05, r1=0.032, bow="ribbon", fwd=(0, -1, 0)):
    """Коса из чередующихся наклонных прядей вдоль ломаной pts; на конце — бант и кисточка."""
    P = catmull(pts, 8)
    acc = [0.0]
    for i in range(1, len(P)):
        acc.append(acc[-1] + (P[i] - P[i - 1]).length)
    tot = acc[-1]

    def at(s):
        for i in range(1, len(P)):
            if acc[i] >= s:
                t = (s - acc[i - 1]) / max(acc[i] - acc[i - 1], 1e-6)
                return P[i - 1].lerp(P[i], t), (P[i] - P[i - 1]).normalized()
        return P[-1], (P[-1] - P[-2]).normalized()
    n = max(6, int(tot / 0.04 + 0.5))
    seg = tot / n
    F = Vector(fwd)
    core = P[::4] + [P[-1]]
    tube(core, [(r0 + (r1 - r0) * i / (len(core) - 1)) * 0.62 for i in range(len(core))], hair, res=4)
    for k in range(n):
        p, d = at((k + 0.5) * seg)
        r = r0 + (r1 - r0) * k / (n - 1)
        nr = (F - d * F.dot(d)).normalized()
        a = d * cos(0.6) + d.cross(nr) * sin(0.6) * (1 if k % 2 else -1)
        with grp(matrix=look(p + d.cross(nr) * r * 0.18 * (1 if k % 2 else -1), a, nr)):
            ell((0, 0, 0), (r * 0.78, r * 0.62, seg * 1.35), hair, segs=12, rings=8)
    p, d = at(tot)
    nr = (F - d * F.dot(d)).normalized()
    with grp(matrix=look(p, d, nr)):
        lathe([(r1 * 0.95, -0.01), (r1 * 1.1, 0.03), (r1 * 0.8, 0.07), (0.0, 0.105)], (0, 0, 0), hair, segs=12)
        if bow:
            ell((0, 0, 0.0), (0.026, 0.022, 0.022), bow, segs=10, rings=6)
            for s in (-1, 1):
                ell((s * 0.05, 0.006, -0.012), (0.046, 0.016, 0.031), bow, rot=(0, s * 25, 0), segs=12, rings=6)
                cyl2((s * 0.008, 0.008, 0.0), (s * 0.034, 0.012, 0.09), 0.013, 0.009, bow, segs=6)


def braid_front(s, hair, end=1.04, x=0.135, out=0.0, bow="ribbon", r0=0.05, top=1.47):
    """Коса через плечо на грудь: s = +1 — левое плечо героини (обращено к зрителю), -1 — правое."""
    def y(z, xx):
        return front_y(z, xx) - r0 * 0.8 - out
    pts = [hp(s * 0.1, 0.07, -0.1), Vector((s * 0.155, -0.01, top)), Vector((s * x, y(1.37, x), 1.37)),
           Vector((s * (x - 0.008), y(1.22, x - 0.008), 1.22))]
    pts.append(Vector((s * (x - 0.018), min(pts[-1].y + 0.012, y(end, x - 0.018)), end)))
    plait(pts, hair, r0=r0, bow=bow)


def braid_back(hair, end=1.0, s=1, bow="ribbon", r0=0.05):
    """Коса по спине — видна у фигурок, повёрнутых к зрителю боком."""
    def y(z, xx):
        return back_y(z, xx) + r0 * 0.8
    pts = [hp(s * 0.02, 0.13, -0.02), hp(s * 0.03, 0.155, -0.13), Vector((s * 0.045, y(1.4, 0.045), 1.4)),
           Vector((s * 0.055, y(1.22, 0.055), 1.22)), Vector((s * 0.06, y(end, 0.06), end))]
    plait(pts, hair, r0=r0, bow=bow, fwd=(0, 1, 0))


def human(coat, pants="trousers_dark", boots="boots", hands=None, poles=(None, None), feet=None,
          sleeve=None, hand_m=("skin", "skin"), boot_top=0.3, k=1.06, legs=None, belt=None, skin="skin",
          hair="hair", cap=True, locks=True, braid=None, braid_end=1.04, bow="ribbon", cuff=None, hat=None,
          hat_dz=HAT_DZ, bust_only=False):
    """Стоящая (или сидящая — legs=[(hip,knee,ankle)]*2) героиня; руки тянутся к точкам hands.
    braid: "+" / "-" — коса через левое (к зрителю) / правое плечо, "+-" — две косы, "back" — по спине;
    bust_only — только верх (танкистка в люке)."""
    sleeve = sleeve or coat
    for i, s in enumerate((-1, 1)):
        if bust_only:
            break
        if legs:
            hip, kn, an = legs[i]
            leg(s, pants, boots, hip, kn, an, boot_top, foot_dir=(0, -1, -0.2))
        else:
            fx, fy = feet[i] if feet else (s * 0.12, 0.0)
            leg(s, pants, boots, ankle=(fx, fy, 0.09), boot_top=boot_top)
    torso(coat, k)
    if belt:
        rx, ry, cy = tr(1.01, k)
        ring((0, cy, 1.01), rx + 0.01, ry + 0.01, 0.022, belt)
    with grp(HC * (1 - HS), scl=HS):
        head(skin, hair, cap, locks)
        if hat:
            with grp((0, 0, hat_dz)):
                hat()
    if braid == "back":
        braid_back(hair, bow=bow)
    elif braid:
        for c in braid:
            braid_front(1 if c == "+" else -1, hair, end=braid_end, bow=bow)
    hands = hands or [(-0.25, -0.05, 0.86), (0.25, -0.05, 0.86)]
    for i, s in enumerate((-1, 1)):
        if hands[i] is not None:
            arm(s, hands[i], sleeve, hand_m[i], poles[i], cuff=cuff)


# ---------- оружие (локальная Z — вдоль оружия, Y — «верх») ----------
def spear(p0, p1, wood="wood", head_m="steel", blade=0.26, r=0.02):
    p0, p1 = Vector(p0), Vector(p1)
    d = (p1 - p0).normalized()
    cyl2(p0, p1, r, r, wood, segs=8)
    with grp(matrix=look(p1, d, (0, -1, 0))):
        cyl2((0, 0, -0.03), (0, 0, 0.05), r * 1.3, r * 1.1, head_m, segs=8)
        ell((0, 0, 0.05 + blade / 2), (0.045, 0.014, blade / 2), head_m, segs=8, rings=8)


def rifle(p0, p1, up=(0, 0, 1), bayonet=0.0, wood="wood", metal="gunmetal", scope=False):
    """Ружьё/винтовка от приклада p0 к дулу p1 (scope — оптический прицел снайпера)."""
    p0, p1 = Vector(p0), Vector(p1)
    L = (p1 - p0).length
    with grp(matrix=look(p0, p1 - p0, up)):
        hexa([(-0.026, -0.075, 0), (0.026, -0.075, 0), (0.026, 0.02, 0), (-0.026, 0.02, 0),
              (-0.022, -0.03, 0.3), (0.022, -0.03, 0.3), (0.022, 0.02, 0.3), (-0.022, 0.02, 0.3)], wood, bevel=0.006)
        box((0.04, 0.042, L * 0.62), (0, 0.0, 0.3 + L * 0.31 - 0.02), wood, bevel=0.006)
        cyl2((0, 0.018, 0.25), (0, 0.018, L), 0.014, 0.013, metal, segs=8)
        if bayonet:
            cyl2((0.02, 0.005, L - 0.05), (0.02, 0.005, L + bayonet), 0.008, 0.002, "steel", segs=6)
        if scope:
            cyl2((0, 0.068, 0.27), (0, 0.068, 0.56), 0.02, 0.02, metal, segs=10)
            cyl2((0, 0.068, 0.5), (0, 0.068, 0.6), 0.026, 0.026, metal, segs=10)
            for z in (0.34, 0.5):
                box((0.016, 0.04, 0.025), (0, 0.045, z), metal)
            ell((0, 0.068, 0.601), (0.02, 0.02, 0.004), "glass", segs=10, rings=4)


def pole_banner(p0, top, w=0.62, h=0.78, m="banner", cross="white_cloth"):
    """Хоругвь: древко, перекладина и полотнище с языками и прямым крестом."""
    p0, top = Vector(p0), Vector(top)
    cyl2(p0, top + Vector((0, 0, 0.12)), 0.022, 0.02, "wood_dark", segs=8)
    ell(top + Vector((0, 0, 0.15)), (0.035, 0.035, 0.05), "gold", segs=10, rings=8)
    with grp(top + Vector((0, 0, 0.0)), (0, 0, 0)):
        cyl2((-0.02, 0, 0), (w + 0.04, 0, 0), 0.014, 0.014, "wood_dark", segs=8)
        for x in (-0.02, w + 0.04):
            ell((x, 0, 0), 0.024, "gold", segs=8, rings=6)

        def bend(u, v):
            return 0.05 * sin(u * 7.0 + v * 2.0) + 0.03 * v
        tails = 3
        out = [(0.0, 0.0), (w, 0.0), (w, -h)]
        for i in range(tails):
            x1 = w - (i + 0.5) * w / tails
            x2 = w - (i + 1) * w / tails
            out += [(x1, -h + 0.16), (x2, -h)]
        slab(out, 0.018, m, "xz", bend)
        cw = 0.07
        cx = w / 2
        slab([(cx - cw / 2, -0.08), (cx + cw / 2, -0.08), (cx + cw / 2, -h + 0.18), (cx - cw / 2, -h + 0.18)],
             0.026, cross, "xz", bend)
        slab([(0.1, -0.28), (w - 0.1, -0.28), (w - 0.1, -0.28 - cw), (0.1, -0.28 - cw)], 0.026, cross, "xz", bend)


def flag(hand, top, m, w=0.3, h=0.22):
    """Сигнальный флажок регулировщицы: палочка от кисти к top и полотнище у верха."""
    hand, top = Vector(hand), Vector(top)
    d = (top - hand).normalized()
    cyl2(hand - d * 0.06, top, 0.011, 0.01, "wood_dark", segs=6)
    with grp(matrix=look(top, d, (0, -1, 0))):
        slab([(0.0, -0.01), (w, -0.03), (w, -h), (0.0, -h + 0.03)], 0.012, m, "xz",
             bend=lambda u, v: 0.04 * sin(u * 9.0), rot=(0, -90, 0))


def wheel(c, r, w, rim="wood", spokes=10, hub="iron", tire="iron", axis="x", spoke_m=None):
    """Деревянное колесо со спицами и железной шиной; ось колеса — X."""
    c = Vector(c)
    with grp(c, (0, 0, 0) if axis == "x" else (0, 0, 90)):
        lathe([(r, -w / 2), (r, w / 2), (r * 0.88, w / 2), (r * 0.88, -w / 2)], (0, 0, 0), tire, segs=28,
              rot=(0, 90, 0), cap0=False, cap1=False, smooth=True)
        lathe([(r * 0.885, -w / 2.6), (r * 0.885, w / 2.6), (r * 0.78, w / 2.6), (r * 0.78, -w / 2.6)], (0, 0, 0),
              rim, segs=28, rot=(0, 90, 0), cap0=False, cap1=False)
        for i in range(spokes):
            a = 2 * pi * i / spokes
            cyl2((0, 0, 0), (0, r * 0.8 * cos(a), r * 0.8 * sin(a)), r * 0.05, r * 0.035, spoke_m or rim, segs=6)
        cyl2((-w * 0.75, 0, 0), (w * 0.75, 0, 0), r * 0.14, r * 0.14, hub, segs=12)


def tyre(c, r, w, m="tire", hub="olive_dark", axis="x"):
    c = Vector(c)
    with grp(c, (0, 0, 0) if axis == "x" else (0, 0, 90)):
        lathe([(r * 0.62, -w / 2), (r * 0.94, -w / 2), (r, -w / 2 + w * 0.15), (r, w / 2 - w * 0.15),
               (r * 0.94, w / 2), (r * 0.62, w / 2)], (0, 0, 0), m, segs=24, rot=(0, 90, 0), cap0=False,
              cap1=False)
        cyl2((-w / 2 + 0.01, 0, 0), (w / 2 - 0.01, 0, 0), r * 0.63, r * 0.63, hub, segs=16)
        cyl2((-w / 2 - 0.004, 0, 0), (w / 2 + 0.004, 0, 0), r * 0.22, r * 0.22, "olive_dark", segs=10)


# ----------------------------------------------------------------------------------------------
#  Головные уборы (относительно центра головы HC)
# ----------------------------------------------------------------------------------------------
def H(dx, dy, dz):
    return HC + Vector((dx, dy, dz))


def hood(c, r, z0, m, face=1.05, brow=0.06, solid=0.022, segs=28, rings=14):
    """Оболочка вокруг головы (шлемофон, лётный шлем, платок): срезана снизу на z0, спереди — овал лица
    (|угол| < face и ниже brow)."""
    rx, ry, rz = r
    th1 = math.acos(max(-1.0, min(1.0, z0 / rz)))
    v = [(0.0, 0.0, rz)]
    for i in range(1, rings + 1):
        th = th1 * i / rings
        for j in range(segs):
            ph = 2 * pi * j / segs
            v.append((rx * sin(th) * sin(ph), -ry * sin(th) * cos(ph), rz * cos(th)))

    def vid(i, j):
        return 0 if i == 0 else 1 + (i - 1) * segs + j % segs

    def opened(i, j):
        th = th1 * (i + 0.5) / rings
        a = (2 * pi * (j + 0.5) / segs + pi) % (2 * pi) - pi
        return abs(a) < face and rz * cos(th) < brow
    f = [(0, vid(1, j), vid(1, j + 1)) for j in range(segs) if not opened(0, j)]
    for i in range(1, rings):
        f += [(vid(i, j), vid(i + 1, j), vid(i + 1, j + 1), vid(i, j + 1)) for j in range(segs) if not opened(i, j)]
    return mk("hood", v, f, m, c, smooth=True, solid=solid)


def hat_shishak():
    lathe([(0.166, 0.0), (0.168, 0.06), (0.152, 0.13), (0.118, 0.2), (0.075, 0.26), (0.035, 0.32),
           (0.014, 0.37), (0.0, 0.41)], HC, "steel", segs=28, cap0=False)
    ring(H(0, 0, 0.02), 0.169, 0.169, 0.016, "gold", n=32)
    rings = [ell_ring(HC.z + 0.03, 0.17, 0.168, cy=HC.y, a0=1.0, a1=2 * pi - 1.0, closed=False),
             ell_ring(HC.z - 0.1, 0.175, 0.172, cy=HC.y + 0.01, a0=1.0, a1=2 * pi - 1.0, closed=False),
             ell_ring(HC.z - 0.24, 0.215, 0.18, cy=HC.y + 0.02, a0=0.9, a1=2 * pi - 0.9, closed=False)]
    loft(rings, "mail", closed=False, solid=0.012)


def hat_kettle():
    with grp(H(0, 0.02, 0.03), (-16, 0, 0)):
        lathe([(0.17, 0.0), (0.168, 0.08), (0.145, 0.165), (0.09, 0.215), (0.0, 0.232)], (0, 0, 0), "steel",
              segs=28, cap0=False)
        lathe([(0.16, 0.03), (0.22, 0.01), (0.27, -0.02)], (0, 0, 0), "steel", segs=36, cap0=False, cap1=False,
              solid=0.016)
        ring((0, 0, 0.03), 0.165, 0.165, 0.012, "leather_dark", n=28)


def hat_bascinet():
    """Бацинет с открытым лицом, кольчужная бармица и красный плюмаж."""
    lathe([(0.168, 0.03), (0.168, 0.08), (0.155, 0.15), (0.125, 0.22), (0.08, 0.28), (0.035, 0.33),
           (0.0, 0.36)], HC, "steel", segs=28, cap0=False)
    lathe([(0.17, -0.1), (0.169, 0.035)], HC, "steel", segs=20, a0=1.05, a1=2 * pi - 1.05, cap0=False,
          cap1=False, solid=0.01)
    ring(H(0, 0, 0.035), 0.171, 0.171, 0.012, "gold", n=32)
    rings = [ell_ring(HC.z - 0.09, 0.173, 0.171, cy=HC.y, a0=1.1, a1=2 * pi - 1.1, closed=False),
             ell_ring(HC.z - 0.2, 0.2, 0.19, cy=HC.y + 0.01, a0=0.95, a1=2 * pi - 0.95, closed=False),
             ell_ring(HC.z - 0.29, 0.235, 0.2, cy=HC.y + 0.02, a0=0.8, a1=2 * pi - 0.8, closed=False)]
    loft(rings, "mail", closed=False, solid=0.012)
    tube([H(0, 0.02, 0.32), H(0, 0.11, 0.33), H(0, 0.2, 0.27), H(0, 0.26, 0.14)], [0.03, 0.048, 0.042, 0.02],
         "feather_red")


def hat_shako():
    lathe([(0.148, 0.04), (0.152, 0.1), (0.172, 0.32), (0.172, 0.335), (0.0, 0.34)], HC, "black_hat",
          segs=28, cap0=False)
    ring(H(0, 0, 0.32), 0.172, 0.172, 0.012, "black_hat", n=28)
    slab([(-0.06, 0), (0.06, 0), (0.07, 0.07), (0.0, 0.1), (-0.07, 0.07)], 0.012, "brass",
         loc=H(0, -0.168, 0.15), rot=(-5, 0, 0))
    slab([(-0.14, 0.0), (0.14, 0.0), (0.1, -0.06), (-0.1, -0.06)], 0.012, "black_hat", "xy",
         loc=H(0, -0.14, 0.055), rot=(-14, 0, 0))
    ell(H(0, -0.13, 0.36), 0.04, "white_cloth", segs=12, rings=8)
    ell(H(0, -0.12, 0.47), (0.035, 0.035, 0.1), "white_cloth", segs=12, rings=8)
    tube([H(-0.15, -0.06, 0.2), H(0, -0.18, 0.12), H(0.15, -0.06, 0.2)], 0.008, "white_cloth")


def hat_wide(feather="feather_red"):
    lathe([(0.155, 0.03), (0.155, 0.2), (0.14, 0.26), (0.0, 0.275)], HC, "black_hat", segs=28, cap0=False)
    with grp(H(0, 0, 0.04), (6, -8, 0)):
        lathe([(0.15, 0.0), (0.27, -0.005), (0.36, 0.03)], (0, 0, 0), "black_hat", segs=40, cap0=False, cap1=False,
              solid=0.014)
    ring(H(0, 0, 0.075), 0.157, 0.157, 0.015, "brass", n=28)
    tube([H(0.12, -0.08, 0.1), H(0.2, 0.04, 0.26), H(0.13, 0.22, 0.3), H(0.0, 0.3, 0.2)],
         [0.025, 0.05, 0.045, 0.02], feather)


def hat_szyszak():
    lathe([(0.168, 0.0), (0.166, 0.09), (0.135, 0.175), (0.075, 0.23), (0.018, 0.27), (0.0, 0.33)], HC,
          "steel", segs=28, cap0=False)
    ring(H(0, 0, 0.02), 0.17, 0.17, 0.012, "gold", n=32)
    rings = [ell_ring(HC.z + 0.02, 0.172, 0.17, cy=HC.y, a0=pi - 1.3, a1=pi + 1.3, closed=False),
             ell_ring(HC.z - 0.14, 0.22, 0.22, cy=HC.y + 0.03, a0=pi - 1.3, a1=pi + 1.3, closed=False)]
    loft(rings, "steel", closed=False, solid=0.012)
    for s in (-1, 1):
        slab([(0, 0), (0.07, 0), (0.06, -0.15), (0.0, -0.12)], 0.01, "steel", loc=H(s * 0.17, -0.06, 0.0),
             rot=(0, 0, 90 + s * 8), scl=(s, 1, 1))
    box((0.016, 0.012, 0.12), H(0, -0.175, -0.0), "gold", rot=(-6, 0, 0))
    slab([(-0.12, 0), (0.12, 0), (0.08, -0.06), (-0.08, -0.06)], 0.01, "steel", "xy", loc=H(0, -0.15, 0.03),
         rot=(-20, 0, 0))


def hat_fur_cap(top="red", feather=None):
    lathe([(0.165, 0.0), (0.172, 0.1), (0.168, 0.2), (0.0, 0.215)], HC, "fur_dark", segs=28, cap0=False)
    ell(H(0.04, 0.05, 0.22), (0.12, 0.13, 0.07), top, rot=(0, 20, 0))
    ell(H(0.16, 0.07, 0.17), (0.04, 0.04, 0.05), "gold", segs=10, rings=6)
    if feather:
        ell(H(-0.1, -0.12, 0.17), (0.03, 0.02, 0.03), "gold", segs=10, rings=6)
        tube([H(-0.1, -0.12, 0.18), H(-0.14, -0.08, 0.33), H(-0.13, 0.02, 0.46)], [0.022, 0.032, 0.01], feather)


def hat_pilotka(m="khaki", star_on=True):
    """Пилотка набекрень со звёздочкой."""
    rings = []
    for y in (-0.165, -0.12, -0.05, 0.03, 0.1, 0.155):
        t = abs(y) / 0.165
        w, hh, zb = 0.104 * (1 - 0.2 * t * t), 0.12 * (1 - 0.45 * t ** 3), -0.05 - 0.015 * t
        rings.append([(-w, y, zb), (w, y, zb), (w * 0.8, y, zb + hh * 0.55), (0.0, y, zb + hh),
                      (-w * 0.8, y, zb + hh * 0.55)])
    with grp(H(-0.03, 0.0, 0.15), (0, -14, 0)):
        loft(rings, m, cap0=True, cap1=True, sub=1)
        for s in (-1, 1):
            tube([(s * 0.098, -0.1, -0.026), (s * 0.101, 0.0, -0.022), (s * 0.098, 0.1, -0.026)], 0.01, m)
        if star_on:
            star((0.0, -0.176, 0.006), 0.03, "red_star", t=0.016)


def hat_kerchief(m="kerchief", hair="hair"):
    """Платок в горошек, повязанный под подбородком, — из-под него видна чёлка."""
    hood(H(0, 0.012, 0.01), (0.163, 0.17, 0.182), -0.14, m, face=1.15, brow=0.095, solid=0.018)
    tube([H(-0.13, -0.085, -0.15), H(-0.152, -0.09, -0.02), H(-0.12, -0.13, 0.085), H(0.0, -0.172, 0.11),
          H(0.12, -0.13, 0.085), H(0.152, -0.09, -0.02), H(0.13, -0.085, -0.15)], 0.017, m)
    tube([H(-0.1, -0.128, 0.06), H(-0.04, -0.148, 0.085), H(0.04, -0.148, 0.085), H(0.1, -0.128, 0.06)],
         [0.016, 0.022, 0.022, 0.016], hair)
    ell(H(0.0, -0.1, -0.195), (0.04, 0.03, 0.028), m, segs=12, rings=8)
    for s in (-1, 1):
        ell(H(s * 0.04, -0.13, -0.235), (0.022, 0.012, 0.052), m, rot=(-20, s * 25, 0), segs=10, rings=6)
    slab([(-0.13, 0.0), (0.13, 0.0), (0.0, -0.24)], 0.012, m, "xz", loc=H(0, 0.165, -0.04), rot=(12, 0, 0))


def hat_tank():
    """Танковый шлемофон: чёрный, с тремя валиками сверху и наушниками."""
    hood(H(0, 0.006, 0.0), (0.164, 0.172, 0.18), -0.12, "tank_helmet", face=1.0, brow=0.07, solid=0.02)
    for x in (-0.065, 0.0, 0.065):
        tube([H(x, -0.14, 0.11), H(x, -0.07, 0.178), H(x, 0.03, 0.193), H(x, 0.12, 0.155), H(x, 0.165, 0.05)],
             0.022, "tank_helmet")
    for s in (-1, 1):
        cyl2(H(s * 0.155, 0.0, -0.035), H(s * 0.19, 0.0, -0.035), 0.06, 0.055, "tank_helmet", segs=16)
        tube([H(s * 0.16, -0.03, -0.08), H(s * 0.12, -0.08, -0.15), H(0.0, -0.1, -0.19)], 0.009, "leather_dark")


def hat_flight():
    """Кожаный лётный шлем с очками на лбу."""
    hood(H(0, 0.006, 0.0), (0.163, 0.171, 0.18), -0.13, "flight_leather", face=1.02, brow=0.07, solid=0.02)
    n = Vector((0, -0.8, 0.6)).normalized()
    ring(H(0, 0.0, 0.11), 0.175, 0.178, 0.012, "leather_dark", tilt=(14, 0, 0))
    for s in (-1, 1):
        p = H(s * 0.058, -0.148, 0.12)
        cyl2(p - n * 0.01, p + n * 0.03, 0.046, 0.044, "brass", segs=16)
        cyl2(p + n * 0.025, p + n * 0.033, 0.036, 0.036, "canopy", segs=16)
        ell(H(s * 0.16, 0.0, -0.04), (0.03, 0.05, 0.06), "flight_leather", segs=10, rings=6)


def hat_forage():
    """Фуражная шапка-бескозырка XIX века с красным околышем."""
    with grp(HC, (0, -8, 0)):
        lathe([(0.152, 0.0), (0.156, 0.05), (0.168, 0.1), (0.172, 0.118), (0.0, 0.125)], (0, 0, 0), "green_coat",
              segs=28, cap0=False)
        lathe([(0.158, -0.002), (0.159, 0.05)], (0, 0, 0), "red", segs=28, cap0=False, cap1=False, solid=0.008)
        ell((0, -0.17, 0.08), 0.016, "brass", segs=8, rings=6)


def hat_peaked(top="olive_uniform", band="red"):
    """Офицерская фуражка: околыш, широкая тулья, лакированный козырёк, кокарда."""
    lathe([(0.152, 0.0), (0.154, 0.06)], HC, band, segs=28, cap0=False, cap1=False, solid=0.01)
    with grp(HC, scl=(1.0, 1.08, 1.0)):
        lathe([(0.152, 0.055), (0.16, 0.085), (0.205, 0.125), (0.206, 0.14), (0.0, 0.15)], (0, 0, 0), top,
              segs=32, cap0=False)
    slab([(-0.13, 0.0), (0.13, 0.0), (0.09, -0.07), (0.0, -0.085), (-0.09, -0.07)], 0.012, "black", "xy",
         loc=H(0, -0.13, 0.01), rot=(-22, 0, 0), bend=lambda u, v: 1.2 * u * u)
    star(H(0, -0.162, 0.06), 0.022, "red_star", t=0.01)
    ell(H(0, -0.16, 0.06), (0.032, 0.008, 0.026), "gold", segs=10, rings=6)


def hat_helmet(m="camo_green"):
    """Современный кевларовый шлем в чехле и гарнитура связи."""
    c = HC + Vector((0, 0.012, 0))
    lathe([(0.178, 0.035), (0.176, 0.08), (0.158, 0.15), (0.115, 0.205), (0.058, 0.232), (0.0, 0.238)], c, m,
          segs=32, cap0=False)
    lathe([(0.18, -0.09), (0.178, 0.04)], c, m, segs=20, a0=0.95, a1=2 * pi - 0.95, cap0=False, cap1=False,
          solid=0.014)
    tube([H(-0.17, 0.0, -0.06), H(0, -0.08, -0.2), H(0.17, 0.0, -0.06)], 0.008, "black")
    for s in (-1, 1):
        cyl2(H(s * 0.168, 0.0, -0.03), H(s * 0.2, 0.0, -0.03), 0.048, 0.046, "black", segs=14)


# ----------------------------------------------------------------------------------------------
#  Фигурки
# ----------------------------------------------------------------------------------------------
def beam(p1, p2, t, m, t2=None, bevel=0.0):
    """Брус сечением t x t2 между двумя точками."""
    p1, p2 = Vector(p1), Vector(p2)
    t2 = t if t2 is None else t2
    d = (p2 - p1).normalized()
    up = Vector((0, 0, 1)) if abs(d.z) < 0.95 else Vector((1, 0, 0))
    a = d.cross(up).normalized() * (t / 2.0)
    b = d.cross(a).normalized() * (t2 / 2.0)
    v = [p1 - a - b, p1 + a - b, p1 + a + b, p1 - a + b, p2 - a - b, p2 + a - b, p2 + a + b, p2 - a + b]
    return mk("beam", v, BOXF, m, bevel=bevel)


def catmull(pts, n):
    """Точки сглаженной ломаной (Катмулл — Ром), n отрезков на звено."""
    P = [Vector(p) for p in pts]
    P = [P[0] * 2 - P[1]] + P + [P[-1] * 2 - P[-2]]
    out = []
    for i in range(1, len(P) - 2):
        p0, p1, p2, p3 = P[i - 1], P[i], P[i + 1], P[i + 2]
        for k in range(n):
            t = k / n
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
                              (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    out.append(P[-2])
    return out


def on_chest(x, z, d=0.0):
    """Точка на передней поверхности туловища (для пуговиц, медалей, нашивок)."""
    return Vector((x, front_y(z, x) - d, z))


def horse(coat, mane, hoof="black", walk=True):
    """Лошадь мордой к -Y; спина ~1.24, холка ~1.3."""
    ell((0, 0.04, 0.98), (0.235, 0.55, 0.255), coat, segs=24, rings=14)
    ell((0, -0.3, 1.01), (0.225, 0.27, 0.275), coat)
    ell((0, 0.38, 1.0), (0.24, 0.27, 0.27), coat)
    tube([(0, -0.36, 1.06), (0, -0.55, 1.33), (0, -0.66, 1.53)], [0.17, 0.125, 0.1], coat)
    with grp((0, -0.7, 1.56), (52, 0, 0)):
        ell((0, -0.16, 0), (0.085, 0.21, 0.095), coat)
        ell((0, -0.33, -0.015), (0.075, 0.075, 0.082), coat)
        for s in (-1, 1):
            cyl2((s * 0.045, 0.05, 0.06), (s * 0.06, 0.13, 0.16), 0.03, 0.006, coat, segs=8)
            ell((s * 0.075, -0.1, 0.035), (0.013, 0.022, 0.016), "eye", segs=8, rings=6)
        ring((0, -0.2, 0), 0.09, 0.03, 0.012, "leather_dark", tilt=(0, 90, 0))
        ring((0, -0.3, -0.01), 0.082, 0.08, 0.01, "leather_dark", tilt=(90, 0, 0))
    tube([(0, -0.26, 1.28), (0, -0.45, 1.5), (0, -0.6, 1.7), (0, -0.7, 1.74)], [0.04, 0.06, 0.055, 0.03], mane)
    tube([(0, 0.62, 1.12), (0, 0.8, 0.98), (0, 0.86, 0.74), (0, 0.83, 0.48)], [0.05, 0.08, 0.075, 0.04], mane)
    for (x, y, front) in ((-0.12, -0.34, True), (0.12, -0.34, True), (-0.13, 0.42, False), (0.13, 0.42, False)):
        top = Vector((x, y, 0.95))
        if walk and front and x > 0:
            knee, fet = Vector((x, y - 0.2, 0.6)), Vector((x, y - 0.06, 0.36))
            hf = fet + Vector((0, 0.04, -0.07))
        elif walk and not front and x < 0:
            knee, fet = Vector((x, y + 0.05, 0.52)), Vector((x, y + 0.14, 0.12))
            hf = fet + Vector((0, 0.03, -0.1))
        else:
            knee, fet = Vector((x, y + (0.0 if front else 0.06), 0.5)), Vector((x, y, 0.12))
            hf = fet + Vector((0, -0.01, -0.11))
        tube([top, knee, fet], [0.085, 0.055, 0.045], coat)
        cyl2(fet + Vector((0, 0, 0.01)), hf, 0.05, 0.058, hoof, segs=10)


def wing(s, pts, n_feather=13, frame="wood_red", feather="feather"):
    """Гусарское крыло: деревянная дуга и ряд перьев назад-наружу."""
    tube(pts, [0.024] * len(pts), frame)
    P = catmull(pts, 6)
    for i in range(n_feather):
        t = (i + 0.5) / n_feather
        q = P[int(t * (len(P) - 1))]
        L = 0.17 + 0.17 * sin(pi * t)
        d = Vector((s * 0.6, 1.0, -0.1 - 0.6 * t)).normalized()
        with grp(matrix=look(q, d, (0, 0, 1))):
            ell((0, 0, L / 2), (0.05, 0.014, L / 2 + 0.01), feather, segs=10, rings=6)


def u_druzhina():
    """Дружинница: шишак с бармицей, кольчуга, красный плащ, копьё и каплевидный щит; коса через плечо."""
    with grp((0.03, 0.08, 0)):
        rings = [ell_ring(1.42, 0.2, 0.145, cy=0.02, a0=pi / 2 - 0.25, a1=3 * pi / 2 + 0.25, closed=False),
                 ell_ring(1.0, 0.27, 0.2, cy=0.06, a0=pi / 2 - 0.35, a1=3 * pi / 2 + 0.35, closed=False),
                 ell_ring(0.3, 0.36, 0.27, cy=0.16, a0=pi / 2 - 0.45, a1=3 * pi / 2 + 0.45, closed=False)]
        loft(rings, "red", closed=False, solid=0.02)
        skirt("mail", 0.56, 0.235, 0.19)
        human("mail", pants="trousers_blue", boots="boots", feet=[(-0.13, -0.04), (0.12, 0.06)],
              hands=[(-0.34, -0.12, 1.06), (0.2, -0.26, 1.06)], poles=[(-1, 0.3, -0.6), (1, 0.4, -0.4)],
              belt="leather", hair="hair_fair", cap=False, locks=False, hat=hat_shishak, braid="-", braid_end=1.0)
        spear((-0.34, -0.12, 0.0), (-0.34, -0.12, 1.95))
        with grp((0.17, -0.37, 0.96), (0, 0, 14)):
            w, h1, h2 = 0.44, 0.27, 0.64
            out = [(w / 2 * cos(a), h1 - w / 2 + w / 2 * sin(a)) for a in [pi * i / 12 for i in range(13)]]
            out += [(-w / 2 + 0.03, h1 - w / 2 - 0.15), (0.0, -h2 + 0.04), (w / 2 - 0.03, h1 - w / 2 - 0.15)]
            bend = lambda u, v: 0.55 * u * u
            slab(out, 0.03, "shield_red", "xz", bend, bevel=0.005)
            ell((0, -0.04, 0.05), (0.065, 0.035, 0.065), "steel", segs=14, rings=8)
            rim = [(u, 0.55 * u * u - 0.022, v) for (u, v) in out] + [(out[0][0], 0.55 * out[0][0] ** 2 - 0.022, out[0][1])]
            tube(rim, 0.012, "gold", res=2)


def u_crossbow():
    """Арбалетчица: стёганый гамбезон, шапель, арбалет наизготовку; позади расписная павеза."""
    with grp((0.25, 0.55, 0.0), (-12, 0, 0)):
        w, h = 0.62, 1.3
        out = [(-w / 2, 0), (w / 2, 0), (w / 2, h - 0.08), (w / 2 - 0.08, h), (-w / 2 + 0.08, h), (-w / 2, h - 0.08)]
        bend = lambda u, v: 0.5 * u * u
        slab(out, 0.04, "pavise_a", "xz", bend, bevel=0.006)
        slab([(-0.055, 0.04), (0.055, 0.04), (0.055, h - 0.04), (-0.055, h - 0.04)], 0.07, "pavise_b", "xz", bend)
        for k in range(3):
            z = 0.25 + k * 0.38
            slab([(-0.2, z), (-0.1, z + 0.1), (-0.2, z + 0.2), (-0.3 + 0.02, z + 0.1)], 0.06, "pavise_b", "xz", bend)
            slab([(0.2, z), (0.3 - 0.02, z + 0.1), (0.2, z + 0.2), (0.1, z + 0.1)], 0.06, "pavise_b", "xz", bend)
        rim = [(u, 0.5 * u * u - 0.024, v) for (u, v) in out] + [(out[0][0], 0.5 * out[0][0] ** 2 - 0.024, 0)]
        tube(rim, 0.014, "wood_dark", res=2)
        cyl2((0, 0.05, 0.9), (0, 0.55, 0.0), 0.016, 0.016, "wood_dark", segs=6)
    with grp((-0.12, 0.0, 0)):
        skirt("gambeson", 0.52, 0.235, 0.19)
        human("gambeson", pants="trousers_dark", boots="boots", feet=[(-0.13, 0.08), (0.11, -0.1)],
              hands=[(-0.12, -0.2, 1.08), (0.0, -0.5, 1.2)], poles=[(-1, 0.2, -0.8), (1, 0.2, -0.8)],
              belt="leather", hair="hair_fair", hat=hat_kettle, braid="-", braid_end=1.15)
        ell((0.19, 0.0, 0.88), (0.06, 0.1, 0.13), "leather", segs=12, rings=8)
        p0 = Vector((-0.12, 0.05, 1.0)); p1 = Vector((0.0, -0.75, 1.28))
        with grp(matrix=look(p0, p1 - p0, (0, 0, 1))):
            box((0.065, 0.085, 0.9), (0, 0, 0.45), "wood_dark", bevel=0.012)
            tube([(-0.4, 0.01, 0.74), (0, 0.0, 0.86), (0.4, 0.01, 0.74)], [0.024, 0.04, 0.024], "wood_red")
            tube([(-0.4, 0.015, 0.74), (0, 0.03, 0.5), (0.4, 0.015, 0.74)], 0.007, "rope", res=2)
            ring((0, 0.0, 0.9), 0.05, 0.035, 0.009, "steel_dark", tilt=(90, 0, 0))
            box((0.03, 0.03, 0.2), (0, 0.045, 0.6), "steel_dark")


def u_khorugv():
    """Рыцарь хоругви: латы, бацинет с открытым лицом, красный сюрко с крестом, хоругвь на древке."""
    with grp((0.1, 0.1, 0)):
        skirt("crimson", 0.55, 0.235, 0.19)
        human("steel", pants="steel", boots="steel_dark", feet=[(-0.13, -0.02), (0.13, 0.04)],
              hands=[(-0.34, -0.1, 1.12), (0.26, -0.12, 0.98)], poles=[(-1, 0.3, -0.6), (1, 0.3, -0.6)],
              sleeve="steel", hand_m=("steel_dark", "steel_dark"), boot_top=0.5, belt="leather_dark",
              hair="hair_blond", cap=False, hat=hat_bascinet, braid="+", braid_end=1.0)
        rings = [ell_ring(z, rx * 1.06 + 0.012, ry * 1.04 + 0.012, cy=cy, n=28) for (z, rx, ry, cy) in TORSO[1:6]]
        loft(rings, "crimson", sub=1)
        fy = lambda u, v: front_y(v, u) - 0.026
        slab([(-0.028, 1.03), (0.028, 1.03), (0.028, 1.29), (-0.028, 1.29)], 0.01, "white_cloth", bend=fy)
        slab([(-0.1, 1.17), (0.1, 1.17), (0.1, 1.225), (-0.1, 1.225)], 0.01, "white_cloth", bend=fy)
        for s in (-1, 1):
            ell((s * (SH_X + 0.01), 0.0, SH_Z + 0.03), (0.105, 0.105, 0.072), "steel", segs=14, rings=10)
        cyl2((0.2, -0.02, 0.98), (0.3, 0.12, 0.3), 0.022, 0.02, "leather_dark", segs=8)
        box((0.17, 0.025, 0.028), (0.19, -0.04, 1.03), "gold", rot=(0, 0, 30))
        with grp((-0.34, -0.1, 0.0), (0, 0, 190)):
            pole_banner((0, 0, 0.0), (0, 0, 1.96), w=0.64, h=0.8)


def u_gunners():
    """Пушкарка: бронзовая пушка на красном лафете и героиня с дымящимся пальником."""
    with grp((0.12, -0.12, 0), (0, 0, -68)):
        cheek = [(-0.3, 0.3), (-0.3, 0.46), (-0.04, 0.53), (0.12, 0.5), (0.7, 0.08), (0.7, 0.0), (0.52, 0.0),
                 (0.02, 0.28)]
        for s in (-1, 1):
            slab(cheek, 0.06, "wood_red", "yz", loc=(s * 0.13, 0, 0), bevel=0.006)
        box((0.32, 0.12, 0.06), (0, 0.6, 0.04), "wood_red")
        cyl2((-0.36, -0.06, 0.3), (0.36, -0.06, 0.3), 0.035, 0.035, "iron", segs=10)
        for s in (-1, 1):
            wheel((s * 0.31, -0.06, 0.3), 0.3, 0.07, rim="wood", spokes=8)
        with grp(matrix=look((0, -0.05, 0.53), (0, -1, 0.1), (0, 0, 1))):
            lathe([(0.0, -0.4), (0.05, -0.39), (0.06, -0.35), (0.03, -0.33), (0.11, -0.31), (0.125, -0.26),
                   (0.12, -0.05), (0.13, -0.03), (0.13, 0.0), (0.105, 0.02), (0.09, 0.62), (0.1, 0.64),
                   (0.1, 0.67), (0.085, 0.69), (0.11, 0.75), (0.11, 0.79), (0.055, 0.79), (0.055, 0.72)],
                  (0, 0, 0), "bronze", segs=24, cap0=False, cap1=False)
            cyl2((-0.16, 0, 0.0), (0.16, 0, 0.0), 0.035, 0.035, "bronze", segs=10)
        puff((0.0, -1.0, 0.62), 0.13)
        puff((-0.06, -1.14, 0.7), 0.1)
        puff((0.05, -1.22, 0.62), 0.08)
    with grp((0.18, 0.42, 0), (0, 0, -32)):
        skirt("red", 0.48, 0.25, 0.19)
        human("red", pants="trousers_blue", boots="boots", feet=[(-0.12, 0.0), (0.12, 0.02)],
              hands=[(-0.26, -0.06, 0.92), (0.3, -0.22, 1.12)], poles=[(-1, 0.6, -1), (1, 0.3, -0.6)],
              belt="leather", hair="hair", hat=hat_fur_cap, braid="+", braid_end=1.02)
        p0 = Vector((0.28, -0.18, 0.0)); p1 = Vector((0.33, -0.25, 1.86))
        cyl2(p0, p1, 0.018, 0.016, "wood_dark", segs=6)
        for s in (-1, 1):
            cyl2(p1, p1 + Vector((s * 0.05, -0.01, 0.08)), 0.01, 0.008, "iron", segs=6)
            ell(p1 + Vector((s * 0.055, -0.01, 0.1)), 0.022, "ember", segs=8, rings=6)
        puff(p1 + Vector((0.02, 0.0, 0.2)), 0.05, "smoke")
        puff(p1 + Vector((0.08, 0.04, 0.31)), 0.04, "smoke")


def u_musketeers():
    """Мушкетёрка: широкополая шляпа, синий кафтан, бандольер, мушкет на сошке."""
    with grp((0.3, 0.2, 0), (0, 0, 18)):
        skirt("blue_coat", 0.5, 0.255, 0.2)
        az = 1.32
        human("blue_coat", pants="trousers_dark", boots="boots", feet=[(-0.16, 0.06), (0.14, -0.16)],
              hands=[(-0.15, -0.2, az - 0.04), (-0.11, -0.62, az - 0.02)],
              poles=[(-1, 0.5, -1), (1, -0.2, -1)], belt="leather", cuff="red", hair="hair_red",
              hat=hat_wide, braid="back")
        tl = radians(-40)
        ring((0, -0.01, 1.2), 0.235, 0.16, 0.026, "leather", tilt=(0, -40, 0))
        for k in range(6):
            t = 3 * pi / 2 + (k - 2.2) * 0.33
            x0, y0 = 0.245 * cos(t), 0.175 * sin(t)
            p = Vector((x0 * cos(tl), y0 - 0.01, 1.2 - x0 * sin(tl)))
            cyl2(p + Vector((0, -0.02, -0.02)), p + Vector((0, -0.02, -0.13)), 0.024, 0.022, "wood_light", segs=8)
            cyl2(p + Vector((0, -0.02, -0.0)), p + Vector((0, -0.02, -0.025)), 0.026, 0.026, "red", segs=8)
        p0 = Vector((-0.15, 0.0, az)); p1 = Vector((-0.11, -1.42, az + 0.07))
        rifle(p0, p1, up=(0, 0, 1))
        f = Vector((-0.11, -0.7, az + 0.0))
        cyl2((-0.12, -0.74, 0.0), f, 0.016, 0.016, "wood_dark", segs=6)
        for s in (-1, 1):
            cyl2(f, f + Vector((s * 0.05, 0, 0.08)), 0.011, 0.008, "iron", segs=6)


def u_hussars():
    """Крылатая гусария: всадница на серой лошади, крылья, леопардовая шкура, копьё с прапорцом."""
    with grp((0.05, 0.05, 0), (0, 0, 0)):
        horse("horse", "horse_mane")
        ell((0, 0.03, 1.2), (0.27, 0.38, 0.075), "crimson", segs=20, rings=10)
        ring((0, 0.03, 1.17), 0.27, 0.38, 0.014, "gold", n=32)
        with grp((0, 0.02, 0.4)):
            legs = [((s * 0.1, 0.0, 0.86), (s * 0.29, -0.2, 0.66), (s * 0.27, -0.06, 0.3)) for s in (-1, 1)]
            human("crimson", pants="crimson", boots="boots_yellow", legs=legs, boot_top=0.6,
                  hands=[(-0.27, -0.18, 1.12), (0.12, -0.42, 1.06)], poles=[(-1, 0.4, -0.6), (1, 0.3, -1)],
                  belt="gold", hair="hair_blond", hat=hat_szyszak, braid="+", braid_end=1.04)
            rings = [ell_ring(z, rx * 1.06 + 0.012, ry * 1.04 + 0.014, cy=cy, n=28, a0=-1.1, a1=1.1, closed=False)
                     for (z, rx, ry, cy) in TORSO[2:6]]
            loft(rings, "steel", closed=False, sub=1, solid=0.01)
            rings = [ell_ring(1.45, 0.2, 0.15, cy=0.01, a0=pi / 2 - 0.9, a1=3 * pi / 2 - 0.2, closed=False),
                     ell_ring(1.12, 0.27, 0.2, cy=0.04, a0=pi / 2 - 0.7, a1=3 * pi / 2 - 0.1, closed=False),
                     ell_ring(0.78, 0.33, 0.27, cy=0.1, a0=pi / 2 - 0.4, a1=3 * pi / 2, closed=False)]
            loft(rings, "leopard", closed=False, solid=0.02)
            for s in (-1, 1):
                wing(s, [(s * 0.13, 0.2, 1.0), (s * 0.2, 0.34, 1.5), (s * 0.2, 0.26, 1.98), (s * 0.15, 0.04, 2.22)],
                     n_feather=17)
            p0 = Vector((-0.28, 0.42, 0.72)); hnd = Vector((-0.27, -0.18, 1.12))
            d = (hnd - p0).normalized()
            tip = p0 + d * 2.45
            cyl2(p0, tip, 0.022, 0.014, "lance", segs=8)
            ell(hnd + d * 0.12, (0.04, 0.04, 0.06), "gold", segs=10, rings=6)
            cyl2(tip, tip + d * 0.12, 0.016, 0.0, "steel", segs=8)
            with grp(matrix=look(tip, d, (0, 0, 1))):
                bend = lambda a, b: 0.04 * sin(b * 9.0)
                slab([(0.0, -0.04), (-0.03, -0.2), (-0.19, -0.78), (-0.11, -0.6), (-0.06, -0.78), (0.0, -0.3)],
                     0.012, "crimson", "yz", bend)
                slab([(-0.03, -0.2), (-0.12, -0.55), (-0.19, -0.78), (-0.11, -0.6), (-0.06, -0.62)],
                     0.016, "gold_cloth", "yz", bend)


def rocket(q, dd, r=0.065, body=0.46, stick=0.9):
    """Пороховая ракета: бумажная гильза в полоску, чёрная головка, направляющий шест."""
    with grp(matrix=look(q, dd, (0, 0, 1))):
        cyl2((0, 0, 0), (0, 0, body), r, r, "rocket_red", segs=14)
        cyl2((0, 0, body), (0, 0, body + r * 2.4), r * 1.08, 0.0, "black", segs=14)
        cyl2((0, 0, -0.04), (0, 0, 0.0), r * 0.7, r * 0.9, "black", segs=12)
        cyl2((0, -r - 0.012, 0.06), (0, -r - 0.012, -stick), 0.014, 0.011, "wood_light", segs=6)


def u_rockets():
    """Ракетная батарея Семеновича: станок с ракетами, взлетающая ракета в дыму и ракетчица с пальником."""
    ang = radians(40)
    d = Vector((0, -cos(ang), sin(ang)))
    n = Vector((0, sin(ang), cos(ang)))
    P0 = Vector((0.0, 0.52, 0.06))
    L = 1.2
    with grp((-0.16, 0.0, 0), scl=0.9):
        for s in (-1, 1):
            beam(P0 + Vector((s * 0.2, 0, 0)), P0 + d * L + Vector((s * 0.2, 0, 0)), 0.075, "wood", bevel=0.01)
            f = P0 + d * (L * 0.8) + Vector((s * 0.2, 0, 0))
            beam(f, Vector((s * 0.33, f.y - 0.1, 0.0)), 0.065, "wood", bevel=0.008)
        for k in range(4):
            q = P0 + d * (0.12 + k * 0.32) - n * 0.03
            beam(q + Vector((-0.24, 0, 0)), q + Vector((0.24, 0, 0)), 0.04, "wood_light", bevel=0.005)
        q = P0 + d * (L * 0.8) + Vector((0, -0.1, -0.35))
        beam(q + Vector((-0.31, 0, 0)), q + Vector((0.31, 0, 0)), 0.045, "wood_light", bevel=0.005)
        for (x, t) in ((-0.1, 0.5), (0.1, 0.42)):
            rocket(P0 + d * L * t + n * 0.085 + Vector((x, 0, 0)), d, stick=0.55)
        d2 = Vector((0, -cos(ang + 0.15), sin(ang + 0.15)))
        q = P0 + d * (L + 0.5) + n * 0.12
        rocket(q, d2, r=0.075, body=0.5, stick=0.75)
        rx = 90 - math.degrees(ang + 0.15)
        ell(q - d2 * 0.1, (0.085, 0.085, 0.16), "fire", rot=(rx, 0, 0))
        ell(q - d2 * 0.07, (0.05, 0.05, 0.09), "fire_core", rot=(rx, 0, 0))
        for k, r in enumerate((0.08, 0.1, 0.12, 0.13, 0.14)):
            puff(q - d2 * (0.28 + k * 0.2) + Vector((0.025 * k, 0.03 * k, -0.025 * k * k)), r)
        for (x, y, z, r) in ((0.0, 0.66, 0.12, 0.15), (0.2, 0.72, 0.16, 0.12), (-0.18, 0.7, 0.14, 0.12)):
            puff((x, y, z), r)
        box((0.3, 0.2, 0.16), (-0.5, 0.25, 0.08), "wood_dark", rot=(0, 0, -20), bevel=0.01)
        for k in range(3):
            rocket(Vector((-0.55 + 0.06 * k, 0.22 + 0.03 * k, 0.2)), Vector((0.42, -0.9, 0.05)), r=0.04,
                   body=0.22, stick=0.2)
    with grp((0.46, 0.18, 0), (0, 0, 14), scl=0.86):
        skirt("teal_coat", 0.5, 0.25, 0.19)
        human("teal_coat", pants="trousers_dark", boots="boots_yellow", boot_top=0.36,
              feet=[(-0.13, -0.02), (0.12, 0.04)], hands=[(-0.3, -0.22, 1.1), (0.25, -0.04, 0.86)],
              poles=[(-1, 0.3, -0.7), (1, 0.3, -1)], belt="gold", cuff="gold_cloth", hair="hair_fair",
              hat=lambda: hat_fur_cap("crimson", "feather"), braid="+", braid_end=1.02)
        p0 = Vector((-0.2, -0.12, 0.62)); p1 = Vector((-0.56, -0.4, 1.32))
        cyl2(p0, p1, 0.017, 0.015, "wood_dark", segs=6)
        ell(p1 + (p1 - p0).normalized() * 0.03, 0.026, "ember", segs=8, rings=6)
        puff(p1 + Vector((-0.02, 0.0, 0.12)), 0.045, "smoke")


def u_line():
    """Линейная пехота XIX века: тёмно-зелёный мундир, белые перевязи, кивер, ружьё со штыком."""
    with grp((0.04, 0.05, 0)):
        skirt("green_coat", 0.6, 0.22, 0.19, a0=pi / 2 + 0.3, a1=3 * pi / 2 - 0.3)
        human("green_coat", pants="white_cloth", boots="boots_black", boot_top=0.36, feet=[(-0.12, 0.0), (0.12, 0.0)],
              hands=[(-0.32, -0.08, 1.12), (0.26, 0.0, 0.84)], poles=[(-1, 0.4, -0.6), (1, 0.2, -1)],
              belt="white_cloth", cuff="red", hair="hair", hat=hat_shako, braid="+", braid_end=1.04)
        for t in (38, -38):
            ring((0, -0.012, 1.17), 0.228, 0.158, 0.02, "white_cloth", tilt=(0, t, 0))
        ell((0, -0.07, 1.445), (0.09, 0.05, 0.035), "red", segs=14, rings=8)
        for z in (1.08, 1.17, 1.26, 1.35):
            ell(on_chest(0, z, 0.004), 0.013, "brass", segs=8, rings=6)
        ell((0.2, 0.05, 0.92), (0.07, 0.1, 0.09), "black", segs=12, rings=8)
        rifle((-0.32, -0.08, 0.0), (-0.32, -0.08, 1.42), up=(0, -1, 0), bayonet=0.38)


def u_artillery():
    """Полевая пушка XIX века на зелёном лафете и канонирка с банником; рядом ядра."""
    with grp((-0.26, 0.06, 0), (0, 0, -60), scl=0.84):
        for s in (-1, 1):
            wheel((s * 0.38, 0.0, 0.5), 0.5, 0.075, rim="gun_green", spokes=12, hub="iron", tire="iron")
        cyl2((-0.44, 0, 0.5), (0.44, 0, 0.5), 0.04, 0.04, "iron", segs=10)
        hexa([(-0.12, -0.12, 0.42), (0.12, -0.12, 0.42), (0.1, 0.88, 0.0), (-0.1, 0.88, 0.0),
              (-0.12, -0.12, 0.6), (0.12, -0.12, 0.6), (0.1, 0.88, 0.1), (-0.1, 0.88, 0.1)], "gun_green", bevel=0.01)
        for s in (-1, 1):
            slab([(-0.18, 0.5), (0.35, 0.56), (0.5, 0.42), (0.0, 0.42)], 0.04, "gun_green", "yz",
                 loc=(s * 0.12, 0, 0), bevel=0.005)
        cyl2((0, 0.8, 0.09), (0, 0.95, 0.09), 0.04, 0.04, "iron", segs=8)
        with grp(matrix=look((0, 0.06, 0.66), (0, -1, 0.1), (0, 0, 1))):
            lathe([(0.0, -0.36), (0.05, -0.35), (0.055, -0.31), (0.03, -0.29), (0.1, -0.27), (0.11, -0.22),
                   (0.105, 0.05), (0.085, 0.08), (0.075, 0.95), (0.085, 0.97), (0.085, 1.02), (0.05, 1.02),
                   (0.05, 0.98)], (0, 0, 0), "gunmetal", segs=24, cap0=False, cap1=False)
            cyl2((-0.15, 0, 0.0), (0.15, 0, 0.0), 0.03, 0.03, "gunmetal", segs=10)
        box((0.24, 0.2, 0.16), (0.0, 0.5, 0.36), "gun_green", rot=(-24, 0, 0), bevel=0.01)
    with grp((0.4, 0.16, 0), (0, 0, -18), scl=0.92):
        skirt("green_coat", 0.62, 0.22, 0.19, a0=pi / 2 + 0.3, a1=3 * pi / 2 - 0.3)
        human("green_coat", pants="trousers_dark", boots="boots_black", boot_top=0.36,
              feet=[(-0.12, 0.0), (0.12, 0.02)], hands=[(-0.3, -0.1, 1.22), (0.26, -0.02, 0.85)],
              poles=[(-1, 0.4, -0.7), (1, 0.2, -1)], belt="white_cloth", cuff="black", hair="hair_fair",
              hat=hat_forage, braid="+", braid_end=1.04)
        ell((0, -0.07, 1.445), (0.09, 0.05, 0.035), "black", segs=14, rings=8)
        for z in (1.08, 1.17, 1.26, 1.35):
            ell(on_chest(0, z, 0.004), 0.013, "brass", segs=8, rings=6)
        cyl2((-0.31, -0.12, 0.0), (-0.31, -0.12, 1.86), 0.017, 0.017, "wood_light", segs=8)
        cyl2((-0.31, -0.12, 1.84), (-0.31, -0.12, 2.04), 0.045, 0.045, "fur_dark", segs=12)
    for (x, y, z) in ((0.12, -0.5, 0.075), (0.27, -0.46, 0.075), (0.19, -0.36, 0.075), (0.19, -0.44, 0.19)):
        ell((x, y, z), 0.075, "iron", segs=14, rings=10)


def u_rifle():
    """Снайпер: гимнастёрка, юбка, сапоги, пилотка со звёздочкой, две косы, винтовка с прицелом."""
    with grp((0.0, 0.06, 0)):
        skirt("skirt_khaki", 0.5, 0.2, 0.165)
        human("khaki", pants="stockings", boots="boots_black", boot_top=0.44, feet=[(-0.11, 0.04), (0.12, -0.06)],
              hands=[(-0.27, -0.12, 1.2), (0.25, -0.04, 0.92)], poles=[(-1, 0.4, -0.6), (1, 0.4, -1)],
              belt="leather_dark", hair="hair_fair", hat=hat_pilotka, braid="+-", braid_end=1.08)
        for x in (-0.07, 0.07):
            box((0.07, 0.035, 0.065), (x, front_y(0.98, x) - 0.012, 0.96), "leather_dark", bevel=0.01)
        p = on_chest(0.075, 1.29, 0.01)
        cyl2(p + Vector((0, 0, 0.05)), p + Vector((0, 0, 0.012)), 0.014, 0.014, "ribbon_grey", segs=6)
        cyl2(p + Vector((0, 0.006, -0.012)), p + Vector((0, -0.006, -0.012)), 0.022, 0.022, "steel", segs=14)
        rifle((-0.3, -0.12, 0.03), (-0.3, -0.16, 1.62), up=(0, 1, 0), scope=True)


def u_air():
    """Лётчица в кожаном шлеме с очками и реглане; за ней — истребитель на стойке."""
    cyl2((0.3, 0.4, 0.0), (0.3, 0.4, 0.04), 0.14, 0.13, "black", segs=24)
    cyl2((0.3, 0.4, 0.04), (0.28, 0.38, 1.62), 0.02, 0.016, "brass", segs=10)
    with grp((0.28, 0.38, 1.7), (10, -18, -56), scl=0.62):
        st = [(-0.98, 0.07, 0.07), (-0.9, 0.125, 0.125), (-0.6, 0.15, 0.155), (-0.25, 0.14, 0.15),
              (0.15, 0.1, 0.12), (0.55, 0.06, 0.08), (0.92, 0.025, 0.045)]
        rings = []
        for (y, rx, rz) in st:
            rings.append([(rx * cos(2 * pi * i / 20), y, rz * sin(2 * pi * i / 20)) for i in range(20)])
        loft(rings, "plane", cap0=True, cap1=True)
        cyl2((0, -0.98, 0), (0, -0.84, 0), 0.075, 0.13, "prop", segs=16)
        cyl2((0, -0.98, 0), (0, -1.12, 0), 0.075, 0.0, "red_bright", segs=16)
        for k in range(3):
            a = radians(30 + 120 * k)
            with grp((0, -1.03, 0), (0, math.degrees(a), 0)):
                slab([(-0.025, 0.0), (0.025, 0.0), (0.03, 0.4), (0.0, 0.43), (-0.03, 0.4)], 0.012, "prop", "xz")
                slab([(-0.03, 0.36), (0.03, 0.36), (0.0, 0.43)], 0.016, "yellow", "xz")
        ell((0, -0.12, 0.13), (0.085, 0.22, 0.1), "canopy", segs=16, rings=10)
        wingo = [(-1.0, -0.36), (-1.04, -0.27), (-1.0, -0.18), (0.0, -0.02), (1.0, -0.18), (1.04, -0.27),
                 (1.0, -0.36), (0.0, -0.56)]
        slab(wingo, 0.045, "plane", "xy", bend=lambda u, v: -0.05 + 0.09 * abs(u))
        slab([(-0.38, 0.76), (-0.38, 0.88), (0.0, 0.93), (0.38, 0.88), (0.38, 0.76), (0.0, 0.68)], 0.03,
             "plane", "xy", loc=(0, 0, 0.02))
        slab([(0.66, 0.02), (0.95, 0.02), (0.97, 0.32), (0.86, 0.33)], 0.03, "plane", "yz")
        for x in (-0.72, 0.72):
            star((x, -0.26, 0.04 + 0.09 * abs(x) - 0.05 + 0.024), 0.1, "red_star", rot=(90, 0, 0), t=0.006)
        star((0.0, 0.88, 0.2), 0.07, "red_star", rot=(0, 0, 90), t=0.04)
    with grp((-0.16, -0.12, 0), (0, 0, 6)):
        skirt("flight_leather", 0.5, 0.235, 0.19)
        human("flight_leather", pants="trousers_dark", boots="boots_black", boot_top=0.42,
              feet=[(-0.12, 0.0), (0.12, -0.04)], hands=[(-0.25, -0.1, 0.92), (0.24, -0.16, 1.02)],
              poles=[(-1, 0.3, -1), (1, 0.6, -1)], belt="leather_dark", hair="hair", cap=False,
              hat=hat_flight, braid="+", braid_end=1.06)
        ring((0, -0.004, 1.43), 0.115, 0.1, 0.038, "fur")
        with grp((0.2, -0.17, 0.93), (0, 0, 18)):
            box((0.2, 0.035, 0.24), (0, 0, 0), "leather", bevel=0.01)
            box((0.17, 0.01, 0.15), (0, -0.02, 0.02), "canopy")
        tube([(0.27, -0.12, 1.0), (0.12, -0.13, 1.33), (-0.12, -0.02, 1.42)], 0.012, "leather_dark")


def u_partisans():
    """Партизанка: ватник-телогрейка, тёмная юбка, валенки, платок, винтовка за плечом; за спиной берёзка."""
    with grp((0.42, 0.4, 0)):
        tube([(0, 0, 0), (0.02, 0.0, 0.6), (-0.02, 0.02, 1.2), (0.03, 0.0, 1.75)], [0.055, 0.045, 0.032, 0.012],
             "birch")
        for (p, q) in (((0.0, 0.0, 0.85), (0.22, 0.08, 1.12)), ((0.0, 0.0, 1.05), (-0.2, -0.05, 1.32)),
                       ((0.0, 0.0, 1.35), (0.16, -0.06, 1.55))):
            tube([p, q], [0.018, 0.008], "wood_dark")
        rnd = random.Random(11)
        for (x, y, z, r) in ((0.2, 0.06, 1.16, 0.15), (-0.18, -0.06, 1.36, 0.15), (0.13, -0.08, 1.58, 0.14),
                             (0.0, 0.02, 1.8, 0.12), (-0.06, 0.1, 1.1, 0.11), (0.1, 0.1, 1.4, 0.13),
                             (-0.12, 0.06, 1.62, 0.12), (0.26, -0.04, 1.32, 0.1)):
            for k in range(3):
                puff((x + rnd.uniform(-0.07, 0.07), y + rnd.uniform(-0.07, 0.07), z + rnd.uniform(-0.05, 0.05)),
                     r * rnd.uniform(0.55, 0.8), "leaf")
        for k in range(7):
            a = 2 * pi * k / 7
            cyl2((0.12 * cos(a), 0.12 * sin(a), 0.0), (0.16 * cos(a), 0.16 * sin(a), 0.12), 0.03, 0.0, "grass", segs=5)
    with grp((-0.08, -0.02, 0)):
        rifle((-0.13, 0.19, 0.8), (-0.27, 0.13, 1.98), up=(0, 1, 0))
        skirt("skirt_dark", 0.46, 0.215, 0.175)
        skirt("telogreika", 0.74, 0.232, 0.185, z0=1.04)
        human("telogreika", pants="stockings_dark", boots="felt", boot_top=0.44, feet=[(-0.12, 0.06), (0.12, -0.08)],
              hands=[(-0.07, -0.17, 1.3), (0.25, -0.04, 0.9)], poles=[(-1, 0.2, -1), (1, 0.3, -1)],
              belt="leather_dark", hair="hair_fair", cap=False, locks=False,
              hat=lambda: hat_kerchief(hair="hair_fair"), braid="+",
              braid_end=1.06)
        tube([(-0.25, 0.12, 1.75), (-0.2, -0.04, 1.47), (-0.09, -0.16, 1.3)], 0.013, "leather_dark")
        tube([(-0.06, -0.17, 1.27), (-0.2, -0.1, 1.05), (-0.16, 0.18, 0.95)], 0.013, "leather_dark")
        for z in (1.12, 1.22, 1.32):
            ell(on_chest(0.0, z, 0.006), 0.014, "black", segs=8, rings=6)


def u_motor():
    """Мотострелки: армейский грузовик МАЗ с тентом и регулировщица с флажками."""
    with grp((-0.18, 0.16, 0.0), scl=0.86):
        for y in (-0.66, 0.45, 0.82):
            for s in (-1, 1):
                tyre((s * 0.38, y, 0.24), 0.24, 0.16, hub="truck")
        box((0.5, 2.0, 0.12), (0, 0.05, 0.36), "olive_dark")
        hexa([(-0.43, -1.08, 0.4), (0.43, -1.08, 0.4), (0.43, -0.48, 0.4), (-0.43, -0.48, 0.4),
              (-0.42, -1.02, 1.1), (0.42, -1.02, 1.1), (0.42, -0.48, 1.1), (-0.42, -0.48, 1.1)], "truck", bevel=0.04)
        box((0.74, 0.02, 0.26), (0, -1.065, 0.88), "glass", rot=(-5, 0, 0))
        for s in (-1, 1):
            box((0.02, 0.3, 0.22), (s * 0.432, -0.72, 0.88), "glass")
            cyl2((s * 0.3, -1.08, 0.56), (s * 0.3, -1.1, 0.56), 0.05, 0.05, "lamp", segs=12)
            box((0.06, 0.03, 0.12), (s * 0.48, -0.9, 0.95), "black")
            hexa([(s * 0.36, -0.95, 0.42), (s * 0.48, -0.95, 0.42), (s * 0.48, -0.38, 0.42), (s * 0.36, -0.38, 0.42),
                  (s * 0.36, -0.92, 0.5), (s * 0.48, -0.92, 0.5), (s * 0.48, -0.42, 0.5), (s * 0.36, -0.42, 0.5)],
                 "olive_dark")
        for k in range(4):
            box((0.4, 0.02, 0.025), (0, -1.085, 0.5 + k * 0.05), "black")
        box((0.9, 0.08, 0.1), (0, -1.1, 0.38), "olive_dark", bevel=0.01)
        box((0.88, 1.5, 0.06), (0, 0.33, 0.6), "truck")
        for s in (-1, 1):
            box((0.03, 1.5, 0.16), (s * 0.43, 0.33, 0.7), "truck")
        arch = [(0.44 * cos(pi * i / 12), 0.0, 0.94 + 0.2 * sin(pi * i / 12)) for i in range(13)]
        prof = [(0.44, 0, 0.66)] + arch + [(-0.44, 0, 0.66)]
        rings = [[(x, y, z) for (x, _, z) in prof] for y in (-0.42, 1.08)]
        loft(rings, "canvas", closed=False, cap0=False, sub=0, smooth=True)
        slab([(x, z) for (x, _, z) in prof], 0.03, "canvas", "xz", loc=(0, 1.08, 0))
        slab([(x, z) for (x, _, z) in prof], 0.03, "canvas", "xz", loc=(0, -0.42, 0))
        for y in (-0.05, 0.33, 0.71):
            tube([(1.02 * x, y, 0.94 + (z - 0.94) * 1.06) for (x, _, z) in arch], 0.016, "khaki_dark", res=2)
        cyl2((0.4, -0.2, 0.46), (0.4, 0.15, 0.46), 0.1, 0.1, "olive_dark", segs=14)
    with grp((0.56, -0.3, 0), (0, 0, 40), scl=1.04):
        skirt("skirt_khaki", 0.5, 0.2, 0.165)
        hl, hr = Vector((0.36, -0.14, 1.66)), Vector((-0.27, -0.12, 0.92))
        human("khaki", pants="stockings", boots="boots_black", boot_top=0.44, feet=[(-0.12, 0.02), (0.12, -0.03)],
              hands=[hr, hl], poles=[(-1, 0.3, -1), (1, 0.2, -0.3)], belt="leather_dark", hair="hair",
              hat=hat_pilotka, braid="+", braid_end=1.06)
        flag(hl, hl + Vector((0.08, -0.04, 0.32)), "red_bright")
        flag(hr, hr + Vector((-0.04, -0.2, -0.26)), "yellow")


def u_tank():
    """Танковая дивизия: средний танк — гусеницы, башня с длинной пушкой; в люке — танкистка."""
    with grp((0.05, 0.05, 0), (0, 0, 0)):
        tr_ = [(-0.98, 0.26), (-0.88, 0.04), (0.86, 0.04), (0.98, 0.2), (0.96, 0.32), (-0.95, 0.34)]
        for s in (-1, 1):
            slab(tr_, 0.22, "track", "yz", loc=(s * 0.48, 0, 0), bevel=0.03)
            for k in range(6):
                y = -0.68 + k * 0.28
                wheel_disc((s * 0.6, y, 0.17), 0.13, 0.04, "olive_dark", segs=18)
                wheel_disc((s * 0.625, y, 0.17), 0.04, 0.02, "olive", segs=10)
            wheel_disc((s * 0.6, 0.86, 0.2), 0.1, 0.04, "olive_dark", segs=14)
            wheel_disc((s * 0.6, -0.9, 0.22), 0.09, 0.04, "olive_dark", segs=14)
        box((1.28, 1.98, 0.04), (0, 0.0, 0.38), "tank", bevel=0.01)
        hexa([(-0.44, -1.02, 0.18), (0.44, -1.02, 0.18), (0.44, 0.98, 0.2), (-0.44, 0.98, 0.2),
              (-0.5, -0.6, 0.52), (0.5, -0.6, 0.52), (0.5, 0.98, 0.5), (-0.5, 0.98, 0.5)], "tank", bevel=0.02)
        for s in (-1, 1):
            box((0.12, 0.5, 0.12), (s * 0.58, 0.45, 0.46), "tank", bevel=0.015)
        cyl2((0, 0.12, 0.5), (0, 0.12, 0.58), 0.4, 0.4, "tank", segs=28)
        ell((0, 0.14, 0.6), (0.43, 0.5, 0.2), "tank", segs=28, rings=14)
        ell((0, -0.32, 0.64), (0.15, 0.1, 0.11), "tank", segs=14, rings=8)
        cyl2((0, -0.36, 0.66), (0, -1.78, 0.69), 0.045, 0.04, "tank", segs=12)
        ell((0, -1.05, 0.675), (0.065, 0.14, 0.065), "tank", segs=12, rings=8)
        # командирский люк: кольцо, откинутая крышка и героиня по пояс
        hx, hy = 0.12, 0.28
        cyl2((hx, hy, 0.7), (hx, hy, 0.8), 0.19, 0.185, "tank", segs=24)
        lathe([(0.185, 0.8), (0.15, 0.8)], (hx, hy, 0), "olive_dark", segs=24, cap0=False, cap1=False)
        with grp((hx, hy + 0.2, 0.8), (-70, 0, 0)):
            cyl2((0, 0.18, 0.0), (0, 0.18, 0.04), 0.18, 0.18, "tank", segs=24)
        cyl2((-0.24, 0.32, 0.74), (-0.24, 0.32, 0.79), 0.07, 0.07, "olive_dark", segs=14)
        box((0.1, 0.08, 0.1), (-0.26, -0.18, 0.75), "olive_dark", bevel=0.01)
        for x in (-0.22, 0.22):
            cyl2((x - 0.16, 1.06, 0.48), (x + 0.16, 1.06, 0.48), 0.1, 0.1, "olive_dark", segs=14)
        star((0.43, 0.12, 0.6), 0.06, "red_star", rot=(0, 0, 90), t=0.04)
        S = 0.92
        with grp((hx, hy, 0.8 - 1.0 * S), (0, 0, 28), scl=S):
            human("tank_overall", hands=[(-0.27, -0.16, 1.03), (0.38, -0.16, 1.82)],
                  poles=[(-1, 0.2, -0.5), (1, 0.2, -0.5)], hair="hair_fair", cap=False, hat=hat_tank,
                  braid="+", braid_end=1.06, bust_only=True)


def u_missile():
    """Ракетная бригада: восьмиколёсный тягач МЗКТ с приподнятой ракетой и офицер-ракетчица."""
    with grp((-0.12, 0.08, 0.0), scl=0.9):
        for y in (-0.95, -0.5, 0.5, 0.95):
            for s in (-1, 1):
                tyre((s * 0.38, y, 0.23), 0.23, 0.17, hub="missile_green")
        box((0.6, 2.6, 0.16), (0, 0.0, 0.4), "olive_dark")
        hexa([(-0.46, -1.32, 0.44), (0.46, -1.32, 0.44), (0.46, -0.72, 0.44), (-0.46, -0.72, 0.44),
              (-0.44, -1.18, 0.98), (0.44, -1.18, 0.98), (0.44, -0.72, 0.98), (-0.44, -0.72, 0.98)],
             "missile_green", bevel=0.035)
        box((0.8, 0.02, 0.22), (0, -1.255, 0.82), "glass", rot=(-15, 0, 0))
        for s in (-1, 1):
            box((0.02, 0.32, 0.2), (s * 0.452, -0.98, 0.82), "glass")
            cyl2((s * 0.32, -1.32, 0.56), (s * 0.32, -1.34, 0.56), 0.045, 0.045, "lamp", segs=12)
        box((0.94, 0.08, 0.1), (0, -1.34, 0.42), "olive_dark", bevel=0.01)
        hexa([(-0.47, -0.7, 0.44), (0.47, -0.7, 0.44), (0.47, 1.32, 0.44), (-0.47, 1.32, 0.44),
              (-0.47, -0.7, 0.66), (0.47, -0.7, 0.66), (0.47, 1.32, 0.66), (-0.47, 1.32, 0.66)], "missile_green",
             bevel=0.02)
        box((0.88, 0.22, 0.36), (0, -0.58, 0.82), "missile_green", bevel=0.02)
        for s in (-1, 1):
            box((0.02, 1.6, 0.1), (s * 0.48, 0.3, 0.55), "olive_dark")
        a = radians(11)
        d = Vector((0, -cos(a), sin(a)))
        t0 = Vector((0, 1.3, 0.8))
        with grp(matrix=look(t0, d, (0, 0, 1))):
            box((0.3, 0.06, 1.9), (0, -0.13, 0.95), "olive_dark", bevel=0.01)
            lathe([(0.0, 0.0), (0.105, 0.0), (0.105, 1.6), (0.095, 1.78), (0.07, 1.92), (0.035, 2.02),
                   (0.0, 2.06)], (0, 0, 0), "missile_white", segs=24)
            lathe([(0.107, 0.3), (0.107, 0.38)], (0, 0, 0), "black", segs=24, cap0=False, cap1=False)
            lathe([(0.07, 1.92), (0.035, 2.02), (0.0, 2.06)], (0, 0, 0.002), "gunmetal", segs=24, cap0=False)
            for k in range(4):
                with grp((0, 0, 0), (0, 0, 45 + 90 * k)):
                    slab([(0.1, 0.0), (0.2, 0.0), (0.2, 0.08), (0.1, 0.22)], 0.015, "missile_green", "xz")
        piv = t0 + d * 1.25
        cyl2((0, 0.55, 0.66), (0, piv.y, piv.z - 0.12), 0.04, 0.04, "steel_dark", segs=10)
    with grp((0.64, -0.52, 0), (0, 0, 50), scl=1.18):
        skirt("olive_uniform", 0.5, 0.2, 0.165)
        human("olive_uniform", pants="stockings", boots="boots_black", boot_top=0.42,
              feet=[(-0.12, 0.02), (0.12, -0.02)], hands=[(-0.25, -0.12, 0.98), (0.24, -0.18, 1.08)],
              poles=[(-1, 0.3, -1), (1, 0.5, -1)], belt="leather_dark", hair="hair_dark", cap=False,
              hat=hat_peaked, braid="+", braid_end=1.08)
        with grp((0.22, -0.24, 1.08), (0, 0, 20)):
            box((0.17, 0.035, 0.22), (0, 0, 0), "black", bevel=0.008)
            box((0.14, 0.01, 0.17), (0, -0.02, 0.01), "canopy")


def u_airdef():
    """ПВО: зенитчица в цифровом камуфляже с переносным зенитным комплексом; позади — машина с радаром."""
    with grp((-0.26, 0.3, 0.0), scl=0.74):
        for y in (-0.62, 0.0, 0.62):
            for s in (-1, 1):
                tyre((s * 0.39, y, 0.25), 0.25, 0.17, hub="camo")
        hexa([(-0.4, -1.0, 0.3), (0.4, -1.0, 0.3), (0.4, 0.95, 0.3), (-0.4, 0.95, 0.3),
              (-0.46, -0.55, 0.7), (0.46, -0.55, 0.7), (0.46, 0.95, 0.7), (-0.46, 0.95, 0.7)], "camo", bevel=0.03)
        for s in (-1, 1):
            box((0.02, 0.2, 0.08), (s * 0.4, -0.75, 0.56), "glass", rot=(0, 0, 0))
        cyl2((0, 0.15, 0.7), (0, 0.15, 0.8), 0.36, 0.34, "camo", segs=24)
        hexa([(-0.3, -0.2, 0.8), (0.3, -0.2, 0.8), (0.3, 0.5, 0.8), (-0.3, 0.5, 0.8),
              (-0.26, -0.1, 1.08), (0.26, -0.1, 1.08), (0.26, 0.45, 1.08), (-0.26, 0.45, 1.08)], "camo", bevel=0.02)
        cyl2((0, 0.3, 1.08), (0, 0.3, 1.38), 0.035, 0.035, "steel_dark", segs=10)
        with grp((0, 0.3, 1.42), (-62, 0, 0)):
            lathe([(0.0, 0.0), (0.12, 0.012), (0.24, 0.05), (0.33, 0.1)], (0, 0, 0), "steel", segs=32,
                  cap0=False, cap1=False, solid=0.018)
            cyl2((0, 0, 0), (0, 0, 0.2), 0.012, 0.012, "steel_dark", segs=6)
            ell((0, 0, 0.21), 0.025, "steel_dark", segs=8, rings=6)
        box((0.5, 0.05, 0.3), (0, -0.2, 1.05), "steel_dark", rot=(-20, 0, 0), bevel=0.01)
        for s in (-1, 1):
            for k in range(2):
                a = radians(28)
                base = Vector((s * (0.36 + 0.11 * k), 0.3, 0.96 + 0.02 * k))
                dd = Vector((0, -cos(a), sin(a)))
                beam(base - dd * 0.15 + Vector((0, 0, -0.05)), base + dd * 0.35 + Vector((0, 0, -0.05)), 0.04,
                     "steel_dark")
                cyl2(base - dd * 0.25, base + dd * 0.45, 0.045, 0.045, "missile_white", segs=12)
                cyl2(base + dd * 0.45, base + dd * 0.6, 0.045, 0.0, "red_bright", segs=12)
            beam((s * 0.3, 0.3, 0.92), (s * 0.42, 0.3, 0.92), 0.06, "steel_dark")
    with grp((0.5, -0.28, 0), (0, 0, 34), scl=1.16):
        human("camo_green", pants="camo_green", boots="boots_black", boot_top=0.36,
              feet=[(-0.13, 0.06), (0.12, -0.08)], hands=[(-0.2, -0.24, 1.36), (-0.14, -0.5, 1.5)],
              poles=[(-1, 0.2, -1), (1, 0.0, -1)], belt="black", hair="hair", hat=hat_helmet,
              braid="+", braid_end=1.06)
        p0, p1 = Vector((-0.24, 0.42, 1.33)), Vector((-0.2, -0.86, 1.76))
        d = (p1 - p0).normalized()
        cyl2(p0, p1, 0.04, 0.04, "missile_green", segs=14)
        cyl2(p1 - d * 0.04, p1 + d * 0.02, 0.05, 0.05, "olive_dark", segs=14)
        cyl2(p0 - d * 0.02, p0 + d * 0.05, 0.05, 0.05, "olive_dark", segs=14)
        q = p0 + d * 0.62
        box((0.05, 0.06, 0.16), q + Vector((0.0, 0.0, -0.1)), "black", rot=(18, 0, 0), bevel=0.008)
        ell(q + d * 0.2 + Vector((0.0, 0.0, -0.08)), (0.04, 0.05, 0.04), "olive_dark", segs=12, rings=8)
        box((0.03, 0.12, 0.08), q + d * 0.12 + Vector((0.06, 0.0, 0.03)), "black", rot=(18, 0, 0))


MODELS = [  # (функция, разворот, масштаб)
    (u_druzhina, -32, 1.22), (u_crossbow, -48, 1.22), (u_khorugv, -32, 1.22), (u_gunners, 0, 1.12),
    (u_musketeers, -62, 1.22), (u_hussars, -50, 1.0), (u_rockets, -52, 1.16), (u_line, -32, 1.22),
    (u_artillery, 0, 1.1), (u_rifle, -40, 1.22), (u_air, -10, 1.14), (u_partisans, -40, 1.22),
    (u_motor, -56, 0.98), (u_tank, -56, 0.94), (u_missile, -58, 0.8), (u_airdef, -56, 0.98),
]
assert len(MODELS) == len(IDS)


# ----------------------------------------------------------------------------------------------
#  Сцена, свет, камера, рендер
# ----------------------------------------------------------------------------------------------
def setup_scene(args):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = sc.render.resolution_y = CELL * SCALE
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.filter_size = 1.2
    im = sc.render.image_settings
    im.file_format, im.color_mode, im.color_depth = "PNG", "RGBA", "8"
    ee = sc.eevee
    ee.taa_render_samples = args["samples"]
    ee.use_gtao = True; ee.gtao_distance = 0.25; ee.gtao_factor = 1.0
    ee.use_soft_shadows = True
    ee.shadow_cube_size = "2048"; ee.shadow_cascade_size = "4096"
    ee.use_shadow_high_bitdepth = True
    ee.use_ssr = True; ee.ssr_thickness = 0.05
    ee.use_bloom = True; ee.bloom_intensity = 0.02; ee.bloom_threshold = 1.5
    sc.view_settings.view_transform = "Filmic"
    sc.view_settings.look = "Medium High Contrast"
    sc.view_settings.exposure = 0.0
    sc.frame_current = 1
    rig = bpy.data.collections.new("Studio"); sc.collection.children.link(rig)
    # мир: тёплый низ, светлый верх (отражения в металле и лаке)
    w = bpy.data.worlds.new("Studio"); sc.world = w; w.use_nodes = True
    nt = w.node_tree; bg = nt.nodes["Background"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[1].default_value = -1.0; mr.inputs[2].default_value = 1.0
    rp = nt.nodes.new("ShaderNodeValToRGB")
    rp.color_ramp.elements[0].color = hexc("#3a2c22"); rp.color_ramp.elements[0].position = 0.4
    rp.color_ramp.elements[1].color = hexc("#c8d0dc"); rp.color_ramp.elements[1].position = 0.8
    e = rp.color_ramp.elements.new(0.55); e.color = hexc("#9a9084")
    nt.links.new(tc.outputs["Generated"], sep.inputs[0]); nt.links.new(sep.outputs[2], mr.inputs[0])
    nt.links.new(mr.outputs[0], rp.inputs[0]); nt.links.new(rp.outputs[0], bg.inputs[0])
    bg.inputs[1].default_value = 0.6
    # ключевой свет — «солнце» с юго-запада, как у построек карты
    sd = bpy.data.lights.new("Key", "SUN")
    sd.energy = 3.6; sd.angle = radians(8); sd.color = (1.0, 0.93, 0.82)
    sd.use_contact_shadow = True; sd.contact_shadow_distance = 0.08; sd.contact_shadow_thickness = 0.02
    sd.shadow_cascade_max_distance = 12.0; sd.shadow_cascade_count = 2
    sun = bpy.data.objects.new("Key", sd); rig.objects.link(sun)
    az, el = radians(225.0), radians(42.0)
    s = Vector((sin(az) * cos(el), cos(az) * cos(el), sin(el)))
    sun.rotation_euler = (-s).to_track_quat("-Z", "Y").to_euler()
    tgt = Vector((0, 0, 0.9))

    def area(name, loc, energy, size, col):
        ld = bpy.data.lights.new(name, "AREA"); ld.energy = energy; ld.size = size; ld.color = col
        ld.use_contact_shadow = True; ld.contact_shadow_distance = 0.05
        ob = bpy.data.objects.new(name, ld); rig.objects.link(ob)
        ob.location = tgt + Vector(loc)
        ob.rotation_euler = (tgt - ob.location).to_track_quat("-Z", "Y").to_euler()
        return ob
    area("Fill", (5.0, -4.0, 1.5), 900.0, 5.0, (0.82, 0.88, 1.0))
    area("Rim", (3.5, 5.0, 3.5), 1600.0, 2.5, (1.0, 0.8, 0.58))
    area("Rim2", (-4.5, 3.5, 2.5), 700.0, 2.5, (1.0, 0.9, 0.78))
    # камера
    cd = bpy.data.cameras.new("Cam")
    cd.type = "ORTHO"; cd.ortho_scale = CELL / PPU; cd.clip_start = 0.1; cd.clip_end = 200
    cam = bpy.data.objects.new("Cam", cd); rig.objects.link(cam); sc.camera = cam
    t = radians(90.0 - ELEV)
    cam.rotation_euler = (t, 0, 0)
    fwd = Vector((0, _ce, -_se)); up = Vector((0, _se, _ce))
    target = up * ((ANCHOR[1] - CELL / 2.0) / PPU)
    cam.location = target - fwd * 60
    if args["zoom"] > 1.0:                 # крупный план лиц для проверки (в лист не идёт)
        cd.ortho_scale /= args["zoom"]
        cam.location = Vector((0, 0, args["zh"])) - fwd * 60
    return sc


def build(sc):
    roots, colls = [], []
    for i, (fn, yaw, s) in enumerate(MODELS):
        nm = "U_%02d_%s" % (i, IDS[i])
        c = bpy.data.collections.new(nm); sc.collection.children.link(c)
        r = bpy.data.objects.new(nm, None); r.empty_display_size = 1.2; c.objects.link(r)
        COLL[0] = c
        PARENT[:] = [r]
        random.seed(500 + i)
        base_disc()
        with grp((0, 0, 0), (0, 0, yaw), s):
            fn()
        roots.append(r); colls.append(c)
    bpy.context.view_layer.update()
    return roots, colls


def check_bounds(i, coll):
    deps = bpy.context.evaluated_depsgraph_get()
    xs, ys = [], []
    for o in coll.all_objects:
        if o.type not in ("MESH", "CURVE"):
            continue
        oe = o.evaluated_get(deps)
        Mw = oe.matrix_world
        for c in oe.bound_box:
            w = Mw @ Vector(c)
            xs.append(ANCHOR[0] + w.x * PPU)
            ys.append(ANCHOR[1] - (w.y * _se + w.z * _ce) * PPU)
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    flag = "  <-- CHECK" if (y0 < 2 or x0 < 2 or x1 > CELL - 2 or y1 > CELL - 1) else ""
    print("[%2d] %-11s x %5.1f..%5.1f  y %5.1f..%5.1f%s" % (i, IDS[i], x0, x1, y0, y1, flag), flush=True)


def main():
    args = parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    os.makedirs(args["work"], exist_ok=True)
    sc = setup_scene(args)
    roots, colls = build(sc)
    for i, c in enumerate(colls):
        check_bounds(i, c)
    todo = [IDS.index(x) for x in args["only"]] if args["only"] else list(range(len(MODELS)))
    t_all = time.time()
    for i in todo:
        if i >= len(colls):
            continue
        t0 = time.time()
        for j, c in enumerate(colls):
            c.hide_render = (j != i)
        sc.render.filepath = os.path.join(args["work"], IDS[i] + ".png")
        bpy.ops.render.render(write_still=True)
        print("UNIT %s %.1fs" % (IDS[i], time.time() - t0), flush=True)
    print("render total %.1fs" % (time.time() - t_all), flush=True)
    for c in colls:
        c.hide_render = False
    if args["save"] and args["zoom"] <= 1.0:
        # в .blend фигурки стоят сеткой 4 x 4 (камера смотрит на первую, в начале координат)
        for i, r in enumerate(roots):
            r.location = ((i % COLS) * 3.0, (i // COLS) * 3.0, 0)
        txt = bpy.data.texts.new("build_units.py")
        with open(os.path.abspath(__file__), encoding="utf-8") as f:
            txt.from_string(f.read())
        out = os.path.join(GAME, "blender", "belarus_units.blend")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=out, compress=True)
        print("saved", out)
    if args["zoom"] <= 1.0:
        subprocess.run(["python3", os.path.abspath(__file__), "--post", "--work", args["work"]], check=False)


main()
