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
static unsigned char *s_sh;          /* тень плиты на плоскости земель государств, 0..255 */
static int     s_shW, s_shH, s_shDirty = 1;
static float   s_shX0, s_shY0;
/* Плита по кускам (Chunk): 0 — земель нет, 1 — одна земля без границ рядом (оттенок без поиска),
 * 2 — рядом граница (поиск земли и линии в каждой точке). Считается при смене земель. */
static unsigned char *s_chMode, *s_chTop;
static int     s_chDirty = 1, s_slabMode, s_slabTop;
static unsigned s_slabPlain, s_slabF;   /* режим 1 без штриховки: цвет земли и доля оттенка 0..256 */

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
    free(s_sh);
    s_sh = 0;
    s_shDirty = 1;                       /* тень земель — от новой плиты */
    free(s_chMode); free(s_chTop);
    s_chMode = s_chTop = 0;
    s_chDirty = 1;
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
    /* отдаление до 0.35 — целиком видны земли государств вокруг (Речь Посполитая, Российская империя) */
    if (!(c->zoom >= 0.35)) c->zoom = 0.35;
    if (c->zoom > 9.0) c->zoom = 9.0;
    c->yaw = fmod(c->yaw, 360.0);
    if (!(c->yaw == c->yaw)) c->yaw = 0;
    if (c->yaw < 0) c->yaw += 360.0;
    /* цель — над плитой или над соседними землями (до ~250 км от плиты), но тем ближе к плите, чем ближе камера */
    double m = c->zoom < 1.0 ? 250.0 : 250.0 / c->zoom;
    if (!(c->tx >= s_x0 - m)) c->tx = s_x0 - m;
    if (c->tx > s_x1 + m) c->tx = s_x1 + m;
    if (!(c->ty >= s_y0 - m)) c->ty = s_y0 - m;
    if (c->ty > s_y1 + m) c->ty = s_y1 + m;
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

enum { M_TERRAIN, M_WALL, M_BLOB, M_FLAT };
static int   s_mode, s_mipL;
static float s_shade;
static float s_tint[3], s_grey;
static unsigned s_flat;                              /* M_FLAT: готовый цвет */

#define MAX_BLOBS 24
static T3Blob s_blobs[MAX_BLOBS];
static int    s_nblobs;

/* ---------- земли государств ---------- */
#define AG_CELL    6.0f      /* ячейка сетки поиска земли, км */
#define AG_MARGIN  7.0f      /* рёбра с линией попадают и в соседние ячейки — запас под ширину линии */
#define AREA_WALL  2.6       /* стенка по краю плиты, км */
#define BOARD_WALL 9.0       /* стенка по краю доски — толщина «столешницы», км */
#define STRIPE_KM  24.0f     /* шаг штриховки, км */
#define SLAB_TOL   7.0       /* ребро ближе к контуру плиты — граница Беларуси, линия не нужна */
#define SH_CELL    2.0f      /* сетка тени плиты Беларуси на плоскости земель, км */

typedef struct { float x0, y0, x1, y1; unsigned char area, line, wall; } AEdge;
typedef struct { float r, g, b; unsigned rgb, srgb; } ACol;      /* плита; она же и вторая полоса штриховки */

static T3Area   s_areas[T3_MAX_AREAS];   /* по возрастанию prio: бит i маски — земля s_areas[i] */
static ACol     s_acol[T3_MAX_AREAS];
static int      s_nareas;
static unsigned s_plateMask, s_wholeMask;   /* земли с плитой; земли n = 0 (вся плита Беларуси) */
static float    s_bx0, s_by0, s_bx1, s_by1;  /* «доска» */
static AEdge   *s_ae;
static int      s_nae;
static int      s_agW, s_agH;
static float    s_agX0, s_agY0;
static unsigned *s_agIn;                 /* земли, в которых лежит центр ячейки */
static int     *s_agOff;                 /* рёбра у ячейки c: s_agE[s_agOff[c] .. s_agOff[c + 1]) */
static unsigned short *s_agE;
/* мелкая сетка AF_CELL (3×3 в ячейке AG_CELL): земли в центре и далеко ли граница — в большинстве точек
 * поиск по рёбрам не нужен */
#define AF_CELL    2.0f
#define AF_HALF    1.4143f   /* полудиагональ мелкой ячейки, км */
#define AF_STEP    0.125f    /* шаг расстояния до линии в s_afInfo, км */
static unsigned short *s_afMask;
static unsigned char  *s_afInfo;         /* бит 7 — ребро проходит через ячейку; биты 0..6 — до линии, шаги AF_STEP */
static int      s_afW, s_afH;
static void     build_fine_grid(void);
static float    s_lineHalf;              /* полуширина линии границы, пикселей рабочего буфера */
static float    s_kmScale;               /* км на пиксель рабочего буфера на глубине 1 км (с поправкой на наклон) */
static int      s_slabOn;                /* земли видны и на плите Беларуси */

