/* Армии на карте: граф фронта, марш, снабжение, железные дороги. Как и весь движок, не зависит от ОС. */
#include "front.h"
#include "war.h"
#include <string.h>
#include <math.h>

#define OCC_FROM 1941
#define OCC_TO   1944

FrontNode FRONT_NODES[FRONT_MAX_NODES];
int       FRONT_NODE_COUNT;
FrontEdge FRONT_EDGES[FRONT_MAX_EDGES];
int       FRONT_EDGE_COUNT;

/* ---------- узлы: города Беларуси (порядок хранится в сохранениях — только дописывать) ---------- */
enum { N_MINSK, N_POLOTSK, N_VITEBSK, N_MOGILEV, N_GOMEL, N_GRODNO, N_BREST, N_TUROV, N_NOVOGRUDOK, N_PINSK,
       N_ORSHA, N_SLUTSK, N_MOZYR, N_BOBRUISK, N_LIDA, N_BORISOV, N_MOLODECHNO, N_BARANOVICHI, N_CITIES };
#define N_REAR N_CITIES               /* сразу за городами */

/* области (REGION_CITY): 0 Брестчина, 1 Гродненщина, 2 Минщина, 3 Витебщина, 4 Могилёвщина, 5 Гомельщина */
static const struct { int city; const wchar_t *name; double lon, lat; int region; } CITY_NODES[N_CITIES] = {
    [N_MINSK]       = { C_MINSK,      0, 0, 0, 2 },
    [N_POLOTSK]     = { C_POLOTSK,    0, 0, 0, 3 },
    [N_VITEBSK]     = { C_VITEBSK,    0, 0, 0, 3 },
    [N_MOGILEV]     = { C_MOGILEV,    0, 0, 0, 4 },
    [N_GOMEL]       = { C_GOMEL,      0, 0, 0, 5 },
    [N_GRODNO]      = { C_GRODNO,     0, 0, 0, 1 },
    [N_BREST]       = { C_BREST,      0, 0, 0, 0 },
    [N_TUROV]       = { C_TUROV,      0, 0, 0, 5 },
    [N_NOVOGRUDOK]  = { C_NOVOGRUDOK, 0, 0, 0, 1 },
    [N_PINSK]       = { C_PINSK,      0, 0, 0, 0 },
    [N_ORSHA]       = { C_ORSHA,      0, 0, 0, 3 },
    [N_SLUTSK]      = { C_SLUTSK,     0, 0, 0, 2 },
    [N_MOZYR]       = { C_MOZYR,      0, 0, 0, 5 },
    [N_BOBRUISK]    = { C_BOBRUISK,   0, 0, 0, 4 },
    [N_LIDA]        = { C_LIDA,       0, 0, 0, 1 },
    [N_BORISOV]     = { C_BORISOV,    0, 0, 0, 2 },
    [N_MOLODECHNO]  = { -1, L"Молодечно",  26.85, 54.31, 2 },     /* узловые станции двух магистралей */
    [N_BARANOVICHI] = { -1, L"Барановичи", 26.01, 53.13, 0 },
};

/* исторические железные дороги (перегоны отмечены в таблице рёбер) */
enum { L_NONE, L_MB, L_LR, L_RO, L_PV, L_POL, L_VZ, L_BS };
static const wchar_t *LINE_NAMES[] = {
    0, L"Московско-Брестская", L"Либаво-Роменская", L"Риго-Орловская", L"Петербурго-Варшавская",
    L"Полесские дороги", L"Витебск — Жлобин", L"Бологое — Седлец"
};

/* Дороги между соседями. ga/gb — группа земли PROVINCES вместо узла-города; year/line — историческая
 * железная дорога на этом перегоне. Порядок хранится в сохранениях (биты Game.rails) — только дописывать. */
typedef struct { int a, b; const char *ga, *gb; int year, line; } EdgeDef;
static const EdgeDef EDGE_DEFS[] = {
    /* Беларусь */
    { N_MINSK, N_BORISOV,        0, 0, 1871, L_MB },
    { N_BORISOV, N_ORSHA,        0, 0, 1871, L_MB },
    { N_MINSK, N_BARANOVICHI,    0, 0, 1871, L_MB },
    { N_BARANOVICHI, N_BREST,    0, 0, 1871, L_MB },
    { N_MINSK, N_MOLODECHNO,     0, 0, 1873, L_LR },
    { N_MINSK, N_BOBRUISK,       0, 0, 1873, L_LR },
    { N_BOBRUISK, N_GOMEL,       0, 0, 1873, L_LR },
    { N_POLOTSK, N_VITEBSK,      0, 0, 1866, L_RO },
    { N_POLOTSK, N_MOLODECHNO,   0, 0, 1907, L_BS },
    { N_MOLODECHNO, N_LIDA,      0, 0, 1907, L_BS },
    { N_VITEBSK, N_ORSHA,        0, 0, 1902, L_VZ },
    { N_ORSHA, N_MOGILEV,        0, 0, 1902, L_VZ },
    { N_MOGILEV, N_GOMEL,        0, 0, 1902, L_VZ },
    { N_BREST, N_PINSK,          0, 0, 1882, L_POL },
    { N_BARANOVICHI, N_PINSK,    0, 0, 1884, L_POL },
    { N_PINSK, N_TUROV,          0, 0, 1886, L_POL },
    { N_TUROV, N_MOZYR,          0, 0, 1886, L_POL },
    { N_MOZYR, N_GOMEL,          0, 0, 1886, L_POL },
    { N_GRODNO, N_LIDA,          0, 0, 0, 0 },
    { N_GRODNO, N_BREST,         0, 0, 0, 0 },
    { N_LIDA, N_NOVOGRUDOK,      0, 0, 1884, L_POL },
    { N_NOVOGRUDOK, N_BARANOVICHI, 0, 0, 1884, L_POL },
    { N_BARANOVICHI, N_SLUTSK,   0, 0, 0, 0 },
    { N_MINSK, N_SLUTSK,         0, 0, 0, 0 },
    { N_SLUTSK, N_BOBRUISK,      0, 0, 0, 0 },
    { N_SLUTSK, N_TUROV,         0, 0, 0, 0 },
    { N_BOBRUISK, N_MOZYR,       0, 0, 0, 0 },
    { N_MOGILEV, N_BOBRUISK,     0, 0, 0, 0 },
    /* Большая земля — линия фронта 1941–1944 */
    { N_REAR, N_VITEBSK,         0, 0, 0, 0 },
    { N_REAR, N_ORSHA,           0, 0, 0, 0 },
    { N_REAR, N_MOGILEV,         0, 0, 0, 0 },
    { N_REAR, N_GOMEL,           0, 0, 0, 0 },
    /* земли для походов */
    { -1, N_ORSHA,      "smolensk", 0, 1870, L_MB },
    { -1, N_VITEBSK,    "smolensk", 0, 1868, L_RO },
    { -1, N_BREST,      "podlasie", 0, 0, 0 },
    { -1, N_GRODNO,     "podlasie", 0, 1862, L_PV },
    { -1, N_POLOTSK,    "latgale",  0, 1866, L_RO },
    { -1, N_BREST,      "volhynia", 0, 0, 0 },
    { -1, N_PINSK,      "volhynia", 0, 0, 0 },
    { -1, N_GOMEL,      "chernigov",0, 0, 0 },
    { -1, N_MOZYR,      "kiev",     0, 0, 0 },
    { -1, -1,           "riga", "latgale", 1861, L_RO },
    { -1, N_POLOTSK,    "riga",     0, 0, 0 },          /* Двина — главный торговый путь Полоцка */
    { -1, N_GRODNO,     "suvalkija",0, 0, 0 },
    { -1, -1,           "suvalkija", "podlasie", 0, 0 },
    { -1, N_GOMEL,      "bryansk",  0, 0, 0 },
    { -1, N_MOGILEV,    "bryansk",  0, 0, 0 },
    { -1, -1,           "bryansk", "smolensk", 0, 0 },
    { -1, -1,           "chernigov", "kiev", 0, 0 },
    { -1, -1,           "chernigov", "bryansk", 0, 0 },
    { -1, -1,           "kiev", "volhynia", 0, 0 },
};
#define EDGE_DEF_COUNT ((int)(sizeof EDGE_DEFS / sizeof EDGE_DEFS[0]))

