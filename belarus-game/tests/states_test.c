/* Проверка исторических территорий (data_states.c) без Windows: многоугольники простые и лежат на «доске»,
 * государство есть в каждом году 1100–2026, подписи внутри своих земель, вся Беларусь чья-то в каждом году,
 * в годы войны 1941–1945 — «СССР», земель в году не больше, чем принимает 3D-карта, доска закрашена без щелей.
 *   gcc -std=c11 -O2 -Isrc tests/states_test.c src/engine.c src/war.c src/front.c src/citynet.c src/data_*.c -lm -o t && ./t */
#include "game.h"
#include "states.h"
#include "terrain3d.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <math.h>

static int errors, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("ОШИБКА: "); printf(__VA_ARGS__); printf("\n"); errors++; } } while (0)

static double cross(const float *o, const float *a, const float *b)
{
    return ((double)a[0] - o[0]) * ((double)b[1] - o[1]) - ((double)a[1] - o[1]) * ((double)b[0] - o[0]);
}

/* отрезки ab и cd пересекаются (касание концами соседних рёбер сюда не попадает) */
static int seg_cross(const float *a, const float *b, const float *c, const float *d)
{
    double d1 = cross(c, d, a), d2 = cross(c, d, b), d3 = cross(a, b, c), d4 = cross(a, b, d);
    if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0))) return 1;
    /* общая точка у несоседних рёбер — тоже не простой многоугольник */
    return (a[0] == c[0] && a[1] == c[1]) || (a[0] == d[0] && a[1] == d[1]) || (b[0] == c[0] && b[1] == c[1]) || (b[0] == d[0] && b[1] == d[1]);
}

static int inside_pts(const float (*p)[2], int n, double x, double y)
{
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++)
        if ((p[i][1] > y) != (p[j][1] > y) && x < (p[j][0] - p[i][0]) * (y - p[i][1]) / (p[j][1] - p[i][1]) + p[i][0]) in = !in;
    return in;
}

static float s_border[128][2];

static int inside_area(const HistArea *a, double x, double y)
{
    return a->n ? inside_pts(a->pts, a->n, x, y) : inside_pts((const float (*)[2])s_border, BORDER_N, x, y);
}

static void test_polygons(void)
{
    for (int i = 0; i < HIST_AREA_COUNT + HIST_WATER_COUNT; i++) {
        const HistArea *a = i < HIST_AREA_COUNT ? &HIST_AREAS[i] : &HIST_WATER[i - HIST_AREA_COUNT];
        char nm[160];
        snprintf(nm, sizeof nm, "%ls (%d–%d)", a->name, a->from, a->to);
        CHECK(a->name && a->name[0], "земля %d без названия", i);
        CHECK(a->role >= HA_STATE && a->role <= HA_SUB, "%s: роль %d", nm, a->role);
        CHECK(a->from >= 1100 && a->from <= a->to && a->to <= 2026, "%s: годы", nm);
        CHECK(a->color != 0 && a->color <= 0xFFFFFF, "%s: цвет", nm);
        CHECK(a->n == 0 ? (a->role == HA_STATE || a->role == HA_PART) : (a->n >= 3 && a->n <= HIST_MAX_PTS), "%s: n = %d", nm, a->n);
        double ar = 0;
        for (int k = 0; k < a->n; k++) {
            const float *p = a->pts[k], *q = a->pts[(k + 1) % a->n];
            CHECK(p[0] >= HIST_LON0 && p[0] <= HIST_LON1 && p[1] >= HIST_LAT0 && p[1] <= HIST_LAT1,
                  "%s: точка %d (%.2f, %.2f) вне доски", nm, k, p[0], p[1]);
            CHECK(p[0] != q[0] || p[1] != q[1], "%s: повтор точки %d", nm, k);
            ar += (double)p[0] * q[1] - (double)q[0] * p[1];
            for (int m = k + 2; m < a->n; m++) {
                if (k == 0 && m == a->n - 1) continue;           /* соседние через замыкание */
                CHECK(!seg_cross(p, q, a->pts[m], a->pts[(m + 1) % a->n]), "%s: рёбра %d и %d пересекаются", nm, k, m);
            }
        }
        if (a->n) CHECK(fabs(ar) / 2 > 0.01, "%s: пустая площадь", nm);
        if (a->labelLon != 0 || a->labelLat != 0)
            CHECK(inside_area(a, a->labelLon, a->labelLat), "%s: подпись (%.2f, %.2f) вне своей земли", nm, a->labelLon, a->labelLat);
        else CHECK(a->role == HA_NEIGHBOR || a->role == HA_SUB, "%s: главной земле нужна подпись", nm);
        /* часть государства лежит внутри государства своего времени */
        if ((a->role == HA_PART || a->role == HA_SUB) && (a->labelLon != 0 || a->labelLat != 0)) {
            int ok = 0;
            for (int s = 0; s < HIST_AREA_COUNT; s++) {
                const HistArea *b = &HIST_AREAS[s];
                if (b->role == HA_STATE && b->from <= a->from && a->from <= b->to && inside_area(b, a->labelLon, a->labelLat)) ok = 1;
            }
            CHECK(ok, "%s: часть вне своего государства", nm);
        }
    }
}

