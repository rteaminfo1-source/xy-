/* Карта: контур, реки, болота и пущи рисуются в тройном разрешении и сглаживаются усреднением. */
#include "ui.h"
#include "map3d.h"
#include "war.h"
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

/* ---------- слой войны: земли, цель похода, оккупация ---------- */
typedef struct { const MapXf *xf; int ox, oy; int x, y, w, h; double pxPerKm; int size; } MapProj;

static int proj_pt(const MapProj *pj, double lon, double lat, int *sx, int *sy)
{
    if (pj->xf) {
        double px, py;
        if (!map3d_project(lon, lat, &px, &py)) return 0;
        art_map_point(pj->xf, px, py, sx, sy);
    } else {
        geo_xy(lon, lat, 1, sx, sy);
        *sx += pj->ox; *sy += pj->oy;
    }
    return 1;
}

static void war_overlay(const MapProj *pj, int interactive, int large)
{
    if (U.screen != SCR_GAME) return;
    int pad = SC(10);
    int clipped = 0, ocx, ocy, ocw, och;
    if (!d_clip_get(&ocx, &ocy, &ocw, &och)) { d_clip(pj->x, pj->y, pj->w, pj->h); clipped = 1; }
    /* оккупация: всё темнеет, у центров областей — знаки */
    if (G.occupied || (G.year >= 1941 && G.year <= 1944)) {
        if (G.occupied) d_blend(pj->x, pj->y, pj->w, pj->h, RGB(24, 14, 12), 70);
        /* кольцо вокруг центра области (модели построек остаются видны) и значок на кольце */
        for (int r = 0; r < REGION_COUNT; r++) {
            int c = REGION_CITY[r], sx, sy;
            if (!proj_pt(pj, CITIES[c].lon, CITIES[c].lat, &sx, &sy)) continue;
            int occ = (G.occupied >> r) & 1u, target = G.warKind == WAR_LIBERATION && G.warTarget == r;
            int ring = pj->size * 11 / 10;
            if (ring < SC(26)) ring = SC(26);
            COLORREF ac = target ? P.red : occ ? RGB(150, 40, 34) : P.green;
            d_circle(sx, sy, ring, CLR_INVALID, ac, target ? 4 : 2);
            int rr = large ? SC(14) : SC(10);
            int bx = sx - ring * 71 / 100, by = sy + ring * 71 / 100;
            d_circle(bx, by, rr, occ ? RGB(40, 24, 20) : mix(P.surface, P.green, 60), ac, 2);
            d_icon(occ ? IC_WAR : IC_CHECK, bx - rr * 6 / 10, by - rr * 6 / 10, rr * 12 / 10, occ ? (target ? P.red : RGB(230, 200, 190)) : P.green);
            if (large) {
                int tw = d_text_w(REGION_NAMES[r], F_TINY);
                d_blend(bx - tw / 2 - SC(4), by + rr + SC(2), tw + SC(8), d_font_h(F_TINY) + SC(2), P.surface, 200);
                d_text(bx - tw / 2, by + rr + SC(3), REGION_NAMES[r], F_TINY, occ ? P.ink : P.green);
            }
            if (interactive) hit(bx - rr, by - rr, rr * 2, rr * 2, A_HOVER_REGION, r);
        }
    }
    /* земли: свои, доступные для похода, цель похода */
    for (int p = 0; p < PROVINCE_COUNT; p++) {
        int st = war_province_state(&G, p);
        int target = G.warKind == WAR_CONQUEST && G.warTarget == p;
        if (st != PV_OWNED && st != PV_AVAILABLE && !target) continue;
        int sx, sy;
        if (!proj_pt(pj, PROVINCES[p].lon, PROVINCES[p].lat, &sx, &sy)) continue;
        if (sx < pj->x + pad) sx = pj->x + pad;
        if (sx > pj->x + pj->w - pad) sx = pj->x + pj->w - pad;
        if (sy < pj->y + pad) sy = pj->y + pad;
        if (sy > pj->y + pj->h - pad) sy = pj->y + pj->h - pad;
        int rr = (int)(PROVINCES[p].radiusKm * 0.55 * pj->pxPerKm);
        int rmax = large ? SC(64) : SC(30);
        if (rr > rmax) rr = rmax;
        if (rr < SC(12)) rr = SC(12);
        COLORREF ac = target ? P.red : st == PV_OWNED ? P.gold : P.blue;
        d_circle(sx, sy, rr, CLR_INVALID, ac, target ? 3 : 2);
        if (st == PV_OWNED) d_circle(sx, sy, rr - SC(4), CLR_INVALID, mix(P.gold, P.surface, 120), 1);
        wchar_t lab[96];
        if (target) swprintf(lab, 96, L"%ls · поход %d%%", PROVINCES[p].name, G.warProgress < 0 ? 0 : G.warProgress);
        else if (st == PV_OWNED) swprintf(lab, 96, L"%ls", PROVINCES[p].name);
        else swprintf(lab, 96, large ? L"%ls · можно в поход" : L"%ls", PROVINCES[p].name);
        int f = large ? F_SMALL_B : F_TINY;
        int tw = d_text_w(lab, f), th = d_font_h(f);
        int lx = sx - tw / 2 + SC(10), ly = sy - th / 2;
        if (lx - SC(22) < pj->x + SC(2)) lx = pj->x + SC(24);
        if (lx + tw + SC(10) > pj->x + pj->w - SC(2)) lx = pj->x + pj->w - tw - SC(12);
        if (ly - SC(3) < pj->y + SC(2)) ly = pj->y + SC(5);
        if (ly + th + SC(3) > pj->y + pj->h - SC(2)) ly = pj->y + pj->h - th - SC(5);
        COLORREF bg = st == PV_OWNED && !target ? mix(P.surface, P.gold, 70) : P.surface;
        d_rrect(lx - SC(22), ly - SC(3), tw + SC(30), th + SC(6), SC(4), bg, ac);
        d_icon(st == PV_OWNED && !target ? IC_FLAG : IC_WAR, lx - SC(18), ly - SC(1), th + SC(2) > SC(16) ? SC(16) : th + SC(2), ac);
        d_text(lx, ly, lab, f, P.ink);
        if (interactive) hit(lx - SC(22), ly - SC(3), tw + SC(30), th + SC(6), A_PROVINCE, p);
    }
    if (clipped) d_unclip();
}

