/* Проверка исторического наполнения без Windows: деятели (в том числе женщины Беларуси из школьного
 * конспекта), их портреты, окна приглашения, события, задания и вопросы летописца.
 *   gcc -std=c11 -O2 -Isrc tests/content_test.c src/engine.c src/war.c src/front.c src/citynet.c src/data_*.c -lm -o t && ./t
 * Запускать из корня belarus-game: тест читает assets/portraits.json. */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

static int errors, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("ОШИБКА: "); printf(__VA_ARGS__); printf("\n"); errors++; } } while (0)

/* женщины, добавленные по конспекту: id из PEOPLE и годы, в которые их точно можно пригласить */
static const struct { const char *id; int year; } WOMEN[] = {
    { "radziwill_fu", 1750 }, { "orzeszkowa", 1890 }, { "ciotka", 1905 }, { "stanyuta", 1937 },
    { "mazanik", 1943 }, { "osipova", 1943 }, { "troyan", 1943 }, { "zenkova", 1942 },
    { "tusnolobova", 1942 }, { "maslovskaya", 1944 }, { "khoruzhaya", 1941 }, { "portnova", 1942 },
    { "korbut", 1970 }, { "alexievich", 2015 }, { "domracheva", 2012 }, { "azarenka", 2012 },
    { "vasilevskaya", 2024 },
};
#define WOMEN_N ((int)(sizeof WOMEN / sizeof WOMEN[0]))
/* героини войны: приглашаются только в ходы 1941–1945 годов */
static const char *HEROINES[] = { "mazanik", "osipova", "troyan", "zenkova", "tusnolobova", "maslovskaya",
                                  "khoruzhaya", "portnova" };
static const char *NEW_EVENTS[] = { "young_avengers", "women_partisans", "vozmezdie", "every_third", "orzeszkowa",
                                    "ciotka", "korbut", "azarenka", "domracheva", "alexievich" };
static const char *NEW_QUESTS[] = { "q_nesvizh_theatre", "q_ciotka", "q_vozmezdie", "q_young_avengers", "q_sport_glory" };
#define N_OF(a) ((int)(sizeof a / sizeof a[0]))

static int person(const char *id)
{
    for (int i = 0; i < PEOPLE_COUNT; i++) if (!strcmp(PEOPLE[i].id, id)) return i;
    return -1;
}
static int hist(const char *id)
{
    for (int i = 0; i < HIST_COUNT; i++) if (!strcmp(HIST_EVENTS[i].id, id)) return i;
    return -1;
}
static int quest(const char *id)
{
    for (int q = 0; q < QUEST_COUNT; q++) if (!strcmp(QUESTS[q].id, id)) return q;
    return -1;
}
static int turn_of(int year)
{
    int t = 0;
    while (t + 1 < TURN_COUNT && TURN_YEARS[t + 1] <= year) t++;
    return t;
}
static int next_year(int t) { return t + 1 < TURN_COUNT ? TURN_YEARS[t + 1] : END_YEAR; }
/* деятель доступен для приглашения в ход t (то же правило, что game_person_state) */
static int open_in(const Person *p, int t) { return p->from < next_year(t) && p->to >= TURN_YEARS[t]; }

/* ---------- портреты: простой разбор assets/portraits.json ---------- */
static char *json_buf, *portrait_ids;
static int has_portrait(const char *id)
{
    char key[80];
    snprintf(key, sizeof key, "\"%s\"", id);
    return portrait_ids && strstr(portrait_ids, key) != 0;
}
static void load_portraits(void)
{
    FILE *f = fopen("assets/portraits.json", "rb");
    CHECK(f != 0, "нет assets/portraits.json");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    buf[got] = 0;
    fclose(f);
    json_buf = buf;
    char *ids = strstr(buf, "\"ids\"");
    CHECK(ids != 0, "в portraits.json нет списка ids");
    portrait_ids = ids;
    /* размер листа: столько клеток, сколько id */
    int cols = 0, count = 0;
    char *c = strstr(buf, "\"cols\"");
    if (c) cols = atoi(strchr(c, ':') + 1);
    for (char *p = ids; (p = strchr(p, '"')) != 0; p++) count++;
    count = (count - 2) / 2;   /* кавычки вокруг "ids" и каждого id */
    CHECK(cols == 10 && count >= PEOPLE_COUNT, "portraits.json: колонок %d, портретов %d", cols, count);
}

