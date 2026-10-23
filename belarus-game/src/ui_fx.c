/* Анимации: взлёт ракеты с белорусскими космонавтами (заставка во весь экран) и стрельба пушек на карте.
 *
 * Всё рисуется процедурно: ракета, стартовый стол и корабль — фигурами GDI, а небо, Земля, огонь и дым —
 * прямо в пикселях заднего буфера (d_bits). Состояния кадра нет: положение каждой частицы — функция
 * времени, поэтому заставку можно пропустить, повторить и снять на скриншот в любой момент. */
#include "mapview.h"
#include "fx.h"
#include "war.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define PI 3.14159265358979

/* ---------- сценарий взлёта, секунды от начала заставки ---------- */
#define T_IGN   3.5     /* зажигание: двигатели выходят на режим */
#define T_LIFT  5.0     /* «Пуск!» — контакт подъёма */
#define T_SEP   8.6     /* отделение боковых блоков: вид с Земли, «крест Королёва» */
#define T_ORB   10.6    /* орбита */
#define T_END   12.6    /* рассказ о полёте */
#define T_DIP   0.25    /* затемнение при смене плана */

/* свои аргументы A_NEXT: окно взлёта во весь экран, других областей нажатия в нём нет */
enum { FX_SKIP = 7001, FX_REPLAY, FX_CLOSE, FX_CHAES_WATCH = FX_ARG_CHAES, FX_BERLIN_WATCH = FX_ARG_BERLIN, FX_WATCH = 7100 /* + индекс деятеля */ };
#define FX_BOARD (-2)   /* U.launchPerson: стенд с пушками для проверки (сцена 75) */
#define FX_TROOPS (-3)  /* U.launchPerson: галерея 3D-отрядов (сцена 39) */
#define FX_CHAES (-4)   /* U.launchPerson: авария на Чернобыльской АЭС (fx_chaes.inc) */
#define FX_BERLIN (-5)  /* U.launchPerson: штурм Берлина, Знамя Победы (fx_berlin.inc) */

/* небо старта: у каждого полёта своё время суток */
enum { SKY_GOLDEN, SKY_DUSK, SKY_NIGHT };
/* станция на орбите */
enum { ST_NONE, ST_SALYUT, ST_ISS };

typedef struct {
    const wchar_t *place;     /* надпись над названием корабля */
    const wchar_t *ship;
    const wchar_t *crew;
    const wchar_t *date;      /* чипы карточки: дата, цель, длительность */
    const wchar_t *goal;
    const wchar_t *length;
    const wchar_t *eyebrow;   /* строка над именем в карточке */
    const wchar_t *orbit;     /* подпись на орбите */
    const wchar_t *text;      /* рассказ в карточке */
    int sky, station;
    int modern;               /* 1 — 2024 год: флаг Беларуси на обтекателе */
    int gold;                 /* золотая карточка: первая женщина-космонавт суверенной Беларуси */
} Mission;

/* в том же порядке, что FX_FLIGHTS (fx.h) */
static const Mission MISSIONS[] = {
    { L"БАЙКОНУР · 18 ДЕКАБРЯ 1973 ГОДА", L"«Союз-13»", L"Экипаж: Пётр Климук и Валентин Лебедев",
      L"18 декабря 1973 года", L"телескоп «Орион-2»", L"почти 8 суток",
      L"ПЕРВЫЙ КОСМОНАВТ ИЗ БЕЛАРУСИ · 1973",
      L"Орбита! Почти 8 суток «Союз-13» будет изучать звёзды",
      L"Родился в деревне Комаровка на Брестчине. В 31 год стал первым космонавтом из Беларуси: на «Союзе-13» "
      L"вместе с Валентином Лебедевым снимал звёзды в ультрафиолете телескопом «Орион-2». Потом были ещё два полёта — "
      L"в 1975 году (63 суток на станции «Салют-4») и в 1978-м. Дважды Герой Советского Союза.",
      SKY_DUSK, ST_NONE, 0, 0 },
    { L"БАЙКОНУР · 15 ИЮНЯ 1978 ГОДА", L"«Союз-29»", L"Экипаж: Владимир Ковалёнок и Александр Иванченков",
      L"15 июня 1978 года", L"станция «Салют-6»", L"140 суток — рекорд",
      L"РЕКОРД ТОГО ВРЕМЕНИ: 140 СУТОК НА ОРБИТЕ",
      L"Орбита! Впереди — станция «Салют-6» и 140 суток полёта",
      L"Родился в деревне Белое Крупского района. Летал в космос трижды: в 1977 году на «Союзе-25», в 1978-м "
      L"провёл 140 суток на станции «Салют-6» вместе с Александром Иванченковым — рекорд того времени, а в 1981-м "
      L"снова командовал экспедицией на «Салюте-6». Дважды Герой Советского Союза.",
      SKY_NIGHT, ST_SALYUT, 0, 0 },
    { L"БАЙКОНУР · 23 МАРТА 2024 ГОДА", L"«Союз МС-25»", L"Экипаж: Олег Новицкий, Марина Василевская, Трейси Дайсон",
      L"23 марта 2024 года", L"МКС", L"около двух недель",
      L"ПОЛЁТ НА МКС · 2024",
      L"Орбита! Через двое суток — стыковка с МКС",
      L"Минчанка, бывшая бортпроводница «Белавиа». Стартовала с Байконура вместе с командиром Олегом Новицким, "
      L"уроженцем Червеня, и Трейси Дайсон из NASA. На МКС выполнила белорусскую научную программу, а 6 апреля "
      L"вернулась на Землю. Первый космонавт суверенной Беларуси и первая белоруска в космосе.",
      SKY_GOLDEN, ST_ISS, 1, 1 },
    { L"БАЙКОНУР · 23 МАРТА 2024 ГОДА", L"«Союз МС-25»", L"Экипаж: Олег Новицкий, Марина Василевская, Трейси Дайсон",
      L"23 марта 2024 года", L"МКС", L"четыре полёта",
      L"КОМАНДИР «СОЮЗА МС-25» · 2024",
      L"Орбита! Через двое суток — стыковка с МКС",
      L"Уроженец Червеня, четыре полёта в космос: 2012, 2016, 2021 и 2024 годы. В последнем командовал «Союзом МС-25», "
      L"который доставил на МКС Марину Василевскую — первую белоруску в космосе.",
      SKY_GOLDEN, ST_ISS, 1, 0 },
};

#define GOLD_TITLE  L"Первая женщина-космонавт суверенной Беларуси"
#define WOMEN_TITLE L"Женщины Беларуси — в науке и в космосе"
#define WOMEN_TEXT  L"Учёные, инженеры, врачи — а теперь и космонавт. Полёт Марины Василевской показал: дорога к звёздам " \
                    L"открыта каждой девочке, которая учится и мечтает."

static double s_devT = -1;          /* сцены разработчика: замороженное время заставки, с */
static int s_back = M_NONE, s_backArg;   /* куда вернуться после заставки (окно деятеля) */

