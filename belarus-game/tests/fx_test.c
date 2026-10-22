/* Проверка расписания заставок взлёта ракеты (fx.h) без Windows: космонавты есть среди деятелей,
 * взлёт показывается ровно один раз в свой ход, при загрузке старого сохранения не всплывает задним числом,
 * при найме после полёта — сразу, а бит заставки переживает сохранение и загрузку.
 *   gcc -std=c11 -O2 -Isrc tests/fx_test.c src/engine.c src/war.c src/front.c src/citynet.c src/data_*.c -lm -o t && ./t */
#include "game.h"
#include "fx.h"
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>

static int errors, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("ОШИБКА: "); printf(__VA_ARGS__); printf("\n"); errors++; } } while (0)

static int person(const char *id)
{
    for (int i = 0; i < PEOPLE_COUNT; i++) if (!strcmp(PEOPLE[i].id, id)) return i;
    return -1;
}

static int is_turn(int year)
{
    for (int i = 0; i < TURN_COUNT; i++) if (TURN_YEARS[i] == year) return 1;
    return 0;
}

/* данные: космонавты, годы, биты заставок */
static void test_schedule(void)
{
    unsigned seen = 0;
    for (int i = 0; i < FX_FLIGHT_COUNT; i++) {
        const FxFlight *f = &FX_FLIGHTS[i];
        int p = person(f->id);
        CHECK(p >= 0, "космонавт %s не найден среди деятелей", f->id);
        if (p < 0) continue;
        CHECK(fx_flight_of(p) == i, "fx_flight_of(%s) = %d", f->id, fx_flight_of(p));
        CHECK(is_turn(f->show), "%s: %d — не ход игры", f->id, f->show);
        CHECK(f->show >= f->year && f->show - f->year < FX_LATE_YEARS, "%s: ход показа %d для полёта %d", f->id, f->show, f->year);
        wchar_t y[8];
        swprintf(y, 8, L"%d", f->year);
        CHECK(wcsstr(PEOPLE[p].desc, y) != 0, "%s: в описании деятеля нет года полёта %d", f->id, f->year);
        if (f->cs) {
            CHECK(!(seen & f->cs), "%s: бит заставки повторяется", f->id);
            CHECK(f->cs < CS_RESERVED && f->cs != CS_METRO_MINSK, "%s: чужой бит заставки", f->id);
            seen |= f->cs;
        }
    }
    CHECK(seen == (CS_LAUNCH_KLIMUK | CS_LAUNCH_KOVALYONOK | CS_LAUNCH_VASILEVSKAYA), "не у всех взлётов есть заставка");
    CHECK(fx_flight_of(-1) < 0 && fx_flight_of(PEOPLE_COUNT) < 0 && fx_flight_of(person("alferov")) < 0, "не космонавт считается космонавтом");
}

static void resolve(Game *g)
{
    QItem q;
    while (game_pending(g, &q)) {
        if (q.type == Q_HIST || q.type == Q_RAND) {
            const Event *e = game_event_ptr(&q);
            int pick = 0;
            for (int c = e->nch - 1; c >= 0; c--) if (game_choice_ok(g, e, c)) { pick = c; break; }
            Outcome o;
            game_choose(g, pick, &o);
        } else game_ack(g);
    }
}

/* та же очередь, что fx_ui_idle: возвращает число показанных заставок, годы — в shown[] */
static int idle(Game *g, int *shown)
{
    int late = 0, i, n = 0;
    while ((i = fx_flight_due(g, &late)) >= 0) {
        g->cutscenes |= FX_FLIGHTS[i].cs;
        if (!late) { shown[i] = shown[i] ? -1 : g->year; n++; }
    }
    return n;
}

