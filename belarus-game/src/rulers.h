/* Правители государств, в которые входили белорусские земли (1100–2026). Только справка, без влияния на баланс. */
#ifndef BY_RULERS_H
#define BY_RULERS_H

#include <wchar.h>

enum { ST_POLOTSK, ST_GDL, ST_RZECZ, ST_EMPIRE, ST_USSR, ST_BSSR, ST_BELARUS, ST_COUNT };

typedef struct {
    const char    *id;        /* совпадает с id портрета в assets/portraits.json */
    const wchar_t *name;
    const wchar_t *title;
    int            state;
    int            from, to;  /* годы власти над белорусскими землями */
    const wchar_t *desc;
} Ruler;

extern const wchar_t *STATE_NAMES[ST_COUNT];
extern const Ruler RULERS[];
extern const int RULER_COUNT;

/* Правители, чья власть приходится на интервал [year, nextYear): до max штук, по порядку списка.
 * Возвращает количество найденных. */
int rulers_in_period(int year, int nextYear, int *out, int max);

/* Главный правитель в году year (не руководитель БССР, если есть союзный); -1 — междуцарствие или нет данных. */
int ruler_at(int year);

#endif
