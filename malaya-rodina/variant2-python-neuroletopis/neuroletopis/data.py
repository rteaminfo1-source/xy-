"""
Данные района: 10 сёл, 1995–2025 годы.

Файл data/district.csv можно открыть в Excel и заменить своими данными:
    village, year, population, youth, internet, digital, remote, smart, roads, heritage
Все факторы — от 0 до 1 (доля жителей / уровень развития).

Если файла нет, он создаётся генератором ниже. Генератор — это «скрытый закон»
развития района: нейросеть его НЕ видит, она восстанавливает зависимости только
по таблице — как настоящий исследователь по статистике.
"""
from __future__ import annotations

import csv
import math
import os
import random
from dataclasses import dataclass, field

FACTORS = ["internet", "digital", "remote", "smart", "roads", "heritage"]
FACTOR_NAMES = {
    "internet": "Интернет",
    "digital": "Цифровые сервисы",
    "remote": "Удалённая работа",
    "smart": "Умное земледелие",
    "roads": "Дороги и транспорт",
    "heritage": "Память и туризм",
}
FACTOR_HINTS = {
    "internet": "доля домов с быстрым интернетом",
    "digital": "Госуслуги, телемедицина, онлайн-школа",
    "remote": "жители, работающие удалённо",
    "smart": "датчики, дроны, точное земледелие",
    "roads": "качество дорог, автобус до райцентра",
    "heritage": "музей, оцифрованный архив, фестивали",
}
INPUTS = ["youth"] + FACTORS
INPUT_NAMES = ["Молодёжь"] + [FACTOR_NAMES[f] for f in FACTORS]
OUTPUT_NAMES = ["Прирост", "Δ молодёжи"]

GROWTH_SCALE = 20.0   # прирост ±5% в год ↔ выход нейросети ±1
YOUTH_SCALE = 40.0

FIRST_YEAR, LAST_YEAR, FORECAST_YEAR = 1995, 2025, 2050

VILLAGES = [
    # имя, население 1995, доля молодёжи, год интернета, потолок удалёнки, пилот умной фермы (год/уровень), дороги, наследие
    ("Берёзовка", 640, 0.32, 2014, 0.15, None, 0.45, (2017, 0.35)),
    ("Сосновка", 410, 0.27, 2018, 0.10, None, 0.35, None),
    ("Заречье", 1180, 0.36, 2008, 0.55, (2016, 0.85), 0.85, (2012, 0.35)),
    ("Ключи", 290, 0.24, 2020, 0.05, None, 0.30, (2019, 0.25)),
    ("Красная Горка", 860, 0.33, 2011, 0.25, (2019, 0.70), 0.70, None),
    ("Липовка", 520, 0.29, 2015, 0.40, None, 0.50, (2010, 0.80)),
    ("Светлое", 1420, 0.38, 2007, 0.60, (2014, 0.90), 0.90, (2018, 0.45)),
    ("Озёрки", 350, 0.26, 2016, 0.15, None, 0.45, (2021, 0.60)),
    ("Каменка", 760, 0.31, 2012, 0.20, (2021, 0.45), 0.60, None),
    ("Ивановка", 230, 0.22, 2021, 0.05, None, 0.25, None),
]


def _logistic(t, mid, width):
    return 1.0 / (1.0 + math.exp(-(t - mid) / width))


def true_step(pop, youth, f, rng=None):
    """Скрытый «закон района». Возвращает население и долю молодёжи через год."""
    inet, dig, rem, smart, roads, herit = (f[k] for k in FACTORS)
    births = 0.004 + 0.042 * youth
    deaths = 0.024 - 0.022 * youth
    pull = 0.026 * (1 - 0.32 * inet * dig - 0.22 * rem - 0.18 * smart - 0.12 * roads - 0.08 * herit)
    inflow = 0.022 * rem * inet + 0.012 * herit * roads + 0.010 * smart * roads + 0.004 * dig
    out = pull * (0.45 + youth)
    growth = births - deaths - out + inflow
    dy = -0.0035 - 0.40 * pull * youth + 0.30 * inflow + 0.003 * smart
    if rng:
        growth += rng.gauss(0, 0.002)
        dy += rng.gauss(0, 0.0012)
    return pop * (1 + growth), min(0.6, max(0.08, youth + dy))


def generate(path: str, seed: int = 2026):
    rng = random.Random(seed)
    rows = []
    for name, p0, y0, inet_year, rem_cap, smart, road0, herit in VILLAGES:
        pop, youth = float(p0), y0
        inet_cap = 0.75 + 0.25 * rng.random()
        for year in range(FIRST_YEAR, LAST_YEAR + 1):
            t = year
            f = {
                "internet": inet_cap * _logistic(t, inet_year + 2, 1.6),
                "digital": 0.9 * _logistic(t, max(inet_year + 5, 2017), 2.0),
                "remote": rem_cap * _logistic(t, 2020.5, 1.0) * _logistic(t, inet_year + 2, 1.6),
                "smart": (smart[1] * _logistic(t, smart[0] + 2, 1.8)) if smart else 0.04 * _logistic(t, 2020, 2),
                "roads": min(1.0, road0 + 0.15 * _logistic(t, 2010 + rng.random() * 8, 2)),
                "heritage": (herit[1] * _logistic(t, herit[0] + 2, 1.5)) if herit else 0.05,
            }
            rows.append({
                "village": name, "year": year, "population": round(pop * (1 + rng.gauss(0, 0.004))),
                "youth": round(youth, 4), **{k: round(v, 4) for k, v in f.items()},
            })
            pop, youth = true_step(pop, youth, f, rng)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=["village", "year", "population", "youth"] + FACTORS)
        w.writeheader()
        w.writerows(rows)


