/* Проверка данных и баланса: сборка на Linux, запуск без графики.
 * gcc -O2 -I../src sim.c ../src/engine.c ../src/data_*.c -lm -o sim && ./sim */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <stdlib.h>
#include <stddef.h>

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

/* Задания: цели существуют, сроки внутри игры и достижимы после открытия задания. */
static void check_quests(void)
{
    int perEra[ERA_COUNT + 1] = {0}, perType[QT_COUNT] = {0};
    if (QUEST_COUNT > MAX_QUESTS) FAIL("заданий %d больше MAX_QUESTS %d", QUEST_COUNT, MAX_QUESTS);
    for (int q = 0; q < QUEST_COUNT; q++) {
        const Quest *Q = &QUESTS[q];
        for (int u = q + 1; u < QUEST_COUNT; u++)
            if (!strcmp(Q->id, QUESTS[u].id)) FAIL("дубль задания %s", Q->id);
        if (!Q->title || !Q->desc || !Q->title[0] || !Q->desc[0]) FAIL("задание %s без текста", Q->id);
        if (Q->type < 0 || Q->type >= QT_COUNT) { FAIL("задание %s: тип", Q->id); continue; }
        if (Q->era < 1 || Q->era > ERA_COUNT) { FAIL("задание %s: эпоха", Q->id); continue; }
        perEra[Q->era]++; perType[Q->type]++;
        if (Q->deadline <= TURN_YEARS[0] || Q->deadline > END_YEAR) FAIL("задание %s: срок %d вне игры", Q->id, Q->deadline);
        int dEra = era_of_year(Q->deadline);
        if (Q->era > dEra) FAIL("задание %s: эпоха %d позже срока %d", Q->id, Q->era, Q->deadline);
        if (Q->deadline < ERAS[Q->era - 1].from) FAIL("задание %s: срок раньше открытия", Q->id);
        if (Q->city < -1 || Q->city >= CITY_COUNT) FAIL("задание %s: город", Q->id);
        if (Q->gold < 0 || Q->sci < 0 || Q->cult < 0 || Q->stab < 0 || Q->gold + Q->sci + Q->cult + Q->stab == 0)
            FAIL("задание %s: награда", Q->id);
        int isObj = Q->type == QT_TECH || Q->type == QT_PROJ || Q->type == QT_PERSON;
        if (isObj) {
            if (Q->ti < 0) { FAIL("задание %s: нет цели %s", Q->id, Q->target ? Q->target : "(null)"); continue; }
            if (Q->type == QT_TECH && TECHS[Q->ti].era > dEra) FAIL("задание %s: технология позже срока", Q->id);
            if (Q->type == QT_PROJ) {
                const Project *P = &PROJECTS[Q->ti];
                if (P->era > dEra) FAIL("задание %s: стройка позже срока", Q->id);
                if (P->until && P->until < ERAS[Q->era - 1].from) FAIL("задание %s: стройка упущена до открытия", Q->id);
            }
            if (Q->type == QT_PERSON) {
                const Person *P = &PEOPLE[Q->ti];
                if (P->to < ERAS[Q->era - 1].from || P->from > Q->deadline) FAIL("задание %s: деятель вне срока", Q->id);
            }
        } else if (Q->num <= 0) FAIL("задание %s: порог", Q->id);
    }
    printf("Задания: %d; по эпохам:", QUEST_COUNT);
    for (int e = 1; e <= ERA_COUNT; e++) printf(" %d", perEra[e]);
    printf("; по типам:");
    for (int t = 0; t < QT_COUNT; t++) printf(" %d", perType[t]);
    printf("\n");
}

/* Механика заданий: открытие, награда, провал, проверка «на срок», счёт. */
static int find_quest(const char *id)
{
    for (int q = 0; q < QUEST_COUNT; q++) if (!strcmp(QUESTS[q].id, id)) return q;
    return -1;
}