static inline unsigned bilerp(unsigned a, unsigned b, unsigned f)   /* f: 0..256 */
{
    unsigned rb = ((a & 0x00FF00FFu) * (256 - f) + (b & 0x00FF00FFu) * f) >> 8;
    unsigned ag = (((a >> 8) & 0x00FF00FFu) * (256 - f) + ((b >> 8) & 0x00FF00FFu) * f) >> 8;
    return (rb & 0x00FF00FFu) | ((ag & 0x00FF00FFu) << 8);
}

static inline unsigned frac256(float t) { return t <= 0 ? 0u : t >= 1 ? 256u : (unsigned)(t * 256.0f); }

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

/* ---------- земли государств: сетка поиска, тень, цвет точки ---------- */
static double outline_dist(double x, double y)
{
    double best = 1e30;
    for (int i = 0; i < TERRAIN_OUTLINE_N; i++) {
        const float *a = TERRAIN_OUTLINE[i], *b = TERRAIN_OUTLINE[(i + 1) % TERRAIN_OUTLINE_N];
        double ex = b[0] - a[0], ey = b[1] - a[1], qx = x - a[0], qy = y - a[1], l2 = ex * ex + ey * ey;
        double s = l2 > 0 ? (qx * ex + qy * ey) / l2 : 0;
        if (s < 0) s = 0;
        if (s > 1) s = 1;
        double dx = qx - s * ex, dy = qy - s * ey, d = dx * dx + dy * dy;
        if (d < best) best = d;
    }
    return sqrt(best);
}

static int on_board_edge(float x0, float y0, float x1, float y1)
{
    const float e = 0.6f;
    return (fabsf(x0 - s_bx0) < e && fabsf(x1 - s_bx0) < e) || (fabsf(x0 - s_bx1) < e && fabsf(x1 - s_bx1) < e) ||
           (fabsf(y0 - s_by0) < e && fabsf(y1 - s_by0) < e) || (fabsf(y0 - s_by1) < e && fabsf(y1 - s_by1) < e);
}

/* расстояние от точки до отрезка, км */
static inline float seg_dist(float x, float y, const AEdge *e)
{
    float ex = e->x1 - e->x0, ey = e->y1 - e->y0, qx = x - e->x0, qy = y - e->y0, l2 = ex * ex + ey * ey;
    float s = l2 > 0 ? (qx * ex + qy * ey) / l2 : 0.0f;
    if (s < 0) s = 0;
    if (s > 1) s = 1;
    float dx = qx - s * ex, dy = qy - s * ey;
    return sqrtf(dx * dx + dy * dy);
}

static int in_slab_mask(double X, double Y)
{
    int i = (int)lround((X - s_x0) / (s_x1 - s_x0) * (s_nx - 1)), j = (int)lround((Y - s_y0) / (s_y1 - s_y0) * (s_ny - 1));
    return i >= 0 && j >= 0 && i < s_nx && j < s_ny && s_in[(size_t)j * s_nx + i];
}

/* Тень плиты Беларуси на плоскости земель: солнце то же, что у стенок (юго-запад, 35°).
 * Считается один раз: плита и солнце не двигаются. Под самой плитой — «тень» тоже, после
 * размытия она даёт мягкое затенение у подножия стенок со всех сторон. */
