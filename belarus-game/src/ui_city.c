/* Городской транспорт в интерфейсе: схема метро, трамвая или троллейбуса во весь экран (M_CITYNET),
 * значки сетей у городов на карте, блок в окне города и заставка 1984 года — камера летит к Минску,
 * и игрок сам прокладывает первые перегоны метро. Правила стройки и доход — в citynet.c.
 * Линии, станции, река и поезда рисуются со сглаживанием прямо в пиксели заднего буфера. */
#include "mapview.h"
#include "citynet.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---------- состояние окна ---------- */
static int s_back = M_NONE, s_backArg;     /* куда вернуться после схемы (окно города или игра) */
static int s_sel = -1;                     /* выбранная станция (узел) или -1 */
static int s_selEdge = -1;                 /* выбранный перегон или -1 */
static DWORD s_introT0;                    /* вступление после заставки 1984 года; 0 — не показывать */
static int s_introNet = -1;
static int s_cut;                          /* камера летит к Минску */
static int s_flyMs = 2500;                 /* длительность полёта (сцена разработчика замедляет) */
static double s_devT = -1;                 /* сцены разработчика: время анимации поездов */
static int s_devHover = -1;                /* сцены разработчика: перегон «под курсором» */

#define INTRO_MS 16000

static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
static COLORREF cr(unsigned x) { return RGB((x >> 16) & 255, (x >> 8) & 255, x & 255); }
static double anim_t(void) { return s_devT >= 0 ? s_devT : GetTickCount() / 1000.0; }

/* число с запятой: «2,4»; целые и большие — без дробной части */
static void fmt1(double v, wchar_t *buf, int n)
{
    if (fabs(v) >= 10 || fabs(v - lround(v)) < 0.05) swprintf(buf, n, L"%ld", lround(v));
    else {
        swprintf(buf, n, L"%.1f", v);
        for (wchar_t *p = buf; *p; p++) if (*p == L'.') *p = L',';
    }
}

/* «8 станций», «1 станция», «3 станции» */
static const wchar_t *plural(int k, const wchar_t *one, const wchar_t *few, const wchar_t *many)
{
    int m10 = k % 10, m100 = k % 100;
    if (m10 == 1 && m100 != 11) return one;
    if (m10 >= 2 && m10 <= 4 && (m100 < 12 || m100 > 14)) return few;
    return many;
}

/* доход сети за ход одной строкой: «+28 казны · +2,4 стабильности · +1,6 культуры» */
static void income_text(int n, wchar_t *buf, int len)
{
    double inc[R_COUNT];
    citynet_net_income(&G, n, inc);
    static const int RS[4] = { R_GOLD, R_IND, R_STAB, R_CULT };
    static const wchar_t *RN[4] = { L"казны", L"производства", L"стабильности", L"культуры" };
    buf[0] = 0;
    for (int i = 0; i < 4; i++) {
        if (inc[RS[i]] < 0.05) continue;
        wchar_t v[24];
        fmt1(inc[RS[i]], v, 24);
        int l = (int)wcslen(buf);
        swprintf(buf + l, len - l, L"%ls+%ls %ls", l ? L" · " : L"", v, RN[i]);
    }
}

/* ---------- пиксели заднего буфера: сглаженные линии, кольца, пунктир ---------- */
typedef struct { unsigned *px; int stride, x0, y0, x1, y1; } Cv;   /* x1, y1 — не включительно */

static int cv_get(Cv *c, int x, int y, int w, int h)
{
    GdiFlush();                       /* сначала GDI дорисовывает всё, что держит в очереди */
    c->px = d_bits();
    c->stride = g_w;
    c->x0 = x > 0 ? x : 0;
    c->y0 = y > 0 ? y : 0;
    c->x1 = x + w < g_w ? x + w : g_w;
    c->y1 = y + h < g_h ? y + h : g_h;
    return c->px && c->x1 > c->x0 && c->y1 > c->y0;
}

static void px_blend(unsigned *p, int r, int g, int b, int a)
{
    if (a <= 0) return;
    if (a >= 255) { *p = ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b; return; }
    unsigned d = *p;
    int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
    dr += ((r - dr) * a) / 255; dg += ((g - dg) * a) / 255; db += ((b - db) * a) / 255;
    *p = ((unsigned)dr << 16) | ((unsigned)dg << 8) | (unsigned)db;
}

/* отрезок толщиной 2r с круглыми концами (r — полутолщина) */
static void aa_seg(const Cv *c, double x0, double y0, double x1, double y1, double r, COLORREF col, int alpha)
{
    if (alpha <= 0 || r <= 0) return;
    int xa = (int)floor((x0 < x1 ? x0 : x1) - r - 1), xb = (int)ceil((x0 > x1 ? x0 : x1) + r + 1);
    int ya = (int)floor((y0 < y1 ? y0 : y1) - r - 1), yb = (int)ceil((y0 > y1 ? y0 : y1) + r + 1);
    if (xa < c->x0) xa = c->x0;
    if (ya < c->y0) ya = c->y0;
    if (xb > c->x1) xb = c->x1;
    if (yb > c->y1) yb = c->y1;
    if (xa >= xb || ya >= yb) return;
    double dx = x1 - x0, dy = y1 - y0, l2 = dx * dx + dy * dy, il2 = l2 > 1e-9 ? 1.0 / l2 : 0;
    double ro = r + 0.5, ro2 = ro * ro, ri = r - 0.5, ri2 = ri > 0 ? ri * ri : 0;
    int cr_ = GetRValue(col), cg = GetGValue(col), cb = GetBValue(col);
    for (int y = ya; y < yb; y++) {
        unsigned *row = c->px + (size_t)y * c->stride;
        double py = y + 0.5 - y0;
        for (int x = xa; x < xb; x++) {
            double px = x + 0.5 - x0;
            double t = (px * dx + py * dy) * il2;
            t = t < 0 ? 0 : t > 1 ? 1 : t;
            double qx = px - t * dx, qy = py - t * dy, d2 = qx * qx + qy * qy;
            if (d2 >= ro2) continue;
            int a = d2 > ri2 ? (int)(alpha * (ro - sqrt(d2))) : alpha;
            px_blend(row + x, cr_, cg, cb, a);
        }
    }
}

static void aa_disc(const Cv *c, double x, double y, double r, COLORREF col, int alpha) { aa_seg(c, x, y, x, y, r, col, alpha); }

/* кольцо радиуса R толщиной t */
static void aa_ring(const Cv *c, double cx, double cy, double R, double t, COLORREF col, int alpha)
{
    double ext = R + t / 2 + 1;
    int xa = (int)floor(cx - ext), xb = (int)ceil(cx + ext), ya = (int)floor(cy - ext), yb = (int)ceil(cy + ext);
    if (xa < c->x0) xa = c->x0;
    if (ya < c->y0) ya = c->y0;
    if (xb > c->x1) xb = c->x1;
    if (yb > c->y1) yb = c->y1;
    int cr_ = GetRValue(col), cg = GetGValue(col), cb = GetBValue(col);
    for (int y = ya; y < yb; y++) {
        unsigned *row = c->px + (size_t)y * c->stride;
        double py = y + 0.5 - cy;
        for (int x = xa; x < xb; x++) {
            double px = x + 0.5 - cx, e = fabs(sqrt(px * px + py * py) - R), cov = t / 2 + 0.5 - e;
            if (cov <= 0) continue;
            px_blend(row + x, cr_, cg, cb, (int)(alpha * (cov > 1 ? 1 : cov)));
        }
    }
}

/* пунктир: штрих dash, промежуток gap */
static void aa_dash(const Cv *c, double x0, double y0, double x1, double y1, double r, COLORREF col, int alpha, double dash, double gap)
{
    double dx = x1 - x0, dy = y1 - y0, L = sqrt(dx * dx + dy * dy);
    if (L < 1) return;
    double ux = dx / L, uy = dy / L;
    double step = dash + gap, s0 = fmod(L - dash, step) / 2;   /* штрихи — симметрично от концов */
    for (double s = s0 - step; s < L; s += step) {
        double a = s < 0 ? 0 : s, b = s + dash > L ? L : s + dash;
        if (b - a < 0.5) continue;
        aa_seg(c, x0 + ux * a, y0 + uy * a, x0 + ux * b, y0 + uy * b, r, col, alpha);
    }
}

