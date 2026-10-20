/* Армии на карте, фронт, железные дороги и поезда — интерфейс (заглушка контракта). */
#include "mapview.h"
#include "front.h"

void front_overlay(const MapProj *pj, int interactive, int large) { (void)pj; (void)interactive; (void)large; }
int  front_ui_click(int act, int arg) { (void)act; (void)arg; return 0; }
int  front_ui_rclick(int act, int arg) { (void)act; (void)arg; return 0; }
void front_ui_hover(int act, int arg, wchar_t *buf, int n) { (void)act; (void)arg; (void)buf; (void)n; }
void front_dev_scene(int n) { (void)n; }
