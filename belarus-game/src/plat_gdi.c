/* plat_gdi.c — подмножество Win32/GDI для сборки без Windows (iPhone; на Linux — для проверки).
 * Только то, чем пользуется интерфейс игры: контексты с буфером 0x00RRGGBB (как DIB в Windows),
 * перья, кисти (сплошные и штриховка), прямоугольное и многоугольное отсечение, линии, многоугольники,
 * сектор круга, копирование буферов, текст (через plat_font.h) с переносом строк и многоточием.
 * Окно, таймеры и файлы обслуживает хозяин (plat_host.h). */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#include "ui.h"
#include "plat_font.h"
#include "plat_host.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

enum { OB_PEN = 1, OB_BRUSH, OB_FONT, OB_RGN, OB_BMP };

typedef struct {
    int empty;
    RECT box;                    /* границы области */
    unsigned char *mask;         /* по пикселю на точку box; NULL — весь прямоугольник */
} Rgn;

struct ShimObj {
    int kind, stock;             /* stock: NULL_PEN, NULL_BRUSH, DC_PEN, DC_BRUSH или 0 */
    COLORREF color;
    int width, hatch;            /* перо: толщина; кисть: штриховка HS_* или -1 */
    wchar_t face[LF_FACESIZE];
    int px, weight, italic;
    PlatFont *pf;
    Rgn rgn;
    unsigned *bits;
    int bw, bh;
};

typedef struct {
    HPEN pen;
    HBRUSH brush;
    HFONT font;
    COLORREF text, dcPen, dcBrush;
    int extra, bkMode, arcDir;
    int clipOn;
    Rgn clip;
} DcState;

struct ShimDC {
    unsigned *bits;
    int w, h;
    HBITMAP bmp;
    POINT cur;
    DcState st;
    DcState saved[8];
    int nsaved;
};

static struct ShimObj s_stock[4] = {
    { OB_PEN, NULL_PEN, 0, 0, -1, { 0 }, 0, 0, 0, NULL, { 1, { 0, 0, 0, 0 }, NULL }, NULL, 0, 0 },
    { OB_BRUSH, NULL_BRUSH, 0, 0, -1, { 0 }, 0, 0, 0, NULL, { 1, { 0, 0, 0, 0 }, NULL }, NULL, 0, 0 },
    { OB_PEN, DC_PEN, 0, 1, -1, { 0 }, 0, 0, 0, NULL, { 1, { 0, 0, 0, 0 }, NULL }, NULL, 0, 0 },
    { OB_BRUSH, DC_BRUSH, 0, 0, -1, { 0 }, 0, 0, 0, NULL, { 1, { 0, 0, 0, 0 }, NULL }, NULL, 0, 0 },
};

HGDIOBJ GetStockObject(int i)
{
    for (int k = 0; k < 4; k++) if (s_stock[k].stock == i) return &s_stock[k];
    return &s_stock[3];
}

static int is_stock(HGDIOBJ o) { return o >= &s_stock[0] && o <= &s_stock[3]; }

/* ---------- области ---------- */
static void rgn_free(Rgn *r) { free(r->mask); r->mask = NULL; r->empty = 1; }

static int rgn_in(const Rgn *r, int x, int y)
{
    if (r->empty || x < r->box.left || x >= r->box.right || y < r->box.top || y >= r->box.bottom) return 0;
    return !r->mask || r->mask[(size_t)(y - r->box.top) * (size_t)(r->box.right - r->box.left) + (size_t)(x - r->box.left)];
}

static void rgn_copy(Rgn *d, const Rgn *s)
{
    *d = *s;
    d->mask = NULL;
    if (s->mask && !s->empty) {
        size_t n = (size_t)(s->box.right - s->box.left) * (size_t)(s->box.bottom - s->box.top);
        d->mask = (unsigned char *)malloc(n);
        if (d->mask) memcpy(d->mask, s->mask, n);
        else d->empty = 1;
    }
}

/* пересечения строки (центр пикселя y + 0.5) с рёбрами многоугольника -> отрезки заливки */
typedef void (*SpanFn)(void *ctx, int y, int x0, int x1);
typedef struct { double x; int dir; } Cross;

static int cross_cmp(const void *a, const void *b)
{
    double d = ((const Cross *)a)->x - ((const Cross *)b)->x;
    return d < 0 ? -1 : d > 0;
}