static void map_draw_flat(int x, int y, int w, int h, int interactive)
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
    MapProj pj = { 0, x, y, x, y, w, h, s_k / 111.0, SC(20) };
    war_overlay(&pj, interactive, w > SC(900));
}

/* ---------- 3D-карта из Blender ---------- */
typedef struct { int x, y, spr, proj; } Placed;

/* Места для моделей вокруг города (в долях размера модели); центр оставлен самому городу */
static const float SLOTS[][2] = {
    {  0.62f, 0.08f }, { -0.62f, 0.08f }, {  0.00f, 0.42f }, {  1.20f, -0.20f }, { -1.20f, -0.20f },
    {  0.60f, 0.76f }, { -0.60f, 0.76f }, {  1.22f,  0.50f }, { -1.22f,  0.50f }, {  0.00f, -0.46f },
};

static void city_point(const MapXf *xf, int c, int *sx, int *sy)
{
    double px, py;
    if (!map3d_city(c, &px, &py) && !map3d_project(CITIES[c].lon, CITIES[c].lat, &px, &py)) { px = 0; py = 0; }
    art_map_point(xf, px, py, sx, sy);
}

void map_draw_ex(int x, int y, int w, int h, int interactive, int large)
{
    MapXf xf;
    if (!art_map_draw(x, y, w, h, &xf)) { map_draw_flat(x, y, w, h, interactive); return; }
    int game = U.screen == SCR_GAME;
    int size = (int)(xf.w * (large ? 0.050 : 0.078));
    if (size < SC(30)) size = SC(30);
    if (size > SC(large ? 116 : 76)) size = SC(large ? 116 : 76);
    int maxPer = large ? (int)(sizeof SLOTS / sizeof SLOTS[0]) : 4;

    int cx[C_COUNT_], cy[C_COUNT_], extra[C_COUNT_], avail[C_COUNT_], built[C_COUNT_];
    static Placed pl[MAX_PROJ];
    int np = 0;
    for (int c = 0; c < CITY_COUNT; c++) {
        city_point(&xf, c, &cx[c], &cy[c]);
        extra[c] = avail[c] = built[c] = 0;
    }
    if (game)
        for (int p = 0; p < PROJECT_COUNT; p++) {
            int c = PROJECTS[p].city, st = game_proj_state(&G, p);
            if (st == PS_AVAILABLE) avail[c]++;
            if (st != PS_DONE) continue;
            int spr = landmark_sprite(p);
            if (spr >= 0 && built[c] < maxPer && np < MAX_PROJ) {
                pl[np].x = cx[c] + (int)(SLOTS[built[c]][0] * size);
                pl[np].y = cy[c] + (int)(SLOTS[built[c]][1] * size);
                pl[np].spr = spr;
                pl[np].proj = p;
                np++;
                built[c]++;
            } else extra[c]++;
        }
    /* дальние (выше на экране) рисуются первыми */
    for (int i = 1; i < np; i++) {
        Placed v = pl[i];
        int j = i - 1;
        while (j >= 0 && pl[j].y > v.y) { pl[j + 1] = pl[j]; j--; }
        pl[j + 1] = v;
    }
    for (int i = 0; i < np; i++) {
        int hv = interactive && hover(A_PROJ, pl[i].proj);
        art_sprite_draw(pl[i].spr, pl[i].x, pl[i].y, size, 255);
        if (hv) d_circle(pl[i].x, pl[i].y, size / 3, CLR_INVALID, P.gold, 2);
        if (interactive) hit(pl[i].x - size / 3, pl[i].y - size * 2 / 3, size * 2 / 3, size * 2 / 3 + SC(4), A_PROJ, pl[i].proj);
    }

    /* города: подписи (без наложений), кольца, флажки заданий */
    int cap = game ? capital_of_era(G.era) : -1;
    RECT used[C_COUNT_];
    int nused = 0;
    for (int pass = 0; pass < 2; pass++)
    for (int c = 0; c < CITY_COUNT; c++) {
        if ((pass == 0) != (CITIES[c].major != 0)) continue;
        int quests = 0;
        if (game)
            for (int q = 0; q < QUEST_COUNT; q++)
                if (QUESTS[q].city == c && game_quest_state(&G, q) == QS_ACTIVE) quests++;
        int rel = game && (built[c] || extra[c] || avail[c] || quests);
        if (!CITIES[c].major && !rel && !large) continue;
        int px = cx[c], py = cy[c];
        int hv = interactive && hover(A_CITY, c);
        if (avail[c]) d_circle(px, py, SC(9), CLR_INVALID, P.blue, 2);
        if (U.city == c || hv) d_circle(px, py, SC(12), CLR_INVALID, P.ink, 2);
        if (c == cap) {
            POINT dmd[5] = { { px, py - SC(6) }, { px + SC(6), py }, { px, py + SC(6) }, { px - SC(6), py }, { px, py - SC(6) } };
            d_poly(dmd, 5, P.red, P.surface, 1);
        }
        if (interactive) hit(px - SC(11), py - SC(11), SC(22), SC(22), A_CITY, c);
        int f = CITIES[c].major ? F_SMALL_B : F_TINY;
        wchar_t lab[64];
        if (extra[c]) swprintf(lab, 64, L"%ls +%d", CITIES[c].name, extra[c]);
        else swprintf(lab, 64, L"%ls", CITIES[c].name);
        int tw = d_text_w(lab, f), th = d_font_h(f);
        /* кандидаты: над точкой, под ней, справа, слева */
        int cand[4][2] = { { px - tw / 2, py - SC(14) - th }, { px - tw / 2, py + SC(10) },
                           { px + SC(12), py - th / 2 }, { px - SC(12) - tw, py - th / 2 } };
        int lx = 0, ly = 0, ok = 0;
        for (int k = 0; k < 4 && !ok; k++) {
            RECT r = { cand[k][0] - SC(4), cand[k][1] - SC(1), cand[k][0] + tw + SC(4), cand[k][1] + th + SC(1) };
            int clash = 0;
            for (int u = 0; u < nused && !clash; u++) { RECT o; clash = IntersectRect(&o, &r, &used[u]); }
            if (!clash) { lx = cand[k][0]; ly = cand[k][1]; used[nused++] = r; ok = 1; }
        }
        if (!ok && !CITIES[c].major && !hv) continue;          /* мелкий город без места — без подписи */
        if (!ok) { lx = cand[0][0]; ly = cand[0][1]; }
        d_blend(lx - SC(4), ly - SC(1), tw + SC(8), th + SC(2), P.surface, 185);
        d_text(lx, ly, lab, f, CITIES[c].major ? P.ink : P.muted);
        for (int q = 0, k = 0; game && q < QUEST_COUNT; q++)
            if (QUESTS[q].city == c && game_quest_state(&G, q) == QS_ACTIVE) {
                int fx = lx - SC(20) - k * SC(16), fy = ly - SC(2);
                d_icon(IC_FLAG, fx, fy, SC(16), P.red);
                if (interactive) hit(fx, fy, SC(16), SC(18), A_QUEST, q);
                k++;
            }
        if (interactive) hit(lx - SC(4), ly - SC(1), tw + SC(8), th + SC(2), A_CITY, c);
    }
    MapProj pj = { &xf, 0, 0, xf.x, xf.y, xf.w, xf.h, map3d_px_per_km() * xf.s, size };
    war_overlay(&pj, interactive, large);
}

void map_draw(int x, int y, int w, int h, int interactive) { map_draw_ex(x, y, w, h, interactive, 0); }
