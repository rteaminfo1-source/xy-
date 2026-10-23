/* Штурм Берлина, апрель — май 1945 года: заставка во весь экран в начале хода 1945 года.
 * Включается в ui_fx.c (общие помощники: пиксели заднего буфера, шрифты, Xf — метры на экран).
 *   План 1 — уличные бои: руины и пожары, залпы «катюш», наша пехота и танк теснят вермахт.
 *   План 2 — Рейхстаг: бойцы бегут через площадь, танк бьёт по зданию; камера поднимается к куполу,
 *            на нём поднимают Знамя Победы.
 *   План 3 — рассвет и салют Победы. В конце — карточка: фронты, знаменосцы, женщины на улицах Берлина.
 * Отряды и танк — те же 3D-фигуры из Blender, что и на карте (troops_draw). */

#define BE_REICH  7.6    /* Рейхстаг */
#define BE_FLAG   12.2   /* Знамя Победы */
#define BE_SALUTE 16.2   /* рассвет, салют */
#define BE_END    20.6   /* карточка */

static int be_unit(const char *id)
{
    for (int i = 0; i < UNIT_COUNT; i++) if (!strcmp(UNIT_TYPES[i].id, id)) return i;
    return -1;
}

/* ---------- руины города: силуэты домов с выбитыми окнами (детерминированно от seed) ---------- */
static void be_ruins(double y0, double hmin, double hmax, COLORREF wall, int seed, double t, double fireAmt)
{
    double x = -g_w * 0.03;
    for (int i = 0; x < g_w * 1.03 && i < 60; i++) {
        double w = g_w * (0.05 + 0.08 * rnd(seed + i * 4)), h = g_h * (hmin + (hmax - hmin) * rnd(seed + i * 4 + 1));
        int broken = rnd(seed + i * 4 + 2) < 0.6;
        POINT p[12];
        int n = 0;
        p[n].x = (LONG)lround(x); p[n].y = (LONG)lround(y0); n++;
        if (broken) {                                      /* обрушенный верх ступеньками */
            for (int k = 0; k <= 6; k++) {
                double xx = x + w * k / 6, hh = h * (0.55 + 0.45 * rnd(seed + i * 31 + k));
                p[n].x = (LONG)lround(xx); p[n].y = (LONG)lround(y0 - hh); n++;
            }
        } else {
            p[n].x = (LONG)lround(x); p[n].y = (LONG)lround(y0 - h); n++;
            p[n].x = (LONG)lround(x + w); p[n].y = (LONG)lround(y0 - h); n++;
        }
        p[n].x = (LONG)lround(x + w); p[n].y = (LONG)lround(y0); n++;
        d_poly(p, n, wall, CLR_INVALID, 1);
        /* окна: пустые проёмы и редкие отсветы пожаров */
        int cols = (int)(w / (g_h * 0.03)), rows = (int)(h * 0.7 / (g_h * 0.045));
        for (int r = 0; r < rows; r++)
            for (int c = 0; c < cols; c++) {
                double q = rnd(seed * 7 + i * 101 + r * 13 + c);
                if (q < 0.25) continue;
                double wx = x + w * (c + 0.3) / cols, wy = y0 - h * 0.12 - r * g_h * 0.045;
                COLORREF wc = q > 0.93 && fireAmt > 0 ? RGB(230, 120, 50) : RGB(14, 12, 14);
                ch_rect(wx, wy - g_h * 0.022, g_h * 0.012, g_h * 0.02, wc);
            }
        x += w * (0.85 + 0.3 * rnd(seed + i * 4 + 3));
    }
    if (fireAmt <= 0) return;
    Cv cv;
    if (!cv_get(&cv)) return;
    for (int i = 0; i < 9; i++) {                          /* пожары в руинах */
        double fx = g_w * rnd(seed + 500 + i), fy = y0 - g_h * (0.05 + 0.2 * rnd(seed + 520 + i));
        double fl = 0.75 + 0.25 * sin(t * (6 + i) + i);
        px_blob(&cv, fx, fy, g_h * 0.05, g_h * 0.035, RGBX(255, 150, 60), RGBX(220, 70, 20), (int)(120 * fireAmt * fl), 1);
    }
}