static void poly_spans(const POINT *p, int n, int winding, int ylo, int yhi, SpanFn fn, void *ctx)
{
    if (n < 3) return;
    int ymin = p[0].y, ymax = p[0].y;
    for (int i = 1; i < n; i++) { if (p[i].y < ymin) ymin = p[i].y; if (p[i].y > ymax) ymax = p[i].y; }
    if (ymin < ylo) ymin = ylo;
    if (ymax > yhi) ymax = yhi;
    Cross stackc[64], *c = n <= 64 ? stackc : (Cross *)malloc(sizeof(Cross) * (size_t)n);
    if (!c) return;
    for (int y = ymin; y < ymax; y++) {
        double yc = y + 0.5;
        int k = 0;
        for (int i = 0; i < n; i++) {
            const POINT *a = &p[i], *b = &p[(i + 1) % n];
            if (a->y == b->y) continue;
            double y0 = a->y, y1 = b->y;
            if ((yc < y0 && yc < y1) || (yc >= y0 && yc >= y1)) continue;
            c[k].x = a->x + (yc - y0) * (b->x - a->x) / (y1 - y0);
            c[k].dir = b->y > a->y ? 1 : -1;
            k++;
        }
        if (k < 2) continue;
        qsort(c, (size_t)k, sizeof(Cross), cross_cmp);
        if (!winding) {
            for (int i = 0; i + 1 < k; i += 2) fn(ctx, y, (int)ceil(c[i].x - 0.5), (int)ceil(c[i + 1].x - 0.5));
        } else {
            int wn = 0;
            for (int i = 0; i + 1 < k; i++) {
                wn += c[i].dir;
                if (wn != 0) fn(ctx, y, (int)ceil(c[i].x - 0.5), (int)ceil(c[i + 1].x - 0.5));
            }
        }
    }
    if (c != stackc) free(c);
}

static void span_mask(void *ctx, int y, int x0, int x1)
{
    Rgn *r = (Rgn *)ctx;
    int w = r->box.right - r->box.left;
    if (x0 < r->box.left) x0 = r->box.left;
    if (x1 > r->box.right) x1 = r->box.right;
    if (x0 < x1) memset(r->mask + (size_t)(y - r->box.top) * (size_t)w + (size_t)(x0 - r->box.left), 1, (size_t)(x1 - x0));
}

static HRGN rgn_new(void)
{
    struct ShimObj *o = (struct ShimObj *)calloc(1, sizeof *o);
    if (o) { o->kind = OB_RGN; o->rgn.empty = 1; }
    return o;
}

HRGN CreateRectRgn(int l, int t, int r, int b)
{
    HRGN o = rgn_new();
    if (!o) return NULL;
    if (r > l && b > t) { o->rgn.empty = 0; o->rgn.box.left = l; o->rgn.box.top = t; o->rgn.box.right = r; o->rgn.box.bottom = b; }
    return o;
}

HRGN CreatePolygonRgn(const POINT *p, int n, int mode)
{
    HRGN o = rgn_new();
    if (!o || n < 3) return o;
    RECT b = { p[0].x, p[0].y, p[0].x, p[0].y };
    for (int i = 1; i < n; i++) {
        if (p[i].x < b.left) b.left = p[i].x;
        if (p[i].x > b.right) b.right = p[i].x;
        if (p[i].y < b.top) b.top = p[i].y;
        if (p[i].y > b.bottom) b.bottom = p[i].y;
    }
    b.right++; b.bottom++;
    size_t sz = (size_t)(b.right - b.left) * (size_t)(b.bottom - b.top);
    if (!sz || sz > 64u * 1024u * 1024u) return o;
    o->rgn.mask = (unsigned char *)calloc(sz, 1);
    if (!o->rgn.mask) return o;
    o->rgn.box = b;
    o->rgn.empty = 0;
    poly_spans(p, n, mode == WINDING, b.top, b.bottom, span_mask, &o->rgn);
    return o;
}

/* RGN_AND, RGN_DIFF и RGN_COPY; RGN_OR — по ограничивающему прямоугольнику (в игре не нужен) */
int CombineRgn(HRGN dst, HRGN a, HRGN b, int mode)
{
    if (!dst || !a) return 0;
    Rgn res = { 1, { 0, 0, 0, 0 }, NULL };
    const Rgn *ra = &a->rgn, *rb = b ? &b->rgn : NULL;
    if (mode == RGN_COPY || !rb) rgn_copy(&res, ra);
    else if (!ra->empty) {
        RECT box = ra->box;
        if (mode == RGN_AND) {
            if (rb->empty) box.right = box.left;
            else {
                if (rb->box.left > box.left) box.left = rb->box.left;
                if (rb->box.top > box.top) box.top = rb->box.top;
                if (rb->box.right < box.right) box.right = rb->box.right;
                if (rb->box.bottom < box.bottom) box.bottom = rb->box.bottom;
            }
        }
        if (box.right > box.left && box.bottom > box.top) {
            res.empty = 0;
            res.box = box;
            if (mode == RGN_AND && !ra->mask && !rb->mask) res.mask = NULL;
            else {
                int w = box.right - box.left, h = box.bottom - box.top;
                res.mask = (unsigned char *)malloc((size_t)w * (size_t)h);
                if (!res.mask) res.empty = 1;
                else
                    for (int y = 0; y < h; y++)
                        for (int x = 0; x < w; x++) {
                            int px = box.left + x, py = box.top + y, ina = rgn_in(ra, px, py), inb = rgn_in(rb, px, py);
                            res.mask[(size_t)y * (size_t)w + (size_t)x] = (unsigned char)(mode == RGN_AND ? ina && inb : mode == RGN_DIFF ? ina && !inb : ina || inb);
                        }
            }
        }
    }
    rgn_free(&dst->rgn);
    dst->rgn = res;
    return res.empty ? 1 : res.mask ? 3 : 2;
}

