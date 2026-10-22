/* Отряды на карте: идущие и стреляющие солдаты (у игрока — женщины-героини с косами), конница,
 * пушки с расчётом, танки, машины и самолёты; вражеские дивизии — в форме своей эпохи
 * (тевтонцы с чёрным крестом, стрельцы, наполеоновская пехота, вермахт в касках).
 * Всё рисуется процедурно и анимируется по времени t: шаг, покачивание, откат, вспышки, дым. */
#include "mapview.h"
#include "war.h"
#include <math.h>
#include <string.h>

#define PI 3.14159265358979

enum { CL_SPEAR, CL_BOW, CL_CAV, CL_MUSKET, CL_GUN, CL_RIFLE, CL_TANK, CL_AIR, CL_VEH };
enum { HK_NONE, HK_POINT, HK_HELM, HK_HAT, HK_SHAKO, HK_CAP, HK_SCARF, HK_STAHL, HK_FUR, HK_TANKER };

typedef struct {
    COLORREF coat, trim, hat, pants, skin, hair, metal;
    int hat_kind, braid, cross, wings, star;
} Look;

static int unit_class(int u)
{
    static const struct { const char *id; int cl; } M[] = {
        { "druzhina", CL_SPEAR }, { "crossbow", CL_BOW }, { "khorugv", CL_CAV }, { "gunners", CL_MUSKET },
        { "musketeers", CL_MUSKET }, { "hussars", CL_CAV }, { "rockets", CL_GUN }, { "line", CL_MUSKET },
        { "artillery", CL_GUN }, { "rifle", CL_RIFLE }, { "air", CL_AIR }, { "partisans", CL_RIFLE },
        { "motor", CL_RIFLE }, { "tank", CL_TANK }, { "missile", CL_VEH }, { "airdef", CL_VEH } };
    if (u < 0 || u >= UNIT_COUNT) return CL_RIFLE;
    for (int i = 0; i < (int)(sizeof M / sizeof M[0]); i++) if (!strcmp(UNIT_TYPES[u].id, M[i].id)) return M[i].cl;
    return CL_RIFLE;
}

/* свои — героини: форма по виду войск */
static Look own_look(int u)
{
    Look L = { RGB(120, 118, 72), RGB(70, 52, 34), RGB(104, 102, 60), RGB(70, 72, 50), RGB(236, 196, 164), RGB(122, 76, 40), RGB(170, 172, 178),
               HK_CAP, 1, 0, 0, 1 };
    const char *id = u >= 0 && u < UNIT_COUNT ? UNIT_TYPES[u].id : "rifle";
    if (!strcmp(id, "druzhina"))      { L.coat = RGB(150, 152, 160); L.trim = RGB(176, 40, 40); L.hat = L.metal; L.hat_kind = HK_POINT; L.pants = RGB(96, 70, 48); L.star = 0; }
    else if (!strcmp(id, "crossbow")) { L.coat = RGB(132, 92, 54); L.trim = RGB(80, 56, 32); L.hat = RGB(96, 66, 38); L.hat_kind = HK_HAT; L.pants = RGB(90, 70, 50); L.star = 0; }
    else if (!strcmp(id, "khorugv"))  { L.coat = RGB(176, 44, 44); L.trim = RGB(230, 220, 200); L.hat = L.metal; L.hat_kind = HK_POINT; L.star = 0; }
    else if (!strcmp(id, "hussars"))  { L.coat = RGB(170, 32, 44); L.trim = RGB(222, 190, 90); L.hat = L.metal; L.hat_kind = HK_HELM; L.wings = 1; L.star = 0; }
    else if (!strcmp(id, "gunners") || !strcmp(id, "musketeers")) { L.coat = RGB(150, 32, 42); L.trim = RGB(226, 200, 120); L.hat = RGB(50, 40, 34); L.hat_kind = HK_HAT; L.pants = RGB(60, 50, 44); L.star = 0; }
    else if (!strcmp(id, "rockets") || !strcmp(id, "line") || !strcmp(id, "artillery")) { L.coat = RGB(44, 84, 56); L.trim = RGB(232, 228, 214); L.hat = RGB(30, 30, 34); L.hat_kind = HK_SHAKO; L.pants = RGB(220, 216, 200); L.star = 0; }
    else if (!strcmp(id, "partisans")) { L.coat = RGB(70, 76, 84); L.trim = RGB(46, 40, 34); L.hat = RGB(196, 46, 40); L.hat_kind = HK_SCARF; L.pants = RGB(54, 56, 60); }
    else if (!strcmp(id, "tank"))     { L.coat = RGB(76, 98, 58); L.hat = RGB(50, 46, 40); L.hat_kind = HK_TANKER; }
    else if (!strcmp(id, "missile") || !strcmp(id, "airdef")) { L.coat = RGB(86, 104, 70); L.hat = RGB(70, 86, 56); L.hat_kind = HK_HELM; }
    return L;
}

