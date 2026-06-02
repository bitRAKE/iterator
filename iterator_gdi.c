// iterator_gdi.c
//
// Native Win32/GDI companion for iterator.html.
//
// MSVC:
//   cl /O2 /W4 /D_CRT_SECURE_NO_WARNINGS iterator_gdi.c user32.lib gdi32.lib /link /SUBSYSTEM:WINDOWS
//
// MinGW:
//   gcc -O2 -std=c11 iterator_gdi.c -lgdi32 -luser32 -lm -mwindows -o iterator_gdi.exe
//
// The program opens one popup+resize-border window per graph. Right-click any
// window for graph options. Left-click/drag applies that window's current
// navigation mode and shared parameter changes redraw all graph windows.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "iterator_mru.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#if defined(_MSC_VER)
#define finite_double(v) _finite(v)
#else
#define finite_double(v) isfinite(v)
#endif

#ifndef NAN
#define NAN (0.0 / 0.0)
#endif

#define APP_CLASS_NAME "IteratorGdiGraphWindow"
#define HEADER_H 56
#define FOOTER_H 24
#define PAD 12
#define MIN_PLOT_W 32
#define MIN_PLOT_H 32
#define MAX_FIND_PERIOD 12
#define MAX_CYCLES 160
#define APP_PI 3.141592653589793238462643383279502884
#define MANDEL_X0 (-2.35)
#define MANDEL_X1 (0.85)
#define MANDEL_Y0 (-1.35)
#define MANDEL_Y1 (1.35)

#define COL_BG        RGB(0x06,0x08,0x0b)
#define COL_PANEL     RGB(0x0b,0x0f,0x14)
#define COL_PANEL2    RGB(0x0e,0x14,0x1b)
#define COL_PLOT      RGB(0x08,0x0b,0x0f)
#define COL_GRID      RGB(0x12,0x1a,0x22)
#define COL_LINE      RGB(0x1e,0x2b,0x38)
#define COL_AXIS      RGB(0x33,0x48,0x5a)
#define COL_INK       RGB(0xc7,0xd6,0xe0)
#define COL_INK_DIM   RGB(0x6f,0x85,0x97)
#define COL_INK_FAINT RGB(0x41,0x52,0x5f)
#define COL_PHOS      RGB(0x5e,0xf2,0xd0)
#define COL_PHOS_DIM  RGB(0x2f,0x7d,0x6e)
#define COL_AMBER     RGB(0xff,0xb4,0x54)
#define COL_MAGENTA   RGB(0xff,0x5d,0x8f)
#define COL_VIOLET    RGB(0x9d,0x7b,0xff)
#define COL_GREEN     RGB(0x8c,0xe5,0x63)
#define COL_ORANGE    RGB(0xff,0x8a,0x3d)
#define COL_PINK      RGB(0xff,0x79,0xc6)
#define COL_BLUE      RGB(0x5f,0xb0,0xff)
#define COL_CHAOS     RGB(0x54,0x62,0x7a)

typedef enum FamilyKind {
    FAM_LOGISTIC = 0,
    FAM_TENT = 1,
    FAM_SINE = 2
} FamilyKind;

typedef enum GraphKind {
    GRAPH_BIF = 0,
    GRAPH_COB = 1,
    GRAPH_MAN = 2,
    GRAPH_LAB = 3,
    GRAPH_COUNT = 4
} GraphKind;

typedef enum DragMode {
    DRAG_PARAM = 0,
    DRAG_ZOOM = 1,
    DRAG_SEED = 2
} DragMode;

typedef struct Dib {
    HDC dc;
    HBITMAP bmp;
    HGDIOBJ old_bmp;
    uint32_t *bits;
    int w;
    int h;
} Dib;

typedef struct CycleInfo {
    int period;
    double orbit[MAX_FIND_PERIOD];
    double multiplier;
} CycleInfo;

typedef struct AppState {
    FamilyKind fam;
    double param;
    double view_min;
    double view_max;
    double bif_x_min;
    double bif_x_max;
    double x0;
    int show_transient;
    int lyap_overlay;
    int period_colors;
    int find_period;
    int cycles_valid;
    int cycle_count;
    int stable_cycle_count;
    CycleInfo cycles[MAX_CYCLES];
    int feig_valid;
    char feig_lines[8][96];
    int feig_count;
} AppState;

typedef struct GraphWindow {
    HWND hwnd;
    GraphKind kind;
    DragMode drag_mode;
    int dragging;
    int hover_valid;
    int tracking_mouse;
    POINT drag_start;
    POINT drag_now;
    POINT hover;
    RECT last_rect;
    int has_last_rect;
    Dib cache;
    uint64_t cache_rev;
} GraphWindow;

enum {
    IDM_FAMILY_LOGISTIC = 1001,
    IDM_FAMILY_TENT,
    IDM_FAMILY_SINE,
    IDM_SHOW_ALL,
    IDM_TILE_WINDOWS,
    IDM_RESET_ALL,
    IDM_CLOSE_THIS,
    IDM_EXIT_APP,
    IDM_PARAM_STEP_LEFT,
    IDM_PARAM_STEP_RIGHT,
    IDM_PARAM_STEP_FINE_LEFT,
    IDM_PARAM_STEP_FINE_RIGHT,
    IDM_PARAM_STEP_COARSE_LEFT,
    IDM_PARAM_STEP_COARSE_RIGHT,
    IDM_FOCUS_NEXT,
    IDM_FOCUS_PREV,

    IDM_BIF_MODE_PARAM = 2001,
    IDM_BIF_MODE_ZOOM,
    IDM_BIF_LYAP,
    IDM_BIF_PERIOD_COLORS,
    IDM_BIF_RESET_VIEW,

    IDM_COB_MODE_SEED = 3001,
    IDM_COB_RESEED,
    IDM_COB_TRANSIENT,

    IDM_MAN_MODE_PARAM = 4001,
    IDM_MAN_REBUILD,

    IDM_LAB_MODE_PARAM = 5001,
    IDM_LAB_PERIOD_DEC,
    IDM_LAB_PERIOD_INC,
    IDM_LAB_FIND,
    IDM_LAB_FEIG
};

#define IDM_BIF_RECENT_BASE 6000
#define IDM_LAB_RECENT_PERIOD_BASE 6100
#define IDM_LAB_STABLE_CYCLE_BASE 6200
#define IDM_LAB_EXPORT_R8 6308
#define IDM_LAB_EXPORT_R16 6316
#define IDM_LAB_EXPORT_R32 6332
#define IDM_LAB_EXPORT_R52 6352

static HINSTANCE g_inst;
static AppState g_state;
static GraphWindow g_windows[GRAPH_COUNT];
static IteratorMruSessionRecord g_start_session;
static int g_have_start_session;
static HFONT g_font_ui;
static HFONT g_font_small;
static HFONT g_font_mono;
static HFONT g_font_mono_small;
static HFONT g_font_title;
static HACCEL g_accel;
static uint64_t g_bif_rev = 1;
static uint64_t g_man_rev = 1;
static int g_live_windows = 0;