/* ---------- мелкие помощники ---------- */
static double clampd(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
static double sstep(double a, double b, double v) { v = clampd((v - a) / (b - a), 0, 1); return v * v * (3 - 2 * v); }

/* детерминированное «случайное» число 0…1 */
static double rnd(int i)
{
    unsigned x = (unsigned)i * 2654435761u + 0x9E3779B9u;
    x ^= x >> 15; x *= 0x85EBCA6Bu; x ^= x >> 13; x *= 0xC2B2AE35u; x ^= x >> 16;
    return (x & 0xFFFFFF) / 16777216.0;
}

static int person_index(const char *id)
{
    for (int i = 0; i < PEOPLE_COUNT; i++) if (!strcmp(PEOPLE[i].id, id)) return i;
    return -1;
}

/* цвета пикселей буфера: 0xRRGGBB (у COLORREF порядок обратный) */
#define RGBX(r, g, b) (((unsigned)(r) << 16) | ((unsigned)(g) << 8) | (unsigned)(b))
static unsigned xmix(unsigned a, unsigned b, double t)
{
    int k = (int)(clampd(t, 0, 1) * 256);
    int r = (int)((a >> 16) & 255), g = (int)((a >> 8) & 255), bl = (int)(a & 255);
    r += (((int)((b >> 16) & 255) - r) * k) >> 8;
    g += (((int)((b >> 8) & 255) - g) * k) >> 8;
    bl += (((int)(b & 255) - bl) * k) >> 8;
    return RGBX(r, g, bl);
}
static COLORREF cr(unsigned x) { return RGB((x >> 16) & 255, (x >> 8) & 255, x & 255); }

/* ---------- пиксели заднего буфера ---------- */
typedef struct { unsigned *px; int stride, x0, y0, x1, y1; } Cv;   /* x1, y1 — не включительно */

static int cv_get(Cv *c)
{
    GdiFlush();                       /* сначала GDI дорисовывает всё, что держит в очереди */
    c->px = d_bits();
    c->stride = g_w;
    c->x0 = 0; c->y0 = 0; c->x1 = g_w; c->y1 = g_h;
    int x, y, w, h;
    if (d_clip_get(&x, &y, &w, &h)) {    /* как и GDI, не выходим за отсечение */
        if (x > c->x0) c->x0 = x;
        if (y > c->y0) c->y0 = y;
        if (x + w < c->x1) c->x1 = x + w;
        if (y + h < c->y1) c->y1 = y + h;
    }
    return c->px && c->x1 > c->x0 && c->y1 > c->y0;
}

/* спад по квадрату расстояния от центра: [0] — дым с плотной серединой и мягким краем, [1] — свечение */
static unsigned char s_lut[2][256];
static void lut_init(void)
{
    if (s_lut[0][0]) return;
    for (int i = 0; i < 256; i++) {
        double v = 1 - i / 256.0, s = clampd(v / 0.6, 0, 1);
        s_lut[0][i] = (unsigned char)(255 * s * s * (3 - 2 * s) + 0.5);
        s_lut[1][i] = (unsigned char)(255 * v * v + 0.5);
    }
}

/* Мягкое пятно-эллипс. add = 0 — полупрозрачный дым (сверху цвет top, снизу bot), 1 — сложение света. */
static void px_blob(const Cv *c, double cx, double cy, double rx, double ry, unsigned top, unsigned bot, int a, int add)
{
    if (a <= 0 || rx < 0.7 || ry < 0.7) return;
    if (a > 255) a = 255;
    int ya = (int)floor(cy - ry), yb = (int)ceil(cy + ry), xa = (int)floor(cx - rx), xb = (int)ceil(cx + rx);
    if (ya < c->y0) ya = c->y0;
    if (yb > c->y1) yb = c->y1;
    if (xa < c->x0) xa = c->x0;
    if (xb > c->x1) xb = c->x1;
    if (xa >= xb || ya >= yb) return;
    lut_init();
    const unsigned char *L = s_lut[add ? 1 : 0];
    double irx = 1.0 / rx, iry = 1.0 / ry;
    int tr = (int)((top >> 16) & 255), tg = (int)((top >> 8) & 255), tb = (int)(top & 255);
    int br = (int)((bot >> 16) & 255), bgr = (int)((bot >> 8) & 255), bb = (int)(bot & 255);
    for (int y = ya; y < yb; y++) {
        double ny = (y + 0.5 - cy) * iry, qy = ny * ny;
        if (qy >= 1) continue;
        double hw = rx * sqrt(1 - qy);
        int xs = (int)(cx - hw), xe = (int)(cx + hw) + 1;
        if (xs < xa) xs = xa;
        if (xe > xb) xe = xb;
        int m = (int)((ny + 1) * 128);
        int sr = tr + (((br - tr) * m) >> 8), sg = tg + (((bgr - tg) * m) >> 8), sb = tb + (((bb - tb) * m) >> 8);
        unsigned *row = c->px + (size_t)y * c->stride;
        for (int x = xs; x < xe; x++) {
            double nx = (x + 0.5 - cx) * irx, q = nx * nx + qy;
            if (q >= 1) continue;
            int w = (L[(int)(q * 256)] * a) >> 8;
            if (w <= 0) continue;
            unsigned d = row[x];
            int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
            if (add) {
                dr += (sr * w) >> 8; dg += (sg * w) >> 8; db += (sb * w) >> 8;
                if (dr > 255) dr = 255;
                if (dg > 255) dg = 255;
                if (db > 255) db = 255;
            } else {
                dr += ((sr - dr) * w) >> 8; dg += ((sg - dg) * w) >> 8; db += ((sb - db) * w) >> 8;
            }
            row[x] = RGBX(dr, dg, db);
        }
    }
}

/* полупрозрачный прямоугольник со скруглёнными углами (подложки надписей) */
static void px_rrect(const Cv *c, int x, int y, int w, int h, int r, unsigned col, int a)
{
    int cr_ = (int)((col >> 16) & 255), cg = (int)((col >> 8) & 255), cb = (int)(col & 255);
    if (r > h / 2) r = h / 2;
    for (int yy = y; yy < y + h; yy++) {
        if (yy < c->y0 || yy >= c->y1) continue;
        double dy = yy < y + r ? y + r - yy - 0.5 : yy >= y + h - r ? yy - (y + h - r) + 0.5 : 0;
        int in = dy > 0 ? (int)(r - sqrt(r * r - dy * dy) + 0.5) : 0;
        int xs = x + in, xe = x + w - in;
        if (xs < c->x0) xs = c->x0;
        if (xe > c->x1) xe = c->x1;
        unsigned *row = c->px + (size_t)yy * c->stride;
        for (int xx = xs; xx < xe; xx++) {
            unsigned d = row[xx];
            int dr = (int)((d >> 16) & 255), dg = (int)((d >> 8) & 255), db = (int)(d & 255);
            dr += ((cr_ - dr) * a) >> 8; dg += ((cg - dg) * a) >> 8; db += ((cb - db) * a) >> 8;
            row[xx] = RGBX(dr, dg, db);
        }
    }
}

/* вертикальная заливка по опорным цветам с упорядоченным размыванием (без полос на градиенте) */
typedef struct { double at; unsigned c; } Stop;
static void grad_at(const Stop *s, int n, double v, double *rgb)
{
    int i = 0;
    while (i < n - 2 && v > s[i + 1].at) i++;
    double t = clampd((v - s[i].at) / (s[i + 1].at - s[i].at), 0, 1);
    for (int k = 0; k < 3; k++) {
        int sh = 16 - 8 * k;
        double a = (s[i].c >> sh) & 255, b = (s[i + 1].c >> sh) & 255;
        rgb[k] = a + (b - a) * t;
    }
}
static void px_grad(const Cv *c, const Stop *s, int n, double y0, double y1, int ya, int yb)
{
    static const int D[2][2] = { { 0, 8 }, { 12, 4 } };
    if (ya < c->y0) ya = c->y0;
    if (yb > c->y1) yb = c->y1;
    for (int y = ya; y < yb; y++) {
        double rgb[3];
        grad_at(s, n, (y - y0) / (y1 - y0), rgb);
        unsigned v[2];
        for (int k = 0; k < 2; k++) {
            int ch[3];
            for (int j = 0; j < 3; j++) {
                ch[j] = ((int)(rgb[j] * 16) + D[y & 1][k]) >> 4;
                if (ch[j] > 255) ch[j] = 255;
                if (ch[j] < 0) ch[j] = 0;
            }
            v[k] = RGBX(ch[0], ch[1], ch[2]);
        }
        unsigned *row = c->px + (size_t)y * c->stride;
        for (int x = c->x0; x < c->x1; x++) row[x] = v[x & 1];
    }
}

static void px_add(const Cv *c, int x, int y, int r, int g, int b)
{
    if (x < c->x0 || x >= c->x1 || y < c->y0 || y >= c->y1) return;
    unsigned *p = c->px + (size_t)y * c->stride + x;
    int dr = (int)((*p >> 16) & 255) + r, dg = (int)((*p >> 8) & 255) + g, db = (int)(*p & 255) + b;
    *p = RGBX(dr > 255 ? 255 : dr, dg > 255 ? 255 : dg, db > 255 ? 255 : db);
}

/* звёзды выше линии ymax; alpha — насколько темно */
static void px_stars(const Cv *c, int n, double alpha, double t, double ymax, int seed)
{
    if (alpha <= 0.02) return;
    for (int i = 0; i < n; i++) {
        int x = (int)(rnd(seed + i * 3) * g_w), y = (int)(rnd(seed + i * 3 + 1) * g_h);
        if (y >= ymax) continue;
        double br = (0.22 + 0.78 * pow(rnd(seed + i * 3 + 2), 3)) * alpha * (0.82 + 0.18 * sin(t * 2.3 + i));
        int a = (int)(br * 255);
        px_add(c, x, y, a, a, a + a / 8);
        if (br > 0.55) {
            px_add(c, x + 1, y, a / 3, a / 3, a / 3);
            px_add(c, x - 1, y, a / 3, a / 3, a / 3);
            px_add(c, x, y + 1, a / 3, a / 3, a / 3);
            px_add(c, x, y - 1, a / 3, a / 3, a / 3);
        }
    }
}

/* тайловый шум 128×128 для облаков и суши на Земле (считается один раз) */
static unsigned char s_noise[128 * 128];
static void noise_init(void)
{
    static int done;
    if (done) return;
    done = 1;
    for (int y = 0; y < 128; y++)
        for (int x = 0; x < 128; x++) {
            double v = 0, amp = 0.5;
            for (int o = 0; o < 4; o++) {
                int cell = 32 >> o, n = 128 / cell;
                double fx = (double)x / cell, fy = (double)y / cell;
                int ix = (int)fx, iy = (int)fy;
                double ux = fx - ix, uy = fy - iy;
                ux = ux * ux * (3 - 2 * ux); uy = uy * uy * (3 - 2 * uy);
                int x0 = ix % n, x1 = (ix + 1) % n, y0 = iy % n, y1 = (iy + 1) % n;
                double a = rnd(o * 10007 + y0 * n + x0), b = rnd(o * 10007 + y0 * n + x1);
                double c2 = rnd(o * 10007 + y1 * n + x0), d = rnd(o * 10007 + y1 * n + x1);
                v += amp * ((a + (b - a) * ux) * (1 - uy) + (c2 + (d - c2) * ux) * uy);
                amp *= 0.5;
            }
            s_noise[y * 128 + x] = (unsigned char)(clampd((v / 0.9375 - 0.5) * 1.8 + 0.5, 0, 1) * 255);
        }
}
static int noise_at(double u, double v)
{
    int iu = (int)floor(u), iv = (int)floor(v);
    int fu = (int)((u - iu) * 256), fv = (int)((v - iv) * 256);
    int x0 = iu & 127, x1 = (iu + 1) & 127, y0 = iv & 127, y1 = (iv + 1) & 127;
    int a = s_noise[y0 * 128 + x0], b = s_noise[y0 * 128 + x1], c = s_noise[y1 * 128 + x0], d = s_noise[y1 * 128 + x1];
    int top = a + (((b - a) * fu) >> 8), bot = c + (((d - c) * fu) >> 8);
    return top + (((bot - top) * fv) >> 8);
}

/* ---------- шрифты заставки (свои: крупнее, чем в интерфейсе) ---------- */
static HFONT fx_font(int px, int weight)
{
    static HFONT f[2];
    static int have[2];
    int k = weight >= FW_BOLD;
    if (px < 8) px = 8;
    if (f[k] && have[k] == px) return f[k];
    if (f[k]) DeleteObject(f[k]);
    f[k] = CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    have[k] = px;
    return f[k];
}
/* текст своим шрифтом; center — x задаёт середину строки; возвращает ширину */
static int fx_text(int x, int y, const wchar_t *s, HFONT f, COLORREF c, int center)
{
    HDC dc = d_dc();
    HGDIOBJ o = SelectObject(dc, f);
    SIZE sz;
    int n = (int)wcslen(s);
    GetTextExtentPoint32W(dc, s, n, &sz);
    SetTextColor(dc, c);
    TextOutW(dc, center ? x - sz.cx / 2 : x, y, s, n);
    SelectObject(dc, o);
    return sz.cx;
}
/* надпись с тенью для неба: читается и на светлом закате, и на чёрном космосе */
static void stext(int x, int y, const wchar_t *s, int f, COLORREF c)
{
    int o = SC(1) > 1 ? SC(1) : 1;
    d_text(x + o, y + o, s, f, RGB(0, 0, 0));
    d_text(x, y, s, f, c);
}

/* ---------- геометрия: локальные метры -> экран ---------- */
typedef struct { double ox, oy, k, c, s; } Xf;   /* экран = (ox, oy) + поворот(x, y) · k; y вверх */
static void xpd(const Xf *f, double x, double y, double *sx, double *sy)
{
    *sx = f->ox + (x * f->c + y * f->s) * f->k;
    *sy = f->oy - (-x * f->s + y * f->c) * f->k;
}
static POINT xp(const Xf *f, double x, double y)
{
    double sx, sy;
    xpd(f, x, y, &sx, &sy);
    POINT p = { (LONG)lround(sx), (LONG)lround(sy) };
    return p;
}
static void xline(const Xf *f, double x0, double y0, double x1, double y1, unsigned col, int t)
{
    POINT a = xp(f, x0, y0), b = xp(f, x1, y1);
    d_line(a.x, a.y, b.x, b.y, cr(col), t < 1 ? 1 : t);
}
static void xquad(const Xf *f, double x0, double y0, double x1, double y1, double x2, double y2, double x3, double y3,
                  unsigned fill, unsigned edge)
{
    POINT p[4] = { xp(f, x0, y0), xp(f, x1, y1), xp(f, x2, y2), xp(f, x3, y3) };
    d_poly(p, 4, cr(fill), cr(edge), 1);
}

/* свет текущего плана: тёплый закат, сумерки или прожекторы ночью */
static unsigned s_light, s_shade, s_tint;
static double s_tintAmt;
static unsigned lit(unsigned base) { return xmix(base, s_tint, s_tintAmt); }

/* Тело вращения вдоль оси из (ax, ay) под углом ang к вертикали (плюс — наклон вправо); pr — пары
 * «вдоль оси, радиус» в метрах. Свет слева от оси: светлая полоса, справа — тень и тёмный край. */
static void body(const Xf *f, double ax, double ay, double ang, const double *pr, int n, unsigned col)
{
    static const double BANDS[4][2] = { { -1, 1 }, { -0.85, -0.22 }, { 0.3, 1 }, { 0.74, 1 } };
    double dx = sin(ang), dy = cos(ang), nx = dy, ny = -dx;
    if (n > 16) n = 16;
    for (int b = 0; b < 4; b++) {
        POINT p[32];
        int k = 0;
        for (int i = 0; i < n; i++) {
            double s = pr[2 * i], r = pr[2 * i + 1] * BANDS[b][0];
            p[k++] = xp(f, ax + dx * s + nx * r, ay + dy * s + ny * r);
        }
        for (int i = n - 1; i >= 0; i--) {
            double s = pr[2 * i], r = pr[2 * i + 1] * BANDS[b][1];
            p[k++] = xp(f, ax + dx * s + nx * r, ay + dy * s + ny * r);
        }
        unsigned c = b == 0 ? col : b == 1 ? xmix(col, s_light, 0.45) : b == 2 ? xmix(col, s_shade, 0.32) : xmix(col, s_shade, 0.58);
        d_poly(p, k, cr(c), b == 0 ? cr(xmix(col, s_shade, 0.72)) : CLR_INVALID, 1);
    }
}

/* ---------- ракета «Союз» (метры; начало — центр сопел, ось вверх) ---------- */
static const double BOOSTER[] = { 0, 1.34, 13.8, 0.98, 17.6, 0.58, 19.0, 0.26, 19.6, 0.04 };
static const double BOOSTER_BAY[] = { 0, 1.34, 1.7, 1.31 };

static void nozzle(const Xf *f, double x, unsigned col)
{
    xquad(f, x - 0.18, 0.45, x + 0.18, 0.45, x + 0.33, -0.25, x - 0.33, -0.25, col, xmix(col, 0, 0.5));
}

static void rocket_draw(const Xf *f, const Mission *m)
{
    unsigned white = lit(RGBX(238, 238, 232)), grey = lit(RGBX(212, 213, 208)), mid = lit(RGBX(150, 152, 150));
    unsigned dark = lit(RGBX(66, 68, 74)), bay = lit(RGBX(96, 98, 102));
    /* боковые блоки (наклонены к центральному) с соплами и воздушными рулями */
    for (int sd = -1; sd <= 1; sd += 2) {
        nozzle(f, sd * 2.1, dark);
        nozzle(f, sd * 3.0, dark);
        double ang = -sd * 0.051;
        body(f, sd * 2.55, 0.45, ang, BOOSTER, 5, grey);
        body(f, sd * 2.55, 0.45, ang, BOOSTER_BAY, 2, bay);
        xquad(f, sd * 3.75, 0.55, sd * 4.6, 0.2, sd * 4.6, 1.5, sd * 3.75, 2.5, mid, xmix(mid, s_shade, 0.6));
    }
    /* центральный блок, ферма, третья ступень */
    static const double CORE[] = { 0, 1.05, 16.4, 1.07, 19.8, 1.47, 27.0, 1.40 };
    body(f, 0, 0.6, 0, CORE, 4, white);
    for (int i = 0; i < 2; i++) {
        double y = 22.6 + i * 2.6;
        xline(f, -1.42, y, 1.42, y, xmix(white, s_shade, 0.25), 1);
    }
    static const double TRUSS[] = { 0, 1.3, 1.3, 1.3 };
    body(f, 0, 27.6, 0, TRUSS, 2, dark);
    for (int i = 0; i < 3; i++) {   /* решётчатый переходник */
        double x = -1.3 + i * 0.866;
        xline(f, x, 27.6, x + 0.433, 28.9, mid, 1);
        xline(f, x + 0.433, 28.9, x + 0.866, 27.6, mid, 1);
    }
    static const double STAGE3[] = { 0, 1.3, 6.3, 1.3 };
    body(f, 0, 28.9, 0, STAGE3, 2, grey);
    xline(f, -1.3, 31.6, 1.3, 31.6, xmix(grey, s_shade, 0.25), 1);
    /* головной обтекатель с системой аварийного спасения */
    static const double FAIRING[] = { 0, 1.5, 4.0, 1.5, 5.6, 1.12, 6.8, 0.5, 7.2, 0.26 };
    static const double FAIR_RING[] = { 0, 1.52, 0.4, 1.52 };
    static const double TOWER[] = { 0, 0.3, 1.8, 0.3, 2.1, 0.17, 3.9, 0.11, 4.6, 0.02 };
    body(f, 0, 35.2, 0, FAIRING, 5, white);
    body(f, 0, 35.2, 0, FAIR_RING, 2, mid);
    body(f, 0, 42.2, 0, TOWER, 5, mid);
    for (int sd = -1; sd <= 1; sd += 2)          /* решётчатые стабилизаторы */
        xquad(f, sd * 1.5, 38.1, sd * 2.15, 38.3, sd * 2.15, 39.0, sd * 1.5, 39.1, dark, dark);
    if (m->modern) {   /* флаг Беларуси на обтекателе */
        xquad(f, -0.62, 36.55, 0.62, 36.55, 0.62, 37.25, -0.62, 37.25, lit(RGBX(200, 32, 44)), lit(RGBX(200, 32, 44)));
        xquad(f, -0.62, 36.2, 0.62, 36.2, 0.62, 36.55, -0.62, 36.55, lit(RGBX(0, 122, 62)), lit(RGBX(0, 122, 62)));
        xquad(f, -0.62, 36.2, -0.42, 36.2, -0.42, 37.25, -0.62, 37.25, lit(RGBX(244, 240, 236)), lit(RGBX(200, 32, 44)));
    } else {           /* красная полоса с надписью «СССР» — флаг-прямоугольник */
        xquad(f, -0.62, 36.2, 0.62, 36.2, 0.62, 37.25, -0.62, 37.25, lit(RGBX(196, 30, 36)), lit(RGBX(150, 20, 26)));
        xquad(f, -0.5, 36.95, -0.32, 36.95, -0.32, 37.13, -0.5, 37.13, lit(RGBX(250, 210, 80)), lit(RGBX(250, 210, 80)));
    }
    /* передний боковой блок закрывает низ центрального */
    nozzle(f, -0.45, dark);
    nozzle(f, 0.45, dark);
    body(f, 0, 0.45, 0, BOOSTER, 5, grey);
    body(f, 0, 0.45, 0, BOOSTER_BAY, 2, bay);
}

/* ---------- стартовый комплекс (метры от центра стола) ---------- */
static void rot_about(double px, double py, double a, double *x, double *y)
{
    double vx = *x - px, vy = *y - py;
    *x = px + vx * cos(a) - vy * sin(a);
    *y = py + vx * sin(a) + vy * cos(a);
}

/* башня обслуживания: шарнир у внешнего нижнего угла, phi — насколько опущена наружу (рад) */
static void service_tower(const Xf *f, int sd, double phi)
{
    unsigned rail = lit(RGBX(184, 86, 40)), brace = lit(RGBX(128, 58, 30)), deck = lit(RGBX(160, 150, 140));
    double px = sd * 8.6, a = -sd * phi;
    int tr = (int)(f->k * 0.32) > 1 ? (int)(f->k * 0.32) : 1;
    /* u — от шарнира к ракете, v — вверх */
#define TW(u, v, X, Y) do { *(X) = px - sd * (u); *(Y) = (v); rot_about(px, 0, a, X, Y); } while (0)
    double x0, y0, x1, y1;
    for (int r = 0; r < 2; r++) {                 /* две стойки */
        double u = r * 4.6;
        TW(u, 0, &x0, &y0);
        TW(u, 44, &x1, &y1);
        xline(f, x0, y0, x1, y1, rail, tr);
    }
    for (int i = 0; i < 11; i++) {                /* распорки и раскосы */
        double v0 = i * 4.0, v1 = v0 + 4.0;
        TW(0, v0, &x0, &y0); TW(4.6, v0, &x1, &y1); xline(f, x0, y0, x1, y1, brace, 1);
        TW(0, v0, &x0, &y0); TW(4.6, v1, &x1, &y1); xline(f, x0, y0, x1, y1, brace, 1);
    }
    TW(0, 44, &x0, &y0); TW(4.6, 44, &x1, &y1); xline(f, x0, y0, x1, y1, rail, tr);
    for (int i = 0; i < 6; i++) {                 /* площадки обслуживания тянутся к ракете */
        double v = 12 + i * 5.2;
        TW(4.6, v, &x0, &y0); TW(6.2, v, &x1, &y1);
        xline(f, x0, y0, x1, y1, deck, tr + 1);
    }
#undef TW
}

/* опорные фермы («тюльпан»): держат ракету, при подъёме раскрываются на psi рад */
static void support_arms(const Xf *f, double psi)
{
    unsigned steel = lit(RGBX(78, 84, 96)), light = lit(RGBX(120, 126, 138));
    int t = (int)(f->k * 0.38) > 2 ? (int)(f->k * 0.38) : 2;
    for (int sd = -1; sd <= 1; sd += 2) {
        double px = sd * 5.6, a = -sd * psi;
        double pts[4][2] = { { sd * 5.6, 0 }, { sd * 4.3, 14.5 }, { sd * 3.6, 18.6 }, { sd * 2.2, 19.6 } };
        double in[3][2] = { { sd * 6.6, 0.5 }, { sd * 5.0, 14.0 }, { sd * 4.1, 18.4 } };
        for (int i = 0; i < 4; i++) rot_about(px, 0, a, &pts[i][0], &pts[i][1]);
        for (int i = 0; i < 3; i++) rot_about(px, 0, a, &in[i][0], &in[i][1]);
        for (int i = 0; i < 3; i++) xline(f, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], steel, t);
        for (int i = 0; i < 2; i++) xline(f, in[i][0], in[i][1], in[i + 1][0], in[i + 1][1], steel, t - 1);
        for (int i = 0; i < 3; i++) xline(f, in[i][0], in[i][1], pts[i + 1][0], pts[i + 1][1], light, 1);
        xquad(f, sd * 5.0, -0.2, sd * 7.4, -0.2, sd * 7.4, 1.6, sd * 5.0, 1.6, steel, xmix(steel, 0, 0.5));   /* противовес */
    }
}