/* враг — по году: дружины соседей, Тевтонский орден, московское войско, шведы и французы, вермахт */
static Look enemy_look(int year, int *cl)
{
    Look L = { RGB(96, 100, 96), RGB(40, 40, 40), RGB(84, 88, 84), RGB(70, 72, 70), RGB(232, 200, 172), RGB(90, 70, 50), RGB(150, 152, 158),
               HK_STAHL, 0, 0, 0, 0 };
    *cl = CL_RIFLE;
    if (year < 1250)      { L.coat = RGB(92, 92, 104); L.trim = RGB(120, 60, 40); L.hat = L.metal; L.hat_kind = HK_POINT; *cl = CL_SPEAR; }
    else if (year < 1450) { L.coat = RGB(226, 224, 216); L.trim = RGB(30, 30, 30); L.hat = L.metal; L.hat_kind = HK_HELM; L.cross = 1; *cl = CL_SPEAR; }
    else if (year < 1650) { L.coat = RGB(126, 40, 30); L.trim = RGB(220, 180, 90); L.hat = RGB(90, 60, 36); L.hat_kind = HK_FUR; *cl = CL_MUSKET; }
    else if (year < 1790) { L.coat = RGB(48, 70, 138); L.trim = RGB(220, 190, 80); L.hat = RGB(30, 30, 34); L.hat_kind = HK_HAT; *cl = CL_MUSKET; }
    else if (year < 1870) { L.coat = RGB(40, 58, 150); L.trim = RGB(236, 236, 236); L.hat = RGB(24, 24, 28); L.hat_kind = HK_SHAKO; L.pants = RGB(232, 232, 228); *cl = CL_MUSKET; }
    else if (year < 1941) { L.coat = RGB(104, 112, 96); L.hat = RGB(84, 90, 80); }
    else if (year <= 1945) { L.coat = RGB(98, 104, 98); L.hat = RGB(80, 86, 82); L.cross = 1; }
    return L;
}

/* ---------- примитивы ---------- */
static void quad(int x1, int y1, int x2, int y2, int x3, int y3, int x4, int y4, COLORREF fill, COLORREF edge)
{
    POINT p[5] = { { x1, y1 }, { x2, y2 }, { x3, y3 }, { x4, y4 }, { x1, y1 } };
    d_poly(p, 5, fill, edge, 1);
}

static void ell(int cx, int cy, int rx, int ry, COLORREF fill, COLORREF edge)
{
    enum { N = 14 };
    POINT p[N + 1];
    for (int i = 0; i < N; i++) { p[i].x = cx + (int)lround(cos(2 * PI * i / N) * rx); p[i].y = cy + (int)lround(sin(2 * PI * i / N) * ry); }
    p[N] = p[0];
    d_poly(p, N + 1, fill, edge, 1);
}

static COLORREF dark(COLORREF c) { return mix(c, RGB(0, 0, 0), 110); }

/* верхняя половина круга — шлем, каска, платок */
static void dome(int cx, int cy, int r, COLORREF fill)
{
    enum { N = 9 };
    POINT p[N + 2];
    for (int i = 0; i <= N; i++) { p[i].x = cx + (int)lround(cos(PI + PI * i / N) * r); p[i].y = cy + (int)lround(sin(PI + PI * i / N) * r); }
    p[N + 1] = p[0];
    d_poly(p, N + 2, fill, dark(fill), 1);
}

