/* citynet.h — городской транспорт, который игрок строит сам: метро в Минске (1984) и похожие
 * «интерактивы» в областных городах (трамвай, троллейбус…). Схема города — узлы-районы и
 * возможные перегоны между ними; игрок строит перегоны, станции появляются на концах.
 * Движок переносимый (без windows.h); интерфейс — ui_city.c. */
#ifndef BY_CITYNET_H
#define BY_CITYNET_H

#include "game.h"

#define NET_MAX_NODES 40        /* узлов на схеме (Game.nets[].nodes и nodesHi — 64 бита) */
#define NET_MAX_EDGES 48        /* перегонов (Game.nets[].edges — 64 бита) */
#define NET_MAX_RIVER 16
#define NET_MAX_LINES 3
#define NET_MAX_XFER  4

/* x, y — 0..1 на схеме (x вправо, y вниз); year — когда открыта настоящая станция (0 — не показывать);
 * note — короткая справка о месте (может быть NULL) */
typedef struct { const wchar_t *name; float x, y; int year; const wchar_t *note; } NetNode;
typedef struct { unsigned char a, b, line; } NetEdge;
typedef struct { const wchar_t *name; unsigned color; int year; } NetLine;   /* линия: имя, цвет 0xRRGGBB, с какого года строится */

enum { NT_METRO, NT_TRAM, NT_TROLLEY };   /* вид транспорта: значок на карте и вагоны на схеме */

typedef struct {
    const char    *id;          /* "minsk_metro" */
    int            city;        /* C_* */
    int            type;        /* NT_* */
    const wchar_t *kind;        /* «Метрополитен», «Трамвай», «Троллейбус» */
    const wchar_t *title;       /* «Минское метро» */
    int            year;        /* с какого года можно строить (историческая дата открытия) */
    const char    *tech;        /* нужная технология или NULL */
    unsigned       color;       /* 0xRRGGBB — цвет значка на карте */
    int            nnodes, nedges, nlines;
    NetNode        nodes[NET_MAX_NODES];
    NetEdge        edges[NET_MAX_EDGES];
    NetLine        lines[NET_MAX_LINES];
    int            startNode;   /* первая станция (центр): первый перегон строится от неё */
    int            nxfer;
    unsigned char  xfer[NET_MAX_XFER][2];     /* пересадки между линиями: станция одной линии открывает соседнюю */
    const wchar_t *riverName;   /* «Свислочь» */
    int            nriver;
    float          river[NET_MAX_RIVER][2];   /* река на схеме, 0..1 */
    int            goldPerEdge, indPerEdge;   /* стоимость перегона в год открытия (до множителей эпохи и сложности) */
    Fx             fxPerStation;              /* доход за ход за каждую станцию, в десятых долях */
    const char    *proj;        /* стройка PROJECTS — первая очередь (её перегоны — первые firstEdges) или NULL */
    int            firstEdges;
    const wchar_t *firstName;   /* «Первая очередь от «Института культуры» до «Московской»» (для сообщения «… готова!») */
    const wchar_t *desc;        /* историческая справка */
    int            techi, proji;   /* заполняются citynet_init() */
} CityNetDef;

extern CityNetDef CITY_NETS[];
extern const int  CITY_NET_COUNT;

enum { NS_FUTURE, NS_LOCKED, NS_AVAILABLE };

void citynet_init(void);                                  /* зовёт data_init(): индексы технологий и строек */
int  citynet_of_city(int c);                              /* индекс сети города или -1 */
int  citynet_state(const Game *g, int n);                 /* NS_* */
int  citynet_edge_built(const Game *g, int n, int e);
int  citynet_node_station(const Game *g, int n, int v);   /* 1 — в узле уже станция */
int  citynet_can_build(const Game *g, int n, int e);      /* перегон примыкает к сети и сеть доступна */
const wchar_t *citynet_cannot_build(const Game *g, int n, int e);   /* NULL — можно; иначе причина (с учётом цены) */
int  citynet_edge_gold(const Game *g, int n, int e);
int  citynet_edge_ind(const Game *g, int n, int e);
int  citynet_build(Game *g, int n, int e);                /* 1 — построено (списывает казну и производство) */
int  citynet_stations(const Game *g, int n);
int  citynet_edges_count(const Game *g, int n);           /* построено перегонов */
unsigned long long citynet_station_mask(const Game *g, int n);   /* биты узлов со станциями */
void citynet_upgrade(Game *g, int from);                  /* после загрузки старого сохранения (версия from) */
void citynet_net_income(const Game *g, int n, double out[R_COUNT]);   /* доход одной сети за ход */
void citynet_income(const Game *g, double out[R_COUNT]);  /* суммарный доход всех сетей за ход */
/* построена стройка-первая очередь (из вкладки «Стройки»): её перегоны появляются на схеме,
 * а за уже проложенные самим игроком перегоны возвращаются деньги (*gold, *ind — сколько) */
void citynet_project_built(Game *g, int proj, int *gold, int *ind);

/* ui_city.c (Windows): окно-схема города во весь экран (U.modal == M_CITYNET, U.modalArg = сеть) */
void citynet_ui_draw(void);
int  citynet_ui_click(int act, int arg);                  /* 1 — обработано */
void citynet_ui_hover(int act, int arg, wchar_t *buf, int n);

#endif