int SelectClipRgn(HDC dc, HRGN r)
{
    if (!dc) return 0;
    rgn_free(&dc->st.clip);
    dc->st.clipOn = r != NULL;
    if (r) rgn_copy(&dc->st.clip, &r->rgn);
    return r ? 2 : 1;
}

int ExtSelectClipRgn(HDC dc, HRGN r, int mode)
{
    if (!dc) return 0;
    if (!dc->st.clipOn || mode == RGN_COPY) return SelectClipRgn(dc, r);
    struct ShimObj cur, res;
    memset(&cur, 0, sizeof cur);
    memset(&res, 0, sizeof res);
    cur.kind = res.kind = OB_RGN;
    cur.rgn = dc->st.clip;
    res.rgn.empty = 1;
    CombineRgn(&res, &cur, r, mode);
    rgn_free(&dc->st.clip);
    dc->st.clip = res.rgn;
    return 2;
}

/* ---------- контексты и битмапы ---------- */
static void state_init(DcState *s)
{
    memset(s, 0, sizeof *s);
    s->pen = GetStockObject(DC_PEN);
    s->brush = GetStockObject(DC_BRUSH);
    s->text = RGB(0, 0, 0);
    s->bkMode = 2;
    s->arcDir = 1;
    s->clip.empty = 1;
}

HDC CreateCompatibleDC(HDC like)
{
    (void)like;
    HDC dc = (HDC)calloc(1, sizeof *dc);
    if (dc) state_init(&dc->st);
    return dc;
}

BOOL DeleteDC(HDC dc)
{
    if (!dc) return FALSE;
    rgn_free(&dc->st.clip);
    for (int i = 0; i < dc->nsaved; i++) rgn_free(&dc->saved[i].clip);
    free(dc);
    return TRUE;
}

HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO *bi, UINT usage, void **bits, HANDLE sec, DWORD off)
{
    (void)dc; (void)usage; (void)sec; (void)off;
    int w = bi->bmiHeader.biWidth, h = bi->bmiHeader.biHeight < 0 ? -bi->bmiHeader.biHeight : bi->bmiHeader.biHeight;
    if (w <= 0 || h <= 0) return NULL;
    struct ShimObj *o = (struct ShimObj *)calloc(1, sizeof *o);
    if (!o) return NULL;
    o->kind = OB_BMP;
    o->bits = (unsigned *)calloc((size_t)w * (size_t)h, sizeof(unsigned));
    if (!o->bits) { free(o); return NULL; }
    o->bw = w; o->bh = h;
    if (bits) *bits = o->bits;
    return o;
}

HGDIOBJ SelectObject(HDC dc, HGDIOBJ o)
{
    if (!dc || !o) return NULL;
    HGDIOBJ old = NULL;
    switch (o->kind) {
    case OB_PEN: old = dc->st.pen; dc->st.pen = o; break;
    case OB_BRUSH: old = dc->st.brush; dc->st.brush = o; break;
    case OB_FONT: old = dc->st.font; dc->st.font = o; break;
    case OB_BMP:
        old = dc->bmp;
        dc->bmp = o; dc->bits = o->bits; dc->w = o->bw; dc->h = o->bh;
        break;
    case OB_RGN: SelectClipRgn(dc, o); return (HGDIOBJ)(intptr_t)2;
    }
    if (!old && o->kind == OB_BMP) {
        static struct ShimObj none = { OB_BMP, 0, 0, 0, -1, { 0 }, 0, 0, 0, NULL, { 1, { 0, 0, 0, 0 }, NULL }, NULL, 0, 0 };
        old = &none;
    }
    return old;
}

BOOL DeleteObject(HGDIOBJ o)
{
    if (!o || is_stock(o)) return TRUE;
    if (o->kind == OB_BMP && !o->bits) return TRUE;           /* «пустой» битмап из SelectObject */
    if (o->kind == OB_FONT && o->pf) pf_close(o->pf);
    if (o->kind == OB_RGN) rgn_free(&o->rgn);
    if (o->kind == OB_BMP) free(o->bits);
    free(o);
    return TRUE;
}

