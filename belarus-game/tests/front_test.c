/* Проверка армий на карте (front.c) без Windows: граф фронта, марш по дорогам и рельсам, бой только
 * у цели, снабжение, железные дороги и поезда, отход 1941 года, земли после 1795 года, старые сохранения.
 *   gcc -std=c11 -O2 -Isrc tests/front_test.c src/engine.c src/war.c src/front.c src/citynet.c src/data_*.c -lm -o t && ./t */
#include "game.h"
#include "war.h"
#include "front.h"
#include <stdio.h>
#include <string.h>
#include <locale.h>

static int errors, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { printf("ОШИБКА: "); printf(__VA_ARGS__); printf("\n"); errors++; } } while (0)

static int node_named(const wchar_t *s)
{
    for (int n = 0; n < FRONT_NODE_COUNT; n++) if (!wcscmp(FRONT_NODES[n].name, s)) return n;
    return -1;
}

static int prov(const char *id)
{
    for (int p = 0; p < PROVINCE_COUNT; p++) if (!strcmp(PROVINCES[p].id, id)) return p;
    return -1;
}

static int unit(const char *id)
{
    for (int u = 0; u < UNIT_COUNT; u++) if (!strcmp(UNIT_TYPES[u].id, id)) return u;
    return -1;
}

static int city(int c) { return front_node_of_city(c); }

/* новая партия, перенесённая в ход года year: технологии своей эпохи изучены, казна полна */
static void fresh(Game *g, int year)
{
    game_new(g, 1, 5);
    g->qn = g->qpos = 0;
    while (g->turn + 1 < TURN_COUNT && TURN_YEARS[g->turn + 1] <= year) g->turn++;
    g->year = TURN_YEARS[g->turn];
    g->era = era_of_year(g->year);
    for (int t = 0; t < TECH_COUNT; t++) if (TECHS[t].era <= g->era && strcmp(TECHS[t].id, "railway")) g->tech[t] = 1;
    g->gold = g->ind = 1e6;
    g->pop = 20000;
    if (g->year >= 1941 && g->year <= 1944) g->occupied = (1u << REGION_COUNT) - 1;
}

static void give_units(Game *g, const char *id, int n)
{
    int u = unit(id);
    if (u < 0) { CHECK(0, "нет войска %s", id); return; }
    g->units[u] += n;
    front_sync(g);
}

static int army_sum(const Game *g, int u)
{
    int s = 0;
    for (int a = 0; a < MAX_ARMIES; a++) if (g->armies[a].node >= 0) s += g->armies[a].units[u];
    return s;
}

static int set_rail(Game *g, int a, int b)
{
    int e = front_edge(a, b);
    if (e >= 0) g->rails[e >> 6] |= 1ull << (e & 63);
    return e;
}