static void build_shadow(void)
{
    s_shDirty = 0;
    free(s_sh);
    s_sh = 0;
    if (!s_ready) return;
    const float pad = 50.0f;
    s_shX0 = (float)s_x0 - pad; s_shY0 = (float)s_y0 - pad;
    s_shW = (int)((s_x1 - s_x0 + 2 * pad) / SH_CELL) + 2;
    s_shH = (int)((s_y1 - s_y0 + 2 * pad) / SH_CELL) + 2;
    size_t n = (size_t)s_shW * s_shH;
    float *v = malloc(sizeof(float) * n), *t = malloc(sizeof(float) * n);
    s_sh = malloc(n);
    if (!v || !t || !s_sh) { free(v); free(t); free(s_sh); s_sh = 0; return; }
    const double kx = -0.5786 / 0.5736, ky = -0.5786 / 0.5736;     /* к солнцу на 1 км подъёма */
    for (int j = 0; j < s_shH; j++)
        for (int i = 0; i < s_shW; i++) {
            double X = s_shX0 + i * SH_CELL, Y = s_shY0 + j * SH_CELL;
            float s = 0;
            if (in_slab_mask(X, Y)) s = 1;
            else
                for (double dz = 0.5; dz < 19.0; dz += 0.75) {
                    double qx = X + kx * dz, qy = Y + ky * dz;
                    if (qx < s_x0 || qy < s_y0) break;                 /* луч ушёл за плиту — дальше её нет */
                    if (in_slab_mask(qx, qy) && t3d_height(qx, qy) >= T3_AREA_Z + dz) { s = 1; break; }
                }
            v[(size_t)j * s_shW + i] = s;
        }
    /* размытие 5×5 дважды — полутень */
    for (int pass = 0; pass < 2; pass++) {
        for (int j = 0; j < s_shH; j++)
            for (int i = 0; i < s_shW; i++) {
                float s = 0;
                int c = 0;
                for (int k = -2; k <= 2; k++) if (i + k >= 0 && i + k < s_shW) { s += v[(size_t)j * s_shW + i + k]; c++; }
                t[(size_t)j * s_shW + i] = s / c;
            }
        for (int j = 0; j < s_shH; j++)
            for (int i = 0; i < s_shW; i++) {
                float s = 0;
                int c = 0;
                for (int k = -2; k <= 2; k++) if (j + k >= 0 && j + k < s_shH) { s += t[(size_t)(j + k) * s_shW + i]; c++; }
                v[(size_t)j * s_shW + i] = s / c;
            }
    }
    for (size_t k = 0; k < n; k++) s_sh[k] = (unsigned char)(v[k] * 255.0f + 0.5f);
    free(v);
    free(t);
}

static inline float shadow_at(float X, float Y)
{
    float fx = (X - s_shX0) * (1.0f / SH_CELL), fy = (Y - s_shY0) * (1.0f / SH_CELL);
    if (!(fx >= 0 && fy >= 0 && fx < s_shW - 1 && fy < s_shH - 1)) return 0;
    int i = (int)fx, j = (int)fy;
    float tx = fx - i, ty = fy - j;
    const unsigned char *p = s_sh + (size_t)j * s_shW + i;
    return ((p[0] * (1 - tx) + p[1] * tx) * (1 - ty) + (p[s_shW] * (1 - tx) + p[s_shW + 1] * tx) * ty) * (1.0f / 255.0f);
}

