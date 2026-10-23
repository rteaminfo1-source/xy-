/* Авария на Чернобыльской АЭС, 26 апреля 1986 года: заставка во весь экран перед событием «Чернобыль».
 * Включается в ui_fx.c (общие помощники: пиксели заднего буфера, шрифты, надписи).
 *   План 1 — ночная станция у самой границы Беларуси, взрыв четвёртого энергоблока, пожар и пожарные.
 *   План 2 — настоящая 3D-карта: облако идёт на Беларусь, выпадают радиоактивные дожди, появляются пятна
 *            загрязнения, затем 30-километровая зона, отселённые деревни и Полесский заповедник.
 *   В конце — карточка: факты, пожарный Василий Игнатенко и книга Светланы Алексиевич.
 * Как и взлёт ракеты, кадр — функция времени: заставку можно пропустить, повторить и снять в любой момент. */

#define CH_BOOM  3.4    /* 1:23 — взрыв */
#define CH_FIRE  6.8    /* пожарные у четвёртого блока */
#define CH_MAP   10.4   /* карта: облако над Беларусью */
#define CH_FALL  14.6   /* 70% осадков, 23% территории */
#define CH_ZONE  18.4   /* 30-километровая зона */
#define CH_END   23.4   /* карточка */

#define CH_LON 30.099   /* Чернобыльская АЭС */
#define CH_LAT 51.389

static void launch_open(int person);
static double s_chT;                 /* время заставки для слоя карты (fx_map_overlay) */
static T3Cam s_chCam;                /* камера игрока: вернуть после заставки */
static int s_chCamInit, s_chCamSaved;

/* ---------- план 1: станция ночью ---------- */
static void ch_rect(double x, double y, double w, double h, COLORREF c)
{
    d_fill((int)lround(x), (int)lround(y), (int)lround(w) > 0 ? (int)lround(w) : 1, (int)lround(h) > 0 ? (int)lround(h) : 1, c);
}

/* вентиляционная труба: сужается кверху, красно-белые пояса (ночью — приглушённые) */
static void ch_stack(double x, double gy, double u, double hgt, double t, int seed)
{
    double b = 1.25 * u, tp = 0.8 * u;
    for (int i = 0; i < 8; i++) {
        double y0 = gy - hgt * u * i / 8, y1 = gy - hgt * u * (i + 1) / 8;
        double w0 = b + (tp - b) * i / 8, w1 = b + (tp - b) * (i + 1) / 8;
        POINT p[4] = { { (LONG)lround(x - w0), (LONG)lround(y0) }, { (LONG)lround(x + w0), (LONG)lround(y0) },
                       { (LONG)lround(x + w1), (LONG)lround(y1) }, { (LONG)lround(x - w1), (LONG)lround(y1) } };
        d_poly(p, 4, i & 1 ? RGB(112, 116, 124) : RGB(104, 34, 36), CLR_INVALID, 1);
    }
    /* площадки-кольца */
    for (int i = 1; i < 4; i++) {
        double y = gy - hgt * u * i / 4, w = b + (tp - b) * i / 4 + 0.35 * u;
        d_line((int)lround(x - w), (int)lround(y), (int)lround(x + w), (int)lround(y), RGB(60, 64, 72), 1);
    }
    Cv cv;
    if (!cv_get(&cv)) return;
    double on = 0.5 + 0.5 * sin(t * 3.1 + seed);
    px_blob(&cv, x, gy - hgt * u - 0.2 * u, 1.3 * u, 1.3 * u, RGBX(255, 60, 50), RGBX(255, 60, 50), (int)(60 + 180 * on), 1);
}

/* пожарная машина: x — середина, y — дорога; едет вправо */
static void ch_truck(double x, double y, double u, double t, int i)
{
    ch_rect(x - 3.4 * u, y - 2.5 * u, 5.2 * u, 1.9 * u, RGB(168, 30, 28));           /* цистерна */
    ch_rect(x + 1.8 * u, y - 3.0 * u, 1.7 * u, 2.4 * u, RGB(186, 36, 32));           /* кабина */
    ch_rect(x + 2.3 * u, y - 2.7 * u, 1.0 * u, 0.8 * u, RGB(70, 90, 110));           /* стекло */
    ch_rect(x - 3.4 * u, y - 0.7 * u, 6.9 * u, 0.25 * u, RGB(40, 40, 44));          /* рама */
    d_line((int)lround(x - 3.2 * u), (int)lround(y - 2.9 * u), (int)lround(x + 1.4 * u), (int)lround(y - 2.9 * u), RGB(170, 170, 176), 1);   /* лестница */
    for (int k = 0; k < 3; k++) {
        double wx = x + (-2.4 + k * 2.2) * u;
        d_circle((int)lround(wx), (int)lround(y - 0.35 * u), (int)lround(0.55 * u), RGB(18, 18, 20), RGB(60, 60, 64), 1);
    }
    Cv cv;
    if (!cv_get(&cv)) return;
    int blink = fmod(t * 3.0 + i * 0.37, 1.0) < 0.5;
    px_blob(&cv, x + 2.6 * u, y - 3.3 * u, 1.4 * u, 1.0 * u, RGBX(80, 130, 255), RGBX(80, 130, 255), blink ? 240 : 50, 1);
    px_blob(&cv, x + 3.7 * u, y - 1.3 * u, 2.4 * u, 0.7 * u, RGBX(255, 236, 190), RGBX(255, 236, 190), 120, 1);   /* фары */
}