/* ---------- граф ---------- */
static void test_graph(void)
{
    CHECK(FRONT_NODE_COUNT > 0 && FRONT_NODE_COUNT <= FRONT_MAX_NODES, "узлов %d", FRONT_NODE_COUNT);
    CHECK(FRONT_EDGE_COUNT > 0 && FRONT_EDGE_COUNT <= FRONT_MAX_EDGES && FRONT_MAX_EDGES <= 128, "рёбер %d", FRONT_EDGE_COUNT);
    /* номера хранятся в сохранениях: начало таблиц не должно сдвигаться */
    CHECK(city(C_MINSK) == 0 && city(C_BORISOV) == 15, "номера узлов городов сдвинулись");
    CHECK(node_named(L"Молодечно") == 16 && node_named(L"Барановичи") == 17, "номера узловых станций сдвинулись");
    CHECK(front_rear_node() == 18 && FRONT_NODES[18].kind == FN_REAR, "Большая земля не 18-й узел");
    CHECK(FRONT_EDGES[0].a == city(C_MINSK) && FRONT_EDGES[0].b == city(C_BORISOV), "ребро 0 — не Минск — Борисов");
    CHECK(front_edge(node_named(L"Барановичи"), city(C_BREST)) == 3, "ребро 3 — не Барановичи — Брест");
    static const int need[] = { C_MINSK, C_POLOTSK, C_VITEBSK, C_MOGILEV, C_GOMEL, C_GRODNO, C_BREST, C_TUROV, C_NOVOGRUDOK,
                                C_PINSK, C_ORSHA, C_SLUTSK, C_MOZYR, C_BOBRUISK, C_LIDA, C_BORISOV };
    for (unsigned i = 0; i < sizeof need / sizeof need[0]; i++) CHECK(city(need[i]) >= 0, "нет узла города %ls", CITIES[need[i]].name);
    for (int p = 0; p < PROVINCE_COUNT; p++) {
        int n = front_node_of_prov(p);
        CHECK(n >= 0 && FRONT_NODES[n].kind == FN_LAND, "нет узла земли %s", PROVINCES[p].id);
        if (n >= 0) CHECK(front_neighbors(n, 0, 0, FRONT_MAX_ADJ) > 0, "земля %s без дорог", PROVINCES[p].id);
    }
    for (int r = 0; r < REGION_COUNT; r++) CHECK(front_node_of_region(r) >= 0, "нет центра области %d", r);
    /* рёбра: симметричны, разумной длины; граф связный */
    for (int e = 0; e < FRONT_EDGE_COUNT; e++) {
        const FrontEdge *E = &FRONT_EDGES[e];
        if (E->a < 0) continue;
        CHECK(front_edge(E->a, E->b) == e && front_edge(E->b, E->a) == e, "ребро %d несимметрично", e);
        CHECK(E->km >= 15 && E->km <= 420, "ребро %ls — %ls: %d км", FRONT_NODES[E->a].name, FRONT_NODES[E->b].name, E->km);
        CHECK(!E->line || (E->year >= 1860 && E->year <= 1910), "ребро %d: год магистрали %d", e, E->year);
    }
    int q[FRONT_MAX_NODES], h = 0, t = 0;
    unsigned char seen[FRONT_MAX_NODES] = { 0 };
    seen[0] = 1; q[t++] = 0;
    while (h < t) {
        int n = q[h++], nb[FRONT_MAX_ADJ];
        int k = front_neighbors(n, nb, 0, FRONT_MAX_ADJ);
        for (int i = 0; i < k; i++) if (!seen[nb[i]]) { seen[nb[i]] = 1; q[t++] = nb[i]; }
    }
    CHECK(t == FRONT_NODE_COUNT, "граф фронта несвязный: достижимо %d из %d", t, FRONT_NODE_COUNT);
    /* бюджет марша: одна дорога в короткий ход, больше — в долгие ходы средних веков */
    int t1941 = 0, t1340 = 0;
    for (int i = 0; i < TURN_COUNT; i++) { if (TURN_YEARS[i] == 1941) t1941 = i; if (TURN_YEARS[i] == 1340) t1340 = i; }
    CHECK(front_turn_budget(t1941) == 3, "бюджет 1941 года %d", front_turn_budget(t1941));
    CHECK(front_turn_budget(t1340) >= 6, "бюджет 1340 года %d", front_turn_budget(t1340));
}

/* ---------- армии: набор, разделение, объединение ---------- */
static void test_armies(void)
{
    static Game g;
    fresh(&g, 1340);
    int dr = unit("druzhina");
    CHECK(front_army_count(&g) == 0, "армия до набора");
    for (int i = 0; i < 7; i++) CHECK(war_recruit(&g, dr), "набор дружины %d", i);
    CHECK(front_army_count(&g) == 1, "армий после набора: %d", front_army_count(&g));
    CHECK(g.armies[0].node == city(C_NOVOGRUDOK), "в эпоху II войска собираются не в Новогрудке");
    CHECK(front_army_size(&g, 0) == 7 && army_sum(&g, dr) == g.units[dr], "состав армии");
    int b = front_split(&g, 0);
    CHECK(b == 1 && front_army_size(&g, 0) + front_army_size(&g, 1) == 7 && front_army_size(&g, 1) == 3, "разделение 7 → %d + %d",
          front_army_size(&g, 0), b >= 0 ? front_army_size(&g, b) : -1);
    CHECK(front_split(&g, 0) == 2 && front_split(&g, 1) == 3 && front_split(&g, 0) == -1, "больше четырёх армий");
    /* армия ушла от столицы — новое соединение собирается у столицы в армии, которая там стоит */
    g.armies[3].node = city(C_LIDA);
    CHECK(war_recruit(&g, dr) && army_sum(&g, dr) == g.units[dr], "набор при четырёх армиях");
    CHECK(front_merge(&g, 0) == 2, "объединение с армиями того же узла");
    CHECK(front_army_count(&g) == 2 && army_sum(&g, dr) == g.units[dr], "после объединения армий %d", front_army_count(&g));
    CHECK(war_disband(&g, dr) && army_sum(&g, dr) == g.units[dr], "роспуск");
    CHECK(front_cannot_send(&g, 0, front_node_of_prov(prov("volhynia"))) != 0, "приказ идти на чужую землю без войны");
    CHECK(front_cannot_send(&g, 0, front_rear_node()) != 0, "Большая земля открыта в 1340 году");
}