void t3d_set_areas(const T3Area *a, int n, double bx0, double by0, double bx1, double by1)
{
    if (n < 0 || !a) n = 0;
    if (n > T3_MAX_AREAS) n = T3_MAX_AREAS;
    /* по возрастанию prio, при равных — в порядке списка */
    s_nareas = 0;
    for (int i = 0; i < n; i++) {
        int j = s_nareas++;
        while (j > 0 && s_areas[j - 1].prio > a[i].prio) { s_areas[j] = s_areas[j - 1]; j--; }
        s_areas[j] = a[i];
        if (s_areas[j].n > T3_AREA_PTS) s_areas[j].n = T3_AREA_PTS;
        if (s_areas[j].n < 3) s_areas[j].n = 0;
    }
    s_bx0 = (float)bx0; s_by0 = (float)by0; s_bx1 = (float)bx1; s_by1 = (float)by1;
    s_chDirty = 1;
    s_plateMask = s_wholeMask = 0;
    s_nae = 0;
    free(s_ae); free(s_agIn); free(s_agOff); free(s_agE); free(s_afMask); free(s_afInfo);
    s_afMask = 0; s_afInfo = 0;
    s_afW = s_afH = 0;
    s_ae = 0; s_agIn = 0; s_agOff = 0; s_agE = 0;
    s_agW = s_agH = 0;
    if (!s_nareas) return;
    int total = 0;
    for (int i = 0; i < s_nareas; i++) total += s_areas[i].n;
    s_ae = malloc(sizeof(AEdge) * (size_t)(total > 0 ? total : 1));
    if (!s_ae) { s_nareas = 0; return; }
    for (int i = 0; i < s_nareas; i++) {
        T3Area *A = &s_areas[i];
        ACol *c = &s_acol[i];
        c->r = (float)(A->color >> 16 & 255); c->g = (float)(A->color >> 8 & 255); c->b = (float)(A->color & 255);
        c->rgb = A->color & 0xFFFFFFu;
        c->srgb = bilerp(c->rgb, 0, 87);                  /* вторая полоса — на треть темнее */
        if (A->slab < 0) A->slab = 0;
        if (A->slab > 1) A->slab = 1;
        if (!A->n) { s_wholeMask |= 1u << i; continue; }
        if (A->plate) s_plateMask |= 1u << i;
        /* рёбра против часовой стрелки: наружу — вправо от ребра */
        double ar = 0;
        for (int k = 0; k < A->n; k++) {
            int k2 = (k + 1) % A->n;
            ar += (double)A->x[k] * A->y[k2] - (double)A->x[k2] * A->y[k];
        }
        for (int k = 0; k < A->n; k++) {
            int k2 = (k + 1) % A->n;
            AEdge *e = &s_ae[s_nae++];
            if (ar >= 0) { e->x0 = A->x[k]; e->y0 = A->y[k]; e->x1 = A->x[k2]; e->y1 = A->y[k2]; }
            else { e->x0 = A->x[k2]; e->y0 = A->y[k2]; e->x1 = A->x[k]; e->y1 = A->y[k]; }
            e->area = (unsigned char)i;
            int board = on_board_edge(e->x0, e->y0, e->x1, e->y1);
            int slab = !board && outline_dist(e->x0, e->y0) < SLAB_TOL && outline_dist(e->x1, e->y1) < SLAB_TOL &&
                       outline_dist((e->x0 + e->x1) / 2, (e->y0 + e->y1) / 2) < SLAB_TOL;
            e->line = !board && !slab;
            e->wall = !A->plate ? 0 : board ? 2 : e->line;      /* 2 — край доски: стенка выше */
        }
    }
    /* сетка поиска на всю доску с запасом */
    s_agX0 = s_bx0 - AG_MARGIN; s_agY0 = s_by0 - AG_MARGIN;
    s_agW = (int)ceilf((s_bx1 - s_bx0 + 2 * AG_MARGIN) / AG_CELL);
    s_agH = (int)ceilf((s_by1 - s_by0 + 2 * AG_MARGIN) / AG_CELL);
    if (s_agW < 1 || s_agH < 1 || s_agW > 4000 || s_agH > 4000) { s_agW = s_agH = 0; return; }
    size_t nc = (size_t)s_agW * s_agH;
    s_agIn = calloc(nc, sizeof(unsigned));
    s_agOff = calloc(nc + 1, sizeof(int));
    int *cur = malloc(sizeof(int) * nc);
    if (!s_agIn || !s_agOff || !cur) { free(cur); s_agW = s_agH = 0; return; }
    /* центры ячеек внутри земель: построчно, правило чёт-нечет */
    for (int cy = 0; cy < s_agH; cy++) {
        float yc = s_agY0 + (cy + 0.5f) * AG_CELL;
        for (int e0 = 0, i = 0; i < s_nareas; i++) {
            int ne = s_areas[i].n;
            float xs[T3_AREA_PTS];
            int k = 0;
            for (int q = e0; q < e0 + ne; q++) {
                const AEdge *e = &s_ae[q];
                if ((e->y0 > yc) != (e->y1 > yc)) xs[k++] = e->x0 + (yc - e->y0) * (e->x1 - e->x0) / (e->y1 - e->y0);
            }
            e0 += ne;
            for (int p = 1; p < k; p++) {
                float v = xs[p];
                int q = p - 1;
                while (q >= 0 && xs[q] > v) { xs[q + 1] = xs[q]; q--; }
                xs[q + 1] = v;
            }
            for (int p = 0; p + 1 < k; p += 2) {
                int c0 = (int)ceilf((xs[p] - s_agX0) / AG_CELL - 0.5f), c1 = (int)floorf((xs[p + 1] - s_agX0) / AG_CELL - 0.5f);
                if (c0 < 0) c0 = 0;
                if (c1 > s_agW - 1) c1 = s_agW - 1;
                for (int cx = c0; cx <= c1; cx++) s_agIn[(size_t)cy * s_agW + cx] |= 1u << i;
            }
        }
    }
    /* рёбра, проходящие через ячейку (с линией — и рядом с ней) */
    const float half = AG_CELL * 0.7072f;
    for (int pass = 0; pass < 2; pass++) {
        for (int q = 0; q < s_nae; q++) {
            const AEdge *e = &s_ae[q];
            float m = e->line ? AG_MARGIN : 0.05f;
            int cx0 = (int)floorf((fminf(e->x0, e->x1) - m - s_agX0) / AG_CELL), cx1 = (int)floorf((fmaxf(e->x0, e->x1) + m - s_agX0) / AG_CELL);
            int cy0 = (int)floorf((fminf(e->y0, e->y1) - m - s_agY0) / AG_CELL), cy1 = (int)floorf((fmaxf(e->y0, e->y1) + m - s_agY0) / AG_CELL);
            if (cx0 < 0) cx0 = 0;
            if (cy0 < 0) cy0 = 0;
            if (cx1 > s_agW - 1) cx1 = s_agW - 1;
            if (cy1 > s_agH - 1) cy1 = s_agH - 1;
            for (int cy = cy0; cy <= cy1; cy++)
                for (int cx = cx0; cx <= cx1; cx++) {
                    if (seg_dist(s_agX0 + (cx + 0.5f) * AG_CELL, s_agY0 + (cy + 0.5f) * AG_CELL, e) > m + half) continue;
                    size_t c = (size_t)cy * s_agW + cx;
                    if (pass == 0) s_agOff[c + 1]++;
                    else s_agE[cur[c]++] = (unsigned short)q;
                }
        }
        if (pass == 0) {
            for (size_t c = 0; c < nc; c++) s_agOff[c + 1] += s_agOff[c];
            s_agE = malloc(sizeof(unsigned short) * (size_t)(s_agOff[nc] > 0 ? s_agOff[nc] : 1));
            if (!s_agE) { free(cur); s_agW = s_agH = 0; return; }
            memcpy(cur, s_agOff, sizeof(int) * nc);
        }
    }
    free(cur);
    build_fine_grid();
}