static void chaes_plant(double t)
{
    Cv cv;
    if (!cv_get(&cv)) return;
    double u = g_h / 82.0, cx = g_w * 0.52, gy = g_h * 0.72, wy = gy + 6.0 * u;
    double te = t - CH_BOOM;                         /* после взрыва (меньше 0 — до него) */
    double bx = cx + 25 * u, by = gy - 15 * u;       /* четвёртый энергоблок */
    double flick = 0.75 + 0.25 * sin(t * 11.0) * sin(t * 3.7 + 1);

    /* небо и звёзды; после взрыва — зарево */
    static const Stop SKY[] = { { 0, RGBX(3, 5, 14) }, { 0.6, RGBX(10, 15, 32) }, { 1, RGBX(28, 30, 46) } };
    px_grad(&cv, SKY, 3, 0, gy, 0, (int)gy + 1);
    px_stars(&cv, 240, 0.9 - 0.55 * sstep(0, 2.5, te), t, gy - 6 * u, 31);
    if (te > 0) {
        double z = sstep(0, 0.8, te);
        px_blob(&cv, bx, by, 70 * u, 40 * u, RGBX(150, 60, 20), RGBX(120, 40, 10), (int)(90 * z * flick), 1);
    }

    /* дальний лес по горизонту */
    static POINT fp[1100];
    int n = 0;
    double step = g_w / 420.0;
    fp[n].x = 0; fp[n].y = (LONG)g_h; n++;
    for (int i = 0; i <= 420 && n < 1090; i++) {
        double x = i * step, h = (1.2 + 1.9 * rnd(900 + i) + 0.8 * sin(i * 0.07)) * u;
        fp[n].x = (LONG)lround(x); fp[n].y = (LONG)lround(gy - h); n++;
    }
    fp[n].x = (LONG)g_w; fp[n].y = (LONG)g_h; n++;
    d_poly(fp, n, RGB(7, 10, 13), CLR_INVALID, 1);
    ch_rect(0, gy, g_w, g_h - gy, RGB(12, 15, 18));
    ch_rect(0, gy + 2.6 * u, g_w, 0.5 * u, RGB(30, 32, 36));                       /* дорога */

    /* водоём-охладитель */
    if (!cv_get(&cv)) return;
    static const Stop WATER[] = { { 0, RGBX(16, 22, 34) }, { 1, RGBX(5, 8, 15) } };
    px_grad(&cv, WATER, 2, wy, g_h, (int)wy, g_h);

    /* опоры ЛЭП слева */
    for (int k = 0; k < 3; k++) {
        double px = cx - (78 - k * 11) * u, ph = 13 * u;
        d_line((int)px, (int)gy, (int)(px - 1.6 * u), (int)(gy - ph), RGB(40, 44, 52), 1);
        d_line((int)px, (int)gy, (int)(px + 1.6 * u), (int)(gy - ph), RGB(40, 44, 52), 1);
        d_line((int)(px - 3 * u), (int)(gy - ph), (int)(px + 3 * u), (int)(gy - ph), RGB(40, 44, 52), 1);
        if (k < 2) d_line((int)(px + 3 * u), (int)(gy - ph), (int)(px + 11 * u - 3 * u), (int)(gy - ph + 0.8 * u), RGB(32, 36, 42), 1);
    }
    d_line((int)(cx - 56 * u - 3 * u), (int)(gy - 12.2 * u), (int)(cx - 44 * u), (int)(gy - 8 * u), RGB(32, 36, 42), 1);

    /* машинный зал и корпуса энергоблоков: 1–2 слева, 3–4 справа */
    ch_rect(cx - 46 * u, gy - 8 * u, 86 * u, 8 * u, RGB(36, 42, 52));
    ch_rect(cx - 46 * u, gy - 8 * u, 86 * u, 0.35 * u, RGB(66, 74, 88));
    ch_rect(cx - 40 * u, gy - 17 * u, 24 * u, 9 * u, RGB(44, 50, 62));
    ch_rect(cx - 4 * u, gy - 18 * u, 20 * u, 10 * u, RGB(44, 50, 62));
    if (te <= 0) ch_rect(cx + 16 * u, gy - 18 * u, 18 * u, 10 * u, RGB(44, 50, 62));
    else {                                           /* разрушенный четвёртый блок */
        POINT p[] = { { (LONG)(cx + 16 * u), (LONG)(gy - 8 * u) }, { (LONG)(cx + 16 * u), (LONG)(gy - 18 * u) },
                      { (LONG)(cx + 18.5 * u), (LONG)(gy - 16 * u) }, { (LONG)(cx + 20 * u), (LONG)(gy - 17.2 * u) },
                      { (LONG)(cx + 22.5 * u), (LONG)(gy - 12.5 * u) }, { (LONG)(cx + 25 * u), (LONG)(gy - 14 * u) },
                      { (LONG)(cx + 27.5 * u), (LONG)(gy - 11.5 * u) }, { (LONG)(cx + 30 * u), (LONG)(gy - 15.5 * u) },
                      { (LONG)(cx + 32 * u), (LONG)(gy - 13 * u) }, { (LONG)(cx + 34 * u), (LONG)(gy - 16 * u) },
                      { (LONG)(cx + 34 * u), (LONG)(gy - 8 * u) } };
        d_poly(p, (int)(sizeof p / sizeof p[0]), RGB(40, 44, 54), CLR_INVALID, 1);
    }
    /* окна */
    for (int i = 0; i < 52; i++) {
        if (rnd(400 + i) < 0.35) continue;
        double x = cx - 44.5 * u + i * 1.62 * u;
        if (te > 0 && x > cx + 14 * u && rnd(450 + i) < 0.7) continue;   /* у разрушенного блока свет погас */
        ch_rect(x, gy - 5.6 * u, 0.62 * u, 1.0 * u, rnd(500 + i) < 0.5 ? RGB(214, 170, 96) : RGB(150, 120, 74));
    }
    for (int i = 0; i < 26; i++) {
        if (rnd(600 + i) < 0.55) continue;
        double x = cx - 39 * u + i * 2.6 * u;
        if (x > cx - 17 * u && x < cx - 3 * u) continue;
        if (te > 0 && x > cx + 15 * u) continue;
        ch_rect(x, gy - 14 * u, 0.7 * u, 0.9 * u, RGB(190, 150, 86));
    }
    ch_stack(cx - 28 * u, gy - 17 * u, u, 17, t, 0);
    ch_stack(cx + 16 * u, gy - 18 * u, u, 18, t, 2);

    /* фонари вдоль дороги */
    if (!cv_get(&cv)) return;
    for (int i = 0; i < 9; i++) {
        double x = cx - 52 * u + i * 12 * u;
        px_blob(&cv, x, gy + 0.9 * u, 2.6 * u, 1.6 * u, RGBX(255, 190, 110), RGBX(255, 160, 80), 120, 1);
        px_blob(&cv, x, wy + 2.5 * u + i % 2 * u, 0.6 * u, 3.5 * u, RGBX(255, 170, 90), RGBX(120, 70, 30), 60, 1);   /* отражение */
    }

    if (te > 0) {
        /* огонь в развалинах и на крыше машинного зала */
        px_blob(&cv, bx, by + 1.5 * u, 10 * u, 5 * u, RGBX(255, 170, 70), RGBX(255, 90, 30), (int)(200 * flick), 1);
        for (int k = 0; k < 6; k++) {
            double on = sstep(0.4 + k * 0.25, 1.1 + k * 0.25, te), fl = 0.8 + 0.2 * sin(t * (9 + k) + k);
            double fx = cx + (2 + k * 6.5) * u, fy = gy - 8.6 * u;
            px_blob(&cv, fx, fy - 1.2 * u * fl, 1.0 * u, 2.0 * u * fl, RGBX(255, 230, 140), RGBX(255, 110, 30), (int)(220 * on), 1);
        }
        /* голубоватое свечение воздуха над открытым реактором */
        double gl = sstep(0.8, 2.0, te);
        px_blob(&cv, bx, by - 24 * u, 2.4 * u, 24 * u, RGBX(90, 130, 255), RGBX(60, 90, 220), (int)(55 * gl * (0.85 + 0.15 * sin(t * 7))), 1);

        /* огненный шар */
        double fb = 1 - sstep(0.3, 2.6, te), rr = (3 + 21 * sstep(0, 0.7, te)) * u, rise = 7 * u * sstep(0, 1.6, te);
        px_blob(&cv, bx, by - rise, rr, rr * 0.9, RGBX(255, 210, 120), RGBX(255, 110, 40), (int)(250 * fb), 1);
        px_blob(&cv, bx, by - rise, rr * 0.45, rr * 0.4, RGBX(255, 250, 230), RGBX(255, 230, 170), (int)(255 * fb * fb), 1);

        /* дымный столб: клубы рождаются каждые 0,2 с, поднимаются и расплываются по ветру */
        for (int k = 0; k < 60; k++) {
            double b = 0.15 + k * 0.2, age = te - b;
            if (age < 0) break;
            if (age > 13) continue;
            double h = (9.5 * age - 0.33 * age * age) * u, x = bx + (2.2 * age + 1.4 * sin(k * 1.7 + age * 0.6)) * u;
            double r = (2.6 + 2.3 * age) * u, a = 165 * (1 - age / 13) * sstep(0, 0.4, age);
            unsigned bot = xmix(RGBX(60, 54, 56), RGBX(200, 92, 40), clampd(1.4 - age * 0.45, 0, 1) * flick);
            px_blob(&cv, x, by - 2 * u - h, r, r * 0.82, RGBX(48, 46, 54), bot, (int)a, 0);
        }

        /* обломки: летят по дугам и остывают */
        double g = 55 * u;
        for (int i = 0; i < 60 && te < 4.5; i++) {
            double vx = (rnd(i * 7) - 0.5) * 70 * u, vy = (0.35 + rnd(i * 7 + 1)) * 62 * u;
            double x = bx + vx * te, y = by - (vy * te - 0.5 * g * te * te);
            if (y > gy - 0.5 * u) continue;
            double s = (0.25 + rnd(i * 7 + 2) * 0.7) * u;
            ch_rect(x - s / 2, y - s / 2, s, s, RGB(22, 18, 16));
            if (!cv_get(&cv)) return;
            px_blob(&cv, x, y, s * 1.7, s * 1.7, RGBX(255, 170, 70), RGBX(255, 110, 40), (int)(220 * (1 - sstep(0.4, 2.4, te))), 1);
        }

        /* отражение пожара в воде */
        if (!cv_get(&cv)) return;
        px_blob(&cv, bx, wy + 3 * u, 18 * u, 3.2 * u, RGBX(255, 140, 60), RGBX(200, 70, 20), (int)(120 * flick * sstep(0, 0.6, te)), 1);

        /* пожарные машины подъезжают к четвёртому блоку и тушат огонь */
        for (int i = 0; i < 3; i++) {
            double t0 = 2.0 + i * 0.55, target = cx + (5 + i * 9) * u, start = -14 * u;
            if (te < t0) continue;
            double x = start + (target - start) * sstep(t0, t0 + 2.8, te);
            ch_truck(x, gy + 2.9 * u, u, t, i);
            double sp = sstep(t0 + 3.0, t0 + 3.6, te);
            if (sp <= 0) continue;
            if (!cv_get(&cv)) return;
            double ax = x - 1.5 * u, ay = gy + 0.2 * u, ex = bx - 6 * u + i * 4 * u, ey = gy - 9.5 * u - i * 1.5 * u;
            double mx = (ax + ex) / 2, my = (ay < ey ? ay : ey) - 9 * u;
            for (int k = 0; k < 16; k++) {
                double s = fmod(k / 16.0 + t * 0.9, 1.0), q = 1 - s;
                double px = q * q * ax + 2 * q * s * mx + s * s * ex, py = q * q * ay + 2 * q * s * my + s * s * ey;
                px_blob(&cv, px, py, 0.55 * u, 0.55 * u, RGBX(170, 210, 255), RGBX(120, 170, 240), (int)(150 * sp), 1);
            }
        }
        /* вспышка взрыва */
        if (te < 1.2) d_blend(0, 0, g_w, g_h, RGB(255, 238, 214), (int)(235 * exp(-te * 4.0)));
    }
}