COLORREF SetDCBrushColor(HDC dc, COLORREF c) { COLORREF o = dc->st.dcBrush; dc->st.dcBrush = c; return o; }
COLORREF SetDCPenColor(HDC dc, COLORREF c) { COLORREF o = dc->st.dcPen; dc->st.dcPen = c; return o; }
COLORREF SetTextColor(HDC dc, COLORREF c) { COLORREF o = dc->st.text; dc->st.text = c; return o; }
int SetTextCharacterExtra(HDC dc, int e) { int o = dc->st.extra; dc->st.extra = e; return o; }
int SetBkMode(HDC dc, int m) { int o = dc->st.bkMode; dc->st.bkMode = m; return o; }
int SetArcDirection(HDC dc, int d) { int o = dc->st.arcDir; dc->st.arcDir = d; return o; }

int SaveDC(HDC dc)
{
    if (!dc || dc->nsaved >= 8) return 0;
    DcState *s = &dc->saved[dc->nsaved++];
    *s = dc->st;
    rgn_copy(&s->clip, &dc->st.clip);
    return dc->nsaved;
}

BOOL RestoreDC(HDC dc, int saved)
{
    if (!dc) return FALSE;
    if (saved < 0) saved = dc->nsaved + 1 + saved;
    if (saved < 1 || saved > dc->nsaved) return FALSE;
    while (dc->nsaved > saved) rgn_free(&dc->saved[--dc->nsaved].clip);
    rgn_free(&dc->st.clip);
    dc->st = dc->saved[--dc->nsaved];
    return TRUE;
}

HPEN CreatePen(int style, int width, COLORREF c)
{
    (void)style;
    struct ShimObj *o = (struct ShimObj *)calloc(1, sizeof *o);
    if (o) { o->kind = OB_PEN; o->color = c; o->width = width < 1 ? 1 : width; o->hatch = -1; }
    return o;
}

HBRUSH CreateHatchBrush(int style, COLORREF c)
{
    struct ShimObj *o = (struct ShimObj *)calloc(1, sizeof *o);
    if (o) { o->kind = OB_BRUSH; o->color = c; o->hatch = style; }
    return o;
}

/* ---------- пиксели ---------- */
static inline unsigned px_of(COLORREF c) { return (unsigned)GetRValue(c) << 16 | (unsigned)GetGValue(c) << 8 | GetBValue(c); }

static inline int vis(HDC dc, int x, int y)
{
    if (x < 0 || y < 0 || x >= dc->w || y >= dc->h) return 0;
    return !dc->st.clipOn || rgn_in(&dc->st.clip, x, y);
}

static inline void blend(unsigned *p, COLORREF c, int a)
{
    if (a >= 255) { *p = px_of(c); return; }
    if (a <= 0) return;
    unsigned d = *p, ia = 255 - (unsigned)a;
    unsigned r = (((d >> 16) & 255) * ia + GetRValue(c) * (unsigned)a + 127) / 255;
    unsigned g = (((d >> 8) & 255) * ia + GetGValue(c) * (unsigned)a + 127) / 255;
    unsigned b = ((d & 255) * ia + GetBValue(c) * (unsigned)a + 127) / 255;
    *p = r << 16 | g << 8 | b;
}

/* строка пикселей с учётом отсечения; hatch >= 0 — только линии штриховки */
typedef struct { HDC dc; COLORREF c; int hatch; } FillCtx;

static void span_fill(void *ctx, int y, int x0, int x1)
{
    FillCtx *f = (FillCtx *)ctx;
    HDC dc = f->dc;
    if (y < 0 || y >= dc->h) return;
    if (x0 < 0) x0 = 0;
    if (x1 > dc->w) x1 = dc->w;
    const Rgn *cl = dc->st.clipOn ? &dc->st.clip : NULL;
    if (cl) {
        if (cl->empty || y < cl->box.top || y >= cl->box.bottom) return;
        if (x0 < cl->box.left) x0 = cl->box.left;
        if (x1 > cl->box.right) x1 = cl->box.right;
    }
    unsigned v = px_of(f->c), *row = dc->bits + (size_t)y * (size_t)dc->w;
    for (int x = x0; x < x1; x++) {
        if (cl && cl->mask && !cl->mask[(size_t)(y - cl->box.top) * (size_t)(cl->box.right - cl->box.left) + (size_t)(x - cl->box.left)]) continue;
        if (f->hatch >= 0) {
            int d1 = ((x + y) & 7) == 0, d2 = ((x - y) & 7) == 0;
            if (!(f->hatch == HS_DIAGCROSS ? d1 || d2 : f->hatch == HS_BDIAGONAL ? d1 : d2)) continue;
        }
        row[x] = v;
    }
}