/* Земли в точке (бит i — s_areas[i]) и ближайшая видимая линия границы: *ld — расстояние, км, *la — чья.
 * extra — земли, которые и так лежат в точке (n = 0 на плите Беларуси). Линия земли, накрытой в этой
 * точке землёй выше, не видна: часть государства и земли под властью соседа ложатся поверх чисто.
 * Линии дальше lim км не ищутся (*ld остаётся огромным). */
static inline unsigned area_mask_slow(float X, float Y, unsigned extra, float lim, float *ld, int *la)
{
    *ld = 1e9f;
    if (!(X >= s_agX0 && Y >= s_agY0)) return 0;
    int cx = (int)((X - s_agX0) * (1.0f / AG_CELL)), cy = (int)((Y - s_agY0) * (1.0f / AG_CELL));
    if (cx >= s_agW || cy >= s_agH) return 0;
    size_t c = (size_t)cy * s_agW + cx;
    unsigned in = s_agIn[c];
    int o0 = s_agOff[c], o1 = s_agOff[c + 1];
    if (o0 == o1) return in;
    /* от центра ячейки (там известно, где он) к точке: каждое пересечённое ребро меняет «внутри» */
    float mx = s_agX0 + (cx + 0.5f) * AG_CELL, my = s_agY0 + (cy + 0.5f) * AG_CELL;
    float px = X - mx, py = Y - my, best = 1e18f;
    float qx0 = fminf(X, mx), qx1 = fmaxf(X, mx), qy0 = fminf(Y, my), qy1 = fmaxf(Y, my);
    for (int k = o0; k < o1; k++) {
        const AEdge *e = &s_ae[s_agE[k]];
        /* рамки отрезка «центр — точка» и ребра не пересекаются — пересечения нет */
        if (fmaxf(e->x0, e->x1) < qx0 || fminf(e->x0, e->x1) > qx1 || fmaxf(e->y0, e->y1) < qy0 || fminf(e->y0, e->y1) > qy1) continue;
        float ex = e->x1 - e->x0, ey = e->y1 - e->y0;
        float den = px * ey - py * ex;
        if (den != 0.0f) {
            float wx = e->x0 - mx, wy = e->y0 - my;
            float t = (wx * ey - wy * ex) / den, u = (wx * py - wy * px) / den;
            if (t >= 0.0f && t < 1.0f && u >= 0.0f && u < 1.0f) in ^= 1u << e->area;
        }
    }
    unsigned all = in | extra;
    best = lim;
    for (int k = o0; k < o1; k++) {
        const AEdge *e = &s_ae[s_agE[k]];
        if (!e->line || all >> e->area >> 1) continue;
        /* быстрый отказ по рамке ребра */
        if (X < fminf(e->x0, e->x1) - best || X > fmaxf(e->x0, e->x1) + best ||
            Y < fminf(e->y0, e->y1) - best || Y > fmaxf(e->y0, e->y1) + best) continue;
        float d = seg_dist(X, Y, e);
        if (d < best) { best = d; *la = e->area; }
    }
    if (best >= lim) return in;
    *ld = best;
    return in;
}

/* то же, сначала по мелкой сетке: точка вдали от рёбер и линий — ответ готов */
static inline unsigned area_mask(float X, float Y, unsigned extra, float lim, float *ld, int *la)
{
    if (s_afInfo && X >= s_agX0 && Y >= s_agY0) {
        int fx = (int)((X - s_agX0) * (1.0f / AF_CELL)), fy = (int)((Y - s_agY0) * (1.0f / AF_CELL));
        if (fx < s_afW && fy < s_afH) {
            size_t f = (size_t)fy * s_afW + fx;
            unsigned inf = s_afInfo[f];
            if (!(inf & 0x80) && (float)(inf & 0x7F) * AF_STEP - AF_HALF > lim) { *ld = 1e9f; return s_afMask[f]; }
        }
    }
    return area_mask_slow(X, Y, extra, lim, ld, la);
}

