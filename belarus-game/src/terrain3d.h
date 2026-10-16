/* terrain3d.h — интерактивная 3D-карта Беларуси: собственный программный растеризатор
 * (без OpenGL и сторонних библиотек). Рельеф и текстура выгружены из сцены Blender
 * (tools/blender/build_terrain3d.py), здесь они рисуются с перспективой, z-буфером,
 * mip-уровнями текстуры и земляными стенками плиты. Код переносимый (без windows.h).
 *
 * Мир — километры, как в Blender: X = (lon - 28) * 65.95, Y = (lat - 53.7) * 111.32, Z вверх. */
#ifndef BY_TERRAIN3D_H
#define BY_TERRAIN3D_H

#include "image.h"

/* Камера вращается вокруг точки (tx, ty) на земле. yaw — поворот по часовой (0 — север вверху),
 * pitch — наклон над горизонтом (90 — сверху), zoom — 1: вся страна в кадре, больше — ближе. */
typedef struct { double yaw, pitch, zoom, tx, ty; } T3Cam;

typedef struct {
    int    w, h;                    /* размер окна вывода, пиксели */
    double eye[3], fwd[3], right[3], up[3];
    double f, cx, cy, dist, nearZ;  /* фокус в пикселях, центр, расстояние до цели */
} T3View;

/* Данные: текстура вида сверху и карта высот (R,G — метры модели, B — маска). Картинки копируются. */
int    t3d_init(const Image *tex, const Image *heights);
int    t3d_ready(void);
void   t3d_free(void);

void   t3d_cam_reset(T3Cam *c);
void   t3d_cam_clamp(T3Cam *c);
void   t3d_view(const T3Cam *c, int w, int h, T3View *v);

/* Проекция точки мира на экран (относительно левого верхнего угла окна вывода).
 * 0 — точка позади камеры. depth — расстояние вдоль взгляда, км. */
int    t3d_project(const T3View *v, double X, double Y, double Z, double *sx, double *sy, double *depth);
/* Точка на плоскости Z = z под пикселем (sx, sy). 0 — луч не пересекает плоскость. */
int    t3d_unproject(const T3View *v, double sx, double sy, double z, double *X, double *Y);

double t3d_height(double X, double Y);           /* высота верха плиты, км */
void   t3d_lonlat(double lon, double lat, double *X, double *Y);
void   t3d_bounds(double *x0, double *y0, double *x1, double *y1);

/* Дополнительные земли (присоединённые, цель похода, доступные для похода): плоские террасы
 * с земляными стенками, пристыкованные к плите. Многоугольник — звёздный относительно (cx, cy).
 * Верх ниже рельефа Беларуси, поэтому край, заходящий под плиту, прячется. Текстура — участок
 * вида сверху со сдвигом (ux, uy) км, окрашенный множителями tint (1 — без изменений). */
#define T3_BLOB_PTS 48
typedef struct {
    float cx, cy, top;
    int   n;
    float x[T3_BLOB_PTS], y[T3_BLOB_PTS];
    float ux, uy;
    float tint[3];
    float grey;            /* 0..1 — обесцвечивание */
} T3Blob;
void   t3d_set_blobs(const T3Blob *b, int n);   /* копируется; n = 0 — убрать */
/* Пересечение луча из (x, y) по направлению (dx, dy) с контуром плиты: расстояние или -1. */
double t3d_outline_hit(double x, double y, double dx, double dy);
/* Ближайшая к (x, y) точка контура плиты; работает и без загруженной карты. */
void   t3d_outline_nearest(double x, double y, double *ox, double *oy);
int    t3d_inside(double x, double y);              /* 1 — точка внутри контура плиты (Беларуси) */

/* Рисует карту в буфер dst (0x00RRGGBB, шаг строки stride) размером v->w x v->h.
 * bg — цвет фона, ss — сглаживание (1 — быстро, при вращении; 2 — 4 отсчёта на пиксель). */
void   t3d_render(const T3View *v, unsigned *dst, int stride, unsigned bg, int ss);

#endif
