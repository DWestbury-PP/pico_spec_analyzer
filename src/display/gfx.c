/**
 * @file gfx.c
 * @brief Indexed framebuffer drawing.
 */

#include "display/gfx.h"
#include "display/font.h"
#include "config.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

static uint8_t  _fbuf[2][FB_BYTES] __attribute__((aligned(4)));
static uint16_t _pal[2][256];
static int _back;
static bool _front_valid;

uint8_t  *g_fb = _fbuf[0];
uint16_t *g_palette = _pal[0];

#define DIM_FACTOR 0.30f

// Save-under for overlays, one set per buffer
#define SAVE_BYTES 7168
#define SAVE_RECTS 4
typedef struct {
    int n, used;
    struct { int16_t x, y, w, h; uint16_t off; } r[SAVE_RECTS];
    uint8_t px[SAVE_BYTES];
} saveunder_t;
static saveunder_t _save[2];

// ============================================================================
// Palette
// ============================================================================

uint32_t gfx_scale_rgb(uint32_t rgb, float k) {
    int r = (int)(((rgb >> 16) & 0xFF) * k), g = (int)(((rgb >> 8) & 0xFF) * k), b = (int)((rgb & 0xFF) * k);
    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static uint16_t to565(uint32_t rgb) {
    return RGB565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

void gfx_set_color(uint8_t idx, uint32_t rgb) {
    idx &= 0x7F;
    g_palette[idx] = to565(rgb);
    g_palette[idx | GFX_DIM] = to565(gfx_scale_rgb(rgb, DIM_FACTOR));
}

static uint32_t lerp_rgb(uint32_t a, uint32_t b, float t) {
    int ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    int br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
    return ((uint32_t)(ar + (br - ar) * t) << 16) | ((uint32_t)(ag + (bg - ag) * t) << 8) |
           (uint32_t)(ab + (bb - ab) * t);
}

void gfx_ramp(uint8_t start, int n, const uint32_t *stops, int nstops) {
    for (int i = 0; i < n; i++) {
        float t = (n > 1) ? (float)i / (n - 1) * (nstops - 1) : 0.0f;
        int seg = (int)t;
        if (seg >= nstops - 1) seg = nstops - 2;
        gfx_set_color((uint8_t)(start + i), lerp_rgb(stops[seg], stops[seg + 1], t - seg));
    }
}

void gfx_init(void) {
    memset(_pal, 0, sizeof(_pal));
    memset(_save, 0, sizeof(_save));
    _back = 0;
    g_fb = _fbuf[0];
    g_palette = _pal[0];
    _front_valid = false;
    gfx_set_color(0, 0x000000);
    gfx_set_color(UI_WHITE,   0xFFFFFF);
    gfx_set_color(UI_LIGHT,   0xC8CCD8);
    gfx_set_color(UI_GREY,    0x80869A);
    gfx_set_color(UI_GRID,    0x1C2130);
    gfx_set_color(UI_GRID_HI, 0x343C52);
    gfx_set_color(UI_ACCENT,  0x3FD0FF);
    gfx_set_color(UI_WARN,    0xFFB020);
    gfx_set_color(UI_GOOD,    0x40FF90);
    gfx_clear(0);
    memcpy(_pal[1], _pal[0], sizeof(_pal[0]));
    memset(_fbuf[1], 0, FB_BYTES);
}

// ============================================================================
// Double buffering
// ============================================================================

const uint8_t *gfx_front_fb(void)       { return _fbuf[_back ^ 1]; }
const uint16_t *gfx_front_palette(void) { return _pal[_back ^ 1]; }

void gfx_swap(void) {
    _back ^= 1;
    g_fb = _fbuf[_back];
    g_palette = _pal[_back];
    memcpy(g_palette, _pal[_back ^ 1], sizeof(_pal[0]));
    _save[_back].n = 0;
    _save[_back].used = 0;
    _front_valid = true;
}

void gfx_invalidate_front(void) {
    _front_valid = false;
}

void RAMFUNC(gfx_copy_front)(void) {
    if (!_front_valid) {
        gfx_clear(0);
        return;
    }
    const uint32_t *s = (const uint32_t *)_fbuf[_back ^ 1];
    uint32_t *d = (uint32_t *)g_fb, *end = d + FB_BYTES / 4;
    while (d < end) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; d += 4; s += 4; }

    // Newest overlay first: later overlays may have saved pixels of earlier ones
    const saveunder_t *sv = &_save[_back ^ 1];
    for (int i = sv->n - 1; i >= 0; i--) {
        const uint8_t *src = &sv->px[sv->r[i].off];
        for (int j = 0; j < sv->r[i].h; j++, src += sv->r[i].w)
            memcpy(&g_fb[(sv->r[i].y + j) * FB_W + sv->r[i].x], src, (size_t)sv->r[i].w);
    }
}

void gfx_overlay_rect(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > FB_W) w = FB_W - x;
    if (y + h > FB_H) h = FB_H - y;
    saveunder_t *sv = &_save[_back];
    if (w <= 0 || h <= 0 || sv->n >= SAVE_RECTS || sv->used + w * h > SAVE_BYTES) return;
    sv->r[sv->n].x = (int16_t)x; sv->r[sv->n].y = (int16_t)y;
    sv->r[sv->n].w = (int16_t)w; sv->r[sv->n].h = (int16_t)h;
    sv->r[sv->n].off = (uint16_t)sv->used;
    uint8_t *dst = &sv->px[sv->used];
    for (int j = 0; j < h; j++, dst += w) memcpy(dst, &g_fb[(y + j) * FB_W + x], (size_t)w);
    sv->used += w * h;
    sv->n++;
}

