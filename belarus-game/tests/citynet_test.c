/* Городской транспорт (citynet.c): данные схем, правила стройки перегонов, доход, первая очередь
 * как стройка PROJECTS и сохранение. Запуск — из tests/run.sh вместе с движком. */
#include "citynet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("ОШИБКА %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int net_by_id(const char *id)
{
    for (int n = 0; n < CITY_NET_COUNT; n++) if (strcmp(CITY_NETS[n].id, id) == 0) return n;
    return -1;
}

static int node_by_name(int n, const wchar_t *name)
{
    for (int v = 0; v < CITY_NETS[n].nnodes; v++) if (wcscmp(CITY_NETS[n].nodes[v].name, name) == 0) return v;
    return -1;
}

static int edge_of(int n, const wchar_t *a, const wchar_t *b)
{
    int va = node_by_name(n, a), vb = node_by_name(n, b);
    for (int e = 0; e < CITY_NETS[n].nedges; e++) {
        const NetEdge *ed = &CITY_NETS[n].edges[e];
        if ((ed->a == va && ed->b == vb) || (ed->a == vb && ed->b == va)) return e;
    }
    return -1;
}

/* связность: перегоны и пересадки от первой станции */
static int connected_from(const CityNetDef *d, int start, unsigned long long edges, int withXfer)
{
    unsigned seen = 1u << start;
    for (int changed = 1; changed;) {
        changed = 0;
        for (int e = 0; e < d->nedges; e++) {
            if (!(edges >> e & 1)) continue;
            unsigned a = 1u << d->edges[e].a, b = 1u << d->edges[e].b;
            if (((seen & a) || (seen & b)) && (seen | a | b) != seen) { seen |= a | b; changed = 1; }
        }
        for (int k = 0; withXfer && k < d->nxfer; k++) {
            unsigned a = 1u << d->xfer[k][0], b = 1u << d->xfer[k][1];
            if (((seen & a) || (seen & b)) && (seen | a | b) != seen) { seen |= a | b; changed = 1; }
        }
    }
    return (int)seen;
}

