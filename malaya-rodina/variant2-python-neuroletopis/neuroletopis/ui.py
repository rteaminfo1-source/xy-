"""
Интерфейс «Нейролетописи» — всё нарисовано вручную на одном холсте Tkinter.
Никаких сторонних библиотек: только стандартный Python.
"""
from __future__ import annotations

import math
import os
import time
import tkinter as tk
import tkinter.font as tkfont

from . import data, nn

# ---------------------------------------------------------------- палитра
BG = "#090d1a"
PANEL = "#10172a"
PANEL2 = "#161f37"
BORDER = "#253252"
TEXT = "#eaeef7"
MUTED = "#8895b4"
GOLD = "#f2b84b"
CYAN = "#4cc9f0"
GREEN = "#76e294"
CORAL = "#ff7a70"
VIOLET = "#a78bfa"

FACTOR_COLORS = {
    "internet": CYAN, "digital": "#5aa9ff", "remote": VIOLET,
    "smart": GREEN, "roads": GOLD, "heritage": "#ff9fb2",
}
FATE_COLORS = {"Возрождение": GREEN, "Сохранение": CYAN, "Угасание": GOLD, "На грани исчезновения": CORAL}

PRESETS = [
    ("Как сейчас", None, MUTED),
    ("Без технологий", dict(internet=0.3, digital=0.1, remote=0.0, smart=0.0, roads=0.3, heritage=0.0), CORAL),
    ("Рывок", dict(internet=1.0, digital=1.0, remote=0.6, smart=0.9, roads=1.0, heritage=0.15), CYAN),
    ("Гармония", dict(internet=1.0, digital=0.95, remote=0.5, smart=0.7, roads=0.9, heritage=0.9), GREEN),
]

TABS = ["Летопись", "Нейросеть", "Прогноз 2050", "О проекте"]
LAYERS = [7, 12, 10, 2]
EPOCHS = 240


def hex2rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    """Смешивание цветов (в Tkinter нет прозрачности — имитируем её)."""
    ra, ga, ba = hex2rgb(a)
    rb, gb, bb = hex2rgb(b)
    t = max(0.0, min(1.0, t))
    return "#%02x%02x%02x" % (round(ra + (rb - ra) * t), round(ga + (gb - ga) * t), round(ba + (bb - ba) * t))


