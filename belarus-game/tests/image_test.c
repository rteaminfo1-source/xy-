/* image_test.c — проверка конвейера картинок: кодировщик QOI (tools/pack_assets.py)
 * -> декодер (src/image.c), масштабирование, наложение и map3d_project.
 *
 *   T=/tmp/qoitest
 *   python3 tools/pack_assets.py --testdata $T
 *   gcc -std=c11 -g -O1 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       -I$T tests/image_test.c -o $T/image_test -lm
 *   $T/image_test $T
 *
 * --testdata пишет PNG (PIL), их QOI (наш кодировщик), эталоны .exp (PIL +
 * премультипликация), синтетические ассеты и assets_gen.h с аффинной сеткой. */
#include "assets_gen.h"          /* синтетический, из каталога testdata (-I$T) */
#include "../src/image.c"
#include "../src/map3d.c"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); \
                           printf(__VA_ARGS__); printf("\n"); } } while (0)

static unsigned char *read_file(const char *dir, const char *name, int *size)
{
    char path[1024];
    FILE *f;
    long n;
    unsigned char *buf;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f) { printf("нет файла %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (unsigned char *)malloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) exit(2);
    fclose(f);
    *size = (int)n;
    return buf;
}

static unsigned rng_state = 2463534242u;
static unsigned rnd(void) { rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5; return rng_state; }

/* испорченные и обрезанные входы: только не падать (ASan/UBSan следят) */
static void fuzz(const unsigned char *q, int n, const char *name)
{
    unsigned char *m = (unsigned char *)malloc((size_t)n);
    Image im;
    int len, it, ok = 0, step = n > 4000 ? n / 997 : 1;
    for (len = 0; len < n; len += step) {
        unsigned char *t = (unsigned char *)malloc(len ? (size_t)len : 1);   /* точный размер: ASan видит выход */
        memcpy(t, q, (size_t)len);
        CHECK(!qoi_decode(t, len, &im) && !im.px, "%s: обрезанный до %d байт принят", name, len);
        free(t);
    }
    for (it = 0; it < 3000; it++) {
        int k, flips = 1 + (int)(rnd() % 8);
        memcpy(m, q, (size_t)n);
        for (k = 0; k < flips; k++) {
            int at = (int)(rnd() % (unsigned)n);
            if (it % 5 == 0 && at < 14) at += 4;        /* чаще портим размеры в заголовке */
            m[at] = (unsigned char)rnd();
        }
        if (qoi_decode(m, n, &im)) {
            ok++;
            CHECK(im.w > 0 && im.h > 0 && im.px, "%s: странный результат", name);
            image_free(&im);
        }
    }
    free(m);
    (void)ok;
}

static void test_files(const char *dir)
{
    char line[512], qn[256], en[256], path[1024];
    FILE *lf;
    snprintf(path, sizeof path, "%s/list.txt", dir);
    lf = fopen(path, "r");
    if (!lf) { printf("нет %s\n", path); exit(2); }
    while (fgets(line, sizeof line, lf)) {
        int qs, es, i, bad = 0, w, h;
        unsigned char *q, *e;
        Image im;
        if (sscanf(line, "%255s %255s", qn, en) != 2) continue;
        q = read_file(dir, qn, &qs);
        e = read_file(dir, en, &es);
        w = e[0] | e[1] << 8 | e[2] << 16 | e[3] << 24;
        h = e[4] | e[5] << 8 | e[6] << 16 | e[7] << 24;
        CHECK(qoi_decode(q, qs, &im), "%s не декодируется", qn);
        CHECK(im.w == w && im.h == h && es == 8 + 4 * w * h, "%s: размер %dx%d, ждали %dx%d", qn, im.w, im.h, w, h);
        if (im.px && im.w == w && im.h == h) {
            for (i = 0; i < w * h; i++) {
                const unsigned char *p = e + 8 + 4 * i;
                unsigned want = (unsigned)p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
                if (im.px[i] != want) {
                    if (bad < 3) printf("  %s: пиксель %d (%d,%d) = %08X, ждали %08X\n", qn, i, i % w, i / w, im.px[i], want);
                    bad++;
                }
            }
            CHECK(bad == 0, "%s: %d несовпавших пикселей из %d", qn, bad, w * h);
            printf("ok  %-24s %4dx%-4d %7d байт QOI\n", qn, w, h, qs);
        }
        image_free(&im);
        fuzz(q, qs, qn);
        free(q);
        free(e);
    }
    fclose(lf);
}

static void test_header_guards(void)
{
    static const unsigned char hdr[] = { 'q', 'o', 'i', 'f', 0x7F, 0xFF, 0xFF, 0xFF, 0x7F, 0xFF, 0xFF, 0xFF, 4, 0,
                                         0xC0 | 61, 0xC0 | 61, 0, 0, 0, 0, 0, 0, 0, 1 };
    unsigned char ok1[] = { 'q', 'o', 'i', 'f', 0, 0, 0, 2, 0, 0, 0, 1, 4, 0, 0xFE, 10, 20, 30, 0xC0, 0, 0, 0, 0, 0, 0, 0, 1 };
    Image im;
    CHECK(!qoi_decode(hdr, (int)sizeof hdr, &im), "гигантский размер принят");
    CHECK(!qoi_decode(NULL, 100, &im) && !qoi_decode(ok1, -5, &im), "NULL/отрицательный размер");
    CHECK(qoi_decode(ok1, (int)sizeof ok1, &im) && im.w == 2 && im.h == 1 &&
          im.px[0] == 0xFF0A141E && im.px[1] == 0xFF0A141E, "мини-файл 2x1 (RGB + RUN)");
    image_free(&im);
    ok1[sizeof ok1 - 1] = 2;                       /* испорченный маркер конца */
    CHECK(!qoi_decode(ok1, (int)sizeof ok1, &im), "битый маркер конца принят");
    ok1[sizeof ok1 - 1] = 1;
    ok1[12] = 5;                                   /* недопустимое число каналов */
    CHECK(!qoi_decode(ok1, (int)sizeof ok1, &im), "channels=5 принят");
}

static void test_premul(void)
{
    unsigned r, a, bad = 0;
    for (r = 0; r < 256; r++)
        for (a = 0; a < 256; a++)
            if (div255(r * a) != (r * a * 2 + 255) / 510) bad++;
    CHECK(bad == 0, "div255 неточен в %u случаях", bad);
}

static Image mk(int w, int h, const unsigned *px)
{
    Image im;
    image_alloc(&im, w, h);
    memcpy(im.px, px, sizeof(unsigned) * (size_t)w * h);
    return im;
}

static void test_scale(void)
{
    static const unsigned chk[4] = { 0xFFFFFFFF, 0, 0, 0xFFFFFFFF };
    static const unsigned bw[4] = { 0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF };
    static const unsigned row2[2] = { 0xFF000000, 0xFFFFFFFF };
    static const unsigned row3[3] = { 0xFF000000, 0xFF5A5A5A, 0xFFFFFFFF };
    Image s, d, big;
    int i, x, y, bad;

    s = mk(2, 2, chk);
    image_scale(&s, 0, 0, 2, 2, &d, 1, 1);
    CHECK(d.px && d.px[0] == 0x80808080, "шахматка 2x2 (прозрачная) -> %08X", d.px ? d.px[0] : 0);
    image_free(&d); image_free(&s);
    s = mk(2, 2, bw);
    image_scale(&s, 0, 0, 2, 2, &d, 1, 1);
    CHECK(d.px && d.px[0] == 0xFF808080, "шахматка 2x2 (ч/б) -> %08X", d.px ? d.px[0] : 0);
    image_free(&d);
    image_scale(&s, 1, 0, 1, 2, &d, 1, 1);         /* правый столбец: белый + чёрный */
    CHECK(d.px && d.px[0] == 0xFF808080, "подпрямоугольник -> %08X", d.px ? d.px[0] : 0);
    image_free(&d); image_free(&s);

    s = mk(2, 1, row2);                            /* увеличение 2 -> 4: 0, 64, 191, 255 */
    image_scale(&s, 0, 0, 2, 1, &d, 4, 1);
    CHECK(d.px && d.px[0] == 0xFF000000 && d.px[1] == 0xFF404040 && d.px[2] == 0xFFBFBFBF && d.px[3] == 0xFFFFFFFF,
          "билинейно 2->4: %08X %08X %08X %08X", d.px[0], d.px[1], d.px[2], d.px[3]);
    image_free(&d); image_free(&s);
    s = mk(3, 1, row3);                            /* площадь 3 -> 2: (2*0+90)/3=30, (90+2*255)/3=200 */
    image_scale(&s, 0, 0, 3, 1, &d, 2, 1);
    CHECK(d.px && d.px[0] == 0xFF1E1E1E && d.px[1] == 0xFFC8C8C8, "площадь 3->2: %08X %08X", d.px[0], d.px[1]);
    image_free(&d); image_free(&s);

    image_alloc(&big, 53, 37);                     /* случайная премультиплицированная картинка */
    for (i = 0; i < 53 * 37; i++) {
        unsigned a = rnd() & 255, r = rnd() % (a + 1), g = rnd() % (a + 1), b = rnd() % (a + 1);
        big.px[i] = a << 24 | r << 16 | g << 8 | b;
    }
    image_scale(&big, 0, 0, 53, 37, &d, 53, 37);
    CHECK(d.px && !memcmp(d.px, big.px, sizeof(unsigned) * 53 * 37), "тождественное масштабирование");
    image_free(&d);
    image_scale(&big, 10, 5, 7, 9, &d, 7, 9);
    bad = 0;
    for (y = 0; y < 9 && d.px; y++)
        for (x = 0; x < 7; x++) bad += d.px[y * 7 + x] != big.px[(5 + y) * 53 + 10 + x];
    CHECK(d.px && !bad, "вырезка без масштаба: %d ошибок", bad);
    image_free(&d);
    {
        static const int sz[][2] = { { 1, 1 }, { 26, 18 }, { 17, 5 }, { 106, 74 }, { 300, 11 }, { 3, 200 }, { 53, 1 } };
        for (i = 0; i < (int)(sizeof sz / sizeof sz[0]); i++) {
            int k, viol = 0;
            image_scale(&big, -3, -2, 60, 45, &d, sz[i][0], sz[i][1]);
            CHECK(d.px && d.w == sz[i][0] && d.h == sz[i][1], "масштаб в %dx%d", sz[i][0], sz[i][1]);
            for (k = 0; d.px && k < d.w * d.h; k++) {
                unsigned p = d.px[k], a = p >> 24;
                viol += (p >> 16 & 255) > a || (p >> 8 & 255) > a || (p & 255) > a;
            }
            CHECK(!viol, "премультипликация нарушена в %d пикселях (%dx%d)", viol, sz[i][0], sz[i][1]);
            image_free(&d);
        }
    }
    image_scale(&big, 60, 0, 5, 5, &d, 4, 4);
    CHECK(!d.px && d.w == 0, "прямоугольник вне картинки");
    image_scale(&big, 0, 0, 5, 5, &d, 0, 4);
    CHECK(!d.px, "нулевой размер");
    image_free(&big);
}

static void test_blit(void)
{
    enum { W = 8, H = 6 };
    unsigned buf[W * H + 2], *dst = buf + 1;
    Image red, semi;
    int x, y, i, bad;
    static const unsigned one[1] = { 0x80800000 };
    image_alloc(&red, 4, 4);
    for (i = 0; i < 16; i++) red.px[i] = 0xFFFF0000;
    for (i = 0; i < W * H + 2; i++) buf[i] = 0x00102030;
    buf[0] = buf[W * H + 1] = 0xDEADBEEF;          /* сторожа */
    image_blit(dst, W, H, -2, -1, &red);
    image_blit(dst, W, H, 6, 4, &red);
    image_blit(dst, W, H, 100, 100, &red);
    image_blit(dst, W, H, -100, 2, &red);
    bad = 0;
    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            int in = (x < 2 && y < 3) || (x >= 6 && y >= 4);
            bad += dst[y * W + x] != (in ? 0x00FF0000u : 0x00102030u);
        }
    CHECK(!bad && buf[0] == 0xDEADBEEF && buf[W * H + 1] == 0xDEADBEEF, "отсечение blit: %d ошибок", bad);

    semi = mk(1, 1, one);
    dst[0] = 0x000000FF;
    image_blit(dst, W, H, 0, 0, &semi);
    CHECK(dst[0] == 0x0080007F, "полупрозрачный blit -> %08X", dst[0]);
    dst[1] = 0x00FFFFFF;
    image_blit_part(dst, W, H, 1, 0, &red, 3, 3, 5, 5, 128);   /* 1 пиксель, непрозрачность 50% */
    CHECK(dst[1] == 0x00FF7F7F && dst[2] == 0x00102030, "blit_part с alpha -> %08X %08X", dst[1], dst[2]);
    image_fill_under(&semi, 0x000000FF);
    CHECK(semi.px[0] == 0xFF80007F, "fill_under -> %08X", semi.px[0]);
    image_free(&semi);
    image_free(&red);
}

