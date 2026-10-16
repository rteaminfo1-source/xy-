/* Встроенные рендеры Blender: 3D-карта, модели построек, портреты деятелей и правителей.
 * Картинки лежат в .exe как RCDATA (QOI) и декодируются собственным кодом (image.c).
 * Все масштабированные копии кэшируются: пересчёт только при смене размера или темы. */
#include "ui.h"
#include "image.h"
#include "map3d.h"
#include "assets_gen.h"
#include <string.h>

/* assets_gen.h объявляет таблицы как static: отметим их использованными, чтобы не было предупреждений */
__attribute__((unused)) static const void *art_tables_[] = { MAP3D_PTS, MAP3D_CITY, SPR_NAMES, PORTRAIT_IDS, UNIT_IDS };



static unsigned rgb_of(COLORREF c) { return (unsigned)GetRValue(c) << 16 | (unsigned)GetGValue(c) << 8 | GetBValue(c); }

/* Наложение с учётом текущего отсечения панели (прокручиваемые списки). */
static void blit_clipped(const Image *img, int dx, int dy, int alpha)
{
    if (!img || !img->px) return;
    int cx = 0, cy = 0, cw = g_w, ch = g_h;
    if (!d_clip_get(&cx, &cy, &cw, &ch)) { cx = 0; cy = 0; cw = g_w; ch = g_h; }
    int x0 = dx, y0 = dy, x1 = dx + img->w, y1 = dy + img->h;
    if (x0 < cx) x0 = cx;
    if (y0 < cy) y0 = cy;
    if (x1 > cx + cw) x1 = cx + cw;
    if (y1 > cy + ch) y1 = cy + ch;
    if (x1 <= x0 || y1 <= y0) return;
    GdiFlush();
    image_blit_part(d_bits(), g_w, g_h, x0, y0, img, x0 - dx, y0 - dy, x1 - x0, y1 - y0, alpha);
}

/* ---------- карта ---------- */
static Image s_map, s_mapScaled;
static int s_mapTried, s_mapOk;
static int s_mapKw, s_mapKh;
static unsigned s_mapKbg;

/* Видимая часть рендера (за её пределами — прозрачные поля) */
#define MAP_SRC_X 40
#define MAP_SRC_Y 100
#define MAP_SRC_W 1520
#define MAP_SRC_H 1086

static int map_load(void)
{
    if (!s_mapTried) {
        s_mapTried = 1;
        s_mapOk = ASSETS_HAVE_MAP3D && image_load_resource(RES_MAP3D_QOI, &s_map);
    }
    return s_mapOk;
}

int art_map_available(void) { return map_load(); }

int art_map_draw(int x, int y, int w, int h, MapXf *xf)
{
    if (!map_load() || w < 40 || h < 40) return 0;
    int sw = MAP_SRC_W, sh = MAP_SRC_H;
    if (MAP_SRC_X + sw > s_map.w) sw = s_map.w - MAP_SRC_X;
    if (MAP_SRC_Y + sh > s_map.h) sh = s_map.h - MAP_SRC_Y;
    double s = (double)w / sw;
    if ((double)h / sh < s) s = (double)h / sh;
    int dw = (int)(sw * s), dh = (int)(sh * s);
    unsigned bg = rgb_of(P.surface);
    if (!s_mapScaled.px || s_mapKw != dw || s_mapKh != dh || s_mapKbg != bg) {
        image_free(&s_mapScaled);
        image_scale(&s_map, MAP_SRC_X, MAP_SRC_Y, sw, sh, &s_mapScaled, dw, dh);
        image_fill_under(&s_mapScaled, bg);
        s_mapKw = dw; s_mapKh = dh; s_mapKbg = bg;
    }
    int ox = x + (w - dw) / 2, oy = y + (h - dh) / 2;
    d_fill(x, y, w, h, P.surface);
    blit_clipped(&s_mapScaled, ox, oy, 255);
    if (xf) { xf->x = ox; xf->y = oy; xf->w = dw; xf->h = dh; xf->s = s; xf->sx = MAP_SRC_X; xf->sy = MAP_SRC_Y; }
    return 1;
}

void art_map_point(const MapXf *xf, double px, double py, int *sx, int *sy)
{
    *sx = xf->x + (int)((px - xf->sx) * xf->s + 0.5);
    *sy = xf->y + (int)((py - xf->sy) * xf->s + 0.5);
}

/* ---------- модели построек ---------- */
static Image s_spr;
static int s_sprTried, s_sprOk;
typedef struct { int size; unsigned tick; Image img[SPR_COUNT > 0 ? SPR_COUNT : 1]; } SprSet;
static SprSet s_sets[2];
static unsigned s_tick;

int art_sprites_available(void)
{
    if (!s_sprTried) {
        s_sprTried = 1;
        s_sprOk = ASSETS_HAVE_SPRITES && image_load_resource(RES_SPRITES_QOI, &s_spr);
    }
    return s_sprOk;
}