/* вспышка выстрела и облачко дыма; f — фаза 0..1 цикла стрельбы */
static void muzzle(int x, int y, int dir, int s, double f)
{
    if (f < 0.10) {
        int r = s * (int)(10 - f * 60) / 10 + 1;
        POINT star[9];
        for (int i = 0; i < 8; i++) {
            double a = i * PI / 4, rr = i % 2 ? r * 0.45 : r;
            star[i].x = x + dir * r / 2 + (int)(cos(a) * rr * 1.4);
            star[i].y = y + (int)(sin(a) * rr);
        }
        star[8] = star[0];
        d_poly(star, 9, RGB(255, 226, 120), RGB(255, 150, 40), 1);
    }
    if (f > 0.04 && f < 0.75) {   /* дым уплывает вверх и тает (светлеет к фону) */
        double g = (f - 0.04) / 0.71;
        int r = (int)(s * (0.6 + g * 1.4)) + 1;
        COLORREF c = mix(RGB(200, 200, 196), P.surface, (int)(g * 200));
        d_circle(x + dir * (int)(s * (1 + g * 2)), y - (int)(g * s * 3), r, c, CLR_INVALID, 1);
    }
}

/* ---------- головной убор и голова ---------- */
static void head(int x, int y, int r, int dir, const Look *L)
{
    if (L->braid && L->hat_kind != HK_SCARF) {   /* коса за спиной — героиня видна даже мелко */
        int bw = r * 7 / 10 > 2 ? r * 7 / 10 : 2;
        d_line(x - dir * r * 6 / 10, y + r / 3, x - dir * (r + r / 2), y + r * 3, dark(L->hair), bw + 1);
        d_line(x - dir * r * 6 / 10, y + r / 3, x - dir * (r + r / 2), y + r * 3, L->hair, bw);
        d_circle(x - dir * (r + r / 2), y + r * 3, bw / 2 + 1, RGB(200, 40, 40), CLR_INVALID, 1);   /* бантик */
    }
    d_circle(x, y, r, L->skin, dark(L->skin), 1);
    int t = r > 3 ? r / 3 : 1;
    switch (L->hat_kind) {
    case HK_POINT:   /* шлем-шишак */
        quad(x - r - 1, y - r / 4, x + r + 1, y - r / 4, x + r / 3, y - r * 2, x - r / 3, y - r * 2, L->hat, dark(L->hat));
        break;
    case HK_HELM:    /* круглый шлем */
        dome(x, y - r / 5, r + 1, L->hat);
        d_fill(x - r - 1, y - r / 4, 2 * r + 3, t, dark(L->hat));
        if (L->cross) { d_fill(x - t, y - r, 2 * t, r, RGB(20, 20, 20)); }
        break;
    case HK_HAT:     /* шляпа-треуголка */
        quad(x - r - r / 2, y - r / 2, x + r + r / 2, y - r / 2, x + r / 2, y - r * 3 / 2, x - r / 2, y - r * 3 / 2, L->hat, dark(L->hat));
        break;
    case HK_SHAKO:   /* кивер */
        d_fill(x - r + 1, y - r * 2, 2 * r - 1, r * 3 / 2, L->hat);
        d_fill(x - r, y - r / 2, 2 * r + 1, t, L->trim);
        break;
    case HK_CAP:     /* пилотка со звёздочкой */
        quad(x - r, y - r / 2, x + r, y - r / 2, x + r - 1, y - r, x - r + 1, y - r * 3 / 2, L->hat, dark(L->hat));
        if (L->star && r >= 4) d_circle(x + dir * r / 3, y - r * 3 / 4, t > 1 ? t : 1, RGB(214, 40, 34), CLR_INVALID, 1);
        break;
    case HK_SCARF:   /* платок партизанки */
        dome(x, y - r / 6, r + 1, L->hat);
        d_line(x - dir * r, y, x - dir * (r + r / 2), y + r, L->hat, t + 1);
        break;
    case HK_STAHL:   /* стальная каска */
        dome(x, y - r / 6, r + 2, L->hat);
        d_fill(x - r - 2, y - r / 6, 2 * r + 5, t + 1, dark(L->hat));
        break;
    case HK_FUR:     /* меховая шапка */
        ell(x, y - r * 3 / 4, r + 1, r * 3 / 4, L->hat, dark(L->hat));
        break;
    case HK_TANKER:  /* шлемофон */
        dome(x, y - r / 6, r + 1, L->hat);
        break;
    }
}