static int s_adjN[FRONT_MAX_NODES], s_adjNode[FRONT_MAX_NODES][FRONT_MAX_ADJ], s_adjEdge[FRONT_MAX_NODES][FRONT_MAX_ADJ];
static int s_cityNode[C_COUNT_];

static double geo_km(double lon1, double lat1, double lon2, double lat2)
{
    double dx = (lon2 - lon1) * 111.32 * cos((lat1 + lat2) * 0.5 * 3.14159265358979 / 180.0), dy = (lat2 - lat1) * 111.32;
    return sqrt(dx * dx + dy * dy);
}

static int land_node(const char *group)
{
    for (int n = N_REAR + 1; n < FRONT_NODE_COUNT; n++)
        if (FRONT_NODES[n].prov >= 0 && strcmp(PROVINCES[FRONT_NODES[n].prov].group, group) == 0) return n;
    return -1;
}

static void add_edge(int a, int b, int year, const wchar_t *line)
{
    if (FRONT_EDGE_COUNT >= FRONT_MAX_EDGES) return;
    FrontEdge *e = &FRONT_EDGES[FRONT_EDGE_COUNT++];
    memset(e, 0, sizeof *e);
    e->a = -1; e->b = -1;
    if (a < 0 || b < 0 || a == b) return;           /* ребро остаётся в таблице, чтобы не сдвигать номера */
    e->a = a; e->b = b;
    e->km = (int)lround(geo_km(FRONT_NODES[a].lon, FRONT_NODES[a].lat, FRONT_NODES[b].lon, FRONT_NODES[b].lat));
    e->year = year;
    e->line = line;
    int ids[2] = { a, b };
    for (int k = 0; k < 2; k++) {
        int n = ids[k];
        if (s_adjN[n] < FRONT_MAX_ADJ) {
            s_adjNode[n][s_adjN[n]] = ids[1 - k];
            s_adjEdge[n][s_adjN[n]] = FRONT_EDGE_COUNT - 1;
            s_adjN[n]++;
        }
    }
}

void front_init(void)
{
    memset(s_adjN, 0, sizeof s_adjN);
    FRONT_NODE_COUNT = 0;
    FRONT_EDGE_COUNT = 0;
    for (int c = 0; c < C_COUNT_; c++) s_cityNode[c] = -1;
    for (int i = 0; i < N_CITIES; i++) {
        FrontNode *n = &FRONT_NODES[FRONT_NODE_COUNT++];
        int c = CITY_NODES[i].city;
        n->name = c >= 0 ? CITIES[c].name : CITY_NODES[i].name;
        n->lon = c >= 0 ? CITIES[c].lon : CITY_NODES[i].lon;
        n->lat = c >= 0 ? CITIES[c].lat : CITY_NODES[i].lat;
        n->kind = FN_CITY;
        n->city = c;
        n->region = CITY_NODES[i].region;
        n->prov = -1;
        if (c >= 0) s_cityNode[c] = i;
    }
    {   /* Большая земля: восточнее границы, за Мстиславлем */
        FrontNode *n = &FRONT_NODES[FRONT_NODE_COUNT++];
        n->name = L"Большая земля";
        n->lon = 33.10; n->lat = 54.15;
        n->kind = FN_REAR; n->city = -1; n->region = -1; n->prov = -1;
    }
    /* земли: по узлу на группу, в порядке PROVINCES (таблица только дописывается) */
    for (int p = 0; p < PROVINCE_COUNT && FRONT_NODE_COUNT < FRONT_MAX_NODES; p++) {
        if (land_node(PROVINCES[p].group) >= 0) continue;
        FrontNode *n = &FRONT_NODES[FRONT_NODE_COUNT++];
        n->name = PROVINCES[p].name;
        n->lon = PROVINCES[p].lon; n->lat = PROVINCES[p].lat;
        n->kind = FN_LAND; n->city = -1; n->region = -1; n->prov = p;
    }
    for (int i = 0; i < EDGE_DEF_COUNT; i++) {
        const EdgeDef *d = &EDGE_DEFS[i];
        int a = d->ga ? land_node(d->ga) : d->a, b = d->gb ? land_node(d->gb) : d->b;
        add_edge(a, b, d->year, d->line ? LINE_NAMES[d->line] : 0);
    }
    /* земли, которых нет в таблице дорог (дописанные позже), — к двум ближайшим городам */
    for (int n = N_REAR + 1; n < FRONT_NODE_COUNT; n++) {
        if (s_adjN[n]) continue;
        for (int k = 0; k < 2; k++) {
            int best = -1;
            double bd = 1e9;
            for (int c = 0; c < N_CITIES; c++) {
                if (front_edge(n, c) >= 0) continue;
                double d = geo_km(FRONT_NODES[n].lon, FRONT_NODES[n].lat, FRONT_NODES[c].lon, FRONT_NODES[c].lat);
                if (d < bd) { bd = d; best = c; }
            }
            add_edge(n, best, 0, 0);
        }
    }
}

