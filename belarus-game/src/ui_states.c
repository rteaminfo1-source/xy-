/* Исторические государства на карте (states.h): земли года для 3D-карты, подписи государств и соседей
 * в духе школьного атласа, обводки на плоской карте и на готовом рендере Blender, сцены 50–59.
 * На 3D-карте сами земли рисует terrain3d (плоскость под плитой Беларуси); здесь — только подписи. */
#include "mapview.h"
#include "states.h"
#include "war.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int s_year = -1;              /* год, чьи земли сейчас в terrain3d (0 — без земель) */

static int map_year(void) { return U.screen == SCR_GAME ? G.year : 0; }

unsigned states_prepare_3d(void)
{
    int y = map_year();
    if (y != s_year) {               /* сетка поиска земель строится заново только при смене года */
        T3Area a[T3_MAX_AREAS];
        double x0, y0, x1, y1;
        hist_t3_board(&x0, &y0, &x1, &y1);
        t3d_set_areas(a, y ? hist_t3_areas(y, a, T3_MAX_AREAS) : 0, x0, y0, x1, y1);
        s_year = y;
    }
    return (unsigned)(s_year + 1) * 2654435761u;
}

/* ---------- подписи ---------- */
static COLORREF cref(unsigned rgb, int k255)      /* 0xRRGGBB -> COLORREF, k255 — яркость */
{
    return RGB((rgb >> 16 & 255) * k255 / 255, (rgb >> 8 & 255) * k255 / 255, (rgb & 255) * k255 / 255);
}

/* текст вразрядку (sp — пикселей между буквами); draw = 0 — только ширина */
static int spaced(int x, int y, const wchar_t *s, int n, int f, int sp, COLORREF c, int draw)
{
    wchar_t ch[2] = { 0, 0 };
    int x0 = x;
    for (int i = 0; i < n; i++) {
        ch[0] = s[i];
        if (draw) d_text(x, y, ch, f, c);
        x += d_text_w(ch, f) + sp;
    }
    return n ? x - x0 - sp : 0;
}

typedef struct { int x, y, w, h; } LRect;

static int free_place(const LRect *r, int n, LRect q)
{
    for (int i = 0; i < n; i++)
        if (q.x < r[i].x + r[i].w && r[i].x < q.x + q.w && q.y < r[i].y + r[i].h && r[i].y < q.y + q.h) return 0;
    return 1;
}

/* Раскладка подписи шрифтом f: одна строка или две (перенос по пробелу ближе к середине). */
typedef struct { int f, sp, lines, cut, w1, w2, w; } Lay;

static Lay layout(const wchar_t *buf, int len, int f, int sp, int maxW)
{
    Lay L = { f, sp, 1, len, 0, 0, 0 };
    L.w1 = L.w = spaced(0, 0, buf, len, f, sp, 0, 0);
    if (L.w <= maxW || !wcschr(buf, L' ')) return L;
    int best = -1;
    for (int i = 1; i < len - 1; i++)
        if (buf[i] == L' ' && (best < 0 || abs(i - len / 2) < abs(best - len / 2))) best = i;
    L.lines = 2;
    L.cut = best;
    L.w1 = spaced(0, 0, buf, best, f, sp, 0, 0);
    L.w2 = spaced(0, 0, buf + best + 1, len - best - 1, f, sp, 0, 0);
    L.w = L.w1 > L.w2 ? L.w1 : L.w2;
    return L;
}

/* точка (lon, lat) внутри земли (правило чёт-нечет) */
static int area_has(const HistArea *a, double lon, double lat)
{
    int n = a->n ? a->n : BORDER_N, in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = a->n ? a->pts[i][0] : BORDER[i].lon, yi = a->n ? a->pts[i][1] : BORDER[i].lat;
        double xj = a->n ? a->pts[j][0] : BORDER[j].lon, yj = a->n ? a->pts[j][1] : BORDER[j].lat;
        if ((yi > lat) != (yj > lat) && lon < xi + (lat - yi) * (xj - xi) / (yj - yi)) in = !in;
    }
    return in;
}

