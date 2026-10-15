/* map3d.c — проекция lon/lat на 3D-рендер карты по сетке из assets_gen.h. */
#include "map3d.h"
#include "assets_gen.h"

int map3d_available(void) { return ASSETS_HAVE_MAP3D; }

double map3d_px_per_km(void) { return ASSETS_HAVE_MAP3D ? MAP3D_PX_PER_KM : 0.0; }

int map3d_project(double lon, double lat, double *px, double *py)
{
#if ASSETS_HAVE_MAP3D && MAP3D_NLON >= 2 && MAP3D_NLAT >= 2
    double fx = (lon - MAP3D_LON0) / MAP3D_STEP, fy = (lat - MAP3D_LAT0) / MAP3D_STEP, u, v;
    const float *p00, *p10, *p01, *p11;
    int i, j;
    /* отрицание ловит и NaN */
    if (!(fx >= 0.0 && fy >= 0.0 && fx <= MAP3D_NLON - 1 && fy <= MAP3D_NLAT - 1)) return 0;
    i = (int)fx;
    j = (int)fy;
    if (i > MAP3D_NLON - 2) i = MAP3D_NLON - 2;
    if (j > MAP3D_NLAT - 2) j = MAP3D_NLAT - 2;
    u = fx - i;
    v = fy - j;
    p00 = MAP3D_PTS[j * MAP3D_NLON + i];
    p10 = MAP3D_PTS[j * MAP3D_NLON + i + 1];
    p01 = MAP3D_PTS[(j + 1) * MAP3D_NLON + i];
    p11 = MAP3D_PTS[(j + 1) * MAP3D_NLON + i + 1];
    if (px) *px = (1 - v) * ((1 - u) * p00[0] + u * p10[0]) + v * ((1 - u) * p01[0] + u * p11[0]);
    if (py) *py = (1 - v) * ((1 - u) * p00[1] + u * p10[1]) + v * ((1 - u) * p01[1] + u * p11[1]);
    return 1;
#else
    (void)lon; (void)lat; (void)px; (void)py;
    return 0;
#endif
}

int map3d_city(int c, double *px, double *py)
{
#if ASSETS_HAVE_MAP3D
    if (c < 0 || c >= MAP3D_NCITY) return 0;
    if (px) *px = MAP3D_CITY[c][0];
    if (py) *py = MAP3D_CITY[c][1];
    return 1;
#else
    (void)c; (void)px; (void)py;
    return 0;
#endif
}