static int brush_color(HDC dc, HBRUSH b, COLORREF *c, int *hatch)
{
    if (!b || b->stock == NULL_BRUSH) return 0;
    *c = b->stock == DC_BRUSH ? dc->st.dcBrush : b->color;
    *hatch = b->stock ? -1 : b->hatch;
    return 1;
}

static int pen_color(HDC dc, COLORREF *c, int *w)
{
    HPEN p = dc->st.pen;
    if (!p || p->stock == NULL_PEN) return 0;
    *c = p->stock == DC_PEN ? dc->st.dcPen : p->color;
    *w = p->stock ? 1 : p->width;
    return 1;
}

BOOL FillRect(HDC dc, const RECT *r, HBRUSH b)
{
    COLORREF c;
    int hatch;
    if (!dc || !dc->bits || !brush_color(dc, b, &c, &hatch)) return FALSE;
    FillCtx f = { dc, c, hatch };
    for (int y = r->top; y < r->bottom; y++) span_fill(&f, y, r->left, r->right);
    return TRUE;
}

BOOL FrameRect(HDC dc, const RECT *r, HBRUSH b)
{
    if (r->right <= r->left || r->bottom <= r->top) return FALSE;
    RECT e[4] = { { r->left, r->top, r->right, r->top + 1 }, { r->left, r->bottom - 1, r->right, r->bottom },
                  { r->left, r->top, r->left + 1, r->bottom }, { r->right - 1, r->top, r->right, r->bottom } };
    for (int i = 0; i < 4; i++) FillRect(dc, &e[i], b);
    return TRUE;
}