/* ---------- камера восхождения ---------- */
typedef struct { double k, k0, rx, ry, gy, h, drift; } Cam;

/* высота ракеты через tau секунд после контакта подъёма, м (время заставки сжато) */
static double rk_h(double tau) { return tau <= 0 ? 0 : 3 * tau * tau + 5 * tau * tau * tau; }
static double rk_tau(double h)
{
    double a = 0, b = 8;
    for (int i = 0; i < 24; i++) { double m = (a + b) / 2; if (rk_h(m) < h) a = m; else b = m; }
    return (a + b) / 2;
}
static double rk_drift(double h) { return 0.05 * h * sstep(20, 200, h); }   /* уход по тангажу вправо */

static void cam_at(double t, Cam *c)
{
    c->k0 = g_h * 0.60 / 47.0;
    c->h = rk_h(t - T_LIFT);
    c->k = c->k0 * (1 - 0.5 * sstep(T_LIFT + 0.8, T_LIFT + 3.3, t));
    double gy0 = g_h * 0.86, L = g_h * 0.30;
    c->ry = gy0 - L * (1 - exp(-c->h * c->k / L));   /* камера плавно подхватывает ракету */
    c->gy = c->ry + c->h * c->k;
    c->rx = g_w * 0.5;
    c->drift = rk_drift(c->h);
}
static double cam_x(const Cam *c, double x) { return c->rx + (x - c->drift) * c->k; }
static double cam_y(const Cam *c, double a) { return c->gy - a * c->k; }

/* ---------- огонь и дым ---------- */
/* факелы трёх видимых блоков; power 0…1, len — длина, м */
static void flames(const Cv *cv, const Xf *f, double t, double power, double len)
{
    static const double NX[3] = { -2.55, 0, 2.55 };
    if (power <= 0.01) return;
    int M = (int)(len / 0.6) + 8;
    if (M > 64) M = 64;
    for (int e = 0; e < 3; e++)
        for (int j = M - 1; j >= 0; j--) {
            double u = (double)j / (M - 1), fl = 0.9 + 0.1 * sin(t * 47 + j * 1.3 + e * 2.1);
            double along = u * len * fl, r = (0.45 + 1.9 * u) * (e == 1 ? 1.2 : 1.0);
            double sx, sy;
            xpd(f, NX[e] * (1 - 0.55 * u) + 0.2 * sin(t * 31 + j * 0.7 + e), -0.3 - along, &sx, &sy);
            unsigned col = u < 0.1 ? RGBX(255, 250, 232) : u < 0.35 ? RGBX(255, 214, 120) : u < 0.65 ? RGBX(255, 140, 52) : RGBX(220, 74, 30);
            int a = (int)(power * 150 * pow(1 - u, 1.3));
            px_blob(cv, sx, sy, r * f->k, r * f->k * 1.2, col, col, a, 1);
        }
    double sx, sy;
    xpd(f, 0, -2.5, &sx, &sy);
    px_blob(cv, sx, sy, 9 * f->k, 11 * f->k, RGBX(255, 140, 60), RGBX(255, 140, 60), (int)(power * 120), 1);
}

/* раскалённые сердцевины у сопел — поверх дыма */
static void flame_cores(const Cv *cv, const Xf *f, double t, double power)
{
    static const double NX[3] = { -2.55, 0, 2.55 };
    for (int e = 0; e < 3; e++) {
        double sx, sy;
        xpd(f, NX[e], -1.2, &sx, &sy);
        double r = (e == 1 ? 1.3 : 1.0) * (0.9 + 0.1 * sin(t * 53 + e)) * f->k;
        px_blob(cv, sx, sy, r, r * 2.2, RGBX(255, 248, 225), RGBX(255, 248, 225), (int)(power * 230), 1);
    }
}

/* облака дыма из газоотводного лотка: катятся в обе стороны по земле */
static void smoke_ground(const Cv *cv, const Cam *c, double t, unsigned top, unsigned bot)
{
    const int N = 52;
    for (int i = 0; i < N; i++) {
        double b = T_IGN + 0.12 + 3.8 * pow((double)i / N, 1.5) + 0.04 * rnd(i);
        double age = t - b;
        if (age <= 0) continue;
        int sd = (i & 1) ? 1 : -1;
        double sp = 0.6 + 0.8 * rnd(i + 100);
        double x = sd * (6.5 + 34 * sp * (1 - exp(-age * 1.1)) + age * 1.6);
        double r = 2.6 + 9 * (1 - exp(-age * 1.0)) + age * 0.5;
        if (r > 15) r = 15;
        double a = 0.8 + age * (0.9 + 1.2 * rnd(i + 200)) + 3.5 * (1 - exp(-age * 1.2)) * rnd(i + 300) + r * 0.5;
        double al = sstep(0, 0.2, age) * (1 - sstep(5, 10, age));
        double hot = exp(-age * 1.3) * (t < T_LIFT + 1.5 ? 1 : 0.4);
        unsigned tp = xmix(top, RGBX(255, 176, 104), hot * 0.65), bt = xmix(bot, RGBX(240, 120, 60), hot * 0.55);
        px_blob(cv, cam_x(c, x), cam_y(c, a), r * c->k, r * c->k * 0.86, tp, bt, (int)(al * 236), 0);
    }
    for (int i = 0; i < 12; i++) {             /* пар поднимается вокруг основания ракеты */
        double age = t - (T_IGN + 0.3 + i * 0.11);
        if (age <= 0) continue;
        double x = (rnd(i + 400) - 0.5) * 9, a = 1.5 + age * (3 + 2 * rnd(i + 410));
        double r = 1.8 + age * 2.0;
        if (r > 8) r = 8;
        double al = 0.6 * sstep(0, 0.2, age) * (1 - sstep(1.2, 3.5, age));
        unsigned tp = xmix(top, RGBX(255, 190, 120), 0.4), bt = xmix(bot, RGBX(250, 140, 70), 0.5);
        px_blob(cv, cam_x(c, x), cam_y(c, a), r * c->k, r * c->k, tp, bt, (int)(al * 255), 0);
    }
}

