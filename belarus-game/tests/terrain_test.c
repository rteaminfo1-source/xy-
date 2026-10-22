/* Проверка 3D-карты без Windows: читает res/terrain*.qoi, рисует несколько ракурсов в PPM и меряет время.
 * Вторая часть — земли государств (states.h) в разные годы при отдалении до всей «доски».
 *   gcc -std=c11 -O2 -Isrc tests/terrain_test.c src/terrain3d.c src/image.c -lm -o terrain_test && ./terrain_test OUTDIR */
#include "terrain3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* tests/run.sh собирает этот тест только с terrain3d.c и image.c — данные земель берём прямо из исходника */
#include "data_states.c"

static int load(const char *path, Image *img)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)n);
    int ok = buf && fread(buf, 1, (size_t)n, f) == (size_t)n && qoi_decode(buf, (int)n, img);
    free(buf);
    fclose(f);
    return ok;
}

static void save_ppm(const char *path, const unsigned *px, int w, int h)
{
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) { unsigned c = px[i]; fputc((c >> 16) & 255, f); fputc((c >> 8) & 255, f); fputc(c & 255, f); }
    fclose(f);
}

typedef struct { const char *name; double yaw, pitch, zoom, dx, dy; int ss, year; } View;

/* рисует вид; возвращает 0 при ошибке проекции */
static int render_view(const View *vw, unsigned *px, int w, int h, const char *out, double *msOut)
{
    T3Cam c;
    t3d_cam_reset(&c);
    c.yaw = vw->yaw; c.pitch = vw->pitch; c.zoom = vw->zoom; c.tx += vw->dx; c.ty += vw->dy;
    t3d_cam_clamp(&c);
    T3View v;
    t3d_view(&c, w, h, &v);
    /* лучшее из нескольких — машина может быть занята другими процессами */
    double ms = 1e9;
    for (int r = 0; r < 5; r++) {
        clock_t t0 = clock();
        t3d_render(&v, px, w, 0x2a2622, vw->ss);
        double t = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC;
        if (t < ms) ms = t;
    }
    int drawn = 0;
    for (int i = 0; i < w * h; i++) drawn += (px[i] & 0xFFFFFF) != (0x2a2622 & 0xFFFFFF);
    char path[512];
    snprintf(path, sizeof path, "%s/t3d_%s.ppm", out, vw->name);
    save_ppm(path, px, w, h);
    double sx, sy, d, X, Y;
    int okp = t3d_project(&v, c.tx, c.ty, 9.0, &sx, &sy, &d);
    int oku = t3d_unproject(&v, sx, sy, 9.0, &X, &Y);
    printf("%-14s ss=%d %6.1f мс, закрашено %4.1f%%\n", vw->name, vw->ss, ms, 100.0 * drawn / (w * h));
    if (msOut) *msOut = ms;
    if (!okp || !oku || fabs(X - c.tx) > 0.01 || fabs(Y - c.ty) > 0.01 || fabs(sx - w / 2.0) > 0.5 || fabs(sy - h / 2.0) > 0.5) {
        printf("ОШИБКА: проекция цели (%.2f, %.2f) -> (%.1f, %.1f) -> (%.2f, %.2f)\n", c.tx, c.ty, sx, sy, X, Y);
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";
    Image tex = {0}, hm = {0};
    if (!load("res/terrain.qoi", &tex) || !load("res/terrain_h.qoi", &hm)) { printf("нет res/terrain*.qoi\n"); return 1; }
    if (!t3d_init(&tex, &hm)) { printf("t3d_init не удался\n"); return 1; }
    int w = 900, h = 640, errors = 0;
    unsigned *px = malloc(sizeof(unsigned) * w * h);
    static const View views[] = {
        { "default", 0, 52, 1.0, 0, 0, 2, 0 }, { "rot60", 60, 40, 1.0, 0, 0, 2, 0 }, { "low", 200, 16, 1.6, 0, 0, 2, 0 },
        { "top", 0, 89, 1.0, 0, 0, 2, 0 }, { "zoom_minsk", 20, 45, 6.0, -29.0, 22.3, 2, 0 }, { "fast", 0, 52, 1.0, 0, 0, 1, 0 },
        { "closest", 135, 14, 9.0, 150, -150, 1, 0 },
    };
    for (unsigned k = 0; k < sizeof views / sizeof views[0]; k++) errors += !render_view(&views[k], px, w, h, out, 0);

    /* земли государств: вся «доска» при самом дальнем отдалении, и обычный вид */
    static const View eras[] = {
        { "y1150", 0, 52, 0.35, 0, 0, 2, 1150 }, { "y1400", 0, 52, 0.35, 0, 0, 2, 1400 }, { "y1600", 0, 52, 0.35, 0, 0, 2, 1600 },
        { "y1780", 0, 52, 0.35, 0, 0, 2, 1780 }, { "y1850", 0, 52, 0.35, 0, 0, 2, 1850 }, { "y1930", 0, 52, 0.35, 0, 0, 2, 1930 },
        { "y1942", 0, 52, 0.5, 0, 0, 2, 1942 }, { "y2000", 0, 52, 0.35, 0, 0, 2, 2000 }, { "y1930_near", 0, 52, 1.0, 0, 0, 2, 1930 },
        { "y1600_fast", 30, 40, 0.35, 0, 0, 1, 1600 }, { "y1930_low", 200, 16, 0.6, 0, 0, 2, 1930 },
        { "y1930_minsk", 20, 45, 6.0, -29.0, 22.3, 2, 1930 }, { "y1930_fast", 0, 52, 1.0, 0, 0, 1, 1930 },
    };
    double worst = 0, base = 0;
    for (unsigned k = 0; k < sizeof eras / sizeof eras[0]; k++) {
        T3Area a[T3_MAX_AREAS];
        double bx0, by0, bx1, by1, ms;
        hist_t3_board(&bx0, &by0, &bx1, &by1);
        int n = hist_t3_areas(eras[k].year, a, T3_MAX_AREAS);
        t3d_set_areas(a, n, bx0, by0, bx1, by1);
        errors += !render_view(&eras[k], px, w, h, out, &ms);
        if (eras[k].ss == 2 && ms > worst) worst = ms;
    }
    t3d_set_areas(0, 0, 0, 0, 0, 0);
    {
        View d = views[0];
        d.name = "default_again";
        errors += !render_view(&d, px, w, h, out, &base);
    }
    /* земли не должны делать кадр медленнее, чем вдвое (и не дольше 250 мс на 900×640 при ss = 2) */
    if (worst > 2.5 * base + 40 || worst > 250) { printf("ОШИБКА: земли рисуются медленно: %.1f мс против %.1f\n", worst, base); errors++; }

    double hm_min = 1e9, hm_max = -1e9;
    for (double X = -300; X <= 300; X += 25) for (double Y = -250; Y <= 250; Y += 25) {
        double z = t3d_height(X, Y);
        if (z < hm_min) hm_min = z;
        if (z > hm_max) hm_max = z;
    }
    printf("высоты: %.2f..%.2f км\n", hm_min, hm_max);
    if (hm_min < 3.5 || hm_max > 15) { printf("ОШИБКА: высоты\n"); errors++; }
    printf(errors ? "Ошибок: %d\n" : "Ошибок нет\n", errors);
    return errors != 0;
}