/* дым столбами из руин */
static void be_smoke(double t, double y0, int seed, double amt)
{
    Cv cv;
    if (!cv_get(&cv) || amt <= 0) return;
    double u = g_h / 100.0;
    for (int s = 0; s < 4; s++) {
        double sx = g_w * (0.12 + 0.25 * s + 0.08 * rnd(seed + s)), sy = y0 - u * (8 + 10 * rnd(seed + 10 + s));
        for (int k = 0; k < 16; k++) {
            double age = fmod(t * 0.9 + k * 0.62 + s * 0.3, 10.0);
            double h = age * 6.5 * u, x = sx + age * 2.2 * u + sin(k + age) * u, r = (3 + 2.4 * age) * u;
            px_blob(&cv, x, sy - h, r, r * 0.8, RGBX(70, 64, 66), RGBX(96, 80, 74), (int)(120 * amt * (1 - age / 10)), 0);
        }
    }
}

/* залпы «катюш»: реактивные снаряды летят по дугам через небо */
static void be_rockets(double t, double sky)
{
    Cv cv;
    if (!cv_get(&cv)) return;
    for (int v = 0; v < 4; v++) {
        double t0 = 0.6 + v * 1.7;
        for (int i = 0; i < 8; i++) {
            double s = (t - t0 - i * 0.09) / 1.5;
            if (s <= 0 || s >= 1.05) continue;
            double ax = -0.04 * g_w + i * g_w * 0.006, bx = g_w * (0.72 + 0.04 * rnd(v * 9 + i));
            double hy = sky - g_h * (0.30 + 0.06 * rnd(v * 9 + i + 3));
            for (int k = 7; k >= 0; k--) {                  /* хвост: дым и пламя */
                double q = s - k * 0.025;
                if (q < 0) continue;
                double x = ax + (bx - ax) * q, y = sky - (sky - hy) * 4 * q * (1 - q);
                px_blob(&cv, x, y, g_h * 0.008 + k * 0.6, g_h * 0.008 + k * 0.6, RGBX(210, 200, 196), RGBX(180, 170, 168), 90 - k * 10, 0);
            }
            if (s > 1) continue;
            double x = ax + (bx - ax) * s, y = sky - (sky - hy) * 4 * s * (1 - s);
            px_blob(&cv, x, y, g_h * 0.012, g_h * 0.012, RGBX(255, 230, 160), RGBX(255, 140, 60), 255, 1);
        }
        /* разрывы за горизонтом */
        double e = t - t0 - 1.5;
        if (e > 0 && e < 0.9)
            px_blob(&cv, g_w * 0.78, sky - g_h * 0.02, g_h * 0.16 * (0.4 + e), g_h * 0.07, RGBX(255, 190, 110), RGBX(220, 90, 30), (int)(200 * (1 - e / 0.9)), 1);
    }
}

/* ---------- план 1: бои на улицах Берлина ---------- */
static void berlin_streets(double t)
{
    Cv cv;
    if (!cv_get(&cv)) return;
    double gy = g_h * 0.80;
    static const Stop SKY[] = { { 0, RGBX(28, 24, 30) }, { 0.55, RGBX(74, 52, 50) }, { 1, RGBX(176, 90, 50) } };
    px_grad(&cv, SKY, 3, 0, gy, 0, (int)gy + 1);
    px_blob(&cv, g_w * 0.65, gy - g_h * 0.08, g_w * 0.5, g_h * 0.22, RGBX(140, 60, 20), RGBX(120, 40, 10), 70, 1);   /* зарево */
    be_ruins(gy - g_h * 0.10, 0.22, 0.42, RGB(58, 46, 46), 11, t, 1);
    be_smoke(t, gy - g_h * 0.10, 3, 1);
    be_rockets(t, gy - g_h * 0.12);
    be_ruins(gy, 0.18, 0.40, RGB(30, 24, 26), 47, t, 0.6);
    /* мостовая, завалы, противотанковые «ежи» */
    ch_rect(0, gy, g_w, g_h - gy, RGB(36, 32, 32));
    for (int i = 0; i < 14; i++) {
        double x = g_w * rnd(800 + i), y = gy + (g_h - gy) * (0.15 + 0.7 * rnd(820 + i)), r = g_h * (0.01 + 0.03 * rnd(840 + i));
        POINT p[5] = { { (LONG)(x - r * 2), (LONG)y }, { (LONG)(x - r), (LONG)(y - r) }, { (LONG)(x + r * 0.5), (LONG)(y - r * 1.3) },
                       { (LONG)(x + r * 2), (LONG)y }, { (LONG)(x), (LONG)(y + r * 0.3) } };
        d_poly(p, 5, RGB(56, 50, 48), CLR_INVALID, 1);
    }
    for (int i = 0; i < 3; i++) {
        double x = g_w * (0.56 + i * 0.05), y = gy + g_h * 0.035, s = g_h * 0.035;
        d_line((int)(x - s), (int)(y + s * 0.5), (int)(x + s), (int)(y - s), RGB(70, 66, 64), SC(3));
        d_line((int)(x - s), (int)(y - s), (int)(x + s), (int)(y + s * 0.5), RGB(70, 66, 64), SC(3));
        d_line((int)x, (int)(y - s * 1.2), (int)x, (int)(y + s * 0.6), RGB(60, 56, 54), SC(3));
    }
    /* наступление: пехота и танк идут вправо, вермахт отстреливается и отходит */
    double adv = sstep(0.4, BE_REICH, t), sz = g_h * 0.30;
    int rifle = be_unit("rifle"), tank = be_unit("tank");
    double ex = g_w * (0.86 + 0.25 * sstep(4.6, 7.4, t));
    troops_draw((int)ex, (int)(g_h * 0.88), (int)(sz * 0.8), -1, 6, t < 4.6 ? -1 : 1, t < 4.6 ? TM_FIRE : TM_MARCH, 1, 1945, t);
    troops_draw((int)(g_w * (0.08 + 0.26 * adv)), (int)(g_h * 0.86), (int)(sz * 0.9), tank, 1, 1, fmod(t, 2.4) < 0.9 ? TM_FIRE : TM_MARCH, 0, 1945, t);
    troops_draw((int)(g_w * (0.22 + 0.30 * adv)), (int)(g_h * 0.93), (int)sz, rifle, 9, 1, fmod(t + 0.8, 3.0) < 1.2 ? TM_FIRE : TM_MARCH, 0, 1945, t);
    ui_anim_request();
}

