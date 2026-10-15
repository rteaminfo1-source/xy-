/* Проверка данных и баланса: сборка на Linux, запуск без графики.
 * gcc -O2 -I../src sim.c ../src/engine.c ../src/war.c ../src/data_*.c -lm -o sim && ./sim */
#include "game.h"
#include "war.h"
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
        int isObj = Q->type == QT_TECH || Q->type == QT_PROJ || Q->type == QT_PERSON || Q->type == QT_PROVINCE;
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
            if (Q->type == QT_PROVINCE) {
                const Province *P = &PROVINCES[Q->ti];
                if (P->to < ERAS[Q->era - 1].from || P->from > Q->deadline) FAIL("задание %s: земля вне срока", Q->id);
            }
        } else if (Q->num <= 0) FAIL("задание %s: порог", Q->id);
    }
    printf("Задания: %d; по эпохам:", QUEST_COUNT);
    for (int e = 1; e <= ERA_COUNT; e++) printf(" %d", perEra[e]);
    printf("; по типам:");
    for (int t = 0; t < QT_COUNT; t++) printf(" %d", perType[t]);
    printf("\n");
}

/* Армия, земли, области, города: данные согласованы. */
static void check_war_data(void)
{
    data_init();
    if (UNIT_COUNT > MAX_UNITS) FAIL("войск %d больше MAX_UNITS", UNIT_COUNT);
    if (PROVINCE_COUNT > MAX_PROVINCES) FAIL("земель %d больше MAX_PROVINCES", PROVINCE_COUNT);
    for (int u = 0; u < UNIT_COUNT; u++) {
        const UnitType *t = &UNIT_TYPES[u];
        for (int v = u + 1; v < UNIT_COUNT; v++) if (!strcmp(t->id, UNIT_TYPES[v].id)) FAIL("дубль войска %s", t->id);
        if (t->era < 1 || t->era > ERA_COUNT) FAIL("войско %s: эпоха", t->id);
        if (t->tech && t->techi < 0) FAIL("войско %s: нет технологии %s", t->id, t->tech);
        if (t->techi >= 0 && TECHS[t->techi].era > t->era) FAIL("войско %s раньше своей технологии", t->id);
        if (t->atk <= 0 || t->def <= 0 || t->gold <= 0 || t->ind <= 0 || t->upkeep <= 0) FAIL("войско %s: числа", t->id);
        if (!t->name || !t->desc) FAIL("войско %s без текста", t->id);
    }
    for (int p = 0; p < PROVINCE_COUNT; p++) {
        const Province *P = &PROVINCES[p];
        for (int q = p + 1; q < PROVINCE_COUNT; q++) if (!strcmp(P->id, PROVINCES[q].id)) FAIL("дубль земли %s", P->id);
        if (P->from < TURN_YEARS[0] || P->to <= P->from || P->to >= 1795) FAIL("земля %s: окно %d–%d", P->id, P->from, P->to);
        int turns = 0;
        for (int i = 0; i < TURN_COUNT; i++) turns += TURN_YEARS[i] >= P->from && TURN_YEARS[i] <= P->to;
        if (turns < 2) FAIL("земля %s: в окно попадает ходов: %d", P->id, turns);
        if (P->era != era_of_year(P->from) && P->era != era_of_year(P->from) + 1) FAIL("земля %s: эпоха %d", P->id, P->era);
        if (P->lon < 20 || P->lon > 36 || P->lat < 49 || P->lat > 58) FAIL("земля %s: координаты", P->id);
        if (P->strength <= 0 || P->popK <= 0 || P->fx.v[R_GOLD] <= 0) FAIL("земля %s: числа", P->id);
        for (int q = 0; q < PROVINCE_COUNT; q++)
            if (q != p && !strcmp(P->group, PROVINCES[q].group) && P->from <= PROVINCES[q].to && PROVINCES[q].from <= P->to)
                FAIL("земли %s и %s одной группы пересекаются по годам", P->id, PROVINCES[q].id);
    }
    for (int r = 0; r < REGION_COUNT; r++)
        if (REGION_CITY[r] < 0 || REGION_CITY[r] >= CITY_COUNT || !REGION_NAMES[r]) FAIL("область %d", r);
    for (int c = 0; c < CITY_COUNT; c++) {
        if (CITY_PROFILES[c].spec < 0 || CITY_PROFILES[c].spec >= SP_COUNT) FAIL("город %ls: специализация", CITIES[c].name);
        if (!CITY_PROFILES[c].desc || !CITY_PROFILES[c].desc[0]) FAIL("город %ls: нет описания", CITIES[c].name);
    }
    for (int sp = 0; sp < SP_COUNT; sp++) {
        if (!SPEC_NAMES[sp]) FAIL("специализация %d без имени", sp);
        int any = 0; for (int r = 0; r < R_COUNT; r++) any |= SPEC_FX[sp].v[r] != 0;
        if (!any) FAIL("специализация %d без бонуса", sp);
    }
    int perSpec[SP_COUNT] = {0};
    for (int c = 0; c < CITY_COUNT; c++) perSpec[CITY_PROFILES[c].spec]++;
    printf("Армия: %d видов войск, %d земель для походов, %d областей; городов по специализациям:", UNIT_COUNT, PROVINCE_COUNT, REGION_COUNT);
    for (int sp = 0; sp < SP_COUNT; sp++) printf(" %d", perSpec[sp]);
    printf("\n");
}