/* след выхлопа за ракетой: точки берутся по высоте, «возраст» — когда ракета там пролетала */
static void smoke_trail(const Cv *cv, const Cam *c, double t, double plume, unsigned top, unsigned bot)
{
    double tau = t - T_LIFT;
    if (tau <= 0.15) return;
    double a = c->h - plume * 0.7;
    for (int guard = 0; guard < 260 && a > 0; guard++) {
        double sy = cam_y(c, a);
        if (sy > g_h + 80) break;
        double age = tau - rk_tau(a);
        double r = 1.3 + 2.0 * age + 1.4 * sqrt(age > 0 ? age : 0);
        double x = rk_drift(a) + 1.6 * age * (0.6 + 0.4 * sin(a * 0.05));
        double al = 0.42 * (1 - sstep(2.5, 6.5, age)) * sstep(0, 0.25, age);
        if (sy > -r * c->k)
            px_blob(cv, cam_x(c, x), sy, r * c->k, r * c->k, top, bot, (int)(al * 255), 0);
        double step = 0.28 * r;
        if (step * c->k < 3) step = 3 / c->k;
        a -= step;
    }
}

/* облачные слои, сквозь которые проходит ракета; front — передние клочья (поверх ракеты) */
static void cloud_layers(const Cv *cv, const Cam *c, int front, unsigned top, unsigned bot)
{
    static const double ALT[3] = { 70, 130, 200 };
    double span = g_w * 0.6 / (c->k0 * 0.5);
    for (int L = 0; L < 3; L++) {
        double sy0 = cam_y(c, ALT[L]);
        if (sy0 < -g_h * 0.2 || sy0 > g_h * 1.25) continue;
        for (int j = 0; j < 9; j++) {
            if ((j & 1) != front) continue;
            double x = (rnd(L * 31 + j) - 0.5) * 2 * span;
            double rx = (0.10 + 0.09 * rnd(L * 7 + j + 50)) * g_w, ry = rx * 0.17;
            double yy = sy0 + (rnd(L * 13 + j + 9) - 0.5) * ry * 2.5;
            px_blob(cv, cam_x(c, x), yy, rx, ry, top, bot, 150 + (int)(70 * rnd(L * 5 + j)), 0);
        }
    }
}

/* ---------- палитры неба ---------- */
typedef struct {
    Stop sky[4], night[4];
    unsigned groundFar, groundNear, smokeTop, smokeBot, cloudTop, cloudBot;
    double stars;          /* звёзды до старта */
} SkyPal;

static const SkyPal SKIES[3] = {
    {   /* золотой вечер, солнце у горизонта (23 марта 2024 года, 17:36 по времени Байконура) */
        { { 0, RGBX(38, 62, 128) }, { 0.5, RGBX(110, 120, 176) }, { 0.82, RGBX(238, 160, 120) }, { 1, RGBX(255, 204, 128) } },
        { { 0, RGBX(4, 6, 18) }, { 0.5, RGBX(12, 22, 62) }, { 0.82, RGBX(36, 64, 132) }, { 1, RGBX(70, 104, 170) } },
        RGBX(176, 128, 96), RGBX(62, 46, 40), RGBX(255, 238, 222), RGBX(178, 160, 168),
        RGBX(255, 236, 226), RGBX(196, 168, 182), 0 },
    {   /* сумерки после заката (18 декабря 1973 года) */
        { { 0, RGBX(14, 20, 52) }, { 0.5, RGBX(46, 52, 104) }, { 0.84, RGBX(150, 104, 128) }, { 1, RGBX(232, 140, 96) } },
        { { 0, RGBX(3, 4, 14) }, { 0.5, RGBX(8, 16, 46) }, { 0.84, RGBX(26, 44, 102) }, { 1, RGBX(54, 80, 140) } },
        RGBX(88, 70, 80), RGBX(30, 26, 32), RGBX(214, 206, 222), RGBX(110, 104, 128),
        RGBX(190, 180, 212), RGBX(100, 96, 132), 0.45 },
    {   /* ночь, стол в лучах прожекторов (15 июня 1978 года) */
        { { 0, RGBX(4, 6, 16) }, { 0.5, RGBX(10, 14, 34) }, { 0.86, RGBX(22, 30, 60) }, { 1, RGBX(40, 50, 84) } },
        { { 0, RGBX(2, 3, 10) }, { 0.5, RGBX(6, 10, 30) }, { 0.86, RGBX(14, 24, 64) }, { 1, RGBX(30, 46, 100) } },
        RGBX(40, 40, 54), RGBX(14, 14, 20), RGBX(206, 212, 228), RGBX(84, 88, 108),
        RGBX(150, 156, 180), RGBX(60, 64, 88), 1.0 },
};

static void set_light(int sky)
{
    if (sky == SKY_GOLDEN) { s_light = RGBX(255, 236, 206); s_shade = RGBX(58, 46, 66); s_tint = RGBX(255, 196, 140); s_tintAmt = 0.16; }
    else if (sky == SKY_DUSK) { s_light = RGBX(214, 206, 238); s_shade = RGBX(26, 26, 50); s_tint = RGBX(112, 104, 156); s_tintAmt = 0.36; }
    else { s_light = RGBX(250, 252, 255); s_shade = RGBX(18, 22, 40); s_tint = RGBX(160, 176, 214); s_tintAmt = 0.28; }
}

/* ---------- план 1: стартовый стол и восхождение ---------- */
static void scene_ascent(const Mission *m, double t)
{
    const SkyPal *sp = &SKIES[m->sky];
    set_light(m->sky);
    Cam c;
    cam_at(t, &c);
    double q = sstep(T_LIFT + 1.6, T_SEP + 0.4, t);    /* набор высоты: небо темнеет */
    double gy0 = g_h * 0.86;
    double hy = gy0 - g_h * 0.16 + (c.gy - gy0) * 0.42; /* горизонт уходит вниз медленнее стола */
    double ign = sstep(T_IGN, T_IGN + 0.9, t) * (0.75 + 0.25 * sstep(T_IGN + 0.9, T_LIFT, t));
    double power = t < T_IGN ? 0 : ign;
    Cv cv;
    if (!cv_get(&cv)) return;

    /* небо */
    Stop st[4];
    for (int i = 0; i < 4; i++) { st[i].at = sp->sky[i].at; st[i].c = xmix(sp->sky[i].c, sp->night[i].c, q); }
    px_grad(&cv, st, 4, 0, hy > g_h * 0.5 ? hy : g_h * 0.5, 0, g_h);
    px_stars(&cv, 240, sp->stars + (1 - sp->stars) * q, t, hy, 11);
    if (m->sky == SKY_GOLDEN) {   /* солнце у горизонта */
        double sx = g_w * 0.17, sy = hy - g_h * 0.035;
        px_blob(&cv, sx, sy, g_h * 0.55, g_h * 0.32, RGBX(255, 150, 80), RGBX(255, 150, 80), (int)(90 * (1 - q)), 1);
        px_blob(&cv, sx, sy, g_h * 0.12, g_h * 0.10, RGBX(255, 220, 160), RGBX(255, 220, 160), (int)(160 * (1 - q)), 1);
        px_blob(&cv, sx, sy, g_h * 0.03, g_h * 0.03, RGBX(255, 250, 230), RGBX(255, 250, 230), 255, 1);
    } else if (m->sky == SKY_DUSK) {
        px_blob(&cv, g_w * 0.25, hy, g_w * 0.5, g_h * 0.14, RGBX(230, 110, 70), RGBX(230, 110, 70), (int)(70 * (1 - q)), 1);
    }
    cloud_layers(&cv, &c, 0, xmix(sp->cloudTop, RGBX(200, 210, 240), q * 0.5), sp->cloudBot);

    /* степь до горизонта */
    if (hy < g_h) {
        Stop gr[3] = { { 0, xmix(sp->groundFar, st[3].c, 0.35) }, { 0.25, sp->groundFar }, { 1, sp->groundNear } };
        px_grad(&cv, gr, 3, hy, hy + g_h * 0.3, (int)hy, g_h);
        for (int i = 0; i < 160; i++) {                /* сухая трава штрихами, ближе — длиннее */
            double z = rnd(i * 5 + 3), y = hy + 2 + (g_h * 0.4) * z * z;
            int x = (int)(rnd(i * 5 + 4) * g_w), len = 2 + (int)(z * 22);
            int dv = (i & 1) ? 18 : -14;
            for (int k = 0; k < len; k++) px_add(&cv, x + k, (int)y, dv, dv, dv);
        }
        /* холмы и монтажно-испытательный корпус на горизонте */
        POINT hl[34];
        int n = 0;
        unsigned hc = xmix(sp->groundFar, st[2].c, 0.45);
        for (int i = 0; i <= 31; i++) {
            double x = g_w * i / 31.0;
            hl[n].x = (LONG)x;
            hl[n].y = (LONG)(hy - g_h * (0.006 + 0.012 * rnd(i + 77)) * (0.5 + 0.5 * sin(i * 0.7)));
            n++;
        }
        hl[n].x = g_w; hl[n].y = (LONG)hy + 2; n++;
        hl[n].x = 0; hl[n].y = (LONG)hy + 2; n++;
        d_poly(hl, n, cr(hc), CLR_INVALID, 1);
        d_fill((int)(g_w * 0.78), (int)(hy - g_h * 0.03), (int)(g_w * 0.08), (int)(g_h * 0.03) + 2, cr(xmix(hc, 0, 0.25)));
        d_fill((int)(g_w * 0.86), (int)(hy - g_h * 0.018), (int)(g_w * 0.05), (int)(g_h * 0.018) + 2, cr(xmix(hc, 0, 0.2)));
    }

    /* стол: громоотводы, бетон, газоотводный лоток */
    Xf pad = { cam_x(&c, 0), c.gy, c.k, 1, 0 };
    if (c.gy - 66 * c.k < g_h) {
        unsigned mast = lit(RGBX(110, 112, 120));
        for (int sd = -1; sd <= 1; sd += 2) {
            double mx = sd * 34;
            xline(&pad, mx - 1.2, 0, mx, 64, mast, 2);
            xline(&pad, mx + 1.2, 0, mx, 64, mast, 2);
            for (int i = 0; i < 10; i++) xline(&pad, mx - 1.2 + i * 0.12, i * 6.0, mx + 1.2 - (i + 1) * 0.12, (i + 1) * 6.0, mast, 1);
        }
        unsigned top = lit(RGBX(176, 172, 164)), face = lit(RGBX(120, 116, 110));
        xquad(&pad, -46, 0.9, 46, 0.9, 54, 0, -54, 0, top, xmix(top, 0, 0.3));
        xquad(&pad, -54, 0, 54, 0, 54, -3.2, -54, -3.2, face, xmix(face, 0, 0.4));
        xquad(&pad, -7, 0.9, 7, 0.9, 6, -3.2, -6, -3.2, RGBX(16, 14, 14), RGBX(16, 14, 14));
        service_tower(&pad, -1, 1.45 * sstep(0.2, 2.8, t));
        service_tower(&pad, 1, 1.45 * sstep(0.2, 2.8, t));
        if (!cv_get(&cv)) return;
        for (int sd = -1; sd <= 1; sd += 2) {   /* огни: красный на громоотводе, прожекторы ночью */
            double sx, sy;
            xpd(&pad, sd * 34, 64.5, &sx, &sy);
            int blink = (int)(t * 1.6 + (sd > 0 ? 0.5 : 0)) & 1;
            px_blob(&cv, sx, sy, SC(9), SC(9), RGBX(255, 60, 50), RGBX(255, 60, 50), blink ? 230 : 90, 1);
            if (m->sky != SKY_GOLDEN) {
                xpd(&pad, sd * 34, 40, &sx, &sy);
                px_blob(&cv, sx, sy, SC(30), SC(30), RGBX(255, 250, 230), RGBX(255, 250, 230), 200, 1);
            }
        }
        if (power > 0) {                       /* отсвет огня в лотке и на бетоне */
            px_blob(&cv, cam_x(&c, 0), c.gy + 1.5 * c.k, 30 * c.k, 10 * c.k, RGBX(255, 150, 70), RGBX(255, 150, 70), (int)(power * 170), 1);
            px_blob(&cv, cam_x(&c, 0), c.gy + 1.2 * c.k, 9 * c.k, 3.2 * c.k, RGBX(255, 214, 140), RGBX(255, 214, 140),
                    (int)(power * (210 + 40 * sin(t * 37))), 1);
        }
    }

    /* ракета */
    double tilt = 0.10 * sstep(T_LIFT + 1.4, T_SEP, t);
    Xf rf = { c.rx, c.ry, c.k, cos(tilt), sin(tilt) };
    rocket_draw(&rf, m);
    if (c.gy - 22 * c.k < g_h) support_arms(&pad, 0.5 * sstep(T_LIFT - 0.05, T_LIFT + 0.8, t));

    if (!cv_get(&cv)) return;
    double plume = t < T_LIFT ? 7 + 2 * ign : 9 + 24 * sstep(T_LIFT, T_LIFT + 1.6, t);
    if (power > 0) {
        double sx, sy;                          /* низ ракеты подсвечен огнём */
        xpd(&rf, 0, 3, &sx, &sy);
        px_blob(&cv, sx, sy, 7 * c.k, 9 * c.k, RGBX(255, 160, 80), RGBX(255, 160, 80), (int)(power * 95), 1);
        flames(&cv, &rf, t, power, plume);
    }
    unsigned smTop = xmix(sp->smokeTop, RGBX(200, 206, 230), q * 0.5), smBot = xmix(sp->smokeBot, RGBX(60, 70, 110), q * 0.5);
    smoke_trail(&cv, &c, t, plume, smTop, smBot);
    smoke_ground(&cv, &c, t, sp->smokeTop, sp->smokeBot);
    cloud_layers(&cv, &c, 1, xmix(sp->cloudTop, RGBX(200, 210, 240), q * 0.5), sp->cloudBot);
    if (power > 0) flame_cores(&cv, &rf, t, power);
}

