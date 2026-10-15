#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""3D-портреты (бюсты-фигурки) исторических деятелей и правителей для игры.

Параметрический генератор персонажей в Blender: одна стилизованная база (голова со
сглаженными чертами — нос, брови, глаза, уши, губы; шея, плечи, грудь) и сменные модули:
причёски, бороды, усы, брови, очки, головные уборы, одежда, награды и предметы в руках.
Таблица PEOPLE_SPEC ниже задаёт каждому персонажу узнаваемый облик (цвет кожи и волос,
фирменные черты: бородка и лысина Ленина, усы и френч Сталина, родимое пятно Горбачёва,
апостольник и шестиконечный крест Евфросинии Полоцкой и т. д.).

Запуск (из корня belarus-game; EEVEE нужен виртуальный дисплей):
    xvfb-run -a -s "-screen 0 1280x1024x24" blender -b -P tools/blender/build_portraits.py -- \
        [--work /tmp/belarus_portraits] [--only lenin,stalin] [--samples 48] [--no-save]
Blender строит каждого персонажа, рендерит бюст в ракурсе 3/4 (мягкий студийный свет:
ключевой спереди-слева, заполняющий, тёплый контровой) в 320x400 с прозрачным фоном,
сохраняет blender/belarus_portraits.blend (сцена + этот скрипт внутри). Затем обычный python3
(нужен Pillow) уменьшает кадры до 160x200 и собирает лист 10 колонок:
    python3 tools/blender/build_portraits.py --post --work /tmp/belarus_portraits
Результат: assets/portraits.png и assets/portraits.json
    {"cell_w":160,"cell_h":200,"cols":10,"ids":[...]}  — порядок: 36 деятелей из PEOPLE
    (src/data_proj.c), затем 35 правителей из src/rulers.c (кроме vitaut — он уже среди деятелей).
