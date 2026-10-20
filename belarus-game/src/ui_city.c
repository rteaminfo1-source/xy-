/* Окно городского транспорта (временная заглушка — реализуется отдельно). */
#include "ui.h"
#include "citynet.h"

void citynet_ui_draw(void) {}
int  citynet_ui_click(int act, int arg) { (void)act; (void)arg; return 0; }
void citynet_ui_hover(int act, int arg, wchar_t *buf, int n) { (void)act; (void)arg; if (n > 0) buf[0] = 0; }
