/* fx.h — расписание космических полётов для заставок взлёта ракеты (ui_fx.c).
 * Без windows.h: те же правила проверяет tests/fx_test.c на Linux. */
#ifndef BY_FX_H
#define BY_FX_H

#include "game.h"
#include <string.h>

typedef struct {
    const char *id;     /* PEOPLE[].id */
    unsigned    cs;     /* бит Game.cutscenes; 0 — взлёт только по кнопке в окне деятеля */
    int         year;   /* год показанного полёта */
    int         show;   /* ход (TURN_YEARS), в начале которого взлёт показывается сам */
} FxFlight;

/* Климук — «Союз-13» (1973), Ковалёнок — рекордный «Союз-29» (1978), Василевская и командир
 * её корабля Новицкий — «Союз МС-25» (2024). Взлёт показывается всем игрокам, приглашён деятель или нет:
 * это урок истории, а не награда. */
static const FxFlight FX_FLIGHTS[] = {
    { "klimuk",       CS_LAUNCH_KLIMUK,       1973, 1975 },
    { "kovalyonok",   CS_LAUNCH_KOVALYONOK,   1978, 1980 },
    { "vasilevskaya", CS_LAUNCH_VASILEVSKAYA, 2024, 2024 },
    { "novitsky",     0,                      2024, 2024 },
};
#define FX_FLIGHT_COUNT ((int)(sizeof FX_FLIGHTS / sizeof FX_FLIGHTS[0]))
#define FX_LATE_YEARS 5     /* опоздали больше чем на ход (загрузка старого сохранения) — взлёт не показываем */

/* полёт деятеля PEOPLE[person]; -1 — не космонавт */
static inline int fx_flight_of(int person)
{
    if (person < 0 || person >= PEOPLE_COUNT) return -1;
    for (int i = 0; i < FX_FLIGHT_COUNT; i++)
        if (!strcmp(FX_FLIGHTS[i].id, PEOPLE[person].id)) return i;
    return -1;
}

/* Какой взлёт пора показать в начале хода: индекс FX_FLIGHTS или -1. *late = 1 — ход давно прошёл:
 * бит заставки ставится, но сама она не открывается. */
static inline int fx_flight_due(const Game *g, int *late)
{
    for (int i = 0; i < FX_FLIGHT_COUNT; i++) {
        const FxFlight *f = &FX_FLIGHTS[i];
        if (!f->cs || (g->cutscenes & f->cs) || g->year < f->show) continue;
        *late = g->year >= f->show + FX_LATE_YEARS;
        return i;
    }
    return -1;
}

/* Деятеля только что пригласили: показать его взлёт сразу (полёт уже был, а заставки ещё не было)? */
static inline int fx_flight_on_hire(const Game *g, int person)
{
    int i = fx_flight_of(person);
    if (i < 0 || !FX_FLIGHTS[i].cs || (g->cutscenes & FX_FLIGHTS[i].cs) || g->year < FX_FLIGHTS[i].year) return -1;
    return i;
}

#endif