static int unit_index(const char *id)
{
    for (int u = 0; u < UNIT_COUNT; u++) if (!strcmp(UNIT_TYPES[u].id, id)) return u;
    return -1;
}
static int prov_index(const char *id)
{
    for (int p = 0; p < PROVINCE_COUNT; p++) if (!strcmp(PROVINCES[p].id, id)) return p;
    return -1;
}

/* Механика войны: набор, предел, война, победа, доход земли, осада, наследие, оккупация. */
static void test_war_rules(void)
{
    static Game g;
    game_new(&g, 1, 5);
    int dr = unit_index("druzhina"), cb = unit_index("crossbow"), rf = unit_index("rifle"), pa = unit_index("partisans");
    int sm = prov_index("smolensk"), sm2 = prov_index("smolensk2");
    if (dr < 0 || cb < 0 || rf < 0 || pa < 0 || sm < 0 || sm2 < 0) { FAIL("нет опорных войск или земель"); return; }
    if (g.warTarget != -1 || g.warKind != WAR_NONE) FAIL("в начале игры уже война");
    if (war_unit_state(&g, dr) != US_AVAILABLE) FAIL("дружина недоступна в начале");
    if (war_unit_state(&g, cb) != US_LOCKED) FAIL("арбалетчики без технологии");
    if (war_unit_state(&g, rf) != US_FUTURE) FAIL("стрелковая дивизия в 1100 году");
    if (war_cannot_declare(&g, WAR_CONQUEST, sm) == 0) FAIL("война без войска разрешена");
    g.gold = 1000; g.ind = 1000;
    double gold0 = g.gold;
    if (!war_recruit(&g, dr) || g.units[dr] != 1 || g.gold != gold0 - war_unit_cost_gold(&g, dr)) FAIL("набор дружины");
    while (war_recruit(&g, dr)) {}
    if (war_army_size(&g) != war_army_cap(&g)) FAIL("предел войска %d/%d", war_army_size(&g), war_army_cap(&g));
    Income inc;
    game_income(&g, &inc);
    if (inc.army[R_GOLD] >= 0 || inc.army[R_DEF] <= 0) FAIL("содержание и оборона войска в доходе");
    if (!war_disband(&g, dr) || war_disband(&g, cb)) FAIL("роспуск");
    war_recruit(&g, dr);
    if (war_cannot_declare(&g, WAR_LIBERATION, 0) == 0) FAIL("освобождение без оккупации");
    if (!war_declare(&g, WAR_CONQUEST, sm)) FAIL("поход на Смоленск не начат: %ls", war_cannot_declare(&g, WAR_CONQUEST, sm));
    if (war_declare(&g, WAR_CONQUEST, prov_index("podlasie"))) FAIL("вторая война одновременно");
    int guard = 0;
    while (g.warKind != WAR_NONE && guard++ < 12) {
        g.gold = 1000;
        while (war_recruit(&g, dr)) {}
        war_end_turn(&g);
    }
    if (g.warKind != WAR_NONE) FAIL("война не закончилась за 10 ходов");
    if (g.provOwned[sm] != 1) {
        /* поражение тоже исход; для проверки дохода присоединяем силой */
        g.provOwned[sm] = 1;
    }
    if (war_province_count(&g) != 1 || war_province_state(&g, sm) != PV_OWNED) FAIL("земля не присоединена");
    if (war_province_state(&g, sm2) != PV_FUTURE) FAIL("состояние будущей земли той же группы");
    game_income(&g, &inc);
    if (inc.provs[R_GOLD] < PROVINCES[sm].fx.v[R_GOLD] - 1e-9) FAIL("доход земли %.1f", inc.provs[R_GOLD]);
    /* города: стройка в столице эпохи даёт бонус специализации */
    double cb0[R_COUNT], cb1[R_COUNT];
    city_bonus(&g, C_POLOTSK, cb0);
    if (city_level(&g, C_POLOTSK) < 2) FAIL("столица эпохи без уровня");
    for (int p = 0; p < PROJECT_COUNT; p++) if (PROJECTS[p].city == C_POLOTSK) { g.proj[p] = 1; break; }
    city_bonus(&g, C_POLOTSK, cb1);
    if (cb1[R_SCI] <= cb0[R_SCI]) FAIL("стройка не подняла уровень города");
    /* осада: Москва берёт Смоленск в 1514 году, если войска нет */
    memset(g.units, 0, sizeof g.units);
    g.year = 1530; g.era = 2;
    war_year_changed(&g, 1510);
    if (g.provOwned[sm] != 0) FAIL("Смоленск не потерян без войска (оборона %.0f)", war_defense_power(&g));
    if (war_province_state(&g, sm2) != PV_AVAILABLE) FAIL("Смоленск нельзя отвоевать после потери");
    /* наследие после 1795 */
    g.provOwned[sm2] = 1;
    g.year = 1795; g.era = 4;
    war_year_changed(&g, 1772);
    if (g.provOwned[sm2] != 2 || war_province_state(&g, sm2) != PV_LEGACY) FAIL("наследие после разделов");
    game_income(&g, &inc);
    if (inc.provs[R_GOLD] <= 0) FAIL("наследие без дохода");
    /* оккупация 1941–1944 */
    g.year = 1941; g.era = 6;
    war_year_changed(&g, 1937);
    if (war_occupied_count(&g) != REGION_COUNT) FAIL("оккупация не началась");
    game_income(&g, &inc);
    if (inc.occMult >= 1.0) FAIL("оккупация не снижает доходы");
    if (war_raid_chance(&g) != 0) FAIL("набеги во время оккупации");
    if (war_cannot_declare(&g, WAR_CONQUEST, sm2) == 0) FAIL("походы во время оккупации");
    g.gold = 100000; g.ind = 100000; g.pop = 9000;
    g.tech[UNIT_TYPES[rf].techi] = 1;
    while (war_recruit(&g, rf)) {}
    if (war_attack_power(&g, WAR_LIBERATION) <= 0) FAIL("нет силы для освобождения");
    if (!war_declare(&g, WAR_LIBERATION, 2)) FAIL("освобождение Минщины: %ls", war_cannot_declare(&g, WAR_LIBERATION, 2));
    guard = 0;
    while (g.warKind != WAR_NONE && guard++ < 12) war_end_turn(&g);
    if (g.occupied & 4u) FAIL("Минщина не освобождена большой армией");
    if (!(g.liberated & 4u)) FAIL("освобождение не засчитано");
    g.year = 1945; g.era = 7;
    war_year_changed(&g, 1944);
    if (g.occupied) FAIL("оккупация не закончилась в 1945 году");
    printf("Война: эта %d ход(а) на Смоленск дружиной, сила области в 1942: %d, набег в эпоху I: %.0f\n",
           war_eta(&g, WAR_CONQUEST, sm), (g.year = 1942, war_region_strength(&g)), (g.era = 1, war_raid_strength(&g)));
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
        if (Q->type == QT_ARMY || Q->type == QT_PROVINCE || Q->type == QT_LIBERATE) {
            int best = -1;
            for (int u = 0; u < UNIT_COUNT; u++)
                if (war_unit_state(g, u) == US_AVAILABLE && (best < 0 || UNIT_TYPES[u].atk * war_unit_eff(g, u) > UNIT_TYPES[best].atk * war_unit_eff(g, best))) best = u;
            int want = Q->type == QT_ARMY ? Q->num : war_army_cap(g);
            while (best >= 0 && war_army_size(g) < want && war_recruit(g, best)) {}
            if (best >= 0 && war_army_size(g) < want) *saveGold = 1;
            if (Q->type == QT_PROVINCE && g->warKind == WAR_NONE && war_eta(g, WAR_CONQUEST, Q->ti) <= 4) war_declare(g, WAR_CONQUEST, Q->ti);
            if (Q->type == QT_LIBERATE && g->warKind == WAR_NONE)
                for (int r = 0; r < REGION_COUNT; r++)
                    if ((g->occupied >> r & 1u) && war_declare(g, WAR_LIBERATION, r)) break;
        }
        if (Q->type == QT_PERSON && game_person_state(g, Q->ti) == HS_AVAILABLE) {
            if (g->gold >= game_person_cost(g, Q->ti)) game_hire(g, Q->ti);
            else *saveGold = 1;
        }
    }
}

