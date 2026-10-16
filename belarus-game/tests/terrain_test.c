/* Проверка 3D-карты без Windows: читает res/terrain*.qoi, рисует несколько ракурсов в PPM и меряет время.
 *   gcc -std=c11 -O2 -Isrc tests/terrain_test.c src/terrain3d.c src/image.c -lm -o terrain_test && ./terrain_test OUTDIR */
#include "terrain3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

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

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : ".";
    Image tex = {0}, hm = {0};
    if (!load("res/terrain.qoi", &tex) || !load("res/terrain_h.qoi", &hm)) { printf("нет res/terrain*.qoi\n"); return 1; }
    if (!t3d_init(&tex, &hm)) { printf("t3d_init не удался\n"); return 1; }
    int w = 900, h = 640, errors = 0;
    unsigned *px = malloc(sizeof(unsigned) * w * h);
    struct { const char *name; double yaw, pitch, zoom, dx, dy; int ss; } views[] = {
        { "default", 0, 52, 1.0, 0, 0, 2 }, { "rot60", 60, 40, 1.0, 0, 0, 2 }, { "low", 200, 16, 1.6, 0, 0, 2 },
        { "top", 0, 89, 1.0, 0, 0, 2 }, { "zoom_minsk", 20, 45, 6.0, -29.0, 22.3, 2 }, { "fast", 0, 52, 1.0, 0, 0, 1 },
        { "closest", 135, 14, 9.0, 150, -150, 1 },
    };
    for (unsigned k = 0; k < sizeof views / sizeof views[0]; k++) {
        T3Cam c;
        t3d_cam_reset(&c);
        c.yaw = views[k].yaw; c.pitch = views[k].pitch; c.zoom = views[k].zoom; c.tx += views[k].dx; c.ty += views[k].dy;
        t3d_cam_clamp(&c);
        T3View v;
        t3d_view(&c, w, h, &v);
        clock_t t0 = clock();
        int reps = 5;
        for (int r = 0; r < reps; r++) t3d_render(&v, px, w, 0x2a2622, views[k].ss);
        double ms = (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC / reps;
        /* доля нарисованных (не фоновых) пикселей */
        int drawn = 0;
        for (int i = 0; i < w * h; i++) drawn += (px[i] & 0xFFFFFF) != (0x2a2622 & 0xFFFFFF) && (i / w) < h;
        char path[512];
        snprintf(path, sizeof path, "%s/t3d_%s.ppm", out, views[k].name);
        save_ppm(path, px, w, h);
        double sx, sy, d, X, Y;
        int okp = t3d_project(&v, c.tx, c.ty, 9.0, &sx, &sy, &d);
        int oku = t3d_unproject(&v, sx, sy, 9.0, &X, &Y);
        if (!okp || !oku || fabs(X - c.tx) > 0.01 || fabs(Y - c.ty) > 0.01 || fabs(sx - w / 2.0) > 0.5 || fabs(sy - h / 2.0) > 0.5) {
            printf("ОШИБКА: проекция цели (%.2f, %.2f) -> (%.1f, %.1f) -> (%.2f, %.2f)\n", c.tx, c.ty, sx, sy, X, Y);
            errors++;
        }
        printf("%-11s ss=%d %6.1f мс, закрашено %4.1f%%\n", views[k].name, views[k].ss, ms, 100.0 * drawn / (w * h));
    }
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