/* ---------- марш по дорогам и по рельсам ---------- */
static void test_march(void)
{
    static Game g;
    wchar_t news[400];
    fresh(&g, 1950);                                       /* ход 1950–1955: одна дорога за ход */
    give_units(&g, "motor", 4);
    int minsk = city(C_MINSK), bar = node_named(L"Барановичи"), brest = city(C_BREST);
    CHECK(front_army_count(&g) == 1 && g.armies[0].node == minsk, "армия не у столицы");
    CHECK(front_eta(&g, minsk, brest) == 2, "от Минска до Бреста без рельсов %d хода", front_eta(&g, minsk, brest));
    CHECK(front_send(&g, 0, brest), "приказ идти в Брест: %ls", front_cannot_send(&g, 0, brest));
    front_end_turn(&g, news, 400);
    CHECK(g.armies[0].node == bar && g.armyPrev[0] == minsk && g.armies[0].dest == brest, "первый ход марша: узел %d", g.armies[0].node);
    CHECK(wcsstr(news, L"идёт") != 0, "нет сводки марша: %ls", news);
    front_end_turn(&g, news, 400);
    CHECK(g.armies[0].node == brest && g.armies[0].dest < 0, "армия не пришла в Брест");
    CHECK(wcsstr(news, L"пришла") != 0, "нет сводки прибытия: %ls", news);
    /* по своей железной дороге — до трёх перегонов за ход */
    g.tech[tech_index("railway")] = 1;
    set_rail(&g, minsk, bar);
    set_rail(&g, bar, brest);
    CHECK(front_rail_count(&g) == 2, "рельсов %d", front_rail_count(&g));
    CHECK(front_eta(&g, brest, minsk) == 1, "по рельсам Брест — Минск за %d хода", front_eta(&g, brest, minsk));
    front_send(&g, 0, minsk);
    front_end_turn(&g, news, 400);
    CHECK(g.armies[0].node == minsk, "по рельсам армия не дошла за ход");
    front_halt(&g, 0);
    CHECK(!front_halt(&g, 0), "остановка без приказа");
}

/* ---------- железные дороги: строительство, доход, магистрали, поезда ---------- */
static void test_rails(void)
{
    static Game g;
    fresh(&g, 1875);
    int minsk = city(C_MINSK), bor = city(C_BORISOV), orsha = city(C_ORSHA), vit = city(C_VITEBSK);
    int e1 = front_edge(minsk, bor), e2 = front_edge(bor, orsha), e3 = front_edge(orsha, vit);
    CHECK(front_cannot_build_rail(&g, e1) != 0, "рельсы без технологии");
    g.tech[tech_index("railway")] = 1;
    Income before, after;
    game_income(&g, &before);
    double gold0 = g.gold;
    CHECK(front_rail_gold(&g, e1) > 0 && front_rail_ind(&g, e1) > 0, "цена перегона");
    CHECK(front_cannot_build_rail(&g, e3) != 0, "рельсы в отрыве от сети столицы");
    CHECK(front_build_rail(&g, e1), "Минск — Борисов: %ls", front_cannot_build_rail(&g, e1));
    CHECK(g.gold == gold0 - front_rail_gold(&g, e1), "казна за перегон");
    CHECK(front_build_rail(&g, e2) && front_build_rail(&g, e3), "продолжение сети до Витебска");
    CHECK(!front_build_rail(&g, e1), "второй раз тот же перегон");
    CHECK(front_rail_count(&g) == 3, "перегонов %d", front_rail_count(&g));
    game_income(&g, &after);
    CHECK(after.cities[R_GOLD] > before.cities[R_GOLD] && after.cities[R_IND] > before.cities[R_IND], "железная дорога без дохода");
    static int routes[8][FRONT_MAX_NODES], lens[8];
    int nr = front_train_routes(&g, routes, lens, 8);
    CHECK(nr == 1 && lens[0] == 4 && routes[0][0] == minsk && routes[0][3] == vit, "маршрут поезда: %d маршрутов", nr);
    /* стройка магистрали прокладывает её перегоны */
    static Game h;
    fresh(&h, 1875);
    for (int p = 0; p < PROJECT_COUNT; p++) if (!strcmp(PROJECTS[p].id, "moscow_brest_rw")) h.proj[p] = 1;
    CHECK(front_rail(&h, front_edge(node_named(L"Барановичи"), city(C_BREST))), "стройка Московско-Брестской не дала рельсов");
    CHECK(front_rail_hist(e1) && FRONT_EDGES[e1].year == 1871, "Минск — Борисов не перегон магистрали 1871 года");
    /* в оккупацию перегоны оккупированных областей не возят войска */
    g.occupied = 1u << 2;                                  /* Минщина */
    CHECK(front_rail(&g, e1) && !front_rail_usable(&g, e1), "рельсы в оккупированной области");
}

