/* Карта: контур, реки, болота и пущи рисуются в тройном разрешении и сглаживаются усреднением. */
#include "ui.h"
#include <math.h>
#include <string.h>

#define SS 3
#define LATC 0.5920   /* cos(53.7°) */

static HDC s_mdc;
static HBITMAP s_mbmp, s_mold;
static unsigned *s_mbits;
static int s_mw, s_mh, s_key = -1;

static double s_k, s_ox, s_oy;   /* проекция в пикселях экрана относительно угла карты */

void map_invalidate(void) { s_key = -1; }

static void geo_xy(double lon, double lat, double scale, int *x, int *y)
{
    *x = (int)lround((s_ox + (lon - 23.0) * LATC * s_k) * scale);
    *y = (int)lround((s_oy + (56.25 - lat) * s_k) * scale);
}

static void fit(int w, int h)
{
    double gw = 9.85 * LATC, gh = 5.10;
    double pad = SC(14);
    double kx = (w - 2 * pad) / gw, ky = (h - 2 * pad) / gh;
    s_k = kx < ky ? kx : ky;
    s_ox = (w - gw * s_k) / 2.0;
    s_oy = (h - gh * s_k) / 2.0;
}

static HDC make_dib(int w, int h, unsigned **bits, HBITMAP *bmp)
{
    BITMAPINFO bi;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(d_dc());
    *bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)bits, NULL, 0);
    return dc;
}

static void pen_poly(HDC dc, const POINT *p, int n, COLORREF c, int t, int closed, COLORREF fill)
{
    HPEN pen = CreatePen(PS_SOLID, t, c);
    HGDIOBJ op = SelectObject(dc, pen);
    HGDIOBJ ob = SelectObject(dc, fill == CLR_INVALID ? GetStockObject(NULL_BRUSH) : GetStockObject(DC_BRUSH));
    if (fill != CLR_INVALID) SetDCBrushColor(dc, fill);
    if (closed) Polygon(dc, p, n); else Polyline(dc, p, n);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
}