class App:
    def __init__(self, root: tk.Tk, data_path: str):
        self.root = root
        self.S = max(1.0, root.winfo_fpixels("1i") / 96.0)
        root.title("Нейролетопись малой родины — нейросеть изучает судьбу сёл")
        root.configure(bg=BG)
        sw, sh = root.winfo_screenwidth(), root.winfo_screenheight()
        w, h = min(self.s(1400), sw - 40), min(self.s(860), sh - 80)
        root.geometry(f"{w}x{h}+{(sw - w) // 2}+{max(0, (sh - h) // 3)}")
        root.minsize(self.s(1180), self.s(740))

        self.canvas = tk.Canvas(root, bg=BG, highlightthickness=0, bd=0)
        self.canvas.pack(fill="both", expand=True)

        families = set(tkfont.families(root))

        def pick(*names):
            return next((n for n in names if n in families), "TkDefaultFont")

        self.f_ui = pick("Segoe UI", "Helvetica Neue", "DejaVu Sans", "Arial")
        self.f_bold = pick("Segoe UI Semibold", "Segoe UI", "DejaVu Sans", "Arial")
        self.f_disp = pick("Bahnschrift SemiBold", "Bahnschrift", "Segoe UI Semibold", "DejaVu Sans", "Arial")
        self.f_light = pick("Bahnschrift Light", "Segoe UI Light", "Segoe UI", "DejaVu Sans", "Arial")
        self.f_mono = pick("Consolas", "Cascadia Mono", "DejaVu Sans Mono", "Courier New")
        self._fonts = {}

        self.villages = data.load(data_path)
        self.sel = next((i for i, v in enumerate(self.villages) if v.name == "Берёзовка"), 0)
        self.tab = 1
        self.hits = []
        self.drag = None
        self.hover = None
        self.t0 = time.time()
        self.dirty = True
        self.preset = "Как сейчас"
        self.targets = self.current_factors()
        self._fc_cache = None

        (self.tx, self.ty), (self.vx, self.vy) = data.dataset(self.villages)
        self.reset_training()

        c = self.canvas
        c.bind("<Configure>", lambda e: self.invalidate())
        c.bind("<Button-1>", self.on_down)
        c.bind("<B1-Motion>", self.on_drag)
        c.bind("<ButtonRelease-1>", lambda e: setattr(self, "drag", None))
        c.bind("<Motion>", self.on_move)
        for i in range(4):
            root.bind(str(i + 1), lambda e, i=i: self.set_tab(i))
        root.bind("<Up>", lambda e: self.select((self.sel - 1) % len(self.villages)))
        root.bind("<Down>", lambda e: self.select((self.sel + 1) % len(self.villages)))
        root.bind("<r>", lambda e: self.reset_training())
        self.tick()

    # ------------------------------------------------------------ утилиты
    def s(self, v):
        return int(round(v * self.S))

    def font(self, size, kind="ui"):
        fam = {"ui": self.f_ui, "bold": self.f_bold, "disp": self.f_disp, "light": self.f_light, "mono": self.f_mono}[kind]
        key = (fam, size, kind)
        if key not in self._fonts:
            weight = "bold" if kind == "bold" and fam == self.f_ui else "normal"
            self._fonts[key] = tkfont.Font(root=self.root, family=fam, size=size, weight=weight)
        return self._fonts[key]

    def rrect(self, x0, y0, x1, y1, r, fill, outline="", width=1):
        r = max(0, min(r, (x1 - x0) / 2, (y1 - y0) / 2))
        pts = [x0 + r, y0, x1 - r, y0, x1, y0, x1, y0 + r, x1, y1 - r, x1, y1, x1 - r, y1,
               x0 + r, y1, x0, y1, x0, y1 - r, x0, y0 + r, x0, y0]
        return self.canvas.create_polygon(pts, smooth=True, fill=fill, outline=outline, width=width)

    def card(self, x0, y0, x1, y1, r=None, fill=PANEL, outline=BORDER):
        self.rrect(x0, y0, x1, y1, r or self.s(14), fill, outline)

    def text(self, x, y, s, size=10, color=TEXT, kind="ui", anchor="nw", width=0, justify="left"):
        return self.canvas.create_text(x, y, text=s, fill=color, font=self.font(size, kind), anchor=anchor,
                                       width=width, justify=justify)

    def hit(self, x0, y0, x1, y1, click=None, drag=None, cursor="hand2", key=None):
        self.hits.append((x0, y0, x1, y1, click, drag, cursor, key))

    def invalidate(self):
        self.dirty = True

    def village(self):
        return self.villages[self.sel]

    def current_factors(self):
        v = self.villages[self.sel]
        return {k: v.factors[k][-1] for k in data.FACTORS}

    # ------------------------------------------------------------ обучение
    def reset_training(self):
        self.net = nn.Network(LAYERS, seed=int(time.time()) % 1000, lr=0.012)
        self.training = True
        self.sample = 0
        self.check = None
        self._fc_cache = None
        self.invalidate()

    def train_step(self):
        budget = time.perf_counter() + 0.018
        while self.training and time.perf_counter() < budget:
            tl = self.net.train_epoch(self.tx, self.ty)
            vl = self.net.loss(self.vx, self.vy)
            self.net.history.append((self.net.epoch, tl, vl))
            if self.net.epoch >= EPOCHS:
                self.training = False
                self._fc_cache = None
            if self.net.epoch % 2 == 0:
                break
        if self.net.epoch % 6 == 0 or not self.training:
            self.check = self.replay(self.villages[[v.name for v in self.villages].index("Каменка")])

    def replay(self, v):
        """Нейросеть «проживает» историю села, которого не видела при обучении."""
        pop, youth = float(v.population[0]), v.youth[0]
        pops = [pop]
        for i in range(len(v.years) - 1):
            st = v.state(i)
            st["youth"] = youth
            out = self.net.predict(data.encode(st))
            pop *= 1 + out[0] / data.GROWTH_SCALE
            youth = min(0.6, max(0.08, youth + out[1] / data.YOUTH_SCALE))
            pops.append(pop)
        err = sum(abs(a - b) / b for a, b in zip(pops, v.population)) / len(pops)
        return v, pops, err

    def forecast(self):
        key = (self.sel, tuple(round(self.targets[k], 4) for k in data.FACTORS), self.net.epoch)
        if self._fc_cache and self._fc_cache[0] == key:
            return self._fc_cache[1]
        v = self.village()
        years, pops, youths = data.forecast(self.net, v, self.targets)
        imp = data.importance(self.net, v, self.targets)
        sigma = math.sqrt(max(1e-6, self.net.history[-1][2])) / data.GROWTH_SCALE if self.net.history else 0.01
        band = [(p * math.exp(-(1.64 * sigma * math.sqrt(k) + 0.004 * k)), p * math.exp(1.64 * sigma * math.sqrt(k) + 0.004 * k))
                for k, p in enumerate(pops)]
        res = (years, pops, youths, imp, band)
        self._fc_cache = (key, res)
        return res

    # ------------------------------------------------------------ события
    def set_tab(self, i):
        self.tab = i
        self.invalidate()

    def select(self, i):
        self.sel = i
        if self.preset == "Как сейчас" or self.preset is None:
            self.targets = self.current_factors()
            self.preset = "Как сейчас"
        self._fc_cache = None
        self.invalidate()

    def apply_preset(self, name, values):
        self.preset = name
        self.targets = dict(values) if values else self.current_factors()
        self.invalidate()

    def find_hit(self, x, y):
        for h in reversed(self.hits):
            if h[0] <= x <= h[2] and h[1] <= y <= h[3]:
                return h
        return None

    def on_down(self, e):
        h = self.find_hit(e.x, e.y)
        if not h:
            return
        if h[5]:
            self.drag = h[5]
            h[5](e.x, e.y)
        elif h[4]:
            h[4]()
        self.invalidate()

    def on_drag(self, e):
        if self.drag:
            self.drag(e.x, e.y)
            self.invalidate()

    def on_move(self, e):
        h = self.find_hit(e.x, e.y)
        self.canvas.configure(cursor=h[6] if h else "")
        key = h[7] if h else None
        if key != self.hover:
            self.hover = key
            self.invalidate()

    # ------------------------------------------------------------ цикл
    def tick(self):
        if self.training:
            self.train_step()
            self.dirty = True
        if self.tab == 1:
            self.dirty = True     # анимация сигналов в нейросети
        if self.dirty:
            self.draw()
            self.dirty = False
        self.root.after(33, self.tick)

    # ------------------------------------------------------------ отрисовка
    def draw(self):
        c = self.canvas
        c.delete("all")
        self.hits = []
        W, H = c.winfo_width(), c.winfo_height()
        if W < 50:
            return
        self.draw_header(W)
        side = self.s(270)
        top = self.s(84)
        self.draw_sidebar(self.s(16), top, side, H - self.s(16))
        x0, y0, x1, y1 = side + self.s(30), top, W - self.s(16), H - self.s(16)
        [self.draw_chronicle, self.draw_network, self.draw_forecast, self.draw_about][self.tab](x0, y0, x1, y1)

    def draw_header(self, W):
        c, s = self.canvas, self.s
        # логотип — маленькая нейросеть
        cx, cy = s(42), s(42)
        nodes = [(cx - s(16), cy - s(10)), (cx - s(16), cy + s(10)), (cx, cy - s(16)), (cx, cy), (cx, cy + s(16)), (cx + s(16), cy)]
        for a in nodes[:2]:
            for b in nodes[2:5]:
                c.create_line(*a, *b, fill=mix(CYAN, BG, 0.5), width=1)
        for b in nodes[2:5]:
            c.create_line(*b, *nodes[5], fill=mix(GOLD, BG, 0.4), width=1)
        t = time.time() - self.t0
        for i, (x, y) in enumerate(nodes):
            glow = 0.5 + 0.5 * math.sin(t * 3 + i)
            col = mix(GOLD if i == 5 else CYAN, TEXT, 0.2 * glow)
            c.create_oval(x - s(4), y - s(4), x + s(4), y + s(4), fill=col, outline="")
        self.text(s(76), s(20), "НЕЙРОЛЕТОПИСЬ МАЛОЙ РОДИНЫ", 17, TEXT, "disp")
        self.text(s(77), s(50), "Нейросеть, написанная с нуля на Python, изучает историю сёл района и прогнозирует их судьбу до 2050 года",
                  9, MUTED)
        # вкладки
        x = W - s(16)
        for i in range(len(TABS) - 1, -1, -1):
            label = f"{i + 1}  {TABS[i]}"
            w = self.font(10, "bold").measure(label) + s(34)
            x -= w
            sel = i == self.tab
            hov = self.hover == ("tab", i)
            fill = mix(GOLD, BG, 0.25) if sel else (PANEL2 if hov else PANEL)
            self.rrect(x, s(22), x + w, s(58), s(18), fill, GOLD if sel else BORDER)
            self.text(x + w / 2, s(40), label, 10, BG if sel else TEXT, "bold", anchor="center")
            self.hit(x, s(22), x + w, s(58), click=lambda i=i: self.set_tab(i), key=("tab", i))
            x -= s(8)

    def draw_sidebar(self, x0, y0, w, y1):
        c, s = self.canvas, self.s
        self.text(x0 + s(4), y0, "СЁЛА РАЙОНА", 8, MUTED, "bold")
        self.text(x0 + w, y0, "1995 → 2025", 8, mix(MUTED, BG, 0.3), anchor="ne")
        y = y0 + s(22)
        status_h = s(118)
        n = len(self.villages)
        ch = min(s(56), (y1 - status_h - s(12) - y) / n - s(6))
        for i, v in enumerate(self.villages):
            sel = i == self.sel
            hov = self.hover == ("v", i)
            fill = mix(GOLD, PANEL, 0.86) if sel else (PANEL2 if hov else PANEL)
            self.card(x0, y, x0 + w, y + ch, s(10), fill, GOLD if sel else BORDER)
            p0, p1 = v.population[0], v.population[-1]
            d = (p1 / p0 - 1) * 100
            self.text(x0 + s(12), y + ch / 2 - s(9), v.name, 10, TEXT, "bold", anchor="w")
            self.text(x0 + s(12), y + ch / 2 + s(10), f"{p1} жит.", 8, MUTED, anchor="w")
            # спарклайн
            sx0, sx1 = x0 + w - s(122), x0 + w - s(52)
            mn, mx = min(v.population), max(v.population)
            pts = []
            for k, p in enumerate(v.population):
                pts += [sx0 + (sx1 - sx0) * k / (len(v.population) - 1), y + ch - s(10) - (p - mn) / max(1, mx - mn) * (ch - s(20))]
            col = GREEN if d >= 0 else (GOLD if d > -40 else CORAL)
            c.create_line(pts, fill=col, width=2, smooth=True)
            self.text(x0 + w - s(10), y + ch / 2, f"{d:+.0f}%", 9, col, "bold", anchor="e")
            self.hit(x0, y, x0 + w, y + ch, click=lambda i=i: self.select(i), key=("v", i))
            y += ch + s(6)
        # статус нейросети
        sy = y1 - status_h
        self.card(x0, sy, x0 + w, y1, s(12))
        self.text(x0 + s(14), sy + s(12), "НЕЙРОСЕТЬ", 8, MUTED, "bold")
        st = "обучается…" if self.training else "обучена ✓"
        self.text(x0 + w - s(14), sy + s(12), st, 8, GOLD if self.training else GREEN, "bold", anchor="ne")
        p = self.net.epoch / EPOCHS
        self.rrect(x0 + s(14), sy + s(36), x0 + w - s(14), sy + s(42), s(3), PANEL2)
        self.rrect(x0 + s(14), sy + s(36), x0 + s(14) + max(s(6), (w - s(28)) * p), sy + s(42), s(3), GOLD if self.training else GREEN)
        loss = self.net.history[-1][2] if self.net.history else float("nan")
        self.text(x0 + s(14), sy + s(52), f"эпоха {self.net.epoch}/{EPOCHS}   ошибка {loss:.4f}", 8, TEXT, "mono")
        self.text(x0 + s(14), sy + s(72), f"слои {'·'.join(map(str, LAYERS))}   параметров {self.net.weight_count()}", 8, MUTED, "mono")
        bx0, by0, bx1, by1 = x0 + s(14), sy + s(92), x0 + w - s(14), sy + s(110)
        hov = self.hover == "retrain"
        self.text((bx0 + bx1) / 2, (by0 + by1) / 2, "↻  обучить заново  (R)", 8, CYAN if hov else mix(CYAN, BG, 0.25), "bold", anchor="center")
        self.hit(bx0, by0, bx1, by1, click=self.reset_training, key="retrain")

    # ------------------------------------------------------------ графики
    def axes(self, x0, y0, x1, y1, ymin, ymax, years, ticks=None, fmt="{:.0f}"):
        c = self.canvas
        for k in range(4):
            v = ymin + (ymax - ymin) * k / 3
            y = y1 - (y1 - y0) * k / 3
            c.create_line(x0, y, x1, y, fill=mix(BORDER, BG, 0.3))
            self.text(x0 - self.s(6), y, fmt.format(v), 8, MUTED, anchor="e")
        for yr in ticks or []:
            x = x0 + (x1 - x0) * (yr - years[0]) / (years[1] - years[0])
            self.text(x, y1 + self.s(6), str(yr), 8, MUTED, anchor="n")

    def mapper(self, x0, y0, x1, y1, ymin, ymax, ya, yb):
        def X(year):
            return x0 + (x1 - x0) * (year - ya) / (yb - ya)

        def Y(v):
            return y1 - (y1 - y0) * (v - ymin) / max(1e-9, ymax - ymin)
        return X, Y

    def legend(self, x_right, y, items):
        """Легенда справа налево: [(цвет, подпись, пунктир?)]."""
        x = x_right
        for col, label, dashed in reversed(items):
            w = self.font(8).measure(label)
            x -= w
            self.text(x, y, label, 8, MUTED, anchor="w")
            x -= self.s(22)
            self.canvas.create_line(x, y, x + self.s(16), y, fill=col, width=3, dash=(4, 3) if dashed else None)
            x -= self.s(14)

    def glow_line(self, pts, color, base=PANEL, width=2):
        c = self.canvas
        c.create_line(pts, fill=mix(color, base, 0.86), width=width + self.s(8), capstyle="round", joinstyle="round")
        c.create_line(pts, fill=mix(color, base, 0.65), width=width + self.s(4), capstyle="round", joinstyle="round")
        c.create_line(pts, fill=color, width=width, capstyle="round", joinstyle="round")

    # ------------------------------------------------------------ вкладка 1
    def draw_chronicle(self, x0, y0, x1, y1):
        s, c, v = self.s, self.canvas, self.village()
        p0, p1 = v.population[0], v.population[-1]
        self.text(x0, y0 - s(4), v.name, 24, TEXT, "disp")
        self.text(x0 + self.font(24, "disp").measure(v.name) + s(16), y0 + s(10),
                  f"1995–2025 · население {p0} → {p1} ({(p1 / p0 - 1) * 100:+.0f}%) · молодёжь {v.youth[0] * 100:.0f}% → {v.youth[-1] * 100:.0f}%",
                  10, MUTED)
        top = y0 + s(48)
        ev_w = s(300)
        chart_h = (y1 - top) * 0.56
        # большой график
        cx0, cy0, cx1, cy1 = x0, top, x1 - ev_w - s(14), top + chart_h
        self.card(cx0, cy0, cx1, cy1)
        self.text(cx0 + s(16), cy0 + s(12), "НАСЕЛЕНИЕ И МОЛОДЁЖЬ", 8, MUTED, "bold")
        self.legend(cx1 - s(16), cy0 + s(19), [(GOLD, "население", False), (VIOLET, "доля молодёжи (правая шкала)", False)])
        gx0, gy0, gx1, gy1 = cx0 + s(56), cy0 + s(44), cx1 - s(24), cy1 - s(34)
        mx = max(v.population) * 1.12
        self.axes(gx0, gy0, gx1, gy1, 0, mx, (1995, 2025), [1995, 2000, 2005, 2010, 2015, 2020, 2025])
        X, Y = self.mapper(gx0, gy0, gx1, gy1, 0, mx, 1995, 2025)
        events = v.events()
        for yr, label, key in events:
            col = FACTOR_COLORS.get(key, GOLD)
            c.create_line(X(yr), gy0, X(yr), gy1, fill=mix(col, PANEL, 0.6), dash=(3, 4))
            c.create_oval(X(yr) - s(4), gy0 - s(4), X(yr) + s(4), gy0 + s(4), fill=col, outline="")
        pts = []
        for yr, p in zip(v.years, v.population):
            pts += [X(yr), Y(p)]
        c.create_polygon([gx0, gy1] + pts + [gx1, gy1], fill=mix(GOLD, PANEL, 0.88), outline="")
        self.glow_line(pts, GOLD, width=3)
        Yy = lambda t: gy1 - (gy1 - gy0) * t / 0.6
        ypts = []
        for yr, yv in zip(v.years, v.youth):
            ypts += [X(yr), Yy(yv)]
        c.create_line(ypts, fill=VIOLET, width=2, smooth=True)
        self.text(gx1 + s(4), Yy(0.6), "60%", 8, VIOLET)
        # летопись событий
        ex0 = x1 - ev_w
        self.card(ex0, cy0, x1, cy1)
        self.text(ex0 + s(16), cy0 + s(12), "ЛЕТОПИСЬ ИЗ ДАННЫХ", 8, MUTED, "bold")
        ey = cy0 + s(40)
        for yr, label, key in events:
            col = FACTOR_COLORS.get(key, GOLD)
            self.rrect(ex0 + s(14), ey, ex0 + s(64), ey + s(24), s(12), col)
            self.text(ex0 + s(39), ey + s(12), str(yr), 9, BG, "bold", anchor="center")
            self.text(ex0 + s(74), ey + s(12), label, 9, TEXT, anchor="w", width=ev_w - s(88))
            ey += s(36)
            if ey > cy1 - s(30):
                break
        # мини-графики технологий
        gtop = cy1 + s(14)
        cols, rows = 3, 2
        gw = (x1 - x0 - s(14) * (cols - 1)) / cols
        gh = (y1 - gtop - s(14) * (rows - 1)) / rows
        for i, k in enumerate(data.FACTORS):
            r, col_i = divmod(i, cols)
            bx0 = x0 + col_i * (gw + s(14))
            by0 = gtop + r * (gh + s(14))
            col = FACTOR_COLORS[k]
            self.card(bx0, by0, bx0 + gw, by0 + gh, s(12))
            self.text(bx0 + s(14), by0 + s(10), data.FACTOR_NAMES[k], 10, TEXT, "bold")
            self.text(bx0 + s(14), by0 + s(30), data.FACTOR_HINTS[k], 8, MUTED)
            val = v.factors[k][-1]
            self.text(bx0 + gw - s(14), by0 + s(8), f"{val * 100:.0f}%", 16, col, "disp", anchor="ne")
            mx0, my0, mx1, my1 = bx0 + s(14), by0 + s(52), bx0 + gw - s(14), by0 + gh - s(12)
            if my1 - my0 < s(14):
                continue
            Xm, Ym = self.mapper(mx0, my0, mx1, my1, 0, 1, 1995, 2025)
            mp = []
            for yr, fv in zip(v.years, v.factors[k]):
                mp += [Xm(yr), Ym(fv)]
            c.create_polygon([mx0, my1] + mp + [mx1, my1], fill=mix(col, PANEL, 0.8), outline="")
            c.create_line(mp, fill=col, width=2)

    # ------------------------------------------------------------ вкладка 2
    def draw_network(self, x0, y0, x1, y1):
        s, c = self.s, self.canvas
        t = time.time() - self.t0
        nw = (x1 - x0) * 0.58
        self.card(x0, y0, x0 + nw, y1)
        self.text(x0 + s(18), y0 + s(14), "КАК ДУМАЕТ НЕЙРОСЕТЬ", 8, MUTED, "bold")
        self.text(x0 + s(18), y0 + s(32), "7 признаков села → 2 скрытых слоя → прогноз на следующий год", 10, TEXT)
        # пример для визуализации — меняется раз в полсекунды
        idx = int(t * 2) % len(self.tx)
        acts = self.net.forward(self.tx[idx])
        n_layers = len(LAYERS)
        lx0, lx1 = x0 + s(150), x0 + nw - s(130)
        ly0, ly1 = y0 + s(78), y1 - s(60)
        pos = []
        for li, n in enumerate(LAYERS):
            x = lx0 + (lx1 - lx0) * li / (n_layers - 1)
            gap = min(s(46), (ly1 - ly0) / max(1, n))
            yc = (ly0 + ly1) / 2
            pos.append([(x, yc + (j - (n - 1) / 2) * gap) for j in range(n)])
        # связи
        strong = []
        for li, layer in enumerate(self.net.layers):
            for j, row in enumerate(layer.w):
                for i, w in enumerate(row):
                    a, b = pos[li][i], pos[li + 1][j]
                    mag = min(1.0, abs(w) / 1.5)
                    col = CYAN if w > 0 else CORAL
                    c.create_line(*a, *b, fill=mix(col, PANEL, 1 - (0.12 + 0.6 * mag)), width=1 + int(mag * 2.5))
                    flow = abs(w * acts[li][i])
                    strong.append((flow, a, b, col))
        strong.sort(key=lambda q: -q[0])
        for k, (flow, a, b, col) in enumerate(strong[:26]):
            ph = (t * 0.9 + k * 0.137) % 1.0
            px, py = a[0] + (b[0] - a[0]) * ph, a[1] + (b[1] - a[1]) * ph
            r = s(2.5) + s(1.5) * min(1, flow)
            c.create_oval(px - r, py - r, px + r, py + r, fill=mix(col, TEXT, 0.4), outline="")
        # нейроны
        for li, layer_pos in enumerate(pos):
            for j, (x, y) in enumerate(layer_pos):
                a = acts[li][j]
                if li == 0:   # вход — сама величина признака 0…100%
                    col = mix(PANEL2, GOLD, (a + 1) / 2)
                else:
                    col = mix(PANEL2, CYAN if a > 0 else CORAL, min(1, abs(a)))
                r = s(13) if li in (0, n_layers - 1) else s(10)
                if abs(a) > 0.6 and li > 0:
                    c.create_oval(x - r - s(5), y - r - s(5), x + r + s(5), y + r + s(5), fill=mix(col, PANEL, 0.7), outline="")
                c.create_oval(x - r, y - r, x + r, y + r, fill=col, outline=mix(TEXT, PANEL, 0.5), width=1)
                if li == 0:
                    self.text(x - r - s(10), y, data.INPUT_NAMES[j], 9, TEXT, anchor="e")
                    raw = (self.tx[idx][j] + 1) / 2
                    self.text(x - r - s(10), y + s(14), f"{raw * 100:.0f}%", 8, MUTED, "mono", anchor="e")
                if li == n_layers - 1:
                    self.text(x + r + s(10), y - s(7), data.OUTPUT_NAMES[j], 9, GOLD, "bold", anchor="w")
                    val = a / (data.GROWTH_SCALE if j == 0 else data.YOUTH_SCALE) * 100
                    self.text(x + r + s(10), y + s(10), f"{val:+.2f}%/год", 8, MUTED, "mono", anchor="w")
        names = ["вход", "скрытый слой 1", "скрытый слой 2", "выход"]
        for li, layer_pos in enumerate(pos):
            self.text(layer_pos[0][0], ly1 + s(26), names[li], 8, MUTED, anchor="center")
        self.text(x0 + s(18), y1 - s(22), "голубые связи — усиливают, красные — ослабляют; яркость — сила связи", 8, mix(MUTED, PANEL, 0.2))

        # кривая обучения
        rx0 = x0 + nw + s(14)
        mid = y0 + (y1 - y0) * 0.46
        self.card(rx0, y0, x1, mid)
        self.text(rx0 + s(16), y0 + s(14), "ОБУЧЕНИЕ", 8, MUTED, "bold")
        loss = self.net.history[-1] if self.net.history else (0, 0, 0)
        self.text(x1 - s(16), y0 + s(12), f"эпоха {self.net.epoch}", 10, GOLD if self.training else GREEN, "bold", anchor="ne")
        gx0, gy0, gx1, gy1 = rx0 + s(56), y0 + s(48), x1 - s(20), mid - s(34)
        hist = self.net.history
        ymax = max([h[1] for h in hist[2:]] + [h[2] for h in hist[2:]] + [0.02]) * 1.1
        self.axes(gx0, gy0, gx1, gy1, 0, ymax, (0, EPOCHS), [0, EPOCHS // 2, EPOCHS], "{:.3f}")
        X, Y = self.mapper(gx0, gy0, gx1, gy1, 0, ymax, 0, EPOCHS)
        if len(hist) > 2:
            tl, vl = [], []
            ea, eb = hist[0][1], hist[0][2]
            for e, a, b in hist:
                ea, eb = ea * 0.8 + a * 0.2, eb * 0.8 + b * 0.2   # сглаживание для наглядности
                tl += [X(e), Y(min(ea, ymax))]
                vl += [X(e), Y(min(eb, ymax))]
            c.create_line(vl, fill=GOLD, width=2, smooth=True)
            self.glow_line(tl, CYAN, width=2)
        self.legend(gx1, gy0 - s(12), [(CYAN, f"обучение {loss[1]:.4f}", False), (GOLD, f"проверка {loss[2]:.4f}", False)])

        # проверка на незнакомом селе
        by0 = mid + s(14)
        self.card(rx0, by0, x1, y1)
        self.text(rx0 + s(16), by0 + s(14), "ЭКЗАМЕН: СЕЛО, КОТОРОГО НЕЙРОСЕТЬ НЕ ВИДЕЛА", 8, MUTED, "bold")
        if self.check:
            v, pops, err = self.check
            acc = max(0.0, 100 - err * 100)
            self.text(rx0 + s(16), by0 + s(34), f"{v.name}: нейросеть «прожила» 30 лет по одним только технологиям", 9, TEXT,
                      width=x1 - rx0 - s(32))
            gx0, gy0, gx1, gy1 = rx0 + s(56), by0 + s(84), x1 - s(20), y1 - s(52)
            mx = max(max(v.population), max(pops)) * 1.1
            self.axes(gx0, gy0, gx1, gy1, 0, mx, (1995, 2025), [1995, 2010, 2025])
            X, Y = self.mapper(gx0, gy0, gx1, gy1, 0, mx, 1995, 2025)
            real, pred = [], []
            for yr, a, b in zip(v.years, v.population, pops):
                real += [X(yr), Y(a)]
                pred += [X(yr), Y(b)]
            c.create_line(real, fill=GOLD, width=3)
            c.create_line(pred, fill=CYAN, width=2, dash=(6, 4))
            self.text(rx0 + s(16), y1 - s(30), "— факт", 9, GOLD, "bold")
            self.text(rx0 + s(80), y1 - s(30), "- - нейросеть", 9, CYAN, "bold")
            self.text(x1 - s(16), y1 - s(32), f"точность {acc:.1f}%", 13, GREEN if acc > 90 else GOLD, "disp", anchor="ne")

    # ------------------------------------------------------------ вкладка 3
    def draw_forecast(self, x0, y0, x1, y1):
        s, c, v = self.s, self.canvas, self.village()
        lw = s(320)
        # --- ползунки ---
        self.card(x0, y0, x0 + lw, y1)
        self.text(x0 + s(16), y0 + s(14), "ТЕХНОЛОГИИ К 2035 ГОДУ", 8, MUTED, "bold")
        px, py = x0 + s(14), y0 + s(38)
        pw = (lw - s(28) - s(8)) / 2
        for i, (name, vals, col) in enumerate(PRESETS):
            bx = px + (i % 2) * (pw + s(8))
            by = py + (i // 2) * s(36)
            sel = self.preset == name
            hov = self.hover == ("preset", i)
            self.rrect(bx, by, bx + pw, by + s(28), s(14), mix(col, PANEL, 0.75) if sel else (PANEL2 if hov else PANEL), col if sel or hov else BORDER)
            self.text(bx + pw / 2, by + s(14), name, 9, TEXT if sel else mix(TEXT, PANEL, 0.15), "bold", anchor="center")
            self.hit(bx, by, bx + pw, by + s(28), click=lambda n=name, vv=vals: self.apply_preset(n, vv), key=("preset", i))
        sy = py + s(84)
        sh = (y1 - s(16) - sy) / len(data.FACTORS)
        cur = self.current_factors()
        for k in data.FACTORS:
            col = FACTOR_COLORS[k]
            val = self.targets[k]
            self.text(x0 + s(16), sy, data.FACTOR_NAMES[k], 10, TEXT, "bold")
            self.text(x0 + lw - s(16), sy, f"{val * 100:.0f}%", 11, col, "disp", anchor="ne")
            self.text(x0 + s(16), sy + s(20), data.FACTOR_HINTS[k], 8, MUTED)
            tx0, tx1, ty = x0 + s(22), x0 + lw - s(22), sy + s(48)
            self.rrect(tx0, ty - s(3), tx1, ty + s(3), s(3), PANEL2)
            self.rrect(tx0, ty - s(3), tx0 + max(s(6), (tx1 - tx0) * val), ty + s(3), s(3), col)
            cx = tx0 + (tx1 - tx0) * cur[k]
            c.create_line(cx, ty - s(9), cx, ty + s(9), fill=MUTED, width=2)
            kx = tx0 + (tx1 - tx0) * val
            hov = self.hover == ("slider", k)
            r = s(10) if hov else s(8)
            c.create_oval(kx - r, ty - r, kx + r, ty + r, fill=TEXT, outline=col, width=3)

            def drag(x, y, k=k, tx0=tx0, tx1=tx1):
                self.targets[k] = max(0.0, min(1.0, (x - tx0) / (tx1 - tx0)))
                self.preset = None
            self.hit(tx0 - s(10), ty - s(14), tx1 + s(10), ty + s(14), drag=drag, cursor="sb_h_double_arrow", key=("slider", k))
            sy += sh

        # --- график прогноза ---
        rw = s(330)
        gx0c, gx1c = x0 + lw + s(14), x1 - rw - s(14)
        self.card(gx0c, y0, gx1c, y1)
        self.text(gx0c + s(16), y0 + s(14), f"ПРОГНОЗ НЕЙРОСЕТИ · {v.name.upper()} · 1995–2050", 8, MUTED, "bold")
        if self.training and self.net.epoch < 60:
            self.text((gx0c + gx1c) / 2, (y0 + y1) / 2, f"Нейросеть обучается… {self.net.epoch * 100 // EPOCHS}%\nпрогноз появится через пару секунд",
                      13, GOLD, "disp", anchor="center", justify="center")
            return
        years, pops, youths, imp, band = self.forecast()
        name, desc = data.fate(pops[-1] / pops[0])
        fcol = FATE_COLORS[name]
        gx0, gy0, gx1, gy1 = gx0c + s(60), y0 + s(54), gx1c - s(40), y1 - s(62)
        mx = max(max(v.population), max(b[1] for b in band)) * 1.1
        self.axes(gx0, gy0, gx1, gy1, 0, mx, (1995, 2050), [1995, 2005, 2015, 2025, 2035, 2050])
        X, Y = self.mapper(gx0, gy0, gx1, gy1, 0, mx, 1995, 2050)
        c.create_rectangle(X(2025), gy0, gx1, gy1, fill=mix(fcol, PANEL, 0.95), outline="")
        c.create_line(X(2025), gy0 - s(6), X(2025), gy1, fill=GOLD, dash=(3, 3))
        self.text(X(2025) + s(6), gy0 - s(4), "сегодня", 8, GOLD)
        c.create_line(X(2035), gy0, X(2035), gy1, fill=mix(MUTED, PANEL, 0.6), dash=(2, 5))
        self.text(X(2035) + s(6), gy0 - s(4), "технологии внедрены", 8, MUTED)
        poly = [X(y) for y in years]
        top = [val for yv, b in zip(years, band) for val in (X(yv), Y(b[1]))]
        bot = [val for yv, b in reversed(list(zip(years, band))) for val in (X(yv), Y(b[0]))]
        c.create_polygon(top + bot, fill=mix(fcol, PANEL, 0.82), outline="")
        hist = []
        for yr, p in zip(v.years, v.population):
            hist += [X(yr), Y(p)]
        c.create_polygon([X(1995), gy1] + hist + [X(2025), gy1], fill=mix(GOLD, PANEL, 0.9), outline="")
        c.create_line(hist, fill=GOLD, width=3)
        fpts = []
        for yr, p in zip(years, pops):
            fpts += [X(yr), Y(p)]
        self.glow_line(fpts, fcol, width=3)
        ex, ey = X(2050), Y(pops[-1])
        c.create_oval(ex - s(6), ey - s(6), ex + s(6), ey + s(6), fill=fcol, outline=TEXT, width=2)
        self.text(ex, ey - s(14), f"{pops[-1]:.0f}", 14, fcol, "disp", anchor="s")
        self.legend(gx1c - s(16), y1 - s(20), [(GOLD, "история", False), (fcol, "прогноз нейросети", False),
                                                (mix(fcol, PANEL, 0.6), "коридор неопределённости", False)])

        # --- вердикт ---
        vx0 = x1 - rw
        vh = s(214)
        self.card(vx0, y0, x1, y0 + vh, outline=fcol)
        self.text(vx0 + s(18), y0 + s(14), "СУДЬБА К 2050 ГОДУ", 8, MUTED, "bold")
        self.text(vx0 + s(18), y0 + s(34), name, 22 if len(name) < 14 else 15, fcol, "disp")
        self.text(vx0 + s(18), y0 + s(78), desc, 9, TEXT, width=rw - s(36))
        ratio = pops[-1] / pops[0]
        self.text(vx0 + s(18), y0 + s(128), "Население", 8, MUTED)
        self.text(vx0 + s(18), y0 + s(144), f"{pops[0]:.0f} → {pops[-1]:.0f}  ({(ratio - 1) * 100:+.0f}%)", 12, TEXT, "disp")
        self.text(vx0 + rw / 2 + s(16), y0 + s(128), "Молодёжь", 8, MUTED)
        self.text(vx0 + rw / 2 + s(16), y0 + s(144), f"{youths[0] * 100:.0f}% → {youths[-1] * 100:.0f}%", 12, TEXT, "disp")
        life = max(0, min(100, 45 + 45 * (ratio - 1) + 80 * (youths[-1] - 0.2)))
        self.rrect(vx0 + s(18), y0 + vh - s(18), x1 - s(18), y0 + vh - s(12), s(3), PANEL2)
        self.rrect(vx0 + s(18), y0 + vh - s(18), vx0 + s(18) + max(s(6), (rw - s(36)) * life / 100), y0 + vh - s(12), s(3), fcol)
        self.text(vx0 + s(18), y0 + vh - s(24), "ИНДЕКС ЖИЗНИ", 7, MUTED, "bold", anchor="sw")
        self.text(x1 - s(18), y0 + vh - s(24), f"{life:.0f} / 100", 9, fcol, "bold", anchor="se")

        # --- объяснимый ИИ ---
        iy0 = y0 + vh + s(14)
        self.card(vx0, iy0, x1, y1)
        self.text(vx0 + s(18), iy0 + s(14), "ЧТО ВАЖНЕЕ ВСЕГО · ОБЪЯСНИМЫЙ ИИ", 8, MUTED, "bold")
        self.text(vx0 + s(18), iy0 + s(32), "сколько жителей добавит к 2050 году +10% каждого фактора", 8, mix(MUTED, PANEL, 0.2),
                  width=rw - s(36))
        items = sorted(imp.items(), key=lambda kv: -1e9 if kv[1] is None else -kv[1])
        mxv = max([1.0] + [abs(val) for _, val in items if val is not None])
        by = iy0 + s(62)
        row_h = min(s(54), (y1 - by - s(76)) / len(items))
        for k, val in items:
            col = FACTOR_COLORS[k]
            self.text(vx0 + s(18), by, data.FACTOR_NAMES[k], 9, TEXT)
            if val is None:
                self.text(x1 - s(18), by, "уже максимум ✓", 9, MUTED, "bold", anchor="ne")
                self.rrect(vx0 + s(18), by + s(18), x1 - s(18), by + s(24), s(3), mix(col, PANEL, 0.75))
            else:
                self.text(x1 - s(18), by, f"{round(val):+d} жит.", 9, col if round(val) >= 0 else CORAL, "bold", anchor="ne")
                bw = (rw - s(36)) * abs(val) / mxv
                self.rrect(vx0 + s(18), by + s(18), vx0 + s(18) + max(s(4), bw), by + s(24), s(3), col)
            by += row_h
        best = next((k for k, val in items if val is not None), items[0][0])
        tip = {
            "internet": "подключить к быстрому интернету каждый дом",
            "digital": "развивать телемедицину и онлайн-образование",
            "remote": "создать условия для удалённой работы: коворкинг, связь",
            "smart": "запустить умное фермерство — это рабочие места",
            "roads": "отремонтировать дорогу и вернуть автобус",
            "heritage": "открыть музей и событийный туризм",
        }[best]
        self.rrect(vx0 + s(14), y1 - s(62), x1 - s(14), y1 - s(12), s(10), mix(GOLD, PANEL, 0.88), mix(GOLD, PANEL, 0.5))
        self.text(vx0 + s(26), y1 - s(37), f"Совет нейросети: {tip}.", 9, GOLD, "bold", anchor="w", width=rw - s(52))

    # ------------------------------------------------------------ вкладка 4
    def draw_about(self, x0, y0, x1, y1):
        s = self.s
        self.card(x0, y0, x1, y1)
        cx = x0 + s(36)
        self.text(cx, y0 + s(26), "Как технологии меняют судьбу малой родины — и может ли нейросеть это понять?", 18, TEXT, "disp",
                  width=x1 - x0 - s(72))
        blocks = [
            (GOLD, "1. Данные", "10 сёл района, 30 лет наблюдений (1995–2025): население, доля молодёжи и шесть технологических факторов. "
                                "Таблица data/district.csv открывается в Excel — впишите туда данные своего села."),
            (CYAN, "2. Нейросеть с нуля", "Многослойный перцептрон 7 → 12 → 10 → 2 на чистом Python: прямой проход, обратное "
                                       "распространение ошибки, оптимизатор Adam. Ни одной сторонней библиотеки — каждую строчку можно прочитать."),
            (VIOLET, "3. Экзамен", "Два села отложены и не участвуют в обучении. Нейросеть «проживает» их историю по одним технологиям — "
                                   "и совпадает с реальностью с точностью около 95%."),
            (GREEN, "4. Прогноз и выбор", "Двигайте ползунки: нейросеть год за годом рассчитывает судьбу села до 2050 года и "
                                          "объясняет, какая технология важнее всего (объяснимый ИИ)."),
        ]
        bw = (x1 - x0 - s(72) - s(16) * 3) / 4
        by0 = y0 + s(110)
        for i, (col, title, body) in enumerate(blocks):
            bx = cx + i * (bw + s(16))
            self.rrect(bx, by0, bx + bw, by0 + s(220), s(12), PANEL2, mix(col, PANEL, 0.4))
            self.rrect(bx + s(16), by0 + s(16), bx + s(56), by0 + s(20), s(2), col)
            self.text(bx + s(16), by0 + s(30), title, 12, col, "disp")
            self.text(bx + s(16), by0 + s(62), body, 9, TEXT, width=bw - s(32))
        # конвейер
        py = by0 + s(250)
        steps = [("Таблица CSV", GOLD), ("Нормализация", MUTED), ("Нейросеть 7·12·10·2", CYAN), ("Шаг за шагом до 2050", VIOLET),
                 ("Судьба + советы ИИ", GREEN)]
        sw = (x1 - x0 - s(72) - s(28) * (len(steps) - 1)) / len(steps)
        for i, (label, col) in enumerate(steps):
            bx = cx + i * (sw + s(28))
            self.rrect(bx, py, bx + sw, py + s(44), s(22), mix(col, PANEL, 0.82), col)
            self.text(bx + sw / 2, py + s(22), label, 10, TEXT, "bold", anchor="center")
            if i < len(steps) - 1:
                ax = bx + sw + s(6)
                self.canvas.create_line(ax, py + s(22), ax + s(16), py + s(22), fill=MUTED, width=2, arrow="last")
        # формула нейрона
        fy = py + s(70)
        self.text(cx, fy, "Один нейрон", 12, CYAN, "disp")
        self.text(cx, fy + s(28), "y = tanh( w₁·x₁ + w₂·x₂ + … + w₇·x₇ + b )", 13, TEXT, "mono")
        self.text(cx, fy + s(56), "x — признаки села, w — веса (их и подбирает обучение), b — смещение, tanh — «мягкий переключатель» от −1 до 1.",
                  9, MUTED, width=(x1 - x0) / 2 - s(40))
        self.text(cx + (x1 - x0) / 2, fy, "Как учится сеть", 12, VIOLET, "disp")
        self.text(cx + (x1 - x0) / 2, fy + s(28),
                  "1) делает прогноз  2) сравнивает с фактом  3) по ошибке подкручивает каждый из 248 весов (обратное распространение)  "
                  "4) повторяет 240 раз — пока ошибка не станет минимальной.", 9, TEXT, width=(x1 - x0) / 2 - s(72))
        ty = fy + s(120)
        self.text(cx, ty, "Главный вывод", 12, GOLD, "disp")
        self.text(cx, ty + s(28),
                  "Технологии сами по себе не спасают и не губят село. Решает то, работают ли они на людей: удалённая работа, умное "
                  "фермерство и цифровые сервисы удерживают молодёжь, а цифровая память и туризм возвращают селу лицо. "
                  "Нейросеть позволяет увидеть это заранее — пока будущее ещё можно выбрать.",
                  11, TEXT, width=x1 - x0 - s(72))
        self.text(cx, y1 - s(56), "Клавиши: 1–4 — вкладки · ↑↓ — выбор села · R — обучить заново", 9, MUTED)
        self.text(cx, y1 - s(34), "Данные демонстрационные (сгенерированы моделью района) — замените их реальными, и нейросеть обучится на них.",
                  9, MUTED)


def run():
    if os.name == "nt":
        try:
            import ctypes
            ctypes.windll.shcore.SetProcessDpiAwareness(1)
        except Exception:
            pass
    root = tk.Tk()
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    App(root, os.path.join(here, "data", "district.csv"))
    root.mainloop()