static void test_quest_rules(void)
{
    static Game g;
    game_new(&g, 1, 11);
    for (int q = 0; q < QUEST_COUNT; q++) {
        int want = QUESTS[q].era == 1 ? QS_ACTIVE : QS_HIDDEN;
        if (game_quest_state(&g, q) != want) FAIL("задание %s: состояние в начале %d", QUESTS[q].id, game_quest_state(&g, q));
    }
    int qs = find_quest("q_scriptorium"), qt = find_quest("q_turov_def"), qe = find_quest("q_eco1986"), qp = find_quest("q_pop_end");
    if (qs < 0 || qt < 0 || qe < 0 || qp < 0) { FAIL("нет опорных заданий для теста"); return; }
    double cur, tgt;
    game_quest_progress(&g, qs, &cur, &tgt);
    if (cur != 0 || tgt != 1) FAIL("прогресс задания-технологии");
    double gold0 = g.gold, sci0 = g.sci, cult0 = g.cult;
    g.tech[QUESTS[qs].ti] = 1;
    game_check_quests(&g);
    if (game_quest_state(&g, qs) != QS_DONE || g.questsDone != 1) FAIL("задание не засчитано");
    if (g.gold != gold0 + QUESTS[qs].gold || g.sci != sci0 + QUESTS[qs].sci || g.cult != cult0 + QUESTS[qs].cult) FAIL("награда задания");
    if (g.logs[g.logN - 1].kind != LOG_QUEST) FAIL("нет записи о задании в летописи");
    game_check_quests(&g);
    if (g.questsDone != 1) FAIL("награда выдана дважды");
    /* срок прошёл — провал */
    g.year = QUESTS[qt].deadline + 1;
    game_check_quests(&g);
    if (game_quest_state(&g, qt) != QS_FAILED) FAIL("просроченное задание не провалено");
    /* экология «по итогам года»: в сам год ещё не решено */
    g.era = 7; g.year = 1986; g.eco = 40;
    game_check_quests(&g);
    if (game_quest_state(&g, qe) != QS_ACTIVE) FAIL("задание на экологию решено раньше срока");
    g.year = 1991; g.eco = 55;
    game_check_quests(&g);
    if (game_quest_state(&g, qe) != QS_DONE) FAIL("задание на экологию не засчитано по итогам года");
    /* население — только в конце летописи */
    g.era = 8; g.year = 2000; g.pop = 9999;
    game_check_quests(&g);
    if (game_quest_state(&g, qp) != QS_ACTIVE) FAIL("задание на население засчитано до конца");
    g.year = END_YEAR; g.over = 1; g.pop = 8500;
    game_check_quests(&g);
    if (game_quest_state(&g, qp) != QS_FAILED) FAIL("задание на население в конце");
    for (int q = 0; q < QUEST_COUNT; q++)
        if (game_quest_state(&g, q) == QS_ACTIVE) FAIL("после финала осталось активное задание %s", QUESTS[q].id);
    Score sc;
    game_score(&g, &sc);
    if (sc.quests != 15 * game_quest_count(&g, QS_DONE)) FAIL("очки за задания");
    wchar_t b1[160], b2[160];
    for (int q = 0; q < QUEST_COUNT; q++) {
        quest_describe_goal(q, b1, 160);
        quest_describe_reward(q, b2, 160);
        if (!b1[0] || !b2[0]) FAIL("задание %s: пустое описание", QUESTS[q].id);
    }
    quest_describe_goal(qe, b1, 160);
    quest_describe_reward(qe, b2, 160);
    printf("Пример задания: %ls — %ls\n", b1, b2);
}

/* Бот «по заданиям»: сначала цели открытых заданий (и путь к ним), на них копит знания и казну. */
static int questBot;

static int tech_path_next(const Game *g, int t)
{
    if (t < 0 || g->tech[t]) return -1;
    if (game_tech_state(g, t) == TS_AVAILABLE) return t;
    for (int k = 0; k < 3; k++) {
        int x = tech_path_next(g, TECHS[t].reqi[k]);
        if (x >= 0) return x;
    }
    return -1;
}