static void test_data(void)
{
    CHECK(CITY_NET_COUNT <= MAX_NETS, "сетей %d больше MAX_NETS", CITY_NET_COUNT);
    static const struct { int city; int year; } HIST[] = {
        { C_MINSK, 1984 }, { C_VITEBSK, 1898 }, { C_GOMEL, 1962 }, { C_MOGILEV, 1970 }, { C_GRODNO, 1974 }, { C_BREST, 1981 } };
    for (unsigned k = 0; k < sizeof HIST / sizeof HIST[0]; k++) {
        int n = citynet_of_city(HIST[k].city);
        CHECK(n >= 0, "у областного центра %ls нет транспорта", CITIES[HIST[k].city].name);
        if (n >= 0) CHECK(CITY_NETS[n].year == HIST[k].year, "%ls: год %d, а не %d", CITY_NETS[n].title, CITY_NETS[n].year, HIST[k].year);
    }
    for (int n = 0; n < CITY_NET_COUNT; n++) {
        const CityNetDef *d = &CITY_NETS[n];
        CHECK(net_by_id(d->id) == n, "повтор id %s", d->id);
        CHECK(d->city >= 0 && d->city < CITY_COUNT, "%s: город", d->id);
        CHECK(d->nnodes >= 2 && d->nnodes <= NET_MAX_NODES && d->nnodes <= 32, "%s: узлов %d", d->id, d->nnodes);
        CHECK(d->nedges >= 1 && d->nedges <= NET_MAX_EDGES && d->nedges <= 64, "%s: перегонов %d", d->id, d->nedges);
        CHECK(d->nlines >= 1 && d->nlines <= NET_MAX_LINES, "%s: линий %d", d->id, d->nlines);
        CHECK(d->startNode >= 0 && d->startNode < d->nnodes, "%s: первая станция", d->id);
        CHECK(d->nriver >= 2 && d->nriver <= NET_MAX_RIVER && d->riverName, "%s: река", d->id);
        CHECK(!d->tech || d->techi >= 0, "%s: нет технологии %s", d->id, d->tech);
        CHECK(!d->proj || d->proji >= 0, "%s: нет стройки %s", d->id, d->proj);
        CHECK(d->goldPerEdge > 0 && d->indPerEdge > 0, "%s: цена", d->id);
        CHECK(d->desc && wcslen(d->desc) > 40 && d->title && d->kind, "%s: тексты", d->id);
        if (d->techi >= 0) CHECK(TECHS[d->techi].year <= d->year, "%s: технология позже открытия", d->id);
        if (d->proji >= 0) {
            CHECK(PROJECTS[d->proji].year == d->year && PROJECTS[d->proji].city == d->city, "%s: стройка не совпадает по году или городу", d->id);
            CHECK(d->firstEdges > 0 && d->firstEdges <= d->nedges && d->firstName, "%s: первая очередь", d->id);
        }
        for (int l = 0; l < d->nlines; l++)
            CHECK(d->lines[l].year >= d->year && d->lines[l].year <= END_YEAR && d->lines[l].name, "%s: линия %d", d->id, l);
        for (int v = 0; v < d->nnodes; v++) {
            const NetNode *a = &d->nodes[v];
            CHECK(a->name && a->name[0], "%s: узел %d без имени", d->id, v);
            CHECK(a->x >= 0 && a->x <= 1 && a->y >= 0 && a->y <= 1, "%s: «%ls» вне схемы", d->id, a->name);
            CHECK(a->year == 0 || (a->year >= d->year && a->year <= END_YEAR), "%s: год станции «%ls»", d->id, a->name);
            for (int u = 0; u < v; u++) {
                CHECK(wcscmp(d->nodes[u].name, a->name) != 0, "%s: повтор имени «%ls»", d->id, a->name);
                double dx = (a->x - d->nodes[u].x) * 1.45, dy = a->y - d->nodes[u].y;
                CHECK(sqrt(dx * dx + dy * dy) > 0.05, "%s: «%ls» и «%ls» слишком близко", d->id, a->name, d->nodes[u].name);
            }
            int deg = 0;
            for (int e = 0; e < d->nedges; e++) {
                const NetEdge *ed = &d->edges[e];
                if (ed->a != v && ed->b != v) continue;
                deg++;
                /* станция не старше своей линии */
                if (a->year) CHECK(a->year >= d->lines[ed->line].year || a->year >= d->year, "%s: «%ls» старше линии", d->id, a->name);
            }
            CHECK(deg > 0, "%s: к «%ls» не ведёт ни один перегон", d->id, a->name);
        }
        for (int e = 0; e < d->nedges; e++) {
            const NetEdge *ed = &d->edges[e];
            CHECK(ed->a < d->nnodes && ed->b < d->nnodes && ed->a != ed->b, "%s: перегон %d", d->id, e);
            CHECK(ed->line < d->nlines, "%s: перегон %d — линия", d->id, e);
            for (int f = 0; f < e; f++) {
                const NetEdge *o = &d->edges[f];
                CHECK(!((o->a == ed->a && o->b == ed->b) || (o->a == ed->b && o->b == ed->a)), "%s: повтор перегона %d", d->id, e);
            }
        }
        for (int k = 0; k < d->nxfer; k++)
            CHECK(d->xfer[k][0] < d->nnodes && d->xfer[k][1] < d->nnodes, "%s: пересадка %d", d->id, k);
        unsigned long long all = d->nedges >= 64 ? ~0ull : (1ull << d->nedges) - 1;
        CHECK(connected_from(d, d->startNode, all, 1) == (int)((d->nnodes >= 32) ? 0xFFFFFFFFu : (1u << d->nnodes) - 1),
              "%s: схема не связна", d->id);
        if (d->firstEdges) {   /* первая очередь — одна связная линия от первой станции */
            unsigned long long fm = (1ull << d->firstEdges) - 1;
            int seen = connected_from(d, d->startNode, fm, 0), want = 0;
            for (int e = 0; e < d->firstEdges; e++) want |= 1 << d->edges[e].a | 1 << d->edges[e].b;
            CHECK(seen == want, "%s: первая очередь не связана с первой станцией", d->id);
        }
    }
    /* Минское метро: 8 станций 1984 года, три линии с настоящими цветами и годами */
    int m = net_by_id("minsk_metro");
    CHECK(m >= 0 && CITY_NETS[m].proji >= 0 && strcmp(PROJECTS[CITY_NETS[m].proji].id, "minsk_metro") == 0, "метро связано со стройкой");
    if (m >= 0) {
        const CityNetDef *d = &CITY_NETS[m];
        int y84 = 0;
        for (int v = 0; v < d->nnodes; v++) y84 += d->nodes[v].year == 1984;
        CHECK(y84 == 8, "станций 1984 года %d, а не 8", y84);
        CHECK(d->nlines == 3 && d->lines[1].year == 1990 && d->lines[2].year == 2020, "годы линий метро");
        CHECK(d->firstEdges == 7, "первая очередь метро — 7 перегонов");
        for (int e = 0; e < d->firstEdges; e++)
            CHECK(d->nodes[d->edges[e].a].year == 1984 && d->nodes[d->edges[e].b].year == 1984, "первая очередь — станции 1984 года");
        CHECK(edge_of(m, L"Институт культуры", L"Площадь Ленина") >= 0 && edge_of(m, L"Парк Челюскинцев", L"Московская") >= 0, "перегоны первой линии");
        CHECK(node_by_name(m, L"Немига") >= 0 && node_by_name(m, L"Купаловская") >= 0 && node_by_name(m, L"Вокзальная") >= 0, "станции 2-й и 3-й линий");
    }
}