/* ---------- геометрия схемы ---------- */
typedef struct { int x, y, w, h, padX, padY; } Box;

static void node_xy(const Box *b, const NetNode *v, double *x, double *y)
{
    *x = b->x + b->padX + v->x * (b->w - 2 * b->padX);
    *y = b->y + b->padY + v->y * (b->h - 2 * b->padY);
}

/* цвет узла — цвет его линии (первого перегона, который к нему ведёт) */
static int node_line(const CityNetDef *d, int v)
{
    for (int e = 0; e < d->nedges; e++) if (d->edges[e].a == v || d->edges[e].b == v) return d->edges[e].line;
    return 0;
}

static int node_xfer(const CityNetDef *d, int v)
{
    for (int k = 0; k < d->nxfer; k++) if (d->xfer[k][0] == v || d->xfer[k][1] == v) return 1;
    return 0;
}

static int st_r(const CityNetDef *d) { return d->type == NT_METRO ? SC(8) : SC(7); }

/* подписи станций: раскладка без наложений (считается один раз для сети и размера окна) */
static struct { int net, w, h, scale; RECT r[NET_MAX_NODES]; } s_lab = { -1, 0, 0, 0, { { 0, 0, 0, 0 } } };

static int rect_hits_seg(const RECT *r, double x0, double y0, double x1, double y1)
{
    double L = hypot(x1 - x0, y1 - y0);
    int n = (int)(L / SC(5)) + 2;
    for (int i = 0; i <= n; i++) {
        double t = (double)i / n, x = x0 + (x1 - x0) * t, y = y0 + (y1 - y0) * t;
        if (x >= r->left && x <= r->right && y >= r->top && y <= r->bottom) return 1;
    }
    return 0;
}

static void layout_labels(int n, const Box *b)
{
    int scale = (int)(g_s * 100);
    if (s_lab.net == n && s_lab.w == b->w && s_lab.h == b->h && s_lab.scale == scale) return;
    const CityNetDef *d = &CITY_NETS[n];
    double nx[NET_MAX_NODES], ny[NET_MAX_NODES];
    for (int v = 0; v < d->nnodes; v++) node_xy(b, &d->nodes[v], &nx[v], &ny[v]);
    int th = d_font_h(F_SMALL_B), rr = st_r(d) + SC(3), placed[NET_MAX_NODES] = { 0 };
    for (int pass = 0; pass < 3; pass++)
        for (int v = 0; v < d->nnodes; v++) {
            int tw = d_text_w(d->nodes[v].name, F_SMALL_B), sx = (int)nx[v], sy = (int)ny[v];
            int cand[8][2] = { { sx + rr, sy - th / 2 }, { sx - rr - tw, sy - th / 2 }, { sx - tw / 2, sy - rr - th },
                               { sx - tw / 2, sy + rr }, { sx + rr - SC(2), sy - rr - th + SC(4) }, { sx + rr - SC(2), sy + rr - SC(4) },
                               { sx - rr - tw + SC(2), sy - rr - th + SC(4) }, { sx - rr - tw + SC(2), sy + rr - SC(4) } };
            int best = 0;
            double bestScore = 1e18;
            for (int k = 0; k < 8; k++) {
                RECT r = { cand[k][0] - SC(3), cand[k][1] - SC(1), cand[k][0] + tw + SC(3), cand[k][1] + th + SC(1) };
                double sc = k * 2;
                if (r.left < b->x + SC(4) || r.right > b->x + b->w - SC(4) || r.top < b->y + SC(4) || r.bottom > b->y + b->h - SC(4)) sc += 5000;
                for (int u = 0; u < d->nnodes; u++) {
                    if (u != v && placed[u]) { RECT o; if (IntersectRect(&o, &r, &s_lab.r[u])) sc += 600 + (o.right - o.left) * (o.bottom - o.top); }
                    double qx = clampd(nx[u], r.left, r.right), qy = clampd(ny[u], r.top, r.bottom);
                    if (u != v && hypot(qx - nx[u], qy - ny[u]) < rr) sc += 500;
                }
                for (int e = 0; e < d->nedges; e++)
                    if (rect_hits_seg(&r, nx[d->edges[e].a], ny[d->edges[e].a], nx[d->edges[e].b], ny[d->edges[e].b])) sc += 120;
                for (int k2 = 0; k2 < d->nxfer; k2++)
                    if (rect_hits_seg(&r, nx[d->xfer[k2][0]], ny[d->xfer[k2][0]], nx[d->xfer[k2][1]], ny[d->xfer[k2][1]])) sc += 120;
                for (int i = 0; i + 1 < d->nriver; i++) {
                    double ax = b->x + b->padX + d->river[i][0] * (b->w - 2 * b->padX), ay = b->y + b->padY + d->river[i][1] * (b->h - 2 * b->padY);
                    double bx = b->x + b->padX + d->river[i + 1][0] * (b->w - 2 * b->padX), by = b->y + b->padY + d->river[i + 1][1] * (b->h - 2 * b->padY);
                    if (rect_hits_seg(&r, ax, ay, bx, by)) sc += 25;
                }
                if (sc < bestScore) { bestScore = sc; best = k; }
            }
            RECT r = { cand[best][0], cand[best][1], cand[best][0] + tw, cand[best][1] + th };
            s_lab.r[v] = r;
            placed[v] = 1;
        }
    s_lab.net = n; s_lab.w = b->w; s_lab.h = b->h; s_lab.scale = scale;
}

/* ---------- значок вида транспорта: «М», вагон с токоприёмником или с двумя штангами ---------- */
static void net_glyph(int type, int cx, int cy, int r, COLORREF fg)
{
    if (type == NT_METRO) {
        int f = r >= SC(14) ? F_BODY_B : F_SMALL_B, th = d_font_h(f);
        d_text_c(cx - r, cy - th / 2, 2 * r, L"М", f, fg);
        return;
    }
    int bw = r * 6 / 5, bh = r * 7 / 10, bx = cx - bw / 2, by = cy - bh / 2 + r / 5;
    d_rrect(bx, by, bw, bh, r / 5 + 1, fg, CLR_INVALID);
    if (type == NT_TRAM) {   /* пантограф-ромб */
        int top = by - r / 2;
        d_line(cx - r / 4, by, cx, top, fg, 1);
        d_line(cx + r / 4, by, cx, top, fg, 1);
        d_line(cx - r / 3, top, cx + r / 3, top, fg, 1);
    } else {                 /* две штанги троллейбуса */
        d_line(bx + bw / 3, by, bx + bw / 3 + r / 2, by - r / 2 - 1, fg, 1);
        d_line(bx + bw / 2, by, bx + bw / 2 + r / 2, by - r / 2 - 1, fg, 1);
    }
}

static void net_badge(int n, int cx, int cy, int r, int filled)
{
    COLORREF c = cr(CITY_NETS[n].color);
    if (filled) d_circle(cx, cy, r, c, P.surface, 2);
    else d_circle(cx, cy, r, P.surface, c, 2);
    net_glyph(CITY_NETS[n].type, cx, cy, r, filled ? RGB(255, 255, 255) : c);
}

/* ---------- поезда на построенных линиях ---------- */
typedef struct { int n, line; unsigned char v[NET_MAX_NODES + 1]; } Path;

/* построенные перегоны каждой линии раскладываются на цепочки (самая длинная — первой) */
static int build_paths(int net, Path *out, int maxp)
{
    const CityNetDef *d = &CITY_NETS[net];
    unsigned long long left = 0;
    for (int e = 0; e < d->nedges; e++) if (citynet_edge_built(&G, net, e)) left |= 1ull << e;
    int np = 0;
    while (left && np < maxp) {
        int e0 = 0;
        while (!(left >> e0 & 1)) e0++;
        int line = d->edges[e0].line, from = d->edges[e0].a, fv = from, pe[NET_MAX_NODES];
        for (int round = 0; round < 2; round++) {   /* дважды поиск в ширину: самый дальний от дальнего */
            int dist[NET_MAX_NODES], q[NET_MAX_NODES], qh = 0, qt = 0;
            for (int v = 0; v < d->nnodes; v++) { dist[v] = -1; pe[v] = -1; }
            dist[from] = 0; q[qt++] = from; fv = from;
            while (qh < qt) {
                int u = q[qh++];
                if (dist[u] > dist[fv]) fv = u;
                for (int e = 0; e < d->nedges; e++) {
                    if (!(left >> e & 1) || d->edges[e].line != line) continue;
                    int w = d->edges[e].a == u ? d->edges[e].b : d->edges[e].b == u ? d->edges[e].a : -1;
                    if (w < 0 || dist[w] >= 0) continue;
                    dist[w] = dist[u] + 1; pe[w] = e; q[qt++] = w;
                }
            }
            if (round == 0) from = fv;
        }
        Path *p = &out[np];
        p->n = 0; p->line = line;
        for (int v = fv; ; ) {
            p->v[p->n++] = (unsigned char)v;
            int e = pe[v];
            if (e < 0) break;
            left &= ~(1ull << e);
            v = d->edges[e].a == v ? d->edges[e].b : d->edges[e].a;
        }
        if (p->n < 2) { left &= ~(1ull << e0); continue; }
        np++;
    }
    return np;
}