static SprSet *sprite_set(int size)
{
    s_tick++;
    for (int i = 0; i < 2; i++)
        if (s_sets[i].size == size) { s_sets[i].tick = s_tick; return &s_sets[i]; }
    SprSet *v = s_sets[0].tick <= s_sets[1].tick ? &s_sets[0] : &s_sets[1];
    for (int k = 0; k < SPR_COUNT; k++) image_free(&v->img[k]);
    v->size = size;
    v->tick = s_tick;
    for (int k = 0; k < SPR_COUNT; k++)
        image_scale(&s_spr, (k % SPR_COLS) * SPR_CELL, (k / SPR_COLS) * SPR_CELL, SPR_CELL, SPR_CELL, &v->img[k], size, size);
    return v;
}

void art_sprite_draw(int sprite, int ax, int ay, int size, int alpha)
{
    if (!art_sprites_available() || sprite < 0 || sprite >= SPR_COUNT || size < 8) return;
    SprSet *set = sprite_set(size);
    int dx = ax - SPR_ANCHOR_X * size / SPR_CELL, dy = ay - SPR_ANCHOR_Y * size / SPR_CELL;
    blit_clipped(&set->img[sprite], dx, dy, alpha);
}

/* ---------- портреты ---------- */
static Image s_por;
static int s_porTried, s_porOk;
#define PCACHE 120
typedef struct { int idx, w, h; unsigned tick; Image img; } PorCache;
static PorCache s_pc[PCACHE];

static int portraits_load(void)
{
    if (!s_porTried) {
        s_porTried = 1;
        s_porOk = ASSETS_HAVE_PORTRAITS && image_load_resource(RES_PORTRAITS_QOI, &s_por);
    }
    return s_porOk;
}

int art_portrait_index(const char *id)
{
    if (!id || !portraits_load()) return -1;
    for (int i = 0; i < PORTRAIT_COUNT; i++)
        if (strcmp(PORTRAIT_IDS[i], id) == 0) return i;
    return -1;
}

void art_portrait_draw(int idx, int x, int y, int w, int h)
{
    if (!portraits_load() || idx < 0 || idx >= PORTRAIT_COUNT || w < 8 || h < 8) return;
    /* вписываем, сохраняя пропорции ячейки */
    int dw = w, dh = w * PORTRAIT_CELL_H / PORTRAIT_CELL_W;
    if (dh > h) { dh = h; dw = h * PORTRAIT_CELL_W / PORTRAIT_CELL_H; }
    s_tick++;
    PorCache *hit = 0, *old = &s_pc[0];
    for (int i = 0; i < PCACHE; i++) {
        if (s_pc[i].img.px && s_pc[i].idx == idx && s_pc[i].w == dw && s_pc[i].h == dh) { hit = &s_pc[i]; break; }
        if (s_pc[i].tick < old->tick) old = &s_pc[i];
    }
    if (!hit) {
        hit = old;
        image_free(&hit->img);
        image_scale(&s_por, (idx % PORTRAIT_COLS) * PORTRAIT_CELL_W, (idx / PORTRAIT_COLS) * PORTRAIT_CELL_H,
                    PORTRAIT_CELL_W, PORTRAIT_CELL_H, &hit->img, dw, dh);
        hit->idx = idx; hit->w = dw; hit->h = dh;
    }
    hit->tick = s_tick;
    blit_clipped(&hit->img, x + (w - dw) / 2, y + (h - dh), 255);
}

/* ---------- фигурки войск ---------- */
static Image s_uni;
static int s_uniTried, s_uniOk;
#define UCACHE 24
typedef struct { int idx, w, h; unsigned tick; Image img; } UniCache;
static UniCache s_uc[UCACHE];

static int units_load(void)
{
    if (!s_uniTried) {
        s_uniTried = 1;
        s_uniOk = ASSETS_HAVE_UNITS && image_load_resource(RES_UNITS_QOI, &s_uni);
    }
    return s_uniOk;
}

int art_unit_index(const char *id)
{
    if (!id || !units_load()) return -1;
    for (int i = 0; i < UNIT_ATLAS_COUNT; i++)
        if (strcmp(UNIT_IDS[i], id) == 0) return i;
    return -1;
}

void art_unit_draw(int idx, int x, int y, int w, int h)
{
    if (!units_load() || idx < 0 || idx >= UNIT_ATLAS_COUNT || w < 8 || h < 8) return;
    int dw = w, dh = w * UNIT_CELL_H / UNIT_CELL_W;
    if (dh > h) { dh = h; dw = h * UNIT_CELL_W / UNIT_CELL_H; }
    s_tick++;
    UniCache *hit = 0, *old = &s_uc[0];
    for (int i = 0; i < UCACHE; i++) {
        if (s_uc[i].img.px && s_uc[i].idx == idx && s_uc[i].w == dw && s_uc[i].h == dh) { hit = &s_uc[i]; break; }
        if (s_uc[i].tick < old->tick) old = &s_uc[i];
    }
    if (!hit) {
        hit = old;
        image_free(&hit->img);
        image_scale(&s_uni, (idx % UNIT_COLS) * UNIT_CELL_W, (idx / UNIT_COLS) * UNIT_CELL_H,
                    UNIT_CELL_W, UNIT_CELL_H, &hit->img, dw, dh);
        hit->idx = idx; hit->w = dw; hit->h = dh;
    }
    hit->tick = s_tick;
    blit_clipped(&hit->img, x + (w - dw) / 2, y + (h - dh) / 2, 255);
}
