/**
 * @file gfx.h
 * @brief 8-bit indexed framebuffer, palette management and drawing primitives.
 *
 * Palette layout:
 *   0          background (black)
 *   1..119     theme colours (set by each theme's enter())
 *   120..127   fixed UI colours
 *   128..255   the same 128 colours at ~30% brightness, so `idx | GFX_DIM`
 *              darkens any pixel (translucent panels, reflections)
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdbool.h>

#define FB_W 320
#define FB_H 240

#define GFX_THEME_FIRST  1
#define GFX_THEME_LAST   119
#define GFX_DIM          0x80

enum {
    UI_WHITE = 120,
    UI_LIGHT,
    UI_GREY,
    UI_GRID,
    UI_GRID_HI,
    UI_ACCENT,
    UI_WARN,
    UI_GOOD,
};

extern uint8_t  g_fb[FB_H * FB_W];
extern uint16_t g_palette[256];

void gfx_init(void);

/** Set colour idx (0..127) from 0xRRGGBB; also sets its dim twin. */
void gfx_set_color(uint8_t idx, uint32_t rgb);

/** Fill n palette entries from start with a gradient through nstops colours. */
void gfx_ramp(uint8_t start, int n, const uint32_t *stops, int nstops);

/** Scale 0xRRGGBB by k (0..1). */
uint32_t gfx_scale_rgb(uint32_t rgb, float k);

void gfx_clear(uint8_t idx);

static inline void gfx_px(int x, int y, uint8_t idx) {
    if ((unsigned)x < FB_W && (unsigned)y < FB_H) g_fb[y * FB_W + x] = idx;
}

/** Write idx only where it is numerically greater (brightness-ordered ramps). */
static inline void gfx_px_max(int x, int y, uint8_t idx) {
    if ((unsigned)x < FB_W && (unsigned)y < FB_H) {
        uint8_t *p = &g_fb[y * FB_W + x];
        if (idx > *p) *p = idx;
    }
}

void gfx_hline(int x0, int x1, int y, uint8_t idx);
void gfx_vline(int x, int y0, int y1, uint8_t idx);
void gfx_fill_rect(int x, int y, int w, int h, uint8_t idx);
void gfx_dim_rect(int x, int y, int w, int h);
void gfx_line(int x0, int y0, int x1, int y1, uint8_t idx);
void gfx_circle(int cx, int cy, int r, uint8_t idx);
void gfx_fill_circle(int cx, int cy, int r, uint8_t idx);

/** Remap every pixel through lut (trails / phosphor persistence). */
void gfx_remap(const uint8_t *lut);

/** 5x7 text, 6 px advance per scale. Returns the drawn width. */
int gfx_text(int x, int y, const char *s, uint8_t idx, int scale);
int gfx_text_width(const char *s, int scale);

/** Ordered-dither threshold 0..15 for (x, y). */
static inline int gfx_bayer4(int x, int y) {
    static const uint8_t m[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    return m[((y & 3) << 2) | (x & 3)];
}

#endif // GFX_H
