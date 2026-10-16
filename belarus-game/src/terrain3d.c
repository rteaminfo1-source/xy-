/* terrain3d.c — программный 3D-рендер карты: перспектива, отсечение по ближней плоскости,
 * z-буфер, билинейная выборка из mip-уровней текстуры, земляные стенки плиты, сглаживание 2x2. */
#include "terrain3d.h"
#include "assets_gen.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef ASSETS_HAVE_TERRAIN          /* старый assets_gen.h без 3D-карты */
#define ASSETS_HAVE_TERRAIN 0
#define TERRAIN_X0 -320.0
#define TERRAIN_Y0 -272.0
#define TERRAIN_X1 317.0
#define TERRAIN_Y1 275.0
#define TERRAIN_ZBOTTOM -22.0
#define TERRAIN_OUTLINE_N 1
static const float TERRAIN_OUTLINE[1][3] = {{0.0f, 0.0f, 0.0f}};
#endif

#define FOV_DEG   34.0
#define TARGET_Z  9.0
#define NEAR_Z    0.5
#define MAXMIP    12
#define CHUNK     16

static const double DEG = 3.14159265358979323846 / 180.0;

/* ---------- данные ---------- */
static int     s_ready;
static Image   s_mip[MAXMIP];
static int     s_nmip;
static int     s_nx, s_ny;           /* сетка высот */
static float  *s_hz;                 /* высоты, км; строка 0 — юг (малые Y) */
static unsigned char *s_in;          /* 1 — внутри границы */
static int    *s_cells;              /* активные ячейки (индекс левого нижнего угла) */
static int     s_ncells;
typedef struct { int c0, n; float x0, y0, z0, x1, y1, z1; } Chunk;
static Chunk  *s_chunks;
static int     s_nchunks;
static double  s_x0 = TERRAIN_X0, s_y0 = TERRAIN_Y0, s_x1 = TERRAIN_X1, s_y1 = TERRAIN_Y1;
static double  s_outlineSign = 1.0;  /* +1 — контур против часовой стрелки */

/* ---------- кадр ---------- */
static float  *s_vx, *s_vy, *s_vz;   /* вершины в координатах камеры */
static float  *s_sx, *s_sy;          /* экран */
static unsigned *s_cb;               /* цвет (рабочий буфер, ss раз крупнее) */
static float  *s_zb;                 /* 1/глубина */
static int     s_bw, s_bh, s_bcap;

int t3d_ready(void) { return s_ready; }

void t3d_bounds(double *x0, double *y0, double *x1, double *y1) { *x0 = s_x0; *y0 = s_y0; *x1 = s_x1; *y1 = s_y1; }

void t3d_lonlat(double lon, double lat, double *X, double *Y)
{
    *X = (lon - 28.0) * 65.95;
    *Y = (lat - 53.7) * 111.32;
}

void t3d_free(void)
{
    for (int i = 0; i < s_nmip; i++) image_free(&s_mip[i]);
    s_nmip = 0;
    free(s_hz); free(s_in); free(s_cells); free(s_chunks);
    free(s_vx); free(s_vy); free(s_vz); free(s_sx); free(s_sy);
    free(s_cb); free(s_zb);
    s_hz = 0; s_in = 0; s_cells = 0; s_chunks = 0; s_vx = s_vy = s_vz = s_sx = s_sy = 0; s_cb = 0; s_zb = 0;
    s_bcap = 0;
    s_ready = 0;
}

static void build_mips(void)
{
    s_nmip = 1;
    while (s_nmip < MAXMIP) {
        const Image *a = &s_mip[s_nmip - 1];
        if (a->w < 4 || a->h < 4) break;
        int w = a->w / 2, h = a->h / 2;
        Image *b = &s_mip[s_nmip];
        if (!image_alloc(b, w, h)) break;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                const unsigned *p = a->px + (size_t)(2 * y) * a->w + 2 * x;
                unsigned q[4] = { p[0], p[1], p[a->w], p[a->w + 1] };
                unsigned rb = 0, ag = 0;
                for (int k = 0; k < 4; k++) { rb += q[k] & 0x00FF00FFu; ag += (q[k] >> 8) & 0x00FF00FFu; }
                b->px[(size_t)y * w + x] = ((rb >> 2) & 0x00FF00FFu) | (((ag >> 2) & 0x00FF00FFu) << 8);
            }
        s_nmip++;
    }
}

