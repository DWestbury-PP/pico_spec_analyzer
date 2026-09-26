/**
 * @file rta.c
 * @brief Real-time analyser: 320-point curve with glow fill, peak-hold trace,
 *        dB/Hz graticule and a live dominant-frequency readout.
 */

#include "display/theme.h"
#include "display/gfx.h"
#include "platform.h"
#include <math.h>
#include <stdio.h>

#define PLOT_TOP   14
#define PLOT_BOT   216
#define PH         (PLOT_BOT - PLOT_TOP)
#define LABEL_Y    (FB_H - 10)

#define FILL_FIRST 1
#define FILL_N     64
#define LINE_IDX   100
#define GLOW_IDX   101
#define PEAK_IDX   102
#define FILL_FADE_PX 110                 // fill fades to dark this far below the curve

#define PEAK_FALL_DB_S 8.0f

static float _pk[SPEC_COLS];
static int16_t _ys[SPEC_COLS];

static void enter(void) {
    static const uint32_t fill[] = { 0x02070C, 0x06243A, 0x0A4E78, 0x1690C4, 0x5ED6F5 };
    gfx_ramp(FILL_FIRST, FILL_N, fill, 5);
    gfx_set_color(LINE_IDX, 0xEFFFFF);
    gfx_set_color(GLOW_IDX, 0x4FC8F0);
    gfx_set_color(PEAK_IDX, 0xFFB020);
    for (int c = 0; c < SPEC_COLS; c++) _pk[c] = 0.0f;
}

static void update(const spectrum_frame_t *f, float dt) {
    const float fall = LEVEL_MAX / f->range_db * PEAK_FALL_DB_S * dt;
    for (int c = 0; c < SPEC_COLS; c++) {
        float lv = f->level[c];
        _pk[c] -= fall;
        if (_pk[c] < lv) _pk[c] = lv;
    }
}

static inline int level_y(float lv) {
    return PLOT_BOT - (int)(lv * PH / LEVEL_MAX);
}

static void RAMFUNC(draw)(const spectrum_frame_t *f) {
    gfx_clear(0);
    char buf[32];

    // Graticule
    const float bottom = f->top_dbfs - f->range_db;
    for (float d = ceilf(bottom / 10.0f) * 10.0f; d <= f->top_dbfs; d += 10.0f) {
        int y = PLOT_BOT - (int)((d - bottom) / f->range_db * PH);
        for (int x = 0; x < FB_W; x += 3) gfx_px(x, y, UI_GRID_HI);
        if (y - 9 > PLOT_TOP) {
            snprintf(buf, sizeof(buf), "%.0f", (double)d);
            gfx_text(2, y - 9, buf, UI_GREY, 1);
        }
    }
    theme_freq_grid(PLOT_TOP, PLOT_BOT, UI_GRID_HI, UI_GRID);

    for (int x = 0; x < SPEC_COLS; x++) _ys[x] = (int16_t)level_y(f->level[x]);

    // Fill: brightest just under the curve, fading with depth; grid shows through dark areas
    for (int x = 0; x < SPEC_COLS; x++) {
        int n = PLOT_BOT - _ys[x];
        if (n > FILL_FADE_PX * (FILL_N - 4) / FILL_N) n = FILL_FADE_PX * (FILL_N - 4) / FILL_N;
        uint8_t *p = &g_fb[(_ys[x] + 1) * FB_W + x];
        for (int d = 1; d <= n; d++, p += FB_W) *p = (uint8_t)(FILL_FIRST + FILL_N - 1 - d * FILL_N / FILL_FADE_PX);
    }

    // Curve with glow
    for (int x = 0; x < SPEC_COLS; x++) {
        int y0 = x ? _ys[x - 1] : _ys[0], y1 = _ys[x];
        int lo = y0 < y1 ? y0 : y1, hi = y0 < y1 ? y1 : y0;
        gfx_px(x, lo - 1, GLOW_IDX);
        gfx_vline(x, lo, hi, LINE_IDX);
    }

    // Peak-hold trace (dotted)
    for (int x = 0; x < SPEC_COLS; x += 2) {
        int y = level_y(_pk[x]);
        if (y < PLOT_BOT - 1) gfx_px(x, y, PEAK_IDX);
    }

    gfx_hline(0, FB_W - 1, PLOT_BOT + 1, UI_GRID_HI);
    theme_freq_labels(LABEL_Y, UI_GREY);

    // Dominant frequency readout + marker
    if (f->peak_dbfs > bottom + 6.0f) {
        int px = theme_hz_x(f->peak_hz);
        gfx_vline(px, PLOT_TOP, PLOT_TOP + 5, PEAK_IDX);
        char hz[16];
        theme_fmt_hz(hz, sizeof(hz), f->peak_hz);
        snprintf(buf, sizeof(buf), "%s %.1f dB", hz, (double)f->peak_dbfs);
        gfx_text(FB_W - gfx_text_width(buf, 1) - 2, 3, buf, UI_WHITE, 1);
    }
}

const theme_t theme_rta = {
    .name = "ANALYZER",
    .enter = enter,
    .update = update,
    .draw = draw,
};