static int label_point(const MapProj *pj, double lon, double lat, int *sx, int *sy);

/* Места для подписи: сама точка подписи и точки вокруг неё (кольцами), лежащие внутри земли. */
#define CAND_MAX 41
static int label_cands(const MapProj *pj, const HistArea *a, POINT *c)
{
    static const float R[5] = { 0.35f, 0.7f, 1.1f, 1.6f, 2.2f };
    static const signed char DIR[8][2] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 }, { 1, 1 }, { -1, 1 }, { 1, -1 }, { -1, -1 } };
    int n = 0, sx, sy;
    if (label_point(pj, a->labelLon, a->labelLat, &sx, &sy)) c[n++] = (POINT){ sx, sy };
    for (int r = 0; r < 5; r++)
        for (int d = 0; d < 8; d++) {
            double lon = a->labelLon + DIR[d][0] * R[r] * (DIR[d][1] ? 1.2 : 1.7), lat = a->labelLat + DIR[d][1] * R[r] * (DIR[d][0] ? 0.7 : 1.0);
            if (area_has(a, lon, lat) && label_point(pj, lon, lat, &sx, &sy)) c[n++] = (POINT){ sx, sy };
        }
    return n;
}

/* годы земли под властью соседа: до года присоединения (Западная Беларусь — 1921–1939), «с 1772 г.»
 * или, если земля прирастала (второй раздел), «с 1772 и 1793 гг.» */
static void foreign_years(const HistArea *a, wchar_t *s, int n)
{
    int f, t;
    hist_area_years(a, &f, &t);
    if (t >= 1900 && t - f < 40) swprintf(s, (size_t)n, L"%d\x2013%d", f, t + 1);
    else if (f < a->from) swprintf(s, (size_t)n, L"с %d и %d гг.", f, a->from);
    else swprintf(s, (size_t)n, L"с %d г.", f);
}

/* Подпись земли в одном из мест c[0..nc) (первое свободное); span — ширина земли на экране, (x0, y0, x1, y1) —
 * окно карты. Государство и белорусская часть — прописными вразрядку шрифтом эпохи, шрифт — самый крупный,
 * что помещается в землю; под властью соседа — с годами. Занятые места — r[*nr]. 0 — места не нашлось. */