@dataclass
class Village:
    name: str
    years: list = field(default_factory=list)
    population: list = field(default_factory=list)
    youth: list = field(default_factory=list)
    factors: dict = field(default_factory=lambda: {k: [] for k in FACTORS})

    def state(self, i):
        return {"youth": self.youth[i], **{k: self.factors[k][i] for k in FACTORS}}

    def events(self):
        """Летопись, извлечённая из данных: когда технология «пришла» в село."""
        marks = [
            ("internet", 0.15, "Пришёл интернет"),
            ("internet", 0.7, "Интернет почти в каждом доме"),
            ("digital", 0.3, "Госуслуги и телемедицина"),
            ("remote", 0.1, "Первые удалёнщики"),
            ("smart", 0.2, "Датчики и дроны на полях"),
            ("heritage", 0.25, "Открыт музей, оцифрован архив"),
        ]
        out = []
        for key, th, text in marks:
            series = self.factors[key]
            for i in range(1, len(series)):
                if series[i - 1] < th <= series[i]:
                    out.append((self.years[i], text, key))
                    break
        peak = max(range(len(self.population)), key=lambda i: self.population[i])
        if 0 < peak < len(self.population) - 1:
            out.append((self.years[peak], f"Пик населения: {self.population[peak]} чел.", "population"))
        return sorted(out)


def load(path: str):
    if not os.path.exists(path):
        generate(path)
    villages: dict[str, Village] = {}
    with open(path, encoding="utf-8-sig") as fh:
        for row in csv.DictReader(fh):
            v = villages.setdefault(row["village"], Village(row["village"]))
            v.years.append(int(row["year"]))
            v.population.append(int(float(row["population"])))
            v.youth.append(float(row["youth"]))
            for k in FACTORS:
                v.factors[k].append(float(row[k]))
    return list(villages.values())


def encode(state):
    """Признаки → вход нейросети (центрируем в диапазон −1…1)."""
    return [state[k] * 2 - 1 for k in INPUTS]


def dataset(villages, holdout=("Каменка", "Озёрки")):
    """Примеры «состояние года t → изменения к году t+1». Два села отложены для проверки."""
    train, val = ([], []), ([], [])
    for v in villages:
        dst = val if v.name in holdout else train
        for i in range(len(v.years) - 1):
            growth = v.population[i + 1] / v.population[i] - 1
            dy = v.youth[i + 1] - v.youth[i]
            dst[0].append(encode(v.state(i)))
            dst[1].append([max(-0.98, min(0.98, growth * GROWTH_SCALE)), max(-0.98, min(0.98, dy * YOUTH_SCALE))])
    return train, val


def forecast(net, village: Village, targets: dict, ramp_years: int = 10, end_year: int = FORECAST_YEAR):
    """Прогноз шаг за шагом: нейросеть сама «проживает» каждый год до 2050."""
    last = len(village.years) - 1
    pop, youth = float(village.population[last]), village.youth[last]
    start = {k: village.factors[k][last] for k in FACTORS}
    years, pops, youths = [village.years[last]], [pop], [youth]
    for step, year in enumerate(range(village.years[last] + 1, end_year + 1), start=1):
        k = min(1.0, step / ramp_years)
        f = {key: start[key] + (targets[key] - start[key]) * k for key in FACTORS}
        out = net.predict(encode({"youth": youth, **f}))
        pop *= 1 + out[0] / GROWTH_SCALE
        youth = min(0.6, max(0.08, youth + out[1] / YOUTH_SCALE))
        years.append(year); pops.append(pop); youths.append(youth)
    return years, pops, youths


def importance(net, village: Village, targets: dict, delta: float = 0.1):
    """Объяснимый ИИ: насколько изменится население 2050, если поднять фактор на 10%."""
    base = forecast(net, village, targets)[1][-1]
    out = {}
    for k in FACTORS:
        t2 = dict(targets)
        if t2[k] > 1.0 - delta / 2:
            out[k] = None          # фактор уже на максимуме — дальше расти некуда
            continue
        t2[k] = min(1.0, t2[k] + delta)
        out[k] = forecast(net, village, t2)[1][-1] - base
    return out


def fate(ratio: float):
    """Вердикт по отношению населения 2050 к сегодняшнему."""
    if ratio >= 1.15:
        return "Возрождение", "Село растёт: молодые семьи возвращаются, появляются рабочие места."
    if ratio >= 0.88:
        return "Сохранение", "Село удерживает жителей: технологии компенсируют отток."
    if ratio >= 0.55:
        return "Угасание", "Отток продолжается: технологий недостаточно, чтобы удержать людей."
    return "На грани исчезновения", "Без перемен село может опустеть уже при жизни нынешних детей."
