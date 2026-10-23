/* mapview.h — слои карты из других модулей и их «крючки» в общем интерфейсе.
 *
 * Карта (ui_map.c) рисуется одной из трёх проекций: интерактивная 3D-карта (T3View), готовый рендер
 * Blender (MapXf) или плоская карта. Модули рисуют свои слои поверх через MapProj и функции mapv_*,
 * не зная, какая проекция сейчас. Каждый модуль реализует только свои функции:
 *   ui_states.c — исторические территории (Полоцкое княжество, ВКЛ, Речь Посполитая, СССР…);
 *   ui_front.c  — армии на карте, фронт, железные дороги снабжения и поезда;
 *   ui_city.c   — городской транспорт (метро Минска, трамваи и троллейбусы областных городов);
 *   ui_fx.c     — анимации: взлёт ракеты с космонавтами, стрельба пушек.
 * Все функции зовутся при каждом кадре или событии: они должны быть быстрыми и не держать состояние,
 * которое ломается при повторном вызове (тела окон рисуются дважды — для измерения и для показа). */
#ifndef BY_MAPVIEW_H
#define BY_MAPVIEW_H

#include "ui.h"

typedef struct MapProj MapProj;

/* ---------- проекция текущего кадра карты (ui_map.c) ---------- */
/* точка lon/lat на земле (+zAdd км над рельефом на 3D-карте); k — пикселей на км в этой точке; 0 — не видна */
int  mapv_ll(const MapProj *pj, double lon, double lat, double zAdd, int *sx, int *sy, double *k);
/* точка мира в км (X = (lon - 28) * 65.95, Y = (lat - 53.7) * 111.32) на высоте Z км (на 2D-картах Z не учитывается) */
int  mapv_xyz(const MapProj *pj, double X, double Y, double Z, int *sx, int *sy, double *k);
/* центр города C_* на экране; 0 — не виден */
int  mapv_city(const MapProj *pj, int c, int *sx, int *sy, double *k);
void mapv_rect(const MapProj *pj, int *x, int *y, int *w, int *h);   /* окно вывода карты */
const T3View *mapv_view(const MapProj *pj);                          /* NULL — 2D-карта */
double mapv_px_per_km(const MapProj *pj);                            /* масштаб у центра (3D) или везде (2D) */
int  mapv_size(const MapProj *pj);                                   /* базовый размер моделей, пиксели */
/* множитель размера значков в точке с масштабом k: растут при приближении, но медленнее земли */
double mapv_scale(const MapProj *pj, double k);

/* ---------- полёт камеры 3D-карты (ui_map.c) ---------- */
/* Плавно ведёт камеру к точке за ms миллисекунд (zoom 1 — вся страна, больше — ближе; pitch 14…89).
 * По окончании вызывает done (если не NULL). Работает, пока карта видна (панель или M_MAP). */
void map_cam_fly_to(double lon, double lat, double zoom, double pitch, int ms, void (*done)(void));
int  map_cam_flying(void);

/* ---------- ui_states.c: исторические территории ---------- */
/* Перед рендером 3D-карты: передаёт в terrain3d земли государств текущего года; возвращает ключ для кэша кадра. */
unsigned states_prepare_3d(void);
/* Подписи и обводки государств (под всеми остальными слоями). */
void states_overlay(const MapProj *pj, int interactive, int large);
void states_dev_scene(int n);                 /* --scene=50…59 */

/* ---------- ui_front.c: армии, фронт, снабжение ---------- */
void front_overlay(const MapProj *pj, int interactive, int large);   /* из war_overlay, до отметки боя */
int  front_ui_click(int act, int arg);        /* 1 — обработано (A_ARMY_SEL, A_FRONT_NODE, A_RAIL_BUILD, A_ARMY_SPLIT…) */
int  front_ui_rclick(int act, int arg);       /* правая кнопка без перетаскивания по области act/arg */
void front_ui_hover(int act, int arg, wchar_t *buf, int n);          /* buf[0] = 0 — не моя область */
void front_dev_scene(int n);                  /* --scene=40…49 */
/* каждую секунду игры (ui_minute_tick): армии идут сами — клетка за 10 секунд, у цели раз в 10 секунд бой */
void front_ui_tick(int dtMs);

/* ---------- ui_troops.c: отряды на карте ---------- */
enum { TM_STAND, TM_MARCH, TM_FIRE };
/* (x, y) — середина отряда на земле, size — ширина строя в пикселях, unit — вид войск UNIT_TYPES (-1 — пехота),
 * count — соединений (рисуется до 9 фигур), dir: +1 — лицом вправо, -1 — влево, mode — TM_*, enemy — враг
 * (форма его эпохи по году year), t — секунды для анимации. Вызывающий сам просит ui_anim_request(). */
void troops_draw(int x, int y, int size, int unit, int count, int dir, int mode, int enemy, int year, double t);
void troops_gallery(double t);               /* сцена 39: все виды отрядов во всех режимах */

/* ---------- ui_city.c: городской транспорт (остальное — в citynet.h) ---------- */
void citynet_map_overlay(const MapProj *pj, int interactive, int large);   /* значки линий у городов */
int  citynet_ui_key(WPARAM vk);               /* M_CITYNET открыт: 1 — клавиша обработана */
int  citynet_ui_idle(void);                   /* нет событий и окон: 1 — открыла заставку/окно */
void citynet_ui_built(int proj);              /* построен проект PROJECTS[proj] */
int  citynet_city_card(int city, int x, int y, int w);   /* блок в окне города; возвращает высоту (0 — нет) */
void citynet_dev_scene(int n);                /* --scene=60…69 */

/* ---------- ui_fx.c: анимации ---------- */
void fx_launch_draw(void);                    /* U.modal == M_LAUNCH, U.launchPerson — кто летит */
int  fx_ui_click(int act, int arg);           /* 1 — обработано */
void fx_ui_hover(int act, int arg, wchar_t *buf, int n);
int  fx_ui_key(WPARAM vk);                    /* M_LAUNCH открыт: 1 — клавиша обработана */
int  fx_ui_idle(void);                        /* нет событий и окон: 1 — открыла заставку */
void fx_ui_hired(int person);                 /* приглашён деятель PEOPLE[person] */
int  fx_ui_event(const Event *e);             /* перед окном события: 1 — открыта заставка (авария на ЧАЭС) */
void fx_map_overlay(const MapProj *pj, int interactive, int large);   /* слой карты во время заставки ЧАЭС */
#define FX_ARG_CHAES 7004                     /* A_NEXT: смотреть заставку «Авария на ЧАЭС» */
#define FX_ARG_BERLIN 7005                    /* A_NEXT: смотреть заставку «Штурм Берлина» */
int  fx_person_card(int person, int x, int y, int w);    /* блок в окне деятеля; возвращает высоту (0 — нет) */
/* пушка у (x, y) — точка опоры колёс; size — длина ствола в пикселях; dir: +1 стреляет вправо, -1 влево;
 * t — время в секундах (выстрел, вспышка, откат, дым — циклом); вызывающий сам просит ui_anim_request() */
void fx_cannon(int x, int y, int size, int dir, double t);
void fx_dev_scene(int n);                     /* --scene=70…79 и 39 (галерея отрядов) */

/* ---------- ui_dev.c: помощники для сцен разработчика ---------- */
void dev_play_until(int year);                /* новая партия ботом до года (события последнего хода не разрешены) */
void dev_resolve(void);                       /* разрешить все ждущие события первым доступным выбором */

#endif