static int draw_label(const HistArea *a, const POINT *c, int nc, int span, int large, int x0, int y0, int x1, int y1,
                      LRect *r, int *nr, int rmax)
{
    static const int BIG[] = { F_ERA_L, F_ERA_M, F_ERA_S, F_SMALL_B, F_TINY }, SMALL[] = { F_SMALL_B, F_SMALL, F_TINY };
    wchar_t buf[96];
    int big = a->role == HA_STATE || a->role == HA_PART;
    wcsncpy(buf, a->name, 95);
    buf[95] = 0;
    if (big) CharUpperBuffW(buf, (DWORD)wcslen(buf));
    int len = (int)wcslen(buf);
    const int *fs = big ? BIG : SMALL;
    int nf = big ? 5 : 3, first = a->role == HA_STATE ? (large ? 0 : 1) : a->role == HA_PART ? (large ? 1 : 2)
              : a->role == HA_NEIGHBOR ? (large ? 1 : 2) : a->role == HA_SUB ? (large ? 0 : 1) : 0;
    int maxW = big ? span * 85 / 100 : span;
    wchar_t sub[48] = L"";
    if (a->role == HA_FOREIGN) foreign_years(a, sub, 48);
    int sh = sub[0] ? d_font_h(F_TINY) : 0, sw = sub[0] ? d_text_w(sub, F_TINY) : 0;
    Lay L = { 0 };
    int px = 0, py = 0, w = 0, h = 0, lh = 0, ok = 0;
    for (int i = first; i < nf && !ok; i++) {
        int sp = big && fs[i] != F_TINY ? (d_font_h(fs[i]) + 6) / 9 : 0;
        L = layout(buf, len, fs[i], sp, maxW);
        if (L.w > maxW && (i < nf - 1 || a->role == HA_NEIGHBOR || a->role == HA_SUB)) continue;
        lh = d_font_h(L.f) * 92 / 100;
        w = L.w > sw ? L.w : sw;
        h = lh * L.lines + sh;
        for (int k = 0; k < nc && !ok; k++) {
            LRect q = { c[k].x - w / 2 - SC(4), c[k].y - h / 2 - SC(2), w + SC(8), h + SC(4) };
            if (q.x < x0 || q.x + q.w > x1 || q.y < y0 || q.y + q.h > y1 || !free_place(r, *nr, q)) continue;
            if (*nr < rmax) r[(*nr)++] = q;
            px = c[k].x; py = c[k].y; ok = 1;
        }
    }
    if (!ok) return 0;

    COLORREF ink = cref(a->color, a->role == HA_NEIGHBOR ? 92 : 70), halo = mix(cref(a->color, 255), RGB(255, 255, 255), 150);
    if (a->role == HA_NEIGHBOR) ink = mix(ink, RGB(70, 64, 58), 120);
    int y = py - h / 2;
    for (int l = 0; l < L.lines; l++) {
        const wchar_t *s = l ? buf + L.cut + 1 : buf;
        int n = L.lines == 1 ? len : l ? len - L.cut - 1 : L.cut, lw = l ? L.w2 : L.w1, x = px - lw / 2;
        if (big) {
            for (int d = 0; d < 4; d++) spaced(x + (d & 1 ? 1 : -1), y + (d & 2 ? 1 : -1), s, n, L.f, L.sp, halo, 1);
            spaced(x, y, s, n, L.f, L.sp, ink, 1);
        } else {
            wchar_t t[96];
            wcsncpy(t, s, (size_t)n);
            t[n] = 0;
            d_text(x + 1, y + 1, t, L.f, halo);
            d_text(x, y, t, L.f, ink);
        }
        y += lh;
    }
    if (sub[0]) {
        d_text(px - sw / 2 + 1, y + 1, sub, F_TINY, halo);
        d_text(px - sw / 2, y, sub, F_TINY, ink);
    }
    return 1;
}

/* ширина земли на экране (в пределах окна карты) */
static int area_span(const MapProj *pj, const HistArea *a, int x0, int x1)
{
    int n = a->n ? a->n : BORDER_N, lo = x1, hi = x0;
    const T3View *v = mapv_view(pj);
    for (int i = 0; i < n; i++) {
        double lon = a->n ? a->pts[i][0] : BORDER[i].lon, lat = a->n ? a->pts[i][1] : BORDER[i].lat, k;
        int sx, sy, ok;
        if (v) {
            double X, Y;
            t3d_lonlat(lon, lat, &X, &Y);
            ok = mapv_xyz(pj, X, Y, a->n ? T3_AREA_Z : 6.0, &sx, &sy, &k);
        } else ok = mapv_ll(pj, lon, lat, 0, &sx, &sy, &k);
        if (!ok) continue;
        if (sx < lo) lo = sx;
        if (sx > hi) hi = sx;
    }
    if (lo < x0) lo = x0;
    if (hi > x1) hi = x1;
    return hi > lo ? hi - lo : 0;
}

/* точка подписи на экране: над плитой Беларуси — на рельефе, иначе — на плоскости земель */
static int label_point(const MapProj *pj, double lon, double lat, int *sx, int *sy)
{
    double k;
    if (!mapv_view(pj)) return mapv_ll(pj, lon, lat, 0, sx, sy, &k);
    double X, Y;
    t3d_lonlat(lon, lat, &X, &Y);
    double Z = t3d_inside(X, Y) ? t3d_height(X, Y) + 0.3 : T3_AREA_Z;
    return mapv_xyz(pj, X, Y, Z, sx, sy, &k);
}