/* ---------- пехотинец: ноги шагают, оружие на плече или наизготовку ---------- */
static void soldier(int x, int y, int h, const Look *L, int cl, int dir, int mode, double t, double ph)
{
    double cyc = mode == 1 ? sin((t * 1.6 + ph) * 2 * PI) : mode == 2 ? 0.55 : 0.12;   /* шаг ≈ 1,6 в секунду; в бою — стойка */
    int bob = mode == 1 ? (int)(fabs(cyc) * h * 0.04) : 0;
    int hipY = y - h * 45 / 100 - bob, shY = y - h * 80 / 100 - bob, hdY = y - h * 90 / 100 - bob;
    int lw = h / 11 > 1 ? h / 11 : 1;
    /* ноги */
    int sw = (int)(cyc * h * 0.17);
    d_line(x, hipY, x + sw, y, L->pants, lw + 1);
    d_line(x, hipY, x - sw, y, dark(L->pants), lw + 1);
    /* туловище */
    int bw = h * 12 / 100 + 1;
    quad(x - bw, hipY + 1, x + bw, hipY + 1, x + bw - 1, shY, x - bw + 1, shY, L->coat, dark(L->coat));
    d_fill(x - bw, hipY - h / 12, 2 * bw + 1, lw, L->trim);   /* ремень */
    if (L->cross && bw > 3) { d_fill(x - lw / 2, shY + 2, lw, hipY - shY - 4, RGB(24, 24, 24)); d_fill(x - bw + 2, (shY + hipY) / 2 - lw / 2, 2 * bw - 3, lw, RGB(24, 24, 24)); }
    int r = h * 10 / 100 + 1;
    head(x, hdY - r / 2, r, dir, L);
    /* оружие */
    double fire = fmod(t * 0.62 + ph * 0.37, 1.0);                     /* залп раз в ~1,6 с, каждый в свой миг */
    switch (cl) {
    case CL_SPEAR: {
        int tipX, tipY;
        if (mode == 2) { tipX = x + dir * (int)(h * (0.55 + 0.12 * sin((t + ph) * 2 * PI * 1.2))); tipY = shY - h / 12; }   /* удары копьём */
        else { tipX = x + dir * h / 6; tipY = y - h * 120 / 100 - bob; }
        d_line(x + dir * h / 10, shY + h / 6, tipX, tipY, RGB(120, 90, 56), lw);
        d_line(tipX, tipY, tipX + (tipX - x - dir * h / 10) / 8, tipY + (tipY - shY - h / 6) / 8, L->metal, lw + 1);
        ell(x + dir * h / 7, (shY + hipY) / 2, h / 7 + 1, h / 5 + 1, L->trim, dark(L->trim));   /* щит */
        break;
    }
    case CL_BOW: {
        int ax = x + dir * h * 30 / 100, ay = shY + h / 12;
        d_line(x, shY + h / 12, ax, ay, L->coat, lw);
        d_line(ax - dir * h / 6, ay, ax + dir * h / 5, ay, RGB(110, 80, 50), lw + 1);          /* ложе */
        d_line(ax + dir * h / 7, ay - h / 7, ax + dir * h / 7, ay + h / 7, RGB(90, 70, 46), lw);   /* дуга */
        if (mode == 2 && fire < 0.25) d_line(ax + dir * h / 5, ay, ax + dir * (int)(h * (0.4 + fire * 4)), ay - h / 20, RGB(60, 50, 40), 1);
        break;
    }
    default: {   /* ружьё / винтовка */
        if (mode == 2) {
            int mx2 = x + dir * h * 55 / 100, my2 = shY + h / 14;
            d_line(x - dir * h / 12, shY + h / 10, mx2, my2, RGB(70, 52, 34), lw + 1);
            d_line(x, shY + h / 12, x + dir * h * 25 / 100, my2, L->coat, lw);                    /* руки на прикладе */
            muzzle(mx2, my2, dir, h / 9 + 1, fire);
        } else {
            d_line(x - dir * h / 14, shY + h / 3, x + dir * h / 5, y - h * 110 / 100 - bob, RGB(70, 52, 34), lw + 1);
            d_line(x, shY + h / 12, x + dir * h / 10, shY + h / 4, L->coat, lw);
        }
        break;
    }
    }
}

