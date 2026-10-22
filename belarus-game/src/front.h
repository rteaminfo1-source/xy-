/* front.h — армии на карте в духе HOI: игрок сам двигает войска по узлам фронта, строит железные
 * дороги снабжения, по ним ходят поезда. Движок переносимый (без windows.h); интерфейс — ui_front.c.
 *
 * Узлы фронта — города Беларуси (и узловые станции Молодечно, Барановичи), по одному узлу на каждую
 * землю для походов (группу PROVINCES) и «Большая земля» на востоке (только в 1941–1944 годах).
 * Рёбра — дороги между соседями. Номера узлов и рёбер хранятся в сохранениях (Game.armies, Game.rails):
 * таблицы только дописываются в конец.
 *
 * Правила для игрока:
 *  - армия за ход проходит одну дорогу или до трёх перегонов по своей железной дороге, а в долгие
 *    ходы — больше: в ход длиной 12–23 года вдвое, в 24 года и больше (средние века) втрое;
 *  - бой похода идёт, только когда армия стоит на земле-цели; освобождение 1941–1944 — когда армия
 *    стоит в центре области; набеги и осады отбивают армии в городе (полностью), в соседних (на три
 *    четверти) и дальние (наполовину) вместе с крепостями;
 *  - снабжение: не дальше двух дорог от столицы, крупного города или города со стройками, своей земли
 *    или станции железной дороги, связанной с ними; в 1941–1944 годах — от Большой земли на востоке и освобождённых областей,
 *    а через оккупированные города снабжение не проходит. Без снабжения сила ×0,6, а на чужой земле
 *    и в оккупированной области каждый второй ход теряется соединение; партизанам снабжение не нужно. */
#ifndef BY_FRONT_H
#define BY_FRONT_H

#include "game.h"

#define FRONT_MAX_NODES 40
#define FRONT_MAX_EDGES 96          /* не больше 128: Game.rails — 128 бит */
#define FRONT_MAX_ADJ   10
#define FRONT_SUPPLY_RANGE 2        /* дорог от источника снабжения */
#define FRONT_UNSUPPLIED 0.6        /* множитель силы без снабжения */

enum { FN_CITY, FN_REAR, FN_LAND };   /* город Беларуси, Большая земля (1941–1944), земля для походов */

typedef struct {
    const wchar_t *name;
    double lon, lat;
    int kind;                       /* FN_* */
    int city;                       /* C_* или -1 (узловая станция, земля) */
    int region;                     /* область REGION_* для оккупации или -1 */
    int prov;                       /* FN_LAND: первая земля группы в PROVINCES, иначе -1 */
} FrontNode;

typedef struct {
    int a, b;                       /* узлы; a < 0 — ребро не используется (нет такой земли) */
    int km;                         /* длина дороги */
    int year;                       /* год исторической железной дороги на этом перегоне (0 — нет) */
    const wchar_t *line;            /* её название или 0 */
} FrontEdge;

extern FrontNode FRONT_NODES[FRONT_MAX_NODES];
extern int       FRONT_NODE_COUNT;
extern FrontEdge FRONT_EDGES[FRONT_MAX_EDGES];
extern int       FRONT_EDGE_COUNT;

void front_init(void);              /* из data_init(): граф фронта */
void front_new_game(Game *g);       /* из game_new(): армий на карте ещё нет */

/* ---------- граф ---------- */
int  front_node_of_city(int c);                 /* узел города или -1 */
int  front_node_near_city(int c);               /* узел города или ближайший к нему */
int  front_node_of_prov(int p);                 /* узел земли PROVINCES[p] */
int  front_node_of_region(int r);               /* центр области */
int  front_rear_node(void);                     /* Большая земля */
int  front_edge(int a, int b);                  /* ребро между узлами или -1 */
int  front_neighbors(int n, int *nodes, int *edges, int max);
int  front_node_open(const Game *g, int n);     /* можно войти: Беларусь, свои земли, цель похода, Большая земля в 1941–1944 */
int  front_node_occupied(const Game *g, int n); /* город в оккупированной области */
int  front_node_visible(const Game *g, int n);  /* показывать на карте */
int  front_capital(const Game *g);              /* столица эпохи (Минск, если столицы нет) */
int  front_recruit_node(const Game *g);         /* где собираются новые войска */
int  front_target_node(const Game *g);          /* узел цели идущей войны или -1 */

