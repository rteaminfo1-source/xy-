/* Проверка данных и баланса: сборка на Linux, запуск без графики.
 * gcc -O2 -I../src sim.c ../src/engine.c ../src/data_*.c -lm -o sim && ./sim */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <stdlib.h>

static int errors;
#define FAIL(...) do { printf("ОШИБКА: "); printf(__VA_ARGS__); printf("\n"); errors++; } while (0)

static void check_data(void)
{
    data_init();
    for (int t = 0; t < TECH_COUNT; t++) {
        for (int u = t + 1; u < TECH_COUNT; u++)
            if (!strcmp(TECHS[t].id, TECHS[u].id)) FAIL("дубль технологии %s", TECHS[t].id);
        for (int k = 0; k < 3; k++) {
            if (TECHS[t].req[k] && TECHS[t].reqi[k] < 0) FAIL("нет технологии %s (нужна для %s)", TECHS[t].req[k], TECHS[t].id);
            if (TECHS[t].reqi[k] >= 0 && TECHS[TECHS[t].reqi[k]].era > TECHS[t].era)
                FAIL("%s требует более позднюю %s", TECHS[t].id, TECHS[t].req[k]);
        }
    }
    for (int p = 0; p < PROJECT_COUNT; p++) {
        if (PROJECTS[p].techi < 0) FAIL("стройка %s: нет технологии %s", PROJECTS[p].id, PROJECTS[p].tech);
        else if (TECHS[PROJECTS[p].techi].era > PROJECTS[p].era) FAIL("стройка %s раньше своей технологии", PROJECTS[p].id);
        if (PROJECTS[p].city < 0 || PROJECTS[p].city >= CITY_COUNT) FAIL("стройка %s: город", PROJECTS[p].id);
    }
    for (int i = 0; i < PEOPLE_COUNT; i++)
        if (PEOPLE[i].discount && PEOPLE[i].disci < 0) FAIL("деятель %s: нет технологии %s", PEOPLE[i].id, PEOPLE[i].discount);
    for (int i = 0; i < HIST_COUNT; i++) {
        const Event *e = &HIST_EVENTS[i];
        if (e->nch < 1) FAIL("событие %s без вариантов", e->id);
        if (e->year < TURN_YEARS[0] || e->year >= END_YEAR) FAIL("событие %s вне игры", e->id);
        for (int c = 0; c < e->nch; c++) {
            if (e->ch[c].reqTech && e->ch[c].reqi < 0) FAIL("событие %s: нет технологии %s", e->id, e->ch[c].reqTech);
            if (e->ch[c].grant && e->ch[c].granti < 0) FAIL("событие %s: нет технологии %s", e->id, e->ch[c].grant);
        }
        /* хотя бы один вариант без условий */
        int free = 0;
        for (int c = 0; c < e->nch; c++) free |= e->ch[c].reqTech == 0;
        if (!free) FAIL("событие %s: все варианты с условиями", e->id);
    }
    for (int i = 0; i < RAND_COUNT; i++) {
        const Event *e = &RAND_EVENTS[i];
        int free = 0;
        for (int c = 0; c < e->nch; c++) {
            if (e->ch[c].reqTech && e->ch[c].reqi < 0) FAIL("случайное %s: нет технологии %s", e->id, e->ch[c].reqTech);
            free |= e->ch[c].reqTech == 0;
        }
        if (!free) FAIL("случайное %s: все варианты с условиями", e->id);
    }
    int perEra[ERA_COUNT + 1] = {0}, techEra[ERA_COUNT + 1] = {0};
    for (int q = 0; q < QUIZ_COUNT; q++) perEra[QUIZ[q].era]++;
    for (int t = 0; t < TECH_COUNT; t++) techEra[TECHS[t].era]++;
    for (int i = 1; i < TURN_COUNT; i++) if (TURN_YEARS[i] <= TURN_YEARS[i - 1]) FAIL("годы ходов не по порядку");
    printf("Данные: %d технологий, %d строек, %d деятелей, %d исторических и %d случайных событий, %d вопросов, %d ходов\n",
           TECH_COUNT, PROJECT_COUNT, PEOPLE_COUNT, HIST_COUNT, RAND_COUNT, QUIZ_COUNT, TURN_COUNT);
    printf("Технологий по эпохам:");
    for (int e = 1; e <= ERA_COUNT; e++) printf(" %d", techEra[e]);
    printf("\nВопросов по эпохам:");
    for (int e = 1; e <= ERA_COUNT; e++) printf(" %d", perEra[e]);
    printf("\n");
}

/* Жадный бот: сначала самые дешёвые технологии, затем стройки, затем деятели. */
static void bot_turn(Game *g, int quizSkill)
{
    for (int guard = 0; guard < 200; guard++) {
        int best = -1, bc = 1 << 30;
        for (int t = 0; t < TECH_COUNT; t++)
            if (game_tech_state(g, t) == TS_AVAILABLE) {
                int c = game_tech_cost(g, t);
                if (c <= g->sci && c < bc) { bc = c; best = t; }
            }
        if (best < 0) break;
        game_research(g, best);
    }
    for (int p = 0; p < PROJECT_COUNT; p++)
        if (game_proj_state(g, p) == PS_AVAILABLE && g->gold >= game_proj_gold(g, p) + 10 && g->ind >= game_proj_ind(g, p))
            game_build(g, p);
    for (int i = 0; i < PEOPLE_COUNT; i++)
        if (game_person_state(g, i) == HS_AVAILABLE && g->gold >= game_person_cost(g, i) * 1.5)
            game_hire(g, i);
    int q = game_quiz_pick(g);
    if (q >= 0) {
        int ans = 0;
        for (int i = 0; i < 4; i++) if (g->quizPerm[i] == QUIZ[q].correct) ans = i;
        if ((int)(g->rng % 100) >= quizSkill) ans = (ans + 1) % 4;
        game_quiz_answer(g, ans);
    }
}