/* мелкая сетка по готовой крупной */
static void build_fine_grid(void)
{
    s_afW = s_agW * 3; s_afH = s_agH * 3;
    size_t n = (size_t)s_afW * s_afH;
    s_afMask = malloc(sizeof(unsigned short) * n);
    s_afInfo = malloc(n);
    if (!s_afMask || !s_afInfo) { free(s_afMask); free(s_afInfo); s_afMask = 0; s_afInfo = 0; s_afW = s_afH = 0; return; }
    for (int fy = 0; fy < s_afH; fy++)
        for (int fx = 0; fx < s_afW; fx++) {
            float X = s_agX0 + (fx + 0.5f) * AF_CELL, Y = s_agY0 + (fy + 0.5f) * AF_CELL, ld;
            int la = 0;
            size_t f = (size_t)fy * s_afW + fx, c = (size_t)(fy / 3) * s_agW + fx / 3;
            s_afMask[f] = (unsigned short)area_mask_slow(X, Y, 0, 0, &ld, &la);
            /* в списке крупной ячейки — все линии ближе ~8 км и все рёбра, проходящие через неё */
            float dl = 127 * AF_STEP;
            int cross = 0;
            for (int k = s_agOff[c]; k < s_agOff[c + 1]; k++) {
                const AEdge *e = &s_ae[s_agE[k]];
                float d = seg_dist(X, Y, e);
                if (d < AF_HALF + 0.01f) cross = 1;
                if (e->line && d < dl) dl = d;
            }
            if (dl > 8.0f) dl = 8.0f;          /* дальше 8 км список неполон */
            s_afInfo[f] = (unsigned char)((cross ? 0x80 : 0) | (int)(dl / AF_STEP));
        }
}

static inline int top_area(unsigned m) { return 31 - __builtin_clz(m); }

/* штриховка: 0 — основной цвет, 1 — вторая полоса; foot — км на пиксель (размытие края полос) */
static inline float stripe(float X, float Y, float foot)
{
    float s = (X + Y) * (1.0f / STRIPE_KM);
    float f = s - floorf(s);
    float tri = 1.0f - 2.0f * fabsf(f - 0.5f);                 /* 0 на краю периода, 1 в середине */
    float w = foot * (1.0f / STRIPE_KM) * 2.5f;                /* ширина перехода в долях tri */
    if (w > 0.6f) return 0.38f;                                /* полосы мельче пикселя — средний тон */
    float m = (tri - 0.62f) / (w + 1e-6f) + 0.5f;
    m = m < 0 ? 0 : m > 1 ? 1 : m;
    if (w > 0.3f) m = 0.38f + (m - 0.38f) * (0.6f - w) / 0.3f;
    return m;
}

static inline unsigned pack_rgb(float r, float g, float b)
{
    unsigned R = r > 255 ? 255u : r < 0 ? 0u : (unsigned)r, Gc = g > 255 ? 255u : g < 0 ? 0u : (unsigned)g, B = b > 255 ? 255u : b < 0 ? 0u : (unsigned)b;
    return R << 16 | Gc << 8 | B;
}

/* оттенок земли a на плите Беларуси (со штриховкой у земель под властью соседа) */
static inline unsigned slab_tint(unsigned col, int a, float X, float Y, float kmpx)
{
    const T3Area *A = &s_areas[a];
    float t = A->slab;
    if (A->stripes) t += (0.46f - t) * stripe(X, Y, kmpx * 1.6f);
    return bilerp(col, A->color, frac256(t));
}

/* оттенок, штриховка и линии границ на плите Беларуси; kmpx — км на пиксель рабочего буфера */
static inline unsigned slab_color(unsigned col, float X, float Y, float kmpx)
{
    float ld;
    int la = 0;
    float hw = s_lineHalf * 1.2f * kmpx;
    unsigned mask = area_mask(X, Y, s_wholeMask, hw + kmpx, &ld, &la) | s_wholeMask;
    if (mask) col = slab_tint(col, top_area(mask), X, Y, kmpx);
    if (ld < hw + kmpx) col = bilerp(col, s_areas[la].line, frac256(((hw - ld) / kmpx + 0.5f) * 0.9f));
    return col;
}