void front_new_game(Game *g)
{
    for (int a = 0; a < MAX_ARMIES; a++) {
        g->armies[a].node = g->armies[a].dest = -1;
        g->armyLow[a] = 0;
        g->armyPrev[a] = -1;
    }
    g->rails[0] = g->rails[1] = 0;
    g->armyMoveTurn = -1;
    g->frontReady = 1;
}

/* ---------- граф ---------- */
int front_node_of_city(int c) { return c >= 0 && c < C_COUNT_ ? s_cityNode[c] : -1; }

int front_node_near_city(int c)
{
    if (c < 0 || c >= CITY_COUNT) return -1;
    if (s_cityNode[c] >= 0) return s_cityNode[c];
    int best = -1;
    double bd = 1e9;
    for (int n = 0; n < N_CITIES; n++) {
        double d = geo_km(CITIES[c].lon, CITIES[c].lat, FRONT_NODES[n].lon, FRONT_NODES[n].lat);
        if (d < bd) { bd = d; best = n; }
    }
    return best;
}

int front_node_of_prov(int p) { return p >= 0 && p < PROVINCE_COUNT ? land_node(PROVINCES[p].group) : -1; }
int front_node_of_region(int r) { return r >= 0 && r < REGION_COUNT ? front_node_of_city(REGION_CITY[r]) : -1; }
int front_rear_node(void) { return N_REAR; }

int front_edge(int a, int b)
{
    if (a < 0 || a >= FRONT_NODE_COUNT) return -1;
    for (int i = 0; i < s_adjN[a]; i++) if (s_adjNode[a][i] == b) return s_adjEdge[a][i];
    return -1;
}

int front_neighbors(int n, int *nodes, int *edges, int max)
{
    if (n < 0 || n >= FRONT_NODE_COUNT) return 0;
    int k = 0;
    for (int i = 0; i < s_adjN[n] && k < max; i++, k++) {
        if (nodes) nodes[k] = s_adjNode[n][i];
        if (edges) edges[k] = s_adjEdge[n][i];
    }
    return k;
}

static int in_occupation_years(const Game *g) { return g->year >= OCC_FROM && g->year <= OCC_TO; }

/* земля своя (любой эпохи той же группы) */
static int land_owned(const Game *g, int n)
{
    int p = FRONT_NODES[n].prov;
    return p >= 0 && (g->provOwned[p] || war_group_owner(g, p) >= 0);
}

int front_target_node(const Game *g)
{
    if (g->warKind == WAR_CONQUEST) return front_node_of_prov(g->warTarget);
    if (g->warKind == WAR_LIBERATION) return front_node_of_region(g->warTarget);
    return -1;
}

int front_node_open(const Game *g, int n)
{
    if (n < 0 || n >= FRONT_NODE_COUNT) return 0;
    switch (FRONT_NODES[n].kind) {
    case FN_CITY: return 1;
    case FN_REAR: return in_occupation_years(g) || g->occupied;
    default:      return land_owned(g, n) || (g->warKind == WAR_CONQUEST && front_target_node(g) == n);
    }
}

int front_node_occupied(const Game *g, int n)
{
    return n >= 0 && n < FRONT_NODE_COUNT && FRONT_NODES[n].region >= 0 && ((g->occupied >> FRONT_NODES[n].region) & 1u);
}

int front_node_visible(const Game *g, int n)
{
    if (n < 0 || n >= FRONT_NODE_COUNT) return 0;
    if (front_node_open(g, n)) return 1;
    int p = FRONT_NODES[n].prov;
    if (p < 0) return 0;
    for (int q = 0; q < PROVINCE_COUNT; q++)    /* земля, за которую сейчас можно идти в поход */
        if (strcmp(PROVINCES[q].group, PROVINCES[p].group) == 0 && war_province_state(g, q) == PV_AVAILABLE) return 1;
    return 0;
}

int front_capital(const Game *g)
{
    int c = capital_of_era(g->era);
    return front_node_of_city(c >= 0 ? c : C_MINSK);
}

int front_recruit_node(const Game *g)
{
    int cap = front_capital(g);
    if (front_node_occupied(g, cap)) return N_REAR;     /* столица под оккупацией: войска собираются на Большой земле */
    return cap;
}

/* ---------- железные дороги ---------- */
static int rail_bit(const Game *g, int e) { return e >= 0 && e < 128 && ((g->rails[e >> 6] >> (e & 63)) & 1ull); }

/* по ребру можно проложить рельсы: города Беларуси или своя земля */
static int rail_edge_ok(const Game *g, int e)
{
    if (e < 0 || e >= FRONT_EDGE_COUNT || FRONT_EDGES[e].a < 0) return 0;
    int ends[2] = { FRONT_EDGES[e].a, FRONT_EDGES[e].b };
    for (int k = 0; k < 2; k++) {
        int n = ends[k];
        if (FRONT_NODES[n].kind == FN_REAR) return 0;
        if (FRONT_NODES[n].kind == FN_LAND && !land_owned(g, n)) return 0;
    }
    return 1;
}