/* ---------- план 2: отделение боковых блоков, вид с Земли ---------- */
static void scene_cross(const Mission *m, double t)
{
    double tc = t - T_SEP, S = g_h;
    Cv cv;
    if (!cv_get(&cv)) return;
    Stop st[3] = { { 0, RGBX(3, 5, 16) }, { 0.6, RGBX(10, 18, 50) }, { 1, m->sky == SKY_NIGHT ? RGBX(18, 28, 66) : RGBX(40, 58, 118) } };
    px_grad(&cv, st, 3, 0, g_h, 0, g_h);
    px_stars(&cv, 260, 1, t, g_h, 23);
    double cx = g_w * 0.5, cy = g_h * (0.47 - 0.025 * tc);
    /* выхлоп центрального блока: он летит дальше */
    px_blob(&cv, cx, cy, S * 0.17, S * 0.17, RGBX(255, 130, 50), RGBX(255, 130, 50), 90, 1);
    px_blob(&cv, cx, cy, S * 0.06, S * 0.06, RGBX(255, 230, 170), RGBX(255, 230, 170), 230, 1);
    double rot = 0.42 + 0.05 * tc;
    for (int i = 0; i < 4; i++) {      /* белые следы от центра к блокам — сам «крест» */
        double an = rot + i * PI / 2, dx = cos(an), dy = sin(an);
        double d = S * (0.03 + 0.075 * tc + 0.028 * tc * tc);
        for (int j = 0; j < 30; j++) {
            double u = j / 29.0;
            px_blob(&cv, cx + dx * d * u, cy + dy * d * u, S * 0.011 * (1 + u), S * 0.011 * (1 + u),
                    RGBX(220, 232, 255), RGBX(190, 204, 236), (int)(40 * (1 - 0.5 * u) * exp(-tc * 0.5)), 0);
        }
    }
    set_light(SKY_GOLDEN);
    unsigned body_c = RGBX(214, 214, 208);
    for (int i = 0; i < 4; i++) {
        double an = rot + i * PI / 2 + 0.22 * sin(tc * 1.3 + i * 1.7) * sstep(0, 1, tc);
        double dx = cos(an), dy = sin(an);
        double d = S * (0.03 + 0.075 * tc + 0.028 * tc * tc);
        double L = S * 0.17 * sin(fmin(PI / 2, tc * 1.5 + 0.15)), wb = S * 0.03 * (1 - 0.1 * tc);
        double bx = cx + cos(rot + i * PI / 2) * d, by = cy + sin(rot + i * PI / 2) * d;
        /* блок — вытянутый конус: основание у центра, острие наружу */
        Xf bf = { bx, by, 1, 1, 0 };
        double ang = atan2(dx, -dy);           /* угол оси от «вверх» по часовой */
        double prof[8] = { 0, wb, L * 0.7, wb * 0.73, L * 0.9, wb * 0.4, L, 0.02 * wb };
        body(&bf, 0, 0, ang, prof, 4, body_c);
        if (!cv_get(&cv)) return;
        double fl = exp(-tc * 1.4);             /* догорающие двигатели и облачко кислорода */
        px_blob(&cv, bx, by, S * 0.04 * fl + 1, S * 0.04 * fl + 1, RGBX(255, 150, 60), RGBX(255, 150, 60), (int)(220 * fl), 1);
        px_blob(&cv, bx - dx * S * 0.012, by - dy * S * 0.012, S * 0.026, S * 0.026, RGBX(235, 240, 255), RGBX(200, 210, 235), (int)(80 * exp(-tc * 0.8)), 0);
    }
    px_blob(&cv, cx, cy, S * 0.022, S * 0.022, RGBX(255, 255, 240), RGBX(255, 255, 240), 255, 1);
}

