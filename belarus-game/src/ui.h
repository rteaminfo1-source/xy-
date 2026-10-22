/* Интерфейс на чистом Win32 API и GDI: собственная отрисовка без сторонних библиотек. */
#ifndef BY_UI_H
#define BY_UI_H

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "game.h"
#include "terrain3d.h"

/* ---------- палитра ---------- */
typedef struct {
    COLORREF bg, surface, surface2, ink, muted, line, red, redSoft, blue, blueSoft,
             green, gold, land, landEdge, water, forest, shadow, onAccent;
} Pal;
extern Pal P;
extern int g_dark;
extern float g_s;          /* масштаб: DPI × зум */
extern int g_w, g_h;       /* размер клиентской области */
#define SC(x) ((int)((x) * g_s + 0.5f))

/* ---------- шрифты ---------- */
enum { F_BODY, F_BODY_B, F_SMALL, F_SMALL_B, F_TINY, F_H3, F_H2, F_H1, F_MONO, F_MONO_B,
       F_ERA_XL, F_ERA_L, F_ERA_M, F_ERA_S, F_HUGE, F_COUNT };
HFONT ui_font(int f);
void  ui_fonts_reset(void);
void  ui_set_era_font(int era);

/* ---------- рисование ---------- */
COLORREF mix(COLORREF a, COLORREF b, int t255);
void d_fill(int x, int y, int w, int h, COLORREF c);
void d_frame(int x, int y, int w, int h, COLORREF c);
void d_rrect(int x, int y, int w, int h, int r, COLORREF fill, COLORREF border);
void d_line(int x1, int y1, int x2, int y2, COLORREF c, int t);
void d_circle(int cx, int cy, int r, COLORREF fill, COLORREF border, int t);
void d_rrect_grad(int x, int y, int w, int h, int r, COLORREF top, COLORREF bottom, COLORREF border);   /* градиент сверху вниз */
void d_rrect_a(int x, int y, int w, int h, int r, COLORREF fill, int alpha);                         /* полупрозрачная */
void d_shadow(int x, int y, int w, int h, int r, int spread, int alpha);                             /* мягкая тень карточки */
void d_grad(int x, int y, int w, int h, COLORREF top, COLORREF bottom);
void d_backdrop(void);                                                                               /* фон окна с виньеткой */
void d_round_mask(int x, int y, int w, int h, int r);                                                /* скруглить углы картинки поверх фона */
void d_poly(const POINT *pts, int n, COLORREF fill, COLORREF border, int t);
void d_text(int x, int y, const wchar_t *s, int f, COLORREF c);
int  d_text_w(const wchar_t *s, int f);
int  d_font_h(int f);
int  d_wrap(int x, int y, int w, const wchar_t *s, int f, COLORREF c, int draw);
void d_text_c(int x, int y, int w, const wchar_t *s, int f, COLORREF c);
void d_text_r(int xr, int y, const wchar_t *s, int f, COLORREF c);
void d_ellipsis(int x, int y, int w, const wchar_t *s, int f, COLORREF c);
void d_blend(int x, int y, int w, int h, COLORREF c, int alpha);
void d_clip(int x, int y, int w, int h);
void d_unclip(void);
void d_ornament(int x, int y, int w, int cell, COLORREF a, COLORREF b);
void d_icon(int kind, int x, int y, int s, COLORREF c);
void d_bitmap(HDC src, int x, int y, int w, int h);
void d_pie(int cx, int cy, int r, double frac, COLORREF fill);
int  d_clip_get(int *x, int *y, int *w, int *h);   /* 1 — отсечение включено */
HDC  d_dc(void);
unsigned *d_bits(void);

enum { IC_GOLD, IC_SCI, IC_IND, IC_POP, IC_STAB, IC_CULT, IC_ECO, IC_DEF,
       IC_POLIT, IC_WAR, IC_DISASTER, IC_SPACE, IC_PERSON, IC_BUILD, IC_TECH, IC_QUIZ, IC_LOCK, IC_CHECK, IC_FLAG, IC_CROWN, IC_COUNT };