int t3d_init(const Image *tex, const Image *heights)
{
    t3d_free();
    if (!tex || !tex->px || !heights || !heights->px || heights->w < 4 || heights->h < 4) return 0;
    if (!image_alloc(&s_mip[0], tex->w, tex->h)) return 0;
    memcpy(s_mip[0].px, tex->px, sizeof(unsigned) * (size_t)tex->w * tex->h);
    build_mips();
    s_nx = heights->w; s_ny = heights->h;
    size_t n = (size_t)s_nx * s_ny;
    s_hz = malloc(sizeof(float) * n);
    s_in = malloc(n);
    s_cells = malloc(sizeof(int) * n);
    s_vx = malloc(sizeof(float) * n); s_vy = malloc(sizeof(float) * n); s_vz = malloc(sizeof(float) * n);
    s_sx = malloc(sizeof(float) * n); s_sy = malloc(sizeof(float) * n);
    int ncx = (s_nx - 1 + CHUNK - 1) / CHUNK, ncy = (s_ny - 1 + CHUNK - 1) / CHUNK;
    s_chunks = malloc(sizeof(Chunk) * (size_t)ncx * ncy);
    if (!s_hz || !s_in || !s_cells || !s_vx || !s_vy || !s_vz || !s_sx || !s_sy || !s_chunks) { t3d_free(); return 0; }
    for (int j = 0; j < s_ny; j++)
        for (int i = 0; i < s_nx; i++) {
            unsigned p = heights->px[(size_t)(s_ny - 1 - j) * s_nx + i];   /* картинка: север сверху */
            unsigned m = ((p >> 16) & 255u) << 8 | ((p >> 8) & 255u);
            s_hz[(size_t)j * s_nx + i] = (float)(m * 0.001);
            s_in[(size_t)j * s_nx + i] = (p & 255u) >= 128;
        }
    /* активные ячейки: хотя бы один угол внутри (с запасом в одну ячейку) — край режет альфа текстуры */
    s_ncells = 0;
    s_nchunks = 0;
    for (int cy = 0; cy < ncy; cy++)
        for (int cx = 0; cx < ncx; cx++) {
            Chunk *ch = &s_chunks[s_nchunks];
            ch->c0 = s_ncells; ch->n = 0;
            ch->x0 = ch->y0 = ch->z0 = 1e9f; ch->x1 = ch->y1 = ch->z1 = -1e9f;
            for (int j = cy * CHUNK; j < (cy + 1) * CHUNK && j < s_ny - 1; j++)
                for (int i = cx * CHUNK; i < (cx + 1) * CHUNK && i < s_nx - 1; i++) {
                    int any = 0;
                    for (int dj = -1; dj <= 2 && !any; dj++)
                        for (int di = -1; di <= 2 && !any; di++) {
                            int ii = i + di, jj = j + dj;
                            if (ii >= 0 && jj >= 0 && ii < s_nx && jj < s_ny && s_in[(size_t)jj * s_nx + ii]) any = 1;
                        }
                    if (!any) continue;
                    s_cells[s_ncells++] = j * s_nx + i;
                    ch->n++;
                    for (int k = 0; k < 4; k++) {
                        int ii = i + (k & 1), jj = j + (k >> 1);
                        float X = (float)(s_x0 + (s_x1 - s_x0) * ii / (s_nx - 1));
                        float Y = (float)(s_y0 + (s_y1 - s_y0) * jj / (s_ny - 1));
                        float Z = s_hz[(size_t)jj * s_nx + ii];
                        if (X < ch->x0) ch->x0 = X;
                        if (X > ch->x1) ch->x1 = X;
                        if (Y < ch->y0) ch->y0 = Y;
                        if (Y > ch->y1) ch->y1 = Y;
                        if (Z < ch->z0) ch->z0 = Z;
                        if (Z > ch->z1) ch->z1 = Z;
                    }
                }
            if (ch->n) s_nchunks++;
        }
    /* направление обхода контура: стенки смотрят наружу */
    double area = 0;
    for (int i = 0; i < TERRAIN_OUTLINE_N; i++) {
        const float *a = TERRAIN_OUTLINE[i], *b = TERRAIN_OUTLINE[(i + 1) % TERRAIN_OUTLINE_N];
        area += (double)a[0] * b[1] - (double)b[0] * a[1];
    }
    s_outlineSign = area >= 0 ? 1.0 : -1.0;
    s_ready = s_ncells > 0 && s_nmip > 0;
    return s_ready;
}

double t3d_height(double X, double Y)
{
    if (!s_ready) return TARGET_Z;
    double fx = (X - s_x0) / (s_x1 - s_x0) * (s_nx - 1), fy = (Y - s_y0) / (s_y1 - s_y0) * (s_ny - 1);
    if (fx < 0) fx = 0;
    if (fy < 0) fy = 0;
    if (fx > s_nx - 1.001) fx = s_nx - 1.001;
    if (fy > s_ny - 1.001) fy = s_ny - 1.001;
    int i = (int)fx, j = (int)fy;
    double tx = fx - i, ty = fy - j;
    const float *r0 = s_hz + (size_t)j * s_nx + i, *r1 = r0 + s_nx;
    return (r0[0] * (1 - tx) + r0[1] * tx) * (1 - ty) + (r1[0] * (1 - tx) + r1[1] * tx) * ty;
}

/* ---------- камера ---------- */
void t3d_cam_reset(T3Cam *c)
{
    c->yaw = 0.0;
    c->pitch = 52.0;
    c->zoom = 1.0;
    c->tx = (s_x0 + s_x1) / 2;
    c->ty = (s_y0 + s_y1) / 2;
}

