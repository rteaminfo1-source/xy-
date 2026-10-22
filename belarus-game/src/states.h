/* states.h — исторические территории для 3D-карты: государство, в которое входили белорусские земли,
 * его части и соседи в разные годы (Полоцкое и Турово-Пинское княжества, ВКЛ, Речь Посполитая,
 * Российская империя, СССР, Западная Беларусь в составе Польши 1921–1939 и т. д.).
 * Многоугольники — простые (без самопересечений), в градусах lon/lat, внутри «доски»
 * lon 19.5…36.5, lat 48.5…58.5. Данные — src/data_states.c. */
#ifndef BY_STATES_H
#define BY_STATES_H

#include <wchar.h>
#include "terrain3d.h"

#define HIST_MAX_PTS 64
#define HIST_LON0 19.5f      /* «доска» — край карты земель */
#define HIST_LON1 36.5f
#define HIST_LAT0 48.5f
#define HIST_LAT1 58.5f

enum { HA_STATE,      /* государство, в которое входили белорусские земли (цвет — «свой») */
       HA_PART,       /* его часть внутри (например, Полоцкое и Турово-Пинское княжества — отдельно) */
       HA_FOREIGN,    /* белорусские земли под властью соседа (Западная Беларусь в Польше 1921–1939) */
       HA_NEIGHBOR,   /* соседнее государство — бледно, мелкая подпись */
       HA_SUB };      /* другая часть того же государства (РСФСР в СССР, Царство Польское в империи) */

typedef struct {
    const wchar_t *name;      /* «Великое Княжество Литовское» */
    int            role;      /* HA_* */
    int            from, to;  /* годы действия, включительно */
    unsigned       color;     /* 0xRRGGBB — заливка и обводка */
    float          labelLon, labelLat;   /* где подписать; (0, 0) — без подписи */
    int            n;         /* 0 — современная граница Беларуси (плита карты, BORDER) */
    float          pts[HIST_MAX_PTS][2]; /* lon, lat */
} HistArea;

extern const HistArea HIST_AREAS[];
extern const int      HIST_AREA_COUNT;
extern const HistArea HIST_WATER[];       /* море и Чудское озеро — подложка карты земель во все годы */
extern const int      HIST_WATER_COUNT;

/* Название государства для года (для подписи на карте), например «СССР» в 1941–1945. */
const wchar_t *hist_state_name(int year);
/* Годы земли целиком: записи подряд с тем же именем и ролью (граница менялась) склеиваются. */
void hist_area_years(const HistArea *a, int *from, int *to);
/* Порядок наложения: соседи (0) < государство < другие его части < белорусская часть < под властью соседа (4). */
int hist_area_prio(int role);
/* Земли года по возрастанию порядка наложения: индексы в HIST_AREAS, не больше max; возвращает число. */
int hist_areas_at(int year, int *idx, int max);
/* Те же земли для 3D-карты (первыми — вода): км, цвета плит и линий границ, оттенок на плите Беларуси,
 * штриховка земель под властью соседа. Возвращает число (не больше max и T3_MAX_AREAS). */
int hist_t3_areas(int year, T3Area *out, int max);
void hist_t3_board(double *x0, double *y0, double *x1, double *y1);   /* «доска» в км */

#endif