static void quest_focus(Game *g, int *saveSci, int *saveGold)
{
    /* курс под численные цели: оборона, экология, стабильность */
    g->priority = 0;
    for (int q = 0; q < QUEST_COUNT; q++) {
        if (game_quest_state(g, q) != QS_ACTIVE) continue;
        const Quest *Q = &QUESTS[q];
        double cur, tgt;
        game_quest_progress(g, q, &cur, &tgt);
        if (Q->type == QT_DEF && cur < tgt) { g->priority = 4; break; }
        if (Q->type == QT_ECO_AT && Q->deadline - g->year <= 45 && cur < tgt + 25) g->priority = 6;
        if (Q->type == QT_STAB && cur < tgt && !g->priority) g->priority = 5;
    }
    for (int q = 0; q < QUEST_COUNT; q++) {
        if (game_quest_state(g, q) != QS_ACTIVE) continue;
        const Quest *Q = &QUESTS[q];
        int t = -1;
        if (Q->type == QT_TECH) t = Q->ti;
        else if (Q->type == QT_PROJ && game_proj_state(g, Q->ti) == PS_LOCKED) t = PROJECTS[Q->ti].techi;
        for (int guard = 0; t >= 0 && guard < 20; guard++) {
            int x = tech_path_next(g, t);
            if (x < 0) break;
            if (g->sci < game_tech_cost(g, x)) { *saveSci = 1; break; }
            game_research(g, x);
        }
        if (Q->type == QT_PROJ && game_proj_state(g, Q->ti) == PS_AVAILABLE) {
            if (g->gold >= game_proj_gold(g, Q->ti) && g->ind >= game_proj_ind(g, Q->ti)) game_build(g, Q->ti);
            else {
                *saveGold = 1;
                if (g->ind < game_proj_ind(g, Q->ti) && !g->priority) g->priority = 3;   /* не хватает производства */
            }
        }
        if (Q->type == QT_PERSON && game_person_state(g, Q->ti) == HS_AVAILABLE) {
            if (g->gold >= game_person_cost(g, Q->ti)) game_hire(g, Q->ti);
            else *saveGold = 1;
        }
    }
}