static void train_draw(const Cv *c, int type, double x, double y, double ux, double uy, COLORREF line)
{
    COLORREF dark = mix(line, RGB(20, 24, 32), 150), body = mix(line, RGB(255, 255, 255), 70);
    double hx = x, hy = y;   /* голова */
    if (type == NT_METRO) {
        double car = SC(9), gap = SC(1.5);
        x += ux * (car * 1.5 + gap);   /* поезд стоит серединой на станции */
        y += uy * (car * 1.5 + gap);
        hx = x; hy = y;
        for (int k = 0; k < 3; k++) {
            double c0 = -k * (car + gap), c1 = c0 - car;
            aa_seg(c, x + ux * c0, y + uy * c0, x + ux * c1, y + uy * c1, SC(4.6), dark, 255);
            aa_seg(c, x + ux * (c0 - SC(0.5)), y + uy * (c0 - SC(0.5)), x + ux * (c1 + SC(0.5)), y + uy * (c1 + SC(0.5)), SC(2.9), body, 255);
        }
    } else {
        double len = type == NT_TRAM ? SC(16) : SC(14);
        x += ux * len / 2; y += uy * len / 2;
        hx = x; hy = y;
        aa_seg(c, x, y, x - ux * len, y - uy * len, SC(4.6), dark, 255);
        aa_seg(c, x - ux * SC(0.5), y - uy * SC(0.5), x - ux * (len - SC(0.5)), y - uy * (len - SC(0.5)), SC(3), body, 255);
        if (type == NT_TROLLEY) {   /* штанги тянутся назад к проводам */
            double px = -uy, py = ux, mx = x - ux * len * 0.45, my = y - uy * len * 0.45;
            for (int s = -1; s <= 1; s += 2)
                aa_seg(c, mx + px * s * SC(1.2), my + py * s * SC(1.2), mx - ux * SC(10) + px * s * SC(2.2), my - uy * SC(10) + py * s * SC(2.2),
                       SC(0.6), dark, 220);
        }
    }
    aa_disc(c, hx - ux * SC(1.2), hy - uy * SC(1.2), SC(1.5), RGB(255, 226, 120), 255);   /* фара */
}

#define T_RUN  1.7     /* секунд на перегон */
#define T_STOP 0.8     /* стоянка на станции */

static void trains_draw(const Cv *c, int net, const double *nx, const double *ny)
{
    const CityNetDef *d = &CITY_NETS[net];
    Path paths[NET_MAX_EDGES];
    int np = build_paths(net, paths, NET_MAX_EDGES);
    double t = anim_t();
    for (int i = 0; i < np; i++) {
        const Path *p = &paths[i];
        int L = p->n - 1, k = 1 + L / 3;
        double seg = T_RUN + T_STOP, D = L * seg, cyc = 2 * D;
        COLORREF col = cr(d->lines[p->line].color);
        for (int j = 0; j < k; j++) {
            double tt = fmod(t + j * cyc / k + i * 1.3, cyc);
            int back = tt >= D;
            if (back) tt -= D;
            int s = (int)(tt / seg);
            if (s >= L) s = L - 1;
            double loc = tt - s * seg, f = loc < T_STOP ? 0 : (loc - T_STOP) / T_RUN;
            f = f * f * (3 - 2 * f);   /* разгон и торможение */
            int a = back ? p->v[L - s] : p->v[s], b = back ? p->v[L - s - 1] : p->v[s + 1];
            double dx = nx[b] - nx[a], dy = ny[b] - ny[a], len = hypot(dx, dy);
            if (len < 1) continue;
            train_draw(c, d->type, nx[a] + dx * f, ny[a] + dy * f, dx / len, dy / len, col);
        }
    }
}

/* ---------- открыть и закрыть ---------- */
static void net_open(int n, int intro)
{
    if (n < 0 || n >= CITY_NET_COUNT) return;
    if (U.modal != M_CITYNET) {
        s_back = U.modal == M_CITY || U.modal == M_PROJ || U.modal == M_MAP ? U.modal : M_NONE;
        s_backArg = U.modalArg;
    }
    U.modal = M_CITYNET;
    U.modalArg = n;
    U.modalScroll = 0;
    s_sel = -1;
    s_selEdge = -1;
    s_introT0 = intro ? GetTickCount() : 0;
    s_introNet = intro ? n : -1;
    ui_invalidate();
}

static void net_close(void)
{
    U.modal = s_back;
    U.modalArg = s_backArg;
    U.modalScroll = 0;
    s_back = M_NONE;
    s_devT = -1;
    s_devHover = -1;
    s_introT0 = 0;
    ui_after_change();       /* очередь событий и заставок идёт дальше */
}

static int intro_on(int n)
{
    if (s_introNet != n || !s_introT0) return 0;
    return g_dev || GetTickCount() - s_introT0 < INTRO_MS;
}