/* ---------- план 2: Рейхстаг (метры от середины фасада у земли, y вверх) ---------- */
static const unsigned BE_STONE = RGBX(122, 112, 98), BE_STONE_D = RGBX(84, 76, 68), BE_STONE_L = RGBX(150, 140, 122);

static void be_box(const Xf *f, double x0, double y0, double x1, double y1, unsigned c)
{
    xquad(f, x0, y0, x1, y0, x1, y1, x0, y1, lit(c), lit(xmix(c, 0, 0.35)));
}

/* купол: каркас из рёбер — стекло выбито */
static void be_dome(const Xf *f)
{
    double cx = 0, by = 44, rx = 17, ry = 22;
    be_box(f, -19, 36, 19, 44, BE_STONE_D);                       /* барабан */
    for (int i = 0; i < 9; i++) be_box(f, -17.5 + i * 4.2, 38, -16.5 + i * 4.2, 43, BE_STONE_L);
    unsigned rib = lit(RGBX(104, 96, 90));
    for (int k = 0; k <= 8; k++) {                               /* рёбра — полуэллипсы в проекции */
        double ph = PI * k / 8, sx = cos(ph);
        double px = cx + rx * sx, py = by;
        for (int s = 1; s <= 12; s++) {
            double a = PI / 2 * s / 12, x = cx + rx * sx * cos(a), y = by + ry * sin(a);
            xline(f, px, py, x, y, rib, (int)(f->k * 0.45) > 1 ? (int)(f->k * 0.45) : 1);
            px = x; py = y;
        }
    }
    for (int r = 1; r <= 3; r++) {                               /* кольца */
        double a = PI / 2 * r / 4, w = rx * cos(a), y = by + ry * sin(a);
        xline(f, -w, y, w, y, rib, (int)(f->k * 0.35) > 1 ? (int)(f->k * 0.35) : 1);
    }
    be_box(f, -5.0, by + ry - 0.8, 5.0, by + ry + 0.2, BE_STONE);     /* площадка вокруг фонаря */
    be_box(f, -2.2, by + ry, 2.2, by + ry + 2.0, BE_STONE_D);         /* фонарь наверху */
}