static unsigned hash2(int a, int b)
{
    unsigned h = (unsigned)a * 374761393u + (unsigned)b * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

/* Слой карты: фон, сетка, суша, болота, пущи, озёра, реки, граница. */
static void render_layer(int w, int h, int marsh)
{
    int W = w * SS, H = h * SS;
    unsigned *big;
    HBITMAP bbmp;
    HDC bdc = make_dib(W, H, &big, &bbmp);
    HGDIOBJ bold = SelectObject(bdc, bbmp);
    RECT all = { 0, 0, W, H };
    SetDCBrushColor(bdc, P.surface);
    FillRect(bdc, &all, (HBRUSH)GetStockObject(DC_BRUSH));

    /* градусная сетка */
    COLORREF grid = mix(P.surface, P.line, 150);
    for (int lon = 23; lon <= 33; lon++) {
        POINT a, b;
        geo_xy(lon, 51.0, SS, (int *)&a.x, (int *)&a.y);
        geo_xy(lon, 56.5, SS, (int *)&b.x, (int *)&b.y);
        POINT l[2] = { a, b };
        pen_poly(bdc, l, 2, grid, SS, 0, CLR_INVALID);
    }
    for (int lat = 51; lat <= 56; lat++) {
        POINT a, b;
        geo_xy(22.8, lat, SS, (int *)&a.x, (int *)&a.y);
        geo_xy(33.2, lat, SS, (int *)&b.x, (int *)&b.y);
        POINT l[2] = { a, b };
        pen_poly(bdc, l, 2, grid, SS, 0, CLR_INVALID);
    }

    /* суша */
    POINT bp[128];
    int n = BORDER_N < 128 ? BORDER_N : 128;
    for (int i = 0; i < n; i++) geo_xy(BORDER[i].lon, BORDER[i].lat, SS, (int *)&bp[i].x, (int *)&bp[i].y);
    pen_poly(bdc, bp, n, P.land, 1, 1, P.land);

    /* оттенок вдоль границы — «тень» края */
    HRGN rg = CreatePolygonRgn(bp, n, WINDING);
    SelectClipRgn(bdc, rg);

    /* полесские болота: чем больше осушено, тем реже штриховка */
    int density = marsh == 0 ? 3 : marsh == 1 ? 5 : 9;
    COLORREF mc = mix(P.land, P.water, 90);
    for (double lon = 24.2; lon < 30.6; lon += 0.11)
        for (double lat = 51.45; lat < 52.75; lat += 0.075) {
            double cx = (lon - 27.3) / 3.3, cy = (lat - 52.05) / 0.62;
            if (cx * cx + cy * cy > 1.0) continue;
            unsigned hsh = hash2((int)(lon * 100), (int)(lat * 100));
            if ((int)(hsh % (unsigned)density) != 0) continue;
            int x, y;
            geo_xy(lon + (hsh % 7) * 0.01, lat, SS, &x, &y);
            POINT d[2] = { { x - SS * 3, y }, { x + SS * 3, y } };
            pen_poly(bdc, d, 2, mc, SS, 0, CLR_INVALID);
        }

    /* пущи: Беловежская, Налибокская, Березинская */
    static const double forests[][3] = { { 23.85, 52.62, 0.35 }, { 26.40, 53.85, 0.30 }, { 28.30, 54.72, 0.28 }, { 27.6, 52.3, 0.25 } };
    for (int f = 0; f < 4; f++)
        for (int i = 0; i < 26; i++) {
            unsigned hsh = hash2(f * 97 + i, i * 31 + 7);
            double a = (hsh % 628) / 100.0, r = (hsh % 1000) / 1000.0 * forests[f][2];
            int x, y;
            geo_xy(forests[f][0] + cos(a) * r, forests[f][1] + sin(a) * r * 0.6, SS, &x, &y);
            int s = SS * 4;
            POINT t[4] = { { x, y - s }, { x + s * 2 / 3, y + s / 2 }, { x - s * 2 / 3, y + s / 2 }, { x, y - s } };
            pen_poly(bdc, t, 4, P.forest, 1, 1, P.forest);
        }

    /* озеро Нарочь */
    {
        int x, y;
        geo_xy(26.75, 54.86, SS, &x, &y);
        HGDIOBJ ob = SelectObject(bdc, GetStockObject(DC_BRUSH));
        HGDIOBJ op = SelectObject(bdc, GetStockObject(NULL_PEN));
        SetDCBrushColor(bdc, P.water);
        Ellipse(bdc, x - SS * 7, y - SS * 4, x + SS * 7, y + SS * 4);
        SelectObject(bdc, ob);
        SelectObject(bdc, op);
    }
    SelectClipRgn(bdc, NULL);
    DeleteObject(rg);

    /* реки */
    for (int r = 0; r < RIVER_COUNT; r++) {
        POINT rp[32];
        int m = RIVERS[r].n < 32 ? RIVERS[r].n : 32;
        for (int i = 0; i < m; i++) geo_xy(RIVERS[r].pts[i].lon, RIVERS[r].pts[i].lat, SS, (int *)&rp[i].x, (int *)&rp[i].y);
        int t = (int)(SS * g_s * (r <= 1 || r == 4 || r == 5 ? 2.3 : 1.5));
        pen_poly(bdc, rp, m, P.water, t, 0, CLR_INVALID);
    }

    /* граница */
    pen_poly(bdc, bp, n, P.landEdge, (int)(SS * g_s * 1.6), 1, CLR_INVALID);

    /* усреднение 3×3 → итоговый слой */
    GdiFlush();
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned r = 0, g = 0, b = 0;
            for (int dy = 0; dy < SS; dy++) {
                const unsigned *row = big + (size_t)(y * SS + dy) * W + x * SS;
                for (int dx = 0; dx < SS; dx++) {
                    unsigned v = row[dx];
                    r += (v >> 16) & 255; g += (v >> 8) & 255; b += v & 255;
                }
            }
            s_mbits[(size_t)y * w + x] = ((r / 9) << 16) | ((g / 9) << 8) | (b / 9);
        }
    SelectObject(bdc, bold);
    DeleteObject(bbmp);
    DeleteDC(bdc);
}

static int city_relevant(int c)
{
    for (int p = 0; p < PROJECT_COUNT; p++)
        if (PROJECTS[p].city == c && game_proj_state(&G, p) != PS_FUTURE) return 1;
    return 0;
}

static int capital_of_era(int era)
{
    if (era == 1) return C_POLOTSK;
    if (era == 2) return C_NOVOGRUDOK;
    if (era >= 5) return C_MINSK;
    return -1;
}

