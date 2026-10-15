/* Окно, двойная буферизация, шрифты, примитивы рисования, виджеты, сохранения. */
#include "ui.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

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

static const Pal PAL_LIGHT = {
    .bg = RGB(236, 236, 228), .surface = RGB(248, 247, 242), .surface2 = RGB(226, 226, 215),
    .ink = RGB(27, 33, 29), .muted = RGB(88, 96, 89), .line = RGB(203, 203, 190),
    .red = RGB(163, 31, 40), .redSoft = RGB(243, 222, 219), .blue = RGB(40, 72, 156), .blueSoft = RGB(222, 229, 246),
    .green = RGB(46, 106, 54), .gold = RGB(146, 100, 14), .land = RGB(232, 231, 216), .landEdge = RGB(58, 64, 58),
    .water = RGB(76, 120, 186), .forest = RGB(92, 136, 84), .shadow = RGB(0, 0, 0), .onAccent = RGB(255, 255, 255)
};
static const Pal PAL_DARK = {
    .bg = RGB(15, 20, 17), .surface = RGB(24, 31, 27), .surface2 = RGB(35, 44, 38),
    .ink = RGB(230, 231, 220), .muted = RGB(150, 160, 148), .line = RGB(54, 65, 58),
    .red = RGB(228, 94, 98), .redSoft = RGB(72, 34, 37), .blue = RGB(140, 165, 238), .blueSoft = RGB(37, 47, 76),
    .green = RGB(140, 190, 116), .gold = RGB(222, 176, 84), .land = RGB(37, 48, 41), .landEdge = RGB(176, 186, 172),
    .water = RGB(92, 138, 210), .forest = RGB(78, 124, 82), .shadow = RGB(0, 0, 0), .onAccent = RGB(22, 14, 15)
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

void d_rrect(int x, int y, int w, int h, int r, COLORREF fill, COLORREF border)
{
    HGDIOBJ ob = SelectObject(s_dc, fill == CLR_INVALID ? GetStockObject(NULL_BRUSH) : GetStockObject(DC_BRUSH));
    HGDIOBJ op = SelectObject(s_dc, border == CLR_INVALID ? GetStockObject(NULL_PEN) : GetStockObject(DC_PEN));
    if (fill != CLR_INVALID) SetDCBrushColor(s_dc, fill);
    if (border != CLR_INVALID) SetDCPenColor(s_dc, border);
    RoundRect(s_dc, x, y, x + w + (border == CLR_INVALID ? 1 : 0), y + h + (border == CLR_INVALID ? 1 : 0), r * 2, r * 2);
    SelectObject(s_dc, ob);
    SelectObject(s_dc, op);
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

void d_circle(int cx, int cy, int r, COLORREF fill, COLORREF border, int t)
{
    HPEN pen = border != CLR_INVALID && t > 1 ? CreatePen(PS_SOLID, t, border) : 0;
    HGDIOBJ op = SelectObject(s_dc, pen ? (HGDIOBJ)pen : border == CLR_INVALID ? GetStockObject(NULL_PEN) : GetStockObject(DC_PEN));
    HGDIOBJ ob = SelectObject(s_dc, fill == CLR_INVALID ? GetStockObject(NULL_BRUSH) : GetStockObject(DC_BRUSH));
    if (border != CLR_INVALID && !pen) SetDCPenColor(s_dc, border);
    if (fill != CLR_INVALID) SetDCBrushColor(s_dc, fill);
    Ellipse(s_dc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(s_dc, op);
    SelectObject(s_dc, ob);
    if (pen) DeleteObject(pen);
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
    int hv = enabled && hover(act, arg);
    COLORREF bg, fg, br = CLR_INVALID;
    if (!enabled) { bg = P.surface2; fg = mix(P.muted, P.surface2, 60); }
    else if (kind == B_PRIMARY || kind == B_DANGER) { bg = hv ? mix(P.red, P.ink, 40) : P.red; fg = P.onAccent; }
    else if (kind == B_SECONDARY) { bg = hv ? mix(P.surface2, P.ink, 22) : P.surface2; fg = P.ink; br = P.line; }
    else { bg = hv ? P.blueSoft : CLR_INVALID; fg = P.blue; }
    d_rrect(x, y, w, h, SC(5), bg, br);
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
    d_rrect(x, y, SC(5), h, SC(2), mix(P.surface, P.line, 120), CLR_INVALID);
    d_rrect(x, ty, SC(5), th, SC(2), P.muted, CLR_INVALID);
}

/* ---------- сервис ---------- */
void ui_invalidate(void) { if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE); }

void ui_toast(const wchar_t *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vswprintf(U.toast, 160, fmt, ap);
    va_end(ap);
    U.toastUntil = GetTickCount() + 2800;
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
    swprintf(out, n, L"%ls\\BelarusTechFate", base);
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
            ui_paint();
            GdiFlush();
            BitBlt(wdc, 0, 0, g_w, g_h, s_dc, 0, 0, SRCCOPY);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int a, g2;
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
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Судьба Республики Беларусь в объективе технологий",
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