static void berlin_reichstag_body(const Xf *f, double t)
{
    /* главный корпус, угловые башни, ступени, портик с колоннами и фронтоном */
    be_box(f, -56, 0, 56, 27, BE_STONE);
    be_box(f, -56, 25.5, 56, 27.5, BE_STONE_L);
    for (int sd = -1; sd <= 1; sd += 2) {
        be_box(f, sd * 50, 0, sd * 69, 38, BE_STONE);
        be_box(f, sd * 50, 36.5, sd * 69, 38.5, BE_STONE_L);
        be_box(f, sd * 52, 38.5, sd * 67, 41, BE_STONE_D);
        for (int k = 0; k < 4; k++) be_box(f, sd * (52.5 + k * 4), 41, sd * (54 + k * 4), 43.5, BE_STONE_D);   /* угловые башенки */
    }
    be_dome(f);
    /* окна корпуса и башен: тёмные проёмы, кое-где пожар */
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < 20; i++) {
            double x = -54 + i * 5.6;
            if (fabs(x) < 22) continue;
            be_box(f, x, 6 + r * 9.5, x + 2.4, 12 + r * 9.5, RGBX(26, 22, 22));
        }
    for (int sd = -1; sd <= 1; sd += 2)
        for (int r = 0; r < 3; r++) be_box(f, sd * 57.5, 5 + r * 10.5, sd * 61, 11 + r * 10.5, RGBX(26, 22, 22));
    /* ступени и портик */
    xquad(f, -25, 0, 25, 0, 21, 6, -21, 6, lit(BE_STONE_L), lit(BE_STONE_D));
    for (int s = 1; s < 6; s++) xline(f, -25 + s * 0.67, s, 25 - s * 0.67, s, lit(BE_STONE_D), 1);
    be_box(f, -21, 24, 21, 28.5, BE_STONE_L);                    /* антаблемент */
    xquad(f, -22, 28.5, 22, 28.5, 0, 36, 0, 36, lit(BE_STONE), lit(BE_STONE_D));   /* фронтон */
    be_box(f, -19, 6, 19, 24, RGBX(40, 34, 32));                 /* глубина портика */
    for (int c = 0; c < 6; c++) {                                /* колонны */
        double x = -17 + c * 6.8;
        be_box(f, x - 1.1, 6, x + 1.1, 24, BE_STONE_L);
        be_box(f, x - 1.5, 23, x + 1.5, 24.2, BE_STONE);
    }
    /* пробоины от снарядов */
    static const double HOLES[][3] = { { -40, 20, 3.2 }, { -30, 9, 2.4 }, { 33, 18, 3.6 }, { 44, 8, 2.2 }, { 60, 30, 2.6 }, { -62, 24, 2.8 }, { 9, 31, 1.8 } };
    for (unsigned i = 0; i < sizeof HOLES / sizeof HOLES[0]; i++) {
        POINT p[7];
        for (int k = 0; k < 7; k++) {
            double a = 2 * PI * k / 7, r = HOLES[i][2] * (0.65 + 0.5 * rnd(900 + i * 7 + k));
            p[k] = xp(f, HOLES[i][0] + cos(a) * r, HOLES[i][1] + sin(a) * r);
        }
        d_poly(p, 7, RGB(20, 16, 16), CLR_INVALID, 1);
    }
    Cv cv;
    if (!cv_get(&cv)) return;
    static const double FIRES[][2] = { { -47, 15 }, { -36, 25 }, { 30, 9 }, { 41, 21 }, { 58, 14 }, { -60, 34 }, { 0, 30 } };
    for (unsigned i = 0; i < sizeof FIRES / sizeof FIRES[0]; i++) {
        double sx, sy, fl = 0.7 + 0.3 * sin(t * (7 + i) + i);
        xpd(f, FIRES[i][0], FIRES[i][1], &sx, &sy);
        px_blob(&cv, sx, sy, 3.5 * f->k, 2.4 * f->k * fl, RGBX(255, 190, 90), RGBX(240, 90, 30), (int)(170 * fl), 1);
    }
}

