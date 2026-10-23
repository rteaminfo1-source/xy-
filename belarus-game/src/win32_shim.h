/* win32_shim.h — подмножество Win32 API и GDI, которым пользуется интерфейс игры, для сборки без Windows
 * (iPhone, а для проверки — Linux). Реализация — plat_gdi.c: свой буфер пикселей 0x00RRGGBB, отсечение,
 * многоугольники и линии, текст — через plat_font.h (CoreText на iPhone, FreeType на Linux). */
#ifndef BY_WIN32_SHIM_H
#define BY_WIN32_SHIM_H
#ifndef _WIN32

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

typedef int BOOL;
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef unsigned int UINT;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef DWORD COLORREF;
typedef wchar_t WCHAR;
typedef const wchar_t *LPCWSTR;
typedef wchar_t *LPWSTR, *PWSTR;
typedef void *HANDLE, *HWND, *HINSTANCE, *HMODULE, *HGLOBAL, *HRSRC, *HCURSOR, *HICON;
typedef struct ShimObj *HGDIOBJ, *HFONT, *HPEN, *HBRUSH, *HRGN, *HBITMAP;
typedef struct ShimDC *HDC;

typedef struct { LONG x, y; } POINT;
typedef struct { LONG left, top, right, bottom; } RECT;
typedef struct { LONG cx, cy; } SIZE;

#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define LF_FACESIZE 32
#define WINAPI
#define CALLBACK

#define RGB(r, g, b) ((COLORREF)(((BYTE)(r)) | ((WORD)((BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(c) ((BYTE)((c) & 0xFF))
#define GetGValue(c) ((BYTE)(((c) >> 8) & 0xFF))
#define GetBValue(c) ((BYTE)(((c) >> 16) & 0xFF))
#define CLR_INVALID 0xFFFFFFFFu
#define LOWORD(l) ((WORD)(((uintptr_t)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((uintptr_t)(l)) >> 16) & 0xffff))

/* шрифты */
#define FW_NORMAL 400
#define FW_MEDIUM 500
#define FW_SEMIBOLD 600
#define FW_BOLD 700
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS 0
#define CLIP_DEFAULT_PRECIS 0
#define CLEARTYPE_QUALITY 5
#define ANTIALIASED_QUALITY 4
#define DEFAULT_PITCH 0
#define FF_SWISS 0x20
typedef struct { LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth, tmMaxCharWidth; } TEXTMETRICW;
HFONT CreateFontW(int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strike, DWORD charset,
                  DWORD outPrec, DWORD clipPrec, DWORD quality, DWORD pitch, LPCWSTR face);
BOOL GetTextExtentPoint32W(HDC dc, LPCWSTR s, int n, SIZE *sz);
BOOL GetTextMetricsW(HDC dc, TEXTMETRICW *tm);
BOOL TextOutW(HDC dc, int x, int y, LPCWSTR s, int n);
COLORREF SetTextColor(HDC dc, COLORREF c);
int SetTextCharacterExtra(HDC dc, int extra);
int SetBkMode(HDC dc, int mode);
#define TRANSPARENT 1
#define DT_TOP 0
#define DT_CENTER 1
#define DT_SINGLELINE 0x20
#define DT_WORDBREAK 0x10
#define DT_CALCRECT 0x400
#define DT_NOPREFIX 0x800
#define DT_END_ELLIPSIS 0x8000
int DrawTextW(HDC dc, LPCWSTR s, int n, RECT *r, UINT fmt);

/* перья, кисти, объекты */
#define PS_SOLID 0
#define NULL_BRUSH 5
#define NULL_PEN 8
#define DC_BRUSH 18
#define DC_PEN 19
#define HS_BDIAGONAL 3
#define HS_DIAGCROSS 5
HPEN CreatePen(int style, int width, COLORREF c);
HBRUSH CreateHatchBrush(int style, COLORREF c);
HGDIOBJ GetStockObject(int i);
HGDIOBJ SelectObject(HDC dc, HGDIOBJ o);
BOOL DeleteObject(HGDIOBJ o);
COLORREF SetDCBrushColor(HDC dc, COLORREF c);
COLORREF SetDCPenColor(HDC dc, COLORREF c);
int SaveDC(HDC dc);
BOOL RestoreDC(HDC dc, int saved);