/* магистраль, которую прокладывает стройка PROJECTS (Московско-Брестская, Либаво-Роменская) */
static int proj_line(const Game *g, int e)
{
    const wchar_t *ln = FRONT_EDGES[e].line;
    if (!ln) return 0;
    if (ln == LINE_NAMES[L_MB] && game_has_proj(g, "moscow_brest_rw")) return 1;
    if (ln == LINE_NAMES[L_LR] && game_has_proj(g, "libava_romny_rw")) return 1;
    return 0;
}

int front_rail(const Game *g, int e)
{
    if (!rail_edge_ok(g, e)) return 0;
    return rail_bit(g, e) || proj_line(g, e);
}

int front_rail_usable(const Game *g, int e)
{
    return front_rail(g, e) && !front_node_occupied(g, FRONT_EDGES[e].a) && !front_node_occupied(g, FRONT_EDGES[e].b);
}

int front_rail_count(const Game *g)
{
    int n = 0;
    for (int e = 0; e < FRONT_EDGE_COUNT; e++) n += front_rail(g, e);
    return n;
}

int front_rail_hist(int e) { return e >= 0 && e < FRONT_EDGE_COUNT && FRONT_EDGES[e].line != 0; }

/* узлы, связанные со столицей построенными рельсами */
static void rail_net(const Game *g, int from, unsigned char *on, int usableOnly)
{
    int q[FRONT_MAX_NODES], h = 0, t = 0;
    memset(on, 0, FRONT_MAX_NODES);
    if (from < 0) return;
    on[from] = 1;
    q[t++] = from;
    while (h < t) {
        int n = q[h++];
        for (int i = 0; i < s_adjN[n]; i++) {
            int m = s_adjNode[n][i], e = s_adjEdge[n][i];
            if (on[m] || !(usableOnly ? front_rail_usable(g, e) : front_rail(g, e))) continue;
            on[m] = 1;
            q[t++] = m;
        }
    }
}

static double cost_k(const Game *g) { return 1.0 + DIFFICULTIES[g->difficulty].costPct / 100.0; }

/* цена перегона: длиннее — дороже, в поздние эпохи — дороже (как и всё остальное) */
static double rail_price(const Game *g, int e)
{
    double len = FRONT_EDGES[e].km / 110.0;
    if (len < 0.6) len = 0.6;
    if (len > 2.2) len = 2.2;
    int era = g->era > 4 ? g->era : 4;
    return len * era_scale(era) / era_scale(4) * cost_k(g);
}
int front_rail_gold(const Game *g, int e) { return e >= 0 && e < FRONT_EDGE_COUNT ? (int)lround(55 * rail_price(g, e)) : 0; }
int front_rail_ind(const Game *g, int e)  { return e >= 0 && e < FRONT_EDGE_COUNT ? (int)lround(35 * rail_price(g, e)) : 0; }

const wchar_t *front_cannot_build_rail(const Game *g, int e)
{
    if (g->over) return L"Летопись завершена.";
    if (e < 0 || e >= FRONT_EDGE_COUNT || FRONT_EDGES[e].a < 0) return L"Нет такой дороги.";
    if (!game_has_tech(g, "railway")) return L"Нужна технология «Железные дороги».";
    if (front_rail(g, e)) return L"Железная дорога здесь уже есть.";
    if (!rail_edge_ok(g, e)) return L"Строить можно только на своей земле.";
    if (front_node_occupied(g, FRONT_EDGES[e].a) || front_node_occupied(g, FRONT_EDGES[e].b)) return L"Область под оккупацией.";
    unsigned char on[FRONT_MAX_NODES];
    int cap = front_capital(g);
    rail_net(g, cap, on, 0);
    if (!on[FRONT_EDGES[e].a] && !on[FRONT_EDGES[e].b]) return L"Дорога должна продолжать сеть от столицы.";
    if (g->gold < front_rail_gold(g, e) || g->ind < front_rail_ind(g, e)) return L"Не хватает казны или производства.";
    return 0;
}

int front_build_rail(Game *g, int e)
{
    if (front_cannot_build_rail(g, e)) return 0;
    g->gold -= front_rail_gold(g, e);
    g->ind -= front_rail_ind(g, e);
    g->rails[e >> 6] |= 1ull << (e & 63);
    game_log(g, LOG_BUILD, L"Железная дорога: %ls — %ls (%d км)", FRONT_NODES[FRONT_EDGES[e].a].name,
             FRONT_NODES[FRONT_EDGES[e].b].name, FRONT_EDGES[e].km);
    return 1;
}

/* Маршруты поездов: дерево путей по рельсам от столицы (или от любого узла сети, если столица
 * без рельсов), маршрут — от корня до каждого тупика. Короткие ветки сливаются с соседними. */
int front_train_routes(const Game *g, int routes[][FRONT_MAX_NODES], int *lens, int maxRoutes)
{
    int nr = 0;
    unsigned char seen[FRONT_MAX_NODES];
    memset(seen, 0, sizeof seen);
    int cap = front_capital(g);
    for (int pass = 0; pass < FRONT_NODE_COUNT && nr < maxRoutes; pass++) {
        /* корень очередной сети: столица, затем любой ещё не охваченный узел с рельсами */
        int root = -1;
        if (pass == 0 && cap >= 0) root = cap;
        else
            for (int n = 0; n < FRONT_NODE_COUNT && root < 0; n++) {
                if (seen[n]) continue;
                for (int i = 0; i < s_adjN[n]; i++) if (front_rail(g, s_adjEdge[n][i])) { root = n; break; }
            }
        if (root < 0) break;
        if (seen[root]) continue;
        int parent[FRONT_MAX_NODES], q[FRONT_MAX_NODES], h = 0, t = 0, kids[FRONT_MAX_NODES];
        for (int n = 0; n < FRONT_MAX_NODES; n++) { parent[n] = -2; kids[n] = 0; }
        parent[root] = -1;
        seen[root] = 1;
        q[t++] = root;
        while (h < t) {
            int n = q[h++];
            for (int i = 0; i < s_adjN[n]; i++) {
                int m = s_adjNode[n][i];
                if (parent[m] != -2 || !front_rail(g, s_adjEdge[n][i])) continue;
                parent[m] = n; kids[n]++; seen[m] = 1;
                q[t++] = m;
            }
        }
        for (int i = t - 1; i >= 1 && nr < maxRoutes; i--) {
            int leaf = q[i];
            if (kids[leaf]) continue;
            int tmp[FRONT_MAX_NODES], k = 0;
            for (int n = leaf; n >= 0 && k < FRONT_MAX_NODES; n = parent[n]) tmp[k++] = n;
            for (int j = 0; j < k; j++) routes[nr][j] = tmp[k - 1 - j];
            lens[nr++] = k;
        }
    }
    return nr;
}

