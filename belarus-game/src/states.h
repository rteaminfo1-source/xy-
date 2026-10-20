/* states.h — исторические территории для 3D-карты: государство, в которое входили белорусские земли,
 * его части и соседи в разные годы (Полоцкое и Турово-Пинское княжества, ВКЛ, Речь Посполитая,
 * Российская империя, СССР, Западная Беларусь в составе Польши 1921–1939 и т. д.).
 * Многоугольники — простые (без самопересечений), в градусах lon/lat, внутри «доски»
 * lon 19.5…36.5, lat 48.5…58.5. Данные — src/data_states.c. */
#ifndef BY_STATES_H
#define BY_STATES_H

#include <wchar.h>

#define HIST_MAX_PTS 64

enum { HA_STATE,      /* государство, в которое входили белорусские земли (цвет — «свой») */
       HA_PART,       /* его часть внутри (например, Полоцкое и Турово-Пинское княжества — отдельно) */
       HA_FOREIGN };  /* белорусские земли под властью соседа (Западная Беларусь в Польше 1921–1939) */

typedef struct {
    const wchar_t *name;      /* «Великое Княжество Литовское» */
    int            role;      /* HA_* */
    int            from, to;  /* годы действия, включительно */
    unsigned       color;     /* 0xRRGGBB — заливка и обводка */
    float          labelLon, labelLat;   /* где подписать */
    int            n;
    float          pts[HIST_MAX_PTS][2]; /* lon, lat */
} HistArea;

extern const HistArea HIST_AREAS[];
extern const int      HIST_AREA_COUNT;

/* Название государства для года (для подписи на карте), например «СССР» в 1941–1945. */
const wchar_t *hist_state_name(int year);

#endif
