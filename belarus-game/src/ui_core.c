/* Окно, двойная буферизация, шрифты, примитивы рисования, виджеты, сохранения. */
#include "ui.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

Pal P;
int g_dark = 1;
float g_s = 1.0f;
int g_w = 1280, g_h = 800;
HWND g_hwnd;
int g_dev;
Game G;
UiState U;

static float s_dpi = 1.0f;
static HDC s_dc;
static HBITMAP s_bmp, s_oldBmp;
static unsigned *s_bits;
static int s_bw, s_bh;

/* светлая — «льняная бумага»: тёплый лён, чернила, кармин и золото вышивки */
static const Pal PAL_LIGHT = {
    .bg = RGB(240, 234, 222), .surface = RGB(253, 250, 244), .surface2 = RGB(238, 231, 218),
    .ink = RGB(30, 34, 40), .muted = RGB(104, 108, 112), .line = RGB(222, 212, 194),
    .red = RGB(186, 28, 48), .redSoft = RGB(248, 226, 226), .blue = RGB(44, 92, 196), .blueSoft = RGB(226, 234, 250),
    .green = RGB(36, 128, 84), .gold = RGB(170, 118, 20), .land = RGB(236, 230, 214), .landEdge = RGB(70, 70, 66),
    .water = RGB(76, 120, 186), .forest = RGB(92, 136, 84), .shadow = RGB(40, 30, 20), .onAccent = RGB(255, 255, 255)
};
/* тёмная — «полночный музей»: глубокий сине-зелёный, золото и кармин */
static const Pal PAL_DARK = {
    .bg = RGB(10, 15, 20), .surface = RGB(20, 27, 35), .surface2 = RGB(31, 41, 52),
    .ink = RGB(238, 241, 244), .muted = RGB(150, 162, 174), .line = RGB(46, 59, 72),
    .red = RGB(236, 76, 96), .redSoft = RGB(70, 30, 40), .blue = RGB(118, 168, 255), .blueSoft = RGB(30, 46, 76),
    .green = RGB(98, 206, 146), .gold = RGB(240, 184, 82), .land = RGB(33, 44, 52), .landEdge = RGB(176, 186, 196),
    .water = RGB(92, 138, 210), .forest = RGB(78, 124, 82), .shadow = RGB(0, 0, 0), .onAccent = RGB(255, 255, 255)
};

/* ---------- шрифты ---------- */
static HFONT s_fonts[F_COUNT];
static int s_era = 1;
static wchar_t s_eraFace[ERA_COUNT + 1][LF_FACESIZE];
static const wchar_t *ERA_FACES[ERA_COUNT + 1][4] = {
    { 0 },
    { L"Palatino Linotype", L"Book Antiqua", L"Georgia", 0 },
    { L"Palatino Linotype", L"Book Antiqua", L"Georgia", 0 },
    { L"Constantia", L"Georgia", L"Times New Roman", 0 },
    { L"Cambria", L"Georgia", L"Times New Roman", 0 },
    { L"Bahnschrift SemiBold Condensed", L"Bahnschrift", L"Arial Narrow", L"Impact" },
    { L"Bahnschrift SemiBold Condensed", L"Bahnschrift", L"Arial Narrow", L"Impact" },
    { L"Bahnschrift SemiBold Condensed", L"Bahnschrift", L"Arial Narrow", L"Impact" },
    { L"Consolas", L"Lucida Console", L"Courier New", 0 },
};

static int CALLBACK face_cb(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM lp)
{
    (void)lf; (void)tm; (void)type;
    *(int *)lp = 1;
    return 0;
}

static int face_exists(const wchar_t *name)
{
    LOGFONTW lf;
    int found = 0;
    memset(&lf, 0, sizeof lf);
    lf.lfCharSet = DEFAULT_CHARSET;
    wcsncpy(lf.lfFaceName, name, LF_FACESIZE - 1);
    HDC dc = GetDC(NULL);
    EnumFontFamiliesExW(dc, &lf, face_cb, (LPARAM)&found, 0);
    ReleaseDC(NULL, dc);
    return found;
}

static void resolve_faces(void)
{
    for (int e = 1; e <= ERA_COUNT; e++) {
        const wchar_t *pick = L"Georgia";
        for (int i = 0; i < 4 && ERA_FACES[e][i]; i++)
            if (face_exists(ERA_FACES[e][i])) { pick = ERA_FACES[e][i]; break; }
        wcsncpy(s_eraFace[e], pick, LF_FACESIZE - 1);
    }
}