/* ---------- окно-схема ---------- */
static void draw_scheme(int n, const Box *b)
{
    const CityNetDef *d = &CITY_NETS[n];
    int st = citynet_state(&G, n);
    double nx[NET_MAX_NODES], ny[NET_MAX_NODES];
    for (int v = 0; v < d->nnodes; v++) node_xy(b, &d->nodes[v], &nx[v], &ny[v]);
    unsigned long long stations = citynet_station_mask(&G, n);
    int hovE = -1, hovN = -1;
    for (int e = 0; e < d->nedges; e++) if (hover(A_NET_EDGE, e)) hovE = e;
    if (s_devHover >= 0) hovE = s_devHover;
    for (int v = 0; v < d->nnodes; v++) if (hover(A_NET_NODE, v)) hovN = v;

    /* подложка схемы: бумага с лёгкой сеткой кварталов */
    COLORREF paper = g_dark ? mix(P.surface, P.bg, 120) : mix(P.surface, P.bg, 70);
    d_rrect(b->x, b->y, b->w, b->h, SC(10), paper, P.line);
    COLORREF grid = mix(paper, P.ink, g_dark ? 14 : 10);
    for (int gx = b->x + SC(40); gx < b->x + b->w - SC(4); gx += SC(40)) d_line(gx, b->y + 1, gx, b->y + b->h - 2, grid, 1);
    for (int gy = b->y + SC(40); gy < b->y + b->h - SC(4); gy += SC(40)) d_line(b->x + 1, gy, b->x + b->w - 2, gy, grid, 1);

    Cv c;
    if (!cv_get(&c, b->x + 1, b->y + 1, b->w - 2, b->h - 2)) return;
    double t = anim_t();

    /* река: сглаженная ломаная (Кэтмелл — Ром) с берегами */
    COLORREF water = g_dark ? mix(P.water, paper, 110) : mix(P.water, paper, 120);
    COLORREF bank = mix(water, P.ink, 40);
    double rx[NET_MAX_RIVER * 8], ry[NET_MAX_RIVER * 8];
    int rn = 0;
    for (int i = 0; i + 1 < d->nriver && rn < NET_MAX_RIVER * 8 - 1; i++) {
        double p[4][2];
        for (int k = 0; k < 4; k++) {
            int j = i - 1 + k;
            if (j < 0) j = 0;
            if (j >= d->nriver) j = d->nriver - 1;
            p[k][0] = b->x + b->padX + d->river[j][0] * (b->w - 2 * b->padX);
            p[k][1] = b->y + b->padY + d->river[j][1] * (b->h - 2 * b->padY);
        }
        for (int s = 0; s < 6; s++) {
            double u = s / 6.0, u2 = u * u, u3 = u2 * u;
            for (int a = 0; a < 2; a++) {
                double v = 0.5 * (2 * p[1][a] + (-p[0][a] + p[2][a]) * u + (2 * p[0][a] - 5 * p[1][a] + 4 * p[2][a] - p[3][a]) * u2 +
                                  (-p[0][a] + 3 * p[1][a] - 3 * p[2][a] + p[3][a]) * u3);
                (a ? ry : rx)[rn] = v;
            }
            rn++;
        }
    }
    rx[rn] = b->x + b->padX + d->river[d->nriver - 1][0] * (b->w - 2 * b->padX);
    ry[rn] = b->y + b->padY + d->river[d->nriver - 1][1] * (b->h - 2 * b->padY);
    rn++;
    /* у краёв схемы река уходит за рамку: концы продлеваются наружу */
    for (int end = 0; end < 2; end++) {
        int i0 = end ? rn - 1 : 0, i1 = end ? rn - 2 : 1;
        double ex = rx[i0] - rx[i1], ey = ry[i0] - ry[i1], el = hypot(ex, ey);
        if (el < 1) continue;
        rx[i0] += ex / el * (b->w + b->h); ry[i0] += ey / el * (b->w + b->h);
    }
    double rw = d->type == NT_METRO ? SC(6.5) : SC(8);
    for (int i = 0; i + 1 < rn; i++) aa_seg(&c, rx[i], ry[i], rx[i + 1], ry[i + 1], rw + SC(1.5), bank, 90);
    for (int i = 0; i + 1 < rn; i++) aa_seg(&c, rx[i], ry[i], rx[i + 1], ry[i + 1], rw, water, 255);

    /* пересадки */
    for (int k = 0; k < d->nxfer; k++) {
        int a = d->xfer[k][0], bb = d->xfer[k][1];
        if ((stations >> a & 1) && (stations >> bb & 1)) {
            aa_seg(&c, nx[a], ny[a], nx[bb], ny[bb], SC(5.5), P.ink, 255);
            aa_seg(&c, nx[a], ny[a], nx[bb], ny[bb], SC(3), P.surface, 255);
        } else aa_dash(&c, nx[a], ny[a], nx[bb], ny[bb], SC(1.2), P.muted, 150, SC(3), SC(4));
    }

    /* перегоны: ещё не построенные — пунктиром, построенные — сплошной линией цвета линии */
    for (int pass = 0; pass < 2; pass++)
        for (int e = 0; e < d->nedges; e++) {
            const NetEdge *ed = &d->edges[e];
            int built = citynet_edge_built(&G, n, e);
            if (built != pass) continue;
            COLORREF lc = cr(d->lines[ed->line].color);
            double x0 = nx[ed->a], y0 = ny[ed->a], x1 = nx[ed->b], y1 = ny[ed->b];
            if (built) {
                double r = d->type == NT_METRO ? SC(4.5) : SC(3.2);
                if (e == hovE || e == s_selEdge) aa_seg(&c, x0, y0, x1, y1, r + SC(4), lc, 60);
                aa_seg(&c, x0, y0, x1, y1, r, lc, 255);
                if (d->type == NT_TRAM)     /* два рельса: светлая полоса посередине пути */
                    aa_seg(&c, x0, y0, x1, y1, SC(0.8), mix(lc, paper, 150), 255);
                else if (d->type == NT_TROLLEY) {   /* два контактных провода */
                    double L = hypot(x1 - x0, y1 - y0), px = -(y1 - y0) / L, py = (x1 - x0) / L;
                    for (int s = -1; s <= 1; s += 2)
                        aa_seg(&c, x0 + px * s * SC(5.5), y0 + py * s * SC(5.5), x1 + px * s * SC(5.5), y1 + py * s * SC(5.5), SC(0.55), mix(lc, P.ink, 90), 200);
                }
            } else {
                int can = citynet_can_build(&G, n, e);
                if (can) {
                    if (e == hovE || e == s_selEdge) aa_seg(&c, x0, y0, x1, y1, SC(8), lc, 45);
                    aa_dash(&c, x0, y0, x1, y1, e == hovE ? SC(3.4) : SC(2.6), lc, e == hovE ? 255 : 210, SC(9), SC(6));
                } else if (st == NS_AVAILABLE && G.year >= d->lines[ed->line].year)
                    aa_dash(&c, x0, y0, x1, y1, SC(1.6), mix(lc, paper, 120), 200, SC(6), SC(6));
                else aa_dash(&c, x0, y0, x1, y1, SC(1.3), mix(P.muted, paper, 90), 200, SC(3), SC(5));
            }
        }

    /* станции */
    int empty = !stations && st == NS_AVAILABLE;
    for (int v = 0; v < d->nnodes; v++) {
        COLORREF lc = cr(d->lines[node_line(d, v)].color);
        if (stations >> v & 1) {
            double r = st_r(d);
            COLORREF ring = node_xfer(d, v) ? P.ink : lc;
            aa_disc(&c, nx[v], ny[v], r, ring, 255);
            aa_disc(&c, nx[v], ny[v], r - SC(3.2), P.surface, 255);
        } else {
            double r = st_r(d) - SC(2);
            aa_disc(&c, nx[v], ny[v], r, mix(lc, paper, 130), 255);
            aa_disc(&c, nx[v], ny[v], r - SC(2), paper, 255);
        }
        if (empty && v == d->startNode) {   /* «начните отсюда»: расходящиеся круги */
            for (int k = 0; k < 2; k++) {
                double ph = fmod(t / 1.4 + k * 0.5, 1.0);
                aa_ring(&c, nx[v], ny[v], st_r(d) + SC(2) + ph * SC(16), SC(2), lc, (int)(220 * (1 - ph)));
            }
        }
        if (v == s_sel) aa_ring(&c, nx[v], ny[v], st_r(d) + SC(4), SC(2.5), P.gold, 255);
        else if (v == hovN) aa_ring(&c, nx[v], ny[v], st_r(d) + SC(3), SC(1.5), P.ink, 200);
    }

    /* поезда */
    trains_draw(&c, n, nx, ny);

    /* подписи станций и реки (GDI поверх пикселей) */
    layout_labels(n, b);
    d_clip(b->x + 1, b->y + 1, b->w - 2, b->h - 2);
    {   /* название реки — в самом свободном месте русла */
        int best = rn / 2;
        double bd = -1;
        for (int i = 2; i + 2 < rn; i++) {
            double m = 1e9;
            for (int v = 0; v < d->nnodes; v++) { double q = hypot(rx[i] - nx[v], ry[i] - ny[v]); if (q < m) m = q; }
            if (rx[i] < b->x + SC(70) || rx[i] > b->x + b->w - SC(70) || ry[i] < b->y + SC(20) || ry[i] > b->y + b->h - SC(20)) continue;
            if (m > bd) { bd = m; best = i; }
        }
        int tw = d_text_w(d->riverName, F_TINY);
        double dx = rx[best + 1] - rx[best], dy = ry[best + 1] - ry[best];
        int lx = (int)rx[best] - tw / 2, ly = (int)ry[best] + (fabs(dx) > fabs(dy) ? (int)rw + SC(3) : -d_font_h(F_TINY) / 2);
        if (fabs(dx) <= fabs(dy)) lx = (int)rx[best] + (int)rw + SC(4);
        SetTextCharacterExtra(d_dc(), 1);
        d_text(lx, ly, d->riverName, F_TINY, mix(P.water, P.ink, g_dark ? 40 : 150));
        SetTextCharacterExtra(d_dc(), 0);
    }
    for (int v = 0; v < d->nnodes; v++) {
        const RECT *r = &s_lab.r[v];
        int built = stations >> v & 1;
        d_blend(r->left - SC(3), r->top, r->right - r->left + SC(6), r->bottom - r->top, paper, 200);
        d_text(r->left, r->top, d->nodes[v].name, built ? F_SMALL_B : F_SMALL, built ? P.ink : (v == hovN || v == s_sel ? P.ink : P.muted));
    }
    d_unclip();

    /* области нажатия: станции с подписями, перегоны — цепочкой квадратиков */
    hits_clip(b->x, b->y, b->w, b->h);
    for (int e = 0; e < d->nedges; e++) {
        const NetEdge *ed = &d->edges[e];
        double x0 = nx[ed->a], y0 = ny[ed->a], x1 = nx[ed->b], y1 = ny[ed->b], L = hypot(x1 - x0, y1 - y0);
        int k = (int)(L / SC(9));
        for (int i = 1; i < k; i++) {
            double f = (double)i / k;
            int hx = (int)(x0 + (x1 - x0) * f), hy = (int)(y0 + (y1 - y0) * f);
            if (hypot(hx - x0, hy - y0) < st_r(d) + SC(5) || hypot(hx - x1, hy - y1) < st_r(d) + SC(5)) continue;
            hit(hx - SC(7), hy - SC(7), SC(14), SC(14), A_NET_EDGE, e);
        }
    }
    for (int v = 0; v < d->nnodes; v++) {
        const RECT *r = &s_lab.r[v];
        hit(r->left - SC(3), r->top - SC(1), r->right - r->left + SC(6), r->bottom - r->top + SC(2), A_NET_NODE, v);
        hit((int)nx[v] - st_r(d) - SC(3), (int)ny[v] - st_r(d) - SC(3), 2 * st_r(d) + SC(6), 2 * st_r(d) + SC(6), A_NET_NODE, v);
    }
    hits_unclip();

    /* цена перегона под курсором */
    if (hovE >= 0 && !citynet_edge_built(&G, n, hovE)) {
        const NetEdge *ed = &d->edges[hovE];
        int mx = (int)((nx[ed->a] + nx[ed->b]) / 2), my = (int)((ny[ed->a] + ny[ed->b]) / 2);
        const wchar_t *why = citynet_cannot_build(&G, n, hovE);
        wchar_t s1[120], s2[200];
        swprintf(s1, 120, L"%d казны · %d производства", citynet_edge_gold(&G, n, hovE), citynet_edge_ind(&G, n, hovE));
        if (!why) swprintf(s2, 200, L"Нажмите, чтобы проложить");
        else { wcsncpy(s2, why, 199); s2[199] = 0; }
        int w1 = d_text_w(s1, F_SMALL_B), w2 = d_text_w(s2, F_TINY), bw = (w1 > w2 ? w1 : w2) + SC(20);
        int bh = d_font_h(F_SMALL_B) + d_font_h(F_TINY) + SC(16);
        int bx = mx - bw / 2, by = my - bh - SC(14);
        if (by < b->y + SC(6)) by = my + SC(14);
        if (bx < b->x + SC(6)) bx = b->x + SC(6);
        if (bx + bw > b->x + b->w - SC(6)) bx = b->x + b->w - SC(6) - bw;
        d_rrect(bx + SC(2), by + SC(3), bw, bh, SC(7), mix(paper, P.shadow, 120), CLR_INVALID);
        d_rrect(bx, by, bw, bh, SC(7), P.ink, CLR_INVALID);
        d_fill(bx + SC(8), by + SC(8), SC(3), bh - SC(16), cr(d->lines[ed->line].color));
        d_text(bx + SC(16), by + SC(6), s1, F_SMALL_B, P.surface);
        d_text(bx + SC(16), by + SC(8) + d_font_h(F_SMALL_B), s2, F_TINY, why ? mix(P.surface, P.red, 120) : mix(P.surface, P.ink, 70));
    }

    /* вступление после заставки 1984 года */
    if (intro_on(n)) {
        wchar_t h1[200], h2[260];
        int stt = citynet_state(&G, n);
        if (stt == NS_LOCKED) {
            swprintf(h1, 200, L"%d год: Минску нужно метро. Сначала изучите технологию «%ls»", d->year,
                     d->techi >= 0 ? TECHS[d->techi].name : L"");
            swprintf(h2, 260, L"Она во вкладке «Технологии». Потом прокладывайте перегоны на этой схеме сами.");
        } else if (stations) {
            swprintf(h1, 200, L"1984 год: в Минске открывается метро. Продлевайте линии сами");
            swprintf(h2, 260, L"Нажимайте на пунктир, чтобы проложить новые перегоны; каждая станция приносит доход.");
        } else {
            swprintf(h1, 200, L"1984 год: в Минске начинают строить метро. Проложите первые перегоны сами");
            swprintf(h2, 260, L"Начните от станции «%ls» — она мигает на схеме. Нажмите на пунктир рядом с ней.", d->nodes[d->startNode].name);
        }
        int pad = SC(16), w1 = d_text_w(h1, F_BODY_B), w2 = d_text_w(h2, F_SMALL), iw = (w1 > w2 ? w1 : w2) + 2 * pad + SC(10);
        if (iw > b->w - SC(32)) iw = b->w - SC(32);
        int ih = d_font_h(F_BODY_B) + d_font_h(F_SMALL) + SC(26), ix = b->x + (b->w - iw) / 2, iy = b->y + SC(14);
        d_rrect(ix + SC(3), iy + SC(4), iw, ih, SC(9), mix(paper, P.shadow, 150), CLR_INVALID);
        d_rrect(ix, iy, iw, ih, SC(9), P.surface, cr(d->color));
        d_fill(ix + SC(10), iy + SC(10), SC(4), ih - SC(20), cr(d->color));
        d_ellipsis(ix + pad + SC(10), iy + SC(10), iw - 2 * pad - SC(10), h1, F_BODY_B, P.ink);
        d_ellipsis(ix + pad + SC(10), iy + SC(14) + d_font_h(F_BODY_B), iw - 2 * pad - SC(10), h2, F_SMALL, P.muted);
    }
    if (empty || stations || intro_on(n)) ui_anim_request();   /* поезда, мигание первой станции, вступление */
}

