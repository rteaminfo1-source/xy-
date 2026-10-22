#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Анимированные 3D-отряды для карты войны: идут, скачут, едут, летят и стреляют.

24 вида: 16 своих (id как в UNIT_TYPES, src/data_war.c) — все бойцы героини с косами и бантами
(дружинница с копьём и круглым щитом, арбалетчица, всадницы хоругви и крылатой гусарии,
мушкетёрки, линейная пехота в киверах, стрелок в пилотке со звёздочкой, партизанка в платке,
танкистка в люке Т-34, лётчица в истребителе) и 8 вражеских по эпохам (дружины соседей,
тевтонцы в белых плащах с чёрным крестом, стрельцы, шведы в треуголках, наполеоновская
пехота, кайзеровская пехота в пикельхаубе, вермахт в касках и серый танк с крестом).

Персонажи собраны из тех же деталей, что и фигурки tools/blender/build_units.py (тело героини,
лицо, косы, головные уборы, оружие, лошадь): скрипт загружает его как модуль. Каждый кадр —
поза «скелета»: таз и плечи — шарниры, колени и локти считаются обратной кинематикой от точек
стоп и кистей, оружие крепится к кистям (кисть — точка хвата), тело наклоняется и качается
вокруг таза. Для каждого кадра фигура пересобирается в своей позе и рендерится.

Кадры одного вида (ряд листа): стоит — 1, шагает — 8 (галоп у конницы, гусеницы и колёса
у техники, полёт с размытым винтом у самолёта), стреляет — 6 (прицел, отдача, вспышка —
светящийся конус, облачко дыма; у копья и меча — выпад и удар; у пушки и ракет — откат,
пламя и дым). Камера ортографическая, ракурс 3/4 сбоку чуть сверху, фигуры смотрят ВПРАВО,
фон прозрачный, подставки нет; стопы всех видов — в одной точке клетки (якорь).

Запуск (из корня belarus-game; EEVEE нужен виртуальный дисплей):
    xvfb-run -a -s "-screen 0 1280x1024x24" blender -b -P tools/blender/build_troops.py -- \\
        [--work /tmp/belarus_troops] [--only rifle,tank] [--frames 0,1,9,10] [--samples 24] [--no-post]
Рендер — вдвое крупнее клетки, в рабочую папку (<id>_<кадр>.png). Затем сам вызывает шаг --post
(обычный python3 + Pillow; можно запускать отдельно):
    python3 tools/blender/build_troops.py --post --work /tmp/belarus_troops
--post кладёт мягкую тень под ноги, уменьшает кадры вдвое и собирает assets/troops.png и
assets/troops.json {cell_w, cell_h, cols, anchor_x, anchor_y, ppu,
                    variants: [{id, stand, walk: [first, count], fire: [first, count]}]}