/* Знамя Победы: древко поднимается, полотнище разворачивается и полощется на ветру */
static void be_flag(const Xf *f, double t, double e)
{
    if (e <= 0) return;
    double baseY = 68, poleH = 12 * sstep(0, 0.35, e), fw = 8.5, fh = 5.2;
    xline(f, 0.3, baseY, 0.3, baseY + poleH, lit(RGBX(80, 60, 40)), (int)(f->k * 0.3) > 1 ? (int)(f->k * 0.3) : 1);
    double unf = sstep(0.3, 1.0, e);                              /* полотнище раскрывается */
    if (unf <= 0) return;
    double top = baseY + poleH - 0.3;
    int N = 14;
    for (int i = 0; i < N; i++) {
        double a0 = (double)i / N, a1 = (double)(i + 1) / N;
        double w0 = sin(t * 5.2 - a0 * 5.5) * 0.9 * a0 * unf, w1 = sin(t * 5.2 - a1 * 5.5) * 0.9 * a1 * unf;
        double x0 = 0.4 + fw * a0 * unf, x1 = 0.4 + fw * a1 * unf, dr = 0.35 * a0 * unf;
        double sl = cos(t * 5.2 - (a0 + a1) * 2.75);
        unsigned col = xmix(RGBX(206, 22, 26), sl > 0 ? RGBX(240, 60, 50) : RGBX(120, 8, 14), fabs(sl) * 0.45);
        xquad(f, x0, top + w0 - dr, x1, top + w1 - dr - 0.025 * fw, x1, top - fh + w1 - dr, x0, top - fh + w0 - dr, col, col);
    }
    if (unf < 0.6) return;
    /* звезда и серп с молотом у древка */
    POINT c = xp(f, 1.5, top - 0.9);
    int r = (int)(f->k * 0.55);
    if (r >= 2) star5(c.x, c.y, r, RGB(255, 210, 70));
    POINT h = xp(f, 1.6, top - 2.2);
    int s = (int)(f->k * 0.7);
    if (s >= 3) {
        d_line(h.x - s, h.y + s, h.x + s, h.y - s, RGB(255, 210, 70), s / 4 > 1 ? s / 4 : 1);               /* рукоять молота */
        d_line(h.x + s / 2, h.y - s, h.x + s + s / 3, h.y - s / 3, RGB(255, 210, 70), s / 3 > 1 ? s / 3 : 1);
        for (int k = 0; k < 6; k++) {                                                                           /* серп */
            double a0 = PI * (0.85 + k * 0.22), a1 = a0 + PI * 0.22;
            d_line(h.x + (int)(cos(a0) * s), h.y + (int)(sin(a0) * s), h.x + (int)(cos(a1) * s), h.y + (int)(sin(a1) * s),
                   RGB(255, 210, 70), s / 4 > 1 ? s / 4 : 1);
        }
    }
}

/* салют: залпы разлетаются шарами разноцветных огней */
static void be_salute(double t, double amt)
{
    Cv cv;
    if (!cv_get(&cv) || amt <= 0) return;
    static const unsigned COL[5] = { RGBX(255, 80, 70), RGBX(255, 210, 90), RGBX(110, 230, 140), RGBX(140, 190, 255), RGBX(255, 255, 240) };
    for (int b = 0; b < 12; b++) {
        double t0 = b * 0.38, e = fmod(t - t0, 4.6);
        if (t - t0 < 0 || e > 1.6) continue;
        int seed = b * 97 + (int)((t - t0) / 4.6) * 13;
        double cx = g_w * (0.12 + 0.76 * rnd(seed)), cy = g_h * (0.12 + 0.28 * rnd(seed + 1)), R = g_h * (0.10 + 0.07 * rnd(seed + 2));
        unsigned c = COL[(int)(rnd(seed + 3) * 5) % 5];
        double fade = (1 - sstep(0.6, 1.6, e)) * amt, ex = 1 - exp(-e * 3.2);
        if (e < 0.25) px_blob(&cv, cx, cy, R * 0.5, R * 0.5, c, c, (int)(160 * (1 - e / 0.25) * amt), 1);
        for (int k = 0; k < 36; k++) {
            double a = 2 * PI * k / 36 + rnd(seed + 10 + k) * 0.1, rr = R * ex * (0.85 + 0.25 * rnd(seed + 50 + k));
            double x = cx + cos(a) * rr, y = cy + sin(a) * rr + g_h * 0.05 * e * e;
            px_blob(&cv, x, y, g_h * 0.005 + 1, g_h * 0.005 + 1, c, c, (int)(255 * fade), 1);
            px_blob(&cv, cx + cos(a) * rr * 0.8, cy + sin(a) * rr * 0.8 + g_h * 0.04 * e * e, 1.4, 1.4, c, c, (int)(120 * fade), 1);
        }
    }
}