/* Бот-полководец: держит войско лучшего доступного вида, идёт в поход, когда победа близка,
 * перед 1941 годом копит армию, в 1941–1944 освобождает области. */
static int armyBot;

static void army_turn(Game *g)
{
    int best = -1;
    double bestV = 0;
    for (int u = 0; u < UNIT_COUNT; u++)
        if (war_unit_state(g, u) == US_AVAILABLE) {
            double v = (UNIT_TYPES[u].atk + UNIT_TYPES[u].def) * war_unit_eff(g, u) / UNIT_TYPES[u].gold;
            if (v > bestV) { bestV = v; best = u; }
        }
    int want = g->era <= 3 ? 3 + 2 * g->era : 3 + g->era;
    if (g->year >= 1925 && g->year <= 1944) want = 16;
    if (want > war_army_cap(g)) want = war_army_cap(g);
    double budget = g->era <= 3 || (g->year >= 1925 && g->year <= 1944) ? g->gold - 5 : g->gold * 0.35;
    while (best >= 0 && war_army_size(g) < want && war_unit_cost_gold(g, best) <= budget &&
           g->ind >= war_unit_cost_ind(g, best)) {
        budget -= war_unit_cost_gold(g, best);
        if (!war_recruit(g, best)) break;
    }
    /* устаревшие войска распускаем, если есть замена */
    for (int u = 0; u < UNIT_COUNT; u++)
        if (g->units[u] > 0 && war_unit_eff(g, u) < 0.7 && best >= 0 && best != u) war_disband(g, u);
    if (g->warKind != WAR_NONE) {
        if (war_eta(g, g->warKind, g->warTarget) > 6) war_peace(g);
        return;
    }
    if (g->occupied) {
        for (int r = 0; r < REGION_COUNT; r++)
            if ((g->occupied >> r & 1u) && war_eta(g, WAR_LIBERATION, r) <= 3) { war_declare(g, WAR_LIBERATION, r); return; }
        return;
    }
    for (int p = 0; p < PROVINCE_COUNT; p++)
        if (war_province_state(g, p) == PV_AVAILABLE && war_eta(g, WAR_CONQUEST, p) <= 3) { war_declare(g, WAR_CONQUEST, p); return; }
}