// ============================================================================
// Primitives
// ============================================================================

void RAMFUNC(gfx_clear)(uint8_t idx) {
    uint32_t w = idx * 0x01010101u;
    uint32_t *p = (uint32_t *)g_fb, *end = p + FB_BYTES / 4;
    while (p < end) { p[0] = w; p[1] = w; p[2] = w; p[3] = w; p += 4; }
}

void RAMFUNC(gfx_hline)(int x0, int x1, int y, uint8_t idx) {
    if ((unsigned)y >= FB_H) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < 0) x0 = 0;
    if (x1 >= FB_W) x1 = FB_W - 1;
    if (x0 > x1) return;
    memset(&g_fb[y * FB_W + x0], idx, (size_t)(x1 - x0 + 1));
}

void RAMFUNC(gfx_vline)(int x, int y0, int y1, uint8_t idx) {
    if ((unsigned)x >= FB_W) return;
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (y0 < 0) y0 = 0;
    if (y1 >= FB_H) y1 = FB_H - 1;
    uint8_t *p = &g_fb[y0 * FB_W + x];
    for (int y = y0; y <= y1; y++, p += FB_W) *p = idx;
}

void gfx_fill_rect(int x, int y, int w, int h, uint8_t idx) {
    for (int j = 0; j < h; j++) gfx_hline(x, x + w - 1, y + j, idx);
}

void gfx_dim_rect(int x, int y, int w, int h) {
    int x0 = x < 0 ? 0 : x, x1 = x + w > FB_W ? FB_W : x + w;
    int y0 = y < 0 ? 0 : y, y1 = y + h > FB_H ? FB_H : y + h;
    for (int j = y0; j < y1; j++) {
        uint8_t *p = &g_fb[j * FB_W];
        for (int i = x0; i < x1; i++) p[i] |= GFX_DIM;
    }
}

void RAMFUNC(gfx_line)(int x0, int y0, int x1, int y1, uint8_t idx) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        gfx_px(x0, y0, idx);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gfx_circle(int cx, int cy, int r, uint8_t idx) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_px(cx + x, cy + y, idx); gfx_px(cx - x, cy + y, idx);
        gfx_px(cx + x, cy - y, idx); gfx_px(cx - x, cy - y, idx);
        gfx_px(cx + y, cy + x, idx); gfx_px(cx - y, cy + x, idx);
        gfx_px(cx + y, cy - x, idx); gfx_px(cx - y, cy - x, idx);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void gfx_fill_circle(int cx, int cy, int r, uint8_t idx) {
    int x = r, y = 0, err = 1 - r;
    while (x >= y) {
        gfx_hline(cx - x, cx + x, cy + y, idx);
        gfx_hline(cx - x, cx + x, cy - y, idx);
        gfx_hline(cx - y, cx + y, cy + x, idx);
        gfx_hline(cx - y, cx + y, cy - x, idx);
        y++;
        if (err < 0) err += 2 * y + 1;
        else { x--; err += 2 * (y - x) + 1; }
    }
}

void RAMFUNC(gfx_remap)(const uint8_t *lut) {
    uint32_t *p = (uint32_t *)g_fb;
    for (int n = FB_W * FB_H / 4; n; n--) {
        uint32_t w = *p;
        if (w) {
            *p = (uint32_t)lut[w & 0xFF] | ((uint32_t)lut[(w >> 8) & 0xFF] << 8) |
                 ((uint32_t)lut[(w >> 16) & 0xFF] << 16) | ((uint32_t)lut[w >> 24] << 24);
        }
        p++;
    }
}

// ============================================================================
// Text
// ============================================================================

int gfx_text_width(const char *s, int scale) {
    int n = (int)strlen(s);
    return n ? (n * 6 - 1) * scale : 0;
}

int gfx_text(int x, int y, const char *s, uint8_t idx, int scale) {
    int x0 = x;
    for (; *s; s++, x += 6 * scale) {
        char ch = *s;
        if (ch < FONT_FIRST || ch > FONT_LAST) ch = '?';
        const uint8_t *g = font5x7[ch - FONT_FIRST];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; bits; row++, bits >>= 1) {
                if (bits & 1) {
                    if (scale == 1) gfx_px(x + col, y + row, idx);
                    else gfx_fill_rect(x + col * scale, y + row * scale, scale, scale, idx);
                }
            }
        }
    }
    return x - x0 - scale;
}