static void check_people(void)
{
    CHECK(PEOPLE_COUNT <= MAX_PEOPLE, "деятелей %d больше MAX_PEOPLE %d", PEOPLE_COUNT, MAX_PEOPLE);
    for (int i = 0; i < PEOPLE_COUNT; i++) {
        const Person *p = &PEOPLE[i];
        for (int j = i + 1; j < PEOPLE_COUNT; j++) CHECK(strcmp(p->id, PEOPLE[j].id), "дубль деятеля %s", p->id);
        CHECK(p->name && p->name[0] && p->life && p->life[0] && p->role && p->role[0] && p->desc && wcslen(p->desc) > 40,
              "деятель %s без текста", p->id);
        CHECK(p->from <= p->to && p->from >= TURN_YEARS[0] && p->to <= END_YEAR, "деятель %s: окно %d–%d", p->id, p->from, p->to);
        int open = 0;
        for (int t = 0; t < TURN_COUNT; t++) open |= open_in(p, t);
        CHECK(open, "деятеля %s нельзя пригласить ни в один ход (окно %d–%d)", p->id, p->from, p->to);
        CHECK(p->cost > 0 && p->cost <= 400, "деятель %s: цена %d", p->id, p->cost);
        int sum = 0;
        for (int r = 0; r < R_COUNT; r++) { CHECK(p->fx.v[r] >= 0 && p->fx.v[r] <= 12, "деятель %s: эффект %d", p->id, p->fx.v[r]); sum += p->fx.v[r]; }
        CHECK(sum > 0 && sum <= 14, "деятель %s: сумма эффектов %d", p->id, sum);
        CHECK(!p->discount || p->disci >= 0, "деятель %s: нет технологии %s", p->id, p->discount);
        CHECK(has_portrait(p->id), "у деятеля %s нет портрета в assets/portraits.json", p->id);
    }
    for (int w = 0; w < WOMEN_N; w++) {
        int i = person(WOMEN[w].id);
        CHECK(i >= 0, "нет деятельницы %s", WOMEN[w].id);
        if (i < 0) continue;
        CHECK(open_in(&PEOPLE[i], turn_of(WOMEN[w].year)), "%s нельзя пригласить в %d году", WOMEN[w].id, WOMEN[w].year);
    }
    for (int h = 0; h < N_OF(HEROINES); h++) {
        int i = person(HEROINES[h]);
        if (i < 0) { CHECK(0, "нет героини %s", HEROINES[h]); continue; }
        int inWar = 0, outside = 0;
        for (int t = 0; t < TURN_COUNT; t++)
            if (open_in(&PEOPLE[i], t)) { if (TURN_YEARS[t] >= 1941 && TURN_YEARS[t] <= 1945) inWar++; else outside++; }
        CHECK(inWar >= 2 && !outside, "героиня %s: ходов в войну %d, вне войны %d", HEROINES[h], inWar, outside);
    }
    /* Василевская: год рождения по энциклопедии — 14 сентября 1990 года */
    int v = person("vasilevskaya");
    CHECK(v >= 0 && !wcscmp(PEOPLE[v].life, L"род. 1990"), "год рождения Василевской");
}

static void check_events(void)
{
    CHECK(HIST_COUNT <= MAX_HIST, "событий %d больше MAX_HIST %d", HIST_COUNT, MAX_HIST);
    for (int i = 0; i < HIST_COUNT; i++) {
        const Event *e = &HIST_EVENTS[i];
        for (int j = i + 1; j < HIST_COUNT; j++) CHECK(strcmp(e->id, HIST_EVENTS[j].id), "дубль события %s", e->id);
        CHECK(e->year >= TURN_YEARS[0] && e->year < END_YEAR, "событие %s вне игры (%d)", e->id, e->year);
        CHECK(e->nch >= 2, "событие %s: вариантов %d", e->id, e->nch);
        int free = 0;
        for (int c = 0; c < e->nch; c++) free |= !e->ch[c].reqTech;
        CHECK(free, "событие %s: все варианты с условиями", e->id);
    }
    for (int k = 0; k < N_OF(NEW_EVENTS); k++) {
        int i = hist(NEW_EVENTS[k]);
        CHECK(i >= 0, "нет события %s", NEW_EVENTS[k]);
        if (i < 0) continue;
        /* у новых событий один вариант доступен всегда: без технологии и без расхода казны (в войну казна пуста) */
        const Event *e = &HIST_EVENTS[i];
        int free = 0;
        for (int c = 0; c < e->nch; c++) free |= !e->ch[c].reqTech && e->ch[c].dGold >= 0;
        CHECK(free, "событие %s: нет варианта без условий и расходов", e->id);
    }
    /* в один ход — не больше четырёх исторических событий, чтобы партия не превращалась в чтение */
    for (int t = 0; t < TURN_COUNT; t++) {
        int n = 0;
        for (int i = 0; i < HIST_COUNT; i++) n += HIST_EVENTS[i].year >= TURN_YEARS[t] && HIST_EVENTS[i].year < next_year(t);
        CHECK(n <= 4, "в ход %d года — %d исторических событий", TURN_YEARS[t], n);
    }
    /* события о войне выпадают на ходы 1942–1944 годов */
    const char *war[] = { "young_avengers", "women_partisans", "vozmezdie", "every_third" };
    for (int k = 0; k < 4; k++) {
        int i = hist(war[k]);
        if (i >= 0) CHECK(era_of_year(HIST_EVENTS[i].year) == 6, "событие %s не в эпоху войны", war[k]);
    }
}

