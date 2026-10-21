/* Проверка фигурок войск без Windows: порядок UNIT_TYPES (на него ссылаются сохранения) совпадает
 * с листом assets/units.json, сетка листа не изменилась, названия и описания помещаются в карточку
 * и подсказку, кавычки — «ёлочки», а у войск XX века в описании есть героини.
 *   gcc -std=c11 -O2 -Isrc tests/figurines_test.c src/engine.c src/war.c src/front.c src/citynet.c src/data_*.c -lm -o t && ./t */
#include "game.h"
#include "war.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

static int errors, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("ОШИБКА: "); printf(__VA_ARGS__); printf("\n"); errors++; } } while (0)

/* порядок войск из сохранений версий 1–6: менять нельзя, новые — только в конец */
static const char *IDS[] = { "druzhina", "crossbow", "khorugv", "gunners", "musketeers", "hussars", "rockets",
                             "line", "artillery", "rifle", "air", "partisans", "motor", "tank", "missile", "airdef" };
#define NIDS ((int)(sizeof IDS / sizeof IDS[0]))

static int read_file(const char *path, char *buf, int cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    int n = (int)fread(buf, 1, (size_t)cap - 1, f);
    fclose(f);
    buf[n < 0 ? 0 : n] = 0;
    return n;
}

static void test_order(void)
{
    CHECK(UNIT_COUNT >= NIDS, "войск %d, ожидалось не меньше %d", UNIT_COUNT, NIDS);
    for (int u = 0; u < NIDS && u < UNIT_COUNT; u++)
        CHECK(!strcmp(UNIT_TYPES[u].id, IDS[u]), "войско %d: «%s» вместо «%s»", u, UNIT_TYPES[u].id, IDS[u]);
}

/* лист фигурок: {"cell_w": 160, "cell_h": 160, "cols": 4, "ids": [...]} и PNG cols*160 x rows*160 */
static void test_sheet(void)
{
    static char js[8192];
    if (read_file("assets/units.json", js, sizeof js) <= 0) { CHECK(0, "нет assets/units.json"); return; }
    CHECK(strstr(js, "\"cell_w\": 160") && strstr(js, "\"cell_h\": 160") && strstr(js, "\"cols\": 4"),
          "сетка листа фигурок изменилась: %s", js);
    const char *p = strstr(js, "\"ids\""), *q = p ? strchr(p, '[') : NULL, *end = q ? strchr(q, ']') : NULL;
    CHECK(q && end, "в units.json нет списка ids");
    int n = 0;
    for (p = q; end && (p = strchr(p + 1, '"')) != NULL && p < end; ) {
        const char *e = strchr(p + 1, '"');
        if (!e) break;
        int len = (int)(e - p - 1);
        if (n < UNIT_COUNT)
            CHECK((int)strlen(UNIT_TYPES[n].id) == len && !strncmp(p + 1, UNIT_TYPES[n].id, (size_t)len),
                  "units.json: фигурка %d «%.*s», а войско — «%s»", n, len, p + 1, UNIT_TYPES[n].id);
        n++;
        p = e;
    }
    CHECK(n == UNIT_COUNT, "в units.json %d фигурок, войск %d", n, UNIT_COUNT);
    unsigned char h[24];
    FILE *f = fopen("assets/units.png", "rb");
    CHECK(f != NULL, "нет assets/units.png");
    if (!f) return;
    int ok = fread(h, 1, 24, f) == 24;
    fclose(f);
    CHECK(ok && !memcmp(h + 1, "PNG", 3), "units.png — не PNG");
    if (!ok) return;
    int w = (h[16] << 24) | (h[17] << 16) | (h[18] << 8) | h[19], hh = (h[20] << 24) | (h[21] << 16) | (h[22] << 8) | h[23];
    CHECK(w == 4 * 160 && hh == (UNIT_COUNT + 3) / 4 * 160, "units.png %dx%d вместо %dx%d", w, hh, 640,
          (UNIT_COUNT + 3) / 4 * 160);
}

static int has(const wchar_t *s, const wchar_t *const *words)
{
    for (; *words; words++) if (wcsstr(s, *words)) return 1;
    return 0;
}

static void test_texts(void)
{
    static const wchar_t *const HEROINES[] = { L"женщин", L"девуш", L"связистк", L"«ночные ведьмы»",
                                               L"Туснолобова", L"Октябрьская", NULL };
    static const char *const XX[] = { "rifle", "air", "partisans", "motor", "tank", "airdef", NULL };
    for (int u = 0; u < UNIT_COUNT; u++) {
        const UnitType *t = &UNIT_TYPES[u];
        int nl = (int)wcslen(t->name), dl = (int)wcslen(t->desc);
        CHECK(nl > 0 && nl <= 24, "%s: название длиной %d", t->id, nl);
        CHECK(dl >= 20 && nl + 3 + dl < 259, "%s: описание длиной %d не влезет в подсказку", t->id, dl);
        CHECK(t->desc[dl - 1] == L'.', "%s: описание без точки в конце", t->id);
        CHECK(!wcschr(t->name, L'"') && !wcschr(t->desc, L'"'), "%s: прямые кавычки вместо «ёлочек»", t->id);
        CHECK(!wcsstr(t->desc, L"  "), "%s: двойной пробел в описании", t->id);
        int open = 0;
        for (const wchar_t *c = t->desc; *c; c++) open += *c == L'«' ? 1 : *c == L'»' ? -1 : 0;
        CHECK(open == 0, "%s: непарные кавычки", t->id);
        for (int k = 0; XX[k]; k++)
            if (!strcmp(t->id, XX[k])) CHECK(has(t->desc, HEROINES), "%s: в описании нет героинь", t->id);
    }
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    data_init();
    test_order();
    test_sheet();
    test_texts();
    printf("Фигурки: %d войск, проверок %d, ошибок %d\n", UNIT_COUNT, checks, errors);
    return errors ? 1 : 0;
}
