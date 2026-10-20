/* citynet.h — городской транспорт, который игрок строит сам: метро в Минске (1984) и похожие
 * «интерактивы» в областных городах (трамвай, троллейбус…). Схема города — узлы-районы и
 * возможные перегоны между ними; игрок строит перегоны, станции появляются на концах.
 * Движок переносимый (без windows.h); интерфейс — ui_city.c. */
#ifndef BY_CITYNET_H
#define BY_CITYNET_H

#include "game.h"

#define NET_MAX_NODES 24
#define NET_MAX_EDGES 48
#define NET_MAX_RIVER 16

typedef struct { const wchar_t *name; float x, y; int popK; } NetNode;   /* x, y — 0..1 на схеме, popK — жителей района, тыс. */
typedef struct { unsigned char a, b; } NetEdge;

typedef struct {
    const char    *id;          /* "minsk_metro" */
    int            city;        /* C_* */
    const wchar_t *kind;        /* «Метрополитен», «Трамвай», «Троллейбус» */
    const wchar_t *title;       /* «Минское метро» */
    int            year;        /* с какого года можно строить (историческая дата открытия) */
    const char    *tech;        /* нужная технология или NULL */
    unsigned       color;       /* 0xRRGGBB — цвет линий */
    int            nnodes, nedges;
    NetNode        nodes[NET_MAX_NODES];
    NetEdge        edges[NET_MAX_EDGES];
    int            startNode;   /* первая станция (центр) */
    int            nriver;
    float          river[NET_MAX_RIVER][2];   /* река на схеме, 0..1 */
    int            goldPerEdge, indPerEdge;   /* стоимость перегона (до множителя сложности) */
    Fx             fxPerStation;              /* доход за ход за каждую станцию */
    const wchar_t *desc;        /* историческая справка */
    int            techi;       /* заполняется citynet_init() */
} CityNetDef;

extern CityNetDef CITY_NETS[];
extern const int  CITY_NET_COUNT;

enum { NS_FUTURE, NS_LOCKED, NS_AVAILABLE };

void citynet_init(void);                                  /* зовёт data_init(): индексы технологий */
int  citynet_of_city(int c);                              /* индекс сети города или -1 */
int  citynet_state(const Game *g, int n);                 /* NS_* */
int  citynet_edge_built(const Game *g, int n, int e);
int  citynet_node_station(const Game *g, int n, int v);   /* 1 — в узле уже станция */
int  citynet_can_build(const Game *g, int n, int e);      /* перегон примыкает к сети и сеть доступна */
int  citynet_edge_gold(const Game *g, int n, int e);
int  citynet_edge_ind(const Game *g, int n, int e);
int  citynet_build(Game *g, int n, int e);                /* 1 — построено (списывает казну и производство) */
int  citynet_stations(const Game *g, int n);
void citynet_income(const Game *g, double out[R_COUNT]);  /* суммарный доход всех сетей за ход */

/* ui_city.c (Windows): окно-схема города во весь экран (U.modal == M_CITYNET, U.modalArg = сеть) */
void citynet_ui_draw(void);
int  citynet_ui_click(int act, int arg);                  /* 1 — обработано */
void citynet_ui_hover(int act, int arg, wchar_t *buf, int n);

#endif