static double clampd(double v, double lo, double hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int clampi(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static COLORREF mix_color(COLORREF a, COLORREF b, double t)
{
    int ar = GetRValue(a), ag = GetGValue(a), ab = GetBValue(a);
    int br = GetRValue(b), bg = GetGValue(b), bb = GetBValue(b);
    int r = (int)(ar + (br - ar) * t + 0.5);
    int g = (int)(ag + (bg - ag) * t + 0.5);
    int bl = (int)(ab + (bb - ab) * t + 0.5);
    return RGB(clampi(r, 0, 255), clampi(g, 0, 255), clampi(bl, 0, 255));
}

static COLORREF scale_color(COLORREF c, double s)
{
    return RGB(clampi((int)(GetRValue(c) * s + 0.5), 0, 255),
               clampi((int)(GetGValue(c) * s + 0.5), 0, 255),
               clampi((int)(GetBValue(c) * s + 0.5), 0, 255));
}

static COLORREF period_color(int p)
{
    switch (p) {
    case 1: return COL_PHOS;
    case 2: return COL_AMBER;
    case 3: return COL_MAGENTA;
    case 4: return COL_VIOLET;
    case 5: return COL_GREEN;
    case 6: return COL_ORANGE;
    case 7: return COL_PINK;
    case 8: return COL_BLUE;
    default:
        return p > 8 ? RGB(0xb5,0x8c,0xff) : COL_CHAOS;
    }
}

static COLORREF bif_period_color(int p)
{
    return p > 0 ? period_color(p) : RGB(0, 0, 0);
}

static const char *family_name(FamilyKind fam)
{
    switch (fam) {
    case FAM_TENT: return "tent";
    case FAM_SINE: return "sine";
    default: return "logistic";
    }
}

static const char *param_name(FamilyKind fam)
{
    switch (fam) {
    case FAM_TENT: return "mu";
    case FAM_SINE: return "a";
    default: return "r";
    }
}

static double family_full_min(FamilyKind fam)
{
    (void)fam;
    return 0.0;
}

static double family_full_max(FamilyKind fam)
{
    switch (fam) {
    case FAM_TENT: return 2.0;
    case FAM_SINE: return 1.0;
    default: return 4.0;
    }
}

static double family_def_min(FamilyKind fam)
{
    switch (fam) {
    case FAM_TENT: return 1.0;
    case FAM_SINE: return 0.65;
    default: return 2.4;
    }
}

static double family_def_max(FamilyKind fam)
{
    switch (fam) {
    case FAM_TENT: return 2.0;
    case FAM_SINE: return 1.0;
    default: return 4.0;
    }
}

static double default_param(FamilyKind fam)
{
    switch (fam) {
    case FAM_TENT: return 1.6;
    case FAM_SINE: return 0.90;
    default: return 3.5;
    }
}

static double map_f(FamilyKind fam, double x, double p)
{
    if (fam == FAM_TENT)
        return p * (x <= 0.5 ? x : 1.0 - x);
    if (fam == FAM_SINE)
        return p * sin(APP_PI * x);
    return p * x * (1.0 - x);
}

static double map_df(FamilyKind fam, double x, double p)
{
    if (fam == FAM_TENT)
        return x < 0.5 ? p : -p;
    if (fam == FAM_SINE)
        return p * APP_PI * cos(APP_PI * x);
    return p * (1.0 - 2.0 * x);
}

static double logistic_r_to_c(double r)
{
    return r * 0.5 - r * r * 0.25;
}

static int detect_period(FamilyKind fam, double p, double x0, int max_p, int transient, double tol)
{
    double x = x0;
    int i;
    for (i = 0; i < transient; ++i) {
        x = map_f(fam, x, p);
        if (!finite_double(x)) return 0;
    }
    {
        double anchor = x;
        double y = x;
        int k;
        for (k = 1; k <= max_p; ++k) {
            y = map_f(fam, y, p);
            if (!finite_double(y)) return 0;
            if (fabs(y - anchor) < tol)
                return k;
        }
    }
    return 0;
}

static double lyapunov_value(FamilyKind fam, double p, double x0, int n, int transient)
{
    double x = x0;
    double s = 0.0;
    int c = 0;
    int i;
    for (i = 0; i < transient; ++i) {
        x = map_f(fam, x, p);
        if (!finite_double(x)) return 0.0;
    }
    for (i = 0; i < n; ++i) {
        double d = fabs(map_df(fam, x, p));
        if (d > 1e-12) {
            s += log(d);
            ++c;
        }
        x = map_f(fam, x, p);
        if (!finite_double(x)) return 0.0;
    }
    return c ? s / (double)c : -DBL_MAX;
}

static int attractor_orbit(FamilyKind fam, double p, double x0, int period, double *out, int cap)
{
    double x = x0;
    int i;
    int n = period < cap ? period : cap;
    for (i = 0; i < 3000; ++i)
        x = map_f(fam, x, p);
    for (i = 0; i < n; ++i) {
        out[i] = x;
        x = map_f(fam, x, p);
    }
    return n;
}

static int complex_cycle(double cre, double cim, int max_p)
{
    double zr = 0.0, zi = 0.0;
    double ar, ai;
    int i, k;
    for (i = 0; i < 600; ++i) {
        double nr = zr * zr - zi * zi + cre;
        double ni = 2.0 * zr * zi + cim;
        zr = nr;
        zi = ni;
        if (zr * zr + zi * zi > 1e6)
            return -1;
    }
    ar = zr;
    ai = zi;
    for (k = 1; k <= max_p; ++k) {
        double nr = zr * zr - zi * zi + cre;
        double ni = 2.0 * zr * zi + cim;
        zr = nr;
        zi = ni;
        if ((zr - ar) * (zr - ar) + (zi - ai) * (zi - ai) < 1e-12)
            return k;
    }
    return 0;
}

static int has_smooth_critical(FamilyKind fam)
{
    return fam == FAM_LOGISTIC || fam == FAM_SINE;
}

static double critical_point(FamilyKind fam)
{
    (void)fam;
    return 0.5;
}

static double superstable_g(FamilyKind fam, double param, int period)
{
    double crit = critical_point(fam);
    double x = crit;
    int i;
    for (i = 0; i < period; ++i)
        x = map_f(fam, x, param);
    return x - crit;
}

static double superstable_period(FamilyKind fam, int period, double lo, double hi)
{
    double fa;
    double a = lo, b = hi;
    int found = 0;
    int i;
    const int n = 12000;

    fa = superstable_g(fam, lo, period);
    for (i = 1; i <= n; ++i) {
        double x = lo + (hi - lo) * (double)i / (double)n;
        double fx = superstable_g(fam, x, period);
        if (!finite_double(fa) || !finite_double(fx)) {
            fa = fx;
            continue;
        }
        if (fabs(fa) < 1e-13)
            return lo + (hi - lo) * (double)(i - 1) / (double)n;
        if (fa * fx < 0.0) {
            a = lo + (hi - lo) * (double)(i - 1) / (double)n;
            b = x;
            found = 1;
            break;
        }
        fa = fx;
    }
    if (!found)
        return NAN;
    for (i = 0; i < 90; ++i) {
        double m = 0.5 * (a + b);
        double ga = superstable_g(fam, a, period);
        double gm = superstable_g(fam, m, period);
        if (ga * gm <= 0.0)
            b = m;
        else
            a = m;
    }
    return 0.5 * (a + b);
}

static int dib_resize(Dib *d, int w, int h)
{
    BITMAPINFO bi;
    HDC screen;

    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (d->bmp && d->w == w && d->h == h)
        return 1;

    if (d->dc) {
        if (d->old_bmp)
            SelectObject(d->dc, d->old_bmp);
        DeleteDC(d->dc);
        d->dc = NULL;
    }
    if (d->bmp) {
        DeleteObject(d->bmp);
        d->bmp = NULL;
    }
    d->bits = NULL;
    d->w = w;
    d->h = h;

    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    screen = GetDC(NULL);
    d->bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, (void **)&d->bits, NULL, 0);
    ReleaseDC(NULL, screen);
    if (!d->bmp || !d->bits)
        return 0;
    d->dc = CreateCompatibleDC(NULL);
    if (!d->dc)
        return 0;
    d->old_bmp = SelectObject(d->dc, d->bmp);
    return 1;
}

static void dib_destroy(Dib *d)
{
    if (d->dc) {
        if (d->old_bmp)
            SelectObject(d->dc, d->old_bmp);
        DeleteDC(d->dc);
    }
    if (d->bmp)
        DeleteObject(d->bmp);
    ZeroMemory(d, sizeof(*d));
}

static void dib_fill(Dib *d, COLORREF c)
{
    uint32_t v = (uint32_t)c;
    int count = d->w * d->h;
    int i;
    for (i = 0; i < count; ++i)
        d->bits[i] = v;
}

static void dib_set_pixel(Dib *d, int x, int y, COLORREF c)
{
    if ((unsigned)x >= (unsigned)d->w || (unsigned)y >= (unsigned)d->h)
        return;
    d->bits[y * d->w + x] = (uint32_t)c;
}

static void dib_add_pixel(Dib *d, int x, int y, int ar, int ag, int ab)
{
    uint32_t *p;
    int r, g, b;
    if ((unsigned)x >= (unsigned)d->w || (unsigned)y >= (unsigned)d->h)
        return;
    p = &d->bits[y * d->w + x];
    r = (int)(*p & 0xff);
    g = (int)((*p >> 8) & 0xff);
    b = (int)((*p >> 16) & 0xff);
    r = clampi(r + ar, 0, 255);
    g = clampi(g + ag, 0, 255);
    b = clampi(b + ab, 0, 255);
    *p = (uint32_t)RGB(r, g, b);
}

static HFONT make_font(const char *face, int px, int weight)
{
    return CreateFontA(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void init_fonts(void)
{
    g_font_ui = make_font("Segoe UI", 14, FW_NORMAL);
    g_font_small = make_font("Segoe UI", 12, FW_NORMAL);
    g_font_mono = make_font("Consolas", 14, FW_NORMAL);
    g_font_mono_small = make_font("Consolas", 11, FW_NORMAL);
    g_font_title = make_font("Consolas", 22, FW_NORMAL);
}

static void destroy_fonts(void)
{
    if (g_font_ui) DeleteObject(g_font_ui);
    if (g_font_small) DeleteObject(g_font_small);
    if (g_font_mono) DeleteObject(g_font_mono);
    if (g_font_mono_small) DeleteObject(g_font_mono_small);
    if (g_font_title) DeleteObject(g_font_title);
}

static void fill_rect(HDC dc, const RECT *r, COLORREF c)
{
    HBRUSH b = CreateSolidBrush(c);
    FillRect(dc, r, b);
    DeleteObject(b);
}

static void draw_line(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int width, int style)
{
    HPEN pen = CreatePen(style, width, c);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, x2, y2);
    SelectObject(dc, old);
    DeleteObject(pen);
}

static void draw_text_at(HDC dc, HFONT font, COLORREF c, int x, int y, const char *s)
{
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    TextOutA(dc, x, y, s, (int)strlen(s));
    SelectObject(dc, old);
}

static void draw_textf_at(HDC dc, HFONT font, COLORREF c, int x, int y, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    draw_text_at(dc, font, c, x, y, buf);
}

static void draw_text_rect(HDC dc, HFONT font, COLORREF c, RECT r, const char *s, UINT flags)
{
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, c);
    DrawTextA(dc, s, -1, &r, flags);
    SelectObject(dc, old);
}

static void draw_crosshair(HDC dc, RECT plot, int x, int y, COLORREF c)
{
    COLORREF line = mix_color(COL_PLOT, c, 0.68);
    int left = plot.left;
    int right = plot.right - 1;
    int top = plot.top;
    int bottom = plot.bottom - 1;
    if (right < left)
        right = left;
    if (bottom < top)
        bottom = top;
    x = clampi(x, left, right);
    y = clampi(y, top, bottom);
    draw_line(dc, x, plot.top, x, plot.bottom, line, 1, PS_DOT);
    draw_line(dc, plot.left, y, plot.right, y, line, 1, PS_DOT);
    draw_line(dc, x - 5, y, x + 5, y, c, 1, PS_SOLID);
    draw_line(dc, x, y - 5, x, y + 5, c, 1, PS_SOLID);
}

static void draw_hover_box(HDC dc, RECT bounds, POINT anchor, const char *line1, const char *line2)
{
    const char *lines[2];
    int count = 0;
    int i;
    int pad = 6;
    int gap = 12;
    int max_w = 0;
    int content_w;
    int avail_w;
    int line_h = 14;
    RECT box;
    HGDIOBJ old;
    TEXTMETRICA tm;
    HBRUSH border;

    if (line1 && line1[0])
        lines[count++] = line1;
    if (line2 && line2[0])
        lines[count++] = line2;
    if (count == 0)
        return;

    old = SelectObject(dc, g_font_mono_small);
    if (GetTextMetricsA(dc, &tm))
        line_h = tm.tmHeight + 2;
    for (i = 0; i < count; ++i) {
        SIZE s;
        if (GetTextExtentPoint32A(dc, lines[i], (int)strlen(lines[i]), &s) && s.cx > max_w)
            max_w = s.cx;
    }
    avail_w = bounds.right - bounds.left - 4;
    if (avail_w < 42)
        avail_w = 42;
    content_w = max_w;
    if (content_w > avail_w - pad * 2)
        content_w = avail_w - pad * 2;

    box.left = anchor.x + gap;
    box.top = anchor.y + gap;
    box.right = box.left + content_w + pad * 2;
    box.bottom = box.top + line_h * count + pad * 2;
    if (box.right > bounds.right - 2) {
        box.right = anchor.x - gap;
        box.left = box.right - content_w - pad * 2;
    }
    if (box.bottom > bounds.bottom - 2) {
        box.bottom = anchor.y - gap;
        box.top = box.bottom - line_h * count - pad * 2;
    }
    if (box.left < bounds.left + 2) {
        box.left = bounds.left + 2;
        box.right = box.left + content_w + pad * 2;
    }
    if (box.top < bounds.top + 2) {
        box.top = bounds.top + 2;
        box.bottom = box.top + line_h * count + pad * 2;
    }

    fill_rect(dc, &box, COL_PANEL2);
    border = CreateSolidBrush(COL_PHOS_DIM);
    FrameRect(dc, &box, border);
    DeleteObject(border);

    SetBkMode(dc, TRANSPARENT);
    for (i = 0; i < count; ++i) {
        RECT text_r;
        SetTextColor(dc, i == 0 ? COL_INK : COL_INK_DIM);
        text_r.left = box.left + pad;
        text_r.top = box.top + pad + line_h * i;
        text_r.right = box.right - pad;
        text_r.bottom = text_r.top + line_h;
        DrawTextA(dc, lines[i], -1, &text_r, DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    SelectObject(dc, old);
}

static const char *graph_title(GraphKind kind)
{
    switch (kind) {
    case GRAPH_BIF: return "01  BIFURCATION";
    case GRAPH_COB: return "02  COBWEB";
    case GRAPH_MAN: return "03  QUADRATIC BRIDGE";
    case GRAPH_LAB: return "04  CYCLE LAB";
    default: return "GRAPH";
    }
}

static const char *graph_equation(GraphKind kind)
{
    if (kind == GRAPH_BIF) {
        if (g_state.fam == FAM_TENT)
            return "x[n+1] = mu * min(x[n], 1-x[n])";
        if (g_state.fam == FAM_SINE)
            return "x[n+1] = a * sin(pi * x[n])";
        return "x[n+1] = r * x[n] * (1-x[n])";
    }
    if (kind == GRAPH_COB) return "orbit of x0";
    if (kind == GRAPH_MAN) return "z[n+1] = z[n]^2 + c";
    return "fixed points of f^p(x) = x";
}

static const char *drag_mode_name(const GraphWindow *gw)
{
    if (gw->drag_mode == DRAG_ZOOM) return "drag: box zoom";
    if (gw->drag_mode == DRAG_SEED) return "drag: set x0";
    return "drag: set parameter";
}

static RECT plot_rect_for(GraphKind kind, int w, int h)
{
    RECT r;
    r.left = PAD;
    r.top = HEADER_H + 8;
    r.right = w - PAD;
    r.bottom = h - FOOTER_H - 8;
    if (kind == GRAPH_COB) {
        r.left = 42;
        r.right = w - 18;
        r.bottom = h - 38;
    } else if (kind == GRAPH_LAB) {
        r.left = PAD;
        r.top = HEADER_H + 8;
        r.right = w - PAD;
        r.bottom = h - PAD;
    }
    if (r.right - r.left < MIN_PLOT_W)
        r.right = r.left + MIN_PLOT_W;
    if (r.bottom - r.top < MIN_PLOT_H)
        r.bottom = r.top + MIN_PLOT_H;
    return r;
}

static int point_in_rect(RECT r, POINT p)
{
    return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
}

static void clear_analysis_cache(void);
static void invalidate_all(void);

static double double_step_around(double center)
{
    double up = nextafter(center, DBL_MAX);
    double down = nextafter(center, -DBL_MAX);
    double a = fabs(up - center);
    double b = fabs(center - down);
    double step = a > b ? a : b;
    if (!finite_double(step) || step <= 0.0)
        step = DBL_MIN;
    return step;
}

static void ensure_representable_span(double *lo, double *hi, double hard_lo, double hard_hi)
{
    double a = *lo;
    double b = *hi;
    if (a > b) {
        double t = a;
        a = b;
        b = t;
    }
    a = clampd(a, hard_lo, hard_hi);
    b = clampd(b, hard_lo, hard_hi);
    {
        double c = a + (b - a) * 0.5;
        double min_span = double_step_around(c);
        if (b > a && (b - a) >= min_span) {
            *lo = a;
            *hi = b;
            return;
        }
        c = clampd(c, hard_lo, hard_hi);
        double lower = nextafter(c, -DBL_MAX);
        double upper = nextafter(c, DBL_MAX);
        if (lower < hard_lo)
            lower = c;
        if (upper > hard_hi)
            upper = c;
        if (lower < c && upper > c) {
            *lo = lower;
            *hi = upper;
        } else if (upper > c) {
            *lo = c;
            *hi = upper;
        } else if (lower < c) {
            *lo = lower;
            *hi = c;
        } else {
            *lo = hard_lo;
            *hi = hard_hi;
        }
    }
}

static double bif_param_from_client_x(RECT plot, int px)
{
    int denom = plot.right - plot.left - 1;
    double t;
    if (denom < 1)
        denom = 1;
    px = clampi(px, plot.left, plot.right);
    t = (double)(px - plot.left) / (double)denom;
    t = clampd(t, 0.0, 1.0);
    return g_state.view_min + (g_state.view_max - g_state.view_min) * t;
}

static double bif_value_from_client_y(RECT plot, int py)
{
    int denom = plot.bottom - plot.top - 1;
    double t;
    if (denom < 1)
        denom = 1;
    py = clampi(py, plot.top, plot.bottom);
    t = (double)(py - plot.top) / (double)denom;
    t = clampd(t, 0.0, 1.0);
    return g_state.bif_x_max - (g_state.bif_x_max - g_state.bif_x_min) * t;
}

static int bif_client_x_from_param(RECT plot, double p)
{
    double span = g_state.view_max - g_state.view_min;
    int denom = plot.right - plot.left - 1;
    double t;
    if (denom < 1)
        denom = 1;
    if (span <= 0.0)
        return plot.left;
    t = (p - g_state.view_min) / span;
    t = clampd(t, 0.0, 1.0);
    return plot.left + (int)(t * (double)denom + 0.5);
}

static double cob_value_from_client_x(RECT plot, int px)
{
    int denom = plot.right - plot.left;
    double t;
    if (denom < 1)
        denom = 1;
    px = clampi(px, plot.left, plot.right);
    t = (double)(px - plot.left) / (double)denom;
    return clampd(t, 0.0, 1.0);
}

static int cob_client_y_from_value(RECT plot, double v)
{
    int span = plot.bottom - plot.top;
    if (span < 1)
        span = 1;
    v = clampd(v, 0.0, 1.0);
    return plot.bottom - (int)(v * (double)span + 0.5);
}

static double mandel_real_from_client_x(RECT plot, int px)
{
    int denom = plot.right - plot.left - 1;
    double t;
    if (denom < 1)
        denom = 1;
    px = clampi(px, plot.left, plot.right - 1);
    t = (double)(px - plot.left) / (double)denom;
    t = clampd(t, 0.0, 1.0);
    return MANDEL_X0 + (MANDEL_X1 - MANDEL_X0) * t;
}

static double mandel_imag_from_client_y(RECT plot, int py)
{
    int denom = plot.bottom - plot.top - 1;
    double t;
    if (denom < 1)
        denom = 1;
    py = clampi(py, plot.top, plot.bottom - 1);
    t = (double)(py - plot.top) / (double)denom;
    t = clampd(t, 0.0, 1.0);
    return MANDEL_Y0 + (MANDEL_Y1 - MANDEL_Y0) * t;
}

static double logistic_r_from_real_c(double c, int *ok)
{
    if (c < -2.0 || c > 0.25) {
        if (ok)
            *ok = 0;
        return 0.0;
    }
    if (ok)
        *ok = 1;
    return 1.0 + sqrt(clampd(1.0 - 4.0 * c, 0.0, 9.0));
}

static RECT bif_aspect_zoom_rect(RECT plot, POINT start, POINT now)
{
    RECT z;
    double aspect = (double)(plot.right - plot.left) / (double)(plot.bottom - plot.top);
    double dx, dy, sx, sy, rw, rh, max_w, max_h;

    start.x = clampi(start.x, plot.left, plot.right);
    start.y = clampi(start.y, plot.top, plot.bottom);
    now.x = clampi(now.x, plot.left, plot.right);
    now.y = clampi(now.y, plot.top, plot.bottom);

    dx = (double)(now.x - start.x);
    dy = (double)(now.y - start.y);
    sx = dx < 0.0 ? -1.0 : 1.0;
    sy = dy < 0.0 ? -1.0 : 1.0;
    rw = fabs(dx);
    rh = fabs(dy);
    if (rw < 1.0)
        rw = 1.0;
    if (rh < 1.0)
        rh = 1.0;

    if (rw / rh > aspect)
        rw = rh * aspect;
    else
        rh = rw / aspect;

    max_w = sx > 0.0 ? (double)(plot.right - start.x) : (double)(start.x - plot.left);
    max_h = sy > 0.0 ? (double)(plot.bottom - start.y) : (double)(start.y - plot.top);
    if (rw > max_w) {
        rw = max_w;
        rh = rw / aspect;
    }
    if (rh > max_h) {
        rh = max_h;
        rw = rh * aspect;
    }

    z.left = start.x;
    z.right = start.x + (int)(sx * rw + (sx > 0.0 ? 0.5 : -0.5));
    z.top = start.y;
    z.bottom = start.y + (int)(sy * rh + (sy > 0.0 ? 0.5 : -0.5));
    if (z.left > z.right) {
        int t = z.left;
        z.left = z.right;
        z.right = t;
    }
    if (z.top > z.bottom) {
        int t = z.top;
        z.top = z.bottom;
        z.bottom = t;
    }
    z.left = clampi(z.left, plot.left, plot.right);
    z.right = clampi(z.right, plot.left, plot.right);
    z.top = clampi(z.top, plot.top, plot.bottom);
    z.bottom = clampi(z.bottom, plot.top, plot.bottom);
    return z;
}

static void reset_state(FamilyKind fam)
{
    ZeroMemory(&g_state, sizeof(g_state));
    g_state.fam = fam;
    g_state.param = default_param(fam);
    g_state.view_min = family_def_min(fam);
    g_state.view_max = family_def_max(fam);
    g_state.bif_x_min = 0.0;
    g_state.bif_x_max = 1.0;
    g_state.x0 = 0.4;
    g_state.show_transient = 1;
    g_state.find_period = 3;
    g_state.cycles_valid = 0;
    g_state.feig_valid = 0;
    ++g_bif_rev;
}

static int valid_family_int(int fam)
{
    return fam == FAM_LOGISTIC || fam == FAM_TENT || fam == FAM_SINE;
}

static void capture_window_rect(GraphWindow *gw)
{
    if (gw && gw->hwnd && GetWindowRect(gw->hwnd, &gw->last_rect))
        gw->has_last_rect = 1;
}

static void save_current_session(void)
{
    IteratorMruSessionRecord r;
    int i;
    ZeroMemory(&r, sizeof(r));
    r.family = g_state.fam;
    r.param = g_state.param;
    r.view_min = g_state.view_min;
    r.view_max = g_state.view_max;
    r.bif_x_min = g_state.bif_x_min;
    r.bif_x_max = g_state.bif_x_max;
    r.x0 = g_state.x0;
    r.show_transient = g_state.show_transient;
    r.lyap_overlay = g_state.lyap_overlay;
    r.period_colors = g_state.period_colors;
    r.find_period = g_state.find_period;
    for (i = 0; i < GRAPH_COUNT && i < ITERATOR_MRU_GRAPH_COUNT; ++i) {
        capture_window_rect(&g_windows[i]);
        r.windows[i].visible = g_windows[i].hwnd != NULL;
        if (g_windows[i].has_last_rect) {
            r.windows[i].left = g_windows[i].last_rect.left;
            r.windows[i].top = g_windows[i].last_rect.top;
            r.windows[i].right = g_windows[i].last_rect.right;
            r.windows[i].bottom = g_windows[i].last_rect.bottom;
        }
    }
    IteratorMruSaveSession(&r);
}

static void apply_session_record(const IteratorMruSessionRecord *r)
{
    int i;
    if (!r || !valid_family_int(r->family))
        return;
    reset_state((FamilyKind)r->family);
    g_state.param = clampd(r->param, family_full_min(g_state.fam), family_full_max(g_state.fam));
    g_state.view_min = r->view_min;
    g_state.view_max = r->view_max;
    g_state.bif_x_min = r->bif_x_min;
    g_state.bif_x_max = r->bif_x_max;
    ensure_representable_span(&g_state.view_min, &g_state.view_max,
                              family_full_min(g_state.fam),
                              family_full_max(g_state.fam));
    ensure_representable_span(&g_state.bif_x_min, &g_state.bif_x_max, 0.0, 1.0);
    g_state.x0 = clampd(r->x0, 0.0, 1.0);
    g_state.show_transient = r->show_transient ? 1 : 0;
    g_state.lyap_overlay = r->lyap_overlay ? 1 : 0;
    g_state.period_colors = r->period_colors ? 1 : 0;
    g_state.find_period = clampi(r->find_period, 1, MAX_FIND_PERIOD);
    for (i = 0; i < GRAPH_COUNT && i < ITERATOR_MRU_GRAPH_COUNT; ++i) {
        RECT wr;
        wr.left = r->windows[i].left;
        wr.top = r->windows[i].top;
        wr.right = r->windows[i].right;
        wr.bottom = r->windows[i].bottom;
        if (wr.right > wr.left + 160 && wr.bottom > wr.top + 120) {
            g_windows[i].last_rect = wr;
            g_windows[i].has_last_rect = 1;
        }
    }
    clear_analysis_cache();
    ++g_bif_rev;
}

static void add_current_bif_view_mru(void)
{
    IteratorMruBifViewRecord r;
    ZeroMemory(&r, sizeof(r));
    r.family = g_state.fam;
    r.param_min = g_state.view_min;
    r.param_max = g_state.view_max;
    r.x_min = g_state.bif_x_min;
    r.x_max = g_state.bif_x_max;
    r.marker_param = g_state.param;
    IteratorMruAddBifView(&r);
}

static void add_current_cycle_period_mru(void)
{
    IteratorMruCyclePeriodRecord r;
    ZeroMemory(&r, sizeof(r));
    r.family = g_state.fam;
    r.period = g_state.find_period;
    IteratorMruAddCyclePeriod(&r);
}

static void add_stable_cycle_mru(int period, double param, double multiplier, const double *orbit)
{
    IteratorMruStableCycleRecord r;
    int i;
    if (!orbit || period < 1 || period > ITERATOR_MRU_CYCLE_POINT_MAX || fabs(multiplier) >= 1.0)
        return;
    ZeroMemory(&r, sizeof(r));
    r.family = g_state.fam;
    r.period = period;
    r.point_count = period;
    r.param = param;
    r.multiplier = multiplier;
    for (i = 0; i < period; ++i)
        r.orbit[i] = orbit[i];
    IteratorMruAddStableCycle(&r);
}

static void apply_bif_view_record(const IteratorMruBifViewRecord *r)
{
    double p_min;
    double p_max;
    double x_min;
    double x_max;
    if (!r || !valid_family_int(r->family))
        return;
    reset_state((FamilyKind)r->family);
    p_min = r->param_min;
    p_max = r->param_max;
    x_min = r->x_min;
    x_max = r->x_max;
    ensure_representable_span(&p_min, &p_max,
                              family_full_min(g_state.fam),
                              family_full_max(g_state.fam));
    ensure_representable_span(&x_min, &x_max, 0.0, 1.0);
    g_state.view_min = p_min;
    g_state.view_max = p_max;
    g_state.bif_x_min = x_min;
    g_state.bif_x_max = x_max;
    g_state.param = clampd(r->marker_param, p_min, p_max);
    clear_analysis_cache();
    ++g_bif_rev;
    invalidate_all();
    save_current_session();
}

static void apply_cycle_period_record(const IteratorMruCyclePeriodRecord *r)
{
    if (!r || !valid_family_int(r->family))
        return;
    if (g_state.fam != (FamilyKind)r->family)
        reset_state((FamilyKind)r->family);
    g_state.find_period = clampi(r->period, 1, MAX_FIND_PERIOD);
    clear_analysis_cache();
    add_current_cycle_period_mru();
    invalidate_all();
    save_current_session();
}

static void apply_stable_cycle_record(const IteratorMruStableCycleRecord *r)
{
    if (!r || !valid_family_int(r->family))
        return;
    if (g_state.fam != (FamilyKind)r->family)
        reset_state((FamilyKind)r->family);
    g_state.param = clampd(r->param, family_full_min(g_state.fam), family_full_max(g_state.fam));
    g_state.find_period = clampi(r->period, 1, MAX_FIND_PERIOD);
    clear_analysis_cache();
    add_current_cycle_period_mru();
    invalidate_all();
    save_current_session();
}

typedef struct FixedRExportItem {
    uint64_t raw_r;
    IteratorMruStableCycleRecord cycle;
} FixedRExportItem;

static uint64_t fixed_mask_for_bits(unsigned bits)
{
    return bits >= 64 ? UINT64_MAX : ((UINT64_C(1) << bits) - 1);
}

static long double fixed_scale_for_bits(unsigned bits)
{
    return bits >= 64 ? 18446744073709551616.0L : (long double)(UINT64_C(1) << bits);
}

static uint64_t logistic_r_to_fixed(double r, unsigned bits)
{
    uint64_t mask = fixed_mask_for_bits(bits);
    long double scaled;
    if (r <= 0.0)
        return 0;
    if (r >= 4.0)
        return mask;
    scaled = ((long double)r / 4.0L) * fixed_scale_for_bits(bits);
    if (scaled >= (long double)mask)
        return mask;
    if (scaled <= 0.0L)
        return 0;
    return (uint64_t)scaled;
}

static int cmp_fixed_r_export_item(const void *a, const void *b)
{
    const FixedRExportItem *x = (const FixedRExportItem *)a;
    const FixedRExportItem *y = (const FixedRExportItem *)b;
    if (x->raw_r < y->raw_r) return -1;
    if (x->raw_r > y->raw_r) return 1;
    if (x->cycle.period < y->cycle.period) return -1;
    if (x->cycle.period > y->cycle.period) return 1;
    if (fabs(x->cycle.multiplier) < fabs(y->cycle.multiplier)) return -1;
    if (fabs(x->cycle.multiplier) > fabs(y->cycle.multiplier)) return 1;
    return 0;
}

static const char *fixed_r_directive(unsigned bits)
{
    if (bits <= 8) return "db";
    if (bits <= 16) return "dw";
    if (bits <= 32) return "dd";
    return "dq";
}

static void app_relative_path(char *dst, size_t dst_size, const char *subdir, const char *file)
{
    char exe[MAX_PATH];
    char *slash;
    exe[0] = 0;
    GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));
    slash = strrchr(exe, '\\');
    if (slash)
        *slash = 0;
    else
        lstrcpynA(exe, ".", (int)sizeof(exe));
    if (subdir && subdir[0]) {
        char dir[MAX_PATH];
        snprintf(dir, sizeof(dir), "%s\\%s", exe, subdir);
        CreateDirectoryA(dir, NULL);
        snprintf(dst, dst_size, "%s\\%s", dir, file);
    } else {
        snprintf(dst, dst_size, "%s\\%s", exe, file);
    }
}

static void export_stable_logistic_r_bits(unsigned bits)
{
    IteratorMruStableCycleRecord recs[ITERATOR_MRU_STABLE_CYCLE_MAX];
    FixedRExportItem items[ITERATOR_MRU_STABLE_CYCLE_MAX];
    int count = 0;
    int item_count = 0;
    int unique_count = 0;
    int i;
    char csv_path[MAX_PATH];
    char inc_path[MAX_PATH];
    char msg[512];
    FILE *csv;
    FILE *inc;
    int hex_width;
    const char *directive;

    if (bits != 8 && bits != 16 && bits != 32 && bits != 52) {
        MessageBoxA(NULL, "Unsupported R bit depth.", "Iterator export", MB_OK | MB_ICONERROR);
        return;
    }

    IteratorMruLoadStableCycles(recs, (int)ARRAYSIZE(recs), &count);
    for (i = 0; i < count && item_count < (int)ARRAYSIZE(items); ++i) {
        if (recs[i].family == FAM_LOGISTIC && fabs(recs[i].multiplier) < 1.0) {
            items[item_count].raw_r = logistic_r_to_fixed(recs[i].param, bits);
            items[item_count].cycle = recs[i];
            ++item_count;
        }
    }
    if (item_count == 0) {
        MessageBoxA(NULL,
                    "No stable logistic cycles are preserved yet. Run Cycle Lab scans in logistic regions first.",
                    "Iterator export", MB_OK | MB_ICONINFORMATION);
        return;
    }

    qsort(items, (size_t)item_count, sizeof(items[0]), cmp_fixed_r_export_item);

    snprintf(msg, sizeof(msg), "stable_logistic_R_q0_%u.csv", bits);
    app_relative_path(csv_path, sizeof(csv_path), "exports", msg);
    snprintf(msg, sizeof(msg), "stable_logistic_R_q0_%u.inc", bits);
    app_relative_path(inc_path, sizeof(inc_path), "exports", msg);

    csv = fopen(csv_path, "wb");
    inc = fopen(inc_path, "wb");
    if (!csv || !inc) {
        if (csv) fclose(csv);
        if (inc) fclose(inc);
        MessageBoxA(NULL, "Could not create export files.", "Iterator export", MB_OK | MB_ICONERROR);
        return;
    }

    hex_width = (int)((bits + 3) / 4);
    directive = fixed_r_directive(bits);

    fprintf(csv, "bits,R_dec,R_hex,r,period,multiplier,orbit\n");
    fprintf(inc, "; iterator_gdi stable logistic R export\n");
    fprintf(inc, "; R = floor((r / 4) * 2^%u), saturated to the Q0.%u mask\n", bits, bits);
    fprintf(inc, "; Source: app-local state\\stable_cycles.mru\n\n");
    fprintf(inc, "stable_logistic_R_q0_%u:\n", bits);

    for (i = 0; i < item_count; ++i) {
        int k;
        if (i > 0 && items[i].raw_r == items[i - 1].raw_r)
            continue;
        ++unique_count;

        fprintf(csv, "%u,%llu,0x%0*llX,%.17g,%d,%.17g,\"",
                bits,
                (unsigned long long)items[i].raw_r,
                hex_width,
                (unsigned long long)items[i].raw_r,
                items[i].cycle.param,
                items[i].cycle.period,
                items[i].cycle.multiplier);
        for (k = 0; k < items[i].cycle.point_count; ++k)
            fprintf(csv, "%s%.17g", k ? " " : "", items[i].cycle.orbit[k]);
        fprintf(csv, "\"\n");

        fprintf(inc, "  %s 0%0*llXh ; R=%llu r=%.17g period=%d |m|=%.6g\n",
                directive,
                hex_width,
                (unsigned long long)items[i].raw_r,
                (unsigned long long)items[i].raw_r,
                items[i].cycle.param,
                items[i].cycle.period,
                fabs(items[i].cycle.multiplier));
    }
    fprintf(inc, "stable_logistic_R_q0_%u_count = %d\n", bits, unique_count);

    fclose(csv);
    fclose(inc);

    snprintf(msg, sizeof(msg),
             "Exported %d unique Q0.%u logistic R values.\n\n%s\n%s",
             unique_count, bits, csv_path, inc_path);
    MessageBoxA(NULL, msg, "Iterator export", MB_OK | MB_ICONINFORMATION);
}

static void invalidate_all(void)
{
    int i;
    for (i = 0; i < GRAPH_COUNT; ++i) {
        if (g_windows[i].hwnd)
            InvalidateRect(g_windows[i].hwnd, NULL, FALSE);
    }
}

static void clear_analysis_cache(void)
{
    g_state.cycles_valid = 0;
    g_state.cycle_count = 0;
    g_state.stable_cycle_count = 0;
}

static void set_family(FamilyKind fam)
{
    if (g_state.fam == fam)
        return;
    reset_state(fam);
    invalidate_all();
}

static void set_param(double v)
{
    double lo = family_full_min(g_state.fam);
    double hi = family_full_max(g_state.fam);
    double nv = clampd(v, lo, hi);
    if (fabs(nv - g_state.param) > 1e-12) {
        g_state.param = nv;
        clear_analysis_cache();
        invalidate_all();
    }
}

static void step_param_by_view_fraction(double fraction)
{
    double span = g_state.view_max - g_state.view_min;
    double delta;
    double min_step;
    if (!finite_double(span) || span <= 0.0)
        span = family_full_max(g_state.fam) - family_full_min(g_state.fam);
    if (!finite_double(span) || span <= 0.0)
        return;
    delta = span * fraction;
    min_step = double_step_around(g_state.param);
    if (fabs(delta) < min_step)
        delta = fraction < 0.0 ? -min_step : min_step;
    set_param(g_state.param + delta);
}

static int current_period(void)
{
    return detect_period(g_state.fam, g_state.param, g_state.x0, 64, 3000, 1e-7);
}

static double current_lyapunov(void)
{
    return lyapunov_value(g_state.fam, g_state.param, g_state.x0, 2000, 800);
}

static void draw_header(HDC dc, const GraphWindow *gw, int w)
{
    RECT hdr = {0, 0, w, HEADER_H};
    RECT right1 = {180, 8, w - 12, 26};
    RECT right2 = {180, 30, w - 12, 48};
    char readout[256];
    char cbuf[40];
    int per = current_period();
    double lam = current_lyapunov();

    fill_rect(dc, &hdr, COL_PANEL2);
    draw_line(dc, 0, HEADER_H - 1, w, HEADER_H - 1, COL_LINE, 1, PS_SOLID);
    draw_text_at(dc, g_font_title, COL_PHOS, 12, 6, graph_title(gw->kind));
    draw_text_at(dc, g_font_mono_small, COL_INK_DIM, 14, 34, graph_equation(gw->kind));

    if (g_state.fam == FAM_LOGISTIC)
        snprintf(cbuf, sizeof(cbuf), "%.5f", logistic_r_to_c(g_state.param));
    else
        snprintf(cbuf, sizeof(cbuf), "-");

    snprintf(readout, sizeof(readout),
             "family %s   %s %.7f   c %s   attractor %s%d   lambda %.3f",
             family_name(g_state.fam), param_name(g_state.fam), g_state.param, cbuf,
             per ? "period " : "chaos", per ? per : 0, lam);
    if (!per) {
        snprintf(readout, sizeof(readout),
                 "family %s   %s %.7f   c %s   attractor chaos   lambda %.3f",
                 family_name(g_state.fam), param_name(g_state.fam), g_state.param, cbuf, lam);
    }
    draw_text_rect(dc, g_font_mono_small, COL_INK, right1, readout,
                   DT_SINGLELINE | DT_RIGHT | DT_END_ELLIPSIS);
    draw_text_rect(dc, g_font_mono_small, COL_INK_DIM, right2, drag_mode_name(gw),
                   DT_SINGLELINE | DT_RIGHT | DT_END_ELLIPSIS);
}

static void draw_window_shell(HDC dc, const GraphWindow *gw, int w, int h)
{
    RECT full = {0, 0, w, h};
    fill_rect(dc, &full, COL_PANEL);
    draw_header(dc, gw, w);
    draw_line(dc, 0, 0, w, 0, COL_LINE, 1, PS_SOLID);
    draw_line(dc, 0, h - 1, w, h - 1, COL_LINE, 1, PS_SOLID);
    draw_line(dc, 0, 0, 0, h, COL_LINE, 1, PS_SOLID);
    draw_line(dc, w - 1, 0, w - 1, h, COL_LINE, 1, PS_SOLID);
}

static int hover_available(const GraphWindow *gw, RECT plot)
{
    return gw && gw->hover_valid && point_in_rect(plot, gw->hover);
}

static void draw_bif_hover(HDC dc, const GraphWindow *gw, RECT plot)
{
    char line1[128];
    char line2[128];
    double p;
    double x;
    double lam;
    int per;

    if (!hover_available(gw, plot))
        return;

    p = bif_param_from_client_x(plot, gw->hover.x);
    x = bif_value_from_client_y(plot, gw->hover.y);
    per = detect_period(g_state.fam, p, 0.5, 64, 900, 1e-6);
    lam = lyapunov_value(g_state.fam, p, 0.5, 900, 400);

    draw_crosshair(dc, plot, gw->hover.x, gw->hover.y, per > 0 ? period_color(per) : COL_INK_DIM);
    snprintf(line1, sizeof(line1), "%s %.12g   x %.12g", param_name(g_state.fam), p, x);
    if (per > 0)
        snprintf(line2, sizeof(line2), "period %d   lambda %.7g", per, lam);
    else
        snprintf(line2, sizeof(line2), "period chaos   lambda %.7g", lam);
    draw_hover_box(dc, plot, gw->hover, line1, line2);
}

static void draw_cob_hover(HDC dc, const GraphWindow *gw, RECT plot)
{
    char line1[96];
    char line2[96];
    double x;
    double y;
    int fy;

    if (!hover_available(gw, plot))
        return;

    x = cob_value_from_client_x(plot, gw->hover.x);
    y = map_f(g_state.fam, x, g_state.param);
    fy = finite_double(y) ? cob_client_y_from_value(plot, y) : gw->hover.y;
    draw_crosshair(dc, plot, gw->hover.x, fy, COL_BLUE);
    snprintf(line1, sizeof(line1), "x_n %.12g", x);
    snprintf(line2, sizeof(line2), "f(x_n) %.12g", y);
    draw_hover_box(dc, plot, gw->hover, line1, line2);
}

static void draw_mandel_hover(HDC dc, const GraphWindow *gw, RECT plot)
{
    char line1[128];
    char line2[128];
    double cr;
    double ci;
    double r;
    int ok = 0;

    if (!hover_available(gw, plot))
        return;

    cr = mandel_real_from_client_x(plot, gw->hover.x);
    ci = mandel_imag_from_client_y(plot, gw->hover.y);
    r = logistic_r_from_real_c(cr, &ok);

    draw_crosshair(dc, plot, gw->hover.x, gw->hover.y, COL_PHOS);
    snprintf(line1, sizeof(line1), "c %.12g %+.12gi", cr, ci);
    if (ok)
        snprintf(line2, sizeof(line2), "inferred r %.12g", r);
    else
        snprintf(line2, sizeof(line2), "inferred r outside real bridge");
    draw_hover_box(dc, plot, gw->hover, line1, line2);
}

static void render_bif_cache(GraphWindow *gw, int w, int h)
{
    int cx, gy, gx;
    const int transient = 1000;
    const int plot = 360;
    double x_span = g_state.bif_x_max - g_state.bif_x_min;

    if (!dib_resize(&gw->cache, w, h))
        return;
    if (x_span <= 0.0)
        x_span = 1.0;

    dib_fill(&gw->cache, COL_PLOT);
    for (gx = 0; gx <= 8; ++gx) {
        int x = (int)((double)gx * (double)(w - 1) / 8.0 + 0.5);
        int y;
        for (y = 0; y < h; ++y)
            dib_set_pixel(&gw->cache, x, y, COL_GRID);
    }
    for (gy = 0; gy <= 4; ++gy) {
        int y = (int)((double)gy * (double)(h - 1) / 4.0 + 0.5);
        int x;
        for (x = 0; x < w; ++x)
            dib_set_pixel(&gw->cache, x, y, COL_GRID);
    }

    for (cx = 0; cx < w; ++cx) {
        double p = g_state.view_min + (g_state.view_max - g_state.view_min) *
                   (double)cx / (double)(w > 1 ? w - 1 : 1);
        double x = 0.5;
        int cr = 36, cg = 70, cb = 64;
        COLORREF categorical = COL_PHOS_DIM;
        int i;
        if (g_state.period_colors) {
            int per = detect_period(g_state.fam, p, 0.5, 64, 900, 1e-6);
            categorical = bif_period_color(per);
            cr = GetRValue(categorical);
            cg = GetGValue(categorical);
            cb = GetBValue(categorical);
        }
        for (i = 0; i < transient; ++i)
            x = map_f(g_state.fam, x, p);
        for (i = 0; i < plot; ++i) {
            int py;
            x = map_f(g_state.fam, x, p);
            if (!finite_double(x)) break;
            if (x < g_state.bif_x_min || x > g_state.bif_x_max)
                continue;
            py = (int)((g_state.bif_x_max - x) / x_span * (double)(h - 1) + 0.5);
            if (g_state.period_colors)
                dib_set_pixel(&gw->cache, cx, py, categorical);
            else
                dib_add_pixel(&gw->cache, cx, py, cr, cg, cb);
        }
    }

    if (g_state.lyap_overlay) {
        HPEN pen = CreatePen(PS_SOLID, 1, COL_AMBER);
        HGDIOBJ old = SelectObject(gw->cache.dc, pen);
        const double lam_min = -3.0;
        const double lam_max = 1.0;
        for (cx = 0; cx < w; ++cx) {
            double p = g_state.view_min + (g_state.view_max - g_state.view_min) *
                       (double)cx / (double)(w > 1 ? w - 1 : 1);
            double lam = lyapunov_value(g_state.fam, p, 0.4, 900, 400);
            int y;
            lam = clampd(lam, lam_min, lam_max);
            y = (int)((1.0 - (lam - lam_min) / (lam_max - lam_min)) * (double)(h - 1) + 0.5);
            if (cx == 0)
                MoveToEx(gw->cache.dc, cx, y, NULL);
            else
                LineTo(gw->cache.dc, cx, y);
        }
        SelectObject(gw->cache.dc, old);
        DeleteObject(pen);
        {
            int z = (int)((1.0 - (0.0 - lam_min) / (lam_max - lam_min)) * (double)(h - 1) + 0.5);
            draw_line(gw->cache.dc, 0, z, w, z, mix_color(COL_PLOT, COL_AMBER, 0.45), 1, PS_DOT);
            draw_text_at(gw->cache.dc, g_font_mono_small, COL_AMBER, 6, z - 14, "lambda=0");
        }
    }
    gw->cache_rev = g_bif_rev;
}

static void draw_bifurcation(HDC dc, GraphWindow *gw, int w, int h)
{
    RECT plot = plot_rect_for(GRAPH_BIF, w, h);
    int pw = plot.right - plot.left;
    int ph = plot.bottom - plot.top;
    int gx, gy;

    draw_window_shell(dc, gw, w, h);
    if (!gw->cache.bmp || gw->cache.w != pw || gw->cache.h != ph || gw->cache_rev != g_bif_rev)
        render_bif_cache(gw, pw, ph);
    if (gw->cache.dc)
        BitBlt(dc, plot.left, plot.top, pw, ph, gw->cache.dc, 0, 0, SRCCOPY);

    for (gx = 0; gx <= 8; ++gx) {
        double p = g_state.view_min + (g_state.view_max - g_state.view_min) * (double)gx / 8.0;
        int x = plot.left + (int)((double)gx * (double)(pw - 1) / 8.0 + 0.5);
        draw_textf_at(dc, g_font_mono_small, COL_INK_FAINT, x + (gx == 8 ? -42 : 2),
                      plot.bottom - 14, "%.3f", p);
    }
    for (gy = 0; gy <= 4; ++gy) {
        double xv = g_state.bif_x_max - (g_state.bif_x_max - g_state.bif_x_min) * (double)gy / 4.0;
        int y = plot.top + (int)((double)gy * (double)(ph - 1) / 4.0 + 0.5);
        draw_textf_at(dc, g_font_mono_small, COL_INK_FAINT, plot.left + 4, y + (gy == 0 ? 4 : -12),
                      "x=%.4g", xv);
    }

    if (g_state.param >= g_state.view_min && g_state.param <= g_state.view_max) {
        int mx = bif_client_x_from_param(plot, g_state.param);
        draw_line(dc, mx - 1, plot.top, mx - 1, plot.bottom, COL_PHOS_DIM, 1, PS_SOLID);
        draw_line(dc, mx, plot.top, mx, plot.bottom, COL_PHOS, 1, PS_SOLID);
        draw_line(dc, mx + 1, plot.top, mx + 1, plot.bottom, COL_PHOS_DIM, 1, PS_SOLID);
    }

    if (gw->dragging && gw->drag_mode == DRAG_ZOOM) {
        RECT z = bif_aspect_zoom_rect(plot, gw->drag_start, gw->drag_now);
        if (z.right - z.left > 2 && z.bottom - z.top > 2) {
            HBRUSH b = CreateHatchBrush(HS_DIAGCROSS, COL_PHOS_DIM);
            FrameRect(dc, &z, b);
            DeleteObject(b);
        }
    }

    draw_line(dc, plot.left, plot.top, plot.right, plot.top, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.left, plot.bottom, plot.right, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.left, plot.top, plot.left, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.right, plot.top, plot.right, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_bif_hover(dc, gw, plot);
    draw_textf_at(dc, g_font_mono_small, COL_INK_DIM, PAD, h - 20,
                  "%s [%.12g, %.12g]   x [%.12g, %.12g]",
                  param_name(g_state.fam), g_state.view_min, g_state.view_max,
                  g_state.bif_x_min, g_state.bif_x_max);
}

static void draw_cobweb(HDC dc, GraphWindow *gw, int w, int h)
{
    RECT plot = plot_rect_for(GRAPH_COB, w, h);
    int pw = plot.right - plot.left;
    int ph = plot.bottom - plot.top;
    int g;
    int per;
    double cycle[MAX_FIND_PERIOD * 6];
    int cycle_n = 0;
    int save;

    #define COB_X(v) (plot.left + (int)((v) * (double)pw + 0.5))
    #define COB_Y(v) (plot.bottom - (int)((v) * (double)ph + 0.5))

    draw_window_shell(dc, gw, w, h);
    fill_rect(dc, &plot, COL_PLOT);
    for (g = 0; g <= 4; ++g) {
        double t = (double)g / 4.0;
        draw_line(dc, COB_X(t), plot.top, COB_X(t), plot.bottom, COL_GRID, 1, PS_SOLID);
        draw_line(dc, plot.left, COB_Y(t), plot.right, COB_Y(t), COL_GRID, 1, PS_SOLID);
    }
    draw_line(dc, COB_X(0), COB_Y(0), COB_X(1), COB_Y(1), COL_AXIS, 1, PS_DASH);

    save = SaveDC(dc);
    IntersectClipRect(dc, plot.left, plot.top, plot.right, plot.bottom);
    {
        HPEN pen = CreatePen(PS_SOLID, 2, COL_BLUE);
        HGDIOBJ old = SelectObject(dc, pen);
        int i;
        for (i = 0; i <= 240; ++i) {
            double xv = (double)i / 240.0;
            double yv = map_f(g_state.fam, xv, g_state.param);
            int x = COB_X(xv);
            int y = COB_Y(yv);
            if (i == 0)
                MoveToEx(dc, x, y, NULL);
            else
                LineTo(dc, x, y);
        }
        SelectObject(dc, old);
        DeleteObject(pen);
    }

    per = detect_period(g_state.fam, g_state.param, g_state.x0, 64, 3000, 1e-7);
    if (per > 0)
        cycle_n = attractor_orbit(g_state.fam, g_state.param, g_state.x0, per, cycle, (int)ARRAYSIZE(cycle));

    {
        double x = g_state.x0;
        double prev_y = 0.0;
        int transient = g_state.show_transient ? 60 : 0;
        int steps = transient + (per > 0 ? per * 6 : 120);
        int i;
        HPEN pen_trans = CreatePen(PS_SOLID, 1, mix_color(COL_PLOT, COL_INK, 0.30));
        HPEN pen_cycle = CreatePen(PS_SOLID, 2, period_color(per));
        HGDIOBJ old = NULL;
        for (i = 0; i < steps; ++i) {
            double y = map_f(g_state.fam, x, g_state.param);
            int in_cycle = per > 0 && i >= steps - per * 6;
            if (old)
                SelectObject(dc, old);
            old = SelectObject(dc, in_cycle ? pen_cycle : pen_trans);
            MoveToEx(dc, COB_X(x), COB_Y(i == 0 ? 0.0 : prev_y), NULL);
            LineTo(dc, COB_X(x), COB_Y(y));
            MoveToEx(dc, COB_X(x), COB_Y(y), NULL);
            LineTo(dc, COB_X(y), COB_Y(y));
            prev_y = y;
            x = y;
            if (!finite_double(x)) break;
        }
        if (old)
            SelectObject(dc, old);
        DeleteObject(pen_trans);
        DeleteObject(pen_cycle);
    }

    if (cycle_n > 0) {
        HBRUSH b = CreateSolidBrush(period_color(per));
        HGDIOBJ old = SelectObject(dc, b);
        int i;
        for (i = 0; i < cycle_n; ++i) {
            int x = COB_X(cycle[i]);
            int y = COB_Y(map_f(g_state.fam, cycle[i], g_state.param));
            Ellipse(dc, x - 4, y - 4, x + 4, y + 4);
        }
        SelectObject(dc, old);
        DeleteObject(b);
    }
    RestoreDC(dc, save);

    draw_line(dc, plot.left, plot.top, plot.right, plot.top, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.left, plot.bottom, plot.right, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.left, plot.top, plot.left, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.right, plot.top, plot.right, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_text_at(dc, g_font_mono_small, COL_INK_FAINT, plot.left - 10, plot.bottom + 4, "0");
    draw_text_at(dc, g_font_mono_small, COL_INK_FAINT, plot.right - 5, plot.bottom + 4, "1");
    draw_text_at(dc, g_font_mono_small, COL_INK_FAINT, plot.left - 24, plot.top + 2, "1");
    draw_text_at(dc, g_font_mono_small, COL_INK_FAINT, plot.left - 24, plot.bottom - 14, "0");
    draw_cob_hover(dc, gw, plot);

    if (per > 0)
        draw_textf_at(dc, g_font_mono_small, period_color(per), PAD, h - 22,
                      "x0 %.6f -> period-%d cycle", g_state.x0, per);
    else
        draw_textf_at(dc, g_font_mono_small, COL_CHAOS, PAD, h - 22,
                      "x0 %.6f -> aperiodic / chaotic orbit", g_state.x0);

    #undef COB_X
    #undef COB_Y
}

static void render_mandel_cache(GraphWindow *gw, int w, int h)
{
    const int maxit = 220;
    int px, py;

    if (!dib_resize(&gw->cache, w, h))
        return;

    for (py = 0; py < h; ++py) {
        double ci = MANDEL_Y0 + (MANDEL_Y1 - MANDEL_Y0) * (double)py / (double)(h > 1 ? h - 1 : 1);
        for (px = 0; px < w; ++px) {
            double cr = MANDEL_X0 + (MANDEL_X1 - MANDEL_X0) * (double)px / (double)(w > 1 ? w - 1 : 1);
            double zr = 0.0, zi = 0.0;
            int it;
            int esc = 0;
            for (it = 0; it < maxit; ++it) {
                double nr = zr * zr - zi * zi + cr;
                double ni = 2.0 * zr * zi + ci;
                zr = nr;
                zi = ni;
                if (zr * zr + zi * zi > 4.0) {
                    esc = 1;
                    break;
                }
            }
            if (esc) {
                double t = (double)it / (double)maxit;
                double g = sqrt(t);
                int r = (int)(6.0 + g * 10.0);
                int gr = (int)(10.0 + g * 34.0);
                int b = (int)(14.0 + g * 30.0);
                dib_set_pixel(&gw->cache, px, py, RGB(r, gr, b));
            } else {
                int cyc = complex_cycle(cr, ci, 40);
                COLORREF c = cyc > 0 ? scale_color(period_color(cyc), 0.78) : RGB(40, 46, 58);
                dib_set_pixel(&gw->cache, px, py, c);
            }
        }
    }
    gw->cache_rev = g_man_rev;
}

static void draw_mandel(HDC dc, GraphWindow *gw, int w, int h)
{
    RECT plot = plot_rect_for(GRAPH_MAN, w, h);
    int pw = plot.right - plot.left;
    int ph = plot.bottom - plot.top;
    int mxden = pw > 1 ? pw - 1 : 1;
    int myden = ph > 1 ? ph - 1 : 1;
    #define MAN_X(v) (plot.left + (int)(((v) - MANDEL_X0) / (MANDEL_X1 - MANDEL_X0) * (double)mxden + 0.5))
    #define MAN_Y(v) (plot.top + (int)(((v) - MANDEL_Y0) / (MANDEL_Y1 - MANDEL_Y0) * (double)myden + 0.5))

    draw_window_shell(dc, gw, w, h);
    if (!gw->cache.bmp || gw->cache.w != pw || gw->cache.h != ph || gw->cache_rev != g_man_rev)
        render_mandel_cache(gw, pw, ph);
    if (gw->cache.dc)
        BitBlt(dc, plot.left, plot.top, pw, ph, gw->cache.dc, 0, 0, SRCCOPY);

    draw_line(dc, plot.left, MAN_Y(0), plot.right, MAN_Y(0), mix_color(COL_PLOT, COL_INK, 0.24), 1, PS_SOLID);
    draw_line(dc, MAN_X(-2.0), MAN_Y(0), MAN_X(0.25), MAN_Y(0), COL_PHOS_DIM, 2, PS_SOLID);
    draw_text_at(dc, g_font_mono_small, COL_PHOS, MAN_X(-2.0) - 4, MAN_Y(0) + 6, "r=4");
    draw_text_at(dc, g_font_mono_small, COL_PHOS, MAN_X(-0.75) - 8, MAN_Y(0) + 18, "r=3");
    draw_text_at(dc, g_font_mono_small, COL_PHOS, MAN_X(0.25) - 16, MAN_Y(0) - 16, "r=1");

    if (g_state.fam == FAM_LOGISTIC) {
        double c = logistic_r_to_c(g_state.param);
        int mx = MAN_X(c);
        int my = MAN_Y(0);
        HBRUSH white = CreateSolidBrush(RGB(255,255,255));
        HGDIOBJ old;
        draw_line(dc, mx - 7, my, mx + 7, my, RGB(255,255,255), 1, PS_SOLID);
        draw_line(dc, mx, my - 7, mx, my + 7, RGB(255,255,255), 1, PS_SOLID);
        old = SelectObject(dc, white);
        Ellipse(dc, mx - 3, my - 3, mx + 3, my + 3);
        SelectObject(dc, old);
        DeleteObject(white);
        draw_textf_at(dc, g_font_mono_small, COL_INK, PAD, h - 22, "c = %.5f", c);
    } else {
        draw_textf_at(dc, g_font_mono_small, COL_INK_DIM, PAD, h - 22,
                      "%s: no quadratic conjugacy; drag sweeps %s",
                      family_name(g_state.fam), param_name(g_state.fam));
    }

    draw_line(dc, plot.left, plot.top, plot.right, plot.top, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.left, plot.bottom, plot.right, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.left, plot.top, plot.left, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, plot.right, plot.top, plot.right, plot.bottom, COL_LINE, 1, PS_SOLID);
    draw_mandel_hover(dc, gw, plot);

    #undef MAN_X
    #undef MAN_Y
}

static double fp_iter(FamilyKind fam, double param, double x, int p)
{
    int i;
    for (i = 0; i < p; ++i)
        x = map_f(fam, x, param);
    return x;
}

static double dfp_iter(FamilyKind fam, double param, double x, int p)
{
    double d = 1.0;
    double y = x;
    int i;
    for (i = 0; i < p; ++i) {
        d *= map_df(fam, y, param);
        y = map_f(fam, y, param);
    }
    return d;
}

static void add_root(double *roots, int *count, int cap, double x)
{
    int i;
    if (*count >= cap)
        return;
    for (i = 0; i < *count; ++i) {
        if (fabs(roots[i] - x) < 1e-7)
            return;
    }
    roots[(*count)++] = x;
}

static int lower_period_root(FamilyKind fam, double param, int p, double x0)
{
    int q;
    for (q = 1; q < p; ++q) {
        if (p % q == 0) {
            double y = fp_iter(fam, param, x0, q);
            if (fabs(y - x0) < 1e-6)
                return 1;
        }
    }
    return 0;
}

static int cmp_cycle_multiplier(const void *a, const void *b)
{
    const CycleInfo *ca = (const CycleInfo *)a;
    const CycleInfo *cb = (const CycleInfo *)b;
    double aa = fabs(ca->multiplier);
    double bb = fabs(cb->multiplier);
    if (aa < bb) return -1;
    if (aa > bb) return 1;
    return 0;
}

static void find_periodic_cycles(void)
{
    const int grid_n = 6500;
    double *roots = (double *)calloc((size_t)grid_n + 2, sizeof(double));
    unsigned char *used = (unsigned char *)calloc((size_t)grid_n + 2, 1);
    int root_count = 0;
    int i;
    int p = g_state.find_period;
    double lo = 0.0, hi = 1.0;
    double xa = lo;
    double ha = fp_iter(g_state.fam, g_state.param, xa, p) - xa;

    if (!roots || !used) {
        free(roots);
        free(used);
        return;
    }

    for (i = 1; i <= grid_n; ++i) {
        double xb = lo + (hi - lo) * (double)i / (double)grid_n;
        double hb = fp_iter(g_state.fam, g_state.param, xb, p) - xb;
        if (fabs(ha) < 1e-14) {
            add_root(roots, &root_count, grid_n + 2, xa);
        } else if (ha * hb < 0.0) {
            double a = xa, b = xb, fa = ha;
            double x;
            int j;
            for (j = 0; j < 50; ++j) {
                double m = 0.5 * (a + b);
                double fm = fp_iter(g_state.fam, g_state.param, m, p) - m;
                if (fa * fm <= 0.0)
                    b = m;
                else {
                    a = m;
                    fa = fm;
                }
            }
            x = 0.5 * (a + b);
            for (j = 0; j < 14; ++j) {
                double hv = fp_iter(g_state.fam, g_state.param, x, p) - x;
                double hp = dfp_iter(g_state.fam, g_state.param, x, p) - 1.0;
                double nx;
                if (fabs(hp) < 1e-13)
                    break;
                nx = x - hv / hp;
                if (!finite_double(nx))
                    break;
                x = nx;
                if (fabs(hv) < 1e-15)
                    break;
            }
            if (x >= lo - 1e-9 && x <= hi + 1e-9)
                add_root(roots, &root_count, grid_n + 2, x);
        }
        xa = xb;
        ha = hb;
    }

    g_state.cycle_count = 0;
    g_state.stable_cycle_count = 0;
    for (i = 0; i < root_count; ++i) {
        double orbit[MAX_FIND_PERIOD];
        double x0;
        double y;
        double mult = 1.0;
        int j, k, mi;
        if (used[i])
            continue;
        x0 = roots[i];
        if (lower_period_root(g_state.fam, g_state.param, p, x0)) {
            used[i] = 1;
            continue;
        }
        orbit[0] = x0;
        y = map_f(g_state.fam, x0, g_state.param);
        for (k = 1; k < p; ++k) {
            orbit[k] = y;
            y = map_f(g_state.fam, y, g_state.param);
        }
        for (j = 0; j < root_count; ++j) {
            if (!used[j]) {
                int hit = 0;
                for (k = 0; k < p; ++k) {
                    if (fabs(orbit[k] - roots[j]) < 1e-6) {
                        hit = 1;
                        break;
                    }
                }
                if (hit)
                    used[j] = 1;
            }
        }
        for (k = 0; k < p; ++k)
            mult *= map_df(g_state.fam, orbit[k], g_state.param);

        mi = 0;
        for (k = 1; k < p; ++k) {
            if (orbit[k] < orbit[mi])
                mi = k;
        }
        if (fabs(mult) < 1.0 && p <= ITERATOR_MRU_CYCLE_POINT_MAX) {
            double stable_orbit[ITERATOR_MRU_CYCLE_POINT_MAX];
            for (k = 0; k < p; ++k)
                stable_orbit[k] = orbit[(mi + k) % p];
            add_stable_cycle_mru(p, g_state.param, mult, stable_orbit);
            ++g_state.stable_cycle_count;
        }
        if (g_state.cycle_count < MAX_CYCLES) {
            CycleInfo *ci = &g_state.cycles[g_state.cycle_count++];
            ci->period = p;
            ci->multiplier = mult;
            for (k = 0; k < p; ++k)
                ci->orbit[k] = orbit[(mi + k) % p];
        }
    }
    qsort(g_state.cycles, (size_t)g_state.cycle_count, sizeof(g_state.cycles[0]), cmp_cycle_multiplier);
    g_state.cycles_valid = 1;
    free(roots);
    free(used);
}

static void estimate_feigenbaum(void)
{
    const int periods[5] = {1, 2, 4, 8, 16};
    double s[5];
    int i;
    double lo = family_full_min(g_state.fam);
    double hi = family_full_max(g_state.fam);
    double start = lo;

    g_state.feig_count = 0;
    if (!has_smooth_critical(g_state.fam)) {
        snprintf(g_state.feig_lines[g_state.feig_count++], sizeof(g_state.feig_lines[0]),
                 "tent map: no smooth critical orbit");
        snprintf(g_state.feig_lines[g_state.feig_count++], sizeof(g_state.feig_lines[0]),
                 "period doubling is not the organizing cascade");
        g_state.feig_valid = 1;
        return;
    }

    for (i = 0; i < 5; ++i) {
        s[i] = superstable_period(g_state.fam, periods[i], start, hi);
        snprintf(g_state.feig_lines[g_state.feig_count++], sizeof(g_state.feig_lines[0]),
                 "2^%d (period %d): %s = %.6f", i, periods[i],
                 param_name(g_state.fam), s[i]);
        if (finite_double(s[i]))
            start = s[i] + (hi - lo) * 1e-6;
    }
    for (i = 1; i < 4; ++i) {
        double d = (s[i] - s[i - 1]) / (s[i + 1] - s[i]);
        snprintf(g_state.feig_lines[g_state.feig_count++], sizeof(g_state.feig_lines[0]),
                 "delta_%d = %.5f -> 4.66920", i, d);
    }
    g_state.feig_valid = 1;
}

static void draw_param_ruler(HDC dc, RECT r)
{
    double lo = family_full_min(g_state.fam);
    double hi = family_full_max(g_state.fam);
    int i;
    int y = (r.top + r.bottom) / 2;
    int mx = r.left + (int)((g_state.param - lo) / (hi - lo) * (double)(r.right - r.left) + 0.5);
    fill_rect(dc, &r, COL_PLOT);
    draw_line(dc, r.left, y, r.right, y, COL_AXIS, 1, PS_SOLID);
    for (i = 0; i <= 8; ++i) {
        double p = lo + (hi - lo) * (double)i / 8.0;
        int x = r.left + (int)((double)i * (double)(r.right - r.left) / 8.0 + 0.5);
        draw_line(dc, x, y - 6, x, y + 6, COL_GRID, 1, PS_SOLID);
        draw_textf_at(dc, g_font_mono_small, COL_INK_FAINT, x + (i == 8 ? -34 : 2), y + 9, "%.2f", p);
    }
    draw_line(dc, mx - 1, r.top + 2, mx - 1, r.bottom - 2, COL_PHOS_DIM, 1, PS_SOLID);
    draw_line(dc, mx, r.top + 2, mx, r.bottom - 2, COL_PHOS, 1, PS_SOLID);
    draw_textf_at(dc, g_font_mono_small, COL_PHOS, r.left + 6, r.top + 5, "%s %.7f", param_name(g_state.fam), g_state.param);
    draw_line(dc, r.left, r.top, r.right, r.top, COL_LINE, 1, PS_SOLID);
    draw_line(dc, r.left, r.bottom, r.right, r.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, r.left, r.top, r.left, r.bottom, COL_LINE, 1, PS_SOLID);
    draw_line(dc, r.right, r.top, r.right, r.bottom, COL_LINE, 1, PS_SOLID);
}

static void draw_legend(HDC dc, int x, int y)
{
    int p;
    draw_text_at(dc, g_font_mono_small, COL_INK_FAINT, x, y, "PERIOD");
    x += 58;
    for (p = 1; p <= 8; ++p) {
        HBRUSH b = CreateSolidBrush(period_color(p));
        HGDIOBJ old = SelectObject(dc, b);
        Ellipse(dc, x, y + 3, x + 8, y + 11);
        SelectObject(dc, old);
        DeleteObject(b);
        draw_textf_at(dc, g_font_mono_small, COL_INK_DIM, x + 13, y, "%d", p);
        x += 34;
    }
    {
        HBRUSH b = CreateSolidBrush(COL_CHAOS);
        HGDIOBJ old = SelectObject(dc, b);
        Ellipse(dc, x, y + 3, x + 8, y + 11);
        SelectObject(dc, old);
        DeleteObject(b);
        draw_text_at(dc, g_font_mono_small, COL_INK_DIM, x + 13, y, "chaos");
    }
}

static void draw_cycle_lab(HDC dc, GraphWindow *gw, int w, int h)
{
    RECT body = plot_rect_for(GRAPH_LAB, w, h);
    RECT left = body;
    RECT right = body;
    RECT ruler;
    int per;
    double lam;
    double mult = 0.0;
    double orbit[MAX_FIND_PERIOD * 6];
    int orbit_n = 0;
    int y;

    draw_window_shell(dc, gw, w, h);

    left.right = left.left + (w < 680 ? w / 2 - 18 : 315);
    if (left.right > body.right - 180)
        left.right = body.right - 180;
    if (left.right < body.left + 220)
        left.right = body.left + 220;
    right.left = left.right + 14;
    fill_rect(dc, &body, COL_PANEL);

    per = detect_period(g_state.fam, g_state.param, g_state.x0, 64, 3000, 1e-7);
    lam = lyapunov_value(g_state.fam, g_state.param, g_state.x0, 2000, 800);
    if (per > 0) {
        int i;
        orbit_n = attractor_orbit(g_state.fam, g_state.param, g_state.x0, per, orbit, (int)ARRAYSIZE(orbit));
        mult = 1.0;
        for (i = 0; i < orbit_n; ++i)
            mult *= map_df(g_state.fam, orbit[i], g_state.param);
    }

    y = body.top;
    draw_text_at(dc, g_font_small, COL_INK_FAINT, left.left, y, "ATTRACTOR PERIOD");
    draw_textf_at(dc, g_font_mono, per > 0 ? period_color(per) : COL_CHAOS, left.left, y + 16,
                  per > 0 ? "%d" : "chaos", per);
    y += 48;
    draw_text_at(dc, g_font_small, COL_INK_FAINT, left.left, y, "MULTIPLIER |product f'|");
    if (per > 0)
        draw_textf_at(dc, g_font_mono, fabs(mult) < 1.0 ? COL_PHOS : COL_AMBER, left.left, y + 16, "%.6f", fabs(mult));
    else
        draw_text_at(dc, g_font_mono, COL_INK_DIM, left.left, y + 16, "-");
    y += 48;
    draw_text_at(dc, g_font_small, COL_INK_FAINT, left.left, y, "LYAPUNOV");
    draw_textf_at(dc, g_font_mono, lam > 0.0 ? COL_MAGENTA : COL_PHOS, left.left, y + 16, "%.6f", lam);
    y += 48;
    draw_text_at(dc, g_font_small, COL_INK_FAINT, left.left, y, "VERDICT");
    if (per > 0 && fabs(mult) < 1.0)
        draw_textf_at(dc, g_font_mono_small, COL_PHOS, left.left, y + 18, "stable period-%d attractor", per);
    else if (per == 0 || lam > 0.0005)
        draw_text_at(dc, g_font_mono_small, COL_MAGENTA, left.left, y + 18, "chaotic regime");
    else
        draw_text_at(dc, g_font_mono_small, COL_AMBER, left.left, y + 18, "marginal / bifurcation point");
    y += 54;

    draw_textf_at(dc, g_font_mono_small, COL_INK_DIM, left.left, y, "x0 %.6f   scan period %d", g_state.x0, g_state.find_period);
    y += 30;
    draw_text_at(dc, g_font_small, COL_INK_FAINT, left.left, y, "FEIGENBAUM CASCADE");
    y += 17;
    if (g_state.feig_valid) {
        int i;
        for (i = 0; i < g_state.feig_count && y < body.bottom - 20; ++i) {
            draw_text_at(dc, g_font_mono_small, i >= 5 ? COL_AMBER : COL_INK_DIM, left.left, y, g_state.feig_lines[i]);
            y += 15;
        }
    } else {
        draw_text_at(dc, g_font_mono_small, COL_INK_DIM, left.left, y, "not estimated");
    }

    draw_line(dc, left.right + 6, body.top, left.right + 6, body.bottom, COL_LINE, 1, PS_SOLID);

    ruler = right;
    ruler.bottom = ruler.top + 64;
    draw_param_ruler(dc, ruler);

    y = ruler.bottom + 16;
    draw_textf_at(dc, g_font_small, COL_INK_FAINT, right.left, y,
                  "PRIMITIVE PERIOD-%d CYCLES", g_state.find_period);
    y += 20;
    if (g_state.cycles_valid) {
        int i;
        draw_textf_at(dc, g_font_mono_small, COL_INK_DIM, right.left, y,
                      "%d orbit%s found   %d stable preserved",
                      g_state.cycle_count, g_state.cycle_count == 1 ? "" : "s",
                      g_state.stable_cycle_count);
        y += 20;
        for (i = 0; i < g_state.cycle_count && y < body.bottom - 34; ++i) {
            CycleInfo *ci = &g_state.cycles[i];
            char line[512];
            int k;
            int off = 0;
            HBRUSH b = CreateSolidBrush(period_color(ci->period));
            HGDIOBJ old = SelectObject(dc, b);
            Ellipse(dc, right.left, y + 4, right.left + 9, y + 13);
            SelectObject(dc, old);
            DeleteObject(b);

            off += snprintf(line + off, sizeof(line) - (size_t)off, "{ ");
            for (k = 0; k < ci->period && off < (int)sizeof(line) - 32; ++k)
                off += snprintf(line + off, sizeof(line) - (size_t)off, "%s%.6f", k ? ", " : "", ci->orbit[k]);
            snprintf(line + off, sizeof(line) - (size_t)off, " }");
            {
                RECT tr = {right.left + 16, y, right.right - 118, y + 16};
                draw_text_rect(dc, g_font_mono_small, COL_INK, tr, line, DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            draw_textf_at(dc, g_font_mono_small, fabs(ci->multiplier) < 1.0 ? COL_PHOS : COL_CHAOS,
                          right.right - 112, y, "%s %.4f",
                          fabs(ci->multiplier) < 1.0 ? "stable" : "unstable",
                          fabs(ci->multiplier));
            y += 20;
        }
        if (i < g_state.cycle_count)
            draw_textf_at(dc, g_font_mono_small, COL_INK_DIM, right.left + 16, y, "... %d more", g_state.cycle_count - i);
    } else {
        draw_text_at(dc, g_font_mono_small, COL_INK_DIM, right.left, y, "not scanned");
    }

    draw_legend(dc, body.left, h - 22);
}

static void paint_graph(GraphWindow *gw, HDC hdc)
{
    RECT rc;
    int w, h;
    HDC mem;
    HBITMAP bmp;
    HGDIOBJ old;

    GetClientRect(gw->hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
        return;

    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, w, h);
    old = SelectObject(mem, bmp);
    SetBkMode(mem, TRANSPARENT);

    switch (gw->kind) {
    case GRAPH_BIF: draw_bifurcation(mem, gw, w, h); break;
    case GRAPH_COB: draw_cobweb(mem, gw, w, h); break;
    case GRAPH_MAN: draw_mandel(mem, gw, w, h); break;
    case GRAPH_LAB: draw_cycle_lab(mem, gw, w, h); break;
    default: break;
    }

    BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void show_graph_window(GraphKind kind);

static RECT active_monitor_work_rect(HWND origin)
{
    RECT r;
    HMONITOR mon = NULL;
    MONITORINFO mi;

    if (origin)
        mon = MonitorFromWindow(origin, MONITOR_DEFAULTTONEAREST);
    if (!mon) {
        POINT pt = {0, 0};
        mon = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    }

    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(mi);
    if (mon && GetMonitorInfoA(mon, &mi))
        return mi.rcWork;

    SystemParametersInfoA(SPI_GETWORKAREA, 0, &r, 0);
    return r;
}

static void tile_windows(GraphWindow *origin)
{
    RECT wa = active_monitor_work_rect(origin ? origin->hwnd : NULL);
    int ww, wh, gap = 12;
    int i;
    for (i = 0; i < GRAPH_COUNT; ++i)
        show_graph_window((GraphKind)i);
    ww = (wa.right - wa.left - gap * 3) / 2;
    wh = (wa.bottom - wa.top - gap * 3) / 2;
    if (ww < 420) ww = 420;
    if (wh < 300) wh = 300;
    for (i = 0; i < GRAPH_COUNT; ++i) {
        int col = i % 2;
        int row = i / 2;
        MoveWindow(g_windows[i].hwnd,
                   wa.left + gap + col * (ww + gap),
                   wa.top + gap + row * (wh + gap),
                   ww, wh, TRUE);
    }
}

static void focus_graph_relative(GraphWindow *origin, int dir)
{
    int start = origin ? (int)origin->kind : 0;
    int i;
    for (i = 1; i <= GRAPH_COUNT; ++i) {
        int idx = (start + dir * i + GRAPH_COUNT * 2) % GRAPH_COUNT;
        HWND hwnd;
        show_graph_window((GraphKind)idx);
        hwnd = g_windows[idx].hwnd;
        if (hwnd) {
            ShowWindow(hwnd, SW_SHOWNORMAL);
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
            return;
        }
    }
}

static void append_checked(HMENU menu, UINT id, const char *label, int checked)
{
    AppendMenuA(menu, MF_STRING | (checked ? MF_CHECKED : 0), id, label);
}

static void show_context_menu(GraphWindow *gw, POINT pt)
{
    HMENU menu = CreatePopupMenu();
    HMENU fam = CreatePopupMenu();
    char period_label[64];

    append_checked(fam, IDM_FAMILY_LOGISTIC, "Logistic family\tCtrl+1", g_state.fam == FAM_LOGISTIC);
    append_checked(fam, IDM_FAMILY_TENT, "Tent family\tCtrl+2", g_state.fam == FAM_TENT);
    append_checked(fam, IDM_FAMILY_SINE, "Sine family\tCtrl+3", g_state.fam == FAM_SINE);
    AppendMenuA(menu, MF_POPUP, (UINT_PTR)fam, "Family");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);

    switch (gw->kind) {
    case GRAPH_BIF:
        append_checked(menu, IDM_BIF_MODE_PARAM, "Drag sets parameter", gw->drag_mode == DRAG_PARAM);
        append_checked(menu, IDM_BIF_MODE_ZOOM, "Drag aspect box-zooms 2D", gw->drag_mode == DRAG_ZOOM);
        AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
        append_checked(menu, IDM_BIF_LYAP, "Lyapunov overlay\tL", g_state.lyap_overlay);
        append_checked(menu, IDM_BIF_PERIOD_COLORS, "Period spectrum; chaos black\tP", g_state.period_colors);
        AppendMenuA(menu, MF_STRING, IDM_BIF_RESET_VIEW, "Reset bifurcation view\tBackspace");
        {
            HMENU recent = CreatePopupMenu();
            IteratorMruBifViewRecord recs[ITERATOR_MRU_BIF_VIEW_MAX];
            int count = 0;
            int i;
            IteratorMruLoadBifViews(recs, (int)ARRAYSIZE(recs), &count);
            for (i = 0; i < count && i < 10; ++i) {
                char label[160];
                snprintf(label, sizeof(label), "%s %s[%.6g, %.6g] x[%.6g, %.6g]",
                         family_name((FamilyKind)recs[i].family),
                         param_name((FamilyKind)recs[i].family),
                         recs[i].param_min, recs[i].param_max,
                         recs[i].x_min, recs[i].x_max);
                AppendMenuA(recent, MF_STRING, IDM_BIF_RECENT_BASE + i, label);
            }
            if (count == 0)
                AppendMenuA(recent, MF_STRING | MF_GRAYED, 0, "(empty)");
            AppendMenuA(menu, MF_POPUP, (UINT_PTR)recent, "Recent bifurcation views");
        }
        break;
    case GRAPH_COB:
        append_checked(menu, IDM_COB_MODE_SEED, "Drag sets x0", gw->drag_mode == DRAG_SEED);
        AppendMenuA(menu, MF_STRING, IDM_COB_RESEED, "New x0\tSpace");
        append_checked(menu, IDM_COB_TRANSIENT, "Show transient", g_state.show_transient);
        break;
    case GRAPH_MAN:
        append_checked(menu, IDM_MAN_MODE_PARAM, "Drag sets parameter", gw->drag_mode == DRAG_PARAM);
        AppendMenuA(menu, MF_STRING, IDM_MAN_REBUILD, "Rebuild period map\tF5");
        break;
    case GRAPH_LAB:
        append_checked(menu, IDM_LAB_MODE_PARAM, "Drag sets parameter", gw->drag_mode == DRAG_PARAM);
        snprintf(period_label, sizeof(period_label), "Search period: %d", g_state.find_period);
        AppendMenuA(menu, MF_STRING | MF_GRAYED, 0, period_label);
        AppendMenuA(menu, MF_STRING, IDM_LAB_PERIOD_DEC, "Search period -\t-");
        AppendMenuA(menu, MF_STRING, IDM_LAB_PERIOD_INC, "Search period +\t+");
        AppendMenuA(menu, MF_STRING, IDM_LAB_FIND, "Scan + Newton refine\tEnter");
        AppendMenuA(menu, MF_STRING, IDM_LAB_FEIG, "Estimate Feigenbaum delta");
        {
            HMENU recent = CreatePopupMenu();
            IteratorMruCyclePeriodRecord recs[ITERATOR_MRU_CYCLE_PERIOD_MAX];
            int count = 0;
            int i;
            IteratorMruLoadCyclePeriods(recs, (int)ARRAYSIZE(recs), &count);
            for (i = 0; i < count && i < 10; ++i) {
                char label[80];
                snprintf(label, sizeof(label), "%s period %d",
                         family_name((FamilyKind)recs[i].family), recs[i].period);
                AppendMenuA(recent, MF_STRING, IDM_LAB_RECENT_PERIOD_BASE + i, label);
            }
            if (count == 0)
                AppendMenuA(recent, MF_STRING | MF_GRAYED, 0, "(empty)");
            AppendMenuA(menu, MF_POPUP, (UINT_PTR)recent, "Recent search periods");
        }
        {
            HMENU stable = CreatePopupMenu();
            IteratorMruStableCycleRecord recs[ITERATOR_MRU_STABLE_CYCLE_MAX];
            int count = 0;
            int i;
            IteratorMruLoadStableCycles(recs, (int)ARRAYSIZE(recs), &count);
            for (i = 0; i < count && i < 12; ++i) {
                char label[128];
                snprintf(label, sizeof(label), "%s %s=%.9g period %d |m| %.5g",
                         family_name((FamilyKind)recs[i].family),
                         param_name((FamilyKind)recs[i].family),
                         recs[i].param,
                         recs[i].period,
                         fabs(recs[i].multiplier));
                AppendMenuA(stable, MF_STRING, IDM_LAB_STABLE_CYCLE_BASE + i, label);
            }
            if (count == 0)
                AppendMenuA(stable, MF_STRING | MF_GRAYED, 0, "(empty)");
            AppendMenuA(menu, MF_POPUP, (UINT_PTR)stable, "Stable cycles found");
        }
        {
            HMENU exp = CreatePopupMenu();
            AppendMenuA(exp, MF_STRING, IDM_LAB_EXPORT_R8, "Q0.8 R table");
            AppendMenuA(exp, MF_STRING, IDM_LAB_EXPORT_R16, "Q0.16 R table");
            AppendMenuA(exp, MF_STRING, IDM_LAB_EXPORT_R32, "Q0.32 R table");
            AppendMenuA(exp, MF_STRING, IDM_LAB_EXPORT_R52, "Q0.52 R table");
            AppendMenuA(menu, MF_POPUP, (UINT_PTR)exp, "Export stable logistic R");
        }
        break;
    default:
        break;
    }

    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_PARAM_STEP_LEFT, "Step parameter left\tLeft");
    AppendMenuA(menu, MF_STRING, IDM_PARAM_STEP_RIGHT, "Step parameter right\tRight");
    AppendMenuA(menu, MF_STRING | MF_GRAYED, 0, "Ctrl+Left/Right fine; Shift coarse");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_SHOW_ALL, "Show all graph windows\tA");
    AppendMenuA(menu, MF_STRING, IDM_TILE_WINDOWS, "Tile graph windows\tT");
    AppendMenuA(menu, MF_STRING, IDM_RESET_ALL, "Reset all parameters\tCtrl+R");
    AppendMenuA(menu, MF_STRING, IDM_FOCUS_NEXT, "Next graph window\tTab");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, IDM_CLOSE_THIS, "Close this graph");
    AppendMenuA(menu, MF_STRING, IDM_EXIT_APP, "Exit iterator");

    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, gw->hwnd, NULL);
    DestroyMenu(menu);
}

static void command_from_window(GraphWindow *gw, int id)
{
    if (id >= IDM_BIF_RECENT_BASE && id < IDM_BIF_RECENT_BASE + ITERATOR_MRU_BIF_VIEW_MAX) {
        IteratorMruBifViewRecord recs[ITERATOR_MRU_BIF_VIEW_MAX];
        int count = 0;
        int idx = id - IDM_BIF_RECENT_BASE;
        if (IteratorMruLoadBifViews(recs, (int)ARRAYSIZE(recs), &count) && idx >= 0 && idx < count)
            apply_bif_view_record(&recs[idx]);
        return;
    }
    if (id >= IDM_LAB_RECENT_PERIOD_BASE &&
        id < IDM_LAB_RECENT_PERIOD_BASE + ITERATOR_MRU_CYCLE_PERIOD_MAX) {
        IteratorMruCyclePeriodRecord recs[ITERATOR_MRU_CYCLE_PERIOD_MAX];
        int count = 0;
        int idx = id - IDM_LAB_RECENT_PERIOD_BASE;
        if (IteratorMruLoadCyclePeriods(recs, (int)ARRAYSIZE(recs), &count) && idx >= 0 && idx < count)
            apply_cycle_period_record(&recs[idx]);
        return;
    }
    if (id >= IDM_LAB_STABLE_CYCLE_BASE &&
        id < IDM_LAB_STABLE_CYCLE_BASE + ITERATOR_MRU_STABLE_CYCLE_MAX) {
        IteratorMruStableCycleRecord recs[ITERATOR_MRU_STABLE_CYCLE_MAX];
        int count = 0;
        int idx = id - IDM_LAB_STABLE_CYCLE_BASE;
        if (IteratorMruLoadStableCycles(recs, (int)ARRAYSIZE(recs), &count) && idx >= 0 && idx < count)
            apply_stable_cycle_record(&recs[idx]);
        return;
    }
    if (id == IDM_LAB_EXPORT_R8) {
        export_stable_logistic_r_bits(8);
        return;
    }
    if (id == IDM_LAB_EXPORT_R16) {
        export_stable_logistic_r_bits(16);
        return;
    }
    if (id == IDM_LAB_EXPORT_R32) {
        export_stable_logistic_r_bits(32);
        return;
    }
    if (id == IDM_LAB_EXPORT_R52) {
        export_stable_logistic_r_bits(52);
        return;
    }

    switch (id) {
    case IDM_FAMILY_LOGISTIC:
        set_family(FAM_LOGISTIC);
        break;
    case IDM_FAMILY_TENT:
        set_family(FAM_TENT);
        break;
    case IDM_FAMILY_SINE:
        set_family(FAM_SINE);
        break;
    case IDM_SHOW_ALL:
        {
            int i;
            for (i = 0; i < GRAPH_COUNT; ++i)
                show_graph_window((GraphKind)i);
            invalidate_all();
        }
        break;
    case IDM_TILE_WINDOWS:
        tile_windows(gw);
        break;
    case IDM_RESET_ALL:
        reset_state(g_state.fam);
        invalidate_all();
        break;
    case IDM_PARAM_STEP_LEFT:
        step_param_by_view_fraction(-0.01);
        break;
    case IDM_PARAM_STEP_RIGHT:
        step_param_by_view_fraction(0.01);
        break;
    case IDM_PARAM_STEP_FINE_LEFT:
        step_param_by_view_fraction(-0.001);
        break;
    case IDM_PARAM_STEP_FINE_RIGHT:
        step_param_by_view_fraction(0.001);
        break;
    case IDM_PARAM_STEP_COARSE_LEFT:
        step_param_by_view_fraction(-0.05);
        break;
    case IDM_PARAM_STEP_COARSE_RIGHT:
        step_param_by_view_fraction(0.05);
        break;
    case IDM_FOCUS_NEXT:
        focus_graph_relative(gw, 1);
        break;
    case IDM_FOCUS_PREV:
        focus_graph_relative(gw, -1);
        break;
    case IDM_CLOSE_THIS:
        DestroyWindow(gw->hwnd);
        break;
    case IDM_EXIT_APP:
        {
            int i;
            for (i = 0; i < GRAPH_COUNT; ++i) {
                if (g_windows[i].hwnd)
                    DestroyWindow(g_windows[i].hwnd);
            }
        }
        PostQuitMessage(0);
        break;

    case IDM_BIF_MODE_PARAM:
        gw->drag_mode = DRAG_PARAM;
        InvalidateRect(gw->hwnd, NULL, FALSE);
        break;
    case IDM_BIF_MODE_ZOOM:
        gw->drag_mode = DRAG_ZOOM;
        InvalidateRect(gw->hwnd, NULL, FALSE);
        break;
    case IDM_BIF_LYAP:
        g_state.lyap_overlay = !g_state.lyap_overlay;
        ++g_bif_rev;
        invalidate_all();
        break;
    case IDM_BIF_PERIOD_COLORS:
        g_state.period_colors = !g_state.period_colors;
        ++g_bif_rev;
        invalidate_all();
        break;
    case IDM_BIF_RESET_VIEW:
        g_state.view_min = family_def_min(g_state.fam);
        g_state.view_max = family_def_max(g_state.fam);
        g_state.bif_x_min = 0.0;
        g_state.bif_x_max = 1.0;
        add_current_bif_view_mru();
        ++g_bif_rev;
        invalidate_all();
        break;

    case IDM_COB_MODE_SEED:
        gw->drag_mode = DRAG_SEED;
        InvalidateRect(gw->hwnd, NULL, FALSE);
        break;
    case IDM_COB_RESEED:
        g_state.x0 = 0.05 + 0.9 * (double)rand() / (double)RAND_MAX;
        clear_analysis_cache();
        invalidate_all();
        break;
    case IDM_COB_TRANSIENT:
        g_state.show_transient = !g_state.show_transient;
        invalidate_all();
        break;

    case IDM_MAN_MODE_PARAM:
        gw->drag_mode = DRAG_PARAM;
        InvalidateRect(gw->hwnd, NULL, FALSE);
        break;
    case IDM_MAN_REBUILD:
        ++g_man_rev;
        invalidate_all();
        break;

    case IDM_LAB_MODE_PARAM:
        gw->drag_mode = DRAG_PARAM;
        InvalidateRect(gw->hwnd, NULL, FALSE);
        break;
    case IDM_LAB_PERIOD_DEC:
        if (g_state.find_period > 1)
            --g_state.find_period;
        add_current_cycle_period_mru();
        clear_analysis_cache();
        invalidate_all();
        break;
    case IDM_LAB_PERIOD_INC:
        if (g_state.find_period < MAX_FIND_PERIOD)
            ++g_state.find_period;
        add_current_cycle_period_mru();
        clear_analysis_cache();
        invalidate_all();
        break;
    case IDM_LAB_FIND:
        add_current_cycle_period_mru();
        find_periodic_cycles();
        invalidate_all();
        break;
    case IDM_LAB_FEIG:
        estimate_feigenbaum();
        invalidate_all();
        break;
    default:
        break;
    }
    save_current_session();
}

static int graph_supports_hover(GraphKind kind)
{
    return kind == GRAPH_BIF || kind == GRAPH_COB || kind == GRAPH_MAN;
}

static void track_mouse_leave(GraphWindow *gw)
{
    TRACKMOUSEEVENT tme;
    if (!gw || !gw->hwnd || gw->tracking_mouse)
        return;
    ZeroMemory(&tme, sizeof(tme));
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = gw->hwnd;
    if (TrackMouseEvent(&tme))
        gw->tracking_mouse = 1;
}

static void clear_hover(GraphWindow *gw)
{
    if (!gw)
        return;
    if (gw->hover_valid) {
        gw->hover_valid = 0;
        if (gw->hwnd)
            InvalidateRect(gw->hwnd, NULL, FALSE);
    }
}

static void update_hover(GraphWindow *gw, POINT p)
{
    RECT rc;
    RECT plot;
    int w;
    int h;
    int valid = 0;

    if (!gw || !gw->hwnd || !graph_supports_hover(gw->kind)) {
        clear_hover(gw);
        return;
    }

    GetClientRect(gw->hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    plot = plot_rect_for(gw->kind, w, h);
    valid = point_in_rect(plot, p);
    if (!valid) {
        clear_hover(gw);
        return;
    }

    if (!gw->hover_valid || gw->hover.x != p.x || gw->hover.y != p.y) {
        gw->hover_valid = 1;
        gw->hover = p;
        InvalidateRect(gw->hwnd, NULL, FALSE);
    }
}

static void start_drag(GraphWindow *gw, POINT p)
{
    RECT rc;
    GetClientRect(gw->hwnd, &rc);
    gw->dragging = 1;
    gw->drag_start = p;
    gw->drag_now = p;
    SetCapture(gw->hwnd);
}

static void handle_drag(GraphWindow *gw, POINT p, int finished)
{
    RECT rc;
    RECT plot;
    int w, h;

    GetClientRect(gw->hwnd, &rc);
    w = rc.right - rc.left;
    h = rc.bottom - rc.top;
    plot = plot_rect_for(gw->kind, w, h);
    gw->drag_now = p;

    if (gw->kind == GRAPH_BIF) {
        if (gw->drag_mode == DRAG_PARAM) {
            set_param(bif_param_from_client_x(plot, p.x));
        } else if (finished) {
            RECT z = bif_aspect_zoom_rect(plot, gw->drag_start, gw->drag_now);
            if (z.right - z.left > 2 && z.bottom - z.top > 2) {
                double p_min = bif_param_from_client_x(plot, z.left);
                double p_max = bif_param_from_client_x(plot, z.right);
                double x_max = bif_value_from_client_y(plot, z.top);
                double x_min = bif_value_from_client_y(plot, z.bottom);
                ensure_representable_span(&p_min, &p_max,
                                          family_full_min(g_state.fam),
                                          family_full_max(g_state.fam));
                ensure_representable_span(&x_min, &x_max, 0.0, 1.0);
                g_state.view_min = p_min;
                g_state.view_max = p_max;
                g_state.bif_x_min = x_min;
                g_state.bif_x_max = x_max;
                add_current_bif_view_mru();
                ++g_bif_rev;
                invalidate_all();
            }
        } else {
            InvalidateRect(gw->hwnd, NULL, FALSE);
        }
    } else if (gw->kind == GRAPH_COB) {
        double t = (double)(clampi(p.x, plot.left, plot.right) - plot.left) /
                   (double)(plot.right - plot.left);
        g_state.x0 = clampd(t, 0.0, 1.0);
        clear_analysis_cache();
        invalidate_all();
    } else if (gw->kind == GRAPH_MAN) {
        double t = (double)(clampi(p.x, plot.left, plot.right) - plot.left) /
                   (double)(plot.right - plot.left);
        if (g_state.fam == FAM_LOGISTIC) {
            double cr = mandel_real_from_client_x(plot, p.x);
            double c = clampd(cr, -2.0, 0.25);
            double r = 1.0 + sqrt(clampd(1.0 - 4.0 * c, 0.0, 9.0));
            set_param(r);
        } else {
            set_param(family_full_min(g_state.fam) +
                      (family_full_max(g_state.fam) - family_full_min(g_state.fam)) * t);
        }
    } else if (gw->kind == GRAPH_LAB) {
        double t = (double)(clampi(p.x, plot.left, plot.right) - plot.left) /
                   (double)(plot.right - plot.left);
        set_param(family_full_min(g_state.fam) +
                  (family_full_max(g_state.fam) - family_full_min(g_state.fam)) * t);
    }
}

static LRESULT CALLBACK graph_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    GraphWindow *gw = (GraphWindow *)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE:
        {
            CREATESTRUCTA *cs = (CREATESTRUCTA *)lp;
            gw = (GraphWindow *)cs->lpCreateParams;
            SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)gw);
            gw->hwnd = hwnd;
            ++g_live_windows;
        }
        return TRUE;
    case WM_NCHITTEST:
        {
            LRESULT hit = DefWindowProcA(hwnd, msg, wp, lp);
            if (hit == HTCLIENT) {
                POINT p;
                p.x = GET_X_LPARAM(lp);
                p.y = GET_Y_LPARAM(lp);
                ScreenToClient(hwnd, &p);
                if (p.y >= 0 && p.y < HEADER_H)
                    return HTCAPTION;
            }
            return hit;
        }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        if (gw) {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            paint_graph(gw, dc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    case WM_CONTEXTMENU:
        if (gw) {
            POINT pt;
            pt.x = GET_X_LPARAM(lp);
            pt.y = GET_Y_LPARAM(lp);
            if (pt.x == -1 && pt.y == -1) {
                RECT r;
                GetWindowRect(hwnd, &r);
                pt.x = (r.left + r.right) / 2;
                pt.y = (r.top + r.bottom) / 2;
            }
            show_context_menu(gw, pt);
            return 0;
        }
        break;
    case WM_RBUTTONUP:
        if (gw) {
            POINT pt;
            pt.x = GET_X_LPARAM(lp);
            pt.y = GET_Y_LPARAM(lp);
            ClientToScreen(hwnd, &pt);
            show_context_menu(gw, pt);
            return 0;
        }
        break;
    case WM_NCRBUTTONUP:
        if (gw) {
            POINT pt;
            pt.x = GET_X_LPARAM(lp);
            pt.y = GET_Y_LPARAM(lp);
            show_context_menu(gw, pt);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (gw) {
            command_from_window(gw, LOWORD(wp));
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (gw) {
            POINT p;
            RECT rc;
            RECT plot;
            int w, h;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            GetClientRect(hwnd, &rc);
            w = rc.right - rc.left;
            h = rc.bottom - rc.top;
            plot = plot_rect_for(gw->kind, w, h);
            track_mouse_leave(gw);
            update_hover(gw, p);
            if (gw->kind == GRAPH_LAB || point_in_rect(plot, p)) {
                start_drag(gw, p);
                handle_drag(gw, p, 0);
                return 0;
            }
        }
        break;
    case WM_MOUSEMOVE:
        if (gw) {
            POINT p;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            track_mouse_leave(gw);
            update_hover(gw, p);
            if (gw->dragging)
                handle_drag(gw, p, 0);
            return 0;
        }
        break;
    case WM_MOUSELEAVE:
        if (gw) {
            gw->tracking_mouse = 0;
            clear_hover(gw);
            return 0;
        }
        break;
    case WM_LBUTTONUP:
        if (gw && gw->dragging) {
            POINT p;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            handle_drag(gw, p, 1);
            gw->dragging = 0;
            update_hover(gw, p);
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
            save_current_session();
            return 0;
        }
        break;
    case WM_SIZE:
        if (gw)
            InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_WINDOWPOSCHANGED:
        if (gw)
            capture_window_rect(gw);
        break;
    case WM_DESTROY:
        if (gw) {
            capture_window_rect(gw);
            save_current_session();
            gw->hwnd = NULL;
            gw->dragging = 0;
            gw->hover_valid = 0;
            gw->tracking_mouse = 0;
            dib_destroy(&gw->cache);
            if (g_live_windows > 0)
                --g_live_windows;
            if (g_live_windows == 0)
                PostQuitMessage(0);
        }
        return 0;
    default:
        break;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void init_graph_defaults(void)
{
    int i;
    ZeroMemory(g_windows, sizeof(g_windows));
    for (i = 0; i < GRAPH_COUNT; ++i) {
        g_windows[i].kind = (GraphKind)i;
        g_windows[i].drag_mode = DRAG_PARAM;
    }
    g_windows[GRAPH_COB].drag_mode = DRAG_SEED;
}

static void show_graph_window(GraphKind kind)
{
    GraphWindow *gw = &g_windows[kind];
    DWORD style = WS_POPUP | WS_THICKFRAME | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    DWORD exstyle = WS_EX_APPWINDOW;
    RECT wa;
    int i = (int)kind;
    int ww, wh, gap = 12;
    int wx, wy;
    if (gw->hwnd) {
        ShowWindow(gw->hwnd, SW_SHOWNORMAL);
        return;
    }
    SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
    ww = (wa.right - wa.left - gap * 3) / 2;
    wh = (wa.bottom - wa.top - gap * 3) / 2;
    if (ww < 520) ww = 520;
    if (wh < 340) wh = 340;
    wx = wa.left + gap + (i % 2) * (ww + gap);
    wy = wa.top + gap + (i / 2) * (wh + gap);
    if (gw->has_last_rect) {
        wx = gw->last_rect.left;
        wy = gw->last_rect.top;
        ww = gw->last_rect.right - gw->last_rect.left;
        wh = gw->last_rect.bottom - gw->last_rect.top;
        if (ww < 320) ww = 320;
        if (wh < 240) wh = 240;
    }
    CreateWindowExA(exstyle, APP_CLASS_NAME, graph_title(kind), style,
                    wx, wy, ww, wh, NULL, NULL, g_inst, gw);
    if (gw->hwnd) {
        ShowWindow(gw->hwnd, SW_SHOWNORMAL);
        UpdateWindow(gw->hwnd);
    }
}

static HACCEL create_app_accelerators(void)
{
    ACCEL a[] = {
        {FVIRTKEY, VK_LEFT, IDM_PARAM_STEP_LEFT},
        {FVIRTKEY, VK_RIGHT, IDM_PARAM_STEP_RIGHT},
        {FVIRTKEY | FCONTROL, VK_LEFT, IDM_PARAM_STEP_FINE_LEFT},
        {FVIRTKEY | FCONTROL, VK_RIGHT, IDM_PARAM_STEP_FINE_RIGHT},
        {FVIRTKEY | FSHIFT, VK_LEFT, IDM_PARAM_STEP_COARSE_LEFT},
        {FVIRTKEY | FSHIFT, VK_RIGHT, IDM_PARAM_STEP_COARSE_RIGHT},
        {FVIRTKEY, VK_NEXT, IDM_PARAM_STEP_COARSE_LEFT},
        {FVIRTKEY, VK_PRIOR, IDM_PARAM_STEP_COARSE_RIGHT},

        {FVIRTKEY, VK_OEM_MINUS, IDM_LAB_PERIOD_DEC},
        {FVIRTKEY | FSHIFT, VK_OEM_MINUS, IDM_LAB_PERIOD_DEC},
        {FVIRTKEY, VK_SUBTRACT, IDM_LAB_PERIOD_DEC},
        {FVIRTKEY, VK_OEM_PLUS, IDM_LAB_PERIOD_INC},
        {FVIRTKEY | FSHIFT, VK_OEM_PLUS, IDM_LAB_PERIOD_INC},
        {FVIRTKEY, VK_ADD, IDM_LAB_PERIOD_INC},

        {FVIRTKEY, VK_SPACE, IDM_COB_RESEED},
        {FVIRTKEY, VK_RETURN, IDM_LAB_FIND},
        {FVIRTKEY, VK_F5, IDM_MAN_REBUILD},
        {FVIRTKEY, VK_BACK, IDM_BIF_RESET_VIEW},

        {FVIRTKEY, 'L', IDM_BIF_LYAP},
        {FVIRTKEY, 'P', IDM_BIF_PERIOD_COLORS},
        {FVIRTKEY, 'A', IDM_SHOW_ALL},
        {FVIRTKEY, 'T', IDM_TILE_WINDOWS},
        {FVIRTKEY | FCONTROL, 'R', IDM_RESET_ALL},

        {FVIRTKEY | FCONTROL, '1', IDM_FAMILY_LOGISTIC},
        {FVIRTKEY | FCONTROL, '2', IDM_FAMILY_TENT},
        {FVIRTKEY | FCONTROL, '3', IDM_FAMILY_SINE},

        {FVIRTKEY, VK_TAB, IDM_FOCUS_NEXT},
        {FVIRTKEY | FSHIFT, VK_TAB, IDM_FOCUS_PREV}
    };
    return CreateAcceleratorTableA(a, (int)ARRAYSIZE(a));
}

static int register_graph_class(void)
{
    WNDCLASSEXA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = graph_proc;
    wc.hInstance = g_inst;
    wc.hCursor = LoadCursorA(NULL, IDC_CROSS);
    wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIconA(NULL, IDI_APPLICATION);
    wc.lpszClassName = APP_CLASS_NAME;
    wc.hbrBackground = NULL;
    return RegisterClassExA(&wc) != 0;
}

int APIENTRY WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    MSG msg;
    (void)prev;
    (void)cmd;
    (void)show;

    g_inst = inst;
    srand((unsigned)GetTickCount());
    IteratorMruInit(inst);
    init_fonts();
    init_graph_defaults();
    reset_state(FAM_LOGISTIC);
    g_have_start_session = IteratorMruLoadSession(&g_start_session);
    if (g_have_start_session)
        apply_session_record(&g_start_session);

    if (!register_graph_class())
        return 1;
    g_accel = create_app_accelerators();

    if (g_have_start_session) {
        int i;
        for (i = 0; i < GRAPH_COUNT; ++i)
            show_graph_window((GraphKind)i);
    } else {
        tile_windows(NULL);
    }

    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        if (g_accel && msg.hwnd && TranslateAcceleratorA(msg.hwnd, g_accel, &msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (g_accel)
        DestroyAcceleratorTable(g_accel);
    destroy_fonts();
    return (int)msg.wParam;
}