/* ---------- план 2: 3D-карта ---------- */
/* камера: от станции — ко всей стране, потом к 30-километровой зоне */
static void chaes_cam(double tm)
{
    if (!U.camInit) { t3d_cam_reset(&U.cam); U.camInit = 1; }
    double e = sstep(0.2, 4.6, tm), z = CH_ZONE - CH_MAP, e2 = sstep(z - 0.4, z + 2.6, tm);
    double lon = 30.15 + (29.55 - 30.15) * e, lat = 51.8 + (52.55 - 51.8) * e;
    double zoom = exp(log(4.0) + (log(1.25) - log(4.0)) * e), pitch = 50 + 10 * e;
    lon += (29.95 - lon) * e2; lat += (51.72 - lat) * e2;
    zoom = exp(log(zoom) + (log(3.3) - log(zoom)) * e2);
    pitch += (52 - pitch) * e2;
    t3d_lonlat(lon, lat, &U.cam.tx, &U.cam.ty);
    U.cam.zoom = zoom; U.cam.pitch = pitch; U.cam.yaw = 0;
    t3d_cam_clamp(&U.cam);
    U.mapFast = (tm > 0.2 && tm < 4.6) || (tm > z - 0.4 && tm < z + 2.6);
}

/* пятна загрязнения цезием-137: lon, lat, радиус км, уровень 1–3, когда появляется (с от начала карты) */
static const struct { double lon, lat, r; int lv; double at; } CH_SPOTS[] = {
    { 30.20, 52.40, 75, 1, 1.0 }, { 31.00, 53.20, 58, 1, 2.8 },             /* широкая дымка: Гомельщина, Могилёвщина */
    { 26.60, 52.05, 48, 1, 3.0 },                                            /* Брестское Полесье */
    { 29.25, 52.05, 20, 1, 1.6 }, { 29.15, 51.80, 18, 1, 1.8 }, { 30.40, 52.36, 18, 1, 2.0 }, { 30.25, 53.50, 18, 1, 3.9 },
    { 26.10, 52.10, 18, 1, 3.4 }, { 26.80, 52.22, 22, 2, 3.0 }, { 26.85, 51.90, 22, 1, 3.2 },
    { 31.10, 52.55, 26, 2, 2.4 }, { 30.65, 52.72, 20, 2, 2.6 }, { 31.35, 52.45, 16, 2, 2.5 }, { 30.85, 51.95, 18, 2, 1.4 },
    { 31.05, 53.56, 22, 2, 3.6 }, { 32.00, 53.35, 22, 2, 3.8 },
    { 29.97, 51.74, 36, 3, 0.4 }, { 29.55, 51.72, 22, 3, 0.7 }, { 30.38, 51.70, 20, 3, 0.6 },   /* у станции */
    { 30.88, 52.92, 22, 3, 2.8 }, { 31.38, 53.38, 28, 3, 3.4 },                                  /* Чечерск, Краснополье */
};