static void berlin_reichstag(double t)
{
    Cv cv;
    if (!cv_get(&cv)) return;
    double zoom = sstep(BE_FLAG - 2.2, BE_FLAG + 0.8, t), dawn = sstep(BE_SALUTE - 1.0, BE_SALUTE + 1.2, t);
    double k0 = g_w * 0.60 / 138.0, k = k0 * (1 + 0.85 * zoom), gy0 = g_h * 0.82;
    double gy = gy0 + (g_h * 0.30 + 70 * k - gy0) * zoom;
    Xf f = { g_w * 0.5, gy, k, 1, 0 };
    /* небо: дымные сумерки, к салюту — ночь и рассвет над городом */
    Stop sky[3] = { { 0, xmix(RGBX(40, 38, 46), RGBX(8, 10, 26), dawn) }, { 0.6, xmix(RGBX(96, 72, 64), RGBX(30, 30, 60), dawn) },
                    { 1, xmix(RGBX(186, 112, 66), RGBX(150, 90, 80), dawn) } };
    px_grad(&cv, sky, 3, 0, gy0 > gy ? gy0 : gy, 0, g_h);
    px_stars(&cv, 160, dawn * 0.8, t, g_h * 0.6, 77);
    be_salute(t - BE_SALUTE + 0.4, dawn);
    be_ruins(gy - 12 * k, 0.12 + 0.2 * zoom, 0.3 + 0.3 * zoom, RGB(66, 56, 54), 23, t, 1 - 0.7 * dawn);
    s_light = RGBX(255, 226, 186); s_shade = RGBX(30, 24, 30); s_tint = RGBX(255, 150, 90); s_tintAmt = 0.12 * (1 - dawn);
    /* площадь перед зданием */
    if (gy < g_h) ch_rect(0, gy, g_w, g_h - gy + 1, RGB(48, 42, 40));
    berlin_reichstag_body(&f, t);
    be_smoke(t, gy - 20 * k, 9, 0.8 * (1 - dawn));
    /* бойцы бегут к ступеням, танк бьёт по зданию */
    if (zoom < 0.95) {
        int rifle = be_unit("rifle"), tank = be_unit("tank");
        double run = sstep(BE_REICH, BE_FLAG - 0.5, t);
        double sx, sy;
        xpd(&f, -112 + 40 * run, 0, &sx, &sy);
        troops_draw((int)sx, (int)(sy + 9 * k), (int)(30 * k), tank, 1, 1, fmod(t, 1.8) < 0.7 ? TM_FIRE : TM_STAND, 0, 1945, t);
        for (int g = 0; g < 2; g++) {
            xpd(&f, -70 + 52 * run + g * 18, 0, &sx, &sy);
            troops_draw((int)sx, (int)(sy + (4 + g * 6) * k), (int)(24 * k), rifle, 7, 1, TM_MARCH, 0, 1945, t + g);
        }
        if (!cv_get(&cv)) return;
        for (int h = 0; h < 4; h++) {                             /* попадания по фасаду */
            double e = fmod(t + h * 0.45, 1.8);
            if (e > 0.5) continue;
            double hx, hy;
            xpd(&f, -40 + 26 * h + 6 * rnd(h + (int)(t / 1.8) * 5), 8 + 14 * rnd(h * 3 + (int)(t / 1.8)), &hx, &hy);
            px_blob(&cv, hx, hy, 6 * k * (0.5 + e), 4 * k * (0.5 + e), RGBX(255, 210, 140), RGBX(255, 120, 50), (int)(230 * (1 - e / 0.5)), 1);
        }
    }
    /* бойцы на куполе и знамя */
    double fe = sstep(BE_FLAG - 0.2, BE_FLAG + 2.4, t);
    if (zoom > 0.2) {
        int rifle = be_unit("rifle");
        double sx, sy;
        xpd(&f, -2.6, 66.2, &sx, &sy);
        troops_draw((int)sx, (int)sy, (int)(5.5 * k), rifle, 2, 1, TM_STAND, 0, 1945, t);
    }
    be_flag(&f, t, fe);
    if (fe > 0.6) {                                               /* отсвет на знамени */
        if (!cv_get(&cv)) return;
        double fx, fy;
        xpd(&f, 4, 74, &fx, &fy);
        px_blob(&cv, fx, fy, 10 * k, 6 * k, RGBX(255, 200, 150), RGBX(255, 120, 90), (int)(40 * (fe - 0.6) / 0.4), 1);
    }
    ui_anim_request();
}

