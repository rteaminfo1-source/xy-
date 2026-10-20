/* Окно городского транспорта (временная заглушка — реализуется отдельно). */
#include "mapview.h"
#include "citynet.h"

void citynet_ui_draw(void) {}
int  citynet_ui_click(int act, int arg) { (void)act; (void)arg; return 0; }
void citynet_ui_hover(int act, int arg, wchar_t *buf, int n) { (void)act; (void)arg; if (n > 0) buf[0] = 0; }

void citynet_map_overlay(const MapProj *pj, int interactive, int large) { (void)pj; (void)interactive; (void)large; }
int  citynet_ui_key(WPARAM vk) { (void)vk; return 0; }
int  citynet_ui_idle(void) { return 0; }
void citynet_ui_built(int proj) { (void)proj; }
int  citynet_city_card(int city, int x, int y, int w) { (void)city; (void)x; (void)y; (void)w; return 0; }
void citynet_dev_scene(int n) { (void)n; }
