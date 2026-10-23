/* plat_font.h через CoreText — шрифты на iPhone. Шрифты Windows заменяются похожими встроенными:
 * Segoe UI -> системный (San Francisco) нужной насыщенности, книжные -> Palatino и Georgia,
 * Bahnschrift и узкие -> Helvetica Neue Condensed, Consolas -> Menlo. Кириллица есть во всех. */
#import <UIKit/UIKit.h>
#import <CoreText/CoreText.h>
#include "plat_font.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct PlatFont {
    CTFontRef font;
    int asc, desc, avg;
};

static CTFontRef make_font(const wchar_t *f, int px, int weight, int italic)
{
    int bold = weight >= 600;
    CGFloat size = px;
    NSString *name = nil;
    if (wcsstr(f, L"Consolas") || wcsstr(f, L"Lucida Console") || wcsstr(f, L"Courier")) name = bold ? @"Menlo-Bold" : @"Menlo-Regular";
    else if (wcsstr(f, L"Bahnschrift") || wcsstr(f, L"Narrow") || wcsstr(f, L"Impact")) name = @"HelveticaNeue-CondensedBold";
    else if (wcsstr(f, L"Palatino") || wcsstr(f, L"Antiqua")) name = bold ? @"Palatino-Bold" : @"Palatino-Roman";
    else if (wcsstr(f, L"Georgia") || wcsstr(f, L"Constantia") || wcsstr(f, L"Cambria") || wcsstr(f, L"Times")) name = bold ? @"Georgia-Bold" : @"Georgia";
    if (name) {
        CTFontRef ct = CTFontCreateWithName((__bridge CFStringRef)name, size, NULL);
        if (ct) return ct;
    }
    UIFontWeight w = weight >= 700 ? UIFontWeightBold : weight >= 600 ? UIFontWeightSemibold : weight >= 500 ? UIFontWeightMedium : UIFontWeightRegular;
    UIFont *ui = [UIFont systemFontOfSize:size weight:w];
    if (italic) {
        UIFontDescriptor *d = [ui.fontDescriptor fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitItalic];
        if (d) ui = [UIFont fontWithDescriptor:d size:size];
    }
    return (CTFontRef)CFBridgingRetain(ui);    /* UIFont и CTFontRef — один и тот же объект */
}

PlatFont *pf_open(const wchar_t *face, int px, int weight, int italic)
{
    PlatFont *f = (PlatFont *)calloc(1, sizeof *f);
    if (!f) return NULL;
    f->font = make_font(face ? face : L"Segoe UI", px, weight, italic);
    if (!f->font) { free(f); return NULL; }
    f->asc = (int)ceil(CTFontGetAscent(f->font));
    f->desc = (int)ceil(CTFontGetDescent(f->font));
    UniChar x = 'x';
    CGGlyph g;
    CGSize adv = { px * 0.5, 0 };
    if (CTFontGetGlyphsForCharacters(f->font, &x, &g, 1)) CTFontGetAdvancesForGlyphs(f->font, kCTFontOrientationHorizontal, &g, &adv, 1);
    f->avg = (int)lround(adv.width);
    return f;
}

void pf_close(PlatFont *f)
{
    if (!f) return;
    if (f->font) CFRelease(f->font);
    free(f);
}

void pf_metrics(PlatFont *f, int *a, int *d, int *avg) { *a = f->asc; *d = f->desc; *avg = f->avg; }

/* строка wchar_t (UTF-32) -> CTLine с нашим шрифтом и разрядкой */
static CTLineRef make_line(PlatFont *f, const wchar_t *s, int n, int extra)
{
    UniChar stackb[256], *u = n * 2 <= 256 ? stackb : (UniChar *)malloc(sizeof(UniChar) * (size_t)n * 2);
    if (!u) return NULL;
    CFIndex k = 0;
    for (int i = 0; i < n; i++) {
        unsigned c = (unsigned)s[i];
        if (c >= 0x10000 && c <= 0x10FFFF) { c -= 0x10000; u[k++] = (UniChar)(0xD800 + (c >> 10)); u[k++] = (UniChar)(0xDC00 + (c & 0x3FF)); }
        else u[k++] = (UniChar)c;
    }
    CFStringRef str = CFStringCreateWithCharacters(NULL, u, k);
    if (u != stackb) free(u);
    if (!str) return NULL;
    CGFloat kern = extra;
    CFNumberRef kn = CFNumberCreate(NULL, kCFNumberCGFloatType, &kern);
    const void *keys[] = { kCTFontAttributeName, kCTKernAttributeName, kCTForegroundColorFromContextAttributeName };
    const void *vals[] = { f->font, kn, kCFBooleanTrue };
    CFDictionaryRef attr = CFDictionaryCreate(NULL, keys, vals, 3, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, str, attr);
    CTLineRef line = as ? CTLineCreateWithAttributedString(as) : NULL;
    if (as) CFRelease(as);
    CFRelease(attr);
    CFRelease(kn);
    CFRelease(str);
    return line;
}

int pf_width(PlatFont *f, const wchar_t *s, int n, int extra)
{
    if (n <= 0) return 0;
    CTLineRef line = make_line(f, s, n, extra);
    if (!line) return 0;
    double w = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
    CFRelease(line);
    return (int)ceil(w);
}

unsigned char *pf_render(PlatFont *f, const wchar_t *s, int n, int extra, int *w, int *h, int *ox, int *oy)
{
    if (n <= 0) return NULL;
    CTLineRef line = make_line(f, s, n, extra);
    if (!line) return NULL;
    int pad = 3, W = (int)ceil(CTLineGetTypographicBounds(line, NULL, NULL, NULL)) + 2 * pad, H = f->asc + f->desc + 2;
    unsigned char *m = (unsigned char *)calloc((size_t)W * (size_t)H, 1);
    if (!m) { CFRelease(line); return NULL; }
    CGContextRef ctx = CGBitmapContextCreate(m, (size_t)W, (size_t)H, 8, (size_t)W, NULL, (CGBitmapInfo)kCGImageAlphaOnly);
    if (!ctx) { free(m); CFRelease(line); return NULL; }
    CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
    CGContextSetShouldAntialias(ctx, true);
    CGContextSetTextMatrix(ctx, CGAffineTransformIdentity);
    CGContextSetTextPosition(ctx, pad, H - f->asc);     /* строка 0 памяти — верх маски, базовая линия на asc ниже */
    CTLineDraw(line, ctx);
    CGContextRelease(ctx);
    CFRelease(line);
    *w = W; *h = H; *ox = -pad; *oy = 0;
    return m;
}