static void resolve_events(Game *g)
{
    QItem q;
    while (game_pending(g, &q)) {
        if (q.type == Q_HIST || q.type == Q_RAND) {
            const Event *e = game_event_ptr(&q);
            int pick = -1;
            for (int c = e->nch - 1; c >= 0; c--)          /* предпочитаем «умные» варианты с технологией */
                if (e->ch[c].reqTech && game_choice_ok(g, e, c)) { pick = c; break; }
            if (pick < 0) for (int c = 0; c < e->nch; c++) if (game_choice_ok(g, e, c)) { pick = c; break; }
            if (pick < 0) { printf("ОШИБКА: событие %s без доступных вариантов (год %d)\n", e->id, g->year); errors++; game_ack(g); continue; }
            Outcome o;
            game_choose(g, pick, &o);
        } else game_ack(g);
    }
}

int main(int argc, char **argv)
{
    setlocale(LC_ALL, "C.UTF-8");
    check_data();
    int verbose = argc > 1 && !strcmp(argv[1], "-v");
    for (int d = 0; d < DIFFICULTY_COUNT; d++) {
        double sumScore = 0, sumTech = 0, sumPop = 0;
        int runs = 20;
        for (int s = 1; s <= runs; s++) {
            static Game g;
            game_new(&g, d, (unsigned)(s * 7919));
            int lastEra = 1;
            while (!g.over) {
                resolve_events(&g);
                if (g.over) break;
                bot_turn(&g, 70);
                if (verbose && s == 1 && d == 1 && g.era != lastEra) lastEra = g.era;
                if (verbose && s == 1 && d == 1) {
                    Income inc; game_income(&g, &inc);
                    printf("%4d э%d поп %6.0f  каз %6.0f(+%4.0f) знан %5.0f(+%4.0f) пр %5.0f(+%4.0f) обор %3.0f стаб %3.0f эко %3.0f культ %5.0f тех %3d стр %2d люд %2d\n",
                           g.year, g.era, g.pop, g.gold, inc.total[R_GOLD], g.sci, inc.total[R_SCI], g.ind, inc.total[R_IND],
                           inc.total[R_DEF], g.stab, g.eco, g.cult, game_tech_count(&g), game_proj_count(&g), game_people_count(&g));
                }
                game_end_turn(&g);
            }
            resolve_events(&g);
            Score sc;
            game_score(&g, &sc);
            sumScore += sc.total; sumTech += game_tech_count(&g); sumPop += g.pop;
            if (s == 1) {
                int ach = 0; for (int a = 0; a < ACH_COUNT; a++) ach += g.ach[a];
                printf("[%ls] техн %d/%d, стройки %d/%d, деятели %d, население %.0f тыс., эко %.0f, стаб %.0f, культ %.0f, достиж %d, счёт %d — %ls\n",
                       DIFFICULTIES[d].name, game_tech_count(&g), TECH_COUNT, game_proj_count(&g), PROJECT_COUNT,
                       game_people_count(&g), g.pop, g.eco, g.stab, g.cult, ach, sc.total, sc.rank);
                if (d == 1 && verbose) printf("%ls\n", sc.epilogue);
            }
        }
        printf("  среднее за %d партий: счёт %.0f, технологий %.1f, население %.0f тыс.\n", runs, sumScore / runs, sumTech / runs, sumPop / runs);
    }
    /* минутный доход: растёт с эпохой, не начисляется после финала */
    {
        static Game m;
        MinuteBonus b1, b8;
        game_new(&m, 1, 3);
        double g0 = m.gold, s0 = m.sci, i0 = m.ind;
        if (!game_minute_bonus(&m, &b1) || b1.gold <= 0 || b1.sci <= 0 || b1.ind <= 0) FAIL("минутный доход в эпоху I");
        if (m.gold != g0 + b1.gold || m.sci != s0 + b1.sci || m.ind != i0 + b1.ind) FAIL("минутный доход не зачислен");
        m.era = 8;
        game_minute_preview(&m, &b8);
        MinuteBonus shown8 = b8;
        if (b8.gold <= b1.gold || b8.sci <= b1.sci || b8.ind <= b1.ind) FAIL("минутный доход не растёт с эпохой");
        m.over = 1;
        double g1 = m.gold;
        if (game_minute_bonus(&m, &b8) || m.gold != g1) FAIL("минутный доход после финала");
        printf("Минутный доход: эпоха I +%d/+%d/+%d, эпоха VIII +%d/+%d/+%d (казна/знания/производство)\n",
               b1.gold, b1.sci, b1.ind, shown8.gold, shown8.sci, shown8.ind);
    }
    /* сохранение и загрузка */
    {
        static Game a, b;
        game_new(&a, 1, 42);
        FILE *f = tmpfile();
        if (!f || !game_save(&a, f)) FAIL("сохранение");
        else { rewind(f); if (!game_load(&b, f) || memcmp(&a, &b, sizeof a)) FAIL("загрузка"); fclose(f); }
    }
    printf(errors ? "Найдено ошибок: %d\n" : "Ошибок нет\n", errors);
    return errors ? 1 : 0;
}