/* режимы кусков плиты: граница ближе SLAB_NEAR км от куска — точный поиск в каждой точке */
#define SLAB_NEAR 9.0f
static void build_chunk_modes(void)
{
    s_chDirty = 0;
    free(s_chMode); free(s_chTop);
    s_chMode = calloc((size_t)(s_nchunks > 0 ? s_nchunks : 1), 1);
    s_chTop = calloc((size_t)(s_nchunks > 0 ? s_nchunks : 1), 1);
    if (!s_chMode || !s_chTop) { free(s_chMode); free(s_chTop); s_chMode = s_chTop = 0; return; }
    for (int c = 0; c < s_nchunks; c++) {
        const Chunk *ch = &s_chunks[c];
        float cx = (ch->x0 + ch->x1) * 0.5f, cy = (ch->y0 + ch->y1) * 0.5f;
        float r = 0.5f * sqrtf((ch->x1 - ch->x0) * (ch->x1 - ch->x0) + (ch->y1 - ch->y0) * (ch->y1 - ch->y0)) + SLAB_NEAR;
        int near = 0;
        for (int q = 0; q < s_nae && !near; q++) near = seg_dist(cx, cy, &s_ae[q]) < r;
        float ld;
        int la = 0;
        unsigned m = area_mask(cx, cy, s_wholeMask, 0, &ld, &la) | s_wholeMask;
        s_chMode[c] = (unsigned char)(near ? 2 : m ? 1 : 0);
        s_chTop[c] = (unsigned char)(m ? top_area(m) : 0);
    }
}