/* ---------- всадница ---------- */
static void rider(int x, int y, int h, const Look *L, int dir, int mode, double t, double ph)
{
    double g = (t * 2.2 + ph) * 2 * PI;
    int moving = mode != 0;
    int by = y - h * 42 / 100 - (moving ? (int)(fabs(sin(g)) * h * 0.05) : 0);
    int bl = h * 36 / 100, bh = h * 13 / 100 + 1, lw = h / 14 > 1 ? h / 14 : 1;
    COLORREF horse = RGB(110, 72, 44);
    /* ноги коня: галоп */
    for (int i = 0; i < 4; i++) {
        int lx = x + (i < 2 ? -bl * 6 / 10 : bl * 6 / 10) + (i % 2 ? h / 20 : -h / 20);
        int sw = moving ? (int)(sin(g + i * 1.6) * h * 0.12) : 0;
        d_line(lx, by + bh / 2, lx + sw, y, dark(horse), lw + 1);
    }
    ell(x, by, bl, bh, horse, dark(horse));
    /* шея и голова */
    int nx = x + dir * bl * 8 / 10, ny = by - bh;
    quad(x + dir * bl / 2, by - bh / 2, nx, ny - h / 7, nx + dir * h / 8, ny - h / 9, x + dir * bl * 7 / 10, by, horse, dark(horse));
    d_line(x - dir * bl, by - bh / 3, x - dir * (bl + h / 8), by + bh, dark(horse), lw + 1);   /* хвост */
    /* всадница */
    int sy = by - bh - h * 28 / 100;
    quad(x - h / 10, by - bh / 2, x + h / 10, by - bh / 2, x + h / 12, sy, x - h / 12, sy, L->coat, dark(L->coat));
    int r = h * 9 / 100 + 1;
    head(x, sy - r, r, dir, L);
    if (L->wings) {   /* крылья гусарии */
        for (int k = 0; k < 6; k++) {
            double a = -PI / 2 - dir * (0.25 + k * 0.1);
            d_line(x - dir * h / 10, by - bh, x - dir * h / 10 + (int)(cos(a) * h * 0.5), by - bh + (int)(sin(a) * h * 0.5), RGB(236, 232, 220), lw);
        }
    }
    /* копьё с флажком; в бою — наклонено вперёд */
    int tipX = x + dir * (mode == 2 ? h * 8 / 10 : h / 4), tipY = mode == 2 ? sy : sy - h * 6 / 10;
    d_line(x - dir * h / 10, by, tipX, tipY, RGB(130, 100, 64), lw);
    quad(tipX, tipY, tipX - dir * h / 6, tipY + h / 20, tipX - dir * h / 6, tipY + h / 8, tipX, tipY + h / 10, L->trim, dark(L->trim));
}

/* ---------- танк: гусеницы катятся, башня стреляет ---------- */
static void tank(int x, int y, int h, const Look *L, int dir, int mode, double t, double ph, int enemy)
{
    int len = h * 80 / 100, hh = h * 22 / 100 + 1, tr = h * 12 / 100 + 1;
    COLORREF body = enemy ? RGB(92, 98, 94) : L->coat, d = dark(body);
    /* гусеница */
    int ty = y - tr;
    d_rrect(x - len / 2, ty - tr / 2, len, tr + 1, tr / 2 + 1, RGB(44, 44, 40), RGB(24, 24, 22));
    double roll = mode == 1 ? fmod(t * 3 + ph, 1.0) : 0;
    for (int i = 0; i < 5; i++) {
        int wx = x - len / 2 + tr / 2 + (int)((i + roll) * (len - tr) / 5);
        if (wx < x + len / 2 - tr / 3) d_circle(wx, ty, tr / 3 + 1, RGB(80, 80, 74), CLR_INVALID, 1);
    }
    /* корпус и башня */
    quad(x - len / 2 + tr / 3, ty - tr / 2, x + len / 2 - tr / 3, ty - tr / 2, x + len / 2 - tr, ty - tr / 2 - hh, x - len / 2 + tr, ty - tr / 2 - hh, body, d);
    int tx = x - dir * len / 12, tyy = ty - tr / 2 - hh;
    quad(tx - len / 5, tyy, tx + len / 5, tyy, tx + len / 7, tyy - hh * 8 / 10, tx - len / 7, tyy - hh * 8 / 10, body, d);
    double rec = 0, f = fmod(t * 0.45 + ph * 0.3, 1.0);
    if (mode == 2 && f < 0.1) rec = (0.1 - f) * 10;
    int bx = tx + dir * (len / 7 + (int)(len * 0.45) - (int)(rec * len * 0.08)), byy = tyy - hh * 4 / 10;
    d_line(tx + dir * len / 7, byy, bx, byy, d, h / 16 + 1);
    if (enemy && L->cross) { d_fill(x - h / 16, ty - tr / 2 - hh * 8 / 10, h / 8 + 1, h / 30 + 1, RGB(20, 20, 20)); d_fill(x - h / 60, ty - tr / 2 - hh * 8 / 10 - h / 16, h / 30 + 1, h / 8 + 1, RGB(20, 20, 20)); }
    else if (!enemy) d_circle(x - dir * len / 6, ty - tr / 2 - hh / 2, h / 22 + 1, RGB(214, 40, 34), CLR_INVALID, 1);   /* звезда */
    if (mode == 2) muzzle(bx, byy, dir, h / 8 + 1, f);
    if (mode == 1) {   /* пыль из-под гусениц */
        double g = fmod(t * 1.3 + ph, 1.0);
        d_circle(x - dir * (len / 2 + (int)(g * h * 0.3)), y - (int)(g * h * 0.15), (int)(h * (0.05 + g * 0.08)) + 1,
                 mix(RGB(170, 150, 120), P.surface, (int)(g * 200)), CLR_INVALID, 1);
    }
}