/* ---------- обводки на 2D-картах ---------- */
static int area_pts(const MapProj *pj, const HistArea *a, POINT *p, int *ok)
{
    int n = a->n ? a->n : BORDER_N;
    if (n > 128) n = 128;
    for (int i = 0; i < n; i++) {
        double lon = a->n ? a->pts[i][0] : BORDER[i].lon, lat = a->n ? a->pts[i][1] : BORDER[i].lat, k;
        int x = 0, y = 0;
        ok[i] = mapv_ll(pj, lon, lat, 0, &x, &y, &k);
        p[i].x = x; p[i].y = y;
    }
    return n;
}

static void outline_2d(const MapProj *pj, const HistArea *a, int flat, HRGN outside)
{
    POINT p[128];
    int ok[128], n = area_pts(pj, a, p, ok), all = 1;
    for (int i = 0; i < n; i++) all &= ok[i];
    COLORREF line = cref(a->color, a->role == HA_NEIGHBOR ? 150 : 110);
    if (flat && all && outside && a->n) {
        /* лёгкая штриховка — только вне Беларуси, сама страна на плоской карте остаётся чистой */
        HDC dc = d_dc();
        int saved = SaveDC(dc);
        ExtSelectClipRgn(dc, outside, RGN_AND);
        HBRUSH br = CreateHatchBrush(a->role == HA_FOREIGN ? HS_DIAGCROSS : HS_BDIAGONAL, cref(a->color, a->role == HA_NEIGHBOR ? 210 : 175));
        HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, GetStockObject(NULL_PEN));
        int bk = SetBkMode(dc, TRANSPARENT);
        Polygon(dc, p, n);
        SetBkMode(dc, bk);
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(br);
        RestoreDC(dc, saved);
    }
    int t = a->role == HA_NEIGHBOR || a->role == HA_SUB ? 1 : SC(2) > 1 ? SC(2) : 2;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        if (ok[i] && ok[j]) d_line(p[i].x, p[i].y, p[j].x, p[j].y, line, t);
    }
}