static HFONT make_font(const wchar_t *face, int px, int weight, int italic)
{
    return CreateFontW(-SC(px), 0, 0, 0, weight, italic, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

HFONT ui_font(int f)
{
    if (s_fonts[f]) return s_fonts[f];
    const wchar_t *eraFace = s_eraFace[s_era][0] ? s_eraFace[s_era] : L"Georgia";
    int eraW = wcsncmp(eraFace, L"Bahnschrift SemiBold", 20) == 0 ? FW_NORMAL : FW_BOLD;
    HFONT h = 0;
    switch (f) {
    case F_BODY:    h = make_font(L"Segoe UI", 15, FW_NORMAL, 0); break;
    case F_BODY_B:  h = make_font(L"Segoe UI", 15, FW_SEMIBOLD, 0); break;
    case F_SMALL:   h = make_font(L"Segoe UI", 13, FW_NORMAL, 0); break;
    case F_SMALL_B: h = make_font(L"Segoe UI", 13, FW_SEMIBOLD, 0); break;
    case F_TINY:    h = make_font(L"Segoe UI", 12, FW_NORMAL, 0); break;
    case F_H3:      h = make_font(L"Segoe UI", 16, FW_SEMIBOLD, 0); break;
    case F_H2:      h = make_font(L"Segoe UI", 20, FW_SEMIBOLD, 0); break;
    case F_H1:      h = make_font(L"Segoe UI", 26, FW_BOLD, 0); break;
    case F_MONO:    h = make_font(L"Consolas", 13, FW_NORMAL, 0); break;
    case F_MONO_B:  h = make_font(L"Consolas", 14, FW_BOLD, 0); break;
    case F_ERA_XL:  h = make_font(eraFace, 44, eraW, 0); break;
    case F_ERA_L:   h = make_font(eraFace, 30, eraW, 0); break;
    case F_ERA_M:   h = make_font(eraFace, 23, eraW, 0); break;
    case F_ERA_S:   h = make_font(eraFace, 17, eraW, 0); break;
    case F_HUGE:    h = make_font(eraFace, 56, eraW, 0); break;
    }
    s_fonts[f] = h;
    return h;
}

void ui_fonts_reset(void)
{
    for (int i = 0; i < F_COUNT; i++)
        if (s_fonts[i]) { DeleteObject(s_fonts[i]); s_fonts[i] = 0; }
}

void ui_set_era_font(int era)
{
    if (era < 1 || era > ERA_COUNT || era == s_era) return;
    s_era = era;
    int ef[] = { F_ERA_XL, F_ERA_L, F_ERA_M, F_ERA_S, F_HUGE };
    for (int i = 0; i < 5; i++)
        if (s_fonts[ef[i]]) { DeleteObject(s_fonts[ef[i]]); s_fonts[ef[i]] = 0; }
}

/* ---------- буфер ---------- */
HDC d_dc(void) { return s_dc; }
unsigned *d_bits(void) { return s_bits; }

static void ensure_buffer(HDC wdc)
{
    if (s_dc && s_bw == g_w && s_bh == g_h) return;
    if (s_dc) { SelectObject(s_dc, s_oldBmp); DeleteObject(s_bmp); DeleteDC(s_dc); }
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = g_w > 0 ? g_w : 1;
    bi.bmiHeader.biHeight = -(g_h > 0 ? g_h : 1);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    s_dc = CreateCompatibleDC(wdc);
    s_bmp = CreateDIBSection(wdc, &bi, DIB_RGB_COLORS, (void **)&s_bits, NULL, 0);
    s_oldBmp = SelectObject(s_dc, s_bmp);
    s_bw = g_w; s_bh = g_h;
    SetBkMode(s_dc, TRANSPARENT);
}

/* ---------- примитивы ---------- */
COLORREF mix(COLORREF a, COLORREF b, int t)
{
    int r = (GetRValue(a) * (255 - t) + GetRValue(b) * t) / 255;
    int g = (GetGValue(a) * (255 - t) + GetGValue(b) * t) / 255;
    int bl = (GetBValue(a) * (255 - t) + GetBValue(b) * t) / 255;
    return RGB(r, g, bl);
}

void d_fill(int x, int y, int w, int h, COLORREF c)
{
    RECT r = { x, y, x + w, y + h };
    SetDCBrushColor(s_dc, c);
    FillRect(s_dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
}

void d_frame(int x, int y, int w, int h, COLORREF c)
{
    RECT r = { x, y, x + w, y + h };
    SetDCBrushColor(s_dc, c);
    FrameRect(s_dc, &r, (HBRUSH)GetStockObject(DC_BRUSH));
}

/* ---------- сглаженные фигуры: рисуем прямо в пиксели буфера (расстояние до края → покрытие) ---------- */
/* окно, куда можно рисовать: пересечение буфера и отсечения */
static int px_box(int *x0, int *y0, int *x1, int *y1)
{
    int cx = 0, cy = 0, cw = s_bw, ch = s_bh;
    if (!s_bits) return 0;
    if (d_clip_get(&cx, &cy, &cw, &ch)) {
        if (*x0 < cx) *x0 = cx;
        if (*y0 < cy) *y0 = cy;
        if (*x1 > cx + cw) *x1 = cx + cw;
        if (*y1 > cy + ch) *y1 = cy + ch;
    }
    if (*x0 < 0) *x0 = 0;
    if (*y0 < 0) *y0 = 0;
    if (*x1 > s_bw) *x1 = s_bw;
    if (*y1 > s_bh) *y1 = s_bh;
    return *x1 > *x0 && *y1 > *y0;
}

static inline void px_blend(unsigned *p, unsigned cr, unsigned cg, unsigned cb, unsigned a)
{
    if (a >= 255) { *p = cr << 16 | cg << 8 | cb; return; }
    unsigned v = *p, ia = 255 - a;
    *p = ((((v >> 16) & 255) * ia + cr * a) / 255) << 16 | ((((v >> 8) & 255) * ia + cg * a) / 255) << 8 | (((v & 255) * ia + cb * a) / 255);
}

/* расстояние со знаком от точки до края скруглённого прямоугольника (полуширины hx, hy, радиус r) */
static inline float sd_rbox(float px, float py, float hx, float hy, float r)
{
    float qx = fabsf(px) - (hx - r), qy = fabsf(py) - (hy - r);
    if (qx <= 0 || qy <= 0) return (qx > qy ? qx : qy) - r;
    return sqrtf(qx * qx + qy * qy) - r;
}

static inline float cover(float d) { return d <= -0.5f ? 1.0f : d >= 0.5f ? 0.0f : 0.5f - d; }

/* заливка (с вертикальным градиентом fill → fill2), рамка толщиной bt, общая прозрачность alpha 0..255 */
static void rrect_px(int x, int y, int w, int h, int r, COLORREF fill, COLORREF fill2, COLORREF border, int bt, int alpha)
{
    if (w <= 0 || h <= 0) return;
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (!px_box(&x0, &y0, &x1, &y1)) return;
    GdiFlush();
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hx = w * 0.5f, hy = h * 0.5f;
    int hasFill = fill != CLR_INVALID, hasBorder = border != CLR_INVALID && bt > 0;
    unsigned br = GetRValue(border), bgc = GetGValue(border), bb = GetBValue(border);
    for (int yy = y0; yy < y1; yy++) {
        unsigned fr = 0, fg = 0, fb = 0;
        if (hasFill) {
            COLORREF c = fill2 == CLR_INVALID ? fill : mix(fill, fill2, h > 1 ? (yy - y) * 255 / (h - 1) : 0);
            fr = GetRValue(c); fg = GetGValue(c); fb = GetBValue(c);
        }
        unsigned *row = s_bits + (size_t)yy * s_bw;
        float py = yy + 0.5f - cy;
        int inner = py > -(hy - r) && py < hy - r;       /* строка без скруглений: середину заполняем быстро */
        for (int xx = x0; xx < x1; xx++) {
            float px = xx + 0.5f - cx;
            float d;
            if (inner) {
                float ex = fabsf(px) - hx, ey = fabsf(py) - hy;
                d = ex > ey ? ex : ey;
            } else d = sd_rbox(px, py, hx, hy, (float)r);
            float cv = cover(d);
            if (cv <= 0) continue;
            if (hasFill) px_blend(row + xx, fr, fg, fb, (unsigned)(cv * alpha + 0.5f));
            if (hasBorder) {
                float ring = cv - cover(d + bt);
                if (ring > 0) px_blend(row + xx, br, bgc, bb, (unsigned)(ring * alpha + 0.5f));
            }
        }
    }
}

static int border_px(void) { return g_s >= 1.75f ? 2 : 1; }

void d_rrect(int x, int y, int w, int h, int r, COLORREF fill, COLORREF border)
{
    rrect_px(x, y, w, h, r, fill, CLR_INVALID, border, border_px(), 255);
}

void d_rrect_grad(int x, int y, int w, int h, int r, COLORREF top, COLORREF bottom, COLORREF border)
{
    rrect_px(x, y, w, h, r, top, bottom, border, border_px(), 255);
}

void d_rrect_a(int x, int y, int w, int h, int r, COLORREF fill, int alpha)
{
    rrect_px(x, y, w, h, r, fill, CLR_INVALID, CLR_INVALID, 0, alpha);
}

/* мягкая тень под карточкой: размытый край, чуть ниже самой карточки */
void d_shadow(int x, int y, int w, int h, int r, int spread, int alpha)
{
    if (spread < 1) spread = 1;
    int oy = spread / 3;
    int x0 = x - spread, y0 = y - spread + oy, x1 = x + w + spread, y1 = y + h + spread + oy;
    if (!px_box(&x0, &y0, &x1, &y1)) return;
    GdiFlush();
    unsigned sr = GetRValue(P.shadow), sg = GetGValue(P.shadow), sb = GetBValue(P.shadow);
    float cx = x + w * 0.5f, cy = y + h * 0.5f + oy, hx = w * 0.5f, hy = h * 0.5f;
    for (int yy = y0; yy < y1; yy++) {
        unsigned *row = s_bits + (size_t)yy * s_bw;
        for (int xx = x0; xx < x1; xx++) {
            float d = sd_rbox(xx + 0.5f - cx, yy + 0.5f - cy, hx, hy, (float)r);
            if (d < -spread) continue;                       /* глубоко под карточкой — её всё равно закроет */
            float t = 1.0f - (d + spread * 0.25f) / (spread * 1.25f);
            if (t <= 0) continue;
            if (t > 1) t = 1;
            px_blend(row + xx, sr, sg, sb, (unsigned)(t * t * alpha));
        }
    }
}

static unsigned *s_back;                       /* готовый фон окна (d_backdrop) */
static int s_backW, s_backH, s_backDark = -1;

/* скруглить углы уже нарисованной картинки: за краем — снова фон окна (d_backdrop) */
void d_round_mask(int x, int y, int w, int h, int r)
{
    if (!s_back || s_backW != s_bw || s_backH != s_bh) return;
    GdiFlush();
    float cx = x + w * 0.5f, cy = y + h * 0.5f, hx = w * 0.5f, hy = h * 0.5f;
    for (int k = 0; k < 4; k++) {
        int x0 = k & 1 ? x + w - r - 1 : x, y0 = k & 2 ? y + h - r - 1 : y, x1 = x0 + r + 1, y1 = y0 + r + 1;
        if (!px_box(&x0, &y0, &x1, &y1)) continue;
        for (int yy = y0; yy < y1; yy++)
            for (int xx = x0; xx < x1; xx++) {
                float out = 1.0f - cover(sd_rbox(xx + 0.5f - cx, yy + 0.5f - cy, hx, hy, (float)r));
                if (out <= 0) continue;
                unsigned b = s_back[(size_t)yy * s_bw + xx];
                px_blend(s_bits + (size_t)yy * s_bw + xx, (b >> 16) & 255, (b >> 8) & 255, b & 255, (unsigned)(out * 255 + 0.5f));
            }
    }
}

/* вертикальный градиент прямоугольника */
void d_grad(int x, int y, int w, int h, COLORREF top, COLORREF bottom)
{
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (!px_box(&x0, &y0, &x1, &y1)) return;
    GdiFlush();
    for (int yy = y0; yy < y1; yy++) {
        COLORREF c = mix(top, bottom, h > 1 ? (yy - y) * 255 / (h - 1) : 0);
        unsigned v = (unsigned)GetRValue(c) << 16 | (unsigned)GetGValue(c) << 8 | GetBValue(c);
        unsigned *row = s_bits + (size_t)yy * s_bw;
        for (int xx = x0; xx < x1; xx++) row[xx] = v;
    }
}

/* фон окна: мягкий градиент со светлым пятном сверху и тёмной виньеткой по краям; считается один раз */

void d_backdrop(void)
{
    if (!s_bits) return;
    if (!s_back || s_backW != s_bw || s_backH != s_bh || s_backDark != g_dark) {
        free(s_back);
        s_back = malloc(sizeof(unsigned) * (size_t)s_bw * s_bh);
        if (!s_back) { d_fill(0, 0, g_w, g_h, P.bg); return; }
        s_backW = s_bw; s_backH = s_bh; s_backDark = g_dark;
        COLORREF top = g_dark ? mix(P.bg, P.blue, 22) : mix(P.bg, RGB(255, 252, 244), 120);
        COLORREF bot = g_dark ? mix(P.bg, RGB(0, 0, 0), 60) : mix(P.bg, P.gold, 18);
        float gx = s_bw * 0.32f, gy = -s_bh * 0.15f, gr = (float)(s_bw > s_bh ? s_bw : s_bh) * 0.9f;
        for (int yy = 0; yy < s_bh; yy++) {
            COLORREF c = mix(top, bot, s_bh > 1 ? yy * 255 / (s_bh - 1) : 0);
            float r0 = GetRValue(c), g0 = GetGValue(c), b0 = GetBValue(c);
            for (int xx = 0; xx < s_bw; xx++) {
                float dx = (xx - gx) / gr, dy = (yy - gy) / gr, d2 = dx * dx + dy * dy;
                float glow = d2 < 1 ? (1 - d2) * (g_dark ? 0.10f : 0.06f) : 0;       /* светлое пятно сверху */
                float vx = (xx - s_bw * 0.5f) / (s_bw * 0.5f), vy = (yy - s_bh * 0.5f) / (s_bh * 0.5f);
                float vig = 1.0f - (vx * vx + vy * vy) * (g_dark ? 0.16f : 0.07f);  /* виньетка */
                float r = (r0 + (255 - r0) * glow) * vig, g = (g0 + (255 - g0) * glow) * vig, b = (b0 + (255 - b0) * glow) * vig;
                s_back[(size_t)yy * s_bw + xx] = (unsigned)r << 16 | (unsigned)g << 8 | (unsigned)b;
            }
        }
    }
    GdiFlush();
    memcpy(s_bits, s_back, sizeof(unsigned) * (size_t)s_bw * s_bh);
}

void d_line(int x1, int y1, int x2, int y2, COLORREF c, int t)
{
    HPEN pen = t > 1 ? CreatePen(PS_SOLID, t, c) : 0;
    HGDIOBJ op = SelectObject(s_dc, pen ? (HGDIOBJ)pen : GetStockObject(DC_PEN));
    if (!pen) SetDCPenColor(s_dc, c);
    MoveToEx(s_dc, x1, y1, NULL);
    LineTo(s_dc, x2, y2);
    SelectObject(s_dc, op);
    if (pen) DeleteObject(pen);
}

void d_poly(const POINT *pts, int n, COLORREF fill, COLORREF border, int t)
{
    HPEN pen = border != CLR_INVALID && t > 1 ? CreatePen(PS_SOLID, t, border) : 0;
    HGDIOBJ op = SelectObject(s_dc, pen ? (HGDIOBJ)pen : border == CLR_INVALID ? GetStockObject(NULL_PEN) : GetStockObject(DC_PEN));
    HGDIOBJ ob = SelectObject(s_dc, fill == CLR_INVALID ? GetStockObject(NULL_BRUSH) : GetStockObject(DC_BRUSH));
    if (border != CLR_INVALID && !pen) SetDCPenColor(s_dc, border);
    if (fill != CLR_INVALID) SetDCBrushColor(s_dc, fill);
    if (fill == CLR_INVALID) Polyline(s_dc, pts, n);
    else Polygon(s_dc, pts, n);
    SelectObject(s_dc, op);
    SelectObject(s_dc, ob);
    if (pen) DeleteObject(pen);
}

/* круг со сглаженным краем; рамка толщиной t — по окружности (как перо GDI) */
void d_circle(int cx, int cy, int r, COLORREF fill, COLORREF border, int t)
{
    if (r < 0) return;
    float ht = border != CLR_INVALID ? (t < 1 ? 1 : t) * 0.5f : 0;
    int e = (int)(r + ht + 2);
    int x0 = cx - e, y0 = cy - e, x1 = cx + e + 1, y1 = cy + e + 1;
    if (!px_box(&x0, &y0, &x1, &y1)) return;
    GdiFlush();
    float fcx = cx + 0.5f, fcy = cy + 0.5f;
    unsigned fr = GetRValue(fill), fg = GetGValue(fill), fb = GetBValue(fill);
    unsigned br = GetRValue(border), bgc = GetGValue(border), bb = GetBValue(border);
    for (int yy = y0; yy < y1; yy++) {
        unsigned *row = s_bits + (size_t)yy * s_bw;
        float dy = yy + 0.5f - fcy;
        for (int xx = x0; xx < x1; xx++) {
            float dx = xx + 0.5f - fcx, d = sqrtf(dx * dx + dy * dy) - r;
            if (fill != CLR_INVALID) { float c = cover(d); if (c > 0) px_blend(row + xx, fr, fg, fb, (unsigned)(c * 255 + 0.5f)); }
            if (border != CLR_INVALID) {
                float c = cover(d - ht) - cover(d + ht);
                if (c > 0) px_blend(row + xx, br, bgc, bb, (unsigned)(c * 255 + 0.5f));
            }
        }
    }
}

void d_text(int x, int y, const wchar_t *s, int f, COLORREF c)
{
    SelectObject(s_dc, ui_font(f));
    SetTextColor(s_dc, c);
    TextOutW(s_dc, x, y, s, (int)wcslen(s));
}

int d_text_w(const wchar_t *s, int f)
{
    SIZE sz;
    SelectObject(s_dc, ui_font(f));
    GetTextExtentPoint32W(s_dc, s, (int)wcslen(s), &sz);
    return sz.cx;
}

int d_font_h(int f)
{
    TEXTMETRICW tm;
    SelectObject(s_dc, ui_font(f));
    GetTextMetricsW(s_dc, &tm);
    return tm.tmHeight;
}

int d_wrap(int x, int y, int w, const wchar_t *s, int f, COLORREF c, int draw)
{
    RECT r = { x, y, x + (w > 1 ? w : 1), y + 10000 };
    SelectObject(s_dc, ui_font(f));
    DrawTextW(s_dc, s, -1, &r, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
    int h = r.bottom - r.top;
    if (draw) {
        r.right = x + (w > 1 ? w : 1);
        SetTextColor(s_dc, c);
        DrawTextW(s_dc, s, -1, &r, DT_WORDBREAK | DT_NOPREFIX);
    }
    return h;
}

void d_text_c(int x, int y, int w, const wchar_t *s, int f, COLORREF c)
{
    RECT r = { x, y, x + w, y + 2000 };
    SelectObject(s_dc, ui_font(f));
    SetTextColor(s_dc, c);
    DrawTextW(s_dc, s, -1, &r, DT_CENTER | DT_SINGLELINE | DT_NOPREFIX | DT_TOP);
}

void d_text_r(int xr, int y, const wchar_t *s, int f, COLORREF c)
{
    d_text(xr - d_text_w(s, f), y, s, f, c);
}

void d_ellipsis(int x, int y, int w, const wchar_t *s, int f, COLORREF c)
{
    RECT r = { x, y, x + w, y + 2000 };
    SelectObject(s_dc, ui_font(f));
    SetTextColor(s_dc, c);
    DrawTextW(s_dc, s, -1, &r, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void d_blend(int x, int y, int w, int h, COLORREF c, int a)
{
    GdiFlush();
    int cx, cy, cw, ch;
    if (d_clip_get(&cx, &cy, &cw, &ch)) {          /* как и GDI, не выходим за отсечение */
        if (x < cx) { w -= cx - x; x = cx; }
        if (y < cy) { h -= cy - y; y = cy; }
        if (x + w > cx + cw) w = cx + cw - x;
        if (y + h > cy + ch) h = cy + ch - y;
        if (w <= 0 || h <= 0) return;
    }
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > s_bw ? s_bw : x + w, y1 = y + h > s_bh ? s_bh : y + h;
    unsigned cr = GetRValue(c), cg = GetGValue(c), cb = GetBValue(c);
    unsigned ia = 255 - (unsigned)a;
    for (int yy = y0; yy < y1; yy++) {
        unsigned *p = s_bits + (size_t)yy * s_bw + x0;
        for (int xx = x0; xx < x1; xx++, p++) {
            unsigned v = *p;
            unsigned r = ((v >> 16) & 255) * ia + cr * (unsigned)a;
            unsigned g = ((v >> 8) & 255) * ia + cg * (unsigned)a;
            unsigned b = (v & 255) * ia + cb * (unsigned)a;
            *p = ((r / 255) << 16) | ((g / 255) << 8) | (b / 255);
        }
    }
}

static int s_clipOn, s_clipX, s_clipY, s_clipW, s_clipH;

int d_clip_get(int *x, int *y, int *w, int *h)
{
    *x = s_clipX; *y = s_clipY; *w = s_clipW; *h = s_clipH;
    return s_clipOn;
}

void d_clip(int x, int y, int w, int h)
{
    s_clipOn = 1; s_clipX = x; s_clipY = y; s_clipW = w; s_clipH = h;
    HRGN rg = CreateRectRgn(x, y, x + w, y + h);
    SelectClipRgn(s_dc, rg);
    DeleteObject(rg);
}

void d_unclip(void) { s_clipOn = 0; SelectClipRgn(s_dc, NULL); }

void d_bitmap(HDC src, int x, int y, int w, int h) { BitBlt(s_dc, x, y, w, h, src, 0, 0, SRCCOPY); }

/* Сектор круга по часовой стрелке от «12 часов»: индикатор минутного дохода. */
void d_pie(int cx, int cy, int r, double frac, COLORREF fill)
{
    if (frac < 0.01) return;   /* иначе концы совпадут и Pie нарисует полный круг */
    if (frac >= 0.999) { d_circle(cx, cy, r, fill, CLR_INVALID, 1); return; }
    const double PI2 = 6.283185307179586;
    double a0 = -PI2 / 4, a1 = a0 + frac * PI2;
    int xs = cx + (int)lround(r * 4 * cos(a0)), ys = cy + (int)lround(r * 4 * sin(a0));
    int xe = cx + (int)lround(r * 4 * cos(a1)), ye = cy + (int)lround(r * 4 * sin(a1));
    HGDIOBJ ob = SelectObject(s_dc, GetStockObject(DC_BRUSH));
    HGDIOBJ op = SelectObject(s_dc, GetStockObject(NULL_PEN));
    SetDCBrushColor(s_dc, fill);
    int old = SetArcDirection(s_dc, AD_CLOCKWISE);
    Pie(s_dc, cx - r, cy - r, cx + r + 1, cy + r + 1, xs, ys, xe, ye);
    SetArcDirection(s_dc, old);
    SelectObject(s_dc, ob);
    SelectObject(s_dc, op);
}

/* Вышивка: ромбы — знак земли и плодородия в белорусском орнаменте. */
void d_ornament(int x, int y, int w, int cell, COLORREF a, COLORREF b)
{
    int cols = w / cell;
    int st = cell > 2 ? cell - 1 : cell;
    for (int col = 0; col < cols; col++) {
        int c16 = col % 16;
        for (int r = 0; r < 9; r++) {
            int dr = r - 4, dm = abs(dr) + abs(c16 - 4);
            COLORREF k = CLR_INVALID;
            if (dm == 3 || dm == 0) k = a;
            else if (dm == 1 && (c16 == 4)) k = b;
            else if ((dr == 0 && abs(c16 - 12) <= 2) || (c16 == 12 && abs(dr) <= 2)) k = b;
            else if ((r == 0 || r == 8) && (col % 4 == 1)) k = a;
            if (k != CLR_INVALID) d_fill(x + col * cell, y + r * cell, st, st, k);
        }
    }
}

/* ---------- значки ---------- */
void d_icon(int kind, int x, int y, int s, COLORREF c)
{
    int t = s >= 18 ? 2 : 1;
    int cx = x + s / 2, cy = y + s / 2, r = s / 2 - 1;
    POINT p[12];
    switch (kind) {
    case IC_GOLD:
        d_circle(cx, cy, r, CLR_INVALID, c, t);
        d_circle(cx, cy, r / 2, CLR_INVALID, c, 1);
        break;
    case IC_SCI:   /* раскрытая книга */
        p[0].x = x + 1; p[0].y = y + s / 5; p[1].x = cx; p[1].y = y + s / 3; p[2].x = x + s - 2; p[2].y = y + s / 5;
        p[3].x = x + s - 2; p[3].y = y + s - 3; p[4].x = cx; p[4].y = y + s - 1; p[5].x = x + 1; p[5].y = y + s - 3; p[6] = p[0];
        d_poly(p, 7, CLR_INVALID, c, t);
        d_line(cx, y + s / 3, cx, y + s - 1, c, 1);
        break;
    case IC_IND:   /* шестерня */
        for (int i = 0; i < 8; i++) {
            double a = i * 3.14159265 / 4;
            d_line(cx + (int)(cos(a) * r * 0.55), cy + (int)(sin(a) * r * 0.55), cx + (int)(cos(a) * r), cy + (int)(sin(a) * r), c, t + 1);
        }
        d_circle(cx, cy, r * 6 / 10, CLR_INVALID, c, t);
        break;
    case IC_POP: case IC_PERSON:
        d_circle(cx, y + s / 3, s / 5, CLR_INVALID, c, t);
        p[0].x = x + s / 6; p[0].y = y + s - 1; p[1].x = x + s / 4; p[1].y = y + s * 2 / 3; p[2].x = cx; p[2].y = y + s * 3 / 5;
        p[3].x = x + s * 3 / 4; p[3].y = y + s * 2 / 3; p[4].x = x + s * 5 / 6; p[4].y = y + s - 1;
        d_poly(p, 5, CLR_INVALID, c, t);
        break;
    case IC_STAB:  /* колонна */
        d_line(x + 2, y + 3, x + s - 2, y + 3, c, t);
        d_line(x + 2, y + s - 2, x + s - 2, y + s - 2, c, t);
        d_line(x + s / 4, y + 4, x + s / 4, y + s - 3, c, t);
        d_line(cx, y + 4, cx, y + s - 3, c, t);
        d_line(x + s * 3 / 4, y + 4, x + s * 3 / 4, y + s - 3, c, t);
        break;
    case IC_CULT:  /* ромб с крестом */
        p[0].x = cx; p[0].y = y + 1; p[1].x = x + s - 1; p[1].y = cy; p[2].x = cx; p[2].y = y + s - 1; p[3].x = x + 1; p[3].y = cy; p[4] = p[0];
        d_poly(p, 5, CLR_INVALID, c, t);
        d_line(cx - s / 6, cy, cx + s / 6 + 1, cy, c, t);
        d_line(cx, cy - s / 6, cx, cy + s / 6 + 1, c, t);
        break;
    case IC_ECO:   /* лист */
        p[0].x = x + 2; p[0].y = y + s - 2; p[1].x = x + 2; p[1].y = cy; p[2].x = x + s / 4; p[2].y = y + s / 4;
        p[3].x = x + s - 2; p[3].y = y + 2; p[4].x = x + s - 3; p[4].y = cy; p[5].x = x + s * 2 / 3; p[5].y = y + s * 3 / 4; p[6] = p[0];
        d_poly(p, 7, CLR_INVALID, c, t);
        d_line(x + 2, y + s - 2, x + s * 2 / 3, y + s / 3, c, 1);
        break;
    case IC_DEF:   /* щит */
        p[0].x = x + 2; p[0].y = y + 2; p[1].x = x + s - 2; p[1].y = y + 2; p[2].x = x + s - 2; p[2].y = cy;
        p[3].x = cx; p[3].y = y + s - 1; p[4].x = x + 2; p[4].y = cy; p[5] = p[0];
        d_poly(p, 6, CLR_INVALID, c, t);
        break;
    case IC_POLIT: /* свиток */
        d_frame(x + 3, y + 2, s - 6, s - 4, c);
        d_line(x + 5, y + s / 3, x + s - 5, y + s / 3, c, 1);
        d_line(x + 5, cy, x + s - 5, cy, c, 1);
        d_line(x + 5, y + s * 2 / 3, x + s - 7, y + s * 2 / 3, c, 1);
        break;
    case IC_WAR:   /* скрещённые мечи */
        d_line(x + 2, y + 2, x + s - 2, y + s - 2, c, t);
        d_line(x + s - 2, y + 2, x + 2, y + s - 2, c, t);
        d_line(x + 2, y + s - 6, x + 6, y + s - 2, c, t);
        d_line(x + s - 2, y + s - 6, x + s - 6, y + s - 2, c, t);
        break;
    case IC_DISASTER: /* пламя */
        p[0].x = cx; p[0].y = y + 1; p[1].x = x + s - 3; p[1].y = y + s * 3 / 5; p[2].x = x + s * 2 / 3; p[2].y = y + s - 1;
        p[3].x = x + s / 3; p[3].y = y + s - 1; p[4].x = x + 3; p[4].y = y + s * 3 / 5; p[5].x = x + s / 3; p[5].y = y + s / 3; p[6] = p[0];
        d_poly(p, 7, CLR_INVALID, c, t);
        break;
    case IC_SPACE: /* ракета */
        p[0].x = cx; p[0].y = y + 1; p[1].x = cx + s / 5; p[1].y = y + s / 3; p[2].x = cx + s / 5; p[2].y = y + s * 3 / 4;
        p[3].x = cx - s / 5; p[3].y = y + s * 3 / 4; p[4].x = cx - s / 5; p[4].y = y + s / 3; p[5] = p[0];
        d_poly(p, 6, CLR_INVALID, c, t);
        d_line(cx - s / 5, y + s * 3 / 5, x + 2, y + s - 2, c, t);
        d_line(cx + s / 5, y + s * 3 / 5, x + s - 2, y + s - 2, c, t);
        break;
    case IC_BUILD: /* башня */
        p[0].x = x + 3; p[0].y = y + s - 1; p[1].x = x + 3; p[1].y = y + 3; p[2].x = x + s / 3; p[2].y = y + 3; p[3].x = x + s / 3; p[3].y = y + 6;
        p[4].x = x + s * 2 / 3; p[4].y = y + 6; p[5].x = x + s * 2 / 3; p[5].y = y + 3; p[6].x = x + s - 3; p[6].y = y + 3; p[7].x = x + s - 3; p[7].y = y + s - 1;
        d_poly(p, 8, CLR_INVALID, c, t);
        break;
    case IC_TECH:  /* искра-идея */
        d_circle(cx, cy - 1, s / 3, CLR_INVALID, c, t);
        d_line(cx - s / 6, y + s - 2, cx + s / 6 + 1, y + s - 2, c, t);
        break;
    case IC_QUIZ:
        d_circle(cx, cy, r, CLR_INVALID, c, t);
        d_text_c(x, y + s / 10, s, L"?", F_SMALL_B, c);
        break;
    case IC_LOCK:
        d_frame(x + 3, cy - 1, s - 6, s / 2, c);
        d_circle(cx, cy - 2, s / 5, CLR_INVALID, c, t);
        break;
    case IC_CHECK:
        d_line(x + 3, cy, x + s / 2 - 1, y + s - 4, c, t + 1);
        d_line(x + s / 2 - 1, y + s - 4, x + s - 3, y + 4, c, t + 1);
        break;
    case IC_FLAG:  /* флажок задания */
        d_line(x + s / 4, y + 1, x + s / 4, y + s - 1, c, t);
        p[0].x = x + s / 4; p[0].y = y + 2; p[1].x = x + s - 2; p[1].y = y + s / 4 + 1;
        p[2].x = x + s / 4; p[2].y = y + s / 2 + 1; p[3] = p[0];
        d_poly(p, 4, c, CLR_INVALID, 1);
        break;
    case IC_CROWN: /* корона правителя */
        p[0].x = x + 2; p[0].y = y + s - 3; p[1].x = x + 2; p[1].y = y + s / 3; p[2].x = x + s / 3; p[2].y = cy;
        p[3].x = cx; p[3].y = y + 3; p[4].x = x + s * 2 / 3; p[4].y = cy; p[5].x = x + s - 2; p[5].y = y + s / 3;
        p[6].x = x + s - 2; p[6].y = y + s - 3; p[7] = p[0];
        d_poly(p, 8, CLR_INVALID, c, t);
        break;
    }
}

/* ---------- области нажатия ---------- */
typedef struct { RECT r; int act, arg; } Hit;
static Hit s_hits[900];
static int s_nhits;
static RECT s_hclip;
static int s_hclipOn;
static int s_hovAct = -1, s_hovArg = -1;

void hits_reset(void) { s_nhits = 0; s_hclipOn = 0; }
void hits_clip(int x, int y, int w, int h) { s_hclip.left = x; s_hclip.top = y; s_hclip.right = x + w; s_hclip.bottom = y + h; s_hclipOn = 1; }
void hits_unclip(void) { s_hclipOn = 0; }

void hit(int x, int y, int w, int h, int act, int arg)
{
    if (s_nhits >= (int)(sizeof s_hits / sizeof s_hits[0])) return;
    RECT r = { x, y, x + w, y + h };
    if (s_hclipOn) {
        RECT o;
        if (!IntersectRect(&o, &r, &s_hclip)) return;
        r = o;
    }
    s_hits[s_nhits].r = r;
    s_hits[s_nhits].act = act;
    s_hits[s_nhits].arg = arg;
    s_nhits++;
}

int hover(int act, int arg) { return s_hovAct == act && s_hovArg == arg; }

int hit_at(int x, int y, int *act, int *arg)
{
    POINT pt = { x, y };
    for (int i = s_nhits - 1; i >= 0; i--)
        if (PtInRect(&s_hits[i].r, pt)) { *act = s_hits[i].act; *arg = s_hits[i].arg; return 1; }
    *act = -1; *arg = -1;
    return 0;
}

/* ---------- виджеты ---------- */
void w_button(int x, int y, int w, int h, const wchar_t *label, int kind, int enabled, int act, int arg)
{
    int hv = enabled && hover(act, arg), r = h / 2 < SC(10) ? h / 2 : SC(10);
    COLORREF fg;
    if (!enabled) {
        d_rrect(x, y, w, h, r, mix(P.surface2, P.surface, 80), P.line);
        fg = mix(P.muted, P.surface2, 70);
    } else if (kind == B_PRIMARY || kind == B_DANGER) {
        /* кармин с объёмом: светлее сверху, тень и блик по верхнему краю */
        COLORREF base = kind == B_DANGER ? mix(P.red, RGB(120, 0, 20), 40) : P.red;
        COLORREF top = mix(base, RGB(255, 255, 255), hv ? 70 : 40), bot = mix(base, RGB(0, 0, 0), hv ? 10 : 40);
        d_shadow(x, y, w, h, r, SC(hv ? 10 : 7), g_dark ? 120 : 70);
        d_rrect_grad(x, y, w, h, r, top, bot, mix(base, RGB(0, 0, 0), 70));
        d_fill(x + r, y + SC(1), w - 2 * r, 1, mix(top, RGB(255, 255, 255), 90));
        fg = P.onAccent;
    } else if (kind == B_SECONDARY) {
        COLORREF top = mix(P.surface2, P.ink, hv ? 26 : 10), bot = mix(P.surface2, P.bg, hv ? 20 : 60);
        d_shadow(x, y, w, h, r, SC(5), g_dark ? 90 : 40);
        d_rrect_grad(x, y, w, h, r, top, bot, hv ? mix(P.line, P.ink, 70) : P.line);
        fg = P.ink;
    } else {
        if (hv) d_rrect(x, y, w, h, r, P.blueSoft, CLR_INVALID);
        fg = P.blue;
    }
    int th = d_font_h(F_BODY_B);
    d_text_c(x, y + (h - th) / 2, w, label, F_BODY_B, fg);
    if (enabled) hit(x, y, w, h, act, arg);
}

int w_pill(int x, int y, const wchar_t *s, COLORREF fg, COLORREF bg)
{
    int w = d_text_w(s, F_TINY) + SC(12), h = d_font_h(F_TINY) + SC(4);
    d_rrect(x, y, w, h, h / 2, bg, CLR_INVALID);
    d_text(x + SC(6), y + SC(2), s, F_TINY, fg);
    return w;
}

void w_scrollbar(int x, int y, int h, int content, int view, int scroll)
{
    if (content <= view || content <= 0) return;
    int th = h * view / content;
    if (th < SC(30)) th = SC(30);
    int ty = y + (int)((long long)(h - th) * scroll / (content - view));
    d_rrect(x + SC(1), y, SC(4), h, SC(2), mix(P.surface, P.line, 140), CLR_INVALID);
    d_rrect(x, ty, SC(6), th, SC(3), mix(P.muted, P.surface, 40), CLR_INVALID);
}

/* ---------- сервис ---------- */
void ui_invalidate(void) { if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE); }

void ui_toast(const wchar_t *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vswprintf(U.toast, sizeof U.toast / sizeof U.toast[0], fmt, ap);
    va_end(ap);
    U.toastUntil = GetTickCount() + 2800 + 35 * (DWORD)wcslen(U.toast);   /* длинное сообщение — дольше */
    SetTimer(g_hwnd, 1, 250, NULL);
    ui_invalidate();
}

static void data_dir(wchar_t *out, int n)
{
    wchar_t base[MAX_PATH];
    DWORD len = GetEnvironmentVariableW(L"APPDATA", base, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) {
        GetModuleFileNameW(NULL, base, MAX_PATH);
        wchar_t *sl = wcsrchr(base, L'\\');
        if (sl) *sl = 0;
        swprintf(out, n, L"%ls", base);
        return;
    }
#ifdef ADMIN_BUILD
    swprintf(out, n, L"%ls\\BelarusTechFate-Admin", base);   /* у админ-версии свои сохранения */
#else
    swprintf(out, n, L"%ls\\BelarusTechFate", base);
#endif
    CreateDirectoryW(out, NULL);
}

static void data_file(const wchar_t *name, wchar_t *out, int n)
{
    wchar_t dir[MAX_PATH];
    data_dir(dir, MAX_PATH);
    swprintf(out, n, L"%ls\\%ls", dir, name);
}

int ui_save(void)
{
    if (g_dev) return 1;
    wchar_t path[MAX_PATH + 32];
    data_file(L"save.dat", path, MAX_PATH + 32);
    FILE *f = _wfopen(path, L"wb");
    if (!f) return 0;
    int ok = game_save(&G, f);
    fclose(f);
    if (ok) U.hasSave = 1;
    return ok;
}

static int load_into(Game *g)
{
    wchar_t path[MAX_PATH + 32];
    data_file(L"save.dat", path, MAX_PATH + 32);
    FILE *f = _wfopen(path, L"rb");
    if (!f) return 0;
    int ok = game_load(g, f);
    fclose(f);
    return ok;
}

int ui_load(void)
{
    static Game tmp;
    if (!load_into(&tmp)) return 0;
    G = tmp;
    return 1;
}

void ui_save_settings(void)
{
    if (g_dev) return;
    wchar_t path[MAX_PATH + 32];
    data_file(L"settings.dat", path, MAX_PATH + 32);
    FILE *f = _wfopen(path, L"wb");
    if (!f) return;
    fwprintf(f, L"%d %d\n", g_dark, U.zoom);
    fclose(f);
}

static void load_settings(void)
{
    wchar_t path[MAX_PATH + 32];
    data_file(L"settings.dat", path, MAX_PATH + 32);
    FILE *f = _wfopen(path, L"rb");
    U.zoom = 100;
    if (!f) return;
    int d = 1, z = 100;
    if (fwscanf(f, L"%d %d", &d, &z) == 2) { g_dark = d ? 1 : 0; U.zoom = z >= 70 && z <= 160 ? z : 100; }
    fclose(f);
}

static void apply_scale(void)
{
    g_s = s_dpi * U.zoom / 100.0f;
    P = g_dark ? PAL_DARK : PAL_LIGHT;
    ui_fonts_reset();
    map_invalidate();
}

void ui_zoom(int delta)
{
    int z = U.zoom + delta;
    if (z < 70) z = 70;
    if (z > 160) z = 160;
    U.zoom = z;
    apply_scale();
    ui_save_settings();
    ui_invalidate();
}

void ui_toggle_theme(void)
{
    g_dark = !g_dark;
    apply_scale();
    ui_save_settings();
    ui_invalidate();
}

/* ---------- оконная процедура ---------- */
/* анимации: кто-то нарисовал движущийся кадр — через 33 мс перерисуем снова */
static int s_animWanted;
void ui_anim_request(void) { s_animWanted = 1; }

/* перетаскивание 3D-карты: 1 — вращение (левая кнопка), 2 — сдвиг (правая) */
static int s_drag, s_dragging, s_dragX0, s_dragY0, s_dragLX, s_dragLY;

static void map_fast(HWND h)
{
    U.mapFast = 1;
    SetTimer(h, 3, 220, NULL);      /* после паузы — перерисовать со сглаживанием */
    ui_invalidate();
}

static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_SIZE:
        g_w = LOWORD(lp); g_h = HIWORD(lp);
        ui_invalidate();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC wdc = BeginPaint(h, &ps);
        if (g_w > 0 && g_h > 0) {
            ensure_buffer(wdc);
            hits_reset();
            d_unclip();
            U.mapVOn = 0;
            s_animWanted = 0;
            ui_paint();
            if (s_animWanted && !IsIconic(h)) SetTimer(h, 4, 33, NULL);
            GdiFlush();
            BitBlt(wdc, 0, 0, g_w, g_h, s_dc, 0, 0, SRCCOPY);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONDBLCLK:            /* у окна CS_DBLCLKS: второе нажатие приходит так */
    case WM_RBUTTONDBLCLK: {
        int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
        if (map_cam_hit(x, y) && !s_drag) {
            s_drag = m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK ? 1 : 2;
            s_dragging = 0;
            s_dragX0 = s_dragLX = x;
            s_dragY0 = s_dragLY = y;
            SetCapture(h);
        }
        return 0;
    }
    case WM_RBUTTONUP: {
        int a, g2, was = 0;
        if (s_drag == 2) {
            was = s_dragging;
            s_drag = s_dragging = 0;
            ReleaseCapture();
        } else if (s_drag) return 0;    /* идёт вращение левой кнопкой — правую не трогаем */
        if (!was && hit_at((short)LOWORD(lp), (short)HIWORD(lp), &a, &g2) && a > 0) ui_rclick(a, g2);
        s_hovAct = s_hovArg = -1;
        ui_invalidate();
        return 0;
    }
    case WM_CAPTURECHANGED:
        if ((HWND)lp != h) s_drag = s_dragging = 0;
        return 0;
    case WM_MOUSEMOVE: {
        int a, g2;
        if (s_drag) {
            int x = (short)LOWORD(lp), y = (short)HIWORD(lp);
            if (!s_dragging && (abs(x - s_dragX0) > 4 || abs(y - s_dragY0) > 4)) s_dragging = 1;
            if (s_dragging) {
                if (s_drag == 1) map_cam_rotate((x - s_dragLX) * 0.35, (y - s_dragLY) * 0.25);
                else map_cam_pan(s_dragLX, s_dragLY, x, y);
                s_dragLX = x; s_dragLY = y;
                map_fast(h);
                return 0;
            }
        }
        hit_at((short)LOWORD(lp), (short)HIWORD(lp), &a, &g2);
        if (a != s_hovAct || g2 != s_hovArg) {
            s_hovAct = a; s_hovArg = g2;
            ui_hover_status(a, g2);
            ui_invalidate();
        }
        TRACKMOUSEEVENT tme = { sizeof tme, TME_LEAVE, h, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        s_hovAct = s_hovArg = -1;
        ui_hover_status(-1, -1);
        ui_invalidate();
        return 0;
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursor(NULL, s_hovAct > 0 && s_hovAct < A_HOVER_RES ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_LBUTTONUP: {
        int a, g2;
        if (s_drag == 2) return 0;      /* идёт сдвиг правой кнопкой */
        if (s_drag) {
            int was = s_dragging;
            s_drag = s_dragging = 0;
            ReleaseCapture();
            if (was) { ui_invalidate(); return 0; }      /* это было вращение, а не щелчок */
        }
        if (hit_at((short)LOWORD(lp), (short)HIWORD(lp), &a, &g2) && a > 0 && a < A_HOVER_RES) ui_click(a, g2);
        /* после действия сетка областей меняется: пересчитаем подсветку */
        s_hovAct = s_hovArg = -1;
        ui_invalidate();
        return 0;
    }
    case WM_MOUSEWHEEL: {
        POINT pt = { (short)LOWORD(lp), (short)HIWORD(lp) };
        ScreenToClient(h, &pt);
        if (GetKeyState(VK_CONTROL) < 0) ui_zoom(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 10 : -10);
        else if (map_cam_hit(pt.x, pt.y)) {
            map_cam_zoom_at(pow(1.18, GET_WHEEL_DELTA_WPARAM(wp) / 120.0), pt.x, pt.y);
            map_fast(h);
        }
        else ui_wheel(GET_WHEEL_DELTA_WPARAM(wp), pt.x, pt.y);
        return 0;
    }
    case WM_KEYDOWN:
        if (GetKeyState(VK_CONTROL) < 0) {
            if (wp == 'S') { if (U.screen == SCR_GAME && ui_save()) ui_toast(L"Игра сохранена"); return 0; }
            if (wp == VK_OEM_PLUS || wp == VK_ADD) { ui_zoom(10); return 0; }
            if (wp == VK_OEM_MINUS || wp == VK_SUBTRACT) { ui_zoom(-10); return 0; }
            if (wp == '0' || wp == VK_NUMPAD0) { ui_zoom(100 - U.zoom); return 0; }
        }
        ui_key(wp);
        return 0;
    case WM_SYSKEYDOWN:
        if (wp == VK_F10) { ui_key(VK_F10); return 0; }
        break;
    case WM_TIMER:
        if (wp == 2) { ui_minute_tick(); return 0; }
        if (wp == 3) { KillTimer(h, 3); U.mapFast = 0; ui_invalidate(); return 0; }
        if (wp == 4) { KillTimer(h, 4); ui_invalidate(); return 0; }
        if (GetTickCount() > U.toastUntil) { U.toast[0] = 0; KillTimer(h, 1); }
        ui_invalidate();
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO *)lp;
        mm->ptMinTrackSize.x = (LONG)(960 * s_dpi);
        mm->ptMinTrackSize.y = (LONG)(640 * s_dpi);
        return 0;
    }
    case WM_CLOSE:
        if (U.screen == SCR_GAME && !G.over) ui_save();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    (void)prev; (void)cmd;
    SetProcessDPIAware();
    HDC sdc = GetDC(NULL);
    s_dpi = GetDeviceCaps(sdc, LOGPIXELSX) / 96.0f;
    ReleaseDC(NULL, sdc);
    if (s_dpi < 1.0f) s_dpi = 1.0f;

    data_init();
    load_settings();
    resolve_faces();
    apply_scale();
    U.screen = SCR_TITLE;
    U.city = -1;
    U.selArmy = -1;
    U.diffPick = 1;
    {
        static Game probe;
        U.hasSave = load_into(&probe) && !probe.over;
    }
    {
        const wchar_t *sc = cmd ? wcsstr(cmd, L"--scene=") : 0;
        if (sc) { g_dev = 1; g_dark = 1; apply_scale(); dev_scene(_wtoi(sc + 8)); }
    }

    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof wc);
    wc.cbSize = sizeof wc;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.hIconSm = wc.hIcon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"BelarusTechFate";
    RegisterClassExW(&wc);

    RECT wa;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int ww = (int)(1400 * s_dpi), wh = (int)(880 * s_dpi);
    if (ww > wa.right - wa.left) ww = wa.right - wa.left;
    if (wh > wa.bottom - wa.top) wh = wa.bottom - wa.top;
#ifdef ADMIN_BUILD
    const wchar_t *title = L"Судьба Республики Беларусь в объективе технологий — админ-версия";
#else
    const wchar_t *title = L"Судьба Республики Беларусь в объективе технологий";
#endif
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, title,
                             WS_OVERLAPPEDWINDOW, wa.left + (wa.right - wa.left - ww) / 2, wa.top + (wa.bottom - wa.top - wh) / 2,
                             ww, wh, NULL, NULL, inst, NULL);
    SetTimer(g_hwnd, 2, 1000, NULL);   /* часы минутного дохода */
    ShowWindow(g_hwnd, show == SW_SHOWMINIMIZED ? SW_SHOWNORMAL : show);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