static void setup(Game *g, int year)
{
    game_new(g, 1, 77u);
    g->year = year;
    g->era = era_of_year(year);
    while (g->turn + 1 < TURN_COUNT && TURN_YEARS[g->turn + 1] <= year) g->turn++;
    g->gold = 100000;
    g->ind = 100000;
}

static void test_rules(void)
{
    Game g;
    int m = net_by_id("minsk_metro");
    const CityNetDef *d = &CITY_NETS[m];
    int tm = tech_index("metro");

    setup(&g, 1980);
    g.tech[tm] = 1;
    CHECK(citynet_state(&g, m) == NS_FUTURE, "в 1980 году метро ещё нет");
    setup(&g, 1984);
    CHECK(citynet_state(&g, m) == NS_LOCKED, "без технологии метро закрыто");
    CHECK(citynet_cannot_build(&g, m, 0) != NULL && !citynet_build(&g, m, 0), "без технологии не строится");
    g.tech[tm] = 1;
    CHECK(citynet_state(&g, m) == NS_AVAILABLE, "с технологией в 1984 году метро доступно");

    int eFar = edge_of(m, L"Парк Челюскинцев", L"Московская");
    int eStart = edge_of(m, L"Площадь Ленина", L"Октябрьская");
    CHECK(!citynet_can_build(&g, m, eFar), "первый перегон — только от первой станции");
    CHECK(citynet_can_build(&g, m, eStart), "перегон от «Октябрьской» можно строить");
    double gold0 = g.gold, ind0 = g.ind;
    int cg = citynet_edge_gold(&g, m, eStart), ci = citynet_edge_ind(&g, m, eStart);
    CHECK(cg > 20 && cg < 200 && ci > 10, "цена перегона %d/%d", cg, ci);
    CHECK(citynet_build(&g, m, eStart), "перегон построен");
    CHECK(fabs(g.gold - (gold0 - cg)) < 1e-6 && fabs(g.ind - (ind0 - ci)) < 1e-6, "списаны казна и производство");
    CHECK(citynet_stations(&g, m) == 2 && citynet_edges_count(&g, m) == 1, "две станции после первого перегона");
    CHECK(!citynet_build(&g, m, eStart), "повторно не строится");
    CHECK(citynet_node_station(&g, m, node_by_name(m, L"Октябрьская")), "станция «Октябрьская»");
    /* вторая линия — с 1990 года, через пересадку «Октябрьская» — «Купаловская» */
    int eL2 = edge_of(m, L"Немига", L"Купаловская");
    CHECK(!citynet_can_build(&g, m, eL2), "вторая линия в 1984 году не строится");
    CHECK(citynet_cannot_build(&g, m, eL2) && wcsstr(citynet_cannot_build(&g, m, eL2), L"1990"), "причина — год линии");
    g.year = 1991; g.era = era_of_year(1991);
    CHECK(citynet_can_build(&g, m, eL2), "в 1991 году — от пересадки");
    CHECK(!citynet_can_build(&g, m, edge_of(m, L"Пушкинская", L"Фрунзенская")), "оторванный перегон второй линии нельзя");
    /* поздняя эпоха — дороже */
    int late = citynet_edge_gold(&g, m, eL2);
    g.year = 1984; g.era = era_of_year(1984);
    CHECK(late > citynet_edge_gold(&g, m, eL2), "в эпоху VIII перегон дороже");
    /* сложность меняет цену */
    int base = citynet_edge_gold(&g, m, eFar);
    g.difficulty = 2;
    CHECK(citynet_edge_gold(&g, m, eFar) > base, "«Суровый век» дороже");
    g.difficulty = 0;
    CHECK(citynet_edge_gold(&g, m, eFar) < base, "«Летописец» дешевле");
    g.difficulty = 1;
    /* денег нет — нет перегона */
    g.gold = 1;
    int eNext = edge_of(m, L"Октябрьская", L"Площадь Победы");
    CHECK(citynet_can_build(&g, m, eNext) && !citynet_build(&g, m, eNext), "без денег не строится");
    CHECK(citynet_cannot_build(&g, m, eNext) && wcsstr(citynet_cannot_build(&g, m, eNext), L"казны"), "причина — казна");
    g.gold = 100000;

    /* вся первая очередь своими руками — стройка «Минское метро» засчитана */
    int p = d->proji;
    CHECK(!g.proj[p], "стройка ещё не построена");
    for (int guard = 0; guard < 20 && !g.proj[p]; guard++)
        for (int e = 0; e < d->firstEdges; e++) if (citynet_can_build(&g, m, e)) citynet_build(&g, m, e);
    CHECK(g.proj[p], "первая очередь достроена — стройка засчитана");
    CHECK(citynet_stations(&g, m) == 8, "8 станций первой очереди, а не %d", citynet_stations(&g, m));
    CHECK(game_proj_state(&g, p) == PS_DONE, "стройка в состоянии «построено»");
    int qm = -1;
    for (int q = 0; q < QUEST_COUNT; q++) if (strcmp(QUESTS[q].id, "q_metro") == 0) qm = q;
    CHECK(qm >= 0 && game_quest_state(&g, qm) == QS_DONE, "задание «Минское метро» выполнено");

    /* стройка из вкладки: перегоны первой очереди появляются, за свои — возврат */
    setup(&g, 1984);
    g.tech[tm] = 1;
    citynet_build(&g, m, eStart);
    int paid = citynet_edge_gold(&g, m, eStart);
    CHECK(citynet_edge_built(&g, m, eFar) == 0, "дальний перегон ещё не построен");
    CHECK(game_build(&g, p), "стройка из вкладки");
    double before = g.gold;
    int rg = 0, ri = 0;
    citynet_project_built(&g, p, &rg, &ri);
    CHECK(rg == paid && fabs(g.gold - before - rg) < 1e-6 && ri > 0, "возврат за свой перегон %d (ожидали %d)", rg, paid);
    CHECK(citynet_edge_built(&g, m, eFar) && citynet_stations(&g, m) == 8, "после стройки — 8 станций");
    CHECK(g.nets[m].nodes == citynet_station_mask(&g, m), "маска станций в сохранении");
    /* старое сохранение со стройкой, но без перегонов: линия всё равно на схеме */
    g.nets[m].edges = 0; g.nets[m].nodes = 0;
    CHECK(citynet_stations(&g, m) == 8, "стройка сама даёт первую очередь");

    /* Витебский трамвай: 1898, технология «Электрический трамвай» */
    int v = net_by_id("vitebsk_tram");
    setup(&g, 1890);
    g.tech[tech_index("tram")] = 1;
    CHECK(citynet_state(&g, v) == NS_FUTURE, "трамвай в 1890 году ещё нет");
    setup(&g, 1905);
    g.tech[tech_index("tram")] = 1;
    CHECK(citynet_state(&g, v) == NS_AVAILABLE, "трамвай в 1905 году");
    CHECK(citynet_build(&g, v, 0), "первый перегон трамвая от вокзала");
    /* Брест: 1981 год, ход 1984 */
    int b = net_by_id("brest_trolley");
    setup(&g, 1980);
    g.tech[tech_index("tram")] = 1;
    CHECK(citynet_state(&g, b) == NS_FUTURE, "брестского троллейбуса в 1980 году нет");
    setup(&g, 1984);
    g.tech[tech_index("tram")] = 1;
    CHECK(citynet_state(&g, b) == NS_AVAILABLE, "брестский троллейбус в 1984 году");
}

