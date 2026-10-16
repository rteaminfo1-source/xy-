/* image.h — встроенные картинки без сторонних библиотек.
 *
 * Рендеры Blender хранятся в .exe как RCDATA в формате QOI (см. tools/pack_assets.py).
 * В памяти пиксель — 0xAARRGGBB с ПРЕМУЛЬТИПЛИЦИРОВАННОЙ альфой (R,G,B <= A):
 * так корректно и масштабирование, и наложение. Код переносимый; windows.h
 * используется только в image_load_resource (под _WIN32). */
#ifndef IMAGE_H
#define IMAGE_H

typedef struct { int w, h; unsigned *px; } Image;    /* px: w*h, построчно сверху вниз */

/* Идентификаторы RCDATA (res/assets.rc генерирует tools/pack_assets.py). */
enum { RES_MAP3D_QOI = 2, RES_SPRITES_QOI = 3, RES_PORTRAITS_QOI = 4, RES_UNITS_QOI = 5,
       RES_TERRAIN_QOI = 6, RES_TERRAIN_H_QOI = 7 };

/* Полный декодер QOI (3 или 4 канала). Устойчив к обрезанным и испорченным данным:
 * никогда не читает за пределы буфера. 1 — успех (out заполнен, освобождать
 * image_free), 0 — ошибка (out->px == NULL). */
int  qoi_decode(const unsigned char *data, int size, Image *out);

/* Выделяет w*h пикселей, заполненных нулём (прозрачным). 1 — успех. */
int  image_alloc(Image *img, int w, int h);
void image_free(Image *img);              /* освобождает px и обнуляет структуру */

#ifdef _WIN32
/* Декодирует RCDATA-ресурс id из .exe (RES_*_QOI). 0 — ресурса нет или он битый. */
int  image_load_resource(int id, Image *out);
#endif

/* Масштабирует прямоугольник (sx,sy,sw,sh) из src в новый dst размером dw x dh
 * (dst выделяется заново; прежнее содержимое dst НЕ освобождается).
 * По каждой оси: уменьшение — усреднение по площади, увеличение — билинейно.
 * Работает с премультиплицированными пикселями. Прямоугольник обрезается по src;
 * при пустом результате или нехватке памяти dst->px == NULL. */
void image_scale(const Image *src, int sx, int sy, int sw, int sh, Image *dst, int dw, int dh);

/* Накладывает img (премультиплицированный) на непрозрачный буфер 0x00RRGGBB
 * dstW x dstH (DIB «сверху вниз», шаг строки = dstW) в точку (dx,dy) с отсечением. */
void image_blit(unsigned *dst, int dstW, int dstH, int dx, int dy, const Image *img);

/* То же для части img (sx,sy,sw,sh) — например, ячейки атласа — с общей
 * непрозрачностью alpha 0..255 (255 — как есть). */
void image_blit_part(unsigned *dst, int dstW, int dstH, int dx, int dy, const Image *img,
                     int sx, int sy, int sw, int sh, int alpha);

/* Подкладывает под img сплошной цвет 0x00RRGGBB (на месте): результат непрозрачен
 * (альфа 255), такую картинку можно выводить и через SetDIBitsToDevice/StretchDIBits. */
void image_fill_under(Image *img, unsigned rgb);

#endif