/* партия целиком: каждый взлёт — ровно один раз и в свой ход */
static void test_full_game(void)
{
    static Game g;
    int shown[FX_FLIGHT_COUNT] = { 0 };
    game_new(&g, 1, 20261021u);
    CHECK(g.cutscenes == 0, "новая партия: заставки уже показаны (%#x)", g.cutscenes);
    for (int guard = 0; guard < 200 && !g.over; guard++) {
        resolve(&g);
        idle(&g, shown);
        game_end_turn(&g);
    }
    resolve(&g);
    idle(&g, shown);
    for (int i = 0; i < FX_FLIGHT_COUNT; i++) {
        if (!FX_FLIGHTS[i].cs) { CHECK(shown[i] == 0, "%s: взлёт без заставки всплыл сам", FX_FLIGHTS[i].id); continue; }
        CHECK(shown[i] == FX_FLIGHTS[i].show, "%s: взлёт показан в %d, ждали %d", FX_FLIGHTS[i].id, shown[i], FX_FLIGHTS[i].show);
    }
}

/* старое сохранение 2000 года: прошлые взлёты не всплывают, взлёт 2024 года — в свой ход */
static void test_old_save(void)
{
    static Game g;
    int shown[FX_FLIGHT_COUNT] = { 0 };
    game_new(&g, 1, 7u);
    g.year = 2000;
    g.cutscenes = 0;
    CHECK(idle(&g, shown) == 0, "загрузка 2000 года: всплыл старый взлёт");
    CHECK((g.cutscenes & (CS_LAUNCH_KLIMUK | CS_LAUNCH_KOVALYONOK)) == (CS_LAUNCH_KLIMUK | CS_LAUNCH_KOVALYONOK),
          "загрузка 2000 года: прошлые взлёты не отмечены");
    CHECK(!(g.cutscenes & CS_LAUNCH_VASILEVSKAYA), "взлёт 2024 года отмечен раньше времени");
    g.year = 2024;
    CHECK(idle(&g, shown) == 1 && shown[2] == 2024, "2024: взлёт Василевской не показан");
    CHECK(idle(&g, shown) == 0, "2024: взлёт Василевской показан дважды");
    g.year = 1975;
    g.cutscenes = 0;
    CHECK(idle(&g, shown) == 1, "1975: взлёт Климука не показан");
}

/* найм: после полёта и без заставки — сразу; до полёта или после заставки — нет */
static void test_hire(void)
{
    static Game g;
    game_new(&g, 1, 9u);
    int kl = person("klimuk"), nv = person("novitsky"), va = person("vasilevskaya");
    g.cutscenes = 0;
    g.year = 1970;
    CHECK(fx_flight_on_hire(&g, kl) < 0, "найм Климука в 1970 году: полёта ещё не было");
    g.year = 1984;
    CHECK(fx_flight_on_hire(&g, kl) == 0, "найм Климука в 1984 году без заставки: взлёт не открыт");
    g.cutscenes = CS_LAUNCH_KLIMUK;
    CHECK(fx_flight_on_hire(&g, kl) < 0, "найм Климука: заставка уже была");
    g.year = 2024;
    CHECK(fx_flight_on_hire(&g, nv) < 0, "Новицкий: взлёт только по кнопке");
    CHECK(fx_flight_on_hire(&g, va) == 2, "найм Василевской в 2024 году без заставки: взлёт не открыт");
    CHECK(fx_flight_on_hire(&g, person("alferov")) < 0, "не космонавт открыл взлёт");
}

/* бит заставки сохраняется */
static void test_save(void)
{
    static Game g, h;
    game_new(&g, 1, 11u);
    g.cutscenes = CS_LAUNCH_KLIMUK | CS_LAUNCH_VASILEVSKAYA;
    FILE *f = tmpfile();
    CHECK(f != 0, "нет временного файла");
    if (!f) return;
    CHECK(game_save(&g, f), "сохранение не записано");
    rewind(f);
    CHECK(game_load(&h, f), "сохранение не прочитано");
    fclose(f);
    CHECK(h.cutscenes == g.cutscenes, "биты заставок после загрузки: %#x вместо %#x", h.cutscenes, g.cutscenes);
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    data_init();
    test_schedule();
    test_full_game();
    test_old_save();
    test_hire();
    test_save();
    printf("Взлёты ракет: %d полёта; проверок %d, ошибок %d\n", FX_FLIGHT_COUNT, checks, errors);
    return errors ? 1 : 0;
}
