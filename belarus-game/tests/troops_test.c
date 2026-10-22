/* Проверка атласа анимированных отрядов без Windows: в assets/troops.json есть вид для каждого войска
 * UNIT_TYPES и для врагов всех эпох, у каждого вида — кадр «стоит», 8 кадров шага и 6 выстрела,
 * все кадры лежат внутри листа assets/troops.png, а якорь — внутри клетки.
 *   gcc -std=c11 -O2 -Isrc tests/troops_test.c src/engine.c src/war.c src/front.c src/citynet.c src/data_*.c -lm -o t && ./t */
#include "game.h"
#include "war.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int errors, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("ОШИБКА: "); printf(__VA_ARGS__); printf("\n"); errors++; } } while (0)

static const char *ENEMY[] = { "enemy_early", "enemy_teutonic", "enemy_strelets", "enemy_18c", "enemy_napoleon",
                               "enemy_ww1", "enemy_wehrmacht", "enemy_panzer" };

static char js[1 << 16];

static int num_after(const char *from, const char *key, int *out)
{
    const char *p = strstr(from, key);
    if (!p) return 0;
    p += strlen(key);
    while (*p == '"' || *p == ':' || *p == ' ') p++;
    *out = atoi(p);
    return 1;
}

/* PNG: ширина и высота из заголовка IHDR */
static int png_size(const char *path, int *w, int *h)
{
    unsigned char b[24];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int n = (int)fread(b, 1, sizeof b, f);
    fclose(f);
    if (n < 24 || memcmp(b + 12, "IHDR", 4)) return 0;
    *w = b[16] << 24 | b[17] << 16 | b[18] << 8 | b[19];
    *h = b[20] << 24 | b[21] << 16 | b[22] << 8 | b[23];
    return 1;
}

/* вид id: "stand": s, "walk": [w0, wn], "fire": [f0, fn] */
static int variant(const char *id, int fr[5])
{
    char key[64];
    snprintf(key, sizeof key, "\"id\": \"%s\"", id);
    const char *p = strstr(js, key);
    if (!p) return 0;
    const char *e = strchr(p, '}');
    int ok = num_after(p, "\"stand\"", &fr[0]);
    const char *w = strstr(p, "\"walk\": ["), *f = strstr(p, "\"fire\": [");
    if (!ok || !w || !f || w > e || f > e) return 0;
    if (sscanf(w + 9, "%d, %d", &fr[1], &fr[2]) != 2 || sscanf(f + 9, "%d, %d", &fr[3], &fr[4]) != 2) return 0;
    return 1;
}

int main(void)
{
    data_init();
    FILE *fp = fopen("assets/troops.json", "rb");
    if (!fp) { printf("Отряды: нет assets/troops.json — пропуск\n"); return 0; }
    int n = (int)fread(js, 1, sizeof js - 1, fp);
    fclose(fp);
    js[n > 0 ? n : 0] = 0;
    int cw = 0, ch = 0, cols = 0, ax = -1, ay = -1, iw = 0, ih = 0;
    CHECK(num_after(js, "\"cell_w\"", &cw) && num_after(js, "\"cell_h\"", &ch) && num_after(js, "\"cols\"", &cols),
          "troops.json: нет размеров клетки");
    CHECK(num_after(js, "\"anchor_x\"", &ax) && num_after(js, "\"anchor_y\"", &ay), "troops.json: нет якоря");
    CHECK(ax > 0 && ax < cw && ay > 0 && ay < ch, "якорь (%d, %d) вне клетки %dx%d", ax, ay, cw, ch);
    CHECK(png_size("assets/troops.png", &iw, &ih), "нет assets/troops.png");
    CHECK(cols > 0 && cols * cw <= iw, "в ряду %d клеток по %d px, а лист шириной %d", cols, cw, iw);
    int total = 0;
    for (int i = 0; i < UNIT_COUNT + 8; i++) {
        const char *id = i < UNIT_COUNT ? UNIT_TYPES[i].id : ENEMY[i - UNIT_COUNT];
        int fr[5];
        if (!variant(id, fr)) { CHECK(0, "нет вида «%s» в troops.json", id); continue; }
        CHECK(fr[2] == 8 && fr[4] == 6, "«%s»: шаг %d кадров, выстрел %d (ожидалось 8 и 6)", id, fr[2], fr[4]);
        int last = fr[0];
        if (fr[1] + fr[2] - 1 > last) last = fr[1] + fr[2] - 1;
        if (fr[3] + fr[4] - 1 > last) last = fr[3] + fr[4] - 1;
        CHECK(cols > 0 && (last / cols + 1) * ch <= ih, "«%s»: кадр %d вне листа", id, last);
        total++;
    }
    printf("Отряды: %d видов, клетка %dx%d, лист %dx%d, проверок %d, ошибок %d\n", total, cw, ch, iw, ih, checks, errors);
    return errors ? 1 : 0;
}