static void test_income(void)
{
    Game g;
    int m = net_by_id("minsk_metro");
    setup(&g, 1984);
    g.tech[tech_index("metro")] = 1;
    double out[R_COUNT];
    citynet_income(&g, out);
    for (int r = 0; r < R_COUNT; r++) CHECK(out[r] == 0, "без станций доход 0");
    Income i0, i1;
    game_income(&g, &i0);
    for (int k = 0; k < 2; k++)          /* в два прохода: первый перегон списка — не от первой станции */
        for (int e = 0; e < CITY_NETS[m].firstEdges; e++) citynet_build(&g, m, e);
    CHECK(citynet_stations(&g, m) == 8, "первая очередь построена");
    citynet_income(&g, out);
    CHECK(out[R_GOLD] > 0 && out[R_STAB] > 0 && out[R_CULT] > 0, "станции дают казну, стабильность и культуру");
    CHECK(fabs(out[R_GOLD] - CITY_NETS[m].fxPerStation.v[R_GOLD] / 10.0 * 8) < 1e-6, "казна = 8 станций × ставка");
    game_income(&g, &i1);
    CHECK(i1.cities[R_GOLD] > i0.cities[R_GOLD], "доход городов вырос");
    CHECK(i1.total[R_GOLD] > i0.total[R_GOLD], "общий доход вырос");
    /* в эпоху VIII казна станций растёт */
    double g84 = out[R_GOLD];
    g.year = 2000; g.era = era_of_year(2000);
    citynet_income(&g, out);
    CHECK(out[R_GOLD] > g84, "доход метро растёт с эпохой");
}