/* ---------- машина: ракетная установка или зенитка ---------- */
static void vehicle(int x, int y, int h, const Look *L, int dir, int mode, double t, double ph, int missile)
{
    int len = h * 80 / 100, hh = h * 18 / 100 + 1, wr = h * 8 / 100 + 1;
    COLORREF body = L->coat, d = dark(body);
    d_fill(x - len / 2, y - wr - hh, len, hh, body);
    d_frame(x - len / 2, y - wr - hh, len, hh, d);
    d_fill(dir > 0 ? x + len / 4 : x - len / 2, y - wr - hh * 2, len / 4, hh, body);   /* кабина */
    for (int i = 0; i < 3; i++) d_circle(x - len / 3 + i * len / 3, y - wr, wr, RGB(40, 40, 38), RGB(20, 20, 20), 1);
    double f = fmod(t * 0.4 + ph * 0.3, 1.0);
    int px = x - dir * len / 8, py = y - wr - hh;
    if (missile) {
        int ex = px + dir * (int)(h * 0.45), ey = py - (int)(h * 0.35);
        d_line(px, py, ex, ey, RGB(220, 220, 214), h / 12 + 1);
        if (mode == 2 && f < 0.35) {   /* ракета уходит вверх с огнём */
            int rx = ex + dir * (int)(f * h * 2.4), ry = ey - (int)(f * h * 1.9);
            d_line(ex, ey, rx, ry, mix(RGB(255, 200, 100), P.surface, 80), 2);
            d_circle(rx, ry, h / 18 + 1, RGB(255, 220, 120), CLR_INVALID, 1);
        }
    } else {
        for (int k = 0; k < 2; k++) d_line(px + k * h / 18, py, px + dir * (int)(h * 0.3) + k * h / 18, py - (int)(h * 0.4), d, h / 22 + 1);
        if (mode == 2 && f < 0.5) {   /* трассеры вверх */
            int k = (int)(f * 10) % 2;
            d_line(px + dir * (int)(h * (0.3 + f * 0.6)), py - (int)(h * (0.4 + f * 0.9)), px + dir * (int)(h * (0.35 + f * 0.6)), py - (int)(h * (0.48 + f * 0.9)),
                   k ? RGB(255, 220, 120) : RGB(255, 160, 80), 2);
        }
    }
}

