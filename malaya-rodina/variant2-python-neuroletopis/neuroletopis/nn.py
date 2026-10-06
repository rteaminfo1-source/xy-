"""
Маленькая нейросеть, написанная с нуля — без numpy, torch и других библиотек.

Многослойный перцептрон (MLP):
    вход (7 признаков) → скрытый слой (tanh) → скрытый слой (tanh) → выход (tanh)
Обучение — метод обратного распространения ошибки + оптимизатор Adam.

Всё сделано на чистом Python, поэтому каждую строчку можно прочитать
и понять, как «думает» нейросеть.
"""
from __future__ import annotations

import math
import random
from operator import mul


def _dot(a, b):
    # Скалярное произведение. map(mul) работает на уровне C — быстро даже без numpy.
    return sum(map(mul, a, b))


class Layer:
    """Полносвязный слой: y = tanh(W·x + b)."""

    def __init__(self, n_in: int, n_out: int, rng: random.Random):
        limit = math.sqrt(6.0 / (n_in + n_out))  # инициализация Ксавье (Glorot)
        self.w = [[rng.uniform(-limit, limit) for _ in range(n_in)] for _ in range(n_out)]
        self.b = [0.0] * n_out
        # моменты Adam
        self.mw = [[0.0] * n_in for _ in range(n_out)]
        self.vw = [[0.0] * n_in for _ in range(n_out)]
        self.mb = [0.0] * n_out
        self.vb = [0.0] * n_out
        # накопленные градиенты мини-батча
        self.gw = [[0.0] * n_in for _ in range(n_out)]
        self.gb = [0.0] * n_out

    def forward(self, x):
        return [math.tanh(_dot(row, x) + b) for row, b in zip(self.w, self.b)]


class Network:
    """Многослойный перцептрон с методом обратного распространения ошибки."""

    def __init__(self, sizes, seed: int = 7, lr: float = 0.01):
        self.sizes = list(sizes)
        self.rng = random.Random(seed)
        self.layers = [Layer(a, b, self.rng) for a, b in zip(sizes[:-1], sizes[1:])]
        self.lr = lr
        self.t = 0                 # шаг Adam
        self.epoch = 0
        self.history = []          # (эпоха, ошибка на обучении, ошибка на проверке)

    # ---------- прямой проход ----------
    def forward(self, x):
        """Возвращает активации всех слоёв (нужны и для обучения, и для визуализации)."""
        acts = [list(x)]
        for layer in self.layers:
            acts.append(layer.forward(acts[-1]))
        return acts

    def predict(self, x):
        return self.forward(x)[-1]

    # ---------- обучение ----------
    def _backward(self, acts, target):
        """Обратное распространение: считаем градиенты и копим их в слоях."""
        out = acts[-1]
        # ошибка выхода (MSE) × производная tanh
        delta = [(o - t) * (1 - o * o) for o, t in zip(out, target)]
        loss = sum((o - t) ** 2 for o, t in zip(out, target)) / len(out)
        for li in range(len(self.layers) - 1, -1, -1):
            layer = self.layers[li]
            a_in = acts[li]
            for j, d in enumerate(delta):
                if d == 0.0:
                    continue
                gw = layer.gw[j]
                for i, a in enumerate(a_in):
                    gw[i] += d * a
                layer.gb[j] += d
            if li > 0:
                # дельта для предыдущего слоя
                prev = []
                for i, a in enumerate(a_in):
                    s = 0.0
                    for j, d in enumerate(delta):
                        s += layer.w[j][i] * d
                    prev.append(s * (1 - a * a))
                delta = prev
        return loss

    def _adam_step(self, batch: int, b1=0.9, b2=0.999, eps=1e-8):
        self.t += 1
        c1 = 1 - b1 ** self.t
        c2 = 1 - b2 ** self.t
        lr = self.lr
        inv = 1.0 / batch
        for layer in self.layers:
            for j in range(len(layer.w)):
                w, gw, mw, vw = layer.w[j], layer.gw[j], layer.mw[j], layer.vw[j]
                for i in range(len(w)):
                    g = gw[i] * inv
                    mw[i] = b1 * mw[i] + (1 - b1) * g
                    vw[i] = b2 * vw[i] + (1 - b2) * g * g
                    w[i] -= lr * (mw[i] / c1) / (math.sqrt(vw[i] / c2) + eps)
                    gw[i] = 0.0
                g = layer.gb[j] * inv
                layer.mb[j] = b1 * layer.mb[j] + (1 - b1) * g
                layer.vb[j] = b2 * layer.vb[j] + (1 - b2) * g * g
                layer.b[j] -= lr * (layer.mb[j] / c1) / (math.sqrt(layer.vb[j] / c2) + eps)
                layer.gb[j] = 0.0

    def train_epoch(self, xs, ys, batch: int = 16):
        """Одна эпоха: проходим все примеры в случайном порядке мини-батчами."""
        order = list(range(len(xs)))
        self.rng.shuffle(order)
        total = 0.0
        for start in range(0, len(order), batch):
            chunk = order[start:start + batch]
            for k in chunk:
                total += self._backward(self.forward(xs[k]), ys[k])
            self._adam_step(len(chunk))
        self.epoch += 1
        return total / max(1, len(xs))

    def loss(self, xs, ys):
        s = 0.0
        for x, y in zip(xs, ys):
            out = self.predict(x)
            s += sum((o - t) ** 2 for o, t in zip(out, y)) / len(out)
        return s / max(1, len(xs))

    def weight_count(self) -> int:
        return sum(len(l.w) * len(l.w[0]) + len(l.b) for l in self.layers)
