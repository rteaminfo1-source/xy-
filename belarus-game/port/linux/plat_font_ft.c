/* plat_font.h через FreeType — для проверки сборки без Windows на Linux (port/linux/run.sh).
 * Шрифты Windows заменяются похожими: Segoe UI -> Inter, книжные -> DejaVu Serif,
 * Bahnschrift -> DejaVu Sans Condensed, Consolas -> DejaVu Sans Mono. */
#include "plat_font.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { int ok, adv, left, top, w, h; unsigned char *bmp; } Glyph;

struct PlatFont {
    FT_Face face;
    int asc, desc, avg;
    Glyph cache[0x500];          /* латиница и кириллица; прочие символы — без кэша */
};

static FT_Library s_lib;

static const char *pick(const wchar_t *f, int weight)
{
    static const char *D = "/usr/share/fonts/truetype/dejavu/", *I = "/usr/share/fonts/opentype/inter/";
    static char path[256];
    int bold = weight >= 600;
    if (wcsstr(f, L"Consolas") || wcsstr(f, L"Lucida Console") || wcsstr(f, L"Courier"))
        snprintf(path, sizeof path, "%sDejaVuSansMono%s.ttf", D, bold ? "-Bold" : "");
    else if (wcsstr(f, L"Bahnschrift") || wcsstr(f, L"Narrow") || wcsstr(f, L"Impact"))
        snprintf(path, sizeof path, "%sDejaVuSansCondensed%s.ttf", D, bold || wcsstr(f, L"SemiBold") ? "-Bold" : "");
    else if (wcsstr(f, L"Palatino") || wcsstr(f, L"Antiqua") || wcsstr(f, L"Georgia") || wcsstr(f, L"Constantia") ||
             wcsstr(f, L"Cambria") || wcsstr(f, L"Times"))
        snprintf(path, sizeof path, "%sDejaVuSerif%s.ttf", D, bold ? "-Bold" : "");
    else
        snprintf(path, sizeof path, "%sInter-%s.otf", I, weight >= 700 ? "Bold" : weight >= 600 ? "SemiBold" : weight >= 500 ? "Medium" : "Regular");
    return path;
}

PlatFont *pf_open(const wchar_t *face, int px, int weight, int italic)
{
    (void)italic;
    if (!s_lib && FT_Init_FreeType(&s_lib)) return NULL;
    PlatFont *f = (PlatFont *)calloc(1, sizeof *f);
    if (!f) return NULL;
    if (FT_New_Face(s_lib, pick(face, weight), 0, &f->face)) { free(f); return NULL; }
    FT_Set_Pixel_Sizes(f->face, 0, (FT_UInt)px);
    f->asc = (int)ceil(f->face->size->metrics.ascender / 64.0);
    f->desc = (int)ceil(-f->face->size->metrics.descender / 64.0);
    f->avg = FT_Load_Char(f->face, 'x', FT_LOAD_DEFAULT) ? px / 2 : (int)(f->face->glyph->advance.x >> 6);
    return f;
}

void pf_close(PlatFont *f)
{
    if (!f) return;
    for (int i = 0; i < 0x500; i++) free(f->cache[i].bmp);
    FT_Done_Face(f->face);
    free(f);
}

void pf_metrics(PlatFont *f, int *a, int *d, int *avg) { *a = f->asc; *d = f->desc; *avg = f->avg; }

static const Glyph *glyph(PlatFont *f, wchar_t ch, Glyph *tmp)
{
    unsigned c = (unsigned)ch;
    Glyph *g = c < 0x500 ? &f->cache[c] : tmp;
    if (g->ok) return g;
    memset(g, 0, sizeof *g);
    g->ok = 1;
    if (FT_Load_Char(f->face, c, FT_LOAD_RENDER | FT_LOAD_TARGET_LIGHT)) return g;
    FT_GlyphSlot s = f->face->glyph;
    g->adv = (int)((s->advance.x + 32) >> 6);
    g->left = s->bitmap_left; g->top = s->bitmap_top;
    g->w = (int)s->bitmap.width; g->h = (int)s->bitmap.rows;
    if (g->w > 0 && g->h > 0) {
        g->bmp = (unsigned char *)malloc((size_t)g->w * (size_t)g->h);
        if (g->bmp)
            for (int y = 0; y < g->h; y++) memcpy(g->bmp + (size_t)y * (size_t)g->w, s->bitmap.buffer + (size_t)y * (size_t)s->bitmap.pitch, (size_t)g->w);
    }
    return g;
}

int pf_width(PlatFont *f, const wchar_t *s, int n, int extra)
{
    int w = 0;
    for (int i = 0; i < n; i++) {
        Glyph tmp = { 0 };
        w += glyph(f, s[i], &tmp)->adv + extra;
        free(tmp.bmp);
    }
    return w;
}

unsigned char *pf_render(PlatFont *f, const wchar_t *s, int n, int extra, int *w, int *h, int *ox, int *oy)
{
    int pad = 3, W = pf_width(f, s, n, extra) + 2 * pad, H = f->asc + f->desc + 2;
    if (W <= 2 * pad || H <= 0) return NULL;
    unsigned char *m = (unsigned char *)calloc((size_t)W * (size_t)H, 1);
    if (!m) return NULL;
    int pen = pad;
    for (int i = 0; i < n; i++) {
        Glyph tmp = { 0 };
        const Glyph *g = glyph(f, s[i], &tmp);
        for (int y = 0; y < g->h; y++) {
            int py = f->asc - g->top + y;
            if (py < 0 || py >= H) continue;
            for (int x = 0; x < g->w; x++) {
                int px = pen + g->left + x;
                if (px < 0 || px >= W) continue;
                unsigned char a = g->bmp[(size_t)y * (size_t)g->w + (size_t)x], *d = &m[(size_t)py * (size_t)W + (size_t)px];
                if (a > *d) *d = a;
            }
        }
        pen += g->adv + extra;
        free(tmp.bmp);
    }
    *w = W; *h = H; *ox = -pad; *oy = 0;
    return m;
}