/* Жадный бот: сначала самые дешёвые технологии, затем стройки, затем деятели. */
static void bot_turn(Game *g, int quizSkill)
{
    int saveSci = 0, saveGold = 0;
    if (questBot) quest_focus(g, &saveSci, &saveGold);
    for (int guard = 0; guard < 200 && !saveSci; guard++) {
        int best = -1, bc = 1 << 30;
        for (int t = 0; t < TECH_COUNT; t++)
            if (game_tech_state(g, t) == TS_AVAILABLE) {
                int c = game_tech_cost(g, t);
                if (c <= g->sci && c < bc) { bc = c; best = t; }
            }
        if (best < 0) break;
        game_research(g, best);
    }
    for (int p = 0; p < PROJECT_COUNT && !saveGold; p++)
        if (game_proj_state(g, p) == PS_AVAILABLE && g->gold >= game_proj_gold(g, p) + 10 && g->ind >= game_proj_ind(g, p))
            game_build(g, p);
    for (int i = 0; i < PEOPLE_COUNT && !saveGold; i++)
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
    check_quests();
    test_quest_rules();
    int verbose = argc > 1 && !strcmp(argv[1], "-v");
    for (int d = 0; d < DIFFICULTY_COUNT; d++) {
        double sumScore = 0, sumTech = 0, sumPop = 0, sumQuest = 0;
        static int questDone[MAX_QUESTS];
        memset(questDone, 0, sizeof questDone);
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
            sumQuest += game_quest_count(&g, QS_DONE);
            for (int q = 0; q < QUEST_COUNT; q++) questDone[q] += game_quest_state(&g, q) == QS_DONE;
            if (game_quest_count(&g, QS_DONE) + game_quest_count(&g, QS_FAILED) != QUEST_COUNT) FAIL("после финала не все задания решены");
            if (s == 1) {
                int ach = 0; for (int a = 0; a < ACH_COUNT; a++) ach += g.ach[a];
                printf("[%ls] техн %d/%d, стройки %d/%d, деятели %d, население %.0f тыс., эко %.0f, стаб %.0f, культ %.0f, достиж %d, счёт %d — %ls\n",
                       DIFFICULTIES[d].name, game_tech_count(&g), TECH_COUNT, game_proj_count(&g), PROJECT_COUNT,
                       game_people_count(&g), g.pop, g.eco, g.stab, g.cult, ach, sc.total, sc.rank);
                if (d == 1 && verbose) printf("%ls\n", sc.epilogue);
            }
        }
        printf("  среднее за %d партий: счёт %.0f, технологий %.1f, население %.0f тыс.\n", runs, sumScore / runs, sumTech / runs, sumPop / runs);
        printf("  задания (бот о них не знает): выполнено %.1f из %d — %.0f%%\n", sumQuest / runs, QUEST_COUNT, 100.0 * sumQuest / runs / QUEST_COUNT);
        if (verbose) {
            printf("  по заданиям, %% партий:");
            for (int q = 0; q < QUEST_COUNT; q++) printf(" %s=%d", QUESTS[q].id + 2, questDone[q] * 100 / runs);
            printf("\n");
        }
    }
    /* тот же бот, но идущий к целям заданий: проверка, что задания достижимы */
    questBot = 1;
    for (int d = 0; d < DIFFICULTY_COUNT; d++) {
        double sumScore = 0, sumQuest = 0, sumPop = 0;
        static int questDone[MAX_QUESTS];
        memset(questDone, 0, sizeof questDone);
        int runs = 20;
        for (int s = 1; s <= runs; s++) {
            static Game g;
            game_new(&g, d, (unsigned)(s * 7919));
            while (!g.over) {
                resolve_events(&g);
                if (g.over) break;
                bot_turn(&g, 70);
                game_end_turn(&g);
            }
            resolve_events(&g);
            Score sc;
            game_score(&g, &sc);
            sumScore += sc.total; sumPop += g.pop;
            sumQuest += game_quest_count(&g, QS_DONE);
            for (int q = 0; q < QUEST_COUNT; q++) questDone[q] += game_quest_state(&g, q) == QS_DONE;
        }
        printf("[%ls] бот по заданиям: выполнено %.1f из %d — %.0f%%, счёт %.0f, население %.0f тыс.\n", DIFFICULTIES[d].name,
               sumQuest / runs, QUEST_COUNT, 100.0 * sumQuest / runs / QUEST_COUNT, sumScore / runs, sumPop / runs);
        if (verbose) {
            printf("  по заданиям, %% партий:");
            for (int q = 0; q < QUEST_COUNT; q++) printf(" %s=%d", QUESTS[q].id + 2, questDone[q] * 100 / runs);
            printf("\n");
        }
    }
    questBot = 0;
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
    /* совместимость: сохранение версии 3 (без заданий) загружается, задания начинаются с нуля */
    {
        static Game a, b;
        game_new(&a, 1, 77);
        for (int t = 0; t < 6; t++) { resolve_events(&a); bot_turn(&a, 70); game_end_turn(&a); }
        if (game_quest_count(&a, QS_HIDDEN) == QUEST_COUNT) FAIL("задания не открылись");
        size_t off = offsetof(Game, questState);
        unsigned v3 = (unsigned)((off + _Alignof(Game) - 1) / _Alignof(Game) * _Alignof(Game));
        static unsigned char blob[sizeof(Game)];
        memcpy(blob, &a, sizeof a);
        ((Game *)blob)->version = 3;
        memset(blob + off, 0xAB, v3 - off);          /* хвостовое выравнивание v3 — мусор */
        FILE *f = tmpfile();
        if (!f) FAIL("tmpfile");
        else {
            fwrite("BYTG", 1, 4, f);
            fwrite(&v3, sizeof v3, 1, f);
            fwrite(blob, 1, v3, f);
            rewind(f);
            memset(&b, 0x5A, sizeof b);
            if (!game_load(&b, f)) FAIL("сохранение версии 3 не загрузилось");
            else {
                if (b.version != GAME_VERSION) FAIL("версия после загрузки v3");
                if (memcmp(&a, &b, off)) FAIL("данные v3 искажены");   /* a.version == 4 == b.version после загрузки */
                for (int q = 0; q < MAX_QUESTS; q++) if (b.questState[q]) { FAIL("задания v3 не обнулены"); break; }
                if (b.questsDone) FAIL("счётчик заданий v3 не обнулён");
                game_check_quests(&b);
                if (game_quest_count(&b, QS_ACTIVE) == 0) FAIL("задания не открылись после загрузки v3");
            }
            fclose(f);
        }
        /* неверный размер и чужая версия отвергаются */
        f = tmpfile();
        if (f) {
            unsigned bad = (unsigned)off - 16;
            fwrite("BYTG", 1, 4, f); fwrite(&bad, sizeof bad, 1, f); fwrite(blob, 1, bad, f);
            rewind(f);
            if (game_load(&b, f)) FAIL("загружено сохранение неверного размера");
            fclose(f);
        }
        printf("Сохранения: v4 %u байт, v3 %u байт — загружаются оба\n", (unsigned)sizeof(Game), v3);
    }
    printf(errors ? "Найдено ошибок: %d\n" : "Ошибок нет\n", errors);
    return errors ? 1 : 0;
}
