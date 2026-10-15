/* map3d.h — привязка 3D-рендера карты (assets/map3d.png) к географии.
 * Сетку «долгота/широта -> пиксель рендера» строит Blender (map3d_grid.json),
 * tools/pack_assets.py переносит её в src/assets_gen.h. Без ассета всё возвращает 0,
 * и игра рисует прежнюю плоскую карту. */
#ifndef BY_MAP3D_HEADER
#define BY_MAP3D_HEADER

int    map3d_available(void);          /* 1, если рендер карты встроен в .exe */

/* Координаты на рендере (пиксели MAP3D_W x MAP3D_H) для точки lon/lat:
 * билинейная интерполяция по сетке MAP3D_PTS. 0 — вне сетки или нет ассета. */
int    map3d_project(double lon, double lat, double *px, double *py);

/* Центр города c (C_*) на рендере — как его отметил Blender. 0 — нет данных. */
int    map3d_city(int c, double *px, double *py);

double map3d_px_per_km(void);          /* пикселей рендера на километр (0 без ассета) */

#endif