/* линия: толщина 1 — как в GDI, без последней точки; толще — со скруглёнными концами и сглаживанием */
static void line_px(HDC dc, int x0, int y0, int x1, int y1, COLORREF c, int w)
{
    if (!dc->bits) return;
    if (w <= 1) {
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
        unsigned v = px_of(c);
        for (int guard = 0; guard < 100000 && (x0 != x1 || y0 != y1); guard++) {
            if (vis(dc, x0, y0)) dc->bits[(size_t)y0 * (size_t)dc->w + (size_t)x0] = v;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
        return;
    }
    double hw = w * 0.5, ax = x0 + 0.5, ay = y0 + 0.5, bx = x1 + 0.5, by = y1 + 0.5;
    double vx = bx - ax, vy = by - ay, L2 = vx * vx + vy * vy;
    int xa = (int)floor((ax < bx ? ax : bx) - hw - 1), xb = (int)ceil((ax > bx ? ax : bx) + hw + 1);
    int ya = (int)floor((ay < by ? ay : by) - hw - 1), yb = (int)ceil((ay > by ? ay : by) + hw + 1);
    if (xa < 0) xa = 0;
    if (ya < 0) ya = 0;
    if (xb > dc->w) xb = dc->w;
    if (yb > dc->h) yb = dc->h;
    for (int y = ya; y < yb; y++)
        for (int x = xa; x < xb; x++) {
            double px = x + 0.5 - ax, py = y + 0.5 - ay, t = L2 > 0 ? (px * vx + py * vy) / L2 : 0;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            double dx = px - vx * t, dy = py - vy * t, d = sqrt(dx * dx + dy * dy);
            double cov = hw + 0.5 - d;
            if (cov <= 0 || !vis(dc, x, y)) continue;
            blend(&dc->bits[(size_t)y * (size_t)dc->w + (size_t)x], c, cov >= 1 ? 255 : (int)(cov * 255));
        }
}

BOOL MoveToEx(HDC dc, int x, int y, POINT *old)
{
    if (old) *old = dc->cur;
    dc->cur.x = x; dc->cur.y = y;
    return TRUE;
}

BOOL LineTo(HDC dc, int x, int y)
{
    COLORREF c;
    int w;
    if (pen_color(dc, &c, &w)) line_px(dc, dc->cur.x, dc->cur.y, x, y, c, w);
    dc->cur.x = x; dc->cur.y = y;
    return TRUE;
}

BOOL Polyline(HDC dc, const POINT *p, int n)
{
    COLORREF c;
    int w;
    if (n < 2 || !pen_color(dc, &c, &w)) return FALSE;
    for (int i = 0; i + 1 < n; i++) line_px(dc, p[i].x, p[i].y, p[i + 1].x, p[i + 1].y, c, w);
    return TRUE;
}

BOOL Polygon(HDC dc, const POINT *p, int n)
{
    if (!dc || !dc->bits || n < 2) return FALSE;
    COLORREF c;
    int hatch, w;
    if (n >= 3 && brush_color(dc, dc->st.brush, &c, &hatch)) {
        FillCtx f = { dc, c, hatch };
        poly_spans(p, n, 0, 0, dc->h, span_fill, &f);
    }
    if (pen_color(dc, &c, &w))
        for (int i = 0; i < n; i++) line_px(dc, p[i].x, p[i].y, p[(i + 1) % n].x, p[(i + 1) % n].y, c, w);
    return TRUE;
}

/* сектор: от луча на (xs, ys) до луча на (xe, ye) по направлению дуги */
BOOL Pie(HDC dc, int l, int t, int r, int b, int xs, int ys, int xe, int ye)
{
    COLORREF c;
    int hatch;
    if (!dc || !dc->bits || !brush_color(dc, dc->st.brush, &c, &hatch)) return FALSE;
    double cx = (l + r) * 0.5, cy = (t + b) * 0.5, rx = (r - l) * 0.5, ry = (b - t) * 0.5;
    const double PI2 = 6.283185307179586;
    double a0 = atan2(ys - cy, xs - cx), a1 = atan2(ye - cy, xe - cx);
    double span = dc->st.arcDir == AD_CLOCKWISE ? fmod(a1 - a0 + 2 * PI2, PI2) : fmod(a0 - a1 + 2 * PI2, PI2);
    unsigned v = px_of(c);
    for (int y = t; y < b; y++)
        for (int x = l; x < r; x++) {
            double dx = (x + 0.5 - cx) / rx, dy = (y + 0.5 - cy) / ry;
            if (dx * dx + dy * dy > 1 || !vis(dc, x, y)) continue;
            double a = atan2(y + 0.5 - cy, x + 0.5 - cx);
            double d = dc->st.arcDir == AD_CLOCKWISE ? fmod(a - a0 + 2 * PI2, PI2) : fmod(a0 - a + 2 * PI2, PI2);
            if (d <= span) dc->bits[(size_t)y * (size_t)dc->w + (size_t)x] = v;
        }
    return TRUE;
}

BOOL Ellipse(HDC dc, int l, int t, int r, int b)
{
    if (!dc || !dc->bits || r <= l || b <= t) return FALSE;
    COLORREF c;
    int hatch, w;
    double cx = (l + r) * 0.5, cy = (t + b) * 0.5, rx = (r - l) * 0.5, ry = (b - t) * 0.5;
    if (brush_color(dc, dc->st.brush, &c, &hatch)) {
        FillCtx f = { dc, c, hatch };
        for (int y = t; y < b; y++) {
            double dy = (y + 0.5 - cy) / ry;
            if (dy * dy >= 1) continue;
            double hw = rx * sqrt(1 - dy * dy);
            span_fill(&f, y, (int)ceil(cx - hw - 0.5), (int)ceil(cx + hw - 0.5));
        }
    }
    if (pen_color(dc, &c, &w)) {
        POINT prev = { 0, 0 };
        for (int k = 0; k <= 48; k++) {
            double a = 6.283185307179586 * k / 48;
            POINT p = { (LONG)lround(cx + cos(a) * rx), (LONG)lround(cy + sin(a) * ry) };
            if (k) line_px(dc, prev.x, prev.y, p.x, p.y, c, w);
            prev = p;
        }
    }
    return TRUE;
}

BOOL GdiFlush(void) { return TRUE; }

BOOL BitBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy, DWORD rop)
{
    (void)rop;
    if (!dst || !src || !dst->bits || !src->bits) return FALSE;
    for (int j = 0; j < h; j++) {
        int dy = y + j, syy = sy + j;
        if (dy < 0 || dy >= dst->h || syy < 0 || syy >= src->h) continue;
        for (int i = 0; i < w; i++) {
            int dx = x + i, sxx = sx + i;
            if (sxx < 0 || sxx >= src->w || !vis(dst, dx, dy)) continue;
            dst->bits[(size_t)dy * (size_t)dst->w + (size_t)dx] = src->bits[(size_t)syy * (size_t)src->w + (size_t)sxx];
        }
    }
    return TRUE;
}

/* ---------- текст ---------- */
HFONT CreateFontW(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike, DWORD charset,
                  DWORD outPrec, DWORD clipPrec, DWORD quality, DWORD pitch, LPCWSTR face)
{
    (void)w; (void)esc; (void)orient; (void)underline; (void)strike; (void)charset; (void)outPrec; (void)clipPrec; (void)quality; (void)pitch;
    struct ShimObj *o = (struct ShimObj *)calloc(1, sizeof *o);
    if (!o) return NULL;
    o->kind = OB_FONT;
    o->px = h < 0 ? -h : h > 0 ? h * 4 / 5 : 13;
    if (o->px < 4) o->px = 4;
    o->weight = weight ? weight : FW_NORMAL;
    o->italic = italic != 0;
    wcsncpy(o->face, face ? face : L"Segoe UI", LF_FACESIZE - 1);
    return o;
}