/* пути облака (26 апреля — начало мая): на северо-запад, на север к Могилёвщине и на запад по Полесью */
static const double CH_TRACK[3][5][2] = {
    { { CH_LON, CH_LAT }, { 29.55, 51.95 }, { 28.40, 52.60 }, { 27.10, 53.40 }, { 25.80, 54.30 } },
    { { CH_LON, CH_LAT }, { 30.45, 52.10 }, { 30.85, 52.80 }, { 31.30, 53.40 }, { 31.90, 54.10 } },
    { { CH_LON, CH_LAT }, { 29.20, 51.85 }, { 28.10, 52.05 }, { 26.90, 52.10 }, { 25.70, 52.20 } },
};

static void ch_km(double lon, double lat, double *X, double *Y) { t3d_lonlat(lon, lat, X, Y); }

/* точка пути tr на расстоянии s км от станции; 0 — путь кончился */
static int ch_track_at(int tr, double s, double *X, double *Y, double *nx, double *ny)
{
    for (int i = 0; i < 4; i++) {
        double ax, ay, bx, by;
        ch_km(CH_TRACK[tr][i][0], CH_TRACK[tr][i][1], &ax, &ay);
        ch_km(CH_TRACK[tr][i + 1][0], CH_TRACK[tr][i + 1][1], &bx, &by);
        double L = hypot(bx - ax, by - ay);
        if (s <= L) {
            *X = ax + (bx - ax) * s / L; *Y = ay + (by - ay) * s / L;
            *nx = -(by - ay) / L; *ny = (bx - ax) / L;
            return 1;
        }
        s -= L;
    }
    return 0;
}