/* ---------- путь ---------- */
static int edge_cost(const Game *g, int e) { return front_rail_usable(g, e) ? 1 : 3; }

/* бюджет марша за ход t: 3 — одна дорога или три перегона железной дороги; в долгие ходы армия
 * успевает больше: ход в 12–23 года — две дороги, в 24 года и больше (средние века) — три */
int front_turn_budget(int t)
{
    if (t < 0) t = 0;
    if (t >= TURN_COUNT) return 3;
    int span = (t + 1 < TURN_COUNT ? TURN_YEARS[t + 1] : END_YEAR) - TURN_YEARS[t];
    int k = 1 + span / 12;
    return 3 * (k > 3 ? 3 : k);
}

int front_path(const Game *g, int from, int to, int *path, int max)
{
    if (from < 0 || to < 0 || from >= FRONT_NODE_COUNT || to >= FRONT_NODE_COUNT) return 0;
    int dist[FRONT_MAX_NODES], prev[FRONT_MAX_NODES];
    unsigned char done[FRONT_MAX_NODES];
    for (int n = 0; n < FRONT_NODE_COUNT; n++) { dist[n] = 1 << 28; prev[n] = -1; done[n] = 0; }
    dist[from] = 0;
    for (;;) {
        int u = -1;
        for (int n = 0; n < FRONT_NODE_COUNT; n++) if (!done[n] && dist[n] < (1 << 28) && (u < 0 || dist[n] < dist[u])) u = n;
        if (u < 0 || u == to) break;
        done[u] = 1;
        if (u != from && !front_node_open(g, u)) continue;      /* через чужую землю не пройти */
        for (int i = 0; i < s_adjN[u]; i++) {
            int v = s_adjNode[u][i];
            if (v != to && !front_node_open(g, v)) continue;
            /* при равной цене — меньше перегонов, затем короче по километрам */
            int c = dist[u] + edge_cost(g, s_adjEdge[u][i]) * 4096 + FRONT_EDGES[s_adjEdge[u][i]].km;
            if (c < dist[v]) { dist[v] = c; prev[v] = u; }
        }
    }
    if (dist[to] >= (1 << 28)) return 0;
    int tmp[FRONT_MAX_NODES], k = 0;
    for (int n = to; n >= 0 && k < FRONT_MAX_NODES; n = prev[n]) tmp[k++] = n;
    if (k > max) return 0;
    for (int i = 0; i < k; i++) path[i] = tmp[k - 1 - i];
    return k;
}

/* сколько узлов пути армия пройдёт за один ход с бюджетом budget, начиная с узла i */
static int turn_steps(const Game *g, const int *path, int n, int i, int budget)
{
    int k = 0;
    while (i + k < n - 1) {
        int c = edge_cost(g, front_edge(path[i + k], path[i + k + 1]));
        if (c > budget) break;
        budget -= c;
        k++;
    }
    return k > 0 ? k : 1;
}

int front_eta(const Game *g, int from, int to)
{
    if (from == to) return 0;
    int path[FRONT_MAX_NODES];
    int n = front_path(g, from, to, path, FRONT_MAX_NODES);
    if (n < 2) return -1;
    int turns = 0;
    for (int i = 0; i < n - 1; turns++) i += turn_steps(g, path, n, i, front_turn_budget(g->turn + turns));
    return turns;
}

/* ---------- снабжение ---------- */
/* Источники: столица, крупные города и города со стройками (склады и рынки), свои земли, в 1941–1944 годах — Большая
 * земля и центры освобождённых областей; станции железной дороги, связанные с источником, — тоже
 * источники. Дальше — до двух дорог. Через оккупированный город снабжение не проходит (он сам может
 * быть концом пути), оккупированные города не снабжают. */
static void supply_map(const Game *g, unsigned char *ok, int extra)   /* extra — узел, открытый для проверки */
{
    int dist[FRONT_MAX_NODES], q[FRONT_MAX_NODES * 2], h = 0, t = 0;
    unsigned char src[FRONT_MAX_NODES];
    memset(src, 0, sizeof src);
    int cap = front_capital(g);
    if (cap >= 0 && !front_node_occupied(g, cap)) src[cap] = 1;
    if (g->occupied || in_occupation_years(g)) {
        src[N_REAR] = 1;
        for (int r = 0; r < REGION_COUNT; r++)
            if (!((g->occupied >> r) & 1u)) { int n = front_node_of_region(r); if (n >= 0) src[n] = 1; }
    }
    for (int n = 0; n < N_CITIES; n++) {
        int c = FRONT_NODES[n].city;
        if (c >= 0 && !front_node_occupied(g, n) && (CITIES[c].major || city_level(g, c) > 0)) src[n] = 1;
    }
    for (int n = N_REAR + 1; n < FRONT_NODE_COUNT; n++) if (land_owned(g, n)) src[n] = 1;
    /* станции: всё, что связано с источником рельсами */
    unsigned char on[FRONT_MAX_NODES];
    for (int n = 0; n < FRONT_NODE_COUNT; n++)
        if (src[n] == 1) {
            rail_net(g, n, on, 1);
            for (int m = 0; m < FRONT_NODE_COUNT; m++) if (on[m] && !src[m]) src[m] = 2;
        }
    for (int n = 0; n < FRONT_NODE_COUNT; n++) {
        dist[n] = src[n] ? 0 : 99;
        if (src[n]) q[t++] = n;
    }
    while (h < t) {
        int n = q[h++];
        if (dist[n] >= FRONT_SUPPLY_RANGE) continue;
        if (!src[n] && front_node_occupied(g, n)) continue;
        for (int i = 0; i < s_adjN[n]; i++) {
            int m = s_adjNode[n][i];
            if (dist[m] <= dist[n] + 1 || (m != extra && !front_node_open(g, m))) continue;
            dist[m] = dist[n] + 1;
            if (t < FRONT_MAX_NODES * 2) q[t++] = m;
        }
    }
    for (int n = 0; n < FRONT_NODE_COUNT; n++) ok[n] = dist[n] <= FRONT_SUPPLY_RANGE;
}