/* ---------- боковая панель ---------- */
static void caps(int x, int y, const wchar_t *s, COLORREF c)
{
    SetTextCharacterExtra(d_dc(), 1);
    d_text(x, y, s, F_TINY, c);
    SetTextCharacterExtra(d_dc(), 0);
}

static void stat_tile(int x, int y, int w, int h, const wchar_t *big, const wchar_t *cap)
{
    d_rrect(x, y, w, h, SC(7), mix(P.surface, P.surface2, 170), P.line);
    d_text_c(x, y + SC(7), w, big, F_H2, P.ink);
    d_text_c(x, y + h - d_font_h(F_TINY) - SC(7), w, cap, F_TINY, P.muted);
}

static void draw_panel(int n, int x, int y, int w, int h)
{
    const CityNetDef *d = &CITY_NETS[n];
    int st = citynet_state(&G, n), y1 = y + h;
    d_clip(x, y, w, h);
    hits_clip(x, y, w, h);
    /* города: переключатель сетей */
    caps(x, y, L"ГОРОДА", P.muted);
    y += d_font_h(F_TINY) + SC(6);
    int gap = SC(6), cw = (w - 2 * gap) / 3, ch = SC(34);
    for (int k = 0; k < CITY_NET_COUNT; k++) {
        int cx = x + (k % 3) * (cw + gap), cy = y + (k / 3) * (ch + gap);
        COLORREF col = cr(CITY_NETS[k].color);
        int on = k == n, hv = hover(A_NET_OPEN, k), stk = citynet_state(&G, k), has = citynet_stations(&G, k) > 0;
        d_rrect(cx, cy, cw, ch, SC(7), on ? mix(P.surface, col, 40) : hv ? P.surface2 : P.surface, on ? col : P.line);
        net_badge(k, cx + SC(16), cy + ch / 2, SC(9), has || stk == NS_AVAILABLE);
        d_ellipsis(cx + SC(30), cy + (ch - d_font_h(F_SMALL_B)) / 2, cw - SC(34), CITIES[CITY_NETS[k].city].name, F_SMALL_B,
                   stk == NS_FUTURE && !has ? P.muted : P.ink);
        hit(cx, cy, cw, ch, A_NET_OPEN, k);
    }
    y += 2 * ch + gap + SC(16);

    /* состояние и цифры */
    wchar_t s[300];
    if (st == NS_FUTURE) swprintf(s, 300, L"%ls появится в %d году. Схему можно рассмотреть уже сейчас.", d->kind, d->year);
    else if (st == NS_LOCKED) swprintf(s, 300, L"Нужна технология «%ls» — изучите её во вкладке «Технологии».", d->techi >= 0 ? TECHS[d->techi].name : L"");
    else if (!citynet_station_mask(&G, n)) swprintf(s, 300, L"Можно строить! Начните от станции «%ls».", d->nodes[d->startNode].name);
    else s[0] = 0;
    if (s[0]) {
        COLORREF c = st == NS_AVAILABLE ? P.green : st == NS_LOCKED ? P.red : P.muted;
        int tw = w - SC(26), hh = d_wrap(0, 0, tw, s, F_SMALL, 0, 0) + SC(16);
        d_rrect(x, y, w, hh, SC(7), mix(P.surface, c, 28), mix(P.line, c, 120));
        d_wrap(x + SC(13), y + SC(8), tw, s, F_SMALL, P.ink, 1);
        y += hh + SC(12);
    }
    {
        int tw3 = (w - 2 * gap) / 3, th = SC(58);
        wchar_t a[32], b[32], c[32], g1[24];
        double inc[R_COUNT];
        citynet_net_income(&G, n, inc);
        swprintf(a, 32, L"%d/%d", citynet_stations(&G, n), d->nnodes);
        swprintf(b, 32, L"%d/%d", citynet_edges_count(&G, n), d->nedges);
        fmt1(inc[R_GOLD], g1, 24);
        swprintf(c, 32, L"+%ls", g1);
        stat_tile(x, y, tw3, th, a, d->type == NT_METRO ? L"станций" : L"остановок");
        stat_tile(x + tw3 + gap, y, tw3, th, b, L"перегонов");
        stat_tile(x + 2 * (tw3 + gap), y, tw3, th, c, L"казны за ход");
        y += th + SC(8);
        wchar_t it[200];
        income_text(n, it, 200);
        if (it[0]) { swprintf(s, 300, L"Доход за ход: %ls", it); y += d_wrap(x, y, w, s, F_TINY, P.muted, 1) + SC(12); }
        else y += SC(6);
    }

    /* линии */
    for (int l = 0; l < d->nlines; l++) {
        COLORREF lc = cr(d->lines[l].color);
        int tot = 0, built = 0;
        for (int e = 0; e < d->nedges; e++) if (d->edges[e].line == l) { tot++; built += citynet_edge_built(&G, n, e); }
        d_rrect(x, y + SC(5), SC(24), SC(7), SC(3), lc, CLR_INVALID);
        d_ellipsis(x + SC(32), y, w - SC(120), d->lines[l].name, F_SMALL_B, P.ink);
        if (G.year < d->lines[l].year) swprintf(s, 300, L"с %d года", d->lines[l].year);
        else swprintf(s, 300, L"%d из %d", built, tot);
        d_text_r(x + w, y, s, F_SMALL, G.year < d->lines[l].year ? P.muted : P.ink);
        y += d_font_h(F_SMALL_B) + SC(8);
    }
    y += SC(8);

    /* выбранная станция или перегон */
    int hovE = -1;
    for (int e = 0; e < d->nedges; e++) if (hover(A_NET_EDGE, e)) hovE = e;
    if (s_devHover >= 0) hovE = s_devHover;
    int ie = hovE >= 0 ? hovE : s_selEdge;
    d_line(x, y, x + w, y, P.line, 1);
    y += SC(12);
    if (s_sel >= 0 && s_sel < d->nnodes && ie < 0) {
        const NetNode *v = &d->nodes[s_sel];
        int built = citynet_node_station(&G, n, s_sel);
        caps(x, y, d->type == NT_METRO ? L"СТАНЦИЯ" : L"ОСТАНОВКА", P.muted);
        y += d_font_h(F_TINY) + SC(4);
        y += d_wrap(x, y, w, v->name, F_H3, P.ink, 1) + SC(4);
        if (v->year) swprintf(s, 300, L"%ls · открыта в %d году · %ls", d->lines[node_line(d, s_sel)].name, v->year,
                              built ? L"построена" : L"ещё не построена");
        else swprintf(s, 300, L"%ls", built ? L"Остановка открыта" : L"Пока не построена");
        y += d_wrap(x, y, w, s, F_SMALL, built ? P.green : P.muted, 1) + SC(6);
        if (v->note) y += d_wrap(x, y, w, v->note, F_SMALL, P.ink, 1) + SC(8);
    } else if (ie >= 0) {
        const NetEdge *ed = &d->edges[ie];
        caps(x, y, L"ПЕРЕГОН", P.muted);
        y += d_font_h(F_TINY) + SC(4);
        swprintf(s, 300, L"«%ls» — «%ls»", d->nodes[ed->a].name, d->nodes[ed->b].name);
        y += d_wrap(x, y, w, s, F_BODY_B, P.ink, 1) + SC(4);
        if (citynet_edge_built(&G, n, ie)) swprintf(s, 300, L"%ls · построен", d->lines[ed->line].name);
        else {
            const wchar_t *why = citynet_cannot_build(&G, n, ie);
            swprintf(s, 300, L"%ls · %d казны, %d производства. %ls", d->lines[ed->line].name, citynet_edge_gold(&G, n, ie),
                     citynet_edge_ind(&G, n, ie), why ? why : L"Можно строить.");
        }
        y += d_wrap(x, y, w, s, F_SMALL, P.ink, 1) + SC(8);
        if (!citynet_edge_built(&G, n, ie) && citynet_can_build(&G, n, ie)) {
            int ok = !citynet_cannot_build(&G, n, ie);
            w_button(x, y, w, SC(38), L"Проложить перегон", B_PRIMARY, ok, A_NET_EDGE, ie);
            y += SC(38) + SC(10);
        }
    } else {
        y += d_wrap(x, y, w, d->type == NT_METRO ? L"Нажмите на станцию, чтобы узнать о ней, или на пунктир, чтобы проложить перегон."
                                                  : L"Нажмите на остановку, чтобы узнать о ней, или на пунктир, чтобы проложить линию.",
                    F_SMALL, P.muted, 1) + SC(10);
    }
    /* историческая справка — до конца панели */
    if (y + SC(60) < y1) {
        y += SC(4);
        caps(x, y, L"ИЗ ИСТОРИИ", P.muted);
        y += d_font_h(F_TINY) + SC(4);
        d_wrap(x, y, w, d->desc, F_SMALL, P.ink, 1);
    }
    hits_unclip();
    d_unclip();
}