/* эллипс на земле: середина, полуоси в пикселях */
static int ch_ground(const MapProj *pj, double X, double Y, double R, double Z, double *sx, double *sy, double *rx, double *ry)
{
    int x, y, x2, y2;
    double k = 0, k2 = 0;
    if (!mapv_xyz(pj, X, Y, Z, &x, &y, &k)) return 0;
    *sx = x; *sy = y; *rx = R * k;
    *ry = mapv_xyz(pj, X, Y + R, Z, &x2, &y2, &k2) ? fabs((double)(y2 - y)) : *rx * 0.6;
    if (*ry < *rx * 0.3) *ry = *rx * 0.3;
    return *rx > 0.5;
}

static void radiation_sign(int cx, int cy, int r)
{
    d_circle(cx, cy, r, RGB(250, 206, 40), RGB(40, 30, 0), 1);
    for (int k = 0; k < 3; k++) {
        double mid = PI / 2 + k * 2 * PI / 3, a0 = mid - PI / 6, a1 = mid + PI / 6, ri = r * 0.28, ro = r * 0.84;
        POINT p[18];
        int n = 0;
        for (int i = 0; i <= 8; i++) { double a = a0 + (a1 - a0) * i / 8; p[n].x = cx + (LONG)lround(cos(a) * ro); p[n].y = cy - (LONG)lround(sin(a) * ro); n++; }
        for (int i = 8; i >= 0; i--) { double a = a0 + (a1 - a0) * i / 8; p[n].x = cx + (LONG)lround(cos(a) * ri); p[n].y = cy - (LONG)lround(sin(a) * ri); n++; }
        d_poly(p, n, RGB(26, 20, 4), CLR_INVALID, 1);
    }
    d_circle(cx, cy, r * 18 / 100 > 1 ? r * 18 / 100 : 1, RGB(26, 20, 4), CLR_INVALID, 1);
}

static void ch_label(int x, int y, const wchar_t *s, const wchar_t *sub, COLORREF c)
{
    int tw = d_text_w(s, F_SMALL_B), sw = sub ? d_text_w(sub, F_TINY) : 0, w = (tw > sw ? tw : sw) + SC(10);
    int h = d_font_h(F_SMALL_B) + (sub ? d_font_h(F_TINY) : 0) + SC(4);
    d_blend(x - SC(5), y - SC(2), w, h, RGB(10, 12, 18), 190);
    d_text(x, y, s, F_SMALL_B, c);
    if (sub) d_text(x, y + d_font_h(F_SMALL_B), sub, F_TINY, RGB(214, 218, 228));
}