"""
import os, sys, json, math, random, bisect

HERE = os.path.dirname(os.path.abspath(__file__))
GAME = os.path.dirname(os.path.dirname(HERE))
CELL_W, CELL_H, COLS = 160, 200, 10

PEOPLE_IDS = ["euphrosyne", "ioann", "bogsha", "kirill", "aleksa", "vitaut", "skaryna", "budny", "sapega",
              "mamonichi", "bernardoni", "sierotka", "siemienowicz", "simeon", "madzharski", "mk_oginski",
              "tizenhaus_p", "gilibert", "oginski", "domeyko", "zhilinsky", "chersky", "narkevich", "kupala",
              "sukhoi", "kosberg", "stepanov", "lopato", "borisevich", "alferov", "klimuk", "kovalyonok",
              "dobkin", "kisly", "novitsky", "vasilevskaya"]
RULER_IDS = ["vseslav", "boris_p", "rogvolod", "mindaugas", "gediminas", "algirdas", "jogaila", "casimir4",
             "sigismund1", "sigismund2", "batory", "sigismund3", "jan_kazimierz", "sobieski", "august2",
             "poniatowski", "catherine2", "paul1", "alexander1", "nicholas1", "alexander2", "alexander3",
             "nicholas2", "lenin", "stalin", "khrushchev", "brezhnev", "andropov", "chernenko", "gorbachev",
             "ponomarenko", "mazurov", "masherov", "shushkevich", "lukashenko"]
IDS = PEOPLE_IDS + RULER_IDS


def parse_args(argv):
    a = {"work": "/tmp/belarus_portraits", "only": None, "samples": 48, "save": True, "post": False,
         "scale": 2}
    i = 0
    while i < len(argv):
        k = argv[i]
        if k == "--work": a["work"] = argv[i + 1]; i += 1
        elif k == "--only": a["only"] = argv[i + 1].split(","); i += 1
        elif k == "--samples": a["samples"] = int(argv[i + 1]); i += 1
        elif k == "--no-save": a["save"] = False
        elif k == "--post": a["post"] = True
        i += 1
    return a


# ----------------------------------------------------------------------------------------------
#  Сборка листа (обычный python3 + Pillow)
# ----------------------------------------------------------------------------------------------
def post(work):
    from PIL import Image
    rows = (len(IDS) + COLS - 1) // COLS
    sheet = Image.new("RGBA", (COLS * CELL_W, rows * CELL_H), (0, 0, 0, 0))
    for n, pid in enumerate(IDS):
        p = os.path.join(work, pid + ".png")
        if not os.path.exists(p):
            print("missing", pid); continue
        im = Image.open(p).convert("RGBA").convert("RGBa")
        im = im.resize((CELL_W, CELL_H), Image.LANCZOS).convert("RGBA")
        sheet.paste(im, ((n % COLS) * CELL_W, (n // COLS) * CELL_H))
    os.makedirs(os.path.join(GAME, "assets"), exist_ok=True)
    sheet.save(os.path.join(GAME, "assets", "portraits.png"), optimize=True)
    with open(os.path.join(GAME, "assets", "portraits.json"), "w", encoding="utf-8") as f:
        json.dump({"cell_w": CELL_W, "cell_h": CELL_H, "cols": COLS, "ids": IDS}, f, ensure_ascii=False)
    print("sheet", sheet.size, len(IDS))


try:
    import bpy, bmesh
    from mathutils import Vector, Matrix, Quaternion
    from mathutils.bvhtree import BVHTree
    IN_BLENDER = True
except ImportError:
    IN_BLENDER = False

# ----------------------------------------------------------------------------------------------
#  Общие утилиты
# ----------------------------------------------------------------------------------------------
def srgb(h):
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return tuple((v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4) for v in c)


def mulc(c, k):
    return tuple(min(1.0, v * k) for v in c)


def mixc(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def sstep(e0, e1, x):
    t = (x - e0) / (e1 - e0)
    if t <= 0.0: return 0.0
    if t >= 1.0: return 1.0
    return t * t * (3.0 - 2.0 * t)


def gauss(d, s):
    q = d / s
    return math.exp(-q * q) if q * q < 40 else 0.0


def clamp(x, a, b):
    return a if x < a else b if x > b else x


def interp(x, xs, ys):
    if x <= xs[0]: return ys[0]
    if x >= xs[-1]: return ys[-1]
    i = bisect.bisect_right(xs, x) - 1
    t = (x - xs[i]) / (xs[i + 1] - xs[i])
    return ys[i] + (ys[i + 1] - ys[i]) * t


def pchip(xs, ys):
    n = len(xs)
    h = [xs[i + 1] - xs[i] for i in range(n - 1)]
    d = [(ys[i + 1] - ys[i]) / h[i] for i in range(n - 1)]
    m = [0.0] * n
    m[0], m[-1] = d[0], d[-1]
    for i in range(1, n - 1):
        if d[i - 1] * d[i] <= 0: m[i] = 0.0
        else:
            w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])

    def f(x):
        if x <= xs[0]: return ys[0]
        if x >= xs[-1]: return ys[-1]
        i = bisect.bisect_right(xs, x) - 1
        t = (x - xs[i]) / h[i]; t2 = t * t; t3 = t2 * t
        return ((2 * t3 - 3 * t2 + 1) * ys[i] + (t3 - 2 * t2 + t) * h[i] * m[i] +
                (-2 * t3 + 3 * t2) * ys[i + 1] + (t3 - t2) * h[i] * m[i + 1])
    return f


# ----------------------------------------------------------------------------------------------
#  Таблица персонажей
# ----------------------------------------------------------------------------------------------
SKIN = {"fair": "#f1cfb6", "light": "#e9bfa0", "warm": "#e2ad8c", "ruddy": "#e4a98f", "olive": "#d6a17f",
        "old": "#dfb196", "pale": "#efd2bf"}
D = dict(sex="m", age=40, skin="light", hair="#3b2a1e", hs="short", brow=None, brows="normal",
         beard=None, bcol=None, blen=0.0, mous=None, eyes="#4a3423", glasses=None, gcol="#1e1a18",
         hat=None, outfit="suit", c1="#30343c", c2="#f2f0ea", c3="#5a2228", c4="#8a1a1a", fur="#5a4030",
         acc=(), prop=None, face={}, extra={})

SPEC = {
    # ---------------- деятели ----------------
    "euphrosyne": dict(sex="f", age=25, skin="fair", hs="none", brow="#5a3d28", brows="thin", eyes="#4a5a6a",
                       hat="nun", outfit="habit", c1="#2a221e", acc=("cross6",), prop="book_red",
                       face=dict(nose=0.85, lips=1.25, chin=0.8, jaw=0.33)),
    "ioann": dict(age=45, hair="#4a3222", hs="monk", beard="full", blen=0.05, outfit="robe", c1="#6b4a30",
                  c2="#4e3422", prop="scroll"),
    "bogsha": dict(age=40, hair="#6a4528", hs="fringe", beard="full", blen=0.03, outfit="tunic12", c1="#a3302a",
                   c2="#d8b04a", acc=("apron", "cross_gold")),
    "kirill": dict(age=72, skin="old", hair="#dcd8d0", hs="monk", beard="long", blen=0.13, hat="klobuk_white",
                   outfit="vestments", c1="#ece6d8", c2="#c8a040", acc=("panagia",), prop="book_gold",
                   face=dict(age=1.0)),
    "aleksa": dict(age=40, hair="#5a3a24", hs="fringe", beard="full", blen=0.04, hat="cap_soft", c4="#6b5a40",
                   outfit="tunic12", c1="#a8452e", c2="#5a4a30", prop="hammer"),
    "vitaut": dict(age=50, hair="#5b4030", hs="bob", mous="long", hat="crown_fur", outfit="armour",
                   c4="#a01c1c", acc=("mantle",), face=dict(nose=1.1, jaw=0.27)),
    "skaryna": dict(age=33, hair="#4a3020", hs="bob", hat="beret", outfit="robe", c1="#2b2622", c2="#1e1a18",
                    fur="#6a4a2e", acc=("fur_collar",), prop="book_sun", face=dict(jaw=0.30)),
    "budny": dict(age=50, hair="#3e2c20", hs="receding", beard="full", blen=0.07, outfit="gown", c1="#1c1b1d",
                  acc=("white_collar",), prop="book_black"),
    "sapega": dict(age=55, hair="#5a4a3e", hs="noble", beard="full", blen=0.07, bcol="#6e6258", hat="kolpak",
                   fur="#3a2a1e", c4="#8a1a1a", outfit="kontusz", c1="#a51c22", c2="#d4b048",
                   extra=dict(feather=None)),
    "mamonichi": dict(age=45, hair="#4b3626", hs="short", beard="full", blen=0.05, hat="fur_cap", fur="#3d2c20",
                      c4="#2e5a3a", outfit="caftan", c1="#2e5a3a", c2="#c9a64a", prop="printed"),
    "bernardoni": dict(age=40, hair="#2a2420", hs="short", hat="biretta", outfit="cassock", c1="#18171a",
                       prop="compass", face=dict(nose=1.15)),
    "sierotka": dict(age=45, hair="#2e241c", hs="noble", beard="pointed", blen=0.06, hat="kolpak",
                     fur="#2a1e16", c4="#245a3a", outfit="kontusz", c1="#5a1e2e", c2="#c9b07a",
                     extra=dict(feather="#1a1a1a"), prop="map"),
    "siemienowicz": dict(age=40, hair="#5a4030", hs="noble", mous="long", outfit="armour",
                         acc=("sash_red",), prop="rocket_old"),
    "simeon": dict(age=45, hair="#2e2620", hs="monk", beard="full", blen=0.07, hat="klobuk", outfit="robe",
                   c1="#1b1a1c", c2="#141315", prop="book_brown"),
    "madzharski": dict(age=45, skin="olive", hair="#2a2220", hs="short", mous="big", eyes="#3a2414",
                       outfit="caftan_east", c1="#7a2a3a", c2="#d8b050", prop="slutsk",
                       face=dict(nose=1.2)),
    "mk_oginski": dict(age=50, hair="#e6e4de", hs="wig_powder", outfit="coat18", c1="#8a1f2a", c2="#eadcb0",
                       c3="#d4b048", acc=("jabot", "order_star"), prop="harp"),
    "tizenhaus_p": dict(age=45, hair="#e2e0dc", hs="wig_powder", outfit="coat18", c1="#2a4a8a", c2="#e0d2a8",
                        c3="#d0c8b0", acc=("jabot",), prop="scroll_doc", face=dict(fat=0.35)),
    "gilibert": dict(age=50, hair="#e8e6e2", hs="wig_powder", outfit="coat18", c1="#5a3a2a", c2="#cdb07c",
                     c3="#c8b090", acc=("jabot",), prop="plant", face=dict(nose=1.15)),
    "oginski": dict(age=35, hair="#3a2618", hs="romantic", outfit="frock_high", c1="#1e2230", c2="#f4f2ec",
                    prop="music", face=dict(jaw=0.31, nose=1.05)),
    "domeyko": dict(age=62, skin="old", hair="#d0ccc4", hs="receding", beard="full", blen=0.10, outfit="frock",
                    c1="#2a2a30", c2="#f0eee8", c3="#151517", prop="mineral", face=dict(age=0.8)),
    "zhilinsky": dict(age=50, hair="#4a3a2e", hs="short", beard="full", blen=0.05, bcol="#5a4a3e",
                      outfit="eng_uniform", c1="#1f2a24", prop="plan"),
    "chersky": dict(age=45, hair="#3a2a20", hs="short", beard="full", blen=0.07, hat="fur_cap", fur="#6a5038",
                    c4="#6a5038", outfit="fur_coat", c1="#4a3626", c2="#2a2420"),
    "narkevich": dict(age=50, hair="#3a2c22", hs="receding", beard="full", blen=0.08, outfit="frock",
                      c1="#232026", c2="#f0eee8", c3="#151517", prop="spark"),
    "kupala": dict(age=40, hair="#3a2a20", hs="combed_back", mous="neat", outfit="vyshyvanka", c1="#3b3c40",
                   face=dict(cheek=1.2)),
    "sukhoi": dict(age=62, skin="old", hair="#b8b4ae", hs="receding", outfit="suit", c1="#2f3540",
                   c3="#5a2a2a", acc=("hero1",), prop="plane", face=dict(age=0.7)),
    "kosberg": dict(age=50, hair="#2a2420", hs="receding", glasses="round", outfit="suit", c1="#3a3a40",
                    c3="#2a3a5a", prop="engine", face=dict(nose=1.1)),
    "stepanov": dict(age=55, hair="#8a8680", hs="short", glasses="rect", outfit="suit", c1="#353a44",
                     c3="#6a2020", prop="laser"),
    "lopato": dict(age=55, hair="#5a5048", hs="combed", outfit="suit", c1="#3a3830", c3="#2a2a2a",
                   prop="tape"),
    "borisevich": dict(age=60, skin="old", hair="#a8a49c", hs="receding", glasses="rect", outfit="suit",
                       c1="#2a2e38", c3="#3a3020", acc=("hero1",), face=dict(age=0.6)),
    "alferov": dict(age=72, skin="old", hair="#dcdad4", hs="combed", brow="#4a4440", brows="thick",
                    glasses="rect", gcol="#8a8478", outfit="suit", c1="#2a2e3a", c3="#7a1e2e",
                    prop="nobel", face=dict(age=0.9, w=1.04, fat=0.3)),
    "klimuk": dict(age=35, hair="#2e241e", hs="combed", outfit="sokol", acc=("patch_cccp",),
                   prop="helmet_space"),
    "kovalyonok": dict(age=40, hair="#3a2c22", hs="combed", outfit="flightsuit", c1="#2a4a8a",
                       acc=("hero2",)),
    "dobkin": dict(age=60, hair="#a09c98", hs="short", glasses="rect", outfit="blazer", c1="#2e3440",
                   c2="#cfdcec", prop="laptop"),
    "kisly": dict(age=45, hair="#3a2e26", hs="buzz", beard="stubble", outfit="casual", c1="#2a2a2e",
                  c2="#55575c", prop="tank"),
    "novitsky": dict(age=40, hair="#3a2e26", hs="buzz", outfit="sokol", acc=("patch_round",)),
    "vasilevskaya": dict(sex="f", age=30, skin="fair", hair="#7a5434", hs="bun", brow="#5a3d28", brows="thin",
                         eyes="#5a6a4a", outfit="flightsuit", c1="#1f4ea0", acc=("flag_by",),
                         face=dict(nose=0.85, lips=1.3)),
    # ---------------- правители ----------------
    "vseslav": dict(age=50, hair="#2e2219", hs="bob", beard="long", blen=0.11, hat="princely", c4="#8a1a1a",
                    outfit="tunic12", c1="#b08a3a", c2="#7a1a1a", fur="#8a8478",
                    acc=("mantle", "fur_collar"), extra=dict(mantle="#a01818"), face=dict(nose=1.15)),
    "boris_p": dict(age=45, hair="#5a3a24", hs="bob", beard="full", blen=0.06, hat="princely", c4="#1e4a7a",
                    outfit="tunic12", c1="#2a5a3a", c2="#c8a040", acc=("mantle", "cross_gold"),
                    extra=dict(mantle="#8a2a1a")),
    "rogvolod": dict(age=40, hair="#6a4a2a", hs="bob", beard="full", blen=0.05, hat="princely", c4="#6a1a4a",
                     outfit="tunic12", c1="#a88a40", c2="#6a1a4a", acc=("mantle",),
                     extra=dict(mantle="#1e3a7a")),
    "mindaugas": dict(age=55, hair="#4a3a2e", hs="bob", beard="long", blen=0.11, hat="crown_med",
                      outfit="armour", acc=("mantle",), c4="#9a1818"),
    "gediminas": dict(age=55, hair="#5a4634", hs="bob", beard="full", blen=0.07, hat="helmet_crown",
                      outfit="armour", acc=("mantle",), c4="#2a3a6a"),
    "algirdas": dict(age=50, hair="#3a2c22", hs="bob", beard="full", blen=0.08, hat="fur_cap", fur="#4a3626",
                     c4="#8a1a1a", outfit="armour", acc=("mantle",), extra=dict(mantle="#2a4a2a")),
    "jogaila": dict(age=40, hair="#5a3a22", hs="long", hat="crown_med", outfit="royal", c1="#c8a040",
                    c4="#8a1a2a", face=dict(nose=1.1, jaw=0.27)),
    "casimir4": dict(age=45, hair="#4a3020", hs="long", hat="crown_closed", outfit="royal", c1="#b89038",
                     c4="#4a1e5a", face=dict(nose=1.15)),
    "sigismund1": dict(age=65, skin="old", hair="#e4e0d8", hs="long", hat="crown_closed", outfit="royal",
                       c1="#c09a40", c4="#6a1a1a", face=dict(age=0.9, fat=0.3)),
    "sigismund2": dict(age=40, hair="#2a2018", hs="short", beard="pointed", blen=0.05, hat="hat_black",
                       outfit="doublet", c1="#1e1c1c", c2="#c8a040", acc=("chain_gold",),
                       face=dict(nose=1.1)),
    "batory": dict(age=50, hair="#4a3a30", hs="noble", beard="full", blen=0.07, bcol="#5a4a3e", hat="kolpak",
                   fur="#3a2a1e", c4="#8a1a1a", outfit="kontusz", c1="#b21e1e", c2="#d8c070",
                   extra=dict(feather="#f4f2ee")),
    "sigismund3": dict(age=45, hair="#6a4a2a", hs="short", beard="pointed", blen=0.06, mous="upturned",
                       outfit="ruff", c1="#18171a", acc=("fleece",)),
    "jan_kazimierz": dict(age=45, hair="#3a2a20", hs="long", mous="neat", outfit="armour",
                          acc=("sash_red",)),
    "sobieski": dict(age=55, hair="#4a3a30", hs="noble", mous="big", outfit="armour", fur="#6a5038",
                     acc=("mantle", "fur_collar"), extra=dict(mantle="#9a1a1a"), face=dict(fat=0.6, w=1.05)),
    "august2": dict(age=45, hair="#3a2a1e", hs="wig_big", outfit="armour", acc=("sash_blue_l",),
                    face=dict(fat=0.3, w=1.03)),
    "poniatowski": dict(age=50, hair="#e4e2de", hs="wig_powder", outfit="coat18", c1="#5a1e2a", c2="#e8dcb8",
                        c3="#d4b048", acc=("jabot", "sash_blue_l", "order_star"), face=dict(nose=1.1)),
    "catherine2": dict(sex="f", age=55, skin="pale", hair="#d8d4cc", hs="female_up", brow="#6a5a4a",
                       brows="thin", eyes="#4a5a6a", hat="crown_imp", outfit="gown_ermine", c1="#e8e2d4",
                       c4="#c8a040", acc=("sash_blue", "order_star"),
                       face=dict(fat=0.7, nose=0.95, lips=1.1, chin=1.1)),
    "paul1": dict(age=45, hair="#e2e0dc", hs="wig_powder", outfit="uniform_imp", c1="#1f3a2a",
                  acc=("maltese",), face=dict(nose=0.62, snub=1.0, nose_w=1.25, nose_v=0.82)),
    "alexander1": dict(age=40, hair="#b8905e", hs="balding_curls", beard="sideburns", outfit="uniform_imp",
                       c1="#1f3a2a", acc=("epaulettes", "sash_blue", "order_star"), eyes="#5a7090",
                       face=dict(fat=0.25)),
    "nicholas1": dict(age=45, hair="#6a4a2a", hs="combed", beard="sideburns", mous="neat", outfit="uniform_imp",
                      c1="#1f3a2a", acc=("epaulettes", "order_star"), face=dict(nose=1.1, h=1.04)),
    "alexander2": dict(age=55, hair="#7a6a5a", hs="thinning", beard="chops", blen=0.0, mous="big",
                       outfit="uniform_imp", c1="#1f3a2a", acc=("epaulettes", "sash_blue", "order_star")),
    "alexander3": dict(age=45, hair="#5a4a3a", hs="bald", beard="full", blen=0.08, outfit="uniform_imp",
                       c1="#1f3a2a", acc=("epaulettes", "order_star"), face=dict(w=1.07, fat=0.4)),
    "nicholas2": dict(age=45, hair="#7a5a3a", hs="short", beard="neat", blen=0.02, bcol="#8a6040", mous="neat",
                      outfit="khaki", c1="#8a8466", acc=("george",), eyes="#5a7090"),
    "lenin": dict(age=50, hair="#8a5a34", hs="bald_fringe", beard="pointed", blen=0.035, bcol="#8a4a28",
                  mous="neat", brow="#7a4a28", outfit="suit", c1="#2b2a2a", c3="#3a3030",
                  acc=("red_bow",), face=dict(cheek=1.5, top=1.06, eye_sep=1.02)),
    "stalin": dict(age=60, hair="#3a3632", hs="swept", mous="big", brow="#2e2a26", brows="thick",
                   outfit="french", c1="#a8a48c", acc=("hero1",), face=dict(w=1.02, nose=1.1)),
    "khrushchev": dict(age=65, skin="ruddy", hair="#b0aaa0", hs="bald", outfit="suit", c1="#3a3a3e",
                       c3="#2a2a3a", acc=("hero_n4",), face=dict(w=1.1, fat=1.0, jaw=0.18, nose=1.05,
                                                                 nose_w=1.3, age=0.7)),
    "brezhnev": dict(age=65, hair="#2e2a26", hs="combed_back", brow="#1e1a16", brows="huge", outfit="suit",
                     c1="#2a2e38", c3="#3a2a2a", acc=("hero_n5",), face=dict(w=1.06, jaw=0.2, fat=0.5,
                                                                            age=0.6)),
    "andropov": dict(age=65, skin="old", hair="#8a8680", hs="thinning", glasses="rect", gcol="#5a544e",
                     outfit="suit", c1="#26282e", c3="#30303a", face=dict(h=1.05, age=0.6)),
    "chernenko": dict(age=70, skin="old", hair="#b8b4ae", hs="combed_back", outfit="suit", c1="#30333a",
                      c3="#3a2a2a", acc=("hero_n3",), face=dict(w=1.05, fat=0.5, age=0.8)),
    "gorbachev": dict(age=55, hair="#5a5048", hs="bald", outfit="suit", c1="#30343c", c3="#a01c22",
                      extra=dict(birthmark=True), face=dict(fat=0.35, w=1.03)),
    "ponomarenko": dict(age=50, hair="#2e2620", hs="combed_back", outfit="general", c1="#6a6a48"),
    "mazurov": dict(age=50, hair="#4a3e34", hs="combed", outfit="suit", c1="#33383e", c3="#3a3a4a",
                    acc=("hero1",)),
    "masherov": dict(age=50, hair="#2a2420", hs="wavy", outfit="suit", c1="#2c3038", c3="#2a3550",
                     acc=("hero1",), face=dict(jaw=0.27)),
    "shushkevich": dict(age=60, skin="old", hair="#c0bcb6", hs="combed", glasses="rect", gcol="#6a645c",
                        outfit="suit", c1="#3a3e46", c3="#5a1e1e", face=dict(age=0.6)),
    "lukashenko": dict(age=55, hair="#5a4c42", hs="thinning", mous="neat", bcol="#4a3c32", outfit="suit",
                       c1="#24272e", c3="#4a1e24", face=dict(w=1.02, jaw=0.24)),
}


def spec_for(pid):
    s = dict(D)
    s.update(SPEC[pid])
    s["id"] = pid
    if s["brow"] is None: s["brow"] = s["hair"] if s["hs"] not in ("none",) else "#3a2a20"
    if s["bcol"] is None: s["bcol"] = s["hair"]
    f = dict(w=1.0, d=1.0, top=1.0, h=1.0, jaw=0.30, chin=1.0, chin_w=1.0, nose=1.0, nose_w=1.0, nose_v=1.0,
             snub=0.0, lips=1.0, cheek=1.0, fat=0.0, brow=1.0, eye_sep=1.0, age=0.0, mz=-0.071)
    if s["sex"] == "f":
        f.update(w=0.95, jaw=0.33, chin=0.75, chin_w=0.85, nose=0.85, nose_w=0.85, brow=0.4, lips=1.25)
    f.update(s["face"])
    s["F"] = f
    return s


# ==============================================================================================
#  Blender
# ==============================================================================================
COLL = None
ROOT = None
HEAD_M = None
MAT_N = [0]
QS_CACHE = {}


def link(ob, head=False):
    COLL.objects.link(ob)
    ob.parent = ROOT
    if head:
        ob.matrix_basis = HEAD_M
    return ob


def make_mesh(name, verts, faces, mat=None, smooth=True, sub=1, head=False, recalc=True, solid=0.0,
              uvs=None, bevel=0.0):
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces])
    if uvs is not None:
        uvl = me.uv_layers.new(name="UV")
        for poly in me.polygons:
            for li in poly.loop_indices:
                uvl.data[li].uv = uvs[me.loops[li].vertex_index]
    if recalc:
        bm = bmesh.new(); bm.from_mesh(me)
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        bm.to_mesh(me); bm.free()
    me.update()
    if smooth:
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    if mat is not None:
        me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    link(ob, head)
    if bevel:
        m = ob.modifiers.new("bev", "BEVEL"); m.width = bevel; m.segments = 3; m.limit_method = "ANGLE"
    if solid:
        m = ob.modifiers.new("sol", "SOLIDIFY"); m.thickness = solid; m.offset = 0.0
    if sub:
        m = ob.modifiers.new("sub", "SUBSURF"); m.levels = sub; m.render_levels = sub
    return ob


def quad_sphere(cuts):
    if cuts in QS_CACHE: return QS_CACHE[cuts]
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=2.0)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=cuts, use_grid_fill=True)
    U = []
    for v in bm.verts:
        x, y, z = v.co
        sx = x * math.sqrt(max(0.0, 1 - y * y / 2 - z * z / 2 + y * y * z * z / 3))
        sy = y * math.sqrt(max(0.0, 1 - z * z / 2 - x * x / 2 + z * z * x * x / 3))
        sz = z * math.sqrt(max(0.0, 1 - x * x / 2 - y * y / 2 + x * x * y * y / 3))
        U.append((sx, sy, sz))
    bm.verts.index_update()
    faces = [[v.index for v in f.verts] for f in bm.faces]
    bm.free()
    QS_CACHE[cuts] = (U, faces)
    return U, faces


def uv_sphere_data(segs=24, rings=16):
    verts = [(0.0, 0.0, 1.0)]
    for r in range(1, rings):
        th = math.pi * r / rings
        for s in range(segs):
            ph = 2 * math.pi * s / segs
            verts.append((math.sin(th) * math.cos(ph), math.sin(th) * math.sin(ph), math.cos(th)))
    verts.append((0.0, 0.0, -1.0))
    last = len(verts) - 1
    faces = []
    for s in range(segs):
        faces.append((0, 1 + s, 1 + (s + 1) % segs))
    for r in range(rings - 2):
        for s in range(segs):
            a = 1 + r * segs + s; b = 1 + r * segs + (s + 1) % segs
            faces.append((a, a + segs, b + segs, b))
    base = 1 + (rings - 2) * segs
    for s in range(segs):
        faces.append((base + s, last, base + (s + 1) % segs))
    return verts, faces


def ellipsoid(name, c, r, mat, head=False, segs=24, rings=16, rot=None, deform=None, sub=1):
    """Эллипсоид: c — центр, r — радиусы, rot — Matrix 3x3, deform(u)->u."""
    V, Fc = uv_sphere_data(segs, rings)
    out = []
    for u in V:
        if deform: u = deform(u)
        p = Vector((u[0] * r[0], u[1] * r[1], u[2] * r[2]))
        if rot is not None: p = rot @ p
        out.append(p + Vector(c))
    return make_mesh(name, out, Fc, mat, head=head, sub=sub)


def blobs(name, items, mat, head=False, segs=10, rings=7, sub=1):
    """Много маленьких сфер в одном меше: items = [(center, radius or (rx,ry,rz))]."""
    V0, F0 = uv_sphere_data(segs, rings)
    verts, faces = [], []
    for c, r in items:
        if not isinstance(r, (tuple, list)): r = (r, r, r)
        o = len(verts)
        for u in V0:
            verts.append((c[0] + u[0] * r[0], c[1] + u[1] * r[1], c[2] + u[2] * r[2]))
        for f in F0:
            faces.append([i + o for i in f])
    if not verts: return None
    return make_mesh(name, verts, faces, mat, head=head, sub=sub, recalc=False)


def loft(name, rings, mat, closed=True, cap_top=False, cap_bot=False, head=False, solid=0.0, sub=1,
         uv=False):
    """Поверхность через кольца точек (каждое кольцо — список Vector одинаковой длины)."""
    n = len(rings[0]); verts, faces, uvs = [], [], []
    for k, ring in enumerate(rings):
        for i, p in enumerate(ring):
            verts.append(tuple(p))
            uvs.append((i / max(1, n - 1), k / max(1, len(rings) - 1)))
    m = n if closed else n - 1
    for k in range(len(rings) - 1):
        for i in range(m):
            a = k * n + i; b = k * n + (i + 1) % n
            faces.append((a, b, b + n, a + n))
    if cap_top:
        c = Vector((0, 0, 0))
        for p in rings[-1]: c += Vector(p)
        verts.append(tuple(c / n)); uvs.append((0.5, 1.0)); ci = len(verts) - 1
        base = (len(rings) - 1) * n
        for i in range(m): faces.append((base + i, base + (i + 1) % n, ci))
    if cap_bot:
        c = Vector((0, 0, 0))
        for p in rings[0]: c += Vector(p)
        verts.append(tuple(c / n)); uvs.append((0.5, 0.0)); ci = len(verts) - 1
        for i in range(m): faces.append(((i + 1) % n, i, ci))
    return make_mesh(name, verts, faces, mat, head=head, solid=solid, sub=sub, uvs=uvs if uv else None)


def ell_ring(z, rx, ry, cx=0.0, cy=0.0, n=64, a0=0.0, a1=2 * math.pi, closed=True):
    """Кольцо-эллипс; угол 0 — спереди (-Y), растёт к +X."""
    pts = []
    cnt = n if closed else n + 1
    for i in range(cnt):
        a = a0 + (a1 - a0) * i / n
        pts.append(Vector((cx + rx * math.sin(a), cy - ry * math.cos(a), z)))
    return pts


def curve_tube(name, pts, radii, bevel, mat, head=False, cyclic=False, res=10, caps=True):
    cu = bpy.data.curves.new(name, "CURVE")
    cu.dimensions = "3D"; cu.bevel_depth = bevel; cu.bevel_resolution = 4; cu.resolution_u = res
    cu.use_fill_caps = caps
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(pts) - 1)
    for bp, p, r in zip(sp.bezier_points, pts, radii):
        bp.co = tuple(p); bp.handle_left_type = "AUTO"; bp.handle_right_type = "AUTO"; bp.radius = r
    sp.use_cyclic_u = cyclic
    if mat: cu.materials.append(mat)
    ob = bpy.data.objects.new(name, cu)
    return link(ob, head)


def box(name, c, size, mat, rot=None, bevel=0.002, head=False):
    sx, sy, sz = size[0] / 2, size[1] / 2, size[2] / 2
    V = [(-sx, -sy, -sz), (sx, -sy, -sz), (sx, sy, -sz), (-sx, sy, -sz),
         (-sx, -sy, sz), (sx, -sy, sz), (sx, sy, sz), (-sx, sy, sz)]
    Fc = [(0, 1, 2, 3), (4, 7, 6, 5), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
    out = []
    for v in V:
        p = Vector(v)
        if rot is not None: p = rot @ p
        out.append(p + Vector(c))
    return make_mesh(name, out, Fc, mat, smooth=False, sub=0, head=head, bevel=bevel)


def cylinder(name, p0, p1, r0, r1, mat, n=16, head=False, sub=0, cap=True, bevel=0.0):
    p0, p1 = Vector(p0), Vector(p1)
    ax = (p1 - p0).normalized()
    q = Vector((0, 0, 1)).rotation_difference(ax)
    verts, faces = [], []
    for k, (p, r) in enumerate(((p0, r0), (p1, r1))):
        for i in range(n):
            a = 2 * math.pi * i / n
            verts.append(p + q @ Vector((math.cos(a) * r, math.sin(a) * r, 0)))
    for i in range(n):
        faces.append((i, (i + 1) % n, n + (i + 1) % n, n + i))
    if cap:
        faces.append(tuple(range(n - 1, -1, -1))); faces.append(tuple(range(n, 2 * n)))
    return make_mesh(name, verts, faces, mat, smooth=True, sub=sub, head=head, bevel=bevel)


def lathe(name, prof, mat, c=(0, 0, 0), rot=None, n=24, head=False, sub=1):
    """Тело вращения вокруг Z: prof = [(r, z), ...]."""
    rings = []
    for r, z in prof:
        ring = []
        for i in range(n):
            a = 2 * math.pi * i / n
            p = Vector((r * math.cos(a), r * math.sin(a), z))
            if rot is not None: p = rot @ p
            ring.append(p + Vector(c))
        rings.append(ring)
    return loft(name, rings, mat, closed=True, head=head, sub=sub)


def star_mesh(name, n, ro, ri, depth, mat, head=False):
    verts = [(0.0, -depth, 0.0)]
    for i in range(2 * n):
        a = math.pi / 2 + math.pi * i / n
        r = ro if i % 2 == 0 else ri
        verts.append((r * math.cos(a), 0.0, r * math.sin(a)))
    verts.append((0.0, 0.0005, 0.0))
    faces = []
    m = 2 * n
    for i in range(m):
        faces.append((0, 1 + i, 1 + (i + 1) % m))
        faces.append((m + 1, 1 + (i + 1) % m, 1 + i))
    return make_mesh(name, verts, faces, mat, smooth=False, sub=0, head=head)


def place(ob, p, n, spin=0.0, head=False):
    q = Vector((0, -1, 0)).rotation_difference(Vector(n).normalized())
    m = Matrix.Translation(p) @ q.to_matrix().to_4x4() @ Matrix.Rotation(spin, 4, "Y")
    ob.matrix_basis = (HEAD_M @ m) if head else m
    return ob


# ----------------------------------------------------------------------------------------------
#  Материалы
# ----------------------------------------------------------------------------------------------
def new_mat(tag):
    MAT_N[0] += 1
    m = bpy.data.materials.new("%s_%d" % (tag, MAT_N[0]))
    m.use_nodes = True
    nt = m.node_tree
    return m, nt, nt.nodes.get("Principled BSDF")


def setp(b, name, v):
    if name in b.inputs: b.inputs[name].default_value = v


def mat_basic(col, rough=0.55, metal=0.0, spec=0.45, sheen=0.0, coat=0.0, bump=None, var=None,
              emit=None, emit_str=0.0, alpha=None, sss=0.0, tag="m"):
    m, nt, b = new_mat(tag)
    c = srgb(col) if isinstance(col, str) else col
    setp(b, "Base Color", (*c, 1.0)); setp(b, "Roughness", rough); setp(b, "Metallic", metal)
    setp(b, "Specular IOR Level", spec)
    if sheen: setp(b, "Sheen Weight", sheen); setp(b, "Sheen Roughness", 0.4)
    if coat: setp(b, "Coat Weight", coat); setp(b, "Coat Roughness", 0.15)
    if sss:
        setp(b, "Subsurface Weight", sss); setp(b, "Subsurface Radius", (1.0, 0.42, 0.25))
        setp(b, "Subsurface Scale", 0.006)
    if emit:
        setp(b, "Emission Color", (*(srgb(emit) if isinstance(emit, str) else emit), 1.0))
        setp(b, "Emission Strength", emit_str)
    if alpha is not None:
        setp(b, "Alpha", alpha); m.blend_method = "BLEND"; m.shadow_method = "NONE"
        m.show_transparent_back = False
    if bump or var:
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        sc = (bump or var)[0]
        mp.inputs["Scale"].default_value = sc
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 1.0; nz.inputs["Detail"].default_value = 3.0
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        if bump:
            bp = nt.nodes.new("ShaderNodeBump")
            bp.inputs["Strength"].default_value = bump[1]; bp.inputs["Distance"].default_value = 0.002
            nt.links.new(nz.outputs["Fac"], bp.inputs["Height"])
            nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
        if var:
            rp = nt.nodes.new("ShaderNodeValToRGB")
            rp.color_ramp.elements[0].position = 0.3; rp.color_ramp.elements[1].position = 0.7
            rp.color_ramp.elements[0].color = (*mulc(c, var[1]), 1.0)
            rp.color_ramp.elements[1].color = (*mulc(c, var[2]), 1.0)
            nt.links.new(nz.outputs["Fac"], rp.inputs["Fac"])
            nt.links.new(rp.outputs["Color"], b.inputs["Base Color"])
    return m


def mat_hair(col, beard=False):
    sc = (260.0, 260.0, 30.0) if beard else (230.0, 26.0, 150.0)
    return mat_basic(col, rough=0.5, spec=0.35, sheen=0.3, bump=(sc, 0.35), var=(sc, 0.78, 1.18), tag="hair")


def mat_fur(col):
    return mat_basic(col, rough=0.8, spec=0.2, sheen=0.6, bump=((380.0, 380.0, 380.0), 0.55),
                     var=((120.0, 120.0, 60.0), 0.7, 1.3), tag="fur")


def mat_ermine():
    m, nt, b = new_mat("ermine")
    setp(b, "Roughness", 0.75); setp(b, "Sheen Weight", 0.6); setp(b, "Specular IOR Level", 0.25)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping"); mp.inputs["Scale"].default_value = (26.0, 26.0, 13.0)
    vo = nt.nodes.new("ShaderNodeTexVoronoi")
    lt = nt.nodes.new("ShaderNodeMath"); lt.operation = "LESS_THAN"; lt.inputs[1].default_value = 0.11
    mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"
    mx.inputs[6].default_value = (*srgb("#f4f1ea"), 1.0); mx.inputs[7].default_value = (0.012, 0.011, 0.01, 1.0)
    nt.links.new(tc.outputs["Object"], mp.inputs["Vector"]); nt.links.new(mp.outputs["Vector"], vo.inputs["Vector"])
    nt.links.new(vo.outputs["Distance"], lt.inputs[0]); nt.links.new(lt.outputs[0], mx.inputs[0])
    nt.links.new(mx.outputs[2], b.inputs["Base Color"])
    nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 400.0
    bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.4
    nt.links.new(tc.outputs["Object"], nz.inputs["Vector"]); nt.links.new(nz.outputs["Fac"], bp.inputs["Height"])
    nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m


def mat_skin(col, tinted=False):
    m, nt, b = new_mat("skin")
    c = srgb(col)
    setp(b, "Base Color", (*c, 1.0)); setp(b, "Roughness", 0.48); setp(b, "Specular IOR Level", 0.35)
    setp(b, "Subsurface Weight", 0.12); setp(b, "Subsurface Radius", (1.0, 0.38, 0.22))
    setp(b, "Subsurface Scale", 0.005)
    setp(b, "Coat Weight", 0.08); setp(b, "Coat Roughness", 0.3)
    if tinted:
        at = nt.nodes.new("ShaderNodeAttribute"); at.attribute_name = "tint"
        vm = nt.nodes.new("ShaderNodeVectorMath"); vm.operation = "MULTIPLY"
        vm.inputs[1].default_value = c
        nt.links.new(at.outputs["Color"], vm.inputs[0])
        nt.links.new(vm.outputs["Vector"], b.inputs["Base Color"])
    return m


def mat_decal(col, img, rough=0.7, metal=0.0, spec=0.4, sheen=0.25, bump=0.06, tag="cloth"):
    """Ткань с «наклейкой»: изображение проецируется спереди (по X,Z объекта)."""
    m, nt, b = new_mat(tag)
    c = srgb(col) if isinstance(col, str) else col
    setp(b, "Roughness", rough); setp(b, "Metallic", metal); setp(b, "Specular IOR Level", spec)
    if sheen: setp(b, "Sheen Weight", sheen)
    tc = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    cmb = nt.nodes.new("ShaderNodeCombineXYZ")
    mp = nt.nodes.new("ShaderNodeMapping")
    W = CANVAS_X1 - CANVAS_X0; H = CANVAS_Z1 - CANVAS_Z0
    mp.inputs["Scale"].default_value = (1.0 / W, 1.0 / H, 1.0)
    mp.inputs["Location"].default_value = (-CANVAS_X0 / W, -CANVAS_Z0 / H, 0.0)
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img; tex.extension = "CLIP"
    tex.interpolation = "Linear"
    lt = nt.nodes.new("ShaderNodeMath"); lt.operation = "LESS_THAN"; lt.inputs[1].default_value = 0.02
    mu = nt.nodes.new("ShaderNodeMath"); mu.operation = "MULTIPLY"
    mx = nt.nodes.new("ShaderNodeMix"); mx.data_type = "RGBA"
    mx.inputs[6].default_value = (*c, 1.0)
    L = nt.links
    L.new(tc.outputs["Object"], sep.inputs[0])
    L.new(sep.outputs[0], cmb.inputs[0]); L.new(sep.outputs[2], cmb.inputs[1])
    L.new(cmb.outputs[0], mp.inputs["Vector"]); L.new(mp.outputs["Vector"], tex.inputs["Vector"])
    L.new(sep.outputs[1], lt.inputs[0])
    L.new(tex.outputs["Alpha"], mu.inputs[0]); L.new(lt.outputs[0], mu.inputs[1])
    L.new(mu.outputs[0], mx.inputs[0]); L.new(tex.outputs["Color"], mx.inputs[7])
    L.new(mx.outputs[2], b.inputs["Base Color"])
    if bump:
        mp2 = nt.nodes.new("ShaderNodeMapping"); mp2.inputs["Scale"].default_value = (420.0, 420.0, 420.0)
        nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 1.0
        bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = bump
        bp.inputs["Distance"].default_value = 0.002
        L.new(tc.outputs["Object"], mp2.inputs["Vector"]); L.new(mp2.outputs["Vector"], nz.inputs["Vector"])
        L.new(nz.outputs["Fac"], bp.inputs["Height"]); L.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m


def mat_image(img, rough=0.6, emit=0.0):
    m, nt, b = new_mat("img")
    setp(b, "Roughness", rough); setp(b, "Specular IOR Level", 0.3)
    tex = nt.nodes.new("ShaderNodeTexImage"); tex.image = img
    nt.links.new(tex.outputs["Color"], b.inputs["Base Color"])
    if emit:
        nt.links.new(tex.outputs["Color"], b.inputs["Emission Color"]); setp(b, "Emission Strength", emit)
    return m


# ----------------------------------------------------------------------------------------------
#  Растровая «кисть» для наклеек на одежду и бумаг (чистый Python)
# ----------------------------------------------------------------------------------------------
CANVAS_X0, CANVAS_X1, CANVAS_Z0, CANVAS_Z1 = -0.22, 0.22, -0.42, 0.04


class Canvas:
    def __init__(self, W=320, H=320, x0=CANVAS_X0, x1=CANVAS_X1, z0=CANVAS_Z0, z1=CANVAS_Z1, bg=None):
        self.W, self.H = W, H
        self.x0, self.x1, self.z0, self.z1 = x0, x1, z0, z1
        if bg is None: self.px = [0.0] * (W * H * 4)
        else: self.px = list(bg) + [1.0]; self.px = self.px * (W * H)
        self.used = False

    def _xy(self, x, z):
        return ((x - self.x0) / (self.x1 - self.x0) * self.W, (z - self.z0) / (self.z1 - self.z0) * self.H)

    def poly(self, pts, col, fn=None):
        self.used = True
        P = [self._xy(x, z) for x, z in pts]
        n = len(P)
        ys = [p[1] for p in P]
        j0 = max(0, int(min(ys))); j1 = min(self.H - 1, int(max(ys)) + 1)
        W = self.W; px = self.px
        c4 = None if col is None else (col[0], col[1], col[2], col[3] if len(col) > 3 else 1.0)
        for j in range(j0, j1 + 1):
            yc = j + 0.5; xs = []
            for i in range(n):
                x1, y1 = P[i]; x2, y2 = P[(i + 1) % n]
                if (y1 <= yc < y2) or (y2 <= yc < y1):
                    xs.append(x1 + (yc - y1) * (x2 - x1) / (y2 - y1))
            xs.sort()
            for k in range(0, len(xs) - 1, 2):
                a = max(0, int(math.ceil(xs[k] - 0.5))); b = min(W - 1, int(math.floor(xs[k + 1] - 0.5)))
                for i in range(a, b + 1):
                    c = c4
                    if fn is not None:
                        wx = self.x0 + (i + 0.5) / W * (self.x1 - self.x0)
                        wz = self.z0 + yc / self.H * (self.z1 - self.z0)
                        c = fn(wx, wz)
                        if c is None: continue
                        if len(c) == 3: c = (c[0], c[1], c[2], 1.0)
                    o = (j * W + i) * 4
                    if c[3] >= 1.0: px[o:o + 4] = c
                    else:
                        t = c[3]
                        px[o] = px[o] * (1 - t) + c[0] * t; px[o + 1] = px[o + 1] * (1 - t) + c[1] * t
                        px[o + 2] = px[o + 2] * (1 - t) + c[2] * t; px[o + 3] = max(px[o + 3], t)

    def rect(self, xa, za, xb, zb, col, fn=None):
        self.poly([(xa, za), (xb, za), (xb, zb), (xa, zb)], col, fn)

    def circle(self, x, z, r, col, n=18):
        self.poly([(x + r * math.cos(2 * math.pi * i / n), z + r * math.sin(2 * math.pi * i / n))
                   for i in range(n)], col)

    def line(self, pts, w, col):
        for i in range(len(pts) - 1):
            (xa, za), (xb, zb) = pts[i], pts[i + 1]
            dx, dz = xb - xa, zb - za; L = math.hypot(dx, dz) or 1e-9
            nx, nz = -dz / L * w / 2, dx / L * w / 2
            self.poly([(xa + nx, za + nz), (xb + nx, zb + nz), (xb - nx, zb - nz), (xa - nx, za - nz)], col)

    def image(self, name):
        img = bpy.data.images.new(name, self.W, self.H, alpha=True, float_buffer=True)
        img.pixels.foreach_set(self.px)
        img.pack()
        return img


def C(h, a=1.0):
    c = srgb(h)
    return (c[0], c[1], c[2], a)


# ----------------------------------------------------------------------------------------------
#  Сцена, свет, камера
# ----------------------------------------------------------------------------------------------
def setup_scene(samples, scale):
    global COLL
    for ob in list(bpy.data.objects): bpy.data.objects.remove(ob, do_unlink=True)
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE"
    sc.render.resolution_x = CELL_W * scale; sc.render.resolution_y = CELL_H * scale
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"; sc.render.image_settings.color_mode = "RGBA"
    sc.render.image_settings.color_depth = "8"
    ee = sc.eevee
    ee.taa_render_samples = samples
    ee.use_gtao = True; ee.gtao_distance = 0.05; ee.gtao_factor = 0.9
    ee.use_soft_shadows = True
    ee.shadow_cube_size = "2048"; ee.shadow_cascade_size = "2048"
    ee.use_ssr = False; ee.use_bloom = True; ee.bloom_intensity = 0.03; ee.bloom_threshold = 1.2
    ee.sss_samples = 9
    sc.view_settings.view_transform = "Filmic"; sc.view_settings.look = "Medium High Contrast"
    sc.view_settings.exposure = 0.0
    COLL = bpy.data.collections.new("Portrait"); sc.collection.children.link(COLL)
    rig = bpy.data.collections.new("Studio"); sc.collection.children.link(rig)
    # мир: мягкий градиент (даёт отражения металлам)
    w = bpy.data.worlds.new("Studio"); sc.world = w; w.use_nodes = True
    nt = w.node_tree; bg = nt.nodes["Background"]
    tc = nt.nodes.new("ShaderNodeTexCoord"); sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    mr = nt.nodes.new("ShaderNodeMapRange"); mr.inputs[1].default_value = -1.0; mr.inputs[2].default_value = 1.0
    rp = nt.nodes.new("ShaderNodeValToRGB")
    rp.color_ramp.elements[0].color = (*srgb("#3a3028"), 1); rp.color_ramp.elements[0].position = 0.35
    rp.color_ramp.elements[1].color = (*srgb("#c8d2e0"), 1); rp.color_ramp.elements[1].position = 0.75
    e = rp.color_ramp.elements.new(0.52); e.color = (*srgb("#9a948a"), 1)
    nt.links.new(tc.outputs["Generated"], sep.inputs[0]); nt.links.new(sep.outputs[2], mr.inputs[0])
    nt.links.new(mr.outputs[0], rp.inputs[0]); nt.links.new(rp.outputs[0], bg.inputs[0])
    bg.inputs[1].default_value = 0.55
    tgt = Vector((0, 0, 0.17))

    def area(name, loc, energy, size, col):
        ld = bpy.data.lights.new(name, "AREA"); ld.energy = energy; ld.size = size; ld.color = col
        ld.use_contact_shadow = True; ld.contact_shadow_distance = 0.03; ld.shadow_soft_size = size
        ob = bpy.data.objects.new(name, ld); rig.objects.link(ob)
        ob.location = tgt + Vector(loc)
        ob.rotation_euler = (tgt - ob.location).to_track_quat("-Z", "Y").to_euler()
        return ob
    area("Key", (-1.15, -1.35, 0.85), 80.0, 1.1, (1.0, 0.95, 0.88))
    area("Fill", (1.5, -1.0, 0.15), 30.0, 1.6, (0.82, 0.88, 1.0))
    area("Rim", (1.0, 1.25, 0.55), 85.0, 0.6, (1.0, 0.80, 0.58))
    area("Rim2", (-1.1, 1.0, 0.35), 45.0, 0.6, (1.0, 0.9, 0.78))
    area("Top", (0.0, -0.2, 1.6), 14.0, 1.2, (1.0, 0.97, 0.94))
    cd = bpy.data.cameras.new("Cam"); cd.lens = 85.0; cd.sensor_fit = "VERTICAL"; cd.sensor_height = 24.0
    cd.clip_start = 0.05; cd.clip_end = 20
    cam = bpy.data.objects.new("Cam", cd); rig.objects.link(cam); sc.camera = cam
    cam.location = (0.0, -1.95, 0.30)
    cam.rotation_euler = (Vector((0, 0, 0.12)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    return sc, cam


def clear_character():
    for ob in list(COLL.objects): bpy.data.objects.remove(ob, do_unlink=True)
    bpy.data.orphans_purge(do_local_ids=True, do_linked_ids=True, do_recursive=True)


# ----------------------------------------------------------------------------------------------
#  Голова
# ----------------------------------------------------------------------------------------------
class Ctx:
    pass


def face_disp(X, Z, F):
    ax = abs(X)
    ex = 0.0315 * F["eye_sep"]
    d = -0.0085 * gauss(ax - ex, 0.0165) * gauss(Z - 0.001, 0.0125)
    d += 0.0055 * F["brow"] * gauss(Z - 0.0245, 0.0105) * sstep(0.07, 0.035, ax)
    nb, nt = 0.014, -0.040 * F["nose_v"]
    L = 0.023 * F["nose"]
    if -0.08 < Z < 0.06:
        s = (nb - Z) / (nb - nt)
        if s < 0: prof = L * 0.16 * gauss(Z - nb, 0.012)
        elif s <= 1: prof = L * (0.16 + 0.84 * s ** (1.3 + 0.8 * F["snub"]))
        else: prof = L * gauss(Z - nt, 0.0058 * (1 + 0.3 * F["snub"]))
        sw = (0.0058 + 0.0042 * clamp(s, 0, 1)) * F["nose_w"]
        d += prof * gauss(X, sw)
        d += 0.0062 * F["nose_w"] * gauss(ax - 0.0118 * F["nose_w"], 0.0062) * gauss(Z - (nt + 0.004), 0.0062)
    mz = F["mz"]
    d += 0.0042 * gauss(X, 0.018) * gauss(Z - (mz + 0.0085), 0.0065)
    d += 0.0026 * F["lips"] * gauss(X, 0.0135) * gauss(Z - (mz + 0.0028), 0.0028)
    d += 0.0046 * F["lips"] * gauss(X, 0.0135) * gauss(Z - (mz - 0.0052), 0.0040)
    d -= 0.0024 * gauss(X, 0.019) * gauss(Z - mz, 0.0016)
    d -= 0.0018 * gauss(X, 0.012) * gauss(Z - (mz - 0.016), 0.0045)
    d += 0.0085 * F["chin"] * gauss(X, 0.021 * F["chin_w"]) * gauss(Z + 0.106, 0.0155)
    d += 0.0055 * F["cheek"] * gauss(ax - 0.047, 0.017) * gauss(Z + 0.020, 0.017)
    d += 0.0075 * F["fat"] * gauss(ax - 0.043, 0.022) * gauss(Z + 0.058, 0.024)
    if F["age"] > 0:
        # носогубная складка: отрезок от крыла носа к углу рта
        ax0, az0, ax1, az1 = 0.017, -0.040, 0.027, -0.076
        vx, vz = ax1 - ax0, az1 - az0; t = clamp(((ax - ax0) * vx + (Z - az0) * vz) / (vx * vx + vz * vz), 0, 1)
        dist = math.hypot(ax - (ax0 + vx * t), Z - (az0 + vz * t))
        d -= 0.0016 * F["age"] * gauss(dist, 0.0022)
        d += 0.0012 * F["age"] * gauss(ax - 0.03, 0.012) * gauss(Z + 0.016, 0.006)
    return d


def head_shape(u, F):
    x, y, z = u
    rx = 0.0765 * F["w"]; ry = 0.093 * F["d"]; rzt = 0.118 * F["top"]; rzb = 0.122 * F["h"]
    X = x * rx; Y = y * ry; Z = z * rzt if z > 0 else z * rzb
    if z < 0:
        # нижняя челюсть: линия от подбородка вверх-назад к углу челюсти
        Z *= 1.0 - 0.30 * sstep(-0.70, 0.10, y) * sstep(-0.25, -0.95, z)
    t = sstep(-0.10, -1.0, z)
    X *= (1.0 - 0.72 * F["jaw"] * t ** 1.25) * (1.0 + 0.10 * F["fat"] * gauss(z + 0.45, 0.35))
    if y > 0:
        Y *= 1.0 - 0.42 * t * sstep(0.0, 0.8, y)
        Y += 0.011 * sstep(-0.35, 0.45, z) * sstep(0.15, 1.0, y)
    else:
        Y *= 1.0 - 0.13 * sstep(-0.2, -1.0, y) * (1 - 0.5 * t)
        Y -= 0.006 * F["fat"] * t * sstep(-0.2, -0.9, y)
    X *= 1.0 - 0.05 * gauss(z - 0.28, 0.25) * sstep(-0.1, -0.7, y)
    wf = sstep(-0.30, -0.80, y)
    if wf > 0:
        Y -= wf * face_disp(X, Z, F)
    return (X, Y, Z)


def build_head(cx, s):
    F = s["F"]
    U, faces = quad_sphere(44)
    pos = [head_shape(u, F) for u in U]
    female = s["sex"] == "f"
    # тонировка кожи: губы, румянец, щетина, родимое пятно
    tint = []
    lip = (0.93, 0.66, 0.64) if female else (0.97, 0.76, 0.72)
    bm_dirs = [Vector(v).normalized() for v in ((0.10, -0.52, 0.85), (0.18, -0.42, 0.89), (0.02, -0.40, 0.92),
                                                (0.22, -0.55, 0.80))]
    shaved = (s["sex"] == "m" and s["age"] > 25 and s["beard"] in (None, "goatee", "sideburns", "chops"))
    stub = s["beard"] == "stubble"
    for u, p in zip(U, pos):
        X, Y, Z = p
        r, g, b = 1.0, 1.0, 1.0
        if u[1] < -0.2:
            wl = gauss(X, 0.0175) * gauss(Z - (F["mz"] - 0.001), 0.0072) * sstep(-0.3, -0.7, u[1])
            r, g, b = r + (lip[0] - 1) * wl, g + (lip[1] - 1) * wl, b + (lip[2] - 1) * wl
            wc = gauss(abs(X) - 0.045, 0.02) * gauss(Z + 0.035, 0.022) * (0.9 if female else 0.55)
            r, g, b = r * (1 - 0.02 * wc), g * (1 - 0.13 * wc), b * (1 - 0.13 * wc)
            wn = gauss(X, 0.012) * gauss(Z + 0.038, 0.01)
            r, g, b = r, g * (1 - 0.06 * wn), b * (1 - 0.06 * wn)
            wo = gauss(abs(X) - 0.0118 * F["nose_w"], 0.004) * gauss(Z - (-0.040 * F["nose_v"] - 0.0035), 0.0028)
            r, g, b = r * (1 - 0.5 * wo), g * (1 - 0.55 * wo), b * (1 - 0.55 * wo)
            # тень век
            we = gauss(abs(X) - 0.031, 0.013) * gauss(Z - 0.012, 0.007)
            r, g, b = r * (1 - 0.08 * we), g * (1 - 0.11 * we), b * (1 - 0.09 * we)
        if shaved or stub:
            at = abs(math.atan2(u[0], -u[1]))
            wb = beard_weight(at, u[2], "full") * (1 - gauss(X, 0.016) * gauss(Z - F["mz"], 0.009))
            k = (0.22 if stub else 0.07) * wb
            r, g, b = r * (1 - 0.30 * k), g * (1 - 0.26 * k), b * (1 - 0.18 * k)
        if s["extra"].get("birthmark"):
            un = Vector(u)
            wbm = max(gauss((un - d).length, 0.13) for d in bm_dirs)
            wbm = sstep(0.35, 0.6, wbm)
            r, g, b = r * (1 - 0.10 * wbm), g * (1 - 0.56 * wbm), b * (1 - 0.50 * wbm)
        tint.append((r, g, b, 1.0))
    skin_t = mat_skin(cx.skin_hex, tinted=True)
    ob = make_mesh("head", pos, faces, skin_t, head=True, recalc=True, sub=1)
    me = ob.data
    ca = me.color_attributes.new("tint", "FLOAT_COLOR", "POINT")
    for i, c in enumerate(tint): ca.data[i].color = c
    normals = [Vector(v.normal) for v in me.vertices]
    cx.U, cx.hfaces, cx.hpos, cx.hnorm = U, faces, [Vector(p) for p in pos], normals
    cx.hbvh = BVHTree.FromPolygons(cx.hpos, faces)
    cx.F = F
    return ob


def hit_front(bvh, x, z, y0=-0.4):
    r = bvh.ray_cast(Vector((x, y0, z)), Vector((0, 1, 0)))
    if r[0] is None: return Vector((x, -0.08, z)), Vector((0, -1, 0))
    return r[0], r[1]


def build_face(cx, s):
    F = cx.F; female = s["sex"] == "f"
    skin = cx.skin
    white = mat_basic("#f3eee6", rough=0.18, spec=0.6, tag="eyew")
    iris = mat_basic(s["eyes"], rough=0.12, spec=0.6, tag="iris")
    pupil = mat_basic("#0b0907", rough=0.08, spec=0.7, tag="pupil")
    lash = mat_basic("#2a1a14" if not female else "#1c1210", rough=0.6, tag="lash")
    g = Vector((0.30, -1.0, 0.03)).normalized()
    re = 0.0128 if female else 0.0124
    ex = 0.0315 * F["eye_sep"]
    for sd in (-1, 1):
        p, n = hit_front(cx.hbvh, sd * ex, 0.001)
        c = Vector((sd * ex * 0.985, p.y - 0.0030 + re, 0.0005))
        ellipsoid("eye", c, (re, re, re), white, head=True, segs=20, rings=14)
        rot = Vector((0, -1, 0)).rotation_difference(g).to_matrix()
        ellipsoid("iris", c + g * (re - 0.0021), (0.0066, 0.0024, 0.0066), iris, head=True, segs=16, rings=10,
                  rot=rot)
        ellipsoid("pupil", c + g * (re - 0.0004), (0.0029, 0.0009, 0.0029), pupil, head=True, segs=12,
                  rings=8, rot=rot)
        # верхнее веко
        a = 0.30; zf = re * (0.52 if female else 0.46)
        hgt = zf + re * math.tan(a)
        nrm = Vector((0, -math.sin(a), math.cos(a)))
        rl = re * 1.10
        V, Fc = uv_sphere_data(20, 14)
        bm = bmesh.new()
        for v in V: bm.verts.new(Vector(v) * rl)
        bm.verts.ensure_lookup_table()
        for f in Fc: bm.faces.new([bm.verts[i] for i in f])
        bmesh.ops.bisect_plane(bm, geom=bm.verts[:] + bm.edges[:] + bm.faces[:], plane_co=(0, 0, hgt * 0.98),
                               plane_no=nrm, clear_inner=True)
        verts = [tuple(v.co + c) for v in bm.verts]
        bm.verts.index_update()
        fl = [[v.index for v in f.verts] for f in bm.faces]
        bm.free()
        make_mesh("lid", verts, fl, skin, head=True, solid=0.0012, sub=1)
        # линия ресниц по краю века
        dd = hgt * math.cos(a) * 0.98
        rc = math.sqrt(max(1e-8, rl * rl - dd * dd))
        cc = c + nrm * dd
        e1 = Vector((1, 0, 0)); e2 = nrm.cross(e1)
        pts = [cc + (e1 * math.sin(t) - e2 * math.cos(t)) * (rc + 0.0004) for t in
               [(-1.25 + 2.5 * i / 8) for i in range(9)]]
        rad = [0.5, 0.8, 1.0, 1.0, 1.0, 1.0, 1.0, 1.1 if female else 0.9, 1.3 if female else 0.6]
        if sd < 0: rad = rad[::-1]
        curve_tube("lash", pts, rad, 0.0011 if female else 0.0008, lash, head=True)
        # нижнее веко
        ellipsoid("lowlid", c + Vector((0, 0.0004, -0.0005)), (re * 1.05, re * 1.05, re * 1.05), skin, head=True,
                  segs=16, rings=10, deform=lambda u: (u[0], u[1], min(u[2], -0.62)))
    # уши
    for sd in (-1, 1):
        r = cx.hbvh.ray_cast(Vector((sd * 0.3, 0.010, -0.012)), Vector((-sd, 0, 0)))
        px = r[0].x if r[0] else sd * 0.072
        rot = (Matrix.Rotation(sd * 0.32, 3, "Z") @ Matrix.Rotation(-0.12, 3, "X"))
        ec = Vector((px + sd * 0.0045, 0.012, -0.012))
        k = 1.12 if s["id"] in ("khrushchev", "brezhnev", "chernenko") else 1.0
        ellipsoid("ear", ec, (0.0075 * k, 0.019 * k, 0.030 * k), cx.skin_ear, head=True, segs=16, rings=12,
                  rot=rot, deform=lambda u: (u[0] + 0.25 * (1 - u[1] * u[1]) * (1 if u[0] > 0 else 0.3), u[1],
                                             u[2] * (1.0 - 0.15 * max(0.0, -u[2]))))
        ellipsoid("ear_in", ec + Vector((sd * 0.0035, -0.002, 0.0)), (0.004 * k, 0.010 * k, 0.017 * k),
                  cx.skin_ear_in, head=True, segs=10, rings=8, rot=rot)
    # брови
    bmat = mat_hair(s["brow"])
    kind = s["brows"]
    bev = {"thin": 0.0016, "normal": 0.0025, "thick": 0.0034, "huge": 0.0050}[kind]
    for sd in (-1, 1):
        xs = [0.010, 0.020, 0.032, 0.044, 0.051]
        zs = [0.020, 0.026, 0.028, 0.025, 0.018] if kind != "thin" else [0.022, 0.029, 0.032, 0.029, 0.021]
        if kind == "huge": zs = [0.022, 0.029, 0.031, 0.027, 0.020]
        pts = []
        for x, z in zip(xs, zs):
            p, n = hit_front(cx.hbvh, sd * x * F["eye_sep"], z)
            pts.append(p + n * (bev * 0.55))
        rad = [0.75, 1.0, 0.95, 0.75, 0.45] if kind != "huge" else [1.0, 1.2, 1.15, 1.0, 0.7]
        curve_tube("brow", pts, rad, bev, bmat, head=True)
    # линия рта
    mzz = F["mz"]
    pts = []
    for x in (-0.020, -0.012, 0.0, 0.012, 0.020):
        p, n = hit_front(cx.hbvh, x, mzz + 0.0015 * (abs(x) / 0.02) ** 2)
        pts.append(p - n * 0.0002)
    curve_tube("mouth", pts, [0.4, 0.9, 1.0, 0.9, 0.4], 0.0010, cx.mouth_mat, head=True)


# ----------------------------------------------------------------------------------------------
#  Волосы, бороды, усы
# ----------------------------------------------------------------------------------------------
HAIR = {
    "short": {}, "buzz": dict(th=0.0045, top=0.3, zf=0.50),
    "combed": dict(th=0.012, top=0.9, quiff=0.4),
    "combed_back": dict(zf=0.55, th=0.012, top=0.9, quiff=0.3, temple=0.05),
    "swept": dict(zf=0.60, temple=0.12, th=0.014, top=1.0, quiff=0.6),
    "wavy": dict(th=0.014, top=1.0, quiff=0.8, wave=1.0),
    "receding": dict(zf=0.74, temple=0.20, th=0.008, top=0.4),
    "thinning": dict(zf=0.68, temple=0.17, th=0.0055, top=0.3),
    "bald_fringe": dict(zf=1.3, temple=0.0, th=0.011, top=0.0, bald=(0.10, 0.27, 0.40, 0.50)),
    "bald": dict(zf=1.3, temple=0.0, th=0.0035, top=0.0, bald=(0.0, 0.08, 0.18, 0.26)),
    "balding_curls": dict(zf=0.82, temple=0.18, th=0.007, sb=-0.42, blobs="temples"),
    "fringe": dict(zf=0.30, temple=-0.10, sb=-0.15, side=-0.15, ears=0.0, back=-0.55, th=0.012, top=0.6),
    "bob": dict(zf=0.40, temple=-0.05, sb=-0.25, side=-0.25, ears=0.0, back=-0.6, th=0.012, top=0.5,
                curtain=dict(bot=-0.165, flare=0.0, open=1.32, curl=0.015)),
    "long": dict(zf=0.45, temple=-0.05, ears=0.0, side=-0.25, sb=-0.25, back=-0.6, th=0.012, top=0.6,
                 curtain=dict(bot=-0.26, flare=0.035, open=1.27, curl=0.0)),
    "monk": dict(zf=0.46, ears=0.0, side=-0.12, sb=-0.15, back=-0.6, th=0.010,
                 curtain=dict(bot=-0.21, flare=0.01, open=1.48, curl=0.0)),
    "romantic": dict(zf=0.40, th=0.016, top=1.0, quiff=0.9, sb=-0.40, blobs="top", wave=1.0, ears=0.6),
    "noble": dict(zf=0.50, temple=0.12, side=0.66, sb=0.66, back=0.62, th=0.010, top=0.6, quiff=0.6),
    "wig_powder": dict(zf=0.66, temple=0.0, sb=0.02, side=0.02, ears=0.5, back=-0.55, th=0.014, top=0.7,
                       rolls=True),
    "wig_big": dict(zf=0.48, temple=0.0, sb=-0.2, side=-0.25, ears=0.0, back=-0.6, th=0.024, top=1.3,
                    curtain=dict(bot=-0.38, flare=0.08, open=1.12, th=0.03, curl=0.0), blobs="curtain"),
    "bun": dict(zf=0.50, th=0.010, top=0.6, sb=-0.02, quiff=0.3, bun=(0.0, 0.080, 0.092, 0.036)),
    "female_up": dict(zf=0.56, th=0.015, top=1.1, quiff=0.6, ears=0.0, side=-0.05, sb=-0.05,
                      bun=(0.0, 0.08, 0.10, 0.038), blobs="sides"),
}


def hair_params(name):
    p = dict(zf=0.52, temple=0.08, sb=-0.08, side=0.16, back=-0.50, th=0.010, top=0.6, quiff=0.0, ears=1.0,
             bald=None, wave=0.0, curtain=None, blobs=None, rolls=False, bun=None)
    p.update(HAIR[name])
    return p


def hair_weight(u, P):
    x, y, z = u
    at = abs(math.atan2(x, -y))
    zf, tp = P["zf"], P["temple"]
    zl = interp(at, [0.0, 0.45, 0.85, 1.05, 1.18, 1.38, 1.62, 1.90, 2.40, 3.15],
                [zf, zf - 0.02, zf - 0.18 + tp, zf - 0.25 + tp * 0.5, P["sb"], P["sb"] + 0.02, P["side"],
                 P["side"] - 0.12, P["back"] + 0.15, P["back"]])
    w = sstep(zl - 0.035, zl + 0.035, z)
    if P["bald"]:
        b = P["bald"]
        zb = interp(at, [0.0, 1.2, 1.7, 2.5, 3.15], [b[0], b[1], b[2], b[3], b[3]])
        w *= sstep(zb + 0.05, zb - 0.05, z)
    if P["ears"] > 0:
        w *= 1.0 - P["ears"] * math.exp(-((at - 1.66) / 0.21) ** 2 - ((z + 0.08) / 0.30) ** 2)
    th = P["th"] * (1.0 + P["top"] * sstep(0.15, 0.9, z) + P["quiff"] * gauss(at, 0.6) * gauss(z - 0.62, 0.24))
    th *= 0.40 + 0.60 * sstep(zl - 0.02, zl + 0.24, z)
    if P["wave"]:
        th *= 1.0 + 0.22 * P["wave"] * math.sin(at * 7.0 + z * 9.0) * sstep(0.2, 0.6, z)
    return w, th


def beard_weight(at, z, kind):
    if kind == "goatee":
        return sstep(-0.63, -0.71, z) * sstep(0.40, 0.26, at)
    if kind == "sideburns":
        return sstep(0.98, 1.12, at) * sstep(1.62, 1.46, at) * sstep(-0.42, -0.30, z) * sstep(0.12, 0.0, z)
    bl = interp(at, [0.0, 0.20, 0.38, 0.62, 0.90, 1.15, 1.32, 1.48],
                [-0.70, -0.68, -0.56, -0.42, -0.26, -0.10, 0.00, 0.05])
    w = sstep(bl + 0.05, bl - 0.05, z) * sstep(1.62, 1.45, at)
    if kind == "chops":
        w *= sstep(0.45, 0.68, at)
    return w


def shell(name, cx, wfun, mat, eps=0.0025, extra=None):
    pos = []
    for u, p, n in zip(cx.U, cx.hpos, cx.hnorm):
        w, th = wfun(u)
        d = th * w - eps
        q = p + n * d
        if extra: q = extra(u, q, w)
        pos.append(q)
    ob = make_mesh(name, pos, cx.hfaces, mat, head=True, recalc=False, sub=1)
    return ob, pos


def head_hit(cx, d, origin_z=0.0):
    """Точка на голове (или волосах) по направлению d от центра."""
    c = Vector((0, 0.006, origin_z))
    bv = cx.outer_bvh or cx.hbvh
    r = bv.ray_cast(c + d * 0.4, -d)
    return r[0] if r[0] else c + d * 0.09


def build_hair(cx, s):
    cx.outer_bvh = None
    if s["hs"] == "none": return
    P = hair_params(s["hs"])
    mat = mat_hair(s["hair"])
    cx.hair_mat = mat
    ob, pos = shell("hair", cx, lambda u: hair_weight(u, P), mat)
    cx.outer_bvh = BVHTree.FromPolygons(pos, cx.hfaces)
    if P["curtain"]:
        cu = P["curtain"]; th = cu.get("th", P["th"])
        rings = []
        levels = [(0.035, 0.0805 + th * 0.55, 0.099 + th * 0.55, 0.006),
                  (-0.02, 0.0815 + th, 0.098 + th, 0.010),
                  (-0.08, 0.0830 + th, 0.094 + th, 0.016),
                  (cu["bot"] * 0.6, 0.086 + th + cu["flare"] * 0.4, 0.090 + th, 0.022),
                  (cu["bot"], 0.092 + th + cu["flare"], 0.086 + th + cu["flare"] * 0.3, 0.030)]
        if cu["curl"]:
            levels.append((cu["bot"] + 0.012, 0.084 + th + cu["flare"], 0.080 + th, 0.032))
        o = cu["open"]
        for z, rx, ry, cy in levels:
            rings.append(ell_ring(z, rx, ry, 0.0, cy, n=40, a0=o, a1=2 * math.pi - o, closed=False))
        loft("hair_curtain", rings, mat, closed=False, head=True, solid=0.006, sub=2)
        if P["blobs"] == "curtain":
            items = []
            rnd = random.Random(7)
            for z, rx, ry, cy in levels[1:]:
                for i in range(15):
                    a = o + (2 * math.pi - 2 * o) * (i + 0.5 * rnd.random()) / 15
                    rr = 0.012 + 0.004 * rnd.random()
                    items.append(((rx * math.sin(a), cy - ry * math.cos(a), z), rr))
            blobs("curls", items, mat, head=True)
    if P["blobs"] in ("top", "temples", "sides"):
        rnd = random.Random(11)
        items = []
        for k in range(600):
            u = Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), rnd.uniform(-1, 1)))
            if u.length < 0.2: continue
            u.normalize()
            at = abs(math.atan2(u.x, -u.y))
            if P["blobs"] == "top" and not (u.z > 0.25 and at < 2.2): continue
            if P["blobs"] == "temples" and not (0.75 < at < 1.5 and -0.2 < u.z < 0.55 or (at < 0.35 and 0.62 < u.z < 0.8)): continue
            if P["blobs"] == "sides" and not (1.0 < at < 1.9 and -0.1 < u.z < 0.45): continue
            w, th = hair_weight(tuple(u), P)
            if w < 0.6 and P["blobs"] != "temples": continue
            p = head_hit(cx, u, 0.0)
            items.append((p + u * 0.002, 0.0075 + 0.003 * rnd.random()))
            if len(items) > 70: break
        blobs("curls", items, mat, head=True)
    if P["rolls"]:
        for sd in (-1, 1):
            for z, r in ((0.002, 0.0115), (-0.030, 0.0105)):
                pts = []
                for yy in (-0.030, -0.005, 0.025, 0.055):
                    d = Vector((sd * 1.0, yy * 6, z * 5)).normalized()
                    p = head_hit(cx, d, 0.0)
                    pts.append(Vector((p.x + sd * r * 0.6, yy, z)))
                curve_tube("roll", pts, [0.7, 1.0, 1.0, 0.8], r, mat, head=True)
        ellipsoid("queue", (0, 0.112, -0.06), (0.012, 0.012, 0.035), mat, head=True)
        ellipsoid("bow", (0, 0.118, -0.035), (0.022, 0.006, 0.010), mat_basic("#141414", 0.4), head=True)
    if P["bun"]:
        b = P["bun"]
        ellipsoid("bun", (b[0], b[1], b[2]), (b[3] * 1.05, b[3], b[3] * 0.95), mat, head=True)


def build_beard(cx, s):
    kind = s["beard"]
    if kind is None and s["mous"] is None: return
    bmat = mat_hair(s["bcol"], beard=True)
    if kind and kind != "stubble":
        T = {"full": 0.010, "long": 0.012, "pointed": 0.008, "neat": 0.0055, "goatee": 0.007,
             "chops": 0.010, "sideburns": 0.005, "short": 0.006}[kind]

        def wf(u):
            at = abs(math.atan2(u[0], -u[1]))
            w = beard_weight(at, u[2], kind)
            th = T * (0.55 + 1.0 * sstep(-0.45, -0.95, u[2]))
            if kind == "chops": th = T * (1.0 + 0.4 * sstep(-0.2, -0.5, u[2]))
            return w, th
        L = s["blen"] if kind in ("full", "long", "pointed", "neat", "goatee") else 0.0
        taper = {"full": 0.30, "long": 0.45, "pointed": 0.85, "neat": 0.40, "goatee": 0.80}.get(kind, 0.0)
        wide = 1.1 if s["F"]["w"] > 1.03 else 1.0

        def ex(u, q, w, L=L, taper=taper, kind=kind):
            # борода — продолжение оболочки вниз от челюсти (без «мешка» под подбородком)
            if L <= 0.0 or w <= 0.001: return q
            at = abs(math.atan2(u[0], -u[1]))
            d = sstep(-0.25, -0.92, u[2]) * w
            fr = 0.35 + 0.65 * sstep(1.55, 0.25, at)
            k = d * fr
            q = Vector(q)
            q.z -= L * k
            q.y -= (0.30 * L + 0.004) * k
            q.x *= (1.0 - taper * k ** 1.6) * wide
            if kind == "pointed": q.y -= 0.25 * L * k ** 3
            return q
        shell("beard", cx, wf, bmat, extra=ex)
    m = s["mous"]
    if m is None and kind in ("full", "long", "pointed", "neat"): m = "full"
    if m is None and kind == "goatee": m = None
    if m:
        T = {"neat": ([(0.0, -0.0555), (0.010, -0.0575), (0.019, -0.0615), (0.025, -0.0665)], [1.0, 0.95, 0.75, 0.4], 0.0032),
             "full": ([(0.0, -0.0555), (0.011, -0.0580), (0.021, -0.0640), (0.027, -0.0720)], [1.0, 1.0, 0.85, 0.6], 0.0042),
             "big": ([(0.0, -0.0550), (0.012, -0.0570), (0.024, -0.0615), (0.033, -0.0680), (0.039, -0.0750)], [1.0, 1.0, 0.9, 0.62, 0.3], 0.0052),
             "long": ([(0.0, -0.0555), (0.012, -0.0590), (0.022, -0.0670), (0.028, -0.0840), (0.030, -0.1030)], [1.0, 0.95, 0.8, 0.55, 0.25], 0.0040),
             "upturned": ([(0.0, -0.0555), (0.012, -0.0585), (0.022, -0.0600), (0.030, -0.0545), (0.034, -0.0460)], [1.0, 0.9, 0.7, 0.5, 0.3], 0.0032)}[m]
        half, rad, bev = T
        pts, rr = [], []
        seq = [(-x, z, r) for (x, z), r in zip(half[::-1], rad[::-1])] + [(x, z, r) for (x, z), r in zip(half[1:], rad[1:])]
        for x, z, r in seq:
            p, n = hit_front(cx.hbvh, x, z)
            pts.append(p + n * bev * 0.65 * r + Vector((0, -0.0015, 0)))
            rr.append(r)
        curve_tube("moustache", pts, rr, bev, bmat, head=True)


# ----------------------------------------------------------------------------------------------
#  Торс и одежда
# ----------------------------------------------------------------------------------------------
TORSO_CTRL = [(-0.62, 0.172, 0.104, 0.098, 2.6), (-0.32, 0.188, 0.114, 0.104, 2.8),
              (-0.17, 0.199, 0.112, 0.104, 2.8), (-0.105, 0.198, 0.100, 0.098, 2.6),
              (-0.068, 0.188, 0.088, 0.090, 2.4), (-0.042, 0.166, 0.075, 0.078, 2.2),
              (-0.022, 0.128, 0.062, 0.064, 2.1), (-0.009, 0.098, 0.063, 0.064, 2.0),
              (0.000, 0.069, 0.060, 0.062, 2.0)]
NECK_D = [0.007]


def spow(v, e):
    return math.copysign(abs(v) ** e, v)


def build_torso(cx, s):
    fem = s["sex"] == "f"
    zs = [c[0] for c in TORSO_CTRL]
    fa = pchip(zs, [c[1] * (0.86 if fem else 1.0) for c in TORSO_CTRL])
    fbf = pchip(zs, [c[2] * (0.94 if fem else 1.0) for c in TORSO_CTRL])
    fbb = pchip(zs, [c[3] * (0.92 if fem else 1.0) for c in TORSO_CTRL])
    fn = pchip(zs, [c[4] for c in TORSO_CTRL])
    K, N = 58, 120
    pos, par = [], []
    for k in range(K):
        t = k / (K - 1)
        z = -0.62 + 0.62 * (1 - (1 - t) ** 1.7)
        a, bf, bb, ne = fa(z), fbf(z), fbb(z), fn(z)
        if fem and z < 0.0: a = max(a, 0.056 + 0.0)
        for i in range(N):
            q = 2 * math.pi * i / N
            sq, cq = math.sin(q), math.cos(q)
            x = a * spow(sq, 2.0 / ne)
            yv = spow(cq, 2.0 / ne)
            y = -bf * yv if cq > 0 else -bb * yv
            # грудь чуть вперёд
            if cq > 0: y -= 0.006 * gauss(z + 0.17, 0.08) * gauss(abs(x) - 0.07, 0.07)
            pos.append(Vector((x, y, z)))
            qq = q if q <= math.pi else q - 2 * math.pi
            par.append((qq, z))
    faces = []
    for k in range(K - 1):
        for i in range(N):
            a = k * N + i; b = k * N + (i + 1) % N
            faces.append((a, b, b + N, a + N))
    cx.tpos, cx.tpar, cx.tfaces = pos, par, faces
    cx.tbvh = BVHTree.FromPolygons(pos, faces)
    me = bpy.data.meshes.new("tmp"); me.from_pydata([tuple(p) for p in pos], [], faces)
    bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.recalc_face_normals(bm, faces=bm.faces); bm.to_mesh(me)
    bm.free(); me.update()
    cx.tnorm = [Vector(v.normal) for v in me.vertices]
    bpy.data.meshes.remove(me)


def torso_obj(cx, mat):
    return make_mesh("torso", cx.tpos, cx.tfaces, mat, recalc=True, sub=1)


def tshell(cx, name, wfun, mat, eps=0.002):
    pos = []
    for p, (q, z), n in zip(cx.tpos, cx.tpar, cx.tnorm):
        w, th = wfun(q, z)
        pos.append(p + n * (th * w - eps))
    return make_mesh(name, pos, cx.tfaces, mat, recalc=True, sub=1)


def tnear(cx, p):
    loc, nrm, idx, dist = cx.tbvh.find_nearest(Vector(p))
    if nrm.y > 0.0 and p[1] < 0: nrm = Vector((0, -1, 0))
    return loc, nrm


def tfront(cx, x, z):
    r = cx.tbvh.ray_cast(Vector((x, -0.5, z)), Vector((0, 1, 0)))
    if r[0] is None: return tnear(cx, (x, -0.12, z))
    return r[0], r[1]


def strip(cx, name, guide, widths, mat, off=0.003, solid=0.004, sub=1, front=True, uv=False):
    """Лента по поверхности торса: guide — список точек (x,y,z) около поверхности."""
    pts = []
    for i in range(len(guide) - 1):
        a, b = Vector(guide[i]), Vector(guide[i + 1])
        n = max(1, int((b - a).length / 0.006))
        for k in range(n): pts.append(a.lerp(b, k / n))
    pts.append(Vector(guide[-1]))
    if not isinstance(widths, (list, tuple)): widths = [widths, widths]
    P, NN = [], []
    for p in pts:
        loc, nrm = tfront(cx, p.x, p.z) if front else tnear(cx, p)
        P.append(loc); NN.append(nrm)
    verts, faces, uvs = [], [], []
    m = len(P)
    for i in range(m):
        t = (P[min(i + 1, m - 1)] - P[max(i - 1, 0)])
        if t.length < 1e-9: t = Vector((0, 0, -1))
        t.normalize()
        sd = NN[i].cross(t).normalized()
        w = interp(i / max(1, m - 1), [k / (len(widths) - 1) for k in range(len(widths))], widths) / 2
        base = P[i] + NN[i] * off
        verts += [base - sd * w, base + sd * w]
        uvs += [(i / (m - 1), 0.0), (i / (m - 1), 1.0)]
    for i in range(m - 1):
        faces.append((2 * i, 2 * i + 1, 2 * i + 3, 2 * i + 2))
    return make_mesh(name, verts, faces, mat, recalc=True, solid=solid, sub=sub, uvs=uvs if uv else None)


def neck_build(cx, s):
    fem = s["sex"] == "f"
    r = 0.052 if fem else 0.061
    rings = [ell_ring(z, r * k, (r + 0.002) * k, 0, cy, n=32) for z, k, cy in
             ((-0.05, 1.08, 0.004), (-0.01, 1.0, 0.008), (0.06, 0.97, 0.012), (0.15, 0.95, 0.014))]
    loft("neck", rings, cx.skin, closed=True, sub=1)


def collar_ring(name, z0, h, rx, ry, mat, cy=0.010, gap=0.0, flare=0.0, solid=0.004, n=48, top_in=0.0):
    a0, a1 = gap, 2 * math.pi - gap
    closed = gap == 0.0
    if rx < 0.07:
        rx += NECK_D[0]; ry += NECK_D[0]
    rings = [ell_ring(z0, rx + flare, ry + flare, 0, cy, n=n, a0=a0, a1=a1, closed=closed),
             ell_ring(z0 + h * 0.5, rx + flare * 0.4, ry + flare * 0.4, 0, cy, n=n, a0=a0, a1=a1, closed=closed),
             ell_ring(z0 + h, rx - top_in, ry - top_in, 0, cy, n=n, a0=a0, a1=a1, closed=closed)]
    return loft(name, rings, mat, closed=closed, solid=solid, sub=2)


def button_row(cx, xs_zs, mat, r=0.0055, flat=0.45):
    for x, z in xs_zs:
        p, n = tfront(cx, x, z)
        ob = ellipsoid("button", (0, 0, 0), (r, r * flat, r), mat, segs=12, rings=8)
        place(ob, p + n * 0.003, n)


def gold():
    return mat_basic("#e9b949", rough=0.26, metal=1.0, spec=0.5, tag="gold")


def silver():
    return mat_basic("#d6d8dc", rough=0.22, metal=1.0, tag="silver")


def steel():
    return mat_basic("#a8adb4", rough=0.28, metal=1.0, tag="steel")


def hero_star(cx, x, z, ribbon="#c4202a"):
    p, n = tfront(cx, x, z)
    st = star_mesh("star", 5, 0.0125, 0.0052, 0.0035, cx.gold)
    place(st, p + n * 0.006, n)
    rb = box("ribbon", (0, 0, 0), (0.014, 0.003, 0.0085), mat_basic(ribbon, 0.5, sheen=0.4), bevel=0.0008)
    p2, n2 = tfront(cx, x, z + 0.019)
    place(rb, p2 + n2 * 0.003, n2)


def order_star(cx, x, z, col="#d6d8dc", centre="#c02028"):
    p, n = tfront(cx, x, z)
    st = star_mesh("ostar", 8, 0.021, 0.010, 0.004, silver())
    place(st, p + n * 0.004, n)
    c = ellipsoid("ostar_c", (0, 0, 0), (0.0065, 0.0025, 0.0065), mat_basic(centre, 0.3, coat=1.0), segs=12, rings=8)
    place(c, p + n * 0.007, n)


def cross_mesh(name, arms, w, depth, mat):
    """arms: список прямоугольников (cx, cz, sx, sz) в плоскости XZ, лицом к -Y."""
    verts, faces = [], []
    for (ax, az, sx, sz) in arms:
        o = len(verts)
        for yy in (-depth / 2, depth / 2):
            verts += [(ax - sx, yy, az - sz), (ax + sx, yy, az - sz), (ax + sx, yy, az + sz), (ax - sx, yy, az + sz)]
        faces += [(o, o + 1, o + 2, o + 3), (o + 7, o + 6, o + 5, o + 4), (o, o + 4, o + 5, o + 1),
                  (o + 1, o + 5, o + 6, o + 2), (o + 2, o + 6, o + 7, o + 3), (o + 3, o + 7, o + 4, o)]
    return make_mesh(name, verts, faces, mat, smooth=False, sub=0, recalc=True, bevel=0.0012)


def pendant_chain(cx, z_bottom, mat, spread=0.055):
    pts = []
    for t in (-1.0, -0.6, 0.0, 0.6, 1.0):
        x = t * spread
        z = z_bottom + 0.075 * abs(t) ** 1.6
        p, n = tfront(cx, x, z)
        pts.append(p + n * 0.003)
    curve_tube("chain", pts, [1, 1, 1, 1, 1], 0.0014, mat)


def build_outfit(cx, s):
    o = s["outfit"]; c1, c2, c3 = s["c1"], s["c2"], s["c3"]
    cv = Canvas()
    acc = set(s["acc"])
    fem = s["sex"] == "f"
    cx.gold = gold()
    geo = []   # отложенные построения после создания материала торса

    def V(depth, wtop, col, fn=None):
        cv.poly([(-wtop, 0.04), (wtop, 0.04), (0.0, depth)], col, fn)

    base_rough, base_metal, base_sheen, bump = 0.72, 0.0, 0.3, 0.07
    if o in ("suit", "blazer", "vyshyvanka"):
        shirt = "#f2f0ea" if o != "blazer" else c2
        if o == "vyshyvanka":
            V(-0.24, 0.07, C("#f4f1ea"))
            red, blk = C("#b8222a"), C("#1a1414")

            def orn(x, z):
                u = (x * 160.0) % 1.0; v = (z * 160.0) % 1.0
                d = abs(u - 0.5) + abs(v - 0.5)
                if d < 0.22: return red
                if 0.32 < d < 0.40: return blk if (int(x * 160) + int(z * 160)) % 2 == 0 else red
                return None
            cv.rect(-0.013, -0.20, 0.013, -0.02, None, orn)
            cv.rect(-0.07, -0.035, 0.07, 0.01, None, orn)
        else:
            V(-0.215, 0.052, C(shirt))
            if o == "suit":
                cv.poly([(-0.010, -0.02), (0.010, -0.02), (0.017, -0.19), (0.0, -0.215), (-0.017, -0.19)], C(c3))
        geo.append("lapels")
    elif o == "french" or o == "general":
        cv.line([(0.0, -0.03), (0.0, -0.42)], 0.004, C(c1, 0.0))
        for sd in (-1, 1):
            cv.rect(sd * 0.115, -0.185, sd * 0.055, -0.115, None, lambda x, z: None)
        geo.append("french")
    elif o == "uniform_imp":
        cv.line([(0.0, -0.035), (0.0, -0.42)], 0.005, C("#8a1a1a"))
        geo.append("imperial")
    elif o == "khaki":
        geo.append("khaki")
    elif o == "armour":
        base_rough, base_metal, base_sheen, bump = 0.30, 1.0, 0.0, 0.02
        g = C("#d9a83a")
        cv.line([(-0.07, 0.0), (-0.035, -0.04), (0.035, -0.04), (0.07, 0.0)], 0.008, g)
        for z in (-0.25, -0.30, -0.35):
            cv.line([(-0.2, z + 0.03), (0.0, z), (0.2, z + 0.03)], 0.004, C("#5a5e64"))
        for sd in (-1, 1):
            for z in (-0.08, -0.13, -0.18):
                cv.circle(sd * 0.115, z, 0.004, g)
        geo.append("armour")
    elif o in ("robe", "cassock"):
        cv.line([(0.0, 0.0), (0.0, -0.42)], 0.003, C("#000000", 0.35))
        geo.append("robe")
    elif o == "habit":
        geo.append("habit")
    elif o == "vestments":
        gcol = C(c2)

        def crosses(x, z):
            u = (x * 30.0) % 1.0; v = (z * 30.0 + 0.5 * (int(x * 30.0) % 2)) % 1.0
            if (abs(u - 0.5) < 0.07 and abs(v - 0.5) < 0.30) or (abs(v - 0.55) < 0.07 and abs(u - 0.5) < 0.22):
                return gcol
            return None
        cv.rect(-0.22, -0.42, 0.22, 0.04, None, crosses)
        cv.rect(-0.03, -0.42, 0.03, -0.03, C("#c8a040"))
        cv.rect(-0.022, -0.42, 0.022, -0.03, C("#8a1a1a"))
        geo.append("vestments")
    elif o == "kontusz":
        cv.poly([(-0.035, 0.04), (0.035, 0.04), (0.055, -0.42), (-0.055, -0.42)], C(c2))
        cv.line([(-0.036, 0.0), (-0.056, -0.42)], 0.005, C("#d9a83a"))
        cv.line([(0.036, 0.0), (0.056, -0.42)], 0.005, C("#d9a83a"))
        geo.append("kontusz")
    elif o == "coat18":
        cv.poly([(-0.032, 0.04), (0.032, 0.04), (0.07, -0.42), (-0.07, -0.42)], C(c2))
        cv.line([(-0.033, 0.0), (-0.071, -0.42)], 0.006, C(c3))
        cv.line([(0.033, 0.0), (0.071, -0.42)], 0.006, C(c3))
        geo.append("coat18")
    elif o in ("frock", "frock_high"):
        V(-0.16, 0.05, C(c2))
        cv.poly([(-0.036, -0.06), (0.036, -0.06), (0.0, -0.16)], C("#4a4648") if o == "frock" else C("#5a2a2a"))
        geo.append(o)
    elif o == "eng_uniform":
        geo.append("eng")
    elif o == "tunic12":
        tr = C(c2)
        cv.poly([(-0.06, 0.04), (0.06, 0.04), (0.05, -0.025), (0.0, -0.06), (-0.05, -0.025)], C("#000000", 0.0))
        cv.line([(-0.062, 0.02), (-0.052, -0.022), (0.0, -0.058), (0.052, -0.022), (0.062, 0.02)], 0.014, tr)
        cv.rect(-0.011, -0.42, 0.011, -0.058, tr)

        def dots(x, z):
            if abs(x) < 0.011 and z < -0.06 and ((z * 90.0) % 1.0) < 0.3: return C("#f0e0b0")
            return None
        cv.rect(-0.011, -0.42, 0.011, -0.06, None, dots)
        geo.append("tunic")
    elif o == "caftan":
        cv.line([(0.0, 0.0), (0.0, -0.42)], 0.004, C("#000000", 0.4))
        for z in (-0.05, -0.085, -0.12, -0.155, -0.19, -0.225):
            cv.line([(-0.035, z), (0.035, z)], 0.006, C(c2))
            cv.circle(-0.035, z, 0.005, C(c2)); cv.circle(0.035, z, 0.005, C(c2))
        geo.append("caftan")
    elif o == "caftan_east":
        cv.poly([(-0.06, 0.04), (0.02, 0.04), (0.11, -0.42), (0.05, -0.42)], C("#5a1e2a"))
        cv.line([(0.02, 0.04), (0.11, -0.42)], 0.008, C(c2))
        cv.line([(-0.06, 0.04), (0.05, -0.42)], 0.005, C(c2))
        geo.append("caftan_east")
    elif o == "gown":
        geo.append("gown")
    elif o == "ruff":
        for z in (-0.06, -0.10, -0.14, -0.18, -0.22):
            cv.circle(0.0, z, 0.0045, C("#d9a83a"))
        cv.line([(0.0, -0.03), (0.0, -0.42)], 0.003, C("#3a3a3a"))
        geo.append("ruff")
    elif o == "doublet":
        for z in (-0.06, -0.10, -0.14, -0.18, -0.22):
            cv.circle(0.0, z, 0.0045, C("#d9a83a"))

        def emb(x, z):
            if ((x * 60.0) % 1.0) < 0.12 and abs(x) > 0.02: return C("#5a4a2a")
            return None
        cv.rect(-0.2, -0.42, 0.2, -0.02, None, emb)
        geo.append("doublet")
    elif o == "royal":
        gd = C("#e0b84a")

        def brocade(x, z):
            u = (x * 40.0) % 1.0; v = (z * 40.0) % 1.0
            d = abs(u - 0.5) + abs(v - 0.5)
            if d < 0.18: return C("#8a5a1a")
            if 0.40 < d < 0.47: return C("#8a5a1a")
            return None
        cv.rect(-0.22, -0.42, 0.22, 0.04, None, brocade)
        geo.append("royal")
    elif o == "gown_ermine":
        sk = C(cx.skin_hex)
        cv.poly([(-0.10, 0.04), (0.10, 0.04), (0.105, -0.06), (0.06, -0.085), (0.0, -0.09), (-0.06, -0.085),
                 (-0.105, -0.06)], sk)
        cv.line([(-0.105, -0.06), (-0.06, -0.085), (0.0, -0.092), (0.06, -0.085), (0.105, -0.06)], 0.008,
                C("#c8a040"))

        def lace(x, z):
            if z < -0.095 and ((x * 50.0) % 1.0 < 0.15 or (z * 50.0) % 1.0 < 0.15): return C("#c8c0b0")
            return None
        cv.rect(-0.2, -0.42, 0.2, -0.095, None, lace)
        geo.append("gown_ermine")
    elif o == "sokol":
        blue = C("#3a64b0")
        for sd in (-1, 1):
            cv.line([(sd * 0.10, 0.0), (sd * 0.035, -0.10), (sd * 0.03, -0.42)], 0.010, blue)
        cv.rect(-0.03, -0.42, 0.03, -0.10, C("#d0d4d8"))
        geo.append("sokol")
    elif o == "flightsuit":
        cv.line([(0.0, -0.02), (0.0, -0.42)], 0.005, C("#c0c6cc"))
        cv.rect(-0.13, -0.135, -0.055, -0.11, C("#e8e8e8"))
        geo.append("flightsuit")
    elif o == "casual":
        cv.poly([(-0.06, 0.04), (0.06, 0.04), (0.055, -0.42), (-0.055, -0.42)], C(c2))
        cv.line([(-0.058, 0.0), (-0.056, -0.42)], 0.004, C("#9a9a9a"))
        cv.line([(0.058, 0.0), (0.056, -0.42)], 0.004, C("#9a9a9a"))
        geo.append("casual")
    elif o == "fur_coat":
        cv.line([(0.02, 0.0), (0.03, -0.42)], 0.004, C("#000000", 0.4))
        geo.append("fur_coat")

    # нашивки
    if "patch_cccp" in acc:
        cv.rect(-0.135, -0.10, -0.07, -0.065, C("#c41e24"))
        cv.rect(-0.128, -0.093, -0.077, -0.072, C("#e8c050"))
        cv.rect(-0.125, -0.090, -0.080, -0.075, C("#c41e24"))
    if "patch_round" in acc:
        cv.circle(-0.10, -0.085, 0.024, C("#1e3a7a")); cv.circle(-0.10, -0.085, 0.017, C("#e8e8e8"))
        cv.circle(-0.10, -0.085, 0.009, C("#3a64b0"))
    if "flag_by" in acc:
        cv.rect(-0.135, -0.075, -0.07, -0.053, C("#c8313e")); cv.rect(-0.135, -0.087, -0.07, -0.075, C("#4aa657"))
        cv.rect(-0.135, -0.087, -0.127, -0.053, C("#f4f2ee"))
    if o == "general":
        cols = ["#c41e24", "#e8c050", "#2a6ab0", "#c41e24", "#f0f0f0", "#3a8a3a", "#e8c050", "#c41e24", "#2a6ab0"]
        for k, col in enumerate(cols):
            r_, c_ = divmod(k, 3)
            cv.rect(0.065 + c_ * 0.017, -0.085 - r_ * 0.009, 0.08 + c_ * 0.017, -0.078 - r_ * 0.009, C(col))
    img = cv.image("decal_%s" % s["id"])
    cx.decal = img
    if o == "armour":
        tm = mat_decal("#a9aeb6", img, rough=0.28, metal=1.0, spec=0.5, sheen=0.0, bump=0.02, tag="armour")
    else:
        tm = mat_decal(c1, img, rough=base_rough, metal=base_metal, sheen=base_sheen, bump=bump)
    cx.tmat = tm
    torso_obj(cx, tm)
    neck_build(cx, s)
    G = cx.gold
    jacket_dark = mat_decal(mulc(srgb(c1), 0.82), img, rough=0.7, sheen=0.3)
    for g in geo:
        if g == "lapels":
            white = mat_basic(s["c2"] if o == "blazer" else "#f2f0ea", rough=0.6, sheen=0.2)
            for sd in (-1, 1):
                strip(cx, "lapel", [(sd * 0.060, -0.1, -0.012), (sd * 0.050, -0.1, -0.06), (sd * 0.030, -0.1, -0.13),
                                    (sd * 0.008, -0.1, -0.205)], [0.040, 0.036, 0.030, 0.012], jacket_dark,
                      off=0.003, solid=0.004)
            if o != "vyshyvanka":
                collar_ring("shirt_collar", -0.035, 0.040, 0.056, 0.058, white, gap=0.42 if o == "suit" else 0.7,
                            flare=0.006)
                for sd in (-1, 1):
                    strip(cx, "collar_pt", [(sd * 0.012, -0.1, -0.008), (sd * 0.030, -0.1, -0.040)],
                          [0.024, 0.008], white, off=0.004, solid=0.003)
            else:
                emb = mat_decal("#f4f1ea", img, rough=0.6)
                collar_ring("vy_collar", -0.035, 0.038, 0.055, 0.057, emb, gap=0.25)
            if o == "suit":
                tie = mat_basic(s["c3"], rough=0.45, sheen=0.5, bump=((300.0, 300.0, 300.0), 0.05))
                ellipsoid("tie_knot", (0, -0.060, -0.030), (0.010, 0.007, 0.011), tie)
                strip(cx, "tie", [(0, -0.1, -0.040), (0, -0.1, -0.20)], [0.018, 0.034], tie, off=0.006, solid=0.004)
            collar_ring("jacket_collar", -0.03, 0.035, 0.064, 0.064, jacket_dark, gap=1.1, flare=0.010)
            if "hero1" in acc or "hero2" in acc or any(a.startswith("hero_n") for a in acc):
                pass
        elif g == "french":
            collar_ring("collar", -0.035, 0.045, 0.057, 0.059, tm, gap=0.12, flare=0.004)
            darker = mat_decal(mulc(srgb(c1), 0.85), img, rough=0.7)
            for sd in (-1, 1):
                strip(cx, "flap", [(sd * 0.115, -0.1, -0.100), (sd * 0.050, -0.1, -0.105)], [0.026, 0.026], darker,
                      off=0.003, solid=0.003)
                strip(cx, "pocket", [(sd * 0.112, -0.1, -0.118), (sd * 0.052, -0.1, -0.12)], [0.004, 0.004], darker,
                      off=0.002, solid=0.002)
            button_row(cx, [(0.0, z) for z in (-0.05, -0.10, -0.15, -0.20, -0.25)],
                       mat_basic(mulc(srgb(c1), 0.8), 0.4) if o == "french" else G, r=0.0050)
            button_row(cx, [(-0.083, -0.108), (0.083, -0.108)], mat_basic(mulc(srgb(c1), 0.8), 0.4), r=0.0045)
            if o == "general":
                for sd in (-1, 1):
                    p, n = tnear(cx, (sd * 0.13, 0.0, 0.0))
                    ob = box("pogon", (0, 0, 0), (0.07, 0.035, 0.005), G, bevel=0.003)
                    place(ob, p + n * 0.004, n, spin=0.0)
                    ob.matrix_basis = Matrix.Translation(p + n * 0.004) @ Vector((0, 0, 1)).rotation_difference(n).to_matrix().to_4x4() @ Matrix.Rotation(sd * 0.25, 4, "Z")
                collar_ring("collar_tab", -0.03, 0.034, 0.059, 0.061, mat_basic("#8a1a1a", 0.6), gap=0.5, flare=0.006, solid=0.002)
        elif g in ("imperial", "khaki", "eng"):
            col_mat = (mat_basic("#8a1a1a", 0.55, sheen=0.4) if g == "imperial" else
                       (mat_basic("#141414", 0.6, sheen=0.6) if g == "eng" else tm))
            collar_ring("collar", -0.035, 0.050, 0.057, 0.059, col_mat, gap=0.10, flare=0.004)
            if g == "imperial":
                collar_ring("collar_gold", -0.004, 0.012, 0.062, 0.064, G, gap=0.10, flare=0.0, solid=0.002)
                button_row(cx, [(sd * 0.045, z) for sd in (-1, 1) for z in (-0.06, -0.11, -0.16, -0.21, -0.26)], G,
                           r=0.0058)
            elif g == "khaki":
                button_row(cx, [(0.0, z) for z in (-0.05, -0.09, -0.13)], mat_basic(mulc(srgb(c1), 0.75), 0.4),
                           r=0.0045)
                for sd in (-1, 1):
                    strip(cx, "flap", [(sd * 0.115, -0.1, -0.100), (sd * 0.050, -0.1, -0.105)], [0.024, 0.024],
                          jacket_dark, off=0.003, solid=0.003)
            else:
                button_row(cx, [(0.0, z) for z in (-0.05, -0.10, -0.15, -0.20, -0.25)], G, r=0.0055)
            if g in ("khaki", "eng") or "epaulettes" not in acc:
                for sd in (-1, 1):
                    p, n = tnear(cx, (sd * 0.135, 0.0, 0.0))
                    ob = box("pogon", (0, 0, 0), (0.075, 0.032, 0.005),
                             mat_basic("#c8b070", 0.35, metal=0.6) if g != "eng" else G, bevel=0.003)
                    ob.matrix_basis = (Matrix.Translation(p + n * 0.004) @
                                       Vector((0, 0, 1)).rotation_difference(n).to_matrix().to_4x4() @
                                       Matrix.Rotation(sd * 0.22, 4, "Z"))
        elif g == "armour":
            st = steel()
            rings = [ell_ring(z, rx, ry, 0, cy, n=48) for z, rx, ry, cy in
                     ((-0.050, 0.118, 0.088, 0.0), (-0.025, 0.088, 0.072, 0.004), (0.000, 0.064, 0.064, 0.008),
                      (0.030, 0.058, 0.060, 0.010))]
            loft("gorget", rings, st, closed=True, solid=0.004, sub=2)
            collar_ring("gorget_trim", -0.054, 0.008, 0.119, 0.089, G, cy=0.0, solid=0.003)
            for sd in (-1, 1):
                for k, (zc, rr, out) in enumerate(((-0.060, (0.080, 0.092, 0.058), 0.0), (-0.098, (0.080, 0.094, 0.050), 0.012))):
                    rot = Matrix.Rotation(sd * 0.55, 3, "Y")
                    ellipsoid("pauldron", (sd * (0.168 + out), 0.0, zc), rr, st, rot=rot, segs=28, rings=16,
                              deform=lambda u: (u[0], u[1], max(u[2], -0.15)))
                ellipsoid("rivet", (sd * 0.205, -0.055, -0.07), (0.006, 0.006, 0.006), G, segs=10, rings=8)
            strip(cx, "ridge", [(0, -0.1, -0.045), (0, -0.1, -0.40)], [0.010, 0.010], st, off=0.002, solid=0.004)
        elif g == "robe":
            collar_ring("collar", -0.035, 0.032, 0.056, 0.058, tm, gap=0.12 if o == "cassock" else 0.0,
                        flare=0.006)
            if o == "cassock":
                button_row(cx, [(0.0, z) for z in (-0.03, -0.06, -0.09, -0.12, -0.15, -0.18, -0.21, -0.24)],
                           mat_basic("#0e0e10", 0.3), r=0.0042)
        elif g == "gown":
            collar_ring("collar", -0.035, 0.030, 0.056, 0.058, tm, gap=0.0, flare=0.006)
        elif g == "tunic":
            collar_ring("neck_trim", -0.03, 0.014, 0.060, 0.062, mat_basic(s["c2"], 0.5, sheen=0.3), flare=0.004)
        elif g == "kontusz":
            zup = mat_decal(s["c2"], img, rough=0.6)
            collar_ring("zupan_collar", -0.035, 0.040, 0.056, 0.058, zup, gap=0.12, flare=0.004)
            button_row(cx, [(0.0, z) for z in (-0.045, -0.075, -0.105, -0.135, -0.165, -0.195, -0.225)], G, r=0.0048)
            for sd in (-1, 1):
                strip(cx, "kontusz_edge", [(sd * 0.044, -0.1, -0.02), (sd * 0.050, -0.1, -0.2), (sd * 0.056, -0.1, -0.40)],
                      [0.012, 0.012], jacket_dark, off=0.003, solid=0.004)
        elif g == "coat18":
            white = mat_basic("#f6f4ee", rough=0.6, sheen=0.4)
            collar_ring("stock", -0.035, 0.040, 0.054, 0.056, white, flare=0.003)
            collar_ring("coat_collar", -0.035, 0.035, 0.066, 0.066, jacket_dark, gap=0.9, flare=0.012)
            button_row(cx, [(sd * 0.05, z) for sd in (-1, 1) for z in (-0.06, -0.11, -0.16, -0.21)], G, r=0.0052)
        elif g in ("frock", "frock_high"):
            white = mat_basic("#f6f4ee", rough=0.6, sheen=0.3)
            for sd in (-1, 1):
                strip(cx, "lapel", [(sd * 0.058, -0.1, -0.015), (sd * 0.048, -0.1, -0.08), (sd * 0.012, -0.1, -0.16)],
                      [0.036, 0.032, 0.014], jacket_dark, off=0.003, solid=0.004)
            if g == "frock":
                collar_ring("shirt_collar", -0.035, 0.05, 0.054, 0.056, white, gap=0.3, flare=0.004)
                collar_ring("cravat", -0.035, 0.026, 0.057, 0.059, mat_basic(s["c3"], 0.5, sheen=0.4), flare=0.004)
                ellipsoid("cravat_knot", (0, -0.062, -0.030), (0.016, 0.008, 0.010), mat_basic(s["c3"], 0.5, sheen=0.4))
            else:
                collar_ring("shirt_collar", -0.035, 0.065, 0.054, 0.056, white, gap=0.35, flare=0.004)
                collar_ring("cravat", -0.035, 0.034, 0.058, 0.060, white, flare=0.006)
                ellipsoid("cravat_knot", (0, -0.064, -0.035), (0.020, 0.011, 0.016), white)
                rings = [ell_ring(z, rx, ry, 0, 0.012, n=40, a0=1.0, a1=2 * math.pi - 1.0, closed=False) for z, rx, ry in
                         ((-0.045, 0.085, 0.080), (0.0, 0.070, 0.070), (0.05, 0.068, 0.068))]
                loft("high_collar", rings, jacket_dark, closed=False, solid=0.004, sub=2)
        elif g == "tunic12":
            pass
        elif g == "caftan":
            collar_ring("collar", -0.035, 0.040, 0.057, 0.059, tm, gap=0.12, flare=0.004)
        elif g == "caftan_east":
            collar_ring("collar", -0.035, 0.035, 0.057, 0.059, mat_basic(s["c2"], 0.4, metal=0.6), gap=0.0, flare=0.004)
        elif g == "ruff":
            wh = mat_basic("#f8f6f0", rough=0.55, sheen=0.4)
            rings = []
            for z, r0, k in ((-0.020, 0.058, 0.0), (-0.034, 0.105, 1.0), (-0.010, 0.128, 1.0), (0.020, 0.112, 1.0),
                             (0.030, 0.062, 0.0)):
                ring = []
                for i in range(160):
                    a = 2 * math.pi * i / 160
                    rr = r0 * (1 + 0.06 * k * math.sin(a * 30))
                    ring.append(Vector((rr * math.sin(a) * 1.05, 0.012 - rr * math.cos(a), z)))
                rings.append(ring)
            rings.append(rings[0])
            loft("ruff", rings, wh, closed=True, sub=1)
        elif g == "doublet":
            collar_ring("collar", -0.035, 0.040, 0.056, 0.058, tm, gap=0.10, flare=0.004)
            collar_ring("collar_w", 0.000, 0.012, 0.060, 0.062, mat_basic("#f6f4ee", 0.6), gap=0.10, flare=0.0, solid=0.002)
        elif g == "royal":
            pass
        elif g == "gown_ermine":
            pass
        elif g == "sokol":
            rings = [ell_ring(z, rx, ry, 0, 0.010, n=48) for z, rx, ry in
                     ((-0.042, 0.090, 0.085), (-0.032, 0.094, 0.088), (-0.012, 0.090, 0.085), (-0.004, 0.080, 0.078))]
            loft("neck_ring", rings, mat_basic("#3a64b0", 0.35, metal=0.3), closed=True, sub=2)
            collar_ring("neck_ring_m", -0.006, 0.010, 0.078, 0.076, mat_basic("#c8ccd2", 0.25, metal=1.0), solid=0.004)
            for sd in (-1, 1):
                p, n = tfront(cx, sd * 0.06, -0.20)
                c = cylinder("hose", p + n * 0.002, p + n * 0.020, 0.010, 0.010, mat_basic("#3a64b0", 0.35, metal=0.3))
                cylinder("hose_m", p + n * 0.020, p + n * 0.024, 0.011, 0.011, mat_basic("#c8ccd2", 0.25, metal=1.0))
        elif g == "flightsuit":
            for sd in (-1, 1):
                strip(cx, "collar", [(sd * 0.012, -0.1, -0.012), (sd * 0.050, -0.1, -0.050)], [0.030, 0.012],
                      jacket_dark, off=0.004, solid=0.003)
            collar_ring("collar_b", -0.035, 0.030, 0.058, 0.060, jacket_dark, gap=0.35, flare=0.008)
        elif g == "casual":
            tsh = mat_basic(s["c2"], 0.8)
            collar_ring("tshirt", -0.035, 0.012, 0.054, 0.056, tsh, flare=0.003)
            for sd in (-1, 1):
                strip(cx, "jk_edge", [(sd * 0.064, -0.1, -0.005), (sd * 0.060, -0.1, -0.2), (sd * 0.057, -0.1, -0.4)],
                      [0.020, 0.016, 0.016], jacket_dark, off=0.003, solid=0.004)
            rings = [ell_ring(z, rx, ry, 0, 0.012, n=40, a0=0.9, a1=2 * math.pi - 0.9, closed=False) for z, rx, ry in
                     ((-0.04, 0.080, 0.078), (-0.01, 0.068, 0.068), (0.02, 0.064, 0.066))]
            loft("jk_collar", rings, jacket_dark, closed=False, solid=0.005, sub=2)
        elif g == "fur_coat":
            pass
        elif g == "vestments" or g == "habit":
            collar_ring("collar", -0.035, 0.03, 0.056, 0.058, tm, flare=0.006)
    # плащи, меха, воротники
    if "mantle" in acc or o in ("royal", "gown_ermine"):
        mc = s["extra"].get("mantle", s["c4"])
        if o == "gown_ermine": mc = "#c8a040"
        mm = mat_basic(mc, rough=0.6, sheen=0.6, bump=((300.0, 300.0, 300.0), 0.04),
                       metal=0.5 if o == "gown_ermine" else 0.0)
        qo = 0.42 if o in ("royal", "gown_ermine") else 0.62

        def mw(q, z):
            op = qo + 0.6 * sstep(-0.02, -0.3, z)
            return sstep(op - 0.06, op + 0.06, abs(q)) * sstep(-0.006, -0.025, z), 0.016
        tshell(cx, "mantle", mw, mm)
        if o not in ("royal", "gown_ermine"):
            for sd in (-1, 1):
                ellipsoid("clasp", (sd * 0.105, -0.075, -0.035), (0.012, 0.006, 0.012), G, segs=14, rings=10)
        if o in ("royal", "gown_ermine"):
            er = mat_ermine()

            def ew(q, z):
                op = qo + 0.6 * sstep(-0.02, -0.3, z)
                edge = sstep(op - 0.05, op + 0.02, abs(q)) * sstep(op + 0.20, op + 0.14, abs(q))
                cap = sstep(-0.115, -0.085, z) * sstep(qo - 0.08, qo, abs(q))
                return max(edge, cap) * sstep(-0.004, -0.02, z), 0.027
            tshell(cx, "ermine", ew, er)
    if "fur_collar" in acc or o == "fur_coat":
        fm = mat_fur(s["fur"])

        def fw(q, z):
            return sstep(-0.10, -0.075, z) * sstep(0.22, 0.34, abs(q)) * sstep(-0.004, -0.018, z), 0.030
        tshell(cx, "fur_collar", fw, fm)
    if "white_collar" in acc:
        wm = mat_basic("#f6f4ee", 0.6, sheen=0.3)

        def ww(q, z):
            return sstep(-0.075, -0.06, z) * sstep(0.05, 0.1, abs(q)) * sstep(-0.004, -0.014, z), 0.005
        tshell(cx, "white_collar", ww, wm)
    if "apron" in acc:
        lm = mat_basic("#6a4628", rough=0.55, bump=((200.0, 200.0, 200.0), 0.12), var=((60.0, 60.0, 60.0), 0.85, 1.1))
        strip(cx, "apron", [(0, -0.1, -0.075), (0, -0.1, -0.42)], [0.15, 0.17], lm, off=0.004, solid=0.004)
        for sd in (-1, 1):
            strip(cx, "strap", [(sd * 0.068, -0.1, -0.08), (sd * 0.05, -0.1, -0.03), (sd * 0.05, 0.0, -0.005)],
                  [0.012, 0.012, 0.012], lm, off=0.004, solid=0.003, front=False)
    if "epaulettes" in acc:
        for sd in (-1, 1):
            p, n = tnear(cx, (sd * 0.15, 0.0, 0.0))
            rot = Vector((0, 0, 1)).rotation_difference(n).to_matrix()
            ob = ellipsoid("epaulette", (0, 0, 0), (0.050, 0.036, 0.007), G, segs=20, rings=10)
            ob.matrix_basis = Matrix.Translation(p + n * 0.006) @ rot.to_4x4()
            pe = p + Vector((sd * 0.035, 0, -0.012))
            rings = [ell_ring(z, 0.012, 0.034, sd * 0.192, pe.y, n=24) for z in (pe.z + 0.008, pe.z - 0.025)]
            fr = mat_basic("#e0b040", rough=0.35, metal=1.0, bump=((900.0, 900.0, 30.0), 0.8))
            loft("fringe", rings, fr, closed=True, sub=1, cap_bot=True)
    # ленты, звёзды, кресты
    if "sash_blue" in acc or "sash_blue_l" in acc or "sash_red" in acc:
        col = "#2a5ec0" if "sash_red" not in acc else "#b8202a"
        sm = mat_basic(col, rough=0.35, sheen=0.7, spec=0.6, bump=((40.0, 900.0, 900.0), 0.08))
        sd = -1 if ("sash_blue" in acc or "sash_red" in acc) else 1
        guide = [(sd * 0.12, 0.04, -0.02), (sd * 0.10, -0.08, -0.05), (sd * 0.02, -0.12, -0.15),
                 (-sd * 0.08, -0.12, -0.27), (-sd * 0.16, -0.10, -0.40)]
        strip(cx, "sash", guide, [0.055, 0.058, 0.06, 0.06, 0.06], sm, off=0.007, solid=0.004, front=False)
    hn = 0
    for a in acc:
        if a.startswith("hero_n"): hn = int(a[6:])
    if "hero1" in acc: hn = 1
    if "hero2" in acc: hn = 2
    if hn:
        for k in range(hn):
            r_, c_ = divmod(k, 3)
            hero_star(cx, 0.072 + 0.026 * c_ + 0.012 * r_, -0.072 - 0.040 * r_)
    if "order_star" in acc:
        order_star(cx, 0.095, -0.165 if o != "coat18" else -0.15)
    if "maltese" in acc:
        p, n = tfront(cx, 0.0, -0.105)
        wm = mat_basic("#f6f4f0", 0.25, coat=1.0)
        verts, faces = [], []
        for k in range(4):
            a = math.pi / 4 + k * math.pi / 2
            o_ = len(verts)
            for (rr, da) in ((0.003, 0.0), (0.030, -0.30), (0.022, 0.0), (0.030, 0.30)):
                verts.append((rr * math.cos(a + da), 0.0, rr * math.sin(a + da)))
            faces.append((o_, o_ + 1, o_ + 2, o_ + 3))
        ob = make_mesh("maltese", verts, faces, wm, smooth=False, sub=0, solid=0.004)
        place(ob, p + n * 0.006, n)
        ob2 = make_mesh("maltese_g", [(v[0] * 1.12, 0.002, v[2] * 1.12) for v in verts], faces, G, smooth=False,
                        sub=0, solid=0.003)
        place(ob2, p + n * 0.005, n)
    if "george" in acc:
        p, n = tfront(cx, 0.085, -0.10)
        ob = cross_mesh("george", [(0, 0, 0.004, 0.012), (0, 0, 0.012, 0.004)], 0, 0.003,
                        mat_basic("#f4f2ee", 0.25, coat=1.0))
        place(ob, p + n * 0.005, n)
        rb = box("g_ribbon", (0, 0, 0), (0.014, 0.003, 0.010), mat_basic("#e08020", 0.5), bevel=0.0008)
        p2, n2 = tfront(cx, 0.085, -0.08)
        place(rb, p2 + n2 * 0.003, n2)
    if "red_bow" in acc:
        p, n = tfront(cx, 0.072, -0.085)
        rm = mat_basic("#c41e24", 0.45, sheen=0.6)
        for sd in (-1, 1):
            ob = ellipsoid("bow", (sd * 0.009, 0, 0.002), (0.010, 0.004, 0.0065), rm, segs=14, rings=10,
                           rot=Matrix.Rotation(sd * 0.3, 3, "Y"))
            place(ob, p + n * 0.008, n)
            ob.matrix_basis = ob.matrix_basis @ Matrix.Translation((sd * 0.009, 0, 0.002))
        ob = ellipsoid("bow_tail", (0, 0, -0.012), (0.004, 0.003, 0.012), rm, segs=10, rings=8)
        place(ob, p + n * 0.007, n)
    if "cross_gold" in acc or "cross6" in acc or "panagia" in acc:
        zc = -0.105
        pendant_chain(cx, zc + 0.035, G)
        p, n = tfront(cx, 0.0, zc)
        if "cross6" in acc:
            arms = [(0, 0, 0.0055, 0.048), (0, 0.020, 0.024, 0.0055), (0, -0.006, 0.034, 0.0055)]
            ob = cross_mesh("cross6", arms, 0, 0.004, G)
            if s["hat"] == "nun":
                # поверх белого апостольника, по центру груди
                p = Vector((0.0, min(p.y, -0.112) - 0.004, -0.090)); n = Vector((0, -1, 0.25)).normalized()
                p, n = p - n * 0.014 + Vector((0, 0, 0.01)), n
            place(ob, p + n * 0.014 + Vector((0, 0, -0.01)), n)
            gm = [mat_basic(c_, 0.15, coat=1.0) for c_ in ("#c01828", "#1e6ac0", "#18a050")]
            for (gx, gz, gi) in ((0, -0.006, 0), (0, 0.020, 1), (0, 0.038, 2), (-0.028, -0.006, 2),
                                 (0.028, -0.006, 2), (-0.019, 0.020, 0), (0.019, 0.020, 0), (0, -0.040, 1)):
                e = ellipsoid("gem", (0, 0, 0), (0.0042, 0.0025, 0.0042), gm[gi], segs=10, rings=8)
                place(e, p + n * 0.014 + Vector((0, 0, -0.01)), n)
                e.matrix_basis = e.matrix_basis @ Matrix.Translation((gx, -0.003, gz))
        elif "panagia" in acc:
            ob = ellipsoid("panagia", (0, 0, 0), (0.020, 0.005, 0.027), G, segs=20, rings=12)
            place(ob, p + n * 0.012, n)
            e = ellipsoid("pan_c", (0, 0, 0), (0.013, 0.004, 0.019), mat_basic("#1e4a9a", 0.2, coat=1.0), segs=16, rings=10)
            place(e, p + n * 0.015, n)
        else:
            arms = [(0, 0, 0.004, 0.030), (0, 0.012, 0.018, 0.004)]
            ob = cross_mesh("cross", arms, 0, 0.004, G)
            place(ob, p + n * 0.012, n)
    if "chain_gold" in acc or "fleece" in acc:
        pts = []
        for t in [i / 10 for i in range(11)]:
            a = 0.9 + (2 * math.pi - 1.8) * 0
            x = (t - 0.5) * 0.20
            z = -0.035 - 0.07 * (1 - (2 * t - 1) ** 2)
            p, n = tfront(cx, x, z)
            pts.append(p + n * 0.006)
        curve_tube("gold_chain", pts, [1] * 11, 0.0035, G)
        if "fleece" in acc:
            ob = ellipsoid("fleece", (0, 0, 0), (0.016, 0.006, 0.008), G, segs=14, rings=8)
            p, n = tfront(cx, 0.0, -0.12)
            place(ob, p + n * 0.012, n)


# ----------------------------------------------------------------------------------------------
#  Головные уборы (в координатах головы)
# ----------------------------------------------------------------------------------------------
def band(cx, zc=0.034, tilt=-0.18, n=64, extra=0.0):
    up = Matrix.Rotation(tilt, 3, "X") @ Vector((0, 0, 1))
    rot = Matrix.Rotation(tilt, 3, "X")
    c = Vector((0, 0.006, zc))
    pts = []
    bv = cx.outer_bvh or cx.hbvh
    for i in range(n):
        a = 2 * math.pi * i / n
        d = rot @ Vector((math.sin(a), -math.cos(a), 0))
        r = bv.ray_cast(c + d * 0.4, -d)
        r2 = cx.hbvh.ray_cast(c + d * 0.4, -d)
        p = r[0] if r[0] else (r2[0] if r2[0] else c + d * 0.09)
        if r2[0] and (r2[0] - c).length > (p - c).length: p = r2[0]
        pts.append(p + d * extra)
    return c, up, pts


def hat_loft(cx, name, B, prof, mat, cap=True, solid=0.0, sub=2, arc=None, squash=None):
    c, up, pts = B
    rings = []
    n = len(pts)
    for dz, sc in prof:
        ring = []
        for i, p in enumerate(pts):
            v = p - c
            if squash: v = squash(v)
            ring.append(c + v * sc + up * dz)
        rings.append(ring)
    return loft(name, rings, mat, closed=True, cap_top=cap, head=True, solid=solid, sub=sub)


def build_hat(cx, s):
    h = s["hat"]
    if not h: return
    G = cx.gold if hasattr(cx, "gold") else gold()
    if h == "nun":
        build_nun(cx, s); return
    B = band(cx)
    c, up, pts = B
    jewels = [mat_basic(cc, 0.12, coat=1.0, spec=0.8) for cc in ("#c0182a", "#1e5ac8", "#18a050")]
    if h in ("princely", "crown_fur"):
        top = mat_basic(s["c4"], rough=0.55, sheen=0.8)
        hat_loft(cx, "cap", B, [(0.0, 1.0), (0.03, 1.03), (0.065, 0.97), (0.095, 0.80), (0.118, 0.52), (0.13, 0.15)],
                 top if h == "princely" else top)
        fm = mat_fur(s["fur"] if s["id"] != "vseslav" else "#4a3a2c")
        hat_loft(cx, "fur_band", B, [(-0.010, 1.0), (-0.004, 1.10), (0.016, 1.17), (0.036, 1.13), (0.046, 1.02)], fm,
                 cap=False)
        if h == "princely":
            for k in range(-2, 3):
                a = k * 0.45
                d = Vector((math.sin(a), -math.cos(a), 0))
                ellipsoid("pearl", c + d * 0.072 + up * 0.075, (0.006, 0.006, 0.006),
                          mat_basic("#f4efe4", 0.2, coat=1.0), head=True, segs=10, rings=8)
            ellipsoid("knob", c + up * 0.135, (0.012, 0.012, 0.012), G, head=True)
            cross = cross_mesh("hat_cross", [(0, 0, 0.003, 0.016), (0, 0.004, 0.011, 0.003)], 0, 0.003, G)
            cross.matrix_basis = HEAD_M @ Matrix.Translation(c + up * 0.158)
        else:
            hat_loft(cx, "crown_band", B, [(0.035, 1.12), (0.065, 1.10)], G, cap=False, solid=0.003)
            for k in range(8):
                a = 2 * math.pi * k / 8
                v = pts[int(k * len(pts) / 8)] - c
                q = c + v * 1.11 + up * 0.08
                ellipsoid("fleur", q, (0.008, 0.008, 0.016), G, head=True, segs=12, rings=8)
                ellipsoid("fleur_b", q + up * 0.016, (0.0055, 0.0055, 0.0055), G, head=True, segs=10, rings=8)
            for k in range(-1, 2):
                v = pts[(k * 4) % len(pts)] - c
                ellipsoid("jewel", c + v * 1.135 + up * 0.050, (0.006, 0.006, 0.006), jewels[k + 1], head=True,
                          segs=10, rings=8)
    elif h in ("crown_med", "crown_closed", "helmet_crown"):
        if h == "helmet_crown":
            st = steel()
            hat_loft(cx, "helmet", B, [(-0.008, 1.03), (0.02, 1.04), (0.055, 0.96), (0.09, 0.78), (0.12, 0.50),
                                       (0.14, 0.18), (0.148, 0.0)], st)
            ellipsoid("spike", c + up * 0.16, (0.006, 0.006, 0.02), st, head=True)
            zb, sc0 = -0.004, 1.07
        else:
            if h == "crown_closed":
                hat_loft(cx, "velvet", B, [(0.0, 1.0), (0.04, 1.0), (0.08, 0.80), (0.10, 0.4), (0.105, 0.0)],
                         mat_basic(s["c4"], 0.55, sheen=0.8))
            zb, sc0 = 0.0, 1.03
        hat_loft(cx, "crown_band", B, [(zb, sc0), (zb + 0.034, sc0 + 0.02)], G, cap=False, solid=0.004)
        npts = len(pts)
        for k in range(10):
            v = pts[int(k * npts / 10)] - c
            q = c + v * (sc0 + 0.025) + up * (zb + 0.050)
            ellipsoid("fleur", q, (0.009, 0.006, 0.020), G, head=True, segs=12, rings=8,
                      deform=lambda u: (u[0] * (1 - 0.6 * max(0, u[2])), u[1], u[2]))
            ellipsoid("fleur_b", q + up * 0.022, (0.0055, 0.0055, 0.0055), G, head=True, segs=10, rings=8)
        for k in range(-2, 3):
            v = pts[(k * 3) % npts] - c
            ellipsoid("jewel", c + v * (sc0 + 0.03) + up * (zb + 0.017), (0.0055, 0.004, 0.0065),
                      jewels[(k + 2) % 3], head=True, segs=10, rings=8)
        if h == "crown_closed":
            for ang in (0.0, math.pi / 2):
                ap = []
                for t in [i / 8 for i in range(9)]:
                    a = math.pi * t
                    d = Vector((math.cos(a) * math.sin(ang), -math.cos(a) * math.cos(ang), 0))
                    ap.append(c + d * 0.072 * math.sin(math.pi * t * 0 + 1.0) * (1.0 if 0 < t < 1 else 1.0) * 1.05 +
                              up * (0.04 + 0.08 * math.sin(a)))
                curve_tube("arch", ap, [1] * 9, 0.004, G, head=True)
            ellipsoid("orb", c + up * 0.128, (0.012, 0.012, 0.012), G, head=True)
            cross = cross_mesh("crown_cross", [(0, 0, 0.003, 0.014), (0, 0.004, 0.010, 0.003)], 0, 0.003, G)
            cross.matrix_basis = HEAD_M @ Matrix.Translation(c + up * 0.152)
    elif h in ("fur_cap", "kolpak"):
        fm = mat_fur(s["fur"])
        if h == "fur_cap":
            hat_loft(cx, "fur_cap", B, [(-0.008, 1.0), (-0.002, 1.09), (0.04, 1.13), (0.075, 1.10), (0.095, 0.88),
                                        (0.104, 0.45), (0.106, 0.0)], fm)
            if s["id"] in ("algirdas", "mamonichi"):
                hat_loft(cx, "cap_top", B, [(0.07, 1.0), (0.10, 0.92), (0.118, 0.6), (0.124, 0.0)],
                         mat_basic(s["c4"], 0.55, sheen=0.8))
        else:
            hat_loft(cx, "kolpak_top", B, [(0.04, 1.02), (0.085, 1.04), (0.11, 0.92), (0.125, 0.55), (0.13, 0.0)],
                     mat_basic(s["c4"], 0.55, sheen=0.8))
            hat_loft(cx, "kolpak_fur", B, [(-0.008, 1.0), (-0.002, 1.10), (0.05, 1.15), (0.072, 1.10), (0.078, 1.0)],
                     fm, cap=False)
            v = pts[0] - c
            q = c + v * 1.16 + up * 0.04
            ellipsoid("clasp", q, (0.014, 0.007, 0.018), G, head=True)
            ellipsoid("clasp_g", q + Vector((0, -0.006, 0)), (0.007, 0.004, 0.009), jewels[0], head=True, segs=12,
                      rings=8)
            fc = s["extra"].get("feather", "#f4f2ee")
            if fc:
                fpts = [q + Vector((0, 0.002, 0.012)), q + Vector((0.006, 0.010, 0.050)),
                        q + Vector((0.012, 0.030, 0.090)), q + Vector((0.016, 0.065, 0.112))]
                fob = curve_tube("feather", fpts, [0.3, 1.0, 0.75, 0.15], 0.010,
                                 mat_basic(fc, 0.6, sheen=0.8, bump=((300.0, 300.0, 30.0), 0.4)), head=True)
    elif h in ("beret", "hat_black"):
        bm = mat_basic("#141214", rough=0.7, sheen=0.9, bump=((300.0, 300.0, 300.0), 0.05))
        B2 = band(cx, zc=0.050, tilt=-0.22)
        hat_loft(cx, "beret", B2, [(-0.006, 1.0), (0.008, 1.10), (0.026, 1.34), (0.040, 1.33), (0.050, 1.06),
                                   (0.056, 0.55), (0.058, 0.0)], bm)
        if h == "hat_black":
            c2, up2, pts2 = B2
            hat_loft(cx, "hat_band", B2, [(0.0, 1.03), (0.012, 1.08)], G, cap=False, solid=0.003)
            q = c2 + (pts2[6] - c2) * 1.1 + up2 * 0.012
            ellipsoid("hat_jewel", q, (0.008, 0.005, 0.008), jewels[0], head=True)
            fpts = [q + Vector((0, 0.004, 0.005)), q + Vector((0.012, 0.02, 0.03)), q + Vector((0.03, 0.05, 0.045))]
            curve_tube("feather", fpts, [0.4, 1.0, 0.2], 0.008, mat_basic("#f4f2ee", 0.6, sheen=0.8), head=True)
    elif h in ("klobuk", "klobuk_white"):
        col = "#141315" if h == "klobuk" else "#f4f2ec"
        km = mat_basic(col, rough=0.6, sheen=0.5)
        if h == "klobuk":
            hat_loft(cx, "kamilavka", B, [(-0.006, 1.0), (0.0, 1.01), (0.10, 1.07), (0.115, 1.08), (0.117, 0.0)], km)
            topz, tops = 0.117, 1.10
        else:
            hat_loft(cx, "klobuk", B, [(-0.006, 1.0), (0.0, 1.02), (0.05, 1.06), (0.09, 0.95), (0.115, 0.62),
                                        (0.125, 0.0)], km)
            topz, tops = 0.06, 1.08
            cross = cross_mesh("kl_cross", [(0, 0, 0.003, 0.012), (0, 0.003, 0.009, 0.003)], 0, 0.003, G)
            v = pts[0] - c
            cross.matrix_basis = HEAD_M @ Matrix.Translation(c + v * 1.07 + up * 0.075)
        # покрывало (наметка)
        ring_pts = []
        npts = len(pts)
        sel = [i for i in range(npts) if abs(math.atan2(math.sin(2 * math.pi * i / npts),
                                                         math.cos(2 * math.pi * i / npts))) > 1.1]
        sel.sort(key=lambda i: (2 * math.pi * i / npts - 1.1) % (2 * math.pi))
        rings = []
        for dz, sc, drop in ((topz, tops, 0.0), (topz - 0.05, tops + 0.03, 0.0), (-0.02, 1.16, 0.0),
                             (-0.12, 1.32, 0.02), (-0.22, 1.62, 0.05)):
            ring = []
            for i in sel:
                v = pts[i] - c
                ring.append(c + Vector((v.x * sc, v.y * sc + drop, v.z)) + up * dz)
            rings.append(ring)
        loft("veil", rings, km, closed=False, head=True, solid=0.004, sub=2)
    elif h == "biretta":
        bm = mat_basic("#121114", rough=0.55, sheen=0.6)
        sq = lambda v: Vector((spow(v.x / 0.08, 0.6) * 0.08, spow(v.y / 0.09, 0.6) * 0.09, v.z))
        hat_loft(cx, "biretta", B, [(-0.006, 1.0), (0.0, 1.02), (0.05, 1.03), (0.058, 0.85), (0.06, 0.0)], bm)
        for k, ang in enumerate((0.0, math.pi / 2, -math.pi / 2)):
            d = Vector((math.sin(ang), -math.cos(ang), 0))
            p0 = c + d * 0.012 + up * 0.06
            p1 = c + d * 0.060 + up * 0.06
            fpts = [p0, (p0 + p1) / 2 + up * 0.03, p1 + up * 0.004]
            curve_tube("fin", fpts, [0.5, 1.0, 0.6], 0.006, bm, head=True)
        ellipsoid("pom", c + up * 0.072, (0.011, 0.011, 0.009), bm, head=True)
    elif h == "cap_soft":
        cm = mat_basic(s["c4"], rough=0.8, sheen=0.4, bump=((300.0, 300.0, 300.0), 0.08))
        hat_loft(cx, "cap", B, [(-0.006, 1.0), (0.0, 1.05), (0.025, 1.10), (0.05, 1.02), (0.068, 0.78), (0.078, 0.35),
                                (0.08, 0.0)], cm)
        hat_loft(cx, "cap_roll", B, [(-0.008, 1.02), (-0.002, 1.12), (0.012, 1.13), (0.02, 1.06)], cm, cap=False)
    elif h == "crown_imp":
        # малая императорская корона на высокой причёске
        cc = Vector((0, 0.020, 0.128))
        upz = Vector((0, 0.15, 1)).normalized()
        sv = mat_basic("#f4f0e6", 0.2, coat=1.0, spec=0.7)
        ring = [cc + Vector((0.034 * math.sin(2 * math.pi * i / 32), -0.030 * math.cos(2 * math.pi * i / 32), 0))
                for i in range(32)]
        loft("ic_band", [ring, [p + upz * 0.012 for p in ring]], G, closed=True, head=True, solid=0.003, sub=1)
        for sd in (-1, 1):
            ellipsoid("ic_half", cc + Vector((sd * 0.016, 0, 0.026)), (0.017, 0.028, 0.030), sv, head=True,
                      deform=lambda u: (u[0], u[1], max(u[2], -0.3)))
        ap = [cc + Vector((0, -0.030, 0.012)), cc + Vector((0, -0.022, 0.050)), cc + Vector((0, 0.0, 0.064)),
              cc + Vector((0, 0.022, 0.050)), cc + Vector((0, 0.030, 0.012))]
        curve_tube("ic_arch", ap, [1] * 5, 0.0045, G, head=True)
        ellipsoid("ic_spinel", cc + Vector((0, 0, 0.072)), (0.007, 0.007, 0.007), jewels[0], head=True)
        cross = cross_mesh("ic_cross", [(0, 0, 0.0025, 0.010), (0, 0.002, 0.007, 0.0025)], 0, 0.0025, G)
        cross.matrix_basis = HEAD_M @ Matrix.Translation(cc + Vector((0, 0, 0.088)))
        for k in range(-2, 3):
            a = k * 0.5
            ellipsoid("ic_jewel", cc + Vector((0.035 * math.sin(a), -0.031 * math.cos(a), 0.006)),
                      (0.004, 0.003, 0.004), jewels[(k + 2) % 3], head=True, segs=8, rings=6)


def build_nun(cx, s):
    white = mat_basic("#f6f4ee", rough=0.65, sheen=0.4, bump=((260.0, 260.0, 260.0), 0.04))
    black = mat_basic(s["c1"] if s["c1"] else "#1c1a1a", rough=0.7, sheen=0.6, bump=((260.0, 260.0, 260.0), 0.05))

    def Fo(u):
        x, y, z = u
        at = abs(math.atan2(x, -y))
        zc = -0.20
        dz = (z - zc) / (0.58 if z > zc else 0.86)
        return (at / 0.70) ** 2 + dz * dz, at, z

    def ww(u):
        f, at, z = Fo(u)
        return sstep(0.92, 1.02, f), 0.0065

    def vw(u):
        f, at, z = Fo(u)
        w = sstep(1.18, 1.30, f) * (1.0 - sstep(-0.30, -0.50, z) * sstep(1.55, 1.20, at))
        return w, 0.017 + 0.006 * sstep(0.3, 0.9, z)
    shell("wimple", cx, ww, white)
    ob, pos = shell("veil", cx, vw, black)
    # горжет-апостольник вокруг шеи
    dz = cx.hoff.z - 0.190   # кольца заданы для исходной высоты головы
    rings = [ell_ring(z + dz * sstep(-0.07, 0.14, z), rx, ry, 0, cy, n=48) for z, rx, ry, cy in
             ((0.14, 0.058, 0.060, 0.016), (0.09, 0.064, 0.066, 0.012), (0.04, 0.078, 0.074, 0.006),
              (0.00, 0.105, 0.088, 0.0), (-0.04, 0.140, 0.104, 0.0), (-0.07, 0.15, 0.112, 0.0))]
    loft("gorget", rings, white, closed=True, solid=0.004, sub=2)
    # покрывало до плеч
    rings = []
    for z, rx, ry, cy, a0 in ((0.25, 0.100, 0.114, 0.006, 0.95), (0.18, 0.102, 0.114, 0.012, 0.98),
                              (0.10, 0.108, 0.112, 0.020, 1.02), (0.04, 0.132, 0.118, 0.022, 1.08),
                              (-0.01, 0.172, 0.126, 0.012, 1.15), (-0.06, 0.200, 0.132, 0.004, 1.22),
                              (-0.20, 0.206, 0.134, 0.0, 1.25)):
        rings.append(ell_ring(z + dz * sstep(-0.20, 0.25, z), rx, ry, 0, cy, n=40, a0=a0, a1=2 * math.pi - a0,
                              closed=False))
    loft("veil_drape", rings, black, closed=False, solid=0.005, sub=2)


# ----------------------------------------------------------------------------------------------
#  Предметы в руках
# ----------------------------------------------------------------------------------------------
def paper_img(kind, W=128, H=128):
    bgc = {"scroll": "#e6d3a8", "music": "#f2ead8", "printed": "#efe4c8", "map": "#e8d8b0",
           "plan": "#2a4e8a", "doc": "#efe6cf", "tape": "#efe0b8", "screen": "#1e2a40",
           "slutsk": "#c89a3a"}[kind]
    cv = Canvas(W, H, 0.0, 1.0, 0.0, 1.0, bg=srgb(bgc))
    ink = C("#4a3020")
    if kind == "scroll":
        cv.line([(0.25, 0.2), (0.25, 0.6), (0.5, 0.78), (0.75, 0.6), (0.75, 0.2), (0.25, 0.2)], 0.025, ink)
        cv.line([(0.42, 0.2), (0.42, 0.4), (0.58, 0.4), (0.58, 0.2)], 0.02, ink)
        cv.line([(0.5, 0.78), (0.5, 0.9)], 0.02, ink); cv.line([(0.45, 0.86), (0.55, 0.86)], 0.02, ink)
    elif kind == "music":
        for st in (0.75, 0.42, 0.10):
            for k in range(5):
                cv.line([(0.06, st + k * 0.04), (0.94, st + k * 0.04)], 0.01, C("#3a3030"))
            for k in range(7):
                cv.circle(0.14 + k * 0.12, st + 0.02 + 0.03 * ((k * 3) % 5), 0.025, C("#1a1414"))
                cv.line([(0.165 + k * 0.12, st + 0.03 + 0.03 * ((k * 3) % 5)), (0.165 + k * 0.12, st + 0.16)], 0.012,
                        C("#1a1414"))
    elif kind in ("printed", "doc"):
        cv.rect(0.12, 0.70, 0.30, 0.88, C("#a8201e"))
        for k in range(14):
            z = 0.86 - k * 0.055
            x0 = 0.34 if z > 0.68 else 0.12
            cv.rect(x0, z, 0.88 - 0.1 * (k % 3 == 2), z + 0.022, C("#2a2420"))
        if kind == "printed":
            cv.rect(0.12, 0.08, 0.88, 0.0 + 0.10, C("#2a2420"))
    elif kind == "map":
        cv.poly([(0.1, 0.2), (0.4, 0.15), (0.7, 0.3), (0.85, 0.7), (0.5, 0.85), (0.2, 0.7)], C("#8aa86a"))
        cv.poly([(0.3, 0.35), (0.55, 0.3), (0.6, 0.6), (0.35, 0.65)], C("#c8b878"))
        cv.line([(0.1, 0.5), (0.3, 0.45), (0.5, 0.55), (0.9, 0.4)], 0.02, C("#3a6ab0"))
        for x, z in ((0.3, 0.55), (0.6, 0.45), (0.45, 0.7)):
            cv.circle(x, z, 0.03, C("#a8201e"))
    elif kind == "plan":
        w = C("#e8f0ff")
        for k in range(6):
            cv.line([(0.1, 0.15 + k * 0.14), (0.9, 0.12 + k * 0.15)], 0.012, w)
        cv.line([(0.2, 0.1), (0.35, 0.9)], 0.02, w); cv.line([(0.6, 0.1), (0.7, 0.9)], 0.02, w)
        cv.circle(0.75, 0.25, 0.08, C("#e8f0ff", 0.0))
    elif kind == "tape":
        for k in range(10):
            for j in range(5):
                if (k * 7 + j * 3) % 4 != 0:
                    cv.circle(0.06 + k * 0.1, 0.15 + j * 0.18, 0.03, C("#3a3028"))
    elif kind == "screen":
        cols = ["#6ac8ff", "#ffd36a", "#9aff9a", "#ff8a8a"]
        for k in range(10):
            cv.rect(0.08 + 0.06 * (k % 3), 0.86 - k * 0.08, 0.5 + 0.3 * ((k * 7) % 5) / 5, 0.86 - k * 0.08 + 0.035,
                    C(cols[k % 4]))
    elif kind == "slutsk":
        def pat(x, z):
            if z < 0.12 or z > 0.88: return C("#7a1a2a")
            if z < 0.18 or z > 0.82: return C("#e8c868")
            u = (x * 6) % 1.0; v = z
            d = abs(u - 0.5) * 1.6 + abs(v - 0.5)
            if d < 0.18: return C("#2a6a3a")
            if d < 0.30: return C("#e8c868")
            if d < 0.36: return C("#7a1a2a")
            return None
        cv.rect(0.0, 0.0, 1.0, 1.0, None, pat)
    img = cv.image("paper_" + kind)
    return img


def sheet(name, w, h, img, c, rot, curl=0.0, emit=0.0):
    verts, faces, uvs = [], [], []
    nx, nz = 8, 8
    for j in range(nz + 1):
        for i in range(nx + 1):
            u, v = i / nx, j / nz
            x = (u - 0.5) * w; z = (v - 0.5) * h
            y = curl * (2 * u - 1) ** 2 * w
            verts.append(rot @ Vector((x, y, z)) + Vector(c)); uvs.append((u, v))
    for j in range(nz):
        for i in range(nx):
            a = j * (nx + 1) + i
            faces.append((a, a + 1, a + nx + 2, a + nx + 1))
    return make_mesh(name, verts, faces, mat_image(img, emit=emit), solid=0.002, sub=0, uvs=uvs, recalc=False)


def fingers(skin, p, right, up, nrm, count=4, scale=1.0):
    for k in range(count):
        q = p + right * (k - (count - 1) / 2) * 0.0115 * scale + nrm * 0.006
        rot = Matrix((right, nrm.cross(right) * -1, up)).transposed() if False else None
        m = Matrix.Identity(3)
        m.col[0] = right; m.col[1] = -nrm; m.col[2] = up
        ellipsoid("finger", q + up * (0.004 * (1 - abs(k - 1.5) / 2)), (0.0072 * scale, 0.0068 * scale, 0.020 * scale),
                  skin, rot=m, segs=12, rings=8)
    m = Matrix.Identity(3); m.col[0] = right; m.col[1] = -nrm; m.col[2] = up
    ellipsoid("palm", p - up * 0.018 * scale - nrm * 0.004, (0.026 * scale, 0.012 * scale, 0.022 * scale), skin,
              rot=m, segs=14, rings=10)


def build_prop(cx, s):
    pr = s["prop"]
    if not pr: return
    G = cx.gold
    A = Vector((0.072, -0.150, -0.150))
    if "cross6" in s["acc"]: A = Vector((0.110, -0.140, -0.185))   # крест на груди должен быть виден
    face = Matrix.Rotation(0.42, 3, "Z")
    R = face @ Matrix.Rotation(0.18, 3, "X")
    right, up, nrm = R @ Vector((1, 0, 0)), R @ Vector((0, 0, 1)), R @ Vector((0, -1, 0))
    skin = cx.skin
    if pr.startswith("book"):
        col = {"book_red": "#7a1f22", "book_gold": "#b8862a", "book_sun": "#5a3a1a", "book_black": "#1e1a18",
               "book_brown": "#5a3a22"}[pr]
        cm = mat_basic(col, 0.45, sheen=0.4, bump=((200.0, 200.0, 200.0), 0.1), metal=0.6 if pr == "book_gold" else 0.0)
        c = A + up * 0.035
        box("book", c, (0.105, 0.030, 0.140), cm, rot=R, bevel=0.004)
        box("pages", c + nrm * -0.0 + right * 0.004, (0.098, 0.024, 0.132), mat_basic("#efe6cc", 0.7), rot=R,
            bevel=0.001)
        if pr in ("book_red", "book_gold", "book_brown", "book_black"):
            ob = cross_mesh("bk_cross", [(0, 0, 0.0045, 0.030), (0, 0.010, 0.020, 0.0045)], 0, 0.004, G)
            ob.matrix_basis = Matrix.Translation(c + nrm * 0.016) @ R.to_4x4()
            for sx, sz in ((-1, -1), (1, -1), (-1, 1), (1, 1)):
                ellipsoid("bk_stud", c + nrm * 0.0155 + right * sx * 0.040 + up * sz * 0.058, (0.006, 0.003, 0.006),
                          G, segs=10, rings=8)
        else:
            e = ellipsoid("sun", (0, 0, 0), (0.020, 0.003, 0.020), G, segs=16, rings=8)
            e.matrix_basis = Matrix.Translation(c + nrm * 0.016 + up * 0.012) @ R.to_4x4()
            e2 = ellipsoid("moon", (0, 0, 0), (0.012, 0.003, 0.012), mat_basic("#d8d8d0", 0.3, metal=1.0), segs=12, rings=8)
            e2.matrix_basis = Matrix.Translation(c + nrm * 0.018 + up * -0.030 + right * 0.01) @ R.to_4x4()
        fingers(skin, A - up * 0.028 + right * 0.02 + nrm * 0.016, right, up, nrm)
    elif pr in ("scroll", "scroll_doc", "printed", "music", "map", "plan"):
        kind = {"scroll": "scroll", "scroll_doc": "doc", "printed": "printed", "music": "music", "map": "map",
                "plan": "plan"}[pr]
        img = paper_img(kind)
        c = A + up * 0.03
        w, h = (0.14, 0.12) if pr in ("scroll", "map", "plan") else (0.11, 0.14)
        sheet("paper", w, h, img, c, R, curl=0.06)
        if pr in ("scroll", "map", "plan", "scroll_doc"):
            rc = mat_basic("#d8c69a" if pr != "plan" else "#2a4e8a", 0.6)
            for sd in (-1, 1):
                p = c + right * sd * w / 2 + nrm * 0.06 * w
                cylinder("roll", p - up * (h / 2 + 0.006), p + up * (h / 2 + 0.006), 0.009, 0.009, rc, sub=0)
        fingers(skin, c - up * h * 0.45 + right * 0.03 + nrm * 0.004, right, up, nrm)
    elif pr == "hammer":
        wood = mat_basic("#8a5a32", 0.6)
        iron = mat_basic("#5a5a5e", 0.35, metal=1.0)
        p0 = A - up * 0.05 + right * 0.02; p1 = A + up * 0.11 - right * 0.02
        cylinder("handle", p0, p1, 0.007, 0.006, wood, sub=0)
        box("hammer_head", p1 + up * 0.005, (0.075, 0.026, 0.026), iron, rot=R @ Matrix.Rotation(-0.25, 3, "Y"),
            bevel=0.004)
        fingers(skin, A - up * 0.02 + nrm * 0.008, right, up, nrm)
    elif pr == "compass":
        img = paper_img("scroll")
        c = A + up * 0.03
        sheet("plan", 0.13, 0.12, img, c - nrm * 0.004, R, curl=0.03)
        mt = mat_basic("#c8ccd0", 0.25, metal=1.0)
        hinge = c + up * 0.05 + nrm * 0.012
        for sd in (-1, 1):
            cylinder("leg", hinge, hinge - up * 0.10 + right * sd * 0.035 + nrm * 0.002, 0.0035, 0.0012, mt, sub=0)
        ellipsoid("hinge", hinge, (0.007, 0.007, 0.007), G)
        fingers(skin, c - up * 0.055 + right * 0.035 + nrm * 0.004, right, up, nrm)
    elif pr == "rocket_old":
        rm = mat_basic("#a8402a", 0.5); bm_ = mat_basic("#d8b46a", 0.5)
        base = A - up * 0.05
        lathe("rocket", [(0.0, 0.0), (0.018, 0.0), (0.018, 0.10), (0.015, 0.11), (0.015, 0.15), (0.0, 0.19)], rm,
              c=base, rot=R, n=20)
        for z in (0.03, 0.07, 0.11):
            lathe("band", [(0.0185, z), (0.0195, z + 0.002), (0.0195, z + 0.008), (0.0185, z + 0.01)], bm_, c=base, rot=R,
                  n=20, sub=0)
        for k in range(3):
            a = 2 * math.pi * k / 3
            d = R @ Vector((math.cos(a), math.sin(a), 0))
            box("fin", base + d * 0.026 + up * 0.012, (0.003, 0.022, 0.035), bm_,
                rot=R @ Matrix.Rotation(a + math.pi / 2, 3, "Z"), bevel=0.001)
        fingers(skin, A + nrm * 0.016, right, up, nrm)
    elif pr == "engine":
        cu = mat_basic("#c87a4a", 0.3, metal=1.0); stl = mat_basic("#b8bcc2", 0.28, metal=1.0)
        base = A + up * 0.02
        lathe("nozzle", [(0.012, 0.06), (0.016, 0.045), (0.022, 0.03), (0.034, 0.0), (0.045, -0.04), (0.043, -0.042),
                         (0.031, -0.002), (0.020, 0.028), (0.012, 0.04)], cu, c=base, rot=R, n=28)
        lathe("chamber", [(0.0, 0.11), (0.024, 0.105), (0.026, 0.08), (0.016, 0.058), (0.0, 0.058)], stl, c=base, rot=R, n=24)
        for sd in (-1, 1):
            pts = [base + up * 0.10 + right * sd * 0.02, base + up * 0.12 + right * sd * 0.05,
                   base + up * 0.07 + right * sd * 0.05, base + up * 0.04 + right * sd * 0.03]
            curve_tube("pipe", pts, [1, 1, 1, 1], 0.004, stl)
        fingers(skin, A - up * 0.03 + nrm * 0.03, right, up, nrm)
    elif pr == "laser":
        dev = mat_basic("#3a3e46", 0.35, metal=0.6)
        p0 = A - up * 0.02; d = (up * 0.6 - right * 0.8 + nrm * 0.1).normalized()
        cylinder("laser_body", p0, p0 + d * 0.09, 0.016, 0.016, dev, sub=0, bevel=0.002)
        cylinder("laser_ring", p0 + d * 0.09, p0 + d * 0.098, 0.012, 0.010, mat_basic("#c8ccd0", 0.25, metal=1.0))
        beam = mat_basic("#ff2020", 0.5, emit="#ff2a1a", emit_str=12.0)
        cylinder("beam", p0 + d * 0.098, p0 + d * 0.60, 0.0028, 0.0028, beam, n=10)
        ellipsoid("flare", p0 + d * 0.10, (0.007, 0.007, 0.007), mat_basic("#ffd0c0", 0.5, emit="#ff6a4a", emit_str=15.0))
        fingers(skin, p0 + d * 0.04 + nrm * 0.016, right, up, nrm)
    elif pr == "tape":
        img = paper_img("tape")
        pts = []
        for k in range(14):
            t = k / 13
            pts.append(A + up * (-0.07 + 0.20 * t) + right * (0.03 * math.sin(t * 7.0)) + nrm * (0.02 * math.cos(t * 5)))
        verts, faces, uvs = [], [], []
        for k, p in enumerate(pts):
            verts += [p - right * 0.014, p + right * 0.014]; uvs += [(k / 13 * 4 % 1.0, 0.0), (k / 13 * 4 % 1.0, 1.0)]
        uvs = []
        for k in range(len(pts)):
            uvs += [(0.0, k / 13), (1.0, k / 13)]
        for k in range(len(pts) - 1):
            faces.append((2 * k, 2 * k + 1, 2 * k + 3, 2 * k + 2))
        make_mesh("tape", verts, faces, mat_image(img), solid=0.0015, sub=1, uvs=uvs, recalc=False)
        fingers(skin, A - up * 0.04 + nrm * 0.012, right, up, nrm)
    elif pr == "plane":
        pm = mat_basic("#9aa8b8", 0.4, metal=0.2); dk = mat_basic("#4a5868", 0.4)
        c = A + up * 0.03
        Rp = R @ Matrix.Rotation(-0.5, 3, "Z") @ Matrix.Rotation(0.35, 3, "Y")
        fx = Rp @ Vector((1, 0, 0)); fy = Rp @ Vector((0, 1, 0)); fz = Rp @ Vector((0, 0, 1))
        ellipsoid("fuselage", c, (0.075, 0.014, 0.013), pm, rot=Rp, segs=24, rings=12,
                  deform=lambda u: (u[0], u[1] * (1 - 0.4 * max(0, u[0]) ** 2), u[2] * (1 - 0.3 * max(0, u[0]) ** 2)))
        wing = [(0.02, 0.0, 0.0), (-0.035, 0.065, 0.0), (-0.050, 0.065, 0.0), (-0.045, 0.0, 0.0)]
        for sd in (-1, 1):
            vv = [c + fx * p_[0] + fy * p_[1] * sd + fz * 0.0 for p_ in wing]
            make_mesh("wing", vv, [(0, 1, 2, 3)], pm, smooth=False, sub=0, solid=0.003)
            tl = [(-0.045, 0.010 * sd, 0.0), (-0.065, 0.016 * sd, 0.034), (-0.075, 0.016 * sd, 0.034), (-0.072, 0.010 * sd, 0.0)]
            vv = [c + fx * p_[0] + fy * p_[1] + fz * p_[2] for p_ in tl]
            make_mesh("tail", vv, [(0, 1, 2, 3)], pm, smooth=False, sub=0, solid=0.003)
        ellipsoid("canopy", c + fx * 0.035 + fz * 0.010, (0.018, 0.007, 0.007), mat_basic("#2a3a50", 0.1, coat=1.0),
                  rot=Rp, segs=14, rings=8)
        fingers(skin, c - fz * 0.035 - fx * 0.01, fx, fz, nrm)
    elif pr == "nobel":
        c = A + up * 0.03
        cylinder("medal", c - nrm * 0.002, c + nrm * 0.004, 0.034, 0.034, G, n=32, sub=0, bevel=0.0015)
        ellipsoid("profile", c + nrm * 0.005 + right * 0.002, (0.012, 0.003, 0.016), G, segs=14, rings=8,
                  rot=Vector((0, -1, 0)).rotation_difference(nrm).to_matrix())
        fingers(skin, c - up * 0.040 + nrm * 0.004, right, up, nrm)
    elif pr == "laptop":
        al = mat_basic("#c8ccd2", 0.3, metal=0.8)
        Rl = R @ Matrix.Rotation(-0.75, 3, "X")
        c = A + up * 0.0
        box("lap_base", c, (0.17, 0.11, 0.008), al, rot=Rl, bevel=0.003)
        fy = Rl @ Vector((0, 1, 0)); fz = Rl @ Vector((0, 0, 1))
        hinge = c + fy * 0.055
        Rs = Rl @ Matrix.Rotation(1.75, 3, "X")
        sc_c = hinge + (Rs @ Vector((0, -0.055, 0.0)))
        box("lap_lid", sc_c, (0.17, 0.11, 0.006), al, rot=Rs, bevel=0.003)
        img = paper_img("screen")
        sheet("screen", 0.15, 0.095, img, sc_c + (Rs @ Vector((0, 0, 0.0045))), Rs @ Matrix.Rotation(-math.pi / 2, 3, "X"), emit=1.5)
        fingers(skin, c - fz * 0.0 - fy * 0.05 + nrm * 0.0, Rl @ Vector((1, 0, 0)), -fy, fz)
    elif pr == "tank":
        ol = mat_basic("#5a6a3a", 0.55); dk = mat_basic("#2a2a26", 0.6)
        c = A + up * 0.015
        Rt = R @ Matrix.Rotation(-0.6, 3, "Z") @ Matrix.Rotation(0.25, 3, "X")
        box("hull", c, (0.11, 0.065, 0.026), ol, rot=Rt, bevel=0.005)
        for sd in (-1, 1):
            box("track", c + Rt @ Vector((0, sd * 0.038, -0.006)), (0.118, 0.018, 0.026), dk, rot=Rt, bevel=0.007)
        ellipsoid("turret", c + Rt @ Vector((-0.01, 0, 0.022)), (0.032, 0.026, 0.014), ol, rot=Rt)
        cylinder("barrel", c + Rt @ Vector((0.015, 0, 0.024)), c + Rt @ Vector((0.10, 0, 0.028)), 0.0045, 0.004, ol)
        fingers(skin, c - (Rt @ Vector((0, 0, 1))) * 0.032, Rt @ Vector((1, 0, 0)), Rt @ Vector((0, 0, 1)), nrm)
    elif pr == "harp":
        gm = G; wood = mat_basic("#8a4a22", 0.4, coat=0.6)
        b0 = A - up * 0.06 + right * 0.0
        col_top = b0 + up * 0.20 - right * 0.01
        curve_tube("harp_col", [b0 - right * 0.0, b0 + up * 0.08 - right * 0.012, col_top], [1, 1, 1.2], 0.006, gm)
        neck = [col_top, col_top + up * 0.01 + right * 0.05, col_top - up * 0.02 + right * 0.09,
                col_top - up * 0.03 + right * 0.12]
        curve_tube("harp_neck", neck, [1.2, 1, 1, 1.1], 0.006, gm)
        sb0 = b0 + right * 0.01; sb1 = neck[-1] - up * 0.005
        cylinder("soundboard", sb0, sb1, 0.010, 0.016, wood, n=12, sub=0)
        sm = mat_basic("#f0e8d0", 0.4)
        for k in range(1, 8):
            t = k / 8
            pa = sb0.lerp(sb1, t) - nrm * 0.0
            pb_ = b0 + up * 0.18 * 1.0
            top = neck[0].lerp(neck[-1], t) - up * 0.005
            cylinder("string", pa, top, 0.0009, 0.0009, sm, n=6)
        fingers(skin, b0 + up * 0.02 + nrm * 0.008, right, up, nrm)
    elif pr == "plant":
        stem = mat_basic("#4a7a2a", 0.5); leaf = mat_basic("#5a9a3a", 0.45, sheen=0.3)
        fl = mat_basic("#e8a0c8", 0.4); fc = mat_basic("#f0d040", 0.4)
        b0 = A - up * 0.03
        tip = b0 + up * 0.17 - right * 0.03
        curve_tube("stem", [b0, b0 + up * 0.08, tip], [1, 0.9, 0.6], 0.0025, stem)
        for k in range(6):
            t = 0.25 + 0.12 * k
            p = b0.lerp(tip, t)
            sd = 1 if k % 2 else -1
            m = Matrix.Rotation(sd * 0.9, 3, "Y")
            ellipsoid("leaf", p + right * sd * 0.016, (0.017, 0.003, 0.008), leaf, rot=R @ m, segs=12, rings=8)
        for k, (dx, dz) in enumerate(((0.0, 0.0), (0.018, -0.012), (-0.016, -0.014))):
            q = tip + right * dx + up * dz
            for j in range(5):
                a = 2 * math.pi * j / 5
                ellipsoid("petal", q + (right * math.cos(a) + up * math.sin(a)) * 0.007, (0.006, 0.002, 0.006), fl,
                          rot=R, segs=8, rings=6)
            ellipsoid("fcentre", q - nrm * 0.0 + nrm * 0.002, (0.004, 0.003, 0.004), fc, segs=8, rings=6)
        fingers(skin, b0 + nrm * 0.004, right, up, nrm)
    elif pr == "mineral":
        rock = mat_basic("#8a8478", 0.8, bump=((150.0, 150.0, 150.0), 0.4))
        cr = mat_basic("#9a6ac8", 0.08, spec=0.9, coat=1.0)
        c = A + up * 0.0
        ellipsoid("rock", c, (0.045, 0.035, 0.025), rock, segs=12, rings=8,
                  deform=lambda u: (u[0] * (1 + 0.15 * math.sin(u[1] * 5)), u[1], u[2] * (1 + 0.2 * math.cos(u[0] * 4))))
        rnd = random.Random(3)
        for k in range(6):
            a = -0.8 + 0.3 * k
            d = (up * math.cos(a) + right * math.sin(a)).normalized()
            p0 = c + d * 0.012 + right * rnd.uniform(-0.015, 0.015)
            L = 0.04 + 0.03 * rnd.random()
            lathe("crystal", [(0.0, -0.005), (0.009, 0.0), (0.009, L), (0.0, L + 0.014)], cr, c=p0,
                  rot=Vector((0, 0, 1)).rotation_difference(d).to_matrix(), n=6, sub=0)
        fingers(skin, c - up * 0.03 + nrm * 0.01, right, up, nrm)
    elif pr == "spark":
        mt = mat_basic("#c8ccd2", 0.2, metal=1.0); br = mat_basic("#c8904a", 0.3, metal=1.0)
        b0 = A - up * 0.03
        for sd in (-1, 1):
            p = b0 + right * sd * 0.045
            cylinder("rod", p - up * 0.04, p + up * 0.07, 0.0035, 0.0035, br)
            ellipsoid("ball", p + up * 0.08, (0.014, 0.014, 0.014), mt)
        bolt = mat_basic("#a0c8ff", 0.5, emit="#7ab0ff", emit_str=18.0)
        pts = [b0 - right * 0.031 + up * 0.08, b0 - right * 0.012 + up * 0.095, b0 + right * 0.0 + up * 0.072,
               b0 + right * 0.014 + up * 0.09, b0 + right * 0.031 + up * 0.08]
        for i in range(len(pts) - 1):
            cylinder("bolt", pts[i], pts[i + 1], 0.0016, 0.0016, bolt, n=8)
        ellipsoid("glow", b0 + up * 0.085, (0.03, 0.006, 0.014), mat_basic("#c0d8ff", 0.5, emit="#6aa0ff", emit_str=3.0, alpha=0.25))
        fingers(skin, b0 - up * 0.02 + nrm * 0.006, right, up, nrm)
    elif pr == "helmet_space":
        wm = mat_basic("#eceeee", 0.3, coat=0.5); vis = mat_basic("#1a2232", 0.05, metal=0.6, coat=1.0)
        bl = mat_basic("#3a64b0", 0.35)
        c = Vector((0.12, -0.16, -0.17))
        ellipsoid("helm", c, (0.085, 0.09, 0.095), wm, segs=28, rings=18)
        Rv = Matrix.Rotation(0.5, 3, "Z")
        ellipsoid("visor", c + Rv @ Vector((0, -0.018, 0.012)), (0.070, 0.078, 0.062), vis, rot=Rv, segs=28, rings=16,
                  deform=lambda u: (u[0], min(u[1], -0.25) if u[1] < 0 else -0.25, u[2]))
        lathe("helm_ring", [(0.07, -0.075), (0.074, -0.068), (0.074, -0.055), (0.07, -0.05)], bl, c=c, n=28)
    elif pr == "slutsk":
        img = paper_img("slutsk", 256, 64)
        bm_ = mat_image(img, rough=0.35)
        setp(bm_.node_tree.nodes.get("Principled BSDF"), "Metallic", 0.35)
        verts, faces, uvs = [], [], []
        guide = [(0.17, -0.04, -0.04), (0.10, -0.12, -0.10), (0.02, -0.14, -0.17), (-0.06, -0.14, -0.23),
                 (-0.16, -0.10, -0.33)]
        ob = strip(cx, "slutsk_belt", guide, [0.075] * 5, bm_, off=0.008, solid=0.004, front=False, uv=True)
        # бахрома
        p, n = tfront(cx, 0.06, -0.20)


# ----------------------------------------------------------------------------------------------
#  Сборка персонажа, кадрирование, рендер
# ----------------------------------------------------------------------------------------------
def build_character(pid):
    global ROOT, HEAD_M
    s = spec_for(pid)
    cx = Ctx()
    cx.outer_bvh = None
    fem = s["sex"] == "f"
    ROOT = bpy.data.objects.new("ROOT_" + pid, None)
    COLL.objects.link(ROOT)
    hs = 1.03 if fem else 1.08
    hoff = Vector((0.0, -0.004, 0.146 if fem else 0.152))
    HEAD_M = Matrix.Translation(hoff) @ Matrix.Scale(hs, 4)
    cx.hs, cx.hoff = hs, hoff
    NECK_D[0] = 0.009 if fem else 0.011
    cx.skin_hex = SKIN.get(s["skin"], s["skin"])
    cx.skin = mat_skin(cx.skin_hex)
    cx.skin_ear = mat_skin(cx.skin_hex)
    setp(cx.skin_ear.node_tree.nodes.get("Principled BSDF"), "Base Color", (*mulc(srgb(cx.skin_hex), 0.97), 1))
    cx.skin_ear_in = mat_skin(cx.skin_hex)
    setp(cx.skin_ear_in.node_tree.nodes.get("Principled BSDF"), "Base Color",
         (*mixc(srgb(cx.skin_hex), (0.55, 0.22, 0.18), 0.25), 1))
    cx.mouth_mat = mat_basic(mixc(srgb(cx.skin_hex), (0.25, 0.06, 0.05), 0.55), rough=0.5)
    cx.gold = gold()
    build_head(cx, s)
    build_face(cx, s)
    build_hair(cx, s)
    build_beard(cx, s)
    build_torso(cx, s)
    build_outfit(cx, s)
    build_hat(cx, s)
    if s["glasses"]: build_glasses(cx, s)
    build_prop(cx, s)
    ROOT.rotation_euler = (0.0, 0.0, -0.49)
    return s, cx


def build_glasses(cx, s):
    gm = mat_basic(s["gcol"], rough=0.3, metal=0.6 if s["gcol"] in ("#8a8478", "#6a645c", "#5a544e") else 0.0)
    glass = mat_basic("#e8f0f8", rough=0.04, spec=0.8, alpha=0.12)
    ex = 0.0315 * cx.F["eye_sep"]
    rnd = s["glasses"] == "round"
    rx, rz = (0.0165, 0.0165) if rnd else (0.0205, 0.0145)
    yf = None
    for sd in (-1, 1):
        p, n = hit_front(cx.hbvh, sd * (ex + 0.004), 0.026)
        p2, n2 = hit_front(cx.hbvh, sd * ex, -0.025)
        y = min(p.y, p2.y) - 0.009
        yf = y
        pts = []
        for i in range(16):
            a = 2 * math.pi * i / 16
            ca, sa = math.cos(a), math.sin(a)
            if not rnd:
                ca = spow(ca, 0.6); sa = spow(sa, 0.6)
            pts.append(Vector((sd * (ex + 0.002) + rx * ca, y + 0.002 * abs(ca), 0.002 + rz * sa)))
        curve_tube("rim", pts, [1] * 16, 0.0013, gm, head=True, cyclic=True)
        make_mesh("lens", [tuple(q) for q in pts], [tuple(range(16))], glass, head=True, sub=0)
        # дужка
        r = cx.hbvh.ray_cast(Vector((sd * 0.3, 0.0, 0.004)), Vector((-sd, 0, 0)))
        sx = r[0].x if r[0] else sd * 0.072
        curve_tube("temple", [Vector((sd * (ex + 0.002 + rx), y + 0.002, 0.006)),
                              Vector((sx + sd * 0.004, y + 0.03, 0.007)), Vector((sx + sd * 0.002, 0.012, 0.004))],
                   [1, 1, 1], 0.0011, gm, head=True)
    curve_tube("bridge", [Vector((-(ex + 0.002 - rx), yf - 0.001, 0.006)), Vector((0, yf - 0.004, 0.010)),
                          Vector(((ex + 0.002 - rx), yf - 0.001, 0.006))], [1, 1, 1], 0.0012, gm, head=True)


def frame_camera(sc, cam, s, cx):
    from bpy_extras.object_utils import world_to_camera_view
    W, H = sc.render.resolution_x, sc.render.resolution_y
    cd = cam.data
    cd.lens = 85.0; cd.shift_x = 0.0; cd.shift_y = 0.0
    bottom = Vector((0.0, -0.05, -0.165))
    topz = cx.hoff.z + cx.hs * 0.118 * s["F"]["top"] + 0.004
    top = Vector((0.0, 0.0, topz))
    headc = Vector((0.0, 0.0, cx.hoff.z))
    bpy.context.view_layer.update()

    def fr(p):
        return world_to_camera_view(sc, cam, p)
    for it in range(3):
        yb, yt = fr(bottom).y, fr(top).y
        cd.lens *= ((CELL_H - 16.0) / CELL_H) / max(1e-6, (yt - yb))
        bpy.context.view_layer.update()
        cd.shift_y += fr(bottom).y
        bpy.context.view_layer.update()
    # высокие уборы: уменьшаем, чтобы влезли
    dg = bpy.context.evaluated_depsgraph_get()
    ymax = 0.0
    for ob in COLL.objects:
        if ob.type not in ("MESH", "CURVE"): continue
        ev = ob.evaluated_get(dg)
        for corner in ev.bound_box:
            wp = ev.matrix_world @ Vector(corner)
            ymax = max(ymax, fr(wp).y)
    lim = 1.0 - 3.0 / CELL_H
    if ymax > lim:
        cd.lens *= lim / ymax * 0.995
        bpy.context.view_layer.update()
        for it in range(2):
            cd.shift_y += fr(bottom).y
            bpy.context.view_layer.update()
    for it in range(2):
        xh = fr(headc).x
        cd.shift_x += (xh - 0.53) * W / max(W, H)
        bpy.context.view_layer.update()


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    a = parse_args(argv)
    os.makedirs(a["work"], exist_ok=True)
    sc, cam = setup_scene(a["samples"], a["scale"])
    ids = a["only"] or IDS
    import time
    for pid in ids:
        t0 = time.time()
        clear_character()
        s, cx = build_character(pid)
        frame_camera(sc, cam, s, cx)
        sc.render.filepath = os.path.join(a["work"], pid + ".png")
        bpy.ops.render.render(write_still=True)
        print("PORTRAIT %s %.1fs" % (pid, time.time() - t0), flush=True)
    if a["save"]:
        if a["only"] is None or len(ids) > 1:
            clear_character()
            s, cx = build_character("euphrosyne")
            frame_camera(sc, cam, s, cx)
        txt = bpy.data.texts.new("build_portraits.py")
        with open(os.path.abspath(__file__), encoding="utf-8") as f:
            txt.from_string(f.read())
        out = os.path.join(GAME, "blender", "belarus_portraits.blend")
        os.makedirs(os.path.dirname(out), exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=out, compress=True)
        print("saved", out)


if __name__ == "__main__":
    if IN_BLENDER:
        main()
    else:
        a = parse_args(sys.argv[1:])
        post(a["work"])