void citynet_ui_draw(void)
{
    int n = U.modalArg;
    if (n < 0 || n >= CITY_NET_COUNT) { U.modal = M_NONE; return; }
    const CityNetDef *d = &CITY_NETS[n];
    d_unclip();
    d_blend(0, 0, g_w, g_h, P.shadow, g_dark ? 160 : 130);
    int m = SC(18), x = m, y = m, w = g_w - 2 * m, h = g_h - 2 * m;
    d_rrect(x + SC(3), y + SC(6), w, h, SC(12), mix(P.bg, P.shadow, 160), CLR_INVALID);
    d_rrect(x, y, w, h, SC(12), P.surface, P.line);
    int pad = SC(20);
    /* заголовок: значок, «МЕТРОПОЛИТЕН · МИНСК · С 1984 ГОДА», название сети */
    int bx = x + pad, by = y + SC(16), br = SC(17);
    net_badge(n, bx + br, by + br + SC(2), br, 1);
    int tx = bx + 2 * br + SC(14);
    wchar_t eb[120];
    swprintf(eb, 120, L"%ls · %ls · с %d года", d->kind, CITIES[d->city].name, d->year);
    for (wchar_t *p = eb; *p; p++) { if (*p >= L'а' && *p <= L'я') *p = (wchar_t)(*p - 32); else if (*p == L'ё') *p = L'Ё'; }
    caps(tx, by - SC(2), eb, cr(d->color));
    d_ellipsis(tx, by + d_font_h(F_TINY), w - (tx - x) - SC(80), d->title, F_ERA_L, P.ink);
    int top = by + d_font_h(F_TINY) + d_font_h(F_ERA_L) + SC(14);
    int cbx = x + w - SC(46), cby = y + SC(12);
    if (hover(A_NET_CLOSE, 0)) d_rrect(cbx, cby, SC(32), SC(32), SC(16), P.surface2, CLR_INVALID);
    d_line(cbx + SC(10), cby + SC(10), cbx + SC(22), cby + SC(22), P.muted, 2);
    d_line(cbx + SC(22), cby + SC(10), cbx + SC(10), cby + SC(22), P.muted, 2);
    hit(cbx, cby, SC(32), SC(32), A_NET_CLOSE, 0);
    /* схема слева, панель справа */
    int pw = SC(330) < w / 3 ? SC(330) : w / 3, helpH = d_font_h(F_TINY) + SC(16);
    Box b = { x + SC(14), top, w - pw - SC(14) - pad - SC(18), h - (top - y) - helpH - SC(6), 0, 0 };
    b.padX = SC(56) < b.w / 10 ? SC(56) : b.w / 10;
    b.padY = SC(34) < b.h / 10 ? SC(34) : b.h / 10;
    draw_scheme(n, &b);
    draw_panel(n, b.x + b.w + SC(18), top, pw, h - (top - y) - SC(14));
    const wchar_t *help = d->type == NT_METRO
        ? L"Пунктир — перегон, который можно проложить: наведите курсор, чтобы узнать цену, и нажмите · ← → — другой город · Esc — закрыть"
        : L"Пунктир — линия, которую можно проложить: наведите курсор, чтобы узнать цену, и нажмите · ← → — другой город · Esc — закрыть";
    d_ellipsis(b.x + SC(4), b.y + b.h + SC(8), b.w - SC(8), help, F_TINY, P.muted);
}