static PlatFont *dc_font(HDC dc)
{
    static HFONT def;
    HFONT f = dc->st.font;
    if (!f) {
        if (!def) def = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        f = def;
    }
    if (f && !f->pf) f->pf = pf_open(f->face, f->px, f->weight, f->italic);
    return f ? f->pf : NULL;
}

static void text_out(HDC dc, int x, int y, const wchar_t *s, int n)
{
    PlatFont *pf = dc_font(dc);
    if (!pf || n <= 0 || !dc->bits) return;
    int w, h, ox, oy;
    unsigned char *m = pf_render(pf, s, n, dc->st.extra, &w, &h, &ox, &oy);
    if (!m) return;
    COLORREF c = dc->st.text;
    for (int j = 0; j < h; j++) {
        int py = y + oy + j;
        if (py < 0 || py >= dc->h) continue;
        for (int i = 0; i < w; i++) {
            int a = m[(size_t)j * (size_t)w + (size_t)i], px = x + ox + i;
            if (a && vis(dc, px, py)) blend(&dc->bits[(size_t)py * (size_t)dc->w + (size_t)px], c, a);
        }
    }
    free(m);
}

BOOL TextOutW(HDC dc, int x, int y, LPCWSTR s, int n)
{
    if (!dc || !s) return FALSE;
    text_out(dc, x, y, s, n);
    return TRUE;
}

BOOL GetTextExtentPoint32W(HDC dc, LPCWSTR s, int n, SIZE *sz)
{
    PlatFont *pf = dc ? dc_font(dc) : NULL;
    int a = 0, d = 0, avg = 0;
    if (pf) pf_metrics(pf, &a, &d, &avg);
    sz->cx = pf && s && n > 0 ? pf_width(pf, s, n, dc->st.extra) : 0;
    sz->cy = a + d;
    return pf != NULL;
}

BOOL GetTextMetricsW(HDC dc, TEXTMETRICW *tm)
{
    PlatFont *pf = dc ? dc_font(dc) : NULL;
    memset(tm, 0, sizeof *tm);
    if (!pf) return FALSE;
    int a, d, avg;
    pf_metrics(pf, &a, &d, &avg);
    tm->tmAscent = a; tm->tmDescent = d; tm->tmHeight = a + d; tm->tmAveCharWidth = avg; tm->tmMaxCharWidth = avg * 2;
    return TRUE;
}

static int tw(HDC dc, const wchar_t *s, int n)
{
    PlatFont *pf = dc_font(dc);
    return pf && n > 0 ? pf_width(pf, s, n, dc->st.extra) : 0;
}

/* DrawTextW: одна строка (по центру или с многоточием) или перенос по словам; DT_CALCRECT — только размер */
int DrawTextW(HDC dc, LPCWSTR s, int n, RECT *r, UINT fmt)
{
    if (!dc || !s || !r) return 0;
    if (n < 0) n = (int)wcslen(s);
    TEXTMETRICW tm;
    GetTextMetricsW(dc, &tm);
    int lh = tm.tmHeight, rw = r->right - r->left, calc = (fmt & DT_CALCRECT) != 0;
    if (fmt & DT_SINGLELINE) {
        int w = tw(dc, s, n);
        if ((fmt & DT_END_ELLIPSIS) && w > rw) {
            static const wchar_t ELL[] = L"\x2026";
            int ew = tw(dc, ELL, 1), k = n;
            while (k > 0 && tw(dc, s, k) + ew > rw) k--;
            while (k > 0 && s[k - 1] == L' ') k--;
            wchar_t buf[512];
            if (k > 510) k = 510;
            memcpy(buf, s, sizeof(wchar_t) * (size_t)k);
            buf[k] = ELL[0];
            if (!calc) text_out(dc, r->left, r->top, buf, k + 1);
            w = tw(dc, buf, k + 1);
        } else if (!calc) text_out(dc, (fmt & DT_CENTER) ? r->left + (rw - w) / 2 : r->left, r->top, s, n);
        if (calc) { r->right = r->left + w; r->bottom = r->top + lh; }
        return lh;
    }
    /* перенос по словам; '\n' — новая строка */
    int y = r->top, maxw = 0, i = 0;
    for (;;) {
        int ls = i, end = i, j = i;
        while (j < n && s[j] != L'\n' && s[j] != L'\r') {
            int we = j;
            while (we < n && s[we] != L' ' && s[we] != L'\n' && s[we] != L'\r') we++;   /* конец слова */
            if (end > ls && tw(dc, s + ls, we - ls) > rw) break;                        /* не помещается — перенос */
            end = we;
            j = we;
            while (j < n && s[j] == L' ') j++;
        }
        int w = tw(dc, s + ls, end - ls);
        if (w > maxw) maxw = w;
        if (!calc && end > ls) text_out(dc, (fmt & DT_CENTER) ? r->left + (rw - w) / 2 : r->left, y, s + ls, end - ls);
        y += lh;
        i = end;
        while (i < n && s[i] == L' ') i++;
        if (i < n && s[i] == L'\r') i++;
        if (i < n && s[i] == L'\n') i++;
        if (i >= n) break;
    }
    if (calc) { r->right = r->left + maxw; r->bottom = y; }
    return y - r->top;
}