void fx_map_overlay(const MapProj *pj, int interactive, int large)
{
    (void)interactive; (void)large;
    if (U.modal != M_LAUNCH || U.launchPerson != FX_CHAES) return;
    double tm = s_chT - CH_MAP, z = CH_ZONE - CH_MAP;
    Cv cv;
    if (!cv_get(&cv)) return;
    double X0, Y0;
    ch_km(CH_LON, CH_LAT, &X0, &Y0);

    /* пятна загрязнения: сначала слабые, сверху сильные */
    static const unsigned SPOT_C[4] = { 0, RGBX(238, 202, 66), RGBX(236, 124, 40), RGBX(198, 34, 58) };
    static const int SPOT_A[4] = { 0, 105, 140, 165 };
    for (int lv = 1; lv <= 3; lv++)
        for (unsigned i = 0; i < sizeof CH_SPOTS / sizeof CH_SPOTS[0]; i++) {
            if (CH_SPOTS[i].lv != lv) continue;
            double e = sstep(CH_SPOTS[i].at, CH_SPOTS[i].at + 1.2, tm);
            if (e <= 0) continue;
            double X, Y, sx, sy, rx, ry;
            ch_km(CH_SPOTS[i].lon, CH_SPOTS[i].lat, &X, &Y);
            if (!ch_ground(pj, X, Y, CH_SPOTS[i].r, 0, &sx, &sy, &rx, &ry)) continue;
            px_blob(&cv, sx, sy, rx, ry, SPOT_C[lv], xmix(SPOT_C[lv], 0, 0.15), (int)(SPOT_A[lv] * e), 0);
        }

    /* 30-километровая зона и Полесский заповедник */
    double ze = sstep(z, z + 1.0, tm);
    if (ze > 0) {
        double sx, sy, rx, ry;
        if (ch_ground(pj, X0, Y0, 30, 0, &sx, &sy, &rx, &ry)) px_blob(&cv, sx, sy, rx, ry, RGBX(120, 10, 30), RGBX(90, 0, 20), (int)(70 * ze), 0);
        double re = sstep(z + 2.4, z + 3.4, tm), RX, RY;
        ch_km(29.9, 51.66, &RX, &RY);
        if (re > 0 && ch_ground(pj, RX, RY, 34, 0, &sx, &sy, &rx, &ry))
            px_blob(&cv, sx, sy, rx * 1.1, ry * 0.7, RGBX(60, 170, 90), RGBX(40, 130, 70), (int)(90 * re), 0);
    }

    /* облако: клубы уходят от станции по трём путям; через неделю оно проходит */
    double fade = 1 - sstep(z - 0.8, z + 1.2, tm);
    for (int i = 0; i < 210 && fade > 0; i++) {
        int tr = i % 3;
        double b = (i / 3) * 0.09, age = tm - b;
        if (age <= 0) continue;
        double s = age * 95, X, Y, nx, ny;
        if (!ch_track_at(tr, s, &X, &Y, &nx, &ny)) continue;
        double off = (rnd(i * 5) - 0.5) * 56 + 9 * sin(age * 1.3 + i), r = 13 + 0.05 * s;
        double sx, sy, rx, ry;
        if (!ch_ground(pj, X + nx * off, Y + ny * off, r, 1.5, &sx, &sy, &rx, &ry)) continue;
        double a = 82 * fade * sstep(0, 0.4, age);
        px_blob(&cv, sx, sy, rx, ry * 1.1, RGBX(96, 84, 108), RGBX(52, 46, 62), (int)a, 0);
    }

    /* граница 30-километровой зоны — пунктир, отселённые деревни */
    if (ze > 0) {
        POINT prev = { 0, 0 };
        int have = 0;
        for (int k = 0; k <= 64; k++) {
            double a = 2 * PI * k / 64;
            int x, y;
            double kk;
            if (!mapv_xyz(pj, X0 + 30 * cos(a), Y0 + 30 * sin(a), 0, &x, &y, &kk)) { have = 0; continue; }
            if (have && k % 2) d_line(prev.x, prev.y, x, y, RGB(236, 64, 80), SC(2));
            prev.x = x; prev.y = y; have = 1;
        }
        double ve = sstep(z + 1.2, z + 2.2, tm);
        for (int i = 0; i < 18 && ve > 0; i++) {
            double a = PI * (0.25 + 0.5 * rnd(700 + i)), d = 19 + 10 * rnd(720 + i);   /* белорусская часть зоны — севернее станции */
            int x, y;
            double kk;
            if (!mapv_xyz(pj, X0 + d * cos(a), Y0 + d * sin(a), 0, &x, &y, &kk)) continue;
            if (i > (int)(18 * ve)) continue;
            int s = SC(4);
            d_fill(x - s, y - s / 2, 2 * s, s + s / 2, RGB(150, 150, 156));
            d_line(x - s - SC(1), y - s, x + s + SC(1), y + s, RGB(236, 64, 80), SC(2));
            d_line(x - s - SC(1), y + s, x + s + SC(1), y - s, RGB(236, 64, 80), SC(2));
        }
    }

    /* станция: знак радиации и пульсирующие кольца */
    int sx, sy;
    double k;
    if (mapv_ll(pj, CH_LON, CH_LAT, 0, &sx, &sy, &k)) {
        for (int j = 0; j < 2 && tm < z; j++) {
            double ph = fmod(tm / 1.4 + j * 0.5, 1.0);
            d_circle(sx, sy, SC(14) + (int)(ph * SC(46)), CLR_INVALID, mix(RGB(236, 64, 80), P.surface, (int)(ph * 220)), 2);
        }
        radiation_sign(sx, sy, SC(13));
        ch_label(sx + SC(18), sy - SC(10), L"Чернобыльская АЭС", L"взорвался 4-й энергоблок", RGB(255, 214, 120));
    }
    if (ze > 0) {
        static const struct { const wchar_t *name; double lon, lat; } TOWNS[] = {
            { L"Брагин", 30.27, 51.79 }, { L"Хойники", 29.96, 51.89 }, { L"Наровля", 29.50, 51.80 } };
        for (int i = 0; i < 3; i++) {
            int x, y;
            double kk;
            if (!mapv_ll(pj, TOWNS[i].lon, TOWNS[i].lat, 0, &x, &y, &kk)) continue;
            d_circle(x, y, SC(4), RGB(240, 240, 240), RGB(20, 20, 24), 1);
            ch_label(x + SC(8), y - SC(8), TOWNS[i].name, NULL, RGB(240, 240, 244));
        }
        int x, y;
        double kk;
        if (mapv_xyz(pj, X0, Y0 + 30, 0, &x, &y, &kk)) ch_label(x - SC(60), y - SC(26), L"30-километровая зона", NULL, RGB(255, 140, 150));
        if (tm > z + 2.8 && mapv_ll(pj, 29.55, 51.58, 0, &x, &y, &kk))
            ch_label(x - SC(150), y + SC(8), L"Полесский заповедник", L"создан в 1988 году", RGB(140, 230, 160));
    }
    ui_anim_request();
}