/* ---------- области нажатия ---------- */
enum {
    A_NONE, A_NEW, A_CONTINUE, A_HELP, A_EXIT, A_DIFF, A_START, A_TAB, A_ERAF, A_RESEARCH, A_TECH,
    A_BUILD, A_PROJ, A_HIRE, A_PERSON, A_PRIORITY, A_QUIZ, A_END_TURN, A_CHOICE, A_NEXT, A_CLOSE,
    A_MENU, A_SAVE, A_LOAD, A_THEME, A_TO_TITLE, A_CITY, A_SELL, A_ERA_START, A_FINAL, A_ZOOM,
    A_PFILTER, A_TIMELINE, A_SCROLLBAR, A_CREDITS, A_MAP_FULL, A_QUEST, A_RULER,
    A_RECRUIT, A_DISBAND, A_DECLARE, A_LIBERATE, A_PEACE, A_PROVINCE, A_MAP_CAM,
    A_NET_OPEN, A_NET_EDGE, A_NET_NODE, A_NET_CLOSE,         /* городской транспорт (ui_city.c) */
    A_ARMY_SEL, A_FRONT_NODE, A_RAIL_BUILD, A_ARMY_SPLIT,     /* армии на карте */
    A_ADMIN                                                   /* админ-версия: команда панели (ui_admin.c) */
};
/* кнопки камеры 3D-карты (аргумент A_MAP_CAM) */
enum { MC_ZOOM_OUT, MC_ZOOM_IN, MC_ROT_LEFT, MC_ROT_RIGHT, MC_RESET, MC_TILT_UP, MC_TILT_DOWN };
/* области только для подсказок (без клика) */
enum { A_HOVER_RES = 100, A_HOVER_ERA, A_HOVER_ACH, A_HOVER_STAT, A_HOVER_MIN, A_HOVER_SPRITE, A_HOVER_REGION, A_HOVER_UNIT, A_HOVER_BATTLE };
void hit(int x, int y, int w, int h, int act, int arg);
int  hover(int act, int arg);
void hits_reset(void);
void hits_clip(int x, int y, int w, int h);
void hits_unclip(void);
int  hit_at(int x, int y, int *act, int *arg);

/* ---------- виджеты ---------- */
enum { B_PRIMARY, B_SECONDARY, B_GHOST, B_DANGER };
void w_button(int x, int y, int w, int h, const wchar_t *label, int kind, int enabled, int act, int arg);
int  w_pill(int x, int y, const wchar_t *s, COLORREF fg, COLORREF bg);
void w_scrollbar(int x, int y, int h, int content, int view, int scroll);

/* ---------- состояние интерфейса ---------- */
enum { SCR_TITLE, SCR_GAME };
enum { M_NONE, M_EVENT, M_ERA, M_FINAL, M_HELP, M_MENU, M_NEWGAME, M_TECH, M_PROJ, M_PERSON, M_CITY, M_CREDITS, M_MAP, M_RULER,
       M_CITYNET, M_LAUNCH, M_ADMIN };
enum { TAB_TECH, TAB_PROJ, TAB_PEOPLE, TAB_ARMY, TAB_QUESTS, TAB_POLICY, TAB_QUIZ, TAB_LOG, TAB_RESULTS, TAB_COUNT };

typedef struct {
    int screen, modal, modalArg, tab, eraFilter, projFilter;
    int scroll[TAB_COUNT], content[TAB_COUNT], view;
    int modalScroll, modalContent, modalView;
    int evResult;                /* в окне события показан итог */
    Outcome outcome;
    const Event *evShown;
    int evChoice;
    int diffPick;
    int city;                    /* выбранный город, -1 */
    wchar_t toast[260];
    DWORD toastUntil;
    wchar_t status[260];
    int hasSave;
    int zoom;                    /* проценты */
    int minuteMs;                /* накоплено до минутного дохода, мс */
    DWORD minuteLast;            /* время прошлого тика таймера */
    int panelX, panelY, panelW, panelH;
    /* 3D-карта: камера и окно, в котором её сейчас можно крутить (заполняется при рисовании) */
    T3Cam cam;
    int camInit;
    int mapVX, mapVY, mapVW, mapVH, mapVOn;
    int mapFast;                 /* идёт вращение: рисуем без сглаживания */
    int selArmy;                 /* выбранная армия на карте, -1 */
    DWORD animStart;             /* начало текущей анимации (взлёт ракеты и т. п.) */
    int launchPerson;            /* M_LAUNCH: кто летит (индекс PEOPLE) */
} UiState;