void t3d_cam_clamp(T3Cam *c)
{
    if (!(c->pitch >= 14.0)) c->pitch = 14.0;      /* !(…) ловит и NaN */
    if (c->pitch > 89.0) c->pitch = 89.0;
    if (!(c->zoom >= 0.7)) c->zoom = 0.7;
    if (c->zoom > 9.0) c->zoom = 9.0;
    c->yaw = fmod(c->yaw, 360.0);
    if (!(c->yaw == c->yaw)) c->yaw = 0;
    if (c->yaw < 0) c->yaw += 360.0;
    if (!(c->tx >= s_x0)) c->tx = s_x0;
    if (c->tx > s_x1) c->tx = s_x1;
    if (!(c->ty >= s_y0)) c->ty = s_y0;
    if (c->ty > s_y1) c->ty = s_y1;
}

static void basis(double yaw, double pitch, double fwd[3], double right[3], double up[3])
{
    double sy = sin(yaw * DEG), cy = cos(yaw * DEG), sp = sin(pitch * DEG), cp = cos(pitch * DEG);
    /* камера стоит «сзади» цели: при yaw = 0 — к югу и смотрит на север */
    fwd[0] = sy * cp; fwd[1] = cy * cp; fwd[2] = -sp;
    right[0] = cy; right[1] = -sy; right[2] = 0;
    up[0] = right[1] * fwd[2] - right[2] * fwd[1];
    up[1] = right[2] * fwd[0] - right[0] * fwd[2];
    up[2] = right[0] * fwd[1] - right[1] * fwd[0];
}

/* расстояние, при котором страна целиком помещается в кадр (zoom = 1). Считается по контуру плиты
 * при yaw = 0, чтобы при вращении масштаб не «дышал»; кэшируется по размеру окна и наклону. */
static double fit_dist(double pitch, int w, int h, double f)
{
    static double cPitch = -1, cRes;
    static int cw, ch;
    if (pitch == cPitch && w == cw && h == ch) return cRes;
    double fwd[3], right[3], up[3];
    basis(0.0, pitch, fwd, right, up);
    double tx = (s_x0 + s_x1) / 2, ty = (s_y0 + s_y1) / 2;
    int step = TERRAIN_OUTLINE_N > 60 ? TERRAIN_OUTLINE_N / 60 : 1;
    double lo = 50.0, hi = 6000.0;
    for (int it = 0; it < 40; it++) {
        double d = (lo + hi) / 2, eye[3] = { tx - fwd[0] * d, ty - fwd[1] * d, TARGET_Z - fwd[2] * d };
        int ok = 1;
        for (int i = 0; i < TERRAIN_OUTLINE_N && ok; i += step)
            for (int z = 0; z < 2 && ok; z++) {
                const float *o = TERRAIN_OUTLINE[i];
                double p[3] = { o[0] - eye[0], o[1] - eye[1], (z ? o[2] : TERRAIN_ZBOTTOM) - eye[2] };
                double zv = p[0] * fwd[0] + p[1] * fwd[1] + p[2] * fwd[2];
                if (zv < 1.0) { ok = 0; break; }
                double sx = f * (p[0] * right[0] + p[1] * right[1] + p[2] * right[2]) / zv;
                double sy = f * (p[0] * up[0] + p[1] * up[1] + p[2] * up[2]) / zv;
                if (fabs(sx) > w * 0.47 || fabs(sy) > h * 0.46) ok = 0;
            }
        if (ok) hi = d; else lo = d;
    }
    cPitch = pitch; cw = w; ch = h; cRes = hi;
    return hi;
}

void t3d_view(const T3Cam *c, int w, int h, T3View *v)
{
    memset(v, 0, sizeof *v);
    v->w = w > 1 ? w : 1;
    v->h = h > 1 ? h : 1;
    v->f = (v->h / 2.0) / tan(FOV_DEG * DEG / 2.0);
    v->cx = v->w / 2.0;
    v->cy = v->h / 2.0;
    basis(c->yaw, c->pitch, v->fwd, v->right, v->up);
    /* «вписать» считаем для центра страны, чтобы масштаб не прыгал при сдвиге */
    double fd = fit_dist(c->pitch, v->w, v->h, v->f);
    v->dist = fd / (c->zoom > 0.1 ? c->zoom : 0.1);
    v->eye[0] = c->tx - v->fwd[0] * v->dist;
    v->eye[1] = c->ty - v->fwd[1] * v->dist;
    v->eye[2] = TARGET_Z - v->fwd[2] * v->dist;
    v->nearZ = NEAR_Z;
}

int t3d_project(const T3View *v, double X, double Y, double Z, double *sx, double *sy, double *depth)
{
    double p[3] = { X - v->eye[0], Y - v->eye[1], Z - v->eye[2] };
    double zv = p[0] * v->fwd[0] + p[1] * v->fwd[1] + p[2] * v->fwd[2];
    if (depth) *depth = zv;
    if (zv < v->nearZ) return 0;
    *sx = v->cx + v->f * (p[0] * v->right[0] + p[1] * v->right[1] + p[2] * v->right[2]) / zv;
    *sy = v->cy - v->f * (p[0] * v->up[0] + p[1] * v->up[1] + p[2] * v->up[2]) / zv;
    return 1;
}