/* ---------- нажатия, подсказки, клавиши ---------- */
static void build_edge(int n, int e)
{
    const CityNetDef *d = &CITY_NETS[n];
    if (citynet_edge_built(&G, n, e)) { s_selEdge = e; s_sel = -1; return; }
    const wchar_t *why = citynet_cannot_build(&G, n, e);
    if (why) { ui_toast(L"%ls", why); s_selEdge = e; s_sel = -1; return; }
    unsigned long long before = citynet_station_mask(&G, n);
    int projBefore = d->proji >= 0 && G.proj[d->proji];
    if (!citynet_build(&G, n, e)) return;
    unsigned long long fresh = citynet_station_mask(&G, n) & ~before;
    const NetEdge *ed = &d->edges[e];
    int v = fresh >> ed->b & 1 ? ed->b : fresh >> ed->a & 1 ? ed->a : -1;
    if (d->proji >= 0 && !projBefore && G.proj[d->proji])
        ui_toast(L"%ls готова! Стройка «%ls» засчитана", d->firstName, PROJECTS[d->proji].name);
    else if (v >= 0) ui_toast(d->type == NT_METRO ? L"Открыта станция «%ls»" : L"Открыта остановка «%ls»", d->nodes[v].name);
    else ui_toast(L"Проложен перегон «%ls» — «%ls»", d->nodes[ed->a].name, d->nodes[ed->b].name);
    s_selEdge = -1;
    s_sel = v;
    s_introT0 = 0;
}

int citynet_ui_click(int act, int arg)
{
    switch (act) {
    case A_NET_OPEN:
        if (arg < 0 || arg >= CITY_NET_COUNT) return 1;
        if (U.modal == M_CITYNET) { U.modalArg = arg; s_sel = -1; s_selEdge = -1; s_introT0 = 0; }
        else net_open(arg, 0);
        return 1;
    case A_NET_CLOSE:
        if (U.modal == M_CITYNET) net_close();
        return 1;
    case A_NET_EDGE:
        if (U.modal == M_CITYNET && arg >= 0 && arg < CITY_NETS[U.modalArg].nedges) build_edge(U.modalArg, arg);
        return 1;
    case A_NET_NODE:
        if (U.modal == M_CITYNET && arg >= 0 && arg < CITY_NETS[U.modalArg].nnodes) { s_sel = s_sel == arg ? -1 : arg; s_selEdge = -1; }
        return 1;
    }
    return 0;
}

void citynet_ui_hover(int act, int arg, wchar_t *buf, int n)
{
    if (n <= 0) return;
    int net = U.modal == M_CITYNET ? U.modalArg : -1;
    if (act == A_NET_OPEN && arg >= 0 && arg < CITY_NET_COUNT) {
        const CityNetDef *d = &CITY_NETS[arg];
        int k = citynet_stations(&G, arg);
        if (k) swprintf(buf, n, L"%ls: %d %ls — открыть схему", d->title, k,
                        d->type == NT_METRO ? plural(k, L"станция", L"станции", L"станций") : plural(k, L"остановка", L"остановки", L"остановок"));
        else swprintf(buf, n, L"%ls (с %d года) — открыть схему", d->title, d->year);
    } else if (act == A_NET_CLOSE) swprintf(buf, n, L"Закрыть схему (Esc)");
    else if (act == A_NET_EDGE && net >= 0 && arg >= 0 && arg < CITY_NETS[net].nedges) {
        const CityNetDef *d = &CITY_NETS[net];
        const NetEdge *ed = &d->edges[arg];
        const wchar_t *why = citynet_edge_built(&G, net, arg) ? L"построен" : citynet_cannot_build(&G, net, arg);
        swprintf(buf, n, L"Перегон «%ls» — «%ls»: %d казны, %d производства — %ls", d->nodes[ed->a].name, d->nodes[ed->b].name,
                 citynet_edge_gold(&G, net, arg), citynet_edge_ind(&G, net, arg), why ? why : L"нажмите, чтобы проложить");
    } else if (act == A_NET_NODE && net >= 0 && arg >= 0 && arg < CITY_NETS[net].nnodes) {
        const NetNode *v = &CITY_NETS[net].nodes[arg];
        if (v->year) swprintf(buf, n, L"«%ls» — настоящая станция открыта в %d году", v->name, v->year);
        else swprintf(buf, n, L"«%ls»", v->name);
    }
}

int citynet_ui_key(WPARAM vk)
{
    if (U.modal != M_CITYNET) return 0;
    if (vk == VK_F1 || vk == VK_F2) return 0;   /* справка и смена темы — как везде */
    if (vk == VK_ESCAPE) net_close();
    else if (vk == VK_LEFT || vk == VK_RIGHT) {
        U.modalArg = (U.modalArg + (vk == VK_LEFT ? CITY_NET_COUNT - 1 : 1)) % CITY_NET_COUNT;
        s_sel = -1; s_selEdge = -1; s_introT0 = 0;
    } else if (vk == VK_RETURN || vk == VK_SPACE) s_introT0 = 0;
    return 1;
}

/* ---------- заставка 1984 года: камера летит к Минску ---------- */
static void cut_done(void)
{
    s_cut = 0;
    if (U.modal == M_MAP || U.modal == M_NONE) {
        U.modal = M_NONE;
        net_open(citynet_of_city(C_MINSK), 1);
    }
}

int citynet_ui_idle(void)
{
    int n = citynet_of_city(C_MINSK);
    if (n < 0 || (G.cutscenes & CS_METRO_MINSK) || G.year < CITY_NETS[n].year) return 0;
    G.cutscenes |= CS_METRO_MINSK;
    if (G.year > 1991) return 0;   /* старое сохранение далеко за 1984 годом — без заставки */
    if (map3d_on()) {
        U.modal = M_MAP;
        s_cut = 1;
        map_cam_fly_to(CITIES[C_MINSK].lon, CITIES[C_MINSK].lat, 6.0, 45.0, s_flyMs, cut_done);
    } else net_open(n, 1);
    return 1;
}

void citynet_ui_built(int proj)
{
    for (int n = 0; n < CITY_NET_COUNT; n++) {
        if (CITY_NETS[n].proji != proj) continue;
        int rg = 0, ri = 0;
        citynet_project_built(&G, proj, &rg, &ri);
        if (rg || ri) ui_toast(L"%ls: первая очередь готова. За уже проложенные перегоны возвращено %d казны и %d производства",
                               CITY_NETS[n].title, rg, ri);
        else ui_toast(L"%ls: первая очередь готова — продлевайте линии на схеме", CITY_NETS[n].title);
        net_open(n, 0);   /* сразу показать, что построено: поезда уже ходят */
        return;
    }
}