/* ---------- надписи поверх заставки ---------- */
static void chaes_hud(double t)
{
    Cv cv;
    if (!cv_get(&cv)) return;
    int x0 = SC(28), y0 = SC(22), bandH = SC(112);
    for (int i = 0; i < bandH; i++) px_rrect(&cv, 0, i, g_w, 1, 0, 0, 110 * (bandH - i) / bandH);
    int map = t >= CH_MAP;
    SetTextCharacterExtra(d_dc(), 1);
    stext(x0, y0, map ? L"БЕЛОРУССКАЯ ССР · АПРЕЛЬ — МАЙ 1986 ГОДА" : L"ЧЕРНОБЫЛЬСКАЯ АЭС · У САМОЙ ГРАНИЦЫ БЕЛАРУСИ", F_SMALL_B, RGB(255, 206, 120));
    SetTextCharacterExtra(d_dc(), 0);
    HFONT ft = fx_font(SC(36), FW_SEMIBOLD);
    const wchar_t *title = map ? L"Облако над Беларусью" : L"26 апреля 1986 года";
    fx_text(x0 + 1, y0 + SC(17) + 1, title, ft, RGB(0, 0, 0), 0);
    int tw = fx_text(x0, y0 + SC(17), title, ft, RGB(255, 255, 255), 0);
    if (!map) {                                       /* часы */
        int cx = x0 + tw + SC(22), cy = y0 + SC(21), cw = SC(96), ch = SC(38);
        if (!cv_get(&cv)) return;
        px_rrect(&cv, cx, cy, cw, ch, SC(8), RGBX(30, 6, 8), 200);
        HFONT fc = fx_font(SC(26), FW_BOLD);
        fx_text(cx + cw / 2, cy + SC(3), L"01:23", fc, t >= CH_BOOM && fmod(t, 0.8) < 0.4 ? RGB(255, 90, 80) : RGB(255, 160, 120), 1);
    }

    /* «Пропустить» */
    int bw = SC(150), bh = SC(36), bx = g_w - bw - SC(24), by = SC(22);
    if (!cv_get(&cv)) return;
    px_rrect(&cv, bx, by, bw, bh, SC(8), 0, hover(A_NEXT, FX_SKIP) ? 190 : 120);
    d_text_c(bx, by + (bh - d_font_h(F_BODY_B)) / 2, bw, L"Пропустить  \x203A", F_BODY_B, RGB(255, 255, 255));
    d_text_c(bx, by + bh + SC(4), bw, ESC_HINT, F_TINY, RGB(200, 204, 214));
    hit(bx, by, bw, bh, A_NEXT, FX_SKIP);

    /* легенда на карте */
    if (map) {
        int lw = SC(230), lh = SC(104), lx = SC(24), ly = g_h - SC(150) - lh;
        if (!cv_get(&cv)) return;
        px_rrect(&cv, lx, ly, lw, lh, SC(10), RGBX(10, 12, 18), 200);
        d_text(lx + SC(12), ly + SC(10), L"Загрязнение цезием-137", F_SMALL_B, RGB(255, 255, 255));
        static const struct { unsigned c; const wchar_t *s; } LG[3] = {
            { RGBX(198, 34, 58), L"сильное" }, { RGBX(236, 124, 40), L"среднее" }, { RGBX(238, 202, 66), L"слабое" } };
        for (int i = 0; i < 3; i++) {
            int yy = ly + SC(36) + i * SC(21);
            d_circle(lx + SC(20), yy + SC(8), SC(7), cr(LG[i].c), CLR_INVALID, 1);
            d_text(lx + SC(34), yy, LG[i].s, F_SMALL, RGB(226, 230, 240));
        }
    }

    /* подпись этапа и шкала времени снизу */
    const wchar_t *cap, *sub = 0;
    if (t < CH_BOOM) { cap = L"Ночь. На четвёртом энергоблоке идёт испытание"; sub = L"Чернобыльская АЭС стоит на Украине, у самой границы Беларуси"; }
    else if (t < CH_FIRE) { cap = L"1:23 — взрыв! Разрушен четвёртый реактор"; sub = L"Над станцией поднимается радиоактивное облако"; }
    else if (t < CH_MAP) { cap = L"Первыми огонь тушили пожарные"; sub = L"Среди них — Василий Игнатенко из Брагинского района Гомельской области"; }
    else if (t < CH_FALL) { cap = L"Ветер несёт облако на Беларусь"; sub = L"Радиоактивные дожди выпадают над Гомельской и Могилёвской областями, над Полесьем"; }
    else if (t < CH_ZONE) { cap = L"На Беларусь выпало около 70% радиоактивных осадков"; sub = L"Загрязнено около 23% территории республики"; }
    else { cap = L"Из 30-километровой зоны отселяют целые деревни"; sub = L"Брагинский, Хойникский и Наровлянский районы. С 1988 года здесь Полесский заповедник"; }
    int fh = d_font_h(F_H2), sh = sub ? d_font_h(F_SMALL) + SC(4) : 0;
    int cw = d_text_w(cap, F_H2), sw = sub ? d_text_w(sub, F_SMALL) : 0;
    int w = (cw > sw ? cw : sw) + SC(44), h = fh + sh + SC(20);
    int cx = (g_w - w) / 2, cy = g_h - SC(34) - h;
    if (!cv_get(&cv)) return;
    px_rrect(&cv, cx, cy, w, h, SC(12), RGBX(6, 8, 18), 165);
    d_text_c(cx, cy + SC(10), w, cap, F_H2, RGB(255, 255, 255));
    if (sub) d_text_c(cx, cy + SC(12) + fh, w, sub, F_SMALL, RGB(206, 212, 230));
    int px0 = SC(28), pw2 = g_w - 2 * px0, py = g_h - SC(14);
    px_rrect(&cv, px0, py, pw2, SC(4), SC(2), RGBX(255, 255, 255), 50);
    px_rrect(&cv, px0, py, (int)(pw2 * clampd(t / CH_END, 0, 1)), SC(4), SC(2), RGBX(236, 90, 90), 230);
    static const double MARKS[3] = { CH_BOOM, CH_MAP, CH_ZONE };
    for (int i = 0; i < 3; i++)
        px_blob(&cv, px0 + pw2 * MARKS[i] / CH_END, py + SC(2), SC(4), SC(4), RGBX(255, 255, 255), RGBX(255, 255, 255), 230, 0);
}

/* ---------- карточка ---------- */
#define CH_TEXT L"В ночь на 26 апреля 1986 года на Чернобыльской АЭС, у самой границы Беларуси, взорвался четвёртый " \
                L"энергоблок. На Беларусь выпало около 70% радиоактивных осадков, загрязнено около 23% территории — " \
                L"больше всего Гомельская и Могилёвская области. Жителей самых опасных деревень отселили, а в 1988 году " \
                L"в зоне создали Полесский радиационно-экологический заповедник. Первыми огонь тушили пожарные — " \
                L"среди них Василий Игнатенко из Брагинского района."