/* ---------- надписи ---------- */
static void berlin_hud(double t)
{
    Cv cv;
    if (!cv_get(&cv)) return;
    int x0 = SC(28), y0 = SC(22), bandH = SC(112);
    for (int i = 0; i < bandH; i++) px_rrect(&cv, 0, i, g_w, 1, 0, 0, 110 * (bandH - i) / bandH);
    SetTextCharacterExtra(d_dc(), 1);
    stext(x0, y0, t < BE_REICH ? L"БЕРЛИНСКАЯ ОПЕРАЦИЯ · 16 АПРЕЛЯ — 8 МАЯ 1945 ГОДА" : L"БЕРЛИН · 30 АПРЕЛЯ — 2 МАЯ 1945 ГОДА", F_SMALL_B, RGB(255, 206, 120));
    SetTextCharacterExtra(d_dc(), 0);
    HFONT ft = fx_font(SC(36), FW_SEMIBOLD);
    const wchar_t *title = t < BE_REICH ? L"Штурм Берлина" : t < BE_SALUTE ? L"Знамя Победы над Рейхстагом" : L"Победа!";
    fx_text(x0 + 1, y0 + SC(17) + 1, title, ft, RGB(0, 0, 0), 0);
    fx_text(x0, y0 + SC(17), title, ft, RGB(255, 255, 255), 0);

    int bw = SC(150), bh = SC(36), bx = g_w - bw - SC(24), by = SC(22);
    if (!cv_get(&cv)) return;
    px_rrect(&cv, bx, by, bw, bh, SC(8), 0, hover(A_NEXT, FX_SKIP) ? 190 : 120);
    d_text_c(bx, by + (bh - d_font_h(F_BODY_B)) / 2, bw, L"Пропустить  \x203A", F_BODY_B, RGB(255, 255, 255));
    d_text_c(bx, by + bh + SC(4), bw, ESC_HINT, F_TINY, RGB(200, 204, 214));
    hit(bx, by, bw, bh, A_NEXT, FX_SKIP);

    const wchar_t *cap, *sub;
    if (t < 3.8) { cap = L"16 апреля 1945 года — начало Берлинской операции"; sub = L"На Берлин идут 1-й Белорусский фронт маршала Жукова и 1-й Украинский фронт маршала Конева"; }
    else if (t < BE_REICH) { cap = L"Бои за каждую улицу"; sub = L"«Катюши», танки и пехота пробиваются к центру города"; }
    else if (t < BE_FLAG) { cap = L"30 апреля — штурм Рейхстага"; sub = L"Бойцы 3-й ударной армии 1-го Белорусского фронта врываются в здание"; }
    else if (t < BE_SALUTE) { cap = L"В ночь на 1 мая над Рейхстагом — Знамя Победы"; sub = L"Его подняли Михаил Егоров и Мелитон Кантария во главе с лейтенантом Алексеем Берестом"; }
    else { cap = L"2 мая гарнизон Берлина сложил оружие"; sub = L"8 мая подписан акт о безоговорочной капитуляции Германии. 9 мая — День Победы"; }
    int fh = d_font_h(F_H2), sh = d_font_h(F_SMALL) + SC(4);
    int cw = d_text_w(cap, F_H2), sw = d_text_w(sub, F_SMALL);
    int w = (cw > sw ? cw : sw) + SC(44), h = fh + sh + SC(20);
    int cx = (g_w - w) / 2, cy = g_h - SC(34) - h;
    if (!cv_get(&cv)) return;
    px_rrect(&cv, cx, cy, w, h, SC(12), RGBX(6, 8, 18), 165);
    d_text_c(cx, cy + SC(10), w, cap, F_H2, RGB(255, 255, 255));
    d_text_c(cx, cy + SC(12) + fh, w, sub, F_SMALL, RGB(206, 212, 230));
    int px0 = SC(28), pw2 = g_w - 2 * px0, py = g_h - SC(14);
    px_rrect(&cv, px0, py, pw2, SC(4), SC(2), RGBX(255, 255, 255), 50);
    px_rrect(&cv, px0, py, (int)(pw2 * clampd(t / BE_END, 0, 1)), SC(4), SC(2), RGBX(236, 70, 70), 230);
    static const double MARKS[3] = { BE_REICH, BE_FLAG, BE_SALUTE };
    for (int i = 0; i < 3; i++)
        px_blob(&cv, px0 + pw2 * MARKS[i] / BE_END, py + SC(2), SC(4), SC(4), RGBX(255, 255, 255), RGBX(255, 255, 255), 230, 0);
}

/* ---------- карточка ---------- */
#define BE_TEXT L"Берлинскую операцию вели войска 1-го Белорусского (маршал Георгий Жуков), 2-го Белорусского " \
                L"(маршал Константин Рокоссовский) и 1-го Украинского (маршал Иван Конев) фронтов. Белорусскими " \
                L"фронты назывались потому, что летом 1944 года освобождали Беларусь в операции «Багратион». " \
                L"В ночь на 1 мая сержант Михаил Егоров и младший сержант Мелитон Кантария во главе с лейтенантом " \
                L"Алексеем Берестом укрепили на Рейхстаге штурмовой флаг 150-й стрелковой дивизии — он стал Знаменем Победы."
#define BE_WOMEN_TITLE L"Женщины на улицах Берлина"
#define BE_WOMEN_TEXT  L"До Берлина дошли санинструкторы, связистки и снайперы. На перекрёстках поверженного города " \
                       L"движением войск управляли девушки-регулировщицы — их фотографии у Бранденбургских ворот " \
                       L"облетели весь мир."