и превью на фоне карты: preview.png (клетки как есть) и preview_small.png (в размер карты).
"""
import os, sys, json, math, random, subprocess, tempfile, time, types

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(os.path.dirname(HERE))
OWN = ["druzhina", "crossbow", "khorugv", "gunners", "musketeers", "hussars", "rockets", "line",
       "artillery", "rifle", "air", "partisans", "motor", "tank", "missile", "airdef"]
ENEMY = ["enemy_early", "enemy_teutonic", "enemy_strelets", "enemy_18c", "enemy_napoleon", "enemy_ww1",
         "enemy_wehrmacht", "enemy_panzer"]
IDS = OWN + ENEMY
CELL_W, CELL_H, SCALE = 144, 128, 2    # клетка листа и во сколько раз крупнее рендер
N_WALK, N_FIRE = 8, 6
COLS = 1 + N_WALK + N_FIRE             # ряд вида: стоит | шагает x8 | стреляет x6
PPU = 48.0                             # пикселей клетки на единицу модели (героиня ~1.9)
ELEV = 24.0                            # подъём камеры, градусы
ANCHOR = (52, 116)                     # точка опоры (между стопами) в клетке
_se, _ce = math.sin(math.radians(ELEV)), math.cos(math.radians(ELEV))


def parse_args(argv):
    a = {"work": os.path.join(tempfile.gettempdir(), "belarus_troops"), "only": None, "samples": 24,
         "post": False, "no_post": False, "frames": None}
    i = 0
    while i < len(argv):
        k = argv[i]
        if k == "--work": a["work"] = argv[i + 1]; i += 1
        elif k == "--only": a["only"] = [x for x in argv[i + 1].split(",") if x]; i += 1
        elif k == "--frames": a["frames"] = [int(x) for x in argv[i + 1].split(",") if x]; i += 1
        elif k == "--samples": a["samples"] = int(argv[i + 1]); i += 1
        elif k == "--post": a["post"] = True
        elif k == "--no-post": a["no_post"] = True
        i += 1
    return a


# ----------------------------------------------------------------------------------------------
#  Сборка листа (обычный python3 + Pillow)
# ----------------------------------------------------------------------------------------------
def post(a):
    from PIL import Image, ImageDraw, ImageFilter
    work = a["work"]
    W, Hh, S = CELL_W * SCALE, CELL_H * SCALE, SCALE
    sheet = Image.new("RGBA", (COLS * CELL_W, len(IDS) * CELL_H), (0, 0, 0, 0))
    variants, done = [], 0
    for row, vid in enumerate(IDS):
        frames = [os.path.join(work, "%s_%02d.png" % (vid, k)) for k in range(COLS)]
        variants.append({"id": vid, "stand": row * COLS, "walk": [row * COLS + 1, N_WALK],
                         "fire": [row * COLS + 1 + N_WALK, N_FIRE]})
        if not os.path.exists(frames[0]):
            print("missing", vid); continue
        # тень: мягкий эллипс под тем, что касается земли (свой для «стоит/стреляет» и для шага)
        def make_shadow(path):
            if vid == "air" or not os.path.exists(path):
                return None
            al = Image.open(path).convert("RGBA").split()[3]
            if al.size != (W, Hh):
                al = al.resize((W, Hh))
            ay = ANCHOR[1] * S
            band = al.crop((0, ay - 12 * S, W, ay + 4 * S)).point(lambda v: 255 if v > 120 else 0)
            bb = band.getbbox()
            if not bb:
                return None
            x0, x1 = bb[0], bb[2]
            cx, rx = (x0 + x1) / 2.0, (x1 - x0) / 2.0 + 5 * S
            ry = max(4.5 * S, rx * 0.26)
            m = Image.new("L", (W, Hh), 0)
            ImageDraw.Draw(m).ellipse((cx - rx, ay - ry + S, cx + rx, ay + ry + S), fill=125)
            m = m.filter(ImageFilter.GaussianBlur(3.5 * S))
            sh = Image.new("RGBA", (W, Hh), (26, 18, 10, 255))
            sh.putalpha(m)
            return sh
        sh_stand, sh_walk = make_shadow(frames[0]), make_shadow(frames[1])
        for k, f in enumerate(frames):
            if not os.path.exists(f):
                print("missing", vid, k); continue
            im = Image.open(f).convert("RGBA")
            if im.size != (W, Hh):
                im = im.resize((W, Hh), Image.LANCZOS)
            shadow = sh_walk if 1 <= k <= N_WALK else sh_stand
            if shadow is not None:
                im = Image.alpha_composite(shadow, im)
            im = im.convert("RGBa").resize((CELL_W, CELL_H), Image.LANCZOS).convert("RGBA")
            sheet.paste(im, (k * CELL_W, row * CELL_H))
        done += 1
    os.makedirs(os.path.join(GAME, "assets"), exist_ok=True)
    sheet.save(os.path.join(GAME, "assets", "troops.png"), optimize=True)
    with open(os.path.join(GAME, "assets", "troops.json"), "w", encoding="utf-8") as f:
        json.dump({"cell_w": CELL_W, "cell_h": CELL_H, "cols": COLS, "anchor_x": ANCHOR[0],
                   "anchor_y": ANCHOR[1], "ppu": PPU, "variants": variants}, f, ensure_ascii=False)
        f.write("\n")
    bg = Image.new("RGBA", sheet.size, (0x8c, 0x8a, 0x5c, 255))
    Image.alpha_composite(bg, sheet).convert("RGB").save(os.path.join(work, "preview.png"))
    sm = sheet.convert("RGBa").resize((sheet.size[0] * 2 // 5, sheet.size[1] * 2 // 5), Image.BOX).convert("RGBA")
    bg = Image.new("RGBA", sm.size, (0x8c, 0x8a, 0x5c, 255))
    Image.alpha_composite(bg, sm).convert("RGB").save(os.path.join(work, "preview_small.png"))
    print("sheet", sheet.size, "variants", done, "->", os.path.join(GAME, "assets", "troops.png"))


try:
    import bpy
    from mathutils import Vector, Matrix
    IN_BLENDER = True
except ImportError:
    IN_BLENDER = False

if not IN_BLENDER:                     # обычный python3: только сборка листа
    if __name__ == "__main__":
        post(parse_args(sys.argv[1:]))
    sys.exit(0)

from math import radians, sin, cos, pi, sqrt


def _load_units():
    """build_units.py как модуль (без вызова его main())."""
    p = os.path.join(HERE, "build_units.py")
    with open(p, encoding="utf-8") as f:
        src = f.read()
    src = src[:src.rindex("\nmain()")]
    m = types.ModuleType("build_units")
    m.__file__ = p
    exec(compile(src, p, "exec"), m.__dict__)
    return m


U = _load_units()
grp, ell, cyl2, tube, box, hexa, lathe = U.grp, U.ell, U.cyl2, U.tube, U.box, U.hexa, U.lathe
slab, loft, ell_ring, ring, look, star, puff = U.slab, U.loft, U.ell_ring, U.ring, U.look, U.star, U.puff
HC, H, SH_X, SH_Z = U.HC, U.H, U.SH_X, U.SH_Z
U.HS = 1.2                             # голова крупнее, чем у фигурок: лица и уборы читаются в 30-60 px
V3 = Vector

# ----------------------------------------------------------------------------------------------
#  Материалы: добавки к палитре фигурок и прозрачные/светящиеся эффекты
# ----------------------------------------------------------------------------------------------
M = U.M
U.PAL.update({
    "mail_dark": M("#6c7076", 0.45, 0.85, kind="mail"),
    "cloak_brown": M("#5a3a22", 0.8, sheen=0.3, var=(40, 0.85, 1.12)),
    "shield_blue": M("#2c3c6a", 0.5, coat=0.2),
    "teut_white": M("#e8e4d8", 0.75, sheen=0.3, var=(40, 0.92, 1.05)),
    "strelets": M("#b0402a", 0.7, sheen=0.3, var=(40, 0.85, 1.12)),
    "swede_blue": M("#2a4a8e", 0.7, sheen=0.3, var=(40, 0.88, 1.1)),
    "buff": M("#c8a864", 0.7, sheen=0.3),
    "french_blue": M("#22346e", 0.7, sheen=0.3, var=(40, 0.88, 1.1)),
    "fg_ww1": M("#6e7258", 0.75, sheen=0.25, var=(40, 0.9, 1.08)),
    "fg_tunic": M("#5c6058", 0.75, sheen=0.25, var=(40, 0.9, 1.08)),
    "fg_helmet": M("#4c514c", 0.5, 0.2),
    "fg_trousers": M("#4e504c", 0.75, sheen=0.2),
    "panzer": M("#5c615f", 0.55, var=(5, 0.88, 1.1)),
    "panzer_dark": M("#3e4240", 0.6),
    "t34": M("#465028", 0.55, var=(5, 0.88, 1.1)),
    "t34_dark": M("#30381a", 0.6),
    "horse_bay": M("#6e3e20", 0.5, sheen=0.25, var=(12, 0.85, 1.1)),
    "horse_mane_dark": M("#1e1410", 0.7, var=(60, 0.75, 1.25)),
    "rubber": M("#1c1b1d", 0.75),
    "khaki_modern": M("#6e6a44", 0.75, sheen=0.25, var=(40, 0.88, 1.1)),
    "helmet_green": M("#4a5434", 0.5, 0.1),
    "crimson_coat": M("#a41c2c", 0.7, sheen=0.3, var=(40, 0.86, 1.12)),
})


def _fx_mats():
    """Прозрачный дым, светящиеся вспышки и размытый винт (их нет в палитре фигурок)."""
    def mk_mat(key, col, alpha=1.0, emit=0.0, rough=0.95, shadow=False):
        m = bpy.data.materials.new(key)
        m.use_nodes = True
        b = m.node_tree.nodes["Principled BSDF"]
        c = U.hexc(col)
        b.inputs["Base Color"].default_value = c
        b.inputs["Roughness"].default_value = rough
        U._in(b, "Specular IOR Level", "Specular").default_value = 0.1
        if emit:
            U._in(b, "Emission Color").default_value = c
            U._in(b, "Emission Strength").default_value = emit
        if alpha < 1.0:
            b.inputs["Alpha"].default_value = alpha
            m.blend_method = "BLEND"
            m.shadow_method = "HASHED" if shadow else "NONE"
            try:
                m.show_transparent_back = False
            except AttributeError:
                pass
        elif emit:
            m.shadow_method = "NONE"
        U.MATS[key] = m
    for a in (85, 65, 45, 28):
        mk_mat("smoke%d" % a, "#eeece6", a / 100.0)
        mk_mat("smokeD%d" % a, "#7a7670", a / 100.0)
    mk_mat("fx_flash", "#ffa030", emit=14.0, rough=0.5)
    mk_mat("fx_core", "#fff0b0", emit=26.0, rough=0.5)
    mk_mat("fx_glow", "#ffb040", alpha=0.38, emit=6.0)
    mk_mat("fx_tracer", "#ffd870", emit=16.0)
    mk_mat("fx_flame", "#ff7a1a", emit=12.0)
    mk_mat("prop_blur", "#3a3a3c", alpha=0.32, rough=0.5)
    mk_mat("match", "#ff6010", emit=8.0)
    mk_mat("canopy_t", "#a8d4ec", alpha=0.42, rough=0.08)


# ----------------------------------------------------------------------------------------------
#  Эффекты
# ----------------------------------------------------------------------------------------------
def _basis(d):
    d = V3(d).normalized()
    side = d.cross(V3((0, 0, 1)))
    if side.length < 1e-4:
        side = V3((1, 0, 0))
    side.normalize()
    return d, side, side.cross(d).normalized()


def flash(p, d, s=1.0, var=0):
    """Вспышка выстрела у дула p в сторону d: язык пламени, лучи и прозрачный ореол."""
    p = V3(p)
    d, side, up = _basis(d)
    cyl2(p, p + d * 0.34 * s, 0.075 * s, 0.0, "fx_flash", segs=10)
    ell(p + d * 0.07 * s, 0.065 * s, "fx_core", segs=10, rings=6)
    for k, (a, b) in enumerate(((0.75, 0.0), (-0.75, 0.0), (0.0, 0.8), (0.0, -0.8))):
        if (k + var) % 4 == 3:
            continue
        q = (d * 0.55 + up * a + side * b).normalized()
        cyl2(p, p + q * 0.2 * s, 0.032 * s, 0.0, "fx_flash", segs=6)
    ell(p + d * 0.12 * s, 0.2 * s, "fx_glow", segs=14, rings=8)


def smoke(c, r, alpha, dark=False, n=3, seed=0, rise=(0, 0, 1)):
    """Клуб дыма из n шаров; alpha — непрозрачность, %: 85, 65, 45 или 28."""
    rnd = random.Random(seed * 31 + int(r * 1000))
    m = ("smokeD%d" if dark else "smoke%d") % alpha
    c = V3(c)
    for i in range(n):
        o = V3((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-0.6, 1))) * r * 0.55
        puff(c + o + V3(rise) * r * 0.2 * i, r * rnd.uniform(0.62, 0.9), m)


def tracer(p, d, length, r=0.012):
    p, d = V3(p), V3(d).normalized()
    cyl2(p, p + d * length, r, r * 0.6, "fx_tracer", segs=6)


# ----------------------------------------------------------------------------------------------
#  Скелет: ноги (шаг, стойка, выпад), наклон тела вокруг таза
# ----------------------------------------------------------------------------------------------
HIP_Z = 0.86


def legs_ik(feet, hz, lift=None):
    out = []
    for i, s in enumerate((-1, 1)):
        hip = V3((s * 0.098, 0.0, hz))
        fx, fy = feet[i]
        ank = V3((fx, fy, 0.09 + (lift[i] if lift else 0.0)))
        out.append((hip, U.ik_elbow(hip, ank, 0.405, 0.405, V3((0, -1, 0.12))), ank))
    return out


def gait(p, stride=0.46, lift=0.13, duty=0.58):
    """Шаг: p — фаза 0..1 (две ступени). Правая нога (к зрителю) впереди при p = 0.
    Опорная стопа едет назад по земле, переносимая — вперёд по дуге; таз выше всего над опорой."""
    hz = 0.845 + 0.022 * cos(4 * pi * (p - duty / 2))
    feet, lifts = [], []
    for i, s in enumerate((-1, 1)):
        q = (p + 0.5 * i) % 1.0
        if q < duty:
            fy, fz = -stride / 2 + stride * q / duty, 0.0
        else:
            u = (q - duty) / (1 - duty)
            fy = stride / 2 - stride * (0.5 - 0.5 * cos(pi * u))
            fz = lift * sin(pi * u)
        feet.append((s * 0.115, fy)); lifts.append(fz)
    return legs_ik(feet, hz, lifts), hz - HIP_Z


def stance(kind):
    if kind == "stand":
        return legs_ik([(-0.11, 0.0), (0.11, 0.0)], HIP_Z), 0.0
    if kind == "aim":
        return legs_ik([(-0.15, 0.17), (0.12, -0.2)], 0.84), 0.84 - HIP_Z
    return legs_ik([(-0.15, 0.3), (0.13, -0.36)], 0.785), 0.785 - HIP_Z   # выпад


class body:
    """Верх тела: подъём dz и наклон вперёд lean (градусы) вокруг таза."""
    def __init__(self, dz=0.0, lean=0.0):
        self.dz, self.lean = dz, lean

    def __enter__(self):
        self.g = []
        for kw in ({"loc": (0, 0, HIP_Z + self.dz)}, {"rot": (self.lean, 0, 0)}, {"loc": (0, 0, -HIP_Z)}):
            g = grp(**kw)                      # создаём и входим по очереди: каждая — внутри предыдущей
            g.__enter__()
            self.g.append(g)

    def __exit__(self, *a):
        for g in reversed(self.g):
            g.__exit__()


# ----------------------------------------------------------------------------------------------
#  Головы врагов, головные уборы
# ----------------------------------------------------------------------------------------------
def head_male(skin="skin", hair="hair", cap=True, locks=True, beard=None):
    """Мужское лицо без ресниц и румянца; короткая стрижка, у кого-то усы или борода."""
    cyl2((0, 0, 1.43), (0, 0, 1.6), 0.056, 0.052, skin, segs=12)
    ell(HC, (0.146, 0.152, 0.163), skin, segs=24, rings=16)
    ell(H(0, -0.148, -0.03), (0.02, 0.026, 0.032), skin, segs=10, rings=8)
    for s in (-1, 1):
        ell(H(s * 0.05, -0.136, 0.004), (0.017, 0.01, 0.02), "eye", segs=10, rings=6)
        tube([H(s * 0.022, -0.146, 0.05), H(s * 0.055, -0.144, 0.056), H(s * 0.085, -0.126, 0.05)],
             [0.009, 0.01, 0.007], hair)
    tube([H(-0.03, -0.142, -0.085), H(0.03, -0.142, -0.085)], 0.006, "leather_dark")
    if cap:
        ell(H(0, 0.03, 0.05), (0.152, 0.156, 0.15), hair, segs=20, rings=12)
    if beard == "full":
        ell(H(0, -0.07, -0.13), (0.125, 0.1, 0.11), hair, segs=16, rings=10)
        tube([H(-0.07, -0.135, -0.055), H(0, -0.152, -0.062), H(0.07, -0.135, -0.055)], [0.014, 0.02, 0.014], hair)
    elif beard == "mustache":
        tube([H(-0.075, -0.128, -0.075), H(0, -0.152, -0.058), H(0.075, -0.128, -0.075)], [0.01, 0.017, 0.01], hair)


def hat_shishak_open():
    """Шишак с бармицей только сзади — лицо героини открыто."""
    lathe([(0.166, 0.0), (0.168, 0.06), (0.152, 0.13), (0.118, 0.2), (0.075, 0.26), (0.035, 0.32),
           (0.014, 0.37), (0.0, 0.41)], HC, "steel", segs=28, cap0=False)
    ring(H(0, 0, 0.02), 0.169, 0.169, 0.016, "gold", n=32)
    rings = [ell_ring(HC.z + 0.03, 0.17, 0.168, cy=HC.y, a0=1.9, a1=2 * pi - 1.9, closed=False),
             ell_ring(HC.z - 0.1, 0.178, 0.175, cy=HC.y + 0.01, a0=1.9, a1=2 * pi - 1.9, closed=False),
             ell_ring(HC.z - 0.2, 0.2, 0.18, cy=HC.y + 0.02, a0=1.9, a1=2 * pi - 1.9, closed=False)]
    loft(rings, "mail", closed=False, solid=0.012)


def hat_nasal():
    lathe([(0.168, 0.0), (0.168, 0.06), (0.15, 0.14), (0.11, 0.22), (0.06, 0.29), (0.015, 0.35), (0.0, 0.37)],
          HC, "steel_dark", segs=28, cap0=False)
    ring(H(0, 0, 0.01), 0.17, 0.17, 0.014, "iron", n=32)
    box((0.03, 0.02, 0.15), H(0, -0.172, -0.05), "steel_dark")


def hat_great_helm():
    lathe([(0.17, -0.24), (0.18, -0.08), (0.178, 0.1), (0.162, 0.2), (0.0, 0.215)], H(0, 0.01, 0), "steel",
          segs=28, cap0=False)
    box((0.26, 0.03, 0.026), H(0, -0.17, 0.035), "black")
    box((0.03, 0.02, 0.24), H(0, -0.18, -0.08), "black")
    box((0.2, 0.02, 0.026), H(0, -0.18, -0.08), "black")


def hat_tricorne():
    with grp(H(0, 0.0, 0.05), (-4, 0, 0)):
        lathe([(0.158, 0.0), (0.158, 0.12), (0.13, 0.17), (0.0, 0.18)], (0, 0, 0), "black_hat", segs=28, cap0=False)
        n = 36
        inner, outer = [], []
        for i in range(n):
            a = 2 * pi * i / n
            c3 = cos(3 * a)                                # углы — спереди и по бокам сзади
            r = 0.27 + 0.075 * c3
            z = 0.02 + 0.13 * (1 - c3) / 2
            inner.append((0.15 * sin(a), -0.15 * cos(a), 0.02))
            outer.append((r * sin(a), -r * cos(a), z))
        loft([inner, outer], "black_hat", sub=0, solid=0.016)
        tube(outer + [outer[0]], 0.01, "gold", res=2)


def hat_pickelhaube():
    lathe([(0.168, 0.03), (0.166, 0.12), (0.13, 0.205), (0.06, 0.24), (0.0, 0.245)], HC, "black_hat", segs=28,
          cap0=False)
    slab([(-0.13, 0.0), (0.13, 0.0), (0.09, -0.06), (-0.09, -0.06)], 0.01, "black_hat", "xy", loc=H(0, -0.14, 0.04),
         rot=(-16, 0, 0))
    cyl2(H(0, 0, 0.235), H(0, 0, 0.36), 0.028, 0.004, "brass", segs=10)
    ell(H(0, 0, 0.24), (0.045, 0.045, 0.02), "brass", segs=12, rings=6)
    ell(H(0, -0.16, 0.12), (0.06, 0.02, 0.05), "brass", segs=12, rings=6)


def hat_stahlhelm():
    c = H(0, 0.012, 0)
    lathe([(0.172, 0.05), (0.17, 0.12), (0.135, 0.2), (0.075, 0.24), (0.0, 0.252)], c, "fg_helmet", segs=32,
          cap0=False)
    lathe([(0.172, 0.05), (0.178, -0.03), (0.2, -0.11)], c, "fg_helmet", segs=24, a0=1.0, a1=2 * pi - 1.0,
          cap0=False, cap1=False, solid=0.012)
    lathe([(0.172, 0.05), (0.19, 0.035)], c, "fg_helmet", segs=12, a0=-1.05, a1=1.05, cap0=False, cap1=False,
          solid=0.01)


def hat_ssh(m="helmet_green"):
    """Советская стальная каска со звёздочкой."""
    c = H(0, 0.008, 0)
    lathe([(0.176, 0.02), (0.174, 0.1), (0.14, 0.19), (0.08, 0.235), (0.0, 0.248)], c, m, segs=32, cap0=False)
    lathe([(0.176, 0.02), (0.188, -0.02)], c, m, segs=28, cap0=False, cap1=False, solid=0.01)
    star(H(0, -0.178, 0.1), 0.035, "red_star", t=0.01, rot=(-20, 0, 0))


def hat_strelets():
    lathe([(0.17, 0.0), (0.17, 0.08), (0.15, 0.24), (0.1, 0.33), (0.0, 0.36)], HC, "strelets", segs=28, cap0=False)
    lathe([(0.168, -0.01), (0.186, 0.0), (0.188, 0.08), (0.17, 0.09)], HC, "fur_dark", segs=28, cap0=False,
          cap1=False)


# ----------------------------------------------------------------------------------------------
#  Оружие в руках (локальная Z — вдоль оружия от приклада/рукояти)
# ----------------------------------------------------------------------------------------------
def round_shield(c, n, r=0.25, m="shield_red", rim="gold", boss="steel"):
    with grp(matrix=look(c, n, (0, 0, 1))):
        lathe([(0.0, 0.035), (r * 0.6, 0.022), (r, 0.0), (r, -0.018), (0.0, -0.012)], (0, 0, 0), m, segs=28)
        ell((0, 0, 0.04), (0.06, 0.06, 0.04), boss, segs=14, rings=8)
        ring((0, 0, 0.0), r, r, 0.014, rim, n=28)


def heater_shield(c, n, m="teut_white", cross="black"):
    with grp(matrix=look(c, n, (0, 0, 1))):
        w, h1, h2 = 0.42, 0.22, 0.4
        out = [(-w / 2, h1), (w / 2, h1), (w / 2, 0.0), (w * 0.3, -h2 * 0.6), (0.0, -h2), (-w * 0.3, -h2 * 0.6),
               (-w / 2, 0.0)]
        slab([(x, z) for (x, z) in out], 0.03, m, "xy", bevel=0.004, bend=lambda u, v: 0.4 * u * u)
        slab([(-0.03, h1 - 0.03), (0.03, h1 - 0.03), (0.03, -h2 + 0.08), (-0.03, -h2 + 0.08)], 0.04, cross, "xy",
             bend=lambda u, v: 0.4 * u * u + 0.004)
        slab([(-w / 2 + 0.04, 0.09), (w / 2 - 0.04, 0.09), (w / 2 - 0.04, 0.03), (-w / 2 + 0.04, 0.03)], 0.04, cross,
             "xy", bend=lambda u, v: 0.4 * u * u + 0.004)


def sword(p0, d, L=0.8):
    with grp(matrix=look(p0, d, (0, 0, 1))):
        cyl2((0, 0, -0.1), (0, 0, 0.02), 0.022, 0.02, "leather_dark", segs=8)
        ell((0, 0, -0.12), 0.034, "brass", segs=10, rings=6)
        box((0.24, 0.032, 0.03), (0, 0, 0.035), "steel_dark", bevel=0.005)
        hexa([(-0.03, -0.007, 0.05), (0.03, -0.007, 0.05), (0.03, 0.007, 0.05), (-0.03, 0.007, 0.05),
              (-0.006, -0.004, L), (0.006, -0.004, L), (0.006, 0.004, L), (-0.006, 0.004, L)], "steel")


def crossbow(p0, d, drawn=True, bolt=True):
    with grp(matrix=look(p0, d, (0, 0, 1))):
        box((0.06, 0.08, 0.82), (0, 0, 0.41), "wood_dark", bevel=0.012)
        tube([(-0.36, 0.01, 0.68), (0, 0.0, 0.8), (0.36, 0.01, 0.68)], [0.022, 0.036, 0.022], "wood_red")
        tube([(-0.36, 0.015, 0.68), (0, 0.03, 0.46 if drawn else 0.79), (0.36, 0.015, 0.68)], 0.007, "rope", res=2)
        ring((0, 0.0, 0.84), 0.05, 0.035, 0.009, "steel_dark", tilt=(90, 0, 0))
        if bolt:
            cyl2((0, 0.05, 0.46), (0, 0.05, 0.9), 0.009, 0.009, "wood_light", segs=6)
            cyl2((0, 0.05, 0.9), (0, 0.05, 0.96), 0.014, 0.0, "steel_dark", segs=6)


def bolt_flying(p, d):
    p, d = V3(p), V3(d).normalized()
    cyl2(p, p + d * 0.42, 0.01, 0.01, "wood_light", segs=6)
    cyl2(p + d * 0.42, p + d * 0.49, 0.015, 0.0, "steel_dark", segs=6)
    cyl2(p - d * 0.25, p, 0.0, 0.008, "smoke45", segs=6)


def smg(p0, d, up=(0, 0, 1)):
    """Автомат: деревянный приклад, ствольная коробка, изогнутый магазин."""
    with grp(matrix=look(p0, d, up)):
        hexa([(-0.024, -0.07, 0), (0.024, -0.07, 0), (0.024, 0.02, 0), (-0.024, 0.02, 0),
              (-0.02, -0.03, 0.24), (0.02, -0.03, 0.24), (0.02, 0.02, 0.24), (-0.02, 0.02, 0.24)], "wood", bevel=0.005)
        box((0.045, 0.06, 0.3), (0, 0.0, 0.38), "gunmetal", bevel=0.006)
        box((0.042, 0.045, 0.16), (0, 0.0, 0.6), "wood", bevel=0.006)
        cyl2((0, 0.018, 0.5), (0, 0.018, 0.82), 0.012, 0.011, "gunmetal", segs=8)
        with grp((0, -0.05, 0.42), (-25, 0, 0)):
            box((0.03, 0.17, 0.06), (0, -0.08, 0), "gunmetal")
        box((0.03, 0.08, 0.035), (0, -0.05, 0.27), "wood", rot=(20, 0, 0))


def berdysh_back():
    cyl2((0.2, 0.2, 0.25), (-0.18, 0.22, 2.0), 0.018, 0.016, "wood_dark", segs=8)
    with grp(matrix=look(V3((-0.12, 0.25, 1.62)), V3((-0.38, 0.02, 1.75)), (0, 1, 0))):
        slab([(0.0, 0.0), (0.09, 0.05), (0.13, 0.25), (0.08, 0.42), (0.0, 0.36)], 0.012, "steel", "xz", bevel=0.003)


# ----------------------------------------------------------------------------------------------
#  Пехота. V — описание вида, mode — stand/walk/fire, k — номер кадра
# ----------------------------------------------------------------------------------------------
def _aim(r=0.0):
    a = radians(2 + 9 * r)
    return V3((-0.16, -0.12 + 0.07 * r, 1.33 + 0.02 * r)), V3((0, -cos(a), sin(a)))


def _two_hands(b, d, g1, g2):
    return [b + d * g1 + V3((-0.01, 0, -0.055)), b + d * g2 + V3((0.02, 0, -0.05))]


def infantry_pose(V, mode, k):
    """Поза кадра: ноги, подъём и наклон тела, кисти и локти, оружие и эффекты."""
    w = V["weapon"]
    P = {"lean": 0.0, "fx": [], "free": None}
    rest_l = V3((0.25, 0.0, 0.88))
    if mode == "walk":
        p = k / float(N_WALK)
        P["legs"], P["dz"] = gait(p)
        P["lean"] = 3.0
        sw = cos(2 * pi * p)
        rest_l = V3((0.25, -0.17 * sw, 0.9 + 0.03 * abs(sw)))
        P["swing"] = sw
    elif mode == "stand":
        P["legs"], P["dz"] = stance("stand")
    else:
        lunge = w in ("spear", "sword", "rammer") and k in (2, 3)
        P["legs"], P["dz"] = stance("lunge" if lunge else "aim")
        if lunge:
            P["lean"] = 9.0
    poles = [V3((-1, 0.3, -0.7)), V3((1, 0.3, -1))]
    hands = [None, rest_l]
    if w in ("musket", "rifle", "smg", "rocket"):
        L = V.get("L", 1.4)
        g1, g2 = (0.22, 0.5) if w == "smg" else (0.3, 0.66)
        if mode == "walk" or (mode == "stand" and w == "smg"):
            if w == "smg":
                b = V3((-0.25, -0.2, 0.98)); d = V3((0.45, -0.28, 0.65)).normalized()
                hands = [b + d * 0.2, b + d * 0.5]
                poles = [V3((-1, 0, -1)), V3((1, -0.5, -1))]
            else:
                b = V3((-0.27, -0.17, 0.97)); sh = V3((-0.24, 0.1, 1.47))
                d = (sh - b).normalized()
                b = b - d * 0.08
                hands[0] = b + d * 0.1
                poles[0] = V3((-1, -0.2, -1))
            P["gun"] = (b, d, (0, -1, 0) if w != "smg" else (0, 0, 1))
        elif mode == "stand":
            b = V3((-0.31, -0.16, 0.03)); d = V3((0, 0.04, 1)).normalized()
            hands[0] = b + d * (0.92 if L > 1.1 else 0.8)
            poles[0] = V3((-0.5, 0.6, -1))
            P["gun"] = (b, d, (0, -1, 0))
        else:
            r = {1: 1.0, 2: 0.45}.get(k, 0.0)
            if w == "smg":
                r = 0.35 if k in (1, 3) else (0.2 if k == 2 else 0.0)
            reload = w == "musket" and k in (4, 5)
            if reload:
                b = V3((-0.2, -0.12, 0.86 + (0.2 if k == 5 else 0.0)))
                d = V3((0.1, -0.4 - (0.35 if k == 5 else 0.0), 1.0 if k == 4 else 0.45)).normalized()
                hands = _two_hands(b, d, 0.22, 0.55)
            else:
                b, d = _aim(r)
                hands = _two_hands(b, d, g1, g2)
                if w == "rifle" and k == 3:            # затвор
                    hands[0] = b + d * 0.36 + V3((-0.06, 0, 0.04))
            poles = [V3((-1, 0.3, -0.6)), V3((0.4, 0, -1))]
            P["gun"] = (b, d, (0, 0, 1))
            P["lean"] = -4.0 * r
            mz = b + d * (L + V.get("bayonet", 0.0) * 0.0)
            big = w == "musket"
            if w == "smg":
                if k in (1, 2, 3):
                    P["fx"].append(("flash", mz, d, 0.75, k))
                if k in (2, 3, 4):
                    P["fx"].append(("smoke", mz + d * (0.08 * k) + V3((0, 0, 0.04 * k)), 0.07 + 0.02 * k,
                                    65 if k < 4 else 45, False, 2))
            else:
                # дым: у дула клуб, дальше растёт и поднимается вверх (в клетке справа места мало)
                ab, ad = _aim(0.0)
                mz0 = ab + ad * L                              # дым — у дула в позе прицела, даже при заряжании
                if k == 1:
                    P["fx"].append(("flash", mz, d, 1.2 if big else 0.95, 0))
                    P["fx"].append(("smoke", mz + d * 0.08, 0.1 if big else 0.06, 65, False, 3))
                elif k == 2:
                    P["fx"].append(("smoke", mz0 + d * 0.12 + V3((0, 0, 0.08)), 0.2 if big else 0.09,
                                    65 if big else 45, False, 4))
                elif k == 3:
                    P["fx"].append(("smoke", mz0 + d * 0.1 + V3((0, 0.1, 0.24)), 0.25 if big else 0.11,
                                    45 if big else 28, False, 4))
                elif k == 4:
                    P["fx"].append(("smoke", mz0 + V3((0, 0.25, 0.42)), 0.28 if big else 0.12, 28, False, 4))
                elif k == 5 and big:
                    P["fx"].append(("smoke", mz0 + V3((0, 0.45, 0.6)), 0.3, 28, False, 3))
    elif w in ("spear", "rammer", "linstock"):
        L = V.get("L", 2.2)
        if mode == "fire" and w == "spear":
            off = [0.0, -0.3, 0.42, 0.38, -0.12, 0.0][k]
            ang = radians([22, 18, 8, 8, 16, 22][k])
            d = V3((0, -cos(ang), sin(ang)))
            hand = V3((-0.25, -0.12, 1.06)) + d * off
            hands[0] = hand
            poles[0] = V3((-1, 0.6, -0.6))
            P["spear"] = (hand - d * 0.95, d)
        elif mode == "fire" and w == "rammer":
            off = [0.0, 0.0, 0.3, 0.3, 0.0, 0.0][k]
            d = V3((0, -0.9, -0.44)).normalized() if k in (2, 3) else V3((0, -0.12, 1)).normalized()
            hand = V3((-0.27, -0.2 - off, 1.05))
            hands = [hand, hand + d * 0.35 + V3((0.05, 0, 0))]
            P["spear"] = (hand - d * (0.5 if k in (2, 3) else 0.95), d)
        else:
            sw = P.get("swing", 0.0)
            hand = V3((-0.31, -0.27 + 0.03 * sw, 1.0))
            d = V3((0, -0.26, 1)).normalized()             # древко наклонено вперёд — не заслоняет лицо
            hands[0] = hand
            poles[0] = V3((-0.6, 0.6, -1))
            P["spear"] = (hand - d * 0.95, d)
    elif w == "sword":
        if mode == "fire":
            hs = [(-0.26, -0.22, 1.1), (-0.24, 0.02, 1.72), (-0.22, -0.18, 1.78), (-0.22, -0.44, 1.36),
                  (-0.24, -0.36, 0.98), (-0.26, -0.26, 1.06)][k]
            ds = [(0, -0.55, 0.83), (0, 0.55, 0.83), (0, -0.25, 0.97), (0, -0.97, -0.08), (0, -0.7, -0.7),
                  (0, -0.75, 0.6)][k]
        elif mode == "walk":
            hs, ds = (-0.27, -0.17, 1.06), (0, 0.32, 0.95)
        else:
            hs, ds = (-0.22, -0.24, 1.0), (0, -0.25, 0.97)
        hands[0] = V3(hs)
        poles[0] = V3((-1, 0.5, -0.5))
        P["sword"] = (V3(hs), V3(ds).normalized())
    elif w == "crossbow":
        drawn, bolt = True, True
        if mode == "walk":
            b = V3((-0.27, -0.15, 0.98)); d = V3((0.04, 0.32, 0.95)).normalized()
            hands[0] = b + d * 0.06
        elif mode == "stand":
            b = V3((-0.2, 0.06, 1.02)); d = V3((0, -0.8, -0.55)).normalized()
            hands = [b + d * 0.12, b + d * 0.48]
        else:
            if k in (0, 1, 2):
                b, d = _aim(0.5 if k == 1 else 0.0)
                b = b + V3((0, 0.06, 0))
                hands = _two_hands(b, d, 0.14, 0.45)
                drawn = bolt = k == 0
                if k in (1, 2):
                    P["fx"].append(("bolt", b + d * (1.0 if k == 1 else 1.55), d))
            else:
                b = V3((-0.18, -0.1, 1.2 if k < 5 else 1.26))
                d = V3((0, -0.55, -0.84)).normalized() if k < 5 else V3((0, -0.85, -0.4)).normalized()
                hands = _two_hands(b, d, 0.2, 0.55)
                drawn, bolt = k >= 4, k >= 4
                if k == 3:
                    hands[0] = b + d * 0.5 + V3((-0.04, 0.0, 0.06))
            poles = [V3((-1, 0.3, -0.6)), V3((0.4, 0, -1))]
        P["xbow"] = (b, d, drawn, bolt)
    elif w == "flags":
        pass
    if V.get("shield"):
        sw = P.get("swing", 0.0)
        P["shield_at"] = V3((0.1, -0.33 - 0.02 * sw, 1.06))
        hands[1] = P["shield_at"] + V3((0.02, 0.06, 0.0))
        poles[1] = V3((1, 0.2, -1))
    P["hands"], P["poles"] = hands, poles
    return P


def infantry(V, mode, k, female=None):
    P = infantry_pose(V, mode, k)
    fem = V.get("female", True) if female is None else female
    for i, s in enumerate((-1, 1)):
        hip, knee, ank = P["legs"][i]
        U.leg(s, V["pants"], V["boots"], hip, knee, ank, V.get("boot_top", 0.3))
    old_head = U.head
    if not fem:
        beard = V.get("beard")
        U.head = lambda skin="skin", hair="hair", cap=True, locks=True: head_male(skin, hair, cap, locks, beard)
    try:
        with body(P["dz"], P["lean"]):
            for sk in V.get("skirts", []):
                U.skirt(*sk)
            if V.get("cloak"):
                sw2 = 0.03 * P.get("swing", 0.0) ** 2
                rings = [ell_ring(1.42, 0.2, 0.15, cy=0.02, a0=pi / 2 - 0.25, a1=3 * pi / 2 + 0.25, closed=False),
                         ell_ring(1.0, 0.215, 0.17, cy=0.05, a0=pi / 2 - 0.2, a1=3 * pi / 2 + 0.2, closed=False),
                         ell_ring(0.5, 0.24, 0.2, cy=0.09 + sw2, a0=pi / 2 - 0.2, a1=3 * pi / 2 + 0.2, closed=False)]
                loft(rings, V["cloak"], closed=False, solid=0.02)
            U.human(V["coat"], sleeve=V.get("sleeve"), hands=P["hands"], poles=P["poles"],
                    hand_m=V.get("hand_m", ("skin", "skin")), belt=V.get("belt"), hair=V.get("hair", "hair"),
                    cap=V.get("cap", True), locks=V.get("locks", fem), hat=V.get("hat"), hat_dz=V.get("hat_dz", U.HAT_DZ),
                    braid=V.get("braid") if fem else None, braid_end=V.get("braid_end", 1.04), cuff=V.get("cuff"),
                    k=V.get("k", 1.06 if fem else 1.12), bust_only=True)
            if V.get("extras"):
                V["extras"]()
            w = V["weapon"]
            if "gun" in P:
                b, d, up = P["gun"]
                if w == "smg":
                    smg(b, d, up)
                elif w == "rocket":
                    U.rocket(b + d * 0.55, d, r=0.06, body=0.42, stick=0.5)
                else:
                    U.rifle(b, b + d * V.get("L", 1.4), up=up, bayonet=V.get("bayonet", 0.0), scope=V.get("scope", False))
                    if V.get("match"):
                        q = b + d * 0.45
                        ell(q + V3((0, 0, 0.03)), 0.014, "match", segs=6, rings=4)
            if "spear" in P:
                p0, d = P["spear"]
                L = V.get("L", 2.2)
                if w == "spear":
                    U.spear(p0, p0 + d * L, blade=0.22)
                else:
                    cyl2(p0, p0 + d * L, 0.017, 0.017, "wood_light", segs=8)
                    if w == "rammer":
                        cyl2(p0 + d * (L - 0.16), p0 + d * (L + 0.02), 0.05, 0.05, "fur_dark", segs=12)
                    else:
                        ell(p0 + d * (L + 0.02), 0.03, "match", segs=8, rings=6)
            if "sword" in P:
                hnd, d = P["sword"]
                sword(hnd, d)
            if "xbow" in P:
                b, d, drawn, bolt = P["xbow"]
                crossbow(b, d, drawn, bolt)
            if "shield_at" in P:
                n = V3((-0.25, -1, 0.05))
                if V["shield"] == "heater":
                    heater_shield(P["shield_at"], n)
                else:
                    round_shield(P["shield_at"], n, m=V["shield"])
            if V.get("back"):
                V["back"]()
            for fx in P["fx"]:
                if fx[0] == "flash":
                    flash(fx[1], fx[2], fx[3], fx[4])
                elif fx[0] == "smoke":
                    smoke(fx[1], fx[2], fx[3], fx[4], fx[5], seed=k)
                elif fx[0] == "bolt":
                    bolt_flying(fx[1], fx[2])
    finally:
        U.head = old_head


# --- детали формы ---
def crossbelts(m="white_cloth"):
    for t in (38, -38):
        ring((0, -0.012, 1.17), 0.228, 0.158, 0.02, m, tilt=(0, t, 0))


def buttons(m="brass", x=0.0, zs=(1.08, 1.17, 1.26, 1.35)):
    for z in zs:
        ell(U.on_chest(x, z, 0.004), 0.013, m, segs=8, rings=6)


def chest_cross(m="black"):
    fy = lambda u, v: U.front_y(v, u) - 0.03
    slab([(-0.035, 0.98), (0.035, 0.98), (0.035, 1.34), (-0.035, 1.34)], 0.012, m, bend=fy)
    slab([(-0.13, 1.17), (0.13, 1.17), (0.13, 1.24), (-0.13, 1.24)], 0.012, m, bend=fy)


def surcoat(m):
    rings = [ell_ring(z, rx * 1.06 + 0.014, ry * 1.04 + 0.014, cy=cy, n=28) for (z, rx, ry, cy) in U.TORSO[1:6]]
    loft(rings, m, sub=1)


def bandolier():
    tl = radians(-40)
    ring((0, -0.01, 1.2), 0.235, 0.16, 0.026, "leather", tilt=(0, -40, 0))
    for k in range(5):
        t = 3 * pi / 2 + (k - 2.2) * 0.33
        x0, y0 = 0.245 * cos(t), 0.175 * sin(t)
        p = V3((x0 * cos(tl), y0 - 0.01, 1.2 - x0 * sin(tl)))
        cyl2(p + V3((0, -0.02, -0.02)), p + V3((0, -0.02, -0.12)), 0.022, 0.02, "wood_light", segs=8)


def pouches(m="leather_dark"):
    for x in (-0.08, 0.08):
        box((0.07, 0.035, 0.065), (x, U.front_y(0.98, x) - 0.012, 0.96), m, bevel=0.01)


def epaulettes(m="red"):
    for s in (-1, 1):
        ell((s * (SH_X + 0.01), 0.0, SH_Z + 0.06), (0.07, 0.07, 0.03), m, segs=12, rings=6)


def bedroll():
    with grp((0, 0.15, 1.38), (0, 90, 0)):
        tube([(0, 0, -0.2), (0, 0.04, 0), (0, 0, 0.2)], 0.05, "canvas")


# ----------------------------------------------------------------------------------------------
#  Конница: лошадь в галопе и всадница
# ----------------------------------------------------------------------------------------------
HORSE_C = V3((0, 0, 1.0))


def horse_pose(mode, k):
    if mode == "stand":
        return [0.0] * 4, 0.0, 0.0, None
    n = N_WALK if mode == "walk" else N_FIRE
    p = k / float(n)
    pitch = 4.5 * sin(2 * pi * (p + 0.15))
    dz = 0.05 * sin(2 * pi * (p - 0.05))
    return None, dz, pitch, p


def horse_leg_targets(p):
    """Галоп: левая задняя, правая задняя, левая передняя, правая передняя."""
    out = []
    for (front, side, ph) in ((False, 1, 0.0), (False, -1, 0.1), (True, 1, 0.42), (True, -1, 0.52)):
        q = (p - ph) % 1.0
        duty = 0.36
        if q < duty:
            u = q / duty
            dy, z = -0.34 + 0.68 * u, 0.0
        else:
            u = (q - duty) / (1 - duty)
            dy = 0.34 - 0.68 * (0.5 - 0.5 * cos(pi * u))
            z = (0.34 if front else 0.26) * sin(pi * u) ** 0.8
        out.append((front, side, dy, z))
    return out


def horse(coat, mane, mode, k, saddle="crimson", rider=None, armor=None):
    _, dz, pitch, p = horse_pose(mode, k)
    T = Matrix.Translation(HORSE_C + V3((0, 0, dz))) @ Matrix.Rotation(radians(pitch), 4, "X") @ \
        Matrix.Translation(-HORSE_C)
    tops = {(False, 1): V3((0.13, 0.42, 0.95)), (False, -1): V3((-0.13, 0.42, 0.95)),
            (True, 1): V3((0.12, -0.34, 0.95)), (True, -1): V3((-0.12, -0.34, 0.95))}
    if p is None:
        targets = [(f, s, 0.0, 0.0) for (f, s) in ((False, 1), (False, -1), (True, 1), (True, -1))]
    else:
        targets = horse_leg_targets(p)
    for (front, side, dy, z) in targets:
        top = T @ tops[(front, side)]
        hoof = V3((top.x, tops[(front, side)].y + dy, 0.0 + z))
        fet = hoof + V3((0, 0.03 if front else 0.02, 0.12))
        pole = V3((0, -1, 0.3)) if front else V3((0, 1, 0.2))
        mid = U.ik_elbow(top, fet, 0.45, 0.42, pole)
        tube([top, mid, fet], [0.088, 0.056, 0.045], coat)
        cyl2(fet, hoof + V3((0, -0.02 if z > 0.05 else 0.0, 0.0)), 0.048, 0.058, "black", segs=10)
    with grp(matrix=T):
        ell((0, 0.04, 0.98), (0.235, 0.55, 0.255), coat, segs=24, rings=14)
        ell((0, -0.3, 1.01), (0.225, 0.27, 0.275), coat)
        ell((0, 0.38, 1.0), (0.24, 0.27, 0.27), coat)
        nod = -1.5 * pitch
        with grp((0, -0.36, 1.06), (nod, 0, 0)):
            with grp((0, 0.36, -1.06)):
                tube([(0, -0.36, 1.06), (0, -0.55, 1.33), (0, -0.66, 1.53)], [0.17, 0.125, 0.1], coat)
                with grp((0, -0.7, 1.56), (52, 0, 0)):
                    ell((0, -0.16, 0), (0.085, 0.21, 0.095), coat)
                    ell((0, -0.33, -0.015), (0.075, 0.075, 0.082), coat)
                    for s in (-1, 1):
                        cyl2((s * 0.045, 0.05, 0.06), (s * 0.06, 0.13, 0.16), 0.03, 0.006, coat, segs=8)
                        ell((s * 0.075, -0.1, 0.035), (0.013, 0.022, 0.016), "eye", segs=8, rings=6)
                    ring((0, -0.2, 0), 0.09, 0.03, 0.012, "leather_dark", tilt=(0, 90, 0))
                    ring((0, -0.3, -0.01), 0.082, 0.08, 0.01, "leather_dark", tilt=(90, 0, 0))
                    if armor:
                        ell((0, -0.12, 0.05), (0.095, 0.2, 0.08), armor, segs=12, rings=8)
                fl = 0.05 * (sin(2 * pi * p) if p is not None else 0.0)
                tube([(0, -0.26, 1.28), (0, -0.45 + fl, 1.5), (0, -0.6 + fl, 1.7), (0, -0.7, 1.74)],
                     [0.04, 0.06, 0.055, 0.03], mane)
        wv = 0.0 if p is None else sin(2 * pi * p)
        tube([(0, 0.62, 1.12), (0, 0.84, 1.02 + 0.05 * wv), (0, 0.98, 0.85 + 0.08 * wv), (0, 1.02, 0.62 + 0.1 * wv)]
             if p is not None else [(0, 0.62, 1.12), (0, 0.8, 0.98), (0, 0.86, 0.74), (0, 0.83, 0.48)],
             [0.05, 0.08, 0.075, 0.04], mane)
        ell((0, 0.03, 1.2), (0.27, 0.38, 0.075), saddle, segs=20, rings=10)
        ring((0, 0.03, 1.17), 0.27, 0.38, 0.014, "gold", n=32)
        if rider:
            with grp((0, 0.02, 0.4)):
                rider(mode, k, p)


def cavalry(V, mode, k):
    def rider(mode, k, p):
        legs = [((s * 0.1, 0.0, 0.86), (s * 0.29, -0.2, 0.66), (s * 0.27, -0.06, 0.3)) for s in (-1, 1)]
        thrust = 0.0
        if mode == "fire":
            thrust = 0.22 * max(0.0, sin(2 * pi * k / N_FIRE))
            hnd = V3((-0.27, -0.12 - thrust, 1.08))
            p0 = V3((-0.25, 0.75 - thrust, 1.12)); d = (hnd - p0).normalized()
            d = (d + V3((0, 0, -0.06))).normalized()
            hands = [hnd, V3((0.12, -0.42, 1.06))]
        else:
            hnd = V3((-0.27, -0.18, 1.12))
            p0 = V3((-0.28, 0.42, 0.72)); d = (hnd - p0).normalized()
            hands = [hnd, V3((0.12, -0.42, 1.06))]
        U.human(V["coat"], pants=V["pants"], boots=V["boots"], legs=legs, boot_top=0.6, hands=hands,
                poles=[V3((-1, 0.4, -0.6)), V3((1, 0.3, -1))], belt=V.get("belt"), hair=V["hair"],
                hat=V["hat"], braid=V["braid"], braid_end=1.04, sleeve=V.get("sleeve"),
                hand_m=V.get("hand_m", ("skin", "skin")), cap=V.get("cap", True))
        if V.get("extras"):
            V["extras"]()
        tip = p0 + d * 2.3
        cyl2(p0, tip, 0.022, 0.014, "lance", segs=8)
        ell(hnd + d * 0.12, (0.04, 0.04, 0.06), "gold", segs=10, rings=6)
        cyl2(tip, tip + d * 0.14, 0.018, 0.0, "steel", segs=8)
        flap = 0.0 if p is None else sin(2 * pi * p)
        with grp(matrix=look(tip, d, (0, 0, 1))):
            bend = lambda a, b: 0.04 * sin(b * 9.0 + flap * 2.0)
            m1, m2 = V.get("pennant", ("crimson", "gold_cloth"))
            slab([(0.0, -0.04), (-0.03, -0.2), (-0.19, -0.74), (-0.11, -0.56), (-0.06, -0.74), (0.0, -0.3)],
                 0.012, m1, "yz", bend)
            slab([(-0.03, -0.2), (-0.12, -0.52), (-0.19, -0.74), (-0.11, -0.56), (-0.06, -0.6)],
                 0.016, m2, "yz", bend)
        if V.get("wings"):
            for s in (-1, 1):
                U.wing(s, [(s * 0.13, 0.2, 1.0), (s * 0.2, 0.34, 1.5), (s * 0.2, 0.26, 1.98), (s * 0.15, 0.04, 2.22)],
                       n_feather=15)
    horse(V["horse"], V["mane"], mode, k, V.get("saddle", "crimson"), rider, V.get("barding"))


# ----------------------------------------------------------------------------------------------
#  Пушка, ракетный станок
# ----------------------------------------------------------------------------------------------
def field_gun(recoil=0.0, wheel_a=0.0):
    with grp((0, recoil, 0)):
        for s in (-1, 1):
            with grp((s * 0.38, 0.0, 0.5), (wheel_a, 0, 0)):
                U.wheel((0, 0, 0), 0.5, 0.075, rim="gun_green", spokes=12, hub="iron", tire="iron")
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
    return V3((0, recoil - 0.96, 0.75))


CREW = dict(coat="green_coat", pants="trousers_dark", boots="boots_black", boot_top=0.36, belt="white_cloth",
            cuff="black", hair="hair_fair", hat=U.hat_forage, braid="back",
            skirts=[("green_coat", 0.62, 0.22, 0.19, 1.02, 0.0, pi / 2 + 0.3, 3 * pi / 2 - 0.3)],
            extras=lambda: buttons(), weapon="rammer", L=1.75)


def artillery(V, mode, k):
    if mode == "fire":
        rec = [0.0, 0.2, 0.16, 0.06, 0.0, 0.0][k]
        mz = field_gun(rec)
        if k == 1:
            flash(mz, (0, -1, 0.1), 1.6)
            smoke(mz + V3((0, -0.2, 0.05)), 0.2, 85, n=4, seed=1)
        elif k in (2, 3, 4, 5):
            a = [85, 65, 45, 28][k - 2]
            smoke(mz + V3((0, -0.3 - 0.12 * k, 0.1 * k)), 0.24 + 0.06 * k, a, n=5, seed=k)
        with grp((-0.5, 0.75, 0), (0, 0, 0)):
            infantry(CREW, "fire", k if k in (2, 3) else 0)   # 2–3: банник вперёд
    elif mode == "walk":
        p = k / float(N_WALK)
        field_gun(0.0, wheel_a=p * 30.0)                 # 12 спиц: за цикл — 1/12 оборота
        with grp((-0.62, 0.35, 0)):
            infantry(CREW, "walk", k)
    else:
        field_gun()
        with grp((-0.55, 0.7, 0)):
            infantry(CREW, "stand", 0)


ROCKET_CREW = dict(CREW, coat="teal_coat", skirts=[("teal_coat", 0.6, 0.24, 0.19, 1.02, 0.0, pi / 2 + 0.3, 3 * pi / 2 - 0.3)],
                   belt="gold", cuff="gold_cloth", hat=lambda: U.hat_fur_cap("crimson", "feather"), weapon="linstock",
                   L=1.5, hair="hair_red")


def rockets(V, mode, k):
    ang = radians(40)
    d = V3((0, -cos(ang), sin(ang)))
    n = V3((0, sin(ang), cos(ang)))
    P0 = V3((0.0, 0.42, 0.06))
    L = 1.2
    if mode == "walk":                                  # на марше ракету несут на плече
        infantry(dict(ROCKET_CREW, weapon="rocket"), "walk", k)
        return
    for s in (-1, 1):
        U.beam(P0 + V3((s * 0.2, 0, 0)), P0 + d * L + V3((s * 0.2, 0, 0)), 0.075, "wood", bevel=0.01)
        f = P0 + d * (L * 0.8) + V3((s * 0.2, 0, 0))
        U.beam(f, V3((s * 0.33, f.y - 0.1, 0.0)), 0.065, "wood", bevel=0.008)
    for j in range(4):
        q = P0 + d * (0.12 + j * 0.32) - n * 0.03
        U.beam(q + V3((-0.24, 0, 0)), q + V3((0.24, 0, 0)), 0.04, "wood_light", bevel=0.005)
    q = P0 + d * (L * 0.8) + V3((0, -0.1, -0.35))
    U.beam(q + V3((-0.31, 0, 0)), q + V3((0.31, 0, 0)), 0.045, "wood_light", bevel=0.005)
    # ракета: на станке, вспыхивает, уходит вверх, станок пустеет и снова заряжен
    t = {0: 0.0, 1: 0.05, 2: 0.9, 3: 2.2}.get(k, None) if mode == "fire" else 0.0
    if mode == "fire" and k == 5:
        t = 0.0
    if t is not None:
        q = P0 + d * (L * 0.42 + t) + n * 0.085
        U.rocket(q, d, r=0.07, body=0.5, stick=0.6)
        if mode == "fire" and k in (1, 2, 3):
            rx = 90 - math.degrees(ang)
            ell(q - d * 0.12, (0.09, 0.09, 0.2), "fx_flame", rot=(rx, 0, 0))
            ell(q - d * 0.08, (0.05, 0.05, 0.1), "fx_core", rot=(rx, 0, 0))
            for j, r in enumerate((0.08, 0.1, 0.12)):
                smoke(q - d * (0.3 + j * 0.22), r, 65, n=2, seed=j + k)
    if mode == "fire" and k in (1, 2, 3, 4):
        a = [85, 85, 65, 45][k - 1]
        smoke(P0 + V3((0, 0.18, 0.12)), 0.14 + 0.05 * k, a, n=4, seed=k)
    if mode == "fire" and k in (3, 4):
        smoke(P0 + d * 1.6 + V3((0, 0, 0.1 * k)), 0.14, 45 if k == 3 else 28, n=3, seed=9)
    with grp((-0.52, 0.55, 0)):
        infantry(ROCKET_CREW, "stand" if mode == "stand" else "fire", 0 if mode == "stand" else 4)


# ----------------------------------------------------------------------------------------------
#  Техника: гусеницы, катки, корпус и башня
# ----------------------------------------------------------------------------------------------
def track_path(y0, y1, zb, cf, rf, cr, rr):
    """Замкнутая лента (выпуклая оболочка): низ zb от y0 до y1, задний обод (центр cr, радиус rr),
    передний (cf, rf). Обход против часовой в осях (y, z): низ едет назад (+Y), верх — вперёд."""
    P = [(y0, zb), (y1, zb)]
    for (c, r) in ((cf, rf), (cr, rr)):
        P += [(c[0] + r * cos(2 * pi * i / 48), c[1] + r * sin(2 * pi * i / 48)) for i in range(48)]
    P = sorted(set((round(y, 5), round(z, 5)) for (y, z) in P))

    def half(seq):
        h = []
        for q in seq:
            while len(h) >= 2 and ((h[-1][0] - h[-2][0]) * (q[1] - h[-2][1]) -
                                   (h[-1][1] - h[-2][1]) * (q[0] - h[-2][0])) <= 0:
                h.pop()
            h.append(q)
        return h
    hull = half(P)[:-1] + half(P[::-1])[:-1]
    pts = [V3((0, y, z)) for (y, z) in hull] + [V3((0, hull[0][0], hull[0][1]))]
    acc = [0.0]
    for i in range(1, len(pts)):
        acc.append(acc[-1] + (pts[i] - pts[i - 1]).length)
    return pts, acc


def _path_at(pts, acc, s):
    tot = acc[-1]
    s %= tot
    for i in range(1, len(pts)):
        if acc[i] >= s:
            t = (s - acc[i - 1]) / max(acc[i] - acc[i - 1], 1e-6)
            return pts[i - 1].lerp(pts[i], t), (pts[i] - pts[i - 1]).normalized()
    return pts[-1], (pts[0] - pts[-1]).normalized()


def tracks(x, width, pts, acc, pitch, phase, m="track"):
    nlinks = int(acc[-1] / pitch)
    pitch = acc[-1] / nlinks
    for i in range(nlinks):
        p, t = _path_at(pts, acc, (i + phase) * pitch)
        nrm = V3((0, t.z, -t.y))                  # наружу от ленты (обход против часовой)
        with grp(matrix=look(V3((x, p.y, p.z)), t, nrm)):
            box((width, 0.032, pitch * 0.82), (0, 0, 0), m)
            box((width * 0.9, 0.018, pitch * 0.25), (0, 0.022, 0), m)


def road_wheel(c, r, ang, rim="t34_dark", holes=6, tyre_m="rubber", w=0.07):
    c = V3(c)
    with grp(c, (ang, 0, 0)):
        U.wheel_disc((0, 0, 0), r, w, tyre_m, segs=22)
        U.wheel_disc((-w * 0.08, 0, 0), r * 0.84, w * 1.1, rim, segs=22)
        U.wheel_disc((-w * 0.2, 0, 0), r * 0.25, w * 1.3, rim, segs=12)
        for i in range(holes):
            a = 2 * pi * i / holes
            U.wheel_disc((-w * 0.2, r * 0.55 * cos(a), r * 0.55 * sin(a)), r * 0.13, w * 1.25, "black", segs=10)


def tank(V, mode, k):
    """Средний танк: style t34 — наклонная броня, большие катки, звезда, танкистка в люке;
    panzer — серый, угловатый, маленькие катки, балочный крест на борту."""
    pz = V.get("style") == "panzer"
    body_m, dark_m = ("panzer", "panzer_dark") if pz else ("t34", "t34_dark")
    p = k / float(N_WALK) if mode == "walk" else 0.0
    pitch_link = 0.0628
    rec = [0.0, 0.16, 0.1, 0.04, 0.0, 0.0][k] if mode == "fire" else 0.0
    rock = [0.0, -2.0, -1.0, 0.5, 0.0, 0.0][k] if mode == "fire" else (0.6 * sin(2 * pi * p * 2) if mode == "walk" else 0.0)
    dz = 0.012 * sin(2 * pi * p * 2) if mode == "walk" else 0.0
    if pz:
        pts, acc = track_path(-0.82, 0.86, 0.035, (-0.98, 0.32), 0.13, (0.98, 0.27), 0.13)
        wheels = [(-0.72 + 0.205 * i, 0.135, 0.105) for i in range(8)]
    else:
        pts, acc = track_path(-0.8, 0.82, 0.035, (-0.95, 0.24), 0.16, (0.93, 0.21), 0.17)
        wheels = [(-0.68 + 0.34 * i, 0.2, 0.18) for i in range(5)]
    cyc = 3 * pitch_link                               # за цикл лента сдвигается на три звена
    for s in (-1, 1):
        tracks(s * 0.5, 0.25, pts, acc, pitch_link, p * 3.0, "track")
        for (y, z, r) in wheels:
            ang = math.degrees(p * cyc / r)
            road_wheel((s * 0.5 + s * 0.02, y, z), r, ang, rim=dark_m, holes=6 if not pz else 8, w=0.08 if pz else 0.1)
        ends = [((-0.98, 0.32), 0.12), ((0.98, 0.27), 0.11)] if pz else [((-0.95, 0.24), 0.14), ((0.93, 0.21), 0.15)]
        for (c, r) in ends:
            road_wheel((s * 0.51, c[0], c[1]), r, math.degrees(p * cyc / r), rim=dark_m, holes=5, tyre_m=dark_m)
        if pz:
            for y in (-0.5, -0.15, 0.2, 0.55):
                U.wheel_disc((s * 0.5, y, 0.4), 0.045, 0.08, "rubber", segs=12)
    with grp((0, 0, 0.45 + dz), (rock, 0, 0)):
        with grp((0, 0, -0.45)):
            if pz:
                box((0.74, 1.86, 0.3), (0, 0.0, 0.3), body_m, bevel=0.01)
                hexa([(-0.64, -0.98, 0.44), (0.64, -0.98, 0.44), (0.64, 0.98, 0.44), (-0.64, 0.98, 0.44),
                      (-0.62, -0.92, 0.64), (0.62, -0.92, 0.64), (0.62, 0.96, 0.64), (-0.62, 0.96, 0.64)], body_m,
                     bevel=0.012)
                box((0.5, 0.12, 0.1), (-0.12, -0.9, 0.7), body_m, bevel=0.01)
                box((0.08, 0.04, 0.03), (-0.2, -0.97, 0.7), "black")
                hexa([(-0.44, -0.52, 0.64), (0.44, -0.52, 0.64), (0.44, 0.38, 0.64), (-0.44, 0.38, 0.64),
                      (-0.35, -0.44, 0.92), (0.35, -0.44, 0.92), (0.35, 0.32, 0.92), (-0.35, 0.32, 0.92)], body_m,
                     bevel=0.01)
                box((0.6, 0.22, 0.2), (0, 0.44, 0.78), body_m, bevel=0.01)
                cyl2((0.0, 0.24, 0.92), (0.0, 0.24, 1.0), 0.12, 0.12, body_m, segs=16)
                cyl2((0.0, 0.24, 1.0), (0.0, 0.24, 1.02), 0.13, 0.11, dark_m, segs=16)
                box((0.26, 0.14, 0.2), (0, -0.56 + rec * 0.0, 0.76), body_m, bevel=0.015)
                cyl2((0, -0.6 + rec, 0.77), (0, -1.78 + rec, 0.77), 0.04, 0.036, body_m, segs=12)
                cyl2((0, -1.72 + rec, 0.77), (0, -1.86 + rec, 0.77), 0.06, 0.06, dark_m, segs=12)
                mz = V3((0, -1.86 + rec, 0.77))
                for (yy, zz, sc_) in ((0.15, 0.48, 1.0), (0.05, 0.8, 0.62)):
                    c = V3((-0.645 if zz < 0.6 else -0.405, yy, zz))
                    box((0.012, 0.3 * sc_, 0.09 * sc_), c, "white_cloth")
                    box((0.012, 0.09 * sc_, 0.3 * sc_), c, "white_cloth")
                    box((0.02, 0.22 * sc_, 0.05 * sc_), c + V3((-0.004, 0, 0)), "black")
                    box((0.02, 0.05 * sc_, 0.22 * sc_), c + V3((-0.004, 0, 0)), "black")
                for s in (-1, 1):
                    box((0.16, 1.9, 0.02), (s * 0.58, 0.0, 0.445), dark_m)
            else:
                box((0.74, 1.8, 0.3), (0, 0.04, 0.3), body_m, bevel=0.01)
                hexa([(-0.64, -1.02, 0.42), (0.64, -1.02, 0.42), (0.64, 0.98, 0.42), (-0.64, 0.98, 0.42),
                      (-0.5, -0.48, 0.63), (0.5, -0.48, 0.63), (0.5, 0.86, 0.63), (-0.5, 0.86, 0.63)], body_m,
                     bevel=0.014)
                hexa([(-0.4, -0.92, 0.16), (0.4, -0.92, 0.16), (0.4, -0.7, 0.16), (-0.4, -0.7, 0.16),
                      (-0.5, -1.02, 0.42), (0.5, -1.02, 0.42), (0.5, -0.7, 0.42), (-0.5, -0.7, 0.42)], body_m)
                box((0.12, 0.08, 0.1), (-0.22, -0.7, 0.62), dark_m, rot=(-30, 0, 0))
                cyl2((0.32, -0.92, 0.5), (0.36, -1.0, 0.56), 0.02, 0.02, dark_m, segs=8)
                rings = [ell_ring(0.63, 0.47, 0.52, cy=-0.12, n=8, a0=pi / 8, a1=2 * pi + pi / 8),
                         ell_ring(0.86, 0.39, 0.43, cy=-0.1, n=8, a0=pi / 8, a1=2 * pi + pi / 8)]
                loft(rings, body_m, cap1=True, smooth=False, sub=0)
                box((0.3, 0.14, 0.22), (0, -0.6, 0.74), body_m, bevel=0.03)
                cyl2((0, -0.62 + rec, 0.75), (0, -1.62 + rec, 0.77), 0.048, 0.04, body_m, segs=12)
                mz = V3((0, -1.62 + rec, 0.77))
                for s in (-1, 1):
                    box((0.16, 1.98, 0.02), (s * 0.58, -0.02, 0.43), dark_m)
                for x in (-0.22, 0.22):
                    cyl2((x - 0.15, 0.97, 0.5), (x + 0.15, 0.97, 0.5), 0.09, 0.09, dark_m, segs=14)
                star(V3((-0.415, -0.12, 0.73)), 0.075, "red_star", rot=(0, 0, 90), t=0.04)
                # люк и танкистка
                hx, hy = 0.08, 0.05
                cyl2((hx, hy, 0.85), (hx, hy, 0.92), 0.17, 0.165, body_m, segs=20)
                with grp((hx, hy + 0.17, 0.92), (-75, 0, 0)):
                    cyl2((0, 0.16, 0.0), (0, 0.16, 0.035), 0.16, 0.16, body_m, segs=20)
                S = 0.62
                with grp((hx, hy - 0.02, 0.92 - 1.0 * S), (0, 0, -32), scl=S):
                    U.human("tank_overall", hands=[(-0.3, -0.16, 1.0), (0.24, -0.1, 1.0)],
                            poles=[(-1, 0.2, -0.5), (1, 0.2, -0.5)], hair="hair_fair", cap=False,
                            hat=U.hat_tank, braid="+-", braid_end=1.06, bust_only=True)
            if mode == "fire":
                if k == 1:
                    flash(mz, (0, -1, 0.02), 1.5)
                    smoke(mz + V3((0, -0.15, 0.02)), 0.12, 85, n=3, seed=2)
                    smoke(V3((0, -0.9, 0.08)), 0.16, 65, dark=True, n=3, seed=3)
                elif k in (2, 3, 4):
                    smoke(mz + V3((0, -0.2 - 0.08 * k, 0.07 * k)), 0.14 + 0.05 * k, [85, 65, 45][k - 2], n=4, seed=k)
            if mode == "walk":
                e = (p * 2) % 1.0
                smoke(V3((-0.25, 1.08 + 0.25 * e, 0.56 + 0.3 * e)), 0.06 + 0.1 * e, [65, 45, 28][int(e * 2.99)],
                      dark=True, n=2, seed=int(p * 8))


def truck_wheels(ys, r, p, x=0.43, hub="truck", w=0.16):
    a = math.degrees(p * 2 * pi / 4)                    # колесо со ступицей: симметрия 4, цикл — четверть оборота
    for y in ys:
        for s in (-1, 1):
            with grp((s * x, y, r), (a, 0, 0)):
                U.tyre((0, 0, 0), r, w, hub=hub)
                for i in range(4):
                    b = 2 * pi * i / 4
                    U.wheel_disc((s * w * 0.42, r * 0.38 * cos(b), r * 0.38 * sin(b)), r * 0.07, 0.04, "black", segs=8)


def missile_truck(V, mode, k):
    """Ракетный комплекс на восьмиколёсном тягаче: в походе ракета лежит, к бою поднята, пуск."""
    p = k / float(N_WALK) if mode == "walk" else 0.0
    truck_wheels((-0.86, -0.42, 0.3, 0.74), 0.21, p, x=0.44, hub="missile_green")
    box((0.62, 2.3, 0.16), (0, -0.02, 0.38), "olive_dark", bevel=0.01)
    for s in (-1, 1):
        hexa([(s * 0.12, -1.24, 0.44), (s * 0.5, -1.24, 0.44), (s * 0.5, -0.66, 0.44), (s * 0.12, -0.66, 0.44),
              (s * 0.14, -1.12, 0.9), (s * 0.48, -1.12, 0.9), (s * 0.48, -0.68, 0.9), (s * 0.14, -0.68, 0.9)],
             "missile_green", bevel=0.03)
        box((0.32, 0.02, 0.2), (s * 0.31, -1.2, 0.74), "glass", rot=(-14, 0, 0))
        box((0.02, 0.26, 0.16), (s * 0.505, -0.9, 0.76), "glass")
        cyl2((s * 0.36, -1.25, 0.52), (s * 0.36, -1.27, 0.52), 0.045, 0.045, "lamp", segs=10)
    box((0.22, 0.6, 0.36), (0, -0.95, 0.62), "missile_green", bevel=0.02)
    box((0.9, 1.5, 0.08), (0, 0.38, 0.5), "missile_green", bevel=0.01)
    for s in (-1, 1):
        box((0.14, 1.4, 0.22), (s * 0.4, 0.4, 0.62), "missile_green", bevel=0.015)
    # ракета: лежит (марш), поднята наклонно (стоит), вертикально и улетает (бой)
    piv = V3((0, 0.95, 0.66))
    if mode == "walk":
        ang, rise = 0.0, 0.0
    elif mode == "stand":
        ang, rise = 55.0, 0.0
    else:
        ang = 90.0
        rise = [0.0, 0.05, 0.6, 2.6, None, None][k]
    with grp(piv, (-ang, 0, 0)):
        U.beam((0, 0.0, 0.0), (0, -1.6, 0.0), 0.12, "olive_dark", t2=0.06)
        if rise is not None:
            with grp((0, -rise, 0.16)):
                cyl2((0, 0.05, 0), (0, -1.15, 0), 0.13, 0.13, "missile_white", segs=20)
                cyl2((0, -1.15, 0), (0, -1.55, 0), 0.13, 0.0, "missile_green", segs=20)
                cyl2((0, 0.1, 0), (0, 0.05, 0), 0.11, 0.13, "olive_dark", segs=16)
                for i in range(4):
                    a = pi / 4 + i * pi / 2
                    slab([(0.0, 0.0), (0.0, 0.2), (0.12, 0.05)], 0.012, "missile_green", "yz",
                         loc=(0.13 * cos(a), 0.05, 0.13 * sin(a)), rot=(0, math.degrees(a), 0))
                if mode == "fire" and k in (1, 2, 3):
                    ell((0, 0.32, 0), (0.12, 0.3, 0.12), "fx_flame")
                    ell((0, 0.22, 0), (0.07, 0.16, 0.07), "fx_core")
                    if k >= 2:
                        cyl2((0, 0.4, 0), (0, 1.6, 0), 0.1, 0.25, "smoke65", segs=12)
    if mode == "walk":
        e = (p * 2) % 1.0
        smoke(V3((0.3, -0.65 + 0.2 * e, 1.0 + 0.3 * e)), 0.05 + 0.08 * e, [65, 45, 28][int(e * 2.99)], dark=True, n=2,
              seed=int(p * 8))
    if mode == "fire" and k >= 1:
        a = [85, 85, 85, 65, 45][k - 1]
        r = [0.22, 0.32, 0.42, 0.5, 0.56][k - 1]
        smoke(piv + V3((0.3, 0.25, 0.15)), r, a, n=4, seed=k)
        smoke(piv + V3((-0.25, 0.15, 0.12)), r * 0.7, a, n=3, seed=k + 5)


def airdef(V, mode, k):
    """Зенитный ракетный комплекс на колёсном шасси: башня с антенной, вращающийся радар, вертикальный пуск."""
    p = k / float(N_WALK) if mode == "walk" else 0.0
    truck_wheels((-0.72, -0.2, 0.32, 0.78), 0.2, p, x=0.44, hub="camo")
    box((0.6, 2.1, 0.16), (0, 0.0, 0.36), "olive_dark", bevel=0.01)
    hexa([(-0.5, -1.12, 0.42), (0.5, -1.12, 0.42), (0.5, -0.5, 0.42), (-0.5, -0.5, 0.42),
          (-0.48, -1.0, 0.86), (0.48, -1.0, 0.86), (0.48, -0.52, 0.86), (-0.48, -0.52, 0.86)], "camo", bevel=0.03)
    box((0.86, 0.02, 0.2), (0, -1.07, 0.72), "glass", rot=(-16, 0, 0))
    box((0.02, 0.3, 0.16), (-0.495, -0.82, 0.72), "glass")
    box((0.96, 1.5, 0.3), (0, 0.32, 0.56), "camo", bevel=0.02)
    rot = (p * 180.0) if mode == "walk" else (30.0 if mode == "stand" else 30.0 + 25 * k)
    with grp((0, 0.32, 0.71)):
        hexa([(-0.4, -0.45, 0.0), (0.4, -0.45, 0.0), (0.4, 0.45, 0.0), (-0.4, 0.45, 0.0),
              (-0.36, -0.36, 0.62), (0.36, -0.36, 0.62), (0.36, 0.42, 0.62), (-0.36, 0.42, 0.62)], "camo", bevel=0.02)
        with grp((0, -0.42, 0.35), (-14, 0, 0)):
            box((0.52, 0.06, 0.34), (0, 0, 0), "olive_dark", bevel=0.01)
            box((0.44, 0.02, 0.26), (0, -0.035, 0), "steel_dark")
        for i in range(2):
            for j in range(4):
                cyl2((-0.18 + j * 0.12, 0.05 + i * 0.18, 0.62), (-0.18 + j * 0.12, 0.05 + i * 0.18, 0.64), 0.045,
                     0.045, "olive_dark", segs=10)
        cyl2((0, 0.0, 0.62), (0, 0.0, 0.76), 0.05, 0.05, "steel_dark", segs=10)
        with grp((0, 0.0, 0.8), (0, 0, rot)):
            box((0.86, 0.08, 0.16), (0, 0, 0), "olive_dark", bevel=0.01)
            box((0.82, 0.02, 0.12), (0, -0.045, 0), "steel")
        if mode == "fire":
            hs = [None, 0.45, 1.1, 2.0, None, None][k]
            if hs is not None:
                tilt = [0, 0, 18, 40, 0, 0][k]
                c = V3((-0.06, 0.05 - 0.6 * (tilt / 40.0) * (hs - 0.4), 0.64 + hs))
                with grp(c, (tilt, 0, 0)):
                    cyl2((0, 0, -0.38), (0, 0, 0.3), 0.06, 0.06, "missile_white", segs=14)
                    cyl2((0, 0, 0.3), (0, 0, 0.45), 0.06, 0.0, "missile_white", segs=14)
                    if k >= 2:
                        ell((0, 0, -0.5), (0.06, 0.06, 0.16), "fx_flame")
                        ell((0, 0, -0.45), (0.035, 0.035, 0.08), "fx_core")
                if k == 1:
                    smoke(V3((-0.06, 0.05, 0.75)), 0.1, 65, n=3, seed=k)
                else:
                    smoke(c + V3((0, 0.25, -0.6)), 0.12, 65, n=3, seed=k)
            if k in (3, 4, 5):
                a = [65, 45, 28][k - 3]
                for j in range(4):
                    smoke(V3((-0.06, 0.05 - 0.15 * j, 0.95 + 0.35 * j)), 0.1 + 0.03 * j + 0.03 * (k - 3), a, n=2,
                          seed=j + 10 * k)
    if mode == "walk":
        e = (p * 2) % 1.0
        smoke(V3((0.35, -0.45 + 0.2 * e, 0.95 + 0.3 * e)), 0.05 + 0.08 * e, [65, 45, 28][int(e * 2.99)], dark=True, n=2,
              seed=int(p * 8))


def plane(V, mode, k):
    """Истребитель в полёте: винт размыт диском, на крыльях и борту — красные звёзды."""
    n = N_FIRE if mode == "fire" else N_WALK
    p = k / float(n)
    bob = 0.04 * sin(2 * pi * p)
    roll = 4.0 * sin(2 * pi * p + 0.6)
    with grp((0, 0, 1.25 + bob), (0, roll, 0), scl=1.0):
        st = [(-0.98, 0.07, 0.07), (-0.9, 0.125, 0.125), (-0.6, 0.15, 0.155), (-0.25, 0.14, 0.15),
              (0.15, 0.1, 0.12), (0.55, 0.06, 0.08), (0.92, 0.025, 0.045)]
        rings = [[(rx * cos(2 * pi * i / 20), y, rz * sin(2 * pi * i / 20)) for i in range(20)] for (y, rx, rz) in st]
        loft(rings, "plane", cap0=True, cap1=True)
        cyl2((0, -0.98, 0), (0, -0.84, 0), 0.075, 0.13, "prop", segs=16)
        cyl2((0, -0.98, 0), (0, -1.12, 0), 0.075, 0.0, "red_bright", segs=16)
        cyl2((0, -1.04, 0), (0, -1.05, 0), 0.44, 0.44, "prop_blur", segs=32)
        for j in range(3):
            a = radians(30 + 120 * j + 47 * k)
            with grp((0, -1.03, 0), (0, math.degrees(a), 0)):
                slab([(-0.03, 0.0), (0.03, 0.0), (0.035, 0.32), (0.0, 0.34), (-0.035, 0.32)], 0.01, "prop_blur", "xz")
        ell((0, -0.12, 0.13), (0.085, 0.22, 0.1), "canopy_t", segs=16, rings=10)
        with grp(V3((0, -0.1, 0.13)) - U.HC * 0.42, scl=0.42):     # лётчица под фонарём
            U.head("skin", "hair_fair", True, False)
            with grp((0, 0, U.HAT_DZ)):
                U.hat_flight()
        wingo = [(-1.0, -0.36), (-1.04, -0.27), (-1.0, -0.18), (0.0, -0.02), (1.0, -0.18), (1.04, -0.27),
                 (1.0, -0.36), (0.0, -0.56)]
        slab(wingo, 0.045, "plane", "xy", bend=lambda u, v: -0.05 + 0.09 * abs(u))
        slab([(-0.38, 0.76), (-0.38, 0.88), (0.0, 0.93), (0.38, 0.88), (0.38, 0.76), (0.0, 0.68)], 0.03,
             "plane", "xy", loc=(0, 0, 0.02))
        slab([(0.66, 0.02), (0.95, 0.02), (0.97, 0.32), (0.86, 0.33)], 0.03, "plane", "yz")
        for x in (-0.72, 0.72):
            star((x, -0.26, 0.04 + 0.09 * abs(x) - 0.05 + 0.024), 0.1, "red_star", rot=(90, 0, 0), t=0.006)
        star((-0.12, 0.55, 0.0), 0.07, "red_star", rot=(0, 0, 90), t=0.01)
        star((0.0, 0.88, 0.2), 0.07, "red_star", rot=(0, 0, 90), t=0.04)
        if mode == "fire":
            for (x, z) in ((-0.42, -0.02), (0.42, -0.02), (-0.06, 0.12)):
                mz = V3((x, -0.5 if x else -0.9, z))
                if (k + (1 if x > 0 else 0)) % 2 == 0:
                    flash(mz, (0, -1, 0), 0.5, k)
                for j in range(2):
                    off = 0.3 + ((k * 0.37 + j * 0.5 + (0.2 if x > 0 else 0)) % 1.0) * 0.9
                    tracer(mz + V3((0, -off, -0.02 * off)), (0, -1, -0.02), 0.22)


# ----------------------------------------------------------------------------------------------
#  Виды войск
# ----------------------------------------------------------------------------------------------
def _skirt_split(m, zb, rx=0.235, ry=0.19, gap=0.55):
    """Подол с разрезом спереди (ноги шагают свободно): от угла gap до 2pi - gap через спину."""
    return (m, zb, rx, ry, 1.02, 0.0, gap, 2 * pi - gap)


def _tails(m, zb=0.6, rx=0.22, ry=0.19):
    return (m, zb, rx, ry, 1.02, 0.0, pi / 2 + 0.3, 3 * pi / 2 - 0.3)


INF = {
    "druzhina": dict(coat="mail", pants="trousers_blue", boots="boots", belt="leather", hair="hair_fair", cap=False,
                     locks=False, hat=hat_shishak_open, braid="-", weapon="spear", shield="shield_red",
                     skirts=[_skirt_split("mail", 0.62)], L=2.25),
    "crossbow": dict(coat="gambeson", pants="trousers_dark", boots="boots", belt="leather", hair="hair_fair",
                     hat=U.hat_kettle, braid="back", weapon="crossbow", skirts=[_skirt_split("gambeson", 0.6)]),
    "gunners": dict(coat="teal_coat", pants="trousers_dark", boots="boots_yellow", boot_top=0.36, belt="gold",
                    cuff="gold_cloth", hair="hair_red", hat=lambda: U.hat_fur_cap("crimson", "feather"), braid="back",
                    weapon="musket", L=1.4, match=True, skirts=[_tails("teal_coat", 0.56, 0.25)],
                    extras=lambda: buttons("gold")),
    "musketeers": dict(coat="crimson_coat", pants="trousers_dark", boots="boots", belt="leather", cuff="gold_cloth",
                       hair="hair_red", hat=U.hat_wide, braid="back", weapon="musket", L=1.42,
                       skirts=[_tails("crimson_coat", 0.55, 0.25)], extras=bandolier),
    "line": dict(coat="green_coat", pants="white_cloth", boots="boots_black", boot_top=0.36, belt="white_cloth",
                 cuff="red", hair="hair", hat=U.hat_shako, braid="back", weapon="musket", L=1.36, bayonet=0.3,
                 skirts=[_tails("green_coat")], extras=lambda: (crossbelts(), buttons())),
    "rifle": dict(coat="khaki", pants="khaki_dark", boots="boots_black", boot_top=0.44, belt="leather_dark",
                  hair="hair_fair", hat=U.hat_pilotka, braid="+-", braid_end=1.08, weapon="rifle", L=1.3,
                  extras=pouches),
    "partisans": dict(coat="telogreika", pants="stockings_dark", boots="felt", boot_top=0.44, belt="leather_dark",
                      hair="hair_fair", cap=False, locks=False, hat=lambda: U.hat_kerchief(hair="hair_fair"),
                      braid="-", braid_end=1.06, weapon="rifle", L=1.3,
                      skirts=[("skirt_dark", 0.5, 0.27, 0.25, 1.02), ("telogreika", 0.74, 0.25, 0.2, 1.04)],
                      extras=lambda: buttons("black", zs=(1.12, 1.22, 1.32))),
    "motor": dict(coat="khaki_modern", pants="khaki_modern", boots="boots_black", boot_top=0.42, belt="leather_dark",
                  hair="hair", hat=hat_ssh, braid="back", weapon="smg", L=0.82, extras=pouches),
    # враги — мужчины в форме своей эпохи
    "enemy_early": dict(female=False, coat="mail_dark", pants="trousers_dark", boots="boots", belt="leather_dark",
                        hair="hair_dark", hat=hat_nasal, beard="full", weapon="spear",
                        shield="shield_blue", skirts=[_skirt_split("mail_dark", 0.6)], L=2.2),
    "enemy_teutonic": dict(female=False, coat="mail", pants="mail", boots="steel_dark", boot_top=0.45,
                           belt="leather_dark", hair="hair", cap=False, hat=hat_great_helm, weapon="sword",
                           shield="heater", skirts=[_skirt_split("teut_white", 0.5, 0.26, 0.21, 0.5)],
                           extras=lambda: (surcoat("teut_white"), chest_cross())),
    "enemy_strelets": dict(female=False, coat="strelets", pants="trousers_dark", boots="boots_yellow", boot_top=0.4,
                           belt="gold", hair="hair", hat=hat_strelets, beard="full", weapon="musket", L=1.4,
                           match=True, skirts=[_skirt_split("strelets", 0.32, 0.25, 0.21, 0.5)],
                           back=berdysh_back, extras=lambda: buttons("gold")),
    "enemy_18c": dict(female=False, coat="swede_blue", pants="buff", boots="boots_black", boot_top=0.45, belt="buff",
                      cuff="yellow", hair="hair_fair", hat=hat_tricorne, weapon="musket", L=1.4, bayonet=0.3,
                      skirts=[_tails("swede_blue", 0.56, 0.25)], extras=lambda: buttons("brass")),
    "enemy_napoleon": dict(female=False, coat="french_blue", pants="white_cloth", boots="boots_black", boot_top=0.3,
                           belt="white_cloth", cuff="red", hair="hair_dark", hat=U.hat_shako, beard="mustache",
                           weapon="musket", L=1.38, bayonet=0.3, skirts=[_tails("french_blue")],
                           extras=lambda: (crossbelts(), epaulettes(), buttons())),
    "enemy_ww1": dict(female=False, coat="fg_ww1", pants="fg_ww1", boots="boots", boot_top=0.42, belt="leather",
                      hair="hair", hat=hat_pickelhaube, beard="mustache", weapon="rifle", L=1.32, bayonet=0.28,
                      extras=lambda: (pouches("leather"), bedroll())),
    "enemy_wehrmacht": dict(female=False, coat="fg_tunic", pants="fg_trousers", boots="boots_black", boot_top=0.45,
                            belt="black", hair="hair", hat=hat_stahlhelm, weapon="rifle", L=1.25,
                            extras=lambda: pouches("black")),
}

CAV = {
    "khorugv": dict(coat="steel", sleeve="steel", pants="steel", boots="steel_dark", hand_m=("steel_dark", "steel_dark"),
                    belt="leather_dark", hair="hair_blond", cap=False, hat=U.hat_bascinet, braid="+",
                    horse="horse_bay", mane="horse_mane_dark", saddle="crimson", pennant=("crimson", "white_cloth"),
                    extras=lambda: (surcoat("crimson"), chest_cross("white_cloth"))),
    "hussars": dict(coat="crimson", pants="crimson", boots="boots_yellow", belt="gold", hair="hair_blond",
                    hat=U.hat_szyszak, braid="+", horse="horse", mane="horse_mane", saddle="leopard", wings=True,
                    pennant=("crimson", "gold_cloth")),
}

# id -> (функция, разворот, масштаб, описание[, сдвиг (x, y) в мире])
VARIANTS = {}
for _id, _v in INF.items():
    VARIANTS[_id] = (infantry, 56, 1.0, _v)
for _id, _v in CAV.items():
    VARIANTS[_id] = (cavalry, 70, 0.8, _v)
VARIANTS.update({
    "artillery": (artillery, 64, 0.82, None),
    "rockets": (rockets, 60, 0.86, None),
    "tank": (tank, 68, 0.74, {"style": "t34"}, (0.0, 0.55)),          # техника чуть дальше от камеры:
    "enemy_panzer": (tank, 68, 0.72, {"style": "panzer"}, (0.0, 0.55)),  # ближний борт не выходит из клетки
    "missile": (missile_truck, 66, 0.66, None, (0.0, 0.35)),
    "airdef": (airdef, 64, 0.72, None, (0.0, 0.35)),
    "air": (plane, 74, 0.9, None),
})
assert set(VARIANTS) == set(IDS), set(IDS) ^ set(VARIANTS)


# ----------------------------------------------------------------------------------------------
#  Сцена, рендер
# ----------------------------------------------------------------------------------------------
def setup(args):
    sc = U.setup_scene({"samples": args["samples"], "zoom": 0.0, "zh": 2.0})
    sc.render.resolution_x, sc.render.resolution_y = CELL_W * SCALE, CELL_H * SCALE
    sc.eevee.shadow_cascade_size = "2048"
    sc.eevee.gtao_distance = 0.2
    ob = bpy.data.objects
    az, el = radians(160.0), radians(46.0)                     # ключ — спереди-справа: лица освещены
    s = V3((sin(az) * cos(el), cos(az) * cos(el), sin(el)))
    ob["Key"].rotation_euler = (-s).to_track_quat("-Z", "Y").to_euler()
    ob["Key"].data.energy = 3.3
    tgt = V3((0, 0, 0.9))
    for name, loc, en in (("Fill", (-5.0, -3.5, 1.8), 700.0), ("Rim", (-3.0, 5.0, 3.5), 1500.0),
                          ("Rim2", (4.5, 4.0, 2.5), 900.0)):
        o = ob[name]
        o.location = tgt + V3(loc)
        o.rotation_euler = (tgt - o.location).to_track_quat("-Z", "Y").to_euler()
        o.data.energy = en
    cam = sc.camera
    cam.data.ortho_scale = CELL_W / PPU
    cam.rotation_euler = (radians(90.0 - ELEV), 0, 0)
    fwd, up = V3((0, _ce, -_se)), V3((0, _se, _ce))
    cam.location = V3(((CELL_W / 2.0 - ANCHOR[0]) / PPU, 0, 0)) + up * ((ANCHOR[1] - CELL_H / 2.0) / PPU) - fwd * 60
    _fx_mats()
    return sc


def clear(coll):
    for o in list(coll.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for blk in (bpy.data.meshes, bpy.data.curves):
        for d in list(blk):
            if d.users == 0:
                blk.remove(d)


def frame_mode(k):
    if k == 0:
        return "stand", 0
    if k <= N_WALK:
        return "walk", k - 1
    return "fire", k - 1 - N_WALK


def bounds(tag, coll):
    deps = bpy.context.evaluated_depsgraph_get()
    xs, ys = [], []
    for o in coll.all_objects:
        if o.type not in ("MESH", "CURVE"):
            continue
        oe = o.evaluated_get(deps)
        for c in oe.bound_box:
            w = oe.matrix_world @ V3(c)
            xs.append(ANCHOR[0] + w.x * PPU)
            ys.append(ANCHOR[1] - (w.y * _se + w.z * _ce) * PPU)
    if not xs:
        return
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    flag = "  <-- CLIP" if (y0 < 0 or x0 < 0 or x1 > CELL_W or y1 > CELL_H) else ""
    print("%-22s x %6.1f..%6.1f  y %6.1f..%6.1f%s" % (tag, x0, x1, y0, y1, flag), flush=True)


def main():
    args = parse_args(sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else [])
    os.makedirs(args["work"], exist_ok=True)
    sc = setup(args)
    coll = bpy.data.collections.new("Troop")
    sc.collection.children.link(coll)
    todo = args["only"] or IDS
    frames = args["frames"] if args["frames"] is not None else list(range(COLS))
    t_all = time.time()
    for vid in todo:
        fn, yaw, scl, spec = VARIANTS[vid][:4]
        off = VARIANTS[vid][4] if len(VARIANTS[vid]) > 4 else (0.0, 0.0)
        for k in frames:
            t0 = time.time()
            mode, i = frame_mode(k)
            clear(coll)
            root = bpy.data.objects.new("root", None)
            coll.objects.link(root)
            U.COLL[0] = coll
            U.PARENT[:] = [root]
            random.seed(1000 + IDS.index(vid) * 31 + k)
            with grp((off[0], off[1], 0), (0, 0, yaw), scl):
                fn(spec, mode, i)
            bpy.context.view_layer.update()
            bounds("%s_%02d %s%d" % (vid, k, mode[0], i), coll)
            sc.render.filepath = os.path.join(args["work"], "%s_%02d.png" % (vid, k))
            bpy.ops.render.render(write_still=True)
            print("FRAME %s %d %.1fs" % (vid, k, time.time() - t0), flush=True)
    print("render total %.1fs" % (time.time() - t_all), flush=True)
    if not args["no_post"]:
        subprocess.run(["python3", os.path.abspath(__file__), "--post", "--work", args["work"]], check=False)


main()