#define CH_WOMEN_TITLE L"Голоса Чернобыля"
#define CH_WOMEN_TEXT  L"Светлана Алексиевич собрала рассказы очевидцев в книгу «Чернобыльская молитва» (1997). Она " \
                       L"открывается рассказом Людмилы Игнатенко — жены пожарного. В 2015 году писательница получила " \
                       L"Нобелевскую премию по литературе."

static int chaes_card(int x, int y, int w, int draw)
{
    int pad = SC(26), cx = x + pad, iw = w - 2 * pad, yy = y + pad;
    COLORREF acc = P.red;
    if (draw) {
        d_icon(IC_DISASTER, cx, yy, SC(20), acc);
        SetTextCharacterExtra(d_dc(), 1);
        d_text(cx + SC(28), yy + SC(2), L"26 АПРЕЛЯ — ДЕНЬ ЧЕРНОБЫЛЬСКОЙ ТРАГЕДИИ", F_SMALL_B, acc);
        SetTextCharacterExtra(d_dc(), 0);
    }
    yy += SC(30);
    yy += d_wrap(cx, yy, iw, L"Чернобыльская катастрофа", F_ERA_L, P.ink, draw) + SC(10);
    const wchar_t *chips[4] = { L"26 апреля 1986 года", L"≈70% осадков — на Беларусь", L"≈23% территории загрязнено", L"заповедник — с 1988 года" };
    int chx = cx, chh = d_font_h(F_TINY) + SC(4);
    for (int i = 0; i < 4; i++) {
        int cw = d_text_w(chips[i], F_TINY) + SC(12);
        if (chx > cx && chx + cw > cx + iw) { chx = cx; yy += chh + SC(6); }
        if (draw) w_pill(chx, yy, chips[i], i == 0 ? P.onAccent : P.ink, i == 0 ? acc : P.surface2);
        chx += cw + SC(6);
    }
    yy += chh + SC(12);
    yy += d_wrap(cx, yy, iw, CH_TEXT, F_BODY, P.ink, draw) + SC(16);
    /* Светлана Алексиевич и «Чернобыльская молитва» */
    int p = person_index("alexievich"), bp = SC(14), pw = p >= 0 ? SC(64) : 0, ph = SC(80);
    int bw = iw - 2 * bp - (pw ? pw + SC(14) : 0);
    int th = d_wrap(0, 0, bw, CH_WOMEN_TITLE, F_BODY_B, P.ink, 0), xh = d_wrap(0, 0, bw, CH_WOMEN_TEXT, F_SMALL, P.ink, 0);
    int body = th + SC(4) + xh, bh = bp + (pw && ph > body ? ph : body) + bp;
    if (draw) {
        d_rrect(cx, yy, iw, bh, SC(10), mix(P.surface, P.gold, 30), P.gold);
        int tx = cx + bp;
        if (pw) { portrait_box(PEOPLE[p].id, PEOPLE[p].name, tx, yy + bp, pw, ph, P.gold); tx += pw + SC(14); }
        d_wrap(tx, yy + bp, bw, CH_WOMEN_TITLE, F_BODY_B, P.ink, 1);
        d_wrap(tx, yy + bp + th + SC(4), bw, CH_WOMEN_TEXT, F_SMALL, P.ink, 1);
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
static void chaes_draw(double t)
{
    double tv = t < CH_END ? t : CH_END + (t - CH_END) * 0.2;
    s_chT = tv;
    if (tv < CH_MAP) chaes_plant(tv);
    else {
        chaes_cam(tv - CH_MAP);
        map_draw_ex(0, 0, g_w, g_h, 0, 1);
        d_unclip();
    }
    double d = fabs(tv - CH_MAP);                     /* смена плана — через затемнение */
    if (d < 0.35) d_blend(0, 0, g_w, g_h, RGB(0, 0, 0), (int)(255 * (1 - d / 0.35)));
    if (t < CH_END) {
        hit(0, 0, g_w, g_h, A_NEXT, FX_SKIP);
        chaes_hud(t);
        return;
    }
    U.mapFast = 0;
    double e = sstep(0, 0.45, t - CH_END);
    d_blend(0, 0, g_w, g_h, RGB(0, 0, 0), (int)(165 * e));
    int w = g_w - SC(48) < SC(820) ? g_w - SC(48) : SC(820);
    int h = chaes_card(0, 0, w, 0);
    int x = (g_w - w) / 2, y = (g_h - h) / 2 + (int)((1 - e) * SC(30));
    if (y < SC(12)) y = SC(12);
    d_rrect(x + SC(3), y + SC(6), w, h, SC(14), RGB(0, 0, 0), CLR_INVALID);
    d_rrect(x, y, w, h, SC(14), P.surface, P.red);
    chaes_card(x, y, w, 1);
}

/* открывается перед событием «Чернобыль» (один раз за партию) */
int fx_ui_event(const Event *e)
{
    if (berlin_due()) return 1;                      /* 1945: штурм Берлина (fx_berlin.inc) — до окон хода */
    if (!e || !e->id || strcmp(e->id, "chernobyl") != 0 || (G.cutscenes & CS_CHERNOBYL)) return 0;
    G.cutscenes |= CS_CHERNOBYL;
    launch_open(FX_CHAES);
    return 1;
}