/* цвет плиты земель в точке */
static inline unsigned plate_color(unsigned mask, float X, float Y, float ld, int la, float kmpx, float foot)
{
    int a = top_area(mask);
    const ACol *c = &s_acol[a];
    unsigned col = c->rgb;
    if (s_areas[a].stripes) col = bilerp(col, c->srgb, frac256(stripe(X, Y, foot)));
    if (s_sh) {
        float sh = shadow_at(X, Y);
        if (sh > 0) col = bilerp(col, 0, frac256(0.55f * sh));
    }
    if (ld < s_lineHalf * kmpx + kmpx) col = bilerp(col, s_areas[la].line, frac256((s_lineHalf * kmpx - ld) / kmpx + 0.5f));
    /* у края доски — тонкий светлый кант, как обрез листа атласа */
    float d = fminf(fminf(X - s_bx0, s_bx1 - X), fminf(Y - s_by0, s_by1 - Y)), e = 1.6f * kmpx;
    if (d < e) col = bilerp(col, 0xF4EEE0u, frac256(0.55f * (1.0f - d / e)));
    return col;
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
                if (s_slabMode == 2)    /* земли государств на плите: u, v — доли вида сверху */
                    col = slab_color(col, (float)s_x0 + u * (float)(s_x1 - s_x0), (float)s_y1 - v * (float)(s_y1 - s_y0), z * s_kmScale);
                else if (s_slabMode == 1)
                    col = s_slabPlain ? bilerp(col, s_slabPlain, s_slabF)
                                      : slab_tint(col, s_slabTop, (float)s_x0 + u * (float)(s_x1 - s_x0), (float)s_y1 - v * (float)(s_y1 - s_y0), z * s_kmScale);
            } else if (s_mode == M_FLAT) {
                col = s_flat;
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
    if (s_mode == M_WALL || s_mode == M_FLAT) return 0;
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

/* стенки по внешнему краю плит земель (внутренние границы прячутся под соседней плитой) и по краю доски */
static void areas_walls(const T3View *v)
{
    const double L[3] = { -0.5786, -0.5786, 0.5736 };
    s_mode = M_FLAT;
    for (int q = 0; q < s_nae; q++) {
        const AEdge *e = &s_ae[q];
        if (!e->wall) continue;
        double ex = e->x1 - e->x0, ey = e->y1 - e->y0, len = sqrt(ex * ex + ey * ey);
        if (len < 1e-6) continue;
        double nx = ey / len, ny = -ex / len;                                   /* наружу (обход против часовой) */
        double mx = (e->x0 + e->x1) / 2 - v->eye[0], my = (e->y0 + e->y1) / 2 - v->eye[1];
        if (nx * mx + ny * my >= 0) continue;
        double d = nx * L[0] + ny * L[1];
        float k = (float)(0.50 + 0.30 * (d > 0 ? d : 0) - 0.10 * (d < 0 ? -d : 0));
        const ACol *c = &s_acol[e->area];
        s_flat = pack_rgb(c->r * k, c->g * k, c->b * k);
        double depth = e->wall == 2 ? BOARD_WALL : AREA_WALL;
        VV p[4];
        view_pt(v, e->x0, e->y0, T3_AREA_Z, &p[0]);
        view_pt(v, e->x1, e->y1, T3_AREA_Z, &p[1]);
        view_pt(v, e->x1, e->y1, T3_AREA_Z - depth, &p[2]);
        view_pt(v, e->x0, e->y0, T3_AREA_Z - depth, &p[3]);
        for (int i = 0; i < 4; i++) p[i].a = p[i].b = p[i].c = 0;
        draw_poly(p, 4);
    }
}

/* Плоскость земель: построчно луч из пикселя до плоскости Z = T3_AREA_Z. Камера без крена, поэтому
 * глубина вдоль строки постоянна, а точка на земле движется равномерно — без деления на пиксель. */
static void areas_plates(const T3View *v)
{
    double hgt = T3_AREA_Z - v->eye[2];
    if (hgt >= 0) return;
    float gx0 = s_agX0, gy0 = s_agY0, gx1 = s_agX0 + s_agW * AG_CELL, gy1 = s_agY0 + s_agH * AG_CELL;
    for (int y = 0; y < s_rh; y++) {
        double b = -((y + 0.5) - s_cy) / s_f;
        double D[3] = { v->fwd[0] + b * v->up[0], v->fwd[1] + b * v->up[1], v->fwd[2] + b * v->up[2] };
        if (D[2] > -1e-4) continue;                                             /* выше горизонта */
        double t = hgt / D[2];                                                  /* глубина */
        float iw = (float)(1.0 / t);
        double a0 = (0.5 - s_cx) / s_f, da = 1.0 / s_f;
        double X0 = v->eye[0] + (D[0] + a0 * v->right[0]) * t, dX = v->right[0] * da * t;
        double Y0 = v->eye[1] + (D[1] + a0 * v->right[1]) * t, dY = v->right[1] * da * t;
        /* пиксели строки, попадающие в сетку земель */
        double lo = 0, hi = s_rw - 1;
        if (fabs(dX) > 1e-12) {
            double p0 = (gx0 - X0) / dX, p1 = (gx1 - X0) / dX;
            if (p0 > p1) { double tt = p0; p0 = p1; p1 = tt; }
            if (p0 > lo) lo = p0;
            if (p1 < hi) hi = p1;
        } else if (X0 < gx0 || X0 >= gx1) continue;
        if (fabs(dY) > 1e-12) {
            double p0 = (gy0 - Y0) / dY, p1 = (gy1 - Y0) / dY;
            if (p0 > p1) { double tt = p0; p0 = p1; p1 = tt; }
            if (p0 > lo) lo = p0;
            if (p1 < hi) hi = p1;
        } else if (Y0 < gy0 || Y0 >= gy1) continue;
        int xa = (int)ceil(lo), xb = (int)floor(hi);
        if (xa > xb) continue;
        /* масштаб: вдоль строки и «в глубину» (наклонная плоскость сжата по вертикали) */
        double dl = sqrt(D[0] * D[0] + D[1] * D[1] + D[2] * D[2]);
        float kmH = (float)(t / s_f), kmV = (float)(t * dl * dl / (s_f * -D[2]));
        float kmpx = sqrtf(kmH * kmV), foot = kmV > kmH ? kmV : kmH, lim = (s_lineHalf + 1.0f) * kmpx;
        unsigned *crow = s_cb + (size_t)y * s_rw;
        float *zrow = s_zb + (size_t)y * s_rw;
        for (int x = xa; x <= xb; x++) {
            if (iw <= zrow[x]) continue;
            float X = (float)(X0 + dX * x), Y = (float)(Y0 + dY * x), ld;
            int la = 0;
            unsigned m = area_mask(X, Y, 0, lim, &ld, &la) & s_plateMask;
            if (!m) continue;
            zrow[x] = iw;
            crow[x] = plate_color(m, X, Y, ld, la, kmpx, foot);
        }
    }
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
    /* земли государств: линии ~1.7 пикселя; на плите — масштаб по глубине с поправкой на наклон */
    if (s_shDirty && s_nareas) build_shadow();
    s_slabOn = s_nareas > 0 && s_agW > 0;
    if (s_slabOn && s_chDirty) build_chunk_modes();
    if (!s_chMode) s_slabOn = 0;
    s_slabMode = 0;
    s_lineHalf = 0.85f * ss;
    {
        double sp = -v->fwd[2];
        s_kmScale = (float)(1.0 / (s_f * sqrt(sp > 0.2 ? sp : 0.2)));
    }
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
        s_slabMode = s_slabOn ? s_chMode[c] : 0;
        s_slabTop = s_slabOn ? s_chTop[c] : 0;
        s_slabPlain = s_slabMode == 1 && !s_areas[s_slabTop].stripes ? s_areas[s_slabTop].color & 0xFFFFFFu : 0;
        s_slabF = s_slabPlain ? frac256(s_areas[s_slabTop].slab) : 0;
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

    /* земли государств — плоскость под плитой: стенки по краю, затем сама плоскость (z-буфер решает, что видно) */
    if (s_slabOn) {
        areas_walls(v);
        areas_plates(v);
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
