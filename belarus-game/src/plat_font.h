/* plat_font.h — шрифты для сборки без Windows: строка -> маска покрытия (8 бит на пиксель).
 * Реализации: ios/plat_font_ct.m (CoreText на iPhone) и plat_font_ft.c (FreeType, проверка на Linux).
 * Имена шрифтов — как в Windows («Segoe UI», «Georgia», «Bahnschrift»…): реализация подбирает похожий. */
#ifndef BY_PLAT_FONT_H
#define BY_PLAT_FONT_H

#include <wchar.h>

typedef struct PlatFont PlatFont;

/* px — высота символа (как -lfHeight у CreateFontW), weight — FW_* (400, 600, 700) */
PlatFont *pf_open(const wchar_t *face, int px, int weight, int italic);
void pf_close(PlatFont *f);
void pf_metrics(PlatFont *f, int *ascent, int *descent, int *avgw);
/* ширина строки в пикселях; extra — добавка после каждого символа (SetTextCharacterExtra) */
int pf_width(PlatFont *f, const wchar_t *s, int n, int extra);
/* Маска строки (освобождать free). Левый верхний угол маски — (ox, oy) от точки вывода TextOutW
 * (левый край строки и верх ячейки, где базовая линия — на ascent ниже). NULL — нечего рисовать. */
unsigned char *pf_render(PlatFont *f, const wchar_t *s, int n, int extra, int *w, int *h, int *ox, int *oy);

#endif