/* анимации: пока функция зовётся при рисовании кадра, окно перерисовывается ~30 раз в секунду */
void ui_anim_request(void);

extern Game G;
extern UiState U;
extern HWND g_hwnd;
extern int g_dev;          /* режим --scene: без записи сохранений и настроек */

/* ui_core.c */
void ui_invalidate(void);
void ui_toast(const wchar_t *fmt, ...);
int  ui_save(void);
int  ui_load(void);
void ui_save_settings(void);
void ui_zoom(int delta);
void ui_toggle_theme(void);

/* ui_game.c */
void ui_paint(void);
void ui_click(int act, int arg);
void ui_rclick(int act, int arg);       /* правая кнопка без перетаскивания */
void ui_key(WPARAM vk);
void ui_wheel(int delta, int x, int y);
void ui_after_change(void);
void ui_hover_status(int act, int arg);
void ui_minute_tick(void);

/* ui_map.c */
void map_draw(int x, int y, int w, int h, int interactive);
void map_draw_ex(int x, int y, int w, int h, int interactive, int large);
void map_invalidate(void);
int  map3d_on(void);                         /* интерактивная 3D-карта доступна */
void map_cam_action(int a);                  /* MC_* */
void map_cam_rotate(double dyaw, double dpitch);
void map_cam_pan(int fromX, int fromY, int toX, int toY);
void map_cam_zoom_at(double factor, int sx, int sy);
int  map_cam_hit(int x, int y);              /* точка внутри окна 3D-карты, где её можно крутить */

/* ui_art.c — встроенные рендеры Blender (карта, постройки, портреты) */
typedef struct { int x, y, w, h; double s; int sx, sy; } MapXf;   /* экран = x + (px - sx) * s */
int  art_map_available(void);
int  art_map_draw(int x, int y, int w, int h, MapXf *xf);        /* 1 — нарисована 3D-карта */
void art_map_point(const MapXf *xf, double px, double py, int *sx, int *sy);
int  art_sprites_available(void);
void art_sprite_draw(int sprite, int ax, int ay, int size, int alpha);  /* (ax,ay) — точка опоры модели */
int  art_portrait_index(const char *id);                          /* -1, если портрета нет */
void art_portrait_draw(int idx, int x, int y, int w, int h);      /* вписать в прямоугольник */
int  art_terrain_ready(void);                                     /* 1 — 3D-карта загружена */
int  art_unit_index(const char *id);                              /* -1, если фигурки нет */
void art_unit_draw(int idx, int x, int y, int w, int h);          /* 3D-фигурка войска, вписать в прямоугольник */

/* ui_tabs.c — общие помощники вкладок */
void     tab_section(int x, int *y, const wchar_t *s);
COLORREF tab_card_fill(void);

/* ui_army.c */
int  tab_army(int x, int y, int w);
void unit_badge(int u, int x, int y, int w, int h, COLORREF border);  /* фигурка или значок */
int  landmark_sprite(int p);                                      /* ui_landmarks.c */

/* ui_admin.c — админ-версия (сборка с -DADMIN_BUILD) */
int  admin_body(int x, int y, int w);
int  admin_click(int arg);

/* ui_dev.c */
void dev_scene(int n);

/* ui_modal.c */
void modal_draw(void);
void portrait_box(const char *id, const wchar_t *initials, int x, int y, int w, int h, COLORREF ring);

/* издатели */
#define CREDIT_SCHOOL  L"Гимназия № 10 г. Минска"
#define CREDIT_HONOR   L"имени дважды Героя Советского Союза П. Я. Головачёва"
#define CREDIT_AUTHOR  L"Биончик Роман"
#define CREDIT_CLASS   L"учащийся 6 «Г» класса"
#define APP_VERSION    L"1.1"

/* общие помощники форматирования */
void fmt_num(double v, wchar_t *buf, int n);
const wchar_t *roman(int era);
int  kind_icon(int kind);
COLORREF kind_color(int kind);
COLORREF branch_color(int br);

#endif