static void check_quests_and_quiz(void)
{
    for (int k = 0; k < N_OF(NEW_QUESTS); k++) {
        int q = quest(NEW_QUESTS[k]);
        CHECK(q >= 0, "нет задания %s", NEW_QUESTS[k]);
        if (q < 0) continue;
        const Quest *Q = &QUESTS[q];
        CHECK(Q->type == QT_PERSON && Q->ti >= 0 && !strcmp(PEOPLE[Q->ti].id, Q->target), "задание %s: цель %s", Q->id, Q->target);
        if (Q->ti < 0) continue;
        int open = 0;   /* деятеля можно пригласить в какой-то ход между открытием задания и сроком */
        for (int t = 0; t < TURN_COUNT; t++)
            if (TURN_YEARS[t] >= ERAS[Q->era - 1].from && TURN_YEARS[t] <= Q->deadline) open |= open_in(&PEOPLE[Q->ti], t);
        CHECK(open, "задание %s невыполнимо: %s вне срока", Q->id, Q->target);
    }
    CHECK(QUIZ_COUNT <= MAX_QUIZ, "вопросов %d больше MAX_QUIZ", QUIZ_COUNT);
    for (int q = 0; q < QUIZ_COUNT; q++) {
        const Quiz *Z = &QUIZ[q];
        CHECK(Z->era >= 1 && Z->era <= ERA_COUNT && Z->correct >= 0 && Z->correct < 4 && Z->q && Z->expl, "вопрос %d", q);
        for (int a = 0; a < 4; a++) {
            CHECK(Z->a[a] && Z->a[a][0], "вопрос %d: пустой ответ", q);
            for (int b = a + 1; b < 4; b++) CHECK(!Z->a[a] || !Z->a[b] || wcscmp(Z->a[a], Z->a[b]), "вопрос %d: одинаковые ответы", q);
        }
        for (int u = q + 1; u < QUIZ_COUNT; u++) CHECK(wcscmp(Z->q, QUIZ[u].q), "дубль вопроса %d", q);
    }
}

/* Партия до 1946 года: героини приглашаются в войну, задание «Возмездие» засчитывается,
 * события о подпольщицах выпадают в свои годы, после 1945 года остаётся наследие. */
static void play_war(void)
{
    Game g;
    game_new(&g, 1, 11);
    int seen[N_OF(NEW_EVENTS)] = {0};
    int maz = person("mazanik"), zen = person("zenkova"), qv = quest("q_vozmezdie"), qy = quest("q_young_avengers");
    if (maz < 0 || zen < 0 || qv < 0 || qy < 0) return;
    while (!g.over && g.year < 1946) {
        QItem q;
        while (game_pending(&g, &q)) {
            if (q.type == Q_HIST || q.type == Q_RAND) {
                const Event *e = game_event_ptr(&q);
                if (q.type == Q_HIST)
                    for (int k = 0; k < N_OF(NEW_EVENTS); k++)
                        if (!strcmp(e->id, NEW_EVENTS[k])) { seen[k] = 1; CHECK(era_of_year(g.year) == era_of_year(e->year), "событие %s не в свой ход", e->id); }
                int pick = -1;
                for (int c = 0; c < e->nch && pick < 0; c++) if (game_choice_ok(&g, e, c)) pick = c;
                CHECK(pick >= 0, "событие %s без доступных вариантов в %d году", e->id, g.year);
                if (pick < 0) { game_ack(&g); continue; }
                Outcome o;
                game_choose(&g, pick, &o);
            } else game_ack(&g);
        }
        if (g.year == 1942) {
            CHECK(game_person_state(&g, maz) == HS_AVAILABLE && game_person_state(&g, zen) == HS_AVAILABLE, "героини недоступны в 1942 году");
            CHECK(game_quest_state(&g, qv) == QS_ACTIVE && game_quest_state(&g, qy) == QS_ACTIVE, "задания о подпольщицах не открыты в 1942 году");
            g.gold += 1000;   /* чтобы проверить найм, а не экономику */
            CHECK(game_hire(&g, maz) && game_hire(&g, zen), "героинь не удалось пригласить");
            CHECK(game_person_state(&g, maz) == HS_ACTIVE, "Мазаник не в команде");
            CHECK(game_quest_state(&g, qv) == QS_DONE && game_quest_state(&g, qy) == QS_DONE, "задания о подпольщицах не засчитаны");
        }
        game_end_turn(&g);
    }
    CHECK(g.year >= 1946 && game_person_state(&g, maz) == HS_LEGACY, "после войны Мазаник — не наследие (год %d)", g.year);
    for (int k = 0; k < 4; k++) CHECK(seen[k], "событие %s не выпало до 1946 года", NEW_EVENTS[k]);
    for (int k = 4; k < 6; k++) CHECK(seen[k], "событие %s не выпало до 1946 года", NEW_EVENTS[k]);
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    data_init();
    load_portraits();
    check_people();
    check_events();
    check_quests_and_quiz();
    play_war();
    printf("Наполнение: %d деятелей (%d женщин из конспекта), %d событий, %d заданий, %d вопросов; проверок %d, ошибок %d\n",
           PEOPLE_COUNT, WOMEN_N, HIST_COUNT, QUEST_COUNT, QUIZ_COUNT, checks, errors);
    free(json_buf);
    return errors ? 1 : 0;
}