/* ---------- бой идёт только у цели ---------- */
static void test_battle_at_node(void)
{
    static Game g;
    fresh(&g, 1310);
    int kh = unit("khorugv"), vol = prov("volhynia"), node = front_node_of_prov(vol);
    for (int i = 0; i < 10; i++) war_recruit(&g, kh);
    CHECK(front_army_count(&g) == 1, "армия для похода");
    CHECK(war_declare(&g, WAR_CONQUEST, vol), "поход на Волынь: %ls", war_cannot_declare(&g, WAR_CONQUEST, vol));
    CHECK(front_node_open(&g, node), "цель похода закрыта для армии");
    /* война идёт сама: армия выступила к цели без приказа */
    CHECK(g.armies[0].dest == node, "армия не выступила к Волыни сама");
    CHECK(war_eta(&g, WAR_CONQUEST, vol) < 99, "прогноз похода с маршем");
    /* игрок отправил армию в другое место: к цели никто не идёт — подсказка, боя нет */
    int other = front_capital(&g);
    g.armies[0].node = other == node ? g.armies[0].node : other;
    front_halt(&g, 0);
    int nb[8], ne[8], k = front_neighbors(other, nb, ne, 8), side = -1;
    for (int i = 0; i < k && side < 0; i++) if (nb[i] != node && !front_cannot_send(&g, 0, nb[i])) side = nb[i];
    if (side >= 0) {
        CHECK(front_send(&g, 0, side), "приказ идти в сторону");
        war_end_turn(&g);
        CHECK(g.warTurns == 0 && g.warKind == WAR_CONQUEST, "бой без армии у цели");
        CHECK(wcsstr(g.warReport, L"другие приказы") != 0, "нет подсказки отправить армию: %ls", g.warReport);
    }
    /* свободная армия на «Завершить ход» снова выступает сама */
    front_halt(&g, 0);
    int arrived = 0;
    for (int i = 0; i < 12 && g.warKind != WAR_NONE; i++) {
        war_end_turn(&g);
        if (g.armies[0].node == node) arrived = 1;
        if (!arrived) CHECK(g.warTurns == 0, "бой до прихода армии (ход %d)", i);
        if (g.warTurns > 0) CHECK(arrived, "бой без армии на узле");
    }
    CHECK(arrived, "армия не дошла до Волыни");
    CHECK(g.provOwned[vol] == 1, "Волынь не взята армией из 10 хоругвей (прогресс %d)", g.warProgress);
    CHECK(g.battleLon == (float)PROVINCES[vol].lon, "место боя не у цели");
    CHECK(front_node_open(&g, node) && g.armies[0].node == node, "армия ушла со своей новой земли");
}