/* была бы снабжена армия на узле n (чужая земля считается целью похода) */
int front_supplied_at(const Game *g, int n)
{
    if (n < 0 || n >= FRONT_NODE_COUNT) return 0;
    unsigned char ok[FRONT_MAX_NODES];
    supply_map(g, ok, n);
    return ok[n];
}

static int unit_partisans(int u) { return strcmp(UNIT_TYPES[u].id, "partisans") == 0; }

int front_army_alive(const Game *g, int a)
{
    return a >= 0 && a < MAX_ARMIES && g->armies[a].node >= 0 && g->armies[a].node < FRONT_NODE_COUNT && front_army_size(g, a) > 0;
}

int front_army_size(const Game *g, int a)
{
    if (a < 0 || a >= MAX_ARMIES || g->armies[a].node < 0) return 0;
    int n = 0;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) n += g->armies[a].units[u] > 0 ? g->armies[a].units[u] : 0;
    return n;
}

int front_army_count(const Game *g)
{
    int n = 0;
    for (int a = 0; a < MAX_ARMIES; a++) n += front_army_alive(g, a);
    return n;
}

int front_lead_unit(const Game *g, int a)
{
    int best = -1;
    double bv = 0;
    if (a < 0 || a >= MAX_ARMIES) return -1;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++)
        if (g->armies[a].units[u] > 0) {
            double v = g->armies[a].units[u] * UNIT_TYPES[u].atk * war_unit_eff(g, u);
            if (v > bv) { bv = v; best = u; }
        }
    return best;
}

/* без снабжения здесь армия ещё и теряет людей: чужая земля или оккупированная область */
int front_attrition_at(const Game *g, int n)
{
    return n >= 0 && n < FRONT_NODE_COUNT && (FRONT_NODES[n].kind == FN_LAND || front_node_occupied(g, n));
}

int front_army_supplied(const Game *g, int a)
{
    if (!front_army_alive(g, a)) return 1;
    if (front_supplied_at(g, g->armies[a].node)) return 1;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++)
        if (g->armies[a].units[u] > 0 && !unit_partisans(u)) return 0;
    return 1;                                     /* одни партизаны: живут лесом */
}

double front_army_attack(const Game *g, int a, int kind, int node)
{
    if (!front_army_alive(g, a)) return 0;
    if (node < 0) node = g->armies[a].node;
    int sup = front_supplied_at(g, node);
    double tm = war_tech_mult(g), atk = 0;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) {
        int n = g->armies[a].units[u];
        if (n <= 0) continue;
        int par = unit_partisans(u);
        double k = war_unit_eff(g, u) * tm * (sup || par ? 1.0 : FRONT_UNSUPPLIED);
        atk += n * UNIT_TYPES[u].atk * k;
        if (par && kind == WAR_LIBERATION) atk += 0.5 * n * UNIT_TYPES[u].atk * k;   /* партизаны знают каждую тропу */
    }
    return atk;
}

double front_army_defense(const Game *g, int a)
{
    if (!front_army_alive(g, a)) return 0;
    int sup = front_supplied_at(g, g->armies[a].node);
    double tm = war_tech_mult(g), def = 0;
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) {
        int n = g->armies[a].units[u];
        if (n > 0) def += n * UNIT_TYPES[u].def * war_unit_eff(g, u) * tm * (sup || unit_partisans(u) ? 1.0 : FRONT_UNSUPPLIED);
    }
    return def;
}

unsigned front_armies_at(const Game *g, int node)
{
    unsigned m = 0;
    for (int a = 0; a < MAX_ARMIES; a++) if (front_army_alive(g, a) && g->armies[a].node == node) m |= 1u << a;
    return m;
}

/* армии в узле — полностью, в соседних — на три четверти, дальние — наполовину (гарнизоны и гонцы) */
double front_defense_at(const Game *g, int node)
{
    if (node < 0) return 0;
    double d = 0;
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(g, a)) continue;
        int n = g->armies[a].node;
        d += front_army_defense(g, a) * (n == node ? 1.0 : front_edge(n, node) >= 0 ? 0.75 : 0.5);
    }
    return d;
}

int front_strongest(const Game *g)
{
    int best = -1;
    double bv = -1;
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(g, a)) continue;
        double v = front_army_attack(g, a, WAR_CONQUEST, -1);
        if (v > bv) { bv = v; best = a; }
    }
    return best;
}

int front_army_for(const Game *g, int node)
{
    int best = -1;
    double bv = -1;
    for (int a = 0; a < MAX_ARMIES; a++)
        if (front_army_alive(g, a) && (g->armies[a].node == node || g->armies[a].dest == node)) {
            double v = front_army_attack(g, a, WAR_CONQUEST, -1);
            if (v > bv) { bv = v; best = a; }
        }
    if (best >= 0) return best;
    /* иначе — самая сильная армия, которая может дойти */
    for (int a = 0; a < MAX_ARMIES; a++)
        if (front_army_alive(g, a) && front_eta(g, g->armies[a].node, node) >= 0) {
            double v = front_army_attack(g, a, WAR_CONQUEST, -1);
            if (v > bv) { bv = v; best = a; }
        }
    return best;
}

/* ---------- приказы ---------- */
const wchar_t *front_cannot_send(const Game *g, int a, int node)
{
    if (g->over) return L"Летопись завершена.";
    if (!front_army_alive(g, a)) return L"Такой армии нет.";
    if (node < 0 || node >= FRONT_NODE_COUNT) return L"Нет такого места.";
    if (g->armies[a].node == node) return L"Армия уже здесь.";
    if (!front_node_open(g, node)) {
        if (FRONT_NODES[node].kind == FN_REAR) return L"Большая земля — тыл фронта только в 1941–1944 годах.";
        if (front_node_visible(g, node)) return L"Это чужая земля: сначала объявите поход на неё.";
        return L"Это земля соседей: поход туда сейчас невозможен.";
    }
    if (front_eta(g, g->armies[a].node, node) < 0) return L"Туда нет дороги через свои земли.";
    return 0;
}