/* Жадный бот: сначала самые дешёвые технологии, затем стройки, затем деятели. */
static void bot_turn(Game *g, int quizSkill)
{
    int saveSci = 0, saveGold = 0;
    if (armyBot) army_turn(g);
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
    check_war_data();
    test_quest_rules();
    test_war_rules();
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
    /* бот-полководец: армия, походы, освобождение */
    armyBot = 1;
    for (int d = 0; d < DIFFICULTY_COUNT; d++) {
        double sumScore = 0, sumPop = 0, sumProv = 0, sumWon = 0, sumLost = 0, sumLib = 0, sumUnits = 0, sumPop45 = 0;
        double sumT = 0, sumP = 0, sumH = 0, sumW = 0;
        int runs = 20;
        for (int s = 1; s <= runs; s++) {
            static Game g;
            game_new(&g, d, (unsigned)(s * 7919));
            int maxProv = 0;
            while (!g.over) {
                resolve_events(&g);
                if (g.over) break;
                bot_turn(&g, 70);
                if (verbose && s == 1 && d == 1) {
                    Income inc; game_income(&g, &inc);
                    Army a; war_army(&g, &a);
                    printf("%4d э%d поп %6.0f каз %6.0f(+%4.0f, войско %4.0f) войско %2d/%2d атк %5.0f обор %5.0f земли %d окк %d %ls\n",
                           g.year, g.era, g.pop, g.gold, inc.total[R_GOLD], inc.army[R_GOLD], a.units, war_army_cap(&g),
                           a.attack, war_defense_power(&g), war_province_count(&g), war_occupied_count(&g),
                           g.warNews ? g.warReport : L"");
                }
                game_end_turn(&g);
                if (war_province_count(&g) > maxProv) maxProv = war_province_count(&g);
                if (g.gold < -1e-6) FAIL("отрицательная казна в %d году", g.year);
                for (int u = 0; u < UNIT_COUNT; u++) if (g.units[u] < 0) FAIL("отрицательное войско");
            }
            resolve_events(&g);
            Score sc;
            game_score(&g, &sc);
            int lib = 0; for (int r = 0; r < REGION_COUNT; r++) lib += (g.liberated >> r) & 1u;
            sumScore += sc.total; sumPop += g.pop; sumProv += maxProv; sumWon += g.battlesWon; sumLost += g.battlesLost;
            sumLib += lib; sumUnits += g.unitsLost; sumPop45 += g.pop1945;
            sumT += game_tech_count(&g); sumP += game_proj_count(&g); sumH += game_people_count(&g); sumW += sc.war;
        }
        printf("[%ls] бот-полководец: счёт %.0f (техн %.0f, стр %.0f, люди %.0f, армия %.0f), население %.0f тыс. (1945: %.0f), земель %.1f, побед %.1f, поражений %.1f, освобождено %.1f, потеряно соединений %.1f\n",
               DIFFICULTIES[d].name, sumScore / runs, sumT / runs, sumP / runs, sumH / runs, sumW / runs, sumPop / runs, sumPop45 / runs, sumProv / runs, sumWon / runs, sumLost / runs, sumLib / runs, sumUnits / runs);
    }
    armyBot = 0;
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
        printf("Сохранения: v5 %u байт, v3 %u байт — загружаются оба\n", (unsigned)sizeof(Game), v3);
    }
    /* совместимость: сохранение версии 4 (без армии) */
    {
        static Game a, b;
        game_new(&a, 1, 91);
        for (int t = 0; t < 8; t++) { resolve_events(&a); bot_turn(&a, 70); game_end_turn(&a); }
        size_t off = offsetof(Game, units);
        unsigned v4 = (unsigned)((off + _Alignof(Game) - 1) / _Alignof(Game) * _Alignof(Game));
        static unsigned char blob[sizeof(Game)];
        memcpy(blob, &a, sizeof a);
        ((Game *)blob)->version = 4;
        memset(blob + off, 0xCD, v4 - off);
        FILE *f = tmpfile();
        if (f) {
            fwrite("BYTG", 1, 4, f); fwrite(&v4, sizeof v4, 1, f); fwrite(blob, 1, v4, f);
            rewind(f);
            memset(&b, 0x5A, sizeof b);
            if (!game_load(&b, f)) FAIL("сохранение версии 4 не загрузилось");
            else {
                if (b.version != GAME_VERSION || b.warKind != WAR_NONE || b.warTarget != -1 || war_army_size(&b) || b.occupied)
                    FAIL("армия v4 не обнулена");
                if (memcmp(&a, &b, offsetof(Game, units))) FAIL("данные v4 искажены");
                game_end_turn(&b);
            }
            fclose(f);
        }
        /* испорченная цель войны отвергается */
        f = tmpfile();
        if (f) {
            static Game c;
            game_new(&c, 1, 3);
            c.warKind = WAR_CONQUEST; c.warTarget = 999;
            game_save(&c, f); rewind(f);
            if (game_load(&b, f)) FAIL("загружена война с неверной целью");
            fclose(f);
        }
        printf("Сохранения: v4 %u байт загружается, армия с нуля\n", v4);
    }
    printf(errors ? "Найдено ошибок: %d\n" : "Ошибок нет\n", errors);
    return errors ? 1 : 0;
}