int t3d_unproject(const T3View *v, double sx, double sy, double z, double *X, double *Y)
{
    double a = (sx - v->cx) / v->f, b = -(sy - v->cy) / v->f;
    double d[3] = { v->fwd[0] + a * v->right[0] + b * v->up[0], v->fwd[1] + a * v->right[1] + b * v->up[1],
                    v->fwd[2] + a * v->right[2] + b * v->up[2] };
    if (fabs(d[2]) < 1e-9) return 0;
    double t = (z - v->eye[2]) / d[2];
    if (t <= 0) return 0;
    *X = v->eye[0] + d[0] * t;
    *Y = v->eye[1] + d[1] * t;
    return 1;
}

/* ---------- растеризация ---------- */
typedef struct { float x, y, iw, a, b, c; } RV;      /* a, b, c — атрибуты, делённые на глубину */
typedef struct { float x, y, z, a, b, c; } VV;       /* вершина в координатах камеры */

static int s_rw, s_rh;                               /* размер рабочего буфера */
static double s_f, s_cx, s_cy;

enum { M_TERRAIN, M_WALL, M_BLOB };
static int   s_mode, s_mipL;
static float s_shade;
static float s_tint[3], s_grey;

#define MAX_BLOBS 24
static T3Blob s_blobs[MAX_BLOBS];
static int    s_nblobs;

void t3d_set_blobs(const T3Blob *b, int n)
{
    if (n < 0) n = 0;
    if (n > MAX_BLOBS) n = MAX_BLOBS;
    if (n) memcpy(s_blobs, b, sizeof(T3Blob) * (size_t)n);
    for (int i = 0; i < n; i++) {
        if (s_blobs[i].n > T3_BLOB_PTS) s_blobs[i].n = T3_BLOB_PTS;
        if (s_blobs[i].n < 3) s_blobs[i].n = 0;
    }
    s_nblobs = n;
}

