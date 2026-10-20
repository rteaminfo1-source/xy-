/* Городской транспорт (временная заглушка — реализуется отдельно). */
#include "citynet.h"
#include <string.h>

CityNetDef CITY_NETS[1];
const int  CITY_NET_COUNT = 0;

void citynet_init(void) {}
int  citynet_of_city(int c) { (void)c; return -1; }
int  citynet_state(const Game *g, int n) { (void)g; (void)n; return NS_FUTURE; }
int  citynet_edge_built(const Game *g, int n, int e) { (void)g; (void)n; (void)e; return 0; }
int  citynet_node_station(const Game *g, int n, int v) { (void)g; (void)n; (void)v; return 0; }
int  citynet_can_build(const Game *g, int n, int e) { (void)g; (void)n; (void)e; return 0; }
int  citynet_edge_gold(const Game *g, int n, int e) { (void)g; (void)n; (void)e; return 0; }
int  citynet_edge_ind(const Game *g, int n, int e) { (void)g; (void)n; (void)e; return 0; }
int  citynet_build(Game *g, int n, int e) { (void)g; (void)n; (void)e; return 0; }
int  citynet_stations(const Game *g, int n) { (void)g; (void)n; return 0; }
void citynet_income(const Game *g, double out[R_COUNT]) { (void)g; memset(out, 0, sizeof(double) * R_COUNT); }