static void test_years(void)
{
    int idx[64];
    for (int y = 1100; y <= 2026; y++) {
        int n = hist_areas_at(y, idx, 64), states = 0;
        CHECK(n + HIST_WATER_COUNT <= T3_MAX_AREAS, "%d: земель %d и вода — больше, чем берёт 3D-карта (%d)", y, n, T3_MAX_AREAS);
        for (int k = 0; k < n; k++) {
            states += HIST_AREAS[idx[k]].role == HA_STATE;
            if (k) CHECK(hist_area_prio(HIST_AREAS[idx[k - 1]].role) <= hist_area_prio(HIST_AREAS[idx[k]].role), "%d: порядок наложения", y);
        }
        CHECK(states >= 1, "%d: нет государства", y);
        CHECK(y < 1250 || states == 1, "%d: государств %d", y, states);
        const wchar_t *s = hist_state_name(y);
        CHECK(s && s[0], "%d: нет названия государства", y);
        /* каждая точка Беларуси (сетка 0.25°) принадлежит какой-то земле этого года */
        int miss = 0;
        double mlon = 0, mlat = 0;
        for (double lon = 23.6; lon < 32.8; lon += 0.25)
            for (double lat = 51.3; lat < 56.2; lat += 0.25) {
                if (!inside_pts((const float (*)[2])s_border, BORDER_N, lon, lat)) continue;
                int hit = 0;
                for (int k = 0; k < n && !hit; k++) hit = inside_area(&HIST_AREAS[idx[k]], lon, lat);
                if (!hit && !miss++) { mlon = lon; mlat = lat; }
            }
        CHECK(miss == 0, "%d: %d точек Беларуси без земли (первая — %.2f, %.2f)", y, miss, mlon, mlat);
    }
    for (int t = 0; t < TURN_COUNT; t++) CHECK(hist_state_name(TURN_YEARS[t])[0], "ход %d без названия", TURN_YEARS[t]);
    for (int y = 1941; y <= 1945; y++) CHECK(!wcscmp(hist_state_name(y), L"СССР"), "%d: «%ls» вместо «СССР»", y, hist_state_name(y));
    CHECK(!wcscmp(hist_state_name(1600), L"Речь Посполитая"), "1600 — Речь Посполитая");
    CHECK(!wcscmp(hist_state_name(1400), L"Великое Княжество Литовское"), "1400 — ВКЛ");
    CHECK(!wcscmp(hist_state_name(1850), L"Российская империя"), "1850 — Российская империя");
    CHECK(!wcscmp(hist_state_name(2026), L"Республика Беларусь"), "2026 — Республика Беларусь");
    /* Западная Беларусь в составе Польши — только между Рижским договором и 1939 годом */
    int wb = 0;
    for (int i = 0; i < HIST_AREA_COUNT; i++)
        if (HIST_AREAS[i].role == HA_FOREIGN && wcsstr(HIST_AREAS[i].name, L"Западная Беларусь"))
            { wb++; CHECK(HIST_AREAS[i].from >= 1921 && HIST_AREAS[i].to <= 1939, "Западная Беларусь: %d–%d", HIST_AREAS[i].from, HIST_AREAS[i].to); }
    CHECK(wb > 0, "нет Западной Беларуси");
    /* годы земли целиком (записи подряд с тем же именем склеиваются): подпись «1921–1939» и «с 1772 г.» */
    int f = 0, t = 0, seen = 0;
    for (int i = 0; i < HIST_AREA_COUNT; i++) {
        const HistArea *a = &HIST_AREAS[i];
        if (a->role != HA_FOREIGN) continue;
        hist_area_years(a, &f, &t);
        CHECK(f <= a->from && a->to <= t, "%ls: годы %d–%d не покрывают %d–%d", a->name, f, t, a->from, a->to);
        if (wcsstr(a->name, L"Западная Беларусь")) { seen++; CHECK(f == 1921 && t == 1938, "Западная Беларусь целиком: %d–%d", f, t); }
        if (a->from == 1772 || a->from == 1793) { seen++; CHECK(f == 1772 && t == 1794, "земли разделов: %d–%d", f, t); }
    }
    CHECK(seen >= 2, "нет земель под властью соседа для проверки лет");
}

/* в каждом году хода вся «доска» закрашена: земли и вода без щелей (сетка 0.1°) */
static void test_cover(void)
{
    int idx[64];
    for (int ti = 0; ti < TURN_COUNT; ti++) {
        int y = TURN_YEARS[ti], n = hist_areas_at(y, idx, 64), miss = 0;
        double mlon = 0, mlat = 0;
        for (double lon = HIST_LON0 + 0.05; lon < HIST_LON1; lon += 0.1)
            for (double lat = HIST_LAT0 + 0.05; lat < HIST_LAT1; lat += 0.1) {
                int hit = 0;
                for (int k = 0; k < HIST_WATER_COUNT && !hit; k++) hit = inside_area(&HIST_WATER[k], lon, lat);
                for (int k = 0; k < n && !hit; k++) hit = inside_area(&HIST_AREAS[idx[k]], lon, lat);
                if (!hit && !miss++) { mlon = lon; mlat = lat; }
            }
        CHECK(miss == 0, "%d: %d точек доски без земли (первая — %.2f, %.2f)", y, miss, mlon, mlat);
    }
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    data_init();
    for (int i = 0; i < BORDER_N && i < 128; i++) { s_border[i][0] = (float)BORDER[i].lon; s_border[i][1] = (float)BORDER[i].lat; }
    test_polygons();
    test_years();
    test_cover();
    printf("Исторические земли: %d многоугольников; проверок %d, ошибок %d\n", HIST_AREA_COUNT, checks, errors);
    return errors ? 1 : 0;
}