/* ---------- снабжение и оккупация ---------- */
static void test_supply(void)
{
    static Game g;
    wchar_t news[400];
    fresh(&g, 1937);
    give_units(&g, "rifle", 6);
    CHECK(g.armies[0].node == city(C_MINSK), "армия 1937 года не в Минске");
    CHECK(front_army_supplied(&g, 0), "у столицы нет снабжения");
    /* 22 июня 1941 года: армии отходят на Большую землю */
    g.turn++; g.year = 1941; g.era = era_of_year(1941);
    war_year_changed(&g, 1937);
    CHECK(g.occupied == (1u << REGION_COUNT) - 1, "оккупация не началась");
    CHECK(g.armies[0].node == front_rear_node(), "армия не отошла на Большую землю");
    CHECK(front_army_supplied(&g, 0), "на Большой земле нет снабжения");
    CHECK(front_recruit_node(&g) == front_rear_node(), "пополнение собирается не на Большой земле");
    int minsk = city(C_MINSK), vit = city(C_VITEBSK);
    CHECK(front_supplied_at(&g, vit) && !front_supplied_at(&g, minsk), "снабжение в 1941: Витебск %d, Минск %d",
          front_supplied_at(&g, vit), front_supplied_at(&g, minsk));
    double atkRear = front_army_attack(&g, 0, WAR_LIBERATION, -1);
    g.armies[0].node = minsk;
    double atkMinsk = front_army_attack(&g, 0, WAR_LIBERATION, -1);
    CHECK(!front_army_supplied(&g, 0) && atkMinsk < atkRear * 0.65, "без снабжения сила %.0f из %.0f", atkMinsk, atkRear);
    int size0 = front_army_size(&g, 0);
    front_end_turn(&g, news, 400);
    CHECK(front_army_size(&g, 0) == size0 && wcsstr(news, L"без снабжения") != 0, "первый ход без снабжения: %ls", news);
    front_end_turn(&g, news, 400);
    CHECK(front_army_size(&g, 0) == size0 - 1 && wcsstr(news, L"потеряно") != 0, "второй ход без снабжения — потеря: %ls", news);
    CHECK(army_sum(&g, unit("rifle")) == g.units[unit("rifle")], "войско и армии разошлись после потерь");
    /* освобождённая Могилёвщина снабжает соседей */
    g.occupied &= ~(1u << 4);
    CHECK(front_supplied_at(&g, city(C_BOBRUISK)), "освобождённая область не снабжает");
    /* партизанам снабжение не нужно */
    static Game p;
    fresh(&p, 1942);
    give_units(&p, "partisans", 3);
    p.armies[0].node = minsk;
    CHECK(front_army_supplied(&p, 0), "партизанам нужно снабжение");
    /* освобождение: бой — только у центра области */
    g.armies[0].node = front_rear_node();
    g.armyLow[0] = 0;
    CHECK(war_declare(&g, WAR_LIBERATION, 3), "освобождение Витебщины: %ls", war_cannot_declare(&g, WAR_LIBERATION, 3));
    front_send(&g, 0, vit);
    give_units(&g, "rifle", 8);
    front_merge(&g, 0);
    for (int i = 0; i < 3 && (g.occupied >> 3 & 1u); i++) war_end_turn(&g);
    CHECK(!(g.occupied >> 3 & 1u) && (g.liberated >> 3 & 1u), "Витебщина не освобождена армией у Витебска");
    /* 1945: армии возвращаются с Большой земли */
    g.armies[0].node = front_rear_node();
    g.turn++; g.year = 1945; g.era = era_of_year(1945);
    war_year_changed(&g, 1944);
    CHECK(!g.occupied && g.armies[0].node >= 0 && FRONT_NODES[g.armies[0].node].kind == FN_CITY, "армия осталась на Большой земле");
}

/* ---------- оборона городов: армии рядом ---------- */
static void test_defense(void)
{
    static Game g;
    fresh(&g, 1200);
    give_units(&g, "druzhina", 6);
    int pol = city(C_POLOTSK), brest = city(C_BREST);
    CHECK(g.armies[0].node == pol, "в эпоху I армия не в Полоцке");
    CHECK(war_defense_at(&g, pol) > war_defense_at(&g, brest), "оборона у армии не выше: %.0f / %.0f",
          war_defense_at(&g, pol), war_defense_at(&g, brest));
    CHECK(front_defense_at(&g, city(C_VITEBSK)) > front_defense_at(&g, brest), "соседний город защищён слабее дальнего");
}

