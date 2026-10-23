/* Карта: контур, реки, болота и пущи рисуются в тройном разрешении и сглаживаются усреднением. */
#include "ui.h"
#include "mapview.h"
#include "map3d.h"
#include "war.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

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

/* ---------- проекция: 3D-карта, готовый рендер или плоская карта ---------- */
struct MapProj {
    const MapXf *xf;          /* готовая картинка Blender */
    const T3View *v;          /* интерактивная 3D-карта */
    int ox, oy;               /* смещение окна вывода */
    int x, y, w, h;           /* окно вывода */
    double pxPerKm;           /* масштаб у цели (3D) или везде (2D) */
    int size;                 /* базовый размер моделей построек */
};

/* точка lon/lat на земле; k — пикселей на километр в этой точке */
static int proj_ll(const MapProj *pj, double lon, double lat, double zAdd, int *sx, int *sy, double *k)
{
    if (k) *k = pj->pxPerKm;
    if (pj->v) {
        double X, Y, fx, fy, depth;
        t3d_lonlat(lon, lat, &X, &Y);
        if (!t3d_project(pj->v, X, Y, t3d_height(X, Y) + zAdd, &fx, &fy, &depth)) return 0;
        *sx = pj->ox + (int)lround(fx);
        *sy = pj->oy + (int)lround(fy);
        if (k) *k = pj->v->f / depth;
        return 1;
    }
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


/* ---------- слой войны: земли, цель похода, оккупация ---------- */
static void km_ll(double X, double Y, double *lon, double *lat) { *lon = 28.0 + X / 65.95; *lat = 53.7 + Y / 111.32; }

/* точка мира (км) с высотой Z над ней — для дуги стрелки; на 2D-картах высота не учитывается */
static int proj_xyz(const MapProj *pj, double X, double Y, double Z, int *sx, int *sy, double *k)
{
    if (pj->v) {
        double fx, fy, d;
        if (!t3d_project(pj->v, X, Y, Z, &fx, &fy, &d)) return 0;
        *sx = pj->ox + (int)lround(fx);
        *sy = pj->oy + (int)lround(fy);
        if (k) *k = pj->v->f / d;
        return 1;
    }
    double lon, lat;
    km_ll(X, Y, &lon, &lat);
    return proj_ll(pj, lon, lat, 0, sx, sy, k);
}

/* ---------- доступ для слоёв других модулей (mapview.h) ---------- */
static int proj_city(const MapProj *pj, int c, int *sx, int *sy, double *k, double *depth);

int  mapv_ll(const MapProj *pj, double lon, double lat, double zAdd, int *sx, int *sy, double *k) { return proj_ll(pj, lon, lat, zAdd, sx, sy, k); }
int  mapv_xyz(const MapProj *pj, double X, double Y, double Z, int *sx, int *sy, double *k) { return proj_xyz(pj, X, Y, Z, sx, sy, k); }
void mapv_rect(const MapProj *pj, int *x, int *y, int *w, int *h) { *x = pj->x; *y = pj->y; *w = pj->w; *h = pj->h; }
const T3View *mapv_view(const MapProj *pj) { return pj->v; }
double mapv_px_per_km(const MapProj *pj) { return pj->pxPerKm; }
int  mapv_size(const MapProj *pj) { return pj->size; }
double mapv_scale(const MapProj *pj, double k) { return pj->v && pj->pxPerKm > 0 ? pow(k / pj->pxPerKm, 0.7) : 1.0; }

int mapv_city(const MapProj *pj, int c, int *sx, int *sy, double *k)
{
    double kk = pj->pxPerKm, depth;
    if (c < 0 || c >= CITY_COUNT) return 0;
    if (pj->xf || pj->v) {
        int ok = proj_city(pj, c, sx, sy, &kk, &depth);
        if (k) *k = kk;
        return ok;
    }
    return proj_ll(pj, CITIES[c].lon, CITIES[c].lat, 0.3, sx, sy, k);
}

/* «вспышка» на месте последнего боя */
static void battle_mark(const MapProj *pj, int interactive, int large)
{
    if (G.battleTurn != G.turn || G.over) return;
    int sx, sy;
    double k;
    if (!proj_ll(pj, G.battleLon, G.battleLat, 1.0, &sx, &sy, &k)) return;
    if (sx < pj->x || sx > pj->x + pj->w || sy < pj->y || sy > pj->y + pj->h) return;
    int r = large ? SC(20) : SC(14);
    POINT star[17];
    for (int i = 0; i < 16; i++) {
        double a = i * 3.14159265 / 8, rr = i % 2 ? r * 0.55 : r;
        star[i].x = sx + (int)(cos(a) * rr);
        star[i].y = sy - r / 2 + (int)(sin(a) * rr);
    }
    star[16] = star[0];
    COLORREF c = G.battleWon ? RGB(236, 180, 60) : RGB(214, 64, 44);
    d_poly(star, 17, c, RGB(60, 16, 12), 1);
    d_icon(IC_WAR, sx - r / 2, sy - r, r, RGB(40, 16, 12));
    const wchar_t *lab = G.battleWon ? L"бой: победа" : L"бой: поражение";
    int f = large ? F_SMALL_B : F_TINY, tw = d_text_w(lab, f), th = d_font_h(f);
    int lx = sx + r + SC(8), ly = sy - r / 2 - th / 2;      /* справа от звезды: слева встаёт армия */
    d_rrect(lx - SC(5), ly - SC(2), tw + SC(10), th + SC(4), SC(4), P.surface, c);
    d_text(lx, ly, lab, f, G.battleWon ? P.ink : P.red);
    if (interactive) hit(lx - SC(5), ly - SC(2), tw + SC(10), th + SC(4), A_HOVER_BATTLE, 0);
}

/* террасы земель для 3D-карты (и их провинции — для обводки); key — подпись состояния для кэша */
static T3Blob s_blobs[MAX_PROVINCES];
static int    s_blobProv[MAX_PROVINCES], s_nblobs;

static int build_blobs(T3Blob *out, int max, unsigned *key)
{
    static const float INTERIOR[6][2] = { { -80, 40 }, { 60, -20 }, { -20, 110 }, { 110, 80 }, { -150, -60 }, { 20, -120 } };
    int n = 0;
    unsigned h = 2166136261u;
    if (U.screen != SCR_GAME) { *key = 0; return 0; }
    for (int p = 0; p < PROVINCE_COUNT && n < max; p++) {
        int st = war_province_state(&G, p);
        int target = G.warKind == WAR_CONQUEST && G.warTarget == p;
        int dup = !G.provOwned[p] && war_group_owner(&G, p) >= 0;
        if (dup || (st != PV_OWNED && st != PV_AVAILABLE && !target)) continue;
        T3Blob *b = &out[n];
        s_blobProv[n] = p;
        double X, Y;
        t3d_lonlat(PROVINCES[p].lon, PROVINCES[p].lat, &X, &Y);
        double R = PROVINCES[p].radiusKm * 1.1;
        double bx, by;
        t3d_outline_nearest(X, Y, &bx, &by);
        double dir0 = atan2(by - Y, bx - X);
        unsigned hs = (unsigned)p * 2654435761u + 12345u;
        double ph1 = (hs % 628) / 100.0, ph2 = ((hs >> 10) % 628) / 100.0, ph3 = ((hs >> 20) % 628) / 100.0;
        b->cx = (float)X; b->cy = (float)Y;
        b->n = 40;
        for (int i = 0; i < b->n; i++) {
            double th = 2 * 3.14159265358979 * i / b->n;
            double r = R * (1 + 0.12 * sin(3 * th + ph1) + 0.07 * sin(5 * th + ph2) + 0.05 * sin(8 * th + ph3));
            /* в сторону Беларуси земля дотягивается до плиты и уходит под неё — но только если граница
             * близко: луч вдоль границы встретит её далеко, и земля расползлась бы на соседние страны */
            if (cos(th - dir0) > 0.7) {
                double d = t3d_outline_hit(X, Y, cos(th), sin(th));
                if (d > 0 && d < R * 1.7 && d + 25 > r) r = d + 25;
            }
            b->x[i] = (float)(X + cos(th) * r);
            b->y[i] = (float)(Y + sin(th) * r);
        }
        b->top = (float)(3.4 - 0.07 * n);
        const float *ip = INTERIOR[p % 6];
        b->ux = ip[0] - (float)X; b->uy = ip[1] - (float)Y;
        if (target) { b->tint[0] = 1.18f; b->tint[1] = 0.74f; b->tint[2] = 0.64f; b->grey = 0.25f; }
        else if (st == PV_OWNED) { b->tint[0] = 1.10f; b->tint[1] = 1.00f; b->tint[2] = 0.72f; b->grey = 0.0f; }
        else { b->tint[0] = 0.92f; b->tint[1] = 0.95f; b->tint[2] = 1.0f; b->grey = 0.8f; }
        h = (h ^ (unsigned)(p * 4 + (target ? 3 : st == PV_OWNED ? 2 : 1))) * 16777619u;
        n++;
    }
    *key = h ^ (unsigned)n;
    return n;
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
            double k;
            if (!proj_ll(pj, CITIES[c].lon, CITIES[c].lat, 0.5, &sx, &sy, &k)) continue;
            int occ = (G.occupied >> r) & 1u, target = G.warKind == WAR_LIBERATION && G.warTarget == r;
            int ring = (int)(pj->size * 11 / 10 * (pj->v ? pow(k / pj->pxPerKm, 0.7) : 1.0));
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
    /* на 3D-карте — обводка террас земель (только снаружи Беларуси, под плитой её не видно) */
    if (pj->v)
        for (int b = 0; b < s_nblobs; b++) {
            const T3Blob *bl = &s_blobs[b];
            int p = s_blobProv[b], st = war_province_state(&G, p);
            int target = G.warKind == WAR_CONQUEST && G.warTarget == p;
            COLORREF ac = target ? P.red : st == PV_OWNED ? RGB(232, 190, 70) : RGB(150, 170, 200);
            int th = target || st == PV_OWNED ? (large ? 4 : 3) : 2;
            int px0 = 0, py0 = 0, ok0 = 0;
            for (int i = 0; i <= bl->n; i++) {
                int k = i % bl->n, x1, y1;
                int in = t3d_inside(bl->x[k], bl->y[k]);
                int ok1 = !in && proj_xyz(pj, bl->x[k], bl->y[k], bl->top, &x1, &y1, 0);
                if (ok0 && ok1) {
                    if (st != PV_OWNED && !target && (i & 1)) { /* пунктир: доступна для похода */ }
                    else d_line(px0, py0, x1, y1, ac, th);
                }
                px0 = x1; py0 = y1; ok0 = ok1;
            }
        }
    /* земли: свои, доступные для похода, цель похода */
    for (int p = 0; p < PROVINCE_COUNT; p++) {
        int st = war_province_state(&G, p);
        int target = G.warKind == WAR_CONQUEST && G.warTarget == p;
        if (st != PV_OWNED && st != PV_AVAILABLE && !target) continue;
        int sx, sy;
        double k;
        if (!proj_ll(pj, PROVINCES[p].lon, PROVINCES[p].lat, 0.5, &sx, &sy, &k)) continue;
        /* на 3D-карте земля за краем кадра просто не видна; на плоской — прижимается к краю */
        if (pj->v && (sx < pj->x || sx > pj->x + pj->w || sy < pj->y || sy > pj->y + pj->h)) continue;
        if (sx < pj->x + pad) sx = pj->x + pad;
        if (sx > pj->x + pj->w - pad) sx = pj->x + pj->w - pad;
        if (sy < pj->y + pad) sy = pj->y + pad;
        if (sy > pj->y + pj->h - pad) sy = pj->y + pj->h - pad;
        int rr = (int)(PROVINCES[p].radiusKm * 0.55 * k);
        int rmax = large ? SC(64) : SC(30);
        if (rr > rmax) rr = rmax;
        if (rr < SC(12)) rr = SC(12);
        COLORREF ac = target ? P.red : st == PV_OWNED ? P.gold : P.blue;
        if (!pj->v) {   /* на 3D-карте земля видна террасой */
            d_circle(sx, sy, rr, CLR_INVALID, ac, target ? 3 : 2);
            if (st == PV_OWNED) d_circle(sx, sy, rr - SC(4), CLR_INVALID, mix(P.gold, P.surface, 120), 1);
        }
        wchar_t lab[96];
        if (target) swprintf(lab, 96, L"%ls · поход %d%%", PROVINCES[p].name, G.warProgress < 0 ? 0 : G.warProgress);
        else if (st == PV_OWNED) swprintf(lab, 96, L"%ls", PROVINCES[p].name);
        else swprintf(lab, 96, large ? L"%ls · можно в поход" : L"%ls", PROVINCES[p].name);
        int f = large ? F_SMALL_B : F_TINY;
        int tw = d_text_w(lab, f), th = d_font_h(f);
        /* подпись — над точкой земли (там её узел фронта и армии); в ход боя — ещё выше, над звездой боя */
        int lx = sx - tw / 2 + SC(10), ly = sy - SC(14) - th;
        if (target && G.battleTurn == G.turn && !G.over) {   /* в ход боя — над звездой и правее: слева от неё стоит армия */
            ly -= large ? SC(30) : SC(21);
            lx = sx + SC(16);
        }
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
    front_overlay(pj, interactive, large);       /* армии, железные дороги, поезда (ui_front.c) */
    battle_mark(pj, interactive, large);
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
    MapProj pj = { 0, 0, x, y, x, y, w, h, s_k / 111.0, SC(20) };
    states_overlay(&pj, interactive, w > SC(900));

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
    war_overlay(&pj, interactive, w > SC(900));
}

/* ---------- модели построек, подписи городов, флажки заданий (поверх любой карты) ---------- */
typedef struct { int x, y, spr, proj, size; double depth; } Placed;

/* Места для моделей вокруг города (в долях размера модели); центр оставлен самому городу */
static const float SLOTS[][2] = {
    {  0.62f, 0.08f }, { -0.62f, 0.08f }, {  0.00f, 0.42f }, {  1.20f, -0.20f }, { -1.20f, -0.20f },
    {  0.60f, 0.76f }, { -0.60f, 0.76f }, {  1.22f,  0.50f }, { -1.22f,  0.50f }, {  0.00f, -0.46f },
};

/* центр города; k — пикселей на км, depth — удалённость (для 3D), 0 — город не виден */
static int proj_city(const MapProj *pj, int c, int *sx, int *sy, double *k, double *depth)
{
    *depth = 0;
    if (pj->v) {
        double X, Y, fx, fy, d;
        t3d_lonlat(CITIES[c].lon, CITIES[c].lat, &X, &Y);
        if (!t3d_project(pj->v, X, Y, t3d_height(X, Y) + 0.3, &fx, &fy, &d)) return 0;
        *sx = pj->ox + (int)lround(fx);
        *sy = pj->oy + (int)lround(fy);
        *k = pj->v->f / d;
        *depth = d;
        /* за краем окна — не рисуем (с запасом под модели) */
        int m = pj->size * 2;
        return *sx > pj->x - m && *sx < pj->x + pj->w + m && *sy > pj->y - m && *sy < pj->y + pj->h + m;
    }
    double px, py;
    if (!map3d_city(c, &px, &py) && !map3d_project(CITIES[c].lon, CITIES[c].lat, &px, &py)) { px = 0; py = 0; }
    art_map_point(pj->xf, px, py, sx, sy);
    *k = pj->pxPerKm;
    *depth = 1000.0 - *sy;            /* чем ниже на картинке, тем ближе */
    return 1;
}

static void map_overlays(const MapProj *pj, int interactive, int large)
{
    int game = U.screen == SCR_GAME;
    int maxPer = large ? (int)(sizeof SLOTS / sizeof SLOTS[0]) : 4;
    states_overlay(pj, interactive, large);      /* исторические государства — под всем (ui_states.c) */
    fx_map_overlay(pj, interactive, large);      /* заставка ЧАЭС: облако и пятна загрязнения (ui_fx.c) */
    int cx[C_COUNT_], cy[C_COUNT_], vis[C_COUNT_], csz[C_COUNT_], extra[C_COUNT_], avail[C_COUNT_], built[C_COUNT_];
    double cdepth[C_COUNT_];
    static Placed pl[MAX_PROJ];
    int np = 0;
    int smax = SC(large ? 170 : 110), smin = SC(18);
    for (int c = 0; c < CITY_COUNT; c++) {
        double k = pj->pxPerKm;
        vis[c] = proj_city(pj, c, &cx[c], &cy[c], &k, &cdepth[c]);
        /* модели растут при приближении, но медленнее, чем земля под ними */
        double sc = pj->v ? pow(k / pj->pxPerKm, 0.7) : 1.0;
        csz[c] = (int)(pj->size * sc);
        if (csz[c] < smin) csz[c] = smin;
        if (csz[c] > smax) csz[c] = smax;
        extra[c] = avail[c] = built[c] = 0;
    }
    if (game)
        for (int p = 0; p < PROJECT_COUNT; p++) {
            int c = PROJECTS[p].city, st = game_proj_state(&G, p);
            if (st == PS_AVAILABLE) avail[c]++;
            if (st != PS_DONE) continue;
            int spr = landmark_sprite(p);
            if (spr >= 0 && built[c] < maxPer && np < MAX_PROJ) {
                if (vis[c]) {
                    pl[np].x = cx[c] + (int)(SLOTS[built[c]][0] * csz[c]);
                    pl[np].y = cy[c] + (int)(SLOTS[built[c]][1] * csz[c]);
                    pl[np].spr = spr;
                    pl[np].proj = p;
                    pl[np].size = csz[c];
                    pl[np].depth = cdepth[c] - SLOTS[built[c]][1] * 0.01;
                    np++;
                }
                built[c]++;
            } else extra[c]++;
        }
    /* дальние рисуются первыми */
    for (int i = 1; i < np; i++) {
        Placed v = pl[i];
        int j = i - 1;
        while (j >= 0 && pl[j].depth < v.depth) { pl[j + 1] = pl[j]; j--; }
        pl[j + 1] = v;
    }
    for (int i = 0; i < np; i++) {
        int hv = interactive && hover(A_PROJ, pl[i].proj);
        art_sprite_draw(pl[i].spr, pl[i].x, pl[i].y, pl[i].size, 255);
        if (hv) d_circle(pl[i].x, pl[i].y, pl[i].size / 3, CLR_INVALID, P.gold, 2);
        if (interactive) hit(pl[i].x - pl[i].size / 3, pl[i].y - pl[i].size * 2 / 3, pl[i].size * 2 / 3, pl[i].size * 2 / 3 + SC(4), A_PROJ, pl[i].proj);
    }

    /* города: подписи (без наложений), кольца, флажки заданий */
    int cap = game ? capital_of_era(G.era) : -1;
    RECT used[C_COUNT_];
    int nused = 0;
    for (int pass = 0; pass < 2; pass++)
    for (int c = 0; c < CITY_COUNT; c++) {
        if (!vis[c]) continue;
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
    if (game) citynet_map_overlay(pj, interactive, large);   /* городской транспорт (ui_city.c) */
    war_overlay(pj, interactive, large);
}

/* ---------- старая картинка Blender (если 3D-карты нет) ---------- */
static void map_draw_art(int x, int y, int w, int h, int interactive, int large)
{
    MapXf xf;
    if (!art_map_draw(x, y, w, h, &xf)) { map_draw_flat(x, y, w, h, interactive); return; }
    int size = (int)(xf.w * (large ? 0.050 : 0.078));
    if (size < SC(30)) size = SC(30);
    if (size > SC(large ? 116 : 76)) size = SC(large ? 116 : 76);
    MapProj pj = { &xf, 0, 0, 0, xf.x, xf.y, xf.w, xf.h, map3d_px_per_km() * xf.s, size };
    map_overlays(&pj, interactive, large);
}

/* ---------- интерактивная 3D-карта ---------- */
typedef struct { unsigned *px; int w, h, ss, cap; T3Cam cam; unsigned bg, extra; } MapCache;
static MapCache s_mc[2];                    /* 0 — панель, 1 — карта во весь экран */

int map3d_on(void) { return art_terrain_ready(); }

static void cam_init(void)
{
    if (!U.camInit) { t3d_cam_reset(&U.cam); U.camInit = 1; }
}

static void blit_rows(const unsigned *src, int w, int h, int x, int y)
{
    unsigned *dst = d_bits();
    int cx = 0, cy = 0, cw = g_w, ch = g_h;
    if (!d_clip_get(&cx, &cy, &cw, &ch)) { cx = 0; cy = 0; cw = g_w; ch = g_h; }
    int x0 = x > cx ? x : cx, y0 = y > cy ? y : cy;
    int x1 = x + w < cx + cw ? x + w : cx + cw, y1 = y + h < cy + ch ? y + h : cy + ch;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > g_w) x1 = g_w;
    if (y1 > g_h) y1 = g_h;
    if (x1 <= x0 || y1 <= y0) return;
    GdiFlush();
    for (int yy = y0; yy < y1; yy++)
        memcpy(dst + (size_t)yy * g_w + x0, src + (size_t)(yy - y) * w + (x0 - x), sizeof(unsigned) * (size_t)(x1 - x0));
}

static unsigned rgb_dib(COLORREF c) { return (unsigned)GetRValue(c) << 16 | (unsigned)GetGValue(c) << 8 | GetBValue(c); }

static void render_cached(int slot, const T3View *v, int x, int y, unsigned extra)
{
    MapCache *mc = &s_mc[slot];
    int w = v->w, h = v->h;
    int ss = U.mapFast || (double)w * h > 1.1e6 ? 1 : 2;     /* на очень больших окнах сглаживание дорого */
    unsigned bg = rgb_dib(mix(P.surface, P.bg, 120));
    if (!mc->px || mc->w != w || mc->h != h || mc->ss < ss || mc->bg != bg || mc->extra != extra || memcmp(&mc->cam, &U.cam, sizeof U.cam)) {
        if (w * h > mc->cap) {
            free(mc->px);
            mc->px = malloc(sizeof(unsigned) * (size_t)w * h);
            mc->cap = mc->px ? w * h : 0;
        }
        if (!mc->px) return;
        t3d_render(v, mc->px, w, bg, ss);
        mc->w = w; mc->h = h; mc->ss = ss; mc->bg = bg; mc->cam = U.cam; mc->extra = extra;
    }
    blit_rows(mc->px, w, h, x, y);
}

/* кнопки камеры: −, +, ‹, ›, сброс */
static void map_controls(int x, int y, int w, int h, int large)
{
    static const wchar_t *L[5] = { L"\x2212", L"+", L"\x2039", L"\x203A", L"\x2302" };
    static const int A[5] = { MC_ZOOM_OUT, MC_ZOOM_IN, MC_ROT_LEFT, MC_ROT_RIGHT, MC_RESET };
    int bs = SC(large ? 32 : 26), gap = SC(4), n = 5;
    int bx = x + w - n * (bs + gap) - SC(6), by = y + h - bs - SC(8);
    for (int i = 0; i < n; i++) {
        int px = bx + i * (bs + gap);
        int hv = hover(A_MAP_CAM, A[i]);
        d_rrect(px, by, bs, bs, SC(6), hv ? P.surface2 : mix(P.surface, P.bg, 60), P.line);
        if (A[i] == MC_RESET) {
            /* домик — «показать всю страну» */
            int cxp = px + bs / 2, cyp = by + bs / 2;
            POINT roof[4] = { { cxp - SC(7), cyp }, { cxp, cyp - SC(7) }, { cxp + SC(7), cyp }, { cxp - SC(7), cyp } };
            d_poly(roof, 4, P.ink, CLR_INVALID, 1);
            d_fill(cxp - SC(5), cyp, SC(10), SC(6), P.ink);
        } else d_text_c(px, by + (bs - d_font_h(F_H3)) / 2, bs, L[i], F_H3, P.ink);
        hit(px, by, bs, bs, A_MAP_CAM, A[i]);
    }
    if (large) {
        const wchar_t *hint = UI_TOUCH ? L"Палец — сдвинуть · щипок — ближе/дальше · два пальца — повернуть и наклонить · долгое нажатие — приказ армии"
                                       : L"Колесо — ближе/дальше · левая кнопка — повернуть и наклонить · правая — сдвинуть · стрелки, +/−, Home";
        int tw = d_text_w(hint, F_TINY);
        d_blend(x + SC(8), by + bs / 2 - d_font_h(F_TINY) / 2 - SC(3), tw + SC(12), d_font_h(F_TINY) + SC(6), P.surface, 170);
        d_text(x + SC(14), by + bs / 2 - d_font_h(F_TINY) / 2, hint, F_TINY, P.muted);
    }
}

/* полёт камеры (map_cam_fly_to) */
static struct { int on; T3Cam from, to; DWORD t0; int ms; void (*done)(void); } s_fly;

static double ease(double t) { return t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3 - 2 * t); }

static void fly_step(void)
{
    if (!s_fly.on) return;
    double t = (double)(GetTickCount() - s_fly.t0) / (s_fly.ms > 1 ? s_fly.ms : 1);
    double e = ease(t);
    /* приближение — по логарифму: равномерно «на глаз» */
    U.cam.tx = s_fly.from.tx + (s_fly.to.tx - s_fly.from.tx) * e;
    U.cam.ty = s_fly.from.ty + (s_fly.to.ty - s_fly.from.ty) * e;
    U.cam.zoom = exp(log(s_fly.from.zoom) + (log(s_fly.to.zoom) - log(s_fly.from.zoom)) * e);
    U.cam.pitch = s_fly.from.pitch + (s_fly.to.pitch - s_fly.from.pitch) * e;
    U.cam.yaw = s_fly.from.yaw + (s_fly.to.yaw - s_fly.from.yaw) * e;
    t3d_cam_clamp(&U.cam);
    if (t >= 1) {
        void (*done)(void) = s_fly.done;
        s_fly.on = 0;
        U.mapFast = 0;
        if (done) done();
        ui_invalidate();
    } else {
        U.mapFast = 1;
        ui_anim_request();
    }
}

void map_cam_fly_to(double lon, double lat, double zoom, double pitch, int ms, void (*done)(void))
{
    cam_init();
    s_fly.from = U.cam;
    s_fly.to = U.cam;
    t3d_lonlat(lon, lat, &s_fly.to.tx, &s_fly.to.ty);
    s_fly.to.zoom = zoom;
    s_fly.to.pitch = pitch;
    /* поворот — кратчайшим путём к «север сверху» */
    double y = fmod(U.cam.yaw, 360.0);
    if (y > 180) y -= 360;
    if (y < -180) y += 360;
    s_fly.from.yaw = y;
    s_fly.to.yaw = 0;
    t3d_cam_clamp(&s_fly.to);
    s_fly.t0 = GetTickCount();
    s_fly.ms = ms;
    s_fly.done = done;
    s_fly.on = 1;
    ui_anim_request();
    ui_invalidate();
}

int map_cam_flying(void) { return s_fly.on; }

/* игрок сам взялся за камеру — полёт сразу заканчивается (и его продолжение выполняется) */
static void fly_finish(void)
{
    if (s_fly.on) { s_fly.t0 = GetTickCount() - (DWORD)s_fly.ms - 1; fly_step(); }
}

static void map_draw_3d(int x, int y, int w, int h, int interactive, int large)
{
    cam_init();
    fly_step();
    T3View v;
    t3d_view(&U.cam, w, h, &v);
    unsigned key;
    s_nblobs = build_blobs(s_blobs, MAX_PROVINCES, &key);
    t3d_set_blobs(s_blobs, s_nblobs);
    key = key * 31u + states_prepare_3d();      /* земли государств (ui_states.c) */
    render_cached(large ? 1 : 0, &v, x, y, key);
    int size = (int)(w * (large ? 0.050 : 0.078));
    if (size < SC(30)) size = SC(30);
    if (size > SC(large ? 116 : 76)) size = SC(large ? 116 : 76);
    MapProj pj = { 0, &v, x, y, x, y, w, h, v.f / v.dist, size };
    int clipped = 0, ocx, ocy, ocw, och;
    if (!d_clip_get(&ocx, &ocy, &ocw, &och)) { d_clip(x, y, w, h); clipped = 1; }
    hits_clip(x, y, w, h);
    /* всё окно карты — место для вращения (только верхний слой: панель без окон поверх или карта во весь экран) */
    if (interactive && ((large && U.modal == M_MAP) || (!large && U.modal == M_NONE))) {
        U.mapVX = x; U.mapVY = y; U.mapVW = w; U.mapVH = h; U.mapVOn = 1;
    }
    map_overlays(&pj, interactive, large);
    if (interactive) map_controls(x, y, w, h, large);
    hits_unclip();
    if (clipped) d_unclip();
}

void map_draw_ex(int x, int y, int w, int h, int interactive, int large)
{
    if (w < 40 || h < 40) return;
    if (!large && (U.modal == M_MAP || U.modal == M_LAUNCH) && map3d_on()) { d_fill(x, y, w, h, P.surface); return; }   /* под картой или заставкой во весь экран — не видно */
    if (map3d_on()) map_draw_3d(x, y, w, h, interactive, large);
    else map_draw_art(x, y, w, h, interactive, large);
}

void map_draw(int x, int y, int w, int h, int interactive) { map_draw_ex(x, y, w, h, interactive, 0); }

/* ---------- управление камерой ---------- */
int map_cam_hit(int x, int y)
{
    return U.mapVOn && map3d_on() && x >= U.mapVX && x < U.mapVX + U.mapVW && y >= U.mapVY && y < U.mapVY + U.mapVH;
}

void map_cam_rotate(double dyaw, double dpitch)
{
    cam_init();
    fly_finish();
    U.cam.yaw += dyaw;
    U.cam.pitch += dpitch;
    t3d_cam_clamp(&U.cam);
}

void map_cam_pan(int fromX, int fromY, int toX, int toY)
{
    cam_init();
    fly_finish();
    if (!U.mapVOn) return;
    T3View v;
    t3d_view(&U.cam, U.mapVW, U.mapVH, &v);
    double ax, ay, bx, by;
    if (!t3d_unproject(&v, fromX - U.mapVX, fromY - U.mapVY, 9.0, &ax, &ay)) return;
    if (!t3d_unproject(&v, toX - U.mapVX, toY - U.mapVY, 9.0, &bx, &by)) return;
    /* у самого горизонта сдвиг огромен — ограничиваем шаг */
    double dx = bx - ax, dy = by - ay, d = sqrt(dx * dx + dy * dy), lim = 120.0 / (U.cam.zoom > 0.3 ? U.cam.zoom : 0.3);
    if (d > lim) { bx = ax + dx * lim / d; by = ay + dy * lim / d; }
    U.cam.tx -= bx - ax;
    U.cam.ty -= by - ay;
    t3d_cam_clamp(&U.cam);
}

void map_cam_zoom_at(double factor, int sx, int sy)
{
    cam_init();
    fly_finish();
    if (!U.mapVOn) { U.cam.zoom *= factor; t3d_cam_clamp(&U.cam); return; }
    T3View v;
    t3d_view(&U.cam, U.mapVW, U.mapVH, &v);
    double ax, ay, bx, by;
    int ok = t3d_unproject(&v, sx - U.mapVX, sy - U.mapVY, 9.0, &ax, &ay);
    double bx0, by0, bx1, by1;
    t3d_bounds(&bx0, &by0, &bx1, &by1);
    if (ok && (ax < bx0 - 300 || ax > bx1 + 300 || ay < by0 - 300 || ay > by1 + 300)) ok = 0;   /* у горизонта — без привязки */
    U.cam.zoom *= factor;
    t3d_cam_clamp(&U.cam);
    /* точка под курсором остаётся под курсором */
    t3d_view(&U.cam, U.mapVW, U.mapVH, &v);
    if (ok && t3d_unproject(&v, sx - U.mapVX, sy - U.mapVY, 9.0, &bx, &by)) {
        U.cam.tx += ax - bx;
        U.cam.ty += ay - by;
        t3d_cam_clamp(&U.cam);
    }
}

void map_cam_action(int a)
{
    cam_init();
    fly_finish();
    switch (a) {
    case MC_ZOOM_IN:  U.cam.zoom *= 1.35; break;
    case MC_ZOOM_OUT: U.cam.zoom /= 1.35; break;
    case MC_ROT_LEFT: U.cam.yaw -= 30; break;
    case MC_ROT_RIGHT: U.cam.yaw += 30; break;
    case MC_TILT_UP:  U.cam.pitch += 8; break;
    case MC_TILT_DOWN: U.cam.pitch -= 8; break;
    case MC_RESET:    t3d_cam_reset(&U.cam); break;
    }
    t3d_cam_clamp(&U.cam);
}