int front_send(Game *g, int a, int node)
{
    if (front_cannot_send(g, a, node)) return 0;
    g->armies[a].dest = node;
    return 1;
}

/* Война идёт сама: если к цели не идёт ни одна армия, к ней отправляется самая сильная армия без
 * приказа, которая может дойти. Армии с приказами игрока не трогаем. Возвращает армию или -1. */
int front_auto_target(Game *g)
{
    int tn = front_target_node(g);
    if (tn < 0 || g->over) return -1;
    for (int a = 0; a < MAX_ARMIES; a++)
        if (front_army_alive(g, a) && (g->armies[a].node == tn || g->armies[a].dest == tn)) return -1;
    int best = -1;
    double bv = -1;
    for (int a = 0; a < MAX_ARMIES; a++)
        if (front_army_alive(g, a) && g->armies[a].dest < 0 && !front_cannot_send(g, a, tn)) {
            double v = front_army_attack(g, a, g->warKind, tn);
            if (v > bv) { bv = v; best = a; }
        }
    if (best >= 0) g->armies[best].dest = tn;
    return best;
}

int front_halt(Game *g, int a)
{
    if (!front_army_alive(g, a) || g->armies[a].dest < 0) return 0;
    g->armies[a].dest = -1;
    return 1;
}

static int free_slot(const Game *g)
{
    for (int a = 0; a < MAX_ARMIES; a++) if (!front_army_alive(g, a)) return a;
    return -1;
}

static void clear_army(Game *g, int a)
{
    memset(&g->armies[a], 0, sizeof g->armies[a]);
    g->armies[a].node = g->armies[a].dest = -1;
    g->armyLow[a] = 0;
    g->armyPrev[a] = -1;
}

int front_split(Game *g, int a)
{
    if (g->over || !front_army_alive(g, a) || front_army_size(g, a) < 2) return -1;
    int b = free_slot(g);
    if (b < 0) return -1;
    clear_army(g, b);
    g->armies[b].node = g->armies[a].node;
    int moved = 0, total = front_army_size(g, a);
    /* половина каждого вида; нечётные остатки — по очереди, пока не наберётся половина */
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) {
        int k = g->armies[a].units[u] / 2;
        g->armies[a].units[u] -= k;
        g->armies[b].units[u] += k;
        moved += k;
    }
    for (int u = UNIT_COUNT - 1; u >= 0 && moved < total / 2; u--)
        if (u < MAX_UNITS && g->armies[a].units[u] > 0 && g->armies[a].units[u] % 2 == 1 && front_army_size(g, a) > 1) {
            g->armies[a].units[u]--; g->armies[b].units[u]++; moved++;
        }
    g->armyLow[b] = g->armyLow[a];
    return b;
}

int front_merge(Game *g, int a)
{
    if (g->over || !front_army_alive(g, a)) return 0;
    int n = 0;
    for (int b = 0; b < MAX_ARMIES; b++) {
        if (b == a || !front_army_alive(g, b) || g->armies[b].node != g->armies[a].node) continue;
        for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) g->armies[a].units[u] += g->armies[b].units[u];
        clear_army(g, b);
        n++;
    }
    return n;
}

/* ---------- согласование с войском g->units ---------- */
static int path_len(const Game *g, int from, int to)
{
    int path[FRONT_MAX_NODES];
    int n = front_path(g, from, to, path, FRONT_MAX_NODES);
    return n > 0 ? n - 1 : 999;
}

static int army_for_recruit(Game *g)
{
    int node = front_recruit_node(g);
    for (int a = 0; a < MAX_ARMIES; a++) if (front_army_alive(g, a) && g->armies[a].node == node) return a;
    int b = free_slot(g);
    if (b >= 0) { clear_army(g, b); g->armies[b].node = node; return b; }
    /* все четыре армии далеко: пополнение идёт к ближайшей */
    int best = 0, bl = 1 << 20;
    for (int a = 0; a < MAX_ARMIES; a++) {
        int l = path_len(g, node, g->armies[a].node);
        if (l < bl) { bl = l; best = a; }
    }
    return best;
}

void front_add_unit(Game *g, int u)
{
    if (u < 0 || u >= MAX_UNITS) return;
    int a = army_for_recruit(g);
    g->armies[a].units[u]++;
}

void front_remove_unit(Game *g, int u)
{
    if (u < 0 || u >= MAX_UNITS) return;
    int cap = front_capital(g), best = -1;
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(g, a) || g->armies[a].units[u] <= 0) continue;
        if (best < 0 || (g->armies[a].node == cap && g->armies[best].node != cap) ||
            ((g->armies[a].node == cap) == (g->armies[best].node == cap) && g->armies[a].units[u] > g->armies[best].units[u])) best = a;
    }
    if (best >= 0) g->armies[best].units[u]--;
}

/* ближайший открытый узел (по дорогам, через любые узлы) */
static int nearest_open(const Game *g, int from)
{
    int q[FRONT_MAX_NODES], h = 0, t = 0;
    unsigned char seen[FRONT_MAX_NODES];
    memset(seen, 0, sizeof seen);
    seen[from] = 1;
    q[t++] = from;
    while (h < t) {
        int n = q[h++];
        if (front_node_open(g, n) && FRONT_NODES[n].kind == FN_CITY) return n;
        for (int i = 0; i < s_adjN[n]; i++) if (!seen[s_adjNode[n][i]]) { seen[s_adjNode[n][i]] = 1; q[t++] = s_adjNode[n][i]; }
    }
    return front_capital(g);
}