/* ---------- блок в окне города ---------- */
int citynet_city_card(int city, int x, int y, int w)
{
    int y0 = y, any = 0;
    for (int n = 0; n < CITY_NET_COUNT; n++) {
        const CityNetDef *d = &CITY_NETS[n];
        if (d->city != city) continue;
        any = 1;
        COLORREF col = cr(d->color);
        int st = citynet_state(&G, n), k = citynet_stations(&G, n), h = SC(70);
        d_rrect(x, y, w, h, SC(6), mix(P.surface, col, 26), mix(P.line, col, 110));
        net_badge(n, x + SC(32), y + h / 2, SC(18), k > 0 || st == NS_AVAILABLE);
        int tx = x + SC(62), bw = SC(150), tw = w - (tx - x) - bw - SC(22);
        d_ellipsis(tx, y + SC(12), tw, d->title, F_BODY_B, P.ink);
        wchar_t s[200], it[160];
        if (k > 0) {
            double inc[R_COUNT];
            citynet_net_income(&G, n, inc);
            fmt1(inc[R_GOLD], it, 160);
            swprintf(s, 200, L"%d %ls из %d · +%ls казны за ход", k, d->type == NT_METRO ? plural(k, L"станция", L"станции", L"станций")
                                                                              : plural(k, L"остановка", L"остановки", L"остановок"), d->nnodes, it);
        } else if (st == NS_FUTURE) swprintf(s, 200, L"%ls · появится в %d году", d->kind, d->year);
        else if (st == NS_LOCKED) swprintf(s, 200, L"Нужна технология «%ls»", d->techi >= 0 ? TECHS[d->techi].name : L"");
        else swprintf(s, 200, L"Можно строить: проложите первый перегон сами");
        d_ellipsis(tx, y + SC(16) + d_font_h(F_BODY_B), tw, s, F_SMALL, k > 0 ? P.ink : P.muted);
        w_button(x + w - bw - SC(12), y + (h - SC(38)) / 2, bw, SC(38), L"Открыть схему", st == NS_AVAILABLE ? B_PRIMARY : B_SECONDARY, 1, A_NET_OPEN, n);
        y += h + SC(8);
    }
    return any ? y - y0 - SC(8) : 0;
}

/* ---------- значки на карте ---------- */
void citynet_map_overlay(const MapProj *pj, int interactive, int large)
{
    int slot[C_COUNT_] = { 0 };
    for (int n = 0; n < CITY_NET_COUNT; n++) {
        const CityNetDef *d = &CITY_NETS[n];
        int k = citynet_stations(&G, n), st = citynet_state(&G, n);
        if (!k && st != NS_AVAILABLE) continue;
        int sx, sy;
        if (!mapv_city(pj, d->city, &sx, &sy, NULL)) continue;
        int r = SC(large ? 10 : 8), cx = sx + SC(11) + slot[d->city] * (2 * r + SC(3)), cy = sy + SC(12) + r / 2;
        slot[d->city]++;
        if (interactive && hover(A_NET_OPEN, n)) d_circle(cx, cy, r + SC(3), CLR_INVALID, P.ink, 2);
        net_badge(n, cx, cy, r, k > 0);
        if (interactive) hit(cx - r - SC(2), cy - r - SC(2), 2 * r + SC(4), 2 * r + SC(4), A_NET_OPEN, n);
    }
    if (!s_cut || U.modal != M_MAP) return;
    /* полёт к Минску: пульсирующее кольцо и подпись */
    double t = GetTickCount() / 1000.0;
    int sx, sy;
    if (mapv_city(pj, C_MINSK, &sx, &sy, NULL))
        for (int k = 0; k < 2; k++) {
            double ph = fmod(t / 1.6 + k * 0.5, 1.0);
            d_circle(sx, sy, SC(14) + (int)(ph * SC(40)), CLR_INVALID, mix(P.red, P.surface, (int)(ph * 200)), 2);
        }
    int mx, my, mw, mh;
    mapv_rect(pj, &mx, &my, &mw, &mh);
    const wchar_t *h1 = L"1984 год · Минск", *h2 = L"В столице начинают строить метро — камера летит к городу";
    int w1 = d_text_w(h1, F_ERA_M), w2 = d_text_w(h2, F_SMALL), bw = (w1 > w2 ? w1 : w2) + SC(48);
    int bh = d_font_h(F_ERA_M) + d_font_h(F_SMALL) + SC(26), bx = mx + (mw - bw) / 2, by = my + SC(18);
    d_rrect(bx + SC(3), by + SC(4), bw, bh, SC(10), mix(P.bg, P.shadow, 160), CLR_INVALID);
    d_rrect(bx, by, bw, bh, SC(10), P.surface, cr(CITY_NETS[citynet_of_city(C_MINSK)].color));
    d_text_c(bx, by + SC(10), bw, h1, F_ERA_M, P.ink);
    d_text_c(bx, by + SC(14) + d_font_h(F_ERA_M), bw, h2, F_SMALL, P.muted);
    ui_anim_request();
}

/* ---------- сцены разработчика: 60…69 ---------- */
/* проложить до count перегонов (по порядку списка, в несколько проходов); казна и производство не меняются */
static void dev_grow(int n, int count)
{
    double gold = G.gold, ind = G.ind;
    const CityNetDef *d = &CITY_NETS[n];
    if (d->techi >= 0) G.tech[d->techi] = 1;
    for (int pass = 0; pass < 8 && count > 0; pass++)
        for (int e = 0; e < d->nedges && count > 0; e++) {
            G.gold = 1e9; G.ind = 1e9;
            if (citynet_build(&G, n, e)) count--;
        }
    G.gold = gold; G.ind = ind;
}

static int dev_node(int n, const wchar_t *name)
{
    for (int v = 0; v < CITY_NETS[n].nnodes; v++) if (wcscmp(CITY_NETS[n].nodes[v].name, name) == 0) return v;
    return -1;
}

void citynet_dev_scene(int n)
{
    int metro = citynet_of_city(C_MINSK), vit = citynet_of_city(C_VITEBSK), gom = citynet_of_city(C_GOMEL);
    switch (n) {
    case 60:   /* 1984: камера летит к Минску (полёт замедлен, чтобы застать середину) */
        dev_play_until(1984); dev_resolve();
        G.tech[CITY_NETS[metro].techi] = 1;
        G.cutscenes &= ~(unsigned)CS_METRO_MINSK;
        s_flyMs = 45000;
        citynet_ui_idle();
        s_flyMs = 2500;
        break;
    case 61:   /* схема метро в 2006 году: первая линия и часть второй, выбрана «Няміга» */
        dev_play_until(2006); dev_resolve();
        dev_grow(metro, 16);
        net_open(metro, 0);
        s_sel = dev_node(metro, L"Няміга");
        s_devT = 3.3;
        break;
    case 62:   /* витебский трамвай в 1937 году */
        dev_play_until(1937); dev_resolve();
        dev_grow(vit, 7);
        net_open(vit, 0);
        s_sel = dev_node(vit, L"Ратуша");
        s_devT = 2.1;
        break;
    case 63:   /* окно города Минска с блоком метро */
        dev_play_until(1991); dev_resolve();
        dev_grow(metro, 9);
        U.modal = M_CITY; U.modalArg = C_MINSK; U.modalScroll = 0;
        break;
    case 64:   /* гомельский троллейбус в 1975 году, цена перегона под курсором */
        dev_play_until(1975); dev_resolve();
        dev_grow(gom, 6);
        net_open(gom, 0);
        for (int e = 0; e < CITY_NETS[gom].nedges; e++)
            if (citynet_can_build(&G, gom, e)) { s_devHover = e; break; }
        s_devT = 4.4;
        break;
    case 65:   /* 1984: схема сразу после заставки, светлая тема */
        dev_play_until(1984); dev_resolve();
        G.tech[CITY_NETS[metro].techi] = 1;
        ui_toggle_theme();
        net_open(metro, 1);
        s_devT = 0.6;
        break;
    case 66:   /* Брест, 1984: сеть ещё пуста — мигает первая остановка */
        dev_play_until(1984); dev_resolve();
        G.tech[CITY_NETS[citynet_of_city(C_BREST)].techi] = 1;
        net_open(citynet_of_city(C_BREST), 0);
        s_devT = 0.4;
        break;
    case 67:   /* карта во весь экран со значками транспорта у городов */
        dev_play_until(2000); dev_resolve();
        for (int k = 0; k < CITY_NET_COUNT; k++) dev_grow(k, 5);
        U.modal = M_MAP;
        break;
    case 68:   /* метро в 2024 году: все три линии */
        dev_play_until(2024); dev_resolve();
        dev_grow(metro, NET_MAX_EDGES);
        net_open(metro, 0);
        s_devT = 7.7;
        break;
    case 69:   /* Могилёв в светлой теме */
        dev_play_until(1991); dev_resolve();
        dev_grow(citynet_of_city(C_MOGILEV), 6);
        ui_toggle_theme();
        net_open(citynet_of_city(C_MOGILEV), 0);
        s_devT = 1.2;
        break;
    }
}