void map_draw(int x, int y, int w, int h, int interactive)
{
    if (w < 40 || h < 40) return;
    int marsh = (U.screen == SCR_GAME ? (game_has_tech(&G, "drainage") + 2 * game_has_tech(&G, "land_reclamation")) : 0);
    if (marsh > 2) marsh = 2;
    int key = (w * 7919 + h) * 16 + g_dark * 4 + marsh;
    fit(w, h);
    if (key != s_key || w != s_mw || h != s_mh || !s_mdc) {
        if (s_mdc) { SelectObject(s_mdc, s_mold); DeleteObject(s_mbmp); DeleteDC(s_mdc); }
        s_mdc = make_dib(w, h, &s_mbits, &s_mbmp);
        s_mold = SelectObject(s_mdc, s_mbmp);
        s_mw = w; s_mh = h;
        render_layer(w, h, marsh);
        s_key = key;
    }
    d_bitmap(s_mdc, x, y, w, h);

    /* подписи рек */
    static const struct { int r; double lon, lat; } rl[] = { { 0, 28.95, 55.12 }, { 1, 30.55, 53.40 }, { 4, 24.6, 53.53 }, { 5, 26.9, 52.0 } };
    for (int i = 0; i < 4; i++) {
        int rx, ry;
        geo_xy(rl[i].lon, rl[i].lat, 1, &rx, &ry);
        d_text(x + rx, y + ry, RIVERS[rl[i].r].name, F_TINY, mix(P.water, P.surface, 40));
    }

    /* города */
    int cap = U.screen == SCR_GAME ? capital_of_era(G.era) : -1;
    for (int c = 0; c < CITY_COUNT; c++) {
        int game = U.screen == SCR_GAME;
        int rel = game ? city_relevant(c) : 0;
        if (!CITIES[c].major && !rel) continue;
        int cx, cy;
        geo_xy(CITIES[c].lon, CITIES[c].lat, 1, &cx, &cy);
        cx += x; cy += y;
        int built = 0, avail = 0;
        if (game)
            for (int p = 0; p < PROJECT_COUNT; p++)
                if (PROJECTS[p].city == c) {
                    int st = game_proj_state(&G, p);
                    if (st == PS_DONE) {
                        /* квадраты — с противоположной от подписи стороны */
                        int col = built % 5, row = built / 5;
                        int bx = CITIES[c].lx < 0 ? cx + SC(6) + col * SC(6) : cx - SC(10) - col * SC(6);
                        int by = cy + SC(6) + row * SC(6);
                        d_fill(bx, by, SC(5), SC(5), PROJECTS[p].era == G.era ? P.red : mix(P.red, P.surface, 110));
                        built++;
                    } else if (st == PS_AVAILABLE) avail++;
                }
        int r = CITIES[c].major ? SC(4) : SC(3);
        int hv = interactive && hover(A_CITY, c);
        if (U.city == c || hv) d_circle(cx, cy, r + SC(5), CLR_INVALID, P.ink, 1);
        if (avail) d_circle(cx, cy, r + SC(3), CLR_INVALID, P.blue, SC(2) > 1 ? 2 : 1);
        if (c == cap) {
            POINT dmd[5] = { { cx, cy - r - SC(2) }, { cx + r + SC(2), cy }, { cx, cy + r + SC(2) }, { cx - r - SC(2), cy }, { cx, cy - r - SC(2) } };
            d_poly(dmd, 5, P.red, P.ink, 1);
        } else {
            d_circle(cx, cy, r, P.ink, P.surface, 1);
        }
        int f = CITIES[c].major ? F_SMALL_B : F_TINY;
        int lx = cx + SC(CITIES[c].lx), ly = cy + SC(CITIES[c].ly) - d_font_h(f) / 2;
        if (CITIES[c].lx < 0) lx = cx - d_text_w(CITIES[c].name, f) - SC(8);
        /* подложка, чтобы подпись читалась поверх рек и штриховки */
        int tw = d_text_w(CITIES[c].name, f);
        d_blend(lx - SC(2), ly, tw + SC(4), d_font_h(f), P.surface, 150);
        d_text(lx, ly, CITIES[c].name, f, CITIES[c].major ? P.ink : P.muted);
        if (interactive) hit(cx - SC(9), cy - SC(9), SC(18), SC(18), A_CITY, c);
    }
}