void front_sync(Game *g)
{
    if (!g->frontReady) {   /* старое сохранение: земли после 1795 года снова свои, армий ещё нет */
        for (int p = 0; p < PROVINCE_COUNT && p < MAX_PROVINCES; p++) if (g->provOwned[p] == 2) g->provOwned[p] = 1;
        for (int a = 0; a < MAX_ARMIES; a++) { clear_army(g, a); }
        g->armyMoveTurn = -1;
        g->frontReady = 1;
    }
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (g->armies[a].node >= FRONT_NODE_COUNT || g->armies[a].node < -1) g->armies[a].node = -1;
        if (g->armies[a].dest >= FRONT_NODE_COUNT || g->armies[a].dest < -1) g->armies[a].dest = -1;
        if (g->armies[a].node < 0) memset(g->armies[a].units, 0, sizeof g->armies[a].units);
        for (int u = 0; u < MAX_UNITS; u++) {
            if (u >= UNIT_COUNT || g->armies[a].units[u] < 0) g->armies[a].units[u] = 0;
        }
    }
    /* число соединений каждого вида — как в войске */
    for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++) {
        int have = g->units[u] > 0 ? g->units[u] : 0, sum = 0;
        for (int a = 0; a < MAX_ARMIES; a++) sum += g->armies[a].units[u];
        for (; sum > have; sum--) {
            int best = -1;
            for (int a = 0; a < MAX_ARMIES; a++)
                if (g->armies[a].units[u] > 0 && (best < 0 || g->armies[a].units[u] > g->armies[best].units[u])) best = a;
            g->armies[best].units[u]--;
        }
        for (; sum < have; sum++) front_add_unit(g, u);
    }
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (g->armies[a].node < 0) continue;
        if (front_army_size(g, a) == 0) { clear_army(g, a); continue; }
        /* земля стала чужой (мир, поражение, конец 1944 года): армия отходит на свой узел */
        if (!front_node_open(g, g->armies[a].node)) {
            g->armies[a].node = nearest_open(g, g->armies[a].node);
            g->armyPrev[a] = -1;
        }
        int d = g->armies[a].dest;
        if (d >= 0 && (d == g->armies[a].node || !front_node_open(g, d))) g->armies[a].dest = -1;
    }
}

int front_retreat_east(Game *g)
{
    int n = 0;
    front_sync(g);
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(g, a)) continue;
        if (g->armies[a].node != N_REAR) { g->armyPrev[a] = (signed char)g->armies[a].node; n++; }
        g->armies[a].node = N_REAR;
        g->armies[a].dest = -1;
        g->armyLow[a] = 0;
    }
    if (n) g->armyMoveTurn = g->turn;
    return n;
}

/* ---------- конец хода ---------- */
static void cat_news(wchar_t *news, int n, const wchar_t *s)
{
    size_t l = wcslen(news);
    if (l && l + 1 < (size_t)n) { news[l++] = L' '; news[l] = 0; }
    for (size_t i = 0; s[i] && l + 1 < (size_t)n; i++) news[l++] = s[i];
    news[l] = 0;
}

void front_end_turn(Game *g, wchar_t *news, int n)
{
    if (news && n > 0) news[0] = 0;
    front_sync(g);
    int moved = 0;
    for (int a = 0; a < MAX_ARMIES; a++) g->armyPrev[a] = -1;
    {   /* идёт война, а к цели никто не идёт — свободная армия выступает сама */
        int a = front_auto_target(g);
        if (a >= 0 && news) {
            wchar_t s[160];
            swprintf(s, 160, L"Армия %d сама выступила к цели: %ls.", a + 1, FRONT_NODES[g->armies[a].dest].name);
            cat_news(news, n, s);
        }
    }
    /* марш */
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(g, a) || g->armies[a].dest < 0) continue;
        int path[FRONT_MAX_NODES], from = g->armies[a].node, dest = g->armies[a].dest;
        int len = front_path(g, from, dest, path, FRONT_MAX_NODES);
        wchar_t s[160];
        if (len < 2) {
            g->armies[a].dest = -1;
            swprintf(s, 160, L"Армия %d остановилась: дорога к цели закрыта.", a + 1);
            if (news) cat_news(news, n, s);
            continue;
        }
        int k = turn_steps(g, path, len, 0, front_turn_budget(g->turn));
        g->armies[a].node = path[k];
        g->armyPrev[a] = (signed char)from;
        moved = 1;
        if (path[k] == dest) {
            g->armies[a].dest = -1;
            swprintf(s, 160, L"Армия %d пришла: %ls.", a + 1, FRONT_NODES[dest].name);
        } else {
            int left = front_eta(g, path[k], dest);
            swprintf(s, 160, L"Армия %d идёт: %ls → %ls, ещё %d %ls.", a + 1, FRONT_NODES[path[k]].name, FRONT_NODES[dest].name,
                     left, left == 1 ? L"ход" : left < 5 ? L"хода" : L"ходов");
        }
        if (news) cat_news(news, n, s);
    }
    g->armyMoveTurn = moved ? g->turn + 1 : -1;
    /* снабжение: без него армия слабее, а на чужой земле и в тылу врага каждый второй ход теряет
     * соединение (партизанам снабжение не нужно) */
    for (int a = 0; a < MAX_ARMIES; a++) {
        if (!front_army_alive(g, a)) continue;
        if (front_army_supplied(g, a)) { g->armyLow[a] = 0; continue; }
        if (g->armyLow[a] < 250) g->armyLow[a]++;
        int node = g->armies[a].node, hard = front_attrition_at(g, node);
        wchar_t s[200];
        s[0] = 0;
        if (hard && g->armyLow[a] % 2 == 0) {
            int worst = -1;
            for (int u = 0; u < UNIT_COUNT && u < MAX_UNITS; u++)
                if (g->armies[a].units[u] > 0 && !unit_partisans(u) && (worst < 0 || g->armies[a].units[u] > g->armies[a].units[worst])) worst = u;
            if (worst >= 0) {
                g->armies[a].units[worst]--;
                g->units[worst]--;
                g->unitsLost++;
                swprintf(s, 200, L"Армия %d без снабжения (%ls): потеряно соединение «%ls».", a + 1,
                         FRONT_NODES[node].name, UNIT_TYPES[worst].name);
                game_log(g, LOG_WAR, L"%ls", s);
            }
        } else if (hard) swprintf(s, 200, L"Армия %d без снабжения (%ls): сила ×0,6, через ход начнутся потери.", a + 1, FRONT_NODES[node].name);
        else if (g->armyLow[a] == 1) swprintf(s, 200, L"Армия %d далеко от складов (%ls): сила ×0,6.", a + 1, FRONT_NODES[node].name);
        if (news && s[0]) cat_news(news, n, s);
    }
    front_sync(g);
}