int t3d_inside(double x, double y)
{
    int in = 0;
    for (int i = 0, j = TERRAIN_OUTLINE_N - 1; i < TERRAIN_OUTLINE_N; j = i++) {
        double xi = TERRAIN_OUTLINE[i][0], yi = TERRAIN_OUTLINE[i][1], xj = TERRAIN_OUTLINE[j][0], yj = TERRAIN_OUTLINE[j][1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
    }
    return in;
}

void t3d_outline_nearest(double x, double y, double *ox, double *oy)
{
    double best = 1e30;
    *ox = x; *oy = y;
    for (int i = 0; i < TERRAIN_OUTLINE_N; i++) {
        double dx = TERRAIN_OUTLINE[i][0] - x, dy = TERRAIN_OUTLINE[i][1] - y, d = dx * dx + dy * dy;
        if (d < best) { best = d; *ox = TERRAIN_OUTLINE[i][0]; *oy = TERRAIN_OUTLINE[i][1]; }
    }
}

double t3d_outline_hit(double x, double y, double dx, double dy)
{
    double best = -1;
    for (int i = 0; i < TERRAIN_OUTLINE_N; i++) {
        const float *a = TERRAIN_OUTLINE[i], *b = TERRAIN_OUTLINE[(i + 1) % TERRAIN_OUTLINE_N];
        double ex = b[0] - a[0], ey = b[1] - a[1];
        double den = dx * ey - dy * ex;
        if (fabs(den) < 1e-12) continue;
        double t = ((a[0] - x) * ey - (a[1] - y) * ex) / den;      /* вдоль луча */
        double u = ((a[0] - x) * dy - (a[1] - y) * dx) / den;      /* вдоль ребра */
        if (t > 0 && u >= 0 && u <= 1 && (best < 0 || t < best)) best = t;
    }
    return best;
}

static inline unsigned bilerp(unsigned a, unsigned b, unsigned f)   /* f: 0..256 */
{
    unsigned rb = ((a & 0x00FF00FFu) * (256 - f) + (b & 0x00FF00FFu) * f) >> 8;
    unsigned ag = (((a >> 8) & 0x00FF00FFu) * (256 - f) + ((b >> 8) & 0x00FF00FFu) * f) >> 8;
    return (rb & 0x00FF00FFu) | ((ag & 0x00FF00FFu) << 8);
}

static inline unsigned tex_sample(const Image *m, float u, float v)
{
    float tu = u * m->w - 0.5f, tv = v * m->h - 0.5f;
    /* за краем (и NaN) — прижимаем к краю; оба соседа выборки тоже внутри картинки */
    if (!(tu >= 0.0f)) tu = 0.0f;
    if (!(tv >= 0.0f)) tv = 0.0f;
    if (tu > (float)(m->w - 1)) tu = (float)(m->w - 1);
    if (tv > (float)(m->h - 1)) tv = (float)(m->h - 1);
    int x0 = (int)tu, y0 = (int)tv;
    unsigned fx = (unsigned)((tu - x0) * 256.0f), fy = (unsigned)((tv - y0) * 256.0f);
    int x1 = x0 + 1 < m->w ? x0 + 1 : x0, y1 = y0 + 1 < m->h ? y0 + 1 : y0;
    if (fx > 256) fx = 256;
    if (fy > 256) fy = 256;
    const unsigned *r0 = m->px + (size_t)y0 * m->w, *r1 = m->px + (size_t)y1 * m->w;
    return bilerp(bilerp(r0[x0], r0[x1], fx), bilerp(r1[x0], r1[x1], fx), fy);
}

/* слои грунта на стенке плиты по абсолютной высоте (км) */
static unsigned wall_color(float z, float s)
{
    float wob = 0.7f * sinf(s * 0.045f) + 0.4f * sinf(s * 0.13f + 1.7f);
    float zz = z + wob;
    unsigned c;
    if (zz > 3.2f) c = 0x4A3424;          /* дёрн и чернозём */
    else if (zz > -2.5f) c = 0x7A5838;    /* суглинок */
    else if (zz > -7.5f) c = 0xB49466;    /* песок */
    else if (zz > -12.0f) c = 0x8E5E44;   /* глина */
    else if (zz > -17.0f) c = 0x9C8770;   /* мел и мергель */
    else c = 0x5E5A57;                    /* гранитный фундамент */
    return c;
}

static void raster(const RV *p0, const RV *p1, const RV *p2)
{
    float area = (p1->x - p0->x) * (p2->y - p0->y) - (p2->x - p0->x) * (p1->y - p0->y);
    if (area == 0.0f || area != area) return;
    if (area < 0) { const RV *t = p1; p1 = p2; p2 = t; area = -area; }
    float minx = fminf(p0->x, fminf(p1->x, p2->x)), maxx = fmaxf(p0->x, fmaxf(p1->x, p2->x));
    float miny = fminf(p0->y, fminf(p1->y, p2->y)), maxy = fmaxf(p0->y, fmaxf(p1->y, p2->y));
    int x0 = (int)floorf(minx), x1 = (int)ceilf(maxx), y0 = (int)floorf(miny), y1 = (int)ceilf(maxy);
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > s_rw - 1) x1 = s_rw - 1;
    if (y1 > s_rh - 1) y1 = s_rh - 1;
    if (x0 > x1 || y0 > y1) return;
    /* рёберные функции: w0 — напротив p0 и т. д. */
    float A0 = p1->y - p2->y, B0 = p2->x - p1->x, C0 = p1->x * p2->y - p2->x * p1->y;
    float A1 = p2->y - p0->y, B1 = p0->x - p2->x, C1 = p2->x * p0->y - p0->x * p2->y;
    float A2 = p0->y - p1->y, B2 = p1->x - p0->x, C2 = p0->x * p1->y - p1->x * p0->y;
    float ia = 1.0f / area;
    /* плоскости атрибутов: q = qx * x + qy * y + q0 */
#define PLANE(f, gx, gy, g0) float gx = (A0 * p0->f + A1 * p1->f + A2 * p2->f) * ia, gy = (B0 * p0->f + B1 * p1->f + B2 * p2->f) * ia, \
                                   g0 = (C0 * p0->f + C1 * p1->f + C2 * p2->f) * ia;
    PLANE(iw, wx, wy, w0)
    PLANE(a, ax, ay, a0)
    PLANE(b, bx, by, b0)
    PLANE(c, cx_, cy_, c0)
#undef PLANE
    const Image *mip = &s_mip[s_mipL];
    for (int y = y0; y <= y1; y++) {
        float py = y + 0.5f, px = x0 + 0.5f;
        float e0 = A0 * px + B0 * py + C0, e1 = A1 * px + B1 * py + C1, e2 = A2 * px + B2 * py + C2;
        unsigned *crow = s_cb + (size_t)y * s_rw;
        float *zrow = s_zb + (size_t)y * s_rw;
        for (int x = x0; x <= x1; x++, px += 1.0f, e0 += A0, e1 += A1, e2 += A2) {
            if (e0 < 0 || e1 < 0 || e2 < 0) continue;
            float iw = wx * px + wy * py + w0;
            if (iw <= zrow[x]) continue;
            float z = 1.0f / iw;
            unsigned col;
            if (s_mode == M_TERRAIN) {
                float u = (ax * px + ay * py + a0) * z, v = (bx * px + by * py + b0) * z;
                unsigned t = tex_sample(mip, u, v);
                unsigned al = t >> 24;
                if (al < 128) continue;
                if (al < 255) {
                    unsigned r = ((t >> 16) & 255u) * 255u / al, g = ((t >> 8) & 255u) * 255u / al, b = (t & 255u) * 255u / al;
                    col = (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
                } else col = t & 0xFFFFFFu;
            } else if (s_mode == M_BLOB) {
                float u = (ax * px + ay * py + a0) * z, v = (bx * px + by * py + b0) * z;
                unsigned t = tex_sample(mip, u, v);
                unsigned al = t >> 24;
                float r, g, b;
                if (al < 16) { r = 150; g = 160; b = 110; }        /* вне Беларуси на текстуре — просто поле */
                else { r = (float)((t >> 16) & 255u) * 255.0f / al; g = (float)((t >> 8) & 255u) * 255.0f / al; b = (float)(t & 255u) * 255.0f / al; }
                float l = 0.3f * r + 0.59f * g + 0.11f * b;
                r += (l - r) * s_grey; g += (l - g) * s_grey; b += (l - b) * s_grey;
                r *= s_tint[0]; g *= s_tint[1]; b *= s_tint[2];
                unsigned R = r > 255 ? 255u : r < 0 ? 0u : (unsigned)r, Gc = g > 255 ? 255u : g < 0 ? 0u : (unsigned)g, B = b > 255 ? 255u : b < 0 ? 0u : (unsigned)b;
                col = R << 16 | Gc << 8 | B;
            } else {
                float wz = (cx_ * px + cy_ * py + c0) * z, s = (ax * px + ay * py + a0) * z;
                unsigned c = wall_color(wz, s);
                float k = s_shade;
                unsigned r = (unsigned)(((c >> 16) & 255) * k), g = (unsigned)(((c >> 8) & 255) * k), b = (unsigned)((c & 255) * k);
                col = (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
            }
            zrow[x] = iw;
            crow[x] = col;
        }
    }
}

static inline RV to_screen(const VV *v)
{
    RV r;
    float iw = 1.0f / v->z;
    r.x = (float)(s_cx + s_f * v->x * iw);
    r.y = (float)(s_cy - s_f * v->y * iw);
    r.iw = iw;
    r.a = v->a * iw; r.b = v->b * iw; r.c = v->c * iw;
    return r;
}

/* mip-уровень по отношению площадей текстуры и экрана */
static int pick_mip(const VV *v, int n, const RV *s)
{
    if (s_mode == M_WALL) return 0;
    double ta = 0, sa = 0;
    for (int i = 1; i + 1 < n; i++) {
        ta += fabs((v[i].a - v[0].a) * (v[i + 1].b - v[0].b) - (v[i + 1].a - v[0].a) * (v[i].b - v[0].b));
        sa += fabs((s[i].x - s[0].x) * (s[i + 1].y - s[0].y) - (s[i + 1].x - s[0].x) * (s[i].y - s[0].y));
    }
    ta *= (double)s_mip[0].w * s_mip[0].h;
    if (sa < 1e-6) return s_nmip - 1;
    double r = ta / sa;
    int L = r <= 1.0 ? 0 : (int)(0.5 * log2(r) + 0.25);
    return L < 0 ? 0 : L >= s_nmip ? s_nmip - 1 : L;
}

static void draw_poly(const VV *v, int n)
{
    VV clip[8];
    int m = 0, allIn = 1;
    for (int i = 0; i < n; i++) if (v[i].z < NEAR_Z) { allIn = 0; break; }
    if (allIn) { memcpy(clip, v, sizeof(VV) * n); m = n; }
    else {
        for (int i = 0; i < n && m < 7; i++) {
            const VV *a = &v[i], *b = &v[(i + 1) % n];
            int ina = a->z >= NEAR_Z, inb = b->z >= NEAR_Z;
            if (ina) clip[m++] = *a;
            if (ina != inb && m < 8) {
                float t = (NEAR_Z - a->z) / (b->z - a->z);
                VV c = { a->x + (b->x - a->x) * t, a->y + (b->y - a->y) * t, NEAR_Z,
                         a->a + (b->a - a->a) * t, a->b + (b->b - a->b) * t, a->c + (b->c - a->c) * t };
                clip[m++] = c;
            }
        }
    }
    if (m < 3) return;
    RV s[8];
    for (int i = 0; i < m; i++) s[i] = to_screen(&clip[i]);
    s_mipL = pick_mip(clip, m, s);
    for (int i = 1; i + 1 < m; i++) raster(&s[0], &s[i], &s[i + 1]);
}

static void view_pt(const T3View *v, double X, double Y, double Z, VV *o)
{
    double p[3] = { X - v->eye[0], Y - v->eye[1], Z - v->eye[2] };
    o->x = (float)(p[0] * v->right[0] + p[1] * v->right[1] + p[2] * v->right[2]);
    o->y = (float)(p[0] * v->up[0] + p[1] * v->up[1] + p[2] * v->up[2]);
    o->z = (float)(p[0] * v->fwd[0] + p[1] * v->fwd[1] + p[2] * v->fwd[2]);
}

/* блок целиком вне кадра? */
static int chunk_hidden(const T3View *v, const Chunk *ch)
{
    int left = 0, right = 0, top = 0, bottom = 0, behind = 0;
    for (int k = 0; k < 8; k++) {
        VV p;
        view_pt(v, k & 1 ? ch->x1 : ch->x0, k & 2 ? ch->y1 : ch->y0, k & 4 ? ch->z1 : ch->z0, &p);
        if (p.z < NEAR_Z) { behind++; continue; }
        double sx = s_cx + s_f * p.x / p.z, sy = s_cy - s_f * p.y / p.z;
        left += sx < 0; right += sx > s_rw; top += sy < 0; bottom += sy > s_rh;
    }
    if (behind == 8) return 1;
    if (behind) return 0;
    return left == 8 || right == 8 || top == 8 || bottom == 8;
}

static int ensure_buffers(int w, int h)
{
    size_t need = (size_t)w * h;
    if ((int)need > s_bcap) {
        free(s_cb); free(s_zb);
        s_cb = malloc(sizeof(unsigned) * need);
        s_zb = malloc(sizeof(float) * need);
        if (!s_cb || !s_zb) { free(s_cb); free(s_zb); s_cb = 0; s_zb = 0; s_bcap = 0; return 0; }
        s_bcap = (int)need;
    }
    s_bw = w; s_bh = h;
    return 1;
}

void t3d_render(const T3View *v, unsigned *dst, int stride, unsigned bg, int ss)
{
    if (ss < 1) ss = 1;
    if (ss > 2) ss = 2;
    int W = v->w * ss, H = v->h * ss;
    if (!s_ready || !ensure_buffers(W, H)) {
        for (int y = 0; y < v->h; y++) for (int x = 0; x < v->w; x++) dst[(size_t)y * stride + x] = bg;
        return;
    }
    s_rw = W; s_rh = H;
    s_f = v->f * ss; s_cx = v->cx * ss; s_cy = v->cy * ss;
    /* фон: лёгкий вертикальный градиент */
    for (int y = 0; y < H; y++) {
        int k = 255 - 26 * y / (H > 1 ? H - 1 : 1);
        unsigned r = ((bg >> 16) & 255) * (unsigned)k / 255, g = ((bg >> 8) & 255) * (unsigned)k / 255, b = (bg & 255) * (unsigned)k / 255;
        unsigned c = r << 16 | g << 8 | b;
        unsigned *row = s_cb + (size_t)y * W;
        for (int x = 0; x < W; x++) row[x] = c;
    }
    memset(s_zb, 0, sizeof(float) * (size_t)W * H);

    /* вершины сетки в координаты камеры */
    double dx = (s_x1 - s_x0) / (s_nx - 1), dy = (s_y1 - s_y0) / (s_ny - 1);
    for (int j = 0; j < s_ny; j++)
        for (int i = 0; i < s_nx; i++) {
            size_t k = (size_t)j * s_nx + i;
            VV p;
            view_pt(v, s_x0 + dx * i, s_y0 + dy * j, s_hz[k], &p);
            s_vx[k] = p.x; s_vy[k] = p.y; s_vz[k] = p.z;
        }

    /* стенки плиты — до рельефа: их перекроет z-буфер только там, где надо */
    s_mode = M_WALL;
    {
        const double L[3] = { -0.5786, -0.5786, 0.5736 };   /* к солнцу: азимут 225°, высота 35° */
        double cum = 0;
        for (int i = 0; i < TERRAIN_OUTLINE_N; i++) {
            const float *a = TERRAIN_OUTLINE[i], *b = TERRAIN_OUTLINE[(i + 1) % TERRAIN_OUTLINE_N];
            double ex = b[0] - a[0], ey = b[1] - a[1], len = sqrt(ex * ex + ey * ey);
            if (len < 1e-6) continue;
            double nx = ey / len * s_outlineSign, ny = -ex / len * s_outlineSign;    /* наружу */
            double mx = (a[0] + b[0]) / 2 - v->eye[0], my = (a[1] + b[1]) / 2 - v->eye[1];
            double s0 = cum;
            cum += len;
            if (nx * mx + ny * my >= 0) continue;                                    /* стенка смотрит от камеры */
            double d = nx * L[0] + ny * L[1];
            s_shade = (float)(0.62 + 0.38 * (d > 0 ? d : 0) - 0.12 * (d < 0 ? -d : 0));
            VV q[4];
            view_pt(v, a[0], a[1], a[2], &q[0]); q[0].a = (float)s0; q[0].c = a[2];
            view_pt(v, b[0], b[1], b[2], &q[1]); q[1].a = (float)cum; q[1].c = b[2];
            view_pt(v, b[0], b[1], TERRAIN_ZBOTTOM, &q[2]); q[2].a = (float)cum; q[2].c = (float)TERRAIN_ZBOTTOM;
            view_pt(v, a[0], a[1], TERRAIN_ZBOTTOM, &q[3]); q[3].a = (float)s0; q[3].c = (float)TERRAIN_ZBOTTOM;
            for (int k = 0; k < 4; k++) q[k].b = 0;
            draw_poly(q, 4);
        }
    }

    /* рельеф */
    s_mode = M_TERRAIN;
    float du = 1.0f / (s_nx - 1), dv = 1.0f / (s_ny - 1);
    for (int c = 0; c < s_nchunks; c++) {
        const Chunk *ch = &s_chunks[c];
        if (chunk_hidden(v, ch)) continue;
        for (int n = 0; n < ch->n; n++) {
            int k = s_cells[ch->c0 + n];
            int i = k % s_nx, j = k / s_nx;
            int id[4] = { k, k + 1, k + s_nx, k + s_nx + 1 };
            VV q[4];
            for (int t = 0; t < 4; t++) {
                int ii = i + (t & 1), jj = j + (t >> 1);
                q[t].x = s_vx[id[t]]; q[t].y = s_vy[id[t]]; q[t].z = s_vz[id[t]];
                q[t].a = ii * du;
                q[t].b = 1.0f - jj * dv;     /* строка 0 текстуры — север */
                q[t].c = 0;
            }
            VV t0[3] = { q[0], q[1], q[3] }, t1[3] = { q[0], q[3], q[2] };
            draw_poly(t0, 3);
            draw_poly(t1, 3);
        }
    }

    /* присоединённые земли: террасы с текстурой и стенками */
    for (int bi = 0; bi < s_nblobs; bi++) {
        const T3Blob *bl = &s_blobs[bi];
        if (bl->n < 3) continue;
        const double L[3] = { -0.5786, -0.5786, 0.5736 };
        /* стенки */
        s_mode = M_WALL;
        double cum = bi * 97.0;
        for (int i = 0; i < bl->n; i++) {
            int i2 = (i + 1) % bl->n;
            double ex = bl->x[i2] - bl->x[i], ey = bl->y[i2] - bl->y[i], len = sqrt(ex * ex + ey * ey);
            if (len < 1e-6) continue;
            double nx = ey / len, ny = -ex / len;                       /* точки идут против часовой: наружу — вправо */
            double mx = (bl->x[i] + bl->x[i2]) / 2 - v->eye[0], my = (bl->y[i] + bl->y[i2]) / 2 - v->eye[1];
            double s0 = cum;
            cum += len;
            if (nx * mx + ny * my >= 0) continue;
            double d = nx * L[0] + ny * L[1];
            s_shade = (float)(0.62 + 0.38 * (d > 0 ? d : 0) - 0.12 * (d < 0 ? -d : 0));
            VV q[4];
            view_pt(v, bl->x[i], bl->y[i], bl->top, &q[0]); q[0].a = (float)s0; q[0].c = bl->top;
            view_pt(v, bl->x[i2], bl->y[i2], bl->top, &q[1]); q[1].a = (float)cum; q[1].c = bl->top;
            view_pt(v, bl->x[i2], bl->y[i2], TERRAIN_ZBOTTOM, &q[2]); q[2].a = (float)cum; q[2].c = (float)TERRAIN_ZBOTTOM;
            view_pt(v, bl->x[i], bl->y[i], TERRAIN_ZBOTTOM, &q[3]); q[3].a = (float)s0; q[3].c = (float)TERRAIN_ZBOTTOM;
            for (int k = 0; k < 4; k++) q[k].b = 0;
            draw_poly(q, 4);
        }
        /* верх: веер от центра, текстура — участок вида сверху со сдвигом */
        s_mode = M_BLOB;
        s_tint[0] = bl->tint[0]; s_tint[1] = bl->tint[1]; s_tint[2] = bl->tint[2];
        s_grey = bl->grey;
        double W = s_x1 - s_x0, H = s_y1 - s_y0;
        VV c;
        view_pt(v, bl->cx, bl->cy, bl->top, &c);
        c.a = (float)((bl->cx + bl->ux - s_x0) / W); c.b = (float)((s_y1 - (bl->cy + bl->uy)) / H); c.c = 0;
        for (int i = 0; i < bl->n; i++) {
            int i2 = (i + 1) % bl->n;
            VV t[3];
            t[0] = c;
            view_pt(v, bl->x[i], bl->y[i], bl->top, &t[1]);
            t[1].a = (float)((bl->x[i] + bl->ux - s_x0) / W); t[1].b = (float)((s_y1 - (bl->y[i] + bl->uy)) / H); t[1].c = 0;
            view_pt(v, bl->x[i2], bl->y[i2], bl->top, &t[2]);
            t[2].a = (float)((bl->x[i2] + bl->ux - s_x0) / W); t[2].b = (float)((s_y1 - (bl->y[i2] + bl->uy)) / H); t[2].c = 0;
            draw_poly(t, 3);
        }
    }

    /* вывод с усреднением 2x2 */
    if (ss == 1) {
        for (int y = 0; y < v->h; y++) memcpy(dst + (size_t)y * stride, s_cb + (size_t)y * W, sizeof(unsigned) * v->w);
    } else {
        for (int y = 0; y < v->h; y++) {
            const unsigned *r0 = s_cb + (size_t)(2 * y) * W, *r1 = r0 + W;
            unsigned *o = dst + (size_t)y * stride;
            for (int x = 0; x < v->w; x++) {
                unsigned q[4] = { r0[2 * x], r0[2 * x + 1], r1[2 * x], r1[2 * x + 1] };
                unsigned rb = 0, g = 0;
                for (int k = 0; k < 4; k++) { rb += q[k] & 0x00FF00FFu; g += q[k] & 0x0000FF00u; }
                o[x] = ((rb >> 2) & 0x00FF00FFu) | ((g >> 2) & 0x0000FF00u);
            }
        }
    }
}
