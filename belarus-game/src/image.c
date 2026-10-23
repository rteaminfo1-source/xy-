/* image.c — декодер QOI, масштабирование и наложение премультиплицированных картинок.
 * Только стандартная библиотека C (плюс kernel32 для ресурсов под Windows). */
#include "image.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define QOI_MAX_SIDE   32768u
#define QOI_MAX_PIXELS (64u * 1024u * 1024u)    /* 64 Мпикс = 256 МБ: больше любого ассета */

/* round(x / 255) для 0 <= x <= 255*255 (точное, проверено перебором в tests/image_test.c) */
static unsigned div255(unsigned x) { x += 128; return (x + (x >> 8)) >> 8; }

static unsigned premul(unsigned r, unsigned g, unsigned b, unsigned a)
{
    if (a == 255) return 0xFF000000u | r << 16 | g << 8 | b;
    if (a == 0) return 0;
    return a << 24 | div255(r * a) << 16 | div255(g * a) << 8 | div255(b * a);
}

int image_alloc(Image *img, int w, int h)
{
    if (!img) return 0;
    img->w = img->h = 0;
    img->px = NULL;
    if (w <= 0 || h <= 0 || (unsigned long long)w * (unsigned long long)h > QOI_MAX_PIXELS) return 0;
    img->px = (unsigned *)calloc((size_t)w * (size_t)h, sizeof(unsigned));
    if (!img->px) return 0;
    img->w = w;
    img->h = h;
    return 1;
}

void image_free(Image *img)
{
    if (!img) return;
    free(img->px);
    img->px = NULL;
    img->w = img->h = 0;
}

/* ------------------------------------------------------------ QOI ---- */

int qoi_decode(const unsigned char *d, int size, Image *out)
{
    static const unsigned char tail[8] = { 0, 0, 0, 0, 0, 0, 0, 1 };
    unsigned char idx[64][4], r = 0, g = 0, b = 0, a = 255;
    unsigned w, h, cur;
    size_t n, i = 0, pos = 14, len;
    int run = 0;

    if (!out) return 0;
    out->w = out->h = 0;
    out->px = NULL;
    if (!d || size < 14 + 8 || memcmp(d, "qoif", 4) != 0) return 0;
    len = (size_t)size;
    w = (unsigned)d[4] << 24 | (unsigned)d[5] << 16 | (unsigned)d[6] << 8 | d[7];
    h = (unsigned)d[8] << 24 | (unsigned)d[9] << 16 | (unsigned)d[10] << 8 | d[11];
    if (w == 0 || h == 0 || w > QOI_MAX_SIDE || h > QOI_MAX_SIDE) return 0;
    if ((d[12] != 3 && d[12] != 4) || d[13] > 1) return 0;
    n = (size_t)w * h;
    /* каждый байт потока даёт не больше 62 пикселей: иначе файл заведомо обрезан */
    if (n > QOI_MAX_PIXELS || n > (len - 14 - 8) * 62) return 0;
    if (!image_alloc(out, (int)w, (int)h)) return 0;

    memset(idx, 0, sizeof idx);
    cur = premul(r, g, b, a);
    while (i < n) {
        int op;
        if (pos >= len) goto fail;
        op = d[pos++];
        if (op == 0xFE) {                                    /* QOI_OP_RGB */
            if (len - pos < 3) goto fail;
            r = d[pos]; g = d[pos + 1]; b = d[pos + 2];
            pos += 3;
        } else if (op == 0xFF) {                             /* QOI_OP_RGBA */
            if (len - pos < 4) goto fail;
            r = d[pos]; g = d[pos + 1]; b = d[pos + 2]; a = d[pos + 3];
            pos += 4;
        } else switch (op >> 6) {
            case 0:                                          /* QOI_OP_INDEX */
                r = idx[op][0]; g = idx[op][1]; b = idx[op][2]; a = idx[op][3];
                break;
            case 1:                                          /* QOI_OP_DIFF */
                r = (unsigned char)(r + ((op >> 4) & 3) - 2);
                g = (unsigned char)(g + ((op >> 2) & 3) - 2);
                b = (unsigned char)(b + (op & 3) - 2);
                break;
            case 2: {                                        /* QOI_OP_LUMA */
                int b2, vg = (op & 0x3F) - 32;
                if (pos >= len) goto fail;
                b2 = d[pos++];
                r = (unsigned char)(r + vg - 8 + ((b2 >> 4) & 15));
                g = (unsigned char)(g + vg);
                b = (unsigned char)(b + vg - 8 + (b2 & 15));
                break;
            }
            default:                                         /* QOI_OP_RUN */
                run = op & 0x3F;
                break;
        }
        {
            unsigned char *e = idx[(r * 3 + g * 5 + b * 7 + a * 11) & 63];
            e[0] = r; e[1] = g; e[2] = b; e[3] = a;
        }
        cur = premul(r, g, b, a);
        out->px[i++] = cur;
        for (; run > 0 && i < n; run--) out->px[i++] = cur;
        run = 0;
    }
    if (len - pos < 8 || memcmp(d + pos, tail, 8) != 0) goto fail;   /* обязательный конец потока */
    return 1;
fail:
    image_free(out);
    return 0;
}

