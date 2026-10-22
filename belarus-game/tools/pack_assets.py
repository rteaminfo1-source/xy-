#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Упаковка 3D-рендеров Blender в ресурсы BelarusTech.exe (вызывается из build.sh).

    assets/map3d.png     + map3d_grid.json  ->  res/map3d.qoi      (RCDATA 2)
    assets/sprites.png   + sprites.json     ->  res/sprites.qoi    (RCDATA 3)
    assets/portraits.png + portraits.json   ->  res/portraits.qoi  (RCDATA 4)
    assets/units.png     + units.json       ->  res/units.qoi      (RCDATA 5)
    assets/terrain.png + terrain_h.png + terrain.json -> res/terrain.qoi, res/terrain_h.qoi (RCDATA 6, 7)
    assets/troops.png    + troops.json      ->  res/troops.qoi     (RCDATA 8)

а также src/assets_gen.h (размеры, сетка проекции карты, ячейки атласов) и
res/assets.rc (строки RCDATA только для имеющихся ассетов; app.rc его включает).

Картинки хранятся в формате QOI (https://qoiformat.org): кодировщик написан здесь,
декодер — в src/image.c, так что в .exe не попадает ни одной сторонней библиотеки.
PIL нужен только на этапе сборки, чтобы прочитать PNG. Каждый ассет необязателен:
если его нет, ASSETS_HAVE_* = 0, а заглушки в assets_gen.h позволяют C-коду
компилироваться как раньше.

    python3 tools/pack_assets.py              # обычная сборка (из build.sh)
    python3 tools/pack_assets.py --dummy      # только заглушки (без ассетов)
    python3 tools/pack_assets.py --encode in.png out.qoi
    python3 tools/pack_assets.py --testdata DIR   # данные для tests/image_test.c
"""
import argparse
import json
import os
import re
import struct
import sys
from array import array

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RES_IDS = {'map3d': 2, 'sprites': 3, 'portraits': 4, 'units': 5, 'terrain': 6, 'terrain_h': 7, 'troops': 8}
ATTR = {'map3d': 'map', 'sprites': 'spr', 'portraits': 'por', 'units': 'uni', 'terrain': 'ter', 'troops': 'tro'}


# ---------------------------------------------------------------- QOI ----

def _s8(x):
    """Разность по модулю 256 как signed char (так считает эталонный qoi.h)."""
    return ((x + 128) & 255) - 128


def qoi_encode(w, h, rgba):
    """Стандартный QOI, 4 канала, sRGB. rgba — w*h*4 байт R,G,B,A построчно сверху вниз."""
    if w <= 0 or h <= 0 or len(rgba) != w * h * 4:
        raise ValueError('qoi_encode: неверный размер данных')
    px = array('I')
    px.frombytes(bytes(rgba))
    if sys.byteorder != 'little':
        px.byteswap()                      # v = r | g<<8 | b<<16 | a<<24
    out = bytearray(b'qoif')
    out += struct.pack('>IIBB', w, h, 4, 0)
    app, ext = out.append, out.extend
    index = [0] * 64                       # 0 == (0,0,0,0), как в эталоне
    prev = 0xFF000000                      # (0,0,0,255)
    pr = pg = pb = 0
    pa = 255
    run = 0
    for v in px:
        if v == prev:
            run += 1
            if run == 62:
                app(0xFD)                  # QOI_OP_RUN | 61
                run = 0
            continue
        if run:
            app(0xC0 | (run - 1))
            run = 0
        r = v & 255
        g = (v >> 8) & 255
        b = (v >> 16) & 255
        a = v >> 24
        hh = (r * 3 + g * 5 + b * 7 + a * 11) & 63
        if index[hh] == v:
            app(hh)                        # QOI_OP_INDEX
        else:
            index[hh] = v
            if a == pa:
                vr = _s8(r - pr)
                vg = _s8(g - pg)
                vb = _s8(b - pb)
                if -3 < vr < 2 and -3 < vg < 2 and -3 < vb < 2:
                    app(0x40 | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2))
                else:
                    vgr = _s8(vr - vg)
                    vgb = _s8(vb - vg)
                    if -9 < vgr < 8 and -33 < vg < 32 and -9 < vgb < 8:
                        app(0x80 | (vg + 32))
                        app((vgr + 8) << 4 | (vgb + 8))
                    else:
                        ext((0xFE, r, g, b))
            else:
                ext((0xFF, r, g, b, a))
        prev = v
        pr, pg, pb, pa = r, g, b, a
    if run:
        app(0xC0 | (run - 1))
    out += b'\0\0\0\0\0\0\0\1'
    return bytes(out)


def load_rgba(path):
    """PNG -> (w, h, байты RGBA). Полностью прозрачные пиксели обнуляются:
    после премультипликации они всё равно 0, а QOI сжимает их в серии."""
    from PIL import Image
    im = Image.open(path)
    im.load()
    if im.mode != 'RGBA':
        im = im.convert('RGBA')
    mask = im.getchannel('A').point(lambda x: 255 if x else 0)
    if mask.getextrema()[0] == 0:
        im = Image.composite(im, Image.new('RGBA', im.size, (0, 0, 0, 0)), mask)
    return im.size[0], im.size[1], im.tobytes()


def png_size(path):
    from PIL import Image
    with Image.open(path) as im:
        return im.size


# ------------------------------------------------------ данные игры (C) ----

def parse_cities(root):
    """Порядок C_* из src/game.h и (имя, lon, lat) из src/data_core.c."""
    enum = []
    try:
        src = open(os.path.join(root, 'src', 'game.h'), encoding='utf-8').read()
        m = re.search(r'enum\s*\{([^}]*\bC_COUNT_\b[^}]*)\}', src)
        if m:
            for tok in m.group(1).split(','):
                tok = re.sub(r'/\*.*?\*/', '', tok, flags=re.S).strip().split('=')[0].strip()
                if tok == 'C_COUNT_':
                    break
                if tok:
                    enum.append(tok)
    except OSError:
        pass
    info = {}
    try:
        src = open(os.path.join(root, 'src', 'data_core.c'), encoding='utf-8').read()
        for m in re.finditer(r'\[(C_\w+)\]\s*=\s*\{\s*L"([^"]*)"\s*,\s*([-\d.]+)\s*,\s*([-\d.]+)', src):
            info[m.group(1)] = (m.group(2), float(m.group(3)), float(m.group(4)))
    except OSError:
        pass
    return [(e,) + info.get(e, ('', None, None)) for e in enum]


def _norm(s):
    return str(s).strip().lower().replace('ё', 'е')


# ----------------------------------------------------------- ассеты ----

class Pack:
    def __init__(self, root, assets, res, header):
        self.root, self.assets, self.res, self.header = root, assets, res, header
        self.cities = parse_cities(root)
        self.map = self.spr = self.por = self.uni = self.ter = self.tro = None
        self.notes = []

    def _qoi(self, name, png):
        """PNG -> res/<name>.qoi (перекодирует только если PNG или упаковщик новее)."""
        out = os.path.join(self.res, name + '.qoi')
        src_t = max(os.path.getmtime(png), os.path.getmtime(os.path.abspath(__file__)))
        if not (os.path.exists(out) and os.path.getmtime(out) >= src_t):
            w, h, raw = load_rgba(png)
            data = qoi_encode(w, h, raw)
            tmp = out + '.tmp'
            with open(tmp, 'wb') as f:
                f.write(data)
            os.replace(tmp, out)
        return out

    def _json(self, name):
        with open(os.path.join(self.assets, name), encoding='utf-8') as f:
            return json.load(f)

    def _have(self, *names):
        return all(os.path.isfile(os.path.join(self.assets, n)) for n in names)

    # --- карта ---
    def do_map(self):
        if not self._have('map3d.png', 'map3d_grid.json'):
            return
        png = os.path.join(self.assets, 'map3d.png')
        js = self._json('map3d_grid.json')
        iw, ih = png_size(png)
        sx = iw / float(js.get('width') or iw)
        sy = ih / float(js.get('height') or ih)
        nlon, nlat = int(js['nlon']), int(js['nlat'])
        lon0, lat0, step = float(js['lon0']), float(js['lat0']), float(js['step'])
        pts = js['pts']
        if pts and isinstance(pts[0], list) and pts[0] and isinstance(pts[0][0], list):
            pts = [p for row in pts for p in row]          # [nlat][nlon][2] -> плоский
        if isinstance(pts[0], dict):
            pts = [(p.get('px', p.get('x')), p.get('py', p.get('y'))) for p in pts]
        if nlon < 2 or nlat < 2 or len(pts) != nlon * nlat or step <= 0:
            raise ValueError('map3d_grid.json: ожидалось nlon*nlat точек (долгота меняется быстрее)')
        pts = [(float(p[0]) * sx, float(p[1]) * sy) for p in pts]

        def project(lon, lat):
            fx, fy = (lon - lon0) / step, (lat - lat0) / step
            if not (0 <= fx <= nlon - 1 and 0 <= fy <= nlat - 1):
                return None
            i, j = min(int(fx), nlon - 2), min(int(fy), nlat - 2)
            u, v = fx - i, fy - j
            p00, p10 = pts[j * nlon + i], pts[j * nlon + i + 1]
            p01, p11 = pts[(j + 1) * nlon + i], pts[(j + 1) * nlon + i + 1]
            return tuple((1 - v) * ((1 - u) * p00[k] + u * p10[k]) + v * ((1 - u) * p01[k] + u * p11[k])
                         for k in (0, 1))

        # города в порядке C_*: по имени, иначе по позиции, иначе проекцией lon/lat
        raw = js.get('cities') or []
        if isinstance(raw, dict):
            raw = [{'name': k, 'px': v[0], 'py': v[1]} if isinstance(v, (list, tuple)) else dict(v, name=k)
                   for k, v in raw.items()]
        n = len(self.cities) or len(raw) or 1
        by_name, by_pos = {}, []
        for c in raw:
            if isinstance(c, dict):
                xy = (c.get('px', c.get('x')), c.get('py', c.get('y')))
                for key in ('name', 'id', 'enum'):
                    if c.get(key) is not None:
                        by_name[_norm(c[key])] = xy
            else:
                xy = (c[0], c[1])
            by_pos.append(xy)
        city = []
        for k in range(n):
            ent = self.cities[k] if k < len(self.cities) else (None, '', None, None)
            xy = by_name.get(_norm(ent[1])) or by_name.get(_norm(ent[0] or '')) if by_name else None
            if xy is None and not by_name and k < len(by_pos):
                xy = by_pos[k]
            if xy is not None and xy[0] is not None:
                xy = (float(xy[0]) * sx, float(xy[1]) * sy)
            elif ent[2] is not None:
                xy = project(ent[2], ent[3])
                self.notes.append('город %s спроецирован по сетке' % (ent[0],))
            city.append(xy or (0.0, 0.0))
        ppk = js.get('px_per_km')
        if ppk:
            ppk = float(ppk) * sx
        else:
            c = (nlat // 2) * nlon + nlon // 2
            ppk = ((pts[c][0] - pts[c - 1][0]) ** 2 + (pts[c][1] - pts[c - 1][1]) ** 2) ** 0.5 / (step * 65.95)
        self.map = dict(w=iw, h=ih, lon0=lon0, lat0=lat0, step=step, nlon=nlon, nlat=nlat,
                        pts=pts, city=city, ppk=ppk, qoi=self._qoi('map3d', png))

    # --- атлас построек ---
    def do_sprites(self):
        if not self._have('sprites.png', 'sprites.json'):
            return
        png = os.path.join(self.assets, 'sprites.png')
        js = self._json('sprites.json')
        iw, ih = png_size(png)
        cell = js.get('cell', js.get('cell_w', 64))
        if isinstance(cell, (list, tuple)):
            cell = cell[0]
        cell = int(cell)
        cols = int(js.get('cols') or max(1, iw // cell))
        anc = js.get('anchor', [js.get('anchor_x', cell / 2), js.get('anchor_y', cell)])
        names = [str(x if not isinstance(x, dict) else x.get('id', x.get('name', ''))) for x in js.get('names', [])]
        count = len(names) or int(js.get('count') or cols * max(1, ih // cell))
        self.spr = dict(cell=cell, cols=cols, ax=int(round(float(anc[0]))), ay=int(round(float(anc[1]))),
                        names=names, count=count, w=iw, h=ih, qoi=self._qoi('sprites', png))

    # --- атласы с ячейками cell_w x cell_h и списком id: портреты и фигурки войск ---
    def do_portraits(self):
        self.por = self._id_atlas('portraits')

    def do_units(self):
        self.uni = self._id_atlas('units')

    # --- анимированные отряды: ряд кадров на вид (стоит, шаг, выстрел), общий якорь ---
    def do_troops(self):
        if not self._have('troops.png', 'troops.json'):
            return
        png = os.path.join(self.assets, 'troops.png')
        js = self._json('troops.json')
        iw, ih = png_size(png)
        cw, ch, cols = int(js['cell_w']), int(js['cell_h']), int(js['cols'])
        ids, frames = [], []
        for v in js['variants']:
            st, wk, fi = int(v['stand']), [int(x) for x in v['walk']], [int(x) for x in v['fire']]
            last = max(st, wk[0] + wk[1] - 1, fi[0] + fi[1] - 1)
            if wk[1] < 1 or fi[1] < 1 or (last // cols + 1) * ch > ih or cols * cw > iw:
                raise ValueError('troops.json: кадры вида %s вне листа' % v['id'])
            ids.append(str(v['id']))
            frames.append((st, wk[0], wk[1], fi[0], fi[1]))
        if not ids:
            raise ValueError('troops.json: нет видов')
        self.tro = dict(cw=cw, ch=ch, cols=cols, ax=int(js['anchor_x']), ay=int(js['anchor_y']),
                        ppu=float(js.get('ppu', 48.0)), ids=ids, frames=frames, count=len(ids), w=iw, h=ih,
                        qoi=self._qoi('troops', png))

    # --- интерактивная 3D-карта: вид сверху, высоты, контур плиты ---
    def do_terrain(self):
        if not self._have('terrain.png', 'terrain_h.png', 'terrain.json'):
            return
        js = self._json('terrain.json')
        tex = os.path.join(self.assets, 'terrain.png')
        hgt = os.path.join(self.assets, 'terrain_h.png')
        tw, th = png_size(tex)
        nx, ny = png_size(hgt)
        outline = [(float(p[0]), float(p[1]), float(p[2])) for p in js.get('outline', [])]
        if len(outline) < 3:
            raise ValueError('terrain.json: нет контура плиты')
        self.ter = dict(w=tw, h=th, nx=nx, ny=ny, x0=float(js['x0']), y0=float(js['y0']),
                        x1=float(js['x1']), y1=float(js['y1']), zb=float(js.get('zbottom', -22.0)),
                        outline=outline, qoi=self._qoi('terrain', tex), qoi_h=self._qoi('terrain_h', hgt))

    def _id_atlas(self, name):
        if not self._have(name + '.png', name + '.json'):
            return None
        png = os.path.join(self.assets, name + '.png')
        js = self._json(name + '.json')
        iw, ih = png_size(png)
        cell = js.get('cell')
        cw = js.get('cell_w', js.get('cellW', js.get('w')))
        ch = js.get('cell_h', js.get('cellH', js.get('h')))
        if isinstance(cell, (list, tuple)):
            cw, ch = cw or cell[0], ch or cell[1]
        elif cell is not None:
            cw, ch = cw or cell, ch or cell
        ids = None
        for key in ('ids', 'names', 'order', 'portraits', 'items'):
            if isinstance(js.get(key), list):
                ids = js[key]
                break
        ids = [str(x if not isinstance(x, dict) else x.get('id', x.get('name', ''))) for x in (ids or [])]
        cols = int(js.get('cols') or js.get('columns') or (iw // int(cw) if cw else 1) or 1)
        if not cw or not ch:
            rows = max(1, (len(ids) + cols - 1) // cols)
            cw, ch = cw or iw // cols, ch or ih // rows
        return dict(cw=int(cw), ch=int(ch), cols=cols, ids=ids, count=len(ids), w=iw, h=ih,
                    qoi=self._qoi(name, png))

    def _all(self):
        return [(name, getattr(self, ATTR[name])) for name in ('map3d', 'sprites', 'portraits', 'units', 'terrain',
                                                                 'troops')]

    def run(self):
        os.makedirs(self.res, exist_ok=True)
        for name, fn in (('map3d', self.do_map), ('sprites', self.do_sprites), ('portraits', self.do_portraits),
                         ('units', self.do_units), ('terrain', self.do_terrain), ('troops', self.do_troops)):
            try:
                fn()
            except Exception as e:          # битый ассет не должен ломать сборку
                print('pack_assets: %s пропущен: %s' % (name, e), file=sys.stderr)
                setattr(self, ATTR[name], None)
        for name, have in self._all():
            for stale in [os.path.join(self.res, name + '.qoi')] + ([os.path.join(self.res, 'terrain_h.qoi')] if name == 'terrain' else []):
                if not have and os.path.exists(stale):
                    os.remove(stale)
        write_if_changed(self.header, header_text(self))
        write_if_changed(os.path.join(self.res, 'assets.rc'), rc_text(self))
        for name, d in self._all():
            if d:
                print('pack_assets: %s %dx%d -> res/%s.qoi (%.0f КБ)' % (
                    name, d['w'], d['h'], name, os.path.getsize(d['qoi']) / 1024.0))
        for n in self.notes:
            print('pack_assets: ' + n)


# ------------------------------------------------------------ вывод ----

def _f(x):
    s = '%.2f' % x
    return ('0.00' if s == '-0.00' else s) + 'f'


def _d(x):
    s = repr(float(x))
    return s if ('.' in s or 'e' in s) else s + '.0'


def _cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


def _rows(items, per):
    return ',\n'.join('    ' + ', '.join(items[i:i + per]) for i in range(0, len(items), per))


def header_text(p):
    m, s, q, u, t, tr = p.map, p.spr, p.por, p.uni, p.ter, p.tro
    ncity = len(p.cities) or (len(m['city']) if m else 1)
    L = ['/* assets_gen.h — СГЕНЕРИРОВАНО tools/pack_assets.py, НЕ РЕДАКТИРОВАТЬ (do not edit).',
         ' * Встроенные рендеры Blender: RCDATA 2 = map3d.qoi, 3 = sprites.qoi, 4 = portraits.qoi, 5 = units.qoi,',
         ' * 6 = terrain.qoi (вид сверху), 7 = terrain_h.qoi (высоты), 8 = troops.qoi (анимированные отряды).',
         ' * Если ассета нет, ASSETS_HAVE_* = 0, а остальные определения — безопасные заглушки. */',
         '#ifndef ASSETS_GEN_H', '#define ASSETS_GEN_H', '']
    L += ['/* ---- 3D-карта: пиксели точек сетки (lon0 + i*step, lat0 + j*step), индекс j*NLON + i ---- */',
          '#define ASSETS_HAVE_MAP3D %d' % (1 if m else 0)]
    if m:
        L += ['#define MAP3D_W %d' % m['w'], '#define MAP3D_H %d' % m['h'],
              '#define MAP3D_LON0 %s' % _d(m['lon0']), '#define MAP3D_LAT0 %s' % _d(m['lat0']),
              '#define MAP3D_STEP %s' % _d(m['step']),
              '#define MAP3D_NLON %d' % m['nlon'], '#define MAP3D_NLAT %d' % m['nlat'],
              '#define MAP3D_PX_PER_KM %s' % _d(round(m['ppk'], 5)),
              '#define MAP3D_NCITY %d' % ncity,
              'static const float MAP3D_PTS[MAP3D_NLON * MAP3D_NLAT][2] = {',
              _rows(['{%s, %s}' % (_f(x), _f(y)) for x, y in m['pts']], 6), '};',
              '/* центры городов на рендере, в порядке C_* (game.h) */',
              'static const float MAP3D_CITY[MAP3D_NCITY][2] = {',
              _rows(['{%s, %s}' % (_f(x), _f(y)) for x, y in m['city'][:ncity]], 6), '};']
    else:
        L += ['#define MAP3D_W 1', '#define MAP3D_H 1', '#define MAP3D_LON0 0.0', '#define MAP3D_LAT0 0.0',
              '#define MAP3D_STEP 1.0', '#define MAP3D_NLON 2', '#define MAP3D_NLAT 2',
              '#define MAP3D_PX_PER_KM 1.0', '#define MAP3D_NCITY %d' % ncity,
              'static const float MAP3D_PTS[MAP3D_NLON * MAP3D_NLAT][2] = {{0.0f, 0.0f}};',
              'static const float MAP3D_CITY[MAP3D_NCITY][2] = {{0.0f, 0.0f}};']
    L += ['', '/* ---- атлас построек: ячейка SPR_CELL x SPR_CELL, SPR_COLS в ряд; якорь — точка опоры ---- */',
          '#define ASSETS_HAVE_SPRITES %d' % (1 if s else 0)]
    if s:
        names = s['names'] or ['']
        L += ['#define SPR_CELL %d' % s['cell'], '#define SPR_COLS %d' % s['cols'],
              '#define SPR_ANCHOR_X %d' % s['ax'], '#define SPR_ANCHOR_Y %d' % s['ay'],
              '#define SPR_COUNT %d' % s['count'],
              'static const char *const SPR_NAMES[%d] = {' % len(names),
              _rows([_cstr(x) for x in names], 6), '};']
    else:
        L += ['#define SPR_CELL 1', '#define SPR_COLS 1', '#define SPR_ANCHOR_X 0', '#define SPR_ANCHOR_Y 0',
              '#define SPR_COUNT 0', 'static const char *const SPR_NAMES[1] = {""};']
    L += ['', '/* ---- атлас портретов: ячейка PORTRAIT_CELL_W x PORTRAIT_CELL_H, PORTRAIT_COLS в ряд ---- */',
          '#define ASSETS_HAVE_PORTRAITS %d' % (1 if q else 0)]
    if q:
        ids = q['ids'] or ['']
        L += ['#define PORTRAIT_CELL_W %d' % q['cw'], '#define PORTRAIT_CELL_H %d' % q['ch'],
              '#define PORTRAIT_COLS %d' % q['cols'], '#define PORTRAIT_COUNT %d' % q['count'],
              'static const char *const PORTRAIT_IDS[%d] = {' % len(ids),
              _rows([_cstr(x) for x in ids], 6), '};']
    else:
        L += ['#define PORTRAIT_CELL_W 1', '#define PORTRAIT_CELL_H 1', '#define PORTRAIT_COLS 1',
              '#define PORTRAIT_COUNT 0', 'static const char *const PORTRAIT_IDS[1] = {""};']
    L += ['', '/* ---- атлас фигурок войск: ячейка UNIT_CELL_W x UNIT_CELL_H, UNIT_COLS в ряд, id как в UNIT_TYPES ---- */',
          '#define ASSETS_HAVE_UNITS %d' % (1 if u else 0)]
    if u:
        ids = u['ids'] or ['']
        L += ['#define UNIT_CELL_W %d' % u['cw'], '#define UNIT_CELL_H %d' % u['ch'],
              '#define UNIT_COLS %d' % u['cols'], '#define UNIT_ATLAS_COUNT %d' % u['count'],
              'static const char *const UNIT_IDS[%d] = {' % len(ids),
              _rows([_cstr(x) for x in ids], 6), '};']
    else:
        L += ['#define UNIT_CELL_W 1', '#define UNIT_CELL_H 1', '#define UNIT_COLS 1',
              '#define UNIT_ATLAS_COUNT 0', 'static const char *const UNIT_IDS[1] = {""};']
    L += ['', '/* ---- 3D-карта: прямоугольник (км), сетка высот, низ плиты и контур её борта (x, y, верх) ---- */',
          '#define ASSETS_HAVE_TERRAIN %d' % (1 if t else 0)]
    if t:
        L += ['#define TERRAIN_X0 %s' % _d(round(t['x0'], 4)), '#define TERRAIN_Y0 %s' % _d(round(t['y0'], 4)),
              '#define TERRAIN_X1 %s' % _d(round(t['x1'], 4)), '#define TERRAIN_Y1 %s' % _d(round(t['y1'], 4)),
              '#define TERRAIN_TEX_W %d' % t['w'], '#define TERRAIN_TEX_H %d' % t['h'],
              '#define TERRAIN_NX %d' % t['nx'], '#define TERRAIN_NY %d' % t['ny'],
              '#define TERRAIN_ZBOTTOM %s' % _d(t['zb']),
              '#define TERRAIN_OUTLINE_N %d' % len(t['outline']),
              'static const float TERRAIN_OUTLINE[TERRAIN_OUTLINE_N][3] = {',
              _rows(['{%s, %s, %s}' % (_f(x), _f(y), _f(z)) for x, y, z in t['outline']], 4), '};']
    else:
        L += ['#define TERRAIN_X0 -320.0', '#define TERRAIN_Y0 -272.0', '#define TERRAIN_X1 317.0', '#define TERRAIN_Y1 275.0',
              '#define TERRAIN_TEX_W 1', '#define TERRAIN_TEX_H 1', '#define TERRAIN_NX 2', '#define TERRAIN_NY 2',
              '#define TERRAIN_ZBOTTOM -22.0', '#define TERRAIN_OUTLINE_N 1',
              'static const float TERRAIN_OUTLINE[1][3] = {{0.0f, 0.0f, 0.0f}};']
    L += ['', '/* ---- анимированные отряды: ряд кадров на вид (стоит | шаг | выстрел), ячейка TROOP_CELL_W x TROOP_CELL_H,',
          ' * TROOP_COLS в ряд; якорь (TROOP_ANCHOR_X, TROOP_ANCHOR_Y) — точка опоры на земле; TROOP_PPU — пикселей',
          ' * клетки на единицу модели (рост героини ~1.9). TROOP_FRAMES: стоит, первый и число кадров шага,',
          ' * первый и число кадров выстрела ---- */',
          '#define ASSETS_HAVE_TROOPS %d' % (1 if tr else 0)]
    if tr:
        L += ['#define TROOP_CELL_W %d' % tr['cw'], '#define TROOP_CELL_H %d' % tr['ch'],
              '#define TROOP_COLS %d' % tr['cols'], '#define TROOP_ANCHOR_X %d' % tr['ax'],
              '#define TROOP_ANCHOR_Y %d' % tr['ay'], '#define TROOP_PPU %s' % _d(tr['ppu']),
              '#define TROOP_COUNT %d' % tr['count'],
              'static const char *const TROOP_IDS[%d] = {' % tr['count'],
              _rows([_cstr(x) for x in tr['ids']], 6), '};',
              'static const short TROOP_FRAMES[%d][5] = {' % tr['count'],
              _rows(['{%d, %d, %d, %d, %d}' % f for f in tr['frames']], 4), '};']
    else:
        L += ['#define TROOP_CELL_W 1', '#define TROOP_CELL_H 1', '#define TROOP_COLS 1', '#define TROOP_ANCHOR_X 0',
              '#define TROOP_ANCHOR_Y 0', '#define TROOP_PPU 1.0', '#define TROOP_COUNT 0',
              'static const char *const TROOP_IDS[1] = {""};',
              'static const short TROOP_FRAMES[1][5] = {{0, 0, 1, 0, 1}};']
    L += ['', '#endif', '']
    return '\n'.join(L)


def rc_text(p):
    L = ['/* assets.rc — СГЕНЕРИРОВАНО tools/pack_assets.py, НЕ РЕДАКТИРОВАТЬ. Включается из app.rc. */']
    for name, d in p._all():
        if d:
            L.append('%d RCDATA "%s.qoi"' % (RES_IDS[name], name))
            if name == 'terrain':
                L.append('%d RCDATA "terrain_h.qoi"' % RES_IDS['terrain_h'])
    return '\n'.join(L) + '\n'


def write_if_changed(path, text):
    try:
        with open(path, encoding='utf-8') as f:
            if f.read() == text:
                return
    except OSError:
        pass
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)


# ------------------------------------------------------- самопроверка ----

def _premul_ref(png):
    """Ожидаемые пиксели для теста: PIL -> RGBA -> премультипликация с округлением."""
    from PIL import Image
    im = Image.open(png).convert('RGBA')
    out = array('I')
    for r, g, b, a in im.getdata():
        pm = lambda c: (c * a * 2 + 255) // 510
        out.append(a << 24 | pm(r) << 16 | pm(g) << 8 | pm(b))
    if sys.byteorder != 'little':
        out.byteswap()
    return struct.pack('<II', im.size[0], im.size[1]) + out.tobytes()


def make_testdata(d):
    """PNG (PIL) -> наш кодировщик -> .qoi и эталон .exp для tests/image_test.c,
    плюс синтетические ассеты и assets_gen.h для проверки map3d_project."""
    import random
    from PIL import Image, ImageDraw
    rnd = random.Random(20260101)
    os.makedirs(d, exist_ok=True)
    imgs = {}
    w, h = 257, 131
    im = Image.new('RGBA', (w, h))
    im.putdata([(x * 255 // (w - 1), y * 255 // (h - 1), (x + y) & 255, (x * 2 + y) & 255)
                for y in range(h) for x in range(w)])
    imgs['gradient'] = im
    big = Image.new('RGBA', (400, 240), (0, 0, 0, 0))
    dr = ImageDraw.Draw(big)
    dr.ellipse((40, 30, 260, 210), fill=(200, 40, 30, 255))
    dr.polygon([(200, 10), (390, 120), (150, 230)], fill=(20, 120, 240, 160))
    dr.line((0, 239, 399, 0), fill=(250, 250, 250, 90), width=9)
    imgs['alpha_edges'] = big.resize((101, 61), Image.LANCZOS)
    imgs['noise_rgba'] = Image.frombytes('RGBA', (73, 41), bytes(rnd.getrandbits(8) for _ in range(73 * 41 * 4)))
    vals, c = [], [128, 128, 128]
    for _ in range(96 * 64):                       # плавное блуждание: DIFF и LUMA
        k = rnd.random()
        step = 1 if k < 0.5 else (6 if k < 0.85 else 40)
        c = [(v + rnd.randint(-step, step)) & 255 for v in c]
        vals.append(tuple(c) + (255,))
    im = Image.new('RGBA', (96, 64))
    im.putdata(vals)
    imgs['walk_opaque'] = im
    im = Image.new('RGBA', (300, 9), (0, 0, 0, 255))   # длинные серии > 62, старт с (0,0,0,255)
    dr = ImageDraw.Draw(im)
    dr.rectangle((70, 0, 199, 4), fill=(10, 200, 30, 255))
    dr.rectangle((120, 3, 299, 8), fill=(0, 0, 0, 0))
    dr.rectangle((250, 0, 299, 2), fill=(255, 255, 255, 128))
    imgs['solid_runs'] = im
    imgs['one_pixel'] = Image.new('RGBA', (1, 1), (12, 34, 56, 78))
    imgs['rgb_photo'] = Image.frombytes('RGB', (37, 23), bytes((i * 7 + (i // 37) * 3) & 255 for i in range(37 * 23 * 3)))
    im = Image.new('RGBA', (40, 20))
    im.putdata([(rnd.randrange(256), rnd.randrange(256), rnd.randrange(256), 0 if (x + y) % 3 else 200)
                for y in range(20) for x in range(40)])
    imgs['hidden_rgb'] = im
    im = Image.new('P', (30, 30), 0)
    im.putpalette([0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255] + [0] * 756)
    ImageDraw.Draw(im).ellipse((5, 5, 25, 25), fill=1)
    ImageDraw.Draw(im).rectangle((0, 0, 9, 29), fill=2)
    im.info['transparency'] = 0
    imgs['palette_trns'] = im
    imgs['grey_alpha'] = Image.frombytes('LA', (16, 16), bytes((i * 13) & 255 for i in range(512)))

    lines = []
    for name, im in imgs.items():
        png = os.path.join(d, name + '.png')
        im.save(png) if im.mode != 'P' else im.save(png, transparency=0)
        w, h, raw = load_rgba(png)
        with open(os.path.join(d, name + '.qoi'), 'wb') as f:
            f.write(qoi_encode(w, h, raw))
        with open(os.path.join(d, name + '.exp'), 'wb') as f:
            f.write(_premul_ref(png))
        lines.append('%s.qoi %s.exp' % (name, name))

    # синтетические ассеты: аффинная «проекция», чтобы билинейная интерполяция была точной
    A = os.path.join(d, 'assets')
    os.makedirs(A, exist_ok=True)
    aff = lambda lon, lat: (50 + 30 * (lon - 23) + 4 * (lat - 51), 400 - 50 * (lat - 51) + 3 * (lon - 23))
    lon0, lat0, step, nlon, nlat = 23.0, 51.0, 0.5, 17, 10
    pts = [list(aff(lon0 + i * step, lat0 + j * step)) for j in range(nlat) for i in range(nlon)]
    cities = parse_cities(ROOT)
    js_c = [{'name': c[1], 'px': k * 10 + 1, 'py': k * 20 + 2} for k, c in enumerate(cities)][:-1][::-1]
    json.dump({'width': 300, 'height': 420, 'lon0': lon0, 'lat0': lat0, 'step': step, 'nlon': nlon,
               'nlat': nlat, 'pts': pts, 'cities': js_c, 'px_per_km': 0.45},
              open(os.path.join(A, 'map3d_grid.json'), 'w'), ensure_ascii=False)
    imgs2 = {'map3d': Image.linear_gradient('L').resize((300, 420)).convert('RGBA'),
             'sprites': imgs['alpha_edges'].resize((64, 32)),
             'portraits': imgs['noise_rgba'].resize((60, 40))}
    for k, im in imgs2.items():
        im.save(os.path.join(A, k + '.png'))
    json.dump({'cell': 16, 'cols': 4, 'anchor': [8, 14], 'names': ['fort', 'church', 'mill']},
              open(os.path.join(A, 'sprites.json'), 'w'))
    json.dump({'cell': [20, 40], 'cols': 3, 'ids': ['euphrosyne', 'skaryna', 'lenin']},
              open(os.path.join(A, 'portraits.json'), 'w'))
    p = Pack(ROOT, A, os.path.join(d, 'res'), os.path.join(d, 'assets_gen.h'))
    p.run()
    for k in imgs2:
        with open(os.path.join(d, 'res', k + '.exp'), 'wb') as f:
            f.write(_premul_ref(os.path.join(A, k + '.png')))
        lines.append('res/%s.qoi res/%s.exp' % (k, k))
    with open(os.path.join(d, 'list.txt'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    with open(os.path.join(d, 'map_expect.txt'), 'w') as f:
        for k, c in enumerate(cities):
            if k < len(cities) - 1:
                f.write('city %d %.3f %.3f\n' % (k, k * 10 + 1, k * 20 + 2))
            elif c[2] is not None:                 # последний город опущен в JSON
                f.write('city %d %.3f %.3f\n' % ((k,) + aff(c[2], c[3])))
        for _ in range(200):
            lon = lon0 + rnd.random() * step * (nlon - 1)
            lat = lat0 + rnd.random() * step * (nlat - 1)
            f.write('grid %.6f %.6f %.4f %.4f\n' % ((lon, lat) + aff(lon, lat)))
        for i in range(nlon):
            for j in range(nlat):
                f.write('grid %.6f %.6f %.4f %.4f\n' % ((lon0 + i * step, lat0 + j * step)
                                                        + aff(lon0 + i * step, lat0 + j * step)))
        for lon, lat in ((lon0 - 0.01, 52), (31.01, 52), (25, lat0 - 0.001), (25, 55.6), (100, 100)):
            f.write('out %.6f %.6f\n' % (lon, lat))
    print('testdata: %d изображений в %s' % (len(lines), d))


# ------------------------------------------------------------- main ----

def main():
    ap = argparse.ArgumentParser(description='Упаковка рендеров в res/*.qoi + src/assets_gen.h + res/assets.rc')
    ap.add_argument('--root', default=ROOT)
    ap.add_argument('--assets')
    ap.add_argument('--res')
    ap.add_argument('--header')
    ap.add_argument('--dummy', action='store_true', help='только заглушки (как без ассетов)')
    ap.add_argument('--encode', nargs=2, metavar=('PNG', 'QOI'))
    ap.add_argument('--testdata', metavar='DIR')
    a = ap.parse_args()
    if a.encode:
        w, h, raw = load_rgba(a.encode[0])
        with open(a.encode[1], 'wb') as f:
            f.write(qoi_encode(w, h, raw))
        return
    if a.testdata:
        make_testdata(a.testdata)
        return
    p = Pack(a.root, a.assets or os.path.join(a.root, 'assets'), a.res or os.path.join(a.root, 'res'),
             a.header or os.path.join(a.root, 'src', 'assets_gen.h'))
    if not a.dummy:
        try:
            import PIL  # noqa: F401
        except ImportError:
            print('pack_assets: нет модуля PIL (pip install pillow) — собираю без 3D-ассетов', file=sys.stderr)
            a.dummy = True
    if a.dummy:
        os.makedirs(p.res, exist_ok=True)
        write_if_changed(p.header, header_text(p))
        write_if_changed(os.path.join(p.res, 'assets.rc'), rc_text(p))
        return
    p.run()


if __name__ == '__main__':
    main()