static int berlin_card(int x, int y, int w, int draw)
{
    int pad = SC(26), cx = x + pad, iw = w - 2 * pad, yy = y + pad;
    COLORREF acc = P.red;
    if (draw) {
        star5(cx + SC(10), yy + SC(10), SC(10), acc);
        SetTextCharacterExtra(d_dc(), 1);
        d_text(cx + SC(28), yy + SC(2), L"БЕРЛИН · 30 АПРЕЛЯ — 2 МАЯ 1945 ГОДА", F_SMALL_B, acc);
        SetTextCharacterExtra(d_dc(), 0);
    }
    yy += SC(30);
    yy += d_wrap(cx, yy, iw, L"Знамя Победы над Рейхстагом", F_ERA_L, P.ink, draw) + SC(10);
    const wchar_t *chips[4] = { L"16 апреля — 8 мая 1945 года", L"1-й и 2-й Белорусские фронты", L"2 мая — Берлин пал", L"9 мая — День Победы" };
    int chx = cx, chh = d_font_h(F_TINY) + SC(4);
    for (int i = 0; i < 4; i++) {
        int cw = d_text_w(chips[i], F_TINY) + SC(12);
        if (chx > cx && chx + cw > cx + iw) { chx = cx; yy += chh + SC(6); }
        if (draw) w_pill(chx, yy, chips[i], i == 3 ? P.onAccent : P.ink, i == 3 ? acc : P.surface2);
        chx += cw + SC(6);
    }
    yy += chh + SC(12);
    yy += d_wrap(cx, yy, iw, BE_TEXT, F_BODY, P.ink, draw) + SC(16);
    int bp = SC(14), bw = iw - 2 * bp - SC(34);
    int th = d_wrap(0, 0, bw, BE_WOMEN_TITLE, F_BODY_B, P.ink, 0), xh = d_wrap(0, 0, bw, BE_WOMEN_TEXT, F_SMALL, P.ink, 0);
    int bh = bp + th + SC(4) + xh + bp;
    if (draw) {
        d_rrect(cx, yy, iw, bh, SC(10), mix(P.surface, P.gold, 30), P.gold);
        star5(cx + bp + SC(12), yy + bp + SC(10), SC(11), P.gold);
        d_wrap(cx + bp + SC(34), yy + bp, bw, BE_WOMEN_TITLE, F_BODY_B, P.ink, 1);
        d_wrap(cx + bp + SC(34), yy + bp + th + SC(4), bw, BE_WOMEN_TEXT, F_SMALL, P.ink, 1);
    }
    yy += bh + SC(18);
    if (draw) {
        int b2 = SC(160), b1 = SC(200);
        w_button(cx + iw - b2, yy, b2, SC(42), L"Продолжить", B_PRIMARY, 1, A_NEXT, FX_CLOSE);
        w_button(cx + iw - b2 - SC(12) - b1, yy, b1, SC(42), L"Смотреть ещё раз", B_SECONDARY, 1, A_NEXT, FX_REPLAY);
    }
    yy += SC(42);
    return yy + pad - y;
}

/* ---------- заставка целиком ---------- */
static void berlin_draw(double t)
{
    double tv = t < BE_END ? t : BE_END + (t - BE_END) * 0.3;
    if (tv < BE_REICH) berlin_streets(tv);
    else berlin_reichstag(tv);
    double d = fabs(tv - BE_REICH);
    if (d < 0.35) d_blend(0, 0, g_w, g_h, RGB(0, 0, 0), (int)(255 * (1 - d / 0.35)));
    if (t < BE_END) {
        hit(0, 0, g_w, g_h, A_NEXT, FX_SKIP);
        berlin_hud(t);
        return;
    }
    double e = sstep(0, 0.45, t - BE_END);
    d_blend(0, 0, g_w, g_h, RGB(0, 0, 0), (int)(150 * e));
    int w = g_w - SC(48) < SC(820) ? g_w - SC(48) : SC(820);
    int h = berlin_card(0, 0, w, 0);
    int x = (g_w - w) / 2, y = (g_h - h) / 2 + (int)((1 - e) * SC(30));
    if (y < SC(12)) y = SC(12);
    d_rrect(x + SC(3), y + SC(6), w, h, SC(14), RGB(0, 0, 0), CLR_INVALID);
    d_rrect(x, y, w, h, SC(14), P.surface, P.red);
    berlin_card(x, y, w, 1);
}

/* в начале хода 1945 года — один раз за партию */
static int berlin_due(void)
{
    if (G.year != 1945 || (G.cutscenes & CS_BERLIN)) return 0;
    G.cutscenes |= CS_BERLIN;
    launch_open(FX_BERLIN);
    return 1;
}