#ifdef _WIN32
int image_load_resource(int id, Image *out)
{
    HRSRC rs;
    HGLOBAL hg;
    const void *p;
    DWORD sz;
    if (!out) return 0;
    out->w = out->h = 0;
    out->px = NULL;
    rs = FindResourceW(NULL, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));   /* 10 = RT_RCDATA */
    if (!rs) return 0;
    sz = SizeofResource(NULL, rs);
    hg = LoadResource(NULL, rs);
    p = hg ? LockResource(hg) : NULL;
    if (!p || sz == 0 || sz > 0x7FFFFFFF) return 0;
    return qoi_decode((const unsigned char *)p, (int)sz, out);
}
#else
#include "plat_host.h"
/* без Windows ресурсы лежат файлами рядом с приложением (assets.rc — номер -> файл) */
int image_load_resource(int id, Image *out)
{
    void *p;
    int sz, ok;
    if (!out) return 0;
    out->w = out->h = 0;
    out->px = NULL;
    if (!plat_resource(id, &p, &sz)) return 0;
    ok = qoi_decode((const unsigned char *)p, sz, out);
    free(p);
    return ok;
}
#endif

/* ---------------------------------------------------- масштабирование ---- */

/* Веса одной оси в 1/65536, сумма для каждого выходного пикселя ровно 65536.
 * first[i], cnt[i] — первый исходный индекс и число отсчётов, веса в wt[off[i]...]. */
typedef struct { int *first, *cnt, *off; unsigned *wt; } Axis;

static void axis_free(Axis *ax)
{
    free(ax->first); free(ax->cnt); free(ax->off); free(ax->wt);
    memset(ax, 0, sizeof *ax);
}

static int axis_make(Axis *ax, int sn, int dn)
{
    int i, k = 0, cap = sn + 2 * dn + 2;
    ax->first = (int *)malloc(sizeof(int) * (size_t)dn);
    ax->cnt = (int *)malloc(sizeof(int) * (size_t)dn);
    ax->off = (int *)malloc(sizeof(int) * (size_t)dn);
    ax->wt = (unsigned *)malloc(sizeof(unsigned) * (size_t)cap);
    if (!ax->first || !ax->cnt || !ax->off || !ax->wt) { axis_free(ax); return 0; }
    for (i = 0; i < dn; i++) {
        ax->off[i] = k;
        if (dn <= sn) {
            /* площадь: выходной i покрывает [i*sn, (i+1)*sn) в единицах 1/dn исходного */
            long long lo = (long long)i * sn, hi = lo + sn;
            int j, j0 = (int)(lo / dn), j1 = (int)((hi - 1) / dn), big = k;
            unsigned sum = 0;
            for (j = j0; j <= j1 && k < cap; j++) {
                long long a0 = (long long)j * dn, a1 = a0 + dn;
                long long ov = (hi < a1 ? hi : a1) - (lo > a0 ? lo : a0);
                unsigned wv = (unsigned)((ov * 65536) / sn);
                ax->wt[k] = wv;
                sum += wv;
                if (wv > ax->wt[big]) big = k;
                k++;
            }
            ax->wt[big] += 65536u - sum;
            ax->first[i] = j0;
            ax->cnt[i] = k - ax->off[i];
        } else {
            /* билинейно: центр выходного пикселя -> ((2i+1)*sn - dn) / (2*dn) */
            long long num = (long long)(2 * i + 1) * sn - dn, den = 2LL * dn;
            int j0 = 0;
            unsigned f = 0;
            if (num > 0) {
                j0 = (int)(num / den);
                f = (unsigned)(((num % den) * 65536 + den / 2) / den);
                if (f >= 65536) { j0++; f = 0; }
            }
            if (j0 >= sn - 1) { j0 = sn - 1; f = 0; }
            ax->first[i] = j0;
            ax->wt[k++] = 65536u - f;
            if (f) ax->wt[k++] = f;
            ax->cnt[i] = f ? 2 : 1;
        }
    }
    return 1;
}