static void test_save(void)
{
    Game g, h;
    setup(&g, 1991);
    g.tech[tech_index("metro")] = 1;
    g.tech[tech_index("tram")] = 1;
    int m = net_by_id("minsk_metro"), go = net_by_id("gomel_trolley");
    citynet_build(&g, m, edge_of(m, L"Площадь Ленина", L"Октябрьская"));
    citynet_build(&g, m, edge_of(m, L"Октябрьская", L"Площадь Победы"));
    citynet_build(&g, m, edge_of(m, L"Институт культуры", L"Площадь Ленина"));
    citynet_build(&g, m, edge_of(m, L"Немига", L"Купаловская"));
    citynet_build(&g, go, 0);
    citynet_build(&g, go, 1);
    g.cutscenes |= CS_METRO_MINSK;
    FILE *f = tmpfile();
    CHECK(f && game_save(&g, f), "сохранение");
    if (!f) return;
    rewind(f);
    memset(&h, 0, sizeof h);
    CHECK(game_load(&h, f), "загрузка");
    fclose(f);
    for (int n = 0; n < MAX_NETS; n++)
        CHECK(h.nets[n].edges == g.nets[n].edges && h.nets[n].nodes == g.nets[n].nodes, "сеть %d после загрузки", n);
    CHECK(citynet_stations(&h, m) == citynet_stations(&g, m) && citynet_stations(&h, m) >= 4, "станции метро после загрузки");
    CHECK(citynet_stations(&h, go) == 3, "троллейбус Гомеля после загрузки");
    CHECK(h.cutscenes & CS_METRO_MINSK, "заставка 1984 года отмечена");
}

int main(void)
{
    data_init();
    test_data();
    test_rules();
    test_income();
    test_save();
    if (fails) { printf("citynet: ошибок %d\n", fails); return 1; }
    printf("citynet: схемы %d городов, правила стройки, доход и сохранение — в порядке\n", CITY_NET_COUNT);
    return 0;
}