/* ---------- самолёт над отрядом ---------- */
static void plane(int x, int y, int h, int dir, int enemy, double t, double ph, int firing)
{
    int len = h * 70 / 100, w = h / 10 + 1;
    COLORREF body = enemy ? RGB(112, 118, 116) : RGB(76, 104, 66), d = dark(body);
    quad(x - len / 2, y - w / 2, x + len / 2, y - w / 3, x + len / 2, y + w / 3, x - len / 2, y + w / 2, body, d);   /* фюзеляж */
    quad(x - dir * len / 10, y, x + dir * len / 6, y, x + dir * len / 10, y + w * 2, x - dir * len / 6, y + w * 2, mix(body, RGB(255, 255, 255), 30), d);   /* крыло */
    quad(x - dir * len / 2, y - w / 2, x - dir * (len / 2 - w * 2), y - w / 2, x - dir * len / 2, y - w * 3, x - dir * len / 2, y - w * 3, body, d);  /* киль */
    if (enemy) d_fill(x - dir * len / 3 - w / 2, y - w / 2, w, w, RGB(20, 20, 20));
    else d_circle(x - dir * len / 3, y, w / 2 + 1, RGB(214, 40, 34), CLR_INVALID, 1);
    int px = x + dir * len / 2;   /* винт */
    int pr = (int)(fabs(sin((t * 9 + ph) * 2 * PI)) * w * 2) + 1;
    d_line(px, y - pr, px, y + pr, RGB(60, 60, 60), 1);
    if (firing) {
        double f = fmod(t * 1.5 + ph, 1.0);
        if (f < 0.5) d_line(px + dir * (int)(f * h), y + (int)(f * h * 0.6), px + dir * (int)((f + 0.1) * h), y + (int)((f + 0.1) * h * 0.6), RGB(255, 210, 110), 2);
    }
}

/* ---------- отряд ---------- */
void troops_draw(int x, int y, int size, int unit, int count, int dir, int mode, int enemy, int year, double t)
{
    if (size < SC(14)) size = SC(14);
    int cl;
    Look L;
    if (enemy) L = enemy_look(year, &cl);
    else { L = own_look(unit); cl = unit_class(unit); }
    if (count < 1) count = 1;
    int maxN = cl == CL_TANK || cl == CL_VEH ? 3 : cl == CL_CAV ? 4 : cl == CL_GUN ? 2 : cl == CL_AIR ? 3 : 9;
    int n = count < maxN ? count : maxN;
    int h = size * (cl == CL_TANK || cl == CL_VEH ? 52 : cl == CL_CAV ? 48 : 46) / 100;
    if (cl == CL_AIR) {   /* самолёты кружат над полем, на земле — аэродромная команда */
        for (int i = 0; i < n; i++) {
            double a = t * 0.7 + i * 2.1;
            int px = x + (int)(cos(a) * size * 0.45), py = y - size * 75 / 100 + (int)(sin(a) * size * 0.12);
            plane(px, py, h, sin(a) < 0 ? dir : -dir, enemy, t, i * 0.4, mode == 2);
        }
        soldier(x, y, h * 8 / 10, &L, CL_RIFLE, dir, mode == 1 ? 1 : 0, t, 0.3);
        return;
    }
    /* строй: две шеренги, задняя чуть выше и мельче; рисуем задних первыми */
    int cols = (n + 1) / 2, sp = cols > 1 ? size / cols : 0;
    for (int row = 1; row >= 0; row--)
        for (int i = 0; i < n; i++) {
            if ((i % 2) != row) continue;
            int c = i / 2;
            int fx = x - (cols - 1) * sp / 2 + c * sp + (row ? sp / 3 : 0) * -dir;
            int fy = y - (row ? h * 22 / 100 : 0);
            int fh = row ? h * 90 / 100 : h;
            double ph = i * 0.37 + (enemy ? 0.5 : 0);
            static const COLORREF HAIR[3] = { RGB(214, 172, 92), RGB(122, 76, 40), RGB(62, 42, 30) };
            if (!enemy) L.hair = HAIR[(i * 7 + 1) % 3];
            switch (cl) {
            case CL_CAV:  rider(fx, fy, fh, &L, dir, mode, t, ph); break;
            case CL_TANK: tank(fx, fy, fh, &L, dir, mode, t, ph, enemy); break;
            case CL_VEH:  vehicle(fx, fy, fh, &L, dir, mode, t, ph, unit >= 0 && !strcmp(UNIT_TYPES[unit].id, "missile")); break;
            case CL_GUN:
                if (i == 0) fx_cannon(fx, fy, fh * 80 / 100, dir, mode == 2 ? t + ph : 0.95);
                else soldier(fx - dir * fh / 3, fy, fh, &L, CL_RIFLE, dir, mode == 1 ? 1 : 0, t, ph);
                break;
            default: soldier(fx, fy, fh, &L, cl, dir, mode, t, ph); break;
            }
        }
    /* у вермахта в 1941–1944 — ещё и танк позади пехоты */
    if (enemy && year >= 1941 && year <= 1945 && count >= 4)
        tank(x + dir * -size * 55 / 100, y - h * 30 / 100, h * 95 / 100, &L, dir, mode, t, 0.2, 1);
}