void image_scale(const Image *src, int sx, int sy, int sw, int sh, Image *dst, int dw, int dh)
{
    Axis ax = { 0 }, ay = { 0 };
    unsigned *acc = NULL;
    int x, y, t;
    if (!dst) return;
    dst->w = dst->h = 0;
    dst->px = NULL;
    if (!src || !src->px || src->w <= 0 || src->h <= 0 || dw <= 0 || dh <= 0) return;
    if (sx < 0) { sw += sx; sx = 0; }
    if (sy < 0) { sh += sy; sy = 0; }
    if (sw > src->w - sx) sw = src->w - sx;
    if (sh > src->h - sy) sh = src->h - sy;
    if (sw <= 0 || sh <= 0) return;
    if (!axis_make(&ax, sw, dw) || !axis_make(&ay, sh, dh)) goto done;
    acc = (unsigned *)malloc(sizeof(unsigned) * 4 * (size_t)sw);
    if (!acc || !image_alloc(dst, dw, dh)) goto done;
    for (y = 0; y < dh; y++) {
        /* по вертикали: строка-аккумулятор (каждый канал <= 255*65536) */
        memset(acc, 0, sizeof(unsigned) * 4 * (size_t)sw);
        for (t = 0; t < ay.cnt[y]; t++) {
            unsigned wv = ay.wt[ay.off[y] + t], *q = acc;
            const unsigned *row = src->px + (size_t)(sy + ay.first[y] + t) * src->w + sx;
            for (x = 0; x < sw; x++, q += 4) {
                unsigned p = row[x];
                if (!p) continue;
                q[0] += wv * (p >> 24);
                q[1] += wv * (p >> 16 & 255);
                q[2] += wv * (p >> 8 & 255);
                q[3] += wv * (p & 255);
            }
        }
        /* по горизонтали */
        for (x = 0; x < dw; x++) {
            unsigned long long s0 = 0, s1 = 0, s2 = 0, s3 = 0;
            const unsigned *q = acc + 4 * (size_t)ax.first[x];
            for (t = 0; t < ax.cnt[x]; t++, q += 4) {
                unsigned long long wv = ax.wt[ax.off[x] + t];
                s0 += wv * q[0]; s1 += wv * q[1]; s2 += wv * q[2]; s3 += wv * q[3];
            }
            dst->px[(size_t)y * dw + x] =
                (unsigned)((s0 + 0x80000000ull) >> 32) << 24 | (unsigned)((s1 + 0x80000000ull) >> 32) << 16 |
                (unsigned)((s2 + 0x80000000ull) >> 32) << 8 | (unsigned)((s3 + 0x80000000ull) >> 32);
        }
    }
done:
    free(acc);
    axis_free(&ax);
    axis_free(&ay);
}

/* ------------------------------------------------------- наложение ---- */

void image_blit_part(unsigned *dst, int dstW, int dstH, int dx, int dy, const Image *img,
                     int sx, int sy, int sw, int sh, int alpha)
{
    int x, y;
    if (!dst || !img || !img->px || dstW <= 0 || dstH <= 0 || alpha <= 0) return;
    if (alpha > 255) alpha = 255;
    /* отсечение по картинке */
    if (sx < 0) { sw += sx; dx -= sx; sx = 0; }
    if (sy < 0) { sh += sy; dy -= sy; sy = 0; }
    if (sw > img->w - sx) sw = img->w - sx;
    if (sh > img->h - sy) sh = img->h - sy;
    /* отсечение по приёмнику */
    if (dx < 0) { sw += dx; sx -= dx; dx = 0; }
    if (dy < 0) { sh += dy; sy -= dy; dy = 0; }
    if (sw > dstW - dx) sw = dstW - dx;
    if (sh > dstH - dy) sh = dstH - dy;
    if (sw <= 0 || sh <= 0) return;
    for (y = 0; y < sh; y++) {
        const unsigned *s = img->px + (size_t)(sy + y) * img->w + sx;
        unsigned *o = dst + (size_t)(dy + y) * dstW + dx;
        for (x = 0; x < sw; x++) {
            unsigned p = s[x], a, inv, q, r, g, b;
            if (alpha < 255 && p)
                p = div255((p >> 24) * alpha) << 24 | div255((p >> 16 & 255) * alpha) << 16 |
                    div255((p >> 8 & 255) * alpha) << 8 | div255((p & 255) * alpha);
            a = p >> 24;
            if (a == 255) { o[x] = p & 0xFFFFFF; continue; }
            if (!p) continue;
            inv = 255 - a;
            q = o[x];
            r = (p >> 16 & 255) + div255((q >> 16 & 255) * inv);
            g = (p >> 8 & 255) + div255((q >> 8 & 255) * inv);
            b = (p & 255) + div255((q & 255) * inv);
            o[x] = (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
        }
    }
}

void image_blit(unsigned *dst, int dstW, int dstH, int dx, int dy, const Image *img)
{
    if (img) image_blit_part(dst, dstW, dstH, dx, dy, img, 0, 0, img->w, img->h, 255);
}

void image_fill_under(Image *img, unsigned rgb)
{
    size_t i, n;
    unsigned cr = rgb >> 16 & 255, cg = rgb >> 8 & 255, cb = rgb & 255;
    if (!img || !img->px || img->w <= 0 || img->h <= 0) return;
    n = (size_t)img->w * img->h;
    for (i = 0; i < n; i++) {
        unsigned p = img->px[i], a = p >> 24, inv, r, g, b;
        if (a == 255) continue;
        inv = 255 - a;
        r = (p >> 16 & 255) + div255(cr * inv);
        g = (p >> 8 & 255) + div255(cg * inv);
        b = (p & 255) + div255(cb * inv);
        img->px[i] = 0xFF000000u | (r > 255 ? 255 : r) << 16 | (g > 255 ? 255 : g) << 8 | (b > 255 ? 255 : b);
    }
}
