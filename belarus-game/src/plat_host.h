/* plat_host.h — сборка без Windows: что даёт «хозяин» (приложение iPhone ios/main.m или проверка на Linux)
 * и чем он управляет игрой. Под Windows всё это делает оконная процедура в ui_core.c. */
#ifndef BY_PLAT_HOST_H
#define BY_PLAT_HOST_H

#include <wchar.h>

/* ---------- хозяин -> игре: события (общие и для Windows, см. ui_core.c) ---------- */
void ui_ev_start(float dpi, const wchar_t *cmd);   /* один раз: данные, настройки, масштаб, --scene=N */
void ui_ev_size(int w, int h);                     /* размер буфера в пикселях */
int  ui_ev_paint(void);                            /* кадр в d_bits(); 1 — анимация, следующий кадр через 33 мс */
int  ui_ev_down(int x, int y, int right);          /* 1 — началось перетаскивание карты */
int  ui_ev_up(int x, int y, int right);            /* 1 — перетаскивание закончилось */
void ui_ev_move(int x, int y);
void ui_ev_leave(void);
void ui_ev_wheel(int delta, int x, int y, int ctrl);
void ui_ev_key(int vk, int ctrl);
void ui_ev_timer(int id);                          /* 1 — тост, 2 — часы (раз в секунду), 3 — сглаживание карты, 4 — анимация */
void ui_ev_close(void);                            /* приложение уходит: сохранить партию */
int  ui_ev_on_map(int x, int y);                   /* точка над 3D-картой (жесты карты) */
int  ui_ev_scroll_unit(void);                      /* сколько пикселей прокрутки даёт delta = 120 */

/* ---------- игра -> хозяину ---------- */
void host_invalidate(void);                        /* нужен новый кадр */
void host_set_timer(int id, int ms);               /* повторяющийся таймер, как SetTimer */
void host_kill_timer(int id);
const char *host_data_dir(void);                   /* папка сохранений (UTF-8) */
const char *host_res_dir(void);                    /* папка с ресурсами .qoi и assets.rc */
void host_quit(void);                              /* кнопка «Выход» */

/* ресурс RCDATA по номеру из assets.rc: файл из host_res_dir() (освобождать free) */
int plat_resource(int id, void **data, int *size);

#endif