static void test_map(const char *dir)
{
    char path[1024], kind[16];
    double lon, lat, ex, ey, px, py, maxe = 0;
    int k, n = 0;
    FILE *f;
    CHECK(map3d_available() == 1 && ASSETS_HAVE_SPRITES && ASSETS_HAVE_PORTRAITS, "синтетические ассеты не найдены");
    CHECK(MAP3D_W == 300 && MAP3D_H == 420 && MAP3D_NLON == 17 && MAP3D_NLAT == 10, "размеры сетки");
    CHECK(fabs(map3d_px_per_km() - 0.45) < 1e-9, "px_per_km");
    CHECK(SPR_CELL == 16 && SPR_COLS == 4 && SPR_ANCHOR_X == 8 && SPR_ANCHOR_Y == 14 && SPR_COUNT == 3 &&
          !strcmp(SPR_NAMES[2], "mill"), "sprites.json");
    CHECK(PORTRAIT_CELL_W == 20 && PORTRAIT_CELL_H == 40 && PORTRAIT_COLS == 3 && PORTRAIT_COUNT == 3 &&
          !strcmp(PORTRAIT_IDS[1], "skaryna"), "portraits.json");
    snprintf(path, sizeof path, "%s/map_expect.txt", dir);
    f = fopen(path, "r");
    if (!f) { printf("нет %s\n", path); exit(2); }
    while (fscanf(f, "%15s", kind) == 1) {
        if (!strcmp(kind, "city")) {
            if (fscanf(f, "%d %lf %lf", &k, &ex, &ey) != 3) break;
            CHECK(map3d_city(k, &px, &py) && fabs(px - ex) < 0.01 && fabs(py - ey) < 0.01,
                  "город %d: (%.2f, %.2f), ждали (%.2f, %.2f)", k, px, py, ex, ey);
        } else if (!strcmp(kind, "grid")) {
            if (fscanf(f, "%lf %lf %lf %lf", &lon, &lat, &ex, &ey) != 4) break;
            CHECK(map3d_project(lon, lat, &px, &py), "точка %.4f %.4f вне сетки", lon, lat);
            if (fabs(px - ex) > maxe) maxe = fabs(px - ex);
            if (fabs(py - ey) > maxe) maxe = fabs(py - ey);
            n++;
        } else if (!strcmp(kind, "out")) {
            if (fscanf(f, "%lf %lf", &lon, &lat) != 2) break;
            CHECK(!map3d_project(lon, lat, &px, &py), "точка %.3f %.3f должна быть вне сетки", lon, lat);
        }
    }
    fclose(f);
    CHECK(n > 100 && maxe < 0.01, "билинейная проекция: %d точек, ошибка %.5f px", n, maxe);
    CHECK(!map3d_project(NAN, 52, &px, &py) && !map3d_city(-1, &px, &py) && !map3d_city(MAP3D_NCITY, &px, &py),
          "NaN и неверный город");
    printf("ok  map3d_project: %d точек, макс. ошибка %.5f px\n", n, maxe);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "/tmp/qoitest";
    test_premul();
    test_header_guards();
    test_files(dir);
    test_scale();
    test_blit();
    test_map(dir);
    printf("%s: %d проверок, %d ошибок\n", fails ? "ПРОВАЛ" : "УСПЕХ", checks, fails);
    return fails ? 1 : 0;
}