/* путь по открытым узлам (to разрешён всегда): узлы с обоих концов, их число; 0 — пути нет */
int  front_path(const Game *g, int from, int to, int *path, int max);
int  front_eta(const Game *g, int from, int to);   /* ходов до цели: 0 — уже там, -1 — не дойти */
int  front_turn_budget(int t);                     /* бюджет марша в ход t: дорога — 3, перегон по рельсам — 1 */

/* ---------- железные дороги ---------- */
int  front_rail(const Game *g, int e);          /* построена (сама или стройкой магистрали) */
int  front_rail_usable(const Game *g, int e);   /* построена и не в оккупированной области */
int  front_rail_count(const Game *g);
int  front_rail_hist(int e);                    /* перегон исторической магистрали */
const wchar_t *front_cannot_build_rail(const Game *g, int e);   /* NULL — можно */
int  front_rail_gold(const Game *g, int e);
int  front_rail_ind(const Game *g, int e);
int  front_build_rail(Game *g, int e);
/* маршруты поездов: от столицы по рельсам до тупиков; узлы маршрута r — в nodes, вернёт их число */
int  front_train_routes(const Game *g, int routes[][FRONT_MAX_NODES], int *lens, int maxRoutes);

/* ---------- армии ---------- */
int  front_army_alive(const Game *g, int a);
int  front_army_size(const Game *g, int a);
int  front_army_count(const Game *g);
int  front_lead_unit(const Game *g, int a);     /* самый сильный вид войск армии или -1 */
int  front_supplied_at(const Game *g, int n);   /* узел в зоне снабжения */
int  front_army_supplied(const Game *g, int a); /* снабжена (или одни партизаны) */
int  front_attrition_at(const Game *g, int n);  /* без снабжения здесь теряются соединения */
/* сила армии a, если она стоит на узле node (node < 0 — там, где стоит): с учётом снабжения */
double front_army_attack(const Game *g, int a, int kind, int node);
double front_army_defense(const Game *g, int a);
unsigned front_armies_at(const Game *g, int node);   /* битовая маска армий на узле */
double front_defense_at(const Game *g, int node);    /* армии на узле, соседние ×0,75, дальние ×0,5 */
int  front_army_for(const Game *g, int node);
int  front_auto_target(Game *g);
/* реальное время: армия с приказом проходит одну клетку (дорогу) за FRONT_STEP_MS, по своей железной
 * дороге — втрое быстрее. front_step сдвигает её на следующий узел пути (1 — сдвинулась). */
#define FRONT_STEP_MS 10000
int  front_next_node(const Game *g, int a);     /* следующий узел пути армии или -1 */
int  front_step_ms(const Game *g, int a);       /* сколько миллисекунд идти до него */
int  front_step(Game *g, int a);                /* война идёт сама: свободная армия выступает к цели, -1 — некому */   /* армия на узле или идущая к нему, иначе самая сильная; -1 */
int  front_strongest(const Game *g);

const wchar_t *front_cannot_send(const Game *g, int a, int node);   /* NULL — приказ можно отдать */
int  front_send(Game *g, int a, int node);
int  front_halt(Game *g, int a);
int  front_split(Game *g, int a);               /* новая армия из половины войск или -1 */
int  front_merge(Game *g, int a);               /* собрать в a все армии с её узла; сколько влилось */

/* аргумент кнопок армий (A_ARMY_SPLIT в интерфейсе): армия a и действие FA_* */
enum { FA_SPLIT, FA_MERGE, FA_HALT, FA_HOME, FA_TARGET };
#define FRONT_ARG(a, op) ((op) * 16 + (a))

/* армии = войско g->units: новые войска — к армии у столицы, лишние убираются; армии на чужой земле
 * отходят на ближайший свой узел. Идемпотентна: можно звать в любой момент. */
void front_sync(Game *g);
void front_add_unit(Game *g, int u);            /* набрано соединение (g->units уже увеличено) */
void front_remove_unit(Game *g, int u);         /* распущено соединение (g->units уже уменьшено) */
/* конец хода (из war_end_turn): марш, снабжение, потери без снабжения; news — сводка для игрока */
void front_end_turn(Game *g, wchar_t *news, int n);
/* 22 июня 1941 года: армии отступают на восток, к Большой земле; вернёт, сколько армий отошло */
int  front_retreat_east(Game *g);

#endif