void states_overlay(const MapProj *pj, int interactive, int large)
{
    int year = map_year();
    if (!year) return;
    int idx[T3_MAX_AREAS], n = hist_areas_at(year, idx, T3_MAX_AREAS);
    int x, y, w, h;
    mapv_rect(pj, &x, &y, &w, &h);
    int ocx, ocy, ocw, och, clipped = 0;
    if (!d_clip_get(&ocx, &ocy, &ocw, &och)) { d_clip(x, y, w, h); clipped = 1; }
    const T3View *v = mapv_view(pj);

    if (!v) {
        /* плоская карта (подложка земли) — со штриховкой соседей; рендер Blender — только линии */
        int flat = mapv_size(pj) == SC(20);      /* у плоской карты базовый размер моделей — SC(20) */
        HRGN outside = 0;
        if (flat) {
            POINT bp[128];
            int ok[128];
            HistArea modern = { L"", HA_STATE, 0, 0, 0, 0, 0, 0, { { 0, 0 } } };
            int bn = area_pts(pj, &modern, bp, ok);
            HRGN all = CreateRectRgn(x, y, x + w, y + h), by = CreatePolygonRgn(bp, bn, WINDING);
            outside = CreateRectRgn(0, 0, 0, 0);
            CombineRgn(outside, all, by, RGN_DIFF);
            DeleteObject(all);
            DeleteObject(by);
        }
        for (int k = 0; k < n; k++) outline_2d(pj, &HIST_AREAS[idx[k]], flat, outside);
        if (outside) DeleteObject(outside);
    }

    /* подписи: сначала главное — государство, его белорусская часть, земли под властью соседа.
     * Места подписей городов (их рисует ui_map.c поверх) заняты заранее — подпись земли их обходит. */
    static const int ORDER[5] = { HA_STATE, HA_PART, HA_FOREIGN, HA_SUB, HA_NEIGHBOR };
    enum { RMAX = 32 + C_COUNT_ };
    LRect r[RMAX];
    int nr = 0;
    /* кнопки камеры и подсказку внизу не закрываем */
    r[nr++] = large ? (LRect){ x, y + h - SC(52), w, SC(52) } : (LRect){ x + w - SC(200), y + h - SC(52), SC(200), SC(52) };
    for (int c = 0; c < CITY_COUNT && nr < RMAX; c++) {
        int sx, sy;
        double k;
        if (!mapv_city(pj, c, &sx, &sy, &k)) continue;
        int f = CITIES[c].major ? F_SMALL_B : F_TINY, tw = d_text_w(CITIES[c].name, f), th = d_font_h(f);
        r[nr++] = (LRect){ sx - tw / 2 - SC(8), sy - SC(16) - th, tw + SC(16), th + SC(28) };
    }
    /* и подписи земель для похода (их рисует ui_map.c над точкой земли) */
    for (int p = 0; p < PROVINCE_COUNT && nr < RMAX; p++) {
        int st = war_province_state(&G, p), sx, sy;
        double k;
        if ((st != PV_OWNED && st != PV_AVAILABLE && !(G.warKind == WAR_CONQUEST && G.warTarget == p)) ||
            !mapv_ll(pj, PROVINCES[p].lon, PROVINCES[p].lat, 0.5, &sx, &sy, &k)) continue;
        wchar_t lab[96];
        swprintf(lab, 96, large ? L"%ls · можно в поход" : L"%ls", PROVINCES[p].name);
        int f = large ? F_SMALL_B : F_TINY, tw = d_text_w(lab, f), th = d_font_h(f);
        r[nr++] = (LRect){ sx - tw / 2 - SC(14), sy - SC(18) - th, tw + SC(32), th + SC(30) };
    }
    const HistArea *missed[4];
    int nmissed = 0, stateShown = 0;
    for (int o = 0; o < 5; o++)
        for (int k = 0; k < n; k++) {
            const HistArea *a = &HIST_AREAS[idx[k]];
            if (a->role != ORDER[o] || (a->labelLon == 0 && a->labelLat == 0)) continue;
            POINT cand[CAND_MAX];
            int nc = label_cands(pj, a, cand);
            int shown = nc && draw_label(a, cand, nc, area_span(pj, a, x, x + w), large, x + SC(6), y + SC(4), x + w - SC(6), y + h - SC(4), r, &nr, RMAX);
            if (a->role == HA_STATE) stateShown |= shown;
            if (!shown && (a->role == HA_STATE || a->role == HA_PART || a->role == HA_FOREIGN) && nmissed < 4) missed[nmissed++] = a;
        }
    for (int k = 0; k < HIST_WATER_COUNT; k++) {
        const HistArea *a = &HIST_WATER[k];
        int sx, sy, tw = d_text_w(a->name, F_SMALL);
        if (a->labelLon == 0 || !label_point(pj, a->labelLon, a->labelLat, &sx, &sy)) continue;
        LRect q = { sx - tw / 2, sy - SC(8), tw, SC(16) };
        if (q.x < x || q.x + q.w > x + w || !free_place(r, nr, q)) continue;
        d_text(sx - tw / 2 + 1, sy - d_font_h(F_SMALL) / 2 + 1, a->name, F_SMALL, RGB(225, 236, 245));
        d_text(sx - tw / 2, sy - d_font_h(F_SMALL) / 2, a->name, F_SMALL, cref(a->color, 105));
    }
    /* название государства и части без места на карте — легендой в углу: образец цвета и подпись */
    if (!stateShown || nmissed) {
        /* несколько княжеств без места — каждое своей строкой; одно государство — его названием на год */
        const HistArea *leg[5];
        int nl = 0, th = d_font_h(F_SMALL_B), sw = th * 3 / 4, lw = 0, nstates = 0;
        for (int k = 0; k < nmissed; k++) nstates += missed[k]->role == HA_STATE;
        HistArea st = { hist_state_name(year), HA_STATE, year, year, 0, 0, 0, 0, { { 0, 0 } } };
        for (int k = 0; k < n; k++)
            if (HIST_AREAS[idx[k]].role == HA_STATE) { st.color = HIST_AREAS[idx[k]].color; break; }
        if (!stateShown && nstates > 1) nstates = 0, leg[nl++] = &st;     /* ни одно княжество не видно — общее название */
        for (int k = 0; k < nmissed; k++)
            if (missed[k]->role != HA_STATE || nstates) leg[nl++] = missed[k];
        if (!nl) leg[nl++] = &st;
        wchar_t txt[5][96];
        for (int k = 0; k < nl; k++) {
            const HistArea *a = leg[k];
            wchar_t yr[48];
            if (a->role == HA_FOREIGN) foreign_years(a, yr, 48);
            if (a->role == HA_FOREIGN) swprintf(txt[k], 96, L"%ls (%ls)", a->name, yr);
            else swprintf(txt[k], 96, L"%ls", a->name);
            int tw = d_text_w(txt[k], F_SMALL_B);
            if (tw > lw) lw = tw;
        }
        int bx = x + SC(8), by = y + SC(8), lh = th + SC(4);
        d_blend(bx, by, lw + sw + SC(26), nl * lh + SC(8), P.surface, 205);
        for (int k = 0; k < nl; k++) {
            int ty = by + SC(4) + k * lh, qx = bx + SC(8), qy = ty + (th - sw) / 2;
            d_fill(qx, qy, sw, sw, cref(leg[k]->color, 255));
            if (leg[k]->role == HA_FOREIGN)              /* штриховка, как на карте */
                for (int t = SC(4); t < sw * 2; t += SC(5)) {
                    int x0 = qx + (t < sw ? 0 : t - sw), y0 = qy + (t < sw ? t : sw), x1 = qx + (t < sw ? t : sw), y1 = qy + (t < sw ? 0 : t - sw);
                    d_line(x0, y0, x1, y1, cref(leg[k]->color, 150), 1);
                }
            d_frame(qx, qy, sw, sw, cref(leg[k]->color, 120));
            d_text(qx + sw + SC(8), ty, txt[k], F_SMALL_B, P.ink);
        }
    }
    (void)interactive;
    if (clipped) d_unclip();
}