/* ---------- прямоугольники ---------- */
BOOL IntersectRect(RECT *d, const RECT *a, const RECT *b)
{
    d->left = a->left > b->left ? a->left : b->left;
    d->top = a->top > b->top ? a->top : b->top;
    d->right = a->right < b->right ? a->right : b->right;
    d->bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    if (d->left >= d->right || d->top >= d->bottom) { memset(d, 0, sizeof *d); return FALSE; }
    return TRUE;
}

BOOL PtInRect(const RECT *r, POINT p) { return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom; }

/* ---------- время, окно, таймеры ---------- */
DWORD GetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)((uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u);
}

BOOL InvalidateRect(HWND h, const RECT *r, BOOL erase) { (void)h; (void)r; (void)erase; host_invalidate(); return TRUE; }
uintptr_t SetTimer(HWND h, uintptr_t id, UINT ms, void *fn) { (void)h; (void)fn; host_set_timer((int)id, (int)ms); return id; }
BOOL KillTimer(HWND h, uintptr_t id) { (void)h; host_kill_timer((int)id); return TRUE; }
short GetKeyState(int vk) { (void)vk; return 0; }

POINT g_plat_cursor;
BOOL GetCursorPos(POINT *p) { *p = g_plat_cursor; return TRUE; }
BOOL ScreenToClient(HWND h, POINT *p) { (void)h; (void)p; return TRUE; }

/* в верхний регистр: латиница и кириллица (белорусская і, ў тоже) — без зависимости от локали */
DWORD CharUpperBuffW(LPWSTR s, DWORD n)
{
    for (DWORD i = 0; i < n; i++) {
        wchar_t c = s[i];
        if (c >= L'a' && c <= L'z') s[i] = (wchar_t)(c - 32);
        else if (c >= 0x430 && c <= 0x44F) s[i] = (wchar_t)(c - 0x20);
        else if (c >= 0x450 && c <= 0x45F) s[i] = (wchar_t)(c - 0x50);
    }
    return n;
}

/* ---------- файлы ---------- */
static void to_utf8(const wchar_t *s, char *out, size_t n)
{
    size_t k = 0;
    for (; s && *s && k + 4 < n; s++) {
        unsigned c = (unsigned)*s;
        if (c < 0x80) out[k++] = (char)c;
        else if (c < 0x800) { out[k++] = (char)(0xC0 | c >> 6); out[k++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { out[k++] = (char)(0xE0 | c >> 12); out[k++] = (char)(0x80 | ((c >> 6) & 63)); out[k++] = (char)(0x80 | (c & 63)); }
        else { out[k++] = (char)(0xF0 | c >> 18); out[k++] = (char)(0x80 | ((c >> 12) & 63)); out[k++] = (char)(0x80 | ((c >> 6) & 63)); out[k++] = (char)(0x80 | (c & 63)); }
    }
    out[k] = 0;
}

FILE *_wfopen(const wchar_t *path, const wchar_t *mode)
{
    char p[1024], m[16];
    to_utf8(path, p, sizeof p);
    to_utf8(mode, m, sizeof m);
    return fopen(p, m);
}

int _wtoi(const wchar_t *s) { return s ? (int)wcstol(s, NULL, 10) : 0; }

/* ресурс по номеру: строка «N RCDATA "файл"» в assets.rc рядом с ресурсами */
int plat_resource(int id, void **data, int *size)
{
    char path[1024], line[512], name[256];
    *data = NULL; *size = 0;
    snprintf(path, sizeof path, "%s/assets.rc", host_res_dir());
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    name[0] = 0;
    while (fgets(line, sizeof line, f)) {
        int n;
        char file[256];
        if (sscanf(line, "%d RCDATA \"%255[^\"]\"", &n, file) == 2 && n == id) { snprintf(name, sizeof name, "%s", file); break; }
    }
    fclose(f);
    if (!name[0]) return 0;
    snprintf(path, sizeof path, "%s/%s", host_res_dir(), name);
    f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *p = sz > 0 ? malloc((size_t)sz) : NULL;
    if (p && fread(p, 1, (size_t)sz, f) == (size_t)sz) { *data = p; *size = (int)sz; }
    else free(p);
    fclose(f);
    return *data != NULL;
}

#endif /* !_WIN32 */
