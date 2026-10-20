/* Анимации: взлёт ракеты с космонавтами, стрельба пушек (заглушка контракта). */
#include "mapview.h"

void fx_launch_draw(void) { U.modal = M_NONE; }
int  fx_ui_click(int act, int arg) { (void)act; (void)arg; return 0; }
void fx_ui_hover(int act, int arg, wchar_t *buf, int n) { (void)act; (void)arg; (void)buf; (void)n; }
int  fx_ui_key(WPARAM vk) { (void)vk; return 0; }
int  fx_ui_idle(void) { return 0; }
void fx_ui_hired(int person) { (void)person; }
int  fx_person_card(int person, int x, int y, int w) { (void)person; (void)x; (void)y; (void)w; return 0; }
void fx_cannon(int x, int y, int size, int dir, double t) { (void)x; (void)y; (void)size; (void)dir; (void)t; }
void fx_dev_scene(int n) { (void)n; }