/* ---------- сцены разработчика 50–59 ---------- */
static void scene(int year, double lon, double lat, double zoom, double pitch)
{
    dev_play_until(year);
    dev_resolve();
    U.modal = M_MAP;
    t3d_cam_reset(&U.cam);
    U.camInit = 1;
    t3d_lonlat(lon, lat, &U.cam.tx, &U.cam.ty);
    U.cam.zoom = zoom; U.cam.pitch = pitch; U.cam.yaw = 0;
    t3d_cam_clamp(&U.cam);
}

void states_dev_scene(int n)
{
    switch (n) {
    case 50: scene(1150, 27.6, 53.9, 0.62, 56); break;     /* Полоцкое, Турово-Пинское и Городенское княжества */
    case 51: scene(1400, 29.0, 52.6, 0.45, 54); break;     /* ВКЛ при Витовте — до Чёрного моря */
    case 52: scene(1610, 26.4, 53.2, 0.48, 56); break;     /* Речь Посполитая и ВКЛ в ней */
    case 53: scene(1772, 27.4, 53.9, 0.58, 56); break;     /* после первого раздела: восток — Российская империя */
    case 54: scene(1845, 27.8, 53.6, 0.52, 56); break;     /* Российская империя, Царство Польское */
    case 55: scene(1929, 27.2, 53.7, 0.70, 54); break;     /* БССР и Западная Беларусь в составе Польши */
    case 56: scene(1942, 27.8, 53.6, 0.62, 56); break;     /* 1942: СССР, оккупация */
    case 57: scene(2000, 27.8, 53.7, 0.62, 56); break;     /* Республика Беларусь и соседи */
    case 58: scene(1610, 26.4, 53.2, 0.48, 56); ui_toggle_theme(); break;   /* тёмная тема */
    case 59: scene(1917, 27.0, 53.8, 0.60, 56); U.modal = M_NONE; break;   /* карта на панели: фронт 1915–1917 */
    }
}