/* ---------- план 3: орбита ---------- */
static void px_earth(const Cv *c, double ex, double ey, double R, double t, double glow)
{
    noise_init();
    double rr = R + glow;
    int ya = (int)(ey - rr);
    if (ya < c->y0) ya = c->y0;
    for (int y = ya; y < c->y1; y++) {
        double dy = y + 0.5 - ey;
        if (dy * dy >= rr * rr) continue;
        double hw = sqrt(rr * rr - dy * dy);
        int xs = (int)(ex - hw), xe = (int)(ex + hw) + 1;
        if (xs < c->x0) xs = c->x0;
        if (xe > c->x1) xe = c->x1;
        xs &= ~1;
        unsigned *row = c->px + (size_t)y * c->stride;
        for (int x = xs; x < xe; x += 2) {     /* через пиксель: крупный план Земли гладкий */
            double dx = x + 1 - ex, d = sqrt(dx * dx + dy * dy);
            unsigned out;
            unsigned dst = row[x];
            if (d > R) {                       /* свечение атмосферы над краем */
                double a = 1 - (d - R) / glow;
                a = a * a * 0.9;
                int r = (int)((dst >> 16) & 255) + (int)(70 * a), g = (int)((dst >> 8) & 255) + (int)(150 * a), b = (int)(dst & 255) + (int)(255 * a);
                out = RGBX(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
            } else {
                double th = sqrt(2 * (1 - d / R));          /* угол от края диска */
                double u = dx / d * 2.6 + t * 0.012;
                int cl = noise_at(u * 115, th * 280), ld = noise_at(u * 60 + 37, th * 140 + 91);
                double ocean = clampd(th / 0.3, 0, 1);
                unsigned col = xmix(RGBX(64, 118, 196), RGBX(14, 50, 118), ocean);
                if (ld > 150) col = xmix(col, xmix(RGBX(92, 112, 64), RGBX(150, 132, 92), (ld - 150) / 105.0), clampd((ld - 150) / 18.0, 0, 1));
                if (cl > 132) col = xmix(col, RGBX(238, 242, 248), clampd((cl - 132) / 60.0, 0, 0.95));
                col = xmix(col, RGBX(150, 196, 252), clampd(1 - th / 0.1, 0, 1) * 0.8);
                double lgt = clampd(0.88 - 0.22 * dx / R, 0.5, 1.1);
                int r = (int)(((col >> 16) & 255) * lgt), g = (int)(((col >> 8) & 255) * lgt), b = (int)((col & 255) * lgt);
                out = RGBX(r > 255 ? 255 : r, g > 255 ? 255 : g, b > 255 ? 255 : b);
            }
            row[x] = out;
            if (x + 1 < xe) row[x + 1] = out;
        }
    }
}

/* корабль «Союз» сбоку, летит вправо; s — пикселей на метр */
static void soyuz_ship(double cx, double cy, double s, double roll)
{
    Xf f = { cx, cy, s, cos(roll), sin(roll) };
    set_light(SKY_GOLDEN);
    s_tintAmt = 0;
    s_shade = RGBX(14, 16, 30);
    /* солнечные батареи: синие панели в золотой рамке */
    for (int sd = -1; sd <= 1; sd += 2) {
        xquad(&f, -3.1, sd * 1.3, -2.6, sd * 1.3, -2.6, sd * 1.8, -3.1, sd * 1.8, RGBX(170, 170, 160), RGBX(90, 90, 90));
        double x0 = -3.5, x1 = -2.2, sk = 0.55;
        xquad(&f, x0, sd * 1.8, x1, sd * 1.8, x1 + sk, sd * 6.2, x0 + sk, sd * 6.2, RGBX(30, 52, 116), RGBX(206, 166, 76));
        for (int i = 1; i < 5; i++) {
            double v = 1.8 + 4.4 * i / 5;
            double o = sk * i / 5;
            xline(&f, x0 + o, sd * v, x1 + o, sd * v, RGBX(80, 112, 180), 1);
        }
        xline(&f, (x0 + x1) / 2, sd * 1.8, (x0 + x1) / 2 + sk, sd * 6.2, RGBX(80, 112, 180), 1);
    }
    static const double SERVICE[] = { 0, 1.0, 0.35, 1.45, 2.6, 1.36 };
    static const double DESCENT[] = { 0, 1.08, 0.25, 1.1, 1.9, 0.62, 2.15, 0.5 };
    static const double ORBITAL[] = { 0, 0.55, 0.25, 0.95, 0.9, 1.12, 1.6, 1.1, 2.3, 0.85, 2.6, 0.45 };
    static const double PROBE[] = { 0, 0.38, 0.35, 0.2, 0.95, 0.05 };
    xquad(&f, -4.5, -0.55, -4.1, -0.35, -4.1, 0.35, -4.5, 0.55, RGBX(60, 60, 66), RGBX(30, 30, 30));
    body(&f, -4.1, 0, PI / 2, SERVICE, 3, RGBX(210, 210, 204));
    for (int i = 0; i < 3; i++) xline(&f, -3.5 + i * 0.7, -1.3, -3.5 + i * 0.7, 1.3, RGBX(150, 150, 146), 1);
    body(&f, -1.5, 0, PI / 2, DESCENT, 4, RGBX(116, 124, 92));
    body(&f, 0.65, 0, PI / 2, ORBITAL, 6, RGBX(128, 136, 104));
    body(&f, 3.25, 0, PI / 2, PROBE, 3, RGBX(190, 190, 186));
    xline(&f, 2.6, 0.9, 3.5, 1.9, RGBX(210, 210, 205), 1);
    xline(&f, 2.2, -1.0, 2.9, -2.1, RGBX(210, 210, 205), 1);
    xline(&f, -1.2, 1.0, -0.6, 1.9, RGBX(210, 210, 205), 1);
}

/* станции вдали: МКС (2024) и «Салют-6» (1978) */
static void station_draw(int kind, double cx, double cy, double s)
{
    if (kind == ST_ISS) {
        d_line((int)(cx - 54 * s), (int)cy, (int)(cx + 54 * s), (int)cy, RGB(200, 200, 196), s * 2.6 > 2 ? (int)(s * 2.6) : 2);
        for (int i = 0; i < 4; i++) {
            double x = (i < 2 ? -1 : 1) * (i % 2 ? 46 : 33);
            for (int sd = -1; sd <= 1; sd += 2) {
                int x0 = (int)(cx + (x - 5) * s), w = (int)(10 * s), y0 = sd < 0 ? (int)(cy - 34 * s) : (int)(cy + 3 * s), h = (int)(31 * s);
                d_fill(x0, y0, w, h, RGB(176, 128, 56));
                d_fill(x0 + w / 2, y0, 1, h, RGB(110, 76, 30));
                d_frame(x0, y0, w, h, RGB(120, 86, 40));
            }
        }
        d_rrect((int)(cx - 20 * s), (int)(cy + 2 * s), (int)(42 * s), (int)(5 * s) + 2, (int)(2 * s), RGB(232, 232, 226), RGB(160, 160, 156));
        d_rrect((int)(cx - 3 * s), (int)(cy - 9 * s), (int)(5 * s) + 2, (int)(20 * s), (int)(2 * s), RGB(226, 226, 220), RGB(160, 160, 156));
        d_fill((int)(cx + 14 * s), (int)(cy - 14 * s), (int)(6 * s) + 1, (int)(12 * s), RGB(236, 236, 236));
        d_fill((int)(cx - 20 * s), (int)(cy - 14 * s), (int)(6 * s) + 1, (int)(12 * s), RGB(236, 236, 236));
    } else if (kind == ST_SALYUT) {
        Xf f = { cx, cy, s, 1, 0 };
        set_light(SKY_GOLDEN);
        s_tintAmt = 0;
        static const double SAL[] = { 0, 1.0, 3.0, 1.0, 3.4, 1.45, 8.6, 1.45, 9.4, 2.08, 15.0, 2.08 };
        for (int sd = -1; sd <= 1; sd += 2)
            xquad(&f, -5.6, sd * 2.0, -3.6, sd * 2.0, -3.6, sd * 8.5, -5.6, sd * 8.5, RGBX(34, 56, 120), RGBX(200, 160, 70));
        body(&f, -7.5, 0, PI / 2, SAL, 6, RGBX(208, 208, 200));
    }
}

static void scene_orbit(const Mission *m, double t)
{
    double to = t - T_ORB;
    Cv cv;
    if (!cv_get(&cv)) return;
    Stop st[2] = { { 0, RGBX(1, 2, 8) }, { 1, RGBX(4, 8, 24) } };
    px_grad(&cv, st, 2, 0, g_h, 0, g_h);
    double R = g_w * 1.5 + g_h, ex = g_w * 0.5, ey = g_h * 0.78 + R;
    px_stars(&cv, 300, 1, t, g_h * 0.8, 47);
    double sx = g_w * 0.05, sy = g_h * 0.36;          /* Солнце слева, свет на корабле — тоже слева */
    px_blob(&cv, sx, sy, g_h * 0.32, g_h * 0.32, RGBX(255, 220, 170), RGBX(255, 220, 170), 70, 1);
    px_blob(&cv, sx, sy, g_w * 0.2, g_h * 0.006 + 1, RGBX(255, 230, 200), RGBX(255, 230, 200), 110, 1);
    px_blob(&cv, sx, sy, g_h * 0.035, g_h * 0.035, RGBX(255, 255, 245), RGBX(255, 255, 245), 255, 1);
    px_earth(&cv, ex, ey, R, t, g_h * 0.035);
    if (m->station == ST_ISS) station_draw(ST_ISS, g_w * 0.84 - to * g_w * 0.004, g_h * 0.16, g_h * 0.0017);
    if (m->station == ST_SALYUT) station_draw(ST_SALYUT, g_w * 0.84 - to * g_w * 0.004, g_h * 0.17, g_h * 0.0075);
    double drift = 1 - exp(-to / 6);
    soyuz_ship(g_w * (0.34 + 0.1 * drift), g_h * (0.46 - 0.02 * drift), g_h * 0.034, 0.06 * sin(to * 0.5) - 0.05);
}

/* ---------- надписи поверх заставки ---------- */
static void star5(int cx, int cy, int r, COLORREF c)
{
    POINT p[10];
    for (int i = 0; i < 10; i++) {
        double a = -PI / 2 + i * PI / 5, rr = i & 1 ? r * 0.42 : r;
        p[i].x = cx + (LONG)lround(cos(a) * rr);
        p[i].y = cy + (LONG)lround(sin(a) * rr);
    }
    d_poly(p, 10, c, CLR_INVALID, 1);
}

static void hud(const Mission *m, int person, double t)
{
    Cv cv;
    if (!cv_get(&cv)) return;
    int x0 = SC(28), y0 = SC(22), pw = SC(64), ph = SC(80);
    int bandH = ph + SC(m->gold ? 92 : 52);
    for (int i = 0; i < bandH; i++)                     /* затемнение сверху: надписи читаются на любом небе */
        px_rrect(&cv, 0, i, g_w, 1, 0, 0, 96 * (bandH - i) / bandH);
    const Person *R = &PEOPLE[person];
    portrait_box(R->id, R->name, x0, y0, pw, ph, m->gold ? P.gold : RGB(220, 220, 220));
    int tx = x0 + pw + SC(16);
    SetTextCharacterExtra(d_dc(), 1);
    stext(tx, y0, m->place, F_SMALL_B, RGB(255, 214, 160));
    SetTextCharacterExtra(d_dc(), 0);
    HFONT ft = fx_font(SC(36), FW_SEMIBOLD);
    fx_text(tx + 1, y0 + SC(17) + 1, m->ship, ft, RGB(0, 0, 0), 0);
    fx_text(tx, y0 + SC(17), m->ship, ft, RGB(255, 255, 255), 0);
    stext(tx, y0 + SC(60), m->crew, F_SMALL, RGB(226, 230, 240));
    if (m->gold) {
        int w = d_text_w(GOLD_TITLE, F_BODY_B) + SC(50), h = SC(32), y = y0 + ph + SC(12);
        if (!cv_get(&cv)) return;
        px_rrect(&cv, x0, y, w, h, h / 2, RGBX(232, 184, 86), 235);
        star5(x0 + SC(18), y + h / 2, SC(9), RGB(120, 70, 0));
        d_text(x0 + SC(34), y + (h - d_font_h(F_BODY_B)) / 2, GOLD_TITLE, F_BODY_B, RGB(48, 30, 4));
    }
    /* «Пропустить» */
    int bw = SC(150), bh = SC(36), bx = g_w - bw - SC(24), by = SC(22);
    if (!cv_get(&cv)) return;
    px_rrect(&cv, bx, by, bw, bh, SC(8), 0, hover(A_NEXT, FX_SKIP) ? 190 : 120);
    d_text_c(bx, by + (bh - d_font_h(F_BODY_B)) / 2, bw, L"Пропустить  \x203A", F_BODY_B, RGB(255, 255, 255));
    d_text_c(bx, by + bh + SC(4), bw, ESC_HINT, F_TINY, RGB(200, 204, 214));
    hit(bx, by, bw, bh, A_NEXT, FX_SKIP);

    /* отсчёт */
    const wchar_t *cd = 0;
    double c0 = 0;
    if (t >= 2.0 && t < 3.0) { cd = L"3"; c0 = 2.0; }
    else if (t >= 3.0 && t < 4.0) { cd = L"2"; c0 = 3.0; }
    else if (t >= 4.0 && t < T_LIFT) { cd = L"1"; c0 = 4.0; }
    else if (t >= T_LIFT && t < T_LIFT + 1.4) { cd = L"Пуск!"; c0 = T_LIFT; }
    if (cd) {
        double e = t - c0;
        int big = cd[1] == 0, px = (int)(g_h * (big ? 0.26 : 0.13));
        int cx = (int)(g_w * 0.2), cy = (int)(g_h * 0.40);
        if (!cv_get(&cv)) return;
        px_blob(&cv, cx, cy, px * 0.9, px * 0.75, RGBX(255, 150, 70), RGBX(255, 150, 70), (int)(110 * (1 - 0.6 * e)), 1);
        HFONT fb = fx_font(px, FW_BOLD);
        int ty = cy - px * 6 / 10 - (int)((1 - sstep(0, 0.18, e)) * SC(26));
        fx_text(cx + SC(4), ty + SC(4), cd, fb, RGB(30, 14, 6), 1);
        fx_text(cx, ty, cd, fb, big ? RGB(255, 246, 228) : RGB(255, 204, 96), 1);
        if (big) stext(cx - d_text_w(L"до старта", F_H2) / 2, cy + px * 5 / 10, L"до старта", F_H2, RGB(255, 226, 190));
    }

    /* подпись этапа и шкала времени снизу */
    const wchar_t *cap, *sub = 0;
    if (t < 2.0) cap = L"Башни обслуживания отходят от ракеты";
    else if (t < T_IGN) cap = L"Предстартовый отсчёт";
    else if (t < T_LIFT) cap = L"Зажигание! Двигатели выходят на режим";
    else if (t < T_SEP) { cap = L"Пуск! «Союз» уходит в небо"; if (t > T_LIFT + 1.6) sub = L"Через две минуты отделятся четыре боковых блока"; }
    else if (t < T_ORB) { cap = L"Отделение боковых блоков — «крест Королёва»"; sub = L"С Земли разлёт четырёх блоков похож на крест: его назвали в честь конструктора Сергея Королёва"; }
    else cap = m->orbit;
    int fh = d_font_h(F_H2), sh = sub ? d_font_h(F_SMALL) + SC(4) : 0;
    int cw = d_text_w(cap, F_H2), sw = sub ? d_text_w(sub, F_SMALL) : 0;
    int w = (cw > sw ? cw : sw) + SC(44), h = fh + sh + SC(20);
    int cx = (g_w - w) / 2, cy = g_h - SC(34) - h;
    if (!cv_get(&cv)) return;
    px_rrect(&cv, cx, cy, w, h, SC(12), RGBX(6, 8, 18), 150);
    d_text_c(cx, cy + SC(10), w, cap, F_H2, RGB(255, 255, 255));
    if (sub) d_text_c(cx, cy + SC(12) + fh, w, sub, F_SMALL, RGB(206, 212, 230));
    int px0 = SC(28), pw2 = g_w - 2 * px0, py = g_h - SC(14);
    px_rrect(&cv, px0, py, pw2, SC(4), SC(2), RGBX(255, 255, 255), 50);
    px_rrect(&cv, px0, py, (int)(pw2 * clampd(t / T_END, 0, 1)), SC(4), SC(2), RGBX(255, 196, 110), 230);
    static const double MARKS[3] = { T_LIFT, T_SEP, T_ORB };
    for (int i = 0; i < 3; i++)
        px_blob(&cv, px0 + pw2 * MARKS[i] / T_END, py + SC(2), SC(4), SC(4), RGBX(255, 255, 255), RGBX(255, 255, 255), 230, 0);
}

/* ---------- карточка о полёте ---------- */
static int card(const Mission *m, int person, int x, int y, int w, int draw)
{
    const Person *R = &PEOPLE[person];
    int pad = SC(26), cx = x + pad, iw = w - 2 * pad, yy = y + pad;
    wchar_t s[200];
    if (m->gold) {     /* золотая лента */
        int bh = SC(40);
        if (draw) {
            for (int i = 0; i < iw; i += SC(4)) {
                int ww = iw - i < SC(4) ? iw - i : SC(4);
                d_fill(cx + i, yy, ww, bh, cr(xmix(RGBX(212, 160, 62), RGBX(246, 214, 128), sin(PI * i / iw))));
            }
            star5(cx + SC(22), yy + bh / 2, SC(10), RGB(120, 64, 0));
            star5(cx + iw - SC(22), yy + bh / 2, SC(10), RGB(120, 64, 0));
            SetTextCharacterExtra(d_dc(), 1);
            d_text_c(cx, yy + (bh - d_font_h(F_BODY_B)) / 2, iw, L"ПЕРВАЯ ЖЕНЩИНА-КОСМОНАВТ СУВЕРЕННОЙ БЕЛАРУСИ", F_BODY_B, RGB(52, 30, 0));
            SetTextCharacterExtra(d_dc(), 0);
        }
        yy += bh + SC(18);
    }
    int pw = SC(148), ph = SC(186);
    if (draw) portrait_box(R->id, R->name, cx, yy, pw, ph, m->gold ? P.gold : P.blue);
    int tx = cx + pw + SC(22), tw = iw - pw - SC(22), ty = yy;
    COLORREF acc = m->gold ? P.gold : P.blue;
    if (draw) {
        d_icon(IC_SPACE, tx, ty, SC(20), acc);
        SetTextCharacterExtra(d_dc(), 1);
        d_text(tx + SC(28), ty + SC(2), m->eyebrow, F_SMALL_B, acc);
        SetTextCharacterExtra(d_dc(), 0);
    }
    ty += SC(30);
    ty += d_wrap(tx, ty, tw, R->name, F_ERA_L, P.ink, draw) + SC(2);
    swprintf(s, 200, L"%ls · %ls", R->role, R->life);
    ty += d_wrap(tx, ty, tw, s, F_SMALL_B, acc, draw) + SC(10);
    /* чипы: дата, корабль, цель, длительность */
    const wchar_t *chips[4] = { m->date, m->ship, m->goal, m->length };
    int chx = tx, chh = d_font_h(F_TINY) + SC(4);
    for (int i = 0; i < 4; i++) {
        int cw = d_text_w(chips[i], F_TINY) + SC(12);
        if (chx > tx && chx + cw > tx + tw) { chx = tx; ty += chh + SC(6); }
        if (draw) w_pill(chx, ty, chips[i], i == 0 ? P.onAccent : P.ink, i == 0 ? acc : P.surface2);
        chx += cw + SC(6);
    }
    ty += chh + SC(12);
    ty += d_wrap(tx, ty, tw, m->text, F_BODY, P.ink, draw) + SC(6);
    yy = (ty > yy + ph ? ty : yy + ph) + SC(16);
    if (draw) d_ornament(cx, yy, iw, 2, m->gold ? P.gold : P.red, mix(m->gold ? P.gold : P.red, P.surface, 150));
    yy += SC(30);
    if (m->gold) {     /* женщины Беларуси в науке и космосе */
        int bp = SC(14), bw = iw - 2 * bp - SC(34);
        int th = d_wrap(0, 0, bw, WOMEN_TITLE, F_BODY_B, P.ink, 0), xh = d_wrap(0, 0, bw, WOMEN_TEXT, F_SMALL, P.ink, 0);
        int bh = bp + th + SC(4) + xh + bp;
        if (draw) {
            d_rrect(cx, yy, iw, bh, SC(10), mix(P.surface, P.gold, 34), P.gold);
            star5(cx + bp + SC(12), yy + bp + SC(10), SC(11), P.gold);
            d_wrap(cx + bp + SC(34), yy + bp, bw, WOMEN_TITLE, F_BODY_B, P.ink, 1);
            d_wrap(cx + bp + SC(34), yy + bp + th + SC(4), bw, WOMEN_TEXT, F_SMALL, P.ink, 1);
        }
        yy += bh + SC(16);
    }
    if (draw) {
        int b2 = SC(160), b1 = SC(200);
        w_button(cx + iw - b2, yy, b2, SC(42), L"Продолжить", B_PRIMARY, 1, A_NEXT, FX_CLOSE);
        w_button(cx + iw - b2 - SC(12) - b1, yy, b1, SC(42), L"Смотреть ещё раз", B_SECONDARY, 1, A_NEXT, FX_REPLAY);
    }
    yy += SC(42);
    return yy + pad - y;
}

/* ---------- стенд с пушками (сцена 75) ---------- */
static void cannon_draw(int x, int y, int size, int dir, double t, int modern);

static void cannon_board(double now)
{
    static const double PH[6] = { 0.0, 0.04, 0.1, 0.25, 0.6, 1.3 };
    d_fill(0, 0, g_w, g_h, P.bg);
    int x0 = SC(40), y = SC(30);
    d_text(x0, y, L"Пушки на карте: проверка анимации", F_H1, P.ink);
    y += SC(44);
    d_text(x0, y, L"Цикл 2,4 с: откат ствола, вспышка, клубы дыма, полёт ядра. Под каждой пушкой — фаза выстрела.", F_SMALL, P.muted);
    y += SC(34);
    int cw = (g_w - 2 * x0) / 6;
    wchar_t s[64];
    for (int st = 0; st < 2; st++) {
        d_text(x0, y, st ? L"XX век: стальная пушка со щитом" : L"До XX века: бронзовый ствол на деревянном лафете", F_BODY_B, P.ink);
        y += SC(26);
        for (int i = 0; i < 6; i++) {
            int cx = x0 + i * cw;
            d_rrect(cx + SC(4), y, cw - SC(8), SC(150), SC(8), P.land, P.line);
            double ph = s_devT >= 0 ? PH[i] : now + i * 0.4;
            cannon_draw(cx + cw / 3, y + SC(112), SC(52), 1, ph, st);
            swprintf(s, 64, L"t = %.2f с", fmod(ph, 2.4));
            d_text(cx + SC(14), y + SC(124), s, F_TINY, P.muted);
        }
        y += SC(166);
    }
    d_text(x0, y, L"Размеры на карте (22 и 30 пикселей), обе стороны боя", F_BODY_B, P.ink);
    y += SC(26);
    for (int i = 0; i < 4; i++) {
        int cx = x0 + i * cw * 3 / 2;
        d_rrect(cx + SC(4), y, cw * 3 / 2 - SC(8), SC(90), SC(8), P.land, P.line);
        double ph = s_devT >= 0 ? 0.05 + i * 0.12 : now + i;
        int sz = SC(i & 1 ? 30 : 22);
        cannon_draw(cx + SC(40), y + SC(66), sz, 1, ph, i >= 2);
        cannon_draw(cx + cw * 3 / 2 - SC(48), y + SC(66), sz, -1, ph + 0.7, i >= 2);
    }
    y += SC(110);
    w_button(x0, y, SC(160), SC(42), L"Закрыть", B_SECONDARY, 1, A_NEXT, FX_CLOSE);
}

/* ---------- заставка целиком ---------- */
static DWORD s_liveAt;              /* сцена 87: заставка начнётся в этот момент (запуск под Wine долгий) */
static double launch_time(void)
{
    if (s_liveAt) {
        if ((int)(GetTickCount() - s_liveAt) < 0) return 0;
        U.animStart = s_liveAt;
        s_liveAt = 0;
    }
    return s_devT >= 0 ? s_devT : (GetTickCount() - U.animStart) / 1000.0;
}

static int berlin_due(void);
#include "fx_chaes.inc"
#include "fx_berlin.inc"

/* когда заставка доходит до рассказа (карточки) */
static double launch_end(void) { return U.launchPerson == FX_CHAES ? CH_END : U.launchPerson == FX_BERLIN ? BE_END : T_END; }

void fx_launch_draw(void)
{
    d_unclip();
    ui_anim_request();
    double t = launch_time();
    if (U.launchPerson == FX_BOARD) { cannon_board(t); return; }
    if (U.launchPerson == FX_CHAES) { chaes_draw(t); return; }
    if (U.launchPerson == FX_BERLIN) { berlin_draw(t); return; }
    if (U.launchPerson == FX_TROOPS) {
        troops_gallery(t);
        w_button(g_w - SC(184), g_h - SC(56), SC(160), SC(42), L"Закрыть", B_SECONDARY, 1, A_NEXT, FX_CLOSE);
        return;
    }
    int fi = fx_flight_of(U.launchPerson);
    if (fi < 0) { U.modal = M_NONE; return; }
    const Mission *m = &MISSIONS[fi];
    double tv = t < T_END ? t : T_END + (t - T_END) * 0.35;    /* под карточкой — медленный полёт */
    if (tv < T_SEP) scene_ascent(m, tv);
    else if (tv < T_ORB) scene_cross(m, tv);
    else scene_orbit(m, tv);
    static const double CUTS[2] = { T_SEP, T_ORB };
    for (int i = 0; i < 2; i++) {                  /* смена плана — через затемнение */
        double d = fabs(tv - CUTS[i]);
        if (d < T_DIP) d_blend(0, 0, g_w, g_h, RGB(0, 0, 0), (int)(255 * (1 - d / T_DIP)));
    }
    if (t < T_END) {
        hit(0, 0, g_w, g_h, A_NEXT, FX_SKIP);       /* щелчок в любом месте — к рассказу */
        hud(m, U.launchPerson, t);
        return;
    }
    double e = sstep(0, 0.45, t - T_END);
    d_blend(0, 0, g_w, g_h, RGB(0, 0, 0), (int)(150 * e));
    int w = g_w - SC(48) < SC(800) ? g_w - SC(48) : SC(800);
    int h = card(m, U.launchPerson, 0, 0, w, 0);
    int x = (g_w - w) / 2, y = (g_h - h) / 2 + (int)((1 - e) * SC(30));
    if (y < SC(12)) y = SC(12);
    d_rrect(x + SC(3), y + SC(6), w, h, SC(14), RGB(0, 0, 0), CLR_INVALID);
    if (m->gold) d_rrect(x - SC(3), y - SC(3), w + SC(6), h + SC(6), SC(17), P.gold, CLR_INVALID);
    d_rrect(x, y, w, h, SC(14), P.surface, m->gold ? P.gold : P.line);
    card(m, U.launchPerson, x, y, w, 1);
}

/* ---------- открыть и закрыть ---------- */
static void launch_open(int person)
{
    s_back = U.modal == M_PERSON || U.modal == M_EVENT ? U.modal : M_NONE;
    s_backArg = U.modalArg;
    if (person == FX_CHAES && !s_chCamSaved) {   /* заставка водит камеру карты — потом вернём как было */
        s_chCam = U.cam; s_chCamInit = U.camInit; s_chCamSaved = 1;
    }
    U.modal = M_LAUNCH;
    U.launchPerson = person;
    U.animStart = GetTickCount();
    U.modalScroll = 0;
    s_devT = -1;
    ui_invalidate();
}

static void launch_close(void)
{
    s_devT = -1;
    if (s_chCamSaved) {
        U.cam = s_chCam; U.camInit = s_chCamInit; s_chCamSaved = 0;
        U.mapFast = 0;
        map_invalidate();
    }
    U.modal = s_back;
    U.modalArg = s_backArg;
    U.modalScroll = 0;
    s_back = M_NONE;
    ui_after_change();       /* очередь событий и заставок идёт дальше */
}

int fx_ui_click(int act, int arg)
{
    if (act != A_NEXT || arg < FX_SKIP) return 0;
    if (arg == FX_CHAES_WATCH) { launch_open(FX_CHAES); return 1; }
    if (arg == FX_BERLIN_WATCH) { launch_open(FX_BERLIN); return 1; }
    if (arg >= FX_WATCH) {
        if (arg - FX_WATCH >= PEOPLE_COUNT) return 0;
        launch_open(arg - FX_WATCH);
        return 1;
    }
    if (arg > FX_CLOSE || U.modal != M_LAUNCH) return 0;
    s_devT = -1;
    if (arg == FX_SKIP) U.animStart = GetTickCount() - (DWORD)(launch_end() * 1000);
    else if (arg == FX_REPLAY) U.animStart = GetTickCount();
    else launch_close();
    return 1;
}

void fx_ui_hover(int act, int arg, wchar_t *buf, int n)
{
    if (n <= 0 || act != A_NEXT || arg < FX_SKIP) return;
    int ch = U.modal == M_LAUNCH && (U.launchPerson == FX_CHAES || U.launchPerson == FX_BERLIN);
    const wchar_t *s = arg >= FX_WATCH ? L"Посмотреть взлёт ракеты с Байконура и узнать о полёте"
                     : arg == FX_CHAES_WATCH ? L"Посмотреть анимацию: авария на Чернобыльской АЭС и облако над Беларусью"
                     : arg == FX_BERLIN_WATCH ? L"Посмотреть анимацию: штурм Берлина и Знамя Победы над Рейхстагом"
                     : arg == FX_SKIP ? (ch ? L"Пропустить и сразу открыть рассказ (Пробел или Enter)" : L"Пропустить взлёт и сразу открыть рассказ о полёте (Пробел или Enter)")
                     : arg == FX_REPLAY ? (ch ? L"Посмотреть анимацию ещё раз" : L"Посмотреть взлёт ракеты ещё раз")
                     : arg == FX_CLOSE ? L"Вернуться к игре (Esc)" : 0;
    if (s) { wcsncpy(buf, s, (size_t)n - 1); buf[n - 1] = 0; }
}

int fx_ui_key(WPARAM vk)
{
    if (U.modal != M_LAUNCH) return 0;
    if (vk == VK_F2) return 0;             /* смена темы — как везде */
    double t = launch_time();
    if (vk == VK_ESCAPE) launch_close();
    else if (vk == VK_RETURN || vk == VK_SPACE) {
        if ((U.launchPerson >= 0 || U.launchPerson == FX_CHAES || U.launchPerson == FX_BERLIN) && t < launch_end()) { s_devT = -1; U.animStart = GetTickCount() - (DWORD)(launch_end() * 1000); }
        else launch_close();
    } else if (vk == 'R') { s_devT = -1; U.animStart = GetTickCount(); }
    return 1;                              /* остальные клавиши во время заставки ничего не делают */
}

/* нет событий и окон: пора ли показать взлёт этого хода */
int fx_ui_idle(void)
{
    if (berlin_due()) return 1;
    int late = 0, i;
    while ((i = fx_flight_due(&G, &late)) >= 0) {
        G.cutscenes |= FX_FLIGHTS[i].cs;
        int p = person_index(FX_FLIGHTS[i].id);
        if (late || p < 0) continue;
        launch_open(p);
        return 1;
    }
    return 0;
}

void fx_ui_hired(int person)
{
    int i = fx_flight_on_hire(&G, person);
    if (i < 0) return;
    G.cutscenes |= FX_FLIGHTS[i].cs;
    launch_open(person);
}

/* блок в окне деятеля: «Смотреть взлёт ракеты» */
int fx_person_card(int person, int x, int y, int w)
{
    int fi = fx_flight_of(person);
    if (fi < 0) return 0;
    const Mission *m = &MISSIONS[fi];
    int h = SC(m->gold ? 84 : 62), hv = hover(A_NEXT, FX_WATCH + person);
    d_rrect(x, y, w, h, SC(10), hv ? RGB(30, 46, 98) : RGB(18, 28, 66), m->gold ? P.gold : RGB(66, 90, 156));
    for (int i = 0; i < 18; i++) {             /* звёзды справа */
        int sx = x + w * 7 / 10 + (int)(rnd(i * 3 + 5) * (w * 3 / 10 - SC(64))), sy = y + SC(6) + (int)(rnd(i * 3 + 6) * (h - SC(12)));
        int sz = i % 4 == 0 ? 2 : 1;
        d_fill(sx, sy, sz, sz, i % 3 ? RGB(150, 166, 220) : RGB(236, 240, 255));
    }
    d_icon(IC_SPACE, x + w - SC(48), y + (h - SC(30)) / 2, SC(30), m->gold ? P.gold : RGB(236, 240, 255));
    int cs = SC(38), ccx = x + SC(14) + cs / 2, ccy = y + SC(31);
    d_circle(ccx, ccy, cs / 2, hv ? RGB(250, 150, 80) : RGB(232, 120, 60), CLR_INVALID, 1);
    POINT tri[3] = { { ccx - SC(5), ccy - SC(8) }, { ccx - SC(5), ccy + SC(8) }, { ccx + SC(9), ccy } };
    d_poly(tri, 3, RGB(255, 255, 255), CLR_INVALID, 1);
    int tx = x + SC(66);
    d_text(tx, y + SC(10), L"Смотреть взлёт ракеты", F_BODY_B, RGB(255, 255, 255));
    wchar_t s[160];
    swprintf(s, 160, L"%ls · Байконур, %ls", m->ship, m->date);
    d_ellipsis(tx, y + SC(32), w - SC(66) - SC(60), s, F_SMALL, RGB(176, 192, 236));
    if (m->gold) {
        star5(tx + SC(6), y + SC(63), SC(7), P.gold);
        d_text(tx + SC(18), y + SC(55), GOLD_TITLE, F_SMALL_B, RGB(240, 202, 112));
    }
    hit(x, y, w, h, A_NEXT, FX_WATCH + person);
    return h;
}

/* ---------- пушка ---------- */
#define CANNON_CYCLE 2.4

/* (x, y) — точка опоры колеса, size — длина ствола; modern — стальная пушка XX века со щитом */
static void cannon_draw(int x, int y, int size, int dir, double t, int modern)
{
    double S = size < 8 ? 8 : size, ph = fmod(t, CANNON_CYCLE);
    if (ph < 0) ph += CANNON_CYCLE;
    double rec = ph < 0.05 ? ph / 0.05 : exp(-(ph - 0.05) * 6.0);   /* откат: резко назад, плавно на место */
    double R = S * (modern ? 0.25 : 0.29), roll = -dir * rec * S * 0.09;
    double wx = x + roll, wy = y - R;
    double el = (modern ? 9 : 14) * PI / 180, bx = dir * cos(el), by = -sin(el);   /* направление ствола */
    double nx = -by * dir, ny = bx * dir;          /* поперёк ствола, вверх */
    if (ny > 0) { nx = -nx; ny = -ny; }
    double pvx = wx, pvy = wy - R * 0.22, br = rec * S * 0.12;
    double back = S * (modern ? 0.25 : 0.30) + br, front = S * (modern ? 0.85 : 0.70) - br;
    double mx = pvx + bx * front, my = pvy + by * front;   /* дуло */
    COLORREF edge = modern ? RGB(34, 40, 24) : RGB(62, 36, 16);
    int lt = S > 28 ? 2 : 1;
    /* лафет */
    if (modern) {
        d_line((int)wx, (int)wy, (int)(wx - dir * S * 0.85), y - 1, RGB(48, 56, 34), (int)(S * 0.08) + 1);
        d_line((int)wx, (int)wy, (int)(wx - dir * S * 0.78), y - (int)(S * 0.04), RGB(80, 92, 58), (int)(S * 0.07) + 1);
    } else {
        d_line((int)wx, (int)wy, (int)(wx - dir * S * 0.82), y - 1, edge, (int)(S * 0.13) + 2);
        d_line((int)wx, (int)wy, (int)(wx - dir * S * 0.82), y - 1, RGB(128, 82, 42), (int)(S * 0.13));
    }
    /* ствол */
    double hb = S * (modern ? 0.06 : 0.10), hm = S * (modern ? 0.04 : 0.065);
    double ex = pvx - bx * back, ey = pvy - by * back;
    POINT bp[4] = {
        { (LONG)(ex + nx * hb), (LONG)(ey + ny * hb) }, { (LONG)(mx + nx * hm), (LONG)(my + ny * hm) },
        { (LONG)(mx - nx * hm), (LONG)(my - ny * hm) }, { (LONG)(ex - nx * hb), (LONG)(ey - ny * hb) } };
    COLORREF metal = modern ? RGB(88, 100, 64) : RGB(182, 124, 56), hi = modern ? RGB(130, 144, 100) : RGB(236, 190, 110);
    if (modern) d_line((int)(pvx - bx * S * 0.1), (int)(pvy - by * S * 0.1 + S * 0.06), (int)(pvx + bx * S * 0.3), (int)(pvy + by * S * 0.3 + S * 0.06), RGB(60, 70, 44), (int)(S * 0.07) + 1);
    d_poly(bp, 4, metal, edge, 1);
    d_line((int)(ex + nx * hb * 0.5), (int)(ey + ny * hb * 0.5), (int)(mx + nx * hm * 0.5), (int)(my + ny * hm * 0.5), hi, 1);
    if (modern) d_line((int)(mx - nx * hm * 1.7), (int)(my - ny * hm * 1.7), (int)(mx + nx * hm * 1.7), (int)(my + ny * hm * 1.7), RGB(50, 58, 36), (int)(S * 0.06) + 1);
    else {
        d_line((int)(mx - nx * hm * 1.3), (int)(my - ny * hm * 1.3), (int)(mx + nx * hm * 1.3), (int)(my + ny * hm * 1.3), RGB(120, 76, 30), (int)(S * 0.05) + 1);
        d_circle((int)(ex - bx * S * 0.03), (int)(ey - by * S * 0.03), (int)(S * 0.045) + 1, metal, edge, 1);
    }
    /* щит и колесо */
    if (modern) {
        POINT sh[4] = { { (LONG)(wx + dir * S * 0.05), (LONG)(wy - R * 1.75) }, { (LONG)(wx + dir * S * 0.15), (LONG)(wy - R * 1.65) },
                        { (LONG)(wx + dir * S * 0.20), (LONG)(wy + R * 0.35) }, { (LONG)(wx + dir * S * 0.10), (LONG)(wy + R * 0.35) } };
        d_poly(sh, 4, RGB(98, 112, 70), edge, 1);
        d_circle((int)wx, (int)wy, (int)R, RGB(36, 36, 36), RGB(20, 20, 20), 1);
        d_circle((int)wx, (int)wy, (int)(R * 0.55), RGB(96, 108, 72), CLR_INVALID, 1);
        d_circle((int)wx, (int)wy, (int)(R * 0.18) + 1, RGB(50, 58, 36), CLR_INVALID, 1);
    } else {
        double spin = -roll / R;
        for (int i = 0; i < 6; i++) {
            double a = spin + i * PI / 3;
            d_line((int)wx, (int)wy, (int)(wx + cos(a) * R * 0.9), (int)(wy + sin(a) * R * 0.9), RGB(110, 70, 34), lt);
        }
        d_circle((int)wx, (int)wy, (int)R, CLR_INVALID, RGB(84, 52, 24), (int)(S * 0.06) + 1);
        d_circle((int)wx, (int)wy, (int)(R * 0.2) + 1, RGB(70, 44, 20), CLR_INVALID, 1);
    }
    /* вспышка выстрела */
    if (ph < 0.13) {
        static const double FL[9][2] = { { 0, 0.10 }, { 0.18, 0.28 }, { 0.32, 0.12 }, { 0.62, 0.17 }, { 0.98, 0 },
                                         { 0.62, -0.17 }, { 0.32, -0.12 }, { 0.18, -0.28 }, { 0, -0.10 } };
        double fl = 1 - ph / 0.13;
        POINT p[9];
        for (int k = 0; k < 2; k++) {
            double sc = (k ? 0.55 : 1) * S * (0.45 + 0.6 * fl);
            for (int i = 0; i < 9; i++) {
                p[i].x = (LONG)(mx + (bx * FL[i][0] + nx * FL[i][1]) * sc);
                p[i].y = (LONG)(my + (by * FL[i][0] + ny * FL[i][1]) * sc);
            }
            d_poly(p, 9, k ? RGB(255, 250, 220) : RGB(255, 182, 60), CLR_INVALID, 1);
        }
    }
    Cv cv;
    if (!cv_get(&cv)) return;
    if (ph < 0.13) px_blob(&cv, mx, my, S * 0.6, S * 0.5, RGBX(255, 170, 70), RGBX(255, 170, 70), (int)(200 * (1 - ph / 0.13)), 1);
    /* ядро или снаряд с трассой */
    if (ph > 0.02 && ph < 0.55) {
        double u = (ph - 0.02) / 0.53, D = S * 4.2 * u;
        double sx = mx + bx * D, sy = my + by * D + S * 1.6 * u * u;
        double vx = bx * 4.2, vy = by * 4.2 + 3.2 * u, vl = sqrt(vx * vx + vy * vy);
        double tl = S * 0.4;
        COLORREF trail = modern ? RGB(255, 220, 140) : RGB(220, 210, 190);
        if (u < 0.85) d_line((int)sx, (int)sy, (int)(sx - vx / vl * tl), (int)(sy - vy / vl * tl), trail, S > 28 ? 2 : 1);
        d_circle((int)sx, (int)sy, (int)(S * 0.045) + 1, modern ? RGB(255, 236, 180) : RGB(30, 30, 30), CLR_INVALID, 1);
        if (!cv_get(&cv)) return;
    }
    /* клубы дыма у дула и пыль у колеса */
    for (int i = 0; i < 4; i++) {
        double age = ph - (0.01 + i * 0.03), life = 1.7;
        if (age <= 0 || age >= life) continue;
        double go = S * (0.25 + 0.55 * i / 3) * (1 - exp(-age * 3)) + S * 0.12 * age;
        double px = mx + bx * go, py = my + by * go - S * 0.2 * age;
        double r = S * (0.10 + 0.08 * i / 3) + S * 0.32 * (1 - exp(-age * 2.2));
        int a = (int)(210 * pow(1 - age / life, 1.3) * (age * 20 < 1 ? age * 20 : 1));
        px_blob(&cv, px, py, r, r * 0.9, RGBX(246, 244, 240), RGBX(170, 168, 162), a, 0);
    }
    if (ph < 0.8) px_blob(&cv, wx - dir * S * 0.25, y - S * 0.06, S * (0.12 + 0.35 * ph), S * (0.06 + 0.12 * ph),
                          RGBX(196, 176, 136), RGBX(170, 150, 112), (int)(120 * (1 - ph / 0.8)), 0);
}

void fx_cannon(int x, int y, int size, int dir, double t)
{
    cannon_draw(x, y, size, dir >= 0 ? 1 : -1, t, G.year >= 1900);
}

/* ---------- сцены разработчика 70–79 ---------- */
void fx_dev_scene(int n)
{
    int v = person_index("vasilevskaya");
    switch (n) {
    case 70: case 71: case 72: case 73: case 76:   /* Василевская: стол, зажигание, подъём, карточка, крест */
        dev_play_until(2024); dev_resolve();
        launch_open(v);
        s_devT = n == 70 ? 0.5 : n == 71 ? 4.1 : n == 72 ? 6.9 : n == 73 ? T_END + 2 : T_SEP + 0.9;
        break;
    case 74:                                       /* Климук: ракета в полёте, сумерки 1973 года */
        dev_play_until(1975); dev_resolve();
        launch_open(person_index("klimuk"));
        s_devT = 7.4;
        break;
    case 75:                                       /* стенд с пушками */
        dev_play_until(1812);
        launch_open(FX_BOARD);
        s_devT = 0;
        break;
    case 77:                                       /* Ковалёнок: орбита и «Салют-6» */
        dev_play_until(1980); dev_resolve();
        launch_open(person_index("kovalyonok"));
        s_devT = T_ORB + 1.2;
        break;
    case 78:                                       /* Климук: карточка в светлой теме */
        dev_play_until(1975); dev_resolve();
        ui_toggle_theme();
        launch_open(person_index("klimuk"));
        s_devT = T_END + 2;
        break;
    case 39:                                       /* галерея 3D-отрядов: живая анимация */
        dev_play_until(1943); dev_resolve();
        launch_open(FX_TROOPS);
        break;
    case 80: case 81: case 82: case 83: case 84:   /* авария на ЧАЭС: станция, взрыв, пожарные, облако, зона */
    case 85:                                       /* карточка */
        dev_play_until(1986);
        launch_open(FX_CHAES);
        s_devT = n == 80 ? 1.5 : n == 81 ? CH_BOOM + 0.5 : n == 82 ? CH_FIRE + 1.8 : n == 83 ? CH_FALL + 1.0 : n == 84 ? CH_ZONE + 4.0 : CH_END + 2;
        break;
    case 88: case 89: case 34: case 35: case 36:   /* Берлин: улица, штурм Рейхстага, знамя, салют, карточка */
        dev_play_until(1945);
        launch_open(FX_BERLIN);
        s_devT = n == 88 ? 4.2 : n == 89 ? BE_REICH + 2.6 : n == 34 ? BE_FLAG + 2.6 : n == 35 ? BE_SALUTE + 2.4 : BE_END + 2;
        break;
    case 37:                                       /* Берлин целиком, живое время */
        dev_play_until(1945);
        launch_open(FX_BERLIN);
        s_liveAt = GetTickCount() + 16000;
        break;
    case 87:                                       /* авария на ЧАЭС целиком, живое время (для записи видео) */
        dev_play_until(1986);
        launch_open(FX_CHAES);
        s_liveAt = GetTickCount() + 16000;
        break;
    case 86:                                       /* окно события «Чернобыль» с кнопкой «Смотреть» */
        dev_play_until(1986);
        break;
    case 79:                                       /* окно деятеля с кнопкой «Смотреть взлёт ракеты» */
        dev_play_until(2024); dev_resolve();
        U.modal = M_PERSON; U.modalArg = v; U.modalScroll = 0;
        break;
    }
}