/* ---------- земли после 1795 года и старые сохранения ---------- */
static void test_lands_and_old_saves(void)
{
    static Game g;
    fresh(&g, 1772);
    int vol = prov("volhynia"), sm2 = prov("smolensk2");
    g.provOwned[vol] = 1;
    g.provOwned[sm2] = 1;
    g.turn++; g.year = 1795; g.era = era_of_year(1795);
    war_year_changed(&g, 1772);
    CHECK(g.provOwned[vol] == 1 && g.provOwned[sm2] == 1 && war_province_count(&g) == 2, "земли не остались своими после 1795 года");
    CHECK(war_province_state(&g, vol) == PV_OWNED, "состояние земли после разделов");
    double inc[R_COUNT];
    provinces_income(&g, inc);
    double want = PROVINCES[vol].fx.v[R_GOLD] * era_scale(g.era) / era_scale(PROVINCES[vol].era) +
                  PROVINCES[sm2].fx.v[R_GOLD] * era_scale(g.era) / era_scale(PROVINCES[sm2].era);
    CHECK(inc[R_GOLD] > want - 1e-6, "доход земель после 1795 года %.1f вместо %.1f", inc[R_GOLD], want);
    /* старое сохранение: наследие снова своя земля, войско встаёт армией у столицы */
    static Game o;
    fresh(&o, 1830);
    o.provOwned[vol] = 2;
    o.units[unit("line")] = 5;
    for (int a = 0; a < MAX_ARMIES; a++) o.armies[a].node = o.armies[a].dest = -1;
    o.frontReady = 0;
    front_sync(&o);
    CHECK(o.frontReady && o.provOwned[vol] == 1, "наследие старого сохранения");
    CHECK(front_army_count(&o) == 1 && front_army_size(&o, 0) == 5 && o.armies[0].node == front_capital(&o), "армия старого сохранения");
    /* сохранение и загрузка армий и рельсов */
    o.tech[tech_index("railway")] = 1;
    set_rail(&o, city(C_MINSK), city(C_BORISOV));
    front_send(&o, 0, city(C_BREST));
    FILE *f = tmpfile();
    static Game b;
    if (f && game_save(&o, f)) {
        rewind(f);
        CHECK(game_load(&b, f) && !memcmp(&o, &b, sizeof o), "армии и рельсы не пережили сохранение");
        fclose(f);
    } else CHECK(0, "сохранение");
}

/* ---------- реальное время: клетка за 10 секунд и бой у цели раз в 10 секунд ---------- */
static void test_realtime(void)
{
    static Game g;
    fresh(&g, 1310);
    int kh = unit("khorugv"), vol = prov("volhynia"), node = front_node_of_prov(vol);
    for (int i = 0; i < 10; i++) war_recruit(&g, kh);
    CHECK(war_declare(&g, WAR_CONQUEST, vol), "поход на Волынь: %ls", war_cannot_declare(&g, WAR_CONQUEST, vol));
    CHECK(g.armies[0].dest == node, "армия не выступила сама");
    CHECK(!war_front_battle(&g), "бой до прихода армии");
    int steps = 0;
    while (g.armies[0].node != node && steps < 20) {
        int ms = front_step_ms(&g, 0);
        CHECK(ms == FRONT_STEP_MS || ms == FRONT_STEP_MS / 3, "время шага %d мс", ms);
        int nx = front_next_node(&g, 0);
        CHECK(front_step(&g, 0) && g.armies[0].node == nx, "шаг на следующую клетку");
        steps++;
    }
    CHECK(g.armies[0].node == node && g.armies[0].dest < 0, "армия не дошла по клеткам (%d шагов)", steps);
    CHECK(!front_step(&g, 0), "шаг без приказа");
    int wt = g.warTurns;
    CHECK(war_front_battle(&g), "нет боя у цели в реальном времени");
    CHECK(g.warTurns == wt + 1 && g.battleTurn == g.turn, "бой не засчитан или звезда боя не на этом ходу");
    for (int i = 0; i < 12 && g.warKind != WAR_NONE; i++) war_front_battle(&g);
    CHECK(g.warKind == WAR_NONE, "война не закончилась боями в реальном времени");
    CHECK(!war_front_battle(&g), "бой без войны");
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    data_init();
    test_graph();
    test_armies();
    test_march();
    test_rails();
    test_battle_at_node();
    test_realtime();
    test_supply();
    test_defense();
    test_lands_and_old_saves();
    printf("Фронт: %d узлов, %d дорог; проверок %d, ошибок %d\n", FRONT_NODE_COUNT, FRONT_EDGE_COUNT, checks, errors);
    return errors ? 1 : 0;
}