/* рисование */
BOOL FillRect(HDC dc, const RECT *r, HBRUSH b);
BOOL FrameRect(HDC dc, const RECT *r, HBRUSH b);
BOOL MoveToEx(HDC dc, int x, int y, POINT *old);
BOOL LineTo(HDC dc, int x, int y);
BOOL Polygon(HDC dc, const POINT *p, int n);
BOOL Polyline(HDC dc, const POINT *p, int n);
BOOL Pie(HDC dc, int l, int t, int r, int b, int xs, int ys, int xe, int ye);
BOOL Ellipse(HDC dc, int l, int t, int r, int b);
#define AD_CLOCKWISE 2
int SetArcDirection(HDC dc, int dir);
BOOL GdiFlush(void);
BOOL BitBlt(HDC dst, int x, int y, int w, int h, HDC src, int sx, int sy, DWORD rop);
#define SRCCOPY 0x00CC0020

/* контексты в памяти и DIB (пиксель 0x00RRGGBB, строки сверху вниз при biHeight < 0) */
typedef struct { DWORD biSize; LONG biWidth, biHeight; WORD biPlanes, biBitCount; DWORD biCompression, biSizeImage;
                 LONG biXPelsPerMeter, biYPelsPerMeter; DWORD biClrUsed, biClrImportant; } BITMAPINFOHEADER;
typedef struct { BITMAPINFOHEADER bmiHeader; DWORD bmiColors[1]; } BITMAPINFO;
#define BI_RGB 0
#define DIB_RGB_COLORS 0
HDC CreateCompatibleDC(HDC dc);
BOOL DeleteDC(HDC dc);
HBITMAP CreateDIBSection(HDC dc, const BITMAPINFO *bi, UINT usage, void **bits, HANDLE sec, DWORD off);

/* отсечение */
#define WINDING 2
#define ALTERNATE 1
#define RGN_AND 1
#define RGN_OR 2
#define RGN_DIFF 4
#define RGN_COPY 5
HRGN CreateRectRgn(int l, int t, int r, int b);
HRGN CreatePolygonRgn(const POINT *p, int n, int mode);
int CombineRgn(HRGN dst, HRGN a, HRGN b, int mode);
int SelectClipRgn(HDC dc, HRGN r);
int ExtSelectClipRgn(HDC dc, HRGN r, int mode);

/* прямоугольники */
BOOL IntersectRect(RECT *dst, const RECT *a, const RECT *b);
BOOL PtInRect(const RECT *r, POINT p);

/* время, окно, таймеры — их обслуживает хозяин (plat_host.h) */
DWORD GetTickCount(void);
BOOL InvalidateRect(HWND h, const RECT *r, BOOL erase);
uintptr_t SetTimer(HWND h, uintptr_t id, UINT ms, void *fn);
BOOL KillTimer(HWND h, uintptr_t id);
short GetKeyState(int vk);
extern POINT g_plat_cursor;          /* последнее касание (ui_ev_move) — вместо курсора мыши */
BOOL GetCursorPos(POINT *p);
BOOL ScreenToClient(HWND h, POINT *p);
#define IsIconic(h) 0
DWORD CharUpperBuffW(LPWSTR s, DWORD n);

/* клавиши */
#define VK_BACK 0x08
#define VK_TAB 0x09
#define VK_RETURN 0x0D
#define VK_SHIFT 0x10
#define VK_CONTROL 0x11
#define VK_ESCAPE 0x1B
#define VK_SPACE 0x20
#define VK_PRIOR 0x21
#define VK_NEXT 0x22
#define VK_END 0x23
#define VK_HOME 0x24
#define VK_LEFT 0x25
#define VK_UP 0x26
#define VK_RIGHT 0x27
#define VK_DOWN 0x28
#define VK_DELETE 0x2E
#define VK_NUMPAD0 0x60
#define VK_ADD 0x6B
#define VK_SUBTRACT 0x6D
#define VK_F1 0x70
#define VK_F2 0x71
#define VK_F3 0x72
#define VK_F9 0x78
#define VK_F10 0x79
#define VK_F11 0x7A
#define VK_F12 0x7B
#define VK_OEM_PLUS 0xBB
#define VK_OEM_MINUS 0xBD

/* файлы и строки */
#include <stdio.h>
FILE *_wfopen(const wchar_t *path, const wchar_t *mode);
int _wtoi(const wchar_t *s);

#endif /* !_WIN32 */
#endif
